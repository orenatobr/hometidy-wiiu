#!/usr/bin/env python3
"""Builds a fake console tree (SD, USB titles, layout file) for the host preview build.

Usage: make_fake_console.py <out_dir>
"""
import colorsys
import random
import struct
import sys
from pathlib import Path

WII_U = ["Super Mario 3D World", "Mario Kart 8", "The Legend of Zelda: Breath of the Wild", "Splatoon",
         "Xenoblade Chronicles X", "Hyrule Warriors", "Bayonetta 2", "Pikmin 3", "Super Smash Bros. for Wii U",
         "New Super Mario Bros. U", "Donkey Kong Country: Tropical Freeze", "Captain Toad: Treasure Tracker",
         "The Legend of Zelda: The Wind Waker HD", "Pokémon Rumble U", "Kirby and the Rainbow Curse",
         "Star Fox Zero", "Yoshi's Woolly World", "Mario Party 10", "Wii Party U", "Rayman Legends", "Lego City Undercover"]
WII = ["Super Mario Galaxy", "Super Mario Galaxy 2", "Mario Kart Wii", "Wii Sports Resort", "Xenoblade Chronicles",
       "Metroid Prime 3: Corruption", "New Super Mario Bros. Wii", "Kirby's Return to Dream Land", "Wii Fit Plus",
       "Donkey Kong Country Returns", "The Legend of Zelda: Twilight Princess", "Punch-Out!!", "Just Dance 3"]


def put(buf: bytearray, off: int, hi: int, lo: int, typ: int) -> None:
    buf[off:off + 16] = struct.pack(">IIII", hi, lo, typ, 0)


def tga(seed: int) -> bytes:
    hue = (seed * 0.137) % 1.0
    px = bytearray()
    for y in range(128):
        for x in range(128):
            r, g, b = colorsys.hsv_to_rgb(hue, 0.55, 0.35 + 0.5 * (y / 127))
            if 28 < x < 100 and 28 < y < 100 and (x // 12 + y // 12) % 2:
                r, g, b = colorsys.hsv_to_rgb((hue + 0.5) % 1, 0.4, 0.9)
            px += bytes((int(b * 255), int(g * 255), int(r * 255), 255))
    head = bytes([0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 128, 0, 128, 0, 32, 8])
    return head + bytes(px) + b"\0" * 8 + b"TRUEVISION-XFILE.\0"


def main(out: Path) -> None:
    random.seed(7)
    layout = bytearray(0xAA for _ in range(0x2D24 + 60 * 1976))
    for i in range(300):  # empty Home pages
        put(layout, i * 16, 0, 0, 1)
        put(layout, 0x12C0 + i * 16, 0, 0, 2)
    for f in range(60):
        base = 0x2D24 + f * 1976
        for i in range(60):
            put(layout, base + i * 16, 0, 0, 1)
            put(layout, base + 960 + i * 16, 0, 0, 2)
    for folder, (name, names, kind, seedbase) in enumerate(
            [("Wii U", WII_U, 0x00050000, 0x10100000), ("Wii", WII, 0x00050002, 0x52000000)]):
        base = 0x2D24 + folder * 1976
        info = base + 1920
        layout[info:info + 34] = name.encode("utf-16-be").ljust(34, b"\0")
        order = list(range(len(names)))
        random.shuffle(order)
        for slot, idx in enumerate(order):
            put(layout, base + 960 + slot * 16, kind, seedbase + idx * 0x100, 2)
            tdir = out / "usb/usr/title" / f"{kind:08x}" / f"{seedbase + idx * 0x100:08x}" / "meta"
            tdir.mkdir(parents=True, exist_ok=True)
            title = names[idx].replace("&", "&amp;")
            (tdir / "meta.xml").write_text(f'<?xml version="1.0"?><menu><longname_en>{title}</longname_en></menu>',
                                           encoding="utf-8")
            (tdir / "iconTex.tga").write_bytes(tga(idx + folder * 31))
    # Home pages: system apps with gaps, plus the two folders
    sysapps = {0x1004a200: "Mii Maker", 0x10047200: "System Settings", 0x1005a200: "Wii U Chat", 0x1004c200: "Daily Log"}
    slot = {0x1005a200: 0, 0x10047200: 2, 0x1004a200: 5, 0x1004c200: 9}
    for lo, name in sysapps.items():
        put(layout, slot[lo] * 16, 0x00050010, lo, 1)
        meta = out / "mlc/sys/title/00050010" / f"{lo:08x}" / "meta"
        meta.mkdir(parents=True, exist_ok=True)
        (meta / "meta.xml").write_text(f'<menu><longname_en type="string" length="512">{name}</longname_en></menu>')
    put(layout, 15 * 16, 0x00050010, 0x10055000, 5)  # a system icon with no readable name
    put(layout, 21 * 16, 0, 1, 0x10)
    put(layout, 23 * 16, 0, 2, 0x10)
    lay_dir = out / "sd/wiiu/homebrew_on_menu_plugin/FEH100000000/save/80000006"
    lay_dir.mkdir(parents=True, exist_ok=True)
    (lay_dir / "BaristaAccountSaveFile.dat").write_bytes(layout)


if __name__ == "__main__":
    main(Path(sys.argv[1]))
