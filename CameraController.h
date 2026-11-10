#pragma once

#include "raylib.h"

// Encapsula a Camera3D do raylib e o modo de controle do raylib (livre, orbital...).
// Nos modos de jogador (1a e 3a pessoa) quem calcula a camera e o PlayerController, e o resultado
// entra aqui por SetCamera.
class CameraController
{
public:
    CameraController(Vector3 position, Vector3 target, float fovy = 60.0f);

    void Update();
    void Reset();
    void SetMode(int mode); // CAMERA_FREE, CAMERA_ORBITAL...

    // Define a camera de fora (jogador). Mantem o campo de visao e o "up" atuais.
    void SetCamera(const Camera3D& camera) { m_camera = camera; }

    const Camera3D& Get() const { return m_camera; }
    Camera3D& Mutable() { return m_camera; }

private:
    Camera3D m_camera{};
    Camera3D m_initial{};
    int m_mode = CAMERA_FREE;
};
