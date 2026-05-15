#!/usr/bin/env python3
"""Generate the Pong Windows app icon (AppIcon.ico).

Run from anywhere; output lands next to this script. Requires Pillow.

    python3 generate_icons.py

The .ico is committed alongside this script so the build doesn't depend on
Python being on PATH. Re-run only when the artwork changes.
"""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent

# Pong palette: white on the renderer's clear-color black.
BG = (0, 0, 0, 255)
FG = (255, 255, 255, 255)

# Master layout at REF=1024. Paddles intentionally offset (left up, right
# down) and the ball pulled toward the right paddle so the icon reads as a
# mid-volley moment rather than a static diagram.
REF = 1024
LEFT_PADDLE = ((140, 220), (260, 700))
RIGHT_PADDLE = ((764, 324), (884, 804))
BALL = ((580, 472), (700, 592))

# Standard Windows ICO ladder. Windows picks the closest size for each
# surface (16/20 tray, 32 title bar, 48 Explorer, 256 large-thumbnail) and
# downsamples from the nearest larger entry; including the rungs explicitly
# avoids a 256→16 resample for the tray icon.
ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]


def _scale(box, size):
    s = size / REF
    (x0, y0), (x1, y1) = box
    return (x0 * s, y0 * s, x1 * s, y1 * s)


def _draw_pong(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), BG)
    d = ImageDraw.Draw(img)
    for box in (LEFT_PADDLE, RIGHT_PADDLE, BALL):
        d.rectangle(_scale(box, size), fill=FG)
    return img


def main():
    master = _draw_pong(max(ICO_SIZES))
    master.save(HERE / "AppIcon.ico", sizes=[(s, s) for s in ICO_SIZES])


if __name__ == "__main__":
    main()
