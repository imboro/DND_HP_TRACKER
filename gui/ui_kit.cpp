#include "ui_kit.h"

#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <vector>

#include "imgui_internal.h"  // GetActiveID, the current window (section boxes)
#include "difficulty.h"      // crLabel / crXp for crCombo
#include "i18n.h"

ImFont* g_fontBold = nullptr;
ImFont* g_fontHeavy = nullptr;
// ImGui sizes a font by its line height (ascent + descent). Arial's faces have
// taller line metrics than Helvetica, so at the same size their glyphs look
// smaller; these factors bring them back to the regular font's glyph size.
static float g_boldMul = 1.0f;
static float g_heavyMul = 1.0f;

ImU32 hpColor(float fraction) {
    if (fraction > 0.5f) return COL_GREEN;
    if (fraction > 0.25f) return COL_AMBER;
    return COL_RED;
}

ImVec4 toVec4(ImU32 c) {
    return ImGui::ColorConvertU32ToFloat4(c);
}

void applyTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsLight();

    style.WindowRounding = 0.0f;
    style.ChildRounding = 16.0f;
    style.FrameRounding = 11.0f;
    style.GrabRounding = 11.0f;
    style.TabRounding = 8.0f;
    style.PopupRounding = 12.0f;
    style.ScrollbarRounding = 8.0f;
    style.ScrollbarSize = 9.0f;
    style.FrameBorderSize = 1.5f;
    style.ChildBorderSize = 2.0f;
    style.WindowBorderSize = 0.0f;
    style.PopupBorderSize = 1.5f;
    style.TabBorderSize = 0.0f;
    style.SeparatorTextBorderSize = 1.5f;
    style.WindowPadding = ImVec2(16, 16);
    style.FramePadding = ImVec2(10, 6);
    style.ItemSpacing = ImVec2(10, 9);
    style.CellPadding = ImVec2(6, 5);

    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg]       = toVec4(COL_BG);
    c[ImGuiCol_ChildBg]        = toVec4(COL_PANEL);
    c[ImGuiCol_PopupBg]        = toVec4(COL_PANEL);
    c[ImGuiCol_Border]         = toVec4(COL_INK);
    c[ImGuiCol_BorderShadow]   = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TitleBg]        = toVec4(COL_PANEL);
    c[ImGuiCol_TitleBgActive]  = toVec4(COL_PANEL);
    c[ImGuiCol_TitleBgCollapsed] = toVec4(COL_PANEL);
    c[ImGuiCol_Text]           = toVec4(COL_TEXT);
    c[ImGuiCol_TextDisabled]   = toVec4(COL_MUTED);
    c[ImGuiCol_TextSelectedBg] = toVec4(IM_COL32(250, 200, 60, 110));
    c[ImGuiCol_FrameBg]        = toVec4(COL_PANEL);
    c[ImGuiCol_FrameBgHovered] = toVec4(IM_COL32(249, 249, 247, 255));
    c[ImGuiCol_FrameBgActive]  = toVec4(IM_COL32(243, 243, 240, 255));
    c[ImGuiCol_Button]         = toVec4(COL_RAISED);
    c[ImGuiCol_ButtonHovered]  = toVec4(COL_RAISED_HI);
    c[ImGuiCol_ButtonActive]   = toVec4(COL_RAISED_LO);
    c[ImGuiCol_CheckMark]      = toVec4(COL_INK);
    c[ImGuiCol_Separator]      = toVec4(COL_BORDER);
    c[ImGuiCol_SeparatorHovered] = toVec4(COL_BORDER_HI);
    c[ImGuiCol_SeparatorActive]  = toVec4(COL_INK);
    c[ImGuiCol_ScrollbarBg]    = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab]  = toVec4(IM_COL32(208, 208, 203, 255));
    c[ImGuiCol_ScrollbarGrabHovered] = toVec4(COL_BORDER_HI);
    c[ImGuiCol_ScrollbarGrabActive]  = toVec4(COL_MUTED);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.1f, 0.1f, 0.1f, 0.28f);
    c[ImGuiCol_Header]         = toVec4(COL_RAISED_HI);
    c[ImGuiCol_HeaderHovered]  = toVec4(COL_RAISED_HI);
    c[ImGuiCol_HeaderActive]   = toVec4(COL_RAISED_LO);
    c[ImGuiCol_SliderGrab]     = toVec4(COL_INK);
    c[ImGuiCol_SliderGrabActive] = toVec4(IM_COL32(0, 0, 0, 255));
    c[ImGuiCol_TableBorderLight] = toVec4(COL_BORDER);
    c[ImGuiCol_TableBorderStrong] = toVec4(COL_BORDER);
    c[ImGuiCol_TableRowBg]     = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt]  = ImVec4(0, 0, 0, 0.025f);
    c[ImGuiCol_NavCursor]      = toVec4(COL_INK);
}

