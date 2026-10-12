#include "save_format.h"

#include <SDL.h>

#include <algorithm>
#include <filesystem>  // UTF-8 save paths (user folders with accents)
#include <fstream>
#include <sstream>

#include "difficulty.h"

// Escapes the characters the line format uses as separators.
std::string esc(const std::string& s) {
    std::string out;
    for (char ch : s) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '|':  out += "\\p"; break;
            case '\n': out += "\\n"; break;
            case ',':  out += "\\c"; break;
            case ':':  out += "\\o"; break;
            case '\r': break;
            default:   out += ch;
        }
    }
    return out;
}

std::string unesc(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            const char n = s[++i];
            if (n == 'p') out += '|';
            else if (n == 'n') out += '\n';
            else if (n == 'c') out += ',';
            else if (n == 'o') out += ':';
            else out += n;
        } else {
            out += s[i];
        }
    }
    return out;
}

std::vector<std::string> splitRaw(const std::string& s, char sep) {
    std::vector<std::string> parts;
    std::string cur;
    for (char ch : s) {
        if (ch == sep) {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur += ch;
        }
    }
    parts.push_back(cur);
    return parts;
}

std::string savePath(const char* file) {
    std::string dir;
    if (char* pref = SDL_GetPrefPath("HotelWorkshop", "HPTracker")) {
        dir = pref;
        SDL_free(pref);
    }
    return dir + file;
}

// Version 2, one combatant per line in stacking order:
//   P|name|maxHp|hp|x|y|temp|ac|speed|pp|level|cr|mod|conc|deathOk|deathFail|cond:rounds,...|notes
// plus R|round, E|ended, M|1 when in combat (absent: setup) and T|index (whose turn
// it is, as an index into the lines below). A|color|icon after a combatant's line
// sets its look (palette and icon indexes, -1 = default). The x and y fields are no longer used (the order of the lines is the
// turn order) but are still written. Version 1 lines (P|name|maxHp|hp|x|y) still load;
// older versions of the app ignore the T line.
std::string serializeBoard(const Encounter& encounter, const BoardState& board) {
    std::ostringstream out;
    out << "HPTRACKER 2\n";
    out << "R|" << board.round << "\n";
    out << "E|" << (encounter.isEndedManually() ? 1 : 0) << "\n";
    if (board.inCombat) out << "M|1\n";
    if (board.turnValid) {
        for (size_t i = 0; i < board.z.size(); i++) {
            if (board.z[i] == board.turn) out << "T|" << i << "\n";
        }
    }
    for (const CardRef& r : board.z) {
        const Creature& c = *encounter.at(r.side, r.index);
        std::string conds;
        for (const Condition& cond : c.getConditions()) {
            if (!conds.empty()) conds += ",";
            conds += esc(cond.name) + ":" + std::to_string(cond.rounds);
        }
        out << (r.side == Side::Players ? 'P' : 'C') << '|' << esc(c.getName()) << '|' << c.getMaxHp() << '|'
            << c.getCurrentHp() << "|0|0|" << c.getTempHp() << '|' << c.getAc()
            << '|' << c.getSpeed() << '|' << c.getPassivePerception() << '|' << c.getLevel() << '|'
            << c.getCrIndex() << '|' << static_cast<int>(c.getDamageMod()) << '|' << (c.isConcentrating() ? 1 : 0)
            << '|' << c.getDeathSuccesses() << '|' << c.getDeathFailures() << '|' << conds << '|'
            << esc(c.getNotes()) << '\n';
        if (c.getColor() >= 0 || c.getIcon() >= 0) out << "A|" << c.getColor() << '|' << c.getIcon() << '\n';
    }
    return out.str();
}

bool deserializeBoard(const std::string& text, Encounter& encounter, BoardState& board) {
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line) || line.rfind("HPTRACKER", 0) != 0) return false;

    encounter.clear();
    board = BoardState();
    int pendingTurn = -1;
    CardRef lastAdded{Side::Players, -1};
    while (std::getline(in, line)) {
        const std::vector<std::string> f = splitRaw(line, '|');
        try {
            if (f.size() == 2 && f[0] == "T") {
                pendingTurn = std::stoi(f[1]);
                continue;
            }
            if (f.size() == 2 && f[0] == "M") {
                board.inCombat = f[1] == "1";
                continue;
            }
            if (f.size() == 3 && f[0] == "A") {
                if (lastAdded.index >= 0) {
                    Creature* last = encounter.at(lastAdded.side, lastAdded.index);
                    if (last) {
                        last->setColor(std::clamp(std::stoi(f[1]), -1, 99));
                        last->setIcon(std::clamp(std::stoi(f[2]), -1, 99));
                    }
                }
                continue;
            }
            if (f.size() == 2 && f[0] == "R") {
                board.round = std::max(1, std::stoi(f[1]));
                continue;
            }
            if (f.size() == 2 && f[0] == "E") {
                if (f[1] == "1") encounter.endManually();
                continue;
            }
            if ((f.size() != 6 && f.size() != 18) || (f[0] != "P" && f[0] != "C")) continue;
            const int maxHp = std::max(1, std::stoi(f[2]));
            Creature c(unesc(f[1]), maxHp);
            c.setHp(std::clamp(std::stoi(f[3]), 0, maxHp));
            if (f.size() == 18) {
                c.setTempHp(std::stoi(f[6]));
                c.setAc(std::stoi(f[7]));
                c.setSpeed(std::stoi(f[8]));
                c.setPassivePerception(std::stoi(f[9]));
                c.setLevel(std::stoi(f[10]));
                c.setCrIndex(std::clamp(std::stoi(f[11]), -1, crCount() - 1));
                c.setDamageMod(static_cast<DamageMod>(std::clamp(std::stoi(f[12]), 0, 3)));
                c.setConcentrating(f[13] == "1");
                c.setDeathSaves(std::stoi(f[14]), std::stoi(f[15]));
                if (!f[16].empty()) {
                    for (const std::string& item : splitRaw(f[16], ',')) {
                        const std::vector<std::string> kv = splitRaw(item, ':');
                        if (kv.size() == 2) c.addCondition(unesc(kv[0]), std::stoi(kv[1]));
                    }
                }
                c.setNotes(unesc(f[17]));
            }
            lastAdded = addCombatant(encounter, board, f[0] == "P" ? Side::Players : Side::Creatures, std::move(c));
        } catch (const std::exception&) {
            continue;  // skip malformed line
        }
    }
    if (pendingTurn >= 0 && pendingTurn < static_cast<int>(board.z.size())) {
        board.turn = board.z[pendingTurn];
        board.turnValid = true;
    }
    return true;
}

// Save paths are UTF-8 (from SDL); on Windows they must be opened as wide paths.
#ifdef _WIN32
static std::filesystem::path fsPath(const std::string& utf8) { return std::filesystem::u8path(utf8); }
#else
static const std::string& fsPath(const std::string& utf8) { return utf8; }
#endif

bool writeFile(const std::string& path, const std::string& text) {
    std::ofstream out(fsPath(path), std::ios::trunc);
    if (!out) return false;
    out << text;
    return static_cast<bool>(out);
}

bool readFile(const std::string& path, std::string& text) {
    std::ifstream in(fsPath(path));
    if (!in) return false;
    std::stringstream buffer;
    buffer << in.rdbuf();
    text = buffer.str();
    return true;
}
