#!/usr/bin/env python3
"""The on-screen touch controls' art (E16, game/render/TouchControls), from Magic Rampage's screen pad.

Magic Rampage (Asantee Games) is the Android game by the studio that made Penumbra in 2010. Its
touch buttons are the look E16's controls take, used with the authors' permission as Ivan stated
it. This reads the pad's PNGs from Magic Rampage 7.8.7's own package, read only - the .xapk
(its base APK, com.asanteegames.magicrampage.apk, is a zip inside it) or a folder the base APK's
assets/ were extracted to:

    python tools/art/make_mr_touch_art.py --xapk "<path>/Magic+Rampage_7.8.7_APKPure.xapk"
    python tools/art/make_mr_touch_art.py --assets out/mr_extract/assets

and writes game/data/images/touch/*.png. Those PNGs are committed: building and playing the game
never need this script or Magic Rampage's package; it records how they were made, and remakes
them for whoever has the package. Needs Pillow and numpy. Magic Rampage's art is palette
PNG with a tRNS chunk; what is written is straight-alpha RGBA, with no colour under full
transparency (a filtered, magnified sprite would bleed it in) and no pixel of exact magenta
#FF00FF, which the HUD's TextureCache keys out.

HOW MAGIC RAMPAGE BUILDS ITS PAD (CharacterScreenPadController, in its compiled script
assets/game_32.bin): six square buttons - left, right, jump, attack, pause and the Arcane Rune's
one-tap special - each one sprites/dpad-frame.png (a 128 px light-grey rounded square with a white
bevelled rim and a soft drop shadow) with a pre-baked icon: light-grey fill, black outline, a soft
shadow to the lower right. A press tints the button; there is no pressed image. 128 px only (no hd/
or fullhd/ variant). So the parts Penumbra needs and Magic Rampage lacks are built the same way,
from its parts: its blank frame plus a glyph of its own, drawn in its icons' style.

WHAT EACH FILE IS (Magic Rampage paths are under the base APK's assets/):
  jump.png         sprites/jump-pad-button.png, as shipped (K_CTRL)
  sword.png        sprites/attack-pad-button.png, as shipped: its melee button (K_S)
  fire.png         sprites/dpad-frame.png + sprites/elements/element-fire.png, the white glyph
                   re-drawn in the pad icons' style (K_D, the fire ball)
  light.png        sprites/dpad-frame.png + sprites/elements/element-light.png, the same way
                   (K_SPACE, the light spell)
  combo_sword.png  sprites/dpad-frame.png + sprites/button_rune_arcane_off.png: the Arcane Rune,
                   Magic Rampage's one-tap special ("Forward three times and Attack", its
                   tips.strings tip26), whose glyph is three chevrons and a sword - side, side,
                   sword. The gem on the frame is this script's; the pairing in Magic Rampage is
                   inferred (the two sit together in its script's constant pool).
  combo_spell.png  sprites/dpad-frame.png + entities/rune_fire.png, its fire rune gem: the pair
                   of the one above for the spell combo (down, side, fire)
  pause.png        sprites/pause-pad-button.png cropped to its pill, 128x86 (the pill sits in the
                   lower part of a 128 square)
  back.png         sprites/inventory-back-button.png, as shipped: Magic Rampage's back button on
                   some forty screens
  dpad_left.png    the direction control (one control: the thumb slides between its buttons, see
  dpad_right.png   TouchControls.hpp) as three of Magic Rampage's buttons - sprites/dpad-left.png,
  dpad_down.png    sprites/dpad-right.png, and a down button made from dpad-right (its face
                   mirrored across the diagonal onto dpad-frame; Magic Rampage has no down) - each
                   alone on a transparent canvas the size of the control, where the control's
                   sectors put its direction. Each brightens while its direction is held, as a
                   pressed Magic Rampage button does.
  dpad.png         the control's base: transparent. Magic Rampage has no disc under its buttons.
  dpad_knob.png    SEF/media/button-highlight.png, the four corner brackets Magic Rampage's UI
                   frames a focused button with: it follows the thumb, onto the button held.

Sizes: touch_controls.json draws these at logical pixels (768 = the screen's height); each 128 px
button is drawn at 96-120, and the direction control's canvas maps DPAD_CANVAS px to DPAD_BOX. The
direction buttons' places are checked here against the sectors TouchControls.cpp reads the thumb
by, so that the left and right buttons are left and right alone wherever the thumb lands on them.
"""

import argparse
import io
import math
import pathlib
import sys
import zipfile

import numpy as np
from PIL import Image, ImageFilter

REPO = pathlib.Path(__file__).resolve().parents[2]
OUT = REPO / "game" / "data" / "images" / "touch"
BASE_APK = "com.asanteegames.magicrampage.apk"

