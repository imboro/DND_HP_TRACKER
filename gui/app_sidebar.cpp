#include "app.h"

#include <algorithm>
#include <cctype>

#include "imgui_internal.h"
#include "board_view.h"
#include "difficulty.h"
#include "i18n.h"
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

    // Headline, with the language switch (EN / ES / CA) at its right.
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const float mul = 1.6f;  // leaves room for the language switch
        heavyText(dl, p0, COL_TEXT, "HP Tracker", mul);
        const float titleH = heavyTextSize("HP Tracker", mul).y;
        static const char* const LANG_LABELS[] = {"EN", "ES", "CA"};
        const float segW = 98 * S, segH = 26 * S;
        const float right = p0.x + ImGui::GetContentRegionAvail().x - 4 * S;
        ImGui::SetCursorScreenPos(ImVec2(right - segW, p0.y + (titleH - segH) * 0.5f));
        int lang = static_cast<int>(language());
        ImGui::PushID("lang");
        segmentedTabs(LANG_LABELS, LANG_COUNT, &lang, S, segW, segH);
        ImGui::PopID();
        if (lang != static_cast<int>(language())) {
            setLanguage(static_cast<Lang>(lang));
            saveSettings();
        }
        ImGui::SetCursorScreenPos(p0);
        ImGui::Dummy(ImVec2(0, titleH));
    }

    drawSummary();

    ImGui::Dummy(ImVec2(0, 2 * S));
    const char* const tabLabels[] = {T("Combat"), T("Library"), T("Log")};
    segmentedTabs(tabLabels, 3, &activeTab, S);
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
    if (statTile("##round", T("Round"), std::to_string(board.round), TileIcon::Cycle, COL_GREEN, tw, S)) {
        ImGui::SetTooltip("%s", TF("Round %d. Timed conditions count down at the start of each round.", board.round).c_str());
    }
    ImGui::SameLine(0, gap);
    if (statTile("##enemies", T("Enemies"), std::to_string(enemiesUp), TileIcon::Bolt, COL_RED, tw, S)) {
        ImGui::SetTooltip("%s", TF("%d of %d enemies standing", enemiesUp, enemyCount).c_str());
    }
    ImGui::SameLine(0, gap);
    if (statTile("##party", T("Party HP"), std::to_string(partyHp), TileIcon::Heart, COL_PLAYER, tw, S)) {
        ImGui::SetTooltip("%s", TF("Party: %d of %d up\nHP %d / %d", partyUp, partyCount, partyHp, partyMax).c_str());
    }

    // More specific than Encounter::isOver(): dying players can still be saved.
    const auto& ps = encounter.getPlayers();
    const auto& cs = encounter.getCreatures();
    const bool enemiesDown = !cs.empty() && std::all_of(cs.begin(), cs.end(), [](const Creature& c) { return c.getCurrentHp() == 0; });
    const bool partyDown = !ps.empty() && std::all_of(ps.begin(), ps.end(), [](const Creature& c) { return c.getCurrentHp() == 0; });
    const bool partyDead = partyDown && std::all_of(ps.begin(), ps.end(), [](const Creature& c) { return c.getDeathFailures() >= 3; });
    if (!board.inCombat) pill(T("Setting up"), COL_GRAY, S);
    else if (enemiesDown) pill(T("Enemies defeated"), COL_GREEN, S);
    else if (partyDead) pill(T("Party defeated"), COL_RED, S);
    else if (partyDown) pill(T("Party down!"), COL_RED, S);
    else pill(T("In progress"), COL_GREEN, S);

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
        const std::string text = std::string(T(difficultyName(dr.rating))) + "  \xC2\xB7  " +
                                 std::to_string(dr.adjustedXp) + " XP";
        // Same row as the status pill when it fits.
        const float need = ImGui::CalcTextSize(text.c_str()).x + 44 * S;
        ImGui::SameLine(0, 8 * S);
        if (ImGui::GetContentRegionAvail().x < need) ImGui::NewLine();
        pill(text.c_str(), col, S);
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(T("Party thresholds (5e):"));
            ImGui::TextUnformatted(TF("Easy %d   Medium %d   Hard %d   Deadly %d", dr.thresholds[0], dr.thresholds[1],
                                      dr.thresholds[2], dr.thresholds[3]).c_str());
            ImGui::TextUnformatted(TF("Monster XP %d  x%.1f multiplier = %d", dr.baseXp, dr.multiplier, dr.adjustedXp).c_str());
            ImGui::TextUnformatted(TF("XP award: %d total, %d per player", dr.baseXp, dr.xpPerPlayer).c_str());
            ImGui::EndTooltip();
        }
    } else {
        ImGui::PushTextWrapPos(0);
        ImGui::TextDisabled("%s", T("Difficulty: set player levels and monster CRs"));
        ImGui::PopTextWrapPos();
    }
}

