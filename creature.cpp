#include "creature.h"
#include <algorithm>
#include <iostream>

Creature::Creature(std::string name, int maxHp)
    : name(std::move(name)), maxHp(std::max(1, maxHp)), currentHp(std::max(1, maxHp)) {}

void Creature::takeDamage(int amount) {
    this->currentHp = std::max(0, currentHp - amount);
}

void Creature::heal(int amount) {
    if (currentHp == 0 && amount > 0) {
        resetDeathSaves();
    }
    this->currentHp = std::min(maxHp, currentHp + amount);
}

DamageResult Creature::applyDamage(int amount) {
    DamageResult r;
    r.rolled = std::max(0, amount);
    r.wasAtZero = currentHp == 0;
    switch (damageMod) {
        case DamageMod::Normal:     r.afterMod = r.rolled; break;
        case DamageMod::Resistant:  r.afterMod = r.rolled / 2; break;  // rounded down
        case DamageMod::Vulnerable: r.afterMod = r.rolled * 2; break;
        case DamageMod::Immune:     r.afterMod = 0; break;
    }
    r.absorbed = std::min(tempHp, r.afterMod);
    tempHp -= r.absorbed;
    r.hpLost = std::min(currentHp, r.afterMod - r.absorbed);
    currentHp -= r.hpLost;
    return r;
}

int Creature::applyHealing(int amount) {
    const int before = currentHp;
    heal(std::max(0, amount));
    return currentHp - before;
}

bool Creature::isDead() const {
    return this->currentHp <= 0;
}

bool Creature::isBloodied() const {
    return currentHp > 0 && currentHp * 2 <= maxHp;
}

void Creature::rename(std::string newName) {
    name = std::move(newName);
}

// Changing max HP keeps current HP within the new maximum.
void Creature::setMaxHp(int value) {
    maxHp = std::max(1, value);
    currentHp = std::min(currentHp, maxHp);
}

void Creature::setHp(int value) {
    if (currentHp == 0 && value > 0) {
        resetDeathSaves();
    }
    currentHp = std::clamp(value, 0, maxHp);
}

void Creature::setTempHp(int value) {
    tempHp = std::max(0, value);
}

void Creature::grantTempHp(int value) {
    tempHp = std::max(tempHp, value);
}

void Creature::setLevel(int value) {
    level = std::clamp(value, 1, 20);
}

void Creature::setDeathSaves(int successes, int failures) {
    deathSuccesses = std::clamp(successes, 0, 3);
    deathFailures = std::clamp(failures, 0, 3);
}

void Creature::addDeathFailures(int count) {
    deathFailures = std::clamp(deathFailures + count, 0, 3);
}

void Creature::resetDeathSaves() {
    deathSuccesses = 0;
    deathFailures = 0;
}

void Creature::addCondition(const std::string& conditionName, int rounds) {
    for (Condition& c : conditions) {
        if (c.name == conditionName) {
            c.rounds = std::max(0, rounds);
            return;
        }
    }
    conditions.push_back(Condition{conditionName, std::max(0, rounds)});
}

void Creature::removeCondition(const std::string& conditionName) {
    conditions.erase(std::remove_if(conditions.begin(), conditions.end(),
                                    [&](const Condition& c) { return c.name == conditionName; }),
                     conditions.end());
}

bool Creature::hasCondition(const std::string& conditionName) const {
    return std::any_of(conditions.begin(), conditions.end(),
                       [&](const Condition& c) { return c.name == conditionName; });
}

std::vector<std::string> Creature::tickConditions() {
    std::vector<std::string> ended;
    std::vector<Condition> kept;
    for (Condition c : conditions) {
        if (c.rounds > 0) {
            c.rounds--;
            if (c.rounds == 0) {
                ended.push_back(c.name);
                continue;
            }
        }
        kept.push_back(c);
    }
    conditions = kept;
    return ended;
}

void Creature::showHp() const {
    std::cout << this->name << " has: " << this->currentHp << "/" << this->maxHp << " HP"
              << (isDead() ? " (dead)" : "") << "\n";
}

const std::string& Creature::getName() const {
    return this->name;
}

int Creature::getCurrentHp() const {
    return this->currentHp;
}

int Creature::getMaxHp() const {
    return this->maxHp;
}
