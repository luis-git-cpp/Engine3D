#include "LightManager.h"

#include <algorithm>
#include <cmath>

#include "raymath.h"
#include "rlgl.h"

namespace
{
// Shaders embutidos no codigo: nao dependem da pasta de execucao.
// Comentarios do GLSL sem acentos de proposito (alguns drivers recusam caracteres nao ASCII).
const char* kVertexShader = R"GLSL(
#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

out vec3 fragPosition;
out vec2 fragTexCoord;
out vec3 fragNormal;

void main()
{
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    fragTexCoord = vertexTexCoord;
    fragNormal   = normalize(vec3(matNormal * vec4(vertexNormal, 1.0)));
    gl_Position  = mvp * vec4(vertexPosition, 1.0);
}
)GLSL";

const char* kFragmentShader = R"GLSL(
#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec3 fragNormal;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec3 viewPos;
uniform vec3 ambientColor;   // cor * intensidade da luz global
uniform float shininess;
uniform float specularStrength;

// Shadow maps: um por luz que projeta sombra.
// Em GLSL 330 um array de samplers so pode ser indexado por constante, entao sao 4 samplers
// separados e a funcao ShadowDepth escolhe o certo.
#define MAX_SHADOWS 4
uniform sampler2D shadowMap0;
uniform sampler2D shadowMap1;
uniform sampler2D shadowMap2;
uniform sampler2D shadowMap3;
uniform mat4 shadowVP[MAX_SHADOWS];            // mundo -> espaco de cada luz
uniform int shadowPerspective[MAX_SHADOWS];    // 1 = spot (perspectiva), 0 = direcional (ortografica)
uniform float shadowNear[MAX_SHADOWS];
uniform float shadowFar[MAX_SHADOWS];
uniform float shadowBias;                      // em unidades do mundo
uniform float shadowTexel;                     // 1.0 / resolucao dos shadow maps

#define MAX_LIGHTS 8
#define LIGHT_DIRECTIONAL 0
#define LIGHT_POINT 1
#define LIGHT_SPOT 2

struct Light
{
    int type;
    int enabled;
    vec3 position;
    vec3 direction;
    vec3 color;       // cor * intensidade
    float range;
    float innerCos;   // cosseno do meio-angulo interno do cone
    float outerCos;   // cosseno do meio-angulo externo do cone
    int shadowIndex;  // qual shadow map essa luz usa (-1 = sem sombra)
};
uniform Light lights[MAX_LIGHTS];

out vec4 finalColor;

float ShadowDepth(int s, vec2 uv)
{
    if (s == 0) return textureLod(shadowMap0, uv, 0.0).r;
    if (s == 1) return textureLod(shadowMap1, uv, 0.0).r;
    if (s == 2) return textureLod(shadowMap2, uv, 0.0).r;
    return textureLod(shadowMap3, uv, 0.0).r;
}

// Converte a profundidade armazenada [0,1] em distancia real da luz (unidades do mundo).
float LinearDepth(int s, float d)
{
    float n = shadowNear[s];
    float f = shadowFar[s];
    if (shadowPerspective[s] == 1)
    {
        float z = d * 2.0 - 1.0;
        return (2.0 * n * f) / (f + n - z * (f - n));
    }
    return n + d * (f - n);
}

