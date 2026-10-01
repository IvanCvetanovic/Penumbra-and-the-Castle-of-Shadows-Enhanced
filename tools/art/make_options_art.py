"""The options screen's and the main menu's language button's art (ENHANCEMENT E27), from Magic Rampage.

    python tools/art/make_options_art.py --xapk "<path>/Magic+Rampage_7.8.7_APKPure.xapk"
    python tools/art/make_options_art.py --assets <a folder of the base APK's assets/>  [--sheet sheet.png]

Writes game/data/images/options/ (--out): the stone-frame panel the screens' groups of rows sit on
(nine-sliced by game/script/optionsArt.cpp), the check boxes, the arrow, minus and plus buttons, the
speaker, music and gamepad icons, a globe and a monitor, and a globe button for the main menu.
images/options/README.md says which Magic Rampage file each one is and what was composed or drawn.

It reads the package and writes nothing into it. It uses the touch art's helpers (make_mr_touch_art.py:
the package reader, the premultiplied resampling, the soft shadow the pad icons have, the styled glyph
and the blank pad frame), so the icons look like the touch buttons.
"""

import argparse
import math
import pathlib
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import make_mr_touch_art as T  # noqa: E402

OUT = T.REPO / "game" / "data" / "images" / "options"

# Magic Rampage's stone frames come with a wide soft shadow past their body, cut off by the image's edge: a
# visible halo box when the image is cropped. body() takes the shadow off (pixels dark and not mostly opaque)
# and crops to the stone, so a panel's rectangle is the visible frame. Every frame's stone is 12 px thick at
# its native size: 91x90 for sprites/smaller-purple-frame-bg.png, 102x102 for check-icon.png, 100x100 for the
# inventory arrow buttons, 93x93 for smaller-frame-bg.png.
SHADOW_ALPHA = 150
SHADOW_DARK = 30
# The nine-slice width, in px of panel.png: each corner piece holds the whole rounded stone corner and the
# inner shade, so the edge strips between them are the same along their length (checked below).
PANEL_SLICE = 26
ICON = 64                               # the icons' canvas
S = T.SUPERSAMPLE


class Pack:
    """Magic Rampage's images, cropped as needed."""

    def __init__(self, source):
        self.source = source

    def image(self, rel):
        return self.source.image(rel)

    def body(self, rel):
        """The image without its shadow, cropped to what is left: the frame's body."""
        a = np.array(self.image(rel))
        shadow = (a[:, :, 3] < SHADOW_ALPHA) & (a[:, :, :3].max(axis=2) < SHADOW_DARK)
        a[shadow] = 0
        image = Image.fromarray(a, "RGBA")
        return image.crop(image.getchannel("A").getbbox())


def to_icon(image, size=ICON):
    return T.resize(image, (size, size))


def nine_slice_resize(image, slice_px, size):
    """image drawn at `size` with its corners kept and its edges and centre stretched (premultiplied)."""
    w, h = image.size
    tw, th = size
    out = Image.new("RGBA", size, (0, 0, 0, 0))
    xs = [0, slice_px, w - slice_px, w]
    ys = [0, slice_px, h - slice_px, h]
    txs = [0, slice_px, tw - slice_px, tw]
    tys = [0, slice_px, th - slice_px, th]
    for j in range(3):
        for i in range(3):
            piece = image.crop((xs[i], ys[j], xs[i + 1], ys[j + 1]))
            dw, dh = txs[i + 1] - txs[i], tys[j + 1] - tys[j]
            if (dw, dh) != piece.size:
                piece = T.resize(piece, (dw, dh))
            out.alpha_composite(piece, (txs[i], tys[j]))
    return out


def check_panel_slice(panel):
    """The top strip between the corner pieces is the same along its length: the slice is wide enough."""
    a = np.array(panel).astype(int)
    w = a.shape[1]
    top = a[:PANEL_SLICE, PANEL_SLICE:w - PANEL_SLICE]
    along = np.abs(top - top[:, :1]).max()
    left = np.array(panel).astype(int)[PANEL_SLICE:w - PANEL_SLICE, :12]   # the stone only, not the shaded inside
    stone = np.abs(left - left[:1]).max()
    if along > 4 or stone > 4:
        raise SystemExit(f"panel.png: a {PANEL_SLICE} px slice is too narrow (edge varies by {along}/{stone})")
    return along, stone


