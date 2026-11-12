#pragma once

#include <string>
#include <vector>

#include "UiFont.h"
#include "raylib.h"

// Menu vertical simples, centralizado na tela. Teclado (setas/WASD, Enter) e mouse.
class Menu
{
public:
    struct Result
    {
        int activated = -1; // item ativado por Enter/Espaco/clique (-1 = nenhum)
        int delta = 0;      // -1 / +1 quando esquerda/direita foi pressionada
    };

    Menu(const UiFont& font, std::string title);

    void SetItems(const std::vector<std::string>& labels);
    void SetLabel(int index, const std::string& label);
    void Reset() { m_selected = 0; }
    int Selected() const { return m_selected; }

    Result Update();
    void Draw() const;

private:
    struct Layout
    {
        float scale = 1.0f;
        float itemSize = 0.0f;
        float titleSize = 0.0f;
        Vector2 titlePos{0.0f, 0.0f};
        std::vector<Rectangle> rows;
        std::vector<Vector2> textPos;
    };

    Layout BuildLayout() const;

    const UiFont* m_font;
    std::string m_title;
    std::vector<std::string> m_items;
    int m_selected = 0;
};
