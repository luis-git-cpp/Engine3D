#include "PhysicsWorld.h"

#include <algorithm>
#include <cmath>

#include "raymath.h"

namespace
{
constexpr float kFixedDt = 1.0f / 120.0f;      // passo fixo da simulacao
constexpr int kMaxStepsPerFrame = 8;           // evita a "espiral da morte" em quadros lentos
constexpr int kSolverIterations = 3;
constexpr float kPenetrationSlop = 0.002f;     // tolerancia para nao tremer em repouso
constexpr float kRestitutionThreshold = 1.0f;  // abaixo desta velocidade de impacto nao quica
constexpr float kMaxSpeed = 60.0f;

constexpr int kKinematicIterations = 4;
constexpr float kContactSkin = 0.003f;         // o corpo cinematico fica "encostado" (sobreposicao minima)
constexpr int kExitGraceSteps = 4;             // passos sem contato antes de dar o evento Exit

Color LayerColor(int layer)
{
    static const Color palette[8] = {GREEN, ORANGE, SKYBLUE, PINK, YELLOW, VIOLET, LIME, RED};
    return palette[((layer % 8) + 8) % 8];
}

bool PassesFilter(const QueryFilter& filter, const GameObject& obj)
{
    if (!obj.HasCollider() || obj.id == filter.ignore) return false;
    if (obj.isTrigger && !filter.includeTriggers) return false;
    if (obj.layer < 0 || obj.layer >= PhysicsWorld::MaxLayers) return false;
    if ((filter.layerMask & (1u << obj.layer)) == 0) return false;
    if (obj.IsDynamic() ? !filter.includeDynamic : !filter.includeStatic) return false;
    return true;
}
} // namespace

PhysicsWorld::PhysicsWorld(ObjectManager& objects)
    : m_objects(objects)
{
    // Cada camada interage so com ela mesma.
    for (int i = 0; i < MaxLayers; i++)
        m_layerMask[i] = 1u << i;
}

// ------------------------------------------------------------------ camadas

void PhysicsWorld::SetLayersInteract(int layerA, int layerB, bool interact)
{
    if (layerA < 0 || layerA >= MaxLayers || layerB < 0 || layerB >= MaxLayers) return;

    if (interact)
    {
        m_layerMask[layerA] |= (1u << layerB);
        m_layerMask[layerB] |= (1u << layerA);
    }
    else
    {
        m_layerMask[layerA] &= ~(1u << layerB);
        m_layerMask[layerB] &= ~(1u << layerA);
    }
}

bool PhysicsWorld::LayersInteract(int layerA, int layerB) const
{
    if (layerA < 0 || layerA >= MaxLayers || layerB < 0 || layerB >= MaxLayers) return false;
    return (m_layerMask[layerA] & (1u << layerB)) != 0;
}

std::uint32_t PhysicsWorld::LayerMask(int layer) const
{
    if (layer < 0 || layer >= MaxLayers) return 0;
    return m_layerMask[layer];
}

// ------------------------------------------------------------------ simulacao

void PhysicsWorld::Reset()
{
    m_accumulator = 0.0f;
    m_touchedNow.clear();
    m_pairs.clear();
    m_enteredThisFrame.clear();
    m_pendingEvents.clear();
}

void PhysicsWorld::Step(float frameTime)
{
    m_accumulator += std::min(frameTime, 0.1f);
    m_enteredThisFrame.clear();

    int steps = 0;
    while (m_accumulator >= kFixedDt && steps < kMaxStepsPerFrame)
    {
        FixedStep(kFixedDt);
        m_accumulator -= kFixedDt;
        steps++;
    }
    if (steps == kMaxStepsPerFrame) m_accumulator = 0.0f;

    // Eventos: Enter/Exit ja foram enfileirados nos passos; Stay sai uma vez por quadro.
    // Os callbacks rodam so aqui, fora de qualquer iteracao, e podem criar ou destruir objetos.
    EmitStayEvents();
    DispatchEvents();

    m_objects.UpdateSpatialIndex();

    // Objetos que cairam para fora do mundo sao destruidos (liberados em CollectGarbage).
    m_objects.ForEach([&](GameObject& obj) {
        if (obj.IsDynamic() && obj.position.y < m_killY)
            m_objects.Destroy(obj.id);
    });
}

