#include "looks.h"

#include <cmath>

#include "i18n.h"
#include "ui_kit.h"

int colorIndexFor(Side side, int color) {
    return (color >= 0 && color < PALETTE_COUNT) ? color : (side == Side::Players ? 0 : 1);
}
int iconIndexFor(Side side, int icon) {
    return (icon >= 0 && icon < ICON_COUNT) ? icon : (side == Side::Players ? 0 : 1);
}
ImU32 creatureColor(Side side, const Creature& c) {
    return PALETTE[colorIndexFor(side, c.getColor())].col;
}

// Draws icon `icon` (an index into ICON_NAMES) as a line glyph in the box [o, o + s].
void drawIcon(ImDrawList* dl, int icon, ImVec2 o, float s, float thick, ImU32 col) {
    auto P = [&](float x, float y) { return ImVec2(o.x + x * s, o.y + y * s); };
    switch (icon) {
        case 0: drawHeart(dl, o, s, thick, col); break;
        case 1: drawBolt(dl, o, s, thick, col); break;
        case 2: {  // shield
            ImVec2 pts[] = {P(0.18f, 0.14f), P(0.82f, 0.14f), P(0.82f, 0.48f), P(0.5f, 0.9f), P(0.18f, 0.48f)};
            dl->AddPolyline(pts, 5, col, ImDrawFlags_Closed, thick);
            dl->AddLine(P(0.5f, 0.26f), P(0.5f, 0.72f), col, thick);
            break;
        }
        case 3: {  // sword
            dl->AddLine(P(0.3f, 0.7f), P(0.84f, 0.16f), col, thick);
            dl->AddLine(P(0.84f, 0.16f), P(0.86f, 0.14f), col, thick);
            dl->AddLine(P(0.2f, 0.56f), P(0.44f, 0.8f), col, thick);
            dl->AddLine(P(0.3f, 0.7f), P(0.13f, 0.87f), col, thick);
            break;
        }
        case 4: {  // skull
            dl->AddCircle(P(0.5f, 0.42f), 0.3f * s, col, 0, thick);
            dl->AddRect(P(0.36f, 0.64f), P(0.64f, 0.86f), col, 0.06f * s, 0, thick);
            dl->AddCircleFilled(P(0.39f, 0.44f), 0.075f * s, col);
            dl->AddCircleFilled(P(0.61f, 0.44f), 0.075f * s, col);
            break;
        }
        case 5: {  // star
            ImVec2 pts[10];
            for (int i = 0; i < 10; i++) {
                const float r = (i % 2 == 0) ? 0.42f : 0.18f;
                const float a = -1.5707963f + static_cast<float>(i) * 0.6283185f;
                pts[i] = P(0.5f + std::cos(a) * r, 0.54f + std::sin(a) * r);
            }
            dl->AddPolyline(pts, 10, col, ImDrawFlags_Closed, thick);
            break;
        }
        case 6: {  // flame
            dl->PathLineTo(P(0.5f, 0.08f));
            dl->PathBezierCubicCurveTo(P(0.7f, 0.3f), P(0.86f, 0.5f), P(0.7f, 0.76f));
            dl->PathBezierCubicCurveTo(P(0.62f, 0.92f), P(0.38f, 0.92f), P(0.3f, 0.76f));
            dl->PathBezierCubicCurveTo(P(0.14f, 0.5f), P(0.3f, 0.3f), P(0.5f, 0.08f));
            dl->PathStroke(col, ImDrawFlags_Closed, thick);
            dl->PathLineTo(P(0.5f, 0.5f));
            dl->PathBezierCubicCurveTo(P(0.62f, 0.62f), P(0.6f, 0.8f), P(0.5f, 0.82f));
            dl->PathBezierCubicCurveTo(P(0.4f, 0.8f), P(0.38f, 0.62f), P(0.5f, 0.5f));
            dl->PathStroke(col, ImDrawFlags_Closed, thick * 0.8f);
            break;
        }
        case 7: {  // paw
            dl->AddCircle(P(0.5f, 0.66f), 0.17f * s, col, 0, thick);
            dl->AddCircle(P(0.22f, 0.42f), 0.08f * s, col, 0, thick);
            dl->AddCircle(P(0.39f, 0.25f), 0.08f * s, col, 0, thick);
            dl->AddCircle(P(0.61f, 0.25f), 0.08f * s, col, 0, thick);
            dl->AddCircle(P(0.78f, 0.42f), 0.08f * s, col, 0, thick);
            break;
        }
        case 8: {  // d20
            ImVec2 hex[] = {P(0.5f, 0.07f), P(0.87f, 0.29f), P(0.87f, 0.71f), P(0.5f, 0.93f), P(0.13f, 0.71f), P(0.13f, 0.29f)};
            dl->AddPolyline(hex, 6, col, ImDrawFlags_Closed, thick);
            ImVec2 tri[] = {P(0.5f, 0.3f), P(0.71f, 0.64f), P(0.29f, 0.64f)};
            dl->AddPolyline(tri, 3, col, ImDrawFlags_Closed, thick * 0.85f);
            dl->AddLine(tri[0], hex[0], col, thick * 0.85f);
            dl->AddLine(tri[1], hex[2], col, thick * 0.85f);
            dl->AddLine(tri[2], hex[4], col, thick * 0.85f);
            break;
        }
        case 9: {  // crown
            ImVec2 pts[] = {P(0.13f, 0.76f), P(0.13f, 0.3f), P(0.33f, 0.52f), P(0.5f, 0.22f),
                            P(0.67f, 0.52f), P(0.87f, 0.3f), P(0.87f, 0.76f)};
            dl->AddPolyline(pts, 7, col, ImDrawFlags_Closed, thick);
            break;
        }
        case 10: {  // moon
            dl->PathArcTo(P(0.5f, 0.5f), 0.38f * s, 0.35f * 3.1415927f, 1.65f * 3.1415927f, 20);
            dl->PathArcTo(P(0.66f, 0.46f), 0.3f * s, 1.45f * 3.1415927f, 0.55f * 3.1415927f, 16);
            dl->PathStroke(col, ImDrawFlags_Closed, thick);
            break;
        }
        default: {  // eye
            dl->PathLineTo(P(0.08f, 0.5f));
            dl->PathBezierCubicCurveTo(P(0.32f, 0.16f), P(0.68f, 0.16f), P(0.92f, 0.5f));
            dl->PathBezierCubicCurveTo(P(0.68f, 0.84f), P(0.32f, 0.84f), P(0.08f, 0.5f));
            dl->PathStroke(col, ImDrawFlags_Closed, thick);
            dl->AddCircle(P(0.5f, 0.5f), 0.17f * s, col, 0, thick);
            dl->AddCircleFilled(P(0.5f, 0.5f), 0.08f * s, col);
            break;
        }
    }
}

