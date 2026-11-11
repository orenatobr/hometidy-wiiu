# HomeTidy

A Wii U Aroma app that does two chores you would otherwise do over FTP:

1. **Sort Home**: puts the Home in order (apps A-Z, then the folders as they are, no empty positions) and the games of every folder in alphabetical order. Games of one series go by release year (see [Series table](#series-table)).
2. **Mark as completed**: draws a green check on the icon of a game you finished. You can remove it later.

It runs on the console, with the GamePad or a Pro Controller. No computer needed.

> **Status:** v1.0.0 passes its host tests and renders correctly on a desktop preview, but it has **not been confirmed on a real console yet**. Back up your SD card first. The app also saves its own backups (see below).

## Install

1. Download `HomeTidy.wuhb` from the [latest release](../../releases/latest).
2. Copy it to `sd:/wiiu/apps/HomeTidy.wuhb`.
3. Start it from the Aroma menu (or from the Wii U Menu if you use the Homebrew on Menu plugin).

Needs Aroma with the Mocha CFW (the same setup used by FTPiiU). Press **Y** on the first screen to switch between English and Brazilian Portuguese.

## How it works

| Job | What it touches | Safety |
| --- | --- | --- |
| Sort Home | `BaristaAccountSaveFile.dat`, the Wii U Menu layout file. With the Homebrew on Menu plugin it lives on the SD card at `wiiu/homebrew_on_menu_plugin/<serial>/save/<account>/`; otherwise in the console storage. | Backup first to `sd:/wiiu/apps/HomeTidy/backup/`, write, read back, and read again after 2 seconds. Only the 120 entry slots of a folder change. |
| Mark as completed | `meta/iconTex.tga` of the game, in place. Only pixels in the top-right corner change. | The first original is saved to `backup/icons/`. Press A on a marked game to restore it. |

The Wii U Menu keeps its layout in RAM and overwrites the file when it closes. HomeTidy is a separate title, so the menu is not running while you use it, and it reads the new order the next time it starts. Close the app (A on the final screen) instead of opening the Home menu first.

## Series table

Order inside a series follows release year, and a series sits in the list under its own name (for example *Hyrule Warriors* goes under **Zelda**). The built-in table is in `source/default_rules.cpp` and comes from memory, so some years can be off. To change it without rebuilding, create `sd:/wiiu/apps/HomeTidy/series.txt`:

```text
# pattern | series | year    (first match wins, your lines win over the built-in ones)
hyrule warriors | zelda | 2014
```

Patterns are matched against the lowercase title without punctuation or a leading "The". Titles that match nothing are sorted by name.

## Files on the SD card

```text
wiiu/apps/HomeTidy/
  backup/BaristaAccountSaveFile_<date>_<time>.dat   layout backups
  backup/icons/<titleid>_iconTex_original.tga       original icons
  config.txt                                        language (lang=en or lang=pt)
  series.txt                                        optional, your series rules
  font.ttf                                          optional, only if the system font is unavailable
  hometidy.log                                      what this run did (flushed line by line)
  hometidy.prev.log                                 the run before, kept in case of a crash
```

To undo a sort, copy a backup over the layout file (with the console off, or from another app while the Wii U Menu is closed).

## Build

You need Docker. The image ships devkitPPC, wut, SDL2 and libmocha.

```sh
docker build -t hometidy_builder .
docker run --rm -v "$PWD":/project hometidy_builder make
```

Build from a normal (APFS/ext4) folder, not from an exFAT card: macOS writes `._*` files there and breaks the build.

Host tests (no console needed, address and undefined-behaviour sanitizers on):

```sh
tests/run.sh
# optional round trip against real console files
HOMETIDY_REAL_LAYOUT=path/to/BaristaAccountSaveFile.dat HOMETIDY_REAL_ICON=path/to/iconTex.tga tests/run.sh
```

UI preview without a console (renders every screen to BMP files inside a Linux container):

```sh
python3 tools/make_fake_console.py /tmp/fake_console
tools/host_preview.sh /tmp/fake_console /tmp/shots
```

## Layout file format

Reverse engineered by [Yardape8000/menu_sort](https://github.com/Yardape8000/menu_sort) and confirmed on a console:

- Entries are 16 bytes, big endian: title hi, title lo, type, padding.
- Home page: 300 NAND entries at `0x0`, then 300 USB entries at `0x12C0`.
- Folder *N* (1-based) at `0x2D24 + (N - 1) * 1976`: 60 NAND entries, 60 USB entries, then 56 bytes of info (UTF-16BE name, colour).
- A folder position holds its title in either the NAND or the USB entry; the other stays empty (type 1 for NAND, 2 for USB).

## Credits

Layout format: Yardape8000. Libraries: [wut](https://github.com/devkitPro/wut), [libmocha](https://github.com/wiiu-env/libmocha), SDL2 and SDL2_ttf/SDL2_gfx for the Wii U. Licensed under GPL-2.0.
