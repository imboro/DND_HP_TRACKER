#include <SDL.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#ifdef _WIN32
#include <filesystem>  // UTF-8 save paths (user folders with accents)
#endif
#include "imgui.h"
#include "imgui_internal.h"  // GetHoveredID/GetActiveID for last-frame hover state
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "creature.h"
#include "difficulty.h"
#include "encounter.h"

// Scales fonts, spacing and hard-coded sizes so the UI isn't tiny.
static const float UI_SCALE = 1.25f;

// Mouse wheel / trackpad scrolling is scaled by this (1.0 = ImGui's default speed).
// Mac trackpads send many small steps; a Windows wheel sends whole notches.
#ifdef _WIN32
static const float WHEEL_SENSITIVITY = 0.8f;
#else
static const float WHEEL_SENSITIVITY = 0.35f;
#endif

// Shortcut modifier as shown in menus (the shortcuts accept Ctrl and Cmd on both).
#ifdef __APPLE__
static const char* const MOD_KEY = "Cmd";
#else
static const char* const MOD_KEY = "Ctrl";
#endif

// Alerts stay fully visible this long, then fade out over ALERT_FADE seconds.
static const double ALERT_HOLD = 6.0;
static const double ALERT_FADE = 1.6;

// Bold and heavy faces for headings and numbers (fall back to the regular font).
static ImFont* g_fontBold = nullptr;
static ImFont* g_fontHeavy = nullptr;
// ImGui sizes a font by its line height (ascent + descent). Arial's faces have
// taller line metrics than Helvetica, so at the same size their glyphs look
// smaller; these factors bring them back to the regular font's glyph size.
static float g_boldMul = 1.0f;
static float g_heavyMul = 1.0f;

// Extra scale for cards on the board (the "Card size" setting), 0.6 - 1.0.
static float g_cardScale = 1.0f;
static float cardS() { return UI_SCALE * g_cardScale; }

// Draws card text at the card scale while alive.
struct CardFontScope {
    // FontSizeBase is the unscaled size currently in use (our font is loaded at a custom size).
    CardFontScope() { ImGui::PushFont(nullptr, ImGui::GetCurrentContext()->FontSizeBase * g_cardScale); }
    ~CardFontScope() { ImGui::PopFont(); }
};

// The standard 5e conditions offered in menus (custom ones can be typed in).
static const char* STANDARD_CONDITIONS[] = {
    "Blinded",   "Charmed",     "Deafened",  "Exhaustion", "Frightened",
    "Grappled",  "Incapacitated", "Invisible", "Paralyzed", "Petrified",
    "Poisoned",  "Prone",       "Restrained", "Stunned",   "Unconscious",
};
static const int STANDARD_CONDITION_COUNT = sizeof(STANDARD_CONDITIONS) / sizeof(STANDARD_CONDITIONS[0]);

// ---- Per-card UI state (lives in the UI layer, parallel to Encounter) -----

// A reference to one combatant: which group and which index in it.
struct CardRef {
    Side side;
    int index;
};

static bool operator==(CardRef a, CardRef b) { return a.side == b.side && a.index == b.index; }

// What a floating label / flash represents.
enum class FxKind { Damage, Heal, Temp, Immune };

// Card animation timings (seconds).
static const float DEATH_LEN = 1.4f;   // death: thud, cross, strike-through, floating label
static const float SPAWN_TIME = 0.32f; // fade-in + drop when a card is added

struct CardState {
    ImVec2 pos = ImVec2(0, 0);      // top-left inside the board
    ImVec2 drawPos = ImVec2(0, 0);  // on-screen top-left last frame (after shake)

    // HP change animation.
    int lastHp = -1;     // last seen actual HP, used to detect changes
    bool active = false;
    float t = 0.0f;      // seconds since the change started
    int oldHp = 0;
    int newHp = 0;
    bool heal = false;
    bool crit = false;
    int delta = 0;
    int requested = 0;   // amount rolled for the last hit/heal (drives "crit")
    FxKind fxKind = FxKind::Damage;
    std::string fxLabel;
    // Set by an action; picked up by the animation on the next frame.
    std::string pendingLabel;
    FxKind pendingKind = FxKind::Damage;

    bool selected = false;

    // Press tracking for click-vs-drag.
    ImVec2 pressMouse = ImVec2(0, 0);

    // Per-frame outputs from drawToken.
    bool beingDragged = false;
    bool wantsEdit = false;
    bool pressed = false;   // drag handle went down this frame
    bool clicked = false;   // released without moving

    // Death, appearance, selection and hover animations.
    bool wasDead = false;         // looked dead last frame (a new death starts the animation)
    float deathT = DEATH_LEN;     // seconds since it died (DEATH_LEN = finished)
    std::string deathLabel;       // "DEFEATED" / "DEAD", floated up when it dies
    float spawnT = SPAWN_TIME;    // seconds since the user added it (negative = staggered wait)
    float selAnim = 0.0f;         // selection ring, 0..1
    float lift = 4.0f;            // shadow offset, eased between rest / hover / held / dead
};

struct BoardState {
    std::vector<CardState> players;
    std::vector<CardState> creatures;
    std::vector<CardRef> z;  // back to front
    CardRef drag{Side::Players, 0};  // card dragged last frame, if dragValid
    bool dragValid = false;
    int round = 1;

    std::vector<CardState>& of(Side side) {
        return side == Side::Players ? players : creatures;
    }
    const std::vector<CardState>& of(Side side) const {
        return side == Side::Players ? players : creatures;
    }
    CardState& get(CardRef r) { return of(r.side)[r.index]; }
};

// ---- Palette: clean light, ink outlines ------------------------------------
// Mostly black and white: ink outlines with hard offset shadows on white
// surfaces. Color is kept for meaning (party, damage, healing...).
// Surfaces, from the back of the screen to the front.
static const ImU32 COL_BG         = IM_COL32(236, 236, 232, 255);  // window behind the panels
static const ImU32 COL_PANEL      = IM_COL32(255, 255, 255, 255);  // sidebar, popups, boxes
static const ImU32 COL_BOARD      = IM_COL32(247, 247, 244, 255);  // the board
static const ImU32 COL_CARD       = IM_COL32(255, 255, 255, 255);  // creature cards
static const ImU32 COL_CARD_HI    = IM_COL32(251, 251, 249, 255);  // hovered card
static const ImU32 COL_RAISED     = IM_COL32(255, 255, 255, 255);  // neutral buttons, chips
static const ImU32 COL_RAISED_HI  = IM_COL32(244, 244, 241, 255);
static const ImU32 COL_RAISED_LO  = IM_COL32(232, 232, 228, 255);
static const ImU32 COL_DISABLED   = IM_COL32(242, 242, 239, 255);
static const ImU32 COL_TRACK      = IM_COL32(236, 236, 232, 255);  // bar tracks
static const ImU32 COL_BORDER     = IM_COL32(228, 228, 223, 255);  // hairlines, separators
static const ImU32 COL_BORDER_HI  = IM_COL32(190, 190, 184, 255);
static const ImU32 COL_INK        = IM_COL32(26, 26, 28, 255);     // outlines, hard shadows
// Text.
static const ImU32 COL_TEXT       = IM_COL32(26, 26, 28, 255);
static const ImU32 COL_MUTED      = IM_COL32(132, 132, 128, 255);
static const ImU32 COL_SHADOW     = COL_INK;
static const ImU32 COL_WHITE      = IM_COL32(255, 255, 255, 255);
// Meaning colors: kept consistent across the whole UI.
static const ImU32 COL_PLAYER     = IM_COL32(59, 130, 246, 255);   // party
static const ImU32 COL_RED        = IM_COL32(226, 72, 60, 255);    // enemies, damage, low HP
static const ImU32 COL_GREEN      = IM_COL32(46, 164, 79, 255);    // healing, healthy
static const ImU32 COL_AMBER      = IM_COL32(230, 150, 20, 255);   // half HP, warnings
static const ImU32 COL_GRAY       = IM_COL32(150, 150, 146, 255);  // dead, neutral
static const ImU32 COL_TEMP       = IM_COL32(14, 160, 225, 255);   // temporary HP
static const ImU32 COL_VIOLET     = IM_COL32(139, 92, 246, 255);   // concentration
static const ImU32 COL_SELECT     = IM_COL32(250, 184, 0, 255);    // selection

// Button looks: fill (normal, hover, pressed) and label color.
struct ButtonStyle {
    ImU32 fill, hover, active, text;
};
static const ButtonStyle BTN_INK    = {COL_INK, IM_COL32(64, 64, 66, 255), IM_COL32(0, 0, 0, 255), COL_WHITE};
static const ButtonStyle BTN_RED    = {IM_COL32(255, 236, 232, 255), IM_COL32(255, 222, 216, 255), IM_COL32(250, 205, 198, 255), IM_COL32(176, 42, 32, 255)};
static const ButtonStyle BTN_GREEN  = {IM_COL32(230, 246, 234, 255), IM_COL32(214, 239, 221, 255), IM_COL32(198, 231, 207, 255), IM_COL32(26, 112, 50, 255)};
static const ButtonStyle BTN_TEMP   = {IM_COL32(227, 243, 252, 255), IM_COL32(210, 235, 249, 255), IM_COL32(193, 226, 245, 255), IM_COL32(8, 102, 148, 255)};
static const ButtonStyle BTN_VIOLET = {IM_COL32(241, 236, 255, 255), IM_COL32(229, 221, 254, 255), IM_COL32(216, 205, 251, 255), IM_COL32(94, 58, 190, 255)};
static const ButtonStyle BTN_QUIET  = {COL_RAISED, COL_RAISED_HI, COL_RAISED_LO, COL_TEXT};

static ImU32 hpColor(float fraction) {
    if (fraction > 0.5f) return COL_GREEN;
    if (fraction > 0.25f) return COL_AMBER;
    return COL_RED;
}

static ImVec4 toVec4(ImU32 c) {
    return ImGui::ColorConvertU32ToFloat4(c);
}

