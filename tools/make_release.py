"""The files a player downloads from a GitHub release, made from what is built.

    python tools/make_release.py windows    out/release/Penumbra-Windows.zip
    python tools/make_release.py linux <build dir>
                                            out/release/Penumbra-Linux.tar.gz
    python tools/make_release.py sums <dir> <file>...
                                            <dir>/SHA256SUMS.txt over those files
    python tools/make_release.py check      the version check alone

`windows` takes the packaged game tools/package.bat leaves in
out/package/Penumbra (tools/make_release.bat runs it first) and writes:

  out/release/Penumbra-Windows/        the release as a folder:
      HOW TO PLAY.txt                  game/windows/how-to-play.txt, English and
                                       Portuguese, CRLF with a UTF-8 BOM so every
                                       Notepad shows the accents
      Penumbra/                        the package: Penumbra.exe and everything
                                       beside it
  out/release/Penumbra-Windows.zip     that folder zipped: "HOW TO PLAY.txt" and
                                       "Penumbra/" at its top, entries in sorted
                                       order with one fixed timestamp (the HEAD
                                       commit's, or SOURCE_DATE_EPOCH), so the
                                       same files always make the same zip
  out/release/SHA256SUMS.txt           the zip's and, when it is there, the
                                       Android APK's (tools/build_android.sh
                                       --release) checksums

The folder is what CI uploads for code signing (docs/code-signing.md): signed,
it zips into the same layout.

`linux` lays a Linux build out as package.bat lays out Windows' (Penumbra with
assets/shaders, data/ and original/ beside it, the README, docs and licences)
in out/release/Penumbra-Linux/, with game/linux/how-to-play.txt beside it as
"HOW TO PLAY.txt", and writes out/release/Penumbra-Linux.tar.gz: a tar keeps
the executable bit a zip would lose. Entries sorted, one fixed time, owner 0,
0755 for the program and the folders and 0644 for the rest, and a gzip header
with no name or time, so the same files always make the same archive. The
release workflow (.github/workflows/release.yml) runs it in an Ubuntu 22.04
container; the Mac and iPhone files are made on a Mac (tools/apple/make_release.sh).

`sums` writes SHA256SUMS.txt for the files named, in the order named: the
release workflow names the two files already published first, so their lines
stay as they were.

It refuses a package without the Visual C++ runtime beside Penumbra.exe
(package.bat only warns): a player without the Redistributable could not start
the game at all, and Windows would say so in words about DLLs. And it refuses
a release whose version differs between the top-level CMakeLists.txt (which the
Windows resources take), the Windows manifest, the Android manifest and the Mac
and iPhone Info.plists.

Nothing here builds, runs the game, signs or uploads anything.
"""

import hashlib
import os
import re
import shutil
import gzip
import subprocess
import sys
import tarfile
import time
import zipfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PACKAGE = os.path.join(REPO, "out", "package", "Penumbra")
RELEASE = os.path.join(REPO, "out", "release")
STAGE = os.path.join(RELEASE, "Penumbra-Windows")
ZIP = os.path.join(RELEASE, "Penumbra-Windows.zip")
APK = os.path.join(RELEASE, "Penumbra-Android.apk")
HOW_TO_PLAY = os.path.join(REPO, "game", "windows", "how-to-play.txt")
LINUX_STAGE = os.path.join(RELEASE, "Penumbra-Linux")
TARBALL = os.path.join(RELEASE, "Penumbra-Linux.tar.gz")
LINUX_HOW_TO_PLAY = os.path.join(REPO, "game", "linux", "how-to-play.txt")

# What package.bat leaves out of extracted/app (robocopy /XF, which ignores case).
ORIGINAL_EXCLUDED_SUFFIXES = (".exe", ".dll", ".as", ".cg", ".ethproj")
ORIGINAL_EXCLUDED_NAMES = ("readme.txt",)

# package.bat's own check list, with the Linux program's name.
LINUX_MARKERS = ("Penumbra", "assets/shaders/frag.spv", "assets/shaders/screen_overlay_frag.spv",
                 "data/strings.json", "data/images/en/entities/menu_buttons.png", "data/touch_controls.json",
                 "data/images/touch/jump.png", "data/images/splash/supersonic-logo.png",
                 "data/fonts/LiberationSans-Bold.ttf", "original/data.enml", "original/hs.enml",
                 "original/data/shadow.dds", "original/scenes/menu.esc",
                 "original/scenes/level1.esc", "original/soundfx/chefao.mp3", "original/entities/menu_buttons.png",
                 "original/penumbra.ico")

