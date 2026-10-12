#include "encounter.h"
#include <algorithm>
#include <iostream>

std::vector<Creature>& Encounter::group(Side side) {
    return side == Side::Players ? players : creatures;
}

const std::vector<Creature>& Encounter::group(Side side) const {
    return side == Side::Players ? players : creatures;
}

// An empty group is not "all dead": an encounter still being set up isn't over.
bool Encounter::allDead(const std::vector<Creature>& g) {
    if (g.empty()) {
        return false;
    }
    for (const Creature& c : g) {
        if (!c.isDead()) {
            return false;
        }
    }
    return true;
}

void Encounter::addPlayer(Creature c) {
    players.push_back(std::move(c));
}

void Encounter::addCreature(Creature c) {
    creatures.push_back(std::move(c));
}

Creature* Encounter::at(Side side, int index) {
    std::vector<Creature>& g = group(side);
    if (index < 0 || index >= static_cast<int>(g.size())) {
        return nullptr;
    }
    return &g[index];
}

const Creature* Encounter::at(Side side, int index) const {
    const std::vector<Creature>& g = group(side);
    if (index < 0 || index >= static_cast<int>(g.size())) {
        return nullptr;
    }
    return &g[index];
}

HitReport Encounter::damage(Side side, int index, int amount) {
    HitReport report;
    Creature* c = at(side, index);
    if (c == nullptr) {
        std::cout << "Invalid creature index.\n";
        return report;
    }
    const int hpBefore = c->getCurrentHp();
    report.damage = c->applyDamage(amount);
    const int taken = report.damage.afterMod;  // temp HP soaking it still counts as damage taken
    report.droppedToZero = hpBefore > 0 && c->getCurrentHp() == 0;

    // Concentration: DC is 10 or half the damage taken, whichever is higher.
    if (c->isConcentrating() && taken > 0) {
        if (c->getCurrentHp() == 0) {
            c->setConcentrating(false);
            report.concentrationLost = true;
        } else {
            report.concentrationDc = std::max(10, taken / 2);
        }
    }

    if (side == Side::Players && taken > 0) {
        if (report.damage.wasAtZero) {
            // Any damage at 0 HP is a failed death save.
            c->addDeathFailures(1);
            report.deathSaveFailed = true;
        } else if (report.droppedToZero) {
            // Massive damage: leftover damage of at least max HP kills outright.
            const int leftover = report.damage.afterMod - report.damage.absorbed - report.damage.hpLost;
            if (leftover >= c->getMaxHp()) {
                c->setDeathSaves(0, 3);
                report.instantDeath = true;
            }
        }
        report.killedOutright = c->getCurrentHp() == 0 && c->getDeathFailures() >= 3;
    }
    return report;
}

int Encounter::heal(Side side, int index, int amount) {
    Creature* c = at(side, index);
    if (c == nullptr) {
        std::cout << "Invalid creature index.\n";
        return 0;
    }
    return c->applyHealing(amount);
}

void Encounter::showStatus() const {
    std::cout << "\n--- Players ---\n";
    for (size_t i = 0; i < players.size(); i++) {
        std::cout << i << ": ";
        players[i].showHp();
    }
    std::cout << "\n--- Creatures ---\n";
    for (size_t i = 0; i < creatures.size(); i++) {
        std::cout << i << ": ";
        creatures[i].showHp();
    }
}

void Encounter::edit(Side side, int index, const std::string& name, int maxHp, int hp) {
    Creature* c = at(side, index);
    if (c == nullptr) {
        return;
    }
    if (!name.empty()) {
        c->rename(name);
    }
    c->setMaxHp(maxHp);
    c->setHp(hp);
}

void Encounter::remove(Side side, int index) {
    std::vector<Creature>& g = group(side);
    if (index < 0 || index >= static_cast<int>(g.size())) {
        return;
    }
    g.erase(g.begin() + index);
}

void Encounter::clear() {
    players.clear();
    creatures.clear();
    endedManually = false;
}

void Encounter::clearSide(Side side) {
    group(side).clear();
}

void Encounter::endManually() {
    endedManually = true;
}

void Encounter::resume() {
    endedManually = false;
}

bool Encounter::isOver() const {
    return endedManually || allDead(players) || allDead(creatures);
}

std::vector<std::string> Encounter::tickConditions() {
    std::vector<std::string> lines;
    for (std::vector<Creature>* g : {&players, &creatures}) {
        for (Creature& c : *g) {
            for (const std::string& ended : c.tickConditions()) {
                lines.push_back(c.getName() + ": " + ended + " ended");
            }
        }
    }
    return lines;
}
