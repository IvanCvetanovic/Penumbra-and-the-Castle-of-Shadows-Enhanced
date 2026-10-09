# Building

Every platform builds from the same tree: the engine as a CMake subproject (`engine/`, a git
submodule), then `game/` and `tests/`. Clone with the submodule:

```bash
git clone --recurse-submodules https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced.git
# or, in an existing clone:
git submodule update --init
```

On Windows, if the folder you clone into has a long path, the clone can stop with "Filename too long": clone
with `git clone -c core.longpaths=true --recurse-submodules <url>` instead, or into a shorter path.

The original game's files are in the repository (`extracted/app`), so nothing else needs to be
downloaded. What each platform has been verified to do is in the README's
[Platform status](../README.md#platform-status).

## Windows (x64)

Prerequisites:

- The Visual Studio Build Tools (or Visual Studio) with "Desktop development with C++" (MSVC). It
  was developed with the version 18 Build Tools; other versions are untried.
  The scripts find the newest installation through `vswhere`; set `PENUMBRA_VCVARS` to the full
  path of a `vcvars64.bat` to choose one.
- CMake 3.20 or newer, and Ninja, on `PATH`. Visual Studio's C++ CMake tools bring both.
- Git.
- Optional: the Vulkan SDK, for the validation layers. Version 1.4.357.0 at its default place
  also brings the `glslc` that reproduces the engine's committed SPIR-V byte for byte, and the
  build then compiles the shaders with it (`PENUMBRA_GLSLC` names another `glslc`). Without them,
  the engine builds from its vendored Vulkan headers and the driver's loader and uses the
  committed SPIR-V, which is all a build needs.

From Git Bash at the repository root:

```bash
cmd //c "tools\build.bat --target Penumbra"    # the game
cmd //c "tools\build.bat"                      # the game and every test suite
cmd //c "tools\package.bat"                    # then: out/package/Penumbra/
```

From `cmd`, run `tools\build.bat --target Penumbra` and `tools\package.bat`.

On Windows 11, Smart App Control can refuse to run an executable you have just built, and it offers no
per-file exception. Build and test on Linux or WSL (`bash tools/build_linux.sh --test`) if that happens.

`tools\build.bat` loads MSVC's environment, then configures `build\` on the first run: Ninja,
Release, Vulkan validation on. After that it builds. Its header lists what it takes and how to
override each. The game's code builds with no warnings at `/W4`.

`tools\check.bat <files>` compiles single `.cpp` files with the build's flags without touching
`build\`.

The package and its requirements are described in [playing.md](playing.md#the-packaged-game-windows).

## Linux (x64)

On Ubuntu 24.04, including under WSL, install:

```bash
sudo apt-get install cmake ninja-build g++ pkg-config libvulkan-dev libx11-dev libxrandr-dev \
    libxinerama-dev libxcursor-dev libxi-dev libxkbcommon-dev libasound2-dev \
    xvfb mesa-vulkan-drivers vulkan-tools   # the last three only for a headless run
bash tools/build_linux.sh --test         # configure ~/pn-build-linux, build, run test_pn_all once
bash tools/build_linux.sh --clang        # the same with clang, in ~/pn-build-linux-clang
```

The build directory must be on the Linux filesystem. Under WSL it must not be under `/mnt/c`.
`tools/build_linux.sh` configures the build as the engine's CI does: Ninja, Release, GLFW without
Wayland, so the game runs under X11 or XWayland, with a Vulkan driver. No `glslc` is needed, because the engine's committed SPIR-V is used. The script also
copies the shaders beside the executable, so the game anchors its working directory there. The
game builds without warnings at `-Wall -Wextra -Wpedantic`, the engine's level for its core. The
suites build without warnings at `-Wall -Wextra`, the engine's level for its own suites. The
script's header lists its other options.

The Microsoft fonts are not used off Windows. The bundled stand-ins in `game/data/fonts` take
their place, drawn with the same metrics (E17). The original's MP3s are decoded by dr_mp3, because
the engine decodes MP3 only through Windows' Media Foundation (E18). With no sound device, as in
WSL, the game runs silent.

A headless run renders on Mesa's lavapipe under Xvfb, for example:

```bash
cd ~/pn-build-linux/game
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json xvfb-run -a -s "-screen 0 1920x1080x24" \
    ./Penumbra --start level1 --window 1920x1080 --fixed-step --frames 300 \
    --screenshot "$HOME/level1.png"
```

This build is for the machine it is built on. The Linux download is built another way, so that it
runs on older systems too: see [The Linux release build](#the-linux-release-build).

## Android (arm64-v8a, x86_64)

A debug APK is built on Windows from Git Bash (or `tools\build_android.bat` from `cmd`), without
Gradle. It needs the Android SDK with NDK 28.2.13676358, build-tools 35.0.0, platform 35, the SDK's
CMake 3.22.1 package and platform-tools (adb, for installing), found
through `ANDROID_SDK`, `ANDROID_HOME` or `ANDROID_SDK_ROOT`, else at Android Studio's default
`%LOCALAPPDATA%\Android\Sdk`; a JDK 17; and Python 3 with Pillow:

```bash
bash tools/build_android.sh               # x86_64 (the emulator's): configure, build, package
bash tools/build_android.sh --abi all     # x86_64 + arm64-v8a (phones) in one APK
bash tools/build_android.sh --install --serial <device>   # adb install -r
```

The APK is `out/android/Penumbra-debug.apk` (about 18 MB for both ABIs, as a debug build), debug-signed. It is a
NativeActivity with a few lines of Java (the engine's `SupersonicActivity`: it hides the system
bars and reports the safe area, and `tools/build_android.sh` compiles it into `classes.dex`); the
engine's `android_main` (`engine/src/platform/android`) runs the game, and `game/android/AndroidMain.cpp` unpacks the original's files and the port's data from the
APK into the app's private storage on the first launch (and again only when they change), and
passes them with `--original`/`--data`.

- Landscape only; minimum Android 8.0 (API 26, for AAudio); a Vulkan 1.1 GPU is the least the phone build takes (main.cpp sets `GameManifest::minimumVulkanMinor` to 1 there; a computer, and the engine's own default, keep 1.2). A start that fails shows the player a dialog with the reason (E39). A run that ended unexpectedly (a native crash, a stop by the system while it was on the screen, a start that ended itself with an error) is reported at the next start, on Android 11 and later, by a dialog with the system's record of how it ended and the end of the game's log, once per end, with a Share button; the game's start waits behind it, and a scripted run (a `penumbra_args.txt` left for it) never meets it (E40). On a phone or tablet the scene target's size follows how fast the GPU draws it, starting at about 2.2 megapixels (E41: `DynamicResolution`, with the scene shader specialised to unlit draws); `--dynamic-res off` (or `--render-scale 1`) gives a scripted run its full-size picture.
- The touch controls (E16) are on by default and play player 1. Keyboard player 2 is off (a phone
  has no keyboard), so a Bluetooth gamepad plays player 2 (E22): in Versus, which asks for one
  until it is connected, and as the campaign's princess.
- The Back key is Esc.
- Development flags go in `penumbra_args.txt` in the app's files directory
  (`tools/build_android.sh --run "<flags>"`).

## macOS and iOS

Both are built on GitHub's macOS runners by `.github/workflows/apple.yml`, which can be run by hand
from the repository's Actions tab (Apple, Run workflow). MoltenVK is linked into the program, so
no Vulkan SDK or loader is needed. The Mac and iPhone/iPad downloads are built by another
workflow, `release.yml` ([Release downloads](#release-downloads)).

- **macOS.** The workflow builds every target for the runner's own Mac (Apple silicon), runs
  `test_pn_all` once, assembles `Penumbra.app` (`tools/apple/make_app.sh`) and captures level 1
  from inside it. Its `macos` artifact is that development build. To play the game on a Mac, use
  the release's `Penumbra-macOS.zip` instead: one app for Apple silicon and Intel Macs, macOS 13.3
  Ventura or newer. It is signed ad hoc and not notarised by Apple, so the first open takes one
  extra step, which the `HOW TO PLAY.txt` beside the app describes (macOS 15 Sequoia and newer:
  System Settings, Privacy & Security, Open Anyway; macOS 13 and 14: close the message, then Control-click the app, Open).
- **iOS.** The simulator and device builds compile and link, but the game has never displayed a
  frame: the simulator's GPU cannot draw the engine's instanced batches (it has no base-instance
  drawing), and no device has run it. The release has the device build as an experimental
  `Penumbra-iOS.ipa` (iOS and iPadOS 16.3 or newer), with no certificate (only the build's ad-hoc
  seal), for a sideloading tool
  (Sideloadly, AltStore or SideStore) to sign with the player's own Apple ID. It has never run on
  an iPhone or iPad, and it may not start.

## Release downloads

A release on GitHub holds five downloads and `SHA256SUMS.txt`, their checksums. Two are made on
the development machine: `Penumbra-Windows.zip` by `tools\make_release.bat` (after a build; it
runs `tools\package.bat`, then `tools/make_release.py windows`), and `Penumbra-Android.apk` by
`bash tools/build_android.sh --release`, signed with the release key, which is kept outside the
repository ([code-signing.md](code-signing.md)). The other three are built on GitHub by
`.github/workflows/release.yml` and added to the release: `Penumbra-Linux.tar.gz`,
`Penumbra-macOS.zip` and `Penumbra-iOS.ipa`.

### The release workflow

It runs only by hand: Actions, Release, Run workflow, with the tag of the release to add the files
to (the default tag is set in `release.yml`). The "publish" box is off by default, and the run is then a dry run: it
builds and checks everything and leaves the three files as the run's artifacts (`release-linux`,
`release-macos`, `release-ios`), with the logs and captures beside them (`linux-logs`,
`macos-logs`, `ios-logs`), to be looked at first. The files are built from the commit the run
starts on, which may be later than the tag; the publish job's summary names that commit.

| Job | Runs on | What it does |
|---|---|---|
| `linux` | an `ubuntu:22.04` container | builds every target ([below](#the-linux-release-build)), runs `test_pn_all` once, checks the program's libraries and glibc symbols, writes `Penumbra-Linux.tar.gz` and runs the package as a player gets it |
| `macos` | `macos-15` | builds `Penumbra` for arm64 and x86_64 in one program (macOS 13.3 or newer, no validation layers, no suites: `apple.yml` runs those), writes `Penumbra-macOS.zip`, and runs the app from the unpacked zip on arm64, then its x86_64 half under Rosetta |
| `ios` | `macos-15` | builds `Penumbra` for devices (iphoneos, arm64, iOS 16.3 or newer) and writes `Penumbra-iOS.ipa`; nothing runs it |
| `publish` | `ubuntu-24.04` | only with "publish" on, and only when the other three passed: adds the files to the release |

The publish job downloads every file the release has and checks each against the release's own
`SHA256SUMS.txt`. It rewrites that file for all five downloads, with the Windows and Android lines
first and compared byte for byte with the published ones, and uploads the three new files and
`SHA256SUMS.txt` (a later publish replaces those four). It never uploads the Windows zip or the
APK, so it never replaces them. Every check the workflow makes is listed in
[testing.md](testing.md#the-release-workflow).

### The Linux release build

The Linux download is built in an Ubuntu 22.04 container, so the program needs no newer glibc than
2.35 (most desktop Linux from about 2022 on). The container gets GCC 13 from the
`ppa:ubuntu-toolchain-r/test` PPA and CMake 3.28 from PyPI (22.04's own is 3.22). The build is
configured first, and `tools/build_linux.sh` then builds and tests it (it leaves a configured
directory as it is):

```bash
cmake -S . -B /tmp/pn -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13 \
    -DSUPERSONIC_ENABLE_VALIDATION=OFF -DGLFW_BUILD_WAYLAND=OFF -DGLSL_COMPILER=OFF \
    -DVulkan_INCLUDE_DIR="$PWD/engine/third_party/Vulkan-Headers-1.3.290/include" \
    -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc"
bash tools/build_linux.sh --build-dir /tmp/pn --jobs 4 --test
```

- `-static-libstdc++ -static-libgcc`: the C++ runtime is linked into the program, so a player's
  system needs none of its own.
- The engine's vendored Vulkan headers, not the system's. The program still loads the system's
  Vulkan loader, `libvulkan.so.1`.
- No validation layers, no Wayland (the game runs under X11 or XWayland), and no shader compiler:
  the engine's committed SPIR-V is used.

Then the program itself is checked:

- Its libraries (`readelf -d`, the NEEDED entries) must include `libvulkan.so.1`, `libasound.so.2`
  and `libc.so.6`, and may name nothing else but `libm.so.6`, `libpthread.so.0`, `libdl.so.2` and
  `ld-linux-x86-64.so.2`. ALSA is required because a build without `libasound2-dev` still
  succeeds, silent. `ld-linux-x86-64.so.2` is glibc's own loader, which every glibc system has; the
  program names it once libstdc++ is linked in statically.
- Its newest glibc symbol version (`objdump -T`, `GLIBC_*`) must be 2.35 or older.

Then it is stripped, packaged and run (see [testing.md](testing.md#the-release-workflow)).

### The packaging scripts

```bash
python3 tools/make_release.py linux <build dir>                # out/release/Penumbra-Linux.tar.gz
python3 tools/make_release.py sums <dir> <file>...             # <dir>/SHA256SUMS.txt over those files
python3 tools/make_release.py check                            # the version check alone
bash tools/apple/make_release.sh macos <build dir> <out dir>   # <out dir>/Penumbra-macOS.zip
bash tools/apple/make_release.sh ios <build dir> <out dir>     # <out dir>/Penumbra-iOS.ipa
```

- **`make_release.py linux`** lays the build out as `tools\package.bat` lays out the Windows game
  (the program with `assets/shaders`, `data/` and `original/` beside it, the README, docs and
  licences) in `out/release/Penumbra-Linux/Penumbra/`, with `game/linux/how-to-play.txt` beside
  that folder as `HOW TO PLAY.txt`, and writes the `.tar.gz`: a tar keeps the program's executable
  bit, which a zip would lose. The same files always make the same archive: entries sorted, one
  fixed time (the HEAD commit's, or `SOURCE_DATE_EPOCH`), owner 0, mode 0755 for the program and
  the folders and 0644 for the rest, and a gzip header with no name or time. Like `windows`, it
  refuses a release whose version differs between `CMakeLists.txt`, the Windows and Android
  manifests and the Mac and iPhone `Info.plist`s.
- **`make_release.py sums`** writes the checksums in the order the files are named. The release
  workflow names the Windows zip and the APK first, so their lines stay as they were published.
- **`make_release.sh macos`** puts `Penumbra.app` (`make_app.sh`, signed ad hoc) and
  `HOW TO PLAY.txt` (`game/macos/how-to-play.txt`) in a folder `Penumbra/` and zips it with
  `ditto`, Apple's own way to zip a signed app: it keeps the executable bit and the app's
  signature seal, which a plain zip writer can lose ("is damaged and can't be opened" on the
  player's Mac). No AppleDouble files, extended attributes or quarantine flags go in. It then
  unpacks the zip again and verifies the app with `codesign --verify --deep --strict`.
- **`make_release.sh ios`** assembles the device build as `Payload/Penumbra.app` and zips it into
  the `.ipa`, signed ad hoc only. It refuses a program that links anything outside the system or
  is encrypted, which no sideloading tool could install, and an `.ipa` that does not start with
  `Payload/` or holds AppleDouble files.

Neither script builds, runs or uploads anything. The only signature is `make_app.sh`'s ad-hoc one.
