#pragma once

#include "GameObject.h"
#include "raylib.h"

// Resultado de um teste de colisao entre dois colisores A e B.
struct Contact
{
    bool hit = false;
    Vector3 normal{0.0f, 1.0f, 0.0f}; // direcao de A para B
    float penetration = 0.0f;         // quanto um entrou no outro (distancia minima para separar)
};

Contact TestSphereSphere(Vector3 centerA, float radiusA, Vector3 centerB, float radiusB);
Contact TestBoxBox(const BoundingBox& a, const BoundingBox& b);
// Normal: da esfera para a caixa.
Contact TestSphereBox(Vector3 center, float radius, const BoundingBox& box);

// Escolhe o teste certo pelos formatos dos dois colisores. A normal sempre vai de a para b.
Contact TestColliders(const GameObject& a, const GameObject& b);

// ---- Raios ----
// Raio contra caixa. "direction" deve estar normalizada. Se a origem estiver dentro da caixa,
// devolve distancia 0. "normal" e a normal da face atingida.
bool RayVsBox(Vector3 origin, Vector3 direction, const BoundingBox& box, float maxDistance,
              float& distance, Vector3& normal);

// Raio contra esfera (mesmas regras).
bool RayVsSphere(Vector3 origin, Vector3 direction, Vector3 center, float radius, float maxDistance,
                 float& distance, Vector3& normal);
