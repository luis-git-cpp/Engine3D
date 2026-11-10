#pragma once

#include "GameObject.h"
#include "ObjectManager.h"
#include "PhysicsWorld.h"
#include "raylib.h"

// Jogador: um corpo cinematico (cilindro de 0,6 x 1,8 m, colisor em caixa) que anda, corre e pula
// com colisao. Tambem calcula a camera em 1a e 3a pessoa; na 3a pessoa a camera e um "braco de
// mola" que encosta nas paredes em vez de atravessa-las.
class PlayerController
{
public:
    PlayerController(ObjectManager& objects, PhysicsWorld& physics);

    // Cria o corpo do jogador (recria se ja existir). "feet" = posicao dos pes.
    void Spawn(Vector3 feet, float yawDegrees = 0.0f);

    // Camada de colisao do jogador (so interage com objetos de camadas liberadas para ela).
    void SetLayer(int layer);
    int Layer() const { return m_layer; }

    void SetVisible(bool visible); // desenha ou nao o corpo (a sombra continua)
    void LookAt(Vector3 direction); // aponta o olhar para a direcao dada

    // Le o teclado (WASD, Shift, Espaco) e o mouse e move o jogador. Chamar 1x por quadro.
    // lookEnabled = false ignora o mouse neste quadro (ex.: logo depois de capturar o cursor).
    // Devolve true se o jogador caiu para fora do mundo e reapareceu no ponto inicial neste quadro
    // (quem usa streaming deve carregar o entorno do novo ponto).
    bool Update(float dt, bool lookEnabled);

    // Camera do jogador. Na 3a pessoa, a colisao da camera e feita aqui.
    Camera3D BuildCamera(bool thirdPerson, float dt);

    ObjectId Id() const { return m_id; }
    Vector3 Position() const;     // pes
    Vector3 EyePosition() const;
    Vector3 Forward() const;      // direcao do olhar
    bool Grounded() const;

private:
    ObjectManager& m_objects;
    PhysicsWorld& m_physics;

    ObjectId m_id = InvalidObjectId;
    int m_layer = 0;
    float m_yaw = 0.0f;   // radianos; 0 = olhando para -Z
    float m_pitch = 0.0f; // radianos; positivo = olhando para cima
    float m_cameraDistance = 4.0f; // distancia atual da camera de 3a pessoa (suavizada)
    Vector3 m_spawn{0.0f, 0.0f, 0.0f};
};
