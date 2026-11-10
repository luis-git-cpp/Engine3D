#include "Scene.h"

#include <algorithm>
#include <memory>
#include <string>

#include "raymath.h"

Scene::Scene(ObjectManager& objects, PhysicsWorld& physics)
    : m_objects(objects), m_physics(physics)
{
    m_depthMaterial = LoadMaterialDefault();
    Build();
}

Scene::~Scene()
{
    UnloadMaterial(m_depthMaterial);
}

void Scene::Reset()
{
    m_objects.DestroyAll();
    m_objects.CollectGarbage();
    m_physics.Reset(); // esquece contatos e eventos dos objetos que acabaram de sumir
    Build();
}

void Scene::Build()
{
    m_physics.SetKillPlane(-20.0f);

    // Camadas: por padrao cada camada so interage com ela mesma. Para liberar duas camadas
    // entre si, use m_physics.SetLayersInteract(0, 1, true).

    // Piso: estatico, camada 0. O colisor e uma caixa grossa logo abaixo do plano visual.
    ObjectParams p;
    p.name = "piso";
    p.tint = Color{200, 200, 200, 255};
    p.collider = ColliderShape::Box;
    p.colliderHalfExtents = {10.0f, 0.5f, 10.0f};
    p.colliderOffset = Vector3{0.0f, -0.5f, 0.0f};
    m_objects.CreatePrimitive(PrimitiveType::Plane, {20.0f, 0.0f, 20.0f}, p);

    // Cubo: estatico, camada 0 (colisor ajustado ao modelo automaticamente).
    p = ObjectParams{};
    p.name = "cubo";
    p.position = {0.0f, 1.0f, 0.0f};
    p.tint = Color{220, 60, 60, 255};
    m_objects.CreatePrimitive(PrimitiveType::Cube, {2.0f, 2.0f, 2.0f}, p);

    // Coluna: estatica, camada 0 (o colisor e uma caixa em volta do cilindro).
    p = ObjectParams{};
    p.name = "coluna";
    p.position = {-4.0f, 0.0f, -3.0f};
    p.tint = Color{90, 190, 110, 255};
    m_objects.CreatePrimitive(PrimitiveType::Cylinder, {1.2f, 3.0f, 0.0f}, p);

    // Esfera azul: dinamica, camada 0. Cai e quica no piso e no cubo.
    p = ObjectParams{};
    p.name = "esfera";
    p.position = {4.0f, 6.0f, -2.0f};
    p.tint = Color{70, 110, 230, 255};
    p.body = BodyType::Dynamic;
    p.mass = 2.0f;
    p.restitution = 0.6f;
    p.collider = ColliderShape::Sphere;
    m_objects.CreatePrimitive(PrimitiveType::Sphere, {2.0f, 0.0f, 0.0f}, p);

    // Plataforma dourada: estatica, CAMADA 1. So objetos da camada 1 batem nela.
    p = ObjectParams{};
    p.name = "plataforma (camada 1)";
    p.position = {-3.5f, 2.5f, 3.5f};
    p.tint = Color{240, 190, 60, 255};
    p.layer = 1;
    m_objects.CreatePrimitive(PrimitiveType::Cube, {4.0f, 0.3f, 4.0f}, p);

    // Duas bolas caem sobre a plataforma: a ciano (camada 1) para nela; a rosa (camada 0)
    // atravessa a plataforma e cai ate o piso.
    SpawnBall({-4.2f, 8.0f, 3.5f}, {0.0f, 0.0f, 0.0f}, 1);
    SpawnBall({-2.8f, 8.0f, 3.5f}, {0.0f, 0.0f, 0.0f}, 0);

    // Parede: estatica. Para testar a colisao do jogador e da camera.
    p = ObjectParams{};
    p.name = "parede";
    p.position = {-7.0f, 1.5f, 0.0f};
    p.tint = Color{150, 150, 170, 255};
    m_objects.CreatePrimitive(PrimitiveType::Cube, {0.5f, 3.0f, 6.0f}, p);

    // Degraus altos (0,5 / 1,0 / 1,5 m): para subir pulando.
    for (int i = 0; i < 3; i++)
    {
        const float height = 0.5f * static_cast<float>(i + 1);
        p = ObjectParams{};
        p.name = "degrau " + std::to_string(i + 1);
        p.position = {5.5f + 1.2f * static_cast<float>(i), height * 0.5f, 6.0f};
        p.tint = Color{170, 140, 200, 255};
        m_objects.CreatePrimitive(PrimitiveType::Cube, {1.2f, height, 1.2f}, p);
    }

    // Caixa dinamica: o jogador consegue empurrar (o corpo cinematico empurra os dinamicos).
    p = ObjectParams{};
    p.name = "caixa";
    p.position = {2.5f, 0.5f, 4.0f};
    p.tint = Color{160, 110, 60, 255};
    p.body = BodyType::Dynamic;
    p.mass = 4.0f;
    p.restitution = 0.1f;
    p.friction = 0.6f;
    m_objects.CreatePrimitive(PrimitiveType::Cube, {1.0f, 1.0f, 1.0f}, p);

    // Gatilho: placa no chao. O colisor (volume acima da placa) nao bloqueia ninguem, mas gera
    // eventos de colisao. O callback do proprio objeto pinta a placa de vermelho enquanto houver
    // alguem em cima (jogador, bola ou caixa).
    p = ObjectParams{};
    p.name = "placa";
    p.position = {3.0f, 0.05f, -6.0f};
    p.tint = Color{60, 200, 90, 255};
    p.isTrigger = true;
    p.collider = ColliderShape::Box;
    p.colliderHalfExtents = {0.9f, 1.0f, 0.9f};
    p.colliderOffset = Vector3{0.0f, 1.0f, 0.0f};
    const ObjectId padId = m_objects.CreatePrimitive(PrimitiveType::Cube, {1.8f, 0.1f, 1.8f}, p);

    if (GameObject* pad = m_objects.Get(padId))
    {
        auto occupants = std::make_shared<int>(0);
        pad->onCollision = [this, occupants](const CollisionEvent& e) {
            if (e.type == CollisionEventType::Stay) return;

            GameObject* self = m_objects.Get(e.self);
            if (self == nullptr) return;

            *occupants = std::max(0, *occupants + (e.type == CollisionEventType::Enter ? 1 : -1));

            const Color color = (*occupants > 0) ? Color{255, 80, 60, 255} : Color{60, 200, 90, 255};
            for (int i = 0; i < self->model.materialCount; i++)
                self->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].color = color;
        };
    }
}

