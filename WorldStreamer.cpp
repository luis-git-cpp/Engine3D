#include "WorldStreamer.h"

#include <algorithm>
#include <limits>

#include "raymath.h"

namespace
{
float DistanceSquared(Vector3 a, Vector3 b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}
} // namespace

WorldStreamer::WorldStreamer(ObjectManager& objects)
    : m_objects(objects), m_loader(2) // 2 threads de I/O
{
}

WorldStreamer::~WorldStreamer()
{
    m_loader.WaitAll();
    Clear();
}

// ------------------------------------------------------------------ registro

StreamId WorldStreamer::Add(const ObjectSpec& spec, bool persistent)
{
    Entry entry;
    entry.spec = spec;
    entry.cell = m_objects.Coordinates().ToCell(spec.params.position);
    entry.persistent = persistent;

    m_entries.push_back(entry);
    const StreamId id = static_cast<StreamId>(m_entries.size());
    m_aliveCount++;
    BucketInsert(id);
    m_dirty = true;

    if (persistent)
        StartLoadAsync(id);

    return id;
}

void WorldStreamer::Clear()
{
    // Cancela trabalhos pendentes (WaitAll garante que nenhum usa m_entries depois daqui).
    m_loader.WaitAll();

    for (StreamId id : m_loaded)
        m_objects.Destroy(EntryOf(id).object);

    m_entries.clear();
    m_byCell.clear();
    m_loaded.clear();
    m_pending.clear();
    m_asyncInFlight.clear();
    m_aliveCount = 0;
    m_hasFocus = false;
    m_dirty = true;
}

void WorldStreamer::SetLoadRadius(int cells)
{
    m_loadRadius = std::max(1, cells);
    m_dirty = true;
}

void WorldStreamer::SetUnloadMargin(int cells)
{
    m_unloadMargin = std::max(0, cells);
}

void WorldStreamer::SetMaxLoadsPerFrame(int count)
{
    m_maxLoadsPerFrame = std::max(1, count);
}

// ------------------------------------------------------------------ atualizacao

void WorldStreamer::Update(Vector3 focus)
{
    Refresh(focus, false);
}

void WorldStreamer::Prime(Vector3 focus)
{
    Refresh(focus, true);
}

