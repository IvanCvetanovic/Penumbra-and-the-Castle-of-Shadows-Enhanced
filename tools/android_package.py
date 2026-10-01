"""The file-shuffling half of tools/build_android.sh: staging the APK's assets
and icon, and putting the native libraries into the linked APK.

    python tools/android_package.py stage <stage dir>
    python tools/android_package.py addlibs <in.apk> <out.apk> <abi>=<libPenumbra.so> [...] [classes.dex=<file>]

`stage` writes <stage>/assets and <stage>/res from the repository:

  assets/original/   extracted/app, the original game (read, never written),
                     without its Windows executables and DLLs, which Android
                     cannot use and which are a fifth of its size
  assets/data/       game/data, the port's own files
  assets/engine/assets/
                     engine/assets, the engine's shaders
  assets/penumbra_assets.txt
                     a stamp, then every path above, one per line. The game
                     unpacks the list on first run and again whenever the stamp
                     changes (game/android/AndroidMain.cpp), because an APK
                     asset has no path the game's file code could open.
  res/mipmap-*/ic_launcher.png
                     the original's icon (extracted/app/penumbra.ico, 48 px at
                     most), scaled to each launcher density: the plain square
                     icon, which no device the APK installs on (Android 8 and
                     up) draws, an adaptive icon being there
  res/mipmap-anydpi-v26/ic_launcher.xml
                     the launcher icon an Android 8+ device draws: an adaptive
                     icon with the layers below. A square PNG alone is put on
                     a white plate; an adaptive icon needs a background of its
                     own, because the system draws the layers over black, so a
                     transparent one shows as a black disc
  res/drawable/ic_launcher_background.xml
                     that background: the deep violet gradient of the menus'
                     cave (#2A1536 at the top, #0F0716 at the bottom), the same
                     tile as the iOS icon (tools/apple/make_app.sh)
  res/mipmap-*/ic_launcher_foreground.png
                     the skull on a 108 dp layer, every visible pixel inside the
                     66 dp circle that no launcher mask cuts
  res/mipmap-*/ic_launcher_monochrome.png
                     the same placement as one shape (bone drawn, the dark
                     outline and the eye sockets not), which Android 13's themed
                     icons paint in the wallpaper's colours

Everything is rebuilt from scratch each time; nothing here is committed.
"""

import hashlib
import math
import os
import random
import shutil
import sys
import zipfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# The APK's asset folders and where each comes from.
SOURCES = [
    ("original", os.path.join(REPO, "extracted", "app")),
    ("data", os.path.join(REPO, "game", "data")),
    ("engine/assets", os.path.join(REPO, "engine", "assets")),
]
# Windows binaries of the 2010 game, and the engine's GLSL sources (the APK
# carries the compiled .spv only).
SKIPPED_EXTENSIONS = {".exe", ".dll", ".pdb", ".frag", ".vert", ".glsl"}

LAUNCHER_SIZES = {"mdpi": 48, "hdpi": 72, "xhdpi": 96, "xxhdpi": 144, "xxxhdpi": 192}

# The adaptive icon. A layer is 108 dp square; the launcher shows a mask (a
# circle, a squircle, a rounded square) of at most 72 dp across it, and 66 dp,
# the circle in the middle, is what every mask keeps whole.
LAYER_DP = 108
SAFE_DP = 66
LAYER_DENSITIES = {"mdpi": 1.0, "hdpi": 1.5, "xhdpi": 2.0, "xxhdpi": 3.0, "xxxhdpi": 4.0}
# The original's icon is a skull in a soft dark glow that fades out to the
# edge of its 48 px image. A pixel under this alpha (of 255) is that glow's
# last breath, invisible on any wallpaper, and is not what is fitted inside
# the safe circle (fitting it would draw the skull 7% smaller).
VISIBLE_ALPHA = 16
# The monochrome shape: the icon's luminance times its alpha (0..255) goes from
# not drawn at or under the first value to drawn from the second, smoothly.
# The dark outline and the eye sockets are near 0, the bone about 25 to 160.
MONOCHROME_RAMP = (18, 40)
# A blob of the shape smaller than this many source pixels is a speck of the
# glow (round the jaw, on the crown), not bone.
MONOCHROME_SPECK = 12