static void applyTheme() {
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
static bool loadSystemFonts(float pixelSize) {
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

// ---- Drawing helpers -------------------------------------------------------

static float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static float easeOutCubic(float t) { float u = 1.0f - t; return 1.0f - u * u * u; }
static float easeInOutCubic(float t) {
    return t < 0.5f ? 4 * t * t * t : 1.0f - std::pow(-2 * t + 2, 3.0f) / 2.0f;
}

// Moves `value` toward `target`, framerate-independent (higher rate = snappier).
static float approach(float value, float target, float rate, float dt) {
    return value + (target - value) * (1.0f - std::exp(-rate * dt));
}

static ImU32 withAlpha(ImU32 c, int alpha) {
    return (c & 0x00FFFFFF) | (static_cast<ImU32>(std::clamp(alpha, 0, 255)) << 24);
}

// Scales a color's existing alpha by k (0..1).
static ImU32 fadeAlpha(ImU32 c, float k) {
    const int a = static_cast<int>((c >> IM_COL32_A_SHIFT) & 0xFF);
    return withAlpha(c, static_cast<int>(static_cast<float>(a) * std::clamp(k, 0.0f, 1.0f)));
}

// Blends two colors (t = 0 gives a, t = 1 gives b); keeps a's alpha.
static ImU32 mixColor(ImU32 a, ImU32 b, float t) {
    auto ch = [&](int shift) {
        const float x = static_cast<float>((a >> shift) & 0xFF), y = static_cast<float>((b >> shift) & 0xFF);
        return static_cast<ImU32>(std::lround(x + (y - x) * t)) << shift;
    };
    return ch(IM_COL32_R_SHIFT) | ch(IM_COL32_G_SHIFT) | ch(IM_COL32_B_SHIFT) | (a & IM_COL32_A_MASK);
}

static ImVec2 operator+(ImVec2 a, ImVec2 b) { return ImVec2(a.x + b.x, a.y + b.y); }
static ImVec2 operator-(ImVec2 a, ImVec2 b) { return ImVec2(a.x - b.x, a.y - b.y); }

static ImFont* boldFont() { return g_fontBold ? g_fontBold : ImGui::GetFont(); }
static ImFont* heavyFont() { return g_fontHeavy ? g_fontHeavy : boldFont(); }

// Text in a given face, at a size relative to the current font size.
// Positions and sizes refer to the glyphs at the regular font's scale: the
// taller line box of Arial faces is trimmed so text lines up with regular text.
static float faceMul(ImFont* font) {
    if (font == g_fontHeavy) return g_heavyMul;
    if (font == g_fontBold) return g_boldMul;
    return 1.0f;
}
static void fontText(ImDrawList* dl, ImFont* font, ImVec2 pos, ImU32 col, const char* text, float sizeMul) {
    const float k = faceMul(font);
    const float size = ImGui::GetFontSize() * sizeMul;
    // Arial's extra line height is mostly above the glyphs; shift up to compensate.
    const float lift = size * (k - 1.0f) * 0.62f;
    dl->AddText(font, size * k, ImVec2(pos.x, pos.y - lift), col, text);
}
static ImVec2 fontTextSize(ImFont* font, const char* text, float sizeMul) {
    const float size = ImGui::GetFontSize() * sizeMul;
    const ImVec2 ts = font->CalcTextSizeA(size * faceMul(font), FLT_MAX, 0.0f, text);
    return ImVec2(ts.x, size);
}

static void boldText(ImDrawList* dl, ImVec2 pos, ImU32 col, const char* text, float sizeMul = 1.0f) {
    fontText(dl, boldFont(), pos, col, text, sizeMul);
}
static ImVec2 boldTextSize(const char* text, float sizeMul = 1.0f) {
    return fontTextSize(boldFont(), text, sizeMul);
}
static void heavyText(ImDrawList* dl, ImVec2 pos, ImU32 col, const char* text, float sizeMul = 1.0f) {
    fontText(dl, heavyFont(), pos, col, text, sizeMul);
}
static ImVec2 heavyTextSize(const char* text, float sizeMul = 1.0f) {
    return fontTextSize(heavyFont(), text, sizeMul);
}

static void sizedText(ImDrawList* dl, ImVec2 pos, ImU32 col, const char* text, float sizeMul) {
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * sizeMul, pos, col, text);
}

static ImVec2 sizedTextSize(const char* text, float sizeMul) {
    const ImVec2 base = ImGui::CalcTextSize(text);
    return ImVec2(base.x * sizeMul, base.y * sizeMul);
}

// The house style: a rounded box with an ink outline and a hard shadow
// offset down and to the right. `alpha` fades the whole thing (0..1).
static void inkBox(ImDrawList* dl, ImVec2 a, ImVec2 b, float round, float S, ImU32 fill = COL_PANEL,
                   float shadow = 3.0f, ImU32 outline = COL_INK, float alpha = 1.0f, float border = 1.75f) {
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
static bool flatButton(const char* label, ImVec2 size, const ButtonStyle& look, bool enabled = true,
                       bool interactive = true) {
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
static bool primaryButton(const char* label, ImVec2 size = ImVec2(0, 0), bool enabled = true) {
    return flatButton(label, size, BTN_INK, enabled);
}
static bool dangerButton(const char* label, ImVec2 size = ImVec2(0, 0), bool enabled = true) {
    return flatButton(label, size, BTN_RED, enabled);
}
static bool greenButton(const char* label, ImVec2 size = ImVec2(0, 0), bool enabled = true) {
    return flatButton(label, size, BTN_GREEN, enabled);
}
static bool tempButton(const char* label, ImVec2 size = ImVec2(0, 0), bool enabled = true) {
    return flatButton(label, size, BTN_TEMP, enabled);
}
static bool quietButton(const char* label, ImVec2 size = ImVec2(0, 0), bool enabled = true) {
    return flatButton(label, size, BTN_QUIET, enabled);
}
static bool violetButton(const char* label, ImVec2 size = ImVec2(0, 0), bool enabled = true) {
    return flatButton(label, size, BTN_VIOLET, enabled);
}

// Width for `count` buttons sharing the current row.
static float rowButtonWidth(int count) {
    const float S = UI_SCALE;
    const float avail = ImGui::GetContentRegionAvail().x - 2 * S;
    return (avail - 8 * S * static_cast<float>(count - 1)) / static_cast<float>(count);
}

// ---- Section boxes ---------------------------------------------------------
// A white box with an ink outline whose heavy title sits in a gap in the top
// edge. The box is drawn behind the content using last frame's height, so
// it never needs draw-list channels (which tables inside would conflict with).

static const float SECTION_PAD = 13.0f;
static std::unordered_map<ImGuiID, float> g_sectionHeights;
struct SectionState {
    ImGuiID id = 0;
    ImVec2 start;
    float savedContentMax = 0.0f;
    float savedWorkMax = 0.0f;
};
static SectionState g_section;

static void beginSection(const char* title, const std::string& note = std::string()) {
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

static void endSection() {
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
static void pill(const char* text, ImU32 color, float S) {
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
static void segmentedTabs(const char* const* labels, int count, int* active, float S) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x - 3 * S - 1 * S;
    const float h = ImGui::GetFrameHeight() + 4 * S;
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
        const ImVec2 ts = boldTextSize(labels[i]);
        boldText(dl, ImVec2((a.x + b.x - ts.x) * 0.5f, (a.y + b.y - ts.y) * 0.5f),
                 mixColor(COL_MUTED, COL_WHITE, under), labels[i]);
    }
    ImGui::SetCursorScreenPos(p0);
    ImGui::Dummy(ImVec2(w, h + 3 * S));
}

static void drawHeart(ImDrawList* dl, ImVec2 o, float s, float thick, ImU32 col) {
    auto pt = [&](float x, float y) { return ImVec2(o.x + x * s, o.y + y * s); };
    dl->AddBezierCubic(pt(0.5f, 0.88f), pt(0.02f, 0.58f), pt(0.08f, 0.12f), pt(0.5f, 0.32f), col, thick);
    dl->AddBezierCubic(pt(0.5f, 0.88f), pt(0.98f, 0.58f), pt(0.92f, 0.12f), pt(0.5f, 0.32f), col, thick);
}

static void drawBolt(ImDrawList* dl, ImVec2 o, float s, float thick, ImU32 col) {
    ImVec2 pts[] = {
        ImVec2(o.x + 0.60f * s, o.y + 0.08f * s), ImVec2(o.x + 0.26f * s, o.y + 0.56f * s),
        ImVec2(o.x + 0.50f * s, o.y + 0.56f * s), ImVec2(o.x + 0.40f * s, o.y + 0.92f * s),
        ImVec2(o.x + 0.76f * s, o.y + 0.42f * s), ImVec2(o.x + 0.52f * s, o.y + 0.42f * s),
    };
    dl->AddPolyline(pts, 6, col, ImDrawFlags_Closed, thick);
}

static void drawCross(ImDrawList* dl, ImVec2 o, float s, float thick, ImU32 col) {
    dl->AddLine(ImVec2(o.x + 0.25f * s, o.y + 0.25f * s), ImVec2(o.x + 0.75f * s, o.y + 0.75f * s), col, thick);
    dl->AddLine(ImVec2(o.x + 0.75f * s, o.y + 0.25f * s), ImVec2(o.x + 0.25f * s, o.y + 0.75f * s), col, thick);
}

// The cross drawn stroke by stroke: each stroke grows from 0 to 1.
static void drawCrossPartial(ImDrawList* dl, ImVec2 o, float s, float thick, ImU32 col, float t1, float t2) {
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
static void fadeVertices(ImDrawList* dl, int start, float k, ImU32 background, ImVec2 shift = ImVec2(0, 0)) {
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

enum class TileIcon { Cycle, Bolt, Heart };

// Summary tile: line icon top-left, big heavy number top-right, label below.
// Returns true while hovered (for a tooltip).
static bool statTile(const char* id, const char* label, const std::string& value, TileIcon icon, ImU32 iconCol,
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

// ---- Life state ------------------------------------------------------------

enum class Life { Up, Dying, Stable, Dead };

// Players at 0 HP are dying until 3 successes (stable) or 3 failures (dead).
static Life lifeOf(Side side, const Creature& c) {
    if (c.getCurrentHp() > 0) return Life::Up;
    if (side == Side::Creatures) return Life::Dead;
    if (c.getDeathFailures() >= 3) return Life::Dead;
    if (c.getDeathSuccesses() >= 3) return Life::Stable;
    return Life::Dying;
}

static const char* damageModName(DamageMod m) {
    switch (m) {
        case DamageMod::Normal:     return "Normal";
        case DamageMod::Resistant:  return "Resistant";
        case DamageMod::Vulnerable: return "Vulnerable";
        case DamageMod::Immune:     return "Immune";
    }
    return "";
}

// ---- Animation helpers -----------------------------------------------------

static const float CRIT_FRACTION = 0.40f;  // hit/heal of >= 40% max HP is a "crit"
static const float ANIM_EXTEND = 0.18f;    // pre-bar extends
static const float ANIM_HOLD = 0.55f;      // then holds until here
static const float ANIM_DROP = 0.45f;      // then the main bar drops/rises
static const float ANIM_TOTAL = ANIM_HOLD + ANIM_DROP + 0.1f;

// What the bar should look like right now, as fractions of max HP.
struct BarVis {
    float fillEnd;    // solid fill goes from 0 to here
    float chunkEnd;   // pre-bar chunk spans [fillEnd, chunkEnd]
    bool hasChunk;
};

static BarVis computeBar(const CardState& st, int hp, int maxHp) {
    BarVis v{0.0f, 0.0f, false};
    if (maxHp <= 0) return v;
    const float M = static_cast<float>(maxHp);
    v.fillEnd = v.chunkEnd = std::min(1.0f, static_cast<float>(hp) / M);
    if (!st.active || st.oldHp == st.newHp) return v;

    const float oldF = std::min(1.0f, static_cast<float>(st.oldHp) / M);
    const float newF = std::min(1.0f, static_cast<float>(st.newHp) / M);
    const float ext = easeOutCubic(clamp01(st.t / ANIM_EXTEND));
    const float drop = easeInOutCubic(clamp01((st.t - ANIM_HOLD) / ANIM_DROP));
    v.hasChunk = true;
    if (!st.heal) {
        // Chunk extends left from the old edge, holds, then the old edge drops onto it.
        v.fillEnd = lerpf(oldF, newF, ext);
        v.chunkEnd = lerpf(oldF, newF, drop);
    } else {
        // Chunk extends right from the old edge, holds, then the fill rises into it.
        v.fillEnd = lerpf(oldF, newF, drop);
        v.chunkEnd = lerpf(oldF, newF, ext);
    }
    return v;
}

// Notices an HP change (or a queued label like "IMMUNE") and (re)starts the
// animation; advances timers.
static void updateCardFx(CardState& st, int hp, int maxHp, float dt) {
    if (st.lastHp < 0) st.lastHp = hp;
    const bool changed = hp != st.lastHp;
    if (changed || !st.pendingLabel.empty()) {
        st.oldHp = st.lastHp;
        st.newHp = hp;
        st.heal = changed ? hp > st.lastHp : st.pendingKind == FxKind::Heal;
        st.delta = std::abs(hp - st.lastHp);
        if (st.pendingLabel.empty()) {
            st.fxKind = st.heal ? FxKind::Heal : FxKind::Damage;
            st.fxLabel = (st.heal ? "+" : "-") + std::to_string(st.delta);
        } else {
            st.fxKind = st.pendingKind;
            st.fxLabel = st.pendingLabel;
        }
        const int measured = st.requested > 0 ? st.requested : st.delta;
        st.crit = changed && (st.fxKind == FxKind::Damage || st.fxKind == FxKind::Heal) && maxHp > 0 &&
                  static_cast<float>(measured) >= CRIT_FRACTION * static_cast<float>(maxHp);
        st.pendingLabel.clear();
        st.requested = 0;
        st.t = 0.0f;
        st.active = true;
        st.lastHp = hp;
    } else if (st.active) {
        st.t += dt;
        if (st.t > ANIM_TOTAL) st.active = false;
    }
}

static ImVec2 shakeOffset(const CardState& st, float S) {
    if (!st.active) return ImVec2(0, 0);
    const float t = st.t;
    if (st.fxKind == FxKind::Damage || st.fxKind == FxKind::Temp) {
        const float dur = st.crit ? 0.5f : 0.18f;
        if (t >= dur) return ImVec2(0, 0);
        const float amp = (st.crit ? 6.0f : 1.8f) * S * (1.0f - t / dur);
        return ImVec2(std::sin(t * 70.0f) * amp, std::cos(t * 91.0f) * amp * 0.6f);
    }
    if (st.fxKind == FxKind::Heal && st.crit && t < 0.6f) {  // heal crit: a little hop
        return ImVec2(0, -std::fabs(std::sin(t * 3.14159f * 3.0f)) * 9.0f * S * (1.0f - t / 0.6f));
    }
    return ImVec2(0, 0);
}

static ImU32 fxColor(FxKind k) {
    switch (k) {
        case FxKind::Damage: return COL_RED;
        case FxKind::Heal:   return COL_GREEN;
        case FxKind::Temp:   return COL_TEMP;
        case FxKind::Immune: return COL_GRAY;
    }
    return COL_RED;
}

static void outlinedText(ImDrawList* dl, ImVec2 pos, ImU32 col, int alpha, const char* text,
                         float sizeMul, float S) {
    const ImU32 ink = withAlpha(COL_SHADOW, alpha);
    const float o = 1.6f * S;
    const ImVec2 offs[] = {{-o, 0}, {o, 0}, {0, -o}, {0, o}, {-o, -o}, {o, -o}, {-o, o}, {o, o}};
    for (const ImVec2& d : offs) {
        heavyText(dl, ImVec2(pos.x + d.x, pos.y + d.y), ink, text, sizeMul);
    }
    heavyText(dl, pos, withAlpha(col, alpha), text, sizeMul);
}

// Floating label and crit starburst; drawn in a top pass above all cards.
static void drawCardEffects(const CardState& st, ImVec2 size, float S) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = st.drawPos;

    // Death: an ink shockwave ring around the card, then "DEFEATED" / "DEAD" floats up.
    if (st.wasDead && st.deathT < DEATH_LEN) {
        const float d = st.deathT;
        if (d < 0.5f) {
            const float u = d / 0.5f;
            const float grow = easeOutCubic(u) * 26.0f * S;
            dl->AddRect(ImVec2(p.x - grow, p.y - grow), ImVec2(p.x + size.x + grow, p.y + size.y + grow),
                        withAlpha(COL_INK, static_cast<int>(150 * (1.0f - u))), 16 * S + grow, 0, (3.0f - 2.0f * u) * S);
        }
        const float start = 0.15f;
        if (d > start && !st.deathLabel.empty()) {
            const float u = clamp01((d - start) / (DEATH_LEN - start));
            const float rise = easeOutCubic(u) * 30.0f * S;
            const float fade = u < 0.6f ? 1.0f : 1.0f - (u - 0.6f) / 0.4f;
            const float pop = u < 0.1f ? lerpf(0.6f, 1.0f, easeOutCubic(u / 0.1f)) : 1.0f;
            const float mul = 1.35f * pop;
            const ImVec2 ts = heavyTextSize(st.deathLabel.c_str(), mul);
            const ImVec2 pos(p.x + size.x * 0.5f - ts.x * 0.5f, p.y - 6 * S - rise - ts.y * 0.5f);
            outlinedText(dl, pos, IM_COL32(205, 205, 200, 255), static_cast<int>(255 * fade), st.deathLabel.c_str(), mul, S);
        }
    }

    if (!st.active) return;
    const float t = st.t;

    // Floating "-N" / "+N" / "IMMUNE" / "-5 temp".
    {
        const float life = 0.95f;
        if (t < life && !st.fxLabel.empty()) {
            const float rise = easeOutCubic(clamp01(t / life)) * 50.0f * S;
            const float fade = t < 0.55f ? 1.0f : 1.0f - clamp01((t - 0.55f) / (life - 0.55f));
            const float pop = t < 0.12f ? lerpf(0.6f, 1.0f, easeOutCubic(t / 0.12f)) : 1.0f;
            const float mul = (st.crit ? 2.3f : 1.5f) * pop;
            const ImVec2 ts = heavyTextSize(st.fxLabel.c_str(), mul);
            const ImVec2 pos(p.x + size.x * 0.5f - ts.x * 0.5f, p.y + 34 * S - rise - ts.y * 0.5f);
            outlinedText(dl, pos, fxColor(st.fxKind), static_cast<int>(255 * fade), st.fxLabel.c_str(), mul, S);
        }
    }

    // Crit starburst.
    if (st.crit && t < ANIM_HOLD + 0.35f) {
        const float end = ANIM_HOLD + 0.35f;
        float scale;
        if (t < 0.10f) scale = lerpf(0.0f, 1.35f, t / 0.10f);
        else if (t < 0.24f) scale = lerpf(1.35f, 1.0f, (t - 0.10f) / 0.14f);
        else scale = 1.0f;
        if (t > end - 0.2f) scale *= clamp01((end - t) / 0.2f);
        const float wobble = 0.18f * std::sin(t * 22.0f) * (1.0f - clamp01(t / 0.6f));

        const ImVec2 c(p.x + size.x - 8 * S, p.y - 2 * S);
        const float R = 34 * S * scale;
        const int spikes = 12;
        std::vector<ImVec2> pts;
        for (int i = 0; i < spikes * 2; i++) {
            const float r = (i % 2 == 0) ? R : R * 0.68f;
            const float ang = wobble + i * 3.14159265f / spikes;
            pts.push_back(ImVec2(c.x + std::cos(ang) * r, c.y + std::sin(ang) * r));
        }
        const ImU32 fill = st.heal ? COL_GREEN : COL_RED;
        const int n = static_cast<int>(pts.size());
        for (int i = 0; i < n; i++) {
            dl->AddTriangleFilled(c, pts[i], pts[(i + 1) % n], fill);
        }
        dl->AddPolyline(pts.data(), n, COL_SHADOW, ImDrawFlags_Closed, 2.0f * S);

        const char* label = st.heal ? "HEAL!" : "CRIT!";
        const float mul = std::max(0.2f, scale);
        const ImVec2 ts = heavyTextSize(label, 0.95f * mul);
        outlinedText(dl, ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), IM_COL32(255, 255, 255, 255),
                     255, label, 0.95f * mul, S);
    }
}

// ---- Cards -----------------------------------------------------------------

static const float CARD_W = 240.0f;
static const float CARD_H = 186.0f;
static const float TAB_H = 14.0f;

static ImVec2 tokenSize() {
    return ImVec2(CARD_W * cardS(), CARD_H * cardS());
}

struct Chip {
    std::string text;
    ImU32 fill;
};

// Status chips along the bottom of a card; "+N" when they don't all fit.
// A chip's color tints its fill and border and colors its text.
static void drawChips(ImDrawList* dl, ImVec2 start, float maxX, const std::vector<Chip>& chips, float S) {
    const float mul = 0.78f;
    const float h = 19 * S;
    const float padX = 7 * S;
    const float gap = 5 * S;
    float x = start.x;
    for (size_t i = 0; i < chips.size(); i++) {
        const ImVec2 ts = fontTextSize(boldFont(), chips[i].text.c_str(), mul);
        const float w = ts.x + padX * 2;
        const size_t remaining = chips.size() - i - 1;
        const float reserve = remaining > 0 ? sizedTextSize("+99", mul).x + padX * 2 + gap : 0.0f;
        if (x + w + reserve > maxX) {
            const std::string more = "+" + std::to_string(chips.size() - i);
            const float mw = sizedTextSize(more.c_str(), mul).x + padX * 2;
            dl->AddRectFilled(ImVec2(x, start.y), ImVec2(x + mw, start.y + h), COL_TRACK, h * 0.5f);
            sizedText(dl, ImVec2(x + padX, start.y + (h - ts.y) * 0.5f), COL_MUTED, more.c_str(), mul);
            return;
        }
        const ImU32 col = chips[i].fill;
        const float k = static_cast<float>(col >> IM_COL32_A_SHIFT) / 255.0f;
        dl->AddRectFilled(ImVec2(x, start.y), ImVec2(x + w, start.y + h),
                          withAlpha(col, static_cast<int>(34 * k)), h * 0.5f);
        dl->AddRect(ImVec2(x, start.y), ImVec2(x + w, start.y + h),
                    withAlpha(col, static_cast<int>(150 * k)), h * 0.5f, 0, 1.0f * S);
        const ImU32 textCol = withAlpha(mixColor(col, COL_INK, 0.35f), static_cast<int>(255 * k));
        fontText(dl, boldFont(), ImVec2(x + padX, start.y + (h - ts.y) * 0.5f), textCol, chips[i].text.c_str(), mul);
        x += w + gap;
    }
}

static std::vector<Chip> cardChips(Side side, const Creature& c, Life life, float time) {
    std::vector<Chip> chips;
    if (life == Life::Dying) {
        const int a = 170 + static_cast<int>(85 * (0.5f + 0.5f * std::sin(time * 6.0f)));
        chips.push_back({"DYING", withAlpha(COL_RED, a)});
    } else if (life == Life::Stable) {
        chips.push_back({"STABLE", COL_GREEN});
    } else if (life == Life::Dead && side == Side::Players) {
        chips.push_back({"DEAD", COL_GRAY});
    }
    if (c.isBloodied()) chips.push_back({"BLOODIED", COL_RED});
    if (c.isConcentrating()) chips.push_back({"CONC", COL_VIOLET});
    switch (c.getDamageMod()) {
        case DamageMod::Resistant:  chips.push_back({"RESIST", IM_COL32(217, 119, 6, 255)}); break;
        case DamageMod::Vulnerable: chips.push_back({"VULN", IM_COL32(219, 39, 119, 255)}); break;
        case DamageMod::Immune:     chips.push_back({"IMMUNE", COL_GRAY}); break;
        default: break;
    }
    for (const Condition& cond : c.getConditions()) {
        std::string text = cond.name;
        if (cond.rounds > 0) text += " " + std::to_string(cond.rounds);
        chips.push_back({text, IM_COL32(202, 138, 4, 255)});
    }
    if (!c.getNotes().empty()) chips.push_back({"NOTE", COL_PLAYER});
    return chips;
}

static void cardTooltip(Side side, const Creature& c) {
    const float S = UI_SCALE;
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(300 * S);
    ImGui::TextUnformatted(c.getName().c_str());
    ImGui::TextDisabled("%s", side == Side::Players ? "Player" : "Enemy");
    ImGui::Separator();
    ImGui::Text("HP %d / %d", c.getCurrentHp(), c.getMaxHp());
    if (c.getTempHp() > 0) {
        ImGui::SameLine();
        ImGui::TextColored(toVec4(COL_TEMP), "+%d temp", c.getTempHp());
    }
    ImGui::Text("AC %d   Speed %d ft   Passive Perception %d", c.getAc(), c.getSpeed(), c.getPassivePerception());
    if (side == Side::Players) {
        ImGui::Text("Level %d", c.getLevel());
    } else if (c.getCrIndex() >= 0) {
        ImGui::Text("CR %s (%d XP)", crLabel(c.getCrIndex()), crXp(c.getCrIndex()));
    }
    if (c.getDamageMod() != DamageMod::Normal) {
        ImGui::Text("Damage: %s", damageModName(c.getDamageMod()));
    }
    if (c.isConcentrating()) ImGui::Text("Concentrating");
    if (side == Side::Players && c.getCurrentHp() == 0) {
        ImGui::Text("Death saves: %d success, %d failure", c.getDeathSuccesses(), c.getDeathFailures());
    }
    if (!c.getConditions().empty()) {
        ImGui::Separator();
        for (const Condition& cond : c.getConditions()) {
            if (cond.rounds > 0) {
                ImGui::BulletText("%s (%d round%s left)", cond.name.c_str(), cond.rounds, cond.rounds == 1 ? "" : "s");
            } else {
                ImGui::BulletText("%s", cond.name.c_str());
            }
        }
    }
    if (!c.getNotes().empty()) {
        ImGui::Separator();
        ImGui::TextUnformatted(c.getNotes().c_str());
    }
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

// What a card can ask the app to do (the app owns undo, logging and alerts).
struct CardActions {
    std::function<void(CardRef)> damage;
    std::function<void(CardRef)> heal;
    std::function<void(CardRef, int, int)> setDeathSaves;
};

// Draws one draggable creature card on the board and handles its buttons.
// `interactive` is false for cards hidden under another card at the mouse
// position, so only the front card receives the press.
static void drawToken(const Encounter& encounter, CardRef ref, CardState& st, ImVec2 boardOrigin,
                      ImVec2 boardSize, const std::string& amountLabel, bool interactive,
                      const CardActions& act) {
    const Creature& c = *encounter.at(ref.side, ref.index);
    const Side side = ref.side;
    const ImVec2 size = tokenSize();
    const float S = cardS();
    const float pad = 12 * S;
    bool showTooltip = false;
    const float round = 16 * S;
    const float outline = 2.0f * S;
    const float shadow = 7 * S;
    const float tabH = TAB_H * S;
    const float time = static_cast<float>(ImGui::GetTime());
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::PushID(static_cast<int>(side) * 10000 + ref.index);
    std::optional<CardFontScope> cardFont;
    cardFont.emplace();
    const int vtxStart = dl->VtxBuffer.Size;  // everything this card draws (for the fade-in)

    ImVec2& pos = st.pos;
    st.beingDragged = false;
    st.wantsEdit = false;
    st.pressed = false;
    st.clicked = false;

    // HP-change animation bookkeeping.
    const float dt = std::min(ImGui::GetIO().DeltaTime, 0.05f);
    updateCardFx(st, c.getCurrentHp(), c.getMaxHp(), dt);

    // A card only looks dead once its final hit has finished dropping; then
    // the death animation plays once (cards loaded already dead skip it).
    const Life life = lifeOf(side, c);
    const bool showDead = life == Life::Dead &&
                          !(st.active && st.fxKind == FxKind::Damage && st.t < ANIM_HOLD + ANIM_DROP);
    if (showDead && !st.wasDead) {
        st.deathT = 0.0f;
        st.deathLabel = side == Side::Players ? "DEAD" : "DEFEATED";
    }
    st.wasDead = showDead;
    if (st.deathT < DEATH_LEN) st.deathT += dt;
    if (st.spawnT < SPAWN_TIME) st.spawnT += dt;
    // 0 = alive look, 1 = fully dead look (fades in over half a second).
    const float deadK = showDead ? easeOutCubic(clamp01(st.deathT / 0.55f)) : 0.0f;
    const float spawnK = easeOutCubic(clamp01(st.spawnT / SPAWN_TIME));

    // Keep the card (including its tab and shadow) inside the board.
    pos.x = std::clamp(pos.x, 0.0f, std::max(0.0f, boardSize.x - size.x - shadow));
    pos.y = std::clamp(pos.y, tabH, std::max(tabH, boardSize.y - size.y - shadow));
    const ImVec2 shake = shakeOffset(st, S);
    // Dying thuds the card down a few pixels before it settles; new cards drop in from above.
    float thud = 0.0f;
    if (showDead && st.deathT < 0.4f) {
        const float u = st.deathT / 0.4f;
        thud = std::sin(u * 3.14159265f) * 7.0f * S * (1.0f - 0.5f * u);
    }
    const float drop = (1.0f - spawnK) * -16.0f * S;
    const ImVec2 p(boardOrigin.x + pos.x + shake.x, boardOrigin.y + pos.y + shake.y + thud + drop);
    const ImVec2 pEnd(p.x + size.x, p.y + size.y);
    st.drawPos = p;

    const ImU32 sideColor = side == Side::Players ? COL_PLAYER : COL_RED;
    const ImU32 accent = mixColor(sideColor, COL_GRAY, deadK);

    // Hover state of the drag handle is known from the previous frame via the
    // item ID, so the fill can react without submitting the handle first.
    const ImGuiID dragId = ImGui::GetID("drag");
    const bool hoveredOrActive = ImGui::GetHoveredID() == dragId || ImGui::GetActiveID() == dragId;

    // Selection ring: a marker-yellow outline that grows out from the card.
    st.selAnim = approach(st.selAnim, st.selected ? 1.0f : 0.0f, 20.0f, dt);
    if (st.selAnim > 0.01f) {
        const float e = easeOutCubic(st.selAnim);
        const float g = lerpf(0.0f, 5.0f, e) * S;
        const ImVec2 r0 = p - ImVec2(g, g), r1 = pEnd + ImVec2(g + 4 * S, g + 5 * S);
        dl->AddRectFilled(r0, r1, withAlpha(COL_SELECT, static_cast<int>(40 * e)), round + g);
        dl->AddRect(r0, r1, withAlpha(COL_SELECT, static_cast<int>(255 * e)), round + g, 0, 3.0f * S);
    }

    // Hard ink shadow: rises on hover, more while held, flattens when dead.
    const bool held = ImGui::GetActiveID() == dragId;
    const float liftTarget = lerpf(held ? 7.0f : (hoveredOrActive ? 5.5f : 4.0f), 1.5f, deadK);
    st.lift = approach(st.lift, liftTarget, 18.0f, dt);
    const float lift = st.lift;
    const ImU32 outlineCol = mixColor(COL_INK, COL_BORDER_HI, deadK);
    dl->AddRectFilled(p + ImVec2(lift * S, (lift + 1) * S), pEnd + ImVec2(lift * S, (lift + 1) * S), outlineCol, round);
    // Body: dying players pulse faintly like a heartbeat; the dead fade to gray.
    ImU32 aliveBody = hoveredOrActive ? COL_CARD_HI : COL_CARD;
    if (life == Life::Dying || (showDead && side == Side::Players)) {
        const float beat = life == Life::Dying ? 0.5f + 0.5f * std::sin(time * 4.0f) : 0.5f;
        aliveBody = mixColor(IM_COL32(255, 244, 242, 255), IM_COL32(255, 228, 224, 255), beat);
    }
    const ImU32 body = mixColor(aliveBody, IM_COL32(241, 241, 238, 255), deadK);
    dl->AddRectFilled(p, pEnd, body, round);
    // Flash on impact, colored by what happened (stronger on a crit).
    if (st.active) {
        const float dur = st.crit ? 0.40f : 0.14f;
        if (st.t < dur) {
            const float k = 1.0f - st.t / dur;
            ImU32 flash = IM_COL32(255, 60, 60, 255);
            if (st.fxKind == FxKind::Heal) flash = IM_COL32(60, 200, 100, 255);
            if (st.fxKind == FxKind::Temp) flash = IM_COL32(40, 160, 240, 255);
            if (st.fxKind == FxKind::Immune) flash = IM_COL32(150, 150, 160, 255);
            dl->AddRectFilled(p, pEnd, withAlpha(flash, static_cast<int>((st.crit ? 110 : 55) * k)), round);
        }
    }
    dl->AddRect(p, pEnd, outlineCol, round, 0, outline);

    // Icon tile (top-left): soft tinted square with a line glyph in the side's color.
    const float iconSize = 38 * S;
    const ImVec2 iconMin(p.x + pad, p.y + pad + 3 * S);
    const ImVec2 iconMax(iconMin.x + iconSize, iconMin.y + iconSize);
    dl->AddRectFilled(iconMin, iconMax, withAlpha(accent, 30), 11 * S);
    const ImVec2 glyphOrigin(iconMin.x + iconSize * 0.15f, iconMin.y + iconSize * 0.15f);
    const float glyphSize = iconSize * 0.7f;
    // On death the heart/bolt fades out while a cross is drawn over it stroke by stroke.
    const float oldGlyph = showDead ? 1.0f - clamp01(st.deathT / 0.2f) : 1.0f;
    if (oldGlyph > 0.0f) {
        if (side == Side::Players) drawHeart(dl, glyphOrigin, glyphSize, 2.4f * S, fadeAlpha(accent, oldGlyph));
        else drawBolt(dl, glyphOrigin, glyphSize, 2.4f * S, fadeAlpha(accent, oldGlyph));
    }
    if (showDead) {
        drawCrossPartial(dl, glyphOrigin, glyphSize, 2.4f * S, accent, easeOutCubic(clamp01(st.deathT / 0.16f)),
                         easeOutCubic(clamp01((st.deathT - 0.14f) / 0.16f)));
    }

    // Quick stats next to the icon: HP (+temp), then AC / speed / passive perception.
    const float lineH = ImGui::GetTextLineHeight();
    const ImVec2 statsPos(iconMax.x + 10 * S, iconMin.y);
    {
        const std::string cur = std::to_string(c.getCurrentHp());
        const std::string rest = " / " + std::to_string(c.getMaxHp()) + " HP";
        const ImVec2 cs = heavyTextSize(cur.c_str());
        heavyText(dl, statsPos, mixColor(COL_TEXT, COL_MUTED, deadK), cur.c_str());
        const float restX = statsPos.x + cs.x;
        dl->AddText(ImVec2(restX, statsPos.y), COL_MUTED, rest.c_str());
        if (c.getTempHp() > 0) {
            const std::string tempText = " +" + std::to_string(c.getTempHp()) + " temp";
            dl->AddText(ImVec2(restX + ImGui::CalcTextSize(rest.c_str()).x, statsPos.y), COL_TEMP, tempText.c_str());
        }
    }
    const std::string statLine = "AC " + std::to_string(c.getAc()) + "  \xC2\xB7  " + std::to_string(c.getSpeed()) +
                                 " ft  \xC2\xB7  PP " + std::to_string(c.getPassivePerception());
    dl->AddText(ImVec2(statsPos.x, statsPos.y + lineH + 2 * S), COL_MUTED, statLine.c_str());

    // Name, heavy, with a strike-through when dead.
    const std::string& name = c.getName();
    const float nameMul = 1.2f;
    const float nameDraw = 1.08f;  // the heavy face runs wide; slightly smaller than the slot
    const ImVec2 namePos(p.x + pad, iconMax.y + 6 * S);
    dl->PushClipRect(ImVec2(p.x, p.y), ImVec2(pEnd.x - (c.getCurrentHp() == 0 && side == Side::Players ? 105 * S : pad), pEnd.y), true);
    heavyText(dl, namePos + ImVec2(0, 1 * S), mixColor(COL_TEXT, COL_MUTED, deadK), name.c_str(), nameDraw);
    dl->PopClipRect();
    if (showDead) {
        // The strike-through is drawn left to right, just after the cross.
        const float strike = easeOutCubic(clamp01((st.deathT - 0.25f) / 0.3f));
        const ImVec2 ns = heavyTextSize(name.c_str(), nameDraw);
        const float y = namePos.y + 1 * S + ns.y * 0.55f;
        const float x1 = std::min(namePos.x + ns.x, pEnd.x - pad);
        if (strike > 0.0f) {
            dl->AddLine(ImVec2(namePos.x, y), ImVec2(lerpf(namePos.x, x1, strike), y), COL_MUTED, 1.5f * S);
        }
    }

    // HP bar: a pill with an ink outline; temp HP as a blue strip below.
    const float barTop = namePos.y + lineH * nameMul + 8 * S;
    const float barHeight = 14 * S;
    const float barRound = barHeight * 0.5f;
    const ImVec2 barMin(p.x + pad, barTop);
    const ImVec2 barMax(pEnd.x - pad, barTop + barHeight);
    const BarVis bar = computeBar(st, c.getCurrentHp(), c.getMaxHp());
    const float barW = barMax.x - barMin.x;
    dl->AddRectFilled(barMin, barMax, COL_TRACK, barRound);
    dl->PushClipRect(barMin, barMax, true);
    if (bar.fillEnd > 0.0f) {
        const float targetFrac = c.getMaxHp() > 0
            ? static_cast<float>(c.getCurrentHp()) / static_cast<float>(c.getMaxHp()) : 0.0f;
        dl->AddRectFilled(barMin, ImVec2(barMin.x + barW * bar.fillEnd, barMax.y), hpColor(targetFrac), barRound);
    }
    if (bar.hasChunk && bar.chunkEnd > bar.fillEnd) {
        // The "pre-bar": the part of the bar about to be lost or gained.
        ImU32 chunk;
        const float k = clamp01(st.t / 0.3f);
        if (!st.heal) {
            chunk = IM_COL32(255, static_cast<int>(lerpf(214, 90, k)), static_cast<int>(lerpf(120, 80, k)), 255);
        } else {
            chunk = IM_COL32(static_cast<int>(lerpf(200, 120, k)), static_cast<int>(lerpf(250, 220, k)), static_cast<int>(lerpf(170, 140, k)), 255);
        }
        const float x0 = barMin.x + barW * bar.fillEnd;
        const float x1 = barMin.x + barW * bar.chunkEnd;
        dl->AddRectFilled(ImVec2(x0, barMin.y), ImVec2(x1, barMax.y), chunk);
        dl->AddLine(ImVec2(x0, barMin.y), ImVec2(x0, barMax.y), COL_INK, 1.5f * S);
    } else if (bar.fillEnd > 0.0f && bar.fillEnd < 1.0f) {
        const float x = barMin.x + barW * bar.fillEnd;
        dl->AddLine(ImVec2(x, barMin.y), ImVec2(x, barMax.y), COL_INK, 1.5f * S);
    }
    dl->PopClipRect();
    dl->AddRect(barMin, barMax, showDead ? COL_BORDER_HI : COL_INK, barRound, 0, 1.5f * S);
    if (c.getTempHp() > 0) {
        const float frac = std::min(1.0f, static_cast<float>(c.getTempHp()) / static_cast<float>(c.getMaxHp()));
        const ImVec2 tMin(barMin.x + 2 * S, barMax.y + 3 * S);
        const ImVec2 tMax(barMin.x + 2 * S + std::max(6 * S, (barW - 4 * S) * frac), barMax.y + 8 * S);
        dl->AddRectFilled(tMin, tMax, COL_TEMP, 3 * S);
    }

    // Damage / Heal buttons.
    const float buttonWidth = (size.x - pad * 2 - 10 * S - 3 * S) * 0.5f;
    const float buttonHeight = 28 * S;
    const float buttonsY = barMax.y + 12 * S;
    ImGui::SetCursorScreenPos(ImVec2(p.x + pad, buttonsY));
    const std::string damageLabel = "Damage " + amountLabel + "##dmg";
    const std::string healLabel = "Heal " + amountLabel + "##heal";
    // Covered cards only draw their buttons (no ImGui item), so they can't
    // steal the press from the card in front.
    if (flatButton(damageLabel.c_str(), ImVec2(buttonWidth, buttonHeight), BTN_RED, true, interactive)) {
        act.damage(ref);
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x + pad + buttonWidth + 10 * S, buttonsY));
    if (flatButton(healLabel.c_str(), ImVec2(buttonWidth, buttonHeight), BTN_GREEN, true, interactive)) {
        act.heal(ref);
    }

    // Death saves (players at 0 HP): 3 successes, 3 failures, click to set.
    if (side == Side::Players && c.getCurrentHp() == 0) {
        const float d = 13 * S;
        const float gap = 4 * S;
        const float groupGap = 12 * S;
        const float cy = namePos.y + lineH * nameMul * 0.5f;
        float x = pEnd.x - pad - (d * 6 + gap * 4 + groupGap);
        for (int group = 0; group < 2; group++) {
            const bool success = group == 0;
            const int have = success ? c.getDeathSuccesses() : c.getDeathFailures();
            for (int i = 0; i < 3; i++) {
                const ImVec2 center(x + d * 0.5f, cy);
                const bool filled = i < have;
                dl->AddCircleFilled(center, d * 0.5f, filled ? (success ? COL_GREEN : COL_RED) : COL_PANEL);
                dl->AddCircle(center, d * 0.5f, filled ? COL_INK : COL_BORDER_HI, 0, 1.5f * S);
                if (interactive) {
                    ImGui::SetCursorScreenPos(ImVec2(x, cy - d * 0.5f));
                    ImGui::PushID(group * 10 + i);
                    if (ImGui::InvisibleButton("pip", ImVec2(d, d))) {
                        const int value = (have == i + 1) ? i : i + 1;  // click the last filled pip to clear it
                        if (success) act.setDeathSaves(ref, value, c.getDeathFailures());
                        else act.setDeathSaves(ref, c.getDeathSuccesses(), value);
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", success ? "Death save successes" : "Death save failures");
                    }
                    ImGui::PopID();
                }
                x += d + gap;
            }
            x += groupGap - gap;
        }
    }

    // Status chips: life state, bloodied, concentration, resistances, conditions, notes.
    drawChips(dl, ImVec2(p.x + pad, buttonsY + buttonHeight + 10 * S), pEnd.x - pad,
              cardChips(side, c, life, time), S);

    // The whole card is the drag handle. It is submitted last so the buttons
    // above get hover/click priority; ImGui gives an earlier overlapping item
    // the press otherwise.
    if (interactive) {
        ImGui::SetCursorScreenPos(p);
        ImGui::InvisibleButton("drag", size);
        const ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsItemActivated()) {
            st.pressed = true;
            st.pressMouse = io.MousePos;
        }
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f)) {
            pos.x += io.MouseDelta.x;
            pos.y += io.MouseDelta.y;
            st.beingDragged = true;
        }
        if (ImGui::IsItemDeactivated()) {
            const ImVec2 d = io.MousePos - st.pressMouse;
            if (d.x * d.x + d.y * d.y < 16.0f) st.clicked = true;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            st.wantsEdit = true;
        }
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal) && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            showTooltip = true;
        }
    }

    // Newly added cards fade in (together with their drop from above).
    if (spawnK < 1.0f) fadeVertices(dl, vtxStart, spawnK, COL_BOARD);

    cardFont.reset();  // tooltips use the normal UI size
    if (showTooltip) cardTooltip(side, c);
    ImGui::PopID();
}