// Tries font files in order; returns null if none load.
static ImFont* loadFirstFont(const std::vector<std::string>& candidates, float pixelSize) {
    for (const std::string& path : candidates) {
        if (FILE* f = std::fopen(path.c_str(), "rb")) {
            std::fclose(f);
            if (ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), pixelSize)) return font;
        }
    }
    return nullptr;
}

// Regular, bold and heavy faces: Helvetica + Arial Bold / Arial Black on macOS,
// the Arial family on Windows. Falls back to ImGui's built-in font.
bool loadSystemFonts(float pixelSize) {
#ifdef _WIN32
    const char* windir = std::getenv("WINDIR");
    const std::string fonts = std::string(windir ? windir : "C:\\Windows") + "\\Fonts\\";
    const std::vector<std::string> helvetica;  // not shipped with Windows
    const std::vector<std::string> arial = {fonts + "arial.ttf"};
    const std::vector<std::string> arialBold = {fonts + "arialbd.ttf"};
    const std::vector<std::string> arialBlack = {fonts + "ariblk.ttf"};
#else
    const std::vector<std::string> helvetica = {"/System/Library/Fonts/Helvetica.ttc"};
    const std::vector<std::string> arial = {"/System/Library/Fonts/Supplemental/Arial.ttf", "/Library/Fonts/Arial.ttf"};
    const std::vector<std::string> arialBold = {"/System/Library/Fonts/Supplemental/Arial Bold.ttf",
                                                "/Library/Fonts/Arial Bold.ttf"};
    const std::vector<std::string> arialBlack = {"/System/Library/Fonts/Supplemental/Arial Black.ttf",
                                                 "/Library/Fonts/Arial Black.ttf"};
#endif
    ImFont* regular = loadFirstFont(helvetica, pixelSize);
    const float regularLine = regular ? 1.0f : 1.117f;  // hhea ascent + descent, in em
    if (regular == nullptr) regular = loadFirstFont(arial, pixelSize);
    if (regular == nullptr) return false;
    g_fontBold = loadFirstFont(arialBold, pixelSize);
    g_fontHeavy = loadFirstFont(arialBlack, pixelSize);
    if (g_fontBold) g_boldMul = 1.117f / regularLine;
    if (g_fontHeavy) g_heavyMul = 1.41f / regularLine;
    ImGui::GetIO().FontDefault = regular;
    return true;
}

ImFont* boldFont() { return g_fontBold ? g_fontBold : ImGui::GetFont(); }
ImFont* heavyFont() { return g_fontHeavy ? g_fontHeavy : boldFont(); }

// Text in a given face, at a size relative to the current font size.
// Positions and sizes refer to the glyphs at the regular font's scale: the
// taller line box of Arial faces is trimmed so text lines up with regular text.
static float faceMul(ImFont* font) {
    if (font == g_fontHeavy) return g_heavyMul;
    if (font == g_fontBold) return g_boldMul;
    return 1.0f;
}
void fontText(ImDrawList* dl, ImFont* font, ImVec2 pos, ImU32 col, const char* text, float sizeMul) {
    const float k = faceMul(font);
    const float size = ImGui::GetFontSize() * sizeMul;
    // Arial's extra line height is mostly above the glyphs; shift up to compensate.
    const float lift = size * (k - 1.0f) * 0.62f;
    dl->AddText(font, size * k, ImVec2(pos.x, pos.y - lift), col, text);
}
ImVec2 fontTextSize(ImFont* font, const char* text, float sizeMul) {
    const float size = ImGui::GetFontSize() * sizeMul;
    const ImVec2 ts = font->CalcTextSizeA(size * faceMul(font), FLT_MAX, 0.0f, text);
    return ImVec2(ts.x, size);
}

