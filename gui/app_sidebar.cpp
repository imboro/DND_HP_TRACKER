#include "app.h"

#include <algorithm>
#include <cctype>

#include "imgui_internal.h"
#include "board_view.h"
#include "difficulty.h"
#include "save_format.h"
#include "ui_kit.h"

void App::drawSidebar(ImVec2 sidebarSize, float panelRound) {
    const float S = UI_SCALE;
    {
        // Hard ink shadow behind the sidebar panel.
        const ImVec2 sp = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(sp + ImVec2(4 * S, 5 * S), sp + sidebarSize + ImVec2(4 * S, 5 * S),
                                                  COL_INK, panelRound);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, panelRound);
    ImGui::BeginChild("Controls", sidebarSize, true, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    // Headline.
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const float mul = 1.9f;
        heavyText(dl, p0, COL_TEXT, "HP Tracker", mul);
        ImGui::Dummy(ImVec2(0, heavyTextSize("HP Tracker", mul).y));
    }

    drawSummary();

    ImGui::Dummy(ImVec2(0, 2 * S));
    static const char* const TAB_LABELS[] = {"Combat", "Library", "Log"};
    segmentedTabs(TAB_LABELS, 3, &activeTab, S);
    ImGui::Dummy(ImVec2(0, 2 * S));
    // Switching tabs slides the new content in from the side of its tab while it fades in.
    if (activeTab != shownTab) {
        tabSlideDir = activeTab > shownTab ? 1.0f : -1.0f;
        shownTab = activeTab;
        tabSwitchTime = ImGui::GetTime();
    }
    tabK = easeOutCubic(clamp01(static_cast<float>(ImGui::GetTime() - tabSwitchTime) / 0.24f));
    if (activeTab == 0) drawCombatTab();
    if (activeTab == 1) drawLibraryTab();
    if (activeTab == 2) drawLogTab();
    ImGui::EndChild();
}

// Switching tabs slides the new content in from the side of its tab while it fades
// in. Applied to a tab body window just before it ends (covers everything it drew).
void App::finishTabBody() {
    const float S = UI_SCALE;
    if (tabK < 1.0f) {
        fadeVertices(ImGui::GetWindowDrawList(), 0, tabK, COL_PANEL, ImVec2((1.0f - tabK) * 22 * S * tabSlideDir, 0));
    }
}

