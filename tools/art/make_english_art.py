#!/usr/bin/env python3
"""English variants of the original's images that have Portuguese baked into their pixels (E5).

Reads the originals from extracted/app (never writes there) and writes each variant to
game/data/images/en/<the same relative path>, the same size, mode and layout as the original, so
every rectangle the scripts and the scenes compute (sprite cuts, hit boxes, DrawSprite rectangles)
still fits it. game/data/strings.json "images" names them; render/Localization::ImageVariant hands
them to the renderers when the language is English.

    python tools/art/make_english_art.py            # write the variants
    python tools/art/make_english_art.py --verify   # measure the models against the originals

Needs Pillow and numpy, and three stock Windows fonts in %WINDIR%\\Fonts: Courier New (cour.ttf),
Courier New Bold (courbd.ttf) and Matura MT Script Capitals (MATURASC.TTF, with Office). The
output is a raster image of their glyphs, our own derived art.

WHICH IMAGES CARRY PORTUGUESE (every image under entities/ and interface/ looked at):
  entities/menu_buttons.png         the menu's seven labels, one per 512x64 frame (buttons.ent,
                                    SpriteCut 1x8), with its normal map
                                    entities/normalmaps/menu_nm_buttons.png and gloss map
                                    entities/menu_buttons_gloss.png carrying the same letters
  entities/gamelogo.png             "e o Castelo das Sombras" under "Penumbra"
  interface/input_options1.png      "Jogador 1" / "Jogador 2" under the keyboard and the pad
  interface/input_options2.png      the same under two pads
  interface/arrow_button.png        "Voltar" on the video options screen's back arrow
Already English or wordless: gameover.png ("Game Over"), Lapide_Bitmap*.png ("CHECK POINT"),
tombstone.png (an illegible smudge), joystick.png ("SELECT"/"START"/"ANALOG"), thumbnails.png,
and every other texture, normal map and interface image.

Every number below was fitted by least squares against the original's pixels (scipy, once; the
script itself needs none), and --verify re-measures them.

THE MENU LABELS. The three maps are one glyph mask G drawn three ways. The colour is black
everywhere (the label is all alpha), the gloss map IS G (L mode), and
      alpha  = max(G, 1 - exp(-1.8989 * gauss(G, 2.2515)))                    a soft dark glow
      normal = floor(127.5 * (normalize(-3.0279 * grad gauss(G, 2.3915), 1) + 1))
with grad the central difference and gauss a zero-padded Gaussian over the WHOLE sheet (the glow
of a descender bleeds into the next frame in the original too). Fed the original gloss, these
reproduce the original alpha within 0.72/255 and the normal map within 0.12/255 on average - the
normal map is an emboss of the blurred label and the glow a blur of it. G is Courier New
(regular) with a 1.5 px outline stroke, 54.9 px to the em across (the 32.9 px pitch every label
has), stretched 1.34x above the baseline and 0.6x below (the original's letters are taller and
its descenders shorter than the font's); each frame keeps the pen origin and baseline fitted to
its Portuguese label, so each English label starts where the Portuguese one did. (Mean absolute
error over the seven labels 0.049; Courier New Bold 0.054, Consolas Bold and Lucida Console worse.)

THE "JOGADOR" LABELS. Lavender (203,203,228) Courier New Bold, 13.37 px to the em across,
stretched 1.22x above the baseline and 0.74x below, over a black drop shadow of 0.78 opacity,
offset (1.3, 1.25) px and blurred by a 0.55 px Gaussian. The old label is erased first: below the
pictures' glow it is transparent; in the glow rows its pixels are refilled from the glow on
either side. "Player N" is centred where "Jogador N" was, on the same baseline.

THE LOGO AND THE ARROW. Their lettering is a hand-cut uncial with no installed match. Matura MT
Script Capitals is the nearest in weight and spirit (heavy broad-pen strokes, swash C and S), at
the original subtitle's x-height (size 36: 16 px) and span. The logo keeps every pixel of
"Penumbra" outside the subtitle's reach (15 px, the glow's extent); inside it the letters are
black and the white glow is c * (1 - exp(-k * gauss(letters, s))) with s 4.3555, k 2.6089,
c 0.9104 (the logo's glow fitted the same way, 9.6/255 mean error: the original's glow is
hand-made), blended into the original over a 2.5 px feather. The arrow's body is flat opaque
white, so "Voltar" is erased by painting its pixels white and "Back" is drawn black on it.
Both are a judgement call about a different typeface; strings.json can drop either by emptying
its "en".
"""

