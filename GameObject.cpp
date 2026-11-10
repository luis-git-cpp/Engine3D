#include "GameObject.h"

#include <algorithm>
#include <cmath>

#include "raymath.h"

Matrix GameObject::WorldMatrix() const
{
    const Matrix s = MatrixScale(scale.x, scale.y, scale.z);
    const Matrix r = MatrixRotateXYZ({DEG2RAD * rotation.x, DEG2RAD * rotation.y, DEG2RAD * rotation.z});
    const Matrix t = MatrixTranslate(position.x, position.y, position.z);
    return MatrixMultiply(model.transform, MatrixMultiply(MatrixMultiply(s, r), t));
}

Vector3 GameObject::ColliderCenter() const
{
    return {position.x + collider.offset.x * scale.x,
            position.y + collider.offset.y * scale.y,
            position.z + collider.offset.z * scale.z};
}

float GameObject::ColliderRadius() const
{
    const float maxScale = std::max({std::fabs(scale.x), std::fabs(scale.y), std::fabs(scale.z)});
    return collider.radius * maxScale;
}

BoundingBox GameObject::ColliderAABB() const
{
    const Vector3 c = ColliderCenter();
    Vector3 h;

    if (collider.shape == ColliderShape::Sphere)
    {
        const float r = ColliderRadius();
        h = {r, r, r};
    }
    else
    {
        h = {std::fabs(collider.halfExtents.x * scale.x),
             std::fabs(collider.halfExtents.y * scale.y),
             std::fabs(collider.halfExtents.z * scale.z)};
    }

    return {Vector3Subtract(c, h), Vector3Add(c, h)};
}

BoundingBox GameObject::WorldBounds() const
{
    const Vector3 a = {localBounds.min.x * scale.x + position.x,
                       localBounds.min.y * scale.y + position.y,
                       localBounds.min.z * scale.z + position.z};
    const Vector3 b = {localBounds.max.x * scale.x + position.x,
                       localBounds.max.y * scale.y + position.y,
                       localBounds.max.z * scale.z + position.z};

    BoundingBox box = {Vector3Min(a, b), Vector3Max(a, b)};

    if (HasCollider())
    {
        const BoundingBox c = ColliderAABB();
        box.min = Vector3Min(box.min, c.min);
        box.max = Vector3Max(box.max, c.max);
    }

    return box;
}
