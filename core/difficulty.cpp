#include "difficulty.h"
#include <algorithm>

namespace {

// XP thresholds per character, levels 1-20: easy, medium, hard, deadly.
const int THRESHOLDS[20][4] = {
    {25, 50, 75, 100},       {50, 100, 150, 200},     {75, 150, 225, 400},
    {125, 250, 375, 500},    {250, 500, 750, 1100},   {300, 600, 900, 1400},
    {350, 750, 1100, 1700},  {450, 900, 1400, 2100},  {550, 1100, 1600, 2400},
    {600, 1200, 1900, 2800}, {800, 1600, 2400, 3600}, {1000, 2000, 3000, 4500},
    {1100, 2200, 3400, 5100}, {1250, 2500, 3800, 5700}, {1400, 2800, 4300, 6400},
    {1600, 3200, 4800, 7200}, {2000, 3900, 5900, 8800}, {2100, 4200, 6300, 9500},
    {2400, 4900, 7300, 10900}, {2800, 5700, 8500, 12700},
};

// Encounter multipliers, from "fewer monsters than a small party" up to 15+.
const float MULTIPLIERS[] = {0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f};

struct CrEntry {
    const char* label;
    int xp;
};

const CrEntry CR_TABLE[] = {
    {"0", 10},       {"1/8", 25},     {"1/4", 50},     {"1/2", 100},    {"1", 200},
    {"2", 450},      {"3", 700},      {"4", 1100},     {"5", 1800},     {"6", 2300},
    {"7", 2900},     {"8", 3900},     {"9", 5000},     {"10", 5900},    {"11", 7200},
    {"12", 8400},    {"13", 10000},   {"14", 11500},   {"15", 13000},   {"16", 15000},
    {"17", 18000},   {"18", 20000},   {"19", 22000},   {"20", 25000},   {"21", 33000},
    {"22", 41000},   {"23", 50000},   {"24", 62000},   {"25", 75000},   {"26", 90000},
    {"27", 105000},  {"28", 120000},  {"29", 135000},  {"30", 155000},
};

}  // namespace

DifficultyReport rateEncounter(const std::vector<int>& playerLevels, const std::vector<int>& monsterXp) {
    DifficultyReport r;
    int monsters = 0;
    for (int xp : monsterXp) {
        if (xp > 0) {
            r.baseXp += xp;
            monsters++;
        }
    }
    if (playerLevels.empty() || monsters == 0) {
        return r;
    }
    r.valid = true;

    for (int level : playerLevels) {
        const int l = std::clamp(level, 1, 20) - 1;
        for (int k = 0; k < 4; k++) {
            r.thresholds[k] += THRESHOLDS[l][k];
        }
    }

    int idx;
    if (monsters == 1) idx = 1;
    else if (monsters == 2) idx = 2;
    else if (monsters <= 6) idx = 3;
    else if (monsters <= 10) idx = 4;
    else if (monsters <= 14) idx = 5;
    else idx = 6;
    const int partySize = static_cast<int>(playerLevels.size());
    if (partySize < 3) idx = std::min(idx + 1, 6);
    else if (partySize >= 6) idx = std::max(idx - 1, 0);
    r.multiplier = MULTIPLIERS[idx];

    r.adjustedXp = static_cast<int>(static_cast<float>(r.baseXp) * r.multiplier + 0.5f);
    r.xpPerPlayer = r.baseXp / partySize;

    if (r.adjustedXp >= r.thresholds[3]) r.rating = Difficulty::Deadly;
    else if (r.adjustedXp >= r.thresholds[2]) r.rating = Difficulty::Hard;
    else if (r.adjustedXp >= r.thresholds[1]) r.rating = Difficulty::Medium;
    else if (r.adjustedXp >= r.thresholds[0]) r.rating = Difficulty::Easy;
    else r.rating = Difficulty::Trivial;
    return r;
}

const char* difficultyName(Difficulty d) {
    switch (d) {
        case Difficulty::Trivial: return "Trivial";
        case Difficulty::Easy:    return "Easy";
        case Difficulty::Medium:  return "Medium";
        case Difficulty::Hard:    return "Hard";
        case Difficulty::Deadly:  return "Deadly";
    }
    return "";
}

int crCount() {
    return static_cast<int>(sizeof(CR_TABLE) / sizeof(CR_TABLE[0]));
}

const char* crLabel(int index) {
    if (index < 0 || index >= crCount()) return "-";
    return CR_TABLE[index].label;
}

int crXp(int index) {
    if (index < 0 || index >= crCount()) return 0;
    return CR_TABLE[index].xp;
}
