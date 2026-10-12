#include "app.h"

#include <algorithm>
#include <cstdio>

#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "qrcodegen.hpp"
#include "board_view.h"
#include "player_state.h"
#include "save_format.h"
#include "ui_kit.h"

// Mouse wheel / trackpad scrolling is scaled by this (1.0 = ImGui's default speed).
// Mac trackpads send many small steps; a Windows wheel sends whole notches.
#ifdef _WIN32
static const float WHEEL_SENSITIVITY = 0.8f;
#else
static const float WHEEL_SENSITIVITY = 0.35f;
#endif

App::App(SDL_Window* window, SDL_Renderer* renderer)
    : window(window),
      renderer(renderer),
      path(savePath("encounter.txt")),
      libraryPath(savePath("library.txt")),
      partyPath(savePath("party.txt")),
      settingsPath(savePath("settings.txt")) {
    // The last session, or a small example encounter the first time.
    {
        std::string text;
        if (readFile(path, text) && deserializeBoard(text, encounter, board)) {
            statusMsg = "Loaded last session.";
        } else {
            Creature hero("Hero", 40);
            hero.setAc(16);
            hero.setLevel(3);
            addCombatant(encounter, board, Side::Players, hero);
            for (const MonsterTemplate& t : defaultLibrary()) {
                if (t.name == "Goblin" || t.name == "Orc") {
                    addCombatant(encounter, board, Side::Creatures, creatureFromTemplate(t));
                }
            }
        }
    }
    // The monster library (the default one the first time) and the party.
    {
        std::string text;
        if (!readFile(libraryPath, text) || !deserializeLibrary(text, library)) {
            library = defaultLibrary();
        }
    }
    {
        std::string text;
        if (readFile(partyPath, text)) deserializeParty(text, party);
    }
    // View settings (not part of the encounter, so undo doesn't touch them).
    {
        std::string text;
        if (readFile(settingsPath, text) && text.rfind("cardScale=", 0) == 0) {
            try {
                g_cardScale = std::clamp(std::stof(text.substr(10)), 0.6f, 1.0f);
            } catch (const std::exception&) {
            }
        }
    }
}

void App::run() {
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_MOUSEWHEEL) {
                // Trackpads send lots of small wheel steps; slow scrolling down.
                event.wheel.preciseX *= WHEEL_SENSITIVITY;
                event.wheel.preciseY *= WHEEL_SENSITIVITY;
            }
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        // The bold and heavy faces are only drawn through ImDrawList::AddText and are
        // never made the current font, so ImGui doesn't hand them the screen's pixel
        // density: they would be rasterized at 1x and stretched (blurry on Retina).
        if (g_fontBold) g_fontBold->CurrentRasterizerDensity = ImGui::GetPixelDensity();
        if (g_fontHeavy) g_fontHeavy->CurrentRasterizerDensity = ImGui::GetPixelDensity();

        drawFrame();
        publishPlayerState();

        if (ImGui::GetTime() - lastAutosave > 3.0) {
            writeFile(path, snapshot());
            lastAutosave = ImGui::GetTime();
        }

        ImGui::Render();
        // On Retina displays ImGui works in points but the renderer in pixels;
        // without this the drawn UI and the mouse positions don't line up.
        const ImGuiIO& frameIo = ImGui::GetIO();
        SDL_RenderSetScale(renderer, frameIo.DisplayFramebufferScale.x, frameIo.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(renderer, 236, 236, 232, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }
    writeFile(path, snapshot());
}

void App::drawFrame() {
    const float S = UI_SCALE;
    const bool anyPopup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    handleShortcuts(anyPopup);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("Encounter", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);

    // The sidebar on the left, the board on the right, both as tall as the window.
    const float panelRound = 22 * S;
    const ImVec2 sidebarSize(310 * S, ImGui::GetContentRegionAvail().y - 10 * S);
    drawSidebar(sidebarSize, panelRound);
    ImGui::SameLine(0, 22 * S);
    drawBoard(sidebarSize.y, panelRound, anyPopup);

    // Menus and windows (they float above everything).
    drawCardMenu();
    drawBoardMenu();
    drawShareWindow();
    drawNewCharacterWindow();
    drawEditWindow();
    drawNewEncounterWindow();

    ImGui::End();
}

