#pragma once
// The app's look and its building blocks: palette, fonts, buttons, section boxes,
// tabs, line icons and small animation helpers. Nothing in here knows about
// creatures or encounters; it only draws.
#include <algorithm>
#include <cmath>
#include <string>

#include "imgui.h"
#include "creature.h"  // DamageMod, for damageModCombo

// Scales fonts, spacing and hard-coded sizes so the UI isn't tiny.
inline constexpr float UI_SCALE = 1.25f;

// ---- Palette: clean light, ink outlines ------------------------------------
// Mostly black and white: ink outlines with hard offset shadows on white
// surfaces. Color is kept for meaning (party, damage, healing...).
// Surfaces, from the back of the screen to the front.
inline constexpr ImU32 COL_BG         = IM_COL32(236, 236, 232, 255);  // window behind the panels
inline constexpr ImU32 COL_PANEL      = IM_COL32(255, 255, 255, 255);  // sidebar, popups, boxes
inline constexpr ImU32 COL_BOARD      = IM_COL32(247, 247, 244, 255);  // the board
inline constexpr ImU32 COL_CARD       = IM_COL32(255, 255, 255, 255);  // creature cards
inline constexpr ImU32 COL_CARD_HI    = IM_COL32(251, 251, 249, 255);  // hovered card
inline constexpr ImU32 COL_RAISED     = IM_COL32(255, 255, 255, 255);  // neutral buttons, chips
inline constexpr ImU32 COL_RAISED_HI  = IM_COL32(244, 244, 241, 255);
inline constexpr ImU32 COL_RAISED_LO  = IM_COL32(232, 232, 228, 255);
inline constexpr ImU32 COL_DISABLED   = IM_COL32(242, 242, 239, 255);
inline constexpr ImU32 COL_TRACK      = IM_COL32(236, 236, 232, 255);  // bar tracks
inline constexpr ImU32 COL_BORDER     = IM_COL32(228, 228, 223, 255);  // hairlines, separators
inline constexpr ImU32 COL_BORDER_HI  = IM_COL32(190, 190, 184, 255);
inline constexpr ImU32 COL_INK        = IM_COL32(26, 26, 28, 255);     // outlines, hard shadows
// Text.
inline constexpr ImU32 COL_TEXT       = IM_COL32(26, 26, 28, 255);
inline constexpr ImU32 COL_MUTED      = IM_COL32(132, 132, 128, 255);
inline constexpr ImU32 COL_SHADOW     = COL_INK;
inline constexpr ImU32 COL_WHITE      = IM_COL32(255, 255, 255, 255);
// Meaning colors: kept consistent across the whole UI.
inline constexpr ImU32 COL_PLAYER     = IM_COL32(59, 130, 246, 255);   // party
inline constexpr ImU32 COL_RED        = IM_COL32(226, 72, 60, 255);    // enemies, damage, low HP
inline constexpr ImU32 COL_GREEN      = IM_COL32(46, 164, 79, 255);    // healing, healthy
inline constexpr ImU32 COL_AMBER      = IM_COL32(230, 150, 20, 255);   // half HP, warnings
inline constexpr ImU32 COL_GRAY       = IM_COL32(150, 150, 146, 255);  // dead, neutral
inline constexpr ImU32 COL_TEMP       = IM_COL32(14, 160, 225, 255);   // temporary HP
inline constexpr ImU32 COL_VIOLET     = IM_COL32(139, 92, 246, 255);   // concentration
inline constexpr ImU32 COL_SELECT     = IM_COL32(250, 184, 0, 255);    // selection

