"""The two script faces of ENHANCEMENT E24: game/data/fonts/NotoSansJP-Bold.ttf and NotoSansArabic-Bold.ttf.

    python tools/l10n/make_fonts.py [--joyo] [--cache DIR]

No face the original's scripts name (Arial Narrow, Arial, Arial Black, Verdana, or their stand-ins) has Japanese,
and only Arial Bold has Arabic, so game/render/FontAtlas routes those scripts to these two, the same on every
platform. They are made from Google's Noto releases as the google/fonts repository carries them, at a pinned
commit, each download checked against its sha256:

  - Noto Sans JP (variable, wght 100-900), subset to what the game draws in Japanese - ASCII and Latin-1, the CJK
    symbols and punctuation, hiragana, katakana, the half- and full-width forms, every character of
    game/data/strings/ja.json and every other language file's characters that go to this face, and the language
    names of strings.json - then instanced at wght 700 (the original drew every face bold; stb_truetype has no
    variations and would draw the default instance, Thin);
  - Noto Sans Arabic (variable, wdth and wght), subset to the whole Arabic blocks (U+0600-06FF, 0750-077F,
    FB50-FDFF, FE70-FEFF: the Presentation Forms are what render/ArabicShaping turns letters into) and ASCII, then
    instanced at wght 700, wdth 100.

The layout tables go (GSUB, GPOS, GDEF: stb_truetype reads none of them; the shaping is the game's own) and so does
hinting (stb_truetype ignores it). A subset and instance is an OFL "Modified Version": each file keeps its name
(neither uses a Reserved Font Name), and its OFL text goes beside it as LICENSE-<name>.txt. The timestamps are
left as upstream set them, so a rerun on the same inputs writes the same bytes.

Rerun whenever game/data/strings/ja.json changes: test_pn_render_hud's font coverage check fails for a character
the Japanese face lacks. --joyo also keeps the 2,136 joyo kanji (Unicode's Unihan kJoyoKanji, 16.0.0, with the
four common variants it maps), for a provisional face before ja.json is written.

Needs fontTools (pip install fonttools); the Windows Python used for tools/art has it.
"""
import argparse
import hashlib
import io
import json
import os
import sys
import tempfile
import urllib.request
import zipfile

from fontTools import subset
from fontTools import version as fonttools_version
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DATA = os.path.join(REPO, "game", "data")
FONTS = os.path.join(DATA, "fonts")

COMMIT = "66a36c8c94b1a5d992ee4e7f392fccfe4945767c"   # google/fonts, 2026-03-25
RAW = f"https://raw.githubusercontent.com/google/fonts/{COMMIT}"
SOURCES = {
    "jp": {
        "font": (f"{RAW}/ofl/notosansjp/NotoSansJP%5Bwght%5D.ttf",
                 "c2f3b4d463500a2ddcd3849cded1fceeb9fd6d1c32e6cbecd568453ba50fc68f"),
        "ofl": (f"{RAW}/ofl/notosansjp/OFL.txt", "1c05c68c34f9708415aada51f17e1b0092d2cea709bf4a94cd38114f9e73d7d9"),
        "upstream": "ofl/notosansjp/NotoSansJP[wght].ttf",
        "out": "NotoSansJP-Bold.ttf",
        "license": "LICENSE-NotoSansJP.txt",
        "location": {"wght": 700},
    },
    "ar": {
        "font": (f"{RAW}/ofl/notosansarabic/NotoSansArabic%5Bwdth,wght%5D.ttf",
                 "63111b5b2e074dd48cc67692e0a2726d86ee94c1c37fe8598257b7b4e87e869e"),
        "ofl": (f"{RAW}/ofl/notosansarabic/OFL.txt", "07fc70bfeb985cc1a87a8587d0a0c80bab11c86c9dc3fd95b6f0cb332f983e96"),
        "upstream": "ofl/notosansarabic/NotoSansArabic[wdth,wght].ttf",
        "out": "NotoSansArabic-Bold.ttf",
        "license": "LICENSE-NotoSansArabic.txt",
        "location": {"wght": 700, "wdth": 100},
    },
}
UNIHAN = ("https://www.unicode.org/Public/16.0.0/ucd/Unihan.zip",
          "b8f000df69de7828d21326a2ffea462b04bc7560022989f7cc704f10521ef3e0")