// ---- Board operations ------------------------------------------------------

static CardRef addCombatant(Encounter& encounter, BoardState& board, Side side, Creature c, ImVec2 pos) {
    CardState st;
    st.pos = pos;
    st.lastHp = c.getCurrentHp();
    st.wasDead = lifeOf(side, c) == Life::Dead;  // already dead (load/undo): no death animation
    std::vector<CardState>& list = board.of(side);
    list.push_back(st);
    const CardRef ref{side, static_cast<int>(list.size()) - 1};
    board.z.push_back(ref);  // new cards go in front
    if (side == Side::Players) {
        encounter.addPlayer(std::move(c));
    } else {
        encounter.addCreature(std::move(c));
    }
    return ref;
}

// Removes a combatant and keeps the UI state and stacking order in sync.
static void removeCombatant(Encounter& encounter, BoardState& board, CardRef ref) {
    encounter.remove(ref.side, ref.index);
    std::vector<CardState>& list = board.of(ref.side);
    if (ref.index >= 0 && ref.index < static_cast<int>(list.size())) {
        list.erase(list.begin() + ref.index);
    }
    std::vector<CardRef> kept;
    for (CardRef r : board.z) {
        if (r.side == ref.side) {
            if (r.index == ref.index) continue;
            if (r.index > ref.index) r.index--;
        }
        kept.push_back(r);
    }
    board.z = kept;
    board.dragValid = false;
}

