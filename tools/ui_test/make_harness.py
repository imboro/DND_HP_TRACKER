# Builds a test copy of the app's gui/ folder: records button rects, can force tabs,
# takes screenshots, and runs a scripted test body each frame (inside App::run, so
# it can reach the app's state: encounter, board, alerts...).
#
#   python3 make_harness.py <gui dir> <test body> <output dir> <scratch dir>
import os, shutil, sys
src, body_file, out, scratch = sys.argv[1:5]
body = open(body_file).read()

if os.path.exists(out):
    shutil.rmtree(out)
shutil.copytree(src, out)

# Globals shared by the patched files.
open(os.path.join(out, 'test_hooks.h'), 'w').write('''#pragma once
#include <SDL.h>
#include <cstdio>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "imgui.h"
#include "imgui_internal.h"
inline std::map<std::string, ImRect> g_rects;
inline std::string g_shot;
inline int g_forceTab = -1;
inline float g_scrollCombat = -1;
inline std::string g_scrollTo;
inline ImVec2 g_boardOrigin;
inline ImVec2 g_boardSize;
inline float g_lineH = 0;
// Simulated input is tagged with this mouse id; the real mouse is ignored, so moving
// it over (or away from) the test window cannot disturb the run.
inline const Uint32 TEST_MOUSE_ID = 4242;
inline ImVec2 g_testMouse(-1, -1);
inline const std::string g_outDir = "%s";
''' % scratch)

def patch(name, pairs):
    p = os.path.join(out, name)
    s = '#include "test_hooks.h"\n' + open(p).read()
    for old, new in pairs:
        assert old in s, name + ": anchor not found: " + old[:60]
        s = s.replace(old, new, 1)
    open(p, 'w').write(s)

patch('ui_kit.cpp', [
    ('    const bool pressed = ImGui::Button(label, drawSize);\n',
     '    const bool pressed = ImGui::Button(label, drawSize);\n    g_rects[label] = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());\n'
     '    if (!g_scrollTo.empty() && g_scrollTo == label) ImGui::SetScrollHereY(0.5f);\n'),
])

patch('app.cpp', [
    # Drop real mouse events and window enter/leave; keep only the simulated ones.
    ('            ImGui_ImplSDL2_ProcessEvent(&event);',
     '            if ((event.type == SDL_MOUSEMOTION && event.motion.which != TEST_MOUSE_ID) ||\n'
     '                ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && event.button.which != TEST_MOUSE_ID) ||\n'
     '                event.type == SDL_MOUSEWHEEL ||\n'
     '                (event.type == SDL_WINDOWEVENT && (event.window.event == SDL_WINDOWEVENT_ENTER || event.window.event == SDL_WINDOWEVENT_LEAVE))) continue;\n'
     '            ImGui_ImplSDL2_ProcessEvent(&event);'),
    # The SDL backend falls back to the real global cursor when the window has focus but is not
    # hovered; re-assert the simulated position after it.
    ('        ImGui_ImplSDL2_NewFrame();',
     '        ImGui_ImplSDL2_NewFrame();\n        if (g_testMouse.x >= 0) ImGui::GetIO().AddMousePosEvent(g_testMouse.x, g_testMouse.y);'),
    ('path(savePath("encounter.txt"))', 'path("%s/test_save.txt")' % scratch),
    ('libraryPath(savePath("library.txt"))', 'libraryPath("%s/test_library.txt")' % scratch),
    ('partyPath(savePath("party.txt"))', 'partyPath("%s/test_party.txt")' % scratch),
    ('settingsPath(savePath("settings.txt"))', 'settingsPath("%s/test_settings.txt")' % scratch),
    ('        if (ImGui::GetTime() - lastAutosave > 3.0) {', body + '\n        if (ImGui::GetTime() - lastAutosave > 3.0) {'),
    ('        SDL_RenderPresent(renderer);', '''        if (!g_shot.empty()) {
            int w = 0, h = 0;
            SDL_GetRendererOutputSize(renderer, &w, &h);
            SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
            SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, surf->pixels, surf->pitch);
            SDL_SaveBMP(surf, ("%s/" + g_shot + ".bmp").c_str());
            SDL_FreeSurface(surf);
            g_shot.clear();
        }
        SDL_RenderPresent(renderer);''' % scratch),
])

patch('app_sidebar.cpp', [
    ('segmentedTabs(TAB_LABELS, 3, &activeTab, S);',
     'if (g_forceTab >= 0) activeTab = g_forceTab;\n    segmentedTabs(TAB_LABELS, 3, &activeTab, S);'),
    ('    ImGui::BeginChild("CombatBody", ImVec2(0, 0), false);\n',
     '    ImGui::BeginChild("CombatBody", ImVec2(0, 0), false);\n    if (g_scrollCombat >= 0) ImGui::SetScrollY(g_scrollCombat);\n'),
])

patch('app_board.cpp', [
    ('    const ImVec2 boardSize = ImGui::GetContentRegionAvail();\n',
     '    const ImVec2 boardSize = ImGui::GetContentRegionAvail();\n    g_boardOrigin = boardOrigin;\n    g_boardSize = boardSize;\n    g_lineH = ImGui::GetTextLineHeight();\n'),
])