void boldText(ImDrawList* dl, ImVec2 pos, ImU32 col, const char* text, float sizeMul) {
    fontText(dl, boldFont(), pos, col, text, sizeMul);
}
ImVec2 boldTextSize(const char* text, float sizeMul) {
    return fontTextSize(boldFont(), text, sizeMul);
}
void heavyText(ImDrawList* dl, ImVec2 pos, ImU32 col, const char* text, float sizeMul) {
    fontText(dl, heavyFont(), pos, col, text, sizeMul);
}
ImVec2 heavyTextSize(const char* text, float sizeMul) {
    return fontTextSize(heavyFont(), text, sizeMul);
}

void sizedText(ImDrawList* dl, ImVec2 pos, ImU32 col, const char* text, float sizeMul) {
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * sizeMul, pos, col, text);
}

ImVec2 sizedTextSize(const char* text, float sizeMul) {
    const ImVec2 base = ImGui::CalcTextSize(text);
    return ImVec2(base.x * sizeMul, base.y * sizeMul);
}

// The house style: a rounded box with an ink outline and a hard shadow
// offset down and to the right. `alpha` fades the whole thing (0..1).
void inkBox(ImDrawList* dl, ImVec2 a, ImVec2 b, float round, float S, ImU32 fill,
            float shadow, ImU32 outline, float alpha, float border) {
    if (shadow > 0.0f) {
        const ImVec2 off(shadow * S, shadow * 1.2f * S);
        dl->AddRectFilled(a + off, b + off, fadeAlpha(outline, alpha), round);
    }
    dl->AddRectFilled(a, b, fadeAlpha(fill, alpha), round);
    dl->AddRect(a, b, fadeAlpha(outline, alpha), round, 0, border * S);
}

// A rounded button with an ink outline and a hard shadow that disappears
// while it is held down. With interactive == false it is only drawn (no ImGui
// item is submitted), so it cannot claim the hover from a card stacked in
// front of it.
bool flatButton(const char* label, ImVec2 size, const ButtonStyle& look, bool enabled, bool interactive) {
    const float S = UI_SCALE;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float rounding = ImGui::GetStyle().FrameRounding;
    ImVec2 drawSize = size;
    if (drawSize.x <= 0) drawSize.x = ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2;
    if (drawSize.y <= 0) drawSize.y = ImGui::GetFrameHeight();
    const ImU32 fill = enabled ? look.fill : COL_DISABLED;
    const ImU32 outline = enabled ? COL_INK : COL_BORDER_HI;
    const ImU32 text = enabled ? look.text : COL_MUTED;
    if (!enabled || !interactive) {
        if (enabled) dl->AddRectFilled(p + ImVec2(0, 2.5f * S), p + drawSize + ImVec2(0, 2.5f * S), COL_INK, rounding);
        dl->AddRectFilled(p, p + drawSize, fill, rounding);
        dl->AddRect(p, p + drawSize, outline, rounding, 0, 1.5f * S);
        const char* end = ImGui::FindRenderedTextEnd(label);
        const ImVec2 ts = ImGui::CalcTextSize(label, end);
        dl->AddText(ImVec2(p.x + (drawSize.x - ts.x) * 0.5f, p.y + (drawSize.y - ts.y) * 0.5f), text, label, end);
        // Reserve the space like a real button would, so SameLine() and the
        // following rows lay out correctly (Dummy is never hover/click-able).
        ImGui::Dummy(drawSize);
        return false;
    }
    // Hard shadow under the button, hidden while it is pressed.
    if (ImGui::GetActiveID() != ImGui::GetID(label)) {
        dl->AddRectFilled(p + ImVec2(0, 2.5f * S), p + drawSize + ImVec2(0, 2.5f * S), COL_INK, rounding);
    }
    ImGui::PushStyleColor(ImGuiCol_Button, toVec4(look.fill));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, toVec4(look.hover));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, toVec4(look.active));
    ImGui::PushStyleColor(ImGuiCol_Border, toVec4(COL_INK));
    ImGui::PushStyleColor(ImGuiCol_Text, toVec4(text));
    const bool pressed = ImGui::Button(label, drawSize);
    ImGui::PopStyleColor(5);
    return pressed;
}