# The direction control, as touch_controls.json has it: its box (logical px, square) and where each
# button's centre is from the box's centre (logical px, +y down). The canvas is DPAD_CANVAS px, so
# a 128 px button is 128 * DPAD_BOX / DPAD_CANVAS = 104 logical px.
DPAD_BOX = 330.0
DPAD_CANVAS = 406
DPAD_CENTRES = {
    "left": (-113.0, -18.0),
    "right": (113.0, -18.0),
    "down": (0.0, 111.0),
}
# TouchControls.cpp: the dead zone (touch_controls.json "deadZone", of the radius) and tan(22.5).
DEAD_ZONE = 0.25
TAN22 = 0.41421356

# Magic Rampage's pad icons (measured on dpad-left.png): a fill of about 215 grey, a black outline
# about 3.5 px wide, a shadow to the lower right that darkens the face to about 0.4 next to the
# icon and fades over some 12 px.
ICON_FILL = (215, 215, 215)
ICON_OUTLINE = 3.5
SHADOW_OFFSET = (3.0, 3.0)
SHADOW_BLUR = 3.5
SHADOW_STRENGTH = 0.6
SUPERSAMPLE = 4


class Source:
    """Magic Rampage's assets/, from the .xapk's base APK or an extracted folder."""

    def __init__(self, xapk=None, assets=None):
        self.apk = None
        self.assets = None
        if xapk:
            xapk = pathlib.Path(xapk)
            if not xapk.is_file():
                raise SystemExit(f"--xapk: no file at {xapk}")
            # Opened for reading only.
            try:
                with zipfile.ZipFile(xapk, "r") as outer:
                    self.apk = zipfile.ZipFile(io.BytesIO(outer.read(BASE_APK)), "r")
            except zipfile.BadZipFile:
                raise SystemExit(f"--xapk: {xapk} is not a zip archive, which an .xapk is") from None
            except KeyError:
                raise SystemExit(f"--xapk: {xapk} holds no {BASE_APK}; is it Magic Rampage's .xapk?") from None
        else:
            self.assets = pathlib.Path(assets)
            if not (self.assets / "sprites" / "dpad-frame.png").is_file():
                raise SystemExit(f"--assets: {self.assets} has no sprites/dpad-frame.png; "
                                 "give the folder holding the base APK's assets/ contents")

    def image(self, rel):
        try:
            if self.apk is not None:
                data = self.apk.read("assets/" + rel)
            else:
                data = (self.assets / rel).read_bytes()
        except (KeyError, FileNotFoundError):
            raise SystemExit(f"Magic Rampage's assets/{rel} is missing; this script reads version 7.8.7") from None
        return straight_rgba(Image.open(io.BytesIO(data)))


def straight_rgba(image):
    """Palette + tRNS (or anything) -> RGBA, with the colour under alpha 0 made black."""
    rgba = np.array(image.convert("RGBA"))
    rgba[rgba[:, :, 3] == 0, :3] = 0
    return Image.fromarray(rgba, "RGBA")


def resize(image, size):
    """Resampled premultiplied, so a transparent edge does not bleed its colour in."""
    return image.convert("RGBa").resize(size, Image.Resampling.LANCZOS).convert("RGBA")


def fit(image, box):
    """Cropped to what is visible and scaled to fit a box x box square."""
    image = image.crop(image.getchannel("A").getbbox())
    scale = box / max(image.width, image.height)
    return resize(image, (max(1, round(image.width * scale)), max(1, round(image.height * scale))))


def disc_dilate(mask, radius):
    """The mask grown by a disc of `radius` px (max over the disc's offsets)."""
    out = mask.copy()
    r = int(math.ceil(radius))
    h, w = mask.shape
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            if dx * dx + dy * dy > radius * radius:
                continue
            shifted = np.zeros_like(mask)
            shifted[max(0, dy):h + min(0, dy), max(0, dx):w + min(0, dx)] = \
                mask[max(0, -dy):h - max(0, dy), max(0, -dx):w - max(0, dx)]
            np.maximum(out, shifted, out=out)
    return out


def shadow_layer(alpha):
    """Magic Rampage's soft shadow to the lower right, from an alpha mask (0..1, supersampled)."""
    s = SUPERSAMPLE
    shadow = Image.fromarray((alpha * 255).astype(np.uint8), "L")
    shadow = shadow.transform(shadow.size, Image.Transform.AFFINE,
                              (1, 0, -SHADOW_OFFSET[0] * s, 0, 1, -SHADOW_OFFSET[1] * s))
    shadow = shadow.filter(ImageFilter.GaussianBlur(SHADOW_BLUR * s))
    return np.array(shadow).astype(np.float32) / 255.0 * SHADOW_STRENGTH