// Removes several at once (highest index first, so the others stay valid).
static void removeCombatants(Encounter& encounter, BoardState& board, std::vector<CardRef> refs) {
    std::sort(refs.begin(), refs.end(), [](CardRef a, CardRef b) {
        if (a.side != b.side) return a.side < b.side;
        return a.index > b.index;
    });
    for (CardRef r : refs) removeCombatant(encounter, board, r);
}

// "Goblin 3" -> ("Goblin", 3); "Goblin" -> ("Goblin", 0).
static std::string splitNumberedName(const std::string& name, int* number) {
    *number = 0;
    const size_t space = name.find_last_of(' ');
    if (space == std::string::npos || space + 1 >= name.size()) return name;
    const std::string tail = name.substr(space + 1);
    if (tail.size() > 4 || !std::all_of(tail.begin(), tail.end(), [](char ch) { return ch >= '0' && ch <= '9'; })) {
        return name;
    }
    *number = std::stoi(tail);
    return name.substr(0, space);
}

// Names for `count` new combatants called `requested`. Numbers them when there
// is more than one (or one already exists), renaming a plain "Goblin" to "Goblin 1".
static std::vector<std::string> makeNumberedNames(Encounter& encounter, const std::string& requested, int count) {
    int dummy = 0;
    const std::string base = splitNumberedName(requested, &dummy);
    int highest = 0;
    bool plainExists = false;
    for (Side side : {Side::Players, Side::Creatures}) {
        const std::vector<Creature>& g = side == Side::Players ? encounter.getPlayers() : encounter.getCreatures();
        for (const Creature& c : g) {
            int n = 0;
            if (splitNumberedName(c.getName(), &n) != base) continue;
            if (n == 0) plainExists = true;
            highest = std::max(highest, n);
        }
    }
    std::vector<std::string> names;
    if (count == 1 && !plainExists && highest == 0) {
        names.push_back(base);
        return names;
    }
    if (plainExists) {
        for (Side side : {Side::Players, Side::Creatures}) {
            const int n = static_cast<int>((side == Side::Players ? encounter.getPlayers() : encounter.getCreatures()).size());
            for (int i = 0; i < n; i++) {
                Creature* c = encounter.at(side, i);
                if (c->getName() == base) c->rename(base + " 1");
            }
        }
        highest = std::max(highest, 1);
    }
    for (int i = 1; i <= count; i++) names.push_back(base + " " + std::to_string(highest + i));
    return names;
}

// ---- Save format -----------------------------------------------------------

// Escapes the characters the line format uses as separators.
static std::string esc(const std::string& s) {
    std::string out;
    for (char ch : s) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '|':  out += "\\p"; break;
            case '\n': out += "\\n"; break;
            case ',':  out += "\\c"; break;
            case ':':  out += "\\o"; break;
            case '\r': break;
            default:   out += ch;
        }
    }
    return out;
}

static std::string unesc(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            const char n = s[++i];
            if (n == 'p') out += '|';
            else if (n == 'n') out += '\n';
            else if (n == 'c') out += ',';
            else if (n == 'o') out += ':';
            else out += n;
        } else {
            out += s[i];
        }
    }
    return out;
}

static std::vector<std::string> splitRaw(const std::string& s, char sep) {
    std::vector<std::string> parts;
    std::string cur;
    for (char ch : s) {
        if (ch == sep) {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur += ch;
        }
    }
    parts.push_back(cur);
    return parts;
}

static std::string savePath(const char* file) {
    std::string dir;
    if (char* pref = SDL_GetPrefPath("HotelWorkshop", "HPTracker")) {
        dir = pref;
        SDL_free(pref);
    }
    return dir + file;
}

// Version 2, one combatant per line in stacking order:
//   P|name|maxHp|hp|x|y|temp|ac|speed|pp|level|cr|mod|conc|deathOk|deathFail|cond:rounds,...|notes
// plus R|round and E|ended. Version 1 lines (P|name|maxHp|hp|x|y) still load.
static std::string serializeBoard(const Encounter& encounter, const BoardState& board) {
    std::ostringstream out;
    out << "HPTRACKER 2\n";
    out << "R|" << board.round << "\n";
    out << "E|" << (encounter.isEndedManually() ? 1 : 0) << "\n";
    for (const CardRef& r : board.z) {
        const Creature& c = *encounter.at(r.side, r.index);
        const CardState& st = board.of(r.side)[r.index];
        std::string conds;
        for (const Condition& cond : c.getConditions()) {
            if (!conds.empty()) conds += ",";
            conds += esc(cond.name) + ":" + std::to_string(cond.rounds);
        }
        out << (r.side == Side::Players ? 'P' : 'C') << '|' << esc(c.getName()) << '|' << c.getMaxHp() << '|'
            << c.getCurrentHp() << '|' << st.pos.x << '|' << st.pos.y << '|' << c.getTempHp() << '|' << c.getAc()
            << '|' << c.getSpeed() << '|' << c.getPassivePerception() << '|' << c.getLevel() << '|'
            << c.getCrIndex() << '|' << static_cast<int>(c.getDamageMod()) << '|' << (c.isConcentrating() ? 1 : 0)
            << '|' << c.getDeathSuccesses() << '|' << c.getDeathFailures() << '|' << conds << '|'
            << esc(c.getNotes()) << '\n';
    }
    return out.str();
}

static bool deserializeBoard(const std::string& text, Encounter& encounter, BoardState& board) {
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line) || line.rfind("HPTRACKER", 0) != 0) return false;

    encounter.clear();
    board = BoardState();
    while (std::getline(in, line)) {
        const std::vector<std::string> f = splitRaw(line, '|');
        try {
            if (f.size() == 2 && f[0] == "R") {
                board.round = std::max(1, std::stoi(f[1]));
                continue;
            }
            if (f.size() == 2 && f[0] == "E") {
                if (f[1] == "1") encounter.endManually();
                continue;
            }
            if ((f.size() != 6 && f.size() != 18) || (f[0] != "P" && f[0] != "C")) continue;
            const int maxHp = std::max(1, std::stoi(f[2]));
            Creature c(unesc(f[1]), maxHp);
            c.setHp(std::clamp(std::stoi(f[3]), 0, maxHp));
            const ImVec2 pos(std::stof(f[4]), std::stof(f[5]));
            if (f.size() == 18) {
                c.setTempHp(std::stoi(f[6]));
                c.setAc(std::stoi(f[7]));
                c.setSpeed(std::stoi(f[8]));
                c.setPassivePerception(std::stoi(f[9]));
                c.setLevel(std::stoi(f[10]));
                c.setCrIndex(std::clamp(std::stoi(f[11]), -1, crCount() - 1));
                c.setDamageMod(static_cast<DamageMod>(std::clamp(std::stoi(f[12]), 0, 3)));
                c.setConcentrating(f[13] == "1");
                c.setDeathSaves(std::stoi(f[14]), std::stoi(f[15]));
                if (!f[16].empty()) {
                    for (const std::string& item : splitRaw(f[16], ',')) {
                        const std::vector<std::string> kv = splitRaw(item, ':');
                        if (kv.size() == 2) c.addCondition(unesc(kv[0]), std::stoi(kv[1]));
                    }
                }
                c.setNotes(unesc(f[17]));
            }
            addCombatant(encounter, board, f[0] == "P" ? Side::Players : Side::Creatures, std::move(c), pos);
        } catch (const std::exception&) {
            continue;  // skip malformed line
        }
    }
    return true;
}

// Save paths are UTF-8 (from SDL); on Windows they must be opened as wide paths.
#ifdef _WIN32
static std::filesystem::path fsPath(const std::string& utf8) { return std::filesystem::u8path(utf8); }
#else
static const std::string& fsPath(const std::string& utf8) { return utf8; }
#endif

static bool writeFile(const std::string& path, const std::string& text) {
    std::ofstream out(fsPath(path), std::ios::trunc);
    if (!out) return false;
    out << text;
    return static_cast<bool>(out);
}

static bool readFile(const std::string& path, std::string& text) {
    std::ifstream in(fsPath(path));
    if (!in) return false;
    std::stringstream buffer;
    buffer << in.rdbuf();
    text = buffer.str();
    return true;
}

// ---- Monster library ---------------------------------------------------------

struct MonsterTemplate {
    std::string name;
    int maxHp = 10;
    int ac = 10;
    int speed = 30;
    int pp = 10;
    int cr = -1;
    DamageMod mod = DamageMod::Normal;
};

static std::vector<MonsterTemplate> defaultLibrary() {
    // name, HP, AC, speed, passive perception, CR index (2 = 1/4, 3 = 1/2, 4 = 1 ...)
    return {
        {"Bandit", 11, 12, 30, 10, 1, DamageMod::Normal},
        {"Goblin", 7, 15, 30, 9, 2, DamageMod::Normal},
        {"Kobold", 5, 12, 30, 8, 1, DamageMod::Normal},
        {"Skeleton", 13, 13, 30, 9, 2, DamageMod::Normal},
        {"Wolf", 11, 13, 40, 13, 2, DamageMod::Normal},
        {"Zombie", 22, 8, 20, 8, 2, DamageMod::Normal},
        {"Orc", 15, 13, 30, 10, 3, DamageMod::Normal},
        {"Hobgoblin", 11, 18, 30, 10, 3, DamageMod::Normal},
        {"Bugbear", 27, 16, 30, 10, 4, DamageMod::Normal},
        {"Ghoul", 22, 12, 30, 10, 4, DamageMod::Normal},
        {"Ogre", 59, 11, 40, 8, 5, DamageMod::Normal},
        {"Owlbear", 59, 13, 40, 13, 6, DamageMod::Normal},
        {"Troll", 84, 15, 30, 12, 8, DamageMod::Normal},
        {"Young Red Dragon", 178, 18, 40, 18, 13, DamageMod::Normal},
    };
}

static std::string serializeLibrary(const std::vector<MonsterTemplate>& lib) {
    std::ostringstream out;
    out << "HPLIBRARY 1\n";
    for (const MonsterTemplate& t : lib) {
        out << "T|" << esc(t.name) << '|' << t.maxHp << '|' << t.ac << '|' << t.speed << '|' << t.pp << '|'
            << t.cr << '|' << static_cast<int>(t.mod) << '\n';
    }
    return out.str();
}