// Summary: round, enemies, party HP tiles; status and difficulty pills.
void App::drawSummary() {
    const float S = UI_SCALE;
    int enemiesUp = 0;
    for (const Creature& c : encounter.getCreatures()) enemiesUp += c.getCurrentHp() > 0 ? 1 : 0;
    int partyHp = 0, partyMax = 0, partyUp = 0;
    for (const Creature& c : encounter.getPlayers()) {
        partyHp += c.getCurrentHp();
        partyMax += c.getMaxHp();
        partyUp += c.getCurrentHp() > 0 ? 1 : 0;
    }
    const int enemyCount = static_cast<int>(encounter.getCreatures().size());
    const int partyCount = static_cast<int>(encounter.getPlayers().size());

    const float gap = 9 * S;
    const float tw = (ImGui::GetContentRegionAvail().x - 5 * S - gap * 2) / 3.0f;
    if (statTile("##round", "Round", std::to_string(board.round), TileIcon::Cycle, COL_GREEN, tw, S)) {
        ImGui::SetTooltip("Round %d. End round (Combat tab) counts timed conditions down.", board.round);
    }
    ImGui::SameLine(0, gap);
    if (statTile("##enemies", "Enemies", std::to_string(enemiesUp), TileIcon::Bolt, COL_RED, tw, S)) {
        ImGui::SetTooltip("%d of %d enemies standing", enemiesUp, enemyCount);
    }
    ImGui::SameLine(0, gap);
    if (statTile("##party", "Party HP", std::to_string(partyHp), TileIcon::Heart, COL_PLAYER, tw, S)) {
        ImGui::SetTooltip("Party: %d of %d up\nHP %d / %d", partyUp, partyCount, partyHp, partyMax);
    }

    // More specific than Encounter::isOver(): dying players can still be saved.
    const auto& ps = encounter.getPlayers();
    const auto& cs = encounter.getCreatures();
    const bool enemiesDown = !cs.empty() && std::all_of(cs.begin(), cs.end(), [](const Creature& c) { return c.getCurrentHp() == 0; });
    const bool partyDown = !ps.empty() && std::all_of(ps.begin(), ps.end(), [](const Creature& c) { return c.getCurrentHp() == 0; });
    const bool partyDead = partyDown && std::all_of(ps.begin(), ps.end(), [](const Creature& c) { return c.getDeathFailures() >= 3; });
    if (!board.inCombat) pill("Setting up", COL_GRAY, S);
    else if (enemiesDown) pill("Enemies defeated", COL_GREEN, S);
    else if (partyDead) pill("Party defeated", COL_RED, S);
    else if (partyDown) pill("Party down!", COL_RED, S);
    else pill("In progress", COL_GREEN, S);

    std::vector<int> levels, xps;
    for (const Creature& c : encounter.getPlayers()) levels.push_back(c.getLevel());
    for (const Creature& c : encounter.getCreatures()) xps.push_back(crXp(c.getCrIndex()));
    const DifficultyReport dr = rateEncounter(levels, xps);
    if (dr.valid) {
        ImU32 col = COL_GRAY;
        switch (dr.rating) {
            case Difficulty::Trivial: col = COL_GRAY; break;
            case Difficulty::Easy:    col = COL_GREEN; break;
            case Difficulty::Medium:  col = IM_COL32(234, 179, 8, 255); break;
            case Difficulty::Hard:    col = IM_COL32(234, 108, 20, 255); break;
            case Difficulty::Deadly:  col = COL_RED; break;
        }
        const std::string text = std::string(difficultyName(dr.rating)) + "  \xC2\xB7  " +
                                 std::to_string(dr.adjustedXp) + " XP";
        // Same row as the status pill when it fits.
        const float need = ImGui::CalcTextSize(text.c_str()).x + 44 * S;
        ImGui::SameLine(0, 8 * S);
        if (ImGui::GetContentRegionAvail().x < need) ImGui::NewLine();
        pill(text.c_str(), col, S);
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("Party thresholds (5e DMG):");
            ImGui::Text("Easy %d   Medium %d   Hard %d   Deadly %d", dr.thresholds[0], dr.thresholds[1],
                        dr.thresholds[2], dr.thresholds[3]);
            ImGui::Text("Monster XP %d  x%.1f multiplier = %d", dr.baseXp, dr.multiplier, dr.adjustedXp);
            ImGui::Text("XP award: %d total, %d per player", dr.baseXp, dr.xpPerPlayer);
            ImGui::EndTooltip();
        }
    } else {
        ImGui::PushTextWrapPos(0);
        ImGui::TextDisabled("Difficulty: set player levels and monster CRs");
        ImGui::PopTextWrapPos();
    }
}

