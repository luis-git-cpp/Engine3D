#pragma once

#include <functional>
#include <string>

#include "Coordinates.h"
#include "Collider.h"
#include "Renderer.h"
#include "RigidBody.h"
#include "Transform.h"
#include "raylib.h"

using ObjectId = unsigned int;
constexpr ObjectId InvalidObjectId = 0;

// ---- Eventos de colisão (inalterado) ----
enum class CollisionEventType
{
    Enter, Stay, Exit
};

struct CollisionEvent
{
    CollisionEventType type = CollisionEventType::Enter;
    bool trigger = false;
    ObjectId self = InvalidObjectId;
    ObjectId other = InvalidObjectId;
    Vector3 normal{0.0f, 1.0f, 0.0f};
    float impactSpeed = 0.0f;
};

using CollisionCallback = std::function<void(const CollisionEvent&)>;

struct GameObject
{
    ObjectId id = InvalidObjectId;
    std::string name;

    // --- Componentes ---
    engine::Transform  transform;
    Renderer   renderer;
    RigidBody  rigidBody;
    Collider   collider;

    // --- Índice espacial (mantido pelo ObjectManager) ---
    CellCoord cell{};
    CellCoord cellMin{};
    CellCoord cellMax{};
    bool indexed = false;

    // --- Gatilho / eventos ---
    bool isTrigger = false;
    CollisionCallback onCollision;
    bool pendingDestroy = false;

    // ============================================================
    //  ALIASES TEMPORÁRIOS (remover quando migrar tudo para as structs)
    //  Existem para não quebrar PhysicsWorld/ObjectManager/Application.
    // ============================================================
    Vector3& position = transform.position;   // NÃO é uma cópia: é uma referência.
    Vector3& rotation = transform.rotation;
    Vector3& scale    = transform.scale;

    BodyType& body = rigidBody.body;
    int& layer = rigidBody.layer;
    Vector3& velocity = rigidBody.velocity;
    float& mass = rigidBody.mass;
    float& restitution = rigidBody.restitution;
    float& friction = rigidBody.friction;
    float& linearDamping = rigidBody.linearDamping;
    bool& grounded = rigidBody.grounded;

    Model& model = renderer.model;
    BoundingBox& localBounds = renderer.localBounds;
    BoundingBox& bounds = renderer.bounds;
    bool& visible = renderer.visible;
    bool& castShadow = renderer.castShadow;

    // ============================================================

    bool IsDynamic() const { return rigidBody.IsDynamic(); }
    bool IsKinematic() const { return rigidBody.IsKinematic(); }
    bool HasCollider() const { return collider.shape != ColliderShape::None; }
    float InverseMass() const { return rigidBody.InverseMass(); }

    Matrix WorldMatrix() const;       // escala * rotação * translação (junto com model.transform)
    Vector3 ColliderCenter() const;   // centro do colisor no mundo
    float ColliderRadius() const;     // raio no mundo (Sphere)
    BoundingBox ColliderAABB() const; // caixa do colisor no mundo
    BoundingBox WorldBounds() const;  // visual + colisor: usada no índice espacial
};