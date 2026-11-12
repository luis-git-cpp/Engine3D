#pragma once

#include <string>
#include <vector>

enum class DisplayMode
{
    Windowed = 0,   // janela comum
    Fullscreen = 1, // tela cheia exclusiva (usa a resolucao escolhida)
    Borderless = 2  // janela sem bordas do tamanho do monitor
};

struct Resolution
{
    int width = 0;
    int height = 0;
};

// Encapsula a janela do raylib (RAII) e o modo de exibicao/resolucao.
class Window
{
public:
    Window(int width, int height, const std::string& title, int targetFps = 60);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool ShouldClose() const;
    int Width() const;
    int Height() const;

    DisplayMode Mode() const { return m_mode; }
    void SetDisplayMode(DisplayMode mode);

    // Resolucoes disponiveis (as que cabem no monitor). Valem para janela e tela cheia;
    // em "sem bordas" a janela sempre usa a resolucao do monitor.
    const std::vector<Resolution>& Resolutions() const { return m_resolutions; }
    int ResolutionIndex() const { return m_resolutionIndex; }
    void SetResolutionIndex(int index);

    Resolution MonitorResolution() const;

private:
    void Apply(DisplayMode target);
    void CenterOnMonitor() const;

    DisplayMode m_mode = DisplayMode::Windowed;
    std::vector<Resolution> m_resolutions;
    int m_resolutionIndex = 0;
};