ObjectId Scene::SpawnBall(Vector3 position, Vector3 velocity, int layer)
{
    ObjectParams p;
    p.name = "bola (camada " + std::to_string(layer) + ")";
    p.position = position;
    p.tint = (layer == 0) ? Color{235, 90, 170, 255} : Color{70, 200, 230, 255};
    p.body = BodyType::Dynamic;
    p.layer = layer;
    p.mass = 1.0f;
    p.restitution = 0.55f;
    p.friction = 0.4f;
    p.collider = ColliderShape::Sphere;

    const ObjectId id = m_objects.CreatePrimitive(PrimitiveType::Sphere, {1.0f, 0.0f, 0.0f}, p);
    if (GameObject* obj = m_objects.Get(id))
        obj->velocity = velocity;

    return id;
}

void Scene::Draw() const
{
    m_objects.ForEach([](const GameObject& obj) {
        if (!obj.visible) return;

        const Matrix world = obj.WorldMatrix();
        for (int i = 0; i < obj.model.meshCount; i++)
            DrawMesh(obj.model.meshes[i], obj.model.materials[obj.model.meshMaterial[i]], world);
    });
}

void Scene::DrawDepth() const
{
    m_objects.ForEach([this](const GameObject& obj) {
        if (!obj.castShadow) return; // objetos invisiveis ainda podem projetar sombra

        const Matrix world = obj.WorldMatrix();
        for (int i = 0; i < obj.model.meshCount; i++)
            DrawMesh(obj.model.meshes[i], m_depthMaterial, world);
    });
}