// Button looks: fill (normal, hover, pressed) and label color.
struct ButtonStyle {
    ImU32 fill, hover, active, text;
};
inline constexpr ButtonStyle BTN_INK    = {COL_INK, IM_COL32(64, 64, 66, 255), IM_COL32(0, 0, 0, 255), COL_WHITE};
inline constexpr ButtonStyle BTN_RED    = {IM_COL32(255, 236, 232, 255), IM_COL32(255, 222, 216, 255), IM_COL32(250, 205, 198, 255), IM_COL32(176, 42, 32, 255)};
inline constexpr ButtonStyle BTN_GREEN  = {IM_COL32(230, 246, 234, 255), IM_COL32(214, 239, 221, 255), IM_COL32(198, 231, 207, 255), IM_COL32(26, 112, 50, 255)};
inline constexpr ButtonStyle BTN_TEMP   = {IM_COL32(227, 243, 252, 255), IM_COL32(210, 235, 249, 255), IM_COL32(193, 226, 245, 255), IM_COL32(8, 102, 148, 255)};
inline constexpr ButtonStyle BTN_VIOLET = {IM_COL32(241, 236, 255, 255), IM_COL32(229, 221, 254, 255), IM_COL32(216, 205, 251, 255), IM_COL32(94, 58, 190, 255)};
inline constexpr ButtonStyle BTN_QUIET  = {COL_RAISED, COL_RAISED_HI, COL_RAISED_LO, COL_TEXT};

// Green above half HP, amber above a quarter, red below.
ImU32 hpColor(float fraction);
ImVec4 toVec4(ImU32 c);

// ---- Theme and fonts ---------------------------------------------------------

// Rounding, borders, spacing and ImGui's colors for the palette above.
void applyTheme();
// Regular, bold and heavy faces: Helvetica + Arial Bold / Arial Black on macOS,
// the Arial family on Windows. Returns false (ImGui's built-in font) if none load.
bool loadSystemFonts(float pixelSize);
// Bold and heavy faces for headings and numbers (null: the regular font is used).
extern ImFont* g_fontBold;
extern ImFont* g_fontHeavy;

// ---- Small math and color helpers --------------------------------------------

inline float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float easeOutCubic(float t) { float u = 1.0f - t; return 1.0f - u * u * u; }
inline float easeInOutCubic(float t) {
    return t < 0.5f ? 4 * t * t * t : 1.0f - std::pow(-2 * t + 2, 3.0f) / 2.0f;
}

// Moves `value` toward `target`, framerate-independent (higher rate = snappier).
inline float approach(float value, float target, float rate, float dt) {
    return value + (target - value) * (1.0f - std::exp(-rate * dt));
}

inline ImU32 withAlpha(ImU32 c, int alpha) {
    return (c & 0x00FFFFFF) | (static_cast<ImU32>(std::clamp(alpha, 0, 255)) << 24);
}

// Scales a color's existing alpha by k (0..1).
inline ImU32 fadeAlpha(ImU32 c, float k) {
    const int a = static_cast<int>((c >> IM_COL32_A_SHIFT) & 0xFF);
    return withAlpha(c, static_cast<int>(static_cast<float>(a) * std::clamp(k, 0.0f, 1.0f)));
}

// Blends two colors (t = 0 gives a, t = 1 gives b); keeps a's alpha.
inline ImU32 mixColor(ImU32 a, ImU32 b, float t) {
    auto ch = [&](int shift) {
        const float x = static_cast<float>((a >> shift) & 0xFF), y = static_cast<float>((b >> shift) & 0xFF);
        return static_cast<ImU32>(std::lround(x + (y - x) * t)) << shift;
    };
    return ch(IM_COL32_R_SHIFT) | ch(IM_COL32_G_SHIFT) | ch(IM_COL32_B_SHIFT) | (a & IM_COL32_A_MASK);
}

// ---- Text ----------------------------------------------------------------------
// Text in a given face, at a size relative to the current font size. Positions and
// sizes refer to the glyphs at the regular font's scale.

ImFont* boldFont();
ImFont* heavyFont();
void fontText(ImDrawList* dl, ImFont* font, ImVec2 pos, ImU32 col, const char* text, float sizeMul);
ImVec2 fontTextSize(ImFont* font, const char* text, float sizeMul);
void boldText(ImDrawList* dl, ImVec2 pos, ImU32 col, const char* text, float sizeMul = 1.0f);
ImVec2 boldTextSize(const char* text, float sizeMul = 1.0f);
void heavyText(ImDrawList* dl, ImVec2 pos, ImU32 col, const char* text, float sizeMul = 1.0f);
ImVec2 heavyTextSize(const char* text, float sizeMul = 1.0f);
// Regular face at a size relative to the current font size.
void sizedText(ImDrawList* dl, ImVec2 pos, ImU32 col, const char* text, float sizeMul);
ImVec2 sizedTextSize(const char* text, float sizeMul);