static bool deserializeLibrary(const std::string& text, std::vector<MonsterTemplate>& lib) {
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line) || line.rfind("HPLIBRARY", 0) != 0) return false;
    lib.clear();
    while (std::getline(in, line)) {
        const std::vector<std::string> f = splitRaw(line, '|');
        if (f.size() != 8 || f[0] != "T") continue;
        try {
            MonsterTemplate t;
            t.name = unesc(f[1]);
            t.maxHp = std::max(1, std::stoi(f[2]));
            t.ac = std::stoi(f[3]);
            t.speed = std::stoi(f[4]);
            t.pp = std::stoi(f[5]);
            t.cr = std::clamp(std::stoi(f[6]), -1, crCount() - 1);
            t.mod = static_cast<DamageMod>(std::clamp(std::stoi(f[7]), 0, 3));
            lib.push_back(t);
        } catch (const std::exception&) {
            continue;
        }
    }
    return true;
}

static Creature creatureFromTemplate(const MonsterTemplate& t) {
    Creature c(t.name, t.maxHp);
    c.setAc(t.ac);
    c.setSpeed(t.speed);
    c.setPassivePerception(t.pp);
    c.setCrIndex(t.cr);
    c.setDamageMod(t.mod);
    return c;
}

// ---- Board decoration ----------------------------------------------------------

static void drawTrash(ImVec2 min, float size, bool dragging, bool hot, int count, float time, float S) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float scale = 1.0f;
    if (hot) scale = 1.14f;
    else if (dragging) scale = 1.0f + 0.04f * std::sin(time * 9.0f);
    const ImVec2 c(min.x + size * 0.5f, min.y + size * 0.5f);
    const float half = size * 0.5f * scale;
    const ImVec2 a(c.x - half, c.y - half), b(c.x + half, c.y + half);
    const float round = 18 * S;

    // Idle: a quiet outlined tile. While dragging it gets an ink outline and
    // shadow; over it, it turns red.
    if (hot) {
        inkBox(dl, a, b, round, S, IM_COL32(255, 232, 228, 255), 3.5f, COL_RED, 1.0f, 2.0f);
    } else if (dragging) {
        inkBox(dl, a, b, round, S, COL_PANEL, 3.5f, COL_INK, 1.0f, 2.0f);
    } else {
        inkBox(dl, a, b, round, S, COL_PANEL, 0.0f, COL_BORDER_HI, 1.0f, 1.5f);
    }
    const ImU32 icon = hot ? COL_RED : (dragging ? COL_INK : COL_MUTED);

    const float k = half * 0.62f;
    const float th = 2.2f * S;
    // Lid lifts and tilts when a card is hovering over the bin.
    const ImVec2 lid = hot ? ImVec2(k * 0.22f, -k * 0.32f) : ImVec2(0, 0);
    auto P = [&](float x, float y, bool onLid) {
        return ImVec2(c.x + x * k + (onLid ? lid.x : 0), c.y + y * k + (onLid ? lid.y : 0));
    };
    ImVec2 body[] = {P(-0.62f, -0.30f, false), P(0.62f, -0.30f, false), P(0.50f, 0.90f, false), P(-0.50f, 0.90f, false)};
    dl->AddPolyline(body, 4, icon, ImDrawFlags_Closed, th);
    for (float x : {-0.22f, 0.0f, 0.22f}) {
        dl->AddLine(P(x, -0.05f, false), P(x, 0.62f, false), icon, th * 0.8f);
    }
    dl->AddLine(P(-0.82f, -0.48f, true), P(0.82f, -0.48f, true), icon, th);
    ImVec2 handle[] = {P(-0.24f, -0.48f, true), P(-0.24f, -0.74f, true), P(0.24f, -0.74f, true), P(0.24f, -0.48f, true)};
    dl->AddPolyline(handle, 4, icon, 0, th);

    if (dragging) {
        const std::string text = count > 1 ? "Drop to delete " + std::to_string(count) : "Drop to delete";
        const ImVec2 ts = boldTextSize(text.c_str());
        boldText(dl, ImVec2(c.x - ts.x * 0.5f, a.y - ts.y - 10 * S), hot ? COL_RED : COL_TEXT, text.c_str());
    }
}

// Plain light board with a faint dot grid, like graph paper. The board's
// own child window paints the fill and the rounded ink outline.
static void drawBoardBackground(ImVec2 origin, ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 end(origin.x + size.x, origin.y + size.y);
    dl->PushClipRect(origin, end, true);
    const float grid = 32 * UI_SCALE;
    for (float x = grid; x < size.x; x += grid) {
        for (float y = grid; y < size.y; y += grid) {
            dl->AddCircleFilled(ImVec2(origin.x + x, origin.y + y), 1.3f * UI_SCALE, IM_COL32(0, 0, 0, 34));
        }
    }
    dl->PopClipRect();
}

// ---- Small widgets -------------------------------------------------------------

