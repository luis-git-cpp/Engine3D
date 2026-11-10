#include "Frustum.h"

#include <cmath>

Frustum Frustum::FromMatrix(const Matrix& m)
{
    // Linhas da matriz (o raylib guarda em ordem de colunas: m0..m3 e a primeira coluna).
    const float r0[4] = {m.m0, m.m4, m.m8,  m.m12};
    const float r1[4] = {m.m1, m.m5, m.m9,  m.m13};
    const float r2[4] = {m.m2, m.m6, m.m10, m.m14};
    const float r3[4] = {m.m3, m.m7, m.m11, m.m15};

    // Metodo de Gribb-Hartmann: um ponto esta dentro quando -w <= x, y, z <= w (coordenadas de
    // clip). Cada desigualdade vira um plano: linha3 +/- linha0 (esquerda/direita), linha1
    // (baixo/cima) e linha2 (perto/longe).
    const float signs[6][2] = {
        {+1.0f, 0}, {-1.0f, 0}, // esquerda, direita  (linha 0)
        {+1.0f, 1}, {-1.0f, 1}, // baixo, cima        (linha 1)
        {+1.0f, 2}, {-1.0f, 2}  // perto, longe       (linha 2)
    };
    const float* rows[3] = {r0, r1, r2};

    Frustum f;
    for (int i = 0; i < 6; i++)
    {
        const float sign = signs[i][0];
        const float* row = rows[static_cast<int>(signs[i][1])];

        const float a = r3[0] + sign * row[0];
        const float b = r3[1] + sign * row[1];
        const float c = r3[2] + sign * row[2];
        const float d = r3[3] + sign * row[3];

        const float length = std::sqrt(a * a + b * b + c * c);
        if (length < 1e-8f) continue;

        f.m_planes[i].normal = {a / length, b / length, c / length};
        f.m_planes[i].d = d / length;
    }

    f.m_valid = true;
    return f;
}

bool Frustum::Intersects(const BoundingBox& box) const
{
    if (!m_valid) return true; // sem frustum valido: nao descarta nada

    for (const Plane& p : m_planes)
    {
        // Canto da caixa mais a frente na direcao da normal ("p-vertex"): se ate ele esta fora, a caixa toda esta.
        const float x = p.normal.x >= 0.0f ? box.max.x : box.min.x;
        const float y = p.normal.y >= 0.0f ? box.max.y : box.min.y;
        const float z = p.normal.z >= 0.0f ? box.max.z : box.min.z;

        if (p.normal.x * x + p.normal.y * y + p.normal.z * z + p.d < 0.0f)
            return false;
    }
    return true;
}

bool Frustum::IntersectsSphere(Vector3 center, float radius) const
{
    if (!m_valid) return true;

    for (const Plane& p : m_planes)
    {
        if (p.normal.x * center.x + p.normal.y * center.y + p.normal.z * center.z + p.d < -radius)
            return false;
    }
    return true;
}
