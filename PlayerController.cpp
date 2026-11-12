#include "PlayerController.h"

#include <algorithm>
#include <cmath>

#include "raymath.h"

namespace
{
constexpr float kBodyRadius = 0.3f;
constexpr float kBodyHeight = 1.8f;
constexpr float kEyeHeight = 1.62f;

constexpr float kLookSensitivity = 0.0025f; // radianos por pixel
constexpr float kMaxPitch = 1.50f;

constexpr float kWalkSpeed = 4.5f;
constexpr float kRunSpeed = 7.5f;
constexpr float kJumpSpeed = 5.0f;
constexpr float kGroundAccel = 40.0f;
constexpr float kAirAccel = 10.0f;
constexpr float kMaxFallSpeed = 40.0f;
constexpr float kRespawnHeight = -20.0f;

// Camera de 3a pessoa
constexpr float kThirdPersonDistance = 4.0f;
constexpr float kPivotHeight = 1.5f;      // altura do ponto que a camera orbita
constexpr float kCameraRadius = 0.25f;    // espessura da camera nos testes de colisao
constexpr float kCameraMinDistance = 0.35f;
constexpr float kCameraRecoverRate = 6.0f; // velocidade com que a camera se afasta de novo

float Approach(float current, float target, float maxDelta)
{
    if (current < target) return std::min(current + maxDelta, target);
    return std::max(current - maxDelta, target);
}
} // namespace

PlayerController::PlayerController(ObjectManager& objects, PhysicsWorld& physics)
    : m_objects(objects), m_physics(physics), m_cameraDistance(kThirdPersonDistance)
{
}

void PlayerController::Spawn(Vector3 feet, float yawDegrees)
{
    m_objects.Destroy(m_id); // ignora se o id for invalido ou ja destruido

    // O cilindro tem a base na origem do objeto, entao a posicao do objeto = posicao dos pes.
    // O colisor (caixa) e ajustado automaticamente ao modelo.
    ObjectParams p;
    p.name = "jogador";
    p.position = feet;
    p.tint = Color{235, 235, 245, 255};
    p.body = BodyType::Kinematic;
    p.layer = m_layer;
    p.collider = ColliderShape::Box;

    m_id = m_objects.CreatePrimitive(PrimitiveType::Cylinder, {kBodyRadius * 2.0f, kBodyHeight, 0.0f}, p);

    m_spawn = feet;
    m_yaw = yawDegrees * DEG2RAD;
    m_pitch = 0.0f;
    m_cameraDistance = kThirdPersonDistance;
}

void PlayerController::SetLayer(int layer)
{
    m_layer = layer;
    if (GameObject* body = m_objects.Get(m_id))
        body->layer = layer;
}

void PlayerController::SetVisible(bool visible)
{
    if (GameObject* body = m_objects.Get(m_id))
        body->visible = visible;
}

void PlayerController::LookAt(Vector3 direction)
{
    const Vector3 d = Vector3Normalize(direction);
    m_yaw = std::atan2(d.x, -d.z);
    m_pitch = std::asin(std::clamp(d.y, -1.0f, 1.0f));
}

