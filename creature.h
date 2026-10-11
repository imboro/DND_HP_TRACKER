#pragma once
#include <string>
#include <utility>
#include <vector>

// How a creature takes damage (applied to every hit it receives).
enum class DamageMod { Normal, Resistant, Vulnerable, Immune };

// A condition such as "Poisoned". rounds == 0 means "until removed".
struct Condition {
    std::string name;
    int rounds = 0;
};

// What actually happened when damage was applied.
struct DamageResult {
    int rolled = 0;       // amount before resistance/vulnerability
    int afterMod = 0;     // amount after resistance/vulnerability/immunity
    int absorbed = 0;     // soaked up by temporary HP
    int hpLost = 0;       // taken from real HP
    bool wasAtZero = false;  // the creature was already at 0 HP
};

class Creature {
private:
    std::string name;
    int maxHp;
    int currentHp;
    int tempHp = 0;

    // Quick-reference stats.
    int ac = 10;
    int speed = 30;
    int passivePerception = 10;
    int level = 1;        // players: character level (for encounter difficulty)
    int crIndex = -1;     // creatures: index into the CR table, -1 = not set

    DamageMod damageMod = DamageMod::Normal;
    bool concentrating = false;
    std::vector<Condition> conditions;
    std::string notes;

    int deathSuccesses = 0;
    int deathFailures = 0;

public:
    Creature(std::string name, int maxHp);

    // Raw HP changes (no temp HP, no resistance).
    void takeDamage(int amount);
    void heal(int amount);

    // Full rules: resistance/vulnerability/immunity, then temp HP, then HP.
    DamageResult applyDamage(int amount);
    // Returns the HP actually regained. Healing from 0 HP resets death saves.
    int applyHealing(int amount);

    bool isDead() const;
    bool isBloodied() const;  // at or below half HP, but above 0

    void rename(std::string newName);
    void setMaxHp(int value);
    void setHp(int value);
    void setTempHp(int value);
    void grantTempHp(int value);  // temp HP doesn't stack: keeps the higher value

    void setAc(int value) { ac = value; }
    void setSpeed(int value) { speed = value; }
    void setPassivePerception(int value) { passivePerception = value; }
    void setLevel(int value);
    void setCrIndex(int value) { crIndex = value; }
    void setDamageMod(DamageMod mod) { damageMod = mod; }
    void setConcentrating(bool value) { concentrating = value; }
    void setNotes(std::string value) { notes = std::move(value); }

    // Death saves (players at 0 HP). Values are clamped to 0..3.
    void setDeathSaves(int successes, int failures);
    void addDeathFailures(int count);
    void resetDeathSaves();

    // Conditions. Adding an existing condition updates its duration.
    void addCondition(const std::string& conditionName, int rounds);
    void removeCondition(const std::string& conditionName);
    bool hasCondition(const std::string& conditionName) const;
    void clearConditions() { conditions.clear(); }
    // Counts down timed conditions by one round; returns the names that ended.
    std::vector<std::string> tickConditions();

    void showHp() const;

    const std::string& getName() const;
    int getCurrentHp() const;
    int getMaxHp() const;
    int getTempHp() const { return tempHp; }
    int getAc() const { return ac; }
    int getSpeed() const { return speed; }
    int getPassivePerception() const { return passivePerception; }
    int getLevel() const { return level; }
    int getCrIndex() const { return crIndex; }
    DamageMod getDamageMod() const { return damageMod; }
    bool isConcentrating() const { return concentrating; }
    const std::vector<Condition>& getConditions() const { return conditions; }
    const std::string& getNotes() const { return notes; }
    int getDeathSuccesses() const { return deathSuccesses; }
    int getDeathFailures() const { return deathFailures; }
};
