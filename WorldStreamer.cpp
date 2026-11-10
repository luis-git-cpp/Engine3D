#include "WorldStreamer.h"

#include <algorithm>
#include <limits>

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
    : m_objects(objects)
{
}

WorldStreamer::~WorldStreamer()
{
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
        Load(id);

    return id;
}

void WorldStreamer::Clear()
{
    for (StreamId id : m_loaded)
        m_objects.Destroy(EntryOf(id).object);

    m_entries.clear();
    m_byCell.clear();
    m_loaded.clear();
    m_pending.clear();
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
    const CellCoord focusCell = m_objects.Coordinates().ToCell(focus);

    // 1) Objetos destruidos por fora (ex.: uma caixa que caiu do mundo) saem do streaming de vez.
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

    // 2) Descarrega o que ficou longe (raio + margem).
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

    // 3) A fila de carga so e refeita quando o foco muda de celula (ou algo foi registrado).
    if (!m_hasFocus || focusCell != m_lastFocusCell || m_dirty || unlimited)
    {
        m_lastFocusCell = focusCell;
        m_hasFocus = true;
        m_dirty = false;
        RebuildPending(focusCell, focus);
    }

    // 4) Carrega alguns por quadro, os mais perto primeiro.
    int budget = unlimited ? std::numeric_limits<int>::max() : m_maxLoadsPerFrame;
    while (budget > 0 && !m_pending.empty())
    {
        const StreamId id = m_pending.back();
        m_pending.pop_back();

        const Entry& e = EntryOf(id);
        if (!e.alive || e.object != InvalidObjectId) continue; // ficou obsoleta

        Load(id);
        budget--;
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
                    if (e.alive && e.object == InvalidObjectId)
                        m_pending.push_back(id);
                }
            }
        }
    }

    // Do mais longe para o mais perto: o mais perto fica no fim e sai primeiro (pop_back).
    const CoordinateSystem& coords = m_objects.Coordinates();
    std::sort(m_pending.begin(), m_pending.end(), [&](StreamId a, StreamId b) {
        return DistanceSquared(coords.CellCenter(EntryOf(a).cell), focus) >
               DistanceSquared(coords.CellCenter(EntryOf(b).cell), focus);
    });
}

// ------------------------------------------------------------------ carregar e descarregar

void WorldStreamer::Load(StreamId id)
{
    Entry& e = EntryOf(id);

    ObjectId object = InvalidObjectId;
    if (e.spec.kind == ObjectSpec::Kind::Primitive)
        object = m_objects.CreatePrimitive(e.spec.primitive, e.spec.size, e.spec.params);
    else
        object = m_objects.CreateFromFile(e.spec.modelPath, e.spec.params);

    if (object == InvalidObjectId)
    {
        // Modelo ausente ou invalido: remove o registro para nao tentar de novo a cada quadro.
        TraceLog(LOG_WARNING, "WorldStreamer: nao foi possivel criar '%s'", e.spec.params.name.c_str());
        Remove(id);
        return;
    }

    if (GameObject* obj = m_objects.Get(object))
    {
        obj->velocity = e.velocity;
        if (e.spec.onCreate) e.spec.onCreate(*obj);
    }

    e.object = object;
    m_loaded.push_back(id);
}

// Guarda o estado do objeto no registro e o destroi. (O chamador tira o id de m_loaded.)
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

    // O objeto pode ter se movido (caixa empurrada, bola rolando): ele agora pertence a outra celula.
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

// ------------------------------------------------------------------ depuracao

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
        {
            if (m_entries[id - 1].object != InvalidObjectId) anyLoaded = true;
        }

        DrawCubeWires(coords.CellCenter(bucket.first), s, s, s,
                      anyLoaded ? Fade(GREEN, 0.8f) : Fade(RED, 0.45f));
    }

    // Limite da area carregada em volta do foco.
    const float side = s * static_cast<float>(2 * m_loadRadius + 1);
    DrawCubeWires(coords.CellCenter(focusCell), side, side, side, YELLOW);
}