// 0.0 = totalmente iluminado, 1.0 = totalmente na sombra (para o shadow map s).
float ShadowFactor(int s, float NdotL)
{
    vec4 lightSpace = shadowVP[s] * vec4(fragPosition, 1.0);
    if (lightSpace.w <= 0.0) return 0.0;                 // atras da luz

    vec3 coords = lightSpace.xyz / lightSpace.w;         // divisao perspectiva
    coords = coords * 0.5 + 0.5;                         // [-1,1] -> [0,1]

    // Fora do shadow map: considera iluminado.
    if (coords.z > 1.0 || coords.x < 0.0 || coords.x > 1.0 || coords.y < 0.0 || coords.y > 1.0)
        return 0.0;

    float current = LinearDepth(s, coords.z);
    float bias = shadowBias * (1.0 + 2.0 * (1.0 - NdotL)); // maior em superficies inclinadas

    // PCF 3x3: media de 9 testes vizinhos suaviza a borda da sombra.
    float shadow = 0.0;
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            vec2 uv = coords.xy + vec2(float(x), float(y)) * shadowTexel;
            float closest = LinearDepth(s, ShadowDepth(s, uv));
            shadow += (current - bias > closest) ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

void main()
{
    vec4 texel  = texture(texture0, fragTexCoord) * colDiffuse;
    vec3 albedo = pow(texel.rgb, vec3(2.2));          // sRGB -> linear

    vec3 N = normalize(fragNormal);
    vec3 V = normalize(viewPos - fragPosition);

    vec3 diffuse  = ambientColor;                     // luz global (nunca fica em sombra)
    vec3 specular = vec3(0.0);

    for (int i = 0; i < MAX_LIGHTS; i++)
    {
        if (lights[i].enabled == 0) continue;

        vec3 L;                                       // fragmento -> luz
        float attenuation = 1.0;

        if (lights[i].type == LIGHT_DIRECTIONAL)
        {
            L = normalize(-lights[i].direction);
        }
        else
        {
            vec3 toLight = lights[i].position - fragPosition;
            float dist = length(toLight);
            L = toLight / max(dist, 0.0001);

            // Atenuacao suave: 1 na luz, 0 em "range"
            float x = clamp(dist / lights[i].range, 0.0, 1.0);
            attenuation = 1.0 - x * x;
            attenuation *= attenuation;

            if (lights[i].type == LIGHT_SPOT)
            {
                float theta = dot(-L, normalize(lights[i].direction));
                attenuation *= smoothstep(lights[i].outerCos, lights[i].innerCos, theta);
            }
        }

        float NdotL = max(dot(N, L), 0.0);            // Lambert (difusa)
        vec3 radiance = lights[i].color * attenuation;

        // A sombra de cada luz reduz so a propria luz: varias sombras coexistem e se somam.
        if (lights[i].shadowIndex >= 0 && NdotL > 0.0)
            radiance *= 1.0 - ShadowFactor(lights[i].shadowIndex, NdotL);

        diffuse += radiance * NdotL;

        if (NdotL > 0.0)                              // Blinn-Phong (especular)
        {
            vec3 H = normalize(L + V);
            specular += radiance * pow(max(dot(N, H), 0.0), shininess) * specularStrength;
        }
    }

    vec3 color = albedo * diffuse + specular;
    color = pow(color, vec3(1.0 / 2.2));              // linear -> sRGB
    finalColor = vec4(color, texel.a);
}
)GLSL";

Vector3 ToVector3(Color c, float intensity)
{
    return {c.r / 255.0f * intensity, c.g / 255.0f * intensity, c.b / 255.0f * intensity};
}
} // namespace

LightManager::LightManager()
{
    m_shadowIndex.fill(-1);

    m_shader = LoadShaderFromMemory(kVertexShader, kFragmentShader);

    m_viewPosLoc    = GetShaderLocation(m_shader, "viewPos");
    m_ambientLoc    = GetShaderLocation(m_shader, "ambientColor");
    m_shininessLoc  = GetShaderLocation(m_shader, "shininess");
    m_specularLoc   = GetShaderLocation(m_shader, "specularStrength");
    m_shadowBiasLoc = GetShaderLocation(m_shader, "shadowBias");
    m_shadowTexelLoc = GetShaderLocation(m_shader, "shadowTexel");

    m_shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(m_shader, "matModel");
    m_shader.locs[SHADER_LOC_VECTOR_VIEW]  = m_viewPosLoc;

    for (int i = 0; i < MaxLights; i++)
    {
        LightLocs& l = m_locs[i];
        l.type        = GetShaderLocation(m_shader, TextFormat("lights[%d].type", i));
        l.enabled     = GetShaderLocation(m_shader, TextFormat("lights[%d].enabled", i));
        l.position    = GetShaderLocation(m_shader, TextFormat("lights[%d].position", i));
        l.direction   = GetShaderLocation(m_shader, TextFormat("lights[%d].direction", i));
        l.color       = GetShaderLocation(m_shader, TextFormat("lights[%d].color", i));
        l.range       = GetShaderLocation(m_shader, TextFormat("lights[%d].range", i));
        l.innerCos    = GetShaderLocation(m_shader, TextFormat("lights[%d].innerCos", i));
        l.outerCos    = GetShaderLocation(m_shader, TextFormat("lights[%d].outerCos", i));
        l.shadowIndex = GetShaderLocation(m_shader, TextFormat("lights[%d].shadowIndex", i));

        // Nenhuma luz usa sombra ate o primeiro RenderShadows.
        const int none = -1;
        SetShaderValue(m_shader, l.shadowIndex, &none, SHADER_UNIFORM_INT);
    }

    // Cada shadow map tem seu sampler (ligado a uma unidade de textura fixa) e seus dados.
    for (int k = 0; k < MaxShadowMaps; k++)
    {
        ShadowLocs& s = m_shadowLocs[k];
        s.sampler     = GetShaderLocation(m_shader, TextFormat("shadowMap%d", k));
        s.viewProj    = GetShaderLocation(m_shader, TextFormat("shadowVP[%d]", k));
        s.perspective = GetShaderLocation(m_shader, TextFormat("shadowPerspective[%d]", k));
        s.nearPlane   = GetShaderLocation(m_shader, TextFormat("shadowNear[%d]", k));
        s.farPlane    = GetShaderLocation(m_shader, TextFormat("shadowFar[%d]", k));

        const int slot = ShadowSlotBase + k;
        SetShaderValue(m_shader, s.sampler, &slot, SHADER_UNIFORM_INT);
    }

    const float texel = 1.0f / static_cast<float>(ShadowResolution);
    SetShaderValue(m_shader, m_shadowTexelLoc, &texel, SHADER_UNIFORM_FLOAT);

    m_lights.reserve(MaxLights);
}