def styled_glyph(glyph, box):
    """A white Magic Rampage glyph re-drawn as its pad icons are: fill, outline, shadow."""
    s = SUPERSAMPLE
    glyph = glyph.crop(glyph.getchannel("A").getbbox())
    inner = box - 2 * ICON_OUTLINE
    scale = inner / max(glyph.width, glyph.height)
    gw, gh = max(1, round(glyph.width * scale * s)), max(1, round(glyph.height * scale * s))
    pad = int(math.ceil((ICON_OUTLINE + SHADOW_OFFSET[0] + 3 * SHADOW_BLUR) * s))
    mask = np.zeros((gh + 2 * pad, gw + 2 * pad), np.float32)
    big = glyph.getchannel("A").resize((gw, gh), Image.Resampling.LANCZOS)
    mask[pad:pad + gh, pad:pad + gw] = np.array(big).astype(np.float32) / 255.0
    outline = disc_dilate(mask, ICON_OUTLINE * s)
    shadow = shadow_layer(outline)

    # Painted from the bottom: the shadow, the black outline, the fill; premultiplied.
    rgb = np.zeros(mask.shape + (3,), np.float32)
    alpha = np.zeros(mask.shape, np.float32)

    def over(colour, a):
        nonlocal rgb, alpha
        rgb = np.array(colour, np.float32) / 255.0 * a[:, :, None] + rgb * (1.0 - a[:, :, None])
        alpha = a + alpha * (1.0 - a)

    over((0, 0, 0), shadow)
    over((0, 0, 0), outline)
    over(ICON_FILL, mask)
    premultiplied = np.dstack([rgb, alpha])
    layer = Image.fromarray((np.clip(premultiplied, 0, 1) * 255 + 0.5).astype(np.uint8), "RGBa")
    small = layer.resize((layer.width // s, layer.height // s), Image.Resampling.LANCZOS)
    return small.convert("RGBA")


def with_shadow(image):
    """A coloured gem with the pad icons' shadow under it."""
    s = SUPERSAMPLE
    pad = int(math.ceil(SHADOW_OFFSET[0] + 3 * SHADOW_BLUR))
    canvas = Image.new("RGBA", (image.width + 2 * pad, image.height + 2 * pad), (0, 0, 0, 0))
    canvas.alpha_composite(image, (pad, pad))
    big = canvas.getchannel("A").resize((canvas.width * s, canvas.height * s), Image.Resampling.LANCZOS)
    shadow = shadow_layer(np.array(big).astype(np.float32) / 255.0)
    shadow = Image.fromarray((shadow * 255 + 0.5).astype(np.uint8), "L").resize(canvas.size, Image.Resampling.LANCZOS)
    under = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    under.putalpha(shadow)
    under.alpha_composite(canvas)
    return under


def on_frame(frame, icon, dx=0, dy=0):
    """An icon centred on the blank frame's face (14..114)."""
    out = frame.copy()
    out.alpha_composite(icon, ((out.width - icon.width) // 2 + dx, (out.height - icon.height) // 2 + dy))
    return out


def down_button(frame, right):
    """dpad-right's face mirrored across the main diagonal onto the blank frame: the right arrow
    becomes a down arrow and keeps its shadow to the lower right (a rotation would swing it to the
    lower left). The shadow and the face's vertical gradient go across as a ratio to the frame,
    the arrow's outline and fill as they are, so no patch shows."""
    f = np.array(frame).astype(np.float32)
    b = np.array(right).astype(np.float32)
    face = np.zeros(f.shape[:2], bool)
    face[30:98, 30:98] = True   # the inner face; the rim is not symmetric about the diagonal
    ratio = np.ones_like(f[:, :, :3])
    ratio[face] = b[:, :, :3][face] / np.maximum(f[:, :, :3][face], 1.0)
    core = (b[:, :, :3].max(axis=2) < 60) | ((b[:, :, :3].min(axis=2) > f[:, :, :3].min(axis=2) + 6) & face)
    out = f.copy()
    rt, ft, ct, bt = ratio.transpose(1, 0, 2), face.T, core.T, b.transpose(1, 0, 2)
    out[:, :, :3][ft] = np.clip(f[:, :, :3][ft] * rt[ft], 0, 255)
    out[:, :, :3][ct] = bt[:, :, :3][ct]
    return Image.fromarray((out + 0.5).astype(np.uint8), "RGBA")


def dpad_canvas(button, direction):
    """One direction button alone where its sector is, on a canvas the size of the control."""
    canvas = Image.new("RGBA", (DPAD_CANVAS, DPAD_CANVAS), (0, 0, 0, 0))
    px = DPAD_CANVAS / DPAD_BOX
    cx, cy = DPAD_CENTRES[direction]
    x = round(DPAD_CANVAS / 2 + cx * px - button.width / 2)
    y = round(DPAD_CANVAS / 2 + cy * px - button.height / 2)
    if x < 0 or y < 0 or x + button.width > DPAD_CANVAS or y + button.height > DPAD_CANVAS:
        raise SystemExit(f"dpad_{direction}: the button leaves the control's box")
    canvas.alpha_composite(button, (x, y))
    return canvas


def sector(dx, dy, radius):
    """What TouchControls::Update holds for a thumb at (dx, dy) from the centre (y down)."""
    if dx * dx + dy * dy <= (DEAD_ZONE * radius) ** 2:
        return frozenset()
    held = set()
    across = abs(dx)
    if not across < abs(dy) * TAN22:
        held.add("right" if dx > 0 else "left")
    if dy > 0 and dy >= across * TAN22:
        held.add("down")
    return frozenset(held)


def check_dpad_sectors(frame):
    """Each button's opaque face, pixel by pixel, through the control's sectors."""
    px = DPAD_CANVAS / DPAD_BOX
    cell = frame.width / px
    face = np.array(frame.getchannel("A")) == 255
    ys, xs = np.nonzero(face)
    radius = DPAD_BOX / 2
    report = {}
    for direction, (cx, cy) in DPAD_CENTRES.items():
        # Each face pixel's centre, in logical px from the control's centre.
        lx = cx + (xs + 0.5 - frame.width / 2) / px
        ly = cy + (ys + 0.5 - frame.height / 2) / px
        alone = sum(1 for x, y in zip(lx, ly) if sector(x, y, radius) == frozenset({direction}))
        report[direction] = alone / len(xs)
    print(f"  direction buttons {cell:.1f} logical px; share of each face that holds its direction alone:")
    for direction, share in report.items():
        print(f"    {direction:5s} {share * 100:5.1f}%")
    for direction in ("left", "right"):
        if report[direction] < 1.0:
            raise SystemExit(f"dpad_{direction}: part of its face is not {direction} alone")
    return report


def check_no_magenta(path):
    data = np.array(Image.open(path).convert("RGBA"))
    if ((data[:, :, 0] == 255) & (data[:, :, 1] == 0) & (data[:, :, 2] == 255)).any():
        raise SystemExit(f"{path.name} has exact magenta, which the HUD keys out")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    where = parser.add_mutually_exclusive_group()
    where.add_argument("--xapk", help="Magic Rampage 7.8.7's .xapk (read only)")
    where.add_argument("--assets", help="a folder holding the base APK's assets/ contents")
    parser.add_argument("--out", default=str(OUT), help="where to write (default game/data/images/touch)")
    args = parser.parse_args()
    if not args.xapk and not args.assets:
        parser.error("Magic Rampage 7.8.7's package is needed: --xapk <its .xapk> or --assets <a folder of "
                     "its base APK's assets/>.\nThe PNGs this writes are committed in game/data/images/touch/: "
                     "building and playing the game never need this script.")
    out = pathlib.Path(args.out).resolve()
    mr = Source(xapk=args.xapk, assets=args.assets)

    frame = mr.image("sprites/dpad-frame.png")
    right = mr.image("sprites/dpad-right.png")
    images = {
        "jump.png": mr.image("sprites/jump-pad-button.png"),
        "sword.png": mr.image("sprites/attack-pad-button.png"),
        "fire.png": on_frame(frame, styled_glyph(mr.image("sprites/elements/element-fire.png"), 60)),
        "light.png": on_frame(frame, styled_glyph(mr.image("sprites/elements/element-light.png"), 62)),
        "combo_sword.png": on_frame(frame, with_shadow(fit(mr.image("sprites/button_rune_arcane_off.png"), 68)), 1, 1),
        "combo_spell.png": on_frame(frame, with_shadow(fit(mr.image("entities/rune_fire.png"), 64)), 1, 1),
        "pause.png": mr.image("sprites/pause-pad-button.png").crop((0, 42, 128, 128)),
        "back.png": mr.image("sprites/inventory-back-button.png"),
        "dpad.png": Image.new("RGBA", (DPAD_CANVAS, DPAD_CANVAS), (0, 0, 0, 0)),
        "dpad_left.png": dpad_canvas(mr.image("sprites/dpad-left.png"), "left"),
        "dpad_right.png": dpad_canvas(right, "right"),
        "dpad_down.png": dpad_canvas(down_button(frame, right), "down"),
        "dpad_knob.png": mr.image("SEF/media/button-highlight.png"),
    }
    check_dpad_sectors(frame)

    print(f"writing {out}")
    out.mkdir(parents=True, exist_ok=True)
    for name, image in images.items():
        path = out / name
        straight_rgba(image).save(path, optimize=True)
        check_no_magenta(path)
        print(f"  {name}  {image.width}x{image.height}")


if __name__ == "__main__":
    sys.exit(main())
