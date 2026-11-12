#pragma once

#include "raylib.h"

enum class LightType
{
    Directional = 0, // "sol": raios paralelos, sem posicao
    Point       = 1, // lampada: irradia em todas as direcoes a partir de uma posicao
    Spot        = 2  // lanterna: ponto de luz restrito a um cone
};

struct Light
{
    LightType type = LightType::Point;
    bool enabled = true;

    Vector3 position{0.0f, 0.0f, 0.0f};  // Point e Spot
    Vector3 direction{0.0f, -1.0f, 0.0f}; // Directional e Spot: para onde a luz viaja

    Color color = WHITE;
    float intensity = 1.0f;

    float range = 10.0f;       // Point e Spot: distancia em que a luz chega a zero
    float innerAngle = 15.0f;  // Spot: meio-angulo (graus) com brilho total
    float outerAngle = 25.0f;  // Spot: meio-angulo (graus) onde o brilho chega a zero

    // Directional e Spot projetam sombra por padrao (ate LightManager::MaxShadowMaps luzes ao mesmo
    // tempo). Point nunca projeta. Coloque false para economizar uma passada de profundidade.
    bool castShadows = true;
};
