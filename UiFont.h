#pragma once

#include <string>

#include "raylib.h"

// Fonte bitmap (sprite font) usada em todo o texto da engine.
// Formato: PNG com os caracteres ASCII 32..127 em grade, separados por magenta.
// ATENCAO: so existem glifos ASCII (sem acentos nem cedilha).
class UiFont
{
public:
    explicit UiFont(const std::string& path);
    ~UiFont();

    UiFont(const UiFont&) = delete;
    UiFont& operator=(const UiFont&) = delete;

    // size = altura do texto em pixels.
    void Draw(const std::string& text, Vector2 position, float size, Color tint = WHITE) const;
    Vector2 Measure(const std::string& text, float size) const;

private:
    static float Spacing(float size) { return size / 10.0f; }

    Font m_font{};
};
