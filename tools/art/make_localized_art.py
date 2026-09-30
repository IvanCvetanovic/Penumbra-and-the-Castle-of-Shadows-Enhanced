#!/usr/bin/env python3
"""Localised variants of the original's images that have Portuguese baked into their pixels (E5, E24).

Reads the originals from extracted/app (never writes there) and writes each variant to
game/data/images/<language id>/<the same relative path>, the same size, mode and layout as the
original, so every rectangle the scripts and the scenes compute (sprite cuts, hit boxes, DrawSprite
rectangles) still fits it. At run time images/<id>/<path> is drawn when it exists, else the English
variant, else the original (render/Localization).

    python tools/art/make_localized_art.py                      # en + every language with a complete "art"
    python tools/art/make_localized_art.py --lang de,ru,ja,ar   # just these (fails if one is incomplete)
    python tools/art/make_localized_art.py --check              # render and fit-check, write nothing
    python tools/art/make_localized_art.py --verify             # measure the models against the originals
    options: --strings-dir DIR (the <id>.json files; default game/data/strings)
             --out-dir DIR     (writes DIR/<id>/...; default game/data/images)
             --fonts-dir DIR   (the downloaded OFL faces; default out/art-fonts, which is gitignored)

tools/art/make_english_art.py is the same tool with --lang en.

WHAT EACH LANGUAGE GETS. en: all seven files, from strings.json "images" (its "labels" tables), which
also says where each file is used. de es fr it ru tr uk ja ar: six files, from the "art" section of
game/data/strings/<id>.json:
      "art": { "labels": { "<Portuguese menu label>": "<text>", ... the seven of them },
               "back": "<the arrow's word>", "player1": "...", "player2": "..." }
  entities/menu_buttons.png, entities/menu_buttons_gloss.png, entities/normalmaps/menu_nm_buttons.png
                              labels[...]; frame order Novo jogo, Creditos, Melhores tempos, Sair,
                              Configuracoes, Como jogar, Versus (the original's accents in the JSON keys)
  interface/arrow_button.png  back
  interface/input_options1.png, interface/input_options2.png   player1, player2
The logo (entities/gamelogo.png) is English-only: the new languages show the English logo (the game's
name). A value must be non-empty, without control characters or edge spaces. The default run takes
en plus each language whose "art" is complete and skips the rest with the keys they miss; a language
named with --lang must be complete. Each menu label has its own room (THE ROOM AND THE FIT below;
--check prints it in characters): from about 8 Courier characters ("Versus") to about 18 ("Melhores
tempos"); "Sair" about 7. The arrow's word holds about 7 Latin letters (3 Japanese).

Needs Pillow built with raqm (Arabic is shaped and laid out right to left by raqm; the tool stops if
PIL.features.check('raqm') is false), numpy, fontTools (only to check glyph coverage), the three stock
Windows fonts below in %WINDIR%\\Fonts, and, for ru uk ja ar (and a Latin word Matura lacks), network
on the first run to fetch the pinned OFL faces. The output is a raster image of their glyphs, our own
derived art; no font file is committed.

FACES, per script and role:
  role            Latin (en de es fr it tr)     Cyrillic (ru uk)       Japanese (ja)             Arabic (ar)
  menu labels     Courier New                    Courier New            Noto Sans JP, wght 700    Noto Sans Arabic, wght 700
  "Player N"      Courier New Bold               Courier New Bold       Noto Sans JP, wght 700    Noto Sans Arabic, wght 700
  arrow's word    Matura MT Script Capitals;     Kurale                 Yuji Syuku                Aref Ruqaa Bold
                  Kurale if Matura lacks a letter
Courier New, Courier New Bold (cour.ttf, courbd.ttf) and Matura MT Script Capitals (MATURASC.TTF, with
Office) are Microsoft's, read from the system and rasterised, as the English art always was; Courier
covers Latin, Turkish and Cyrillic. The rest are SIL Open Font License 1.1 faces from google/fonts, fetched at commit
9710da1 and checked by sha256 (the Face definitions below); their licences are
tools/art/fonts/OFL-*.txt. The first run needs network to fetch them (into out/art-fonts):
  Noto Sans JP   (Google, from Adobe's Source Han Sans; the face the game bundles for Japanese text).
                 Japanese has no Courier; a monoline gothic is the nearest to Courier's stroke, and
                 weight 700 unstroked has the Courier labels' stems (6-7 px, measured as run lengths
                 in the gloss maps; 600 is thinner, 800 clogs the kanji). Instanced by FreeType.
  Noto Sans Arabic (Google; the face the game bundles for Arabic text), wght 700 wdth 100. Courier's
                 own Arabic is monospaced: a spaced-out typewriter Arabic. This low-contrast sans sits
                 best by Courier's even stroke, its stems unstroked are the Courier labels' 6-7 px, and
                 it stays legible at the player labels' 15 px, where Noto Naskh Arabic (compared, with
                 a 1 px stroke) thins out.
  Kurale         (Eduardo Tunni) - a soft serif with pen-like curled terminals: calligraphic, upright
                 like "Voltar", with modern Cyrillic letterforms (the game's players include children,
                 so the Н must not be the historical N-shaped one) and flared Д descenders that recall
                 the old book hands; a 0.75 px stroke gives it Matura's weight. Full Russian, Ukrainian
                 (і ї є ґ) and Turkish, hence also the Latin fallback. Compared in the arrow at its real
                 size: Monomakh (an ustav whose Н reads as "N"), Philosopher Bold and Bold Italic (a
                 modern humanist face), Cormorant Unicase (thin capitals), Ruslan Display (geometric
                 poster capitals), Lobster (1950s script), Marck Script (its capital И reads as "U"),
                 Oranienbaum, Yeseva One, Forum, Kelly Slab, Amatic SC and Bad Script.
  Yuji Syuku     (Kinuta Font Factory) - the brush handwriting of the calligrapher Yuji Kataoka: the
                 Japanese counterpart of a medieval pen hand, legible at the arrow's size, with a 0.5 px
                 stroke for Matura's weight. Yuji Boku and Yuji Mai write ru (U+308B) as a cursive
                 loop; Potta One reads as "3"; Zen Antique and Kaisei Tokumin are printing types.
  Aref Ruqaa     (Abdullah Aref, Khaled Hosny; Latin after Hermann Zapf) - classical Ruqaa, the Arabic
                 everyday calligraphic hand, heavy and legible. Katibeh (lighter) and Blaka (a
                 blackletter-style display Arabic) were compared.

WHICH IMAGES CARRY PORTUGUESE (every image under entities/ and interface/ looked at):
  entities/menu_buttons.png         the menu's seven labels, one per 512x64 frame (buttons.ent,
                                    SpriteCut 1x8, frame 7 blank), with its normal map
                                    entities/normalmaps/menu_nm_buttons.png and gloss map
                                    entities/menu_buttons_gloss.png carrying the same letters
  entities/gamelogo.png             "e o Castelo das Sombras" under "Penumbra"
  interface/input_options1.png      "Jogador 1" / "Jogador 2" under the keyboard and the pad
  interface/input_options2.png      the same under two pads
  interface/arrow_button.png        "Voltar" on the video options screen's back arrow
Already English or wordless: gameover.png ("Game Over"), Lapide_Bitmap*.png ("CHECK POINT"),
tombstone.png (an illegible smudge), joystick.png ("SELECT"/"START"/"ANALOG"), thumbnails.png,
and every other texture, normal map and interface image. They stay as they are in every language.

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
its Portuguese label, so each label starts where the Portuguese one did. (Mean absolute
error over the seven labels 0.049; Courier New Bold 0.054, Consolas Bold and Lucida Console worse.)
Per script: Latin and Cyrillic as fitted. The split stretch suits Latin Courier only, so Japanese is
drawn unstretched, 48 px to the em (ideographs 44-46 px tall, the Latin capitals' 42 px plus their
accents), baseline 53 px into each frame; Arabic in Noto Sans Arabic, unstretched and unstroked,
50 px to the em, baseline 47 px into each frame (its tallest hamza and deepest descender are moved
a few px within the frame).

THE ROOM AND THE FIT. The menu scene (menu.esc) puts each frame at its own place, with statues,
torches and the description panel (x 633 on) beside the labels, so each frame has its own room
(MenuRoom, printed by --check with the characters it holds). A left-to-right label keeps the fitted
pen (it lines up with the Portuguese and English ones) and ends by the wider of the Portuguese and
English inks plus 20 px (4% of the frame), but 4 px short of the first obstacle to its right at its
rows (measured in the menu captures), and never short of where the Portuguese or English already
reach. That is frame 0 "Novo jogo": up to column 302 (the Portuguese; the right devil statue's
horns are at 284); 1 "Creditos": 289; 2 "Melhores tempos": 500 (the Portuguese; the panel is at
504); 4 "Configuracoes": 448; 5 "Como jogar": 369 (the English "How to Play"; the right torch's
brazier is at 357); 6 "Versus": 222. Frame 3 "Sair" is the exception (MENU_FREE_FLOOR): its label
may use the whole free floor, up to column 181, 4 px short of the lower torch (its pot rim and
flame start at column 186 in the label's rows, measured row by row), and be condensed down to
0.74x, so the 7-letter standard words fit ("Beenden", "Quitter": 0.75x). buttons.ent's click
box (columns 53.5-422.5) is not the limit: the original's labels overhang it on both sides. A
right-to-left (Arabic) label ends where the Portuguese label's ink ends, where its reading starts,
and starts no further left than the Portuguese ink. A label that is too wide is condensed
horizontally (its height stays that of its neighbours) down to 0.80x (frame 3: 0.74x). One that would cross into a
neighbouring frame (an accented capital) is first moved up to 4 px into the frame's spare rows,
then shortened down to 0.85x. Beyond either, the language is reported with each label that does
not fit, its room and about how many characters of its width would, and nothing is written for
it; the other languages are written and the tool exits 1. "Ink" is any non-zero byte of G,
measured where it is drawn; the glow is not checked (the original's bleeds too). The blank frame 7
must stay blank. English passes every check unchanged, as --check prints (its "Settings" touches
frame 4's first row, and "How to Play" sets frame 5's room).

THE "JOGADOR" LABELS. Lavender (203,203,228) Courier New Bold, 13.37 px to the em across,
stretched 1.22x above the baseline and 0.74x below, over a black drop shadow of 0.78 opacity,
offset (1.3, 1.25) px and blurred by a 0.55 px Gaussian. The old label is erased first: below the
pictures' glow it is transparent; in the glow rows its pixels are refilled from the glow on
either side. The new label is centred where "Jogador N" was, on the same baseline (Latin,
Cyrillic), or, for Japanese (Noto Sans JP 700 at 13.5 px) and Arabic (Noto Sans Arabic 700 at
15 px), unstretched, with its ink centred on row 64. The ink must lie in rows 56-71 (the English
and Portuguese ascenders reach row 56) and in its own picture's columns (between the image's edge
and the thin bar between the pictures, 2 px clear of the bar), condensed down to 0.80x if needed.

THE LOGO AND THE ARROW. Their lettering is a hand-cut uncial with no installed match. Matura MT
Script Capitals is the nearest in weight and spirit (heavy broad-pen strokes, swash C and S), at
the original subtitle's x-height (size 36: 16 px) and span. The logo keeps every pixel of
"Penumbra" outside the subtitle's reach (15 px, the glow's extent); inside it the letters are
black and the white glow is c * (1 - exp(-k * gauss(letters, s))) with s 4.3555, k 2.6089,
c 0.9104 (the logo's glow fitted the same way, 9.6/255 mean error: the original's glow is
hand-made), blended into the original over a 2.5 px feather. The arrow's body is flat opaque
white, so "Voltar" is erased by painting its pixels white and the new word is drawn black on it,
centred on "Voltar"'s columns: Latin at size 28 (x-height 12 px, "Voltar"'s) and Cyrillic in
Kurale at size 26 (the same x-height) with a 0.75 px stroke, on "Voltar"'s baseline; Japanese in
Yuji Syuku at size 26 with a 0.5 px stroke and Arabic in Aref Ruqaa Bold at size 28, with their ink
centred on the body's middle row (44). English centres by Pillow's text box, as its art always
did; the other languages by the drawn ink (a brush or Ruqaa word's box is lopsided). Every inked
pixel must fall on the opaque white body inside rows 28-60, columns 16-108; a word that does not
fit is drawn smaller, half a size at a time, down to 20 (Latin, Cyrillic) or 18, and then the tool
stops. Both are a judgement call about a different typeface; strings.json can drop the English ones
by emptying their "en", and deleting images/<id>/interface/arrow_button.png falls back to English.
"""

