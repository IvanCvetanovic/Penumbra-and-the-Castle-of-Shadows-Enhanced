#!/usr/bin/env python3
"""Placeholder art for the on-screen touch controls (E16, game/render/TouchControls).

Writes simple, readable buttons to game/data/images/touch/: white icons on translucent dark discs,
at twice the logical size game/data/touch_controls.json draws them at (a 120 px button is a 240 px
image), so they stay sharp on a phone whose screen has more pixels than the logical 768.

    python tools/art/make_touch_art.py

Needs Pillow. Drawn from shapes only (no fonts, no text), so the art is the same in both languages
and is ours outright.

THESE ARE PLACEHOLDERS. Ivan's ruling (2026-09-28): the real buttons may come from Magic Rampage,
which is not on this machine. Swapping them is data only: replace the PNGs (any size; the
manifest's "size" is what they are drawn at, stretched) and edit touch_controls.json - anchor
corner, offset, size, hit padding, the direction control's arrows and knob. Two things the art must
keep: straight (not premultiplied) alpha, and no pixel of exact magenta #FF00FF, which the HUD's
TextureCache keys out as 0.7.12 keyed every sprite (TextureVariant::Sprite).

WHAT EACH IS (the icons follow the gamepad mapping E3 gave the actions: A jump, X sword, B fire,
Y light, and the diamond is laid out as a pad's face buttons):
  dpad.png          the direction control's disc, with a faint ring where the dead zone ends
  dpad_left.png     its left arrow alone, drawn over the disc and brightened while held
  dpad_right.png    its right arrow
  dpad_down.png     its down arrow (the next_level door, the spell combo); there is no up
  dpad_knob.png     the knob that follows the thumb
  jump.png          an up arrow            (K_CTRL)
  sword.png         a sword                (K_S)
  fire.png          a flame                (K_D, the fire ball)
  light.png         a sun                  (K_SPACE, the light spell)
  combo_sword.png   two chevrons and a sword    the sword combo: side, side, sword
  combo_spell.png   a down-then-on arrow and a fire ball    the spell combo: down, side, fire
  pause.png         two bars               (K_ESC in play: E13's pause)
  back.png          a return arrow         (K_ESC elsewhere: back to the menu)
"""

import math
import pathlib

from PIL import Image, ImageDraw

REPO = pathlib.Path(__file__).resolve().parents[2]
OUT = REPO / "game" / "data" / "images" / "touch"

# Drawn this many times larger, then scaled down: Pillow's shapes have no antialiasing.
SUPERSAMPLE = 4

# (pixel size of the PNG) = 2 x the logical size touch_controls.json gives each.
BUTTON = 240       # jump, sword, fire, light: 120 logical
CORNER = 168       # pause, back: 84 logical
DPAD = 520         # 260 logical
KNOB = 208         # 104 logical
COMBO = 200        # swordCombo, spellCombo: 100 logical

WHITE = (255, 255, 255, 255)
DISC = (8, 8, 12, 150)          # the translucent dark under every icon
RING = (255, 255, 255, 215)


class Canvas:
    """A square drawn in unit coordinates: (0, 0) the centre, +-1 the edges, +y down."""

    def __init__(self, size):
        self.size = size
        self.big = size * SUPERSAMPLE
        self.image = Image.new("RGBA", (self.big, self.big), (0, 0, 0, 0))
        self.draw = ImageDraw.Draw(self.image)

    def px(self, x, y):
        half = self.big / 2.0
        return (half + x * half, half + y * half)

    def length(self, unit):
        return unit * self.big / 2.0

    def polygon(self, points, fill):
        self.draw.polygon([self.px(x, y) for x, y in points], fill=fill)

    def circle(self, x, y, r, fill=None, outline=None, width=0.0):
        (x0, y0), (x1, y1) = self.px(x - r, y - r), self.px(x + r, y + r)
        self.draw.ellipse((x0, y0, x1, y1), fill=fill, outline=outline,
                          width=max(1, round(self.length(width))) if outline else 0)

    def rounded(self, x0, y0, x1, y1, radius, fill):
        a, b = self.px(x0, y0), self.px(x1, y1)
        self.draw.rounded_rectangle((a[0], a[1], b[0], b[1]), radius=self.length(radius), fill=fill)

    def arc(self, x, y, r, start, end, width, fill):
        (x0, y0), (x1, y1) = self.px(x - r, y - r), self.px(x + r, y + r)
        self.draw.arc((x0, y0, x1, y1), start, end, fill=fill, width=max(1, round(self.length(width))))

    def save(self, name):
        # Down-sampled premultiplied, so the edges of white on transparent do not pick up the
        # black of the transparent pixels; written back as straight alpha.
        small = self.image.convert("RGBa").resize((self.size, self.size), Image.Resampling.LANCZOS).convert("RGBA")
        OUT.mkdir(parents=True, exist_ok=True)
        small.save(OUT / name, optimize=True)
        print(f"  {name}  {self.size}x{self.size}")