import argparse
import os
import pathlib
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

REPO = pathlib.Path(__file__).resolve().parents[2]
ORIGINAL = REPO / "extracted" / "app"
OUT = REPO / "game" / "data" / "images" / "en"
FONTS = pathlib.Path(os.environ.get("WINDIR", r"C:\Windows")) / "Fonts"

# The fits were made at these; the stroke is whole pixels at RENDER_EM, so they are part of the look.
RENDER_EM = 400
SUPERSAMPLE = 2


def font_path(name):
    path = FONTS / name
    if not path.is_file():
        sys.exit(f"make_english_art: {path} is missing (a stock Windows font)")
    return str(path)


def gaussian(image, sigma, truncate=4.0):
    """scipy.ndimage.gaussian_filter(image, sigma, mode='constant') without scipy: separable, the
    kernel cut at truncate * sigma and normalised, zero outside the image."""
    radius = int(truncate * sigma + 0.5)
    x = np.arange(-radius, radius + 1, dtype=np.float64)
    kernel = np.exp(-0.5 * (x / sigma) ** 2)
    kernel /= kernel.sum()
    out = image.astype(np.float64)
    for axis in (0, 1):
        padded = np.pad(out, [(radius, radius) if a == axis else (0, 0) for a in (0, 1)])
        acc = np.zeros_like(out)
        size = out.shape[axis]
        for i, w in enumerate(kernel):
            acc += w * (padded[i:i + size, :] if axis == 0 else padded[:, i:i + size])
        out = acc
    return out


def dilate(mask, radius):
    """Binary dilation by a (2 radius + 1) square, numpy only."""
    out = mask.copy()
    for _ in range(radius):
        grown = out.copy()
        grown[1:, :] |= out[:-1, :]
        grown[:-1, :] |= out[1:, :]
        grown[:, 1:] |= out[:, :-1]
        grown[:, :-1] |= out[:, 1:]
        out = grown
    return out


def components(mask):
    """4-connected components of a boolean mask: a list of (ys, xs) index arrays."""
    seen = np.zeros_like(mask, bool)
    height, width = mask.shape
    found = []
    for y0, x0 in zip(*np.nonzero(mask)):
        if seen[y0, x0]:
            continue
        stack = [(y0, x0)]
        seen[y0, x0] = True
        ys, xs = [], []
        while stack:
            y, x = stack.pop()
            ys.append(y)
            xs.append(x)
            for ny, nx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
                if 0 <= ny < height and 0 <= nx < width and mask[ny, nx] and not seen[ny, nx]:
                    seen[ny, nx] = True
                    stack.append((ny, nx))
        found.append((np.array(ys), np.array(xs)))
    return found


