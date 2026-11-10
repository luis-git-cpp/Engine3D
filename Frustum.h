#pragma once

#include <array>

#include "raylib.h"

// Frustum de visao (o "tronco de piramide" que a camera enxerga): 6 planos extraidos de uma matriz
// de visao x projecao. Serve para descartar objetos fora da tela (frustum culling), tanto para a
// camera quanto para a visao de cada luz (shadow maps). Funciona com projecao em perspectiva e
// ortografica.
class Frustum
{
public:
    Frustum() = default;

    // viewProjection = view * projection, a mesma matriz usada no shader (clip = viewProjection * posicao).
    static Frustum FromMatrix(const Matrix& viewProjection);

    // false so se a caixa estiver totalmente fora de algum plano (teste conservador: nunca
    // descarta algo que apareceria; uma caixa perto de uma quina pode passar sem estar visivel).
    bool Intersects(const BoundingBox& box) const;
    bool IntersectsSphere(Vector3 center, float radius) const;

private:
    struct Plane
    {
        Vector3 normal{0.0f, 0.0f, 0.0f}; // aponta para dentro do frustum
        float d = 0.0f;                   // dentro: dot(normal, p) + d >= 0
    };

    std::array<Plane, 6> m_planes{};
    bool m_valid = false;
};
