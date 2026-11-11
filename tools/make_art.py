#!/usr/bin/env python3
"""Generates resources/icon.png, tv.png and drc.png (app icon and splash screens)."""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parent.parent / "resources"
BG, CARD, GREEN, WHITE, MUTED = (15, 23, 42), (25, 33, 52), (21, 128, 61), (248, 250, 252), (148, 163, 184)
FONTS = ["/System/Library/Fonts/Supplemental/Arial Bold.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"]


def font(size: int) -> ImageFont.FreeTypeFont:
    for p in FONTS:
        if Path(p).exists():
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()


def badge(d: ImageDraw.ImageDraw, cx: int, cy: int, r: int) -> None:
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=GREEN)
    w = max(3, r // 4)
    pts = [(cx - r * 0.45, cy + r * 0.02), (cx - r * 0.1, cy + r * 0.4), (cx + r * 0.5, cy - r * 0.38)]
    d.line(pts, fill=WHITE, width=w, joint="curve")
    for x, y in (pts[0], pts[2]):
        d.ellipse((x - w / 2, y - w / 2, x + w / 2, y + w / 2), fill=WHITE)


def icon() -> Image.Image:
    s = 4
    im = Image.new("RGBA", (128 * s, 128 * s), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((0, 0, 128 * s - 1, 128 * s - 1), radius=26 * s, fill=BG + (255,))
    for i, w in enumerate((60, 48, 36, 24)):  # sorted bars
        d.rounded_rectangle((22 * s, (28 + i * 14) * s, (22 + w) * s, (28 + i * 14 + 8) * s), radius=4 * s,
                            fill=(34, 197, 94) if i == 0 else MUTED)
    badge(d, 92 * s, 92 * s, 24 * s)
    return im.resize((128, 128), Image.LANCZOS)


def splash(w: int, h: int) -> Image.Image:
    im = Image.new("RGB", (w, h), BG)
    d = ImageDraw.Draw(im)
    r = h // 9
    badge(d, w // 2, h // 2 - r // 2, r)
    d.text((w // 2, h // 2 + r * 1.3), "HomeTidy", font=font(h // 9), fill=WHITE, anchor="mm")
    d.text((w // 2, h // 2 + r * 2.4), "Sort your Home. Mark what you finished.", font=font(h // 24), fill=MUTED, anchor="mm")
    return im


if __name__ == "__main__":
    OUT.mkdir(exist_ok=True)
    icon().save(OUT / "icon.png")
    splash(1280, 720).save(OUT / "tv.png")
    splash(854, 480).save(OUT / "drc.png")