// Shorthands for the common button roles.
bool primaryButton(const char* label, ImVec2 size, bool enabled) {
    return flatButton(label, size, BTN_INK, enabled);
}
bool dangerButton(const char* label, ImVec2 size, bool enabled) {
    return flatButton(label, size, BTN_RED, enabled);
}
bool greenButton(const char* label, ImVec2 size, bool enabled) {
    return flatButton(label, size, BTN_GREEN, enabled);
}
bool tempButton(const char* label, ImVec2 size, bool enabled) {
    return flatButton(label, size, BTN_TEMP, enabled);
}
bool quietButton(const char* label, ImVec2 size, bool enabled) {
    return flatButton(label, size, BTN_QUIET, enabled);
}
bool violetButton(const char* label, ImVec2 size, bool enabled) {
    return flatButton(label, size, BTN_VIOLET, enabled);
}

// Width of an ImGui::SmallButton with a one-character label ("+", "x"), border included.
float smallButtonWidth() {
    return ImGui::CalcTextSize("+").x + ImGui::GetStyle().FramePadding.x * 2.0f + 2.0f * UI_SCALE;
}

// Width for `count` buttons sharing the current row.
float rowButtonWidth(int count) {
    const float S = UI_SCALE;
    const float avail = ImGui::GetContentRegionAvail().x - 2 * S;
    return (avail - 8 * S * static_cast<float>(count - 1)) / static_cast<float>(count);
}

// ---- Section boxes ---------------------------------------------------------
// A white box with an ink outline whose heavy title sits in a gap in the top
// edge. The box is drawn behind the content using last frame's height, so
// it never needs draw-list channels (which tables inside would conflict with).

static std::unordered_map<ImGuiID, float> g_sectionHeights;
struct SectionState {
    ImGuiID id = 0;
    ImVec2 start;
    float savedContentMax = 0.0f;
    float savedWorkMax = 0.0f;
};
static SectionState g_section;

void beginSection(const char* title, const std::string& note) {
    const float S = UI_SCALE;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* dl = window->DrawList;
    const float pad = SECTION_PAD * S;
    const float shadow = 3.0f * S;
    g_section.id = ImGui::GetID(title);
    g_section.start = ImGui::GetCursorScreenPos();
    const ImVec2 start = g_section.start;
    const float right = start.x + ImGui::GetContentRegionAvail().x - shadow - 1 * S;
    const ImVec2 ts = heavyTextSize(title, 1.0f);
    const float top = start.y + ts.y * 0.5f;

    const auto it = g_sectionHeights.find(g_section.id);
    if (it != g_sectionHeights.end()) {
        inkBox(dl, ImVec2(start.x, top), ImVec2(right, start.y + it->second), 16 * S, S);
    }
    // Title in a gap of the top edge, then an optional muted note after it.
    const float lx = start.x + 16 * S;
    float gapW = ts.x;
    const ImVec2 ns = note.empty() ? ImVec2(0, 0) : ImGui::CalcTextSize(note.c_str());
    if (!note.empty()) gapW += 6 * S + ns.x;
    dl->AddRectFilled(ImVec2(lx - 7 * S, top - 3 * S), ImVec2(lx + gapW + 7 * S, top + 3 * S), COL_PANEL);
    heavyText(dl, ImVec2(lx, start.y), COL_TEXT, title);
    if (!note.empty()) dl->AddText(ImVec2(lx + ts.x + 6 * S, start.y + (ts.y - ns.y) * 0.5f), COL_MUTED, note.c_str());

    ImGui::Dummy(ImVec2(0, ts.y + 3 * S));
    ImGui::Indent(pad);
    // Narrow the content region so widths computed from it stay inside the box.
    g_section.savedContentMax = window->ContentRegionRect.Max.x;
    g_section.savedWorkMax = window->WorkRect.Max.x;
    const float inner = right - pad;
    window->ContentRegionRect.Max.x = std::min(window->ContentRegionRect.Max.x, inner);
    window->WorkRect.Max.x = std::min(window->WorkRect.Max.x, inner);
}

