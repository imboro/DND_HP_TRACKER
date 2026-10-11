#include <iostream>
#include "creature.h"
#include "encounter.h"

int main () {
    Encounter encounter;
    encounter.addPlayer(Creature("Hero", 40));
    encounter.addCreature(Creature("Goblin", 30));
    encounter.addCreature(Creature("Orc", 50));

    encounter.showStatus();

    encounter.damage(Side::Creatures, 0, 30);
    encounter.damage(Side::Creatures, 1, 50);

    encounter.showStatus();
    std::cout << (encounter.isOver() ? "Encounter over.\n" : "Encounter ongoing.\n");
}
