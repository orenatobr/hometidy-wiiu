// Entry point shared by the console build and the host preview build.
#include <SDL.h>
#include <SDL_ttf.h>

#include <cstdlib>
#include <string>
#include <vector>

#include "app.h"
#include "gfx.h"
#include "platform.h"

#ifndef __WIIU__
// Host only: HOMETIDY_SHOTS=<dir> plays a scripted tour and saves a BMP per step.
namespace {

struct Step {
    platform::Input in;
    const char* shot;  // file name to save after the step, or nullptr
    int wait;          // extra frames before capturing
};

platform::Input key(char k) {
    platform::Input i;
    switch (k) {
        case 'A': i.a = true; break;
        case 'B': i.b = true; break;
        case 'X': i.x = true; break;
        case 'Y': i.y = true; break;
        case 'L': i.l = true; break;
        case 'R': i.r = true; break;
        case 'u': i.up = true; break;
        case 'd': i.down = true; break;
        case 'l': i.left = true; break;
        case 'r': i.right = true; break;
        default: break;
    }
    return i;
}

void capture(SDL_Renderer* r, const std::string& path) {
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, 1280, 720, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_RenderReadPixels(r, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
    SDL_SaveBMP(s, path.c_str());
    SDL_FreeSurface(s);
}

}  // namespace
#endif

int main(int, char**) {
    platform::Env env = platform::init();
    platform::log("storageOk=%d titleRoots=%zu", env.storageOk, env.titleRoots.size());
    if (SDL_Init(SDL_INIT_VIDEO) != 0 || TTF_Init() != 0) {
        platform::log("SDL/TTF init FAILED: %s", SDL_GetError());
        platform::shutdown();
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("HomeTidy", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, 0);
#ifdef __WIIU__
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
#else
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, std::getenv("HOMETIDY_SHOTS") ? SDL_RENDERER_SOFTWARE : SDL_RENDERER_ACCELERATED);
#endif
    if (!window || !renderer) {
        platform::log("window/renderer failed: %s", SDL_GetError());
        platform::shutdown();
        return 1;
    }
    SDL_RenderSetLogicalSize(renderer, gfx::Canvas::kWidth, gfx::Canvas::kHeight);
    platform::log("SDL window and renderer ready");

    {
        gfx::Canvas canvas(renderer, platform::fontBytes());
        platform::log("canvas ready, font=%d", canvas.hasFont());
        App app(canvas, env);
        bool quit = false;

#ifndef __WIIU__
        if (const char* dir = std::getenv("HOMETIDY_SHOTS")) {
            const std::string out = dir;
            // Tour: home, sort preview (2 folders), confirm, done, mark grid, mark, unmark, Portuguese home.
            const std::vector<Step> tour = {
                {{}, "01_home.bmp", 8},        {key('A'), "02_sort.bmp", 2},  {key('R'), "03_sort_folder2.bmp", 1},
                {key('d'), nullptr, 0},        {key('d'), "04_sort_scroll.bmp", 1},
                {key('A'), "05_confirm.bmp", 1}, {key('A'), nullptr, 0},       {{}, "06_done.bmp", 12},
                {key('B'), "07_home_after.bmp", 1}, {key('r'), nullptr, 0},    {key('A'), "08_mark.bmp", 2},
                {key('r'), nullptr, 0},        {key('A'), "09_marked.bmp", 2}, {key('A'), "10_unmarked.bmp", 2},
                {key('d'), "11_mark_row2.bmp", 2}, {key('B'), nullptr, 0},     {key('Y'), "12_home_pt.bmp", 2},
            };
            for (const Step& s : tour) {
                if (s.wait >= 0) {
                    app.update(s.in);
                    for (int i = 0; i < s.wait; ++i) app.update({});
                    app.draw();
                    if (s.shot) capture(renderer, out + "/" + s.shot);
                }
            }
            quit = true;
        }
#endif
        while (!quit && !app.wantsQuit()) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) {
                    platform::log("SDL_QUIT received");
                    quit = true;
                }
            }
            app.update(platform::poll());
            app.draw();
            canvas.present();
        }
    }

    platform::log("main loop ended, shutting down");
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    platform::log("clean exit");
    platform::shutdown();
    return 0;
}
