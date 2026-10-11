#pragma once
#include <random>
#include <string>

// Result of rolling an expression like "2d6+3", "d20", "8d6", "12" or "1d8+1d6-1".
struct DiceRoll {
    bool ok = false;
    int total = 0;
    bool hasDice = false;   // false for plain numbers like "12"
    std::string detail;     // e.g. "2d6[3,5] + 3 = 11"
    std::string error;      // set when ok == false
};

DiceRoll rollExpression(const std::string& expression, std::mt19937& rng);