# The Visual C++ runtime Penumbra.exe imports (dumpbin /dependents), app-local.
RUNTIME = ("msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll")


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def check_versions():
    cmake = read(os.path.join(REPO, "CMakeLists.txt"))
    found = re.search(r"^project\(Penumbra VERSION (\d+\.\d+\.\d+)\b", cmake, re.M)
    if not found:
        sys.exit("CMakeLists.txt: no project(Penumbra VERSION x.y.z ...)")
    version = found.group(1)
    windows = read(os.path.join(REPO, "game", "windows", "Penumbra.manifest"))
    android = read(os.path.join(REPO, "game", "android", "AndroidManifest.xml"))
    problems = []
    if f'version="{version}.0"' not in windows:
        problems.append(f"game/windows/Penumbra.manifest: assemblyIdentity version is not {version}.0")
    if f'android:versionName="{version}"' not in android:
        problems.append(f"game/android/AndroidManifest.xml: versionName is not {version}")
    for platform in ("macos", "ios"):
        plist = read(os.path.join(REPO, "game", platform, "Info.plist"))
        short = re.search(r"<key>CFBundleShortVersionString</key>\s*<string>([^<]*)</string>", plist)
        if not short or short.group(1) != version:
            problems.append(f"game/{platform}/Info.plist: CFBundleShortVersionString is not {version}")
    if problems:
        sys.exit("version mismatch:\n  " + "\n  ".join(problems))
    code = re.search(r'android:versionCode="(\d+)"', android)
    print(f"version {version} (Android versionCode {code.group(1) if code else '?'})")
    return version


def fixed_seconds():
    epoch = os.environ.get("SOURCE_DATE_EPOCH")
    if not epoch:
        try:
            epoch = subprocess.run(["git", "-C", REPO, "log", "-1", "--format=%ct"],
                                   capture_output=True, text=True, check=True).stdout.strip()
        except (OSError, subprocess.CalledProcessError):
            epoch = ""
    return int(epoch) if epoch.isdigit() else 1759190400   # 2025-09-30, when git cannot say


def fixed_time():
    stamp = time.gmtime(max(fixed_seconds(), 315532800))   # a zip cannot hold a date before 1980
    return stamp[:6]


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def stage_windows():
    exe = os.path.join(PACKAGE, "Penumbra.exe")
    if not os.path.isfile(exe):
        sys.exit(f"no {exe}: build, then package (tools\\make_release.bat does both steps after the build)")
    present = {name.lower() for name in os.listdir(PACKAGE)}
    missing = [dll for dll in RUNTIME if dll not in present]
    if missing:
        sys.exit("the package has no " + ", ".join(missing) + " beside Penumbra.exe; package.bat found no "
                 "Visual C++ runtime to copy (VCToolsRedistDir). A release must carry it.")
    for marker in (os.path.join("original", "data.enml"), os.path.join("data", "strings.json"),
                   os.path.join("assets", "shaders", "frag.spv")):
        if not os.path.isfile(os.path.join(PACKAGE, marker)):
            sys.exit(f"the package has no {marker}")

    if os.path.isdir(STAGE):
        shutil.rmtree(STAGE)
    os.makedirs(STAGE)
    shutil.copytree(PACKAGE, os.path.join(STAGE, "Penumbra"))
    text = read(HOW_TO_PLAY).replace("\r\n", "\n")
    with open(os.path.join(STAGE, "HOW TO PLAY.txt"), "w", encoding="utf-8-sig", newline="\r\n") as f:
        f.write(text)


def zip_windows():
    when = fixed_time()
    files = []
    for root, dirs, names in os.walk(STAGE):
        dirs.sort()
        for name in sorted(names):
            path = os.path.join(root, name)
            files.append((os.path.relpath(path, STAGE).replace(os.sep, "/"), path))
    files.sort(key=lambda item: item[0])
    if os.path.exists(ZIP):
        os.remove(ZIP)
    with zipfile.ZipFile(ZIP, "w") as archive:
        for name, path in files:
            info = zipfile.ZipInfo(name, date_time=when)
            info.compress_type = zipfile.ZIP_DEFLATED
            # A plain file, the same whichever system zips it (zipfile's own
            # default depends on the platform it runs on).
            info.create_system = 3
            info.external_attr = 0o100644 << 16
            with open(path, "rb") as f:
                archive.writestr(info, f.read(), compresslevel=9)
    size = os.path.getsize(ZIP)
    print(f"{ZIP}: {len(files)} files, {size / 1024 / 1024:.1f} MB")


def copy_original(destination):
    source = os.path.join(REPO, "extracted", "app")
    for root, dirs, names in os.walk(source):
        dirs.sort()
        target = os.path.join(destination, os.path.relpath(root, source))
        os.makedirs(target, exist_ok=True)
        for name in sorted(names):
            low = name.lower()
            if low.endswith(ORIGINAL_EXCLUDED_SUFFIXES) or low in ORIGINAL_EXCLUDED_NAMES:
                continue
            shutil.copy2(os.path.join(root, name), os.path.join(target, name))


