#pragma once

#include "raylib.h"

enum class BodyType
{
    Static,    // só colide: nunca se move
    Dynamic,   // sofre gravidade e colisões
    Kinematic  // movido por código: sem gravidade nem empurrões, mas empurra os dinâmicos
};

// Dados físicos de um corpo rígido.
// A física linear (posição/velocidade/massa) é usada pelo solver atual.
// A física angular (orientation/angularVelocity/inertia) está preparada mas o solver
// ainda não aplica torque — vem numa próxima etapa.
struct RigidBody
{
    BodyType body = BodyType::Static;
    int layer = 0;

    // --- Linear ---
    Vector3 velocity{0.0f, 0.0f, 0.0f};
    float mass = 1.0f;
    float restitution = 0.3f;      // 0 = não quica, 1 = quica sem perder energia
    float friction = 0.5f;
    float linearDamping = 0.02f;
    bool grounded = false;

    // --- Angular (reservado — ainda não usado pelo solver) ---
    Vector3 angularVelocity{0.0f, 0.0f, 0.0f};   // rad/s
    float angularDamping = 0.05f;
    // Tensor de inércia diagonal (kg·m²), para caixas/esferas.
    Vector3 inertia{1.0f, 1.0f, 1.0f};
    // Inverso do tensor (cache). (0,0,0) = estático/infinito.
    Vector3 inverseInertia{0.0f, 0.0f, 0.0f};

    bool IsDynamic() const { return body == BodyType::Dynamic; }
    bool IsKinematic() const { return body == BodyType::Kinematic; }
    float InverseMass() const { return (IsDynamic() && mass > 0.0f) ? 1.0f / mass : 0.0f; }

    // Recalcula o tensor de inércia a partir da forma e do tamanho do colisor.
    // Chamado quando o colisor é definido (Box/Sphere). Para AABB usa a caixa.
    void UpdateInertiaFromCollider(const struct Collider& collider, Vector3 scale);
};