void PhysicsWorld::FixedStep(float dt)
{
    // 1) Integra os dinamicos: gravidade -> velocidade -> posicao.
    m_objects.ForEach([&](GameObject& obj) {
        if (!obj.IsDynamic()) return;
        obj.grounded = false;
        Integrate(obj, dt);
    });

    // 2) Atualiza o indice espacial (usado para achar quem pode colidir) e resolve colisoes.
    m_objects.UpdateSpatialIndex();
    for (int i = 0; i < kSolverIterations; i++)
        ResolveCollisions();

    // 3) Compara os contatos deste passo com os anteriores: gera Enter e Exit.
    UpdatePairs();
}

void PhysicsWorld::Integrate(GameObject& obj, float dt) const
{
    obj.velocity = Vector3Add(obj.velocity, Vector3Scale(m_gravity, dt));
    obj.velocity = Vector3Scale(obj.velocity, std::max(0.0f, 1.0f - obj.linearDamping * dt));

    const float speed = Vector3Length(obj.velocity);
    if (speed > kMaxSpeed)
        obj.velocity = Vector3Scale(obj.velocity, kMaxSpeed / speed);

    obj.position = Vector3Add(obj.position, Vector3Scale(obj.velocity, dt));
}

void PhysicsWorld::ResolveCollisions()
{
    m_objects.ForEach([&](GameObject& a) {
        const bool dynamic = a.IsDynamic();
        if ((!dynamic && !a.IsKinematic()) || !a.HasCollider()) return;

        // Amplia (broad phase): so quem esta perto, pelo indice de celulas.
        m_candidates.clear();
        m_objects.QueryAABB(a.ColliderAABB(), m_candidates);

        for (ObjectId id : m_candidates)
        {
            if (id == a.id) continue;

            GameObject* b = m_objects.Get(id);
            if (b == nullptr || !b->HasCollider()) continue;

            // Camadas: objetos de camadas que nao interagem se ignoram.
            if (!LayersInteract(a.layer, b->layer)) continue;

            if (dynamic)
            {
                // Dois dinamicos formam um par so uma vez (tratado pelo de menor id).
                if (b->IsDynamic() && b->id < a.id) continue;
                Resolve(a, *b);
            }
            else
            {
                // Cinematico: a colisao fisica dele e feita em MoveKinematic; aqui so geramos
                // contatos para os eventos. Pares com dinamicos sao tratados pelo dinamico.
                if (b->IsDynamic()) continue;
                if (b->IsKinematic() && b->id < a.id) continue;

                const Contact contact = TestColliders(a, *b);
                if (contact.hit) RecordContact(a, *b, contact, 0.0f);
            }
        }
    });
}

