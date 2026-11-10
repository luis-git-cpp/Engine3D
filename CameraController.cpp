#include "CameraController.h"

CameraController::CameraController(Vector3 position, Vector3 target, float fovy)
{
    m_camera.position = position;
    m_camera.target = target;
    m_camera.up = {0.0f, 1.0f, 0.0f};
    m_camera.fovy = fovy;
    m_camera.projection = CAMERA_PERSPECTIVE;

    m_initial = m_camera;
}

void CameraController::Update()
{
    UpdateCamera(&m_camera, m_mode);
}

void CameraController::Reset()
{
    m_camera = m_initial;
}

void CameraController::SetMode(int mode)
{
    m_mode = mode;
}
