#include "board.h"

#include <algorithm>

// ---- BoardState ------------------------------------------------------------

std::vector<CardRef> BoardState::selectedRefs() const {
    std::vector<CardRef> refs;
    for (const CardRef& r : z) {
        if (get(r).selected) refs.push_back(r);
    }
    return refs;
}

void BoardState::clearSelection() {
    for (CardState& s : players) s.selected = false;
    for (CardState& s : creatures) s.selected = false;
}

void BoardState::selectAll() {
    for (CardState& s : players) s.selected = true;
    for (CardState& s : creatures) s.selected = true;
}

int BoardState::indexInOrder(CardRef r) const {
    for (size_t i = 0; i < z.size(); i++) {
        if (z[i] == r) return static_cast<int>(i);
    }
    return -1;
}

void BoardState::placeInTurnOrder(CardRef r, int slot) {
    std::vector<CardRef> others;
    for (const CardRef& o : z) {
        if (!(o == r) && get(o).column == 0) others.push_back(o);
    }
    slot = std::clamp(slot, 0, static_cast<int>(others.size()));
    z.erase(std::remove(z.begin(), z.end(), r), z.end());
    if (others.empty()) {
        z.push_back(r);
    } else if (slot < static_cast<int>(others.size())) {
        const auto at = std::find(z.begin(), z.end(), others[slot]);
        z.insert(at, r);
    } else {
        const auto at = std::find(z.begin(), z.end(), others.back());
        z.insert(at + 1, r);
    }
}

// ---- Life state ------------------------------------------------------------

// Players at 0 HP are dying until 3 successes (stable) or 3 failures (dead).
Life lifeOf(Side side, const Creature& c) {
    if (c.getCurrentHp() > 0) return Life::Up;
    if (side == Side::Creatures) return Life::Dead;
    if (c.getDeathFailures() >= 3) return Life::Dead;
    if (c.getDeathSuccesses() >= 3) return Life::Stable;
    return Life::Dying;
}

const char* damageModName(DamageMod m) {
    switch (m) {
        case DamageMod::Normal:     return "Normal";
        case DamageMod::Resistant:  return "Resistant";
        case DamageMod::Vulnerable: return "Vulnerable";
        case DamageMod::Immune:     return "Immune";
    }
    return "";
}

CardRef addCombatant(Encounter& encounter, BoardState& board, Side side, Creature c) {
    CardState st;
    st.lastHp = c.getCurrentHp();
    st.wasDead = lifeOf(side, c) == Life::Dead;  // already dead (load/undo): no death animation
    std::vector<CardState>& list = board.of(side);
    list.push_back(st);
    const CardRef ref{side, static_cast<int>(list.size()) - 1};
    board.z.push_back(ref);  // new cards go last in the turn order
    if (side == Side::Players) {
        encounter.addPlayer(std::move(c));
    } else {
        encounter.addCreature(std::move(c));
    }
    return ref;
}

// Removes a combatant and keeps the UI state and stacking order in sync.
void removeCombatant(Encounter& encounter, BoardState& board, CardRef ref) {
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
    if (board.turnValid && board.turn.side == ref.side) {
        if (board.turn.index == ref.index) board.turnValid = false;  // re-picked by the layout
        else if (board.turn.index > ref.index) board.turn.index--;
    }
    if (board.focusValid && board.focus.side == ref.side) {
        if (board.focus.index == ref.index) board.focusValid = false;
        else if (board.focus.index > ref.index) board.focus.index--;
    }
}

// Removes several at once (highest index first, so the others stay valid).
void removeCombatants(Encounter& encounter, BoardState& board, std::vector<CardRef> refs) {
    std::sort(refs.begin(), refs.end(), [](CardRef a, CardRef b) {
        if (a.side != b.side) return a.side < b.side;
        return a.index > b.index;
    });
    for (CardRef r : refs) removeCombatant(encounter, board, r);
}

// "Goblin 3" -> ("Goblin", 3); "Goblin" -> ("Goblin", 0).
std::string splitNumberedName(const std::string& name, int* number) {
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
std::vector<std::string> makeNumberedNames(Encounter& encounter, const std::string& requested, int count) {
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