// "a" e sempre dinamico; "b" pode ser estatico, cinematico ou dinamico.
void PhysicsWorld::Resolve(GameObject& a, GameObject& b)
{
    const Contact contact = TestColliders(a, b);
    if (!contact.hit) return;

    const Vector3 n = contact.normal; // de a para b

    // Velocidade de aproximacao ao longo da normal (negativa = se aproximando).
    const Vector3 relative = Vector3Subtract(b.velocity, a.velocity);
    const float normalSpeed = Vector3DotProduct(relative, n);

    RecordContact(a, b, contact, std::max(0.0f, -normalSpeed));

    // Gatilhos so geram eventos: nao bloqueiam nem empurram.
    if (a.isTrigger || b.isTrigger) return;

    const float invA = a.InverseMass();
    const float invB = b.InverseMass();
    const float invSum = invA + invB;
    if (invSum <= 0.0f) return;

    // 1) Separacao: cada corpo recua na proporcao da sua massa inversa. Estaticos nao se movem.
    const float depth = std::max(contact.penetration - kPenetrationSlop, 0.0f);
    a.position = Vector3Subtract(a.position, Vector3Scale(n, depth * invA / invSum));
    b.position = Vector3Add(b.position, Vector3Scale(n, depth * invB / invSum));

    // Apoiado: o outro objeto esta embaixo.
    if (n.y < -0.7f) a.grounded = true;
    if (b.IsDynamic() && n.y > 0.7f) b.grounded = true;

    // 2) Velocidade: impulso na direcao da normal (so se estiverem se aproximando).
    if (normalSpeed >= 0.0f) return;

    float restitution = std::min(a.restitution, b.restitution);
    if (-normalSpeed < kRestitutionThreshold) restitution = 0.0f; // impactos fracos nao quicam

    const float j = -(1.0f + restitution) * normalSpeed / invSum;
    const Vector3 impulse = Vector3Scale(n, j);
    a.velocity = Vector3Subtract(a.velocity, Vector3Scale(impulse, invA));
    b.velocity = Vector3Add(b.velocity, Vector3Scale(impulse, invB));

    // 3) Atrito (Coulomb) na direcao tangente, limitado pelo impulso normal.
    Vector3 tangent = Vector3Subtract(relative, Vector3Scale(n, normalSpeed));
    const float tangentLength = Vector3Length(tangent);
    if (tangentLength > 1e-5f)
    {
        tangent = Vector3Scale(tangent, 1.0f / tangentLength);
        const float mu = std::sqrt(a.friction * b.friction);
        const float jt = std::clamp(-Vector3DotProduct(relative, tangent) / invSum, -j * mu, j * mu);
        const Vector3 frictionImpulse = Vector3Scale(tangent, jt);
        a.velocity = Vector3Subtract(a.velocity, Vector3Scale(frictionImpulse, invA));
        b.velocity = Vector3Add(b.velocity, Vector3Scale(frictionImpulse, invB));
    }
}

// ------------------------------------------------------------------ corpos cinematicos

void PhysicsWorld::MoveKinematic(GameObject& body, Vector3 displacement)
{
    body.grounded = false;
    body.position = Vector3Add(body.position, displacement);
    if (!body.HasCollider()) return;

    // Empurra o corpo para fora de tudo que ele atravessa. Varias passadas, porque sair de uma
    // superficie pode colocar o corpo dentro de outra (ex.: encostado no chao e numa parede).
    for (int iteration = 0; iteration < kKinematicIterations; iteration++)
    {
        bool pushed = false;

        m_candidates.clear();
        m_objects.QueryAABB(body.ColliderAABB(), m_candidates);

        for (ObjectId id : m_candidates)
        {
            if (id == body.id) continue;

            const GameObject* other = m_objects.Get(id);
            if (other == nullptr || !other->HasCollider() || other->isTrigger) continue;
            if (other->IsDynamic()) continue; // os dinamicos reagem sozinhos (sao empurrados pelo corpo)
            if (!LayersInteract(body.layer, other->layer)) continue;

            const Contact c = TestColliders(body, *other); // normal: do corpo para o obstaculo
            if (!c.hit) continue;

            // Deixa uma sobreposicao minima para o contato continuar existindo no proximo quadro.
            const float depth = std::max(c.penetration - kContactSkin, 0.0f);
            body.position = Vector3Subtract(body.position, Vector3Scale(c.normal, depth));

            // Remove da velocidade a parte que vai contra o obstaculo (o resto desliza).
            const float into = Vector3DotProduct(body.velocity, c.normal);
            if (into > 0.0f)
                body.velocity = Vector3Subtract(body.velocity, Vector3Scale(c.normal, into));

            if (c.normal.y < -0.7f) body.grounded = true; // obstaculo embaixo = chao
            if (depth > 0.0f) pushed = true;
        }

        if (!pushed) break;
    }
}

