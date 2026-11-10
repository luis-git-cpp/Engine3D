#include "Application.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "raylib.h"
#include "raymath.h"

namespace
{
constexpr const char* kFontPath = "resources/fonts/custom_jupiter_crash.png";

const char* kDisplayModeNames[3] = {"Janela", "Tela cheia", "Sem bordas"};

// Linhas do menu de opcoes.
constexpr int kOptionDisplayMode = 0;
constexpr int kOptionResolution = 1;
constexpr int kOptionShadows = 2;
constexpr int kOptionBack = 3;

constexpr std::size_t kMaxSpawned = 64;          // limite de bolas criadas pelo jogador
constexpr Vector3 kPlayerStart = {0.0f, 0.0f, 8.0f};
constexpr float kFreeCameraRadius = 0.3f;        // espessura da camera livre nos testes de colisao
constexpr float kAimDistance = 50.0f;
constexpr float kPushImpulse = 8.0f;             // clique: empurra o objeto mirado
constexpr float kHardImpactSpeed = 4.0f;         // impactos acima disso entram no registro de eventos
constexpr std::size_t kMaxLogLines = 5;
constexpr double kLogLifetime = 6.0;
} // namespace

Application::Application(int width, int height, const std::string& title)
    : m_window(width, height, title),
      m_font(kFontPath),
      m_camera({6.0f, 5.0f, 8.0f}, {0.0f, 1.0f, 0.0f}, 60.0f),
      m_lights(),
      m_objects(m_lights),
      m_physics(m_objects),
      m_scene(m_objects, m_physics),
      m_player(m_objects, m_physics),
      m_mainMenu(m_font, "ENGINE 3D"),
      m_optionsMenu(m_font, "OPCOES"),
      m_pauseMenu(m_font, "PAUSADO")
{
    SetupLights();
    SetupMenus();

    // Todos os eventos de colisao passam por aqui (alem do callback de cada objeto).
    m_physics.SetEventCallback([this](const CollisionEvent& e) { OnCollisionEvent(e); });

    // Fundo do menu principal: camera girando em volta da cena.
    m_camera.SetMode(CAMERA_ORBITAL);
}

// ------------------------------------------------------------------ configuracao

void Application::SetupLights()
{
    // Luz global: ambiente fraco e azulado + "sol" direcional.
    m_lights.SetAmbient({120, 130, 160, 255}, 0.15f);
    m_sunLight = m_lights.AddDirectional({-0.4f, -1.0f, -0.3f}, {255, 244, 214, 255}, 0.8f);

    // Point: lampada laranja que orbita o cubo (posicao atualizada em UpdateLights). Nao projeta sombra.
    m_pointLight = m_lights.AddPoint({3.0f, 1.5f, 0.0f}, {255, 140, 40, 255}, 2.5f, 9.0f);

    // Spot fixo (holofote): fica no alto, apontando para o cubo.
    m_spotLight = m_lights.AddSpot({-6.0f, 7.0f, 6.0f}, {0.0f, 1.0f, 0.0f}, WHITE,
                                   3.0f, 30.0f, 10.0f, 18.0f);

    // Spot lanterna: presa a camera (posicao e direcao atualizadas em UpdateLights).
    m_flashLight = m_lights.AddSpot({0.0f, 5.0f, 5.0f}, {0.0f, 1.0f, 0.0f}, {255, 255, 220, 255},
                                    2.0f, 30.0f, 12.0f, 22.0f);

    // Sol, holofote e lanterna projetam sombra ao mesmo tempo (castShadows e true por padrao).
    // Regiao coberta pela sombra do sol: centro e raio da cena (o piso tem 20x20).
    m_lights.SetShadowFocus({0.0f, 0.0f, 0.0f}, 15.0f);
}

void Application::SetupMenus()
{
    // Os textos nao podem ter acentos: a fonte so tem caracteres ASCII.
    m_mainMenu.SetItems({"Iniciar", "Opcoes", "Sair"});
    m_pauseMenu.SetItems({"Retornar", "Voltar ao menu principal", "Fechar"});
    m_optionsMenu.SetItems({"", "", "", "Voltar"}); // textos das 3 primeiras linhas: RefreshOptionsLabels
    RefreshOptionsLabels();
}

// ------------------------------------------------------------------ estados

