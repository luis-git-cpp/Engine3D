#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Coordinates.h"
#include "GameObject.h"
#include "LightManager.h"
#include "raylib.h"
#include <unordered_map>

enum class PrimitiveType
{
    Plane,    // size.x = largura, size.z = comprimento
    Cube,     // size = largura, altura, profundidade
    Sphere,   // size.x = diametro
    Cylinder  // size.x = diametro, size.y = altura (a base fica na posicao do objeto)
};

// Tudo que da para escolher ao criar um objeto.
struct ObjectParams
{
    std::string name;

    // Coordenadas
    Vector3 position{0.0f, 0.0f, 0.0f};
    Vector3 rotation{0.0f, 0.0f, 0.0f}; // graus
    Vector3 scale{1.0f, 1.0f, 1.0f};

    // Visual
    std::optional<Color> tint; // se vazio, mantem as cores do proprio modelo
    bool visible = true;       // desenhado na passada final
    bool castShadow = true;    // desenhado nos shadow maps

    // Fisica
    BodyType body = BodyType::Static;
    int layer = 0;
    bool isTrigger = false; // gatilho: gera eventos, mas nao bloqueia nem empurra ninguem
    float mass = 1.0f;
    float restitution = 0.3f;
    float friction = 0.5f;
    float linearDamping = 0.02f;

    // Colisor. Auto/Box/Sphere sem tamanho usam a caixa do modelo; informe tamanho e offset para
    // um colisor diferente do visual. Use ColliderShape::None para nao ter colisao.
    ColliderShape collider = ColliderShape::Auto;
    Vector3 colliderHalfExtents{0.0f, 0.0f, 0.0f}; // Box (0,0,0 = ajustar ao modelo)
    float colliderRadius = 0.0f;                   // Sphere (0 = ajustar ao modelo)
    std::optional<Vector3> colliderOffset;         // vazio = centro da caixa do modelo
};

// Cria e destroi objetos, guarda as coordenadas de cada um e mantem um indice espacial
// (grade de celulas) para consultas por regiao ou distancia.
class ObjectManager
{
public:
    explicit ObjectManager(const LightManager& lights, float cellSize = 8.0f);
    ~ObjectManager();

    ObjectManager(const ObjectManager&) = delete;
    ObjectManager& operator=(const ObjectManager&) = delete;

    // --- Criacao (devolvem InvalidObjectId em caso de falha) ---
    ObjectId CreatePrimitive(PrimitiveType type, Vector3 size, const ObjectParams& params);
    // Modelo externo: .obj, .gltf, .glb, .iqm, .vox, .m3d (caminho relativo a pasta do programa).
    ObjectId CreateFromFile(const std::string& modelPath, const ObjectParams& params);
    // Modelo ja carregado: o ObjectManager assume a posse e descarrega ao destruir o objeto.
    ObjectId CreateFromModel(Model model, const ObjectParams& params);
    // Carrega (ou reusa do cache) o modelo. Incrementa refCount.

    // --- Destruicao ---
    // Marca o objeto; ele some na hora (Get devolve nullptr) e e liberado em CollectGarbage().
    void Destroy(ObjectId id);
    void DestroyAll();
    // Libera de fato os objetos marcados. Chamar 1x por frame, fora de qualquer iteracao.
    void CollectGarbage();

    GameObject* Get(ObjectId id);
    const GameObject* Get(ObjectId id) const;
    int Count() const; // objetos vivos

    // Percorre os objetos vivos, na ordem de criacao.
    template <typename Fn>
    void ForEach(Fn&& fn)
    {
        for (auto& entry : m_objects)
        {
            GameObject& obj = *entry.second;
            if (!obj.pendingDestroy) fn(obj);
        }
    }

    template <typename Fn>
    void ForEach(Fn&& fn) const
    {
        for (const auto& entry : m_objects)
        {
            const GameObject& obj = *entry.second;
            if (!obj.pendingDestroy) fn(obj);
        }
    }

    // --- Coordenadas e consultas espaciais ---
    const CoordinateSystem& Coordinates() const { return m_coords; }

    // Recalcula a celula de cada objeto e atualiza o indice. A fisica ja chama isto; chame tambem
    // depois de mover objetos manualmente.
    void UpdateSpatialIndex();

    // Objetos cuja caixa (visual + colisor) toca a regiao. Os resultados sao acrescentados a "out".
    void QueryAABB(const BoundingBox& box, std::vector<ObjectId>& out) const;
    void QueryRadius(Vector3 center, float radius, std::vector<ObjectId>& out) const;
    void QueryCell(CellCoord cell, std::vector<ObjectId>& out) const;

private:
    void BuildCollider(GameObject& obj, const ObjectParams& params) const;
    void Reindex(GameObject& obj);
    void IndexInsert(GameObject& obj);
    void IndexRemove(GameObject& obj);

    const LightManager& m_lights;
    CoordinateSystem m_coords;
    ObjectId m_nextId = 1;

    std::map<ObjectId, std::unique_ptr<GameObject>> m_objects;
    std::unordered_map<CellCoord, std::vector<ObjectId>, CellCoordHash> m_grid;
};