class LabelRenderer:
    """One font, drawn large once per text, then mapped onto the target with a float affine (so a
    pen origin or baseline can sit between pixels) and box-filtered from a supersampled grid. The
    part above the baseline and the part below are stretched separately."""

    def __init__(self, font_file, em, above, below, stroke):
        self.font = ImageFont.truetype(font_path(font_file), RENDER_EM)
        self.em, self.above, self.below, self.stroke = em, above, below, stroke
        self.cache = {}

    def _large(self, text):
        if text not in self.cache:
            stroke = int(round(self.stroke * RENDER_EM / self.em))
            width = int(self.font.getlength(text)) + RENDER_EM
            image = Image.new("L", (width, RENDER_EM * 2), 0)
            origin, base = RENDER_EM // 2, int(RENDER_EM * 1.3)
            ImageDraw.Draw(image).text((origin, base), text, font=self.font, fill=255, anchor="ls",
                                       stroke_width=stroke, stroke_fill=255)
            self.cache[text] = (image, origin, base)
        return self.cache[text]

    def rows(self, text, x0, baseline, y0, y1, width):
        """The label's coverage (0..1) on target rows y0..y1-1, pen at (x0, baseline)."""
        image, origin, base = self._large(text)
        s = self.em / RENDER_EM
        w, h = width * SUPERSAMPLE, (y1 - y0) * SUPERSAMPLE
        out = np.zeros((h, w))
        v = ((np.arange(h) + 0.5) / SUPERSAMPLE + y0)[:, None]
        for k, above in ((self.above, True), (self.below, False)):
            a = 1.0 / (SUPERSAMPLE * s)
            c = origin - x0 / s
            e = 1.0 / (SUPERSAMPLE * s * k)
            f = base + (y0 - baseline) / (s * k)
            part = np.asarray(image.transform((w, h), Image.Transform.AFFINE, (a, 0, c, 0, e, f),
                                              resample=Image.BILINEAR), dtype=np.float64) / 255.0
            out += part * ((v < baseline) if above else (v >= baseline))
        return out.reshape(y1 - y0, SUPERSAMPLE, width, SUPERSAMPLE).mean(axis=(1, 3))

    def ink_span(self, text):
        """The label's inked columns relative to the pen, at the target scale."""
        mask = self.rows(text, 0.0, 0.0, -int(self.em * 2), int(self.em), int(self.font.getlength(text)
                                                                                * self.em / RENDER_EM) + 8)
        columns = np.nonzero(mask.max(axis=0) > 0.3)[0]
        return float(columns.min()), float(columns.max() + 1)


def plain_text(font_file, size, text, x, baseline, width, height, scale=4):
    """Unstretched text coverage (0..1), drawn `scale`x and box-filtered, pen at (x, baseline)."""
    font = ImageFont.truetype(font_path(font_file), size * scale)
    image = Image.new("L", (width * scale, height * scale), 0)
    ImageDraw.Draw(image).text((x * scale, baseline * scale), text, font=font, fill=255, anchor="ls")
    return np.asarray(image.resize((width, height), Image.BOX), np.float64) / 255.0


def plain_width(font_file, size, text):
    """The inked extent (left, right) of unstretched text relative to its pen, in pixels."""
    box = ImageFont.truetype(font_path(font_file), size * 4).getbbox(text, anchor="ls")
    return box[0] / 4.0, box[2] / 4.0


def to_byte(values):
    return np.clip(np.round(values * 255.0), 0, 255).astype(np.uint8)


def load(relative, mode=None):
    image = Image.open(ORIGINAL / relative)
    return image.convert(mode) if mode else image


def rgba(relative):
    return np.asarray(load(relative, "RGBA"), np.float64) / 255.0


def from_premultiplied(premultiplied):
    alpha = premultiplied[..., 3:4]
    colour = np.where(alpha > 0, premultiplied[..., :3] / np.maximum(alpha, 1e-9), 0.0)
    return Image.fromarray(to_byte(np.concatenate([colour, alpha], -1)), "RGBA")


def over(top_colour, top_alpha, under):
    """Premultiplied `top` (a flat colour at coverage top_alpha) over premultiplied `under`."""
    a = top_alpha[..., None]
    return np.concatenate([np.asarray(top_colour, np.float64) * a, a], -1) + under * (1.0 - a)


def mean_abs(a, b):
    return float(np.abs(np.asarray(a, np.float64) - np.asarray(b, np.float64)).mean())


def max_abs(a, b):
    return float(np.abs(np.asarray(a, np.float64) - np.asarray(b, np.float64)).max())


# ---- the menu labels --------------------------------------------------------------------------

MENU_SHEET = "entities/menu_buttons.png"
MENU_GLOSS = "entities/menu_buttons_gloss.png"
MENU_NORMAL = "entities/normalmaps/menu_nm_buttons.png"

MENU_FONT = "cour.ttf"
MENU_EM = 54.9          # px to the em across: Courier's advance is 0.6 em, so a 32.94 px pitch
MENU_ABOVE = 1.34       # vertical stretch above the baseline, relative to MENU_EM
MENU_BELOW = 0.6        # and below it
MENU_STROKE = 1.5       # outline stroke, px at the label's size