// Keyboard shortcuts (not while typing or with a popup open).
void App::handleShortcuts(bool anyPopup) {
    const ImGuiIO& kio = ImGui::GetIO();
    const bool mod = kio.KeyCtrl || kio.KeySuper;
    if (!kio.WantTextInput && !anyPopup) {
        if (mod && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
            if (kio.KeyShift) doRedo(); else doUndo();
        } else if (mod && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
            doRedo();
        } else if (mod && ImGui::IsKeyPressed(ImGuiKey_A, false)) {
            board.selectAll();
        } else if (mod && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
            duplicate(board.selectedRefs());
        } else if (board.inCombat && (ImGui::IsKeyPressed(ImGuiKey_Space, false) || ImGui::IsKeyPressed(ImGuiKey_RightArrow, false))) {
            nextTurn();
        } else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            board.clearSelection();
        } else if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false)) {
            deleteRefs(board.selectedRefs(), false);
        }
    }
}

// Player screens: publish the public state when it changed (checked ~10 times a second).
void App::publishPlayerState() {
    if (playerServer.running() && ImGui::GetTime() - lastPublish > 0.1) {
        lastPublish = ImGui::GetTime();
        const std::string state = playerStateJson(encounter, board);
        if (state != lastPublished) {
            lastPublished = state;
            publishVersion++;
            playerServer.publish("{\"v\":" + std::to_string(publishVersion) + "," + state.substr(1));
        }
    }
}

// ---- Log and alerts

void App::addLog(const std::string& text, ImU32 color) {
    log.push_back(LogEntry{text, color, board.round});
    if (log.size() > 1000) log.erase(log.begin());
    logScrollToBottom = true;
}

void App::addAlert(const std::string& text, ImU32 color) {
    alerts.erase(std::remove_if(alerts.begin(), alerts.end(), [&](const Alert& a) { return a.text == text; }),
                 alerts.end());
    alerts.push_back(Alert{text, color, ImGui::GetTime()});
    if (alerts.size() > 6) alerts.erase(alerts.begin());
    addLog(text, color);
}

// ---- Saving

void App::saveLibrary() { writeFile(libraryPath, serializeLibrary(library)); }

void App::saveParty() { writeFile(partyPath, serializeParty(party)); }

void App::saveSettings() { writeFile(settingsPath, "cardScale=" + std::to_string(g_cardScale) + "\n"); }

// ---- Undo

std::string App::snapshot() const { return serializeBoard(encounter, board); }

void App::pushUndoState(const std::string& state) { history.push(state); }

void App::pushUndo() { pushUndoState(snapshot()); }

void App::doUndo() {
    if (!history.canUndo()) return;
    deserializeBoard(history.undo(snapshot()), encounter, board);
    addLog("Undo", COL_MUTED);
}

void App::doRedo() {
    if (!history.canRedo()) return;
    deserializeBoard(history.redo(snapshot()), encounter, board);
    addLog("Redo", COL_MUTED);
}

// ---- Rules actions (each caller pushes undo once before a batch)

std::string App::nameOf(CardRef r) const { return encounter.at(r.side, r.index)->getName(); }

