#pragma once

#include <optional>

#include "raylib.h"

// Dados visuais de um objeto: modelo + flags de render + bounds locais.
struct Renderer
{
    Model model{};
    BoundingBox localBounds{};   // caixa do modelo, sem escala nem posição
    BoundingBox bounds{};        // caixa no mundo (visual + colisor) — mantida pelo ObjectManager
    bool visible = true;         // desenhado na passada final
    bool castShadow = true;      // desenhado nos shadow maps (mesmo invisível)

    // Cor aplicada ao material difuso (se o modelo tiver materialCount > 0).
    void SetTint(Color tint);
}; 