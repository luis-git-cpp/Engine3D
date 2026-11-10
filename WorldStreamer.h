#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Coordinates.h"
#include "GameObject.h"
#include "ObjectManager.h"
#include "raylib.h"

using StreamId = std::uint32_t;
constexpr StreamId InvalidStreamId = 0;

// Descricao de um objeto que ainda nao existe no mundo ("dormente"). O WorldStreamer cria o
// objeto de verdade quando o jogador chega perto e o destroi quando ele se afasta.
struct ObjectSpec
{
    enum class Kind
    {
        Primitive, // plano, cubo, esfera ou cilindro
        ModelFile  // modelo externo (.obj, .gltf, .glb ...)
    };

    Kind kind = Kind::Primitive;
    PrimitiveType primitive = PrimitiveType::Cube;
    Vector3 size{1.0f, 1.0f, 1.0f};   // Primitive: ver PrimitiveType
    std::string modelPath;             // ModelFile
    ObjectParams params;               // posicao, camada, fisica... (a posicao e a posicao inicial)

    // Chamado toda vez que o objeto e (re)criado: ideal para ligar callbacks, mudar materiais etc.
    std::function<void(GameObject&)> onCreate;
};

// Carregamento por celulas: mantem objetos so ao redor de um foco (jogador ou camera).
// Cada objeto registrado pertence a uma celula da grade do mundo (pela posicao dele). Celulas a
// ate "loadRadius" celulas do foco ficam carregadas; as que passam de "loadRadius + margem" sao
// descarregadas (a margem evita carregar e descarregar sem parar na fronteira).
// Objetos dinamicos guardam posicao e velocidade ao serem descarregados e voltam do mesmo jeito.
class WorldStreamer
{
public:
    explicit WorldStreamer(ObjectManager& objects);
    ~WorldStreamer();

    WorldStreamer(const WorldStreamer&) = delete;
    WorldStreamer& operator=(const WorldStreamer&) = delete;

    // Registra um objeto. "persistent" = e criado na hora e nunca descarregado.
    StreamId Add(const ObjectSpec& spec, bool persistent = false);

    // Esquece todos os registros e destroi os objetos carregados.
    void Clear();

    void SetLoadRadius(int cells);        // padrao: 6
    void SetUnloadMargin(int cells);      // padrao: 1
    void SetMaxLoadsPerFrame(int count);  // padrao: 4 (evita travadas ao andar)
    int LoadRadius() const { return m_loadRadius; }

    // Carrega e descarrega conforme o foco. Chamar 1x por quadro.
    void Update(Vector3 focus);
    // Igual ao Update, mas carrega tudo de uma vez (use no inicio e depois de teleportes).
    void Prime(Vector3 focus);

    // Desenha as celulas com objetos (verde = carregada, vermelho = dormente) e o limite de carga.
    void DrawDebug(Vector3 focus) const;

    int RegisteredCount() const { return m_aliveCount; }
    int LoadedCount() const { return static_cast<int>(m_loaded.size()); }
    int PendingCount() const { return static_cast<int>(m_pending.size()); }

private:
    struct Entry
    {
        ObjectSpec spec;
        CellCoord cell{};                      // celula da posicao registrada
        Vector3 velocity{0.0f, 0.0f, 0.0f};    // estado guardado ao descarregar
        ObjectId object = InvalidObjectId;     // objeto vivo (se carregado)
        bool persistent = false;
        bool alive = true;                     // false = removido do streaming para sempre
    };

    Entry& EntryOf(StreamId id) { return m_entries[id - 1]; }

    void Refresh(Vector3 focus, bool unlimited);
    void RebuildPending(CellCoord focusCell, Vector3 focus);
    void Load(StreamId id);
    void Unload(StreamId id);
    void Remove(StreamId id);
    void BucketInsert(StreamId id);
    void BucketRemove(StreamId id);

    ObjectManager& m_objects;

    std::vector<Entry> m_entries;                                       // indice = StreamId - 1
    std::unordered_map<CellCoord, std::vector<StreamId>, CellCoordHash> m_byCell;
    std::vector<StreamId> m_loaded;
    std::vector<StreamId> m_pending;                                    // a carregar; o mais perto fica no fim

    int m_aliveCount = 0;
    int m_loadRadius = 6;
    int m_unloadMargin = 1;
    int m_maxLoadsPerFrame = 4;

    CellCoord m_lastFocusCell{};
    bool m_hasFocus = false;
    bool m_dirty = true;
};
