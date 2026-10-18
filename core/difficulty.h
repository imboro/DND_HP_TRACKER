#pragma once
#include <string>
#include <vector>

// Encounter difficulty using the 5e XP threshold method (thresholds by level, monster multipliers).

enum class Difficulty { Trivial, Easy, Medium, Hard, Deadly };

struct DifficultyReport {
    bool valid = false;       // needs at least one player and one monster with a CR
    int baseXp = 0;           // sum of monster XP (also the XP award)
    float multiplier = 1.0f;  // for number of monsters, adjusted for party size
    int adjustedXp = 0;
    int thresholds[4] = {0, 0, 0, 0};  // party totals: easy, medium, hard, deadly
    int xpPerPlayer = 0;
    Difficulty rating = Difficulty::Trivial;
};

DifficultyReport rateEncounter(const std::vector<int>& playerLevels, const std::vector<int>& monsterXp);
const char* difficultyName(Difficulty d);

// Challenge rating table: index 0 is CR 0, then 1/8, 1/4, 1/2, 1 ... 30.
int crCount();
const char* crLabel(int index);
int crXp(int index);  // XP for that CR, 0 for an invalid index
