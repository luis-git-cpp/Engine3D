#include "Collision.h"

#include <algorithm>
#include <cmath>

#include "raymath.h"

namespace
{
Vector3 Center(const BoundingBox& b)
{
    return {(b.min.x + b.max.x) * 0.5f, (b.min.y + b.max.y) * 0.5f, (b.min.z + b.max.z) * 0.5f};
}

float Component(const Vector3& v, int axis)
{
    return axis == 0 ? v.x : (axis == 1 ? v.y : v.z);
}

Vector3 AxisVector(int axis, float sign)
{
    return {axis == 0 ? sign : 0.0f, axis == 1 ? sign : 0.0f, axis == 2 ? sign : 0.0f};
}
} // namespace

Contact TestSphereSphere(Vector3 centerA, float radiusA, Vector3 centerB, float radiusB)
{
    Contact c;

    const Vector3 d = Vector3Subtract(centerB, centerA);
    const float sum = radiusA + radiusB;
    const float dist2 = Vector3DotProduct(d, d);
    if (dist2 >= sum * sum) return c;

    const float dist = std::sqrt(dist2);
    c.hit = true;
    c.penetration = sum - dist;
    c.normal = dist > 1e-6f ? Vector3Scale(d, 1.0f / dist) : Vector3{0.0f, 1.0f, 0.0f};
    return c;
}

Contact TestBoxBox(const BoundingBox& a, const BoundingBox& b)
{
    Contact c;

    // Sobreposicao em cada eixo; a colisao so existe se houver nos tres.
    const float ox = std::min(a.max.x, b.max.x) - std::max(a.min.x, b.min.x);
    const float oy = std::min(a.max.y, b.max.y) - std::max(a.min.y, b.min.y);
    const float oz = std::min(a.max.z, b.max.z) - std::max(a.min.z, b.min.z);
    if (ox <= 0.0f || oy <= 0.0f || oz <= 0.0f) return c;

    const Vector3 ca = Center(a);
    const Vector3 cb = Center(b);

    // Separa pelo eixo de menor sobreposicao.
    c.hit = true;
    if (ox <= oy && ox <= oz)
    {
        c.penetration = ox;
        c.normal = {cb.x >= ca.x ? 1.0f : -1.0f, 0.0f, 0.0f};
    }
    else if (oy <= oz)
    {
        c.penetration = oy;
        c.normal = {0.0f, cb.y >= ca.y ? 1.0f : -1.0f, 0.0f};
    }
    else
    {
        c.penetration = oz;
        c.normal = {0.0f, 0.0f, cb.z >= ca.z ? 1.0f : -1.0f};
    }
    return c;
}

Contact TestSphereBox(Vector3 center, float radius, const BoundingBox& box)
{
    Contact res;

    // Ponto da caixa mais proximo do centro da esfera.
    const Vector3 closest = {std::clamp(center.x, box.min.x, box.max.x),
                             std::clamp(center.y, box.min.y, box.max.y),
                             std::clamp(center.z, box.min.z, box.max.z)};

    const Vector3 d = Vector3Subtract(center, closest); // da caixa para a esfera
    const float dist2 = Vector3DotProduct(d, d);
    if (dist2 > radius * radius) return res;

    if (dist2 > 1e-8f)
    {
        const float dist = std::sqrt(dist2);
        res.hit = true;
        res.penetration = radius - dist;
        res.normal = Vector3Scale(d, -1.0f / dist); // da esfera para a caixa
        return res;
    }

    // Centro dentro da caixa: sai pela face mais proxima.
    struct Face
    {
        float dist;
        Vector3 outward;
    };
    const Face faces[6] = {
        {center.x - box.min.x, {-1.0f, 0.0f, 0.0f}}, {box.max.x - center.x, {1.0f, 0.0f, 0.0f}},
        {center.y - box.min.y, {0.0f, -1.0f, 0.0f}}, {box.max.y - center.y, {0.0f, 1.0f, 0.0f}},
        {center.z - box.min.z, {0.0f, 0.0f, -1.0f}}, {box.max.z - center.z, {0.0f, 0.0f, 1.0f}},
    };

    const Face* best = &faces[0];
    for (const Face& f : faces)
    {
        if (f.dist < best->dist) best = &f;
    }

    res.hit = true;
    res.penetration = best->dist + radius;
    res.normal = Vector3Negate(best->outward);
    return res;
}

