// Checks the game-rules layer (no UI). Build target: logic_tests.
#include <cstdio>
#include <random>
#include <string>
#include "creature.h"
#include "dice.h"
#include "difficulty.h"
#include "encounter.h"

static int failures = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                 \
        }                                                               \
    } while (0)

static void testDamageRules() {
    Creature c("Ogre", 59);
    DamageResult r = c.applyDamage(10);
    CHECK(r.hpLost == 10 && c.getCurrentHp() == 49);

    c.setDamageMod(DamageMod::Resistant);
    r = c.applyDamage(9);  // halved, rounded down
    CHECK(r.afterMod == 4 && c.getCurrentHp() == 45);

    c.setDamageMod(DamageMod::Vulnerable);
    r = c.applyDamage(5);
    CHECK(r.afterMod == 10 && c.getCurrentHp() == 35);

    c.setDamageMod(DamageMod::Immune);
    r = c.applyDamage(30);
    CHECK(r.afterMod == 0 && c.getCurrentHp() == 35);

    c.setDamageMod(DamageMod::Normal);
    c.grantTempHp(8);
    c.grantTempHp(5);  // doesn't stack: keeps 8
    CHECK(c.getTempHp() == 8);
    r = c.applyDamage(12);
    CHECK(r.absorbed == 8 && r.hpLost == 4 && c.getTempHp() == 0 && c.getCurrentHp() == 31);

    CHECK(!c.isBloodied());
    c.setHp(29);
    CHECK(c.isBloodied());  // 29 <= 59/2
    c.setHp(30);
    CHECK(!c.isBloodied());

    CHECK(c.applyHealing(100) == 29 && c.getCurrentHp() == 59);
}

static void testDeathSavesAndConcentration() {
    Encounter e;
    e.addPlayer(Creature("Cleric", 20));
    e.at(Side::Players, 0)->setConcentrating(true);

    HitReport h = e.damage(Side::Players, 0, 8);
    CHECK(h.concentrationDc == 10);  // half of 8 is 4, minimum 10
    h = e.damage(Side::Players, 0, 10);
    CHECK(h.concentrationDc == 10);
    e.heal(Side::Players, 0, 20);
    h = e.damage(Side::Players, 0, 19);  // 19 / 2 = 9 -> DC 10
    CHECK(h.concentrationDc == 10);
    e.heal(Side::Players, 0, 20);
    e.at(Side::Players, 0)->setMaxHp(60);
    e.at(Side::Players, 0)->setHp(60);
    h = e.damage(Side::Players, 0, 30);
    CHECK(h.concentrationDc == 15);

    // Drop to 0: concentration ends, then each hit at 0 is a failed death save.
    h = e.damage(Side::Players, 0, 40);
    CHECK(h.droppedToZero && h.concentrationLost && !e.at(Side::Players, 0)->isConcentrating());
    CHECK(!h.instantDeath);
    h = e.damage(Side::Players, 0, 1);
    CHECK(h.deathSaveFailed && e.at(Side::Players, 0)->getDeathFailures() == 1);
    e.damage(Side::Players, 0, 1);
    h = e.damage(Side::Players, 0, 1);
    CHECK(h.killedOutright && e.at(Side::Players, 0)->getDeathFailures() == 3);

    // Healing from 0 resets death saves.
    e.heal(Side::Players, 0, 5);
    CHECK(e.at(Side::Players, 0)->getDeathFailures() == 0 && e.at(Side::Players, 0)->getCurrentHp() == 5);

    // Massive damage: leftover >= max HP is instant death.
    e.addPlayer(Creature("Wizard", 10));
    h = e.damage(Side::Players, 1, 25);  // 10 to reach 0, 15 left over >= 10
    CHECK(h.instantDeath && h.killedOutright);

    // Creatures don't get death saves.
    e.addCreature(Creature("Goblin", 7));
    e.damage(Side::Creatures, 0, 7);
    h = e.damage(Side::Creatures, 0, 5);
    CHECK(!h.deathSaveFailed && e.at(Side::Creatures, 0)->getDeathFailures() == 0);
}

static void testConditions() {
    Encounter e;
    e.addCreature(Creature("Goblin", 7));
    Creature* g = e.at(Side::Creatures, 0);
    g->addCondition("Poisoned", 2);
    g->addCondition("Prone", 0);
    g->addCondition("Poisoned", 3);  // updates, doesn't duplicate
    CHECK(g->getConditions().size() == 2 && g->getConditions()[0].rounds == 3);
    CHECK(e.tickConditions().empty());
    CHECK(e.tickConditions().empty());
    std::vector<std::string> ended = e.tickConditions();
    CHECK(ended.size() == 1 && ended[0] == "Goblin: Poisoned ended");
    CHECK(g->hasCondition("Prone") && !g->hasCondition("Poisoned"));
    g->removeCondition("Prone");
    CHECK(g->getConditions().empty());
}

static void testDice() {
    std::mt19937 rng(1234);
    DiceRoll r = rollExpression("12", rng);
    CHECK(r.ok && r.total == 12 && !r.hasDice);
    for (int i = 0; i < 200; i++) {
        r = rollExpression("2d6+3", rng);
        CHECK(r.ok && r.hasDice && r.total >= 5 && r.total <= 15);
        r = rollExpression("d20", rng);
        CHECK(r.ok && r.total >= 1 && r.total <= 20);
        r = rollExpression(" 1d8 + 1D6 - 1 ", rng);
        CHECK(r.ok && r.total >= 1 && r.total <= 13);
    }
    CHECK(!rollExpression("", rng).ok);
    CHECK(!rollExpression("abc", rng).ok);
    CHECK(!rollExpression("2d", rng).ok);
    CHECK(!rollExpression("1000d6", rng).ok);
    CHECK(!rollExpression("3d6x", rng).ok);
    r = rollExpression("1d1+1d1", rng);
    CHECK(r.ok && r.total == 2 && r.detail == "1d1[1] + 1d1[1] = 2");
}

static void testDifficulty() {
    // Party of four level 1: thresholds 100/200/300/400.
    DifficultyReport r = rateEncounter({1, 1, 1, 1}, {50, 50});  // 2 goblins, 100 XP x1.5 = 150
    CHECK(r.valid && r.thresholds[0] == 100 && r.thresholds[3] == 400);
    CHECK(r.adjustedXp == 150 && r.rating == Difficulty::Easy && r.xpPerPlayer == 25);

    r = rateEncounter({1, 1, 1, 1}, {50, 50, 50, 50});  // 200 x2 = 400 -> deadly
    CHECK(r.adjustedXp == 400 && r.rating == Difficulty::Deadly);

    r = rateEncounter({5, 5}, {1800});  // small party: x1 -> x1.5
    CHECK(r.multiplier == 1.5f && r.adjustedXp == 2700 && r.rating == Difficulty::Deadly);

    r = rateEncounter({}, {50});
    CHECK(!r.valid);
    CHECK(crXp(2) == 50 && std::string(crLabel(3)) == "1/2" && crXp(crCount() - 1) == 155000);
}

int main() {
    testDamageRules();
    testDeathSavesAndConcentration();
    testConditions();
    testDice();
    testDifficulty();
    if (failures == 0) {
        std::printf("All logic tests passed.\n");
        return 0;
    }
    std::printf("%d check(s) failed.\n", failures);
    return 1;
}