# Frame, Portuguese, English (strings.json "labels"), the fitted pen origin x and baseline y.
MENU_FRAMES = [
    (0, "Novo jogo", "New Game", 8.610, 50.618),
    (1, "Cr\u00e9ditos", "Credits", 9.803, 117.663),
    (2, "Melhores tempos", "Best Times", 9.962, 179.795),
    (3, "Sair", "Quit", 9.203, 243.447),
    (4, "Configura\u00e7\u00f5es", "Settings", 4.179, 304.674),
    (5, "Como jogar", "How to Play", 7.468, 371.664),
    (6, "Versus", "Versus", 9.283, 437.616),
]
MENU_FRAME_HEIGHT = 64

GLOW_SIGMA = 2.2515
GLOW_GAIN = 1.8989
NORMAL_SIGMA = 2.3915
NORMAL_GAIN = 3.0279


def menu_mask(portuguese=False):
    renderer = LabelRenderer(MENU_FONT, MENU_EM, MENU_ABOVE, MENU_BELOW, MENU_STROKE)
    width, height = 512, 512
    mask = np.zeros((height, width))
    for _, pt, en, x0, baseline in MENU_FRAMES:
        y0 = max(0, int(baseline) - MENU_FRAME_HEIGHT)
        y1 = min(height, int(baseline) + 24)
        rows = renderer.rows(pt if portuguese else en, x0, baseline, y0, y1, width)
        mask[y0:y1] = np.maximum(mask[y0:y1], rows)
    return np.clip(mask, 0.0, 1.0)


def glow_alpha(mask):
    return np.maximum(mask, 1.0 - np.exp(-GLOW_GAIN * gaussian(mask, GLOW_SIGMA)))