void drawCreatureIcon(ImDrawList* dl, Side side, const Creature& c, ImVec2 o, float s, float thick, ImU32 col) {
    drawIcon(dl, iconIndexFor(side, c.getIcon()), o, s, thick, col);
}

// Color swatches and icon tiles for picking a card's look (-1 = the default for its
// side). Lays itself out in fixed rows so it works in auto-sized windows. Returns
// true when something changed.
bool lookPicker(int* color, int* icon, Side side, float S) {
    bool changed = false;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 shown = PALETTE[colorIndexFor(side, *color)].col;

    // Colors: "default" first, then the palette, in one row.
    ImGui::TextDisabled("%s", T("Color"));
    {
        const float d = 24 * S, gap = 7 * S;
        const ImVec2 start = ImGui::GetCursorScreenPos();
        for (int i = -1; i < PALETTE_COUNT; i++) {
            const ImVec2 a(start.x + static_cast<float>(i + 1) * (d + gap), start.y);
            const ImVec2 c(a.x + d * 0.5f, a.y + d * 0.5f);
            ImGui::SetCursorScreenPos(a);
            ImGui::PushID(i + 100);
            if (ImGui::InvisibleButton("swatch", ImVec2(d, d))) {
                *color = i;
                changed = true;
            }
            const bool hovered = ImGui::IsItemHovered();
            if (hovered) ImGui::SetTooltip("%s", T(i < 0 ? "Default" : PALETTE[i].name));
            ImGui::PopID();
            const ImU32 fill = i < 0 ? PALETTE[side == Side::Players ? 0 : 1].col : PALETTE[i].col;
            dl->AddCircleFilled(c, d * 0.5f - 2 * S, fill);
            if (i < 0) {  // "default": a white slash marks it
                dl->AddLine(ImVec2(c.x - d * 0.22f, c.y + d * 0.22f), ImVec2(c.x + d * 0.22f, c.y - d * 0.22f), COL_WHITE, 2.0f * S);
            }
            if (*color == i) dl->AddCircle(c, d * 0.5f + 1.5f * S, COL_INK, 0, 2.5f * S);
            else if (hovered) dl->AddCircle(c, d * 0.5f + 1.5f * S, COL_BORDER_HI, 0, 1.5f * S);
        }
        ImGui::SetCursorScreenPos(start);
        ImGui::Dummy(ImVec2(static_cast<float>(PALETTE_COUNT + 1) * (d + gap), d));
    }

    // Icons: "default" first, then the set, seven per row, drawn in the chosen color.
    ImGui::TextDisabled("%s", T("Icon"));
    {
        const float t = 36 * S, gap = 7 * S;
        const int perRow = 7;
        const ImVec2 start = ImGui::GetCursorScreenPos();
        int rows = 0;
        for (int i = -1; i < ICON_COUNT; i++) {
            const int k = i + 1;
            const ImVec2 a(start.x + static_cast<float>(k % perRow) * (t + gap), start.y + static_cast<float>(k / perRow) * (t + gap));
            rows = k / perRow + 1;
            ImGui::SetCursorScreenPos(a);
            ImGui::PushID(i + 200);
            if (ImGui::InvisibleButton("icon", ImVec2(t, t))) {
                *icon = i;
                changed = true;
            }
            const bool hovered = ImGui::IsItemHovered();
            if (hovered) ImGui::SetTooltip("%s", T(i < 0 ? "Default" : ICON_NAMES[i]));
            ImGui::PopID();
            const bool on = *icon == i;
            dl->AddRectFilled(a, a + ImVec2(t, t), on ? withAlpha(shown, 40) : (hovered ? COL_RAISED_HI : COL_TRACK), 9 * S);
            if (on) dl->AddRect(a, a + ImVec2(t, t), COL_INK, 9 * S, 0, 2.0f * S);
            const int glyph = i < 0 ? (side == Side::Players ? 0 : 1) : i;
            drawIcon(dl, glyph, a + ImVec2(t * 0.17f, t * 0.17f), t * 0.66f, 2.0f * S, i < 0 ? COL_MUTED : shown);
        }
        ImGui::SetCursorScreenPos(start);
        ImGui::Dummy(ImVec2(static_cast<float>(perRow) * (t + gap), static_cast<float>(rows) * (t + gap) - gap));
    }
    return changed;
}