def paint(layers, size):
    """Layers of (colour, alpha mask 0..1 at S x) painted bottom up, premultiplied, reduced to `size`."""
    shape = layers[0][1].shape
    rgb = np.zeros(shape + (3,), np.float32)
    alpha = np.zeros(shape, np.float32)
    for colour, mask in layers:
        m = mask[:, :, None]
        rgb = np.array(colour, np.float32) / 255.0 * m + rgb * (1.0 - m)
        alpha = mask + alpha * (1.0 - mask)
    layer = Image.fromarray((np.clip(np.dstack([rgb, alpha]), 0, 1) * 255 + 0.5).astype(np.uint8), "RGBa")
    return layer.resize(size, Image.Resampling.LANCZOS).convert("RGBA")


def soft_shadow(mask, offset=(1.5, 2.0), blur=2.0, strength=0.55):
    """Magic Rampage's soft shadow to the lower right, lighter than the pad icons' for a small icon."""
    from PIL import ImageFilter
    img = Image.fromarray((mask * 255).astype(np.uint8), "L")
    img = img.transform(img.size, Image.Transform.AFFINE, (1, 0, -offset[0] * S, 0, 1, -offset[1] * S))
    img = img.filter(ImageFilter.GaussianBlur(blur * S))
    return np.array(img).astype(np.float32) / 255.0 * strength


WHITE = (250, 250, 250)


def white_icon(mask):
    """A white glyph with the soft shadow, on a transparent ICON x ICON canvas."""
    return paint([((0, 0, 0), soft_shadow(mask)), (WHITE, mask)], (ICON, ICON))


def canvas():
    return Image.new("L", (ICON * S, ICON * S), 0)


def to_mask(image):
    return np.array(image).astype(np.float32) / 255.0


def globe_mask():
    """A disc with a meridian ellipse, the central meridian, the equator and two latitudes cut out of it
    (the rim stays whole): the usual globe, bold enough to read at 28 px."""
    im = canvas()
    d = ImageDraw.Draw(im)
    c, r = 31.5 * S, 27.0 * S
    d.ellipse((c - r, c - r, c + r, c + r), fill=255)
    cut = Image.new("L", im.size, 0)
    cd = ImageDraw.Draw(cut)
    lw = round(3.3 * S)
    rx = r * 0.50
    cd.ellipse((c - rx, c - r, c + rx, c + r), outline=255, width=lw)
    cd.line((c, c - r, c, c + r), fill=255, width=lw)
    for dy in (-0.53 * r, 0.0, 0.53 * r):
        cd.line((c - r, c + dy, c + r, c + dy), fill=255, width=lw)
    # Only inside the rim: the cuts stop 3.3 px short of the disc's edge.
    inside = Image.new("L", im.size, 0)
    ImageDraw.Draw(inside).ellipse((c - r + lw, c - r + lw, c + r - lw, c + r - lw), fill=255)
    cut = Image.fromarray(np.minimum(np.array(cut), np.array(inside)), "L")
    return to_mask(im) * (1.0 - to_mask(cut))


def monitor_mask():
    """A rounded screen in a bezel on a short stand and a base."""
    im = canvas()
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((5 * S, 9 * S, 58 * S, 42 * S), radius=int(6 * S), fill=255)
    d.rounded_rectangle((10 * S, 14 * S, 53 * S, 37 * S), radius=int(2.5 * S), fill=0)
    d.rectangle((28 * S, 41 * S, 35 * S, 50 * S), fill=255)
    d.rounded_rectangle((19 * S, 49 * S, 44 * S, 55 * S), radius=int(2.5 * S), fill=255)
    return to_mask(im)


def minus_plus_mask(plus):
    im = canvas()
    d = ImageDraw.Draw(im)
    half, thick = 15.0, 3.6
    c = 32.0
    d.rounded_rectangle(((c - half) * S, (c - thick) * S, (c + half) * S, (c + thick) * S), radius=int(2.4 * S), fill=255)
    if plus:
        d.rounded_rectangle(((c - thick) * S, (c - half) * S, (c + thick) * S, (c + half) * S), radius=int(2.4 * S), fill=255)
    return to_mask(im)


def glyph_button(frame, mask):
    """A white glyph with its soft shadow on a stone frame already at ICON x ICON."""
    out = frame.copy()
    out.alpha_composite(paint([((0, 0, 0), soft_shadow(mask, (1.2, 1.6), 1.6, 0.6)), (WHITE, mask)], (ICON, ICON)))
    return out