def emboss_normal(mask):
    height = gaussian(mask, NORMAL_SIGMA)
    gx = np.zeros_like(height)
    gy = np.zeros_like(height)
    gx[:, 1:-1] = (height[:, 2:] - height[:, :-2]) / 2
    gy[1:-1, :] = (height[2:, :] - height[:-2, :]) / 2
    n = np.stack([-NORMAL_GAIN * gx, -NORMAL_GAIN * gy, np.ones_like(height)], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return n


def encode_normal(n):
    # floor, as the original's (127,127,255) flat texels say: 127.5 floored.
    return np.clip(np.floor((n + 1.0) * 127.5), 0, 255).astype(np.uint8)


def menu_images(mask):
    sheet = np.zeros(mask.shape + (4,), np.uint8)
    sheet[..., 3] = to_byte(glow_alpha(mask))
    return {
        MENU_SHEET: Image.fromarray(sheet, "RGBA"),
        MENU_GLOSS: Image.fromarray(to_byte(mask), "L"),
        MENU_NORMAL: Image.fromarray(encode_normal(emboss_normal(mask)), "RGB"),
    }


# ---- "Jogador 1" / "Jogador 2" ------------------------------------------------------------------

PLAYER_FONT = "courbd.ttf"
PLAYER_EM = 13.37
PLAYER_ABOVE = 1.22
PLAYER_BELOW = 0.74
PLAYER_COLOUR = (203 / 255, 203 / 255, 228 / 255)
SHADOW_OPACITY = 0.78
SHADOW_OFFSET = (1.3, 1.25)
SHADOW_SIGMA = 0.55
LABEL_TOP = 57          # the first row a label touches; the pictures' glow ends by row 59
GLOW_END = 59           # rows from here down hold nothing but the label and its shadow

# Image, the original's text, the English, the fitted baseline. The columns are found in the image.
PLAYER_LABELS = [
    ("interface/input_options1.png", ["Jogador 1", "Jogador 2"], ["Player 1", "Player 2"], [67.60, 68.26]),
    ("interface/input_options2.png", ["Jogador 1", "Jogador 2"], ["Player 1", "Player 2"], [67.56, 67.27]),
]


def label_columns(image):
    """The column spans of the labels: runs of non-transparent columns below the glow, merged
    across the gap a space leaves."""
    used = (image[GLOW_END + 1:, :, 3] > 0).any(axis=0)
    spans = []
    for x in np.nonzero(used)[0]:
        if spans and x - spans[-1][1] <= 10:
            spans[-1][1] = x
        else:
            spans.append([x, x])
    return [(int(a), int(b) + 1) for a, b in spans]


def text_centre(image, x0, x1):
    """The centre of the lavender text between columns x0..x1 (its shadow excluded)."""
    premultiplied_blue_minus_red = (image[..., 2] - image[..., 0]) * image[..., 3]
    lavender = premultiplied_blue_minus_red[GLOW_END + 1:, x0:x1] > 12 / 255
    columns = np.nonzero(lavender.any(axis=0))[0]
    return x0 + (columns.min() + columns.max() + 1) / 2.0


def erase_labels(image, spans):
    """The image with the labels gone, premultiplied."""
    out = image.copy()
    out[..., :3] *= out[..., 3:4]
    for x0, x1 in spans:
        a, b = max(0, x0 - 1), min(image.shape[1], x1 + 1)
        out[GLOW_END:, a:b] = 0.0
        for y in range(LABEL_TOP, GLOW_END):
            row = image[y]
            # The glow is white; anything else in these rows is the label's top or its shadow.
            glow = (row[:, :3].min(axis=1) >= 0xF0 / 255) | (row[:, 3] == 0)
            keep = np.nonzero(glow)[0]
            for x in range(a, b):
                if glow[x]:
                    continue
                left = keep[keep < x]
                right = keep[keep > x]
                if len(left) == 0 or len(right) == 0:
                    out[y, x] = 0.0
                    continue
                l, r = left.max(), right.min()
                t = (x - l) / (r - l)
                out[y, x] = out[y, l] * (1 - t) + out[y, r] * t
    return out


def draw_player_label(background, renderer, text, centre, baseline):
    width = background.shape[1]
    y0, y1 = LABEL_TOP - 4, background.shape[0]
    left, right = renderer.ink_span(text)
    pen = centre - (left + right) / 2.0
    text_mask = np.zeros(background.shape[:2])
    shadow = np.zeros(background.shape[:2])
    text_mask[y0:y1] = renderer.rows(text, pen, baseline, y0, y1, width)
    shadow[y0:y1] = renderer.rows(text, pen + SHADOW_OFFSET[0], baseline + SHADOW_OFFSET[1], y0, y1, width)
    shadow = SHADOW_OPACITY * gaussian(shadow, SHADOW_SIGMA)
    return over(PLAYER_COLOUR, text_mask, over((0.0, 0.0, 0.0), shadow, background))


def player_image(relative, portuguese=False):
    image = rgba(relative)
    spans = label_columns(image)
    _, pt, en, baselines = next(entry for entry in PLAYER_LABELS if entry[0] == relative)
    if len(spans) != len(pt):
        sys.exit(f"make_english_art: {relative}: found {len(spans)} labels, expected {len(pt)}")
    renderer = LabelRenderer(PLAYER_FONT, PLAYER_EM, PLAYER_ABOVE, PLAYER_BELOW, 0.0)
    out = erase_labels(image, spans)
    for (x0, x1), old, new, baseline in zip(spans, pt, en, baselines):
        out = draw_player_label(out, renderer, old if portuguese else new, text_centre(image, x0, x1), baseline)
    return from_premultiplied(out), spans


# ---- the back arrow -------------------------------------------------------------------------------

ARROW = "interface/arrow_button.png"
ARROW_TEXT = (28, 61, 16, 109)      # rows y0..y1, columns x0..x1 around "Voltar"; its opaque part
                                    # is flat white body and the word, nothing else
SCRIPT_FONT = "MATURASC.TTF"
ARROW_SIZE = 28                     # x-height 12 px, "Voltar"'s
ARROW_BASELINE = 50


def arrow_image():
    image = rgba(ARROW)
    y0, y1, x0, x1 = ARROW_TEXT
    region = np.zeros(image.shape[:2], bool)
    region[y0:y1, x0:x1] = True
    body = region & (image[..., 3] >= 1.0)
    ink = body & (image[..., 0] < 0.98)
    out = image.copy()
    out[body, :3] = 1.0
    height, width = image.shape[:2]
    left, right = plain_width(SCRIPT_FONT, ARROW_SIZE, "Back")
    columns = np.nonzero(ink.any(axis=0))[0]
    centre = (columns.min() + columns.max() + 1) / 2.0
    coverage = plain_text(SCRIPT_FONT, ARROW_SIZE, "Back", centre - (left + right) / 2.0, ARROW_BASELINE,
                          width, height)
    if ((coverage > 0) & ~body).any():
        sys.exit("make_english_art: \"Back\" does not fit inside the arrow's white body")
    out[..., :3] *= (1.0 - coverage)[..., None]
    return Image.fromarray(to_byte(out), "RGBA")


# ---- the logo ---------------------------------------------------------------------------------------

LOGO = "entities/gamelogo.png"
LOGO_SUBTITLE = "and the Castle of Shadows"
LOGO_SIZE = 36                      # x-height 16 px, the original subtitle's
LOGO_BASELINE = 125
LOGO_SPAN = (106, 544)              # the original subtitle's inked columns
LOGO_REACH = 15                     # how far the glow reaches from a letter
LOGO_FEATHER = 2.5
LOGO_GLOW = (4.3555, 2.6089, 0.9104)


def logo_layers(image):
    """Letter coverage and white glow: the logo is black letters over a white glow, so in
    premultiplied terms letters = A (1 - R) and glow = R A / (1 - letters)."""
    alpha, red = image[..., 3], image[..., 0]
    letters = alpha * (1.0 - red)
    glow = np.where(letters < 0.999, red * alpha / np.maximum(1.0 - letters, 1e-9), 0.0)
    return letters, glow


def logo_glow(letters):
    s, k, c = LOGO_GLOW
    return c * (1.0 - np.exp(-k * gaussian(letters, s)))


def logo_image():
    image = rgba(LOGO)
    letters, glow = logo_layers(image)
    height, width = letters.shape
    # The subtitle is every letter shape that starts below "Penumbra" and right of its P's tail.
    old = np.zeros_like(letters, bool)
    for ys, xs in components(letters > 0.02):
        if ys.min() >= 94 and xs.min() >= 60:
            old[ys, xs] = True
    left, right = plain_width(SCRIPT_FONT, LOGO_SIZE, LOGO_SUBTITLE)
    pen = (LOGO_SPAN[0] + LOGO_SPAN[1]) / 2.0 - (left + right) / 2.0
    new = plain_text(SCRIPT_FONT, LOGO_SIZE, LOGO_SUBTITLE, pen, LOGO_BASELINE, width, height)
    kept = np.where(old, 0.0, letters)
    final = np.maximum(kept, new)
    zone = dilate(old | (new > 0.02), LOGO_REACH).astype(np.float64)
    weight = np.clip(gaussian(zone, LOGO_FEATHER), 0.0, 1.0)
    # Full weight wherever the old subtitle or its glow was, so none of it survives.
    weight = np.maximum(weight, dilate(old, LOGO_REACH - 3))
    glow = glow * (1.0 - weight) + logo_glow(final) * weight
    alpha = final + (1.0 - final) * glow
    colour = np.where(alpha > 0, (1.0 - final) * glow / np.maximum(alpha, 1e-9), 0.0)
    out = np.dstack([colour, colour, colour, alpha])
    return Image.fromarray(to_byte(out), "RGBA"), weight


# ---- output -----------------------------------------------------------------------------------------

def save(relative, image):
    original = load(relative)
    if image.size != original.size:
        sys.exit(f"make_english_art: {relative} came out {image.size}, the original is {original.size}")
    path = OUT / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, optimize=True)
    print(f"wrote {path.relative_to(REPO).as_posix()}  {image.size[0]}x{image.size[1]} {image.mode}")


