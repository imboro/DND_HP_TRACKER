#pragma once
// Saving: the encounter as text (also used for undo snapshots) and reading and
// writing files in the app's data folder.
#include <string>
#include <vector>

#include "board.h"
#include "encounter.h"

// Escapes / restores the characters the line formats use as separators.
std::string esc(const std::string& s);
std::string unesc(const std::string& s);
// Splits on `sep` (no unescaping).
std::vector<std::string> splitRaw(const std::string& s, char sep);

// The whole board and its combatants as text, and back. deserializeBoard returns
// false (and changes nothing) when the text isn't a saved board.
std::string serializeBoard(const Encounter& encounter, const BoardState& board);
bool deserializeBoard(const std::string& text, Encounter& encounter, BoardState& board);

// `file` inside the app's data folder (from SDL, so it is per user and per OS).
std::string savePath(const char* file);
bool writeFile(const std::string& path, const std::string& text);
bool readFile(const std::string& path, std::string& text);