def globe_alpha_glyph():
    """The globe as the styled-glyph helper wants it: a white shape with an alpha channel (its cuts clear)."""
    mask = globe_mask()
    glyph = Image.new("RGBA", (ICON * S, ICON * S), (255, 255, 255, 0))
    glyph.putalpha(Image.fromarray((mask * 255 + 0.5).astype(np.uint8), "L"))
    return glyph.resize((ICON * 2, ICON * 2), Image.Resampling.LANCZOS)


def make_images(pack):
    images = {}
    panel = pack.body("sprites/smaller-purple-frame-bg.png")
    images["panel.png"] = panel
    check_on = pack.body("sprites/check-icon.png")
    # The empty box is the panel's frame stretched to the check box's size, so the two have the same stone.
    check_off = nine_slice_resize(panel, PANEL_SLICE, check_on.size)
    images["check_on.png"] = to_icon(check_on)
    images["check_off.png"] = to_icon(check_off)
    images["arrow_left.png"] = to_icon(pack.body("sprites/inventory-back-button.png"))
    images["arrow_right.png"] = to_icon(pack.body("sprites/inventory-next-button.png"))
    dark = to_icon(pack.body("sprites/smaller-frame-bg.png"))
    images["minus.png"] = glyph_button(dark, minus_plus_mask(False))
    images["plus.png"] = glyph_button(dark, minus_plus_mask(True))
    images["speaker.png"] = pack.image("sprites/audio-toggle-on.png")
    images["music.png"] = pack.image("sprites/music-toggle-on.png")
    pad = pack.image("sprites/fighter-controller.png")
    pad = pad.crop(pad.getchannel("A").getbbox())
    images["pad.png"] = T.resize(pad, (96, round(96 * pad.height / pad.width)))
    images["globe.png"] = white_icon(globe_mask())
    images["monitor.png"] = white_icon(monitor_mask())
    # The main menu's globe button: the touch buttons' blank pad frame with the globe styled as their icons are.
    frame = pack.image("sprites/dpad-frame.png")
    button = T.on_frame(frame, T.styled_glyph(globe_alpha_glyph(), 62))
    images["globe_button.png"] = T.resize(button, (96, 96))
    return images


def contact_sheet(images, path):
    """Every image at 3x on the menu's purple, for looking at."""
    names = sorted(images)
    cell = 3 * 100
    cols = 4
    rows = -(-len(names) // cols)
    sheet = Image.new("RGBA", (cols * cell, rows * (cell + 24)), (70, 40, 106, 255))
    d = ImageDraw.Draw(sheet)
    for i, name in enumerate(names):
        im = images[name]
        big = im.resize((im.width * 3, im.height * 3), Image.Resampling.NEAREST if im.width < 40 else Image.Resampling.LANCZOS)
        x, y = (i % cols) * cell, (i // cols) * (cell + 24)
        sheet.alpha_composite(big, (x + (cell - big.width) // 2, y + 4))
        d.text((x + 4, y + cell + 6), f"{name} {im.width}x{im.height}", fill=(255, 255, 255, 255))
    sheet.convert("RGB").save(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    where = parser.add_mutually_exclusive_group()
    where.add_argument("--xapk", help="Magic Rampage 7.8.7's .xapk (read only)")
    where.add_argument("--assets", help="a folder holding the base APK's assets/ contents")
    parser.add_argument("--out", default=str(OUT), help="where to write (default game/data/images/options)")
    parser.add_argument("--sheet", help="also write a contact sheet of the images (not committed) to this PNG")
    args = parser.parse_args()
    if not args.xapk and not args.assets:
        parser.error("Magic Rampage 7.8.7's package is needed: --xapk <its .xapk> or --assets <a folder of "
                     "its base APK's assets/>.\nThe PNGs this writes are committed in game/data/images/options/: "
                     "building and playing the game never need this script.")
    out = pathlib.Path(args.out).resolve()
    pack = Pack(T.Source(xapk=args.xapk, assets=args.assets))
    images = make_images(pack)
    along, stone = check_panel_slice(images["panel.png"])
    print(f"panel.png {images['panel.png'].width}x{images['panel.png'].height}: slice {PANEL_SLICE} px "
          f"(its edge varies by {along} along a strip, {stone} in the stone)")

    print(f"writing {out}")
    out.mkdir(parents=True, exist_ok=True)
    for name, image in images.items():
        path = out / name
        T.straight_rgba(image).save(path, optimize=True)
        T.check_no_magenta(path)
        print(f"  {name}  {image.width}x{image.height}")
    if args.sheet:
        contact_sheet(images, args.sheet)
        print(f"contact sheet: {args.sheet}")


if __name__ == "__main__":
    sys.exit(main())
