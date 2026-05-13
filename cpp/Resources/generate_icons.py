#!/usr/bin/env python3
"""Generate Pong app icons (macOS AppIcon.icns + iOS Assets.xcassets).

Run from anywhere; outputs land next to this script. Requires Pillow and
the macOS `iconutil` tool (preinstalled on macOS).

    python3 generate_icons.py
"""

from __future__ import annotations

import shutil
import subprocess
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

# macOS squircle approximation. Apple's template uses a continuous-curvature
# squircle; PIL's rounded_rectangle gives circular corners, which reads the
# same in the dock at typical sizes.
MAC_CORNER_RADIUS = 185


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


def _mac_icon(size: int) -> Image.Image:
    """macOS variant: Pong inside a rounded square, transparent outside."""
    base = _draw_pong(size)
    radius = MAC_CORNER_RADIUS * size / REF
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        (0, 0, size - 1, size - 1), radius=radius, fill=255
    )
    out = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    out.paste(base, (0, 0), mask)
    return out


def _ios_icon(size: int) -> Image.Image:
    """iOS variant: full-bleed RGB; the system applies its own squircle mask."""
    return _draw_pong(size).convert("RGB")


def build_macos():
    iconset = HERE / "AppIcon.iconset"
    if iconset.exists():
        shutil.rmtree(iconset)
    iconset.mkdir()
    # iconutil expects this exact naming convention.
    for pt in (16, 32, 128, 256, 512):
        for scale in (1, 2):
            px = pt * scale
            suffix = "@2x" if scale == 2 else ""
            _mac_icon(px).save(iconset / f"icon_{pt}x{pt}{suffix}.png")
    subprocess.run(
        ["iconutil", "-c", "icns", str(iconset), "-o", str(HERE / "AppIcon.icns")],
        check=True,
    )
    shutil.rmtree(iconset)


def build_ios():
    xcassets = HERE / "Assets.xcassets"
    appicon = xcassets / "AppIcon.appiconset"
    appicon.mkdir(parents=True, exist_ok=True)
    _ios_icon(1024).save(appicon / "icon-1024.png")
    (appicon / "Contents.json").write_text(
        '{\n'
        '  "images" : [\n'
        '    {\n'
        '      "filename" : "icon-1024.png",\n'
        '      "idiom" : "universal",\n'
        '      "platform" : "ios",\n'
        '      "size" : "1024x1024"\n'
        '    }\n'
        '  ],\n'
        '  "info" : {\n'
        '    "author" : "xcode",\n'
        '    "version" : 1\n'
        '  }\n'
        '}\n'
    )
    (xcassets / "Contents.json").write_text(
        '{\n'
        '  "info" : {\n'
        '    "author" : "xcode",\n'
        '    "version" : 1\n'
        '  }\n'
        '}\n'
    )


def main():
    build_macos()
    build_ios()


if __name__ == "__main__":
    main()
