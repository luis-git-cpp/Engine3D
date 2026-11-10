#pragma once

#include <functional>
#include <string>

#include "Coordinates.h"
#include "raylib.h"

using ObjectId = unsigned int;
constexpr ObjectId InvalidObjectId = 0;

enum class BodyType
{
    Static,    // so colide: nunca se move
    Dynamic,   // sofre gravidade e colisoes
    Kinematic  // movido por codigo (ex.: jogador): sem gravidade nem empurroes, mas empurra os dinamicos
};

enum class ColliderShape
{
    None,   // sem colisao
    Auto,   // (so em ObjectParams) caixa ajustada ao modelo
    Box,    // caixa alinhada aos eixos (AABB)
    Sphere
};

struct Collider
{
    ColliderShape shape = ColliderShape::None;
    Vector3 halfExtents{0.5f, 0.5f, 0.5f}; // Box: metade do tamanho, antes da escala do objeto
    float radius = 0.5f;                   // Sphere: antes da escala do objeto
    Vector3 offset{0.0f, 0.0f, 0.0f};      // centro do colisor relativo a posicao do objeto
};

// ---- Eventos de colisao ----
enum class CollisionEventType
{
    Enter, // os objetos comecaram a se tocar
    Stay,  // continuam se tocando (um por quadro)
    Exit   // deixaram de se tocar (ou um deles foi destruido)
};

struct CollisionEvent
{
    CollisionEventType type = CollisionEventType::Enter;
    bool trigger = false;                 // true se um dos objetos e um gatilho (sem resposta fisica)
    ObjectId self = InvalidObjectId;      // quem recebe o evento
    ObjectId other = InvalidObjectId;     // com quem colidiu
    Vector3 normal{0.0f, 1.0f, 0.0f};     // direcao de self para other
    float impactSpeed = 0.0f;             // velocidade de aproximacao no contato, em m/s
};

using CollisionCallback = std::function<void(const CollisionEvent&)>;

// Um objeto do mundo. Quem cria e destroi e o ObjectManager.
struct GameObject
{
    ObjectId id = InvalidObjectId;
    std::string name;

    // --- Coordenadas proprias de cada objeto ---
    Vector3 position{0.0f, 0.0f, 0.0f}; // posicao no mundo
    Vector3 rotation{0.0f, 0.0f, 0.0f}; // graus (XYZ); so visual, a fisica nao rotaciona
    Vector3 scale{1.0f, 1.0f, 1.0f};
    CellCoord cell{};                   // celula que contem a posicao (mantida pelo ObjectManager)
    CellCoord cellMin{};                // faixa de celulas tocadas pelo objeto (indice espacial)
    CellCoord cellMax{};
    bool indexed = false;

    // --- Visual ---
    Model model{};
    BoundingBox localBounds{}; // caixa do modelo, sem escala nem posicao
    bool visible = true;       // desenhado na passada final
    bool castShadow = true;    // desenhado nos shadow maps (mesmo invisivel)

    // --- Fisica ---
    BodyType body = BodyType::Static;
    int layer = 0;             // so interage com objetos de camadas liberadas (ver PhysicsWorld)
    Collider collider;
    bool isTrigger = false;    // gatilho: gera eventos, mas nao bloqueia nem empurra ninguem
    Vector3 velocity{0.0f, 0.0f, 0.0f};
    float mass = 1.0f;
    float restitution = 0.3f;  // 0 = nao quica, 1 = quica sem perder energia
    float friction = 0.5f;
    float linearDamping = 0.02f;
    bool grounded = false;     // true se apoiado em algo neste passo

    // Chamado em eventos de colisao envolvendo este objeto (self = este objeto).
    CollisionCallback onCollision;

    bool pendingDestroy = false;

    bool IsDynamic() const { return body == BodyType::Dynamic; }
    bool IsKinematic() const { return body == BodyType::Kinematic; }
    bool HasCollider() const { return collider.shape != ColliderShape::None; }
    float InverseMass() const { return (IsDynamic() && mass > 0.0f) ? 1.0f / mass : 0.0f; }

    Matrix WorldMatrix() const;       // escala * rotacao * translacao (junto com model.transform)
    Vector3 ColliderCenter() const;   // centro do colisor no mundo
    float ColliderRadius() const;     // raio no mundo (Sphere)
    BoundingBox ColliderAABB() const; // caixa do colisor no mundo
    BoundingBox WorldBounds() const;  // visual + colisor: usada no indice espacial
};