# The background layer. A transparent one is drawn over black by the system (an adaptive icon's
# layers are composed on black), so the skull gets a deliberate tile: the iOS icon's colours.
BACKGROUND_TOP = "#2A1536"
BACKGROUND_BOTTOM = "#0F0716"
BACKGROUND_XML = f"""<?xml version="1.0" encoding="utf-8"?>
<shape xmlns:android="http://schemas.android.com/apk/res/android" android:shape="rectangle">
    <gradient android:angle="270" android:startColor="{BACKGROUND_TOP}" android:endColor="{BACKGROUND_BOTTOM}" />
</shape>
"""

ADAPTIVE_XML = """<?xml version="1.0" encoding="utf-8"?>
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@drawable/ic_launcher_background" />
    <foreground android:drawable="@mipmap/ic_launcher_foreground" />
    <monochrome android:drawable="@mipmap/ic_launcher_monochrome" />
</adaptive-icon>
"""


def smallest_circle(points):
    """(x, y, radius) of the smallest circle holding every point: the incremental
    algorithm over the points in a fixed shuffled order, so the same icon always
    gives the same circle."""
    points = list(points)
    random.Random(7).shuffle(points)

    def through2(a, b):
        return (a[0] + b[0]) / 2, (a[1] + b[1]) / 2, math.hypot(a[0] - b[0], a[1] - b[1]) / 2

    def through3(a, b, c):
        d = 2 * (a[0] * (b[1] - c[1]) + b[0] * (c[1] - a[1]) + c[0] * (a[1] - b[1]))
        if abs(d) < 1e-12:   # in a line: the widest pair
            return max((through2(a, b), through2(a, c), through2(b, c)), key=lambda circle: circle[2])
        ux = ((a[0] ** 2 + a[1] ** 2) * (b[1] - c[1]) + (b[0] ** 2 + b[1] ** 2) * (c[1] - a[1])
              + (c[0] ** 2 + c[1] ** 2) * (a[1] - b[1])) / d
        uy = ((a[0] ** 2 + a[1] ** 2) * (c[0] - b[0]) + (b[0] ** 2 + b[1] ** 2) * (a[0] - c[0])
              + (c[0] ** 2 + c[1] ** 2) * (b[0] - a[0])) / d
        return ux, uy, math.hypot(a[0] - ux, a[1] - uy)

    x, y, r = points[0][0], points[0][1], 0.0

    def holds(p):
        return math.hypot(p[0] - x, p[1] - y) <= r + 1e-9

    for i, p in enumerate(points):
        if holds(p):
            continue
        x, y, r = p[0], p[1], 0.0
        for j in range(i):
            if holds(points[j]):
                continue
            x, y, r = through2(p, points[j])
            for k in range(j):
                if not holds(points[k]):
                    x, y, r = through3(p, points[j], points[k])
    return x, y, r


def without_specks(image, floor, smallest):
    """`image` (L) with every connected blob (8-way) of pixels at `floor` or
    more that is under `smallest` pixels taken out, and the faint pixels that
    ringed it with it."""
    from PIL import ImageChops, ImageFilter, Image

    width, height = image.size
    data = image.load()
    seen = set()
    keep = Image.new("L", image.size, 0)
    kept = keep.load()
    for y in range(height):
        for x in range(width):
            if data[x, y] < floor or (x, y) in seen:
                continue
            blob = [(x, y)]
            seen.add((x, y))
            for px, py in blob:   # the list grows as the blob does
                for ny in range(max(0, py - 1), min(height, py + 2)):
                    for nx in range(max(0, px - 1), min(width, px + 2)):
                        if data[nx, ny] >= floor and (nx, ny) not in seen:
                            seen.add((nx, ny))
                            blob.append((nx, ny))
            if len(blob) >= smallest:
                for px, py in blob:
                    kept[px, py] = 255
    # The kept blobs and one pixel round them (their soft edge), no more.
    return ImageChops.multiply(image, keep.filter(ImageFilter.MaxFilter(3)))