LightManager::~LightManager()
{
    UnloadShader(m_shader);
}

void LightManager::SetAmbient(Color color, float intensity)
{
    m_ambientColor = color;
    m_ambientIntensity = intensity;
}

void LightManager::SetSpecular(float shininess, float strength)
{
    m_shininess = shininess;
    m_specularStrength = strength;
}

int LightManager::AddDirectional(Vector3 direction, Color color, float intensity)
{
    Light light;
    light.type = LightType::Directional;
    light.direction = Vector3Normalize(direction);
    light.color = color;
    light.intensity = intensity;
    return Add(light);
}

int LightManager::AddPoint(Vector3 position, Color color, float intensity, float range)
{
    Light light;
    light.type = LightType::Point;
    light.position = position;
    light.color = color;
    light.intensity = intensity;
    light.range = range;
    return Add(light);
}

int LightManager::AddSpot(Vector3 position, Vector3 target, Color color, float intensity,
                          float range, float innerAngle, float outerAngle)
{
    Light light;
    light.type = LightType::Spot;
    light.position = position;
    light.direction = Vector3Normalize(Vector3Subtract(target, position));
    light.color = color;
    light.intensity = intensity;
    light.range = range;
    light.innerAngle = innerAngle;
    light.outerAngle = outerAngle;
    return Add(light);
}

int LightManager::Add(const Light& light)
{
    if (static_cast<int>(m_lights.size()) >= MaxLights)
    {
        TraceLog(LOG_WARNING, "LightManager: limite de %d luzes atingido", MaxLights);
        return -1;
    }
    m_lights.push_back(light);
    return static_cast<int>(m_lights.size()) - 1;
}

Light* LightManager::Get(int index)
{
    if (index < 0 || index >= static_cast<int>(m_lights.size())) return nullptr;
    return &m_lights[index];
}

ShadowMap& LightManager::EnsureShadowMap(int slot)
{
    if (!m_shadows[slot])
        m_shadows[slot] = std::make_unique<ShadowMap>(ShadowResolution);
    return *m_shadows[slot];
}

