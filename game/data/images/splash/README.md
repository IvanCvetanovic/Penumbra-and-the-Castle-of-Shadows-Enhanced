# The Supersonic Engine's logo (E35)

`supersonic-logo.png` is the picture the game shows for two seconds when it starts: the logo of the
Supersonic Engine, which this edition runs on. `game/render/Splash.hpp` says how it is placed and
faded; `game/PenumbraLayer.cpp` draws it before the first menu frame.

| | |
|---|---|
| Size | 2258 x 640, RGB (no alpha), 91,721 bytes |
| Source | `engine/assets/branding/supersonic-logo.svg` (viewBox 1200 x 320), the engine's own logo, drawn at 2x from a copy with three edits (below) |
| Ground | `#14171C` (20, 23, 28): the fill of the logo's own panel. The canvas is this colour all round the lockup, and the intro's flat background is the same colour (`kSplashGround` in `Splash.hpp`), so the picture's edge cannot be seen and the lockup floats on the screen |
| Lockup | The mark, SUPERSONIC, ENGINE and the rule (every pixel more than 30 levels off the ground) fill x 160 to 2096 and y 143 to 496: 160 and 161 px of ground to the left and right (the width is even, so they differ by one) and 143 above and below. The lockup is centred in the picture, so centring the picture centres it |
| Terms | The author's own work, **MIT-0**, like the engine (`engine/LICENSE`, which covers its branding). It is not the original game's art and not Magic Rampage's |
| Magenta | none: not one pixel is exactly `#FF00FF`, which the HUD's sprite loader keys out. The intro loads it with `TextureVariant::Plain`, which keys nothing |

## How it was made

One script, run from the repository's root with Pillow installed and Microsoft Edge in its usual place
(the wordmark is set in the system's Arial Bold, as the SVG's font list picks it on Windows). It edits a
copy of the engine's SVG (the engine's file is not touched), has Edge draw it at 2x, and writes the
picture. The three edits:

- The four faint vertical bands (the schlieren fringes, the SVG's `<g fill="#35D6E8" opacity="0.045">`)
  are removed. They run the panel's whole height, so on a flat ground they would end in square edges at
  the picture's top and bottom.
- The panel's `rx="28"` is removed. It is the same colour as the ground, and a square panel leaves no
  anti-aliased pixel on the picture's border once the canvas is wider than the panel.
- The rule under the wordmark gets a mask that fades it from x 984 (where the wordmark ends) to nothing
  at x 1040. The rule's own gradient fades over a long way to the right of the wordmark, and cut at the
  picture's edge it would end in a step; this way the lockup ends with the wordmark.

Then the canvas: 2258 x 640 of ground, with Edge's 2400 x 640 drawing pasted 99 px from its left edge, so
the picture is the drawing from x -99 to x 2159. The paste puts the lockup (x 61 to 1997 in the drawing)
160 px from the left and right of the canvas, and keeps the drawing's own vertical placement.

```python
import pathlib
import re
import subprocess
import tempfile

from PIL import Image

EDGE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
GROUND = (20, 23, 28)  # #14171C
work = pathlib.Path(tempfile.mkdtemp())

svg = pathlib.Path("engine/assets/branding/supersonic-logo.svg").read_text(encoding="utf-8")
svg, n = re.subn(r'\s*<g fill="#35D6E8" opacity="0\.045">.*?</g>', "", svg, flags=re.S)  # the bands
assert n == 1 and ' rx="28"' in svg
svg = svg.replace(' rx="28"', "")  # a square panel
fade = ('<linearGradient id="g-fade" gradientUnits="userSpaceOnUse" x1="984" y1="0" x2="1040" y2="0">'
        '<stop offset="0" stop-color="#fff"/><stop offset="1" stop-color="#000"/></linearGradient>'
        '<mask id="m-fade" maskUnits="userSpaceOnUse" x="0" y="0" width="1200" height="320">'
        '<rect width="1200" height="320" fill="url(#g-fade)"/></mask>')
rule = '<path d="M 334 189 H 1160" stroke="url(#l-rule)" stroke-width="3"/>'
assert rule in svg
svg = svg.replace("</defs>", fade + "</defs>", 1).replace(rule, rule[:-2] + ' mask="url(#m-fade)"/>')
(work / "logo.svg").write_text(svg, encoding="utf-8")
(work / "logo.html").write_text(
    '<!doctype html><html><head><meta charset="utf-8"><style>'
    "html,body{margin:0;padding:0;background:#14171C;overflow:hidden}"
    "img{display:block;width:2400px;height:640px}</style></head><body>"
    '<img src="%s" width="2400" height="640"></body></html>' % (work / "logo.svg").as_uri(),
    encoding="utf-8")

subprocess.run([EDGE, "--headless=new", "--disable-gpu", "--hide-scrollbars",
                "--force-device-scale-factor=1", "--force-color-profile=srgb",
                "--default-background-color=FF14171C", "--screenshot=" + str(work / "raw.png"),
                "--window-size=2400,640", (work / "logo.html").as_uri()], check=True)

raw = Image.open(work / "raw.png").convert("RGB")
logo = Image.new("RGB", (2258, 640), GROUND)
logo.paste(raw, (99, 0))
logo.save("game/data/images/splash/supersonic-logo.png", optimize=True)
```

Checked: exactly 2258 x 640; every pixel of the picture's border is (20, 23, 28); the lockup (pixels more
than 30 levels off the ground) spans x 160 to 2096 and y 143 to 496; no pixel is `#FF00FF`; only the
chunks `IHDR`, `IDAT` and `IEND` (no colour profile, no gamma).

## Where it travels

Every package copies `game/data` whole (`tools/package.bat`, `tools/make_release.py`,
`tools/android_package.py`, `tools/apple/make_app.sh`), so this folder is in the Windows zip, the Linux
tar.gz, the APK's `assets/data/`, and the Mac and iPhone/iPad bundles. A game that cannot find the file
starts without the intro and says so in its log.
