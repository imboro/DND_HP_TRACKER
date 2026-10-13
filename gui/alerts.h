#pragma once
// The combat log and the alerts (toasts) that pop up over the board.
#include <string>
#include <vector>

#include "imgui.h"

struct LogEntry {
    std::string text;
    ImU32 color;
    int round;
};

struct Alert {
    std::string text;
    ImU32 color;
    double born;             // ImGui time when raised (slide-in and fade-out)
    float y = -1.0f;         // animated top offset in the stack (-1 = not placed yet)
    double clicked = -1.0;   // ImGui time it was clicked away (-1 = not clicked)
    ImVec2 rectMin, rectMax; // where it was drawn last frame (for clicks)
    bool hot = false;        // under the mouse this frame
};

// 0..1 opacity: fully visible for a few seconds, then fading out; a click
// starts a quick fade right away.
float alertOpacity(const Alert& al, double now);
bool alertExpired(const Alert& al, double now);

// Alert toasts in the board's bottom-right corner, stacked upward (newest at the
// bottom).
void drawToasts(std::vector<Alert>& alerts, ImVec2 boardOrigin, ImVec2 boardSize, double now, float dt, float S);
