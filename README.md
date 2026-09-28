# Penumbra e o Castelo das Sombras — Enhanced

A remake of **Penumbra e o Castelo das Sombras** ("Penumbra and the Castle of Shadows"), a 2D
side-view action platformer released for the PC in 2010 by André Santee (Asantee). You play a
wizard with a sword, fireballs and a light spell. There are three campaign levels and a king to
beat. A second player can join the campaign as a summoned creature, or fight the first player in
Versus across six arenas.

The original ran on André Santee's **Ethanon Engine 0.7.12** (Direct3D 9, NVIDIA Cg, AngelScript).
This remake runs on **Supersonic**, a C++20/Vulkan engine by Ivan Cvetanovic, which is a git
submodule at `engine/`. The original's 25 AngelScript files are ported to C++ almost line by line.
They run on a small emulation of the Ethanon runtime, so the game plays as the original did. The
original's own art, sound and level files are read unchanged.

It is **enhanced**: widescreen, any window size, modern gamepads, a keyboard second player,
English text and settings that persist are all added (see [Enhancements](#enhancements)). Each
change is listed with what the original did.

It is remade with the permission of the original's authors.

## Playing it

### The packaged game

`tools\package.bat` puts a copy of the game in `out\package\Penumbra\` that plays outside this
repository. Copy that folder anywhere and run `Penumbra.exe`; it does not matter which folder it
is started from.

```
Penumbra\
  Penumbra.exe
  assets\shaders\*.spv     the engine's shaders
  data\                    the remake's own data: strings.json (English), images\en
  original\                the original game's data files (scenes, entities, sounds, ...)
  *.dll                    the Visual C++ runtime
  README.md, licenses\
```

You need:

- Windows 10 or 11 (64-bit).
- A graphics driver that supports Vulkan 1.2.
- Media Foundation, which decodes the MP3 music. An "N" edition of Windows needs the Media
  Feature Pack.

`Penumbra.exe` is a console program, so a console window opens beside the game and shows its log.

The game writes its own files into its folder: the engine's shader pipeline cache
(`cache\pipeline_cache.bin`) and an empty `assets\scenes\`. If the folder is read-only the game
still runs, but it logs that it could not write them. Saves go elsewhere (see
[Settings and saves](#settings-and-saves)).

Text is drawn with the Windows fonts in `%WINDIR%\Fonts`. The original used Arial Narrow, which
comes with Microsoft Office rather than Windows. When it is missing, Arial Bold is used instead
(`game/render/FontAtlas.cpp`).

### From a build

```bash
build/game/Penumbra.exe
```

A build finds the original's files in `extracted/app` and the remake's data in `game/data`, as
absolute paths baked in by CMake. It finds the engine's shaders in `engine/assets/shaders`. It
can therefore be started from any directory.

### Where the files are found

At startup the game looks for two folders, and logs where it found each one:

| | What marks it | Looked for, in order |
|---|---|---|
| The original game | `data.enml` | `--original <dir>`; `original\` beside the executable; the build's `extracted/app` |
| The remake's data | `strings.json` | `--data <dir>`; `data\` beside the executable; the build's `game/data` |

- If a folder named with `--original` or `--data` does not hold the file that marks it, the game
  refuses to start. It does not fall back to another folder.
- Without the original, the game stops with a message.
- Without the remake's data, the game still runs, in Portuguese with the original's images.
- The engine finds its own shaders by a similar rule: the executable's folder if it holds
  `assets\shaders`, else the working directory, else the engine checkout the build names
  (`engine/README.md`, "Building a game against the engine").
- The game opens these files by narrow (code page) paths, as the original did. If the path to the
  original's folder has characters outside the system code page, the game refuses to start and
  says so; move the folder to a plainer path.

## Controls

### Player 1, keyboard

These are the original's keys:

| Action | Key |
|---|---|
| Walk | ← → |
| Jump | ↑ or Ctrl |
| Sword | S |
| Fireball (10 mana) | D |
| Light spell (50 mana) | Space |
| Confirm (menus) | Enter; the mouse drives the main menu |
| Pause (in a level or an arena; Main menu inside) | Esc |
| Fullscreen / window | Alt+Enter |

Esc in the menus, the options and game over goes back as in the original. In the pause, the arrows
choose, Enter confirms and Esc resumes; the mouse works too.

**Combos.** Each gap between presses must be at most about 210 ms (`combo.as`,
`docs/spec/11-logic-player-combat.md` §10):

- **Sword combo (5 mana):** ← ← S or → → S. A stronger sword and a beam; the screen shakes.
- **Blast (25 mana):** ↓ ← D or ↓ → D. A big fireball with 225 damage.

**The original's hidden keys.** When the menu's fade ends on New Game:

- Hold 2 or 3 to start at level 2 or level 3.
- Hold Page Up to start both characters at level 15 (`main.as`).

### Player 2, keyboard (enhancement E4)

This is on by default. Player 2 is presented to the game as the joystick that player 2 reads.

| Action | Key |
|---|---|
| Walk | J L |
| Up / down | I K |
| Jump | I |
| Sword | U |
| Fireball | O |
| Light spell | P |
| Start (summon the creature in the campaign) | Backspace |

- In the campaign, player 2 joins by pressing Start. The summon costs player 1 50 mana and a life.
- Only player 1 can take checkpoints and finish a level.

### Gamepads (enhancement E3)

Any pad that GLFW recognises as a gamepad (an Xbox layout, for example) is mapped by what each
button means onto the button numbers the original read:

| Action | Pad | Original's button |
|---|---|---|
| Walk / up / down | Left stick or D-pad | axes |
| Jump | A | 3 |
| Sword | X | 4 |
| Fireball | B | 2 |
| Light spell | Y | 1 |
| Confirm / summon | Start | 10 |
| Pause (player 1, in a level or an arena) / back | Back | 9 |
| Confirm / back in the menus (E14) | A / B | 10 / 9 |

- Pads are read every frame. The original's "hold J to detect joysticks" is no longer needed.
- By default the first pad plays the wizard (player 1) and drives the menu, and a second pad plays
  the princess (E12). In 2010 the first pad was player 2's; `firstPadIsPlayer1: false` in
  `settings.json` restores that, and the options screen's input switch (the original's) still swaps
  which joystick each player reads.

The keyboard bindings of both players are in `settings.json` and can be changed there.

### Touch controls (enhancement E16)

On a phone or a tablet the game draws its own buttons (`touchControls` in `settings.json`: `auto`, on
in the Android and iOS builds; `on`; `off`). On a desktop, `--touch` shows them and the mouse, held
down, is the finger.

| Action | Touch | The key it presses |
|---|---|---|
| Walk / down | The disc at the bottom left: slide the thumb | Left, Right, Down |
| Jump | Bottom button of the four at the bottom right | Ctrl |
| Sword | Left button | S |
| Fireball | Right button | D |
| Light spell | Top button | Space |
| Sword combo | The left button of the two above the four | the way he faces twice, then S |
| Spell combo | The right button of the two above the four | Down, the way he faces, then D |
| Pause (in a level) / back (arena select, options, game over) | The button at the top right | Esc |

- Several fingers work at once: hold the disc and tap the buttons.
- A combo button presses its combo's keys one a tick, toward the way the wizard faces. If a
  direction, the sword or the fireball was pressed in the last fifth of a second, it waits until the
  game's combo memory has emptied (the original forgets a combo 210 ms after its last key), so the
  combo always registers. While it runs the disc, the sword and the fireball buttons wait; jump,
  light and pause do not. A second tap is ignored until it is done; the pause or a new scene stops it.
- In the menus, the options, game over and the pause, the buttons are hidden and a tap clicks
  where it lands.
- The buttons are placeholders. Their images and layout are in `data/touch_controls.json` and
  `data/images/touch/`, so new art is a data change; `"enabled": false` there removes a button
  (the combo buttons, say).

## Enhancements

Each enhancement is listed with what the original did. The full record is
`docs/planning/2026-09-27-penumbra-port.md`.

| # | Enhancement | The original |
|---|---|---|
| E1 | Widescreen: the view is 768 logical pixels tall and as wide as the window. The menus stay 1024x768, pillarboxed. | 1024x768 only |
| E2 | Any window size, and a real fullscreen at the monitor's resolution, drawn at native resolution. Alt+Enter and the options screen switch between them. | 1024x768 or one of the listed video modes |
| E3 | Modern gamepads, mapped by meaning, for both players | Button numbers of the pad's own winmm driver |
| E4 | A keyboard second player | Player 2 needed a joystick |
| E5 | English alongside Portuguese, including the words baked into the menu art | Portuguese only |
| E6 | Settings are kept: language, window, volumes, controls | Options reset at every launch |
| E7 | Plain bugs fixed. `data.enml` has no level-20 threshold, so a player skipped level 20, and past level 30 the experience code never returned. | — |
| E8 | Smooth motion on displays faster than 60 Hz: the world (sprites, lights, shadows, particles, camera) is drawn between the last two ticks, one tick behind. On the options screen and as `smoothMotion` in `settings.json`; off under `--fixed-step` unless `--smooth on`. | One tick per 60 Hz vsync |
| E9 | Shadows drawn live and ending at the light's reach | Shadows baked into lightmaps, eight times the caster's height |
| E10 | The options screen also sets keyboard player 2, widescreen or 4:3 levels, Portuguese or English, the music and effects volumes in 10% steps, and smooth motion (E8), all saved | The screen had the video modes, pixel shaders, window/fullscreen and the joystick layout, all forgotten at exit |
| E11 | Standing on two floor tiles at once counts as standing | The last tile decided, so the wizard was airborne for one frame at each seam |
| E12 | One gamepad plays the wizard and drives the menu; a second plays the princess | The first pad was player 2's |
| E13 | A pause: in a level or an arena, Esc or player 1's Back freezes the game (the clock too, so best times leave it out) under Resume / Main menu, with the music at 40%. It also opens when the window loses focus (`pauseOnFocusLoss`, a row on the options screen). Not on the end screens, where Esc and Back still go to the menu. | No pause: Esc in a level went straight to the main menu and the run was lost |
| E14 | In the menus, the arena select, the options and game over, a pad's A also confirms and B also goes back | Only Start confirmed and only Back went back |
| E16 | On-screen touch controls for phones and tablets: a direction disc, jump, sword, fireball, light, the two combos and pause, pressing player 1's own keys; a tap clicks in the menus. Placeholder art, laid out by `data/touch_controls.json`. `touchControls` in `settings.json`, `--touch` on a desktop. | Keyboard and joysticks only |

## Settings and saves

Everything the game writes for the player goes to `%APPDATA%\Penumbra\`. Nothing is ever written
into the original's folder.

| File | What it is |
|---|---|
| `settings.json` | Language (`"en"`/`"pt"`), window size and fullscreen, widescreen, volumes, pixel shaders, `smoothMotion`, `pauseOnFocusLoss`, `touchControls` (`"auto"`, `"on"`, `"off"`), and the controls: `joystickLayout`, `keyboardPlayer2`, `firstPadIsPlayer1`, `rawJoysticks`, `stickDeadzone`, and the `player1`/`player2` key lists. A broken or missing field falls back to its default, field by field. |
| `hs.enml` | The best times, written after a new record. Until then the original's `hs.enml` is read. |
| `scenes\checkpoint.esc` | The level saved at the last checkpoint. |

The first language follows the system: Portuguese on a Portuguese Windows, or where the POSIX
locale (`LC_ALL`, `LC_MESSAGES`, `LANG`) starts with `pt`. It is English otherwise. On Linux the
files go to `$XDG_DATA_HOME/Penumbra`, else `~/.local/share/Penumbra`. On macOS they go to
`~/Library/Application Support/Penumbra`.

## Command line

The game's own options:

| Option | What it does |
|---|---|
| `--start <scene>` | Skip the menu and start `scenes/<scene>.esc`: `level1`–`level3` or `pvp_lv1`–`pvp_lv6`; `arena_select`, `gameover` and `videoModes` start as the scripts start them |
| `--tour <a,b,...>@<N>` | After the menu, start each scene in turn for *N* ticks: many screens in one launch |
| `--lang pt\|en` | This run's language. It is not saved. |
| `--widescreen on\|off` | This run's view. It is not saved. |
| `--smooth on\|off` | This run's motion between ticks (E8). It is not saved. Off under `--fixed-step` unless given as `on`, so fixed-step captures show the ticks themselves. |
| `--original <dir>`, `--data <dir>` | Where the original's files and the remake's data are ([above](#where-the-files-are-found)) |
| `--hold <KEY>@<a>-<b>` | Hold an Ethanon key from tick *a* to tick *b*, for scripted captures. KEY is one of `UP DOWN LEFT RIGHT CTRL ALT SHIFT SPACE ENTER ESC BACKSPACE PAGEUP PAGEDOWN J S D 1 2 3 LMOUSE RMOUSE`. |
| `--cursor <x>,<y>` | Pin the scripts' cursor at a point of the 1024x768 menu screen, for menu captures |
| `--touch [on\|off]` | This run's touch controls (E16); on by itself. On a desktop the held left mouse button is the finger. It is not saved. |

The engine's options that matter here:

| Option | What it does |
|---|---|
| `--window <W>x<H>` | Open a window of this size. This implies a windowed run unless `--fullscreen` is also given. |
| `--fullscreen`, `--windowed` | This run only; not saved |
| `--frames <n>` | Run *n* frames, then exit |
| `--fixed-step` | Simulate at exactly 1/60 s per frame, so a run reproduces |
| `--screenshot <path>` | Write a PNG of the last frame and exit. Give an **absolute** path: the engine changes the working directory at startup. |
| `--help` | List the options |

For example, a headless capture of level 1 after five seconds:

```bash
build/game/Penumbra.exe --start level1 --window 1280x720 --fixed-step --frames 300 \
    --screenshot "$PWD/out/shots/level1.png"
```

## Building

Prerequisites (Windows, x64):

- Visual Studio Build Tools 18 with the C++ workload (MSVC).
- The Vulkan SDK 1.4.357.0, at `C:\VulkanSDK\1.4.357.0` as `tools\build.bat` and
  `tools\check.bat` expect. The engine itself can configure without it, from its bundled
  headers and the driver's loader, but then there are no validation layers and `check.bat`
  finds no Vulkan headers.
- CMake 3.20 or newer, and Ninja. Strawberry Perl's `c\bin` provides both on the machine this
  was written on.
- Git, with the submodule: `git clone --recurse-submodules`, or `git submodule update --init`
  in an existing clone.

From Git Bash at the repository root:

```bash
cmd //c "tools\build.bat --target Penumbra"    # the game
cmd //c "tools\build.bat"                      # the game and every test suite
cmd //c "tools\package.bat"                    # then: out/package/Penumbra/
```

From `cmd`, run `tools\build.bat --target Penumbra` and `tools\package.bat`.

`tools\build.bat` loads MSVC's environment, then configures `build\` on the first run: Ninja,
Release, Vulkan validation on. After that it builds. The paths to the Build Tools and to the
SDK's `glslc` are written in it; edit them if yours differ. The game's code builds with no
warnings at `/W4`.

`tools\check.bat <files>` compiles single `.cpp` files with the build's flags without touching
`build\`.

### Linux

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
Wayland. No `glslc` is needed, because the engine's committed SPIR-V is used. The script also
copies the shaders beside the executable, so the game anchors its working directory there. The
game builds without warnings at `-Wall -Wextra -Wpedantic`, the engine's level for its core. The
suites build without warnings at `-Wall -Wextra`, the engine's level for its own suites.

The Microsoft fonts are not used off Windows. The bundled stand-ins in `game/data/fonts` take
their place, drawn with the same metrics. The original's MP3s are decoded by dr_mp3, because the
engine decodes MP3 only through Windows' Media Foundation. With no sound device, as in WSL, the
game runs silent.

### Android

A debug APK is built on Windows from Git Bash, without Gradle, with the Android SDK in
`%LOCALAPPDATA%\Android\Sdk` (NDK 28.2.13676358, build-tools 35.0.0, platform 35), a JDK 17 and
Python 3 with Pillow:

```bash
bash tools/build_android.sh               # x86_64 (the emulator's): configure, build, package
bash tools/build_android.sh --abi all     # x86_64 + arm64-v8a (phones) in one APK
bash tools/build_android.sh --install --serial <device>   # adb install -r
```

The APK is `out/android/Penumbra-debug.apk` (about 18 MB for both ABIs), debug-signed. It is a
NativeActivity with no Java: the engine's `android_main` (engine/src/platform/android) runs the
game, `game/android/AndroidMain.cpp` unpacks the original's files and the port's data from the
APK into the app's private storage on the first launch (and again only when they change), and
passes them with `--original`/`--data`. Landscape only; minimum Android 8.0 (API 26, for AAudio);
Vulkan 1.2 is required. The touch controls (E16) are on by default, keyboard player 2 is off (a
phone has no keyboard: Versus waits for a second pad), and the Back key is Esc. Development flags
go in `penumbra_args.txt` in the app's files directory (`tools/build_android.sh --run "<flags>"`).

## Tests

The suites are `tests/test_pn_*.cpp`, one executable each, built by the engine's
`supersonic_add_test`. A suite that needs the original's files exits 77 (reported as skipped)
when they are missing.

```bash
ctest --test-dir build --output-on-failure    # all of them, each through its own executable
build/tests/test_pn_all.exe                   # all of them, through one executable
build/tests/test_pn_all.exe --suite boot      # one, as build/tests/test_pn_boot.exe runs it
build/tests/test_pn_all.exe --list            # the suites test_pn_all holds
build/tests/test_pn_scenarios.exe             # one
```

`test_pn_all` exists for Smart App Control, which on the development laptop judges every
freshly linked executable on its first launch and refuses some. A build relinks every suite
and so gives it one chance to refuse per suite; `test_pn_all` is one executable, so one
judgement per build. It holds every `tests/test_pn_*.cpp`, found by the same glob as the
suites' own executables, which stay as they are. Each suite is compiled from a generated
wrapper (`tests/all/WrapSuite.cmake`, rewritten whenever the suite changes): the suite's own
`#include`s, then the suite inside a namespace of its own, so that suites keep their `main()`
and their same-named helpers apart. A suite must include its headers at the top of the file,
outside any `#if`; the wrapper refuses otherwise.

Every suite still runs in a process of its own, a child of `test_pn_all` itself (the same
file, so the same verdict). The ported script's globals live for the whole program, as they
did in the original, so the suites that boot the real game cannot share one. Each suite
prints what its own executable prints, its summary line included; `test_pn_all` adds a line
per suite and a total. It exits 0 when nothing failed, 1 when a suite failed, crashed, timed
out (`--timeout <seconds>`, default 1500) or could not be started (it then starts no further
suite), and 77 when every suite skipped. It is not registered with ctest: ctest already runs
every suite through its own executable, and a second registration would run each twice.

| Suite | What it checks |
|---|---|
| `test_pn_smoke` | The build finds the original |
| `test_pn_paths` | Where the original and the data are found: flag, beside the executable, the build's path |
| `test_pn_formats` | The original's files: every scene and entity parses, the 0.7.12 writer reproduces the shipped scenes byte for byte, ENML, image headers, cp1252 text |
| `test_pn_runtime` | The emulated Ethanon runtime: buckets, custom data, callbacks, frame order, input, samples |
| `test_pn_particles` | The Ethanon particle manager |
| `test_pn_boot` | The real game boots headless: the menu, level 1, the wizard walks, jumps and swings |
| `test_pn_scenarios` | The real game played headless: combat, spells and both combos, potions, hazards, checkpoints, game over, level changes, the king, Versus, the co-op creature |
| `test_pn_audio` | Every sound the original ships decodes (19 Ogg Vorbis, 16 MP3) |
| `test_pn_render_textures` | Colour keys, DDS, normal maps, blend variants |
| `test_pn_render_lights` | Lights, halos and projected shadows |
| `test_pn_render_particles` | Particles drawn as pooled quads |
| `test_pn_render_hud` | `strings.json` against every string the scripts draw, the font layout, the HUD's quads |
| `test_pn_render_english` | The English images: each variant exists at the original's size, and the renderers swap them with the language |
| `test_pn_render_input` | Keys, pads and the keyboard second player mapped onto the original's input; A and B in the menus (E14); the settings file |
| `test_pn_render_interp` | Smooth motion (E8): the blend between two ticks, whole pixels kept whole, never across a scene load, a frame gap or a jump |
| `test_pn_render_pause` | The pause (E13): when it opens, the frozen ticks, the menu, the one-tick cancel to the main menu, focus loss, the overlay, the input held back after it |
| `test_pn_render_touch` | The touch controls (E16): the key each control presses, the direction disc, several fingers at once, the pause opened and tapped, a tap in a menu as a click, the layout on 4:3 and widescreen with a safe area, the manifest and its art, the setting; the combo buttons' key timelines, and the combos firing through the ported combo buffer and in level 1 |

## Repository layout

```
CMakeLists.txt       the engine as a subproject, then game/ and tests/
engine/              Supersonic, a git submodule pinned to a commit
game/
  eth/               the Ethanon 0.7.12 runtime, emulated (no renderer); Paths.* finds the files
  script/            the original's .as files ported to C++, one .cpp per .as
  render/            drawing on the engine: sprites, lights, shadows, particles, text, HUD, input, audio
  data/              the remake's own data: strings.json (English) and images/en
  windows/           the icon resource, version information and manifest
  PenumbraLayer.*    the engine layer that runs one Ethanon frame per 60 Hz tick and draws it
  main.cpp
tests/               the test_pn_* suites; all/ builds every one of them into test_pn_all
tools/               build.bat, check.bat, package.bat; art/ makes the English images
docs/spec/           what the original is and does, decoded, with citations
docs/planning/       the port's step record, rulings and enhancements
extracted/app/       the original game as installed; read, never written
```

## Credits

**Penumbra e o Castelo das Sombras (2010)**, as its credits screen gives them (`menu.as`):

- **André Santee**: programming, scripting and game mechanics, special effects, game design.
  André Santee also wrote the Ethanon Engine.
- **Arthur Santee**: 3D modelling, game design.
- **Gabriel Duarte**: soundtrack (gabrielduarte.wordpress.com).
- **Approaching Thunderstorm**: www.freesoundtrackmusic.com.
- Special thanks: James Hastings-Trew, for some of his textures (planetpixelemporium.com); José
  Rodolfo Ortale; Rafael "Pet" Alencar; Taina Monclaire.

**This remake:** Ivan Cvetanovic, on his Supersonic engine.

## Licences

- The original's scripts (`extracted/app/*.as`) are free software under the GNU Lesser General
  Public License, version 3 or (at your option) any later version, as their headers say. The C++
  ports in `game/script/` keep that notice.
- TinyXML (`game/third_party/tinyxml`), from the Ethanon 0.7.12 tree, is under the zlib licence.
- dr_mp3 (`game/third_party/dr_mp3`) is public domain or MIT No Attribution. The bundled
  stand-in fonts (`game/data/fonts`) are Liberation Sans Bold (SIL OFL 1.1) and DejaVu Sans Bold
  (Bitstream Vera licence). Their licence texts are in that folder.
- The Supersonic engine is under the MIT licence (`engine/LICENSE`). Its third-party components
  are listed in `engine/THIRD_PARTY_LICENSES.md`.
- The original's art, music and sound are the work of the people credited above.