def adaptive_layers(icon, density):
    """The foreground and the monochrome layer (RGBA, 108 dp square at this
    density) of the skull in `icon` (RGBA): its smallest enclosing circle, over
    the pixels that are visible, put at the layer's centre and made as large as
    the safe circle allows. Each is resampled straight from the 48 px image
    (LANCZOS, as the plain icons are), box by box, so the placement is exact to
    a fraction of a pixel rather than to a whole one."""
    from PIL import Image, ImageChops

    width, height = icon.size
    alpha = icon.getchannel("A")
    data = alpha.load()
    corners = set()
    for y in range(height):
        for x in range(width):
            if data[x, y] >= VISIBLE_ALPHA:
                corners.update(((x, y), (x + 1, y), (x, y + 1), (x + 1, y + 1)))
    cx, cy, radius = smallest_circle(corners)
    # Layer pixels per source pixel: the skull's circle fills the safe circle
    # (the circle is drawn round the visible pixels' outer corners, which
    # leaves their resampled edge a little inside it).
    scale = (SAFE_DP / 2.0) * density / radius

    size = round(LAYER_DP * density)
    half = size / (2.0 * scale)   # source pixels from the centre to the layer's edge
    pad = int(math.ceil(half)) + 1
    box = (cx + pad - half, cy + pad - half, cx + pad + half, cy + pad + half)

    def resampled(image, fill):
        padded = Image.new(image.mode, (width + 2 * pad, height + 2 * pad), fill)
        padded.paste(image, (pad, pad))
        return padded.resize((size, size), Image.LANCZOS, box=box)

    foreground = resampled(icon, (0, 0, 0, 0))

    # Luminance times alpha: the bone, and none of the dark outline, the eye
    # sockets or the glow, less the lone bright specks the glow has round the
    # jaw. It is resampled as it is and thresholded softly at the layer's
    # size, which keeps the shape's edge a pixel wide rather than the eight or
    # so an upscaled hard threshold would leave.
    bone = without_specks(ImageChops.multiply(icon.convert("L"), alpha), sum(MONOCHROME_RAMP) // 2, MONOCHROME_SPECK)
    shape = resampled(bone, 0)
    low, high = MONOCHROME_RAMP

    def ramp(value):
        t = min(1.0, max(0.0, (value - low) / float(high - low)))
        return round(255 * t * t * (3 - 2 * t))

    shape = shape.point([ramp(value) for value in range(256)])
    # Only the alpha counts: a themed icon paints this layer in the wallpaper's
    # colours, whatever colour it has.
    black = Image.new("L", (size, size), 0)
    monochrome = Image.merge("RGBA", (black, black, black, shape))
    return foreground, monochrome


def check_inside_safe_circle(layer, density, name):
    """Every pixel of `layer` whose alpha is VISIBLE_ALPHA or more lies inside
    the safe circle (to a quarter of a pixel): a stage that would draw part of
    the skull where a launcher's mask can cut it stops here."""
    from PIL import Image, ImageChops, ImageDraw

    size = layer.width
    outside = Image.new("L", (size, size), 255)
    centre = size / 2.0
    r = SAFE_DP / 2.0 * density + 0.25
    ImageDraw.Draw(outside).ellipse((centre - r, centre - r, centre + r, centre + r), fill=0)
    visible = layer.getchannel("A").point([255 if value >= VISIBLE_ALPHA else 0 for value in range(256)])
    if ImageChops.multiply(visible, outside).getbbox() is not None:
        raise SystemExit(f"{name}: part of the skull lies outside the {SAFE_DP} dp safe circle at {density}x")


def adaptive_icon(out, icon):
    """res/mipmap-anydpi-v26/ic_launcher.xml and its layers, under `out`/res."""
    xml_folder = os.path.join(out, "res", "mipmap-anydpi-v26")
    os.makedirs(xml_folder, exist_ok=True)
    with open(os.path.join(xml_folder, "ic_launcher.xml"), "w", newline="\n", encoding="utf-8") as f:
        f.write(ADAPTIVE_XML)
    drawable_folder = os.path.join(out, "res", "drawable")
    os.makedirs(drawable_folder, exist_ok=True)
    with open(os.path.join(drawable_folder, "ic_launcher_background.xml"), "w", newline="\n", encoding="utf-8") as f:
        f.write(BACKGROUND_XML)
    for name, density in LAYER_DENSITIES.items():
        foreground, monochrome = adaptive_layers(icon, density)
        check_inside_safe_circle(foreground, density, "ic_launcher_foreground")
        check_inside_safe_circle(monochrome, density, "ic_launcher_monochrome")
        folder = os.path.join(out, "res", f"mipmap-{name}")
        os.makedirs(folder, exist_ok=True)
        foreground.save(os.path.join(folder, "ic_launcher_foreground.png"), optimize=True)
        monochrome.save(os.path.join(folder, "ic_launcher_monochrome.png"), optimize=True)


def stage(out):
    if os.path.isdir(out):
        shutil.rmtree(out)
    assets = os.path.join(out, "assets")
    os.makedirs(assets)

    listed = []
    digest = hashlib.sha1()
    for prefix, source in SOURCES:
        for root, dirs, files in os.walk(source):
            dirs.sort()
            for name in sorted(files):
                if os.path.splitext(name)[1].lower() in SKIPPED_EXTENSIONS:
                    continue
                src = os.path.join(root, name)
                rel = prefix + "/" + os.path.relpath(src, source).replace(os.sep, "/")
                dst = os.path.join(assets, rel.replace("/", os.sep))
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                shutil.copyfile(src, dst)
                listed.append(rel)
                with open(src, "rb") as f:
                    digest.update(rel.encode("utf-8") + b"\0" + f.read())

    stamp = digest.hexdigest()[:16]
    with open(os.path.join(assets, "penumbra_assets.txt"), "w", newline="\n") as f:
        f.write(stamp + "\n")
        for rel in listed:
            f.write(rel + "\n")
    print(f"staged {len(listed)} files, stamp {stamp}")

    from PIL import Image

    icon = Image.open(os.path.join(REPO, "extracted", "app", "penumbra.ico"))
    sizes = sorted(icon.info.get("sizes", {(icon.width, icon.height)}))
    icon.size = sizes[-1]
    icon = icon.convert("RGBA")
    for density, size in LAUNCHER_SIZES.items():
        folder = os.path.join(out, "res", f"mipmap-{density}")
        os.makedirs(folder, exist_ok=True)
        icon.resize((size, size), Image.LANCZOS).save(os.path.join(folder, "ic_launcher.png"))
    adaptive_icon(out, icon)


def addlibs(apk_in, apk_out, libs):
    # Rewritten entry by entry rather than appended to: an append leaves
    # aapt2's local headers and Python's central directory disagreeing, which
    # zipalign then reports once per entry. Each entry keeps its compression
    # (aapt2 stores PNGs and audio uncompressed on purpose).
    with zipfile.ZipFile(apk_in) as src, zipfile.ZipFile(apk_out, "w") as apk:
        for info in src.infolist():
            copy = zipfile.ZipInfo(info.filename, date_time=info.date_time)
            copy.compress_type = info.compress_type
            copy.external_attr = info.external_attr
            apk.writestr(copy, src.read(info.filename))
        for spec in libs:
            # <abi>=<libPenumbra.so>, or classes.dex=<file> for the Java side.
            key, path = spec.split("=", 1)
            name = key if key.endswith(".dex") else f"lib/{key}/libPenumbra.so"
            apk.write(path, name, compress_type=zipfile.ZIP_DEFLATED)
            print(f"added {name} ({os.path.getsize(path) // 1024} KB)")


def main(argv):
    if len(argv) >= 2 and argv[0] == "stage":
        stage(argv[1])
    elif len(argv) >= 4 and argv[0] == "addlibs":
        addlibs(argv[1], argv[2], argv[3:])
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