// ------------------------------------------------------------------ consultas

bool PhysicsWorld::Raycast(Vector3 origin, Vector3 direction, float maxDistance, RaycastHit& hit,
                           const QueryFilter& filter, float radius) const
{
    hit = RaycastHit{};

    const float length = Vector3Length(direction);
    if (length < 1e-6f || maxDistance <= 0.0f) return false;
    const Vector3 dir = Vector3Scale(direction, 1.0f / length);

    // Caixa que envolve o segmento (mais o raio): candidatos pelo indice de celulas.
    const Vector3 end = Vector3Add(origin, Vector3Scale(dir, maxDistance));
    const Vector3 margin = {radius, radius, radius};
    const BoundingBox segment = {Vector3Subtract(Vector3Min(origin, end), margin),
                                 Vector3Add(Vector3Max(origin, end), margin)};

    std::vector<ObjectId> candidates;
    m_objects.QueryAABB(segment, candidates);

    float best = maxDistance;

    for (ObjectId id : candidates)
    {
        const GameObject* obj = m_objects.Get(id);
        if (obj == nullptr || !PassesFilter(filter, *obj)) continue;

        // "Sphere cast": um raio contra o colisor engordado pelo raio da esfera (conservador nos cantos).
        float t = 0.0f;
        Vector3 normal = {0.0f, 1.0f, 0.0f};
        bool found = false;

        if (obj->collider.shape == ColliderShape::Sphere)
        {
            found = RayVsSphere(origin, dir, obj->ColliderCenter(), obj->ColliderRadius() + radius, best, t, normal);
        }
        else
        {
            BoundingBox box = obj->ColliderAABB();
            box.min = Vector3Subtract(box.min, margin);
            box.max = Vector3Add(box.max, margin);
            found = RayVsBox(origin, dir, box, best, t, normal);
        }

        if (found && t <= best)
        {
            best = t;
            hit.hit = true;
            hit.object = id;
            hit.distance = t;
            hit.normal = normal;
            hit.point = Vector3Subtract(Vector3Add(origin, Vector3Scale(dir, t)), Vector3Scale(normal, radius));
        }
    }

    return hit.hit;
}

Vector3 PhysicsWorld::PushOutSphere(Vector3 center, float radius, const QueryFilter& filter) const
{
    const Vector3 margin = {radius, radius, radius};
    std::vector<ObjectId> candidates;

    for (int iteration = 0; iteration < kKinematicIterations; iteration++)
    {
        bool pushed = false;

        candidates.clear();
        m_objects.QueryAABB({Vector3Subtract(center, margin), Vector3Add(center, margin)}, candidates);

        for (ObjectId id : candidates)
        {
            const GameObject* obj = m_objects.Get(id);
            if (obj == nullptr || !PassesFilter(filter, *obj)) continue;

            const Contact c = (obj->collider.shape == ColliderShape::Sphere)
                                  ? TestSphereSphere(center, radius, obj->ColliderCenter(), obj->ColliderRadius())
                                  : TestSphereBox(center, radius, obj->ColliderAABB());
            if (!c.hit) continue;

            // normal: da esfera para o objeto; a esfera recua no sentido oposto.
            center = Vector3Subtract(center, Vector3Scale(c.normal, c.penetration));
            pushed = true;
        }

        if (!pushed) break;
    }

    return center;
}

// ------------------------------------------------------------------ eventos de colisao

std::uint64_t PhysicsWorld::PairKey(ObjectId a, ObjectId b)
{
    const std::uint64_t lo = std::min(a, b);
    const std::uint64_t hi = std::max(a, b);
    return (lo << 32) | hi;
}

