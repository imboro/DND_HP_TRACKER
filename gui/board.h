#pragma once
// The board: what the UI keeps for each combatant next to the rules (Encounter).
// Encounter knows HP and conditions; BoardState knows the turn order, whose turn it
// is, which cards are selected and where each card is drawn and animated.
#include <string>
#include <vector>

#include "imgui.h"
#include "creature.h"
#include "encounter.h"

// A reference to one combatant: which group and which index in it.
struct CardRef {
    Side side;
    int index;
};

inline bool operator==(CardRef a, CardRef b) { return a.side == b.side && a.index == b.index; }

// What a floating label / flash represents.
enum class FxKind { Damage, Heal, Temp, Immune };

// Card animation timings (seconds).
inline constexpr float DEATH_LEN = 1.4f;   // death: thud, cross, strike-through, floating label
inline constexpr float SPAWN_TIME = 0.32f; // fade-in + drop when a card is added

// Per-card UI state, parallel to the creature in Encounter.
struct CardState {
    ImVec2 pos = ImVec2(0, 0);      // top-left inside the board (animated toward `target`)
    ImVec2 drawPos = ImVec2(0, 0);  // on-screen top-left last frame (after shake)

    // Column layout (set by the board layout every frame).
    ImVec2 target = ImVec2(0, 0);   // where the card wants to be (board coordinates)
    ImVec2 size = ImVec2(0, 0);     // card size in pixels
    bool placed = false;            // false until the first layout (then snaps, no glide)
    int column = 0;                 // 0 = Turn order, 1 = Defeated
    ImVec2 clipMin = ImVec2(0, 0);  // screen rect of the column's card area (hit testing)
    ImVec2 clipMax = ImVec2(0, 0);
    float turnAnim = 0.0f;          // 0..1: how much this card holds the turn marker

    // Screen positions of its buttons last frame (hit testing and UI tests).
    ImVec2 damageCenter = ImVec2(0, 0);
    ImVec2 healCenter = ImVec2(0, 0);
    ImVec2 turnCenter = ImVec2(0, 0);
    ImVec2 pipCenter[6];
    bool hasPips = false;

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
    std::vector<CardRef> z;  // turn order, first to last
    CardRef drag{Side::Players, 0};  // card dragged last frame, if dragValid
    bool dragValid = false;
    int round = 1;

    // Setup (false: adding and ordering combatants) or combat (true: turns run).
    bool inCombat = false;
    // Whose turn it is (a member of z), if any. Only used in combat.
    CardRef turn{Side::Players, 0};
    bool turnValid = false;
    // The card shown big in the spotlight panel: follows the turn, or the last clicked card.
    CardRef focus{Side::Players, 0};
    bool focusValid = false;
    // Vertical scroll of the two columns (0 = Turn order, 1 = Defeated).
    float scroll[2] = {0.0f, 0.0f};
    float scrollTarget[2] = {0.0f, 0.0f};

    std::vector<CardState>& of(Side side) {
        return side == Side::Players ? players : creatures;
    }
    const std::vector<CardState>& of(Side side) const {
        return side == Side::Players ? players : creatures;
    }
    CardState& get(CardRef r) { return of(r.side)[r.index]; }
    const CardState& get(CardRef r) const { return of(r.side)[r.index]; }

    // Selection.
    std::vector<CardRef> selectedRefs() const;  // in turn order
    void clearSelection();
    void selectAll();

    // Turn order.
    int indexInOrder(CardRef r) const;  // position in z, -1 if absent
    // Moves a card to slot `slot` of the Turn order (0 = first), among the cards that
    // are not defeated.
    void placeInTurnOrder(CardRef r, int slot);
};

// ---- Life state ------------------------------------------------------------

enum class Life { Up, Dying, Stable, Dead };

// Players at 0 HP are dying until 3 successes (stable) or 3 failures (dead).
Life lifeOf(Side side, const Creature& c);
const char* damageModName(DamageMod m);

// ---- Adding and removing combatants (keeps Encounter and BoardState in step) -----

// Adds a combatant at the end of the turn order.
CardRef addCombatant(Encounter& encounter, BoardState& board, Side side, Creature c);
void removeCombatant(Encounter& encounter, BoardState& board, CardRef ref);
// Removes several at once (highest index first, so the others stay valid).
void removeCombatants(Encounter& encounter, BoardState& board, std::vector<CardRef> refs);

// "Goblin 3" -> ("Goblin", 3); "Goblin" -> ("Goblin", 0).
std::string splitNumberedName(const std::string& name, int* number);
// Names for `count` new combatants called `requested`. Numbers them when there
// is more than one (or one already exists), renaming a plain "Goblin" to "Goblin 1".
std::vector<std::string> makeNumberedNames(Encounter& encounter, const std::string& requested, int count);