void Application::SetState(State state)
{
    m_state = state;

    if (state == State::Playing)
    {
        DisableCursor(); // mouse capturado para olhar ao redor
        m_skipCameraFrame = true; // evita salto de camera no primeiro frame
    }
    else
    {
        EnableCursor();
    }

    switch (state)
    {
        case State::MainMenu: m_mainMenu.Reset();    break;
        case State::Options:  m_optionsMenu.Reset(); break;
        case State::Paused:   m_pauseMenu.Reset();   break;
        case State::Playing:  break;
    }
}

void Application::StartGame()
{
    // Cada "Iniciar" recria o mundo de demonstracao (as bolas caem de novo).
    m_spawned.clear();
    m_eventLog.clear();
    m_scene.Reset();

    m_camera.Reset();
    m_player.Spawn(kPlayerStart, 0.0f);
    m_cameraMode = CameraMode::ThirdPerson;
    m_aimHit = RaycastHit{};

    SetState(State::Playing);
}

void Application::GoToMainMenu()
{
    m_camera.Reset();
    m_camera.SetMode(CAMERA_ORBITAL);
    SetState(State::MainMenu);
}

void Application::Run()
{
    while (!m_quit && !m_window.ShouldClose())
    {
        Update();
        Draw();
    }
}

// ------------------------------------------------------------------ update

void Application::Update()
{
    if (m_state != State::Paused)
        m_time += GetFrameTime();

    switch (m_state)
    {
        case State::MainMenu: UpdateMainMenu(); break;
        case State::Options:  UpdateOptions();  break;
        case State::Playing:  UpdatePlaying();  break;
        case State::Paused:   UpdatePaused();   break;
    }

    // Fisica: so roda durante o jogo (menus e pausa congelam o mundo).
    if (m_state == State::Playing)
        m_physics.Step(GetFrameTime());

    // Libera de fato os objetos destruidos neste frame (por tecla, por queda ou por Reset).
    m_objects.CollectGarbage();

    UpdateLights();
}

void Application::UpdateMainMenu()
{
    m_camera.Update(); // modo orbital: gira sozinha

    switch (m_mainMenu.Update().activated)
    {
        case 0: StartGame();                break;
        case 1: SetState(State::Options);   break;
        case 2: m_quit = true;              break;
        default: break;
    }
}

void Application::UpdateOptions()
{
    m_camera.Update();

    if (IsKeyPressed(KEY_ESCAPE))
    {
        SetState(State::MainMenu);
        return;
    }

    const Menu::Result result = m_optionsMenu.Update();
    const int row = (result.activated >= 0) ? result.activated : m_optionsMenu.Selected();

    // Setas mudam o valor; Enter/clique avanca para o proximo valor.
    int change = result.delta;
    if (change == 0 && result.activated >= 0) change = 1;

    if (change != 0)
    {
        if (row == kOptionDisplayMode) CycleDisplayMode(change);
        if (row == kOptionResolution)  CycleResolution(change);
        if (row == kOptionShadows)     ToggleShadows();
    }

    if (result.activated == kOptionBack)
    {
        SetState(State::MainMenu);
        return;
    }

    RefreshOptionsLabels();
}

void Application::UpdatePlaying()
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        SetState(State::Paused);
        return;
    }

    HandleGameInput();

    const float dt = GetFrameTime();
    const bool lookEnabled = !m_skipCameraFrame;
    m_skipCameraFrame = false;

    switch (m_cameraMode)
    {
        case CameraMode::Free:
            if (lookEnabled) m_camera.Update();
            PushCameraOutOfWalls();
            break;

        case CameraMode::FirstPerson:
        case CameraMode::ThirdPerson:
            m_player.Update(dt, lookEnabled);
            m_camera.SetCamera(m_player.BuildCamera(m_cameraMode == CameraMode::ThirdPerson, dt));
            break;
    }

    UpdateAim();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        PushAimedObject();
}

void Application::UpdatePaused()
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        SetState(State::Playing);
        return;
    }

    switch (m_pauseMenu.Update().activated)
    {
        case 0: SetState(State::Playing); break; // Retornar
        case 1: GoToMainMenu();           break; // Voltar ao menu principal
        case 2: m_quit = true;            break; // Fechar
        default: break;
    }
}

