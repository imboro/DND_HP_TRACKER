#include "board_view.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

#include "difficulty.h"
#include "looks.h"
#include "ui_kit.h"

float g_cardScale = 1.0f;
float cardS() { return UI_SCALE * g_cardScale; }

// ---- Animation helpers -----------------------------------------------------

static const float CRIT_FRACTION = 0.40f;  // hit/heal of >= 40% max HP is a "crit"
static const float ANIM_EXTEND = 0.18f;    // pre-bar extends
static const float ANIM_HOLD = 0.55f;      // then holds until here
static const float ANIM_DROP = 0.45f;      // then the main bar drops/rises
static const float ANIM_TOTAL = ANIM_HOLD + ANIM_DROP + 0.1f;

// What the bar should look like right now, as fractions of max HP.
struct BarVis {
    float fillEnd;    // solid fill goes from 0 to here
    float chunkEnd;   // pre-bar chunk spans [fillEnd, chunkEnd]
    bool hasChunk;
};

// Fills the part of a pill-shaped bar [barMin, barMax] between x0 and x1, following
// its rounded ends (a plain rectangle would poke out of the outline at the corners).
static void fillBarSpan(ImDrawList* dl, ImVec2 barMin, ImVec2 barMax, float x0, float x1, ImU32 col) {
    x0 = std::max(x0, barMin.x);
    x1 = std::min(x1, barMax.x);
    if (x1 - x0 < 0.5f) return;
    const float r = (barMax.y - barMin.y) * 0.5f;
    const float cy = barMin.y + r;
    const float leftCap = barMin.x + r, rightCap = barMax.x - r;  // centers of the round ends
    auto halfHeight = [&](float x) {
        const float dx = x < leftCap ? leftCap - x : (x > rightCap ? x - rightCap : 0.0f);
        return std::sqrt(std::max(0.0f, r * r - dx * dx));
    };
    // Sample densely across the round ends; the straight middle needs no points.
    std::vector<float> xs = {x0, x1};
    for (int i = 0; i <= 10; i++) {
        const float t = static_cast<float>(i) / 10.0f;
        xs.push_back(barMin.x + r * t);
        xs.push_back(rightCap + r * t);
    }
    std::sort(xs.begin(), xs.end());
    std::vector<ImVec2> pts;
    for (float x : xs) {
        if (x >= x0 && x <= x1) pts.push_back(ImVec2(x, cy - halfHeight(x)));
    }
    for (auto it = xs.rbegin(); it != xs.rend(); ++it) {
        if (*it >= x0 && *it <= x1) pts.push_back(ImVec2(*it, cy + halfHeight(*it)));
    }
    dl->AddConvexPolyFilled(pts.data(), static_cast<int>(pts.size()), col);
}

static BarVis computeBar(const CardState& st, int hp, int maxHp) {
    BarVis v{0.0f, 0.0f, false};
    if (maxHp <= 0) return v;
    const float M = static_cast<float>(maxHp);
    v.fillEnd = v.chunkEnd = std::min(1.0f, static_cast<float>(hp) / M);
    if (!st.active || st.oldHp == st.newHp) return v;

    const float oldF = std::min(1.0f, static_cast<float>(st.oldHp) / M);
    const float newF = std::min(1.0f, static_cast<float>(st.newHp) / M);
    const float ext = easeOutCubic(clamp01(st.t / ANIM_EXTEND));
    const float drop = easeInOutCubic(clamp01((st.t - ANIM_HOLD) / ANIM_DROP));
    v.hasChunk = true;
    if (!st.heal) {
        // Chunk extends left from the old edge, holds, then the old edge drops onto it.
        v.fillEnd = lerpf(oldF, newF, ext);
        v.chunkEnd = lerpf(oldF, newF, drop);
    } else {
        // Chunk extends right from the old edge, holds, then the fill rises into it.
        v.fillEnd = lerpf(oldF, newF, drop);
        v.chunkEnd = lerpf(oldF, newF, ext);
    }
    return v;
}

// Notices an HP change (or a queued label like "IMMUNE") and (re)starts the
// animation; advances timers.
static void updateCardFx(CardState& st, int hp, int maxHp, float dt) {
    if (st.lastHp < 0) st.lastHp = hp;
    const bool changed = hp != st.lastHp;
    if (changed || !st.pendingLabel.empty()) {
        st.oldHp = st.lastHp;
        st.newHp = hp;
        st.heal = changed ? hp > st.lastHp : st.pendingKind == FxKind::Heal;
        st.delta = std::abs(hp - st.lastHp);
        if (st.pendingLabel.empty()) {
            st.fxKind = st.heal ? FxKind::Heal : FxKind::Damage;
            st.fxLabel = (st.heal ? "+" : "-") + std::to_string(st.delta);
        } else {
            st.fxKind = st.pendingKind;
            st.fxLabel = st.pendingLabel;
        }
        const int measured = st.requested > 0 ? st.requested : st.delta;
        st.crit = changed && (st.fxKind == FxKind::Damage || st.fxKind == FxKind::Heal) && maxHp > 0 &&
                  static_cast<float>(measured) >= CRIT_FRACTION * static_cast<float>(maxHp);
        st.pendingLabel.clear();
        st.requested = 0;
        st.t = 0.0f;
        st.active = true;
        st.lastHp = hp;
    } else if (st.active) {
        st.t += dt;
        if (st.t > ANIM_TOTAL) st.active = false;
    }
}

static ImVec2 shakeOffset(const CardState& st, float S) {
    if (!st.active) return ImVec2(0, 0);
    const float t = st.t;
    if (st.fxKind == FxKind::Damage || st.fxKind == FxKind::Temp) {
        const float dur = st.crit ? 0.5f : 0.18f;
        if (t >= dur) return ImVec2(0, 0);
        const float amp = (st.crit ? 6.0f : 1.8f) * S * (1.0f - t / dur);
        return ImVec2(std::sin(t * 70.0f) * amp, std::cos(t * 91.0f) * amp * 0.6f);
    }
    if (st.fxKind == FxKind::Heal && st.crit && t < 0.6f) {  // heal crit: a little hop
        return ImVec2(0, -std::fabs(std::sin(t * 3.14159f * 3.0f)) * 9.0f * S * (1.0f - t / 0.6f));
    }
    return ImVec2(0, 0);
}

