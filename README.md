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
| Back to the menu | Esc |
| Fullscreen / window | Alt+Enter |

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
| Back | Back | 9 |

- Pads are read every frame. The original's "hold J to detect joysticks" is no longer needed.
- By default the first pad is player 2's, as in 2010: the keyboard is player 1.
- The options screen's input switch (the original's) changes that, and so does
  `firstPadIsPlayer1` in `settings.json`.

The keyboard bindings of both players are in `settings.json` and can be changed there.

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
| E8 | Smooth motion on displays faster than 60 Hz: the world (sprites, lights, shadows, particles, camera) is drawn between the last two ticks, one tick behind. `smoothMotion` in `settings.json`; off under `--fixed-step` unless `--smooth on`. | One tick per 60 Hz vsync |
| E9 | Shadows drawn live and ending at the light's reach | Shadows baked into lightmaps, eight times the caster's height |
| E10 | The options screen also sets keyboard player 2, widescreen or 4:3 levels, Portuguese or English, and the music and effects volumes in 10% steps, all saved | The screen had the video modes, pixel shaders, window/fullscreen and the joystick layout, all forgotten at exit |

## Settings and saves

Everything the game writes for the player goes to `%APPDATA%\Penumbra\`. Nothing is ever written
into the original's folder.

| File | What it is |
|---|---|
| `settings.json` | Language (`"en"`/`"pt"`), window size and fullscreen, widescreen, volumes, pixel shaders, `smoothMotion`, and the controls: `joystickLayout`, `keyboardPlayer2`, `firstPadIsPlayer1`, `rawJoysticks`, `stickDeadzone`, and the `player1`/`player2` key lists. A broken or missing field falls back to its default, field by field. |
| `hs.enml` | The best times, written after a new record. Until then the original's `hs.enml` is read. |
| `scenes\checkpoint.esc` | The level saved at the last checkpoint. |

The first language follows Windows: Portuguese on a Portuguese Windows, English otherwise.

## Command line

The game's own options:

| Option | What it does |
|---|---|
| `--start <scene>` | Skip the menu and start `scenes/<scene>.esc`: `level1`–`level3` or `pvp_lv1`–`pvp_lv6` |
| `--lang pt\|en` | This run's language. It is not saved. |
| `--widescreen on\|off` | This run's view. It is not saved. |
| `--smooth on\|off` | This run's motion between ticks (E8). It is not saved. Off under `--fixed-step` unless given as `on`, so fixed-step captures show the ticks themselves. |
| `--original <dir>`, `--data <dir>` | Where the original's files and the remake's data are ([above](#where-the-files-are-found)) |
| `--hold <KEY>@<a>-<b>` | Hold an Ethanon key from tick *a* to tick *b*, for scripted captures. KEY is one of `UP DOWN LEFT RIGHT CTRL ALT SHIFT SPACE ENTER ESC BACKSPACE PAGEUP PAGEDOWN J S D 1 2 3 LMOUSE RMOUSE`. |
| `--cursor <x>,<y>` | Pin the scripts' cursor at a point of the 1024x768 menu screen, for menu captures |

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

## Tests

The suites are `tests/test_pn_*.cpp`, one executable each, built by the engine's
`supersonic_add_test`. A suite that needs the original's files exits 77 (reported as skipped)
when they are missing.

```bash
ctest --test-dir build --output-on-failure    # all of them
build/tests/test_pn_scenarios.exe             # one
```

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
| `test_pn_render_input` | Keys, pads and the keyboard second player mapped onto the original's input; the settings file |
| `test_pn_render_interp` | Smooth motion (E8): the blend between two ticks, whole pixels kept whole, never across a scene load, a frame gap or a jump |

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
tests/               the test_pn_* suites
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
- The Supersonic engine is under the MIT licence (`engine/LICENSE`). Its third-party components
  are listed in `engine/THIRD_PARTY_LICENSES.md`.
- The original's art, music and sound are the work of the people credited above.