void PhysicsWorld::RecordContact(const GameObject& a, const GameObject& b, const Contact& contact,
                                 float impactSpeed)
{
    ContactInfo& info = m_touchedNow[PairKey(a.id, b.id)];

    if (info.a == InvalidObjectId) // primeiro registro deste par no passo: define quem e "a"
    {
        info.a = a.id;
        info.b = b.id;
    }

    info.normal = contact.normal;
    info.impact = std::max(info.impact, impactSpeed);
    info.trigger = a.isTrigger || b.isTrigger;
}

void PhysicsWorld::UpdatePairs()
{
    // Pares novos (Enter) e continuados.
    for (const auto& entry : m_touchedNow)
    {
        auto it = m_pairs.find(entry.first);
        if (it == m_pairs.end())
        {
            PairState state;
            state.last = entry.second;
            m_pairs.emplace(entry.first, state);
            m_enteredThisFrame.insert(entry.first);
            QueueEvent(CollisionEventType::Enter, entry.second);
        }
        else
        {
            it->second.last = entry.second;
            it->second.missed = 0;
        }
    }

    // Pares que deixaram de se tocar (Exit). A tolerancia evita Exit/Enter falsos quando o contato
    // some por um passo, por exemplo na hora em que um objeto quica.
    for (auto it = m_pairs.begin(); it != m_pairs.end();)
    {
        if (m_touchedNow.count(it->first) != 0)
        {
            ++it;
            continue;
        }

        if (++it->second.missed < kExitGraceSteps)
        {
            ++it;
            continue;
        }

        QueueEvent(CollisionEventType::Exit, it->second.last);
        m_enteredThisFrame.erase(it->first);
        it = m_pairs.erase(it);
    }

    m_touchedNow.clear();
}

void PhysicsWorld::EmitStayEvents()
{
    for (const auto& entry : m_pairs)
    {
        if (entry.second.missed != 0) continue;
        if (m_enteredThisFrame.count(entry.first) != 0) continue; // acabou de entrar: ja teve Enter

        QueueEvent(CollisionEventType::Stay, entry.second.last);
    }
}

void PhysicsWorld::QueueEvent(CollisionEventType type, const ContactInfo& info)
{
    CollisionEvent e;
    e.type = type;
    e.trigger = info.trigger;
    e.self = info.a;
    e.other = info.b;
    e.normal = info.normal;
    e.impactSpeed = info.impact;
    m_pendingEvents.push_back(e);
}

void PhysicsWorld::DispatchEvents()
{
    // Copia a fila: callbacks podem causar novos contatos, mas eles so valem no proximo quadro.
    std::vector<CollisionEvent> events;
    events.swap(m_pendingEvents);

    for (const CollisionEvent& e : events)
    {
        // 1) Ouvinte global (self = a, other = b). Os objetos podem ja ter sido destruidos.
        if (m_eventCallback) m_eventCallback(e);

        // 2) Callback de cada objeto, com a perspectiva dele (self = ele mesmo, normal invertida).
        GameObject* a = m_objects.Get(e.self);
        if (a != nullptr && a->onCollision) a->onCollision(e);

        GameObject* b = m_objects.Get(e.other);
        if (b != nullptr && b->onCollision)
        {
            CollisionEvent flipped = e;
            flipped.self = e.other;
            flipped.other = e.self;
            flipped.normal = Vector3Negate(e.normal);
            b->onCollision(flipped);
        }
    }
}

// ------------------------------------------------------------------ depuracao

void PhysicsWorld::DrawDebug() const
{
    m_objects.ForEach([](const GameObject& obj) {
        if (!obj.HasCollider()) return;

        Color color = LayerColor(obj.layer);
        if (!obj.IsDynamic()) color = Fade(color, 0.6f);
        if (obj.isTrigger) color = YELLOW;

        if (obj.collider.shape == ColliderShape::Sphere)
            DrawSphereWires(obj.ColliderCenter(), obj.ColliderRadius(), 12, 12, color);
        else
            DrawBoundingBox(obj.ColliderAABB(), color);
    });
}
