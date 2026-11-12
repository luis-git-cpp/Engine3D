#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <string>

#include "raylib.h"

// Resultado do trabalho de uma thread: dados prontos para upload na GPU (main thread).
struct MeshData
{
    // Primitiva: apenas os parâmetros para gerar a mesh no main thread.
    enum class Kind { None, Primitive, ModelFile } kind = Kind::None;

    // Primitiva
    int primitiveType = 0; // cast de PrimitiveType
    Vector3 size{1.0f, 1.0f, 1.0f};

    // Modelo externo: dados brutos lidos do disco
    std::string modelPath;
    bool fileExists = false;

    // Erro (ex: modelo ausente)
    bool failed = false;
};

// Loader assíncrono simples com pool de threads.
// - Submit() enfileira um trabalho e devolve um std::future<MeshData>.
// - O trabalho roda em thread secundária e NÃO toca em OpenGL.
// - O main thread faz Poll() para coletar os prontos e deve então chamar o callback
//   de upload (que usa Raylib normalmente).
class AsyncLoader
{
public:
    explicit AsyncLoader(int threadCount = 2);
    ~AsyncLoader();

    AsyncLoader(const AsyncLoader&) = delete;
    AsyncLoader& operator=(const AsyncLoader&) = delete;

    // Enfileira uma primitiva. O trabalho em si é trivial, mas libera o main thread.
    std::shared_future<MeshData> SubmitPrimitive(int primitiveType, Vector3 size);

    // Enfileira o carregamento de um modelo externo. Faz I/O do disco em thread.
    // O upload (LoadModel) é feito no main thread depois.
    std::shared_future<MeshData> SubmitModelFile(std::string path);

    // Executa uma tarefa genérica em thread e devolve future. Útil para tarefas custom.
    std::shared_future<MeshData> Submit(std::function<MeshData()> task);

    // Deve ser chamado no main thread a cada frame. Executa callbacks dos prontos.
    // O callback roda no main thread (pode chamar Raylib/OpenGL).
    using ReadyCallback = std::function<void(MeshData&&)>;
    void Poll(ReadyCallback onReady);

    // Número de trabalhos pendentes (em fila + em execução).
    int PendingCount() const;

    // Espera todos os trabalhos terminarem (para shutdown).
    void WaitAll();

private:
    struct Job
    {
        std::function<MeshData()> task;
        std::promise<MeshData> promise;
        std::shared_future<MeshData> future;
    };

    void WorkerLoop();

    std::vector<std::thread> m_workers;
    std::queue<std::unique_ptr<Job>> m_queue;
    mutable std::mutex m_queueMutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_stop{false};
    std::atomic<int> m_pending{0};

    // Fila de resultados prontos (main thread consome).
    mutable std::mutex m_readyMutex;
    std::vector<std::shared_future<MeshData>> m_ready;

    // Trabalhos cujo future já foi consumido/coletado.
    std::vector<std::shared_ptr<Job>> m_inFlight;
    mutable std::mutex m_inFlightMutex;
};