void App::drawCombatTab() {
    const float S = UI_SCALE;
    ImGui::BeginChild("CombatBody", ImVec2(0, 0), false);

    beginSection(T("Amount"));
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
    beginSection(T("Selection"), sel.empty() ? std::string() : TF("%d selected", static_cast<int>(sel.size())));
    if (sel.empty()) {
        ImGui::PushTextWrapPos(0);
        ImGui::TextDisabled("%s", T("Click a card to select it. Shift/Cmd-click to add more, or drag a box on the empty board."));
        ImGui::PopTextWrapPos();
        if (primaryButton(T("Select all"), ImVec2(rowButtonWidth(1), 0))) board.selectAll();
    } else {
        const float w2 = rowButtonWidth(2);
        const int count = static_cast<int>(sel.size());
        if (dangerButton(T("Damage all"), ImVec2(w2, 0))) {
            pushUndo();
            if (count > 1) addLog(TF("Area damage %d to %d targets", actionAmount, count), COL_RED);
            for (CardRef r : sel) applyHit(r, actionAmount);
        }
        ImGui::SameLine(0, 10 * S);
        const int half = actionAmount / 2;
        const std::string halfLabel = TF("Half damage (%d)", half);
        if (dangerButton(halfLabel.c_str(), ImVec2(w2, 0), half > 0)) {
            pushUndo();
            addLog(TF("Half damage (saved) %d to %d target(s)", half, count), COL_RED);
            for (CardRef r : sel) applyHit(r, half);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", T("For targets that made their save: half the amount, rounded down."));
        }
        if (greenButton(T("Heal all"), ImVec2(w2, 0))) {
            pushUndo();
            for (CardRef r : sel) applyHeal(r, actionAmount);
        }
        ImGui::SameLine(0, 10 * S);
        if (tempButton(T("Give temp HP"), ImVec2(w2, 0), actionAmount > 0)) {
            pushUndo();
            for (CardRef r : sel) applyTemp(r, actionAmount);
        }

        // The conditions are stored in English and shown in the current language.
        const char* conditionLabels[STANDARD_CONDITION_COUNT];
        for (int i = 0; i < STANDARD_CONDITION_COUNT; i++) conditionLabels[i] = T(STANDARD_CONDITIONS[i]);
        ImGui::SetNextItemWidth((ImGui::GetContentRegionAvail().x - 10 * S) * 0.55f);
        ImGui::Combo("##cond", &selCondition, conditionLabels, STANDARD_CONDITION_COUNT);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2 * S);
        ImGui::InputInt("##rounds", &selRounds, 1, 1);
        selRounds = std::clamp(selRounds, 0, 999);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T("Rounds (0 = until removed)"));
        if (primaryButton(T("Apply condition"), ImVec2(w2, 0))) {
            pushUndo();
            for (CardRef r : sel) encounter.at(r.side, r.index)->addCondition(STANDARD_CONDITIONS[selCondition], selRounds);
            addLog(TF("%s applied to %d target(s)", T(STANDARD_CONDITIONS[selCondition]), count), COL_AMBER);
        }
        ImGui::SameLine(0, 10 * S);
        if (quietButton(T("Remove it"), ImVec2(w2, 0))) {
            pushUndo();
            for (CardRef r : sel) encounter.at(r.side, r.index)->removeCondition(STANDARD_CONDITIONS[selCondition]);
        }
        if (quietButton(T("Clear conds."), ImVec2(w2, 0))) {
            pushUndo();
            for (CardRef r : sel) encounter.at(r.side, r.index)->clearConditions();
        }
        ImGui::SameLine(0, 10 * S);
        if (violetButton(T("Toggle conc."), ImVec2(w2, 0))) {
            pushUndo();
            for (CardRef r : sel) {
                Creature* c = encounter.at(r.side, r.index);
                c->setConcentrating(!c->isConcentrating());
            }
        }
        if (quietButton(T("Duplicate"), ImVec2(w2, 0))) duplicate(sel);
        ImGui::SameLine(0, 10 * S);
        if (dangerButton(T("Delete"), ImVec2(w2, 0))) deleteRefs(sel, false);
        if (quietButton(T("Deselect"), ImVec2(rowButtonWidth(1), 0))) board.clearSelection();
    }
    endSection();

    // ---- Rounds.
    if (board.inCombat) {
        beginSection(T("Combat"), TF("round %d", board.round));
        if (primaryButton(T("Next turn"), ImVec2(rowButtonWidth(1), 0))) nextTurn();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", T("Passes the turn to the next combatant (Space).\nAfter the last one a new round starts."));
        }
        const std::string turnName = board.turnValid && encounter.at(board.turn.side, board.turn.index)
            ? encounter.at(board.turn.side, board.turn.index)->getName() : std::string("-");
        ImGui::TextDisabled("%s", TF("Turn: %s", turnName.c_str()).c_str());
        if (dangerButton(T("End combat"), ImVec2(rowButtonWidth(1), 0))) endCombat();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T("The enemies leave; the party stays, with the HP it has now."));
        endSection();
    } else {
        beginSection(T("Combat"), T("setup"));
        if (primaryButton(T("Start combat"), ImVec2(rowButtonWidth(1), 0), anyoneStanding())) startCombat();
        ImGui::PushTextWrapPos(0);
        ImGui::TextDisabled("%s", T("Add characters with a right-click on the board and drag them into turn order."));
        ImGui::PopTextWrapPos();
        endSection();
    }

    // ---- Board view.
    beginSection(T("Board"));
    {
        int percent = static_cast<int>(std::lround(g_cardScale * 100));
        const std::string sliderLabel = std::string(T("Card size")) + "##cardsize";
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(T("Card size")).x - 14 * S);
        if (ImGui::SliderInt(sliderLabel.c_str(), &percent, 60, 100, "%d%%")) {
            g_cardScale = static_cast<float>(percent) / 100.0f;
            saveSettings();
        }
    }
    endSection();

    // ---- Encounter.
    beginSection(T("Encounter"));
    {
        const float w2 = rowButtonWidth(2);
        {
            const std::string label = (playerServer.running()
                ? TF("Player screens: %d connected", playerServer.viewers())
                : std::string(T("Player screens (QR)"))) + "##share";
            if (quietButton(label.c_str(), ImVec2(rowButtonWidth(1), 0))) openShare = true;
        }
        if (!board.inCombat) {
            if (quietButton(T("New encounter..."), ImVec2(rowButtonWidth(1), 0))) openNewEncounter = true;
        }

        if (quietButton(T("Save"), ImVec2(w2, 0))) {
            statusMsg = writeFile(path, snapshot()) ? "Saved." : "Could not save.";
        }
        ImGui::SameLine(0, 10 * S);
        if (quietButton(T("Load"), ImVec2(w2, 0))) {
            const std::string before = snapshot();
            std::string text;
            if (readFile(path, text) && deserializeBoard(text, encounter, board)) {
                pushUndoState(before);
                statusMsg = "Loaded.";
            } else {
                statusMsg = "Nothing to load.";
            }
        }
        const std::string undoLabel = TF("Undo (%d)", static_cast<int>(history.undoCount())) + "##undo";
        const std::string redoLabel = TF("Redo (%d)", static_cast<int>(history.redoCount())) + "##redo";
        if (quietButton(undoLabel.c_str(), ImVec2(w2, 0), history.canUndo())) doUndo();
        ImGui::SameLine(0, 10 * S);
        if (quietButton(redoLabel.c_str(), ImVec2(w2, 0), history.canRedo())) doRedo();
        ImGui::TextDisabled("%s", statusMsg.empty() ? T("Autosaves while you play.") : TKnown(statusMsg));
    }
    endSection();
    finishTabBody();
    ImGui::EndChild();
}