void App::applyHit(CardRef r, int amount) {
    Creature* c = encounter.at(r.side, r.index);
    if (c == nullptr) return;
    CardState& st = board.get(r);
    const std::string name = c->getName();
    const HitReport h = encounter.damage(r.side, r.index, amount);
    const DamageResult& d = h.damage;

    if (d.hpLost > 0) {
        st.requested = d.afterMod;  // "crit" is judged on damage actually taken
        st.pendingKind = FxKind::Damage;
        st.pendingLabel = "-" + std::to_string(d.hpLost);
    } else if (d.absorbed > 0) {
        st.pendingKind = FxKind::Temp;
        st.pendingLabel = "-" + std::to_string(d.absorbed) + " temp";
    } else if (d.rolled > 0 && c->getDamageMod() == DamageMod::Immune) {
        st.pendingKind = FxKind::Immune;
        st.pendingLabel = "IMMUNE";
    }

    std::string line = name + " takes " + std::to_string(d.afterMod);
    if (c->getDamageMod() == DamageMod::Resistant) line += " (resisted " + std::to_string(d.rolled) + ")";
    if (c->getDamageMod() == DamageMod::Vulnerable) line += " (vulnerable, " + std::to_string(d.rolled) + " x2)";
    if (c->getDamageMod() == DamageMod::Immune) line = name + " is immune (" + std::to_string(d.rolled) + " ignored)";
    if (d.absorbed > 0) line += ", " + std::to_string(d.absorbed) + " absorbed by temp HP";
    line += " -> " + std::to_string(c->getCurrentHp()) + "/" + std::to_string(c->getMaxHp());
    addLog(line, COL_RED);

    if (h.concentrationDc > 0) {
        addAlert(name + ": concentration save, DC " + std::to_string(h.concentrationDc), COL_VIOLET);
    }
    if (h.concentrationLost) addAlert(name + " drops to 0 HP and loses concentration", COL_VIOLET);
    if (h.instantDeath) {
        addAlert(name + " is killed outright (massive damage)", COL_RED);
    } else if (h.deathSaveFailed) {
        if (h.killedOutright) {
            addAlert(name + " fails a third death save and dies", COL_RED);
        } else {
            addAlert(name + " is hit at 0 HP: death save failure (" + std::to_string(c->getDeathFailures()) + "/3)",
                     COL_RED);
        }
    } else if (h.droppedToZero) {
        addLog(name + (r.side == Side::Players ? " falls unconscious and is dying" : " is defeated"), COL_MUTED);
    }
}

void App::applyHeal(CardRef r, int amount) {
    Creature* c = encounter.at(r.side, r.index);
    if (c == nullptr) return;
    const bool wasDown = c->getCurrentHp() == 0;
    const int gained = encounter.heal(r.side, r.index, amount);
    CardState& st = board.get(r);
    st.requested = amount;
    st.pendingKind = FxKind::Heal;
    st.pendingLabel = gained > 0 ? "+" + std::to_string(gained) : "FULL";
    std::string line = c->getName() + " heals " + std::to_string(gained) + " -> " +
                       std::to_string(c->getCurrentHp()) + "/" + std::to_string(c->getMaxHp());
    addLog(line, COL_GREEN);
    if (wasDown && gained > 0 && r.side == Side::Players) addLog(c->getName() + " is back on their feet", COL_GREEN);
}

void App::applyTemp(CardRef r, int amount) {
    Creature* c = encounter.at(r.side, r.index);
    if (c == nullptr) return;
    const int before = c->getTempHp();
    c->grantTempHp(amount);
    CardState& st = board.get(r);
    st.pendingKind = FxKind::Temp;
    st.pendingLabel = "+" + std::to_string(c->getTempHp() - before) + " temp";
    addLog(c->getName() + " has " + std::to_string(c->getTempHp()) + " temp HP" +
               (amount <= before ? " (kept the higher value)" : ""),
           COL_TEMP);
}

// ---- Turns and combat

bool App::isDefeated(CardRef r) const {
    const Creature* c = encounter.at(r.side, r.index);
    return c != nullptr && lifeOf(r.side, *c) == Life::Dead;
}

bool App::anyoneStanding() const {
    return std::any_of(board.z.begin(), board.z.end(), [&](CardRef r) { return !isDefeated(r); });
}

// A new round: timed conditions count down (with an alert for each one that ends).
void App::beginNewRound() {
    const std::vector<std::string> ended = encounter.tickConditions();
    board.round++;
    addLog("--- Round " + std::to_string(board.round) + " ---", COL_MUTED);
    for (const std::string& line : ended) addAlert(line, COL_AMBER);
}