void endSection() {
    const float S = UI_SCALE;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const float pad = SECTION_PAD * S;
    const float shadow = 3.0f * S;
    window->ContentRegionRect.Max.x = g_section.savedContentMax;
    window->WorkRect.Max.x = g_section.savedWorkMax;
    ImGui::Unindent(pad);
    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    const float lastBottom = ImGui::GetCursorScreenPos().y - spacing;
    g_sectionHeights[g_section.id] = lastBottom + pad - g_section.start.y;
    ImGui::Dummy(ImVec2(0, std::max(0.0f, pad - spacing + shadow + 2 * S)));
}

// A labelled pill: white with an ink outline and a colored dot.
void pill(const char* text, ImU32 color, float S) {
    const ImVec2 ts = ImGui::CalcTextSize(text);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const float px = 11 * S, py = 4 * S, dot = 4 * S;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 pe(p0.x + ts.x + px * 2 + dot * 2 + 6 * S, p0.y + ts.y + py * 2);
    const float round = (pe.y - p0.y) * 0.5f;
    dl->AddRectFilled(p0, pe, COL_PANEL, round);
    dl->AddRect(p0, pe, COL_INK, round, 0, 1.5f * S);
    dl->AddCircleFilled(ImVec2(p0.x + px + dot, (p0.y + pe.y) * 0.5f), dot, color);
    dl->AddText(ImVec2(p0.x + px + dot * 2 + 6 * S, p0.y + py), COL_TEXT, text);
    ImGui::Dummy(pe - p0);
}

// Pill-shaped segmented control (used for the sidebar tabs).
void segmentedTabs(const char* const* labels, int count, int* active, float S, float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const float w = width > 0.0f ? width : ImGui::GetContentRegionAvail().x - 3 * S - 1 * S;
    const float h = height > 0.0f ? height : ImGui::GetFrameHeight() + 4 * S;
    const float labelMul = height > 0.0f ? 0.85f : 1.0f;  // a compact control gets smaller labels
    const float inset = 4 * S;
    inkBox(dl, p0, p0 + ImVec2(w, h), h * 0.5f, S);
    const float segW = (w - inset * 2) / static_cast<float>(count);
    // Where the pill is, in tabs (eased toward the active one between frames).
    static std::unordered_map<ImGuiID, float> pillPos;
    const ImGuiID key = ImGui::GetID("##segpill");
    auto found = pillPos.find(key);
    float& pill = found != pillPos.end() ? found->second : (pillPos[key] = static_cast<float>(*active));
    for (int i = 0; i < count; i++) {
        const ImVec2 a(p0.x + inset + segW * static_cast<float>(i), p0.y + inset);
        const ImVec2 b(a.x + segW, p0.y + h - inset);
        ImGui::SetCursorScreenPos(a);
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("seg", b - a)) *active = i;
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        if (hovered && *active != i) dl->AddRectFilled(a, b, COL_RAISED_HI, (b.y - a.y) * 0.5f);
    }
    pill = approach(pill, static_cast<float>(*active), 16.0f, ImGui::GetIO().DeltaTime);
    if (std::fabs(pill - static_cast<float>(*active)) < 0.002f) pill = static_cast<float>(*active);
    {
        const ImVec2 a(p0.x + inset + segW * pill, p0.y + inset);
        dl->AddRectFilled(a, ImVec2(a.x + segW, p0.y + h - inset), COL_INK, (h - inset * 2) * 0.5f);
    }
    for (int i = 0; i < count; i++) {
        const ImVec2 a(p0.x + inset + segW * static_cast<float>(i), p0.y + inset);
        const ImVec2 b(a.x + segW, p0.y + h - inset);
        // Labels turn white as the pill slides under them.
        const float under = clamp01(1.0f - std::fabs(pill - static_cast<float>(i)));
        const ImVec2 ts = boldTextSize(labels[i], labelMul);
        boldText(dl, ImVec2((a.x + b.x - ts.x) * 0.5f, (a.y + b.y - ts.y) * 0.5f),
                 mixColor(COL_MUTED, COL_WHITE, under), labels[i], labelMul);
    }
    ImGui::SetCursorScreenPos(p0);
    ImGui::Dummy(ImVec2(w, h + 3 * S));
}

