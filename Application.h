#pragma once

#include <string>
#include <vector>

#include "CameraController.h"
#include "LightManager.h"
#include "Menu.h"
#include "ObjectManager.h"
#include "PhysicsWorld.h"
#include "PlayerController.h"
#include "Scene.h"
#include "UiFont.h"
#include "Window.h"

class Application
{
public:
    Application(int width, int height, const std::string& title);

    void Run();

private:
    enum class State
    {
        MainMenu,
        Options,
        Playing,
        Paused
    };

    enum class CameraMode
    {
        Free,        // camera voadora do raylib, com colisao
        FirstPerson, // jogador, camera nos olhos
        ThirdPerson  // jogador, camera atras com colisao ("braco de mola")
    };

    struct LogLine
    {
        std::string text;
        double time = 0.0;
    };

    // Configuracao
    void SetupLights();
    void SetupMenus();

    // Estados
    void SetState(State state);
    void StartGame();
    void GoToMainMenu();

    void Update();
    void UpdateMainMenu();
    void UpdateOptions();
    void UpdatePlaying();
    void UpdatePaused();
    void UpdateLights();
    Vector3 StreamFocus() const; // ponto ao redor do qual o mundo e carregado (jogador ou camera)

    // Jogo
    void HandleGameInput();
    void SetCameraMode(CameraMode mode);
    void PushCameraOutOfWalls();
    void UpdateAim();
    void PushAimedObject();
    void OnCollisionEvent(const CollisionEvent& e);
    void AddLog(const std::string& text);
    void ToggleLight(int index);
    void ToggleShadows();
    void SpawnBall(int layer);
    void DestroyLastSpawned();
    void CycleDisplayMode(int direction);
    void CycleResolution(int direction);
    void RefreshOptionsLabels();

    // Desenho
    void Draw();
    void DrawOverlay(float alpha) const;
    void DrawHud() const;
    void DrawCrosshair() const;
    void DrawEventLog() const;

    // Ordem importa: a janela vem primeiro (contexto OpenGL); luzes antes dos objetos
    // (eles usam o shader); objetos antes da fisica, da cena e do jogador.
    Window m_window;
    UiFont m_font;
    CameraController m_camera;
    LightManager m_lights;
    ObjectManager m_objects;
    PhysicsWorld m_physics;
    Scene m_scene;
    PlayerController m_player;

    Menu m_mainMenu;
    Menu m_optionsMenu;
    Menu m_pauseMenu;

    State m_state = State::MainMenu;
    CameraMode m_cameraMode = CameraMode::ThirdPerson;
    bool m_quit = false;
    bool m_skipCameraFrame = false;
    bool m_showColliders = false;
    bool m_showCells = false;
    float m_time = 0.0f; // tempo de animacao (para quando o jogo esta pausado)

    RaycastHit m_aimHit;                 // o que esta no centro da tela
    std::vector<LogLine> m_eventLog;     // ultimos eventos de colisao
    std::vector<ObjectId> m_spawned;     // objetos criados pelo jogador (B/N), do mais antigo ao mais novo

    int m_sunLight = -1;
    int m_pointLight = -1;
    int m_spotLight = -1;   // holofote fixo
    int m_flashLight = -1;  // lanterna presa a camera
};
