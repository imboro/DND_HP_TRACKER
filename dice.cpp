#include "dice.h"
#include <cctype>

namespace {

const int MAX_DICE = 100;
const int MAX_SIDES = 1000;
const int MAX_NUMBER = 100000;

// Reads a run of digits at `i`. Returns -1 if there are none (or it's absurdly large).
int readNumber(const std::string& s, size_t& i) {
    if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i]))) {
        return -1;
    }
    long value = 0;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
        value = value * 10 + (s[i] - '0');
        if (value > MAX_NUMBER) {
            return -1;
        }
        i++;
    }
    return static_cast<int>(value);
}

}  // namespace

DiceRoll rollExpression(const std::string& expression, std::mt19937& rng) {
    DiceRoll roll;

    // Lowercase, no spaces.
    std::string s;
    for (char ch : expression) {
        if (!std::isspace(static_cast<unsigned char>(ch))) {
            s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
    }
    if (s.empty()) {
        roll.error = "Empty amount";
        return roll;
    }

    size_t i = 0;
    int total = 0;
    bool first = true;
    std::string detail;
    while (i < s.size()) {
        int sign = 1;
        if (s[i] == '+' || s[i] == '-') {
            sign = s[i] == '-' ? -1 : 1;
            i++;
        } else if (!first) {
            roll.error = "Expected + or - at \"" + s.substr(i) + "\"";
            return roll;
        }

        int count = readNumber(s, i);
        if (i < s.size() && s[i] == 'd') {
            // Dice term: [count]d<sides>
            i++;
            if (count < 0) count = 1;  // "d20" means "1d20"
            const int sides = readNumber(s, i);
            if (count < 1 || count > MAX_DICE || sides < 1 || sides > MAX_SIDES) {
                roll.error = "Dice must look like 2d6 (1-100 dice, 1-1000 sides)";
                return roll;
            }
            std::uniform_int_distribution<int> die(1, sides);
            int sum = 0;
            std::string faces;
            for (int k = 0; k < count; k++) {
                const int face = die(rng);
                sum += face;
                faces += (k ? "," : "") + std::to_string(face);
            }
            total += sign * sum;
            roll.hasDice = true;
            if (!first || sign < 0) detail += sign < 0 ? " - " : " + ";
            detail += std::to_string(count) + "d" + std::to_string(sides) + "[" + faces + "]";
        } else if (count >= 0) {
            total += sign * count;
            if (!first || sign < 0) detail += sign < 0 ? " - " : " + ";
            detail += std::to_string(count);
        } else {
            roll.error = "Not a number or dice at \"" + s.substr(i) + "\"";
            return roll;
        }
        first = false;
    }

    roll.ok = true;
    roll.total = total;
    roll.detail = roll.hasDice ? detail + " = " + std::to_string(total) : std::to_string(total);
    return roll;
}