static ImU32 fxColor(FxKind k) {
    switch (k) {
        case FxKind::Damage: return COL_RED;
        case FxKind::Heal:   return COL_GREEN;
        case FxKind::Temp:   return COL_TEMP;
        case FxKind::Immune: return COL_GRAY;
    }
    return COL_RED;
}

static void outlinedText(ImDrawList* dl, ImVec2 pos, ImU32 col, int alpha, const char* text,
                         float sizeMul, float S) {
    const ImU32 ink = withAlpha(COL_SHADOW, alpha);
    const float o = 1.6f * S;
    const ImVec2 offs[] = {{-o, 0}, {o, 0}, {0, -o}, {0, o}, {-o, -o}, {o, -o}, {-o, o}, {o, o}};
    for (const ImVec2& d : offs) {
        heavyText(dl, ImVec2(pos.x + d.x, pos.y + d.y), ink, text, sizeMul);
    }
    heavyText(dl, pos, withAlpha(col, alpha), text, sizeMul);
}

// Floating label and crit starburst; drawn in a top pass above all cards.
void drawCardEffects(const CardState& st, ImVec2 size, float S) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = st.drawPos;

    // Death: an ink shockwave ring around the card, then "DEFEATED" / "DEAD" floats up.
    if (st.wasDead && st.deathT < DEATH_LEN) {
        const float d = st.deathT;
        if (d < 0.5f) {
            const float u = d / 0.5f;
            const float grow = easeOutCubic(u) * 26.0f * S;
            dl->AddRect(ImVec2(p.x - grow, p.y - grow), ImVec2(p.x + size.x + grow, p.y + size.y + grow),
                        withAlpha(COL_INK, static_cast<int>(150 * (1.0f - u))), 16 * S + grow, 0, (3.0f - 2.0f * u) * S);
        }
        const float start = 0.15f;
        if (d > start && !st.deathLabel.empty()) {
            const float u = clamp01((d - start) / (DEATH_LEN - start));
            const float rise = easeOutCubic(u) * 30.0f * S;
            const float fade = u < 0.6f ? 1.0f : 1.0f - (u - 0.6f) / 0.4f;
            const float pop = u < 0.1f ? lerpf(0.6f, 1.0f, easeOutCubic(u / 0.1f)) : 1.0f;
            const float mul = 1.35f * pop;
            const ImVec2 ts = heavyTextSize(st.deathLabel.c_str(), mul);
            const ImVec2 pos(p.x + size.x * 0.5f - ts.x * 0.5f, p.y - 6 * S - rise - ts.y * 0.5f);
            outlinedText(dl, pos, IM_COL32(205, 205, 200, 255), static_cast<int>(255 * fade), st.deathLabel.c_str(), mul, S);
        }
    }

    if (!st.active) return;
    const float t = st.t;

    // Floating "-N" / "+N" / "IMMUNE" / "-5 temp".
    {
        const float life = 0.95f;
        if (t < life && !st.fxLabel.empty()) {
            const float rise = easeOutCubic(clamp01(t / life)) * 50.0f * S;
            const float fade = t < 0.55f ? 1.0f : 1.0f - clamp01((t - 0.55f) / (life - 0.55f));
            const float pop = t < 0.12f ? lerpf(0.6f, 1.0f, easeOutCubic(t / 0.12f)) : 1.0f;
            const float mul = (st.crit ? 2.3f : 1.5f) * pop;
            const ImVec2 ts = heavyTextSize(st.fxLabel.c_str(), mul);
            const ImVec2 pos(p.x + size.x * 0.5f - ts.x * 0.5f, p.y + 34 * S - rise - ts.y * 0.5f);
            outlinedText(dl, pos, fxColor(st.fxKind), static_cast<int>(255 * fade), st.fxLabel.c_str(), mul, S);
        }
    }

    // Crit starburst.
    if (st.crit && t < ANIM_HOLD + 0.35f) {
        const float end = ANIM_HOLD + 0.35f;
        float scale;
        if (t < 0.10f) scale = lerpf(0.0f, 1.35f, t / 0.10f);
        else if (t < 0.24f) scale = lerpf(1.35f, 1.0f, (t - 0.10f) / 0.14f);
        else scale = 1.0f;
        if (t > end - 0.2f) scale *= clamp01((end - t) / 0.2f);
        const float wobble = 0.18f * std::sin(t * 22.0f) * (1.0f - clamp01(t / 0.6f));

        const ImVec2 c(p.x + size.x - 8 * S, p.y - 2 * S);
        const float R = 34 * S * scale;
        const int spikes = 12;
        std::vector<ImVec2> pts;
        for (int i = 0; i < spikes * 2; i++) {
            const float r = (i % 2 == 0) ? R : R * 0.68f;
            const float ang = wobble + i * 3.14159265f / spikes;
            pts.push_back(ImVec2(c.x + std::cos(ang) * r, c.y + std::sin(ang) * r));
        }
        const ImU32 fill = st.heal ? COL_GREEN : COL_RED;
        const int n = static_cast<int>(pts.size());
        for (int i = 0; i < n; i++) {
            dl->AddTriangleFilled(c, pts[i], pts[(i + 1) % n], fill);
        }
        dl->AddPolyline(pts.data(), n, COL_SHADOW, ImDrawFlags_Closed, 2.0f * S);

        const char* label = st.heal ? "HEAL!" : "CRIT!";
        const float mul = std::max(0.2f, scale);
        const ImVec2 ts = heavyTextSize(label, 0.95f * mul);
        outlinedText(dl, ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), IM_COL32(255, 255, 255, 255),
                     255, label, 0.95f * mul, S);
    }
}


// ---- Cards -----------------------------------------------------------------

