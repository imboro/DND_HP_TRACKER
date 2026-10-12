# Builds a test copy of gui_main.cpp: records button rects, can force tabs,
# takes screenshots, and runs a scripted test body each frame.
import sys
src, body_file, out, scratch = sys.argv[1:5]
s = open(src).read()
body = open(body_file).read()

def rep(old, new):
    global s
    assert old in s, "anchor not found: " + old[:60]
    s = s.replace(old, new, 1)

rep('#include "creature.h"\n', '#include "creature.h"\n#include <map>\n'
    'static std::map<std::string, ImRect> g_rects;\nstatic std::string g_shot;\nstatic int g_forceTab = -1;\n'
    'static float g_scrollCombat = -1;\nstatic std::string g_scrollTo;\nstatic ImVec2 g_boardOrigin;\nstatic ImVec2 g_boardSize;\nstatic float g_lineH = 0;\n'
    '// Simulated input is tagged with this mouse id; the real mouse is ignored, so moving\n'
    '// it over (or away from) the test window cannot disturb the run.\n'
    'static const Uint32 TEST_MOUSE_ID = 4242;\nstatic ImVec2 g_testMouse(-1, -1);\n')
# Drop real mouse events and window enter/leave; keep only the simulated ones.
rep('            ImGui_ImplSDL2_ProcessEvent(&event);',
    '            if ((event.type == SDL_MOUSEMOTION && event.motion.which != TEST_MOUSE_ID) ||\n'
    '                ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && event.button.which != TEST_MOUSE_ID) ||\n'
    '                event.type == SDL_MOUSEWHEEL ||\n'
    '                (event.type == SDL_WINDOWEVENT && (event.window.event == SDL_WINDOWEVENT_ENTER || event.window.event == SDL_WINDOWEVENT_LEAVE))) continue;\n'
    '            ImGui_ImplSDL2_ProcessEvent(&event);')
# The SDL backend falls back to the real global cursor when the window has focus but is not
# hovered; re-assert the simulated position after it.
rep('        ImGui_ImplSDL2_NewFrame();',
    '        ImGui_ImplSDL2_NewFrame();\n        if (g_testMouse.x >= 0) ImGui::GetIO().AddMousePosEvent(g_testMouse.x, g_testMouse.y);')
rep('    const bool pressed = ImGui::Button(label, drawSize);\n',
    '    const bool pressed = ImGui::Button(label, drawSize);\n    g_rects[label] = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());\n    if (!g_scrollTo.empty() && g_scrollTo == label) ImGui::SetScrollHereY(0.5f);\n')
rep('segmentedTabs(TAB_LABELS, 3, &activeTab, S);',
    'if (g_forceTab >= 0) activeTab = g_forceTab;\n        segmentedTabs(TAB_LABELS, 3, &activeTab, S);')
rep('const std::string path = savePath("encounter.txt");', 'const std::string path = "%s/test_save.txt";' % scratch)
rep('const std::string settingsPath = savePath("settings.txt");', 'const std::string settingsPath = "%s/test_settings.txt";' % scratch)
rep('const std::string libraryPath = savePath("library.txt");', 'const std::string libraryPath = "%s/test_library.txt";' % scratch)
rep('const std::string partyPath = savePath("party.txt");', 'const std::string partyPath = "%s/test_party.txt";' % scratch)
rep('        const ImVec2 boardSize = ImGui::GetContentRegionAvail();\n',
    '        const ImVec2 boardSize = ImGui::GetContentRegionAvail();\n        g_boardOrigin = boardOrigin;\n        g_boardSize = boardSize;\n        g_lineH = ImGui::GetTextLineHeight();\n')
rep('        if (ImGui::GetTime() - lastAutosave > 3.0) {', body + '\n        if (ImGui::GetTime() - lastAutosave > 3.0) {')
rep('        SDL_RenderPresent(renderer);', '''        if (!g_shot.empty()) {
            int w = 0, h = 0;
            SDL_GetRendererOutputSize(renderer, &w, &h);
            SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
            SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, surf->pixels, surf->pitch);
            SDL_SaveBMP(surf, ("%s/" + g_shot + ".bmp").c_str());
            SDL_FreeSurface(surf);
            g_shot.clear();
        }
        SDL_RenderPresent(renderer);''' % scratch)
rep('                ImGui::BeginChild("CombatBody", ImVec2(0, 0), false);\n', '                ImGui::BeginChild("CombatBody", ImVec2(0, 0), false);\n                if (g_scrollCombat >= 0) ImGui::SetScrollY(g_scrollCombat);\n')
open(out, 'w').write(s)