// Gives the turn to the next combatant standing after order index `start`
// (-1 = from the top). Going past the end of the order starts a new round.
// `skipFallen` also skips cards that are still playing their death animation.
bool App::passTurnFrom(int start, bool skipFallen) {
    const int n = static_cast<int>(board.z.size());
    for (int step = 1; step <= n; step++) {
        const int idx = ((start + step) % n + n) % n;
        const CardRef cand = board.z[idx];
        if (isDefeated(cand) || (skipFallen && board.get(cand).wasDead)) continue;
        if (start >= 0 && idx <= start) beginNewRound();
        board.turn = cand;
        board.turnValid = true;
        scrollToTurn = true;
        addLog("Turn: " + encounter.at(cand.side, cand.index)->getName(), COL_MUTED);
        return true;
    }
    return false;
}

// In combat, keeps the turn marker valid: when nobody has it, the first card
// standing gets it; when the card that holds it has been defeated, the turn moves
// on by itself (the defeated card has left the Turn order, so a marker left on it
// would be invisible).
void App::ensureTurn() {
    if (!board.inCombat) return;
    if (board.turnValid && !board.get(board.turn).wasDead) return;
    passTurnFrom(board.turnValid ? board.indexInOrder(board.turn) : -1, true);
}

// Passes the turn to the next combatant still standing. After the last one, a new
// round starts (round counter up, timed conditions tick down).
void App::nextTurn() {
    if (!board.inCombat || !anyoneStanding()) return;
    pushUndo();
    passTurnFrom(board.turnValid ? board.indexInOrder(board.turn) : -1, false);
}

void App::startTurnAt(CardRef r) {
    pushUndo();
    board.turn = r;
    board.turnValid = true;
    scrollToTurn = true;
    addLog("Turn: " + encounter.at(r.side, r.index)->getName(), COL_MUTED);
}

// Setup -> combat: round 1, the turn goes to the first card standing.
void App::startCombat() {
    if (!anyoneStanding()) return;
    pushUndo();
    board.inCombat = true;
    board.round = 1;
    board.turnValid = false;
    encounter.resume();
    board.clearSelection();
    addLog("=== Combat started ===", COL_MUTED);
    passTurnFrom(-1, false);
}

// Combat -> setup: the enemies leave; the party stays, with the HP it has now.
void App::endCombat() {
    pushUndo();
    std::vector<CardRef> enemies;
    for (int i = 0; i < static_cast<int>(board.creatures.size()); i++) enemies.push_back({Side::Creatures, i});
    removeCombatants(encounter, board, enemies);
    board.inCombat = false;
    board.round = 1;
    board.turnValid = false;
    board.focusValid = false;
    alerts.clear();
    addLog("=== Combat ended ===", COL_MUTED);
}

// ---- Party

bool App::onBoard(const std::string& name) const {
    for (const Creature& c : encounter.getPlayers()) {
        if (c.getName() == name) return true;
    }
    return false;
}

// Saves a player to the party, or updates the saved one with the same name.
void App::saveToParty(const Creature& c) {
    const PlayerTemplate t = playerFromCreature(c);
    const auto it = std::find_if(party.begin(), party.end(), [&](const PlayerTemplate& p) { return p.name == t.name; });
    if (it != party.end()) *it = t;
    else party.push_back(t);
    saveParty();
    statusMsg = "Saved " + t.name + " to the party.";
}

std::vector<PlayerTemplate> App::partyMissing() const {
    std::vector<PlayerTemplate> out;
    for (const PlayerTemplate& t : party) {
        if (!onBoard(t.name)) out.push_back(t);
    }
    return out;
}

