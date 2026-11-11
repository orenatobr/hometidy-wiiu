// Everything that differs between the console and the host preview build.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace platform {

/// Controller state for one frame. Directions repeat while held.
struct Input {
    bool up = false, down = false, left = false, right = false;
    bool a = false, b = false, x = false, y = false;
    bool l = false, r = false;
};

/// Where things live; filled by init().
struct Env {
    std::string sdRoot;                  ///< e.g. "fs:/vol/external01"
    std::string mlcRoot;                 ///< e.g. "storage_mlc:" (empty when unavailable)
    std::vector<std::string> titleRoots; ///< "<storage>/usr/title" directories to scan
    std::string appDir;                  ///< Backups, config and series.txt live here.
    bool storageOk = false;              ///< Console storage could be opened.
};

/// Opens the storage and controllers. Call before SDL_Init.
Env init();

/// Releases everything opened by init().
void shutdown();

/// Reads the controllers (GamePad and Pro Controller) once per frame.
Input poll();

/// Bytes of a TrueType font (the system font on the console). Valid until shutdown().
std::vector<std::uint8_t> fontBytes();

/// "YYYYMMDD_HHMMSS" in local time.
std::string timestamp();

/// Sleeps for the given time.
void sleepMs(int ms);

/// Logs a line to the app log on the SD card (and to stderr on the host).
void log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

}  // namespace platform