def verify():
    gloss = np.asarray(load(MENU_GLOSS, "L"), np.float64) / 255.0
    sheet = np.asarray(load(MENU_SHEET, "RGBA"))
    normal = np.asarray(load(MENU_NORMAL, "RGB"))
    alpha = sheet[..., 3]
    print("menu_buttons.png: colour where alpha > 0 is black everywhere:",
          bool((sheet[alpha > 0][:, :3] == 0).all()))
    print("1. menu: the relation, from the ORIGINAL gloss (= the glyph mask) to the original maps:")
    a = to_byte(glow_alpha(gloss))
    n = encode_normal(emboss_normal(gloss))
    flat = np.zeros_like(normal) + np.array([127, 127, 255], np.uint8)
    print(f"   alpha  mean |err| {mean_abs(a, alpha):.3f}/255  max {max_abs(a, alpha):.0f}")
    print(f"   normal mean |err| {mean_abs(n, normal):.3f}/255  max {max_abs(n, normal):.0f}"
          f"  (a flat map would be {mean_abs(flat, normal):.3f})")
    print("2. menu: the Portuguese labels redrawn with the fitted font, through the same pipeline:")
    maps = menu_images(menu_mask(portuguese=True))
    g = np.asarray(maps[MENU_GLOSS])
    print(f"   gloss  mean |err| {mean_abs(g, gloss * 255):.3f}/255")
    print(f"   alpha  mean |err| {mean_abs(np.asarray(maps[MENU_SHEET])[..., 3], alpha):.3f}/255")
    print(f"   normal mean |err| {mean_abs(np.asarray(maps[MENU_NORMAL]), normal):.3f}/255")
    ink, ours = gloss > 0.5, g > 127
    print(f"   glyph overlap (IoU at half coverage) {float((ink & ours).sum() / (ink | ours).sum()):.3f}")
    print("3. \"Jogador N\" erased and redrawn with the fitted font and shadow (premultiplied RGBA, label boxes):")
    for relative, *_ in PLAYER_LABELS:
        redrawn, spans = player_image(relative, portuguese=True)
        original = np.asarray(load(relative, "RGBA"), np.float64)
        ours = np.asarray(redrawn, np.float64)
        pre_o = original.copy()
        pre_o[..., :3] *= pre_o[..., 3:4] / 255
        pre_n = ours.copy()
        pre_n[..., :3] *= pre_n[..., 3:4] / 255
        box = [(slice(LABEL_TOP, None), slice(x0 - 1, x1 + 1)) for x0, x1 in spans]
        err = np.mean([mean_abs(pre_n[b], pre_o[b]) for b in box])
        outside = np.ones(original.shape[:2], bool)
        for b in box:
            outside[b] = False
        print(f"   {relative}: labels at columns {spans}; mean |err| in the boxes {err:.2f}/255; "
              f"pixels changed outside them {int((np.abs(ours - original).max(axis=-1)[outside] > 0).sum())}")
    print("4. the logo: pixels outside the subtitle's reach are the original's:")
    logo, weight = logo_image()
    original = np.asarray(load(LOGO, "RGBA"), np.int32)
    diff = np.abs(np.asarray(logo, np.int32) - original).max(axis=-1)
    print(f"   {int((weight <= 0.0).sum())} pixels outside it, of which {int((diff[weight <= 0.0] > 1).sum())} "
          f"differ by more than 1/255")
    letters, glow = logo_layers(rgba(LOGO))
    around = letters < 0.02
    print(f"   the logo's glow model against the original glow: mean |err| "
          f"{mean_abs(logo_glow(letters)[around], glow[around]) * 255:.2f}/255")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--verify", action="store_true", help="measure the models, write nothing")
    args = parser.parse_args()
    if not (ORIGINAL / MENU_SHEET).is_file():
        sys.exit(f"make_english_art: the original is not at {ORIGINAL}")
    if args.verify:
        verify()
        return
    for relative, image in menu_images(menu_mask()).items():
        save(relative, image)
    for relative, *_ in PLAYER_LABELS:
        save(relative, player_image(relative)[0])
    save(ARROW, arrow_image())
    save(LOGO, logo_image()[0])


if __name__ == "__main__":
    main()
