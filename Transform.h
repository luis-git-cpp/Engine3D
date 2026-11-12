#pragma once

#include "raylib.h"
#include "raymath.h"

// Posição, rotação e escala de um objeto no mundo.
// - position: metros
// - rotation: graus (XYZ) — mantido por compatibilidade com o código atual.
// - orientation: quaternion (rotação "de verdade" para física com torque).
//                  Se você setar rotation (graus), a orientation NÃO é atualizada automaticamente
//                  por enquanto (isso vem na etapa de física com quaternions).
// - scale: multiplicador aplicado ao modelo (não afeta física além do colisor).
namespace engine
{
struct Transform
{
    Vector3 position{0.0f, 0.0f, 0.0f};
    Vector3 rotation{0.0f, 0.0f, 0.0f};       // graus XYZ (visual, compatibilidade)
    Quaternion orientation{0, 0, 0, 1};       // identidade — reservado para física angular
    Vector3 scale{1.0f, 1.0f, 1.0f};

    // Matriz de mundo: scale * rotation * translation.
    // Usa `rotation` (graus) por enquanto; quando o solver angular entrar,
    // troque para usar `orientation` (QuaternionToMatrix).
    Matrix ToMatrix(const Matrix& modelLocal = MatrixIdentity()) const;
};
}