#pragma once

#include <array>
#include <functional>
#include <memory>
#include <vector>

#include "Light.h"
#include "ShadowMap.h"
#include "raylib.h"

// Gerencia o shader de iluminacao, a lista de luzes (ate MaxLights) e os shadow maps.
// Precisa ser criado DEPOIS da janela (usa contexto OpenGL).
class LightManager
{
public:
    static constexpr int MaxLights = 8;          // deve ser igual ao MAX_LIGHTS do shader
    static constexpr int MaxShadowMaps = 4;      // deve ser igual ao MAX_SHADOWS do shader
    static constexpr int ShadowSlotBase = 12;    // shadow maps usam as unidades de textura 12..15
    static constexpr int ShadowResolution = 2048;

    LightManager();
    ~LightManager();

    LightManager(const LightManager&) = delete;
    LightManager& operator=(const LightManager&) = delete;

    // Luz global (ambiente): iluminacao base aplicada a tudo, sem direcao.
    void SetAmbient(Color color, float intensity);
    // Brilho especular: shininess maior = reflexo menor e mais concentrado.
    void SetSpecular(float shininess, float strength);

    // Cada Add* devolve o indice da luz, ou -1 se a lista estiver cheia.
    int AddDirectional(Vector3 direction, Color color, float intensity = 1.0f);
    int AddPoint(Vector3 position, Color color, float intensity = 1.0f, float range = 10.0f);
    int AddSpot(Vector3 position, Vector3 target, Color color, float intensity = 1.0f,
                float range = 15.0f, float innerAngle = 15.0f, float outerAngle = 25.0f);

    Light* Get(int index); // nullptr se o indice for invalido

    // --- Sombras (shadow map) ---
    // Liga/desliga todas as sombras. Desligadas, nenhuma passada de profundidade e feita.
    void SetShadowsEnabled(bool enabled) { m_shadowsEnabled = enabled; }
    bool ShadowsEnabled() const { return m_shadowsEnabled; }
    // Margem anti "acne" em unidades do mundo (maior = menos acne, mais "peter panning").
    void SetShadowBias(float worldUnits) { m_shadowBias = worldUnits; }
    // Regiao coberta pelas luzes direcionais: centro e raio da cena.
    void SetShadowFocus(Vector3 center, float radius) { m_shadowCenter = center; m_shadowRadius = radius; }

    // Passadas de profundidade: cada luz Directional/Spot ativa (com castShadows) recebe um shadow
    // map e a cena e renderizada do ponto de vista dela. Chamar todo frame, antes de desenhar a cena.
    // drawDepth desenha a geometria (so profundidade).
    void RenderShadows(const std::function<void()>& drawDepth);

    // Envia camera, luz ambiente e todas as luzes para o shader. Chamar 1x por frame.
    void Update(const Camera3D& camera);

    // Aplica o shader de iluminacao a todos os materiais do modelo.
    void ApplyTo(Model& model) const;

    // Desenha marcadores das luzes (chamar dentro de BeginMode3D/EndMode3D).
    void DrawGizmos() const;

private:
    struct LightLocs
    {
        int type = -1, enabled = -1, position = -1, direction = -1;
        int color = -1, range = -1, innerCos = -1, outerCos = -1;
        int shadowIndex = -1;
    };

    struct ShadowLocs
    {
        int sampler = -1, viewProj = -1, perspective = -1, nearPlane = -1, farPlane = -1;
    };

    int Add(const Light& light);
    void Upload(int index) const;
    ShadowMap& EnsureShadowMap(int slot);

    Shader m_shader{};
    std::vector<Light> m_lights;
    std::array<LightLocs, MaxLights> m_locs{};

    // Um shadow map por luz que projeta sombra (criados sob demanda).
    std::array<std::unique_ptr<ShadowMap>, MaxShadowMaps> m_shadows;
    std::array<ShadowLocs, MaxShadowMaps> m_shadowLocs{};
    std::array<int, MaxLights> m_shadowIndex{}; // shadow map usado por cada luz (-1 = nenhum)

    int m_viewPosLoc = -1;
    int m_ambientLoc = -1;
    int m_shininessLoc = -1;
    int m_specularLoc = -1;
    int m_shadowBiasLoc = -1;
    int m_shadowTexelLoc = -1;

    bool m_shadowsEnabled = true;
    float m_shadowBias = 0.04f;
    Vector3 m_shadowCenter{0.0f, 0.0f, 0.0f};
    float m_shadowRadius = 15.0f;

    Color m_ambientColor = {255, 255, 255, 255};
    float m_ambientIntensity = 0.15f;
    float m_shininess = 32.0f;
    float m_specularStrength = 0.5f;
};
