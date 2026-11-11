// Host implementation used for screenshots and manual checks on a desktop.
// HOMETIDY_ROOT points at a fake console tree: sd/, mlc/ and usb/.
#include <SDL.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#include "fsx.h"
#include "platform.h"

namespace platform {

Env init() {
    Env env;
    const char* root = std::getenv("HOMETIDY_ROOT");
    const std::string base = root ? root : "./fake_console";
    env.sdRoot = base + "/sd";
    env.mlcRoot = base + "/mlc";
    env.titleRoots = {base + "/usb/usr/title", base + "/mlc/usr/title"};
    env.appDir = env.sdRoot + "/wiiu/apps/HomeTidy";
    env.storageOk = true;
    fsx::makeDirs(env.appDir);
    return env;
}

void shutdown() {}

Input poll() {
    static Uint8 prev[SDL_NUM_SCANCODES] = {0};
    const Uint8* k = SDL_GetKeyboardState(nullptr);
    auto edge = [&](SDL_Scancode s) { return k[s] && !prev[s]; };
    Input in;
    in.up = edge(SDL_SCANCODE_UP);
    in.down = edge(SDL_SCANCODE_DOWN);
    in.left = edge(SDL_SCANCODE_LEFT);
    in.right = edge(SDL_SCANCODE_RIGHT);
    in.a = edge(SDL_SCANCODE_RETURN);
    in.b = edge(SDL_SCANCODE_ESCAPE);
    in.x = edge(SDL_SCANCODE_X);
    in.y = edge(SDL_SCANCODE_Y);
    in.l = edge(SDL_SCANCODE_Q);
    in.r = edge(SDL_SCANCODE_E);
    for (int i = 0; i < SDL_NUM_SCANCODES; ++i) prev[i] = k[i];
    return in;
}

std::vector<std::uint8_t> fontBytes() {
    std::vector<std::uint8_t> out;
    const char* env = std::getenv("HOMETIDY_FONT");
    const char* candidates[] = {env, "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                                "/System/Library/Fonts/Supplemental/Arial.ttf"};
    for (const char* p : candidates) {
        if (p && fsx::readBytes(p, out)) return out;
    }
    return out;
}

std::string timestamp() {
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y%m%d_%H%M%S", std::localtime(&t));
    return buf;
}

void sleepMs(int ms) { SDL_Delay(ms); }

void log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(stderr, fmt, ap);
    va_end(ap);
    std::fputc('\n', stderr);
}

}  // namespace platform