void App::drawCombatTab() {
    const float S = UI_SCALE;
    ImGui::BeginChild("CombatBody", ImVec2(0, 0), false);

    beginSection("Amount");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2 * S);
    ImGui::InputInt("##amount", &actionAmount, 1, 5);
    actionAmount = std::clamp(actionAmount, 0, 9999);
    {
        const char* numbers[] = {"1", "5", "10", "20"};
        const int values[] = {1, 5, 10, 20};
        const float w4 = rowButtonWidth(4);
        for (int i = 0; i < 4; i++) {
            if (i > 0) ImGui::SameLine(0, 8 * S);
            if (quietButton(numbers[i], ImVec2(w4, 0))) actionAmount = values[i];
        }
    }
    endSection();

    // ---- Selection / area actions.
    const std::vector<CardRef> sel = board.selectedRefs();
    beginSection("Selection", sel.empty() ? std::string() : std::to_string(sel.size()) + " selected");
    if (sel.empty()) {
        ImGui::PushTextWrapPos(0);
        ImGui::TextDisabled("Click a card to select it. Shift/Cmd-click to add more, or drag a box on the empty board.");
        ImGui::PopTextWrapPos();
        if (primaryButton("Select all", ImVec2(rowButtonWidth(1), 0))) board.selectAll();
    } else {
        const float w2 = rowButtonWidth(2);
        if (dangerButton("Damage all", ImVec2(w2, 0))) {
            pushUndo();
            if (sel.size() > 1) addLog("Area damage " + std::to_string(actionAmount) + " to " + std::to_string(sel.size()) + " targets", COL_RED);
            for (CardRef r : sel) applyHit(r, actionAmount);
        }
        ImGui::SameLine(0, 10 * S);
        const int half = actionAmount / 2;
        const std::string halfLabel = "Half damage (" + std::to_string(half) + ")";
        if (dangerButton(halfLabel.c_str(), ImVec2(w2, 0), half > 0)) {
            pushUndo();
            addLog("Half damage (saved) " + std::to_string(half) + " to " + std::to_string(sel.size()) + " target(s)", COL_RED);
            for (CardRef r : sel) applyHit(r, half);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("For targets that made their save: half the amount, rounded down.");
        }
        if (greenButton("Heal all", ImVec2(w2, 0))) {
            pushUndo();
            for (CardRef r : sel) applyHeal(r, actionAmount);
        }
        ImGui::SameLine(0, 10 * S);
        if (tempButton("Give temp HP", ImVec2(w2, 0), actionAmount > 0)) {
            pushUndo();
            for (CardRef r : sel) applyTemp(r, actionAmount);
        }

        ImGui::SetNextItemWidth((ImGui::GetContentRegionAvail().x - 10 * S) * 0.55f);
        ImGui::Combo("##cond", &selCondition, STANDARD_CONDITIONS, STANDARD_CONDITION_COUNT);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2 * S);
        ImGui::InputInt("##rounds", &selRounds, 1, 1);
        selRounds = std::clamp(selRounds, 0, 999);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rounds (0 = until removed)");
        if (primaryButton("Apply condition", ImVec2(w2, 0))) {
            pushUndo();
            for (CardRef r : sel) encounter.at(r.side, r.index)->addCondition(STANDARD_CONDITIONS[selCondition], selRounds);
            addLog(std::string(STANDARD_CONDITIONS[selCondition]) + " applied to " + std::to_string(sel.size()) + " target(s)", COL_AMBER);
        }
        ImGui::SameLine(0, 10 * S);
        if (quietButton("Remove it", ImVec2(w2, 0))) {
            pushUndo();
            for (CardRef r : sel) encounter.at(r.side, r.index)->removeCondition(STANDARD_CONDITIONS[selCondition]);
        }
        if (quietButton("Clear conds.", ImVec2(w2, 0))) {
            pushUndo();
            for (CardRef r : sel) encounter.at(r.side, r.index)->clearConditions();
        }
        ImGui::SameLine(0, 10 * S);
        if (violetButton("Toggle conc.", ImVec2(w2, 0))) {
            pushUndo();
            for (CardRef r : sel) {
                Creature* c = encounter.at(r.side, r.index);
                c->setConcentrating(!c->isConcentrating());
            }
        }
        if (quietButton("Duplicate", ImVec2(w2, 0))) duplicate(sel);
        ImGui::SameLine(0, 10 * S);
        if (dangerButton("Delete", ImVec2(w2, 0))) deleteRefs(sel, false);
        if (quietButton("Deselect", ImVec2(rowButtonWidth(1), 0))) board.clearSelection();
    }
    endSection();

    // ---- Rounds.
    if (board.inCombat) {
        beginSection("Combat", "round " + std::to_string(board.round));
        if (primaryButton("Next turn", ImVec2(rowButtonWidth(1), 0))) nextTurn();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Passes the turn to the next combatant (Space).\nAfter the last one a new round starts.");
        }
        const std::string turnName = board.turnValid && encounter.at(board.turn.side, board.turn.index)
            ? encounter.at(board.turn.side, board.turn.index)->getName() : std::string("-");
        ImGui::TextDisabled("Turn: %s", turnName.c_str());
        if (dangerButton("End combat", ImVec2(rowButtonWidth(1), 0))) endCombat();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("The enemies leave; the party stays, with the HP it has now.");
        endSection();
    } else {
        beginSection("Combat", "setup");
        if (primaryButton("Start combat", ImVec2(rowButtonWidth(1), 0), anyoneStanding())) startCombat();
        ImGui::PushTextWrapPos(0);
        ImGui::TextDisabled("Add characters with a right-click on the board and drag them into turn order.");
        ImGui::PopTextWrapPos();
        endSection();
    }

    // ---- Board view.
    beginSection("Board");
    {
        int percent = static_cast<int>(std::lround(g_cardScale * 100));
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80 * S);
        if (ImGui::SliderInt("Card size", &percent, 60, 100, "%d%%")) {
            g_cardScale = static_cast<float>(percent) / 100.0f;
            saveSettings();
        }
    }
    endSection();

    // ---- Encounter.
    beginSection("Encounter");
    {
        const float w2 = rowButtonWidth(2);
        {
            const std::string label = playerServer.running()
                ? "Player screens: " + std::to_string(playerServer.viewers()) + " connected##share"
                : std::string("Player screens (QR)##share");
            if (quietButton(label.c_str(), ImVec2(rowButtonWidth(1), 0))) openShare = true;
        }
        if (!board.inCombat) {
            if (quietButton("New encounter...", ImVec2(rowButtonWidth(1), 0))) openNewEncounter = true;
        }

        if (quietButton("Save", ImVec2(w2, 0))) {
            statusMsg = writeFile(path, snapshot()) ? "Saved." : "Could not save.";
        }
        ImGui::SameLine(0, 10 * S);
        if (quietButton("Load", ImVec2(w2, 0))) {
            const std::string before = snapshot();
            std::string text;
            if (readFile(path, text) && deserializeBoard(text, encounter, board)) {
                pushUndoState(before);
                statusMsg = "Loaded.";
            } else {
                statusMsg = "Nothing to load.";
            }
        }
        const std::string undoLabel = "Undo (" + std::to_string(history.undoCount()) + ")";
        const std::string redoLabel = "Redo (" + std::to_string(history.redoCount()) + ")";
        if (quietButton(undoLabel.c_str(), ImVec2(w2, 0), history.canUndo())) doUndo();
        ImGui::SameLine(0, 10 * S);
        if (quietButton(redoLabel.c_str(), ImVec2(w2, 0), history.canRedo())) doRedo();
        ImGui::TextDisabled("%s", statusMsg.empty() ? "Autosaves while you play." : statusMsg.c_str());
    }
    endSection();
    finishTabBody();
    ImGui::EndChild();
}

