#include "Window.h"

#include <algorithm>

#include "raylib.h"

Window::Window(int width, int height, const std::string& title, int targetFps)
{
    // Sem FLAG_WINDOW_RESIZABLE: o tamanho da janela so muda pelo menu de opcoes.
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(width, height, title.c_str());
    SetTargetFPS(targetFps);

    // ESC nao fecha mais a janela (o botao X da janela continua fechando).
    SetExitKey(KEY_NULL);

    // Resolucoes que cabem no monitor.
    const Resolution monitor = MonitorResolution();
    const Resolution candidates[] = {
        {1280, 720}, {1366, 768}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}};

    for (const Resolution& r : candidates)
    {
        if (r.width <= monitor.width && r.height <= monitor.height)
            m_resolutions.push_back(r);
    }

    // Garante que a resolucao inicial esteja na lista.
    auto isInitial = [width, height](const Resolution& r) { return r.width == width && r.height == height; };
    if (std::none_of(m_resolutions.begin(), m_resolutions.end(), isInitial))
        m_resolutions.push_back({width, height});

    std::sort(m_resolutions.begin(), m_resolutions.end(),
              [](const Resolution& a, const Resolution& b) { return a.width * a.height < b.width * b.height; });

    m_resolutionIndex = static_cast<int>(
        std::find_if(m_resolutions.begin(), m_resolutions.end(), isInitial) - m_resolutions.begin());
}

Window::~Window()
{
    CloseWindow();
}

bool Window::ShouldClose() const
{
    return WindowShouldClose();
}

int Window::Width() const
{
    return GetScreenWidth();
}

int Window::Height() const
{
    return GetScreenHeight();
}

Resolution Window::MonitorResolution() const
{
    const int monitor = GetCurrentMonitor();
    return {GetMonitorWidth(monitor), GetMonitorHeight(monitor)};
}

void Window::SetDisplayMode(DisplayMode mode)
{
    Apply(mode);
}

void Window::SetResolutionIndex(int index)
{
    if (m_resolutions.empty()) return;

    m_resolutionIndex = std::clamp(index, 0, static_cast<int>(m_resolutions.size()) - 1);

    // Em "sem bordas" a janela acompanha o monitor; a escolha so vale ao sair desse modo.
    if (m_mode != DisplayMode::Borderless)
        Apply(m_mode);
}

void Window::Apply(DisplayMode target)
{
    const Resolution res = m_resolutions[m_resolutionIndex];

    // 1) Volta para uma janela comum, saindo do modo atual.
    if (m_mode == DisplayMode::Fullscreen)      ToggleFullscreen();
    else if (m_mode == DisplayMode::Borderless) ToggleBorderlessWindowed();
    m_mode = DisplayMode::Windowed;

    // 2) Entra no modo pedido.
    switch (target)
    {
        case DisplayMode::Windowed:
            SetWindowSize(res.width, res.height);
            CenterOnMonitor();
            break;

        case DisplayMode::Fullscreen:
            // O ToggleFullscreen usa o tamanho atual da janela como resolucao da tela cheia.
            SetWindowSize(res.width, res.height);
            ToggleFullscreen();
            break;

        case DisplayMode::Borderless:
            ToggleBorderlessWindowed();
            break;
    }

    m_mode = target;
}

void Window::CenterOnMonitor() const
{
    const int monitor = GetCurrentMonitor();
    const Vector2 origin = GetMonitorPosition(monitor);
    const int x = static_cast<int>(origin.x) + (GetMonitorWidth(monitor) - GetScreenWidth()) / 2;
    const int y = static_cast<int>(origin.y) + (GetMonitorHeight(monitor) - GetScreenHeight()) / 2;
    SetWindowPosition(x, y);
}