import argparse
import hashlib
import json
import os
import pathlib
import sys
import unicodedata
import urllib.request

import numpy as np
from PIL import Image, ImageDraw, ImageFont, features

REPO = pathlib.Path(__file__).resolve().parents[2]
ORIGINAL = REPO / "extracted" / "app"
IMAGES = REPO / "game" / "data" / "images"
STRINGS_JSON = REPO / "game" / "data" / "strings.json"
STRINGS_DIR = REPO / "game" / "data" / "strings"
FONT_CACHE = REPO / "out" / "art-fonts"
WINDOWS_FONTS = pathlib.Path(os.environ.get("WINDIR", r"C:\Windows")) / "Fonts"

# The picker's order without Portuguese, which is the original.
LANGUAGES = ["en", "de", "es", "fr", "it", "ru", "tr", "uk", "ja", "ar"]

# The fits were made at these; the stroke is whole pixels at RENDER_EM, so they are part of the look.
RENDER_EM = 400
SUPERSAMPLE = 2

TOOL = "make_localized_art"


def fail(message):
    sys.exit(f"{TOOL}: {message}")


class Misfit(Exception):
    """A language's words that do not fit (or cannot be drawn): that language is reported and not
    written, and the run goes on with the others."""


# ---- faces ------------------------------------------------------------------------------------------

GOOGLE_FONTS = "https://raw.githubusercontent.com/google/fonts/9710da1eacb3be272583c3224dcb70f9da6eadbb/ofl/"


