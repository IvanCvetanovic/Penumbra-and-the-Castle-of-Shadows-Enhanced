<div align="center">

# Penumbra and the Castle of Shadows — Enhanced

**The 2010 action platformer by André Santee, enhanced for modern systems.**

<a href="https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/releases/latest/download/Penumbra-Windows.zip"><img alt="Download for Windows" height="56" src="https://img.shields.io/badge/Download%20for-Windows-D9531E?style=for-the-badge&labelColor=2F1F42&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+PHBhdGggZmlsbD0iI2ZmZiIgZD0iTTExIDNoMnY5LjZsMy4zLTMuMyAxLjQgMS40TDEyIDE2LjRsLTUuNy01LjcgMS40LTEuNCAzLjMgMy4zVjN6TTQgMThoMTZ2Mkg0eiIvPjwvc3ZnPg=="></a>&nbsp;&nbsp;<a href="https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/releases/latest/download/Penumbra-Android.apk"><img alt="Download for Android" height="56" src="https://img.shields.io/badge/Download%20for-Android-1E8C6E?style=for-the-badge&labelColor=2F1F42&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+PHBhdGggZmlsbD0iI2ZmZiIgZD0iTTExIDNoMnY5LjZsMy4zLTMuMyAxLjQgMS40TDEyIDE2LjRsLTUuNy01LjcgMS40LTEuNCAzLjMgMy4zVjN6TTQgMThoMTZ2Mkg0eiIvPjwvc3ZnPg=="></a>

Also for **Mac**, **Linux** and, experimental, **iPhone and iPad**: [the other downloads](https://ivancvetanovic.github.io/Penumbra-and-the-Castle-of-Shadows-Enhanced/#other)

### [How to install: the download page](https://ivancvetanovic.github.io/Penumbra-and-the-Castle-of-Shadows-Enhanced/)

Most players should just use that page: every download and simple install steps, in English
and [Portuguese](https://ivancvetanovic.github.io/Penumbra-and-the-Castle-of-Shadows-Enhanced/?lang=pt).<br>
Free. Windows 10 or 11 (64-bit), Android 8 or newer, or 64-bit (x86_64) Linux from about 2022 on, with
Vulkan 1.2 graphics; or a Mac with macOS 13.3 or newer.<br>
The Windows download is not code-signed yet, and the Mac app is not notarised by Apple:
[Code signing policy](docs/code-signing.md).

[![CI](https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/actions/workflows/ci.yml/badge.svg)](https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/actions/workflows/ci.yml)
[![Apple](https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/actions/workflows/apple.yml/badge.svg)](https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/actions/workflows/apple.yml)
![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)
![Vulkan](https://img.shields.io/badge/Vulkan-1.2-AC162C?logo=vulkan&logoColor=white)
[![Licence: MIT-0](https://img.shields.io/badge/licence-MIT--0-FF7A3D)](LICENSE.md)
[![Built with Claude Code](https://img.shields.io/badge/built%20with-Claude%20Code-D97757?logo=anthropic&logoColor=white)](#how-it-was-made)

<img src="docs/images/menu.jpg" alt="The main menu, filling a 16:9 window" width="860">

<img src="docs/images/level2.jpg" alt="Level 2: a fireball toward three guards" width="32%">
<img src="docs/images/arena-lava.jpg" alt="A Versus arena over lava" width="32%">
<img src="docs/images/touch-controls.jpg" alt="The touch controls on Android" width="32%">
<img src="docs/images/options.jpg" alt="The options screen" width="32%">
<img src="docs/images/arena-pipes.jpg" alt="A Versus arena with pipes" width="32%">
<img src="docs/images/credits.jpg" alt="The credits, with the enhanced edition" width="32%">

</div>

## About

*Penumbra and the Castle of Shadows* (2010) was made by André Santee (Asantee) on his Ethanon
Engine. You play a wizard with a sword, fireballs and a light spell, fighting through three levels
to the castle's king, alone, in co-op, or in Versus.

This is an **enhanced edition**, not a remake: the original's own scripts are ported to C++ line by
line and read the original's own files, so the game plays exactly as it did. It runs on Ivan
Cvetanović's [Supersonic Engine](https://github.com/IvanCvetanovic/Supersonic-Engine), and is
published with André Santee's explicit permission.

## What's enhanced

| | |
|---|---|
| **Display** | Widescreen at any resolution; the best resolution and refresh rate chosen automatically, or by hand; smooth motion on fast monitors |
| **Controls** | Modern gamepads, a keyboard second player, on-screen touch controls, a pause |
| **Language** | Portuguese and English, switchable in game |
| **Platforms** | Downloads for Windows, Android, Mac and Linux, and an experimental one for iPhone and iPad |
| **Fixes** | Bugs of the original fixed, and its rendering matched more closely |

Every change is listed with what the original did in [`docs/enhancements.md`](docs/enhancements.md).

## Platform status

| Platform | Status | What was checked | Download |
|---|---|---|---|
| Windows | ![playtested](https://img.shields.io/badge/-playtested-2ea44f) | Played through by a person; over 10,000 automated checks | [Penumbra-Windows.zip](https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/releases/latest/download/Penumbra-Windows.zip) |
| Linux | ![tested](https://img.shields.io/badge/-tested-1f6feb) | All 17 test suites pass in the release build; the download itself, unpacked read-only, plays level 1 on a software Vulkan driver; not played by a person | [Penumbra-Linux.tar.gz](https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/releases/latest/download/Penumbra-Linux.tar.gz) |
| Android | ![emulator](https://img.shields.io/badge/-emulator%20only-d29922) | Scripted tests on an emulator; not run on a real phone | [Penumbra-Android.apk](https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/releases/latest/download/Penumbra-Android.apk) |
| macOS | ![CI](https://img.shields.io/badge/-CI%20only-d29922) | Tests pass on GitHub Actions; the download runs level 1 on GitHub's Apple silicon Macs, and its Intel half there under Rosetta; not played; never run on a real Intel Mac; not notarised | [Penumbra-macOS.zip](https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/releases/latest/download/Penumbra-macOS.zip) |
| iOS | ![builds](https://img.shields.io/badge/-builds%20only-8b949e) | Compiles; never run on any iPhone or iPad | Experimental: [Penumbra-iOS.ipa](https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/releases/latest/download/Penumbra-iOS.ipa), for [sideloading](docs/playing.md#iphone-and-ipad-experimental) |

> [!IMPORTANT]
> Only the Windows version has been played by a person. Linux, Android and macOS are verified by
> automated tests only; the iPhone/iPad version has never run on an iPhone or iPad.

## Build from source

```bash
git clone --recurse-submodules https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced.git
```

| Platform | Build and run | Needs |
|---|---|---|
| Windows | `cmd //c "tools\build.bat"`, then `build\game\Penumbra.exe` | Visual Studio Build Tools (C++), CMake, Ninja |
| Linux | `bash tools/build_linux.sh`, then `~/pn-build-linux/game/Penumbra` | [packages](docs/building.md#linux-x64), X11 or XWayland |
| Android | `bash tools/build_android.sh --abi all` (APK in `out/android/`) | Android SDK and NDK, JDK 17, Python 3 |
| macOS, iOS | The [Apple workflow](.github/workflows/apple.yml) on GitHub Actions; the downloads, the [release workflow](.github/workflows/release.yml) | |

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
