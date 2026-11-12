#pragma once

#include <cstdint>
#include <functional>
#include <future>
#include <string>
#include <unordered_map>
#include <vector>
#include <set>
#include <map>

#include "AsyncLoader.h"
#include "Coordinates.h"
#include "GameObject.h"
#include "ObjectManager.h"
#include "raylib.h"
#include "LodSpec.h"

using StreamId = std::uint32_t;
constexpr StreamId InvalidStreamId = 0;

struct ObjectSpec
{
    enum class Kind { Primitive, ModelFile } kind = Kind::Primitive;

    //Kind kind = Kind::Primitive;
    PrimitiveType primitive = PrimitiveType::Cube;
    Vector3 size{1.0f, 1.0f, 1.0f};
    std::string modelPath;

    LodSpec lod;

    ObjectParams params;
    std::function<void(GameObject&)> onCreate;

    bool HasLod() const { return lod.IsValid(); }
};

class WorldStreamer
{
public:
    explicit WorldStreamer(ObjectManager& objects);
    ~WorldStreamer();

    WorldStreamer(const WorldStreamer&) = delete;
    WorldStreamer& operator=(const WorldStreamer&) = delete;

    StreamId Add(const ObjectSpec& spec, bool persistent = false);
    void Clear();

    void SetLoadRadius(int cells);
    void SetUnloadMargin(int cells);
    void SetMaxLoadsPerFrame(int count);
    int  LoadRadius() const { return m_loadRadius; }

    // Quantos trabalhos assíncronos estão em andamento (útil para o HUD).
    int AsyncPending() const { return m_loader.PendingCount(); }

    int TotalLoadedLods() const
    {
        int total = 0;
        for (const Entry& e : m_entries)
            total += static_cast<int>(e.loadedLods.size());
        return total;
    }

    // Update chama Poll() do loader no início para processar prontos do frame anterior.
    void Update(Vector3 focus);
    void Prime(Vector3 focus);

    void DrawDebug(Vector3 focus) const;

    int RegisteredCount() const { return m_aliveCount; }
    int LoadedCount() const { return static_cast<int>(m_loaded.size()); }
    int PendingCount() const { return static_cast<int>(m_pending.size()); }

private:
    struct Entry
    {
        ObjectSpec spec;
        CellCoord cell{};
        Vector3 velocity{0.0f, 0.0f, 0.0f};
        ObjectId object = InvalidObjectId;
        bool persistent = false;
        bool alive = true;

        // --- Async ---
        enum class AsyncState { Idle, Loading, Ready } asyncState = AsyncState::Idle;
        std::shared_future<MeshData> asyncFuture;
        MeshData asyncResult; // guardado até o main thread consumir

        // --- LOD ---
        int currentLod = -1;
        std::set<int> loadedLods;
        std::map<int, std::shared_future<MeshData>> pendingLodLoads;
        std::map<int, MeshData> readyLodData; 
    };

    Entry& EntryOf(StreamId id) { return m_entries[id - 1]; }
    const Entry& EntryOf(StreamId id) const { return m_entries[id - 1]; }

    void Refresh(Vector3 focus, bool unlimited);
    void RebuildPending(CellCoord focusCell, Vector3 focus);
    void StartLoadAsync(StreamId id);
    void FinishLoadOnMainThread(StreamId id);
    void Unload(StreamId id);
    void Remove(StreamId id);
    void BucketInsert(StreamId id);
    void BucketRemove(StreamId id);
    void StartLodLoad(StreamId id, int lodIndex);
    void ApplyLod(StreamId id, int lodIndex);
    void PollLodLoads();
    int  DesiredLod(StreamId id, Vector3 focus) const;
    float DistanceToFocus(const Entry& e, Vector3 focus) const;

    ObjectManager& m_objects;
    AsyncLoader m_loader;

    std::vector<Entry> m_entries;
    std::unordered_map<CellCoord, std::vector<StreamId>, CellCoordHash> m_byCell;
    std::vector<StreamId> m_loaded;
    std::vector<StreamId> m_pending;
    std::vector<StreamId> m_asyncInFlight; // ids com asyncState == Loading

    int m_aliveCount = 0;
    int m_loadRadius = 6;
    int m_unloadMargin = 1;
    int m_maxLoadsPerFrame = 4;

    CellCoord m_lastFocusCell{};
    bool m_hasFocus = false;
    bool m_dirty = true;
};