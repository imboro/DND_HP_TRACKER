// DnD HP Tracker: opens the window, sets up ImGui, and hands over to App.
#include <SDL.h>

#include <algorithm>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "app.h"
#include "ui_kit.h"

int main(int, char**) {
#ifdef _WIN32
    // Crisp text on scaled displays (125%, 150%...): work in scaled points like
    // macOS does, and render at the real pixel density.
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
#endif
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("DnD HP Tracker", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, 1360, 900,
                                          SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    applyTheme();
    ImGui::GetStyle().ScaleAllSizes(UI_SCALE);

    // Rasterize the font at pixel density so text stays crisp on Retina,
    // then scale it back down to ImGui's point-based coordinates.
    int winW = 1, outW = 1, winH = 1, outH = 1;
    SDL_GetWindowSize(window, &winW, &winH);
    SDL_GetRendererOutputSize(renderer, &outW, &outH);
    const float dpi = winW > 0 ? std::max(1.0f, static_cast<float>(outW) / static_cast<float>(winW)) : 1.0f;
    if (loadSystemFonts(15.0f * UI_SCALE * dpi)) {
        io.FontGlobalScale = 1.0f / dpi;
    } else {
        io.FontGlobalScale = UI_SCALE;
    }

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    {
        App app(window, renderer);
        app.run();
    }  // the app is gone (and has saved) before ImGui and SDL shut down

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