void drawHeart(ImDrawList* dl, ImVec2 o, float s, float thick, ImU32 col) {
    auto pt = [&](float x, float y) { return ImVec2(o.x + x * s, o.y + y * s); };
    dl->AddBezierCubic(pt(0.5f, 0.88f), pt(0.02f, 0.58f), pt(0.08f, 0.12f), pt(0.5f, 0.32f), col, thick);
    dl->AddBezierCubic(pt(0.5f, 0.88f), pt(0.98f, 0.58f), pt(0.92f, 0.12f), pt(0.5f, 0.32f), col, thick);
}

void drawBolt(ImDrawList* dl, ImVec2 o, float s, float thick, ImU32 col) {
    ImVec2 pts[] = {
        ImVec2(o.x + 0.60f * s, o.y + 0.08f * s), ImVec2(o.x + 0.26f * s, o.y + 0.56f * s),
        ImVec2(o.x + 0.50f * s, o.y + 0.56f * s), ImVec2(o.x + 0.40f * s, o.y + 0.92f * s),
        ImVec2(o.x + 0.76f * s, o.y + 0.42f * s), ImVec2(o.x + 0.52f * s, o.y + 0.42f * s),
    };
    dl->AddPolyline(pts, 6, col, ImDrawFlags_Closed, thick);
}

void drawCross(ImDrawList* dl, ImVec2 o, float s, float thick, ImU32 col) {
    dl->AddLine(ImVec2(o.x + 0.25f * s, o.y + 0.25f * s), ImVec2(o.x + 0.75f * s, o.y + 0.75f * s), col, thick);
    dl->AddLine(ImVec2(o.x + 0.75f * s, o.y + 0.25f * s), ImVec2(o.x + 0.25f * s, o.y + 0.75f * s), col, thick);
}

// The cross drawn stroke by stroke: each stroke grows from 0 to 1.
void drawCrossPartial(ImDrawList* dl, ImVec2 o, float s, float thick, ImU32 col, float t1, float t2) {
    auto stroke = [&](float x0, float y0, float x1, float y1, float t) {
        if (t <= 0.0f) return;
        const ImVec2 a(o.x + x0 * s, o.y + y0 * s);
        dl->AddLine(a, ImVec2(a.x + (x1 - x0) * s * t, a.y + (y1 - y0) * s * t), col, thick);
    };
    stroke(0.25f, 0.25f, 0.75f, 0.75f, std::min(t1, 1.0f));
    stroke(0.75f, 0.25f, 0.25f, 0.75f, std::min(t2, 1.0f));
}

// Fades in (k: 0 -> 1) and shifts every vertex added to `dl` since index
// `start`: a cheap way to animate a whole group of shapes and widgets at once.
// Colors blend toward the background behind the group instead of becoming
// transparent, so stacked layers (a box over its hard shadow) don't turn gray.
void fadeVertices(ImDrawList* dl, int start, float k, ImU32 background, ImVec2 shift) {
    const float hide = 1.0f - std::clamp(k, 0.0f, 1.0f);
    for (int i = start; i < dl->VtxBuffer.Size; i++) {
        ImDrawVert& v = dl->VtxBuffer[i];
        v.col = mixColor(v.col, background, hide);
        v.pos.x += shift.x;
        v.pos.y += shift.y;
    }
}

