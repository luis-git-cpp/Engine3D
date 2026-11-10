#include "Menu.h"

#include <algorithm>
#include <utility>

Menu::Menu(const UiFont& font, std::string title)
    : m_font(&font), m_title(std::move(title))
{
}

void Menu::SetItems(const std::vector<std::string>& labels)
{
    m_items = labels;
    if (m_selected >= static_cast<int>(m_items.size()))
        m_selected = 0;
}

void Menu::SetLabel(int index, const std::string& label)
{
    if (index >= 0 && index < static_cast<int>(m_items.size()))
        m_items[index] = label;
}

Menu::Layout Menu::BuildLayout() const
{
    Layout l;

    const float sw = static_cast<float>(GetScreenWidth());
    const float sh = static_cast<float>(GetScreenHeight());

    // Tudo escala com a altura da janela (referencia: 720 px).
    l.scale = sh / 720.0f;
    l.itemSize = 34.0f * l.scale;
    l.titleSize = 64.0f * l.scale;

    const float rowH = l.itemSize * 1.5f;
    const float gap = 10.0f * l.scale;
    const float titleGap = 40.0f * l.scale;
    const float pad = 24.0f * l.scale;

    float widest = 0.0f;
    for (const std::string& label : m_items)
        widest = std::max(widest, m_font->Measure(label, l.itemSize).x);
    const float rowW = std::max(widest + pad * 2.0f, 560.0f * l.scale);

    const Vector2 titleDim = m_title.empty() ? Vector2{0.0f, 0.0f} : m_font->Measure(m_title, l.titleSize);
    const float titleBlock = m_title.empty() ? 0.0f : titleDim.y + titleGap;

    const int count = static_cast<int>(m_items.size());
    const float itemsBlock = count > 0 ? count * rowH + (count - 1) * gap : 0.0f;

    float y = (sh - (titleBlock + itemsBlock)) * 0.5f;

    l.titlePos = {(sw - titleDim.x) * 0.5f, y};
    y += titleBlock;

    for (const std::string& label : m_items)
    {
        const Vector2 dim = m_font->Measure(label, l.itemSize);
        l.rows.push_back({(sw - rowW) * 0.5f, y, rowW, rowH});
        l.textPos.push_back({(sw - dim.x) * 0.5f, y + (rowH - dim.y) * 0.5f});
        y += rowH + gap;
    }

    return l;
}

Menu::Result Menu::Update()
{
    Result result;

    const int count = static_cast<int>(m_items.size());
    if (count == 0) return result;

    // Teclado
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) m_selected = (m_selected + 1) % count;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))   m_selected = (m_selected + count - 1) % count;
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))  result.delta = -1;
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) result.delta = 1;
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_SPACE))
        result.activated = m_selected;

    // Mouse: passar por cima seleciona (so se o mouse mexeu, para nao brigar com o teclado).
    const Layout layout = BuildLayout();
    const Vector2 mouse = GetMousePosition();
    const Vector2 move = GetMouseDelta();
    const bool moved = (move.x != 0.0f) || (move.y != 0.0f);

    for (int i = 0; i < count; i++)
    {
        if (!CheckCollisionPointRec(mouse, layout.rows[i])) continue;

        if (moved) m_selected = i;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            m_selected = i;
            result.activated = i;
        }
    }

    return result;
}

void Menu::Draw() const
{
    const Layout layout = BuildLayout();

    if (!m_title.empty())
        m_font->Draw(m_title, layout.titlePos, layout.titleSize, WHITE);

    for (int i = 0; i < static_cast<int>(m_items.size()); i++)
    {
        const bool selected = (i == m_selected);
        const Rectangle& row = layout.rows[i];

        if (selected)
        {
            DrawRectangleRec(row, Fade(SKYBLUE, 0.22f));
            DrawRectangleLinesEx(row, std::max(1.0f, 2.0f * layout.scale), Fade(SKYBLUE, 0.8f));
        }

        m_font->Draw(m_items[i], layout.textPos[i], layout.itemSize, selected ? WHITE : Fade(WHITE, 0.6f));
    }
}
