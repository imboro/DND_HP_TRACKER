#include "app.h"

#include <algorithm>
#include <cmath>
#include <functional>

#include "board_view.h"
#include "looks.h"
#include "ui_kit.h"

// Shortcut modifier as shown in menus (the shortcuts accept Ctrl and Cmd on both).
#ifdef __APPLE__
static const char* const MOD_KEY = "Cmd";
#else
static const char* const MOD_KEY = "Ctrl";
#endif

// Context menu (right-click a card).
void App::drawCardMenu() {
    const float S = UI_SCALE;
    if (openMenu) {
        ImGui::OpenPopup("CardMenu");
        openMenu = false;
    }
    if (ImGui::BeginPopup("CardMenu")) {
        Creature* c = encounter.at(menuRef.side, menuRef.index);
        if (c == nullptr || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            ImGui::CloseCurrentPopup();
        } else {
            const std::vector<CardRef> sel = board.selectedRefs();
            const bool many = sel.size() > 1;
            ImGui::TextDisabled("%s", many ? (std::to_string(sel.size()) + " selected").c_str() : c->getName().c_str());
            ImGui::Separator();
            if (!many && board.inCombat && ImGui::MenuItem("Start turn here")) startTurnAt(menuRef);
            if (!many && ImGui::MenuItem("Edit...")) openEditor(menuRef);
            if (ImGui::MenuItem("Duplicate", (std::string(MOD_KEY) + "+D").c_str())) duplicate(sel);
            if (ImGui::MenuItem("Concentrating", nullptr, c->isConcentrating())) {
                pushUndo();
                const bool value = !c->isConcentrating();
                for (CardRef r : sel) encounter.at(r.side, r.index)->setConcentrating(value);
            }
            // Look: a swatch / glyph is drawn in the blank space at the start of each item.
            if (ImGui::BeginMenu("Color")) {
                for (int i = -1; i < PALETTE_COUNT; i++) {
                    const std::string label = std::string("      ") + (i < 0 ? "Default" : PALETTE[i].name);
                    if (ImGui::MenuItem(label.c_str(), nullptr, c->getColor() == i)) {
                        pushUndo();
                        for (CardRef r : sel) encounter.at(r.side, r.index)->setColor(i);
                    }
                    const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
                    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(mn.x + 10 * S, (mn.y + mx.y) * 0.5f), 6 * S,
                                                                PALETTE[colorIndexFor(menuRef.side, i)].col);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Icon")) {
                const ImU32 shown = creatureColor(menuRef.side, *c);
                for (int i = -1; i < ICON_COUNT; i++) {
                    const std::string label = std::string("      ") + (i < 0 ? "Default" : ICON_NAMES[i]);
                    if (ImGui::MenuItem(label.c_str(), nullptr, c->getIcon() == i)) {
                        pushUndo();
                        for (CardRef r : sel) encounter.at(r.side, r.index)->setIcon(i);
                    }
                    const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
                    const float g = 17 * S;
                    drawIcon(ImGui::GetWindowDrawList(), iconIndexFor(menuRef.side, i), ImVec2(mn.x + 2 * S, (mn.y + mx.y - g) * 0.5f), g,
                             1.6f * S, i < 0 ? COL_MUTED : shown);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Damage type")) {
                for (int m = 0; m < 4; m++) {
                    const DamageMod mod = static_cast<DamageMod>(m);
                    if (ImGui::MenuItem(damageModName(mod), nullptr, c->getDamageMod() == mod)) {
                        pushUndo();
                        for (CardRef r : sel) encounter.at(r.side, r.index)->setDamageMod(mod);
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Conditions")) {
                for (int i = 0; i < STANDARD_CONDITION_COUNT; i++) {
                    const bool has = c->hasCondition(STANDARD_CONDITIONS[i]);
                    if (ImGui::MenuItem(STANDARD_CONDITIONS[i], nullptr, has)) {
                        pushUndo();
                        for (CardRef r : sel) {
                            Creature* t = encounter.at(r.side, r.index);
                            if (has) t->removeCondition(STANDARD_CONDITIONS[i]);
                            else t->addCondition(STANDARD_CONDITIONS[i], 0);
                        }
                    }
                }
                ImGui::EndMenu();
            }
            if (!many && menuRef.side == Side::Players && ImGui::MenuItem("Save to party")) saveToParty(*c);
            if (!many && menuRef.side == Side::Creatures && ImGui::MenuItem("Save to library")) {
                MonsterTemplate t;
                int n = 0;
                t.name = splitNumberedName(c->getName(), &n);
                t.maxHp = c->getMaxHp();
                t.ac = c->getAc();
                t.speed = c->getSpeed();
                t.pp = c->getPassivePerception();
                t.cr = c->getCrIndex();
                t.color = c->getColor();
                t.icon = c->getIcon();
                t.mod = c->getDamageMod();
                library.push_back(t);
                saveLibrary();
                statusMsg = "Saved " + t.name + " to the library.";
            }
            ImGui::Separator();
            if (ImGui::MenuItem(many ? "Delete selected" : "Delete", "Del")) deleteRefs(sel, false);
        }
        ImGui::EndPopup();
    }
}

// Board menu (right-click on empty space): New character, Add from library.
void App::drawBoardMenu() {
    if (openBoardMenu) {
        ImGui::OpenPopup("BoardMenu");
        openBoardMenu = false;
    }
    if (ImGui::BeginPopup("BoardMenu")) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) ImGui::CloseCurrentPopup();
        if (ImGui::MenuItem("New character...")) {
            openNewChar = true;
            newCharFocus = true;
            newCharSlot = boardMenuSlot;
            newName[0] = '\0';
            newColor = -1;
            newIcon = -1;
        }
        {
            const std::vector<PlayerTemplate> missing = partyMissing();
            const std::string label = "Add the party (" + std::to_string(missing.size()) + ")";
            if (ImGui::MenuItem(label.c_str(), nullptr, false, !missing.empty())) {
                const std::vector<CardRef> added = addPartyMembers(missing);
                if (boardMenuSlot >= 0) {
                    for (size_t k = 0; k < added.size(); k++) board.placeInTurnOrder(added[k], boardMenuSlot + static_cast<int>(k));
                }
            }
        }
        if (ImGui::BeginMenu("Add from party", !party.empty())) {
            for (int i = 0; i < static_cast<int>(party.size()); i++) {
                const PlayerTemplate t = party[i];
                const std::string info = "Lv " + std::to_string(t.level) + "  " + std::to_string(t.maxHp) + " HP";
                if (ImGui::MenuItem((t.name + "##party" + std::to_string(i)).c_str(), info.c_str(), false, !onBoard(t.name))) {
                    const std::vector<CardRef> added = addPartyMembers({t});
                    if (boardMenuSlot >= 0) {
                        for (size_t k = 0; k < added.size(); k++) board.placeInTurnOrder(added[k], boardMenuSlot + static_cast<int>(k));
                    }
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Add from library")) {
            for (int i = 0; i < static_cast<int>(library.size()); i++) {
                const MonsterTemplate t = library[i];
                const std::string info = std::to_string(t.maxHp) + " HP  AC " + std::to_string(t.ac);
                if (ImGui::MenuItem((t.name + "##lib" + std::to_string(i)).c_str(), info.c_str())) {
                    const std::vector<CardRef> added = addFromTemplate(t, Side::Creatures, 1);
                    if (boardMenuSlot >= 0) {
                        for (size_t k = 0; k < added.size(); k++) board.placeInTurnOrder(added[k], boardMenuSlot + static_cast<int>(k));
                    }
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
}

// Player screens: the QR code the phones scan to follow the fight.
void App::drawShareWindow() {
    const float S = UI_SCALE;
    if (openShare) {
        ImGui::OpenPopup("Player screens");
        openShare = false;
        if (playerServer.running()) {  // the network may have changed since it started
            shareUrl = playerServer.url();
            makeQr(shareUrl);
        }
    }
    // Centered every frame: the window grows when the QR code appears, and it should
    // stay in the middle of the screen rather than grow downward.
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16 * S);
    const bool shareOpen = ImGui::BeginPopupModal("Player screens", nullptr,
                                                  ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
    ImGui::PopStyleVar(2);
    if (shareOpen) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) ImGui::CloseCurrentPopup();
        const float w = 340 * S;
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + w);
        if (!playerServer.running()) {
            ImGui::TextUnformatted("Let the players follow the fight on their phones: their character's HP and "
                                   "stats, the turn order and whose turn it is.");
            ImGui::Dummy(ImVec2(0, 2 * S));
            ImGui::TextDisabled("The phones must be on the same Wi-Fi as this computer. They can only look; "
                                "enemies show up by name only.");
            if (!shareError.empty()) ImGui::TextColored(toVec4(COL_RED), "%s", shareError.c_str());
            ImGui::Dummy(ImVec2(0, 4 * S));
            if (primaryButton("Start sharing", ImVec2(w, 0))) startSharing();
        } else {
            // The QR code: black modules on white, with a quiet zone, snapped to whole points.
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const int quiet = 3;
            const float m = std::floor(w / static_cast<float>(qrSize + quiet * 2));
            const float box = m * static_cast<float>(qrSize + quiet * 2);
            const ImVec2 p0 = ImGui::GetCursorScreenPos() + ImVec2(std::floor((w - box) * 0.5f), 0);
            dl->AddRectFilled(p0, p0 + ImVec2(box, box), COL_WHITE, 10 * S);
            dl->AddRect(p0, p0 + ImVec2(box, box), COL_INK, 10 * S, 0, 2 * S);
            for (int y = 0; y < qrSize; y++) {
                for (int x = 0; x < qrSize; x++) {
                    if (!qrModules[static_cast<size_t>(y * qrSize + x)]) continue;
                    const ImVec2 a = p0 + ImVec2(static_cast<float>(x + quiet) * m, static_cast<float>(y + quiet) * m);
                    dl->AddRectFilled(a, a + ImVec2(m, m), IM_COL32(0, 0, 0, 255));
                }
            }
            ImGui::Dummy(ImVec2(w, box + 4 * S));
            const ImVec2 us = boldTextSize(shareUrl.c_str());
            const ImVec2 up = ImGui::GetCursorScreenPos() + ImVec2(std::max(0.0f, (w - us.x) * 0.5f), 0);
            boldText(dl, up, COL_TEXT, shareUrl.c_str());
            ImGui::Dummy(ImVec2(w, us.y + 2 * S));
            const int phones = playerServer.viewers();
            const std::string connected = phones == 0 ? std::string("No phones connected yet")
                                        : std::to_string(phones) + (phones == 1 ? " phone connected" : " phones connected");
            const ImVec2 cs = ImGui::CalcTextSize(connected.c_str());
            const ImVec2 cp = ImGui::GetCursorScreenPos() + ImVec2(std::max(0.0f, (w - cs.x - 14 * S) * 0.5f), 0);
            dl->AddCircleFilled(cp + ImVec2(4 * S, cs.y * 0.5f), 4 * S, phones > 0 ? COL_GREEN : COL_GRAY);
            dl->AddText(cp + ImVec2(14 * S, 0), phones > 0 ? COL_TEXT : COL_MUTED, connected.c_str());
            ImGui::Dummy(ImVec2(w, cs.y + 4 * S));
            ImGui::TextDisabled("Scan it with the phone's camera. The phones must be on the same Wi-Fi as "
                                "this computer.");
            // Chat apps (WhatsApp...) don't turn a bare network address into a link.
            ImGui::TextDisabled("Sending the link in a chat? It won't be clickable there: copy it into "
                                "the phone's browser instead.");
            ImGui::Dummy(ImVec2(0, 4 * S));
            const float w2 = (w - 10 * S) * 0.5f;
            if (quietButton("Copy link", ImVec2(w2, 0))) ImGui::SetClipboardText(shareUrl.c_str());
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Paste it into the phone's browser address bar");
            ImGui::SameLine(0, 10 * S);
            if (dangerButton("Stop sharing", ImVec2(w2, 0))) {
                playerServer.stop();
                addLog("Player screens off", COL_MUTED);
            }
        }
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(0, 2 * S));
        if (quietButton("Close", ImVec2(w, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

// New character (from the board menu).
void App::drawNewCharacterWindow() {
    const float S = UI_SCALE;
    if (openNewChar) {
        ImGui::OpenPopup("New character");
        openNewChar = false;
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16 * S);
    const bool newCharOpen = ImGui::BeginPopupModal("New character", nullptr,
                                                    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
    ImGui::PopStyleVar(2);
    if (newCharOpen) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) ImGui::CloseCurrentPopup();
        const float fieldW = 150 * S;
        if (newCharFocus) {
            ImGui::SetKeyboardFocusHere();
            newCharFocus = false;
        }
        ImGui::SetNextItemWidth(260 * S);
        const bool enterPressed = ImGui::InputText("Name", newName, sizeof(newName), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::RadioButton("Player", &newSide, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Creature", &newSide, 1);
        lookPicker(&newColor, &newIcon, newSide == 0 ? Side::Players : Side::Creatures, S);
        ImGui::SetNextItemWidth(fieldW);
        ImGui::InputInt("Max HP", &newMaxHp);
        newMaxHp = std::max(1, newMaxHp);
        ImGui::SetNextItemWidth(fieldW);
        ImGui::InputInt("AC", &newAc);
        newAc = std::clamp(newAc, 0, 40);
        if (newSide == 0) {
            ImGui::SetNextItemWidth(fieldW);
            ImGui::InputInt("Level", &newLevel);
            newLevel = std::clamp(newLevel, 1, 20);
            ImGui::Checkbox("Also save to the party", &newSaveParty);
        } else {
            ImGui::SetNextItemWidth(fieldW);
            crCombo("CR", &newCr);
            ImGui::Checkbox("Also save to the monster library", &newSaveLib);
        }
        ImGui::SetNextItemWidth(fieldW);
        ImGui::InputInt("How many", &newCount);
        newCount = std::clamp(newCount, 1, 20);
        ImGui::Dummy(ImVec2(0, 4 * S));
        const bool canCreate = newName[0] != '\0';
        const float bw = 120 * S;
        bool create = primaryButton("Create", ImVec2(bw, 0), canCreate);
        ImGui::SameLine(0, 12 * S);
        const bool cancel = quietButton("Cancel", ImVec2(bw, 0));
        if (enterPressed && canCreate) create = true;
        if (create) {
            pushUndo();
            const Side side = newSide == 0 ? Side::Players : Side::Creatures;
            board.clearSelection();
            const std::vector<std::string> names = makeNumberedNames(encounter, newName, newCount);
            std::vector<CardRef> added;
            for (size_t i = 0; i < names.size(); i++) {
                Creature c(names[i], newMaxHp);
                c.setAc(newAc);
                c.setColor(newColor);
                c.setIcon(newIcon);
                if (side == Side::Players) c.setLevel(newLevel);
                else c.setCrIndex(newCr);
                const CardRef r = addCombatant(encounter, board, side, c);
                board.get(r).selected = true;
                board.get(r).spawnT = -0.06f * static_cast<float>(i);  // staggered fade-in
                added.push_back(r);
            }
            addLog("Added " + std::to_string(newCount) + " x " + newName, COL_MUTED);
            if (side == Side::Creatures && newSaveLib) {
                MonsterTemplate t;
                t.name = newName;
                t.maxHp = newMaxHp;
                t.ac = newAc;
                t.cr = newCr;
                t.color = newColor;
                t.icon = newIcon;
                library.push_back(t);
                saveLibrary();
                statusMsg = std::string("Saved ") + newName + " to the library.";
            }
            if (side == Side::Players && newSaveParty) {
                Creature saved(newName, newMaxHp);
                saved.setAc(newAc);
                saved.setLevel(newLevel);
                saved.setColor(newColor);
                saved.setIcon(newIcon);
                saveToParty(saved);
            }
            if (newCharSlot >= 0) {
                for (size_t i = 0; i < added.size(); i++) board.placeInTurnOrder(added[i], newCharSlot + static_cast<int>(i));
            }
            newName[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        if (cancel) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

// Edit window (double-click a card).
void App::drawEditWindow() {
    const float S = UI_SCALE;
    if (openEdit) {
        ImGui::OpenPopup("Edit combatant");
        openEdit = false;
    }
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(560 * S, display.x - 40 * S), std::min(700 * S, display.y - 40 * S)),
                             ImGuiCond_Appearing);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16 * S);
    const bool editOpen = ImGui::BeginPopupModal("Edit combatant", nullptr, ImGuiWindowFlags_NoMove);
    ImGui::PopStyleVar(2);
    if (editOpen) {
        if (!editCopy || encounter.at(editRef.side, editRef.index) == nullptr ||
            ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            ImGui::CloseCurrentPopup();
        } else {
            Creature& e = *editCopy;
            const bool isPlayer = editRef.side == Side::Players;
            // Fields scroll; the Apply/Cancel footer below always stays visible.
            const float footerH = ImGui::GetFrameHeight() + 22 * S;
            ImGui::BeginChild("EditBody", ImVec2(0, -footerH), false);
            const float half = (ImGui::GetContentRegionAvail().x - 10 * S) * 0.5f;
            const float fieldW = half - 120 * S;
            auto intField = [&](const char* label, int value, int lo, int hi, const std::function<void(int)>& set) {
                ImGui::SetNextItemWidth(fieldW);
                int v = value;
                if (ImGui::InputInt(label, &v)) set(std::clamp(v, lo, hi));
            };

            ImGui::TextDisabled("%s", isPlayer ? "Player" : "Enemy");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 70 * S);
            ImGui::InputText("Name", editName, sizeof(editName));

            ImGui::SeparatorText("Look");
            {
                int color = e.getColor(), icon = e.getIcon();
                if (lookPicker(&color, &icon, editRef.side, S)) {
                    e.setColor(color);
                    e.setIcon(icon);
                }
            }

            ImGui::SeparatorText("Hit points");
            intField("Max HP", e.getMaxHp(), 1, 99999, [&](int v) { e.setMaxHp(v); });
            ImGui::SameLine(half + 10 * S);
            intField("Current HP", e.getCurrentHp(), 0, e.getMaxHp(), [&](int v) { e.setHp(v); });
            intField("Temp HP", e.getTempHp(), 0, 99999, [&](int v) { e.setTempHp(v); });
            ImGui::SameLine(half + 10 * S);
            {
                DamageMod mod = e.getDamageMod();
                ImGui::SetNextItemWidth(fieldW);
                if (damageModCombo("Damage", &mod)) e.setDamageMod(mod);
            }

            ImGui::SeparatorText("Quick reference");
            intField("AC", e.getAc(), 0, 40, [&](int v) { e.setAc(v); });
            ImGui::SameLine(half + 10 * S);
            intField("Speed", e.getSpeed(), 0, 500, [&](int v) { e.setSpeed(v); });
            intField("Passive Perc.", e.getPassivePerception(), 0, 40, [&](int v) { e.setPassivePerception(v); });
            ImGui::SameLine(half + 10 * S);
            if (isPlayer) {
                intField("Level", e.getLevel(), 1, 20, [&](int v) { e.setLevel(v); });
            } else {
                int cr = e.getCrIndex();
                ImGui::SetNextItemWidth(fieldW);
                if (crCombo("CR", &cr)) e.setCrIndex(cr);
            }

            ImGui::SeparatorText("Status");
            {
                bool conc = e.isConcentrating();
                if (ImGui::Checkbox("Concentrating on a spell", &conc)) e.setConcentrating(conc);
            }
            if (isPlayer) {
                const int s = e.getDeathSuccesses(), f = e.getDeathFailures();
                intField("Death successes", s, 0, 3, [&](int v) { e.setDeathSaves(v, f); });
                ImGui::SameLine(half + 10 * S);
                intField("Death failures", f, 0, 3, [&](int v) { e.setDeathSaves(s, v); });
            }

            ImGui::SeparatorText("Conditions");
            if (ImGui::BeginTable("conds", 3)) {
                for (int i = 0; i < STANDARD_CONDITION_COUNT; i++) {
                    ImGui::TableNextColumn();
                    bool has = e.hasCondition(STANDARD_CONDITIONS[i]);
                    if (ImGui::Checkbox(STANDARD_CONDITIONS[i], &has)) {
                        if (has) e.addCondition(STANDARD_CONDITIONS[i], 0);
                        else e.removeCondition(STANDARD_CONDITIONS[i]);
                    }
                }
                ImGui::EndTable();
            }
            ImGui::SetNextItemWidth(half);
            ImGui::InputTextWithHint("##custom", "Custom condition (e.g. Hexed)", editCustomCond, sizeof(editCustomCond));
            ImGui::SameLine();
            if (ImGui::Button("Add condition") && editCustomCond[0] != '\0') {
                e.addCondition(editCustomCond, 0);
                editCustomCond[0] = '\0';
            }
            if (!e.getConditions().empty()) {
                ImGui::TextDisabled("Duration in rounds (0 = until removed):");
                std::vector<Condition> conds = e.getConditions();
                for (const Condition& cond : conds) {
                    ImGui::PushID(cond.name.c_str());
                    ImGui::SetNextItemWidth(fieldW);
                    int rounds = cond.rounds;
                    if (ImGui::InputInt(cond.name.c_str(), &rounds)) e.addCondition(cond.name, std::clamp(rounds, 0, 999));
                    ImGui::SameLine();
                    if (ImGui::SmallButton("remove")) e.removeCondition(cond.name);
                    ImGui::PopID();
                }
            }

            ImGui::SeparatorText("Notes");
            ImGui::InputTextMultiline("##notes", editNotes, sizeof(editNotes),
                                      ImVec2(ImGui::GetContentRegionAvail().x - 4 * S, 80 * S));
            ImGui::EndChild();

            ImGui::Separator();
            const float bw = 110 * S;
            if (greenButton("Apply", ImVec2(bw, 0))) {
                pushUndo();
                if (editName[0] != '\0') e.rename(editName);
                e.setNotes(editNotes);
                *encounter.at(editRef.side, editRef.index) = e;
                addLog("Edited " + e.getName(), COL_MUTED);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine(0, 14 * S);
            if (quietButton("Cancel", ImVec2(bw, 0))) ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// New encounter (keeps players by default: HP carries over between fights).
void App::drawNewEncounterWindow() {
    const float S = UI_SCALE;
    if (openNewEncounter) {
        ImGui::OpenPopup("New encounter");
        openNewEncounter = false;
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16 * S);
    const bool newOpen = ImGui::BeginPopupModal("New encounter", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
    ImGui::PopStyleVar(2);
    if (newOpen) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) ImGui::CloseCurrentPopup();
        ImGui::TextUnformatted("Start a new encounter? (Undo can bring the old one back.)");
        ImGui::Dummy(ImVec2(0, 4 * S));
        const float bw = 200 * S;
        if (primaryButton("Remove enemies, keep party", ImVec2(bw * 1.3f, 0))) {
            pushUndo();
            std::vector<CardRef> enemies;
            for (int i = 0; i < static_cast<int>(board.creatures.size()); i++) enemies.push_back({Side::Creatures, i});
            removeCombatants(encounter, board, enemies);
            encounter.resume();
            board.round = 1;
            alerts.clear();
            addLog("=== New encounter ===", COL_MUTED);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0, 12 * S);
        if (dangerButton("Remove everything", ImVec2(bw, 0))) {
            pushUndo();
            encounter.clear();
            board = BoardState();
            alerts.clear();
            addLog("=== New encounter (empty) ===", COL_MUTED);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0, 12 * S);
        if (quietButton("Cancel", ImVec2(bw * 0.6f, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}