def rotate(points, degrees):
    """Clockwise on screen (y is down)."""
    c, s = math.cos(math.radians(degrees)), math.sin(math.radians(degrees))
    return [(x * c - y * s, x * s + y * c) for x, y in points]


def button_base(size):
    canvas = Canvas(size)
    canvas.circle(0, 0, 0.955, fill=DISC)
    canvas.circle(0, 0, 0.93, outline=RING, width=0.05)
    return canvas


def jump():
    c = button_base(BUTTON)
    c.polygon([(0.0, -0.58), (0.40, -0.10), (0.15, -0.10), (0.15, 0.46),
               (-0.15, 0.46), (-0.15, -0.10), (-0.40, -0.10)], WHITE)
    c.save("jump.png")


def sword():
    c = button_base(BUTTON)
    # Pointing up, then turned 45 degrees to point up and right.
    blade = [(0.0, -0.66), (0.085, -0.50), (0.085, 0.20), (-0.085, 0.20), (-0.085, -0.50)]
    guard = [(-0.28, 0.20), (0.28, 0.20), (0.28, 0.29), (-0.28, 0.29)]
    grip = [(-0.05, 0.29), (0.05, 0.29), (0.05, 0.50), (-0.05, 0.50)]
    for part in (blade, guard, grip):
        c.polygon(rotate(part, 45), WHITE)
    px, py = rotate([(0.0, 0.57)], 45)[0]
    c.circle(px, py, 0.085, fill=WHITE)
    # The fuller: a dark groove down the blade.
    c.polygon(rotate([(-0.022, -0.44), (0.022, -0.44), (0.022, 0.14), (-0.022, 0.14)], 45), DISC)
    c.save("sword.png")


def flame_points(scale, centre, lean, steps=180):
    """A teardrop with its point up, its tip bent sideways by `lean` (+ right)."""
    points = []
    for i in range(steps):
        t = 2.0 * math.pi * i / steps
        x = math.sin(t) * math.sin(t / 2.0) ** 1.4
        y = -math.cos(t)
        tip = max(0.0, -y)
        x += lean * tip * tip
        points.append((centre[0] + x * scale * 0.62, centre[1] + y * scale))
    return points


def fire():
    c = button_base(BUTTON)
    # A tall tongue bent right, a short one bent left, a dark heart low in the middle.
    c.polygon(flame_points(0.58, (0.04, 0.04), 0.45), WHITE)
    c.polygon(flame_points(0.36, (-0.17, 0.25), -0.55), WHITE)
    c.polygon(flame_points(0.22, (0.02, 0.36), 0.25), DISC)
    c.save("fire.png")


def light():
    c = button_base(BUTTON)
    c.circle(0, 0, 0.22, fill=WHITE)
    for k in range(8):
        ray = [(-0.055, -0.33), (0.055, -0.33), (0.028, -0.60), (-0.028, -0.60)]
        c.polygon(rotate(ray, k * 45.0), WHITE)
    c.save("light.png")


def chevron(c, x, y, size, thickness):
    """A '>' pointing right, its tip at (x + size/2, y)."""
    c.polygon([(x - size / 2, y - size), (x - size / 2 + thickness, y - size), (x + size / 2 + thickness, y),
               (x - size / 2 + thickness, y + size), (x - size / 2, y + size), (x + size / 2, y)], WHITE)


