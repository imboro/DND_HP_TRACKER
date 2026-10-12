#pragma once
// Player screens: the public state of the fight that the phones get, as JSON.
#include <string>

#include "board.h"
#include "encounter.h"

// What the phones may see: the round, whose turn it is and the turn order. Players
// come with their stats; enemies only with their name and look (no HP), and they
// only appear once combat has started (setup stays a surprise) and until defeated.
std::string playerStateJson(const Encounter& encounter, const BoardState& board);
