#include "library.h"

#include <algorithm>
#include <sstream>

#include "difficulty.h"
#include "save_format.h"  // esc, unesc, splitRaw

std::vector<MonsterTemplate> defaultLibrary() {
    // name, HP, AC, speed, passive perception, CR index (2 = 1/4, 3 = 1/2, 4 = 1 ...)
    return {
        {"Bandit", 11, 12, 30, 10, 1, DamageMod::Normal},
        {"Goblin", 7, 15, 30, 9, 2, DamageMod::Normal},
        {"Kobold", 5, 12, 30, 8, 1, DamageMod::Normal},
        {"Skeleton", 13, 13, 30, 9, 2, DamageMod::Normal},
        {"Wolf", 11, 13, 40, 13, 2, DamageMod::Normal},
        {"Zombie", 22, 8, 20, 8, 2, DamageMod::Normal},
        {"Orc", 15, 13, 30, 10, 3, DamageMod::Normal},
        {"Hobgoblin", 11, 18, 30, 10, 3, DamageMod::Normal},
        {"Bugbear", 27, 16, 30, 10, 4, DamageMod::Normal},
        {"Ghoul", 22, 12, 30, 10, 4, DamageMod::Normal},
        {"Ogre", 59, 11, 40, 8, 5, DamageMod::Normal},
        {"Owlbear", 59, 13, 40, 13, 6, DamageMod::Normal},
        {"Troll", 84, 15, 30, 12, 8, DamageMod::Normal},
        {"Young Red Dragon", 178, 18, 40, 18, 13, DamageMod::Normal},
    };
}

std::string serializeLibrary(const std::vector<MonsterTemplate>& lib) {
    std::ostringstream out;
    out << "HPLIBRARY 1\n";
    for (const MonsterTemplate& t : lib) {
        out << "T|" << esc(t.name) << '|' << t.maxHp << '|' << t.ac << '|' << t.speed << '|' << t.pp << '|'
            << t.cr << '|' << static_cast<int>(t.mod) << '\n';
        if (t.color >= 0 || t.icon >= 0) out << "S|" << t.color << '|' << t.icon << '\n';  // look; older versions skip it
    }
    return out.str();
}

bool deserializeLibrary(const std::string& text, std::vector<MonsterTemplate>& lib) {
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line) || line.rfind("HPLIBRARY", 0) != 0) return false;
    lib.clear();
    while (std::getline(in, line)) {
        const std::vector<std::string> f = splitRaw(line, '|');
        if (f.size() == 3 && f[0] == "S" && !lib.empty()) {
            try {
                lib.back().color = std::stoi(f[1]);
                lib.back().icon = std::stoi(f[2]);
            } catch (const std::exception&) {
            }
            continue;
        }
        if (f.size() != 8 || f[0] != "T") continue;
        try {
            MonsterTemplate t;
            t.name = unesc(f[1]);
            t.maxHp = std::max(1, std::stoi(f[2]));
            t.ac = std::stoi(f[3]);
            t.speed = std::stoi(f[4]);
            t.pp = std::stoi(f[5]);
            t.cr = std::clamp(std::stoi(f[6]), -1, crCount() - 1);
            t.mod = static_cast<DamageMod>(std::clamp(std::stoi(f[7]), 0, 3));
            lib.push_back(t);
        } catch (const std::exception&) {
            continue;
        }
    }
    return true;
}

Creature creatureFromTemplate(const MonsterTemplate& t) {
    Creature c(t.name, t.maxHp);
    c.setAc(t.ac);
    c.setSpeed(t.speed);
    c.setPassivePerception(t.pp);
    c.setCrIndex(t.cr);
    c.setDamageMod(t.mod);
    c.setColor(t.color);
    c.setIcon(t.icon);
    return c;
}

// One player per line: P|name|maxHp|ac|speed|pp|level
std::string serializeParty(const std::vector<PlayerTemplate>& party) {
    std::ostringstream out;
    out << "HPPARTY 1\n";
    for (const PlayerTemplate& t : party) {
        out << "P|" << esc(t.name) << '|' << t.maxHp << '|' << t.ac << '|' << t.speed << '|' << t.pp << '|'
            << t.level << '\n';
        if (t.color >= 0 || t.icon >= 0) out << "S|" << t.color << '|' << t.icon << '\n';
    }
    return out.str();
}

bool deserializeParty(const std::string& text, std::vector<PlayerTemplate>& party) {
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line) || line.rfind("HPPARTY", 0) != 0) return false;
    party.clear();
    while (std::getline(in, line)) {
        const std::vector<std::string> f = splitRaw(line, '|');
        if (f.size() == 3 && f[0] == "S" && !party.empty()) {
            try {
                party.back().color = std::stoi(f[1]);
                party.back().icon = std::stoi(f[2]);
            } catch (const std::exception&) {
            }
            continue;
        }
        if (f.size() != 7 || f[0] != "P") continue;
        try {
            PlayerTemplate t;
            t.name = unesc(f[1]);
            t.maxHp = std::max(1, std::stoi(f[2]));
            t.ac = std::stoi(f[3]);
            t.speed = std::stoi(f[4]);
            t.pp = std::stoi(f[5]);
            t.level = std::clamp(std::stoi(f[6]), 1, 20);
            party.push_back(t);
        } catch (const std::exception&) {
            continue;
        }
    }
    return true;
}

// A fresh player from the party: full HP, no conditions.
Creature creatureFromPlayer(const PlayerTemplate& t) {
    Creature c(t.name, t.maxHp);
    c.setAc(t.ac);
    c.setSpeed(t.speed);
    c.setPassivePerception(t.pp);
    c.setLevel(t.level);
    c.setColor(t.color);
    c.setIcon(t.icon);
    return c;
}

PlayerTemplate playerFromCreature(const Creature& c) {
    PlayerTemplate t;
    t.name = c.getName();
    t.maxHp = c.getMaxHp();
    t.ac = c.getAc();
    t.speed = c.getSpeed();
    t.pp = c.getPassivePerception();
    t.level = c.getLevel();
    t.color = c.getColor();
    t.icon = c.getIcon();
    return t;
}