// Cards are compact rows as wide as their column; their height scales with the
// card size setting. The full details of one card live in the spotlight panel.
static const float CARD_H = 58.0f;

float cardHeight() {
    return CARD_H * cardS();
}

struct Chip {
    std::string text;
    ImU32 fill;
};

// Status chips along the bottom of a card; "+N" when they don't all fit.
// A chip's color tints its fill and border and colors its text.
static void drawChips(ImDrawList* dl, ImVec2 start, float maxX, const std::vector<Chip>& chips, float S,
                      float scale = 1.0f) {
    const float mul = 0.78f * scale;
    const float h = 19 * S * scale;
    const float padX = 7 * S * scale;
    const float gap = 5 * S * scale;
    float x = start.x;
    for (size_t i = 0; i < chips.size(); i++) {
        const ImVec2 ts = fontTextSize(boldFont(), chips[i].text.c_str(), mul);
        const float w = ts.x + padX * 2;
        const size_t remaining = chips.size() - i - 1;
        const float reserve = remaining > 0 ? sizedTextSize("+99", mul).x + padX * 2 + gap : 0.0f;
        if (x + w + reserve > maxX) {
            const std::string more = "+" + std::to_string(chips.size() - i);
            const float mw = sizedTextSize(more.c_str(), mul).x + padX * 2;
            dl->AddRectFilled(ImVec2(x, start.y), ImVec2(x + mw, start.y + h), COL_TRACK, h * 0.5f);
            sizedText(dl, ImVec2(x + padX, start.y + (h - ts.y) * 0.5f), COL_MUTED, more.c_str(), mul);
            return;
        }
        const ImU32 col = chips[i].fill;
        const float k = static_cast<float>(col >> IM_COL32_A_SHIFT) / 255.0f;
        dl->AddRectFilled(ImVec2(x, start.y), ImVec2(x + w, start.y + h),
                          withAlpha(col, static_cast<int>(34 * k)), h * 0.5f);
        dl->AddRect(ImVec2(x, start.y), ImVec2(x + w, start.y + h),
                    withAlpha(col, static_cast<int>(150 * k)), h * 0.5f, 0, 1.0f * S);
        const ImU32 textCol = withAlpha(mixColor(col, COL_INK, 0.35f), static_cast<int>(255 * k));
        fontText(dl, boldFont(), ImVec2(x + padX, start.y + (h - ts.y) * 0.5f), textCol, chips[i].text.c_str(), mul);
        x += w + gap;
    }
}

static std::vector<Chip> cardChips(Side side, const Creature& c, Life life, float time) {
    std::vector<Chip> chips;
    if (life == Life::Dying) {
        const int a = 170 + static_cast<int>(85 * (0.5f + 0.5f * std::sin(time * 6.0f)));
        chips.push_back({"DYING", withAlpha(COL_RED, a)});
    } else if (life == Life::Stable) {
        chips.push_back({"STABLE", COL_GREEN});
    } else if (life == Life::Dead && side == Side::Players) {
        chips.push_back({"DEAD", COL_GRAY});
    }
    if (c.isBloodied()) chips.push_back({"BLOODIED", COL_RED});
    if (c.isConcentrating()) chips.push_back({"CONC", COL_VIOLET});
    switch (c.getDamageMod()) {
        case DamageMod::Resistant:  chips.push_back({"RESIST", IM_COL32(217, 119, 6, 255)}); break;
        case DamageMod::Vulnerable: chips.push_back({"VULN", IM_COL32(219, 39, 119, 255)}); break;
        case DamageMod::Immune:     chips.push_back({"IMMUNE", COL_GRAY}); break;
        default: break;
    }
    for (const Condition& cond : c.getConditions()) {
        std::string text = cond.name;
        if (cond.rounds > 0) text += " " + std::to_string(cond.rounds);
        chips.push_back({text, IM_COL32(202, 138, 4, 255)});
    }
    if (!c.getNotes().empty()) chips.push_back({"NOTE", COL_PLAYER});
    return chips;
}

