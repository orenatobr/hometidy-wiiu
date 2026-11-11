// Wii U implementation: libmocha storage, VPAD + KPAD input, system font.
#include "platform.h"

#include <coreinit/memory.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <mocha/mocha.h>
#include <padscore/kpad.h>
#include <padscore/wpad.h>
#include <vpad/input.h>
#include <whb/sdcard.h>

#include <cstdarg>
#include <cstdio>
#include <ctime>

#include "fsx.h"

namespace platform {
namespace {

constexpr int kRepeatDelayMs = 400;
constexpr int kRepeatEveryMs = 90;

FILE* g_log = nullptr;
bool g_mochaOk = false;
bool g_sdOk = false;
std::string g_fontFallback;  // appDir/font.ttf

// One held button with auto repeat.
struct Repeater {
    bool held = false;
    std::int64_t since = 0;
    std::int64_t last = 0;

    bool step(bool down, bool pressedNow, std::int64_t now) {
        if (!down) {
            held = false;
            return false;
        }
        if (pressedNow || !held) {
            held = true;
            since = last = now;
            return true;
        }
        if (now - since >= kRepeatDelayMs && now - last >= kRepeatEveryMs) {
            last = now;
            return true;
        }
        return false;
    }
};

Repeater g_rep[4];
std::uint32_t g_prevHold = 0;

std::int64_t nowMs() { return OSTicksToMilliseconds(OSGetTime()); }

}  // namespace

Env init() {
    Env env;
    VPADInit();
    KPADInit();
    WPADEnableURCC(1);

    g_sdOk = WHBMountSdCard();
    env.sdRoot = g_sdOk ? WHBGetSdCardMountPath() : "fs:/vol/external01";
    env.appDir = env.sdRoot + "/wiiu/apps/HomeTidy";
    fsx::makeDirs(env.appDir);
    g_fontFallback = env.appDir + "/font.ttf";
    g_log = std::fopen((env.appDir + "/hometidy.log").c_str(), "w");

    g_mochaOk = Mocha_InitLibrary() == MOCHA_RESULT_SUCCESS;
    if (g_mochaOk) {
        const bool usb = Mocha_MountFS("storage_usb", nullptr, "/vol/storage_usb01") == MOCHA_RESULT_SUCCESS;
        const bool mlc = Mocha_MountFS("storage_mlc", nullptr, "/vol/storage_mlc01") == MOCHA_RESULT_SUCCESS;
        if (usb) env.titleRoots.push_back("storage_usb:/usr/title");
        if (mlc) {
            env.titleRoots.push_back("storage_mlc:/usr/title");
            env.mlcRoot = "storage_mlc:";
        }
        env.storageOk = usb || mlc;
        log("mocha ok usb=%d mlc=%d", usb, mlc);
    } else {
        log("mocha init failed");
    }
    log("sd=%s ok=%d", env.sdRoot.c_str(), g_sdOk);
    return env;
}

void shutdown() {
    if (g_mochaOk) {
        Mocha_UnmountFS("storage_usb");
        Mocha_UnmountFS("storage_mlc");
        Mocha_DeInitLibrary();
    }
    if (g_sdOk) WHBUnmountSdCard();
    if (g_log) std::fclose(g_log);
    g_log = nullptr;
    KPADShutdown();
}

Input poll() {
    Input in;
    const std::int64_t now = nowMs();

    std::uint32_t hold = 0, trig = 0;  // bit layout of VPAD
    VPADStatus vpad;
    VPADReadError err;
    if (VPADRead(VPAD_CHAN_0, &vpad, 1, &err) > 0 && err == VPAD_READ_SUCCESS) {
        hold |= vpad.hold;
        trig |= vpad.trigger;
        // left stick as D-pad
        if (vpad.leftStick.y > 0.6f) hold |= VPAD_BUTTON_UP;
        if (vpad.leftStick.y < -0.6f) hold |= VPAD_BUTTON_DOWN;
        if (vpad.leftStick.x < -0.6f) hold |= VPAD_BUTTON_LEFT;
        if (vpad.leftStick.x > 0.6f) hold |= VPAD_BUTTON_RIGHT;
    }
    for (int chan = 0; chan < 4; ++chan) {
        KPADStatus kpad;
        if (KPADRead(static_cast<KPADChan>(chan), &kpad, 1) > 0 && kpad.extensionType == WPAD_EXT_PRO_CONTROLLER) {
            const std::uint32_t h = kpad.pro.hold, t = kpad.pro.trigger;
            auto map = [&](std::uint32_t pro, std::uint32_t vp) {
                if (h & pro) hold |= vp;
                if (t & pro) trig |= vp;
            };
            map(WPAD_PRO_BUTTON_UP, VPAD_BUTTON_UP);
            map(WPAD_PRO_BUTTON_DOWN, VPAD_BUTTON_DOWN);
            map(WPAD_PRO_BUTTON_LEFT, VPAD_BUTTON_LEFT);
            map(WPAD_PRO_BUTTON_RIGHT, VPAD_BUTTON_RIGHT);
            map(WPAD_PRO_BUTTON_A, VPAD_BUTTON_A);
            map(WPAD_PRO_BUTTON_B, VPAD_BUTTON_B);
            map(WPAD_PRO_BUTTON_X, VPAD_BUTTON_X);
            map(WPAD_PRO_BUTTON_Y, VPAD_BUTTON_Y);
            map(WPAD_PRO_BUTTON_L, VPAD_BUTTON_L);
            map(WPAD_PRO_BUTTON_R, VPAD_BUTTON_R);
            if (kpad.pro.leftStick.y > 0.6f) hold |= VPAD_BUTTON_UP;
            if (kpad.pro.leftStick.y < -0.6f) hold |= VPAD_BUTTON_DOWN;
            if (kpad.pro.leftStick.x < -0.6f) hold |= VPAD_BUTTON_LEFT;
            if (kpad.pro.leftStick.x > 0.6f) hold |= VPAD_BUTTON_RIGHT;
        }
    }

    const std::uint32_t fresh = (hold & ~g_prevHold) | trig;  // pressed this frame
    g_prevHold = hold;
    auto pressed = [&](std::uint32_t m) { return (fresh & m) != 0; };

    in.a = pressed(VPAD_BUTTON_A);
    in.b = pressed(VPAD_BUTTON_B);
    in.x = pressed(VPAD_BUTTON_X);
    in.y = pressed(VPAD_BUTTON_Y);
    in.l = pressed(VPAD_BUTTON_L);
    in.r = pressed(VPAD_BUTTON_R);
    in.up = g_rep[0].step(hold & VPAD_BUTTON_UP, pressed(VPAD_BUTTON_UP), now);
    in.down = g_rep[1].step(hold & VPAD_BUTTON_DOWN, pressed(VPAD_BUTTON_DOWN), now);
    in.left = g_rep[2].step(hold & VPAD_BUTTON_LEFT, pressed(VPAD_BUTTON_LEFT), now);
    in.right = g_rep[3].step(hold & VPAD_BUTTON_RIGHT, pressed(VPAD_BUTTON_RIGHT), now);
    return in;
}

std::vector<std::uint8_t> fontBytes() {
    // 1) the system font from shared memory (what most homebrew uses)
    void* data = nullptr;
    std::uint32_t size = 0;
    if (OSGetSharedData(OS_SHAREDDATATYPE_FONT_STANDARD, 0, &data, &size) && data && size > 0) {
        const auto* p = static_cast<const std::uint8_t*>(data);
        return std::vector<std::uint8_t>(p, p + size);
    }
    log("shared font unavailable, trying fallbacks");
    std::vector<std::uint8_t> out;
    // 2) a font the user placed next to the app config
    if (fsx::readBytes(g_fontFallback, out) && !out.empty()) return out;
    // 3) the same font straight from the system shared-data titles
    for (const std::string& title : fsx::listDir("storage_mlc:/sys/title/0005001b")) {
        if (fsx::readBytes("storage_mlc:/sys/title/0005001b/" + title + "/content/CafeStd.ttf", out) && !out.empty()) {
            return out;
        }
    }
    return {};
}

std::string timestamp() {
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y%m%d_%H%M%S", std::localtime(&t));
    return buf;
}

void sleepMs(int ms) { OSSleepTicks(OSMillisecondsToTicks(ms)); }

void log(const char* fmt, ...) {
    if (!g_log) return;
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(g_log, fmt, ap);
    va_end(ap);
    std::fputc('\n', g_log);
    std::fflush(g_log);
}

}  // namespace platform
