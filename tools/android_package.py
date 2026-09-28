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
                     most), scaled to each launcher density

Everything is rebuilt from scratch each time; nothing here is committed.
"""

import hashlib
import os
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
