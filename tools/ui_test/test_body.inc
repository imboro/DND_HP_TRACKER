        {
            static int frame = 0;
            frame++;
            static size_t si = 0;
            static int waitFrames = 0;
            static int fails = 0;
            static double holdUntil = 0.0;  // steps pause until this ImGui time
            static std::vector<std::pair<int, std::function<void()>>> steps;
            static std::function<void(float, float)> motion;
            static std::function<void(bool, float, float)> button;
            static std::function<void(bool, SDL_Keycode, SDL_Scancode, Uint16)> key;
            if (!motion) {
                const Uint32 wid = SDL_GetWindowID(window);
                motion = [wid](float x, float y) { g_testMouse = ImVec2(x, y); SDL_Event e{}; e.type = SDL_MOUSEMOTION; e.motion.windowID = wid; e.motion.which = TEST_MOUSE_ID; e.motion.x = (int)x; e.motion.y = (int)y; SDL_PushEvent(&e); };
                button = [wid](bool down, float x, float y) { SDL_Event e{}; e.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP; e.button.windowID = wid; e.button.which = TEST_MOUSE_ID; e.button.button = SDL_BUTTON_LEFT; e.button.state = down ? SDL_PRESSED : SDL_RELEASED; e.button.clicks = 1; e.button.x = (int)x; e.button.y = (int)y; SDL_PushEvent(&e); };
                key = [wid](bool down, SDL_Keycode sym, SDL_Scancode sc, Uint16 mod) { SDL_Event e{}; e.type = down ? SDL_KEYDOWN : SDL_KEYUP; e.key.windowID = wid; e.key.state = down ? SDL_PRESSED : SDL_RELEASED; e.key.keysym.sym = sym; e.key.keysym.scancode = sc; e.key.keysym.mod = mod; SDL_PushEvent(&e); };
            }
            auto check = [](bool ok, const std::string& what) { std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str()); if (!ok) fails++; };

            if (frame == 1) {
                static const float s = UI_SCALE;
                static auto findRef = [&](const std::string& n) {
                    for (int i = 0; i < (int)encounter.getPlayers().size(); i++) if (encounter.getPlayers()[i].getName() == n) return CardRef{Side::Players, i};
                    for (int i = 0; i < (int)encounter.getCreatures().size(); i++) if (encounter.getCreatures()[i].getName() == n) return CardRef{Side::Creatures, i};
                    return CardRef{Side::Players, -1};
                };
                static auto exists = [&](const std::string& n) { return findRef(n).index >= 0; };
                static auto C = [&](const std::string& n) { CardRef r = findRef(n); return encounter.at(r.side, r.index); };
                static auto CS = [&](const std::string& n) -> CardState& { return board.get(findRef(n)); };
                static auto dmgPt = [&](const std::string& n) { return CS(n).damageCenter; };
                static auto healPt = [&](const std::string& n) { return CS(n).healCenter; };
                static auto bodyPt = [&](const std::string& n) { CardState& st = CS(n); return ImVec2(st.drawPos.x + st.size.x * 0.45f, st.drawPos.y + 22 * s); };
                static auto pipPt = [&](const std::string& n, int group, int i) { return CS(n).pipCenter[group * 3 + i]; };
                static auto rectCenter = [](const std::string& label) { auto it = g_rects.find(label); if (it == g_rects.end()) { std::printf("  (no rect for %s)\n", label.c_str()); return ImVec2(-100, -100); } return ImVec2((it->second.Min.x + it->second.Max.x) / 2, (it->second.Min.y + it->second.Max.y) / 2); };
                static auto clickAt = [&](std::function<ImVec2()> where, int after = 4) {
                    steps.push_back({2, [=]() { ImVec2 p = where(); motion(p.x, p.y); button(true, p.x, p.y); }});
                    steps.push_back({after, [=]() { ImVec2 p = where(); button(false, p.x, p.y); }});
                };
                static auto shiftClick = [&](std::function<ImVec2()> where) {
                    steps.push_back({1, [=]() { key(true, SDLK_LSHIFT, SDL_SCANCODE_LSHIFT, KMOD_LSHIFT); }});
                    clickAt(where);
                    steps.push_back({2, [=]() { key(false, SDLK_LSHIFT, SDL_SCANCODE_LSHIFT, 0); }});
                };
                static auto cmdKey = [&](SDL_Keycode sym, SDL_Scancode sc) {
                    steps.push_back({1, [=]() { key(true, SDLK_LGUI, SDL_SCANCODE_LGUI, KMOD_LGUI); }});
                    steps.push_back({1, [=]() { key(true, sym, sc, KMOD_LGUI); }});
                    steps.push_back({1, [=]() { key(false, sym, sc, KMOD_LGUI); }});
                    steps.push_back({3, [=]() { key(false, SDLK_LGUI, SDL_SCANCODE_LGUI, 0); }});
                };
                static auto setAmount = [&](int v) { steps.push_back({2, [&, v]() { actionAmount = v; }}); };
                static auto act = [&](int after, std::function<void()> f) { steps.push_back({after, f}); };
                static auto alertHas = [&](const std::string& t) { for (auto& a : alerts) if (a.text.find(t) != std::string::npos) return true; return false; };
                static auto alertNamed = [&](const std::string& t) -> const Alert* { for (auto& a : alerts) if (a.text == t) return &a; return nullptr; };
                static auto waitSeconds = [&](double sec) { steps.push_back({1, [&, sec]() { holdUntil = ImGui::GetTime() + sec; }}); };

                act(10, [&]() { check(!board.inCombat, "the app starts in setup"); g_shot = "00_setup"; });
                clickAt([=]() { return rectCenter("Start combat"); });
                act(30, [&]() { check(board.inCombat && board.round == 1 && board.turnValid && board.turn == findRef("Hero"), "Start combat: round 1, the turn goes to the first card");
                                g_shot = "01_default"; });
                // 1) Card damage with a plain amount
                setAmount(3);
                clickAt([=]() { return dmgPt("Goblin"); });
                act(2, [&]() { check(C("Goblin")->getCurrentHp() == 4, "card Damage button deals the amount: Goblin 7 -> 4 (hp " + std::to_string(C("Goblin")->getCurrentHp()) + ")");
                               bool found = false; for (auto& e : log) if (e.text.find("Goblin takes 3") != std::string::npos) found = true; check(found, "log records the hit"); });
                act(80, [&]() {});
                // 2) Box-select from empty space beside the Hero card, dragging over Goblin
                static ImVec2 mq0, mq1;
                act(1, [&]() { CardState& h = CS("Hero"); CardState& g = CS("Goblin");
                               mq0 = ImVec2(h.drawPos.x + h.size.x + 4 * s, h.drawPos.y + 30 * s);
                               mq1 = ImVec2(g.drawPos.x + 40 * s, g.drawPos.y + 30 * s); });
                act(2, [&]() { motion(mq0.x, mq0.y); button(true, mq0.x, mq0.y); });
                act(2, [&]() { motion(mq1.x, mq1.y); });
                act(1, [&]() { g_shot = "02_marquee"; });
                act(3, [&]() { button(false, mq1.x, mq1.y); });
                act(1, [&]() { check(board.get(findRef("Hero")).selected && board.get(findRef("Goblin")).selected && !board.get(findRef("Orc")).selected, "box selection picks Hero + Goblin, not Orc"); });
                // 3) Click selects one; shift-click adds
                clickAt([=]() { return bodyPt("Goblin"); });
                shiftClick([=]() { return bodyPt("Orc"); });
                act(1, [&]() { check(selectedRefs().size() == 2 && board.get(findRef("Orc")).selected && !board.get(findRef("Hero")).selected, "click + shift-click selects Goblin and Orc (" + std::to_string(selectedRefs().size()) + ")"); });
                // 4) Area damage, then half damage for the ones that saved
                setAmount(10);
                clickAt([=]() { return rectCenter("Damage all"); });
                act(1, [&]() { check(C("Orc")->getCurrentHp() == 5 && C("Goblin")->getCurrentHp() == 0, "Damage all: Orc 15->5, Goblin to 0 (orc " + std::to_string(C("Orc")->getCurrentHp()) + ")"); });
                clickAt([=]() { return rectCenter("Half damage (5)"); });
                act(1, [&]() { check(C("Orc")->getCurrentHp() == 0, "Half damage deals 5 to Orc"); });
                // 5) Undo twice with Cmd+Z
                cmdKey(SDLK_z, SDL_SCANCODE_Z);
                cmdKey(SDLK_z, SDL_SCANCODE_Z);
                act(2, [&]() { check(C("Orc")->getCurrentHp() == 15, "Cmd+Z twice restores Orc to 15 (" + std::to_string(C("Orc")->getCurrentHp()) + ")"); });
                // 6) Temp HP on the Hero, concentration, then a hit
                clickAt([=]() { return bodyPt("Hero"); });
                setAmount(5);
                clickAt([=]() { return rectCenter("Give temp HP"); });
                act(2, [&]() { check(C("Hero")->getTempHp() == 5, "Give temp HP: Hero has 5 temp"); C("Hero")->setConcentrating(true); });
                setAmount(8);
                clickAt([=]() { return dmgPt("Hero"); });
                act(2, [&]() { check(C("Hero")->getTempHp() == 0 && C("Hero")->getCurrentHp() == 37, "temp HP absorbs first: Hero 37/40, temp 0 (hp " + std::to_string(C("Hero")->getCurrentHp()) + ")");
                               check(alertHas("Hero: concentration save, DC 10"), "concentration alert DC 10"); });
                // 7) Right-click menu opens
                act(2, [&]() { ImVec2 p = bodyPt("Orc"); motion(p.x, p.y); SDL_Event e{}; e.type = SDL_MOUSEBUTTONDOWN; e.button.windowID = SDL_GetWindowID(window); e.button.which = TEST_MOUSE_ID; e.button.button = SDL_BUTTON_RIGHT; e.button.state = SDL_PRESSED; e.button.clicks = 1; e.button.x = (int)p.x; e.button.y = (int)p.y; SDL_PushEvent(&e); });
                act(2, [&]() { ImVec2 p = bodyPt("Orc"); SDL_Event e{}; e.type = SDL_MOUSEBUTTONUP; e.button.windowID = SDL_GetWindowID(window); e.button.which = TEST_MOUSE_ID; e.button.button = SDL_BUTTON_RIGHT; e.button.state = SDL_RELEASED; e.button.x = (int)p.x; e.button.y = (int)p.y; SDL_PushEvent(&e); });
                act(3, [&]() { check(ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel), "right-click opens the card menu"); g_shot = "03_menu"; });
                act(3, [&]() { key(true, SDLK_ESCAPE, SDL_SCANCODE_ESCAPE, 0); });
                act(3, [&]() { key(false, SDLK_ESCAPE, SDL_SCANCODE_ESCAPE, 0); });
                // 8) Drop the Hero to 0, hit again (death save), click a success pip
                setAmount(37);
                clickAt([=]() { return dmgPt("Hero"); });
                act(2, [&]() { check(C("Hero")->getCurrentHp() == 0 && !C("Hero")->isConcentrating() && alertHas("loses concentration"), "Hero at 0 loses concentration"); });
                act(70, [&]() {});
                setAmount(1);
                clickAt([=]() { return dmgPt("Hero"); });
                act(2, [&]() { check(C("Hero")->getDeathFailures() == 1 && alertHas("death save failure (1/3)"), "hit at 0 HP = death save failure 1/3"); });
                clickAt([=]() { return pipPt("Hero", 0, 0); });
                act(2, [&]() { check(C("Hero")->getDeathSuccesses() == 1, "clicking a success pip sets 1 success"); });
                // 9) Conditions on the Orc via the sidebar, then the round ends by passing the last turn
                clickAt([=]() { return bodyPt("Orc"); });
                act(3, [&]() { selCondition = 10; selRounds = 1; g_scrollTo = "Apply condition"; });
                clickAt([=]() { return rectCenter("Apply condition"); });
                act(1, [&]() { selCondition = 11; selRounds = 0; });
                clickAt([=]() { return rectCenter("Apply condition"); });
                act(1, [&]() { check(C("Orc")->hasCondition("Poisoned") && C("Orc")->hasCondition("Prone"), "Poisoned (1 round) and Prone applied to Orc"); C("Orc")->setNotes("Carries the key"); C("Orc")->setDamageMod(DamageMod::Resistant); });
                act(40, [&]() { g_shot = "04_states"; g_scrollTo = "Next turn"; });
                act(3, [&]() { startTurnAt(findRef("Orc")); });  // the last card in the order
                clickAt([=]() { return rectCenter("Next turn"); });
                act(2, [&]() { g_scrollTo.clear(); g_shot = "04b_toasts"; });
                act(2, [&]() { check(board.round == 2 && !C("Orc")->hasCondition("Poisoned") && C("Orc")->hasCondition("Prone") && alertHas("Orc: Poisoned ended"), "Next turn past the last card: round 2, Poisoned expired with alert, Prone stays"); });
                // clicking a toast fades it away quickly, without reaching the cards under it
                static std::string clickedText;
                static size_t selBefore = 0;
                act(1, [&]() { clickedText = alerts.empty() ? "" : alerts.front().text; selBefore = selectedRefs().size();
                               std::printf("  alerts on screen: %zu, clicking \"%s\"\n", alerts.size(), clickedText.c_str()); });
                clickAt([&]() { const Alert& a = alerts.front(); return ImVec2((a.rectMin.x + a.rectMax.x) / 2, (a.rectMin.y + a.rectMax.y) / 2); });
                act(1, [&]() { const Alert* a = alertNamed(clickedText); check(a && a->clicked >= 0.0, "clicking a toast starts its fade");
                               check(selectedRefs().size() == selBefore, "the click doesn't reach the cards under the toast"); });
                waitSeconds(0.5);
                act(1, [&]() { check(!alertNamed(clickedText), "a clicked toast is gone after a quick fade"); });
                // 10) Resistance halves damage
                setAmount(9);
                clickAt([=]() { return dmgPt("Orc"); });
                act(2, [&]() { check(C("Orc")->getCurrentHp() == 11, "resistant Orc takes 4 from 9 (" + std::to_string(C("Orc")->getCurrentHp()) + ")"); });
                // 11) Duplicate with Cmd+D (Orc is selected)
                clickAt([=]() { return bodyPt("Orc"); });
                cmdKey(SDLK_d, SDL_SCANCODE_D);
                act(2, [&]() { check(exists("Orc 1") && exists("Orc 2") && !exists("Orc"), "Cmd+D: Orc renamed Orc 1, copy is Orc 2"); check(C("Orc 2")->getCurrentHp() == 15 && C("Orc 2")->getConditions().empty(), "duplicate starts fresh (full HP, no conditions)"); });
                // 12) Add 3 goblins from the library
                act(2, [&]() { addFromTemplate(defaultLibrary()[1], Side::Creatures, 3); });
                act(2, [&]() { check(exists("Goblin 1") && exists("Goblin 4") && !exists("Goblin"), "library x3: Goblin 2..4 added, Goblin renamed Goblin 1");
                               check(board.get(findRef("Goblin 4")).spawnT < SPAWN_TIME, "new cards fade in (staggered)"); });
                // 13) Edit via double-click
                act(2, [&]() { ImVec2 p = bodyPt("Hero"); motion(p.x, p.y); button(true, p.x, p.y); });
                act(1, [&]() { ImVec2 p = bodyPt("Hero"); button(false, p.x, p.y); });
                act(1, [&]() { ImVec2 p = bodyPt("Hero"); SDL_Event e{}; e.type = SDL_MOUSEBUTTONDOWN; e.button.windowID = SDL_GetWindowID(window); e.button.which = TEST_MOUSE_ID; e.button.button = SDL_BUTTON_LEFT; e.button.state = SDL_PRESSED; e.button.clicks = 2; e.button.x = (int)p.x; e.button.y = (int)p.y; SDL_PushEvent(&e); });
                act(3, [&]() { ImVec2 p = bodyPt("Hero"); button(false, p.x, p.y); });
                act(3, [&]() { check(ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) && editCopy.has_value(), "double-click opens the edit popup");
                               std::snprintf(editName, sizeof(editName), "Paladin"); g_shot = "05_edit"; });
                clickAt([=]() { return rectCenter("Apply"); });
                act(3, [&]() { check(exists("Paladin") && !exists("Hero"), "edit popup Apply renames Hero to Paladin"); });
                // 14) Drag Orc 2 into the trash
                act(2, [&]() { ImVec2 p = bodyPt("Orc 2"); motion(p.x, p.y); button(true, p.x, p.y); });
                act(3, [&]() { motion(g_boardOrigin.x + 300 * s, g_boardOrigin.y + 300 * s); });
                act(3, [&]() { const float ts = 84 * s; motion(g_boardOrigin.x + g_boardSize.x - ts / 2 - 24 * s, g_boardOrigin.y + g_boardSize.y - ts / 2 - 24 * s); });
                act(1, [&]() { g_shot = "06_trash"; });
                act(3, [&]() { button(false, g_boardOrigin.x + g_boardSize.x - 84 * s / 2 - 24 * s, g_boardOrigin.y + g_boardSize.y - 84 * s / 2 - 24 * s); });
                act(2, [&]() { check(!exists("Orc 2") && exists("Orc 1"), "dragging Orc 2 to the trash deletes it"); });
                // 15) Save format round trip + v1 compatibility
                act(1, [&]() {
                    const std::string a = serializeBoard(encounter, board);
                    Encounter e2; BoardState b2;
                    check(deserializeBoard(a, e2, b2) && serializeBoard(e2, b2) == a, "v2 save round-trips exactly");
                    Encounter e3; BoardState b3;
                    check(deserializeBoard("HPTRACKER 1\nP|Old Hero|40|33|50|62.5\n", e3, b3) && e3.getPlayers().size() == 1 && e3.getPlayers()[0].getCurrentHp() == 33, "v1 save files still load");
                    Encounter e4; BoardState b4; Creature w("We|ird, name: x", 9); w.setNotes("line1\nline|2, with: stuff"); w.addCondition("Hex: custom", 2);
                    addCombatant(e4, b4, Side::Creatures, w, ImVec2(1, 2));
                    Encounter e5; BoardState b5; deserializeBoard(serializeBoard(e4, b4), e5, b5);
                    check(e5.getCreatures().size() == 1 && e5.getCreatures()[0].getName() == "We|ird, name: x" && e5.getCreatures()[0].getNotes() == "line1\nline|2, with: stuff" && e5.getCreatures()[0].hasCondition("Hex: custom"), "separators in names/notes/conditions survive saving");
                });
                // 17) Tabs (the content slides in, so shots wait for it to settle)
                act(3, [&]() { g_forceTab = 1; });
                act(4, [&]() { g_shot = "12_tab_sliding"; });
                act(25, [&]() { g_shot = "07_add"; });
                act(2, [&]() { g_forceTab = 2; });
                act(25, [&]() { g_shot = "08_log"; });
                act(2, [&]() { g_forceTab = 0; });
                act(25, [&]() {});
                // 17b) Death animation: plays once after the HP bar has drained, then stays dead
                act(2, [&]() { CardRef r = findRef("Goblin 2"); pushUndo(); applyHit(r, 50); });
                waitSeconds(1.08);
                act(1, [&]() { CardState& st = board.get(findRef("Goblin 2")); check(st.wasDead && st.deathT < 0.3f, "death animation starts once the bar has drained (t " + std::to_string(st.deathT) + ")"); g_shot = "11_death_a"; });
                waitSeconds(0.25);
                act(1, [&]() { g_shot = "11_death_b"; });
                waitSeconds(0.6);
                act(1, [&]() { g_shot = "11_death_c"; });
                waitSeconds(0.8);
                act(1, [&]() { CardState& st = board.get(findRef("Goblin 2")); check(st.deathT >= DEATH_LEN, "death animation finishes and stays dead"); check(st.column == 1, "a dead card falls to the Defeated column"); });
                cmdKey(SDLK_z, SDL_SCANCODE_Z);  // undo the kill
                act(2, [&]() { check(C("Goblin 2")->getCurrentHp() > 0, "undo brings Goblin 2 back"); });
                cmdKey(SDLK_y, SDL_SCANCODE_Y);  // redo it
                act(2, [&]() { CardState& st = board.get(findRef("Goblin 2")); check(C("Goblin 2")->getCurrentHp() == 0 && st.wasDead && st.deathT >= DEATH_LEN, "redo shows it dead without replaying the animation"); });
                // 18) Turn order: fresh scenario Aria (player), Bram, Cora (creatures), Dax (player)
                static auto turnIs = [&](const std::string& n) { return board.turnValid && board.turn == findRef(n); };
                static auto zIndex = [&](const std::string& n) { CardRef r = findRef(n); for (size_t i = 0; i < board.z.size(); i++) if (board.z[i] == r) return (int)i; return -1; };
                act(2, [&]() {
                    encounter.clear(); board = BoardState(); undoStack.clear(); redoStack.clear(); clearSelection();
                    Creature a("Aria", 20); a.setLevel(3); a.setAc(15);
                    Creature b("Bram", 20); Creature c("Cora", 20); Creature d("Dax", 20); d.setLevel(3);
                    addCombatant(encounter, board, Side::Players, a, ImVec2(0, 0));
                    addCombatant(encounter, board, Side::Creatures, b, ImVec2(0, 0));
                    addCombatant(encounter, board, Side::Creatures, c, ImVec2(0, 0));
                    addCombatant(encounter, board, Side::Players, d, ImVec2(0, 0));
                    startCombat();
                });
                act(30, [&]() { g_scrollTo = "Next turn"; });
                act(2, [&]() { check(turnIs("Aria"), "the first combatant standing holds the turn"); g_shot = "13_turns"; });
                clickAt([=]() { return rectCenter("Next turn"); });
                act(2, [&]() { check(turnIs("Bram") && board.round == 1, "Next turn passes to Bram, round untouched"); });
                clickAt([=]() { return rectCenter("Next turn"); });
                clickAt([=]() { return rectCenter("Next turn"); });
                act(2, [&]() { check(turnIs("Dax"), "Next turn x3 reaches Dax"); });
                clickAt([=]() { return rectCenter("Next turn"); });
                act(2, [&]() { check(turnIs("Aria") && board.round == 2, "Next turn wraps around to Aria and starts round 2"); });
                steps.push_back({1, [=]() { key(true, SDLK_SPACE, SDL_SCANCODE_SPACE, 0); }});
                steps.push_back({3, [=]() { key(false, SDLK_SPACE, SDL_SCANCODE_SPACE, 0); }});
                act(2, [&]() { check(turnIs("Bram"), "Space also passes the turn"); });
                // The ribbon on the active card passes the turn too.
                act(20, [&]() {});
                clickAt([&]() { return CS("Bram").turnCenter; });
                act(2, [&]() { check(turnIs("Cora"), "clicking the TURN ribbon passes the turn"); });
                // Kill Cora (the card holding the turn): she falls to Defeated, keeping her slot.
                act(2, [&]() { pushUndo(); applyHit(findRef("Cora"), 99); });
                waitSeconds(1.7);
                act(1, [&]() { check(CS("Cora").column == 1 && zIndex("Cora") == 2, "a killed enemy falls to Defeated (slot kept in the order)");
                               check(turnIs("Dax"), "the turn moves on by itself when its card is defeated"); });
                cmdKey(SDLK_z, SDL_SCANCODE_Z);
                waitSeconds(0.4);
                act(1, [&]() { check(C("Cora")->getCurrentHp() == 20 && CS("Cora").column == 0 && zIndex("Cora") == 2, "undo brings her back to her old slot"); });
                // Mid-fall: the card is still visible while it crosses to the Defeated column.
                act(2, [&]() { pushUndo(); applyHit(findRef("Bram"), 99); });
                waitSeconds(1.0);
                act(1, [&]() { g_shot = "14_dying"; });
                waitSeconds(0.82);
                act(1, [&]() { g_shot = "14_defeated"; });
                waitSeconds(0.9);
                cmdKey(SDLK_z, SDL_SCANCODE_Z);
                waitSeconds(0.4);
                // Dead cards are skipped by Next turn.
                act(2, [&]() { pushUndo(); applyHit(findRef("Cora"), 99); });
                waitSeconds(1.7);
                act(1, [&]() { startTurnAt(findRef("Bram")); });
                act(3, [&]() { g_scrollTo = "Next turn"; });
                clickAt([=]() { return rectCenter("Next turn"); });
                act(2, [&]() { check(turnIs("Dax"), "Next turn skips the defeated card"); });
                // A dying player (0 HP, death saves) stays in the Turn order.
                act(2, [&]() { pushUndo(); applyHit(findRef("Dax"), 20); });  // exactly max HP: dying, not massive damage
                waitSeconds(1.7);
                act(1, [&]() { check(C("Dax")->getCurrentHp() == 0 && CS("Dax").column == 0, "a dying player stays in the Turn order"); });
                // Reorder by dragging Bram above Aria.
                static ImVec2 dragFrom, dragTo;
                act(1, [&]() { dragFrom = bodyPt("Bram"); dragTo = ImVec2(CS("Aria").drawPos.x + CS("Aria").size.x * 0.45f, CS("Aria").drawPos.y - 4 * s); });
                act(2, [&]() { motion(dragFrom.x, dragFrom.y); button(true, dragFrom.x, dragFrom.y); });
                act(3, [&]() { motion(dragFrom.x, (dragFrom.y + dragTo.y) / 2); });
                act(3, [&]() { motion(dragTo.x, dragTo.y); });
                act(1, [&]() { g_shot = "15_reorder"; });
                act(3, [&]() { button(false, dragTo.x, dragTo.y); });
                waitSeconds(0.5);
                act(1, [&]() { check(zIndex("Bram") == 0 && zIndex("Aria") == 1, "dragging Bram above Aria reorders the Turn order"); });
                act(1, [&]() {
                    const std::string text = serializeBoard(encounter, board);
                    Encounter e2; BoardState b2;
                    check(deserializeBoard(text, e2, b2) && serializeBoard(e2, b2) == text, "order and turn survive save and load");
                    const Creature* first = e2.at(b2.z[0].side, b2.z[0].index);
                    check(first && first->getName() == "Bram" && b2.turnValid && e2.at(b2.turn.side, b2.turn.index)->getName() == "Dax", "loaded order starts with Bram, turn still on Dax");
                    // A save from the old free board (no T line) still loads.
                    Encounter e3; BoardState b3;
                    check(deserializeBoard("HPTRACKER 2\nR|3\nE|0\nP|Old|10|10|120.5|40.0|0|10|30|10|1|-1|0|0|0|0||\nC|Gob|7|7|300|80|0|12|30|9|1|2|0|0|0|0||\n", e3, b3)
                          && b3.z.size() == 2 && b3.round == 3 && !b3.turnValid && !b3.inCombat, "saves from the free board still load (in setup)");
                });
                // Right-click on empty space opens the board menu.
                act(1, [&]() { CardState& last = CS("Dax"); dragTo = ImVec2(last.drawPos.x + last.size.x * 0.5f, last.drawPos.y + last.size.y + 24 * s); });
                act(2, [&]() { motion(dragTo.x, dragTo.y); SDL_Event e{}; e.type = SDL_MOUSEBUTTONDOWN; e.button.windowID = SDL_GetWindowID(window); e.button.which = TEST_MOUSE_ID; e.button.button = SDL_BUTTON_RIGHT; e.button.state = SDL_PRESSED; e.button.clicks = 1; e.button.x = (int)dragTo.x; e.button.y = (int)dragTo.y; SDL_PushEvent(&e); });
                act(2, [&]() { SDL_Event e{}; e.type = SDL_MOUSEBUTTONUP; e.button.windowID = SDL_GetWindowID(window); e.button.which = TEST_MOUSE_ID; e.button.button = SDL_BUTTON_RIGHT; e.button.state = SDL_RELEASED; e.button.x = (int)dragTo.x; e.button.y = (int)dragTo.y; SDL_PushEvent(&e); });
                act(3, [&]() { check(ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) && boardMenuSlot >= 0, "right-click on empty space opens the board menu (slot " + std::to_string(boardMenuSlot) + ")"); g_shot = "16_board_menu"; });
                act(3, [&]() { key(true, SDLK_ESCAPE, SDL_SCANCODE_ESCAPE, 0); });
                act(3, [&]() { key(false, SDLK_ESCAPE, SDL_SCANCODE_ESCAPE, 0); });
                // New character dialog: creates the card at the chosen slot.
                act(2, [&]() { openNewChar = true; newCharFocus = false; newCharSlot = 1; std::snprintf(newName, sizeof(newName), "Zed"); newSide = 1; newCount = 1; });
                act(4, [&]() { g_shot = "17_new_character"; });
                clickAt([=]() { return rectCenter("Create"); });
                act(3, [&]() { check(exists("Zed") && zIndex("Zed") == 1 && CS("Zed").selected, "New character creates the card at the chosen slot"); });
                act(70, [&]() { g_scrollTo = "End combat"; clearSelection(); motion(g_boardOrigin.x + g_boardSize.x - 60 * s, g_boardOrigin.y + g_boardSize.y * 0.5f); });
                act(1, [&]() { g_shot = "18_final"; });
                clickAt([=]() { return rectCenter("End combat"); });
                act(20, [&]() { check(!board.inCombat && encounter.getCreatures().empty() && exists("Aria") && exists("Dax") && C("Dax")->getCurrentHp() == 0,
                                      "End combat: enemies leave, the party stays as it is, back to setup"); g_scrollTo.clear(); });
                act(40, [&]() { motion(g_boardOrigin.x + g_boardSize.x - 60 * s, g_boardOrigin.y + g_boardSize.y * 0.5f); });
                act(1, [&]() { g_shot = "19_setup_again"; });
                // 18b) The party: save players once (updated by name), bring the missing ones back
                act(2, [&]() {
                    saveToParty(*C("Aria")); saveToParty(*C("Dax"));
                    C("Aria")->setLevel(5); saveToParty(*C("Aria"));
                    check(party.size() == 2 && party[0].name == "Aria" && party[0].level == 5, "Save to party stores each player once, updated by name");
                    const std::string text = serializeParty(party);
                    std::vector<PlayerTemplate> p2;
                    check(deserializeParty(text, p2) && p2.size() == 2 && p2[1].name == "Dax", "the party file round-trips");
                });
                act(2, [&]() { deleteRefs({findRef("Aria")}, false); });
                act(30, [&]() { check(!exists("Aria") && partyMissing().size() == 1, "Aria is missing from the board"); });
                clickAt([=]() { return rectCenter("Add the party (1)##setup"); });
                act(30, [&]() {
                    check(exists("Aria") && C("Aria")->getCurrentHp() == C("Aria")->getMaxHp() && C("Aria")->getLevel() == 5 && !exists("Dax 1"),
                          "Add the party brings back only the missing player, at full HP");
                    g_forceTab = 1; });
                act(30, [&]() { g_shot = "20_party"; });
                act(2, [&]() { g_forceTab = 0; });
                // 18c) Card looks: color and icon, kept by the save file and both libraries
                act(2, [&]() {
                    C("Aria")->setColor(4); C("Aria")->setIcon(8);
                    C("Dax")->setColor(6); C("Dax")->setIcon(2);
                    const std::string text = serializeBoard(encounter, board);
                    Encounter e2; BoardState b2;
                    bool kept = deserializeBoard(text, e2, b2) && serializeBoard(e2, b2) == text;
                    for (const Creature& c : e2.getPlayers()) if (c.getName() == "Aria") kept = kept && c.getColor() == 4 && c.getIcon() == 8;
                    check(kept, "card looks survive save and load");
                    MonsterTemplate t; t.name = "Imp"; t.color = 5; t.icon = 6;
                    std::vector<MonsterTemplate> lib{t}, lib2;
                    check(deserializeLibrary(serializeLibrary(lib), lib2) && lib2.size() == 1 && lib2[0].color == 5 && lib2[0].icon == 6, "the monster library keeps looks");
                    saveToParty(*C("Aria"));
                    std::vector<PlayerTemplate> p2;
                    check(deserializeParty(serializeParty(party), p2) && p2[0].color == 4 && p2[0].icon == 8, "the party keeps looks");
                    std::vector<MonsterTemplate> lib3;
                    check(deserializeLibrary("HPLIBRARY 1\nT|Old|7|15|30|9|2|0\n", lib3) && lib3.size() == 1 && lib3[0].color == -1, "libraries without looks still load");
                    startCombat();
                });
                act(40, [&]() { motion(g_boardOrigin.x + g_boardSize.x - 60 * s, g_boardOrigin.y + g_boardSize.y * 0.5f); });
                act(1, [&]() { g_shot = "21_looks"; });
                act(2, [&]() { const CardRef r = findRef("Aria"); editRef = r; editCopy = *C("Aria");
                               std::snprintf(editName, sizeof(editName), "%s", editCopy->getName().c_str()); editNotes[0] = '\0'; openEdit = true; });
                act(20, [&]() { g_shot = "22_look_picker"; });
                act(3, [&]() { key(true, SDLK_ESCAPE, SDL_SCANCODE_ESCAPE, 0); });
                act(3, [&]() { key(false, SDLK_ESCAPE, SDL_SCANCODE_ESCAPE, 0); });

                // 18d) Player screens: the state the phones get, and the page itself (rendered with WebKit)
                static auto httpGet = [](const std::string& url) {
                    std::string out;
                    if (FILE* f = popen(("curl -s --max-time 3 '" + url + "'").c_str(), "r")) {
                        char buf[4096]; size_t n;
                        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
                        pclose(f);
                    }
                    return out;
                };
                static auto localUrl = [&](const std::string& path) { return "http://127.0.0.1:" + std::to_string(playerServer.port()) + "/" + path; };
                act(2, [&]() {
                    Creature g("Ghoul", 22); g.setColor(4); g.setIcon(4);
                    addCombatant(encounter, board, Side::Creatures, g, ImVec2(0, 0));
                    C("Aria")->setHp(20);
                    startSharing();
                    check(playerServer.running() && playerServer.port() >= 8080, "player screens start on a local port (" + std::to_string(playerServer.port()) + ")");
                    std::printf("  phone URL: %s\n", shareUrl.c_str());
                    openShare = true;
                });
                act(25, [&]() { g_shot = "23_share_qr"; });
                act(3, [&]() { key(true, SDLK_ESCAPE, SDL_SCANCODE_ESCAPE, 0); });
                act(3, [&]() { key(false, SDLK_ESCAPE, SDL_SCANCODE_ESCAPE, 0); });
                act(1, [&]() {
                    const std::string page = httpGet(localUrl(""));
                    const std::string state = httpGet(localUrl("state"));
                    check(page.find("HP Tracker") != std::string::npos && page.find("</html>") != std::string::npos, "the phone page is served");
                    check(state.find("\"name\":\"Aria\"") != std::string::npos && state.find("\"hp\":20") != std::string::npos && state.find("\"combat\":true") != std::string::npos,
                          "the state has the players' HP and the combat");
                    const size_t g = state.find("\"name\":\"Ghoul\"");
                    const std::string ghoul = g == std::string::npos ? std::string() : state.substr(g, state.find('}', g) - g);
                    check(!ghoul.empty() && ghoul.find("\"hp\"") == std::string::npos, "enemies are sent by name only (" + ghoul + ")");
                    BoardState setup = board; setup.inCombat = false;
                    check(playerStateJson(encounter, setup).find("Ghoul") == std::string::npos, "enemies stay hidden during setup");
                });
                // WebKit renders the page as a phone; the HP drops and the turn passes while it is open.
                act(1, [&]() {
                    const std::string cmd = "'" + g_outDir + "/phone_snapshot' '" + localUrl("?me=Aria") + "' '" + g_outDir + "/phone' 1.6 2.75 3.3 4.6 > /dev/null 2>&1 &";
                    std::system(cmd.c_str());
                });
                waitSeconds(2.2);
                act(1, [&]() { pushUndo(); applyHit(findRef("Aria"), 9); startTurnAt(findRef("Aria")); });
                waitSeconds(3.2);
                act(1, [&]() {
                    std::ifstream last(g_outDir + "/phone_3.png");
                    check(last.good() && playerServer.viewers() >= 1, "a phone (WebKit) follows the page (" + std::to_string(playerServer.viewers()) + " connected)");
                    playerServer.stop();
                    check(!playerServer.running(), "player screens stop");
                });

                // 19) Alerts fade out on their own
                act(2, [&]() { addAlert("Fade test", COL_AMBER); });
                waitSeconds(1.5);
                act(1, [&]() { const Alert* a = alertNamed("Fade test"); check(a && alertOpacity(*a, ImGui::GetTime()) == 1.0f, "alert fully visible after 1.5 s"); });
                waitSeconds(2.4);
                act(1, [&]() { const Alert* a = alertNamed("Fade test"); const float o = a ? alertOpacity(*a, ImGui::GetTime()) : -1.0f;
                               check(a && o > 0.0f && o < 1.0f, "alert is fading after ~4 s (opacity " + std::to_string(o) + ")"); g_shot = "10_fading"; });
                waitSeconds(1.2);
                act(1, [&]() { check(!alertHas("Fade test"), "alert gone after the fade"); });
                act(3, [&]() { g_forceTab = -1; std::printf("%s: %d failure(s)\n", fails ? "DONE WITH FAILURES" : "ALL UI CHECKS PASSED", fails); running = false; });
            }
            if (ImGui::GetTime() < holdUntil) {
            } else if (waitFrames > 0) {
                waitFrames--;
            } else if (si < steps.size()) {
                steps[si].second();
                waitFrames = steps[si].first;
                si++;
            }
        }
