# Building

Every platform builds from the same tree: the engine as a CMake subproject (`engine/`, a git
submodule), then `game/` and `tests/`. Clone with the submodule:

```bash
git clone --recurse-submodules https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced.git
# or, in an existing clone:
git submodule update --init
```

The original game's files are in the repository (`extracted/app`), so nothing else needs to be
downloaded. What each platform has been verified to do is in the README's
[Platform status](../README.md#platform-status).

## Windows (x64)

Prerequisites:

- The Visual Studio Build Tools (or Visual Studio) with "Desktop development with C++" (MSVC). It
  was developed with the version 18 Build Tools; other versions are untried.
  The scripts find the newest installation through `vswhere`; set `PENUMBRA_VCVARS` to the full
  path of a `vcvars64.bat` to choose one. The game was developed with Build Tools 18.
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

The APK is `out/android/Penumbra-debug.apk` (about 18 MB for both ABIs), debug-signed. It is a
NativeActivity with no Java: the engine's `android_main` (`engine/src/platform/android`) runs the
game, and `game/android/AndroidMain.cpp` unpacks the original's files and the port's data from the
APK into the app's private storage on the first launch (and again only when they change), and
passes them with `--original`/`--data`.

- Landscape only; minimum Android 8.0 (API 26, for AAudio); Vulkan 1.2 is required.
- The touch controls (E16) are on by default and play player 1. Keyboard player 2 is off (a phone
  has no keyboard), so a Bluetooth gamepad plays player 2 (E22): in Versus, which asks for one
  until it is connected, and as the campaign's princess.
- The Back key is Esc.
- Development flags go in `penumbra_args.txt` in the app's files directory
  (`tools/build_android.sh --run "<flags>"`).

## macOS and iOS

Both are built on GitHub's macOS runners by `.github/workflows/apple.yml`, which can be run by hand
from the repository's Actions tab (Apple, Run workflow). MoltenVK is linked into the program, so
no Vulkan SDK or loader is needed.

- **macOS (Apple Silicon).** The workflow builds every target, runs `test_pn_all` once, assembles
  `Penumbra.app` (`tools/apple/make_app.sh`) and captures level 1 from inside it. Download the
  run's `macos` artifact. The app is only ad-hoc signed and not notarised, so run
  `xattr -dr com.apple.quarantine Penumbra.app` before opening it the first time.
- **iOS.** The simulator and device builds compile and link, but the game has never displayed a
  frame: the simulator's GPU cannot draw the engine's instanced batches (it has no base-instance
  drawing), and no device build has been signed or run. iOS is untested.
