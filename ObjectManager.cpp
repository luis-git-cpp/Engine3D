#include "ObjectManager.h"

#include <algorithm>
#include <cmath>

#include "Assets.h"
#include "raymath.h"
#include "rlgl.h"

// Em algumas versoes do raylib esta constante nao esta no cabecalho publico.
#ifndef MAX_MATERIAL_MAPS
#define MAX_MATERIAL_MAPS 12
#endif

namespace
{
// UnloadModel nao descarrega texturas (elas podem ser compartilhadas), entao fazemos aqui,
// sem repetir ids e sem tocar na textura padrao do raylib.
void ReleaseModel(Model& model)
{
    std::vector<unsigned int> unloaded;
    const unsigned int defaultTexture = rlGetTextureIdDefault();

    for (int m = 0; m < model.materialCount; m++)
    {
        for (int i = 0; i < MAX_MATERIAL_MAPS; i++)
        {
            const unsigned int id = model.materials[m].maps[i].texture.id;
            if (id == 0 || id == defaultTexture) continue;
            if (std::find(unloaded.begin(), unloaded.end(), id) != unloaded.end()) continue;

            unloaded.push_back(id);
            rlUnloadTexture(id);
        }
    }

    UnloadModel(model); // o shader de iluminacao pertence ao LightManager e nao e descarregado aqui
}
} // namespace

ObjectManager::ObjectManager(const LightManager& lights, float cellSize)
    : m_lights(lights), m_coords(cellSize)
{
}

ObjectManager::~ObjectManager()
{
    DestroyAll();
    CollectGarbage();
}

// ------------------------------------------------------------------ criacao

ObjectId ObjectManager::CreatePrimitive(PrimitiveType type, Vector3 size, const ObjectParams& params)
{
    Mesh mesh{};

    switch (type)
    {
        case PrimitiveType::Plane:    mesh = GenMeshPlane(size.x, size.z, 1, 1);            break;
        case PrimitiveType::Cube:     mesh = GenMeshCube(size.x, size.y, size.z);           break;
        case PrimitiveType::Sphere:   mesh = GenMeshSphere(size.x * 0.5f, 32, 32);          break;
        case PrimitiveType::Cylinder: mesh = GenMeshCylinder(size.x * 0.5f, size.y, 24);    break;
    }

    return CreateFromModel(LoadModelFromMesh(mesh), params);
}

ObjectId ObjectManager::CreateFromFile(const std::string& modelPath, const ObjectParams& params)
{
    const Model model = LoadModel(ResolveAssetPath(modelPath).c_str());

    if (model.meshCount == 0)
    {
        TraceLog(LOG_WARNING, "ObjectManager: nao foi possivel carregar o modelo '%s'", modelPath.c_str());
        return InvalidObjectId;
    }

    return CreateFromModel(model, params);
}

ObjectId ObjectManager::CreateFromModel(Model model, const ObjectParams& params)
{
    if (model.meshCount == 0)
    {
        TraceLog(LOG_WARNING, "ObjectManager: modelo sem malhas, objeto nao criado");
        return InvalidObjectId;
    }

    auto obj = std::make_unique<GameObject>();
    obj->id = m_nextId++;
    obj->name = params.name.empty() ? "objeto" + std::to_string(obj->id) : params.name;

    obj->position = params.position;
    obj->rotation = params.rotation;
    obj->scale = params.scale;
    obj->visible = params.visible;
    obj->castShadow = params.castShadow;

    obj->body = params.body;
    obj->layer = params.layer;
    obj->isTrigger = params.isTrigger;
    obj->mass = params.mass > 0.0f ? params.mass : 1.0f;
    obj->restitution = params.restitution;
    obj->friction = params.friction;
    obj->linearDamping = params.linearDamping;

    obj->model = model;
    obj->localBounds = GetModelBoundingBox(model);

    // Cor (so se pedida) e shader de iluminacao em todos os materiais do modelo.
    if (params.tint.has_value())
    {
        for (int i = 0; i < obj->model.materialCount; i++)
            obj->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].color = *params.tint;
    }
    m_lights.ApplyTo(obj->model);

    BuildCollider(*obj, params);
    Reindex(*obj);

    const ObjectId id = obj->id;
    m_objects.emplace(id, std::move(obj));
    return id;
}

void ObjectManager::BuildCollider(GameObject& obj, const ObjectParams& params) const
{
    Collider& c = obj.collider;
    c = Collider{};

    if (params.collider == ColliderShape::None) return;

    // Caixa do modelo: centro e metade do tamanho (com espessura minima para modelos planos).
    const BoundingBox& b = obj.localBounds;
    const Vector3 center = {(b.min.x + b.max.x) * 0.5f, (b.min.y + b.max.y) * 0.5f, (b.min.z + b.max.z) * 0.5f};
    Vector3 half = {(b.max.x - b.min.x) * 0.5f, (b.max.y - b.min.y) * 0.5f, (b.max.z - b.min.z) * 0.5f};
    half = {std::max(half.x, 0.01f), std::max(half.y, 0.01f), std::max(half.z, 0.01f)};

    c.offset = params.colliderOffset.has_value() ? *params.colliderOffset : center;

    if (params.collider == ColliderShape::Sphere)
    {
        c.shape = ColliderShape::Sphere;
        c.radius = params.colliderRadius > 0.0f ? params.colliderRadius : std::max({half.x, half.y, half.z});
    }
    else // Box e Auto
    {
        c.shape = ColliderShape::Box;
        const Vector3& h = params.colliderHalfExtents;
        const bool explicitSize = h.x > 0.0f || h.y > 0.0f || h.z > 0.0f;
        c.halfExtents = explicitSize ? h : half;
    }
}