// Adds party members (full HP) at the end of the turn order, skipping anyone
// who is already on the board. One undo step for the whole group.
std::vector<CardRef> App::addPartyMembers(const std::vector<PlayerTemplate>& members) {
    std::vector<CardRef> added;
    std::vector<PlayerTemplate> todo;
    for (const PlayerTemplate& t : members) {
        if (!onBoard(t.name)) todo.push_back(t);
    }
    if (todo.empty()) return added;
    pushUndo();
    board.clearSelection();
    std::string names;
    for (size_t i = 0; i < todo.size(); i++) {
        const CardRef r = addCombatant(encounter, board, Side::Players, creatureFromPlayer(todo[i]));
        board.get(r).selected = true;
        board.get(r).spawnT = -0.06f * static_cast<float>(i);  // staggered fade-in
        added.push_back(r);
        names += (names.empty() ? "" : ", ") + todo[i].name;
    }
    addLog("Added " + names, COL_MUTED);
    return added;
}

// ---- Adding, copying and removing cards

void App::duplicate(const std::vector<CardRef>& refs) {
    if (refs.empty()) return;
    pushUndo();
    std::vector<std::pair<Side, Creature>> copies;
    for (CardRef r : refs) {
        Creature copy = *encounter.at(r.side, r.index);
        copy.setHp(copy.getMaxHp());
        copy.setTempHp(0);
        copy.clearConditions();
        copy.setConcentrating(false);
        copy.resetDeathSaves();
        copies.emplace_back(r.side, copy);
    }
    board.clearSelection();
    for (size_t i = 0; i < copies.size(); i++) {
        Creature c = copies[i].second;
        c.rename(makeNumberedNames(encounter, c.getName(), 1)[0]);
        const CardRef nr = addCombatant(encounter, board, copies[i].first, c);
        board.get(nr).selected = true;
        board.get(nr).spawnT = -0.06f * static_cast<float>(i);  // staggered fade-in
        addLog("Added " + c.getName(), COL_MUTED);
    }
}

void App::deleteRefs(const std::vector<CardRef>& refs, bool alreadyPushedUndo) {
    if (refs.empty()) return;
    if (!alreadyPushedUndo) pushUndo();
    std::string names;
    for (CardRef r : refs) names += (names.empty() ? "" : ", ") + nameOf(r);
    removeCombatants(encounter, board, refs);
    addLog("Removed " + names, COL_MUTED);
}

std::vector<CardRef> App::addFromTemplate(const MonsterTemplate& t, Side side, int count) {
    pushUndo();
    const std::vector<std::string> names = makeNumberedNames(encounter, t.name, count);
    board.clearSelection();
    std::vector<CardRef> added;
    for (size_t i = 0; i < names.size(); i++) {
        Creature c = creatureFromTemplate(t);
        c.rename(names[i]);
        const CardRef r = addCombatant(encounter, board, side, c);
        board.get(r).selected = true;
        board.get(r).spawnT = -0.06f * static_cast<float>(i);  // staggered fade-in
        added.push_back(r);
    }
    addLog("Added " + std::to_string(count) + " x " + t.name, COL_MUTED);
    return added;
}

// Opens the Edit window on a copy of the combatant (Apply writes it back).
void App::openEditor(CardRef r) {
    editRef = r;
    editCopy = *encounter.at(r.side, r.index);
    std::snprintf(editName, sizeof(editName), "%s", editCopy->getName().c_str());
    std::snprintf(editNotes, sizeof(editNotes), "%s", editCopy->getNotes().c_str());
    editCustomCond[0] = '\0';
    openEdit = true;
}

// ---- Player screens

void App::makeQr(const std::string& text) {
    const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(text.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
    qrSize = qr.getSize();
    qrModules.assign(static_cast<size_t>(qrSize * qrSize), false);
    for (int y = 0; y < qrSize; y++) {
        for (int x = 0; x < qrSize; x++) qrModules[static_cast<size_t>(y * qrSize + x)] = qr.getModule(x, y);
    }
}

void App::startSharing() {
    shareError.clear();
    if (!playerServer.start(8080)) {
        shareError = "Couldn't open a port for the player screens (8080-8099 are all busy).";
        return;
    }
    shareUrl = playerServer.url();
    makeQr(shareUrl);
    lastPublished.clear();  // publish right away
    addLog("Player screens on at " + shareUrl, COL_MUTED);
}