static void cardTooltip(Side side, const Creature& c) {
    const float S = UI_SCALE;
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(300 * S);
    ImGui::TextUnformatted(c.getName().c_str());
    ImGui::TextDisabled("%s", side == Side::Players ? "Player" : "Enemy");
    ImGui::Separator();
    ImGui::Text("HP %d / %d", c.getCurrentHp(), c.getMaxHp());
    if (c.getTempHp() > 0) {
        ImGui::SameLine();
        ImGui::TextColored(toVec4(COL_TEMP), "+%d temp", c.getTempHp());
    }
    ImGui::Text("AC %d   Speed %d ft   Passive Perception %d", c.getAc(), c.getSpeed(), c.getPassivePerception());
    if (side == Side::Players) {
        ImGui::Text("Level %d", c.getLevel());
    } else if (c.getCrIndex() >= 0) {
        ImGui::Text("CR %s (%d XP)", crLabel(c.getCrIndex()), crXp(c.getCrIndex()));
    }
    if (c.getDamageMod() != DamageMod::Normal) {
        ImGui::Text("Damage: %s", damageModName(c.getDamageMod()));
    }
    if (c.isConcentrating()) ImGui::Text("Concentrating");
    if (side == Side::Players && c.getCurrentHp() == 0) {
        ImGui::Text("Death saves: %d success, %d failure", c.getDeathSuccesses(), c.getDeathFailures());
    }
    if (!c.getConditions().empty()) {
        ImGui::Separator();
        for (const Condition& cond : c.getConditions()) {
            if (cond.rounds > 0) {
                ImGui::BulletText("%s (%d round%s left)", cond.name.c_str(), cond.rounds, cond.rounds == 1 ? "" : "s");
            } else {
                ImGui::BulletText("%s", cond.name.c_str());
            }
        }
    }
    if (!c.getNotes().empty()) {
        ImGui::Separator();
        ImGui::TextUnformatted(c.getNotes().c_str());
    }
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

void drawToken(const Encounter& encounter, CardRef ref, CardState& st, ImVec2 boardOrigin,
               const std::string& amountLabel, bool interactive, bool isTurn, bool isFocus,
               const CardActions& act) {
    const Creature& c = *encounter.at(ref.side, ref.index);
    const Side side = ref.side;
    const ImVec2 size = st.size;
    const float S = cardS();
    const float pad = 9 * S;
    bool showTooltip = false;
    const float round = 12 * S;
    const float outline = (isTurn ? 3.0f : 2.0f) * S;
    const float time = static_cast<float>(ImGui::GetTime());
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::PushID(static_cast<int>(side) * 10000 + ref.index);
    std::optional<CardFontScope> cardFont;
    cardFont.emplace();
    const int vtxStart = dl->VtxBuffer.Size;  // everything this card draws (for the fade-in)

    ImVec2& pos = st.pos;
    st.beingDragged = false;
    st.wantsEdit = false;
    st.pressed = false;
    st.clicked = false;

    // HP-change animation bookkeeping.
    const float dt = std::min(ImGui::GetIO().DeltaTime, 0.05f);
    updateCardFx(st, c.getCurrentHp(), c.getMaxHp(), dt);

    // A card only looks dead once its final hit has finished dropping; then
    // the death animation plays once (cards loaded already dead skip it).
    const Life life = lifeOf(side, c);
    const bool showDead = life == Life::Dead &&
                          !(st.active && st.fxKind == FxKind::Damage && st.t < ANIM_HOLD + ANIM_DROP);
    if (showDead && !st.wasDead) {
        st.deathT = 0.0f;
        st.deathLabel = side == Side::Players ? "DEAD" : "DEFEATED";
    }
    st.wasDead = showDead;
    if (st.deathT < DEATH_LEN) st.deathT += dt;
    if (st.spawnT < SPAWN_TIME) st.spawnT += dt;
    // 0 = alive look, 1 = fully dead look (fades in over half a second).
    const float deadK = showDead ? easeOutCubic(clamp01(st.deathT / 0.55f)) : 0.0f;
    const float spawnK = easeOutCubic(clamp01(st.spawnT / SPAWN_TIME));
    st.turnAnim = approach(st.turnAnim, isTurn ? 1.0f : 0.0f, 16.0f, dt);

    const ImVec2 shake = shakeOffset(st, S);
    // Dying thuds the card down a few pixels before it settles; new cards drop in from above.
    float thud = 0.0f;
    if (showDead && st.deathT < 0.4f) {
        const float u = st.deathT / 0.4f;
        thud = std::sin(u * 3.14159265f) * 6.0f * S * (1.0f - 0.5f * u);
    }
    const float drop = (1.0f - spawnK) * -14.0f * S;
    const ImVec2 p(boardOrigin.x + pos.x + shake.x, boardOrigin.y + pos.y + shake.y + thud + drop);
    const ImVec2 pEnd(p.x + size.x, p.y + size.y);
    st.drawPos = p;

    const ImU32 sideColor = creatureColor(side, c);
    const ImU32 accent = mixColor(sideColor, COL_GRAY, deadK);

    // Hover state of the drag handle is known from the previous frame via the
    // item ID, so the fill can react without submitting the handle first.
    const ImGuiID dragId = ImGui::GetID("drag");
    const bool hoveredOrActive = ImGui::GetHoveredID() == dragId || ImGui::GetActiveID() == dragId;

    // Selection ring: a marker-yellow outline that grows out from the card.
    st.selAnim = approach(st.selAnim, st.selected ? 1.0f : 0.0f, 20.0f, dt);
    if (st.selAnim > 0.01f) {
        const float e = easeOutCubic(st.selAnim);
        const float g = lerpf(0.0f, 4.0f, e) * S;
        const ImVec2 r0 = p - ImVec2(g, g), r1 = pEnd + ImVec2(g + 3 * S, g + 4 * S);
        dl->AddRectFilled(r0, r1, withAlpha(COL_SELECT, static_cast<int>(40 * e)), round + g);
        dl->AddRect(r0, r1, withAlpha(COL_SELECT, static_cast<int>(255 * e)), round + g, 0, 2.5f * S);
    }

    // Hard ink shadow: rises on hover, more while held, flattens when dead.
    const bool held = ImGui::GetActiveID() == dragId;
    const float liftTarget = lerpf(held ? 6.0f : (hoveredOrActive ? 4.5f : 3.0f), 1.0f, deadK);
    st.lift = approach(st.lift, liftTarget, 18.0f, dt);
    const float lift = st.lift;
    const ImU32 outlineCol = mixColor(COL_INK, COL_BORDER_HI, deadK);
    dl->AddRectFilled(p + ImVec2(lift * S, (lift + 1) * S), pEnd + ImVec2(lift * S, (lift + 1) * S), outlineCol, round);
    // Body: dying players pulse faintly like a heartbeat; the dead fade to gray;
    // the card shown in the spotlight gets a faint tint.
    ImU32 aliveBody = hoveredOrActive ? COL_CARD_HI : COL_CARD;
    if (isFocus) aliveBody = IM_COL32(250, 248, 238, 255);
    if (life == Life::Dying || (showDead && side == Side::Players)) {
        const float beat = life == Life::Dying ? 0.5f + 0.5f * std::sin(time * 4.0f) : 0.5f;
        aliveBody = mixColor(IM_COL32(255, 244, 242, 255), IM_COL32(255, 228, 224, 255), beat);
    }
    const ImU32 body = mixColor(aliveBody, IM_COL32(241, 241, 238, 255), deadK);
    dl->AddRectFilled(p, pEnd, body, round);
    // Flash on impact, colored by what happened (stronger on a crit).
    if (st.active) {
        const float dur = st.crit ? 0.40f : 0.14f;
        if (st.t < dur) {
            const float k = 1.0f - st.t / dur;
            ImU32 flash = IM_COL32(255, 60, 60, 255);
            if (st.fxKind == FxKind::Heal) flash = IM_COL32(60, 200, 100, 255);
            if (st.fxKind == FxKind::Temp) flash = IM_COL32(40, 160, 240, 255);
            if (st.fxKind == FxKind::Immune) flash = IM_COL32(150, 150, 160, 255);
            dl->AddRectFilled(p, pEnd, withAlpha(flash, static_cast<int>((st.crit ? 110 : 55) * k)), round);
        }
    }
    // A strip of the card's color down its left edge: the card's own rounded shape,
    // clipped to its first few pixels, so the strip follows the corners. (A narrow
    // rect can't take the card's corner radius and would poke out of the outline.)
    dl->PushClipRect(p, ImVec2(p.x + 6 * S, pEnd.y), true);
    dl->AddRectFilled(p, pEnd, fadeAlpha(accent, 0.85f), round);
    dl->PopClipRect();
    dl->AddRect(p, pEnd, outlineCol, round, 0, outline);

    // Left: icon tile with a line glyph in the side's color.
    const float iconSize = 32 * S;
    const ImVec2 iconMin(p.x + pad + 4 * S, p.y + (size.y - iconSize) * 0.5f);
    const ImVec2 iconMax(iconMin.x + iconSize, iconMin.y + iconSize);
    dl->AddRectFilled(iconMin, iconMax, withAlpha(accent, 30), 9 * S);
    const ImVec2 glyphOrigin(iconMin.x + iconSize * 0.15f, iconMin.y + iconSize * 0.15f);
    const float glyphSize = iconSize * 0.7f;
    // On death the heart/bolt fades out while a cross is drawn over it stroke by stroke.
    const float oldGlyph = showDead ? 1.0f - clamp01(st.deathT / 0.2f) : 1.0f;
    if (oldGlyph > 0.0f) {
        drawCreatureIcon(dl, side, c, glyphOrigin, glyphSize, 2.2f * S, fadeAlpha(accent, oldGlyph));
    }
    if (showDead) {
        drawCrossPartial(dl, glyphOrigin, glyphSize, 2.2f * S, accent, easeOutCubic(clamp01(st.deathT / 0.16f)),
                         easeOutCubic(clamp01((st.deathT - 0.14f) / 0.16f)));
    }

    // Right: small Damage (-) / Heal (+) buttons for the current amount.
    const float btn = 26 * S;
    const float btnGap = 6 * S;
    const ImVec2 healMin(pEnd.x - pad - btn, p.y + (size.y - btn) * 0.5f);
    const ImVec2 dmgMin(healMin.x - btnGap - btn, healMin.y);
    st.damageCenter = ImVec2(dmgMin.x + btn * 0.5f, dmgMin.y + btn * 0.5f);
    st.healCenter = ImVec2(healMin.x + btn * 0.5f, healMin.y + btn * 0.5f);
    // Covered cards only draw their buttons (no ImGui item), so they can't
    // steal the press from the card in front.
    ImGui::SetCursorScreenPos(dmgMin);
    if (flatButton("-##dmg", ImVec2(btn, btn), BTN_RED, true, interactive)) act.damage(ref);
    if (interactive && ImGui::IsItemHovered()) ImGui::SetTooltip("Damage %s", amountLabel.c_str());
    ImGui::SetCursorScreenPos(healMin);
    if (flatButton("+##heal", ImVec2(btn, btn), BTN_GREEN, true, interactive)) act.heal(ref);
    if (interactive && ImGui::IsItemHovered()) ImGui::SetTooltip("Heal %s", amountLabel.c_str());

    const float lineH = ImGui::GetTextLineHeight();
    const float textX = iconMax.x + 9 * S;
    const float rightLimit = dmgMin.x - 10 * S;

    // Line 1, right: HP (+temp), right-aligned against the buttons.
    float hpLeft = rightLimit;
    const float line1Y = p.y + pad;
    {
        const std::string cur = std::to_string(c.getCurrentHp());
        const std::string rest = " / " + std::to_string(c.getMaxHp());
        const std::string tempText = c.getTempHp() > 0 ? " +" + std::to_string(c.getTempHp()) : "";
        const float wCur = heavyTextSize(cur.c_str()).x;
        const float wRest = ImGui::CalcTextSize(rest.c_str()).x;
        const float wTemp = tempText.empty() ? 0.0f : ImGui::CalcTextSize(tempText.c_str()).x;
        hpLeft = rightLimit - (wCur + wRest + wTemp);
        heavyText(dl, ImVec2(hpLeft, line1Y), mixColor(COL_TEXT, COL_MUTED, deadK), cur.c_str());
        dl->AddText(ImVec2(hpLeft + wCur, line1Y), COL_MUTED, rest.c_str());
        if (!tempText.empty()) dl->AddText(ImVec2(hpLeft + wCur + wRest, line1Y), COL_TEMP, tempText.c_str());
    }

    // Line 1, left: name (heavy, struck through when dead), then status chips.
    const std::string& name = c.getName();
    const float nameDraw = 1.02f;
    const float nameRight = std::max(textX + 20 * S, hpLeft - 8 * S);
    const ImVec2 ns = heavyTextSize(name.c_str(), nameDraw);
    dl->PushClipRect(ImVec2(textX, p.y), ImVec2(nameRight, pEnd.y), true);
    heavyText(dl, ImVec2(textX, line1Y), mixColor(COL_TEXT, COL_MUTED, deadK), name.c_str(), nameDraw);
    dl->PopClipRect();
    if (showDead) {
        // The strike-through is drawn left to right, just after the cross.
        const float strike = easeOutCubic(clamp01((st.deathT - 0.25f) / 0.3f));
        const float y = line1Y + ns.y * 0.55f;
        const float x1 = std::min(textX + ns.x, nameRight);
        if (strike > 0.0f) dl->AddLine(ImVec2(textX, y), ImVec2(lerpf(textX, x1, strike), y), COL_MUTED, 1.5f * S);
    }
    // Line 2: the HP bar (or the death-save pips for a player at 0 HP), then the
    // status chips when there are any.
    const std::vector<Chip> chips = cardChips(side, c, life, time);
    const float barH = 8 * S;
    const float line2Y = pEnd.y - pad - barH - 2 * S;
    const float barEnd = chips.empty() ? rightLimit : std::max(textX + 60 * S, textX + (rightLimit - textX) * 0.42f);
    float chipsX = barEnd + 10 * S;
    st.hasPips = side == Side::Players && c.getCurrentHp() == 0;
    if (st.hasPips) {
        const float d = 12 * S;
        const float gap = 4 * S;
        const float groupGap = 12 * S;
        const float cy = line2Y + barH * 0.5f;
        float x = textX;
        int pip = 0;
        for (int group = 0; group < 2; group++) {
            const bool success = group == 0;
            const int have = success ? c.getDeathSuccesses() : c.getDeathFailures();
            for (int i = 0; i < 3; i++) {
                const ImVec2 center(x + d * 0.5f, cy);
                st.pipCenter[pip++] = center;
                const bool filled = i < have;
                dl->AddCircleFilled(center, d * 0.5f, filled ? (success ? COL_GREEN : COL_RED) : COL_PANEL);
                dl->AddCircle(center, d * 0.5f, filled ? COL_INK : COL_BORDER_HI, 0, 1.5f * S);
                if (interactive) {
                    ImGui::SetCursorScreenPos(ImVec2(x, cy - d * 0.5f));
                    ImGui::PushID(group * 10 + i);
                    if (ImGui::InvisibleButton("pip", ImVec2(d, d))) {
                        const int value = (have == i + 1) ? i : i + 1;  // click the last filled pip to clear it
                        if (success) act.setDeathSaves(ref, value, c.getDeathFailures());
                        else act.setDeathSaves(ref, c.getDeathSuccesses(), value);
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", success ? "Death save successes" : "Death save failures");
                    }
                    ImGui::PopID();
                }
                x += d + gap;
            }
            x += groupGap - gap;
        }
        chipsX = x + 6 * S;
    } else {
        const float barRound = barH * 0.5f;
        const ImVec2 barMin(textX, line2Y);
        const ImVec2 barMax(barEnd, line2Y + barH);
        const BarVis bar = computeBar(st, c.getCurrentHp(), c.getMaxHp());
        const float barW = barMax.x - barMin.x;
        dl->AddRectFilled(barMin, barMax, COL_TRACK, barRound);
        dl->PushClipRect(barMin, barMax, true);
        if (bar.fillEnd > 0.0f) {
            const float targetFrac = c.getMaxHp() > 0
                ? static_cast<float>(c.getCurrentHp()) / static_cast<float>(c.getMaxHp()) : 0.0f;
            fillBarSpan(dl, barMin, barMax, barMin.x, barMin.x + barW * bar.fillEnd, hpColor(targetFrac));
        }
        if (bar.hasChunk && bar.chunkEnd > bar.fillEnd) {
            // The "pre-bar": the part of the bar about to be lost or gained.
            ImU32 chunk;
            const float k = clamp01(st.t / 0.3f);
            if (!st.heal) {
                chunk = IM_COL32(255, static_cast<int>(lerpf(214, 90, k)), static_cast<int>(lerpf(120, 80, k)), 255);
            } else {
                chunk = IM_COL32(static_cast<int>(lerpf(200, 120, k)), static_cast<int>(lerpf(250, 220, k)), static_cast<int>(lerpf(170, 140, k)), 255);
            }
            const float x0 = barMin.x + barW * bar.fillEnd;
            const float x1 = barMin.x + barW * bar.chunkEnd;
            fillBarSpan(dl, barMin, barMax, x0, x1, chunk);
        }
        dl->PopClipRect();
        dl->AddRect(barMin, barMax, showDead ? COL_BORDER_HI : COL_INK, barRound, 0, 1.25f * S);
    }
    if (!chips.empty() && chipsX + 30 * S < rightLimit) {
        drawChips(dl, ImVec2(chipsX, line2Y + barH * 0.5f - 8.0f * S), rightLimit, chips, S, 0.84f);
    }
    (void)lineH;

    // The turn ribbon: a black "TURN" tab on the card's top edge. Clicking it
    // passes the turn on. Submitted before the drag handle so it wins the press.
    if (st.turnAnim > 0.02f) {
        const float k = easeOutCubic(st.turnAnim);
        const float rh = 16 * S;
        const ImVec2 tw = fontTextSize(boldFont(), "TURN", 0.74f);
        const float rw = 13 * S + tw.x + 18 * S;
        const ImVec2 r0(p.x + 14 * S, p.y - rh * 0.6f - (1.0f - k) * 6 * S);
        const ImVec2 r1(r0.x + rw, r0.y + rh);
        st.turnCenter = ImVec2((r0.x + r1.x) * 0.5f, (r0.y + r1.y) * 0.5f);
        bool hot = false;
        if (interactive && k > 0.9f) {
            ImGui::SetCursorScreenPos(r0);
            if (ImGui::InvisibleButton("turn", r1 - r0)) act.nextTurn();
            hot = ImGui::IsItemHovered();
            if (hot) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                ImGui::SetTooltip("Next turn (Space)");
            }
        }
        dl->AddRectFilled(r0, r1, fadeAlpha(hot ? IM_COL32(64, 64, 66, 255) : COL_INK, k), rh * 0.5f);
        const float mid = (r0.y + r1.y) * 0.5f;
        const float tx = r0.x + 8 * S;
        dl->AddTriangleFilled(ImVec2(tx, mid - 3.5f * S), ImVec2(tx, mid + 3.5f * S), ImVec2(tx + 6 * S, mid),
                              fadeAlpha(COL_WHITE, k));
        fontText(dl, boldFont(), ImVec2(tx + 11 * S, mid - tw.y * 0.5f), fadeAlpha(COL_WHITE, k), "TURN", 0.74f);
    }

    // The whole card is the drag handle. It is submitted last so the buttons
    // above get hover/click priority; ImGui gives an earlier overlapping item
    // the press otherwise.
    if (interactive) {
        ImGui::SetCursorScreenPos(p);
        ImGui::InvisibleButton("drag", size);
        const ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsItemActivated()) {
            st.pressed = true;
            st.pressMouse = io.MousePos;
        }
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f)) {
            pos.x += io.MouseDelta.x;
            pos.y += io.MouseDelta.y;
            st.beingDragged = true;
        }
        if (ImGui::IsItemDeactivated()) {
            const ImVec2 d = io.MousePos - st.pressMouse;
            if (d.x * d.x + d.y * d.y < 16.0f) st.clicked = true;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            st.wantsEdit = true;
        }
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal) && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            showTooltip = true;
        }
    }

    // Newly added cards fade in (together with their drop from above).
    if (spawnK < 1.0f) fadeVertices(dl, vtxStart, spawnK, COL_BOARD);

    cardFont.reset();  // tooltips use the normal UI size
    if (showTooltip) cardTooltip(side, c);
    ImGui::PopID();
}