// ------------------------------------------------------------------ destruicao

void ObjectManager::Destroy(ObjectId id)
{
    auto it = m_objects.find(id);
    if (it != m_objects.end())
        it->second->pendingDestroy = true;
}

void ObjectManager::DestroyAll()
{
    for (auto& entry : m_objects)
        entry.second->pendingDestroy = true;
}

void ObjectManager::CollectGarbage()
{
    for (auto it = m_objects.begin(); it != m_objects.end();)
    {
        GameObject& obj = *it->second;
        if (!obj.pendingDestroy)
        {
            ++it;
            continue;
        }

        IndexRemove(obj);
        ReleaseModel(obj.model);
        it = m_objects.erase(it);
    }
}

GameObject* ObjectManager::Get(ObjectId id)
{
    auto it = m_objects.find(id);
    if (it == m_objects.end() || it->second->pendingDestroy) return nullptr;
    return it->second.get();
}

const GameObject* ObjectManager::Get(ObjectId id) const
{
    auto it = m_objects.find(id);
    if (it == m_objects.end() || it->second->pendingDestroy) return nullptr;
    return it->second.get();
}

int ObjectManager::Count() const
{
    int count = 0;
    for (const auto& entry : m_objects)
    {
        if (!entry.second->pendingDestroy) count++;
    }
    return count;
}

// ------------------------------------------------------------------ indice espacial

void ObjectManager::UpdateSpatialIndex()
{
    for (auto& entry : m_objects)
    {
        GameObject& obj = *entry.second;
        if (!obj.pendingDestroy) Reindex(obj);
    }
}

void ObjectManager::Reindex(GameObject& obj)
{
    obj.cell = m_coords.ToCell(obj.position);

    CellCoord minCell, maxCell;
    m_coords.CellRange(obj.WorldBounds(), minCell, maxCell);

    if (obj.indexed && minCell == obj.cellMin && maxCell == obj.cellMax) return;

    IndexRemove(obj);
    obj.cellMin = minCell;
    obj.cellMax = maxCell;
    IndexInsert(obj);
}

void ObjectManager::IndexInsert(GameObject& obj)
{
    for (int x = obj.cellMin.x; x <= obj.cellMax.x; x++)
        for (int y = obj.cellMin.y; y <= obj.cellMax.y; y++)
            for (int z = obj.cellMin.z; z <= obj.cellMax.z; z++)
                m_grid[CellCoord{x, y, z}].push_back(obj.id);

    obj.indexed = true;
}

void ObjectManager::IndexRemove(GameObject& obj)
{
    if (!obj.indexed) return;

    for (int x = obj.cellMin.x; x <= obj.cellMax.x; x++)
    {
        for (int y = obj.cellMin.y; y <= obj.cellMax.y; y++)
        {
            for (int z = obj.cellMin.z; z <= obj.cellMax.z; z++)
            {
                auto it = m_grid.find(CellCoord{x, y, z});
                if (it == m_grid.end()) continue;

                std::vector<ObjectId>& ids = it->second;
                ids.erase(std::remove(ids.begin(), ids.end(), obj.id), ids.end());
                if (ids.empty()) m_grid.erase(it);
            }
        }
    }

    obj.indexed = false;
}

void ObjectManager::QueryAABB(const BoundingBox& box, std::vector<ObjectId>& out) const
{
    CellCoord lo, hi;
    m_coords.CellRange(box, lo, hi);

    const std::size_t start = out.size();

    for (int x = lo.x; x <= hi.x; x++)
    {
        for (int y = lo.y; y <= hi.y; y++)
        {
            for (int z = lo.z; z <= hi.z; z++)
            {
                auto it = m_grid.find(CellCoord{x, y, z});
                if (it != m_grid.end())
                    out.insert(out.end(), it->second.begin(), it->second.end());
            }
        }
    }

    // Um objeto grande aparece em varias celulas: remove os repetidos.
    std::sort(out.begin() + start, out.end());
    out.erase(std::unique(out.begin() + start, out.end()), out.end());

    // Confere a caixa exata de cada candidato.
    auto outside = [&](ObjectId id) {
        const GameObject* obj = Get(id);
        return obj == nullptr || !CheckCollisionBoxes(box, obj->WorldBounds());
    };
    out.erase(std::remove_if(out.begin() + start, out.end(), outside), out.end());
}

void ObjectManager::QueryRadius(Vector3 center, float radius, std::vector<ObjectId>& out) const
{
    const Vector3 r = {radius, radius, radius};
    const BoundingBox box = {Vector3Subtract(center, r), Vector3Add(center, r)};

    const std::size_t start = out.size();
    QueryAABB(box, out);

    // Descarta os que estao no canto da caixa, mas fora da esfera.
    auto tooFar = [&](ObjectId id) {
        const BoundingBox b = Get(id)->WorldBounds();
        const float dx = center.x - std::clamp(center.x, b.min.x, b.max.x);
        const float dy = center.y - std::clamp(center.y, b.min.y, b.max.y);
        const float dz = center.z - std::clamp(center.z, b.min.z, b.max.z);
        return dx * dx + dy * dy + dz * dz > radius * radius;
    };
    out.erase(std::remove_if(out.begin() + start, out.end(), tooFar), out.end());
}

void ObjectManager::QueryCell(CellCoord cell, std::vector<ObjectId>& out) const
{
    auto it = m_grid.find(cell);
    if (it == m_grid.end()) return;

    for (ObjectId id : it->second)
    {
        if (Get(id) != nullptr) out.push_back(id);
    }
}