class Face:
    """A font file: a stock Windows face read from %WINDIR%\\Fonts, or an OFL face from google/fonts
    pinned by commit and sha256, fetched once into the fonts directory. `axes` instances a variable
    face, in its own axis order (FreeType's instancing, through Pillow)."""

    directory = FONT_CACHE

    def __init__(self, file, source=None, sha256=None, axes=None):
        self.file, self.source, self.sha256, self.axes = file, source, sha256, axes
        self.fonts = {}
        self.cmap = None

    def path(self):
        if self.source is None:
            path = WINDOWS_FONTS / self.file
            if not path.is_file():
                fail(f"{path} is missing (a stock Windows font)")
            return path
        path = Face.directory / self.file
        if path.is_file() and sha256_of(path) == self.sha256:
            return path
        url = GOOGLE_FONTS + self.source
        print(f"fetching {url}")
        try:
            data = urllib.request.urlopen(url, timeout=120).read()
        except OSError as error:
            fail(f"cannot fetch {url} ({error}); put {self.file} in {Face.directory} or pass --fonts-dir")
        if hashlib.sha256(data).hexdigest() != self.sha256:
            fail(f"{url} does not have the pinned sha256 {self.sha256}")
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        return path

    def font(self, size):
        if size not in self.fonts:
            font = ImageFont.truetype(str(self.path()), size)
            if self.axes is not None:
                font.set_variation_by_axes(self.axes)
            self.fonts[size] = font
        return self.fonts[size]

    def missing(self, text):
        """The characters of `text` the face has no glyph for."""
        if self.cmap is None:
            try:
                from fontTools.ttLib import TTFont
            except ImportError:
                fail("needs fontTools to check glyph coverage (pip install fonttools)")
            self.cmap = set(TTFont(str(self.path()), lazy=True).getBestCmap())
        return sorted({c for c in text if ord(c) not in self.cmap})

    def __str__(self):
        return self.file if self.axes is None else f"{self.file} at {self.axes}"


