#include "AsyncLoader.h"

#include <chrono>
#include <algorithm>

AsyncLoader::AsyncLoader(int threadCount)
{
    threadCount = std::max(1, threadCount);
    for (int i = 0; i < threadCount; i++)
        m_workers.emplace_back(&AsyncLoader::WorkerLoop, this);
}

AsyncLoader::~AsyncLoader()
{
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_stop = true;
    }
    m_cv.notify_all();
    for (auto& t : m_workers)
        if (t.joinable()) t.join();
}

void AsyncLoader::WorkerLoop()
{
    while (!m_stop)
    {
        std::unique_ptr<Job> job;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_cv.wait(lock, [this] { return m_stop || !m_queue.empty(); });
            if (m_stop && m_queue.empty()) return;
            job = std::move(m_queue.front());
            m_queue.pop();
        }

        MeshData result;
        try
        {
            result = job->task();
        }
        catch (...)
        {
            result.failed = true;
        }

        job->promise.set_value(std::move(result));

        // Registra o future pronto para o main thread consumir.
        {
            std::lock_guard<std::mutex> lock(m_readyMutex);
            m_ready.push_back(job->future);
        }

        // Mantém o job vivo até o main thread consumir.
        {
            std::lock_guard<std::mutex> lock(m_inFlightMutex);
            m_inFlight.push_back(std::move(job));
        }

        m_pending--;
    }
}

std::shared_future<MeshData> AsyncLoader::Submit(std::function<MeshData()> task)
{
    auto job = std::make_unique<Job>();
    job->task = std::move(task);
    job->future = job->promise.get_future().share();

    std::shared_future<MeshData> result = job->future;   // shared_future é copiável
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_queue.push(std::move(job));
    }
    m_pending++;
    m_cv.notify_one();
    return result;
}

std::shared_future<MeshData> AsyncLoader::SubmitPrimitive(int primitiveType, Vector3 size)
{
    return Submit([primitiveType, size]() {
        MeshData d;
        d.kind = MeshData::Kind::Primitive;
        d.primitiveType = primitiveType;
        d.size = size;
        return d;
    });
}

std::shared_future<MeshData> AsyncLoader::SubmitModelFile(std::string path)
{
    return Submit([path = std::move(path)]() {
        MeshData d;
        d.kind = MeshData::Kind::ModelFile;
        d.modelPath = path;
        if (!FileExists(path.c_str()))
        {
            const std::string besideExe = std::string(GetApplicationDirectory()) + path;
            if (FileExists(besideExe.c_str()))
                d.modelPath = besideExe;
            else
            {
                d.failed = true;
                return d;
            }
        }
        d.fileExists = true;
        return d;
    });
}

void AsyncLoader::Poll(ReadyCallback onReady)
{
    std::vector<std::shared_future<MeshData>> readyNow;
    {
        std::lock_guard<std::mutex> lock(m_readyMutex);
        readyNow.swap(m_ready);
    }

    for (auto& fut : readyNow)
    {
        if (fut.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            continue;

        MeshData data = fut.get();
        if (onReady) onReady(std::move(data));
    }

    // Limpa jobs cujos futures já foram consumidos.
    {
        std::lock_guard<std::mutex> lock(m_inFlightMutex);
        m_inFlight.erase(
            std::remove_if(m_inFlight.begin(), m_inFlight.end(),
                           [](const std::shared_ptr<Job>& j) {
                               return j->future.wait_for(std::chrono::seconds(0)) ==
                                      std::future_status::ready;
                           }),
            m_inFlight.end());
    }
}

int AsyncLoader::PendingCount() const
{
    return m_pending.load();
}

void AsyncLoader::WaitAll()
{
    // Aguarda até que não haja mais pendentes.
    // Timeout de segurança: se ficar preso >5s, libera mesmo assim.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (m_pending.load() > 0)
    {
        if (std::chrono::steady_clock::now() > deadline)
        {
            TraceLog(LOG_WARNING, "AsyncLoader::WaitAll: timeout, %d jobs pendentes", m_pending.load());
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::lock_guard<std::mutex> lock(m_readyMutex);
    m_ready.clear();
}