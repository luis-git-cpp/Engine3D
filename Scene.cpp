#include "Scene.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>

#include "raymath.h"

namespace
{
// Menor distancia entre um ponto e uma caixa (0 se o ponto estiver dentro).
float DistanceToBox(Vector3 p, const BoundingBox& b)
{
    const float dx = p.x - std::clamp(p.x, b.min.x, b.max.x);
    const float dy = p.y - std::clamp(p.y, b.min.y, b.max.y);
    const float dz = p.z - std::clamp(p.z, b.min.z, b.max.z);
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// Gerador pseudoaleatorio simples e deterministico (o mesmo mundo em toda execucao).
struct Random
{
    std::uint32_t state;
    explicit Random(std::uint32_t seed) : state(seed) {}
    float Next() // [0, 1)
    {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>(state >> 8) / 16777216.0f;
    }
    float Range(float lo, float hi) { return lo + (hi - lo) * Next(); }
};
} // namespace

Scene::Scene(ObjectManager& objects, PhysicsWorld& physics)
    : m_objects(objects), m_physics(physics), m_streamer(objects)
{
    m_depthMaterial = LoadMaterialDefault();
    Build();
    m_streamer.Prime({0.0f, 0.0f, 0.0f});
    m_streamer.SetMaxLoadsPerFrame(10);
    m_streamer.SetUnloadMargin(2);

}

Scene::~Scene()
{
    m_streamer.Clear();
    UnloadMaterial(m_depthMaterial);
}

void Scene::Reset(Vector3 focus)
{
    m_streamer.Clear();
    m_objects.DestroyAll(); // inclui os objetos criados fora do streaming (bolas, jogador)
    m_objects.CollectGarbage();
    m_physics.Reset();      // esquece contatos e eventos dos objetos que acabaram de sumir
    Build();
    m_streamer.Prime(focus);
}

void Scene::Update(Vector3 focus)
{
    m_streamer.Update(focus);
}

void Scene::Prime(Vector3 focus)
{
    m_streamer.Prime(focus);
}

// ------------------------------------------------------------------ mundo

void Scene::Build()
{
    m_physics.SetKillPlane(-20.0f);

    // Camadas: por padrao cada camada so interage com ela mesma. Para liberar duas camadas
    // entre si, use m_physics.SetLayersInteract(0, 1, true).

    // Tudo abaixo e apenas REGISTRADO: o WorldStreamer cria cada objeto quando o jogador chega
    // perto e o destroi quando ele se afasta (objetos dinamicos voltam do jeito que ficaram).
    // Helper para criar LODs de primitivas (cubo próximo / cubo distante)

    auto add = [this](PrimitiveType type, Vector3 size, const ObjectParams& params) {
        ObjectSpec spec;
        spec.kind = ObjectSpec::Kind::Primitive;
        spec.primitive = type;
        spec.size = size;
        spec.params = params;
        return m_streamer.Add(spec);
    };

    // ---- Arena central ----

    // Cubo: estatico, camada 0 (colisor ajustado ao modelo automaticamente).
    ObjectParams p;
    p.name = "cubo";
    p.position = {0.0f, 1.0f, 0.0f};
    p.tint = Color{220, 60, 60, 255};
    add(PrimitiveType::Cube, {2.0f, 2.0f, 2.0f}, p);

    // Coluna: estatica, camada 0 (o colisor e uma caixa em volta do cilindro).
    p = ObjectParams{};
    p.name = "coluna";
    p.position = {-4.0f, 0.0f, -3.0f};
    p.tint = Color{90, 190, 110, 255};
    add(PrimitiveType::Cylinder, {1.2f, 3.0f, 0.0f}, p);

    // Esfera azul: dinamica, camada 0. Cai e quica no piso e no cubo.
    p = ObjectParams{};
    p.name = "esfera";
    p.position = {4.0f, 6.0f, -2.0f};
    p.tint = Color{70, 110, 230, 255};
    p.body = BodyType::Dynamic;
    p.mass = 2.0f;
    p.restitution = 0.6f;
    p.collider = ColliderShape::Sphere;
    add(PrimitiveType::Sphere, {2.0f, 0.0f, 0.0f}, p);

    // Plataforma dourada: estatica, CAMADA 1. So objetos da camada 1 batem nela.
    p = ObjectParams{};
    p.name = "plataforma (camada 1)";
    p.position = {-3.5f, 2.5f, 3.5f};
    p.tint = Color{240, 190, 60, 255};
    p.layer = 1;
    add(PrimitiveType::Cube, {4.0f, 0.3f, 4.0f}, p);

    // Duas bolas caem sobre a plataforma: a ciano (camada 1) para nela; a rosa (camada 0)
    // atravessa a plataforma e cai ate o piso.
    for (int layer = 1; layer >= 0; layer--)
    {
        p = ObjectParams{};
        p.name = "bola (camada " + std::to_string(layer) + ")";
        p.position = {layer == 1 ? -4.2f : -2.8f, 8.0f, 3.5f};
        p.tint = (layer == 0) ? Color{235, 90, 170, 255} : Color{70, 200, 230, 255};
        p.body = BodyType::Dynamic;
        p.layer = layer;
        p.restitution = 0.55f;
        p.friction = 0.4f;
        p.collider = ColliderShape::Sphere;
        add(PrimitiveType::Sphere, {1.0f, 0.0f, 0.0f}, p);
    }

    // Parede: estatica. Para testar a colisao do jogador e da camera.
    p = ObjectParams{};
    p.name = "parede";
    p.position = {-7.0f, 1.5f, 0.0f};
    p.tint = Color{150, 150, 170, 255};
    add(PrimitiveType::Cube, {0.5f, 3.0f, 6.0f}, p);

    // Degraus altos (0,5 / 1,0 / 1,5 m): para subir pulando.
    for (int i = 0; i < 3; i++)
    {
        const float height = 0.5f * static_cast<float>(i + 1);
        p = ObjectParams{};
        p.name = "degrau " + std::to_string(i + 1);
        p.position = {5.5f + 1.2f * static_cast<float>(i), height * 0.5f, 6.0f};
        p.tint = Color{170, 140, 200, 255};
        add(PrimitiveType::Cube, {1.2f, height, 1.2f}, p);
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
    add(PrimitiveType::Cube, {1.0f, 1.0f, 1.0f}, p);

    // Gatilho: placa no chao. O colisor (volume acima da placa) nao bloqueia ninguem, mas gera
    // eventos de colisao. O callback do proprio objeto pinta a placa de vermelho enquanto houver
    // alguem em cima (jogador, bola ou caixa). O onCreate religa o callback sempre que a placa
    // e recriada pelo streaming.
    {
        ObjectSpec spec;
        spec.kind = ObjectSpec::Kind::Primitive;
        spec.primitive = PrimitiveType::Cube;
        spec.size = {1.8f, 0.1f, 1.8f};

        spec.params.name = "placa";
        spec.params.position = {3.0f, 0.05f, -6.0f};
        spec.params.tint = Color{60, 200, 90, 255};
        spec.params.isTrigger = true;
        spec.params.collider = ColliderShape::Box;
        spec.params.colliderHalfExtents = {0.9f, 1.0f, 0.9f};
        spec.params.colliderOffset = Vector3{0.0f, 1.0f, 0.0f};

        auto occupants = std::make_shared<int>(0);
        spec.onCreate = [this, occupants](GameObject& pad) {
            *occupants = 0; // recriada: ninguem esta em cima ainda
            pad.onCollision = [this, occupants](const CollisionEvent& e) {
                if (e.type == CollisionEventType::Stay) return;

                GameObject* self = m_objects.Get(e.self);
                if (self == nullptr) return;

                *occupants = std::max(0, *occupants + (e.type == CollisionEventType::Enter ? 1 : -1));

                const Color color = (*occupants > 0) ? Color{255, 80, 60, 255} : Color{60, 200, 90, 255};
                for (int i = 0; i < self->model.materialCount; i++)
                    self->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].color = color;
            };
        };
        m_streamer.Add(spec);
    }

    // ---- Mundo grande: 9 x 9 pisos de 20 m (180 x 180 m) com objetos espalhados ----
    // O piso do meio e o da arena. Os pisos tem colisor um pouco maior para nao haver frestas.
    const Color tileColors[2] = {Color{200, 200, 200, 255}, Color{165, 165, 175, 255}};
    const Color propColors[6] = {Color{210, 90, 80, 255},  Color{90, 170, 120, 255}, Color{100, 130, 210, 255},
                                 Color{220, 180, 80, 255}, Color{170, 110, 200, 255}, Color{90, 190, 200, 255}};

    for (int i = -4; i <= 4; i++)
    {
        for (int j = -4; j <= 4; j++)
        {
            const float cx = static_cast<float>(i) * 20.0f;
            const float cz = static_cast<float>(j) * 20.0f;

            p = ObjectParams{};
            p.name = "piso";
            p.position = {cx, 0.0f, cz};
            p.tint = tileColors[((i + j) % 2 == 0) ? 0 : 1];
            p.collider = ColliderShape::Box;
            p.colliderHalfExtents = {10.02f, 0.5f, 10.02f};
            p.colliderOffset = Vector3{0.0f, -0.5f, 0.0f};
            add(PrimitiveType::Plane, {20.0f, 0.0f, 20.0f}, p);

            if (i == 0 && j == 0) continue; // a arena central ja tem os seus objetos

            // Objetos espalhados (sempre os mesmos: a semente depende do piso).
            Random rng(static_cast<std::uint32_t>(7919 * (i + 10) + 104729 * (j + 10) + 12345));
            const int count = 2 + static_cast<int>(rng.Next() * 2.99f);

            for (int k = 0; k < count; k++)
            {
                const float x = cx + rng.Range(-8.0f, 8.0f);
                const float z = cz + rng.Range(-8.0f, 8.0f);
                const float kind = rng.Next();
                const Color color = propColors[static_cast<int>(rng.Next() * 5.99f)];

                p = ObjectParams{};
                p.tint = color;

                if (kind < 0.40f)
                {
                    const float s = rng.Range(1.0f, 3.0f);
                }
                else if (kind < 0.75f)
                {
                    const float height = rng.Range(2.0f, 6.0f);
                    p.name = "pilar";
                    p.position = {x, 0.0f, z};
                    add(PrimitiveType::Cylinder, {rng.Range(0.8f, 1.6f), height, 0.0f}, p);
                }
                else
                {
                    p.name = "caixa";
                    p.position = {x, 0.5f, z};
                    p.body = BodyType::Dynamic;
                    p.mass = 4.0f;
                    p.restitution = 0.1f;
                    p.friction = 0.6f;
                    add(PrimitiveType::Cube, {1.0f, 1.0f, 1.0f}, p);
                }
            }
        }
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

// ------------------------------------------------------------------ desenho

void Scene::Draw(const Frustum& frustum, Vector3 eye, float maxDistance) const
{
    int drawn = 0;

    m_objects.ForEach([&](const GameObject& obj) {
        if (!obj.visible) return;

        // Culling: fora do campo de visao ou longe demais, nem chega na GPU.
        if (!frustum.Intersects(obj.bounds)) return;
        if (DistanceToBox(eye, obj.bounds) > maxDistance) return;

        const Matrix world = obj.WorldMatrix();
        for (int i = 0; i < obj.model.meshCount; i++)
            DrawMesh(obj.model.meshes[i], obj.model.materials[obj.model.meshMaterial[i]], world);

        drawn++;
    });

    m_drawnCount = drawn;
}

void Scene::DrawDepth(const Frustum& frustum) const
{
    m_objects.ForEach([&](const GameObject& obj) {
        if (!obj.castShadow) return; // objetos invisiveis ainda podem projetar sombra
        if (!frustum.Intersects(obj.bounds)) return; // fora da visao da luz: nao entra no shadow map

        const Matrix world = obj.WorldMatrix();
        for (int i = 0; i < obj.model.meshCount; i++)
            DrawMesh(obj.model.meshes[i], m_depthMaterial, world);
    });
}

void Scene::DrawStreamingDebug(Vector3 focus) const
{
    m_streamer.DrawDebug(focus);
}
