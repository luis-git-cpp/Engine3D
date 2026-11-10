#pragma once

#include <functional>

#include "Frustum.h"
#include "Light.h"
#include "raylib.h"

// Mapa de profundidade (shadow map) renderizado do ponto de vista de uma luz.
// Suporta luz Directional (projecao ortografica) e Spot (projecao em perspectiva).
class ShadowMap
{
public:
    explicit ShadowMap(int resolution = 2048);
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;

    bool IsValid() const { return m_target.id > 0; }

    // Regiao do mundo coberta pela sombra da luz direcional (esfera: centro + raio).
    void SetFocus(Vector3 center, float radius);

    // Renderiza a cena (drawDepth) do ponto de vista da luz, so profundidade. O callback recebe o
    // frustum da luz: objetos fora dele nao entram no shadow map e podem ser pulados.
    void Render(const Light& light, const std::function<void(const Frustum&)>& drawDepth);

    // Liga/desliga a textura de profundidade numa unidade de textura do OpenGL.
    void Bind(int slot) const;
    void Unbind(int slot) const;

    const Matrix& LightViewProj() const { return m_lightVP; }
    bool IsPerspective() const { return m_perspective; }
    float Near() const { return m_near; }
    float Far() const { return m_far; }
    int Resolution() const { return m_resolution; }

private:
    Camera3D BuildLightCamera(const Light& light) const;

    RenderTexture2D m_target{};
    int m_resolution = 0;

    Matrix m_lightVP{};
    bool m_perspective = false;
    float m_near = 0.01f;
    float m_far = 1000.0f;

    Vector3 m_focusCenter{0.0f, 0.0f, 0.0f};
    float m_focusRadius = 15.0f;
};
