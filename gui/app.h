#pragma once
// The application: owns the encounter, the board, the libraries and the player
// screens, and draws the whole window once per frame.
//
// Its methods are split over a few files by area:
//   app.cpp          the frame loop, loading and saving, and the actions
//                    (damage, turns, undo...) the UI calls
//   app_sidebar.cpp  the sidebar: summary tiles and the Combat / Library / Log tabs
//   app_board.cpp    the board: Turn order and Defeated columns, spotlight, trash
//   app_popups.cpp   menus and windows: card menu, New character, Edit, QR...
#include <SDL.h>

#include <optional>
#include <string>
#include <vector>

#include "imgui.h"
#include "alerts.h"
#include "board.h"
#include "encounter.h"
#include "library.h"
#include "player_server.h"
#include "undo_history.h"

// The standard 5e conditions offered in menus (custom ones can be typed in).
inline constexpr const char* STANDARD_CONDITIONS[] = {
    "Blinded",   "Charmed",     "Deafened",  "Exhaustion", "Frightened",
    "Grappled",  "Incapacitated", "Invisible", "Paralyzed", "Petrified",
    "Poisoned",  "Prone",       "Restrained", "Stunned",   "Unconscious",
};
inline constexpr int STANDARD_CONDITION_COUNT = sizeof(STANDARD_CONDITIONS) / sizeof(STANDARD_CONDITIONS[0]);

class App {
public:
    // The window and renderer stay owned by the caller (main), which made them.
    App(SDL_Window* window, SDL_Renderer* renderer);

    // Runs until the window is closed, then saves the encounter.
    void run();

private:
    // ---- One frame (app.cpp)
    void drawFrame();
    void handleShortcuts(bool anyPopup);
    void publishPlayerState();

    // ---- Sidebar (app_sidebar.cpp)
    void drawSidebar(ImVec2 sidebarSize, float panelRound);
    void drawSummary();
    void drawCombatTab();
    void drawLibraryTab();
    void drawLogTab();
    void finishTabBody();  // the tab switch animation, applied to a tab's window

    // ---- Board (app_board.cpp)
    void drawBoard(float height, float panelRound, bool anyPopup);
    void drawSetupPanel(ImVec2 a, ImVec2 b);

    // ---- Menus and windows (app_popups.cpp)
    void drawCardMenu();
    void drawBoardMenu();
    void drawShareWindow();
    void drawNewCharacterWindow();
    void drawEditWindow();
    void drawNewEncounterWindow();

    // ---- Log and alerts
    void addLog(const std::string& text, ImU32 color);
    void addAlert(const std::string& text, ImU32 color);

    // ---- Saving
    void saveLibrary();
    void saveParty();
    void saveSettings();

    // ---- Undo: every change pushes the state before it, once per user action.
    std::string snapshot() const;
    void pushUndoState(const std::string& state);
    void pushUndo();
    void doUndo();
    void doRedo();

    // ---- Rules actions (each caller pushes undo once before a batch)
    std::string nameOf(CardRef r) const;
    void applyHit(CardRef r, int amount);
    void applyHeal(CardRef r, int amount);
    void applyTemp(CardRef r, int amount);

    // ---- Turns and combat
    bool isDefeated(CardRef r) const;
    bool anyoneStanding() const;
    void beginNewRound();
    bool passTurnFrom(int start, bool skipFallen);
    void ensureTurn();
    void nextTurn();
    void startTurnAt(CardRef r);
    void startCombat();
    void endCombat();

    // ---- Party
    bool onBoard(const std::string& name) const;
    void saveToParty(const Creature& c);
    std::vector<PlayerTemplate> partyMissing() const;
    std::vector<CardRef> addPartyMembers(const std::vector<PlayerTemplate>& members);

    // ---- Adding, copying and removing cards
    void duplicate(const std::vector<CardRef>& refs);
    void deleteRefs(const std::vector<CardRef>& refs, bool alreadyPushedUndo);
    std::vector<CardRef> addFromTemplate(const MonsterTemplate& t, Side side, int count);
    void openEditor(CardRef r);

    // ---- Player screens
    void makeQr(const std::string& text);
    void startSharing();

    // ================= State =================

    SDL_Window* window;
    SDL_Renderer* renderer;

    Encounter encounter;  // the rules: combatants, HP, conditions
    BoardState board;     // the UI around them: turn order, selection, animations

    // Log & alerts
    std::vector<LogEntry> log;
    std::vector<Alert> alerts;
    bool logScrollToBottom = true;

    // Files
    std::string path;          // the encounter (autosaved)
    std::string libraryPath;
    std::string partyPath;
    std::string settingsPath;  // view settings (card size), not undoable
    std::string statusMsg;
    double lastAutosave = 0.0;
    std::vector<MonsterTemplate> library;
    std::vector<PlayerTemplate> party;

    // Undo
    UndoHistory history;
    std::string pressSnapshot;  // board state when the mouse last went down

    // Amount used by every Damage / Heal / temp HP button.
    int actionAmount = 5;

    // Player screens: a web page on the local network that phones open by QR code.
    PlayerServer playerServer;
    bool openShare = false;
    std::string shareUrl, shareError;
    std::vector<bool> qrModules;  // row by row, true = dark
    int qrSize = 0;
    std::string lastPublished;
    int publishVersion = 0;
    double lastPublish = -1.0;

    // Turn order (board.z is the order, top to bottom; board.turn holds the turn)
    bool scrollToTurn = false;   // bring the card that holds the turn into view
    int dragSlot = -1;           // Turn order slot the dragged card would drop into
    bool openBoardMenu = false;  // right-click on empty space
    int boardMenuSlot = -1;      // Turn order slot under that right-click (-1 = at the end)
    bool spotTurnValid = false;  // the turn the spotlight last followed
    CardRef spotTurn{Side::Players, 0};

    // Edit window
    std::optional<Creature> editCopy;
    CardRef editRef{Side::Players, 0};
    bool openEdit = false;
    char editName[64] = "";
    char editNotes[1024] = "";
    char editCustomCond[40] = "";

    // Menus and the other windows
    bool openMenu = false;
    CardRef menuRef{Side::Players, 0};
    bool openNewEncounter = false;
    bool openNewChar = false;     // "New character" window
    bool newCharFocus = false;    // focus the name field when it opens
    int newCharSlot = -1;         // Turn order slot to insert the new cards at (-1 = end)
    bool newSaveLib = false;      // also save a new creature to the monster library
    bool newSaveParty = true;     // also save a new player to the party
    int newColor = -1;            // look of the new character (-1 = default)
    int newIcon = -1;
    char newName[64] = "";
    int newMaxHp = 20;
    int newAc = 12;
    int newCount = 1;
    int newSide = 1;
    int newLevel = 1;
    int newCr = -1;

    // Selection box on the board
    bool marqueeActive = false;
    ImVec2 marqueeStart{0, 0};

    // Sidebar
    char libraryFilter[48] = "";
    int libraryCount = 1;
    int selCondition = 10;  // Poisoned
    int selRounds = 0;
    int activeTab = 0;      // 0 Combat, 1 Library, 2 Log
    int shownTab = 0;       // tab the content animation last started for
    double tabSwitchTime = -10.0;
    float tabSlideDir = 1.0f;  // +1: new content comes from the right, -1: from the left
    float tabK = 1.0f;         // tab switch animation this frame, 0..1
};
