#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Collision.h"
#include "GameObject.h"
#include "ObjectManager.h"
#include "raylib.h"

// Filtro das consultas (raycast e afins).
struct QueryFilter
{
    std::uint32_t layerMask = 0xFFFFFFFFu; // bit n ligado = considera objetos da camada n
    ObjectId ignore = InvalidObjectId;     // objeto ignorado (ex.: o proprio jogador)
    bool includeStatic = true;             // estaticos e cinematicos
    bool includeDynamic = true;
    bool includeTriggers = false;
};

struct RaycastHit
{
    bool hit = false;
    ObjectId object = InvalidObjectId;
    float distance = 0.0f;           // ate o ponto de impacto (centro da esfera, no caso de radius > 0)
    Vector3 point{0.0f, 0.0f, 0.0f}; // ponto de contato na superficie
    Vector3 normal{0.0f, 1.0f, 0.0f};
};

// Fisica simples: gravidade, colisao entre caixas e esferas, camadas de colisao, consultas,
// corpos cinematicos (jogador) e eventos de colisao.
// - Static: so colide, nunca se move.
// - Dynamic: sofre gravidade e e empurrado por tudo que colide.
// - Kinematic: movido por codigo (MoveKinematic); empurra os dinamicos, mas ninguem o empurra.
// - Dois objetos so interagem se as camadas estiverem liberadas entre si
//   (por padrao, cada camada interage apenas com ela mesma).
class PhysicsWorld
{
public:
    static constexpr int MaxLayers = 32;

    explicit PhysicsWorld(ObjectManager& objects);

    void SetGravity(Vector3 gravity) { m_gravity = gravity; }
    Vector3 Gravity() const { return m_gravity; }

    // Objetos dinamicos que caem abaixo desta altura sao destruidos.
    void SetKillPlane(float y) { m_killY = y; }

    // --- Camadas ---
    // Libera (ou bloqueia) a interacao entre duas camadas, nos dois sentidos.
    void SetLayersInteract(int layerA, int layerB, bool interact);
    bool LayersInteract(int layerA, int layerB) const;
    // Mascara de bits com as camadas que interagem com "layer" (util para QueryFilter).
    std::uint32_t LayerMask(int layer) const;

    // --- Simulacao ---
    // Avanca a simulacao com passos fixos internos e dispara os eventos de colisao do quadro.
    void Step(float frameTime);
    // Esquece contatos e eventos pendentes (use ao recriar o mundo).
    void Reset();

    // --- Consultas ---
    // Lanca um raio (radius = 0) ou uma esfera (radius > 0, "sphere cast") e devolve o primeiro
    // objeto atingido. "direction" nao precisa estar normalizada.
    bool Raycast(Vector3 origin, Vector3 direction, float maxDistance, RaycastHit& hit,
                 const QueryFilter& filter = QueryFilter{}, float radius = 0.0f) const;

    // Empurra uma esfera para fora dos colisores que ela atravessa e devolve o novo centro.
    Vector3 PushOutSphere(Vector3 center, float radius, const QueryFilter& filter) const;

    // --- Corpos cinematicos ---
    // Move o corpo por "displacement" e o empurra para fora de paredes e chao: desliza nas
    // superficies, zera a velocidade contra elas e atualiza body.grounded. Colide so com
    // estaticos e cinematicos (os dinamicos sao empurrados pela propria simulacao).
    void MoveKinematic(GameObject& body, Vector3 displacement);

    // --- Eventos ---
    // Recebe todos os eventos (self = um dos objetos, other = o outro). Cada objeto tambem pode
    // ter o seu proprio GameObject::onCollision.
    void SetEventCallback(CollisionCallback callback) { m_eventCallback = std::move(callback); }

    // Desenha os colisores em arame (chamar dentro de BeginMode3D/EndMode3D).
    void DrawDebug() const;

private:
    struct ContactInfo
    {
        ObjectId a = InvalidObjectId;
        ObjectId b = InvalidObjectId;
        Vector3 normal{0.0f, 1.0f, 0.0f}; // de a para b
        float impact = 0.0f;
        bool trigger = false;
    };

    struct PairState
    {
        ContactInfo last;
        int missed = 0; // passos seguidos sem contato
    };

    void FixedStep(float dt);
    void Integrate(GameObject& obj, float dt) const;
    void ResolveCollisions();
    void Resolve(GameObject& a, GameObject& b);

    void RecordContact(const GameObject& a, const GameObject& b, const Contact& contact, float impactSpeed);
    void UpdatePairs();
    void EmitStayEvents();
    void QueueEvent(CollisionEventType type, const ContactInfo& info);
    void DispatchEvents();
    static std::uint64_t PairKey(ObjectId a, ObjectId b);

    ObjectManager& m_objects;
    Vector3 m_gravity{0.0f, -9.81f, 0.0f};
    float m_killY = -30.0f;
    float m_accumulator = 0.0f;

    std::array<std::uint32_t, MaxLayers> m_layerMask{}; // bit b ligado = camada a interage com b
    std::vector<ObjectId> m_candidates;

    // Eventos de colisao
    std::unordered_map<std::uint64_t, ContactInfo> m_touchedNow;  // contatos do passo atual
    std::unordered_map<std::uint64_t, PairState> m_pairs;         // pares que se tocam
    std::unordered_set<std::uint64_t> m_enteredThisFrame;
    std::vector<CollisionEvent> m_pendingEvents;
    CollisionCallback m_eventCallback;
};
