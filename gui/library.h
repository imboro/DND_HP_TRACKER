#pragma once
// The two libraries the DM keeps between sessions: monsters to add in a click,
// and the party (the players who fight every combat).
#include <string>
#include <vector>

#include "creature.h"

// ---- Monster library -------------------------------------------------------

struct MonsterTemplate {
    std::string name;
    int maxHp = 10;
    int ac = 10;
    int speed = 30;
    int pp = 10;
    int cr = -1;
    DamageMod mod = DamageMod::Normal;
    int color = -1;  // look (palette / icon index), -1 = default
    int icon = -1;
};

std::vector<MonsterTemplate> defaultLibrary();
std::string serializeLibrary(const std::vector<MonsterTemplate>& lib);
bool deserializeLibrary(const std::string& text, std::vector<MonsterTemplate>& lib);
Creature creatureFromTemplate(const MonsterTemplate& t);

// ---- Party -------------------------------------------------------------------

struct PlayerTemplate {
    std::string name;
    int maxHp = 20;
    int ac = 12;
    int speed = 30;
    int pp = 10;
    int level = 1;
    int color = -1;  // look (palette / icon index), -1 = default
    int icon = -1;
};

std::string serializeParty(const std::vector<PlayerTemplate>& party);
bool deserializeParty(const std::string& text, std::vector<PlayerTemplate>& party);
// A fresh player from the party: full HP, no conditions.
Creature creatureFromPlayer(const PlayerTemplate& t);
PlayerTemplate playerFromCreature(const Creature& c);