static bool crCombo(const char* label, int* index) {
    bool changed = false;
    const std::string preview = *index < 0 ? std::string("-") : std::string(crLabel(*index)) + "  (" +
                                std::to_string(crXp(*index)) + " XP)";
    if (ImGui::BeginCombo(label, preview.c_str())) {
        if (ImGui::Selectable("-  (not set)", *index < 0)) {
            *index = -1;
            changed = true;
        }
        for (int i = 0; i < crCount(); i++) {
            const std::string item = std::string("CR ") + crLabel(i) + "  (" + std::to_string(crXp(i)) + " XP)";
            if (ImGui::Selectable(item.c_str(), *index == i)) {
                *index = i;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

static bool damageModCombo(const char* label, DamageMod* mod) {
    int v = static_cast<int>(*mod);
    const char* items[] = {"Normal", "Resistant (half)", "Vulnerable (double)", "Immune (none)"};
    if (ImGui::Combo(label, &v, items, 4)) {
        *mod = static_cast<DamageMod>(v);
        return true;
    }
    return false;
}

// ---- Log and alerts --------------------------------------------------------------

struct LogEntry {
    std::string text;
    ImU32 color;
    int round;
};

struct Alert {
    std::string text;
    ImU32 color;
    double born;     // ImGui time when raised (slide-in and fade-out)
    float y = -1.0f; // animated top offset in the stack (-1 = not placed yet)
};

// 0..1 opacity: fully visible for ALERT_HOLD seconds, then fading out.
static float alertOpacity(const Alert& al, double now) {
    const double age = now - al.born;
    if (age <= ALERT_HOLD) return 1.0f;
    return static_cast<float>(std::clamp(1.0 - (age - ALERT_HOLD) / ALERT_FADE, 0.0, 1.0));
}

static bool alertExpired(const Alert& al, double now) {
    return now - al.born >= ALERT_HOLD + ALERT_FADE;
}

// Alert toasts in the board's top-right corner. They are display-only (the
// mouse goes straight through to the board) and fade away on their own.
static void drawToasts(std::vector<Alert>& alerts, ImVec2 boardOrigin, ImVec2 boardSize, double now, float dt,
                       float S) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float tw = std::min(340 * S, boardSize.x - 40 * S);
    const float x1 = boardOrigin.x + boardSize.x - 22 * S;
    const float textW = tw - 46 * S;
    const float lineH = ImGui::GetTextLineHeight();
    float target = 16 * S;
    for (Alert& al : alerts) {
        const ImVec2 ts = ImGui::CalcTextSize(al.text.c_str(), nullptr, false, textW);
        const float h = ts.y + 22 * S;
        // Glide to the stack position, so the rest move up smoothly when one fades out.
        if (al.y < 0.0f) al.y = target;
        else al.y += (target - al.y) * std::min(1.0f, dt * 12.0f);
        const float age = static_cast<float>(now - al.born);
        const float alpha = alertOpacity(al, now);
        const float slide = (1.0f - easeOutCubic(clamp01(age / 0.25f))) * 80 * S;
        const ImVec2 a(x1 - tw + slide, boardOrigin.y + al.y), b(x1 + slide, boardOrigin.y + al.y + h);
        inkBox(dl, a, b, 14 * S, S, COL_PANEL, 3.0f, COL_INK, alpha);
        dl->AddCircleFilled(ImVec2(a.x + 18 * S, a.y + 11 * S + lineH * 0.5f), 5 * S, fadeAlpha(al.color, alpha));
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(a.x + 32 * S, a.y + 11 * S),
                    fadeAlpha(COL_TEXT, alpha), al.text.c_str(), nullptr, textW);
        target += h + 12 * S;
    }
}

int main(int, char**) {
#ifdef _WIN32
    // Crisp text on scaled displays (125%, 150%...): work in scaled points like
    // macOS does, and render at the real pixel density.
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
#endif
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("DnD HP Tracker", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, 1360, 900,
                                          SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    applyTheme();
    ImGui::GetStyle().ScaleAllSizes(UI_SCALE);

    // Rasterize the font at pixel density so text stays crisp on Retina,
    // then scale it back down to ImGui's point-based coordinates.
    int winW = 1, outW = 1, winH = 1, outH = 1;
    SDL_GetWindowSize(window, &winW, &winH);
    SDL_GetRendererOutputSize(renderer, &outW, &outH);
    const float dpi = winW > 0 ? std::max(1.0f, static_cast<float>(outW) / static_cast<float>(winW)) : 1.0f;
    if (loadSystemFonts(15.0f * UI_SCALE * dpi)) {
        io.FontGlobalScale = 1.0f / dpi;
    } else {
        io.FontGlobalScale = UI_SCALE;
    }

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    const float S = UI_SCALE;

    Encounter encounter;
    BoardState board;
    // New cards go to the first free grid spot on the board (cascading if it's full).
    ImVec2 lastBoardSize(900, 850);  // updated every frame once the board is laid out
    auto spawnPos = [&]() {
        const ImVec2 cs = tokenSize();
        const float tab = TAB_H * cardS();
        const float pitchX = cs.x + 30 * S, pitchY = cs.y + tab + 22 * S;
        const int cols = std::max(1, static_cast<int>((lastBoardSize.x - 20 * S) / pitchX));
        const int rows = std::max(1, static_cast<int>((lastBoardSize.y - 30 * S) / pitchY));
        auto isFree = [&](ImVec2 p) {
            for (const std::vector<CardState>* list : {&board.players, &board.creatures}) {
                for (const CardState& o : *list) {
                    const bool overlap = p.x < o.pos.x + cs.x && p.x + cs.x > o.pos.x &&
                                         p.y - tab < o.pos.y + cs.y && p.y + cs.y > o.pos.y - tab;
                    if (overlap) return false;
                }
            }
            return true;
        };
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                const ImVec2 p(20 * S + c * pitchX, 22 * S + tab + r * pitchY);
                if (isFree(p)) return p;
            }
        }
        // Board is full: take the spot that overlaps existing cards the least.
        ImVec2 best(20 * S, 22 * S + tab);
        float bestOverlap = FLT_MAX;
        const float step = 24 * S;
        for (float y = 22 * S + tab; y + cs.y <= lastBoardSize.y; y += step) {
            for (float x = 20 * S; x + cs.x <= lastBoardSize.x; x += step) {
                float overlap = 0.0f;
                for (const std::vector<CardState>* list : {&board.players, &board.creatures}) {
                    for (const CardState& o : *list) {
                        const float w = std::min(x + cs.x, o.pos.x + cs.x) - std::max(x, o.pos.x);
                        const float h = std::min(y + cs.y, o.pos.y + cs.y) - std::max(y - tab, o.pos.y - tab);
                        if (w > 0 && h > 0) overlap += w * h;
                    }
                }
                if (overlap < bestOverlap) {
                    bestOverlap = overlap;
                    best = ImVec2(x, y);
                }
            }
        }
        return best;
    };

    // ---- Log & alerts
    std::vector<LogEntry> log;
    std::vector<Alert> alerts;
    bool logScrollToBottom = true;
    auto addLog = [&](const std::string& text, ImU32 color) {
        log.push_back(LogEntry{text, color, board.round});
        if (log.size() > 1000) log.erase(log.begin());
        logScrollToBottom = true;
    };
    auto addAlert = [&](const std::string& text, ImU32 color) {
        alerts.erase(std::remove_if(alerts.begin(), alerts.end(), [&](const Alert& a) { return a.text == text; }),
                     alerts.end());
        alerts.push_back(Alert{text, color, ImGui::GetTime()});
        if (alerts.size() > 6) alerts.erase(alerts.begin());
        addLog(text, color);
    };

    // ---- Persistence
    const std::string path = savePath("encounter.txt");
    const std::string libraryPath = savePath("library.txt");
    std::string statusMsg;
    {
        std::string text;
        if (readFile(path, text) && deserializeBoard(text, encounter, board)) {
            statusMsg = "Loaded last session.";
        } else {
            Creature hero("Hero", 40);
            hero.setAc(16);
            hero.setLevel(3);
            addCombatant(encounter, board, Side::Players, hero, spawnPos());
            for (const MonsterTemplate& t : defaultLibrary()) {
                if (t.name == "Goblin" || t.name == "Orc") {
                    addCombatant(encounter, board, Side::Creatures, creatureFromTemplate(t), spawnPos());
                }
            }
        }
    }
    std::vector<MonsterTemplate> library;
    {
        std::string text;
        if (!readFile(libraryPath, text) || !deserializeLibrary(text, library)) {
            library = defaultLibrary();
        }
    }
    auto saveLibrary = [&]() { writeFile(libraryPath, serializeLibrary(library)); };

    // View settings (not part of the encounter, so undo doesn't touch them).
    const std::string settingsPath = savePath("settings.txt");
    {
        std::string text;
        if (readFile(settingsPath, text) && text.rfind("cardScale=", 0) == 0) {
            try {
                g_cardScale = std::clamp(std::stof(text.substr(10)), 0.6f, 1.0f);
            } catch (const std::exception&) {
            }
        }
    }
    auto saveSettings = [&]() { writeFile(settingsPath, "cardScale=" + std::to_string(g_cardScale) + "\n"); };
    double lastAutosave = 0.0;

    // Undo/redo works on whole-board snapshots (same text as the save file),
    // so moves, HP changes, adds, edits and deletes are all undoable.
    std::vector<std::string> undoStack, redoStack;
    std::string pressSnapshot;  // board state when the mouse last went down
    auto snapshot = [&]() { return serializeBoard(encounter, board); };
    auto pushUndoState = [&](const std::string& state) {
        undoStack.push_back(state);
        if (undoStack.size() > 100) undoStack.erase(undoStack.begin());
        redoStack.clear();
    };
    auto pushUndo = [&]() { pushUndoState(snapshot()); };
    auto doUndo = [&]() {
        if (undoStack.empty()) return;
        redoStack.push_back(snapshot());
        const std::string state = undoStack.back();
        undoStack.pop_back();
        deserializeBoard(state, encounter, board);
        addLog("Undo", COL_MUTED);
    };
    auto doRedo = [&]() {
        if (redoStack.empty()) return;
        undoStack.push_back(snapshot());
        const std::string state = redoStack.back();
        redoStack.pop_back();
        deserializeBoard(state, encounter, board);
        addLog("Redo", COL_MUTED);
    };

    // ---- Amount used by every Damage / Heal / temp HP button.
    int actionAmount = 5;

    // ---- Rules actions (each caller pushes undo once before a batch)
    auto nameOf = [&](CardRef r) { return encounter.at(r.side, r.index)->getName(); };
    auto applyHit = [&](CardRef r, int amount) {
        Creature* c = encounter.at(r.side, r.index);
        if (c == nullptr) return;
        CardState& st = board.get(r);
        const std::string name = c->getName();
        const HitReport h = encounter.damage(r.side, r.index, amount);
        const DamageResult& d = h.damage;

        if (d.hpLost > 0) {
            st.requested = d.afterMod;  // "crit" is judged on damage actually taken
            st.pendingKind = FxKind::Damage;
            st.pendingLabel = "-" + std::to_string(d.hpLost);
        } else if (d.absorbed > 0) {
            st.pendingKind = FxKind::Temp;
            st.pendingLabel = "-" + std::to_string(d.absorbed) + " temp";
        } else if (d.rolled > 0 && c->getDamageMod() == DamageMod::Immune) {
            st.pendingKind = FxKind::Immune;
            st.pendingLabel = "IMMUNE";
        }

        std::string line = name + " takes " + std::to_string(d.afterMod);
        if (c->getDamageMod() == DamageMod::Resistant) line += " (resisted " + std::to_string(d.rolled) + ")";
        if (c->getDamageMod() == DamageMod::Vulnerable) line += " (vulnerable, " + std::to_string(d.rolled) + " x2)";
        if (c->getDamageMod() == DamageMod::Immune) line = name + " is immune (" + std::to_string(d.rolled) + " ignored)";
        if (d.absorbed > 0) line += ", " + std::to_string(d.absorbed) + " absorbed by temp HP";
        line += " -> " + std::to_string(c->getCurrentHp()) + "/" + std::to_string(c->getMaxHp());
        addLog(line, COL_RED);

        if (h.concentrationDc > 0) {
            addAlert(name + ": concentration save, DC " + std::to_string(h.concentrationDc), COL_VIOLET);
        }
        if (h.concentrationLost) addAlert(name + " drops to 0 HP and loses concentration", COL_VIOLET);
        if (h.instantDeath) {
            addAlert(name + " is killed outright (massive damage)", COL_RED);
        } else if (h.deathSaveFailed) {
            if (h.killedOutright) {
                addAlert(name + " fails a third death save and dies", COL_RED);
            } else {
                addAlert(name + " is hit at 0 HP: death save failure (" + std::to_string(c->getDeathFailures()) + "/3)",
                         COL_RED);
            }
        } else if (h.droppedToZero) {
            addLog(name + (r.side == Side::Players ? " falls unconscious and is dying" : " is defeated"), COL_MUTED);
        }
    };
    auto applyHeal = [&](CardRef r, int amount) {
        Creature* c = encounter.at(r.side, r.index);
        if (c == nullptr) return;
        const bool wasDown = c->getCurrentHp() == 0;
        const int gained = encounter.heal(r.side, r.index, amount);
        CardState& st = board.get(r);
        st.requested = amount;
        st.pendingKind = FxKind::Heal;
        st.pendingLabel = gained > 0 ? "+" + std::to_string(gained) : "FULL";
        std::string line = c->getName() + " heals " + std::to_string(gained) + " -> " +
                           std::to_string(c->getCurrentHp()) + "/" + std::to_string(c->getMaxHp());
        addLog(line, COL_GREEN);
        if (wasDown && gained > 0 && r.side == Side::Players) addLog(c->getName() + " is back on their feet", COL_GREEN);
    };
    auto applyTemp = [&](CardRef r, int amount) {
        Creature* c = encounter.at(r.side, r.index);
        if (c == nullptr) return;
        const int before = c->getTempHp();
        c->grantTempHp(amount);
        CardState& st = board.get(r);
        st.pendingKind = FxKind::Temp;
        st.pendingLabel = "+" + std::to_string(c->getTempHp() - before) + " temp";
        addLog(c->getName() + " has " + std::to_string(c->getTempHp()) + " temp HP" +
                   (amount <= before ? " (kept the higher value)" : ""),
               COL_TEMP);
    };

    // Lays every card out in a grid (party first, enemies from a new row),
    // shrinking the cards if they wouldn't fit otherwise.
    auto tidyBoard = [&]() {
        pushUndo();
        const int nPlayers = static_cast<int>(board.players.size());
        const int nCreatures = static_cast<int>(board.creatures.size());
        auto layoutFits = [&](int* colsOut) {
            const ImVec2 cs = tokenSize();
            const float tab = TAB_H * cardS();
            const float pitchX = cs.x + 24 * S, pitchY = cs.y + tab + 18 * S;
            const int cols = std::max(1, static_cast<int>((lastBoardSize.x - 16 * S + 24 * S) / pitchX));
            const int rows = (nPlayers + cols - 1) / cols + (nCreatures + cols - 1) / cols;
            *colsOut = cols;
            return 14 * S + rows * pitchY <= lastBoardSize.y + 4 * S;
        };
        int cols = 1;
        while (!layoutFits(&cols) && g_cardScale > 0.6f + 0.001f) {
            g_cardScale = std::max(0.6f, g_cardScale - 0.05f);
        }
        saveSettings();
        const ImVec2 cs = tokenSize();
        const float tab = TAB_H * cardS();
        const float pitchX = cs.x + 24 * S, pitchY = cs.y + tab + 18 * S;
        for (int i = 0; i < nPlayers; i++) {
            board.players[i].pos = ImVec2(16 * S + (i % cols) * pitchX, 14 * S + tab + (i / cols) * pitchY);
        }
        const int firstEnemyRow = (nPlayers + cols - 1) / cols;
        for (int i = 0; i < nCreatures; i++) {
            board.creatures[i].pos = ImVec2(16 * S + (i % cols) * pitchX,
                                            14 * S + tab + (firstEnemyRow + i / cols) * pitchY);
        }
    };

    // ---- Selection
    auto selectedRefs = [&]() {
        std::vector<CardRef> refs;
        for (const CardRef& r : board.z) {
            if (board.get(r).selected) refs.push_back(r);
        }
        return refs;
    };
    auto clearSelection = [&]() {
        for (CardState& s : board.players) s.selected = false;
        for (CardState& s : board.creatures) s.selected = false;
    };
    auto selectAll = [&]() {
        for (CardState& s : board.players) s.selected = true;
        for (CardState& s : board.creatures) s.selected = true;
    };

    // ---- Structural actions
    auto duplicate = [&](const std::vector<CardRef>& refs) {
        if (refs.empty()) return;
        pushUndo();
        std::vector<std::pair<Side, Creature>> copies;
        for (CardRef r : refs) {
            Creature copy = *encounter.at(r.side, r.index);
            copy.setHp(copy.getMaxHp());
            copy.setTempHp(0);
            copy.clearConditions();
            copy.setConcentrating(false);
            copy.resetDeathSaves();
            copies.emplace_back(r.side, copy);
        }
        clearSelection();
        for (size_t i = 0; i < copies.size(); i++) {
            Creature c = copies[i].second;
            c.rename(makeNumberedNames(encounter, c.getName(), 1)[0]);
            const CardRef nr = addCombatant(encounter, board, copies[i].first, c, spawnPos());
            board.get(nr).selected = true;
            board.get(nr).spawnT = -0.06f * static_cast<float>(i);  // staggered fade-in
            addLog("Added " + c.getName(), COL_MUTED);
        }
    };
    auto deleteRefs = [&](const std::vector<CardRef>& refs, bool alreadyPushedUndo) {
        if (refs.empty()) return;
        if (!alreadyPushedUndo) pushUndo();
        std::string names;
        for (CardRef r : refs) names += (names.empty() ? "" : ", ") + nameOf(r);
        removeCombatants(encounter, board, refs);
        addLog("Removed " + names, COL_MUTED);
    };
    auto addFromTemplate = [&](const MonsterTemplate& t, Side side, int count) {
        pushUndo();
        const std::vector<std::string> names = makeNumberedNames(encounter, t.name, count);
        clearSelection();
        for (size_t i = 0; i < names.size(); i++) {
            Creature c = creatureFromTemplate(t);
            c.rename(names[i]);
            const CardRef r = addCombatant(encounter, board, side, c, spawnPos());
            board.get(r).selected = true;
            board.get(r).spawnT = -0.06f * static_cast<float>(i);  // staggered fade-in
        }
        addLog("Added " + std::to_string(count) + " x " + t.name, COL_MUTED);
    };

    // ---- Popup state
    std::optional<Creature> editCopy;
    CardRef editRef{Side::Players, 0};
    bool openEdit = false;
    char editName[64] = "";
    char editNotes[1024] = "";
    char editCustomCond[40] = "";

    bool openMenu = false;
    CardRef menuRef{Side::Players, 0};
    bool openNewEncounter = false;

    // ---- Board interaction state
    bool marqueeActive = false;
    ImVec2 marqueeStart(0, 0);

    // ---- Sidebar form state
    char newName[64] = "";
    int newMaxHp = 20;
    int newAc = 12;
    int newCount = 1;
    int newSide = 1;
    int newLevel = 1;
    int newCr = -1;
    char libraryFilter[48] = "";
    int libraryCount = 1;
    int selCondition = 10;  // Poisoned
    int selRounds = 0;
    int activeTab = 0;      // 0 Combat, 1 Add, 2 Log
    int shownTab = 0;       // tab the content animation last started for
    double tabSwitchTime = -10.0;
    float tabSlideDir = 1.0f;  // +1: new content comes from the right, -1: from the left

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_MOUSEWHEEL) {
                // Trackpads send lots of small wheel steps; slow scrolling down.
                event.wheel.preciseX *= WHEEL_SENSITIVITY;
                event.wheel.preciseY *= WHEEL_SENSITIVITY;
            }
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        const bool anyPopup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);

        // Keyboard shortcuts (not while typing or with a popup open).
        {
            const ImGuiIO& kio = ImGui::GetIO();
            const bool mod = kio.KeyCtrl || kio.KeySuper;
            if (!kio.WantTextInput && !anyPopup) {
                if (mod && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
                    if (kio.KeyShift) doRedo(); else doUndo();
                } else if (mod && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
                    doRedo();
                } else if (mod && ImGui::IsKeyPressed(ImGuiKey_A, false)) {
                    selectAll();
                } else if (mod && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
                    duplicate(selectedRefs());
                } else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
                    clearSelection();
                } else if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false)) {
                    deleteRefs(selectedRefs(), false);
                }
            }
        }

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Encounter", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);

        // ===================== Sidebar =====================
        const float panelRound = 22 * S;
        const ImVec2 sidebarSize(310 * S, ImGui::GetContentRegionAvail().y - 10 * S);
        {
            // Hard ink shadow behind the sidebar panel.
            const ImVec2 sp = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(sp + ImVec2(4 * S, 5 * S), sp + sidebarSize + ImVec2(4 * S, 5 * S),
                                                      COL_INK, panelRound);
        }
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, panelRound);
        ImGui::BeginChild("Controls", sidebarSize, true, ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar();

        // Headline.
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 p0 = ImGui::GetCursorScreenPos();
            const float mul = 1.9f;
            heavyText(dl, p0, COL_TEXT, "HP Tracker", mul);
            ImGui::Dummy(ImVec2(0, heavyTextSize("HP Tracker", mul).y));
        }

        // ---- Summary: round, enemies, party HP tiles; status and difficulty pills.
        {
            int enemiesUp = 0;
            for (const Creature& c : encounter.getCreatures()) enemiesUp += c.getCurrentHp() > 0 ? 1 : 0;
            int partyHp = 0, partyMax = 0, partyUp = 0;
            for (const Creature& c : encounter.getPlayers()) {
                partyHp += c.getCurrentHp();
                partyMax += c.getMaxHp();
                partyUp += c.getCurrentHp() > 0 ? 1 : 0;
            }
            const int enemyCount = static_cast<int>(encounter.getCreatures().size());
            const int partyCount = static_cast<int>(encounter.getPlayers().size());

            const float gap = 9 * S;
            const float tw = (ImGui::GetContentRegionAvail().x - 5 * S - gap * 2) / 3.0f;
            if (statTile("##round", "Round", std::to_string(board.round), TileIcon::Cycle, COL_GREEN, tw, S)) {
                ImGui::SetTooltip("Round %d. End round (Combat tab) counts timed conditions down.", board.round);
            }
            ImGui::SameLine(0, gap);
            if (statTile("##enemies", "Enemies", std::to_string(enemiesUp), TileIcon::Bolt, COL_RED, tw, S)) {
                ImGui::SetTooltip("%d of %d enemies standing", enemiesUp, enemyCount);
            }
            ImGui::SameLine(0, gap);
            if (statTile("##party", "Party HP", std::to_string(partyHp), TileIcon::Heart, COL_PLAYER, tw, S)) {
                ImGui::SetTooltip("Party: %d of %d up\nHP %d / %d", partyUp, partyCount, partyHp, partyMax);
            }

            // More specific than Encounter::isOver(): dying players can still be saved.
            const auto& ps = encounter.getPlayers();
            const auto& cs = encounter.getCreatures();
            const bool enemiesDown = !cs.empty() && std::all_of(cs.begin(), cs.end(), [](const Creature& c) { return c.getCurrentHp() == 0; });
            const bool partyDown = !ps.empty() && std::all_of(ps.begin(), ps.end(), [](const Creature& c) { return c.getCurrentHp() == 0; });
            const bool partyDead = partyDown && std::all_of(ps.begin(), ps.end(), [](const Creature& c) { return c.getDeathFailures() >= 3; });
            if (encounter.isEndedManually()) pill("Encounter over", COL_GRAY, S);
            else if (enemiesDown) pill("Enemies defeated", COL_GREEN, S);
            else if (partyDead) pill("Party defeated", COL_RED, S);
            else if (partyDown) pill("Party down!", COL_RED, S);
            else pill("In progress", COL_GREEN, S);

            std::vector<int> levels, xps;
            for (const Creature& c : encounter.getPlayers()) levels.push_back(c.getLevel());
            for (const Creature& c : encounter.getCreatures()) xps.push_back(crXp(c.getCrIndex()));
            const DifficultyReport dr = rateEncounter(levels, xps);
            if (dr.valid) {
                ImU32 col = COL_GRAY;
                switch (dr.rating) {
                    case Difficulty::Trivial: col = COL_GRAY; break;
                    case Difficulty::Easy:    col = COL_GREEN; break;
                    case Difficulty::Medium:  col = IM_COL32(234, 179, 8, 255); break;
                    case Difficulty::Hard:    col = IM_COL32(234, 108, 20, 255); break;
                    case Difficulty::Deadly:  col = COL_RED; break;
                }
                const std::string text = std::string(difficultyName(dr.rating)) + "  \xC2\xB7  " +
                                         std::to_string(dr.adjustedXp) + " XP";
                // Same row as the status pill when it fits.
                const float need = ImGui::CalcTextSize(text.c_str()).x + 44 * S;
                ImGui::SameLine(0, 8 * S);
                if (ImGui::GetContentRegionAvail().x < need) ImGui::NewLine();
                pill(text.c_str(), col, S);
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Party thresholds (5e DMG):");
                    ImGui::Text("Easy %d   Medium %d   Hard %d   Deadly %d", dr.thresholds[0], dr.thresholds[1],
                                dr.thresholds[2], dr.thresholds[3]);
                    ImGui::Text("Monster XP %d  x%.1f multiplier = %d", dr.baseXp, dr.multiplier, dr.adjustedXp);
                    ImGui::Text("XP award: %d total, %d per player", dr.baseXp, dr.xpPerPlayer);
                    ImGui::EndTooltip();
                }
            } else {
                ImGui::PushTextWrapPos(0);
                ImGui::TextDisabled("Difficulty: set player levels and monster CRs");
                ImGui::PopTextWrapPos();
            }
        }

        ImGui::Dummy(ImVec2(0, 2 * S));
        static const char* const TAB_LABELS[] = {"Combat", "Add", "Log"};
        segmentedTabs(TAB_LABELS, 3, &activeTab, S);
        ImGui::Dummy(ImVec2(0, 2 * S));
        // Switching tabs slides the new content in from the side of its tab while it fades in.
        if (activeTab != shownTab) {
            tabSlideDir = activeTab > shownTab ? 1.0f : -1.0f;
            shownTab = activeTab;
            tabSwitchTime = ImGui::GetTime();
        }
        const float tabK = easeOutCubic(clamp01(static_cast<float>(ImGui::GetTime() - tabSwitchTime) / 0.24f));
        // Applied to a tab body window just before it ends (covers everything it drew).
        auto tabBodyFx = [&]() {
            if (tabK < 1.0f) {
                fadeVertices(ImGui::GetWindowDrawList(), 0, tabK, COL_PANEL, ImVec2((1.0f - tabK) * 22 * S * tabSlideDir, 0));
            }
        };
        {
            // ================= Combat tab =================
            if (activeTab == 0) {
                ImGui::BeginChild("CombatBody", ImVec2(0, 0), false);

                beginSection("Amount");
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2 * S);
                ImGui::InputInt("##amount", &actionAmount, 1, 5);
                actionAmount = std::clamp(actionAmount, 0, 9999);
                {
                    const char* numbers[] = {"1", "5", "10", "20"};
                    const int values[] = {1, 5, 10, 20};
                    const float w4 = rowButtonWidth(4);
                    for (int i = 0; i < 4; i++) {
                        if (i > 0) ImGui::SameLine(0, 8 * S);
                        if (quietButton(numbers[i], ImVec2(w4, 0))) actionAmount = values[i];
                    }
                }
                endSection();

                // ---- Selection / area actions.
                const std::vector<CardRef> sel = selectedRefs();
                beginSection("Selection", sel.empty() ? std::string() : std::to_string(sel.size()) + " selected");
                if (sel.empty()) {
                    ImGui::PushTextWrapPos(0);
                    ImGui::TextDisabled("Click a card to select it. Shift/Cmd-click to add more, or drag a box on the empty board.");
                    ImGui::PopTextWrapPos();
                    if (primaryButton("Select all", ImVec2(rowButtonWidth(1), 0))) selectAll();
                } else {
                    const float w2 = rowButtonWidth(2);
                    if (dangerButton("Damage all", ImVec2(w2, 0))) {
                        pushUndo();
                        if (sel.size() > 1) addLog("Area damage " + std::to_string(actionAmount) + " to " + std::to_string(sel.size()) + " targets", COL_RED);
                        for (CardRef r : sel) applyHit(r, actionAmount);
                    }
                    ImGui::SameLine(0, 10 * S);
                    const int half = actionAmount / 2;
                    const std::string halfLabel = "Half damage (" + std::to_string(half) + ")";
                    if (dangerButton(halfLabel.c_str(), ImVec2(w2, 0), half > 0)) {
                        pushUndo();
                        addLog("Half damage (saved) " + std::to_string(half) + " to " + std::to_string(sel.size()) + " target(s)", COL_RED);
                        for (CardRef r : sel) applyHit(r, half);
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("For targets that made their save: half the amount, rounded down.");
                    }
                    if (greenButton("Heal all", ImVec2(w2, 0))) {
                        pushUndo();
                        for (CardRef r : sel) applyHeal(r, actionAmount);
                    }
                    ImGui::SameLine(0, 10 * S);
                    if (tempButton("Give temp HP", ImVec2(w2, 0), actionAmount > 0)) {
                        pushUndo();
                        for (CardRef r : sel) applyTemp(r, actionAmount);
                    }

                    ImGui::SetNextItemWidth((ImGui::GetContentRegionAvail().x - 10 * S) * 0.55f);
                    ImGui::Combo("##cond", &selCondition, STANDARD_CONDITIONS, STANDARD_CONDITION_COUNT);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2 * S);
                    ImGui::InputInt("##rounds", &selRounds, 1, 1);
                    selRounds = std::clamp(selRounds, 0, 999);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rounds (0 = until removed)");
                    if (primaryButton("Apply condition", ImVec2(w2, 0))) {
                        pushUndo();
                        for (CardRef r : sel) encounter.at(r.side, r.index)->addCondition(STANDARD_CONDITIONS[selCondition], selRounds);
                        addLog(std::string(STANDARD_CONDITIONS[selCondition]) + " applied to " + std::to_string(sel.size()) + " target(s)", COL_AMBER);
                    }
                    ImGui::SameLine(0, 10 * S);
                    if (quietButton("Remove it", ImVec2(w2, 0))) {
                        pushUndo();
                        for (CardRef r : sel) encounter.at(r.side, r.index)->removeCondition(STANDARD_CONDITIONS[selCondition]);
                    }
                    if (quietButton("Clear conds.", ImVec2(w2, 0))) {
                        pushUndo();
                        for (CardRef r : sel) encounter.at(r.side, r.index)->clearConditions();
                    }
                    ImGui::SameLine(0, 10 * S);
                    if (violetButton("Toggle conc.", ImVec2(w2, 0))) {
                        pushUndo();
                        for (CardRef r : sel) {
                            Creature* c = encounter.at(r.side, r.index);
                            c->setConcentrating(!c->isConcentrating());
                        }
                    }
                    if (quietButton("Duplicate", ImVec2(w2, 0))) duplicate(sel);
                    ImGui::SameLine(0, 10 * S);
                    if (dangerButton("Delete", ImVec2(w2, 0))) deleteRefs(sel, false);
                    if (quietButton("Deselect", ImVec2(rowButtonWidth(1), 0))) clearSelection();
                }
                endSection();

                // ---- Rounds.
                beginSection("Rounds");
                {
                    const float w2 = rowButtonWidth(2);
                    if (primaryButton("End round", ImVec2(w2, 0))) {
                        pushUndo();
                        const std::vector<std::string> ended = encounter.tickConditions();
                        board.round++;
                        addLog("--- Round " + std::to_string(board.round) + " ---", COL_MUTED);
                        for (const std::string& line : ended) addAlert(line, COL_AMBER);
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Counts down timed conditions by one round.");
                    ImGui::SameLine(0, 10 * S);
                    if (quietButton("Reset to 1", ImVec2(w2, 0))) {
                        pushUndo();
                        board.round = 1;
                    }
                }
                endSection();

                // ---- Board view.
                beginSection("Board");
                {
                    int percent = static_cast<int>(std::lround(g_cardScale * 100));
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80 * S);
                    if (ImGui::SliderInt("Card size", &percent, 60, 100, "%d%%")) {
                        g_cardScale = static_cast<float>(percent) / 100.0f;
                        saveSettings();
                    }
                    if (quietButton("Tidy board", ImVec2(rowButtonWidth(1), 0))) tidyBoard();
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Lines up every card (party first), shrinking them if needed to fit.");
                    }
                }
                endSection();

                // ---- Encounter.
                beginSection("Encounter");
                {
                    const float w2 = rowButtonWidth(2);
                    if (encounter.isEndedManually()) {
                        if (greenButton("Resume", ImVec2(w2, 0))) {
                            pushUndo();
                            encounter.resume();
                            addLog("Encounter resumed", COL_MUTED);
                        }
                    } else if (dangerButton("End encounter", ImVec2(w2, 0))) {
                        pushUndo();
                        encounter.endManually();
                        addLog("Encounter ended", COL_MUTED);
                    }
                    ImGui::SameLine(0, 10 * S);
                    if (primaryButton("New encounter", ImVec2(w2, 0))) openNewEncounter = true;

                    if (quietButton("Save", ImVec2(w2, 0))) {
                        statusMsg = writeFile(path, snapshot()) ? "Saved." : "Could not save.";
                    }
                    ImGui::SameLine(0, 10 * S);
                    if (quietButton("Load", ImVec2(w2, 0))) {
                        const std::string before = snapshot();
                        std::string text;
                        if (readFile(path, text) && deserializeBoard(text, encounter, board)) {
                            pushUndoState(before);
                            statusMsg = "Loaded.";
                        } else {
                            statusMsg = "Nothing to load.";
                        }
                    }
                    const std::string undoLabel = "Undo (" + std::to_string(undoStack.size()) + ")";
                    const std::string redoLabel = "Redo (" + std::to_string(redoStack.size()) + ")";
                    if (quietButton(undoLabel.c_str(), ImVec2(w2, 0), !undoStack.empty())) doUndo();
                    ImGui::SameLine(0, 10 * S);
                    if (quietButton(redoLabel.c_str(), ImVec2(w2, 0), !redoStack.empty())) doRedo();
                    ImGui::TextDisabled("%s", statusMsg.empty() ? "Autosaves while you play." : statusMsg.c_str());
                }
                endSection();
                tabBodyFx();
                ImGui::EndChild();
            }

            // ================= Add tab =================
            if (activeTab == 1) {
                ImGui::BeginChild("AddBody", ImVec2(0, 0), false);
                beginSection("New combatant");
                const float fieldW = 140 * S;
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 60 * S);
                ImGui::InputText("Name", newName, sizeof(newName));
                ImGui::RadioButton("Player", &newSide, 0);
                ImGui::SameLine();
                ImGui::RadioButton("Creature", &newSide, 1);
                ImGui::SetNextItemWidth(fieldW);
                ImGui::InputInt("Max HP", &newMaxHp);
                newMaxHp = std::max(1, newMaxHp);
                ImGui::SetNextItemWidth(fieldW);
                ImGui::InputInt("AC", &newAc);
                newAc = std::clamp(newAc, 0, 40);
                if (newSide == 0) {
                    ImGui::SetNextItemWidth(fieldW);
                    ImGui::InputInt("Level", &newLevel);
                    newLevel = std::clamp(newLevel, 1, 20);
                } else {
                    ImGui::SetNextItemWidth(fieldW);
                    crCombo("CR", &newCr);
                }
                ImGui::SetNextItemWidth(fieldW);
                ImGui::InputInt("How many", &newCount);
                newCount = std::clamp(newCount, 1, 20);
                const float w2 = rowButtonWidth(2);
                const bool canAdd = newName[0] != '\0';
                if (primaryButton("Add", ImVec2(w2, 0), canAdd)) {
                    pushUndo();
                    const Side side = newSide == 0 ? Side::Players : Side::Creatures;
                    clearSelection();
                    const std::vector<std::string> names = makeNumberedNames(encounter, newName, newCount);
                    for (size_t i = 0; i < names.size(); i++) {
                        Creature c(names[i], newMaxHp);
                        c.setAc(newAc);
                        if (side == Side::Players) c.setLevel(newLevel);
                        else c.setCrIndex(newCr);
                        const CardRef r = addCombatant(encounter, board, side, c, spawnPos());
                        board.get(r).selected = true;
                        board.get(r).spawnT = -0.06f * static_cast<float>(i);  // staggered fade-in
                    }
                    addLog("Added " + std::to_string(newCount) + " x " + newName, COL_MUTED);
                    newName[0] = '\0';
                }
                ImGui::SameLine(0, 10 * S);
                if (quietButton("Save to library", ImVec2(w2, 0), canAdd && newSide == 1)) {
                    MonsterTemplate t;
                    t.name = newName;
                    t.maxHp = newMaxHp;
                    t.ac = newAc;
                    t.cr = newCr;
                    library.push_back(t);
                    saveLibrary();
                    statusMsg = std::string("Saved ") + newName + " to the library.";
                }
                endSection();

                beginSection("Monster library", std::to_string(library.size()));
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 100 * S);
                ImGui::InputTextWithHint("##filter", "Search...", libraryFilter, sizeof(libraryFilter));
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2 * S);
                ImGui::InputInt("##libcount", &libraryCount, 1, 1);
                libraryCount = std::clamp(libraryCount, 1, 20);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("How many to add");

                std::string filter = libraryFilter;
                std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
                int removeIndex = -1;
                if (ImGui::BeginTable("library", 4, ImGuiTableFlags_BordersInnerH)) {
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Info", ImGuiTableColumnFlags_WidthFixed, 92 * S);
                    ImGui::TableSetupColumn("Add", ImGuiTableColumnFlags_WidthFixed, 26 * S);
                    ImGui::TableSetupColumn("Del", ImGuiTableColumnFlags_WidthFixed, 22 * S);
                    for (int i = 0; i < static_cast<int>(library.size()); i++) {
                        const MonsterTemplate& t = library[i];
                        std::string lower = t.name;
                        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                        if (!filter.empty() && lower.find(filter) == std::string::npos) continue;
                        ImGui::PushID(i);
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted(t.name.c_str());
                        ImGui::TableNextColumn();
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextDisabled("%dhp AC%d CR%s", t.maxHp, t.ac, crLabel(t.cr));
                        ImGui::TableNextColumn();
                        if (ImGui::SmallButton("+")) addFromTemplate(t, Side::Creatures, libraryCount);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add %d", libraryCount);
                        ImGui::TableNextColumn();
                        if (ImGui::SmallButton("x")) removeIndex = i;
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remove from library");
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
                if (removeIndex >= 0) {
                    library.erase(library.begin() + removeIndex);
                    saveLibrary();
                }
                ImGui::PushTextWrapPos(0);
                ImGui::TextDisabled("Right-click a card > Save to library to add your own monsters.");
                ImGui::PopTextWrapPos();
                if (quietButton("Restore default library", ImVec2(rowButtonWidth(1), 0))) {
                    library = defaultLibrary();
                    saveLibrary();
                }
                endSection();
                tabBodyFx();
                ImGui::EndChild();
            }

            // ================= Log tab =================
            if (activeTab == 2) {
                ImGui::BeginChild("LogTab", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
                beginSection("Combat log", std::to_string(log.size()));
                const float w2 = rowButtonWidth(2);
                if (quietButton("Copy log", ImVec2(w2, 0))) {
                    std::string all;
                    for (const LogEntry& e : log) all += "[R" + std::to_string(e.round) + "] " + e.text + "\n";
                    ImGui::SetClipboardText(all.c_str());
                    statusMsg = "Log copied to clipboard.";
                }
                ImGui::SameLine(0, 10 * S);
                if (quietButton("Clear log", ImVec2(w2, 0))) log.clear();
                // Fill the rest of the panel, leaving room for the box's bottom edge.
                const float logH = ImGui::GetContentRegionAvail().y - SECTION_PAD * S - 8 * S;
                ImGui::BeginChild("LogBody", ImVec2(0, std::max(60 * S, logH)), false);
                ImGui::PushTextWrapPos(0);
                for (const LogEntry& e : log) {
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    const ImVec2 p0 = ImGui::GetCursorScreenPos();
                    dl->AddCircleFilled(p0 + ImVec2(5 * S, ImGui::GetTextLineHeight() * 0.5f), 4 * S, e.color);
                    ImGui::Indent(14 * S);
                    ImGui::TextDisabled("R%d", e.round);
                    ImGui::SameLine();
                    ImGui::TextUnformatted(e.text.c_str());
                    ImGui::Unindent(14 * S);
                }
                ImGui::PopTextWrapPos();
                if (logScrollToBottom) {
                    ImGui::SetScrollHereY(1.0f);
                    logScrollToBottom = false;
                }
                tabBodyFx();  // the log list is its own window
                ImGui::EndChild();
                endSection();
                tabBodyFx();
                ImGui::EndChild();
            }
        }
        ImGui::EndChild();

        ImGui::SameLine(0, 22 * S);

        // ===================== Board =====================
        const ImVec2 boardChildSize(ImGui::GetContentRegionAvail().x - 10 * S, sidebarSize.y);
        {
            // Hard ink shadow behind the board panel.
            const ImVec2 bp = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(bp + ImVec2(4 * S, 5 * S), bp + boardChildSize + ImVec2(4 * S, 5 * S),
                                                      COL_INK, panelRound);
        }
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, panelRound);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, toVec4(COL_BOARD));
        ImGui::BeginChild("Board", boardChildSize, true,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        const ImVec2 boardOrigin = ImGui::GetCursorScreenPos();
        const ImVec2 boardSize = ImGui::GetContentRegionAvail();
        lastBoardSize = boardSize;
        drawBoardBackground(boardOrigin, boardSize);

        const ImVec2 cardSize = tokenSize();
        const float tabH = TAB_H * cardS();
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const bool shiftLike = io.KeyShift || io.KeyCtrl || io.KeySuper;
        auto cardRectContains = [&](CardRef r, ImVec2 pt) {
            const ImVec2 cp = board.get(r).pos;
            const float x0 = boardOrigin.x + cp.x, y0 = boardOrigin.y + cp.y - tabH;
            return pt.x >= x0 && pt.x <= x0 + cardSize.x && pt.y >= y0 && pt.y <= y0 + cardSize.y + tabH;
        };
        auto topCardAt = [&](ImVec2 pt) {
            for (int k = static_cast<int>(board.z.size()) - 1; k >= 0; k--) {
                if (cardRectContains(board.z[k], pt)) return k;
            }
            return -1;
        };
        const bool mouseOnBoard = !anyPopup && ImGui::IsWindowHovered() &&
                                  mouse.x >= boardOrigin.x && mouse.x <= boardOrigin.x + boardSize.x &&
                                  mouse.y >= boardOrigin.y && mouse.y <= boardOrigin.y + boardSize.y;

        // Trash drop zone (bottom-right). Releasing a dragged card over it deletes it.
        const float trashSize = 84 * S;
        const ImVec2 trashMin(boardOrigin.x + boardSize.x - trashSize - 24 * S,
                              boardOrigin.y + boardSize.y - trashSize - 24 * S);
        auto overTrash = [&](CardRef r) {
            const float m = 18 * S;
            const bool mouseIn = mouse.x >= trashMin.x - m && mouse.x <= trashMin.x + trashSize + m &&
                                 mouse.y >= trashMin.y - m && mouse.y <= trashMin.y + trashSize + m;
            const ImVec2 cp = board.get(r).pos;
            const ImVec2 center(boardOrigin.x + cp.x + cardSize.x * 0.5f, boardOrigin.y + cp.y + cardSize.y * 0.5f);
            const bool centerIn = center.x >= trashMin.x && center.x <= trashMin.x + trashSize &&
                                  center.y >= trashMin.y && center.y <= trashMin.y + trashSize;
            return mouseIn || centerIn;
        };

        // Press: remember the state (for undo), raise the card, or start a selection box.
        if (mouseOnBoard && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            pressSnapshot = snapshot();
            const int k = topCardAt(mouse);
            if (k >= 0) {
                const CardRef hit = board.z[k];
                board.z.erase(board.z.begin() + k);
                board.z.push_back(hit);
            } else {
                marqueeActive = true;
                marqueeStart = mouse;
            }
        }
        // Right-click: context menu for the card under the mouse.
        if (mouseOnBoard && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            const int k = topCardAt(mouse);
            if (k >= 0) {
                menuRef = board.z[k];
                if (!board.get(menuRef).selected) {
                    clearSelection();
                    board.get(menuRef).selected = true;
                }
                openMenu = true;
            }
        }

        // Dragging a selected card moves the whole selection with it.
        const bool dragIsGroup = board.dragValid && board.get(board.drag).selected;
        if (dragIsGroup && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            for (const CardRef& r : selectedRefs()) {
                if (!(r == board.drag)) board.get(r).pos = board.get(r).pos + io.MouseDelta;
            }
        }
        if (board.dragValid && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            pushUndoState(pressSnapshot);  // a move or a delete: undo goes back to before the press
            if (overTrash(board.drag)) {
                const std::vector<CardRef> victims = dragIsGroup ? selectedRefs() : std::vector<CardRef>{board.drag};
                deleteRefs(victims, true);
            }
        }

        // Card actions (each one undoable).
        const std::string amountLabel = std::to_string(actionAmount);
        CardActions actions;
        actions.damage = [&](CardRef r) {
            pushUndo();
            applyHit(r, actionAmount);
        };
        actions.heal = [&](CardRef r) {
            pushUndo();
            applyHeal(r, actionAmount);
        };
        actions.setDeathSaves = [&](CardRef r, int s, int f) {
            pushUndo();
            Creature* c = encounter.at(r.side, r.index);
            c->setDeathSaves(s, f);
            if (s >= 3) addLog(c->getName() + " is stable", COL_GREEN);
            if (f >= 3) addAlert(c->getName() + " has died", COL_RED);
        };

        // Stacking order: the card on top of the stack wins the mouse. Cards are
        // drawn back to front, but ImGui gives overlapping presses to whichever
        // item is submitted first, so a card is made non-interactive whenever a
        // card in front of it also covers the mouse position.
        for (size_t k = 0; k < board.z.size(); k++) {
            bool covered = false;
            for (size_t j = k + 1; j < board.z.size() && !covered; j++) {
                covered = cardRectContains(board.z[j], mouse);
            }
            const CardRef r = board.z[k];
            drawToken(encounter, r, board.get(r), boardOrigin, boardSize, amountLabel,
                      !covered && !marqueeActive, actions);
        }
        // Effects (floating numbers, crit bursts) always sit above every card.
        {
            CardFontScope cardFont;
            for (const CardRef& r : board.z) {
                drawCardEffects(board.get(r), cardSize, cardS());
            }
        }

        // Clicks select; double-clicks open the editor.
        for (const CardRef& r : board.z) {
            CardState& st = board.get(r);
            if (st.pressed && !shiftLike && !st.selected) {
                clearSelection();
                st.selected = true;
            }
            if (st.clicked) {
                if (shiftLike) {
                    st.selected = !st.selected;
                } else {
                    clearSelection();
                    st.selected = true;
                }
            }
            if (st.wantsEdit) {
                editRef = r;
                editCopy = *encounter.at(r.side, r.index);
                std::snprintf(editName, sizeof(editName), "%s", editCopy->getName().c_str());
                std::snprintf(editNotes, sizeof(editNotes), "%s", editCopy->getNotes().c_str());
                editCustomCond[0] = '\0';
                openEdit = true;
            }
        }

        // Selection box on the empty board.
        if (marqueeActive) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 a(std::min(marqueeStart.x, mouse.x), std::min(marqueeStart.y, mouse.y));
            const ImVec2 b(std::max(marqueeStart.x, mouse.x), std::max(marqueeStart.y, mouse.y));
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                dl->AddRectFilled(a, b, withAlpha(COL_SELECT, 38), 6 * S);
                dl->AddRect(a, b, COL_SELECT, 6 * S, 0, 1.5f * S);
            } else {
                const bool tiny = (b.x - a.x) < 4 && (b.y - a.y) < 4;
                if (!shiftLike) clearSelection();
                if (!tiny) {
                    for (const CardRef& r : board.z) {
                        const ImVec2 cp = boardOrigin + board.get(r).pos;
                        const bool overlaps = cp.x < b.x && cp.x + cardSize.x > a.x &&
                                              cp.y - tabH < b.y && cp.y + cardSize.y > a.y;
                        if (overlaps) board.get(r).selected = true;
                    }
                }
                marqueeActive = false;
            }
        }

        // Remember which card is being dragged (read next frame on mouse release).
        board.dragValid = false;
        for (const CardRef& r : board.z) {
            if (board.get(r).beingDragged) {
                board.drag = r;
                board.dragValid = true;
            }
        }
        const bool groupDragNow = board.dragValid && board.get(board.drag).selected;
        drawTrash(trashMin, trashSize, board.dragValid, board.dragValid && overTrash(board.drag),
                  board.dragValid ? (groupDragNow ? static_cast<int>(selectedRefs().size()) : 1) : 0,
                  static_cast<float>(ImGui::GetTime()), S);
        {
            const double now = ImGui::GetTime();
            alerts.erase(std::remove_if(alerts.begin(), alerts.end(), [&](const Alert& a) { return alertExpired(a, now); }),
                         alerts.end());
            drawToasts(alerts, boardOrigin, boardSize, now, std::min(io.DeltaTime, 0.05f), S);
        }
        ImGui::EndChild();

        // ===================== Popups =====================
        // Context menu (right-click a card).
        if (openMenu) {
            ImGui::OpenPopup("CardMenu");
            openMenu = false;
        }
        if (ImGui::BeginPopup("CardMenu")) {
            Creature* c = encounter.at(menuRef.side, menuRef.index);
            if (c == nullptr || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
                ImGui::CloseCurrentPopup();
            } else {
                const std::vector<CardRef> sel = selectedRefs();
                const bool many = sel.size() > 1;
                ImGui::TextDisabled("%s", many ? (std::to_string(sel.size()) + " selected").c_str() : c->getName().c_str());
                ImGui::Separator();
                if (!many && ImGui::MenuItem("Edit...")) {
                    editRef = menuRef;
                    editCopy = *c;
                    std::snprintf(editName, sizeof(editName), "%s", c->getName().c_str());
                    std::snprintf(editNotes, sizeof(editNotes), "%s", c->getNotes().c_str());
                    editCustomCond[0] = '\0';
                    openEdit = true;
                }
                if (ImGui::MenuItem("Duplicate", (std::string(MOD_KEY) + "+D").c_str())) duplicate(sel);
                if (ImGui::MenuItem("Concentrating", nullptr, c->isConcentrating())) {
                    pushUndo();
                    const bool value = !c->isConcentrating();
                    for (CardRef r : sel) encounter.at(r.side, r.index)->setConcentrating(value);
                }
                if (ImGui::BeginMenu("Damage type")) {
                    for (int m = 0; m < 4; m++) {
                        const DamageMod mod = static_cast<DamageMod>(m);
                        if (ImGui::MenuItem(damageModName(mod), nullptr, c->getDamageMod() == mod)) {
                            pushUndo();
                            for (CardRef r : sel) encounter.at(r.side, r.index)->setDamageMod(mod);
                        }
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Conditions")) {
                    for (int i = 0; i < STANDARD_CONDITION_COUNT; i++) {
                        const bool has = c->hasCondition(STANDARD_CONDITIONS[i]);
                        if (ImGui::MenuItem(STANDARD_CONDITIONS[i], nullptr, has)) {
                            pushUndo();
                            for (CardRef r : sel) {
                                Creature* t = encounter.at(r.side, r.index);
                                if (has) t->removeCondition(STANDARD_CONDITIONS[i]);
                                else t->addCondition(STANDARD_CONDITIONS[i], 0);
                            }
                        }
                    }
                    ImGui::EndMenu();
                }
                if (!many && ImGui::MenuItem("Save to library")) {
                    MonsterTemplate t;
                    int n = 0;
                    t.name = splitNumberedName(c->getName(), &n);
                    t.maxHp = c->getMaxHp();
                    t.ac = c->getAc();
                    t.speed = c->getSpeed();
                    t.pp = c->getPassivePerception();
                    t.cr = c->getCrIndex();
                    t.mod = c->getDamageMod();
                    library.push_back(t);
                    saveLibrary();
                    statusMsg = "Saved " + t.name + " to the library.";
                }
                ImGui::Separator();
                if (ImGui::MenuItem(many ? "Delete selected" : "Delete", "Del")) deleteRefs(sel, false);
            }
            ImGui::EndPopup();
        }

        // Edit popup (double-click a card).
        if (openEdit) {
            ImGui::OpenPopup("Edit combatant");
            openEdit = false;
        }
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(std::min(560 * S, display.x - 40 * S), std::min(700 * S, display.y - 40 * S)),
                                 ImGuiCond_Appearing);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16 * S);
        const bool editOpen = ImGui::BeginPopupModal("Edit combatant", nullptr, ImGuiWindowFlags_NoMove);
        ImGui::PopStyleVar(2);
        if (editOpen) {
            if (!editCopy || encounter.at(editRef.side, editRef.index) == nullptr ||
                ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
                ImGui::CloseCurrentPopup();
            } else {
                Creature& e = *editCopy;
                const bool isPlayer = editRef.side == Side::Players;
                // Fields scroll; the Apply/Cancel footer below always stays visible.
                const float footerH = ImGui::GetFrameHeight() + 22 * S;
                ImGui::BeginChild("EditBody", ImVec2(0, -footerH), false);
                const float half = (ImGui::GetContentRegionAvail().x - 10 * S) * 0.5f;
                const float fieldW = half - 120 * S;
                auto intField = [&](const char* label, int value, int lo, int hi, const std::function<void(int)>& set) {
                    ImGui::SetNextItemWidth(fieldW);
                    int v = value;
                    if (ImGui::InputInt(label, &v)) set(std::clamp(v, lo, hi));
                };

                ImGui::TextDisabled("%s", isPlayer ? "Player" : "Enemy");
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 70 * S);
                ImGui::InputText("Name", editName, sizeof(editName));

                ImGui::SeparatorText("Hit points");
                intField("Max HP", e.getMaxHp(), 1, 99999, [&](int v) { e.setMaxHp(v); });
                ImGui::SameLine(half + 10 * S);
                intField("Current HP", e.getCurrentHp(), 0, e.getMaxHp(), [&](int v) { e.setHp(v); });
                intField("Temp HP", e.getTempHp(), 0, 99999, [&](int v) { e.setTempHp(v); });
                ImGui::SameLine(half + 10 * S);
                {
                    DamageMod mod = e.getDamageMod();
                    ImGui::SetNextItemWidth(fieldW);
                    if (damageModCombo("Damage", &mod)) e.setDamageMod(mod);
                }

                ImGui::SeparatorText("Quick reference");
                intField("AC", e.getAc(), 0, 40, [&](int v) { e.setAc(v); });
                ImGui::SameLine(half + 10 * S);
                intField("Speed", e.getSpeed(), 0, 500, [&](int v) { e.setSpeed(v); });
                intField("Passive Perc.", e.getPassivePerception(), 0, 40, [&](int v) { e.setPassivePerception(v); });
                ImGui::SameLine(half + 10 * S);
                if (isPlayer) {
                    intField("Level", e.getLevel(), 1, 20, [&](int v) { e.setLevel(v); });
                } else {
                    int cr = e.getCrIndex();
                    ImGui::SetNextItemWidth(fieldW);
                    if (crCombo("CR", &cr)) e.setCrIndex(cr);
                }

                ImGui::SeparatorText("Status");
                {
                    bool conc = e.isConcentrating();
                    if (ImGui::Checkbox("Concentrating on a spell", &conc)) e.setConcentrating(conc);
                }
                if (isPlayer) {
                    const int s = e.getDeathSuccesses(), f = e.getDeathFailures();
                    intField("Death successes", s, 0, 3, [&](int v) { e.setDeathSaves(v, f); });
                    ImGui::SameLine(half + 10 * S);
                    intField("Death failures", f, 0, 3, [&](int v) { e.setDeathSaves(s, v); });
                }

                ImGui::SeparatorText("Conditions");
                if (ImGui::BeginTable("conds", 3)) {
                    for (int i = 0; i < STANDARD_CONDITION_COUNT; i++) {
                        ImGui::TableNextColumn();
                        bool has = e.hasCondition(STANDARD_CONDITIONS[i]);
                        if (ImGui::Checkbox(STANDARD_CONDITIONS[i], &has)) {
                            if (has) e.addCondition(STANDARD_CONDITIONS[i], 0);
                            else e.removeCondition(STANDARD_CONDITIONS[i]);
                        }
                    }
                    ImGui::EndTable();
                }
                ImGui::SetNextItemWidth(half);
                ImGui::InputTextWithHint("##custom", "Custom condition (e.g. Hexed)", editCustomCond, sizeof(editCustomCond));
                ImGui::SameLine();
                if (ImGui::Button("Add condition") && editCustomCond[0] != '\0') {
                    e.addCondition(editCustomCond, 0);
                    editCustomCond[0] = '\0';
                }
                if (!e.getConditions().empty()) {
                    ImGui::TextDisabled("Duration in rounds (0 = until removed):");
                    std::vector<Condition> conds = e.getConditions();
                    for (const Condition& cond : conds) {
                        ImGui::PushID(cond.name.c_str());
                        ImGui::SetNextItemWidth(fieldW);
                        int rounds = cond.rounds;
                        if (ImGui::InputInt(cond.name.c_str(), &rounds)) e.addCondition(cond.name, std::clamp(rounds, 0, 999));
                        ImGui::SameLine();
                        if (ImGui::SmallButton("remove")) e.removeCondition(cond.name);
                        ImGui::PopID();
                    }
                }

                ImGui::SeparatorText("Notes");
                ImGui::InputTextMultiline("##notes", editNotes, sizeof(editNotes),
                                          ImVec2(ImGui::GetContentRegionAvail().x - 4 * S, 80 * S));
                ImGui::EndChild();

                ImGui::Separator();
                const float bw = 110 * S;
                if (greenButton("Apply", ImVec2(bw, 0))) {
                    pushUndo();
                    if (editName[0] != '\0') e.rename(editName);
                    e.setNotes(editNotes);
                    *encounter.at(editRef.side, editRef.index) = e;
                    addLog("Edited " + e.getName(), COL_MUTED);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine(0, 14 * S);
                if (quietButton("Cancel", ImVec2(bw, 0))) ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // New encounter (keeps players by default: HP carries over between fights).
        if (openNewEncounter) {
            ImGui::OpenPopup("New encounter");
            openNewEncounter = false;
        }
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16 * S);
        const bool newOpen = ImGui::BeginPopupModal("New encounter", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
        ImGui::PopStyleVar(2);
        if (newOpen) {
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) ImGui::CloseCurrentPopup();
            ImGui::TextUnformatted("Start a new encounter? (Undo can bring the old one back.)");
            ImGui::Dummy(ImVec2(0, 4 * S));
            const float bw = 200 * S;
            if (primaryButton("Remove enemies, keep party", ImVec2(bw * 1.3f, 0))) {
                pushUndo();
                std::vector<CardRef> enemies;
                for (int i = 0; i < static_cast<int>(board.creatures.size()); i++) enemies.push_back({Side::Creatures, i});
                removeCombatants(encounter, board, enemies);
                encounter.resume();
                board.round = 1;
                alerts.clear();
                addLog("=== New encounter ===", COL_MUTED);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine(0, 12 * S);
            if (dangerButton("Remove everything", ImVec2(bw, 0))) {
                pushUndo();
                encounter.clear();
                board = BoardState();
                alerts.clear();
                addLog("=== New encounter (empty) ===", COL_MUTED);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine(0, 12 * S);
            if (quietButton("Cancel", ImVec2(bw * 0.6f, 0))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::End();

        if (ImGui::GetTime() - lastAutosave > 3.0) {
            writeFile(path, snapshot());
            lastAutosave = ImGui::GetTime();
        }

        ImGui::Render();
        // On Retina displays ImGui works in points but the renderer in pixels;
        // without this the drawn UI and the mouse positions don't line up.
        const ImGuiIO& frameIo = ImGui::GetIO();
        SDL_RenderSetScale(renderer, frameIo.DisplayFramebufferScale.x, frameIo.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(renderer, 236, 236, 232, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    writeFile(path, snapshot());

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
