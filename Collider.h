#pragma once

#include "raylib.h"

enum class ColliderShape
{
    None,   // sem colisão
    Auto,   // (só em ObjectParams) caixa ajustada ao modelo
    Box,    // caixa alinhada aos eixos (AABB)
    Sphere
};

struct Collider
{
    ColliderShape shape = ColliderShape::None;
    Vector3 halfExtents{0.5f, 0.5f, 0.5f}; // Box: metade do tamanho, antes da escala do objeto
    float radius = 0.5f;                   // Sphere: antes da escala do objeto
    Vector3 offset{0.0f, 0.0f, 0.0f};      // centro do colisor relativo à posição do objeto
};