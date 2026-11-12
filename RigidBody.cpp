#include "RigidBody.h"

#include "Collider.h"   // ← declarado abaixo (vou reordenar os includes depois)

#include <algorithm>
#include <cmath>

void RigidBody::UpdateInertiaFromCollider(const Collider& c, Vector3 scale)
{
    if (!IsDynamic())
    {
        inertia = {1.0f, 1.0f, 1.0f};
        inverseInertia = {0.0f, 0.0f, 0.0f};
        return;
    }

    const float m = mass > 0.0f ? mass : 1.0f;

    if (c.shape == ColliderShape::Sphere)
    {
        const float r = c.radius * std::max({std::fabs(scale.x), std::fabs(scale.y), std::fabs(scale.z)});
        const float i = (2.0f / 5.0f) * m * r * r;
        inertia = {i, i, i};
    }
    else if (c.shape == ColliderShape::Box)
    {
        const Vector3 h = {c.halfExtents.x * std::fabs(scale.x),
                           c.halfExtents.y * std::fabs(scale.y),
                           c.halfExtents.z * std::fabs(scale.z)};
        const float ix = (m / 3.0f) * (h.y * h.y + h.z * h.z);
        const float iy = (m / 3.0f) * (h.x * h.x + h.z * h.z);
        const float iz = (m / 3.0f) * (h.x * h.x + h.y * h.y);
        inertia = {ix, iy, iz};
    }
    else
    {
        inertia = {1.0f, 1.0f, 1.0f};
    }

    inverseInertia = {inertia.x > 1e-8f ? 1.0f / inertia.x : 0.0f,
                      inertia.y > 1e-8f ? 1.0f / inertia.y : 0.0f,
                      inertia.z > 1e-8f ? 1.0f / inertia.z : 0.0f};
}