def stage_linux(build):
    program = os.path.join(build, "game", "Penumbra")
    if not os.path.isfile(program):
        sys.exit(f"no {program}: build the Penumbra target first (tools/build_linux.sh)")
    if os.path.isdir(LINUX_STAGE):
        shutil.rmtree(LINUX_STAGE)
    package = os.path.join(LINUX_STAGE, "Penumbra")
    os.makedirs(os.path.join(package, "assets", "shaders"))
    shutil.copy2(program, os.path.join(package, "Penumbra"))
    shaders = os.path.join(REPO, "engine", "assets", "shaders")
    for name in sorted(os.listdir(shaders)):
        if name.endswith(".spv"):
            shutil.copy2(os.path.join(shaders, name), os.path.join(package, "assets", "shaders", name))
    shutil.copytree(os.path.join(REPO, "game", "data"), os.path.join(package, "data"))
    copy_original(os.path.join(package, "original"))
    shutil.copy2(os.path.join(REPO, "README.md"), os.path.join(package, "README.md"))
    shutil.copy2(os.path.join(REPO, "LICENSE"), os.path.join(package, "LICENSE.txt"))
    # The README links docs/*.md and shows docs/images: they travel with it.
    docs = os.path.join(REPO, "docs")
    os.makedirs(os.path.join(package, "docs"))
    for name in sorted(os.listdir(docs)):
        if name.endswith(".md"):
            shutil.copy2(os.path.join(docs, name), os.path.join(package, "docs", name))
    shutil.copytree(os.path.join(docs, "images"), os.path.join(package, "docs", "images"))
    licenses = os.path.join(package, "licenses")
    os.makedirs(licenses)
    for source, name in ((("engine", "LICENSE"), "Supersonic-Engine-LICENSE.txt"),
                         (("engine", "THIRD_PARTY_LICENSES.md"), "Supersonic-Engine-THIRD_PARTY_LICENSES.md"),
                         (("LICENSE.md",), "LICENSE.md"),
                         (("licenses", "LGPL-3.0.txt"), "LGPL-3.0.txt"),
                         (("licenses", "GPL-3.0.txt"), "GPL-3.0.txt")):
        shutil.copy2(os.path.join(REPO, *source), os.path.join(licenses, name))

    missing = [marker for marker in LINUX_MARKERS if not os.path.isfile(os.path.join(package, marker))]
    if missing:
        sys.exit("the Linux package has no " + ", ".join(missing))
    for root, _, names in os.walk(os.path.join(package, "original")):
        for name in names:
            if name.lower().endswith((".exe", ".dll", ".as", ".cg")):
                sys.exit(f"the original's {os.path.join(root, name)} was copied; it must not be")

    text = read(LINUX_HOW_TO_PLAY).replace("\r\n", "\n")
    with open(os.path.join(LINUX_STAGE, "HOW TO PLAY.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def tar_linux():
    when = fixed_seconds()
    entries = []
    for root, dirs, names in os.walk(LINUX_STAGE):
        for name in dirs + names:
            path = os.path.join(root, name)
            entries.append((os.path.relpath(path, LINUX_STAGE).replace(os.sep, "/"), path))
    entries.sort(key=lambda item: item[0])
    if os.path.exists(TARBALL):
        os.remove(TARBALL)
    files = 0
    with open(TARBALL, "wb") as raw:
        # No file name and no time in the gzip header: they would change the bytes on every run.
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0, compresslevel=9) as packed:
            with tarfile.open(fileobj=packed, mode="w", format=tarfile.PAX_FORMAT) as archive:
                for name, path in entries:
                    info = tarfile.TarInfo(name)
                    info.mtime = when
                    info.uid = info.gid = 0
                    info.uname = info.gname = ""
                    if os.path.isdir(path):
                        info.type = tarfile.DIRTYPE
                        info.mode = 0o755
                        archive.addfile(info)
                        continue
                    info.size = os.path.getsize(path)
                    info.mode = 0o755 if name == "Penumbra/Penumbra" else 0o644
                    with open(path, "rb") as f:
                        archive.addfile(info, f)
                    files += 1
    print(f"{TARBALL}: {files} files, {os.path.getsize(TARBALL) / 1024 / 1024:.1f} MB")


def write_sums(directory, names):
    lines = []
    for name in names:
        path = os.path.join(directory, name)
        if not os.path.isfile(path):
            sys.exit(f"no {path} to checksum")
        lines.append(f"{sha256(path)}  {name}")
    with open(os.path.join(directory, "SHA256SUMS.txt"), "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    for line in lines:
        print(line)


def write_checksums():
    lines = []
    for path in (ZIP, APK):
        if os.path.isfile(path):
            lines.append(f"{sha256(path)}  {os.path.basename(path)}")
    with open(os.path.join(RELEASE, "SHA256SUMS.txt"), "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    for line in lines:
        print(line)


def main(argv):
    if argv == ["check"]:
        check_versions()
        return 0
    if argv == ["windows"]:
        check_versions()
        os.makedirs(RELEASE, exist_ok=True)
        stage_windows()
        zip_windows()
        write_checksums()
        return 0
    if len(argv) == 2 and argv[0] == "linux":
        check_versions()
        os.makedirs(RELEASE, exist_ok=True)
        stage_linux(os.path.abspath(argv[1]))
        tar_linux()
        return 0
    if len(argv) >= 3 and argv[0] == "sums":
        write_sums(argv[1], argv[2:])
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