def combo_sword():
    c = button_base(COMBO)
    # "Forward, forward, strike": two chevrons, then a smaller sword up and right.
    chevron(c, -0.52, 0.32, 0.17, 0.12)
    chevron(c, -0.28, 0.32, 0.17, 0.12)
    parts = [
        [(0.0, -0.66), (0.085, -0.50), (0.085, 0.20), (-0.085, 0.20), (-0.085, -0.50)],
        [(-0.28, 0.20), (0.28, 0.20), (0.28, 0.29), (-0.28, 0.29)],
        [(-0.05, 0.29), (0.05, 0.29), (0.05, 0.50), (-0.05, 0.50)],
    ]
    for part in parts:
        c.polygon([(0.28 + 0.62 * x, -0.20 + 0.62 * y) for x, y in rotate(part, 45)], WHITE)
    px, py = rotate([(0.0, 0.57)], 45)[0]
    c.circle(0.28 + 0.62 * px, -0.20 + 0.62 * py, 0.055, fill=WHITE)
    c.save("combo_sword.png")


def combo_spell():
    c = button_base(COMBO)
    # "Down, then on, then fire": an arrow that goes down and turns forward,
    # and a fire ball with its tail behind it.
    c.rounded(-0.62, -0.52, -0.46, 0.30, 0.03, WHITE)
    c.rounded(-0.62, 0.14, -0.02, 0.30, 0.03, WHITE)
    c.polygon([(0.14, 0.22), (-0.08, 0.02), (-0.08, 0.42)], WHITE)
    c.circle(0.30, -0.22, 0.20, fill=WHITE)
    for dy, length in ((-0.12, 0.40), (0.0, 0.52), (0.12, 0.40)):
        c.polygon([(0.26, -0.22 + dy - 0.07), (0.26 - length, -0.22 + dy * 1.6), (0.26, -0.22 + dy + 0.07)], WHITE)
    c.circle(0.30, -0.22, 0.09, fill=DISC)
    c.save("combo_spell.png")


def pause():
    c = button_base(CORNER)
    c.rounded(-0.30, -0.40, -0.09, 0.40, 0.05, WHITE)
    c.rounded(0.09, -0.40, 0.30, 0.40, 0.05, WHITE)
    c.save("pause.png")


def back():
    c = button_base(CORNER)
    # A return arrow: the right half of a ring, its top end running left into the head.
    c.arc(0.06, 0.06, 0.30, 270, 90, 0.15, WHITE)
    c.rounded(-0.18, -0.315, 0.10, -0.165, 0.0, WHITE)
    c.polygon([(-0.46, -0.24), (-0.16, -0.50), (-0.16, 0.02)], WHITE)
    c.rounded(-0.06, 0.285, 0.10, 0.435, 0.0, WHITE)
    c.save("back.png")


def dpad():
    c = Canvas(DPAD)
    c.circle(0, 0, 0.975, fill=(8, 8, 12, 120))
    c.circle(0, 0, 0.955, outline=RING, width=0.03)
    # Where the dead zone ends (touch_controls.json "deadZone" 0.25 of the radius).
    c.circle(0, 0, 0.25, outline=(255, 255, 255, 70), width=0.015)
    c.save("dpad.png")

    def arrow(name, points):
        a = Canvas(DPAD)
        a.polygon(points, WHITE)
        a.save(name)

    arrow("dpad_left.png", [(-0.88, 0.0), (-0.58, -0.22), (-0.58, 0.22)])
    arrow("dpad_right.png", [(0.88, 0.0), (0.58, -0.22), (0.58, 0.22)])
    arrow("dpad_down.png", [(0.0, 0.88), (-0.22, 0.58), (0.22, 0.58)])


def knob():
    c = Canvas(KNOB)
    c.circle(0, 0, 0.94, fill=(255, 255, 255, 70))
    c.circle(0, 0, 0.90, outline=(255, 255, 255, 220), width=0.07)
    c.save("dpad_knob.png")


def check_no_magenta():
    for path in sorted(OUT.glob("*.png")):
        data = Image.open(path).convert("RGBA").tobytes()
        if any(data[i:i + 3] == b"\xff\x00\xff" for i in range(0, len(data), 4)):
            raise SystemExit(f"{path.name} has exact magenta, which the HUD keys out")


def main():
    print(f"writing {OUT.relative_to(REPO)}")
    for make in (dpad, knob, jump, sword, fire, light, combo_sword, combo_spell, pause, back):
        make()
    check_no_magenta()


if __name__ == "__main__":
    main()