void App::drawLibraryTab() {
    const float S = UI_SCALE;
    ImGui::BeginChild("AddBody", ImVec2(0, 0), false);
    beginSection(T("Party"), std::to_string(party.size()));
    if (party.empty()) {
        ImGui::PushTextWrapPos(0);
        ImGui::TextDisabled("%s", T("No players saved yet. Right-click a player's card > Save to party, "
                                    "or tick \"Also save to the party\" in New character."));
        ImGui::PopTextWrapPos();
    } else {
        int removeMember = -1;
        // Name with its details below it (the sidebar is too narrow for a details column).
        if (ImGui::BeginTable("party", 3, ImGuiTableFlags_BordersInnerH)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Add", ImGuiTableColumnFlags_WidthFixed, std::max(smallButtonWidth(), ImGui::CalcTextSize(T("in")).x));
            ImGui::TableSetupColumn("Del", ImGuiTableColumnFlags_WidthFixed, smallButtonWidth());
            for (int i = 0; i < static_cast<int>(party.size()); i++) {
                const PlayerTemplate t = party[i];
                ImGui::PushID(i);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(t.name.c_str());
                ImGui::PushFont(nullptr, ImGui::GetCurrentContext()->FontSizeBase * 0.88f);
                ImGui::TextDisabled("%s", TF("Lv %d \xC2\xB7 %d HP \xC2\xB7 AC %d", t.level, t.maxHp, t.ac).c_str());
                ImGui::PopFont();
                ImGui::TableNextColumn();
                if (onBoard(t.name)) {
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextDisabled("%s", T("in"));
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T("Already on the board"));
                } else {
                    if (ImGui::SmallButton("+")) addPartyMembers({t});
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TF("Add %s", t.name.c_str()).c_str());
                }
                ImGui::TableNextColumn();
                if (ImGui::SmallButton("x")) removeMember = i;
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T("Remove from the party"));
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (removeMember >= 0) {
            party.erase(party.begin() + removeMember);
            saveParty();
        }
        const std::vector<PlayerTemplate> missing = partyMissing();
        const std::string label = (missing.empty() ? std::string(T("The whole party is here"))
                                                   : TF("Add the party (%d)", static_cast<int>(missing.size()))) + "##lib";
        if (primaryButton(label.c_str(), ImVec2(rowButtonWidth(1), 0), !missing.empty())) addPartyMembers(missing);
    }
    endSection();

    beginSection(T("Monster library"), std::to_string(library.size()));
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 100 * S);
    ImGui::InputTextWithHint("##filter", T("Search..."), libraryFilter, sizeof(libraryFilter));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2 * S);
    ImGui::InputInt("##libcount", &libraryCount, 1, 1);
    libraryCount = std::clamp(libraryCount, 1, 20);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T("How many to add"));

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
            ImGui::TextDisabled("%s", TF("%d HP \xC2\xB7 AC %d \xC2\xB7 CR %s", t.maxHp, t.ac, crLabel(t.cr)).c_str());
            ImGui::PopFont();
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("+")) addFromTemplate(t, Side::Creatures, libraryCount);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TF("Add %d", libraryCount).c_str());
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("x")) removeIndex = i;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T("Remove from library"));
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (removeIndex >= 0) {
        library.erase(library.begin() + removeIndex);
        saveLibrary();
    }
    ImGui::PushTextWrapPos(0);
    ImGui::TextDisabled("%s", T("Right-click the board to add a new character, or a card > Save to library."));
    ImGui::PopTextWrapPos();
    if (quietButton(T("Restore default library"), ImVec2(rowButtonWidth(1), 0))) {
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
    beginSection(T("Combat log"), std::to_string(log.size()));
    const float w2 = rowButtonWidth(2);
    if (quietButton(T("Copy log"), ImVec2(w2, 0))) {
        std::string all;
        for (const LogEntry& e : log) all += "[R" + std::to_string(e.round) + "] " + e.text + "\n";
        ImGui::SetClipboardText(all.c_str());
        statusMsg = "Log copied to clipboard.";
    }
    ImGui::SameLine(0, 10 * S);
    if (quietButton(T("Clear log"), ImVec2(w2, 0))) log.clear();
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