void Application::UpdateLights()
{
    const Camera3D& cam = m_camera.Get();

    if (Light* point = m_lights.Get(m_pointLight))
        point->position = {3.0f * std::cos(m_time), 1.5f, 3.0f * std::sin(m_time)};

    if (Light* flash = m_lights.Get(m_flashLight))
    {
        flash->position = cam.position;
        flash->direction = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    }

    m_lights.Update(cam);
}

// ------------------------------------------------------------------ jogo

void Application::HandleGameInput()
{
    if (IsKeyPressed(KEY_ONE))   SetCameraMode(CameraMode::Free);
    if (IsKeyPressed(KEY_TWO))   SetCameraMode(CameraMode::FirstPerson);
    if (IsKeyPressed(KEY_THREE)) SetCameraMode(CameraMode::ThirdPerson);

    if (IsKeyPressed(KEY_R))
    {
        if (m_cameraMode == CameraMode::Free) m_camera.Reset();
        else                                  m_player.Spawn(kPlayerStart, 0.0f);
    }

    if (IsKeyPressed(KEY_FOUR))  ToggleLight(m_sunLight);
    if (IsKeyPressed(KEY_FIVE))  ToggleLight(m_pointLight);
    if (IsKeyPressed(KEY_SIX))   ToggleLight(m_spotLight);
    if (IsKeyPressed(KEY_SEVEN)) ToggleLight(m_flashLight);
    if (IsKeyPressed(KEY_EIGHT)) ToggleShadows();

    if (IsKeyPressed(KEY_B))     SpawnBall(0);
    if (IsKeyPressed(KEY_N))     SpawnBall(1);
    if (IsKeyPressed(KEY_X))     DestroyLastSpawned();
    if (IsKeyPressed(KEY_NINE))  m_showColliders = !m_showColliders;
}

void Application::SetCameraMode(CameraMode mode)
{
    if (mode == m_cameraMode) return;

    const bool wasFree = (m_cameraMode == CameraMode::Free);
    m_cameraMode = mode;

    if (mode == CameraMode::Free)
    {
        m_camera.SetMode(CAMERA_FREE);
        m_player.SetVisible(true);
    }
    else if (wasFree)
    {
        // Mantem para onde a camera estava olhando.
        const Camera3D& cam = m_camera.Get();
        m_player.LookAt(Vector3Subtract(cam.target, cam.position));
    }
}

// Colisao da camera livre: uma esfera em volta da camera e empurrada para fora de paredes e do chao.
void Application::PushCameraOutOfWalls()
{
    Camera3D& cam = m_camera.Mutable();

    QueryFilter filter;
    filter.layerMask = m_physics.LayerMask(m_player.Layer());
    filter.ignore = m_player.Id();
    filter.includeDynamic = false; // objetos soltos nao empurram a camera

    const Vector3 corrected = m_physics.PushOutSphere(cam.position, kFreeCameraRadius, filter);
    const Vector3 delta = Vector3Subtract(corrected, cam.position);

    cam.position = corrected;
    cam.target = Vector3Add(cam.target, delta); // o alvo acompanha para nao mudar a direcao
}

// Raycast do centro da tela: descobre o que o jogador esta mirando.
void Application::UpdateAim()
{
    const Camera3D& cam = m_camera.Get();
    const Vector3 direction = Vector3Normalize(Vector3Subtract(cam.target, cam.position));

    QueryFilter filter;
    filter.ignore = m_player.Id();

    m_physics.Raycast(cam.position, direction, kAimDistance, m_aimHit, filter);
}

// Clique: da um empurrao no objeto dinamico mirado.
void Application::PushAimedObject()
{
    if (!m_aimHit.hit) return;

    GameObject* obj = m_objects.Get(m_aimHit.object);
    if (obj == nullptr || !obj->IsDynamic()) return;

    const Camera3D& cam = m_camera.Get();
    const Vector3 direction = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    obj->velocity = Vector3Add(obj->velocity, Vector3Scale(direction, kPushImpulse * obj->InverseMass()));
}

