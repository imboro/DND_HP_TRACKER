#include "alerts.h"

#include <algorithm>

#include "ui_kit.h"

// Alerts stay fully visible this long, then fade out over ALERT_FADE seconds.
// Clicking one fades it out early, over ALERT_CLICK_FADE seconds.
static const double ALERT_HOLD = 3.0;
static const double ALERT_FADE = 1.6;
static const double ALERT_CLICK_FADE = 0.35;

// 0..1 opacity: fully visible for ALERT_HOLD seconds, then fading out;
// a click starts a quick fade right away.
float alertOpacity(const Alert& al, double now) {
    double k = 1.0;
    const double age = now - al.born;
    if (age > ALERT_HOLD) k = 1.0 - (age - ALERT_HOLD) / ALERT_FADE;
    if (al.clicked >= 0.0) k = std::min(k, 1.0 - (now - al.clicked) / ALERT_CLICK_FADE);
    return static_cast<float>(std::clamp(k, 0.0, 1.0));
}

bool alertExpired(const Alert& al, double now) {
    return alertOpacity(al, now) <= 0.0f;
}

// Alert toasts in the board's bottom-right corner, stacked upward above the
// trash (newest at the bottom). They fade away on their own; clicking one fades
// it out early.
void drawToasts(std::vector<Alert>& alerts, ImVec2 boardOrigin, ImVec2 boardSize, double now, float dt,
                float S) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float tw = std::min(340 * S, boardSize.x - 40 * S);
    const float x1 = boardOrigin.x + boardSize.x - 22 * S;
    const float textW = tw - 46 * S;
    const float lineH = ImGui::GetTextLineHeight();
    const float bottom = boardSize.y - 24 * S - 84 * S - 18 * S;  // just above the trash
    const float gapY = 12 * S;
    float total = 0.0f;
    for (const Alert& al : alerts) total += ImGui::CalcTextSize(al.text.c_str(), nullptr, false, textW).y + 22 * S + gapY;
    float target = bottom - total + gapY;
    for (Alert& al : alerts) {
        const ImVec2 ts = ImGui::CalcTextSize(al.text.c_str(), nullptr, false, textW);
        const float h = ts.y + 22 * S;
        // Glide to the stack position, so the rest move up smoothly when one fades out.
        if (al.y < 0.0f) al.y = target;
        else al.y += (target - al.y) * std::min(1.0f, dt * 12.0f);
        const float age = static_cast<float>(now - al.born);
        const float alpha = alertOpacity(al, now);
        const float slide = (1.0f - easeOutCubic(clamp01(age / 0.25f))) * 80 * S;
        const ImVec2 a(x1 - tw + slide, boardOrigin.y + al.y), b(x1 + slide, boardOrigin.y + al.y + h);
        al.rectMin = a;
        al.rectMax = b;
        inkBox(dl, a, b, 14 * S, S, al.hot ? COL_RAISED_HI : COL_PANEL, 3.0f, COL_INK, alpha);
        dl->AddCircleFilled(ImVec2(a.x + 18 * S, a.y + 11 * S + lineH * 0.5f), 5 * S, fadeAlpha(al.color, alpha));
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(a.x + 32 * S, a.y + 11 * S),
                    fadeAlpha(COL_TEXT, alpha), al.text.c_str(), nullptr, textW);
        target += h + 12 * S;
    }
}