BASIC = list(range(0x20, 0x7F)) + list(range(0xA0, 0x100))
JP_BLOCKS = list(range(0x3000, 0x3100)) + list(range(0x31F0, 0x3200)) + list(range(0xFF00, 0xFFF0))
AR_BLOCKS = (list(range(0x0600, 0x0700)) + list(range(0x0750, 0x0780)) + list(range(0xFB50, 0xFE00)) +
             list(range(0xFE70, 0xFF00)))

README_BEGIN = "<!-- make_fonts.py: begin -->"
README_END = "<!-- make_fonts.py: end -->"


def is_japanese(cp):
    """game/render/FontAtlas.cpp ScriptOf: what goes to the Japanese face."""
    return (0x3000 <= cp <= 0x30FF or 0x31F0 <= cp <= 0x31FF or 0x3400 <= cp <= 0x4DBF or 0x4E00 <= cp <= 0x9FFF
            or 0xFF00 <= cp <= 0xFFEF)


def fetch(url, sha256, cache):
    name = hashlib.sha256(url.encode()).hexdigest()[:16] + "-" + os.path.basename(url).replace("%5B", "[").replace(
        "%5D", "]")
    path = os.path.join(cache, name)
    if not os.path.exists(path):
        print(f"downloading {url}")
        with urllib.request.urlopen(url, timeout=600) as response:
            data = response.read()
        with open(path, "wb") as f:
            f.write(data)
    data = open(path, "rb").read()
    digest = hashlib.sha256(data).hexdigest()
    if digest != sha256:
        sys.exit(f"{url}: sha256 {digest}, expected {sha256}")
    return data


def strings_of(value, out):
    if isinstance(value, str):
        out.append(value)
    elif isinstance(value, dict):
        for k, v in value.items():
            if not k.startswith("_"):
                strings_of(v, out)
    elif isinstance(value, list):
        for v in value:
            strings_of(v, out)


def japanese_characters():
    """Every character that goes to the Japanese face in any language file, and the language names."""
    texts = []
    base = json.load(open(os.path.join(DATA, "strings.json"), encoding="utf-8"))
    strings_of([entry.get("name", "") for entry in base.get("languages", [])], texts)
    folder = os.path.join(DATA, "strings")
    files = sorted(f for f in os.listdir(folder) if f.endswith(".json")) if os.path.isdir(folder) else []
    for name in files:
        doc = json.load(open(os.path.join(folder, name), encoding="utf-8"))
        # "art" is the image tool's, drawn into pictures, not by the game.
        strings_of({k: v for k, v in doc.items() if k != "art"}, texts)
    chars = {ord(c) for text in texts for c in text if is_japanese(ord(c))}
    return chars, files


def joyo_kanji(cache):
    data = fetch(*UNIHAN, cache)
    kanji = set()
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        for line in archive.read("Unihan_OtherMappings.txt").decode("utf-8").splitlines():
            parts = line.split("\t")
            if len(parts) == 3 and parts[1] == "kJoyoKanji":
                kanji.add(int(parts[0][2:], 16))
    if len(kanji) < 2136:
        sys.exit(f"Unihan lists {len(kanji)} joyo kanji, expected at least 2136")
    return kanji


def build(key, unicodes, required, cache):
    source = SOURCES[key]
    raw = fetch(*source["font"], cache)
    font = TTFont(io.BytesIO(raw), recalcTimestamp=False)
    options = subset.Options()
    options.layout_features = []
    options.drop_tables += ["GSUB", "GPOS", "GDEF", "BASE", "JSTF", "MATH", "DSIG"]
    options.hinting = False
    options.notdef_outline = True
    options.name_IDs = ["*"]
    options.name_legacy = True
    options.recalc_timestamp = False
    subsetter = subset.Subsetter(options)
    subsetter.populate(unicodes=sorted(unicodes))
    subsetter.subset(font)
    # Named for the instance ("Noto Sans JP Bold"), not the default one (Thin), from the STAT table.
    font = instancer.instantiateVariableFont(font, source["location"], updateFontNames=True)
    font.recalcTimestamp = False
    out = os.path.join(FONTS, source["out"])
    font.save(out)
    check = TTFont(out)
    cmap = check.getBestCmap()
    missing = sorted(cp for cp in required if cp not in cmap)
    if "fvar" in check:
        sys.exit(f"{out}: still variable")
    ofl = fetch(*source["ofl"], cache)
    with open(os.path.join(FONTS, source["license"]), "wb") as f:
        f.write(ofl)
    data = open(out, "rb").read()
    return {
        "file": source["out"],
        "license": source["license"],
        "upstream": source["upstream"],
        "upstream_sha256": source["font"][1],
        "sha256": hashlib.sha256(data).hexdigest(),
        "bytes": len(data),
        "glyphs": check["maxp"].numGlyphs,
        "mapped": len(cmap),
        "missing": missing,
        "location": ", ".join(f"{k} {v}" for k, v in source["location"].items()),
    }


