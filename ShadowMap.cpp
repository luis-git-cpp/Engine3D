#include "ShadowMap.h"

#include <cmath>

#include "raymath.h"
#include "rlgl.h"

namespace
{
// Recupera os planos near/far a partir da matriz de projecao que o raylib montou.
// Assim o shader consegue converter profundidade em distancia real (em unidades do mundo),
// independente da versao do raylib ou de como os planos foram configurados.
void ExtractClipPlanes(const Matrix& proj, bool perspective, float& nearPlane, float& farPlane)
{
    if (perspective)
    {
        const float a = proj.m10;
        const float b = proj.m14;
        nearPlane = b / (a - 1.0f);
        farPlane  = b / (a + 1.0f);
    }
    else
    {
        const float range = -2.0f / proj.m10;   // far - near
        const float sum   = -proj.m14 * range;  // far + near
        nearPlane = (sum - range) * 0.5f;
        farPlane  = (sum + range) * 0.5f;
    }
}
} // namespace

ShadowMap::ShadowMap(int resolution)
    : m_resolution(resolution)
{
    m_lightVP = MatrixIdentity();

    // A assinatura de rlLoadFramebuffer mudou no raylib 5.5.
#if (RAYLIB_VERSION_MAJOR > 5) || (RAYLIB_VERSION_MAJOR == 5 && RAYLIB_VERSION_MINOR >= 5)
    m_target.id = rlLoadFramebuffer();
#else
    m_target.id = rlLoadFramebuffer(resolution, resolution);
#endif

    // O raylib usa estes valores para montar viewport e aspecto da projecao.
    m_target.texture.width = resolution;
    m_target.texture.height = resolution;

    if (m_target.id > 0)
    {
        rlEnableFramebuffer(m_target.id);

        // Shadow map nao precisa de cor: so uma textura de profundidade que o shader possa ler.
        m_target.depth.id = rlLoadTextureDepth(resolution, resolution, false);
        m_target.depth.width = resolution;
        m_target.depth.height = resolution;
        m_target.depth.mipmaps = 1;

        rlFramebufferAttach(m_target.id, m_target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);

        if (!rlFramebufferComplete(m_target.id))
            TraceLog(LOG_WARNING, "ShadowMap: framebuffer incompleto, sombras desativadas");

        rlDisableFramebuffer();
    }
    else
    {
        TraceLog(LOG_WARNING, "ShadowMap: nao foi possivel criar o framebuffer");
    }
}

ShadowMap::~ShadowMap()
{
    // Descarrega tambem a textura de profundidade anexada.
    if (m_target.id > 0)
        rlUnloadFramebuffer(m_target.id);
}

void ShadowMap::SetFocus(Vector3 center, float radius)
{
    m_focusCenter = center;
    m_focusRadius = radius;
}

Camera3D ShadowMap::BuildLightCamera(const Light& light) const
{
    Camera3D cam{};
    const Vector3 dir = Vector3Normalize(light.direction);

    // Evita o "up" paralelo a direcao da luz (lookAt degenerado quando a luz aponta reto para baixo).
    cam.up = (std::fabs(dir.y) > 0.99f) ? Vector3{0.0f, 0.0f, 1.0f} : Vector3{0.0f, 1.0f, 0.0f};

    if (light.type == LightType::Directional)
    {
        // Raios paralelos: caixa ortografica de lado 2*raio, olhando para o centro da cena.
        // O centro e "encaixado" na grade de texels do shadow map (nos eixos da luz). Sem isso, a
        // sombra tremeria sempre que o foco (jogador) se move, porque os texels mudariam de lugar.
        const Vector3 right = Vector3Normalize(Vector3CrossProduct(dir, cam.up));
        const Vector3 up = Vector3CrossProduct(right, dir);
        const float texel = (m_focusRadius * 2.0f) / static_cast<float>(m_resolution);

        const float x = std::floor(Vector3DotProduct(m_focusCenter, right) / texel) * texel;
        const float y = std::floor(Vector3DotProduct(m_focusCenter, up) / texel) * texel;
        const float z = Vector3DotProduct(m_focusCenter, dir);
        const Vector3 center = Vector3Add(Vector3Add(Vector3Scale(right, x), Vector3Scale(up, y)),
                                          Vector3Scale(dir, z));

        cam.projection = CAMERA_ORTHOGRAPHIC;
        cam.position = Vector3Subtract(center, Vector3Scale(dir, m_focusRadius * 2.0f));
        cam.target = center;
        cam.fovy = m_focusRadius * 2.0f; // no modo ortografico, fovy e a altura da caixa
    }
    else
    {
        // Spot: perspectiva saindo da posicao da luz, com um pouco de folga alem do cone.
        cam.projection = CAMERA_PERSPECTIVE;
        cam.position = light.position;
        cam.target = Vector3Add(light.position, dir);
        cam.fovy = Clamp(light.outerAngle * 2.0f + 4.0f, 10.0f, 170.0f);
    }

    return cam;
}

void ShadowMap::Render(const Light& light, const std::function<void(const Frustum&)>& drawDepth)
{
    if (!IsValid()) return;

    const Camera3D cam = BuildLightCamera(light);
    m_perspective = (light.type != LightType::Directional);

    BeginTextureMode(m_target);
    ClearBackground(WHITE); // limpa cor e profundidade (profundidade = 1.0, "infinito")

    BeginMode3D(cam);
    const Matrix view = rlGetMatrixModelview();
    const Matrix proj = rlGetMatrixProjection();

    // Matriz que leva um ponto do mundo para o espaco da luz (usada no shader e no culling).
    m_lightVP = MatrixMultiply(view, proj);

    drawDepth(Frustum::FromMatrix(m_lightVP));
    EndMode3D();

    EndTextureMode();

    ExtractClipPlanes(proj, m_perspective, m_near, m_far);
}

void ShadowMap::Bind(int slot) const
{
    rlActiveTextureSlot(slot);
    rlEnableTexture(m_target.depth.id);
    rlActiveTextureSlot(0);
}

void ShadowMap::Unbind(int slot) const
{
    rlActiveTextureSlot(slot);
    rlDisableTexture();
    rlActiveTextureSlot(0);
}
