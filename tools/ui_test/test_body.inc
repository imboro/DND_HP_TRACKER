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
                motion = [wid](float x, float y) { SDL_Event e{}; e.type = SDL_MOUSEMOTION; e.motion.windowID = wid; e.motion.x = (int)x; e.motion.y = (int)y; SDL_PushEvent(&e); };
                button = [wid](bool down, float x, float y) { SDL_Event e{}; e.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP; e.button.windowID = wid; e.button.button = SDL_BUTTON_LEFT; e.button.state = down ? SDL_PRESSED : SDL_RELEASED; e.button.clicks = 1; e.button.x = (int)x; e.button.y = (int)y; SDL_PushEvent(&e); };
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
                static auto cardP = [&](const std::string& n) { CardRef r = findRef(n); return g_boardOrigin + board.get(r).pos; };
                static auto bw = []() { return (240 * s - 24 * s - 13 * s) / 2; };
                static auto btnY = []() { return 58 * s + g_lineH * 1.2f + 8 * s + 14 * s + 12 * s + 14 * s; };
                static auto dmgPt = [&](const std::string& n) { ImVec2 p = cardP(n); return ImVec2(p.x + 12 * s + bw() / 2, p.y + btnY()); };
                static auto bodyPt = [&](const std::string& n) { ImVec2 p = cardP(n); return ImVec2(p.x + 200 * s, p.y + 22 * s); };
                static auto pipPt = [&](const std::string& n, int group, int i) {
                    ImVec2 p = cardP(n); const float d = 13 * s, gap = 4 * s, gg = 12 * s;
                    float x = p.x + 240 * s - 12 * s - (d * 6 + gap * 4 + gg);
                    if (group == 1) x += 3 * (d + gap) + gg - gap;
                    x += i * (d + gap) + d / 2;
                    return ImVec2(x, p.y + 58 * s + g_lineH * 0.6f);
                };
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

                act(10, [&]() { g_shot = "01_default"; });
                // 1) Card damage with a plain amount
                setAmount(3);
                clickAt([=]() { return dmgPt("Goblin"); });
                act(2, [&]() { check(C("Goblin")->getCurrentHp() == 4, "card Damage button deals the amount: Goblin 7 -> 4 (hp " + std::to_string(C("Goblin")->getCurrentHp()) + ")");
                               bool found = false; for (auto& e : log) if (e.text.find("Goblin takes 3") != std::string::npos) found = true; check(found, "log records the hit"); });
                act(80, [&]() {});
                // 2) Box-select from the empty top-left corner
                act(2, [&]() { motion(g_boardOrigin.x + 8 * s, g_boardOrigin.y + 8 * s); button(true, g_boardOrigin.x + 8 * s, g_boardOrigin.y + 8 * s); });
                act(2, [&]() { motion(g_boardOrigin.x + 320 * s, g_boardOrigin.y + 120 * s); });
                act(1, [&]() { g_shot = "02_marquee"; });
                act(3, [&]() { button(false, g_boardOrigin.x + 320 * s, g_boardOrigin.y + 120 * s); });
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
                act(2, [&]() { ImVec2 p = bodyPt("Orc"); motion(p.x, p.y); SDL_Event e{}; e.type = SDL_MOUSEBUTTONDOWN; e.button.windowID = SDL_GetWindowID(window); e.button.button = SDL_BUTTON_RIGHT; e.button.state = SDL_PRESSED; e.button.clicks = 1; e.button.x = (int)p.x; e.button.y = (int)p.y; SDL_PushEvent(&e); });
                act(2, [&]() { ImVec2 p = bodyPt("Orc"); SDL_Event e{}; e.type = SDL_MOUSEBUTTONUP; e.button.windowID = SDL_GetWindowID(window); e.button.button = SDL_BUTTON_RIGHT; e.button.state = SDL_RELEASED; e.button.x = (int)p.x; e.button.y = (int)p.y; SDL_PushEvent(&e); });
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
                // 9) Conditions on the Orc via the sidebar, then End round
                clickAt([=]() { return bodyPt("Orc"); });
                act(3, [&]() { selCondition = 10; selRounds = 1; g_scrollTo = "Apply condition"; });
                clickAt([=]() { return rectCenter("Apply condition"); });
                act(1, [&]() { selCondition = 11; selRounds = 0; });
                clickAt([=]() { return rectCenter("Apply condition"); });
                act(1, [&]() { check(C("Orc")->hasCondition("Poisoned") && C("Orc")->hasCondition("Prone"), "Poisoned (1 round) and Prone applied to Orc"); C("Orc")->setNotes("Carries the key"); C("Orc")->setDamageMod(DamageMod::Resistant); });
                act(40, [&]() { g_shot = "04_states"; g_scrollTo = "End round"; });
                act(3, [&]() {});
                clickAt([=]() { return rectCenter("End round"); });
                act(2, [&]() { g_scrollTo.clear(); g_shot = "04b_toasts"; });
                // clicking where the toasts are must not dismiss them (they only fade)
                act(1, [&]() { std::printf("  alerts on screen: %zu\n", alerts.size()); });
                clickAt([=]() { return ImVec2(g_boardOrigin.x + g_boardSize.x - 22 * s - 170 * s, g_boardOrigin.y + 16 * s + 14 * s); });
                act(1, [&]() { check(alertHas("Orc: Poisoned ended"), "clicking a toast does not dismiss it"); });
                act(2, [&]() { check(board.round == 2 && !C("Orc")->hasCondition("Poisoned") && C("Orc")->hasCondition("Prone") && alertHas("Orc: Poisoned ended"), "End round: round 2, Poisoned expired with alert, Prone stays"); });
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
                act(2, [&]() { check(exists("Goblin 1") && exists("Goblin 4") && !exists("Goblin"), "library x3: Goblin 2..4 added, Goblin renamed Goblin 1"); });
                // 13) Edit via double-click
                act(2, [&]() { ImVec2 p = bodyPt("Hero"); motion(p.x, p.y); button(true, p.x, p.y); });
                act(1, [&]() { ImVec2 p = bodyPt("Hero"); button(false, p.x, p.y); });
                act(1, [&]() { ImVec2 p = bodyPt("Hero"); SDL_Event e{}; e.type = SDL_MOUSEBUTTONDOWN; e.button.windowID = SDL_GetWindowID(window); e.button.button = SDL_BUTTON_LEFT; e.button.state = SDL_PRESSED; e.button.clicks = 2; e.button.x = (int)p.x; e.button.y = (int)p.y; SDL_PushEvent(&e); });
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
                // 16) Tidy board (shrinks cards to fit everything)
                act(2, [&]() { tidyBoard(); });
                act(40, [&]() {
                    bool overlap = false;
                    const ImVec2 cs = tokenSize();
                    std::vector<ImVec2> ps; for (auto& st : board.players) ps.push_back(st.pos); for (auto& st : board.creatures) ps.push_back(st.pos);
                    for (size_t i = 0; i < ps.size(); i++) for (size_t j = i + 1; j < ps.size(); j++)
                        if (std::fabs(ps[i].x - ps[j].x) < cs.x && std::fabs(ps[i].y - ps[j].y) < cs.y) overlap = true;
                    bool inside = true; for (auto& p : ps) if (p.y + cs.y > g_boardSize.y + 1 || p.x + cs.x > g_boardSize.x + 1) inside = false;
                    check(!overlap && inside, "Tidy board: no overlaps, all inside (card size " + std::to_string((int)std::lround(g_cardScale * 100)) + "%)");
                    g_shot = "09_tidy"; });
                // 17) Tabs
                act(3, [&]() { g_forceTab = 1; });
                act(3, [&]() { g_shot = "07_add"; });
                act(2, [&]() { g_forceTab = 2; });
                act(3, [&]() { g_shot = "08_log"; });
                act(2, [&]() { g_forceTab = 0; });
                // 18) Alerts fade out on their own
                act(2, [&]() { addAlert("Fade test", COL_AMBER); });
                waitSeconds(3.0);
                act(1, [&]() { const Alert* a = alertNamed("Fade test"); check(a && alertOpacity(*a, ImGui::GetTime()) == 1.0f, "alert fully visible after 3 s"); });
                waitSeconds(3.9);
                act(1, [&]() { const Alert* a = alertNamed("Fade test"); const float o = a ? alertOpacity(*a, ImGui::GetTime()) : -1.0f;
                               check(a && o > 0.0f && o < 1.0f, "alert is fading after ~7 s (opacity " + std::to_string(o) + ")"); g_shot = "10_fading"; });
                waitSeconds(1.5);
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
