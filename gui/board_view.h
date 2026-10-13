#pragma once
// Drawing the board's pieces: the compact card rows, the spotlight panel, the
// floating damage numbers. These functions draw and report what
// the user clicked; the app decides what happens (undo, log, rules).
#include <functional>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"
#include "board.h"
#include "encounter.h"

// Extra scale for cards on the board (the "Card size" setting), 0.6 - 1.0.
extern float g_cardScale;
float cardS();         // UI_SCALE * g_cardScale
float cardHeight();    // height of a compact card row

// Draws card text at the card scale while alive.
struct CardFontScope {
    // FontSizeBase is the unscaled size currently in use (our font is loaded at a custom size).
    CardFontScope() { ImGui::PushFont(nullptr, ImGui::GetCurrentContext()->FontSizeBase * g_cardScale); }
    ~CardFontScope() { ImGui::PopFont(); }
};

// What a card can ask the app to do (the app owns undo, logging and alerts).
struct CardActions {
    std::function<void(CardRef)> damage;
    std::function<void(CardRef)> heal;
    std::function<void(CardRef, int, int)> setDeathSaves;
    std::function<void()> nextTurn;
    std::function<void(CardRef)> temp;   // spotlight: grant temp HP
    std::function<void(CardRef)> edit;   // spotlight: open the edit window
};

// Draws one compact row card (it lives in a column of the board) and handles
// its buttons. `interactive` is false for cards hidden under another card at
// the mouse position, so only the front card receives the press. `isTurn`
// marks the card that holds the turn (it steps forward and wears a "TURN"
// ribbon that advances the turn when clicked). Details live in the spotlight.
void drawToken(const Encounter& encounter, CardRef ref, CardState& st, ImVec2 boardOrigin,
               const std::string& amountLabel, bool interactive, bool isTurn, bool isFocus,
               const CardActions& act);

// The spotlight panel: one combatant shown big (the one whose turn it is, or
// the last one clicked), with everything the compact row has no room for.
void drawSpotlight(const Encounter& encounter, CardRef ref, CardState& st, ImVec2 a, ImVec2 b,
                   const std::string& amountLabel, bool holdsTurn, const CardActions& act);

// The spotlight panel's box, drawn behind the cards: white, or the enemy look (dark,
// tinted, cut corners) when `ref` is a living enemy. `ref` may be null (setup panel).
void drawSpotlightBox(ImVec2 a, ImVec2 b, const Encounter& encounter, const CardRef* ref, float S);

// Floating label and crit starburst; drawn in a top pass above all cards.
void drawCardEffects(const CardState& st, ImVec2 size, float S);