void App::drawLibraryTab() {
    const float S = UI_SCALE;
    ImGui::BeginChild("AddBody", ImVec2(0, 0), false);
    beginSection("Party", std::to_string(party.size()));
    if (party.empty()) {
        ImGui::PushTextWrapPos(0);
        ImGui::TextDisabled("No players saved yet. Right-click a player's card > Save to party, "
                            "or tick \"Also save to the party\" in New character.");
        ImGui::PopTextWrapPos();
    } else {
        int removeMember = -1;
        // Name with its details below it (the sidebar is too narrow for a details column).
        if (ImGui::BeginTable("party", 3, ImGuiTableFlags_BordersInnerH)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Add", ImGuiTableColumnFlags_WidthFixed, std::max(smallButtonWidth(), ImGui::CalcTextSize("in").x));
            ImGui::TableSetupColumn("Del", ImGuiTableColumnFlags_WidthFixed, smallButtonWidth());
            for (int i = 0; i < static_cast<int>(party.size()); i++) {
                const PlayerTemplate t = party[i];
                ImGui::PushID(i);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(t.name.c_str());
                ImGui::PushFont(nullptr, ImGui::GetCurrentContext()->FontSizeBase * 0.88f);
                ImGui::TextDisabled("Lv %d \xC2\xB7 %d HP \xC2\xB7 AC %d", t.level, t.maxHp, t.ac);
                ImGui::PopFont();
                ImGui::TableNextColumn();
                if (onBoard(t.name)) {
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextDisabled("in");
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Already on the board");
                } else {
                    if (ImGui::SmallButton("+")) addPartyMembers({t});
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add %s", t.name.c_str());
                }
                ImGui::TableNextColumn();
                if (ImGui::SmallButton("x")) removeMember = i;
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remove from the party");
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (removeMember >= 0) {
            party.erase(party.begin() + removeMember);
            saveParty();
        }
        const std::vector<PlayerTemplate> missing = partyMissing();
        const std::string label = missing.empty() ? std::string("The whole party is here##lib")
                                                  : "Add the party (" + std::to_string(missing.size()) + ")##lib";
        if (primaryButton(label.c_str(), ImVec2(rowButtonWidth(1), 0), !missing.empty())) addPartyMembers(missing);
    }
    endSection();

    beginSection("Monster library", std::to_string(library.size()));
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 100 * S);
    ImGui::InputTextWithHint("##filter", "Search...", libraryFilter, sizeof(libraryFilter));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2 * S);
    ImGui::InputInt("##libcount", &libraryCount, 1, 1);
    libraryCount = std::clamp(libraryCount, 1, 20);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("How many to add");

    std::string filter = libraryFilter;
    std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
    int removeIndex = -1;
    if (ImGui::BeginTable("library", 3, ImGuiTableFlags_BordersInnerH)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Add", ImGuiTableColumnFlags_WidthFixed, smallButtonWidth());
        ImGui::TableSetupColumn("Del", ImGuiTableColumnFlags_WidthFixed, smallButtonWidth());
        for (int i = 0; i < static_cast<int>(library.size()); i++) {
            const MonsterTemplate& t = library[i];
            std::string lower = t.name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (!filter.empty() && lower.find(filter) == std::string::npos) continue;
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(t.name.c_str());
            ImGui::PushFont(nullptr, ImGui::GetCurrentContext()->FontSizeBase * 0.88f);
            ImGui::TextDisabled("%d HP \xC2\xB7 AC %d \xC2\xB7 CR %s", t.maxHp, t.ac, crLabel(t.cr));
            ImGui::PopFont();
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("+")) addFromTemplate(t, Side::Creatures, libraryCount);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add %d", libraryCount);
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("x")) removeIndex = i;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remove from library");
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (removeIndex >= 0) {
        library.erase(library.begin() + removeIndex);
        saveLibrary();
    }
    ImGui::PushTextWrapPos(0);
    ImGui::TextDisabled("Right-click the board to add a new character, or a card > Save to library.");
    ImGui::PopTextWrapPos();
    if (quietButton("Restore default library", ImVec2(rowButtonWidth(1), 0))) {
        library = defaultLibrary();
        saveLibrary();
    }
    endSection();
    finishTabBody();
    ImGui::EndChild();
}

