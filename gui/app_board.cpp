#include "app.h"

#include <algorithm>
#include <cmath>

#include "imgui_internal.h"
#include "board_view.h"
#include "difficulty.h"
#include "ui_kit.h"

// The board: the Turn order list on the left; on the right the spotlight (one card
// shown big, or the setup panel before combat) above the Defeated column.
void App::drawBoard(float height, float panelRound, bool anyPopup) {
    const float S = UI_SCALE;
    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 boardChildSize(ImGui::GetContentRegionAvail().x - 10 * S, height);
    {
        // Hard ink shadow behind the board panel.
        const ImVec2 bp = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(bp + ImVec2(4 * S, 5 * S), bp + boardChildSize + ImVec2(4 * S, 5 * S),
                                                  COL_INK, panelRound);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, panelRound);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, toVec4(COL_BOARD));
    ImGui::BeginChild("Board", boardChildSize, true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    const ImVec2 boardOrigin = ImGui::GetCursorScreenPos();
    const ImVec2 boardSize = ImGui::GetContentRegionAvail();
    const float dtB = std::min(io.DeltaTime, 0.05f);
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool shiftLike = io.KeyShift || io.KeyCtrl || io.KeySuper;
    const bool mouseOnBoard = !anyPopup && ImGui::IsWindowHovered() &&
                              mouse.x >= boardOrigin.x && mouse.x <= boardOrigin.x + boardSize.x &&
                              mouse.y >= boardOrigin.y && mouse.y <= boardOrigin.y + boardSize.y;

    // ---- Layout, in board coordinates: the Turn order list on the left; on the
    // right the spotlight (one card shown big, or the setup panel) above Defeated.
    const float colMargin = 16 * S, colGap = 14 * S, colHeader = 44 * S, colPad = 10 * S;
    const float turnColW = std::clamp(std::floor((boardSize.x - 2 * colMargin - colGap) * 0.47f), 320 * S, 520 * S);
    const float rightX = colMargin + turnColW + colGap;
    const float rightW = std::max(220 * S, boardSize.x - colMargin - rightX);
    // The spotlight's height. Before combat it grows to fit the setup panel (its
    // buttons must stay inside the box); Defeated is empty then and gives up the room.
    float spotTarget = std::clamp(std::floor(boardSize.y * 0.52f), 300 * S, 430 * S);
    if (!board.inCombat && setupPanelH > 0.0f) {
        const float most = boardSize.y - 2 * colMargin - colGap - colHeader - 40 * S;
        spotTarget = std::max(spotTarget, std::min(setupPanelH, most));
    }
    if (spotH < 0.0f) spotH = spotTarget;
    spotH = approach(spotH, spotTarget, 14.0f, dtB);
    if (std::fabs(spotH - spotTarget) < 0.5f) spotH = spotTarget;
    const ImVec2 spotMin(rightX, colMargin), spotMax(rightX + rightW, colMargin + spotH);
    const ImVec2 colMin[2] = {ImVec2(colMargin, colMargin), ImVec2(rightX, colMargin + spotH + colGap)};
    const ImVec2 colMax[2] = {ImVec2(colMargin + turnColW, boardSize.y - colMargin),
                              ImVec2(rightX + rightW,
                                     std::max(colMargin + spotH + colGap + colHeader + 40 * S, boardSize.y - colMargin))};
    // Area the cards live in (screen coordinates); a little extra on top for the turn ribbon.
    ImVec2 areaMin[2], areaMax[2];
    for (int col = 0; col < 2; col++) {
        areaMin[col] = boardOrigin + ImVec2(colMin[col].x + colPad, colMin[col].y + colHeader);
        areaMax[col] = boardOrigin + ImVec2(colMax[col].x - colPad, colMax[col].y - colPad);
    }
    const float cardH = cardHeight();
    const float slotH = cardH + 11 * S;

    // Releasing a dragged card drops it into the gap in the Turn order (reordering it).
    // This runs before the layout so everything below sees the updated order.
    if (board.dragValid && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (board.get(board.drag).column == 0 && dragSlot >= 0) {
            board.placeInTurnOrder(board.drag, dragSlot);
            if (snapshot() != pressSnapshot) pushUndoState(pressSnapshot);
        }
        board.dragValid = false;
    }

    ensureTurn();
    // The spotlight follows the turn whenever it moves (and can be pointed at any card by clicking it).
    if (board.inCombat && board.turnValid && !(spotTurnValid && spotTurn == board.turn)) {
        board.focus = board.turn;
        board.focusValid = true;
    }
    spotTurn = board.turn;
    spotTurnValid = board.inCombat && board.turnValid;
    std::vector<CardRef> colList[2];
    for (const CardRef& r : board.z) {
        CardState& st = board.get(r);
        // A dead card drops to Defeated once its death animation has played a moment.
        st.column = (st.wasDead && st.deathT >= 0.7f) ? 1 : 0;
        colList[st.column].push_back(r);
    }
    const bool dragging = board.dragValid;
    const CardRef dragRef = board.drag;

    // Scrolling: the wheel scrolls the column under the mouse (eased).
    if (mouseOnBoard && io.MouseWheel != 0.0f) {
        for (int col = 0; col < 2; col++) {
            const ImVec2 a = boardOrigin + colMin[col], b = boardOrigin + colMax[col];
            if (mouse.x >= a.x && mouse.x <= b.x && mouse.y >= a.y && mouse.y <= b.y) {
                board.scrollTarget[col] -= io.MouseWheel * 5.0f * ImGui::GetFontSize();
            }
        }
    }
    float contentH[2], viewH[2];
    for (int col = 0; col < 2; col++) {
        contentH[col] = static_cast<float>(colList[col].size()) * slotH + 8 * S;
        viewH[col] = areaMax[col].y - areaMin[col].y;
    }
    // Bring the card that holds the turn into view after the turn changed.
    if (scrollToTurn) {
        scrollToTurn = false;
        int i = 0;
        for (const CardRef& r : colList[0]) {
            if (board.turnValid && r == board.turn) {
                const float top = static_cast<float>(i) * slotH;
                if (top < board.scrollTarget[0]) board.scrollTarget[0] = top - 6 * S;
                else if (top + cardH > board.scrollTarget[0] + viewH[0]) board.scrollTarget[0] = top + cardH - viewH[0] + 8 * S;
            }
            i++;
        }
    }
    for (int col = 0; col < 2; col++) {
        const float maxScroll = std::max(0.0f, contentH[col] - viewH[col]);
        board.scrollTarget[col] = std::clamp(board.scrollTarget[col], 0.0f, maxScroll);
        board.scroll[col] = approach(board.scroll[col], board.scrollTarget[col], 18.0f, dtB);
        if (std::fabs(board.scroll[col] - board.scrollTarget[col]) < 0.3f) board.scroll[col] = board.scrollTarget[col];
        board.scroll[col] = std::clamp(board.scroll[col], 0.0f, maxScroll);
    }

    // Where a card being dragged would land in the Turn order (a gap opens there).
    int gapSlot = -1;
    if (dragging && board.get(dragRef).column == 0) {
        const float topY = boardOrigin.y + board.get(dragRef).pos.y;
        const float listTop = areaMin[0].y - board.scroll[0];
        const int others = static_cast<int>(colList[0].size()) - 1;
        gapSlot = std::clamp(static_cast<int>(std::lround((topY - listTop) / slotH)), 0, std::max(0, others));
    }
    dragSlot = gapSlot;

    // Targets for every card; cards glide there (the dragged one follows the mouse).
    for (int col = 0; col < 2; col++) {
        const float cardW = std::max(150 * S, (areaMax[col].x - areaMin[col].x) - 14 * S);
        int slot = 0;
        for (const CardRef& r : colList[col]) {
            CardState& st = board.get(r);
            st.size = ImVec2(cardW, cardH);
            const bool isDragged = dragging && r == dragRef;
            if (!isDragged && col == 0 && gapSlot >= 0 && slot == gapSlot) slot++;  // leave the gap open
            const bool holdsTurn = board.inCombat && col == 0 && board.turnValid && r == board.turn;
            const float stepForward = holdsTurn ? 8.0f * S * easeOutCubic(st.turnAnim) : 0.0f;
            st.target = ImVec2(colMin[col].x + colPad + 2 * S + stepForward,
                               colMin[col].y + colHeader + static_cast<float>(slot) * slotH - board.scroll[col]);
            if (!isDragged) slot++;
            if (!st.placed) {
                st.pos = st.target;
                st.placed = true;
            } else if (!isDragged) {
                st.pos.x = approach(st.pos.x, st.target.x, 16.0f, dtB);
                st.pos.y = approach(st.pos.y, st.target.y, 16.0f, dtB);
                if (std::fabs(st.pos.x - st.target.x) < 0.4f) st.pos.x = st.target.x;
                if (std::fabs(st.pos.y - st.target.y) < 0.4f) st.pos.y = st.target.y;
            }
            // Clipped to its column, except while it is dragged or crossing to the other one.
            const bool crossing = std::fabs(st.pos.x - st.target.x) > 6 * S;
            if (isDragged || crossing) {
                st.clipMin = boardOrigin;
                st.clipMax = boardOrigin + boardSize;
            } else {
                st.clipMin = ImVec2(areaMin[col].x - 4 * S, areaMin[col].y - 14 * S);
                st.clipMax = ImVec2(areaMax[col].x + 10 * S, areaMax[col].y + 4 * S);
            }
        }
    }

    // The screen rect of a card, and whether a point is on its visible part.
    auto cardRectContains = [&](CardRef r, ImVec2 pt) {
        const CardState& st = board.get(r);
        const ImVec2 a = boardOrigin + st.pos;
        const ImVec2 b = a + st.size;
        return pt.x >= std::max(a.x, st.clipMin.x) && pt.x <= std::min(b.x, st.clipMax.x) &&
               pt.y >= std::max(a.y, st.clipMin.y) && pt.y <= std::min(b.y, st.clipMax.y);
    };
    // The TURN ribbon sticks out above its card: clicks there belong to the ribbon.
    auto overRibbon = [&](ImVec2 pt) {
        for (const CardRef& r : board.z) {
            const CardState& st = board.get(r);
            if (st.column != 0 || st.turnAnim < 0.5f) continue;
            if (std::fabs(pt.x - st.turnCenter.x) <= 48 * S && std::fabs(pt.y - st.turnCenter.y) <= 12 * S) return true;
        }
        return false;
    };
    auto topCardAt = [&](ImVec2 pt) {
        for (int k = static_cast<int>(board.z.size()) - 1; k >= 0; k--) {
            if (cardRectContains(board.z[k], pt)) return k;
        }
        return -1;
    };
    // ---- Column panels (behind the cards).
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const char* titles[2] = {"Turn order", "Defeated"};
        for (int col = 0; col < 2; col++) {
            const ImVec2 a = boardOrigin + colMin[col], b = boardOrigin + colMax[col];
            inkBox(dl, a, b, 18 * S, S, IM_COL32(241, 241, 237, 255), 3.0f);
            const ImU32 titleCol = col == 0 ? COL_TEXT : COL_MUTED;
            const ImVec2 ts = heavyTextSize(titles[col], 1.0f);
            heavyText(dl, ImVec2(a.x + 16 * S, a.y + 12 * S), titleCol, titles[col], 1.0f);
            const std::string count = std::to_string(colList[col].size());
            dl->AddText(ImVec2(a.x + 16 * S + ts.x + 8 * S, a.y + 12 * S + (ts.y - ImGui::GetTextLineHeight()) * 0.5f),
                        COL_MUTED, count.c_str());
            if (col == 0) {
                const std::string roundText = board.inCombat ? "Round " + std::to_string(board.round) : std::string("Setup");
                const ImVec2 rs = boldTextSize(roundText.c_str(), 0.95f);
                boldText(dl, ImVec2(b.x - 16 * S - rs.x, a.y + 12 * S + (ts.y - rs.y) * 0.5f), COL_MUTED,
                         roundText.c_str(), 0.95f);
            }
            if (colList[col].empty()) {
                const char* hint = col == 0 ? "Right-click to add a character" : "Defeated creatures fall here";
                const ImVec2 hs = ImGui::CalcTextSize(hint);
                dl->AddText(ImVec2((a.x + b.x - hs.x) * 0.5f, a.y + colHeader + 30 * S), COL_MUTED, hint);
            }
            // A slim scroll indicator when the column overflows.
            if (contentH[col] > viewH[col] + 1.0f) {
                const float trackH = viewH[col];
                const float thumbH = std::max(24 * S, trackH * viewH[col] / contentH[col]);
                const float maxScroll = contentH[col] - viewH[col];
                const float y0 = areaMin[col].y + (trackH - thumbH) * (board.scroll[col] / maxScroll);
                const float x1 = b.x - 5 * S;
                dl->AddRectFilled(ImVec2(x1 - 4 * S, y0), ImVec2(x1, y0 + thumbH), COL_BORDER_HI, 2 * S);
            }
        }
        {
            const bool showsCard = board.inCombat && board.focusValid && encounter.at(board.focus.side, board.focus.index);
            drawSpotlightBox(boardOrigin + spotMin, boardOrigin + spotMax, encounter, showsCard ? &board.focus : nullptr, S);
        }
        // The gap a dragged card would drop into.
        if (gapSlot >= 0) {
            const float gy = boardOrigin.y + colMin[0].y + colHeader + static_cast<float>(gapSlot) * slotH - board.scroll[0];
            const ImVec2 a(boardOrigin.x + colMin[0].x + colPad + 2 * S, gy);
            const ImVec2 b(a.x + board.get(dragRef).size.x, gy + cardH);
            dl->PushClipRect(ImVec2(areaMin[0].x - 4 * S, areaMin[0].y), ImVec2(areaMax[0].x + 10 * S, areaMax[0].y), true);
            dl->AddRectFilled(a, b, withAlpha(COL_INK, 14), 12 * S);
            dl->AddRect(a, b, withAlpha(COL_INK, 70), 12 * S, 0, 1.5f * S);
            dl->PopClipRect();
        }
    }

    // Alerts: clicking one fades it away. While the mouse is over one, the
    // cards and the board underneath ignore it (rects are from last frame).
    Alert* hotToast = nullptr;
    {
        const double now = ImGui::GetTime();
        for (Alert& al : alerts) {
            al.hot = false;
            const bool inside = mouse.x >= al.rectMin.x && mouse.x <= al.rectMax.x &&
                                mouse.y >= al.rectMin.y && mouse.y <= al.rectMax.y;
            if (mouseOnBoard && inside && al.clicked < 0.0 && alertOpacity(al, now) > 0.3f) hotToast = &al;
        }
    }
    const bool mouseOverToast = hotToast != nullptr;
    if (mouseOverToast) {
        hotToast->hot = true;
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) hotToast->clicked = ImGui::GetTime();
    }

    // Press: remember the state (for undo), or start a selection box on empty space.
    if (mouseOnBoard && !mouseOverToast && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        pressSnapshot = snapshot();
        if (topCardAt(mouse) < 0 && !overRibbon(mouse)) {
            marqueeActive = true;
            marqueeStart = mouse;
        }
    }
    // Right-click: the card's menu, or the board menu (New character...) on empty space.
    if (mouseOnBoard && !mouseOverToast && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        const int k = topCardAt(mouse);
        if (k >= 0) {
            menuRef = board.z[k];
            if (!board.get(menuRef).selected) {
                board.clearSelection();
                board.get(menuRef).selected = true;
            }
            openMenu = true;
        } else if (!overRibbon(mouse)) {
            // Insert at the slot under the cursor when it is over the Turn order column.
            boardMenuSlot = -1;
            const ImVec2 a = boardOrigin + colMin[0], b = boardOrigin + colMax[0];
            if (mouse.x >= a.x && mouse.x <= b.x && mouse.y >= a.y && mouse.y <= b.y) {
                int above = 0;
                for (const CardRef& r : colList[0]) {
                    const CardState& st = board.get(r);
                    if (boardOrigin.y + st.target.y + cardH * 0.5f < mouse.y) above++;
                }
                boardMenuSlot = above;
            }
            openBoardMenu = true;
        }
    }

    // Card actions (each one undoable).
    const std::string amountLabel = std::to_string(actionAmount);
    CardActions actions;
    actions.damage = [&](CardRef r) {
        pushUndo();
        applyHit(r, actionAmount);
    };
    actions.heal = [&](CardRef r) {
        pushUndo();
        applyHeal(r, actionAmount);
    };
    actions.setDeathSaves = [&](CardRef r, int s, int f) {
        pushUndo();
        Creature* c = encounter.at(r.side, r.index);
        c->setDeathSaves(s, f);
        if (s >= 3) addLog(c->getName() + " is stable", COL_GREEN);
        if (f >= 3) addAlert(c->getName() + " has died", COL_RED);
    };
    actions.nextTurn = [&]() { nextTurn(); };
    actions.temp = [&](CardRef r) {
        pushUndo();
        applyTemp(r, actionAmount);
    };
    actions.edit = [&](CardRef r) { openEditor(r); };

    // Draw order: Turn order cards, Defeated cards, then the dragged card on top.
    std::vector<CardRef> drawOrder;
    for (int col = 0; col < 2; col++) {
        for (const CardRef& r : colList[col]) {
            if (!(dragging && r == dragRef)) drawOrder.push_back(r);
        }
    }
    if (dragging) drawOrder.push_back(dragRef);
    // Overlapping cards (a dragged one, or ones mid-glide): the card drawn last wins
    // the mouse, because ImGui gives overlapping presses to whichever item is
    // submitted first, so a card is made non-interactive whenever one in front of it
    // also covers the mouse position.
    for (size_t k = 0; k < drawOrder.size(); k++) {
        bool covered = false;
        for (size_t j = k + 1; j < drawOrder.size() && !covered; j++) {
            covered = cardRectContains(drawOrder[j], mouse);
        }
        const CardRef r = drawOrder[k];
        CardState& st = board.get(r);
        const bool isDraggedCard = dragging && r == dragRef;
        const bool holdsTurn = board.inCombat && st.column == 0 && board.turnValid && r == board.turn;
        const bool isFocus = board.inCombat && board.focusValid && r == board.focus;
        if (!isDraggedCard) ImGui::PushClipRect(st.clipMin, st.clipMax, true);
        drawToken(encounter, r, st, boardOrigin, amountLabel,
                  !covered && !marqueeActive && !mouseOverToast, holdsTurn, isFocus, actions);
        if (!isDraggedCard) ImGui::PopClipRect();
    }
    // Effects (floating numbers, crit bursts) always sit above every card.
    {
        CardFontScope cardFont;
        for (const CardRef& r : drawOrder) {
            const CardState& st = board.get(r);
            const ImVec2 a = boardOrigin + st.pos;
            const bool visible = a.y + st.size.y > st.clipMin.y && a.y < st.clipMax.y;
            if (visible) drawCardEffects(st, st.size, cardS());
        }
    }

    // ---- Spotlight: the focused card in combat, or the setup panel before it.
    {
        const ImVec2 a = boardOrigin + spotMin, b = boardOrigin + spotMax;
        if (board.inCombat) {
            if (board.focusValid && encounter.at(board.focus.side, board.focus.index)) {
                const bool holdsTurn = board.turnValid && board.focus == board.turn;
                drawSpotlight(encounter, board.focus, board.get(board.focus), a, b, amountLabel, holdsTurn, actions);
            } else {
                const char* hint = "Click a card to see it here";
                const ImVec2 hs = ImGui::CalcTextSize(hint);
                ImGui::GetWindowDrawList()->AddText(ImVec2((a.x + b.x - hs.x) * 0.5f, (a.y + b.y - hs.y) * 0.5f), COL_MUTED, hint);
            }
        } else {
            drawSetupPanel(a, b);
        }
    }

    // Clicks select (and point the spotlight at the card); double-clicks open the editor.
    for (const CardRef& r : board.z) {
        CardState& st = board.get(r);
        if (st.pressed && !shiftLike && !st.selected) {
            board.clearSelection();
            st.selected = true;
        }
        if (st.clicked) {
            if (shiftLike) {
                st.selected = !st.selected;
            } else {
                board.clearSelection();
                st.selected = true;
            }
            board.focus = r;
            board.focusValid = true;
        }
        if (st.wantsEdit) openEditor(r);
    }

    // Selection box on empty space.
    if (marqueeActive) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 a(std::min(marqueeStart.x, mouse.x), std::min(marqueeStart.y, mouse.y));
        const ImVec2 b(std::max(marqueeStart.x, mouse.x), std::max(marqueeStart.y, mouse.y));
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            dl->AddRectFilled(a, b, withAlpha(COL_SELECT, 38), 6 * S);
            dl->AddRect(a, b, COL_SELECT, 6 * S, 0, 1.5f * S);
        } else {
            const bool tiny = (b.x - a.x) < 4 && (b.y - a.y) < 4;
            if (!shiftLike) board.clearSelection();
            if (!tiny) {
                for (const CardRef& r : board.z) {
                    const CardState& st = board.get(r);
                    const ImVec2 cp = boardOrigin + st.pos;
                    // Only the part of the card that is visible in its column counts.
                    const ImVec2 lo(std::max(cp.x, st.clipMin.x), std::max(cp.y, st.clipMin.y));
                    const ImVec2 hi(std::min(cp.x + st.size.x, st.clipMax.x), std::min(cp.y + st.size.y, st.clipMax.y));
                    const bool overlaps = lo.x < hi.x && lo.y < hi.y && lo.x < b.x && hi.x > a.x &&
                                          lo.y < b.y && hi.y > a.y;
                    if (overlaps) board.get(r).selected = true;
                }
            }
            marqueeActive = false;
        }
    }

    // Remember which card is being dragged (read next frame on mouse release).
    board.dragValid = false;
    for (const CardRef& r : board.z) {
        if (board.get(r).beingDragged) {
            board.drag = r;
            board.dragValid = true;
        }
    }
    {
        const double now = ImGui::GetTime();
        alerts.erase(std::remove_if(alerts.begin(), alerts.end(), [&](const Alert& a) { return alertExpired(a, now); }),
                     alerts.end());
        drawToasts(alerts, boardOrigin, boardSize, now, std::min(io.DeltaTime, 0.05f), S);
    }
    ImGui::EndChild();
}

