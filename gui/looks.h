#pragma once
// Card looks: the color and icon a combatant wears on its card, in the spotlight
// and on the players' phones. A creature stores them as indexes into the tables
// below (-1 = the default for its side).
#include "imgui.h"
#include "creature.h"
#include "encounter.h"  // Side

struct PaletteColor {
    const char* name;
    ImU32 col;
};
// Index 0 is the players' default and 1 the enemies' default.
inline constexpr PaletteColor PALETTE[] = {
    {"Blue", IM_COL32(59, 130, 246, 255)},   {"Red", IM_COL32(226, 72, 60, 255)},
    {"Green", IM_COL32(46, 164, 79, 255)},   {"Amber", IM_COL32(222, 150, 20, 255)},
    {"Purple", IM_COL32(139, 92, 246, 255)}, {"Pink", IM_COL32(219, 39, 119, 255)},
    {"Teal", IM_COL32(13, 148, 136, 255)},   {"Orange", IM_COL32(234, 108, 20, 255)},
    {"Brown", IM_COL32(146, 95, 50, 255)},   {"Slate", IM_COL32(71, 85, 105, 255)},
};
inline constexpr int PALETTE_COUNT = sizeof(PALETTE) / sizeof(PALETTE[0]);

// Index 0 is the players' default and 1 the enemies' default.
inline constexpr const char* ICON_NAMES[] = {"Heart", "Bolt", "Shield", "Sword", "Skull", "Star",
                                             "Flame", "Paw",  "D20",    "Crown", "Moon",  "Eye"};
inline constexpr int ICON_COUNT = sizeof(ICON_NAMES) / sizeof(ICON_NAMES[0]);

// The palette / icon index actually shown (the side's default for -1 or out of range).
int colorIndexFor(Side side, int color);
int iconIndexFor(Side side, int icon);
ImU32 creatureColor(Side side, const Creature& c);

// Draws icon `icon` (an index into ICON_NAMES) as a line glyph in the box [o, o + s].
void drawIcon(ImDrawList* dl, int icon, ImVec2 o, float s, float thick, ImU32 col);
void drawCreatureIcon(ImDrawList* dl, Side side, const Creature& c, ImVec2 o, float s, float thick, ImU32 col);

// Color swatches and icon tiles for picking a card's look (-1 = the default for its
// side). Returns true when something changed.
bool lookPicker(int* color, int* icon, Side side, float S);