void App::drawLogTab() {
    const float S = UI_SCALE;
    ImGui::BeginChild("LogTab", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
    beginSection("Combat log", std::to_string(log.size()));
    const float w2 = rowButtonWidth(2);
    if (quietButton("Copy log", ImVec2(w2, 0))) {
        std::string all;
        for (const LogEntry& e : log) all += "[R" + std::to_string(e.round) + "] " + e.text + "\n";
        ImGui::SetClipboardText(all.c_str());
        statusMsg = "Log copied to clipboard.";
    }
    ImGui::SameLine(0, 10 * S);
    if (quietButton("Clear log", ImVec2(w2, 0))) log.clear();
    // Fill the rest of the panel, leaving room for the box's bottom edge.
    const float logH = ImGui::GetContentRegionAvail().y - SECTION_PAD * S - 8 * S;
    ImGui::BeginChild("LogBody", ImVec2(0, std::max(60 * S, logH)), false);
    ImGui::PushTextWrapPos(0);
    for (const LogEntry& e : log) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        dl->AddCircleFilled(p0 + ImVec2(5 * S, ImGui::GetTextLineHeight() * 0.5f), 4 * S, e.color);
        ImGui::Indent(14 * S);
        ImGui::TextDisabled("R%d", e.round);
        ImGui::SameLine();
        ImGui::TextUnformatted(e.text.c_str());
        ImGui::Unindent(14 * S);
    }
    ImGui::PopTextWrapPos();
    if (logScrollToBottom) {
        ImGui::SetScrollHereY(1.0f);
        logScrollToBottom = false;
    }
    finishTabBody();  // the log list is its own window
    ImGui::EndChild();
    endSection();
    finishTabBody();
    ImGui::EndChild();
}