Contact TestColliders(const GameObject& a, const GameObject& b)
{
    const bool sphereA = a.collider.shape == ColliderShape::Sphere;
    const bool sphereB = b.collider.shape == ColliderShape::Sphere;

    if (sphereA && sphereB)
        return TestSphereSphere(a.ColliderCenter(), a.ColliderRadius(), b.ColliderCenter(), b.ColliderRadius());

    if (sphereA)
        return TestSphereBox(a.ColliderCenter(), a.ColliderRadius(), b.ColliderAABB());

    if (sphereB)
    {
        Contact c = TestSphereBox(b.ColliderCenter(), b.ColliderRadius(), a.ColliderAABB());
        c.normal = Vector3Negate(c.normal); // estava de b para a
        return c;
    }

    return TestBoxBox(a.ColliderAABB(), b.ColliderAABB());
}

bool RayVsBox(Vector3 origin, Vector3 direction, const BoundingBox& box, float maxDistance,
              float& distance, Vector3& normal)
{
    // Origem dentro da caixa: acerto imediato.
    const bool inside = origin.x >= box.min.x && origin.x <= box.max.x &&
                        origin.y >= box.min.y && origin.y <= box.max.y &&
                        origin.z >= box.min.z && origin.z <= box.max.z;
    if (inside)
    {
        distance = 0.0f;
        normal = Vector3Negate(direction);
        return true;
    }

    // Metodo das "placas" (slabs): intervalo de t em que o raio esta dentro de cada par de planos.
    float tMin = 0.0f;
    float tMax = maxDistance;
    Vector3 hitNormal = {0.0f, 0.0f, 0.0f};

    for (int axis = 0; axis < 3; axis++)
    {
        const float o = Component(origin, axis);
        const float d = Component(direction, axis);
        const float lo = Component(box.min, axis);
        const float hi = Component(box.max, axis);

        if (std::fabs(d) < 1e-8f)
        {
            if (o < lo || o > hi) return false; // paralelo e fora da placa
            continue;
        }

        float t1 = (lo - o) / d;
        float t2 = (hi - o) / d;
        float sign = -1.0f; // entrando pela face minima: a normal aponta para -eixo

        if (t1 > t2)
        {
            std::swap(t1, t2);
            sign = 1.0f;    // entrando pela face maxima: a normal aponta para +eixo
        }

        if (t1 > tMin)
        {
            tMin = t1;
            hitNormal = AxisVector(axis, sign);
        }
        tMax = std::min(tMax, t2);

        if (tMin > tMax) return false;
    }

    distance = tMin;
    normal = hitNormal;
    return true;
}

bool RayVsSphere(Vector3 origin, Vector3 direction, Vector3 center, float radius, float maxDistance,
                 float& distance, Vector3& normal)
{
    const Vector3 oc = Vector3Subtract(origin, center);
    const float b = Vector3DotProduct(oc, direction);
    const float c = Vector3DotProduct(oc, oc) - radius * radius;

    if (c <= 0.0f) // origem dentro da esfera
    {
        distance = 0.0f;
        normal = Vector3Negate(direction);
        return true;
    }

    if (b > 0.0f) return false; // fora e apontando para longe

    const float discriminant = b * b - c;
    if (discriminant < 0.0f) return false;

    const float t = -b - std::sqrt(discriminant);
    if (t > maxDistance) return false;

    distance = t;
    normal = Vector3Normalize(Vector3Subtract(Vector3Add(origin, Vector3Scale(direction, t)), center));
    return true;
}
