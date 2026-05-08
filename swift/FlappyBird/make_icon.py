"""Generate the synthwave app icon (1024x1024 PNG) for Flappy Bird.

Composition: deep-purple → hot-magenta → orange gradient sky with a
low horizon, an Outrun-style perspective grid in pink/cyan receding to
the vanishing point, and a glowing wireframe cube in the foreground —
the current MyGame placeholder, foreshadowing the eventual bird.

Usage:
    python3 make_icon.py [output_path]

The icon ships in `FlappyBird/Assets.xcassets/AppIcon.appiconset/`;
discrete macOS sizes are downscaled from the 1024×1024 master with
`sips -z N N icon-1024.png --out mac-N.png` (and `2N` for @2x).
"""

import random
import sys

from PIL import Image, ImageDraw, ImageFilter


SIZE = 1024
HORIZON = int(SIZE * 0.55)  # sky 0..0.55, ground 0.55..1.0


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def sky():
    """Vertical sky gradient: indigo top → magenta band → orange horizon."""
    deep = (24, 8, 48)
    purple = (75, 20, 110)
    magenta = (220, 40, 140)
    orange = (255, 120, 60)
    img = Image.new("RGB", (SIZE, HORIZON), (0, 0, 0))
    for y in range(HORIZON):
        t = y / max(1, HORIZON - 1)
        if t < 0.5:
            c = lerp(deep, purple, t / 0.5)
        elif t < 0.85:
            c = lerp(purple, magenta, (t - 0.5) / 0.35)
        else:
            c = lerp(magenta, orange, (t - 0.85) / 0.15)
        img.paste(Image.new("RGB", (SIZE, 1), c), (0, y))
    return img


def stars(img):
    """Scatter of dim stars in the upper sky. Seeded for reproducibility."""
    rng = random.Random(42)
    overlay = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(overlay)
    for _ in range(80):
        x = rng.randint(0, SIZE - 1)
        y = rng.randint(0, int(HORIZON * 0.6))
        a = rng.randint(120, 255)
        s = rng.choice([1, 1, 1, 2])
        d.ellipse((x, y, x + s, y + s), fill=(255, 255, 255, a))
    img.paste(overlay, (0, 0), overlay)
    return img


def sun(img):
    """Glowing yellow disk at the horizon with the classic Outrun scanlines."""
    cx, cy = SIZE // 2, HORIZON
    radius = int(SIZE * 0.18)
    layer = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    for alpha, scale in [(60, 1.8), (110, 1.4), (200, 1.1), (255, 1.0)]:
        r = int(radius * scale)
        d.ellipse((cx - r, cy - r, cx + r, cy + r),
                  fill=(255, 200, 90, alpha))
    layer = layer.filter(ImageFilter.GaussianBlur(radius=2))
    img.paste(layer, (0, 0), layer)
    # Black scanline bands cut the sun horizontally.
    for i, y_off in enumerate([-6, 8, 24, 44, 70]):
        band_h = max(2, 4 + i * 2)
        y0 = cy - 6 + y_off
        ImageDraw.Draw(img).rectangle(
            (cx - radius * 1.6, y0, cx + radius * 1.6, y0 + band_h),
            fill=(0, 0, 0))
    return img


def ground():
    """Black ground tile with a perspective grid receding to (cx, top)."""
    h = SIZE - HORIZON
    g = Image.new("RGB", (SIZE, h), (4, 0, 12))
    d = ImageDraw.Draw(g, "RGBA")
    cx = SIZE // 2
    near = (255, 60, 180, 220)   # hot magenta close
    far = (60, 220, 255, 110)    # cyan toward horizon

    def blend(t):
        return tuple(int(far[i] + (near[i] - far[i]) * t) for i in range(4))

    # Horizontal lines, concentrated toward the camera by z**1.6.
    for i in range(1, 13):
        z = i / 12
        y = int(h * (z ** 1.6))
        d.line([(0, y), (SIZE, y)], fill=blend(z), width=max(1, int(2 + 5 * z)))

    # Vertical lines fanning from the vanishing point at (cx, 0).
    for i in range(-16, 17):
        x_far = cx + i * (SIZE * 0.12)
        for seg in range(20):
            t1 = seg / 20
            t2 = (seg + 1) / 20
            x1, y1 = cx + (x_far - cx) * t1, h * t1
            x2, y2 = cx + (x_far - cx) * t2, h * t2
            d.line([(x1, y1), (x2, y2)],
                   fill=blend(t1), width=max(1, int(1 + 3 * t1)))
    return g


def cube(img):
    """Foreground glowing wireframe cube: front face + offset back face,
    six visible edges connecting them. Sits low so the iOS rounded-rect
    mask doesn't clip it."""
    cx = SIZE // 2
    cy = int(SIZE * 0.78)
    s = int(SIZE * 0.16)
    fx0, fy0, fx1, fy1 = cx - s, cy - s, cx + s, cy + s
    dx, dy = -int(s * 0.55), -int(s * 0.55)
    bx0, by0, bx1, by1 = fx0 + dx, fy0 + dy, fx1 + dx, fy1 + dy

    # Soft glow underneath, drawn into a separate layer and blurred.
    glow = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    for w, a in [(28, 50), (16, 80), (8, 130)]:
        c = (255, 60, 200, a)
        gd.rectangle((fx0, fy0, fx1, fy1), outline=c, width=w)
        gd.rectangle((bx0, by0, bx1, by1), outline=c, width=w)
        gd.line((fx0, fy0, bx0, by0), fill=c, width=w)
        gd.line((fx1, fy0, bx1, by0), fill=c, width=w)
        gd.line((fx0, fy1, bx0, by1), fill=c, width=w)
        gd.line((fx1, fy1, bx1, by1), fill=c, width=w)
    glow = glow.filter(ImageFilter.GaussianBlur(radius=10))

    # Sharp magenta edges on top of the glow.
    sharp = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    sd = ImageDraw.Draw(sharp)
    edge = (255, 60, 200, 255)
    sd.rectangle((fx0, fy0, fx1, fy1), outline=edge, width=6)
    sd.rectangle((bx0, by0, bx1, by1), outline=edge, width=6)
    sd.line((fx0, fy0, bx0, by0), fill=edge, width=6)
    sd.line((fx1, fy0, bx1, by0), fill=edge, width=6)
    sd.line((fx0, fy1, bx0, by1), fill=edge, width=6)
    sd.line((fx1, fy1, bx1, by1), fill=edge, width=6)

    img.paste(glow, (0, 0), glow)
    img.paste(sharp, (0, 0), sharp)
    return img


def main(out_path):
    canvas = Image.new("RGB", (SIZE, SIZE), (0, 0, 0))
    canvas.paste(sky(), (0, 0))
    canvas.paste(ground(), (0, HORIZON))
    canvas = stars(canvas)
    canvas = sun(canvas)
    canvas = cube(canvas)
    canvas.save(out_path, "PNG", optimize=True)
    print(f"wrote {out_path} ({SIZE}x{SIZE})")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "icon-1024.png")