// The panel in the spotlight's place before combat: what is set up, how to set
// it up, and the Start combat button.
void App::drawSetupPanel(ImVec2 a, ImVec2 b) {
    const float S = UI_SCALE;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float pad = 22 * S;
    const float x0 = a.x + pad, x1 = b.x - pad;
    const float lineH = ImGui::GetTextLineHeight();
    float y = a.y + pad;
    const char* title = "Set up the encounter";
    heavyText(dl, ImVec2(x0, y), COL_TEXT, title, 1.45f);
    y += heavyTextSize(title, 1.45f).y + 14 * S;

    int players = 0, enemies = 0;
    std::vector<int> levels, xps;
    for (const Creature& c : encounter.getPlayers()) {
        players++;
        levels.push_back(c.getLevel());
    }
    for (const Creature& c : encounter.getCreatures()) {
        enemies++;
        xps.push_back(crXp(c.getCrIndex()));
    }
    const std::string counts = std::to_string(players) + (players == 1 ? " player" : " players") + "   \xC2\xB7   " +
                               std::to_string(enemies) + (enemies == 1 ? " enemy" : " enemies");
    boldText(dl, ImVec2(x0, y), COL_TEXT, counts.c_str());
    y += lineH + 6 * S;
    const DifficultyReport dr = rateEncounter(levels, xps);
    const std::string diff = dr.valid ? std::string("Difficulty: ") + difficultyName(dr.rating) + "   \xC2\xB7   " +
                                            std::to_string(dr.adjustedXp) + " adjusted XP"
                                      : std::string("Difficulty: set player levels and monster CRs");
    dl->AddText(ImVec2(x0, y), COL_MUTED, diff.c_str());
    y += lineH + 20 * S;

    const char* tips[] = {
        "Right-click the list to add a new character, or a monster from the library.",
        "Drag the cards into turn order: the top card acts first.",
        "Double-click a card to edit it.",
    };
    for (const char* tip : tips) {
        dl->AddCircleFilled(ImVec2(x0 + 4 * S, y + lineH * 0.5f), 3 * S, COL_INK);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(x0 + 16 * S, y), COL_TEXT, tip, nullptr, x1 - x0 - 16 * S);
        y += ImGui::CalcTextSize(tip, nullptr, false, x1 - x0 - 16 * S).y + 10 * S;
    }

    // Buttons at the bottom of the panel, but never over the text above them.
    const float bh = 46 * S;
    const float ph = 34 * S;
    const float gap = 10 * S;
    const float total = ph + gap + (party.empty() ? 0.0f : ph + gap) + bh;
    // The height this panel needs (read by the board layout next frame).
    setupPanelH = (y + 6 * S + total + pad) - a.y;
    float by = std::max(y + 6 * S, b.y - pad - total);
    {
        const std::string label = playerServer.running()
            ? "Player screens: " + std::to_string(playerServer.viewers()) + " connected##setup"
            : std::string("Show the QR for player screens##setup");
        ImGui::SetCursorScreenPos(ImVec2(x0, by));
        if (quietButton(label.c_str(), ImVec2(x1 - x0, ph))) openShare = true;
        by += ph + gap;
    }
    float startY = by + (party.empty() ? 0.0f : ph + gap);
    if (!party.empty()) {
        const std::vector<PlayerTemplate> missing = partyMissing();
        const float partyY = by;
        ImGui::SetCursorScreenPos(ImVec2(x0, partyY));
        const std::string label = missing.empty() ? std::string("The whole party is here##setup")
                                                  : "Add the party (" + std::to_string(missing.size()) + ")##setup";
        if (quietButton(label.c_str(), ImVec2(x1 - x0, ph), !missing.empty())) addPartyMembers(missing);
    }
    ImGui::SetCursorScreenPos(ImVec2(x0, startY));
    if (primaryButton("Start combat", ImVec2(x1 - x0, bh), anyoneStanding())) startCombat();
}