// ---- Boxes and buttons -----------------------------------------------------------

// The house style: a rounded box with an ink outline and a hard shadow
// offset down and to the right. `alpha` fades the whole thing (0..1).
void inkBox(ImDrawList* dl, ImVec2 a, ImVec2 b, float round, float S, ImU32 fill = COL_PANEL,
            float shadow = 3.0f, ImU32 outline = COL_INK, float alpha = 1.0f, float border = 1.75f);

// A rounded button with an ink outline and a hard shadow that disappears while it
// is held down. With interactive == false it is only drawn (no ImGui item is
// submitted), so it cannot claim the hover from a card stacked in front of it.
bool flatButton(const char* label, ImVec2 size, const ButtonStyle& look, bool enabled = true,
                bool interactive = true);
// Shorthands for the common button roles.
bool primaryButton(const char* label, ImVec2 size = ImVec2(0, 0), bool enabled = true);
bool dangerButton(const char* label, ImVec2 size = ImVec2(0, 0), bool enabled = true);
bool greenButton(const char* label, ImVec2 size = ImVec2(0, 0), bool enabled = true);
bool tempButton(const char* label, ImVec2 size = ImVec2(0, 0), bool enabled = true);
bool quietButton(const char* label, ImVec2 size = ImVec2(0, 0), bool enabled = true);
bool violetButton(const char* label, ImVec2 size = ImVec2(0, 0), bool enabled = true);

// Width of an ImGui::SmallButton with a one-character label ("+", "x"), border included.
float smallButtonWidth();
// Width for `count` buttons sharing the current row.
float rowButtonWidth(int count);

// ---- Sections, pills and tabs ------------------------------------------------------

// Padding inside a section box (before UI_SCALE).
inline constexpr float SECTION_PAD = 13.0f;
// A white box with an ink outline whose heavy title sits in a gap in its top edge.
// Everything drawn between beginSection and endSection goes inside it.
void beginSection(const char* title, const std::string& note = std::string());
void endSection();

// A labelled pill: white with an ink outline and a colored dot.
void pill(const char* text, ImU32 color, float S);
// Pill-shaped segmented control (the sidebar tabs). Sets *active when one is clicked.
void segmentedTabs(const char* const* labels, int count, int* active, float S);

// ---- Line icons ------------------------------------------------------------------------
// Each one is drawn in the box [o, o + s].

void drawHeart(ImDrawList* dl, ImVec2 o, float s, float thick, ImU32 col);
void drawBolt(ImDrawList* dl, ImVec2 o, float s, float thick, ImU32 col);
void drawCross(ImDrawList* dl, ImVec2 o, float s, float thick, ImU32 col);
// The cross drawn stroke by stroke: each stroke grows from 0 to 1.
void drawCrossPartial(ImDrawList* dl, ImVec2 o, float s, float thick, ImU32 col, float t1, float t2);

// Fades in (k: 0 -> 1) and shifts every vertex added to `dl` since index `start`:
// a cheap way to animate a whole group of shapes and widgets at once.
void fadeVertices(ImDrawList* dl, int start, float k, ImU32 background, ImVec2 shift = ImVec2(0, 0));

enum class TileIcon { Cycle, Bolt, Heart };
// Summary tile: line icon top-left, big heavy number top-right, label below.
// Returns true while hovered (for a tooltip).
bool statTile(const char* id, const char* label, const std::string& value, TileIcon icon, ImU32 iconCol,
              float w, float S);

// ---- Small widgets ------------------------------------------------------------------------

// Challenge rating picker (-1 = not set). Returns true when it changed.
bool crCombo(const char* label, int* index);
bool damageModCombo(const char* label, DamageMod* mod);
