#pragma once
#include <string>
#include <vector>
#include "creature.h"

enum class Side { Players, Creatures };

// What a hit did, including the rules that trigger off it.
struct HitReport {
    DamageResult damage;
    bool droppedToZero = false;      // went from above 0 to 0 HP
    bool deathSaveFailed = false;    // player hit while already at 0 HP
    bool instantDeath = false;       // player: leftover damage >= max HP
    bool killedOutright = false;     // player now has 3 failed death saves
    int concentrationDc = 0;         // > 0: the creature must make a concentration save
    bool concentrationLost = false;  // dropped to 0 while concentrating
};

class Encounter {
private:
    std::vector<Creature> players;
    std::vector<Creature> creatures;
    bool endedManually = false;

    std::vector<Creature>& group(Side side);
    const std::vector<Creature>& group(Side side) const;
    static bool allDead(const std::vector<Creature>& g);

public:
    void addPlayer(Creature c);
    void addCreature(Creature c);
    HitReport damage(Side side, int index, int amount);
    int heal(Side side, int index, int amount);  // returns HP regained
    void showStatus() const;
    void edit(Side side, int index, const std::string& name, int maxHp, int hp);
    void remove(Side side, int index);
    void clear();
    void clearSide(Side side);
    void endManually();
    void resume();
    bool isEndedManually() const { return endedManually; }
    bool isOver() const;

    // Counts down every timed condition by one round.
    // Returns lines like "Goblin 2: Poisoned ended".
    std::vector<std::string> tickConditions();

    Creature* at(Side side, int index);
    const Creature* at(Side side, int index) const;

    const std::vector<Creature>& getPlayers() const { return players; }
    const std::vector<Creature>& getCreatures() const { return creatures; }
};
