<div align="center">

# Penumbra e o Castelo das Sombras — Enhanced

**The 2010 action platformer by André Santee, enhanced for modern systems.**

[![CI](https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/actions/workflows/ci.yml/badge.svg)](https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/actions/workflows/ci.yml)
[![Apple](https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/actions/workflows/apple.yml/badge.svg)](https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/actions/workflows/apple.yml)
![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)
![Vulkan](https://img.shields.io/badge/Vulkan-1.2-AC162C?logo=vulkan&logoColor=white)
[![Licence: MIT-0](https://img.shields.io/badge/licence-MIT--0-FF7A3D)](LICENSE.md)
[![Built with Claude Code](https://img.shields.io/badge/built%20with-Claude%20Code-D97757?logo=anthropic&logoColor=white)](#how-it-was-made)

![Windows: playtested](https://img.shields.io/badge/Windows-playtested-2ea44f)
![Linux: tested](https://img.shields.io/badge/Linux-tested-1f6feb?logo=linux&logoColor=white)
![Android: emulator](https://img.shields.io/badge/Android-emulator%20tested-d29922?logo=android&logoColor=white)
![macOS: CI](https://img.shields.io/badge/macOS-CI%20tested-d29922?logo=apple&logoColor=white)
![iOS: builds](https://img.shields.io/badge/iOS-builds%20only-8b949e?logo=apple&logoColor=white)

<img src="docs/images/menu.jpg" alt="The main menu, filling a 16:9 window" width="860">

<img src="docs/images/level2.jpg" alt="Level 2: a fireball toward three guards" width="32%">
<img src="docs/images/arena-lava.jpg" alt="A Versus arena over lava" width="32%">
<img src="docs/images/touch-controls.jpg" alt="The touch controls on Android" width="32%">
<img src="docs/images/options.jpg" alt="The options screen" width="32%">
<img src="docs/images/arena-pipes.jpg" alt="A Versus arena with pipes" width="32%">
<img src="docs/images/credits.jpg" alt="The credits, with the enhanced edition" width="32%">

</div>

## About

*Penumbra e o Castelo das Sombras* (2010) was made by André Santee (Asantee) on his Ethanon
Engine. You play a wizard with a sword, fireballs and a light spell, fighting through three levels
to the castle's king, alone, in co-op, or in Versus.

This is an **enhanced edition**, not a remake: the original's own scripts are ported to C++ line by
line and read the original's own files, so the game plays exactly as it did. It runs on Ivan
Cvetanović's [Supersonic Engine](https://github.com/IvanCvetanovic/Supersonic-Engine).

## What's enhanced

| | |
|---|---|
| **Display** | Widescreen at any resolution; the best resolution and refresh rate chosen automatically, or by hand; smooth motion on fast monitors |
| **Controls** | Modern gamepads, a keyboard second player, on-screen touch controls, a pause |
| **Language** | Portuguese and English, switchable in game |
| **Platforms** | Windows, Linux, Android, macOS and iOS |
| **Fixes** | Bugs of the original fixed, and its rendering matched more closely |

Every change is listed with what the original did in [`docs/enhancements.md`](docs/enhancements.md).

## Platform status

| Platform | Status | What was checked |
|---|---|---|
| Windows | ![playtested](https://img.shields.io/badge/-playtested-2ea44f) | Played through by a person; 10,056 automated checks |
| Linux | ![tested](https://img.shields.io/badge/-tested-1f6feb) | Automated checks and headless rendering; not played by a person |
| Android | ![emulator](https://img.shields.io/badge/-emulator%20only-d29922) | Scripted tests on an emulator; not run on a real phone |
| macOS | ![CI](https://img.shields.io/badge/-CI%20only-d29922) | Builds, tests and renders on GitHub Actions; not played; unsigned |
| iOS | ![builds](https://img.shields.io/badge/-builds%20only-8b949e) | Compiles; never run |

> [!IMPORTANT]
> Only the Windows version has been played by a person. The others are verified by automated
> tests only.

## Quick start

```bash
git clone --recurse-submodules https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced.git
```

| Platform | Build and run | Needs |
|---|---|---|
| Windows | `cmd //c "tools\build.bat"`, then `build\game\Penumbra.exe` | Visual Studio Build Tools (C++), CMake, Ninja |
| Linux | `bash tools/build_linux.sh`, then `~/pn-build-linux/game/Penumbra` | [packages](docs/building.md#linux-x64), X11 or XWayland |
| Android | `bash tools/build_android.sh --abi all` (APK in `out/android/`) | Android SDK and NDK, JDK 17, Python 3 |
| macOS, iOS | The [Apple workflow](.github/workflows/apple.yml) on GitHub Actions | |

A GPU with **Vulkan 1.2** is required. Details: [`docs/building.md`](docs/building.md) ·
[`docs/playing.md`](docs/playing.md).

## Controls

| Action | Keyboard | Gamepad |
|---|---|---|
| Walk | ← → | Stick or D-pad |
| Jump | ↑ or Ctrl | A |
| Sword · Fireball · Light | S · D · Space | X · B · Y |
| Pause | Esc | Back |

> [!TIP]
> Combos: **← ← S** (sword beam) and **↓ ← D** (blast). On phones the game draws its own buttons,
> including one-tap combos. Everything else is in [`docs/controls.md`](docs/controls.md).

## How it was made

> [!NOTE]
> The enhanced edition was directed and reviewed by **Ivan Cvetanović** and developed with
> **[Claude Code](https://claude.com/claude-code)**, Anthropic's AI coding tool: the porting, the
> engine work, the tests and the documentation. [`CLAUDE.md`](CLAUDE.md), [`DEVLOG.md`](DEVLOG.md)
> and [`docs/planning/`](docs/planning/2026-09-27-penumbra-port.md) are the working record.

Tests: 17 suites check the port against the original's files, and five of them play the game
headless ([`docs/testing.md`](docs/testing.md)).

## Credits

- **Original game (2010):** André Santee (programming, game design, Ethanon Engine), Arthur
  Santee (3D modelling, game design), Gabriel Duarte (soundtrack), Approaching Thunderstorm
  (music); thanks to James Hastings-Trew, José Rodolfo Ortale, Rafael "Pet" Alencar, Taina Monclaire.
- **Enhanced edition:** Ivan Cvetanović, developed with Claude Code.
- **Touch buttons:** from *Magic Rampage*, by Asantee Games.
- **Third-party:** TinyXML, dr_mp3, Liberation and DejaVu fonts, and the engine's libraries
  ([list](https://github.com/IvanCvetanovic/Supersonic-Engine/blob/main/THIRD_PARTY_LICENSES.md)).

## Licence

- **Ivan Cvetanović's code and the Supersonic Engine:** [MIT No Attribution](LICENSE), completely
  free to use.
- **The ported scripts** (`game/script/`) **and the Ethanon runtime** (`game/eth/`): LGPL-3.0.
- **The original game's files and the Magic Rampage buttons:** their authors' property.

> [!WARNING]
> Provided **as is, without any warranty**. Ivan Cvetanović is not responsible for anything that
> happens through its use, including lost data, damage to hardware, or display problems (the game
> can change a monitor's resolution and refresh rate). Use it at your own risk. Full terms:
> [`LICENSE.md`](LICENSE.md).
