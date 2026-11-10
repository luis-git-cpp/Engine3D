#pragma once

#include "ObjectManager.h"
#include "PhysicsWorld.h"
#include "raylib.h"

// Mundo de demonstracao e desenho dos objetos. Os objetos em si vivem no ObjectManager.
class Scene
{
public:
    Scene(ObjectManager& objects, PhysicsWorld& physics);
    ~Scene();

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // Destroi tudo e recria o mundo de demonstracao.
    void Reset();

    // Cria uma bola dinamica na camada indicada (camada 0 = rosa, camada 1 = ciano).
    ObjectId SpawnBall(Vector3 position, Vector3 velocity, int layer);

    // Passada final: materiais com o shader de iluminacao (e sombras).
    void Draw() const;
    // Passada do shadow map: mesma geometria, com o shader padrao (so profundidade importa).
    void DrawDepth() const;

private:
    void Build();

    ObjectManager& m_objects;
    PhysicsWorld& m_physics;
    Material m_depthMaterial{};
};