def sha256_of(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


COURIER = Face("cour.ttf")
COURIER_BOLD = Face("courbd.ttf")
MATURA = Face("MATURASC.TTF")
NOTO_SANS_JP_700 = Face("NotoSansJP[wght].ttf", "notosansjp/NotoSansJP%5Bwght%5D.ttf",
                        "c2f3b4d463500a2ddcd3849cded1fceeb9fd6d1c32e6cbecd568453ba50fc68f", axes=[700])
NOTO_SANS_ARABIC_700 = Face("NotoSansArabic[wdth,wght].ttf", "notosansarabic/NotoSansArabic%5Bwdth,wght%5D.ttf",
                            "63111b5b2e074dd48cc67692e0a2726d86ee94c1c37fe8598257b7b4e87e869e", axes=[700, 100])
KURALE = Face("Kurale-Regular.ttf", "kurale/Kurale-Regular.ttf",
              "6055aaca6870ca64210dd81fa47f8bb71595a9c8fbe570351ed45a15f1a0820d")
YUJI_SYUKU = Face("YujiSyuku-Regular.ttf", "yujisyuku/YujiSyuku-Regular.ttf",
                  "82728ebafc8c97391e2dab633414a806f344b8e4e2227d307179f07b548fca61")
AREF_RUQAA_BOLD = Face("ArefRuqaa-Bold.ttf", "arefruqaa/ArefRuqaa-Bold.ttf",
                       "247071015b7eefd63f94d6e47949c5d10294ed31bd432f809ba9a219d93f91bb")


# ---- per-script styles --------------------------------------------------------------------------------

class LabelStyle:
    """How a label is drawn in one script: a face at `em` px to the em across, stretched `above` and
    `below` the baseline, with an outline `stroke` (px), raqm `layout` options, and where it goes:
    `baseline` None keeps the fitted Portuguese baseline, a number is rows into the frame (menu) or the
    row the ink is centred on (player labels); `align` 'pen' starts at the fitted pen (left to right),
    'portuguese' ends where the Portuguese label's ink ends (right to left, see MenuRoom)."""

    def __init__(self, face, em, above, below, stroke=0.0, layout=None, baseline=None, align="pen"):
        self.face, self.em, self.above, self.below, self.stroke = face, em, above, below, stroke
        self.layout = layout or {}
        self.baseline, self.align = baseline, align
        self.renderers = {}

    def renderer(self, squeeze=1.0, vertical=1.0, scale=1.0):
        key = (squeeze, vertical, scale)
        if key not in self.renderers:
            if key == (1.0, 1.0, 1.0):
                self.renderers[key] = LabelRenderer(self.face, self.em, self.above, self.below, self.stroke,
                                                    self.layout)
            else:
                self.renderers[key] = LabelRenderer(self.face, self.em * scale, self.above * vertical,
                                                    self.below * vertical, self.stroke * scale, self.layout,
                                                    squeeze)
        return self.renderers[key]


class ArrowStyle:
    """The arrow's word: a face at `size` (px, Pillow's) with an outline `stroke` (px) to give it
    Matura's weight, drawn smaller down to `smallest` if it does not fit; `centred` puts the ink's
    middle on the body's middle row instead of on "Voltar"'s baseline."""

    def __init__(self, face, size, smallest, layout=None, stroke=0.0, centred=False):
        self.face, self.size, self.smallest, self.stroke = face, size, smallest, stroke
        self.layout = layout or {}
        self.centred = centred


RTL = {"direction": "rtl", "language": "ar"}
JA = {"language": "ja"}

KURALE_ARROW = ArrowStyle(KURALE, 26, 20, stroke=0.75)

SCRIPTS = {
    "latin": {
        "menu": LabelStyle(COURIER, 54.9, 1.34, 0.6, 1.5),
        "player": LabelStyle(COURIER_BOLD, 13.37, 1.22, 0.74),
        "arrow": [ArrowStyle(MATURA, 28, 20), KURALE_ARROW],
    },
    "cyrillic": {
        "menu": LabelStyle(COURIER, 54.9, 1.34, 0.6, 1.5),
        "player": LabelStyle(COURIER_BOLD, 13.37, 1.22, 0.74),
        "arrow": [KURALE_ARROW],
    },
    "japanese": {
        "menu": LabelStyle(NOTO_SANS_JP_700, 48.0, 1.0, 1.0, 0.0, JA, baseline=53.0),
        "player": LabelStyle(NOTO_SANS_JP_700, 13.5, 1.0, 1.0, 0.0, JA, baseline=64.0),
        "arrow": [ArrowStyle(YUJI_SYUKU, 26, 18, JA, stroke=0.5, centred=True)],
    },
    "arabic": {
        "menu": LabelStyle(NOTO_SANS_ARABIC_700, 50.0, 1.0, 1.0, 0.0, RTL, baseline=47.0, align="portuguese"),
        "player": LabelStyle(NOTO_SANS_ARABIC_700, 15.0, 1.0, 1.0, 0.0, RTL, baseline=64.0),
        "arrow": [ArrowStyle(AREF_RUQAA_BOLD, 28, 18, RTL, centred=True)],
    },
}

SCRIPT_OF = {"en": "latin", "de": "latin", "es": "latin", "fr": "latin", "it": "latin", "tr": "latin",
             "ru": "cyrillic", "uk": "cyrillic", "ja": "japanese", "ar": "arabic"}

MIN_SQUEEZE = 0.80      # the narrowest a label is condensed to fit (0.72 looks squeezed)
MIN_VERTICAL = 0.85     # the most a label is shortened to stay in its frame
MAX_SHIFT = 4           # px a menu label may move up or down in its frame before it is shortened


# ---- image helpers --------------------------------------------------------------------------------------

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


def ink_bounds(coverage, x_offset=0.0, y_offset=0.0):
    """(left, right, top, bottom) of every pixel that rounds to a non-zero byte, as pixel edges
    (right and bottom exclusive), or None if there is none."""
    ink = coverage >= 0.5 / 255.0
    if not ink.any():
        return None
    ys = np.nonzero(ink.any(axis=1))[0]
    xs = np.nonzero(ink.any(axis=0))[0]
    return (xs.min() + x_offset, xs.max() + 1 + x_offset, ys.min() + y_offset, ys.max() + 1 + y_offset)


class LabelRenderer:
    """One font, drawn large once per text, then mapped onto the target with a float affine (so a
    pen origin or baseline can sit between pixels) and box-filtered from a supersampled grid. The
    part above the baseline and the part below are stretched separately; `squeeze` condenses it
    horizontally."""

    def __init__(self, face, em, above, below, stroke, layout=None, squeeze=1.0):
        self.font = face.font(RENDER_EM)
        self.em, self.above, self.below, self.stroke = em, above, below, stroke
        self.layout = layout or {}
        self.squeeze = squeeze
        self.cache = {}

    def _large(self, text):
        if text not in self.cache:
            stroke = int(round(self.stroke * RENDER_EM / self.em))
            width = int(self.font.getlength(text, **self.layout)) + RENDER_EM
            image = Image.new("L", (width, RENDER_EM * 2), 0)
            origin, base = RENDER_EM // 2, int(RENDER_EM * 1.3)
            ImageDraw.Draw(image).text((origin, base), text, font=self.font, fill=255, anchor="ls",
                                       stroke_width=stroke, stroke_fill=255, **self.layout)
            self.cache[text] = (image, origin, base)
        return self.cache[text]

    def rows(self, text, x0, baseline, y0, y1, width):
        """The label's coverage (0..1) on target rows y0..y1-1, pen at (x0, baseline)."""
        image, origin, base = self._large(text)
        s = self.em / RENDER_EM
        sx = s if self.squeeze == 1.0 else s * self.squeeze
        w, h = width * SUPERSAMPLE, (y1 - y0) * SUPERSAMPLE
        out = np.zeros((h, w))
        v = ((np.arange(h) + 0.5) / SUPERSAMPLE + y0)[:, None]
        for k, above in ((self.above, True), (self.below, False)):
            a = 1.0 / (SUPERSAMPLE * sx)
            c = origin - x0 / sx
            e = 1.0 / (SUPERSAMPLE * s * k)
            f = base + (y0 - baseline) / (s * k)
            part = np.asarray(image.transform((w, h), Image.Transform.AFFINE, (a, 0, c, 0, e, f),
                                              resample=Image.BILINEAR), dtype=np.float64) / 255.0
            out += part * ((v < baseline) if above else (v >= baseline))
        return out.reshape(y1 - y0, SUPERSAMPLE, width, SUPERSAMPLE).mean(axis=(1, 3))

    def ink_span(self, text):
        """The label's inked columns relative to the pen, at the target scale."""
        mask = self.rows(text, 0.0, 0.0, -int(self.em * 2), int(self.em), int(self.font.getlength(text, **self.layout)
                                                                                * self.em / RENDER_EM) + 8)
        columns = np.nonzero(mask.max(axis=0) > 0.3)[0]
        return float(columns.min()), float(columns.max() + 1)

    def ink_box(self, text, pen, baseline):
        """(left, right, top, bottom) of every non-zero byte with the pen at (pen, baseline), as pixel
        edges on the target grid (so a fractional pen or baseline is measured where it will be drawn)."""
        margin = int(self.em)
        x0, y0 = int(np.floor(pen)) - margin, int(np.floor(baseline)) - 2 * margin
        width = int(self.font.getlength(text, **self.layout) * self.em / RENDER_EM) + 2 * margin
        coverage = self.rows(text, pen - x0, baseline, y0, y0 + 3 * margin, width)
        return ink_bounds(coverage, x0, y0)


def stroked(layout, stroke, scale):
    """Pillow's keyword arguments for a layout and an outline stroke (px) drawn `scale`x."""
    options = dict(layout or {})
    if stroke:
        options.update(stroke_width=int(round(stroke * scale)))
    return options


def plain_text(face, size, text, x, baseline, width, height, scale=4, layout=None, stroke=0.0):
    """Unstretched text coverage (0..1), drawn `scale`x and box-filtered, pen at (x, baseline)."""
    font = face.font(size * scale)
    image = Image.new("L", (width * scale, height * scale), 0)
    options = stroked(layout, stroke, scale)
    if stroke:
        options.update(stroke_fill=255)
    ImageDraw.Draw(image).text((x * scale, baseline * scale), text, font=font, fill=255, anchor="ls", **options)
    return np.asarray(image.resize((width, height), Image.BOX), np.float64) / 255.0


def plain_box(face, size, text, layout=None, stroke=0.0):
    """Pillow's box (left, top, right, bottom) of unstretched text relative to its pen, in pixels."""
    box = face.font(size * 4).getbbox(text, anchor="ls", **stroked(layout, stroke, 4))
    return tuple(v / 4.0 for v in box)


def plain_width(face, size, text, layout=None):
    """The inked extent (left, right) of unstretched text relative to its pen, in pixels."""
    left, _, right, _ = plain_box(face, size, text, layout)
    return left, right


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


def condensing(width, room, least=None):
    """The squeeze that brings an ink `width` to `room`, or 1 if it already fits."""
    return 1.0 if width <= room else max(least or MIN_SQUEEZE, room / width - 0.002)


# ---- the menu labels --------------------------------------------------------------------------------------

MENU_SHEET = "entities/menu_buttons.png"
MENU_GLOSS = "entities/menu_buttons_gloss.png"
MENU_NORMAL = "entities/normalmaps/menu_nm_buttons.png"

# Frame, the Portuguese label, and its fitted pen origin x and baseline y.
MENU_FRAMES = [
    (0, "Novo jogo", 8.610, 50.618),
    (1, "Cr\u00e9ditos", 9.803, 117.663),
    (2, "Melhores tempos", 9.962, 179.795),
    (3, "Sair", 9.203, 243.447),
    (4, "Configura\u00e7\u00f5es", 4.179, 304.674),
    (5, "Como jogar", 7.468, 371.664),
    (6, "Versus", 9.283, 437.616),
]
MENU_FRAME_HEIGHT = 64
MENU_WIDTH = 512

# Each frame's place in the menu scene (menu.esc: the button entity's x; the sprite is centred, so the
# frame's column 0 is at x - 256), and the first thing to the right of its label at the label's rows,
# in scene x, measured in the menu captures (out/shots/l10n/before/menu_newgame_pt.png and _en.png,
# 1024x768, the scene at 1:1). The description panel, drawn while a button is under the cursor,
# starts at x 633.
MENU_SCENE_X = {0: 434, 1: 535, 2: 385, 3: 652, 4: 432, 5: 399, 6: 484}
MENU_OBSTACLE = {
    0: (462, "the right devil statue's horns"),
    1: (633, "the description panel"),
    2: (633, "the description panel"),
    3: (582, "the lower torch"),
    4: (633, "the description panel"),
    5: (500, "the right torch's brazier"),
    6: (488, "the right devil statue's arm"),
}
MENU_MARGIN = 20        # px past the wider of the Portuguese and English labels: 4% of the frame
# Frames whose label may use the whole free floor up to its obstacle, not just MENU_MARGIN past the
# Portuguese and English, and be condensed further: {frame: the narrowest condensation}. "Sair" and
# "Quit" have 4 letters where the standard words ("Beenden", "Quitter") have 7.
MENU_FREE_FLOOR = {3: 0.74}
OBSTACLE_GAP = 4        # px kept clear of the obstacle
COURIER_PITCH = 54.9 * 0.6

GLOW_SIGMA = 2.2515
GLOW_GAIN = 1.8989
NORMAL_SIGMA = 2.3915
NORMAL_GAIN = 3.0279


class MenuRoom:
    """Where one frame's label may be, in frame columns (right edges exclusive). A left-to-right label
    starts at the fitted pen and ends by `right`: the wider of the Portuguese and English inks plus
    MENU_MARGIN, but not nearer than OBSTACLE_GAP to the obstacle, and never short of where the
    Portuguese or English already reach. A right-to-left label ends where the Portuguese does (its
    reading starts there) and starts no further left than the Portuguese: `portuguese`."""

    def __init__(self, frame, portuguese_box, english_box):
        self.frame = frame
        self.portuguese = (portuguese_box[0], portuguese_box[1])
        self.base = max(portuguese_box[1], english_box[1])
        scene_x, self.obstacle = MENU_OBSTACLE[frame]
        self.obstacle_column = scene_x - (MENU_SCENE_X[frame] - MENU_WIDTH // 2)
        self.least = MENU_FREE_FLOOR.get(frame, MIN_SQUEEZE)
        if frame in MENU_FREE_FLOOR:
            self.right = max(self.base, self.obstacle_column - OBSTACLE_GAP)
        else:
            self.right = max(self.base, min(self.base + MENU_MARGIN, self.obstacle_column - OBSTACLE_GAP))
        if self.right == self.base:
            self.why = f"where the Portuguese or English already ends; {self.obstacle} at column {self.obstacle_column}"
        elif self.right < self.base + MENU_MARGIN or frame in MENU_FREE_FLOOR:
            self.why = f"{OBSTACLE_GAP} px short of {self.obstacle} at column {self.obstacle_column}"
        else:
            self.why = f"{MENU_MARGIN} px past the wider of the Portuguese and English"


_MENU_ROOMS = {}


def menu_rooms():
    """{frame: MenuRoom}, measured on the Portuguese and English labels drawn as fitted."""
    if not _MENU_ROOMS:
        renderer = SCRIPTS["latin"]["menu"].renderer()
        english = english_words().menu
        for (frame, pt, x0, baseline), en in zip(MENU_FRAMES, english):
            _MENU_ROOMS[frame] = MenuRoom(frame, renderer.ink_box(pt, x0, baseline),
                                          renderer.ink_box(en, x0, baseline))
    return _MENU_ROOMS


def most_characters(text, width, room, least=None):
    """About how many characters of `text`'s average width fit `room` condensed to `least`."""
    return int(len(text) * room / (width * (least or MIN_SQUEEZE)))


def fit_menu_label(style, text, frame, x0, baseline):
    """Where and how to draw one label so that it passes the fit: (renderer, pen, baseline, report)."""
    top = frame * MENU_FRAME_HEIGHT
    bottom = top + MENU_FRAME_HEIGHT
    room_of = menu_rooms()[frame]
    if style.baseline is not None:
        baseline = top + style.baseline
    rtl = style.align == "portuguese"
    left_limit, right_limit = room_of.portuguese if rtl else (-np.inf, room_of.right)
    squeeze, vertical, shift = 1.0, 1.0, 0
    for _ in range(64):
        renderer = style.renderer(squeeze, vertical)
        pen = x0
        box = renderer.ink_box(text, pen, baseline)
        if box is None:
            raise Misfit(f"menu label {text!r} draws no ink")
        if rtl:
            # A whole-pixel shift moves the ink by exactly that much.
            pen += right_limit - box[1]
            box = renderer.ink_box(text, pen, baseline)
        left, right, up, down = box
        room = right_limit - (left_limit if rtl else left)
        fits_h = left >= left_limit and right <= right_limit
        fits_v = up >= top and down <= bottom
        if fits_h and fits_v:
            limit = (f"the Portuguese {left_limit:.0f}-{right_limit - 1:.0f}" if rtl else f"up to {right_limit - 1:.0f}")
            return renderer, pen, baseline, (left, right, up - top, down - top, squeeze, vertical, shift, limit)
        if not fits_v:
            # First move it (whole pixels, so the ink moves exactly) into the frame's spare rows...
            over_top, over_bottom = top - up, down - bottom
            if shift == 0 and (over_top > 0) != (over_bottom > 0):
                move = min(over_top, bottom - down, MAX_SHIFT) if over_top > 0 else \
                    -min(over_bottom, up - top, MAX_SHIFT)
                if move:
                    baseline += move
                    shift = move
                    continue
            # ...then shorten it.
            vertical = round(vertical - 0.01, 4)
            if vertical < MIN_VERTICAL:
                raise Misfit(f"menu label {text!r} (frame {frame}) spans frame rows {up - top:.0f}-"
                             f"{down - top:.0f}, outside 0-{MENU_FRAME_HEIGHT} even at {MIN_VERTICAL}x height")
            continue
        width = (right - left) / squeeze
        least = room_of.least
        new = condensing(width, room, least)
        if new >= squeeze:
            new = round(squeeze - 0.005, 4)
        if new < least:
            limit = (f"columns {left_limit:.0f}-{right_limit - 1:.0f}, the Portuguese label's" if rtl else
                     f"up to column {right_limit - 1:.0f}: {room_of.why}")
            raise Misfit(f"menu label {text!r} (frame {frame}, Portuguese {MENU_FRAMES[frame][1]!r}) is "
                         f"{width:.0f} px wide and has {room:.0f} px ({limit}); even condensed to "
                         f"{least}x it is {width * least:.0f} px: at most about "
                         f"{most_characters(text, width, room, least)} characters of this width fit")
        squeeze = new
    raise Misfit(f"menu label {text!r} (frame {frame}) did not settle")


def menu_mask(texts, style=None, fit=True, report=None, misfits=None):
    """The whole sheet's glyph mask G; `texts` in frame order. fit=False draws at the fitted
    geometry unchecked (--verify redraws the Portuguese that way). With `misfits` a list, a label
    that does not fit is noted there and left out instead of stopping."""
    style = style or SCRIPTS["latin"]["menu"]
    width, height = MENU_WIDTH, 512
    mask = np.zeros((height, width))
    for (frame, _, x0, baseline), text in zip(MENU_FRAMES, texts):
        renderer, pen = style.renderer(), x0
        if fit:
            try:
                renderer, pen, baseline, placed = fit_menu_label(style, text, frame, x0, baseline)
            except Misfit as misfit:
                if misfits is None:
                    raise
                misfits.append(str(misfit))
                continue
            if report is not None:
                report.append((frame, text) + placed)
        y0 = max(0, int(baseline) - MENU_FRAME_HEIGHT)
        y1 = min(height, int(baseline) + 24)
        rows = renderer.rows(text, pen, baseline, y0, y1, width)
        mask[y0:y1] = np.maximum(mask[y0:y1], rows)
    return np.clip(mask, 0.0, 1.0)


# Common menu words, to give Arabic's proportional letters an average width for the room table.
ARABIC_SAMPLE = "\u0644\u0639\u0628\u0629 \u062c\u062f\u064a\u062f\u0629 \u062e\u0631\u0648\u062c " \
                "\u0627\u0644\u0625\u0639\u062f\u0627\u062f\u0627\u062a \u0645\u0648\u0627\u062c\u0647\u0629"


def print_rooms():
    """The room each frame's label has, for translators: px, and about how many characters."""
    arabic = SCRIPTS["arabic"]["menu"]
    arabic_pitch = (arabic.face.font(RENDER_EM).getlength(ARABIC_SAMPLE, **arabic.layout) * arabic.em
                    / RENDER_EM / len(ARABIC_SAMPLE))
    japanese_pitch = SCRIPTS["japanese"]["menu"].em
    print(f"menu label room per frame, in frame columns; characters at full width / condensed as far as the frame "
          f"allows ({MIN_SQUEEZE}x; frame 3 {MENU_FREE_FLOOR[3]}x) "
          f"(Latin and Cyrillic: Courier, {COURIER_PITCH:.1f} px each; Japanese {japanese_pitch:.0f} px each; "
          f"Arabic about {arabic_pitch:.1f} px per letter):")
    for frame, pt, x0, _ in MENU_FRAMES:
        room = menu_rooms()[frame]
        ltr = room.right - x0
        rtl = room.portuguese[1] - room.portuguese[0]
        least = room.least
        print(f"  frame {frame} {pt!r}: left to right from the pen at {x0:.0f} to column {room.right - 1:.0f}, "
              f"{ltr:.0f} px ({room.why})")
        print(f"      Latin/Cyrillic {ltr / COURIER_PITCH:.1f} / {ltr / COURIER_PITCH / least:.1f}, "
              f"Japanese {ltr / japanese_pitch:.1f} / {ltr / japanese_pitch / least:.1f};  Arabic, right to "
              f"left in the Portuguese columns {room.portuguese[0]:.0f}-{room.portuguese[1] - 1:.0f}, {rtl:.0f} px: "
              f"{rtl / arabic_pitch:.1f} / {rtl / arabic_pitch / least:.1f}")


def check_menu_sheet(mask):
    """The composed sheet: each frame's ink stays in its own rows (fit_menu_label saw each label
    alone), and the blank frame 7 stays blank."""
    ink = to_byte(mask) > 0
    blank = ink[len(MENU_FRAMES) * MENU_FRAME_HEIGHT:]
    if blank.any():
        fail("ink in the blank frame 7 of the menu sheet")


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

PLAYER_COLOUR = (203 / 255, 203 / 255, 228 / 255)
SHADOW_OPACITY = 0.78
SHADOW_OFFSET = (1.3, 1.25)
SHADOW_SIGMA = 0.55
LABEL_TOP = 57          # the first row a label touches; the pictures' glow ends by row 59
GLOW_END = 59           # rows from here down hold nothing but the label and its shadow
LABEL_ROWS = (56, 72)   # where a label's ink may be: the English and Portuguese ascenders reach row 56
SEPARATOR_WIDTH = 4     # the bar between the two pictures is 3 columns wide
AREA_MARGIN = (1, 2)    # columns kept clear of the image's edge, and of the bar

# Image, the Portuguese labels, the fitted baselines. The columns are found in the image.
PLAYER_LABELS = [
    ("interface/input_options1.png", ["Jogador 1", "Jogador 2"], [67.60, 68.26]),
    ("interface/input_options2.png", ["Jogador 1", "Jogador 2"], [67.56, 67.27]),
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


def label_areas(image, count):
    """The columns each label may use: from the image's edge to the thin bar between the pictures."""
    used = (image[:LABEL_TOP, :, 3] > 0).any(axis=0)
    runs = []
    for x in np.nonzero(used)[0]:
        if runs and x - runs[-1][1] <= 1:
            runs[-1][1] = x
        else:
            runs.append([x, x])
    bars = [(a, b + 1) for a, b in runs if b + 1 - a <= SEPARATOR_WIDTH]
    if len(bars) != count - 1:
        fail(f"found {len(bars)} bars between the pictures, expected {count - 1}")
    starts = [AREA_MARGIN[0]] + [end + AREA_MARGIN[1] for _, end in bars]
    ends = [start - AREA_MARGIN[1] for start, _ in bars] + [image.shape[1] - AREA_MARGIN[0]]
    return list(zip(starts, ends))


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


def fit_player_label(style, text, centre, baseline, area, height):
    """(renderer, baseline, report) for one label: its ink inside `area`'s columns and rows
    LABEL_ROWS[0]..height-1, centred where the Portuguese was."""
    squeeze, scale = 1.0, 1.0
    for _ in range(64):
        renderer = style.renderer(squeeze, 1.0, scale)
        span_left, span_right = renderer.ink_span(text)
        pen = centre - (span_left + span_right) / 2.0
        base = baseline
        if style.baseline is not None:
            # Centre the ink's rows on style.baseline, measured where it will be drawn.
            _, _, up, down = renderer.ink_box(text, pen, 0.0)
            base = style.baseline - (up + down) / 2.0
        box = renderer.ink_box(text, pen, base)
        if box is None:
            raise Misfit(f"player label {text!r} draws no ink")
        left, right, up, down = box
        fits_h = left >= area[0] and right <= area[1]
        fits_v = up >= LABEL_ROWS[0] and down <= height
        if fits_h and fits_v:
            return renderer, base, (left, right, up, down, squeeze, scale)
        if not fits_v:
            scale = round(scale - 0.01, 4)
            if scale < MIN_VERTICAL:
                raise Misfit(f"player label {text!r} spans rows {up:.0f}-{down:.0f}, outside "
                             f"{LABEL_ROWS[0]}-{height} even at {MIN_VERTICAL}x size")
            continue
        room = 2.0 * min(centre - area[0], area[1] - centre)
        new = condensing((right - left) / squeeze, room)
        if new >= squeeze:
            new = round(squeeze - 0.005, 4)
        if new < MIN_SQUEEZE:
            width = (right - left) / squeeze
            raise Misfit(f"player label {text!r} is {width:.0f} px wide and has {room:.0f} px around column "
                         f"{centre:.0f}; even condensed to {MIN_SQUEEZE}x it is {width * MIN_SQUEEZE:.0f} px: at "
                         f"most about {most_characters(text, width, room)} characters of this width fit")
        squeeze = new
    raise Misfit(f"player label {text!r} did not settle")


def player_image(relative, texts, style=None, fit=True, report=None):
    style = style or SCRIPTS["latin"]["player"]
    image = rgba(relative)
    spans = label_columns(image)
    _, pt, baselines = next(entry for entry in PLAYER_LABELS if entry[0] == relative)
    if len(spans) != len(pt):
        fail(f"{relative}: found {len(spans)} labels, expected {len(pt)}")
    out = erase_labels(image, spans)
    areas = label_areas(image, len(spans)) if fit else [None] * len(spans)
    for (x0, x1), text, baseline, area in zip(spans, texts, baselines, areas):
        centre = text_centre(image, x0, x1)
        renderer = style.renderer()
        if fit:
            renderer, baseline, placed = fit_player_label(style, text, centre, baseline, area, image.shape[0])
            if report is not None:
                report.append((relative, text, area) + placed)
        out = draw_player_label(out, renderer, text, centre, baseline)
    return from_premultiplied(out), spans


# ---- the back arrow -------------------------------------------------------------------------------

ARROW = "interface/arrow_button.png"
ARROW_TEXT = (28, 61, 16, 109)      # rows y0..y1, columns x0..x1 around "Voltar"; its opaque part
                                    # is flat white body and the word, nothing else
ARROW_BASELINE = 50
ARROW_MIDDLE = 44.0                 # the white body's middle row (rows 24-63), and "Voltar"'s


def arrow_image(word, styles, report=None, by_ink=False):
    """The arrow with `word` on it. The word is centred on "Voltar"'s columns by Pillow's text box, as
    the English art always was, or with by_ink by its drawn ink (a brush or Ruqaa word's box is
    lopsided), which also centres a `centred` style's ink rows on the body's middle."""
    image = rgba(ARROW)
    y0, y1, x0, x1 = ARROW_TEXT
    region = np.zeros(image.shape[:2], bool)
    region[y0:y1, x0:x1] = True
    body = region & (image[..., 3] >= 1.0)
    ink = body & (image[..., 0] < 0.98)
    out = image.copy()
    out[body, :3] = 1.0
    height, width = image.shape[:2]
    columns = np.nonzero(ink.any(axis=0))[0]
    centre = (columns.min() + columns.max() + 1) / 2.0
    tried = []
    for style in styles:
        missing = style.face.missing(word)
        if missing:
            tried.append(f"{style.face} lacks {''.join(missing)!r}")
            continue
        size = style.size
        while size >= style.smallest:
            left, top, right, bottom = plain_box(style.face, size, word, style.layout, style.stroke)
            pen = centre - (left + right) / 2.0
            baseline = ARROW_MIDDLE - (top + bottom) / 2.0 if style.centred else ARROW_BASELINE
            coverage = plain_text(style.face, size, word, pen, baseline, width, height, layout=style.layout,
                                  stroke=style.stroke)
            if by_ink:
                ink_left, ink_right, ink_top, ink_bottom = ink_bounds(coverage)
                pen += centre - (ink_left + ink_right) / 2.0
                if style.centred:
                    baseline += ARROW_MIDDLE - (ink_top + ink_bottom) / 2.0
                coverage = plain_text(style.face, size, word, pen, baseline, width, height, layout=style.layout,
                                      stroke=style.stroke)
            if not ((coverage > 0) & ~body).any():
                if report is not None:
                    report.append((word, str(style.face), size, ink_bounds(coverage)))
                out[..., :3] *= (1.0 - coverage)[..., None]
                return Image.fromarray(to_byte(out), "RGBA")
            size -= 0.5
        tried.append(f"{style.face} does not fit inside the arrow's white body even at size {style.smallest}")
    raise Misfit(f"the arrow's word {word!r}: " + "; ".join(tried))


# ---- the logo (English only) ---------------------------------------------------------------------------

LOGO = "entities/gamelogo.png"
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


def logo_image(subtitle):
    image = rgba(LOGO)
    letters, glow = logo_layers(image)
    height, width = letters.shape
    # The subtitle is every letter shape that starts below "Penumbra" and right of its P's tail.
    old = np.zeros_like(letters, bool)
    for ys, xs in components(letters > 0.02):
        if ys.min() >= 94 and xs.min() >= 60:
            old[ys, xs] = True
    left, right = plain_width(MATURA, LOGO_SIZE, subtitle)
    pen = (LOGO_SPAN[0] + LOGO_SPAN[1]) / 2.0 - (left + right) / 2.0
    new = plain_text(MATURA, LOGO_SIZE, subtitle, pen, LOGO_BASELINE, width, height)
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


# ---- the words, per language -------------------------------------------------------------------------

def nfc(text):
    return unicodedata.normalize("NFC", text)


def problem(value):
    """Why `value` cannot be drawn, or None."""
    if not isinstance(value, str) or not value.strip():
        return "empty"
    if value != value.strip():
        return "starts or ends with a space"
    if any(unicodedata.category(c) == "Cc" for c in value):
        return "holds a control character"
    return None


class Words:
    """What one language's images say."""

    def __init__(self, menu, back, players, logo=None):
        self.menu, self.back, self.players, self.logo = menu, back, players, logo


def english_words():
    images = json.loads(STRINGS_JSON.read_text(encoding="utf-8"))["images"]
    labels = {nfc(k): v for k, v in images[MENU_SHEET]["labels"].items()}
    players = {}
    for relative, pt, _ in PLAYER_LABELS:
        table = {nfc(k): v for k, v in images[relative]["labels"].items()}
        players[relative] = [table[key] for key in pt]
    logo = {nfc(k): v for k, v in images[LOGO]["labels"].items()}
    return Words([labels[nfc(pt)] for _, pt, _, _ in MENU_FRAMES],
                 images[ARROW]["labels"]["Voltar"], players, logo["e o Castelo das Sombras"])


def language_words(language, strings_dir):
    """(Words, None), or (None, what is missing) when the "art" section is not complete."""
    path = strings_dir / f"{language}.json"
    if not path.is_file():
        return None, f"no {path}"
    art = json.loads(path.read_text(encoding="utf-8")).get("art")
    if not isinstance(art, dict):
        return None, "no \"art\" section"
    labels = {nfc(k): v for k, v in (art.get("labels") or {}).items()}
    wanted = [(f"labels[{pt}]", labels.get(nfc(pt))) for _, pt, _, _ in MENU_FRAMES]
    wanted += [(key, art.get(key)) for key in ("back", "player1", "player2")]
    bad = [f"{key} {problem(value)}" for key, value in wanted if problem(value)]
    if bad:
        return None, "; ".join(bad)
    values = [value for _, value in wanted]
    players = {relative: values[8:10] for relative, _, _ in PLAYER_LABELS}
    return Words(values[:7], values[7], players), None


# ---- output -----------------------------------------------------------------------------------------

def render_language(language, words):
    """{relative path: image} for one language, the fit report, and every misfit (a language with
    any is not written)."""
    style = SCRIPTS[SCRIPT_OF[language]]
    report = {"menu": [], "player": [], "arrow": []}
    misfits = []
    for role, texts in (("menu", words.menu), ("player", [t for ts in words.players.values() for t in ts])):
        face = style[role].face
        for text in texts:
            missing = face.missing(text)
            if missing:
                misfits.append(f"{face} has no glyph for {''.join(missing)!r} in {text!r}")
    if misfits:
        return {}, report, misfits
    images = {}
    mask = menu_mask(words.menu, style["menu"], report=report["menu"], misfits=misfits)
    check_menu_sheet(mask)
    images.update(menu_images(mask))
    for relative, _, _ in PLAYER_LABELS:
        try:
            images[relative] = player_image(relative, words.players[relative], style["player"],
                                            report=report["player"])[0]
        except Misfit as misfit:
            misfits.append(f"{pathlib.PurePosixPath(relative).name}: {misfit}")
    try:
        images[ARROW] = arrow_image(words.back, style["arrow"], report["arrow"], by_ink=language != "en")
    except Misfit as misfit:
        misfits.append(str(misfit))
    if words.logo is not None:
        images[LOGO] = logo_image(words.logo)[0]
    return images, report, misfits


def print_report(language, report):
    """Where each word's ink landed (first and last inked pixel) against its limits."""
    print(f"[{language}] fit (inked columns and rows, first-last, against the limits)")
    for frame, text, left, right, top, bottom, squeeze, vertical, shift, limit in report["menu"]:
        notes = [f"condensed {squeeze:.3f}x"] if squeeze != 1.0 else []
        notes += [f"moved {shift:+d} px"] if shift else []
        notes += [f"height {vertical:.2f}x"] if vertical != 1.0 else []
        print(f"  menu {frame} {text!r}: columns {left:.0f}-{right - 1:.0f} ({limit}), frame rows {top:.0f}-"
              f"{bottom - 1:.0f} (0-{MENU_FRAME_HEIGHT - 1})" + ("  " + ", ".join(notes) if notes else ""))
    for relative, text, area, left, right, top, bottom, squeeze, scale in report["player"]:
        notes = [f"condensed {squeeze:.3f}x"] if squeeze != 1.0 else []
        notes += [f"size {scale:.2f}x"] if scale != 1.0 else []
        print(f"  {pathlib.PurePosixPath(relative).name} {text!r}: columns {left:.0f}-{right - 1:.0f} "
              f"(area {area[0]}-{area[1] - 1}), rows {top:.0f}-{bottom - 1:.0f} ({LABEL_ROWS[0]}-{LABEL_ROWS[1] - 1})"
              + ("  " + ", ".join(notes) if notes else ""))
    for word, face, size, box in report["arrow"]:
        print(f"  arrow {word!r}: {face} size {size:g}, columns {box[0]:.0f}-{box[1] - 1:.0f}, rows {box[2]:.0f}-"
              f"{box[3] - 1:.0f} (inside the opaque white of rows {ARROW_TEXT[0]}-{ARROW_TEXT[1] - 1}, columns "
              f"{ARROW_TEXT[2]}-{ARROW_TEXT[3] - 1})")


def save(out_dir, language, relative, image):
    original = load(relative)
    if image.size != original.size:
        fail(f"{relative} came out {image.size}, the original is {original.size}")
    path = out_dir / language / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, optimize=True)
    shown = path.relative_to(REPO).as_posix() if path.is_relative_to(REPO) else path.as_posix()
    print(f"wrote {shown}  {image.size[0]}x{image.size[1]} {image.mode}")


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
    maps = menu_images(menu_mask([pt for _, pt, _, _ in MENU_FRAMES], fit=False))
    g = np.asarray(maps[MENU_GLOSS])
    print(f"   gloss  mean |err| {mean_abs(g, gloss * 255):.3f}/255")
    print(f"   alpha  mean |err| {mean_abs(np.asarray(maps[MENU_SHEET])[..., 3], alpha):.3f}/255")
    print(f"   normal mean |err| {mean_abs(np.asarray(maps[MENU_NORMAL]), normal):.3f}/255")
    ink, ours = gloss > 0.5, g > 127
    print(f"   glyph overlap (IoU at half coverage) {float((ink & ours).sum() / (ink | ours).sum()):.3f}")
    print("3. \"Jogador N\" erased and redrawn with the fitted font and shadow (premultiplied RGBA, label boxes):")
    for relative, pt, _ in PLAYER_LABELS:
        redrawn, spans = player_image(relative, pt, fit=False)
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
    logo, weight = logo_image(english_words().logo)
    original = np.asarray(load(LOGO, "RGBA"), np.int32)
    diff = np.abs(np.asarray(logo, np.int32) - original).max(axis=-1)
    print(f"   {int((weight <= 0.0).sum())} pixels outside it, of which {int((diff[weight <= 0.0] > 1).sum())} "
          f"differ by more than 1/255")
    letters, glow = logo_layers(rgba(LOGO))
    around = letters < 0.02
    print(f"   the logo's glow model against the original glow: mean |err| "
          f"{mean_abs(logo_glow(letters)[around], glow[around]) * 255:.2f}/255")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--lang", help="comma-separated ids (default: en and every language whose "
                                       "strings/<id>.json has a complete \"art\" section)")
    parser.add_argument("--strings-dir", type=pathlib.Path, default=STRINGS_DIR,
                        help="where the <id>.json language files are")
    parser.add_argument("--out-dir", type=pathlib.Path, default=IMAGES, help="writes OUT_DIR/<id>/<path>")
    parser.add_argument("--fonts-dir", type=pathlib.Path, default=FONT_CACHE,
                        help="where the pinned OFL faces are kept (fetched when missing)")
    parser.add_argument("--check", action="store_true", help="render and fit-check, write nothing")
    parser.add_argument("--verify", action="store_true", help="measure the models, write nothing")
    args = parser.parse_args(argv)
    if not (ORIGINAL / MENU_SHEET).is_file():
        fail(f"the original is not at {ORIGINAL}")
    Face.directory = args.fonts_dir.resolve()
    if args.verify:
        verify()
        return
    if args.lang:
        languages = [code.strip() for code in args.lang.split(",") if code.strip()]
        unknown = [code for code in languages if code not in SCRIPT_OF]
        if unknown:
            fail(f"unknown language {', '.join(unknown)} (known: {', '.join(LANGUAGES)})")
    else:
        languages = LANGUAGES
    work = []
    for language in languages:
        if language == "en":
            work.append((language, english_words()))
            continue
        words, missing = language_words(language, args.strings_dir)
        if words is None:
            if args.lang:
                fail(f"{language}: the \"art\" section is not complete: {missing}")
            print(f"[{language}] skipped: {missing}")
            continue
        work.append((language, words))
    if any(SCRIPT_OF[language] == "arabic" for language, _ in work) and not features.check("raqm"):
        fail("Pillow has no raqm here, so Arabic would come out unshaped and left to right")
    out_dir = args.out_dir.resolve()
    if args.check:
        print_rooms()
    failed = []
    for language, words in work:
        images, report, misfits = render_language(language, words)
        print_report(language, report)
        if misfits:
            failed.append(language)
            print(f"[{language}] DOES NOT FIT, nothing written for it:")
            for misfit in misfits:
                print(f"  - {misfit}")
            continue
        if args.check:
            continue
        for relative, image in images.items():
            save(out_dir, language, relative, image)
    if failed:
        fail(f"{', '.join(failed)} did not fit (see above); the others were "
             + ("checked" if args.check else "written"))


if __name__ == "__main__":
    main()
