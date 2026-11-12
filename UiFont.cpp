#include "UiFont.h"

#include "Assets.h"

UiFont::UiFont(const std::string& path)
{
    // Para PNG, o LoadFont trata a imagem como sprite font: magenta separa os glifos
    // e o primeiro glifo e o caractere 32 (espaco). Se falhar, cai na fonte padrao do raylib.
    m_font = LoadFont(ResolveAssetPath(path).c_str());
    SetTextureFilter(m_font.texture, TEXTURE_FILTER_BILINEAR);
}

UiFont::~UiFont()
{
    UnloadFont(m_font); // ignora a fonte padrao do raylib, se foi o fallback
}

void UiFont::Draw(const std::string& text, Vector2 position, float size, Color tint) const
{
    DrawTextEx(m_font, text.c_str(), position, size, Spacing(size), tint);
}

Vector2 UiFont::Measure(const std::string& text, float size) const
{
    return MeasureTextEx(m_font, text.c_str(), size, Spacing(size));
}
