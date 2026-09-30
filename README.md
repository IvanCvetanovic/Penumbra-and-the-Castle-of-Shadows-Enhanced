# Penumbra e o Castelo das Sombras — Enhanced

*Penumbra and the Castle of Shadows*, remade on the Supersonic Engine.

An enhanced remake of **Penumbra e o Castelo das Sombras**, a 2D side-view action platformer
released for the PC in 2010 by André Santee (Asantee). You play a wizard with a sword, fireballs
and a light spell, fighting through three levels to the king of the Castle of Shadows. A second
player can join the campaign as a summoned princess, or fight the first in Versus across six
arenas. The original's own scripts are ported to C++ and run on Ivan Cvetanović's Supersonic
Engine (C++20, Vulkan), so the game plays as the original did. It now runs in widescreen at any
resolution, in Portuguese or English, with gamepads, a keyboard second player and touch controls.
It builds for Windows, Linux, Android, macOS and iOS; [Platform status](#platform-status) says
what has been tested where.

<p align="center">
  <img src="docs/images/menu.jpg" alt="The main menu in English, filling a 16:9 window" width="880">
</p>
<p align="center">
  <img src="docs/images/level2.jpg" alt="Level 2: the wizard casts a fireball toward three guards" width="32%">
  <img src="docs/images/touch-controls.jpg" alt="Level 1 with the on-screen touch controls, on the Android emulator" width="32%">
  <img src="docs/images/options.jpg" alt="The options screen: display mode, refresh rate, controls, language, volumes" width="32%">
</p>
<p align="center"><sub>The main menu in a 16:9 window; level 2; the touch controls; the options
screen. Captured on Linux (Mesa lavapipe), except the touch controls, captured on the Android
emulator.</sub></p>

## Contents

- [About](#about)
- [Highlights](#highlights)
- [Platform status](#platform-status)
- [Getting started](#getting-started)
- [Controls](#controls)
- [Settings and saves](#settings-and-saves)
- [Testing](#testing)
- [Repository layout](#repository-layout)
- [How this was made](#how-this-was-made)
- [Credits](#credits)
- [Disclaimer](#disclaimer)
- [Licence](#licence)

## About

*Penumbra e o Castelo das Sombras* was made by André Santee, with Arthur Santee, Gabriel Duarte
and the others listed under [Credits](#credits), and released for the PC in 2010. It ran on
André Santee's own **Ethanon Engine 0.7.12** (Direct3D 9, NVIDIA Cg, AngelScript).

This remake is made, and published with the original game's files, **with the permission of the
original's authors**. Their art, music, sounds, levels and data stay theirs.

**How it works.** The original's 25 AngelScript files are ported to C++ almost line by line, each
function keeping its name, its order of operations and its numbers. They run on a small emulation
of the Ethanon 0.7.12 runtime (`game/eth/`): its files, entity buckets, callbacks, frame order,
particles, input and sound, one Ethanon frame per 60 Hz tick. The emulated frame is then drawn with
the Supersonic Engine: sprites, normal-mapped lights, live shadows, particles, text and the HUD.
The original's art, sounds and level files are read unchanged from `extracted/app`; nothing is
converted. What the original is and does, decoded from its files and the Ethanon source, is
written down with citations in [`docs/spec/`](docs/spec/README.md).

The Supersonic Engine is Ivan Cvetanović's own engine, a git submodule at `engine/`
([IvanCvetanovic/Supersonic-Engine](https://github.com/IvanCvetanovic/Supersonic-Engine)).

## Highlights

Every change from the original is listed with what the original did, and most changes that affect
play can be switched off (a few only at build time). The player's list is
[`docs/enhancements.md`](docs/enhancements.md); the full record (E1-E23, with the reasoning and
measurements) is in
[`docs/planning/2026-09-27-penumbra-port.md`](docs/planning/2026-09-27-penumbra-port.md).

- **Widescreen and modern displays.** The levels fill any window shape, and the 1024x768 menus
  carry their scenes out to the edges of wide windows, up to 4:1 (E1). Any window size, a real
  fullscreen at native resolution, and a display mode chosen automatically (the desktop's
  resolution at the monitor's highest refresh rate) or by hand, refresh rate included (E2, E23).
  Smooth motion between ticks on displays faster than 60 Hz (E8). Shadows drawn live, ending at
  the light's reach (E9).
- **Controls.** Modern gamepads mapped by meaning, for both players (E3, E12, E14). A keyboard
  second player (E4). On-screen touch controls with Magic Rampage's buttons, including one-tap
  combos (E16), and a phone plus one Bluetooth gamepad for two players (E22). A pause, which also
  opens when the window loses focus (E13).
- **Languages.** Portuguese and English, switchable in the game, including the words drawn into
  the menu art (E5). The first language follows the system (E19).
- **Settings.** The options screen gains keyboard player 2, widescreen or 4:3, the language,
  music and effects volumes, smooth motion and the pause on focus loss, and everything is saved
  (E6, E10, E20).
- **Fidelity fixes.** Plain bugs of the original fixed: the missing level-20 threshold and the
  experience code past level 30 (E7), a frame spent in the air at each floor seam (E11), five sound
  markers that never played (E15).
- **New platforms.** Linux, Android, macOS and iOS builds, with open-licence stand-in fonts (E17)
  and MP3 decoding that does not need Windows (E18).
- **Credits.** The game's own Credits panel names the enhanced edition after the 2010 team (E21).

## Platform status

| Platform | Build | Automated tests | Playtested |
|---|---|---|---|
| **Windows 10/11** (x64, MSVC, Vulkan) | Yes | `test_pn_all`: 17 suites, 10056 checks, 0 failures | **Yes.** Ivan played the whole game through on a 1920x1200 laptop with an AMD Radeon 780M. The pause was also checked with real keyboard and mouse input. |
| **Linux** (x64; GCC 13 / Clang 18; tested in WSL, Ubuntu 24.04) | Yes | The suites pass (9967 checks); headless rendering on Mesa lavapipe | No. Not played by a person on a Linux desktop; sound not heard. |
| **Android 8+** (arm64-v8a and x86_64 APK; Vulkan 1.2 required) | Yes | Run on an Android 13 emulator (SwiftShader) with scripted adb input: menus, touch controls (two fingers too), pause, Back/Home, a gamepad, the options | No. Not run on a real phone; sound not heard. |
| **macOS** (Apple Silicon, MoltenVK) | Yes, on GitHub Actions macOS runners | Run on the runners: the suites pass, level 1 renders, CoreAudio produces output | No. Not played by a person. Not signed or notarised: `xattr -dr com.apple.quarantine Penumbra.app` is needed before the first launch. |
| **iOS** | Builds only: the simulator and device builds link | None. It has never displayed a frame: the simulator's GPU lacks base-instance drawing, and no signed device run was done. | No. **Untested.** |

**Only Windows has been played by a person.** Everything said about the other platforms comes
from automated suites, scripted runs and captures.

## Getting started

The game is built from source; the original game's files are part of this repository, so nothing
else has to be downloaded. Clone with the engine submodule:

```bash
git clone --recurse-submodules https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced.git
```

The game renders with **Vulkan 1.2** (through MoltenVK on Apple platforms), so the GPU driver
must support it.

**Windows** (the Visual Studio Build Tools with the C++ workload - developed with version 18; CMake 3.20+
and Ninja; the Vulkan SDK optional), from Git Bash at the repository root:

```bash
cmd //c "tools\build.bat --target Penumbra"    # build the game
build/game/Penumbra.exe                        # play it
cmd //c "tools\package.bat"                    # a folder that plays anywhere: out/package/Penumbra/
```

**Linux** (Ubuntu 24.04 or WSL; the packages are listed in [`docs/building.md`](docs/building.md#linux-x64)):

```bash
bash tools/build_linux.sh --test               # build in ~/pn-build-linux, run the suites once
~/pn-build-linux/game/Penumbra
```

It runs under X11 or XWayland (GLFW is built without its Wayland backend) and needs a Vulkan driver.

**Android** (built on Windows from Git Bash: the Android SDK with NDK 28.2.13676358,
build-tools 35, the SDK's CMake 3.22.1 package and platform-tools, a JDK 17, Python 3 with Pillow):

```bash
bash tools/build_android.sh --abi all          # out/android/Penumbra-debug.apk, phones and emulator
bash tools/build_android.sh --install --serial <device>
```

**macOS and iOS** are built by the GitHub Actions workflow `.github/workflows/apple.yml`, which runs
on every push to `main` (a fork can run it from its own Actions tab). A run's macOS artifact holds
`Penumbra.app` for seven days; the CMake steps in the workflow also build it on a Mac by hand.

More:

- [`docs/building.md`](docs/building.md): prerequisites and build details for every platform.
- [`docs/playing.md`](docs/playing.md): the packaged game, where the game finds its files, and
  the command line (`--start level2`, `--lang en`, `--window 1920x1080`, headless captures).

## Controls

| Action | Keyboard (player 1) | Gamepad | Keyboard (player 2) |
|---|---|---|---|
| Walk | ← → | Left stick or D-pad | J L |
| Jump | ↑ or Ctrl | A | I |
| Sword | S | X | U |
| Fireball (10 mana) | D | B | O |
| Light spell (50 mana) | Space | Y | P |
| Pause / back | Esc | Back | |
| Summon player 2 in the campaign | | Start | Backspace |
| Fullscreen / window | Alt+Enter | | |

- **Combos:** ← ← S or → → S for the sword combo (5 mana); ↓ ← D or ↓ → D for the blast (25 mana).
- **Menus:** the mouse, the arrows and Enter; a pad's A confirms and B goes back.
- **Touch:** on phones and tablets the game draws its own buttons, and a tap clicks in the menus.
  On a desktop, `--touch` shows them.
- The first gamepad plays the wizard; a second plays the princess. With the touch controls on,
  the first gamepad is player 2.

Everything else, including the original's hidden keys and the touch layout, is in
[`docs/controls.md`](docs/controls.md).

## Settings and saves

The game keeps its settings (language, window and fullscreen mode, refresh rate, widescreen,
volumes, smooth motion, the pause on focus loss, the touch controls and the key bindings) in
`settings.json`, beside the best times (`hs.enml`) and the checkpoint save. It never writes into
the original's folder. They are kept in:

| Platform | Folder |
|---|---|
| Windows | `%APPDATA%\Penumbra\` |
| Linux | `$XDG_DATA_HOME/Penumbra`, else `~/.local/share/Penumbra` |
| macOS | `~/Library/Application Support/Penumbra` |

Every field is described in [`docs/playing.md`](docs/playing.md#settings-and-saves).

## Testing

Seventeen suites (`tests/test_pn_*.cpp`) check the port against the original's files. Five boot
the real game headless and play it through its ported scripts: combat, spells, combos, potions,
hazards, checkpoints, the king, Versus, co-op, the pause, the menus, the touch controls, the
display modes. `test_pn_all` holds all of them in one executable.

```bash
build/tests/test_pn_all.exe                   # Windows: every suite, once
build/tests/test_pn_all.exe --suite boot      # one suite
bash tools/build_linux.sh --test              # Linux: build, then every suite
```

On GitHub, the workflows in `.github/workflows/` build the port and run `test_pn_all` on Linux and
Windows (`ci.yml`) and on macOS (`apple.yml`). The suites and what each checks are listed in
[`docs/testing.md`](docs/testing.md).

## Repository layout

```
CMakeLists.txt       the engine as a subproject, then game/ and tests/
engine/              the Supersonic Engine, a git submodule pinned to a commit
game/
  eth/               the Ethanon 0.7.12 runtime, emulated (no renderer); Paths.* finds the files
  script/            the original's .as files ported to C++, one .cpp per .as
  render/            drawing on the engine: sprites, lights, shadows, particles, text, HUD, input, audio
  data/              the remake's own data: strings.json (English), images, fonts, touch controls
  android/ ios/ macos/ windows/   each platform's entry point and resources
  third_party/       TinyXML, dr_mp3
  PenumbraLayer.*    the engine layer that runs one Ethanon frame per 60 Hz tick and draws it
  main.cpp
tests/               the test_pn_* suites; all/ builds every one of them into test_pn_all
tools/               build and package scripts for each platform; art/ makes the English and touch images
docs/                the guides linked above, and images/ for this page
docs/spec/           what the original is and does, decoded, with citations
docs/planning/       the port's step record, rulings and enhancements
extracted/app/       the original game as installed; read, never written
penumbra_setup.exe   the original game's installer
licenses/            the GNU licence texts
.github/workflows/   GitHub Actions: ci.yml (Linux, Windows), apple.yml (macOS, iOS)
CLAUDE.md            the working rules for development sessions
DEVLOG.md            the session log
```

## How this was made

The enhanced edition was directed and reviewed by Ivan Cvetanović. The porting of the original's
scripts, the work on the Supersonic Engine, the tests and the documentation were done with
**[Claude Code](https://claude.com/claude-code), Anthropic's AI coding tool**, working under his
direction. He set the goals and made the rulings (what to enhance, what to keep as it was in 2010,
which platforms to support), reviewed the results and played the game.

The working record is in the repository: [`CLAUDE.md`](CLAUDE.md) holds the rules the
development sessions followed, [`DEVLOG.md`](DEVLOG.md) records what each session built, what
broke and what was measured, and [`docs/planning/`](docs/planning/2026-09-27-penumbra-port.md)
holds each step with its rulings and measurements.

## Credits

**Penumbra e o Castelo das Sombras (2010)**, as its credits screen gives them (`menu.as`):

- **André Santee**: programming; scripting and game mechanics; special effects; game design.
  André Santee also wrote the Ethanon Engine.
- **Arthur Santee**: 3D modelling; game design.
- **Gabriel Duarte**: soundtrack (gabrielduarte.wordpress.com).
- **Approaching Thunderstorm**: www.freesoundtrackmusic.com.
- Special thanks (*Agradecimentos especiais*): James Hastings-Trew, for letting them use some of
  his textures (planetpixelemporium.com); José Rodolfo Ortale; Rafael "Pet" Alencar; Taina
  Monclaire.

**The enhanced edition:** Ivan Cvetanović, on his Supersonic Engine, developed with Claude Code.
The game's own Credits panel names him after the original team (*Edição aprimorada* /
*Enhanced edition*, E21).

**Magic Rampage's buttons**, used for the touch controls: Asantee Games, with the authors'
permission.

**Third-party code and fonts:** TinyXML (zlib); dr_mp3 by David Reid, based on minimp3
(public domain or MIT-0); Liberation Sans Bold (SIL OFL 1.1) and DejaVu Sans Bold (Bitstream Vera
licence) as stand-in fonts; and, through the engine, GLFW, GLM, EnTT, Dear ImGui, ImGuizmo, stb,
tinygltf, Vulkan Memory Allocator and Vulkan-Headers
([`THIRD_PARTY_LICENSES.md`](https://github.com/IvanCvetanovic/Supersonic-Engine/blob/main/THIRD_PARTY_LICENSES.md)
in the engine). The Apple builds link MoltenVK.

## Disclaimer

This project is provided **as is, without any warranty**. Ivan Cvetanović is not responsible for
anything that happens through its use - including lost data, damage to hardware, or display and
driver problems (the game can change a monitor's resolution and refresh rate). You use it at your
own risk. The full wording is in [`LICENSE.md`](LICENSE.md#no-warranty-no-liability).

## Licence

This repository holds parts under different terms; [`LICENSE.md`](LICENSE.md) lists every one.

- **Ivan Cvetanović's own code** for the enhanced edition, and the **Supersonic Engine**, are
  under the **MIT No Attribution** licence (MIT-0): free to use for any purpose, with no
  conditions ([`LICENSE`](LICENSE), `engine/LICENSE`).
- **The port of the original's scripts** (`game/script/`) and **the Ethanon runtime emulation**
  (`game/eth/`) are under the **GNU LGPL, version 3 or later**, as the code they derive from is.
  A built game includes them, so its distribution follows the LGPL for those parts.
- **The original game's files** (`extracted/`, `penumbra_setup.exe`) and **the Magic Rampage
  buttons** belong to their authors and are published here with their permission. They are not
  covered by the licences above.
- Third-party code and fonts keep their own licences.