// The spotlight panel: one combatant shown big (the one whose turn it is, or
// the last one clicked), with everything the compact row has no room for:
// big HP and bar, stats, the action buttons, death saves, chips and notes.
void drawSpotlight(const Encounter& encounter, CardRef ref, CardState& st, ImVec2 a, ImVec2 b,
                   const std::string& amountLabel, bool holdsTurn, const CardActions& act) {
    const Creature& c = *encounter.at(ref.side, ref.index);
    const Side side = ref.side;
    const float S = UI_SCALE;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float pad = 18 * S;
    const Life life = lifeOf(side, c);
    const bool dead = life == Life::Dead;
    const float time = static_cast<float>(ImGui::GetTime());
    const float lineH = ImGui::GetTextLineHeight();
    const ImU32 accent = dead ? COL_GRAY : creatureColor(side, c);
    const float x0 = a.x + pad, x1 = b.x - pad;
    float y = a.y + pad;

    ImGui::PushID("spotlight");
    ImGui::PushClipRect(a, b, true);

    // Header: icon, name, what it is; a TURN pill when it holds the turn.
    {
        const float icon = 46 * S;
        const ImVec2 iconMin(x0, y), iconMax(x0 + icon, y + icon);
        dl->AddRectFilled(iconMin, iconMax, withAlpha(accent, 30), 12 * S);
        const ImVec2 go(iconMin.x + icon * 0.15f, iconMin.y + icon * 0.15f);
        if (dead) drawCross(dl, go, icon * 0.7f, 2.8f * S, accent);
        else drawCreatureIcon(dl, side, c, go, icon * 0.7f, 2.8f * S, accent);

        std::string what = side == Side::Players ? "Player  \xC2\xB7  Level " + std::to_string(c.getLevel()) : "Enemy";
        if (side == Side::Creatures && c.getCrIndex() >= 0) {
            what += std::string("  \xC2\xB7  CR ") + crLabel(c.getCrIndex()) + "  \xC2\xB7  " +
                    std::to_string(crXp(c.getCrIndex())) + " XP";
        }
        float nameRight = x1;
        if (holdsTurn) {
            const ImVec2 tw = boldTextSize("TURN", 0.85f);
            const float ph = 24 * S, pw = tw.x + 34 * S;
            const ImVec2 p0(x1 - pw, y + 2 * S), p1(x1, y + 2 * S + ph);
            dl->AddRectFilled(p0, p1, COL_INK, ph * 0.5f);
            const float mid = (p0.y + p1.y) * 0.5f, tx = p0.x + 11 * S;
            dl->AddTriangleFilled(ImVec2(tx, mid - 4.5f * S), ImVec2(tx, mid + 4.5f * S), ImVec2(tx + 8 * S, mid), COL_WHITE);
            boldText(dl, ImVec2(tx + 13 * S, mid - tw.y * 0.5f), COL_WHITE, "TURN", 0.85f);
            nameRight = p0.x - 10 * S;
        }
        const float nx = iconMax.x + 14 * S;
        dl->PushClipRect(ImVec2(nx, a.y), ImVec2(nameRight, b.y), true);
        heavyText(dl, ImVec2(nx, y - 1 * S), dead ? COL_MUTED : COL_TEXT, c.getName().c_str(), 1.45f);
        dl->PopClipRect();
        dl->AddText(ImVec2(nx, y + icon - lineH - 1 * S), COL_MUTED, what.c_str());
        y += icon + 16 * S;
    }

    // Big HP, then a thick bar (same pre-bar animation as the row).
    {
        const std::string cur = std::to_string(c.getCurrentHp());
        const std::string rest = " / " + std::to_string(c.getMaxHp()) + " HP";
        const ImVec2 cs = heavyTextSize(cur.c_str(), 2.0f);
        heavyText(dl, ImVec2(x0, y), dead ? COL_MUTED : COL_TEXT, cur.c_str(), 2.0f);
        const float restY = y + cs.y - lineH * 1.15f;
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 1.15f, ImVec2(x0 + cs.x + 4 * S, restY), COL_MUTED, rest.c_str());
        if (c.getTempHp() > 0) {
            const std::string temp = "+" + std::to_string(c.getTempHp()) + " temp";
            const ImVec2 rs = sizedTextSize(rest.c_str(), 1.15f);
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 1.15f, ImVec2(x0 + cs.x + 12 * S + rs.x, restY), COL_TEMP, temp.c_str());
        }
        y += cs.y + 8 * S;

        const float barH = 16 * S;
        const ImVec2 barMin(x0, y), barMax(x1, y + barH);
        const float barW = barMax.x - barMin.x;
        const BarVis bar = computeBar(st, c.getCurrentHp(), c.getMaxHp());
        dl->AddRectFilled(barMin, barMax, COL_TRACK, barH * 0.5f);
        dl->PushClipRect(barMin, barMax, true);
        if (bar.fillEnd > 0.0f) {
            const float frac = c.getMaxHp() > 0 ? static_cast<float>(c.getCurrentHp()) / static_cast<float>(c.getMaxHp()) : 0.0f;
            fillBarSpan(dl, barMin, barMax, barMin.x, barMin.x + barW * bar.fillEnd, hpColor(frac));
        }
        if (bar.hasChunk && bar.chunkEnd > bar.fillEnd) {
            const float k = clamp01(st.t / 0.3f);
            const ImU32 chunk = !st.heal
                ? IM_COL32(255, static_cast<int>(lerpf(214, 90, k)), static_cast<int>(lerpf(120, 80, k)), 255)
                : IM_COL32(static_cast<int>(lerpf(200, 120, k)), static_cast<int>(lerpf(250, 220, k)), static_cast<int>(lerpf(170, 140, k)), 255);
            fillBarSpan(dl, barMin, barMax, barMin.x + barW * bar.fillEnd, barMin.x + barW * bar.chunkEnd, chunk);
        }
        dl->PopClipRect();
        dl->AddRect(barMin, barMax, dead ? COL_BORDER_HI : COL_INK, barH * 0.5f, 0, 1.75f * S);
        if (c.getTempHp() > 0 && c.getMaxHp() > 0) {
            const float frac = std::min(1.0f, static_cast<float>(c.getTempHp()) / static_cast<float>(c.getMaxHp()));
            dl->AddRectFilled(ImVec2(barMin.x + 3 * S, barMax.y + 4 * S),
                              ImVec2(barMin.x + 3 * S + std::max(8 * S, (barW - 6 * S) * frac), barMax.y + 9 * S), COL_TEMP, 3 * S);
        }
        y += barH + 16 * S;
    }

    // Stats line.
    {
        std::string stats = "AC " + std::to_string(c.getAc()) + "   \xC2\xB7   Speed " + std::to_string(c.getSpeed()) +
                            " ft   \xC2\xB7   Passive Perception " + std::to_string(c.getPassivePerception());
        if (c.getDamageMod() != DamageMod::Normal) stats += std::string("   \xC2\xB7   ") + damageModName(c.getDamageMod());
        if (ImGui::CalcTextSize(stats.c_str()).x > x1 - x0) {  // narrow panel: the short form
            stats = "AC " + std::to_string(c.getAc()) + "  \xC2\xB7  " + std::to_string(c.getSpeed()) + " ft  \xC2\xB7  PP " +
                    std::to_string(c.getPassivePerception());
            if (c.getDamageMod() != DamageMod::Normal) stats += std::string("  \xC2\xB7  ") + damageModName(c.getDamageMod());
        }
        dl->AddText(ImVec2(x0, y), COL_MUTED, stats.c_str());
        y += lineH + 14 * S;
    }

    // Death saves (players at 0 HP): big pips, click to set.
    if (side == Side::Players && c.getCurrentHp() == 0) {
        dl->AddText(ImVec2(x0, y + 2 * S), COL_TEXT, "Death saves");
        const float d = 18 * S, gap = 6 * S;
        float x = x0 + ImGui::CalcTextSize("Death saves").x + 16 * S;
        for (int group = 0; group < 2; group++) {
            const bool success = group == 0;
            const int have = success ? c.getDeathSuccesses() : c.getDeathFailures();
            for (int i = 0; i < 3; i++) {
                const ImVec2 center(x + d * 0.5f, y + lineH * 0.5f + 2 * S);
                const bool filled = i < have;
                dl->AddCircleFilled(center, d * 0.5f, filled ? (success ? COL_GREEN : COL_RED) : COL_PANEL);
                dl->AddCircle(center, d * 0.5f, filled ? COL_INK : COL_BORDER_HI, 0, 1.75f * S);
                ImGui::SetCursorScreenPos(ImVec2(x, center.y - d * 0.5f));
                ImGui::PushID(group * 10 + i);
                if (ImGui::InvisibleButton("pip", ImVec2(d, d))) {
                    const int value = (have == i + 1) ? i : i + 1;
                    if (success) act.setDeathSaves(ref, value, c.getDeathFailures());
                    else act.setDeathSaves(ref, c.getDeathSuccesses(), value);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", success ? "Death save successes" : "Death save failures");
                ImGui::PopID();
                x += d + gap;
            }
            x += 14 * S;
        }
        y += lineH + 18 * S;
    }

    // Actions for this combatant, with the current amount.
    {
        // Four in a row when they fit, otherwise two rows of two.
        const float gap = 10 * S;
        const float bh = 34 * S;
        const int perRow = (x1 - x0) >= 520 * S ? 4 : 2;
        const float bw = (x1 - x0 - gap * static_cast<float>(perRow - 1)) / static_cast<float>(perRow);
        auto at = [&](int i) {
            ImGui::SetCursorScreenPos(ImVec2(x0 + (bw + gap) * static_cast<float>(i % perRow),
                                             y + (bh + gap) * static_cast<float>(i / perRow)));
        };
        at(0);
        if (flatButton(("Damage " + amountLabel + "##spot").c_str(), ImVec2(bw, bh), BTN_RED)) act.damage(ref);
        at(1);
        if (flatButton(("Heal " + amountLabel + "##spot").c_str(), ImVec2(bw, bh), BTN_GREEN)) act.heal(ref);
        at(2);
        if (flatButton(("+" + amountLabel + " temp HP##spot").c_str(), ImVec2(bw, bh), BTN_TEMP)) act.temp(ref);
        at(3);
        if (flatButton("Edit...##spot", ImVec2(bw, bh), BTN_QUIET)) act.edit(ref);
        y += (bh + gap) * static_cast<float>(4 / perRow) - gap + 16 * S;
    }

    // Status chips, then notes.
    {
        const std::vector<Chip> chips = cardChips(side, c, life, time);
        if (!chips.empty()) {
            drawChips(dl, ImVec2(x0, y), x1, chips, S);
            y += 19 * S + 12 * S;
        }
        if (!c.getNotes().empty()) {
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(x0, y), COL_MUTED, c.getNotes().c_str(), nullptr, x1 - x0);
        }
    }

    ImGui::PopClipRect();
    ImGui::PopID();
}