void LightManager::RenderShadows(const std::function<void()>& drawDepth)
{
    // 0) Solta as texturas de sombra antes de escrever nelas (nunca ler e escrever a mesma textura).
    for (int k = 0; k < MaxShadowMaps; k++)
    {
        if (m_shadows[k])
            m_shadows[k]->Unbind(ShadowSlotBase + k);
    }

    // 1) Cada luz Directional/Spot ativa recebe um shadow map livre, na ordem em que foi adicionada.
    //    Luzes alem de MaxShadowMaps ficam sem sombra. Point nunca projeta (exigiria um cubemap).
    std::array<int, MaxShadowMaps> casterOfMap{};
    int used = 0;

    for (int i = 0; i < static_cast<int>(m_lights.size()); i++)
    {
        const Light& l = m_lights[i];
        const bool eligible = m_shadowsEnabled && l.enabled && l.castShadows && l.type != LightType::Point;

        int slot = -1;
        if (eligible && used < MaxShadowMaps && EnsureShadowMap(used).IsValid())
        {
            slot = used;
            casterOfMap[used] = i;
            used++;
        }

        m_shadowIndex[i] = slot;
        SetShaderValue(m_shader, m_locs[i].shadowIndex, &slot, SHADER_UNIFORM_INT);
    }

    // 2) Uma passada de profundidade por shadow map (cena vista de cada luz).
    for (int k = 0; k < used; k++)
    {
        ShadowMap& map = *m_shadows[k];
        map.SetFocus(m_shadowCenter, m_shadowRadius);
        map.Render(m_lights[casterOfMap[k]], drawDepth);

        const ShadowLocs& loc = m_shadowLocs[k];
        const int perspective = map.IsPerspective() ? 1 : 0;
        const float nearPlane = map.Near();
        const float farPlane = map.Far();

        SetShaderValueMatrix(m_shader, loc.viewProj, map.LightViewProj());
        SetShaderValue(m_shader, loc.perspective, &perspective, SHADER_UNIFORM_INT);
        SetShaderValue(m_shader, loc.nearPlane, &nearPlane, SHADER_UNIFORM_FLOAT);
        SetShaderValue(m_shader, loc.farPlane, &farPlane, SHADER_UNIFORM_FLOAT);

        map.Bind(ShadowSlotBase + k);
    }

    SetShaderValue(m_shader, m_shadowBiasLoc, &m_shadowBias, SHADER_UNIFORM_FLOAT);
}

void LightManager::Update(const Camera3D& camera)
{
    const Vector3 ambient = ToVector3(m_ambientColor, m_ambientIntensity);

    SetShaderValue(m_shader, m_viewPosLoc, &camera.position, SHADER_UNIFORM_VEC3);
    SetShaderValue(m_shader, m_ambientLoc, &ambient, SHADER_UNIFORM_VEC3);
    SetShaderValue(m_shader, m_shininessLoc, &m_shininess, SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_shader, m_specularLoc, &m_specularStrength, SHADER_UNIFORM_FLOAT);

    for (int i = 0; i < static_cast<int>(m_lights.size()); i++)
        Upload(i);
}

void LightManager::Upload(int index) const
{
    const Light& l = m_lights[index];
    const LightLocs& loc = m_locs[index];

    const int type = static_cast<int>(l.type);
    const int enabled = l.enabled ? 1 : 0;
    const int shadowIndex = m_shadowIndex[index];
    const Vector3 direction = Vector3Normalize(l.direction);
    const Vector3 color = ToVector3(l.color, l.intensity);

    // Garante inner < outer (o smoothstep do shader exige borda menor < borda maior).
    const float outer = std::max(l.outerAngle, 0.02f);
    const float inner = std::min(l.innerAngle, outer - 0.01f);
    const float innerCos = std::cos(DEG2RAD * inner);
    const float outerCos = std::cos(DEG2RAD * outer);

    SetShaderValue(m_shader, loc.type, &type, SHADER_UNIFORM_INT);
    SetShaderValue(m_shader, loc.enabled, &enabled, SHADER_UNIFORM_INT);
    SetShaderValue(m_shader, loc.shadowIndex, &shadowIndex, SHADER_UNIFORM_INT);
    SetShaderValue(m_shader, loc.position, &l.position, SHADER_UNIFORM_VEC3);
    SetShaderValue(m_shader, loc.direction, &direction, SHADER_UNIFORM_VEC3);
    SetShaderValue(m_shader, loc.color, &color, SHADER_UNIFORM_VEC3);
    SetShaderValue(m_shader, loc.range, &l.range, SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_shader, loc.innerCos, &innerCos, SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_shader, loc.outerCos, &outerCos, SHADER_UNIFORM_FLOAT);
}

void LightManager::ApplyTo(Model& model) const
{
    for (int i = 0; i < model.materialCount; i++)
        model.materials[i].shader = m_shader;
}

void LightManager::DrawGizmos() const
{
    for (const Light& l : m_lights)
    {
        if (!l.enabled) continue;

        if (l.type == LightType::Point)
        {
            DrawSphere(l.position, 0.12f, l.color);
        }
        else if (l.type == LightType::Spot)
        {
            const Vector3 tip = Vector3Add(l.position, Vector3Scale(Vector3Normalize(l.direction), 1.5f));
            DrawLine3D(l.position, tip, l.color);
        }
    }
}