// Two arrows chasing each other round a circle (rounds).
static void drawCycle(ImDrawList* dl, ImVec2 o, float s, float thick, ImU32 col) {
    const ImVec2 c(o.x + 0.5f * s, o.y + 0.5f * s);
    const float r = 0.32f * s;
    const float pi = 3.14159265f;
    for (int half = 0; half < 2; half++) {
        const float a0 = pi * (half == 0 ? 1.15f : 0.15f);
        const float a1 = a0 + pi * 0.72f;
        dl->PathArcTo(c, r, a0, a1, 12);
        dl->PathStroke(col, 0, thick);
        const ImVec2 tip(c.x + std::cos(a1) * r, c.y + std::sin(a1) * r);
        const ImVec2 dir(-std::sin(a1), std::cos(a1));      // direction of travel
        const ImVec2 out(std::cos(a1), std::sin(a1));       // away from the center
        const float k = 0.14f * s;
        dl->AddTriangleFilled(ImVec2(tip.x + dir.x * k, tip.y + dir.y * k),
                              ImVec2(tip.x - dir.x * k * 0.4f + out.x * k, tip.y - dir.y * k * 0.4f + out.y * k),
                              ImVec2(tip.x - dir.x * k * 0.4f - out.x * k, tip.y - dir.y * k * 0.4f - out.y * k), col);
    }
}

bool statTile(const char* id, const char* label, const std::string& value, TileIcon icon, ImU32 iconCol,
              float w, float S) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const float h = 62 * S;
    inkBox(dl, p0, p0 + ImVec2(w, h), 14 * S, S);
    const float is = 22 * S;
    const ImVec2 io(p0.x + 9 * S, p0.y + 8 * S);
    switch (icon) {
        case TileIcon::Cycle: drawCycle(dl, io, is, 2.0f * S, iconCol); break;
        case TileIcon::Bolt:  drawBolt(dl, io, is, 2.0f * S, iconCol); break;
        case TileIcon::Heart: drawHeart(dl, io, is, 2.0f * S, iconCol); break;
    }
    // The number pops (grows, then settles) whenever it changes.
    static std::unordered_map<ImGuiID, std::pair<std::string, double>> lastValue;
    const ImGuiID key = ImGui::GetID(id);
    const double now = ImGui::GetTime();
    auto seen = lastValue.find(key);
    if (seen == lastValue.end()) seen = lastValue.emplace(key, std::make_pair(value, -10.0)).first;
    else if (seen->second.first != value) seen->second = std::make_pair(value, now);
    const float popT = clamp01(static_cast<float>(now - seen->second.second) / 0.35f);
    const float pop = 1.0f + 0.32f * (1.0f - popT) * (1.0f - popT);
    const float baseMul = value.size() > 4 ? 1.15f : 1.45f;
    const float mul = baseMul * pop;
    const ImVec2 vs0 = heavyTextSize(value.c_str(), baseMul);
    const ImVec2 vs = heavyTextSize(value.c_str(), mul);
    heavyText(dl, ImVec2(p0.x + w - 10 * S - vs.x, p0.y + 7 * S + (vs0.y - vs.y) * 0.5f), COL_TEXT, value.c_str(), mul);
    const ImVec2 ls = boldTextSize(label, 0.92f);
    boldText(dl, ImVec2(p0.x + 10 * S, p0.y + h - 9 * S - ls.y), COL_TEXT, label, 0.92f);
    ImGui::InvisibleButton(id, ImVec2(w, h + 4 * S));
    return ImGui::IsItemHovered();
}

bool crCombo(const char* label, int* index) {
    bool changed = false;
    const std::string preview = *index < 0 ? std::string("-") : std::string(crLabel(*index)) + "  (" +
                                std::to_string(crXp(*index)) + " XP)";
    if (ImGui::BeginCombo(label, preview.c_str())) {
        if (ImGui::Selectable(T("-  (not set)"), *index < 0)) {
            *index = -1;
            changed = true;
        }
        for (int i = 0; i < crCount(); i++) {
            const std::string item = TF("CR %s", crLabel(i)) + "  (" + std::to_string(crXp(i)) + " XP)";
            if (ImGui::Selectable(item.c_str(), *index == i)) {
                *index = i;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool damageModCombo(const char* label, DamageMod* mod) {
    int v = static_cast<int>(*mod);
    const char* items[] = {T("Normal"), T("Resistant (half)"), T("Vulnerable (double)"), T("Immune (none)")};
    if (ImGui::Combo(label, &v, items, 4)) {
        *mod = static_cast<DamageMod>(v);
        return true;
    }
    return false;
}