bool PlayerController::Update(float dt, bool lookEnabled)
{
    GameObject* body = m_objects.Get(m_id);
    if (body == nullptr) return false;

    // Olhar com o mouse.
    if (lookEnabled)
    {
        const Vector2 mouse = GetMouseDelta();
        m_yaw += mouse.x * kLookSensitivity;
        m_pitch = std::clamp(m_pitch - mouse.y * kLookSensitivity, -kMaxPitch, kMaxPitch);
    }

    // Direcao desejada, relativa ao olhar (so no plano horizontal).
    const Vector3 forward = {std::sin(m_yaw), 0.0f, -std::cos(m_yaw)};
    const Vector3 right = {std::cos(m_yaw), 0.0f, std::sin(m_yaw)};

    Vector3 wish = {0.0f, 0.0f, 0.0f};
    if (IsKeyDown(KEY_W)) wish = Vector3Add(wish, forward);
    if (IsKeyDown(KEY_S)) wish = Vector3Subtract(wish, forward);
    if (IsKeyDown(KEY_D)) wish = Vector3Add(wish, right);
    if (IsKeyDown(KEY_A)) wish = Vector3Subtract(wish, right);
    if (Vector3Length(wish) > 0.0f) wish = Vector3Normalize(wish);

    const float speed = IsKeyDown(KEY_LEFT_SHIFT) ? kRunSpeed : kWalkSpeed;
    const Vector3 target = Vector3Scale(wish, speed);

    // Acelera ate a velocidade desejada (menos controle no ar).
    const float accel = (body->grounded ? kGroundAccel : kAirAccel) * dt;
    body->velocity.x = Approach(body->velocity.x, target.x, accel);
    body->velocity.z = Approach(body->velocity.z, target.z, accel);

    // Gravidade e pulo (so com o pe no chao).
    body->velocity.y = std::max(body->velocity.y + m_physics.Gravity().y * dt, -kMaxFallSpeed);
    if (body->grounded && IsKeyPressed(KEY_SPACE))
        body->velocity.y = kJumpSpeed;

    // Move com colisao: desliza em paredes, apoia no chao e atualiza body->grounded.
    m_physics.MoveKinematic(*body, Vector3Scale(body->velocity, dt));

    // Caiu para fora do mundo: volta ao ponto inicial.
    if (body->position.y < kRespawnHeight)
    {
        Spawn(m_spawn, m_yaw * RAD2DEG);
        return true;
    }

    return false;
}

Camera3D PlayerController::BuildCamera(bool thirdPerson, float dt)
{
    Camera3D cam{};
    cam.up = {0.0f, 1.0f, 0.0f};
    cam.fovy = 60.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    GameObject* body = m_objects.Get(m_id);
    if (body == nullptr)
    {
        cam.position = {0.0f, kEyeHeight, 0.0f};
        cam.target = {0.0f, kEyeHeight, -1.0f};
        return cam;
    }

    const Vector3 forward = Forward();

    // 1a pessoa: a camera fica nos olhos e o corpo nao e desenhado (a sombra continua).
    if (!thirdPerson)
    {
        const Vector3 eye = EyePosition();
        cam.position = eye;
        cam.target = Vector3Add(eye, forward);
        m_cameraDistance = kThirdPersonDistance;
        body->visible = false;
        return cam;
    }

    // 3a pessoa: a camera fica atras do jogador, olhando na mesma direcao. Um "sphere cast" do
    // pivo ate a posicao desejada descobre o que esta no caminho, e a camera para antes disso.
    const Vector3 pivot = Vector3Add(body->position, {0.0f, kPivotHeight, 0.0f});
    const Vector3 back = Vector3Negate(forward);

    QueryFilter filter;
    filter.layerMask = m_physics.LayerMask(body->layer); // colide com o que o jogador colide
    filter.ignore = m_id;
    filter.includeDynamic = false;                       // objetos soltos nao empurram a camera

    float desired = kThirdPersonDistance;
    RaycastHit hit;
    if (m_physics.Raycast(pivot, back, kThirdPersonDistance, hit, filter, kCameraRadius))
        desired = std::max(hit.distance, kCameraMinDistance);

    // Encosta na parede na hora, mas volta devagar (evita "pulos" ao passar por quinas).
    if (desired < m_cameraDistance)
        m_cameraDistance = desired;
    else
        m_cameraDistance += (desired - m_cameraDistance) * std::min(1.0f, dt * kCameraRecoverRate);

    cam.position = Vector3Add(pivot, Vector3Scale(back, m_cameraDistance));
    cam.target = Vector3Add(cam.position, forward);

    // Camera colada no jogador: esconde o corpo para nao ver por dentro dele.
    body->visible = m_cameraDistance > 0.9f;
    return cam;
}

Vector3 PlayerController::Position() const
{
    const GameObject* body = m_objects.Get(m_id);
    return body != nullptr ? body->position : m_spawn;
}

Vector3 PlayerController::EyePosition() const
{
    return Vector3Add(Position(), {0.0f, kEyeHeight, 0.0f});
}

Vector3 PlayerController::Forward() const
{
    return {std::sin(m_yaw) * std::cos(m_pitch), std::sin(m_pitch), -std::cos(m_yaw) * std::cos(m_pitch)};
}

bool PlayerController::Grounded() const
{
    const GameObject* body = m_objects.Get(m_id);
    return body != nullptr && body->grounded;
}