// Recebe todos os eventos de colisao. Aqui so registramos os mais interessantes na tela.
void Application::OnCollisionEvent(const CollisionEvent& e)
{
    if (e.type == CollisionEventType::Stay) return;

    const GameObject* a = m_objects.Get(e.self);
    const GameObject* b = m_objects.Get(e.other);
    if (a == nullptr && b == nullptr) return;

    const std::string nameA = a ? a->name : "(removido)";
    const std::string nameB = b ? b->name : "(removido)";

    if (e.trigger)
    {
        // Descobre qual dos dois e o gatilho para montar a frase "X entrou em PLACA".
        const bool aIsTrigger = (a != nullptr && a->isTrigger);
        const std::string& who = aIsTrigger ? nameB : nameA;
        const std::string& zone = aIsTrigger ? nameA : nameB;
        const char* verb = (e.type == CollisionEventType::Enter) ? " entrou em " : " saiu de ";

        AddLog("gatilho: " + who + verb + zone);
    }
    else if (e.type == CollisionEventType::Enter && e.impactSpeed >= kHardImpactSpeed)
    {
        AddLog("impacto: " + nameA + " x " + nameB + " (" + TextFormat("%.1f", e.impactSpeed) + " m/s)");
    }
}

void Application::AddLog(const std::string& text)
{
    m_eventLog.push_back({text, GetTime()});
    if (m_eventLog.size() > kMaxLogLines)
        m_eventLog.erase(m_eventLog.begin());
}

void Application::ToggleLight(int index)
{
    if (Light* light = m_lights.Get(index))
        light->enabled = !light->enabled;
}

void Application::ToggleShadows()
{
    m_lights.SetShadowsEnabled(!m_lights.ShadowsEnabled());
}

// Cria uma bola saindo dos olhos do jogador (ou da camera, no modo livre), lancada para frente.
void Application::SpawnBall(int layer)
{
    const Camera3D& cam = m_camera.Get();
    const Vector3 forward = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    const Vector3 origin = (m_cameraMode == CameraMode::Free) ? cam.position : m_player.EyePosition();
    const Vector3 position = Vector3Add(origin, Vector3Scale(forward, 1.5f));

    const ObjectId id = m_scene.SpawnBall(position, Vector3Scale(forward, 10.0f), layer);
    if (id == InvalidObjectId) return;

    m_spawned.push_back(id);

    // Passou do limite: destroi a mais antiga.
    while (m_spawned.size() > kMaxSpawned)
    {
        m_objects.Destroy(m_spawned.front());
        m_spawned.erase(m_spawned.begin());
    }
}

// Destroi o objeto criado mais recentemente que ainda existe.
void Application::DestroyLastSpawned()
{
    while (!m_spawned.empty())
    {
        const ObjectId id = m_spawned.back();
        m_spawned.pop_back();

        if (m_objects.Get(id) != nullptr) // pula os que ja sumiram (ex.: cairam do mundo)
        {
            m_objects.Destroy(id);
            break;
        }
    }
}

void Application::CycleDisplayMode(int direction)
{
    const int count = 3;
    const int next = (static_cast<int>(m_window.Mode()) + direction + count) % count;
    m_window.SetDisplayMode(static_cast<DisplayMode>(next));
}

void Application::CycleResolution(int direction)
{
    // Em "sem bordas" a resolucao e a do monitor, entao nao ha o que escolher.
    if (m_window.Mode() == DisplayMode::Borderless) return;

    const int count = static_cast<int>(m_window.Resolutions().size());
    const int next = (m_window.ResolutionIndex() + direction + count) % count;
    m_window.SetResolutionIndex(next);
}

void Application::RefreshOptionsLabels()
{
    const DisplayMode mode = m_window.Mode();
    m_optionsMenu.SetLabel(kOptionDisplayMode, std::string("Tela: ") + kDisplayModeNames[static_cast<int>(mode)]);

    Resolution res;
    std::string suffix;
    if (mode == DisplayMode::Borderless)
    {
        res = m_window.MonitorResolution();
        suffix = " (monitor)";
    }
    else
    {
        res = m_window.Resolutions()[m_window.ResolutionIndex()];
    }

    m_optionsMenu.SetLabel(kOptionResolution,
                           "Resolucao: " + std::to_string(res.width) + "x" + std::to_string(res.height) + suffix);

    m_optionsMenu.SetLabel(kOptionShadows,
                           std::string("Sombras: ") + (m_lights.ShadowsEnabled() ? "Ligadas" : "Desligadas"));
}

// ------------------------------------------------------------------ desenho

