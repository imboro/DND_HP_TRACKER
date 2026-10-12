#include "player_state.h"

#include <cstdio>
#include <sstream>

#include "looks.h"

static std::string jsonString(const std::string& s) {
    std::string out = "\"";
    for (const unsigned char ch : s) {
        switch (ch) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': break;
            case '\t': out += "\\t"; break;
            default:
                if (ch < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", ch);
                    out += buf;
                } else {
                    out += static_cast<char>(ch);
                }
        }
    }
    return out + "\"";
}

static std::string jsonColor(ImU32 c) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", static_cast<unsigned>(c & 0xFF),
                  static_cast<unsigned>((c >> 8) & 0xFF), static_cast<unsigned>((c >> 16) & 0xFF));
    return jsonString(buf);
}

// What the phones may see: the round, whose turn it is and the turn order. Players
// come with their stats; enemies only with their name and look (no HP), and they
// only appear once combat has started (setup stays a surprise) and until defeated.
std::string playerStateJson(const Encounter& encounter, const BoardState& board) {
    std::ostringstream o;
    o << "{\"combat\":" << (board.inCombat ? "true" : "false") << ",\"round\":" << board.round << ",\"turn\":";
    const Creature* turn = board.inCombat && board.turnValid ? encounter.at(board.turn.side, board.turn.index) : nullptr;
    o << (turn ? jsonString(turn->getName()) : std::string("null")) << ",\"order\":[";
    bool firstItem = true;
    for (const CardRef& r : board.z) {
        const Creature* c = encounter.at(r.side, r.index);
        if (c == nullptr) continue;
        const Life life = lifeOf(r.side, *c);
        const bool player = r.side == Side::Players;
        if (!player && (!board.inCombat || life == Life::Dead)) continue;
        if (!firstItem) o << ',';
        firstItem = false;
        o << "{\"name\":" << jsonString(c->getName()) << ",\"side\":\"" << (player ? "player" : "enemy") << "\""
          << ",\"color\":" << jsonColor(creatureColor(r.side, *c)) << ",\"icon\":" << iconIndexFor(r.side, c->getIcon());
        if (player) {
            const char* lifeName = life == Life::Dead ? "dead" : life == Life::Dying ? "dying" : life == Life::Stable ? "stable" : "up";
            o << ",\"hp\":" << c->getCurrentHp() << ",\"max\":" << c->getMaxHp() << ",\"temp\":" << c->getTempHp()
              << ",\"ac\":" << c->getAc() << ",\"speed\":" << c->getSpeed() << ",\"pp\":" << c->getPassivePerception()
              << ",\"level\":" << c->getLevel() << ",\"life\":\"" << lifeName << "\""
              << ",\"saves\":[" << c->getDeathSuccesses() << ',' << c->getDeathFailures() << ']'
              << ",\"conc\":" << (c->isConcentrating() ? "true" : "false")
              << ",\"bloodied\":" << (c->isBloodied() ? "true" : "false")
              << ",\"mod\":" << jsonString(c->getDamageMod() == DamageMod::Normal ? "" : damageModName(c->getDamageMod()))
              << ",\"conditions\":[";
            for (size_t i = 0; i < c->getConditions().size(); i++) {
                const Condition& cond = c->getConditions()[i];
                o << (i ? "," : "") << '[' << jsonString(cond.name) << ',' << cond.rounds << ']';
            }
            o << ']';
        }
        o << '}';
    }
    o << "]}";
    return o.str();
}
