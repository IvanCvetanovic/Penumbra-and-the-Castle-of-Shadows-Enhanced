"""The files a player downloads from a GitHub release, made from what is built.

    python tools/make_release.py windows    out/release/Penumbra-Windows.zip
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

It refuses a package without the Visual C++ runtime beside Penumbra.exe
(package.bat only warns): a player without the Redistributable could not start
the game at all, and Windows would say so in words about DLLs. And it refuses
a release whose version differs between the top-level CMakeLists.txt (which the
Windows resources take), the Windows manifest and the Android manifest.

Nothing here builds, runs the game, signs or uploads anything.
"""

import hashlib
import os
import re
import shutil
import subprocess
import sys
import time
import zipfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PACKAGE = os.path.join(REPO, "out", "package", "Penumbra")
RELEASE = os.path.join(REPO, "out", "release")
STAGE = os.path.join(RELEASE, "Penumbra-Windows")
ZIP = os.path.join(RELEASE, "Penumbra-Windows.zip")
APK = os.path.join(RELEASE, "Penumbra-Android.apk")
HOW_TO_PLAY = os.path.join(REPO, "game", "windows", "how-to-play.txt")

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
    if problems:
        sys.exit("version mismatch:\n  " + "\n  ".join(problems))
    code = re.search(r'android:versionCode="(\d+)"', android)
    print(f"version {version} (Android versionCode {code.group(1) if code else '?'})")
    return version


def fixed_time():
    epoch = os.environ.get("SOURCE_DATE_EPOCH")
    if not epoch:
        try:
            epoch = subprocess.run(["git", "-C", REPO, "log", "-1", "--format=%ct"],
                                   capture_output=True, text=True, check=True).stdout.strip()
        except (OSError, subprocess.CalledProcessError):
            epoch = ""
    seconds = int(epoch) if epoch.isdigit() else 1759190400   # 2025-09-30, when git cannot say
    stamp = time.gmtime(max(seconds, 315532800))               # a zip cannot hold a date before 1980
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
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