void Application::Draw()
{
    BeginDrawing();

    // Passada 1: uma passada de profundidade por luz que projeta sombra (sol e spots).
    m_lights.RenderShadows([this] { m_scene.DrawDepth(); });

    // Passada 2: cena vista da camera, comparando a profundidade de cada pixel com os shadow maps.
    ClearBackground({20, 22, 30, 255});

    BeginMode3D(m_camera.Get());
    m_scene.Draw();
    m_lights.DrawGizmos();
    if (m_showColliders)
        m_physics.DrawDebug();
    if (m_state == State::Playing && m_aimHit.hit)
        DrawSphere(m_aimHit.point, 0.05f, YELLOW); // ponto onde o raio da mira acertou
    EndMode3D();

    switch (m_state)
    {
        case State::MainMenu:
            DrawOverlay(0.35f);
            m_mainMenu.Draw();
            break;

        case State::Options:
            DrawOverlay(0.55f);
            m_optionsMenu.Draw();
            break;

        case State::Playing:
            DrawHud();
            DrawCrosshair();
            DrawEventLog();
            break;

        case State::Paused:
            DrawHud();
            DrawEventLog();
            DrawOverlay(0.6f);
            m_pauseMenu.Draw();
            break;
    }

    EndDrawing();
}

void Application::DrawOverlay(float alpha) const
{
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, alpha));
}

void Application::DrawHud() const
{
    const float scale = static_cast<float>(GetScreenHeight()) / 720.0f;
    const float size = 20.0f * scale;
    const float x = 12.0f * scale;
    float y = 10.0f * scale;
    const float step = size * 1.3f;

    const CellCoord cell = m_objects.Coordinates().ToCell(m_camera.Get().position);

    m_font.Draw(TextFormat("FPS: %d | Objetos: %d | Celula: (%d, %d, %d)", GetFPS(), m_objects.Count(),
                           cell.x, cell.y, cell.z),
                {x, y}, size);
    y += step;
    m_font.Draw("WASD: mover | SHIFT: correr | ESPACO: pular | CLIQUE: empurrar | R: reiniciar jogador", {x, y}, size);
    y += step;
    m_font.Draw("1/2/3: camera livre / 1a pessoa / 3a pessoa", {x, y}, size);
    y += step;
    m_font.Draw(TextFormat("4: sol | 5: point | 6: holofote | 7: lanterna | 8: sombras (%s)",
                           m_lights.ShadowsEnabled() ? "ligadas" : "desligadas"),
                {x, y}, size);
    y += step;
    m_font.Draw("B/N: bola camada 0/1 | X: destruir | 9: colisores", {x, y}, size);
}

// Mira no centro da tela e o nome do que esta sendo mirado (resultado do raycast).
void Application::DrawCrosshair() const
{
    const int cx = GetScreenWidth() / 2;
    const int cy = GetScreenHeight() / 2;
    const float scale = static_cast<float>(GetScreenHeight()) / 720.0f;
    const int arm = static_cast<int>(8.0f * scale);
    const int thick = std::max(1, static_cast<int>(2.0f * scale));
    const Color color = Fade(WHITE, 0.85f);

    DrawRectangle(cx - arm, cy - thick / 2, arm * 2, thick, color);
    DrawRectangle(cx - thick / 2, cy - arm, thick, arm * 2, color);

    if (!m_aimHit.hit) return;

    const GameObject* target = m_objects.Get(m_aimHit.object);
    if (target == nullptr) return;

    const float size = 18.0f * scale;
    const std::string label = TextFormat("%s (%.1f m)", target->name.c_str(), m_aimHit.distance);
    const Vector2 dim = m_font.Measure(label, size);
    m_font.Draw(label, {cx - dim.x * 0.5f, cy + 16.0f * scale}, size, Fade(WHITE, 0.9f));
}

// Registro dos ultimos eventos de colisao, no canto inferior esquerdo.
void Application::DrawEventLog() const
{
    const float scale = static_cast<float>(GetScreenHeight()) / 720.0f;
    const float size = 18.0f * scale;
    const float step = size * 1.3f;
    const double now = GetTime();

    float y = static_cast<float>(GetScreenHeight()) - 14.0f * scale - step * static_cast<float>(m_eventLog.size());

    for (const LogLine& line : m_eventLog)
    {
        const double age = now - line.time;
        if (age < kLogLifetime)
        {
            const float alpha = age > kLogLifetime - 1.0 ? static_cast<float>(kLogLifetime - age) : 1.0f;
            m_font.Draw(line.text, {12.0f * scale, y}, size, Fade(WHITE, alpha));
        }
        y += step;
    }
}