def write_readme(results, notes):
    path = os.path.join(FONTS, "README.md")
    text = open(path, encoding="utf-8").read()
    rows = []
    for r in results:
        rows.append(f"| `{r['file']}` | google/fonts `{r['upstream']}` at commit `{COMMIT}` (sha256 "
                    f"`{r['upstream_sha256']}`) | subset + instance ({r['location']}): {r['mapped']} characters, "
                    f"{r['glyphs']} glyphs, {r['bytes']:,} bytes, sha256 `{r['sha256']}` | SIL Open Font License 1.1 "
                    f"(`{r['license']}`) |")
    block = "\n".join([
        README_BEGIN,
        "## Script faces (ENHANCEMENT E24): Japanese and Arabic",
        "",
        "Written by `tools/l10n/make_fonts.py` (rerun it whenever `game/data/strings/ja.json` changes; this section is "
        "its own and is rewritten). `game/render/FontAtlas` draws hiragana, katakana, CJK ideographs, CJK punctuation "
        "and the full-width forms from the first, Arabic from the second, on every platform, scaled to the em of the "
        "line's face. Each is an OFL Modified Version of the upstream file - a subset, instanced at bold - under its "
        "own name (neither upstream declares a Reserved Font Name the file uses), with its licence beside it.",
        "",
        "| File | Source | Made | Licence |",
        "|---|---|---|---|",
        *rows,
        "",
        *notes,
        README_END,
    ])
    if README_BEGIN in text:
        before = text[:text.index(README_BEGIN)]
        after = text[text.index(README_END) + len(README_END):]
        text = before + block + after
    else:
        text = text.rstrip("\n") + "\n\n" + block + "\n"
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--joyo", action="store_true", help="also keep the 2,136 joyo kanji (a provisional face)")
    parser.add_argument("--cache", default=os.path.join(tempfile.gettempdir(), "penumbra-make-fonts"),
                        help="where the downloads are kept between runs")
    args = parser.parse_args(argv)
    os.makedirs(args.cache, exist_ok=True)

    japanese, files = japanese_characters()
    joyo = joyo_kanji(args.cache) if args.joyo else set()
    jp = set(BASIC) | set(JP_BLOCKS) | japanese | joyo
    ar = set(range(0x20, 0x7F)) | set(AR_BLOCKS)
    # What must come out mapped: the Japanese the game draws; the Arabic letters and every form they are shaped
    # into (render/ArabicShaping.cpp: U+0621-064A, Forms-B U+FE80-FEFC, alef maksura's FBE8-FBE9).
    ar_required = (set(range(0x0621, 0x063B)) | set(range(0x0640, 0x064B)) | set(range(0xFE80, 0xFEFD)) |
                   {0xFBE8, 0xFBE9})
    results = [build("jp", jp, japanese | joyo, args.cache), build("ar", ar, ar_required, args.cache)]
    for r in results:
        print(f"{r['file']}: {r['mapped']} characters, {r['glyphs']} glyphs, {r['bytes']:,} bytes, sha256 {r['sha256']}")
        if r["missing"]:
            print(f"  NOT in the upstream face: {' '.join(f'U+{cp:04X}' for cp in r['missing'][:40])}")
    kanji = sorted(cp for cp in japanese if 0x3400 <= cp <= 0x9FFF)
    notes = [
        f"Japanese characters from the language files ({', '.join(files) or 'none yet'}) and the language names: "
        f"{len(japanese)}, of them {len(kanji)} kanji" + (f"; plus the {len(joyo)} joyo kanji of Unihan 16.0.0 "
                                                           f"(`--joyo`, a provisional face)." if joyo else "."),
        f"Made with fontTools {fonttools_version}.",
    ]
    write_readme(results, notes)
    missing = [r for r in results if r["missing"]]
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