void WorldStreamer::Refresh(Vector3 focus, bool unlimited)
{
    // === 0) Coleta prontos do AsyncLoader (chamado ANTES de qualquer trabalho novo) ===
    m_loader.Poll([this](MeshData&& data) {
        // Encontra o entry correspondente pelo path (para ModelFile) ou pelo estado Ready.
        // Mais simples: percorremos a fila de in-flight e casamos por ordem.
        // Como o loader devolve futures compartilhados, usamos o estado Ready do Entry.
        // Abaixo, FinishLoadOnMainThread é chamado pelo main loop — aqui só limpamos fila.

        // Estratégia: marcar todos os entries Loading cujo future ficou ready.
        // (Mais barato que casar por path e resolve o caso de paths iguais.)
        (void)data; // usado só para manter a assinatura
        for (StreamId id : m_asyncInFlight)
        {
            Entry& e = EntryOf(id);
            if (e.asyncState != Entry::AsyncState::Loading) continue;
            if (!e.asyncFuture.valid()) continue;
            if (e.asyncFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
                continue;

            e.asyncResult = e.asyncFuture.get();
            e.asyncState = Entry::AsyncState::Ready;
        }
    });

    PollLodLoads();

    const CellCoord focusCell = m_objects.Coordinates().ToCell(focus);

    // === 1) Remove objects destruídos por fora ===
    for (std::size_t i = 0; i < m_loaded.size();)
    {
        const StreamId id = m_loaded[i];
        if (m_objects.Get(EntryOf(id).object) == nullptr)
        {
            Remove(id);
            m_loaded.erase(m_loaded.begin() + static_cast<std::ptrdiff_t>(i));
        }
        else
        {
            ++i;
        }
    }

    // === 2) Descarrega longe ===
    for (std::size_t i = 0; i < m_loaded.size();)
    {
        const StreamId id = m_loaded[i];
        const Entry& e = EntryOf(id);

        if (!e.persistent && CoordinateSystem::CellDistance(e.cell, focusCell) > m_loadRadius + m_unloadMargin)
        {
            Unload(id);
            m_loaded.erase(m_loaded.begin() + static_cast<std::ptrdiff_t>(i));
        }
        else
        {
            ++i;
        }
    }

    // === 3) Fila de carga só quando o foco muda ===
    if (!m_hasFocus || focusCell != m_lastFocusCell || m_dirty || unlimited)
    {
        m_lastFocusCell = focusCell;
        m_hasFocus = true;
        m_dirty = false;
        RebuildPending(focusCell, focus);
    }

    // === 4) Inicia cargas assíncronas até o orçamento (as mais perto primeiro) ===
    int budget = unlimited ? std::numeric_limits<int>::max() : m_maxLoadsPerFrame;
    while (budget > 0 && !m_pending.empty())
    {
        const StreamId id = m_pending.back();
        m_pending.pop_back();

        Entry& e = EntryOf(id);
        if (!e.alive || e.object != InvalidObjectId) continue; // já carregado
        if (e.asyncState == Entry::AsyncState::Loading) continue; // já em andamento
        if (e.asyncState == Entry::AsyncState::Ready)
        {
            FinishLoadOnMainThread(id);
            budget--;
            continue;
        }

        StartLoadAsync(id);
        budget--;
    }

    // === 4.5) Aplica LODs prontos
    for (StreamId id : m_loaded)
    {
        Entry& e = EntryOf(id);
        if (!e.spec.HasLod()) continue;
        if (!e.alive) continue;

        const int desired = DesiredLod(id, focus);

        // Precisa trocar?
        if (desired != e.currentLod && e.readyLodData.count(desired) != 0)
        {
            ApplyLod(id, desired);
        }

        // Pré-carrega vizinhos (próximo mais fino + próximo mais grosso)
        const int fine = std::max(0, desired - 1);
        const int coarse = std::min(static_cast<int>(e.spec.lod.lods.size()) - 1, desired + 1);
        StartLodLoad(id, fine);
        StartLodLoad(id, coarse);
    }

    // === 5) Finaliza no main thread os que já ficaram prontos ===
    // (pode ser que o future tenha resolvido enquanto estávamos no loop acima)
    for (std::size_t i = 0; i < m_asyncInFlight.size();)
    {
        const StreamId id = m_asyncInFlight[i];
        Entry& e = EntryOf(id);
        if (!e.alive)
        {
            m_asyncInFlight.erase(m_asyncInFlight.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }

        if (e.asyncState == Entry::AsyncState::Ready)
        {
            FinishLoadOnMainThread(id);
            m_asyncInFlight.erase(m_asyncInFlight.begin() + static_cast<std::ptrdiff_t>(i));
        }
        else if (e.object != InvalidObjectId)
        {
            // Já carregado por outro caminho (ex: re-adicionado). Só limpa.
            m_asyncInFlight.erase(m_asyncInFlight.begin() + static_cast<std::ptrdiff_t>(i));
        }
        else
        {
            ++i;
        }
    }
}

void WorldStreamer::RebuildPending(CellCoord focusCell, Vector3 focus)
{
    m_pending.clear();

    const int r = m_loadRadius;
    for (int x = focusCell.x - r; x <= focusCell.x + r; x++)
    {
        for (int y = focusCell.y - r; y <= focusCell.y + r; y++)
        {
            for (int z = focusCell.z - r; z <= focusCell.z + r; z++)
            {
                auto it = m_byCell.find(CellCoord{x, y, z});
                if (it == m_byCell.end()) continue;

                for (StreamId id : it->second)
                {
                    const Entry& e = EntryOf(id);
                    if (!e.alive) continue;
                    if (e.object != InvalidObjectId) continue;
                    if (e.asyncState == Entry::AsyncState::Loading) continue;
                    m_pending.push_back(id);
                }
            }
        }
    }

    const CoordinateSystem& coords = m_objects.Coordinates();
    std::sort(m_pending.begin(), m_pending.end(), [&](StreamId a, StreamId b) {
        return DistanceSquared(coords.CellCenter(EntryOf(a).cell), focus) >
               DistanceSquared(coords.CellCenter(EntryOf(b).cell), focus);
    });
}

// ------------------------------------------------------------------ carregar (assíncrono)

void WorldStreamer::StartLoadAsync(StreamId id)
{
    Entry& e = EntryOf(id);
    if (e.asyncState != Entry::AsyncState::Idle) return;

    if (e.spec.HasLod())
    {
        // Multi-LOD: começa pelo LOD mais próximo (0).
        StartLodLoad(id, 0);
        return;
    }

    if (e.spec.kind == ObjectSpec::Kind::Primitive)
    {
        e.asyncFuture = m_loader.SubmitPrimitive(static_cast<int>(e.spec.primitive), e.spec.size);
    }
    else
    {
        e.asyncFuture = m_loader.SubmitModelFile(e.spec.modelPath);
    }

    e.asyncState = Entry::AsyncState::Loading;
    m_asyncInFlight.push_back(id);
}

// Chamado SEMPRE no main thread. Faz o upload para GPU.
void WorldStreamer::FinishLoadOnMainThread(StreamId id)
{
    Entry& e = EntryOf(id);
    const MeshData& data = e.asyncResult;

    ObjectId object = InvalidObjectId;

    if (data.failed)
    {
        TraceLog(LOG_WARNING, "WorldStreamer: falha ao carregar '%s'", e.spec.params.name.c_str());
        Remove(id);
        return;
    }

    if (data.kind == MeshData::Kind::Primitive)
    {
        object = m_objects.CreatePrimitive(
            static_cast<PrimitiveType>(data.primitiveType), data.size, e.spec.params);
    }
    else if (data.kind == MeshData::Kind::ModelFile)
    {
        // AQUI é main thread: seguro chamar LoadModel (usa OpenGL).
        object = m_objects.CreateFromFile(data.modelPath, e.spec.params);
    }

    if (object == InvalidObjectId)
    {
        TraceLog(LOG_WARNING, "WorldStreamer: não foi possível criar '%s'", e.spec.params.name.c_str());
        Remove(id);
        return;
    }

    if (GameObject* obj = m_objects.Get(object))
    {
        obj->velocity = e.velocity;
        if (e.spec.onCreate) e.spec.onCreate(*obj);
    }

    e.object = object;
    e.asyncState = Entry::AsyncState::Idle;
    e.asyncResult = MeshData{}; // libera
    m_loaded.push_back(id);
}

// Guarda estado e destrói.
void WorldStreamer::Unload(StreamId id)
{
    Entry& e = EntryOf(id);

    if (const GameObject* obj = m_objects.Get(e.object))
    {
        e.spec.params.position = obj->position;
        e.spec.params.rotation = obj->rotation;
        e.velocity = obj->velocity;
    }
    m_objects.Destroy(e.object);
    e.object = InvalidObjectId;
    e.asyncState = Entry::AsyncState::Idle;
    e.asyncResult = MeshData{};

    e.currentLod = -1;
    e.loadedLods.clear();
    e.pendingLodLoads.clear();
    e.readyLodData.clear();

    const CellCoord newCell = m_objects.Coordinates().ToCell(e.spec.params.position);
    if (newCell != e.cell)
    {
        BucketRemove(id);
        e.cell = newCell;
        BucketInsert(id);
    }
}

void WorldStreamer::Remove(StreamId id)
{
    Entry& e = EntryOf(id);
    if (!e.alive) return;

    BucketRemove(id);
    e.alive = false;
    e.object = InvalidObjectId;
    e.asyncState = Entry::AsyncState::Idle;
    e.asyncResult = MeshData{};
    m_aliveCount--;
}

void WorldStreamer::BucketInsert(StreamId id)
{
    m_byCell[EntryOf(id).cell].push_back(id);
}

void WorldStreamer::BucketRemove(StreamId id)
{
    auto it = m_byCell.find(EntryOf(id).cell);
    if (it == m_byCell.end()) return;

    std::vector<StreamId>& ids = it->second;
    ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
    if (ids.empty()) m_byCell.erase(it);
}

// ------------------------------------------------------------------ debug

void WorldStreamer::DrawDebug(Vector3 focus) const
{
    const CoordinateSystem& coords = m_objects.Coordinates();
    const float s = coords.CellSize();
    const CellCoord focusCell = coords.ToCell(focus);

    for (const auto& bucket : m_byCell)
    {
        if (CoordinateSystem::CellDistance(bucket.first, focusCell) > m_loadRadius + 3) continue;

        bool anyLoaded = false;
        for (StreamId id : bucket.second)
            if (m_entries[id - 1].object != InvalidObjectId) anyLoaded = true;

        DrawCubeWires(coords.CellCenter(bucket.first), s, s, s,
                      anyLoaded ? Fade(GREEN, 0.8f) : Fade(RED, 0.45f));
    }

    const float side = s * static_cast<float>(2 * m_loadRadius + 1);
    DrawCubeWires(coords.CellCenter(focusCell), side, side, side, YELLOW);
}

void WorldStreamer::StartLodLoad(StreamId id, int lodIndex)
{
    Entry& e = EntryOf(id);
    const LodSpec& spec = e.spec.lod;

    if (lodIndex < 0 || lodIndex >= static_cast<int>(spec.lods.size())) return;
    if (e.loadedLods.count(lodIndex) != 0) return;
    if (e.pendingLodLoads.count(lodIndex) != 0) return;

    const LodLevel& level = spec.lods[lodIndex];

    std::shared_future<MeshData> fut;
    if (level.kind == LodLevel::Kind::Primitive)
        fut = m_loader.SubmitPrimitive(static_cast<int>(level.primitive), level.size);
    else
        fut = m_loader.SubmitModelFile(level.modelPath);

    e.pendingLodLoads.emplace(lodIndex, std::move(fut));
}

void WorldStreamer::PollLodLoads()
{
    for (StreamId id : m_loaded)
    {
        Entry& e = EntryOf(id);
        if (e.pendingLodLoads.empty()) continue;

        for (auto it = e.pendingLodLoads.begin(); it != e.pendingLodLoads.end();)
        {
            if (it->second.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            {
                ++it;
                continue;
            }

            MeshData data = it->second.get();
            if (data.failed)
            {
                TraceLog(LOG_WARNING, "WorldStreamer: LOD %d de '%s' falhou",
                         it->first, e.spec.params.name.c_str());
                it = e.pendingLodLoads.erase(it);
                continue;
            }

            e.readyLodData[it->first] = std::move(data);
            it = e.pendingLodLoads.erase(it);
        }
    }
}

void WorldStreamer::ApplyLod(StreamId id, int lodIndex)
{
    Entry& e = EntryOf(id);
    const LodSpec& spec = e.spec.lod;

    if (lodIndex < 0 || lodIndex >= static_cast<int>(spec.lods.size())) return;
    if (e.currentLod == lodIndex) return;
    if (e.readyLodData.count(lodIndex) == 0) return; // ainda não pronto

    // Cria o novo modelo
    MeshData& data = e.readyLodData[lodIndex];
    const LodLevel& level = spec.lods[lodIndex];

    ObjectParams p = e.spec.params;
    if (level.tint.has_value()) p.tint = *level.tint;

    ObjectId newObject = InvalidObjectId;
    if (data.kind == MeshData::Kind::Primitive)
    {
        newObject = m_objects.CreatePrimitive(
            static_cast<PrimitiveType>(data.primitiveType), data.size, p);
    }
    else
    {
        newObject = m_objects.CreateFromFile(data.modelPath, p);
    }

    if (newObject == InvalidObjectId)
    {
        TraceLog(LOG_WARNING, "WorldStreamer: falha ao aplicar LOD %d em '%s'",
                 lodIndex, e.spec.params.name.c_str());
        return;
    }

    // Destrói o objeto antigo (se houver)
    if (e.object != InvalidObjectId)
    {
        // Preserva posição/velocidade atuais
        if (const GameObject* old = m_objects.Get(e.object))
        {
            if (GameObject* nw = m_objects.Get(newObject))
            {
                nw->position = old->position;
                nw->rotation = old->rotation;
                nw->velocity = old->velocity;
                nw->body     = old->body;
                nw->grounded = old->grounded;
            }
        }
        m_objects.Destroy(e.object);
    }

    e.object = newObject;
    e.currentLod = lodIndex;
    e.loadedLods.insert(lodIndex);
    e.readyLodData.erase(lodIndex); // já consumido

    if (GameObject* obj = m_objects.Get(newObject))
    {
        if (e.spec.onCreate) e.spec.onCreate(*obj);
    }

    // Marca como carregado no streamer
    if (std::find(m_loaded.begin(), m_loaded.end(), id) == m_loaded.end())
        m_loaded.push_back(id);
}

float WorldStreamer::DistanceToFocus(const Entry& e, Vector3 focus) const
{
    const CoordinateSystem& coords = m_objects.Coordinates();
    // Usa a posição real do objeto vivo (se carregado) ou a posição registrada.
    if (const GameObject* obj = m_objects.Get(e.object))
        return Vector3Distance(obj->position, focus);
    return Vector3Distance(coords.CellCenter(e.cell), focus);
}

int WorldStreamer::DesiredLod(StreamId id, Vector3 focus) const
{
    const Entry& e = EntryOf(id);
    if (!e.spec.HasLod()) return -1;
    const float d = DistanceToFocus(e, focus);
    return e.spec.lod.SelectLod(d, e.currentLod);
}