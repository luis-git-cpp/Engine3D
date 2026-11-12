#pragma once

#include "Frustum.h"
#include "ObjectManager.h"
#include "PhysicsWorld.h"
#include "WorldStreamer.h"
#include "raylib.h"

// Mundo de demonstracao e desenho dos objetos. Os objetos em si vivem no ObjectManager; o que
// existe no mundo e decidido pelo WorldStreamer (so o que esta perto do foco fica carregado).
class Scene
{
public:
    Scene(ObjectManager& objects, PhysicsWorld& physics);
    ~Scene();

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // Destroi tudo, recria o mundo de demonstracao e ja carrega o entorno de "focus".
    void Reset(Vector3 focus);

    // Streaming: carrega e descarrega objetos conforme a distancia ate "focus". 1x por quadro.
    void Update(Vector3 focus);
    // Carrega de uma vez tudo ao redor de "focus" (depois de teleportar o jogador).
    void Prime(Vector3 focus);

    // Cria uma bola dinamica na camada indicada (camada 0 = rosa, camada 1 = ciano).
    // Bolas criadas assim nao fazem parte do streaming: existem ate serem destruidas.
    ObjectId SpawnBall(Vector3 position, Vector3 velocity, int layer);

    // Passada final: so desenha o que esta dentro do frustum da camera e a ate "maxDistance" metros.
    void Draw(const Frustum& frustum, Vector3 eye, float maxDistance) const;
    // Passada do shadow map: so o que esta dentro do frustum da luz, com o shader padrao.
    void DrawDepth(const Frustum& frustum) const;

    // Celulas do streaming (verde = carregada, vermelho = dormente).
    void DrawStreamingDebug(Vector3 focus) const;

    // Estatisticas para o HUD.
    int AsyncPending() const { return m_streamer.AsyncPending(); }
    int DrawnCount() const { return m_drawnCount; }
    int LoadedCount() const { return m_streamer.LoadedCount(); }
    int RegisteredCount() const { return m_streamer.RegisteredCount(); }
    int PendingCount() const { return m_streamer.PendingCount(); }
    int TotalLoadedLods() const { return m_streamer.TotalLoadedLods(); }

    void SetStreamingRadius(int cells) { m_streamer.SetLoadRadius(cells); }

private:
    void Build();

    ObjectManager& m_objects;
    PhysicsWorld& m_physics;
    WorldStreamer m_streamer;
    Material m_depthMaterial{};
    mutable int m_drawnCount = 0;
};
