# Playing

How to run the game once it is built ([building.md](building.md)), where it looks for its files,
what it saves, and its command line. The controls are in [controls.md](controls.md).

## The packaged game (Windows)

`tools\package.bat` puts a copy of the game in `out\package\Penumbra\` that plays outside this
repository. Copy that folder anywhere and run `Penumbra.exe`; it does not matter which folder it
is started from.

```
Penumbra\
  Penumbra.exe
  assets\shaders\*.spv     the engine's shaders
  data\                    the enhanced edition's own data: strings.json (English), images\en, fonts, touch art
  original\                the original game's data files (scenes, entities, sounds, ...)
  *.dll                    the Visual C++ runtime
  README.md, licenses\
```

You need:

- Windows 10 or 11 (64-bit).
- A graphics driver that supports Vulkan 1.2.
- Windows' media features (Media Foundation), which decode the music. Every edition of Windows has
  them except the "N" editions, which get them from Microsoft's free Media Feature Pack (Settings,
  Apps, Optional features, Add a feature). `Penumbra.exe` imports Media Foundation (`MFPlat.DLL`,
  `MFReadWrite.dll`) when it loads, not on demand, so without them Windows does not start the game
  at all: it says "MFPlat.DLL was not found". The game's own MP3 decoder (enhancement E18) is for
  the other platforms, and on Windows only for a file Media Foundation refuses.

If Windows says "VCRUNTIME140.dll was not found" or "MSVCP140.dll was not found", the game was
started from inside the zip: Explorer then copies out `Penumbra.exe` alone, without the Visual C++
runtime that sits beside it in the folder. Extract the whole zip first. (On a PC that has the
Visual C++ Redistributable installed, the game starts instead and says it could not find its game
files, for the same reason.)

The game writes its own files into its folder: the engine's shader pipeline cache
(`cache\pipeline_cache.bin`) and an empty `assets\scenes\`. If the folder is read-only, the game
still runs, but it logs that it could not write them. Saves go elsewhere (see
[Settings and saves](#settings-and-saves)).

The first launch covers the screen at the monitor's own resolution and its highest refresh rate
(E23); Alt+Enter gives a window sized to the monitor. Both can be chosen by hand on the options
screen (Settings): the resolution list, whose first line "Automatic (best)" goes back to the
automatic choice, and the "Refresh rate" row beside the Windowed/Fullscreen switch.

Text is drawn with the Windows fonts in `%WINDIR%\Fonts`. The original used Arial Narrow, which
comes with Microsoft Office rather than Windows. When a face is missing, the bundled stand-in drawn
at its metrics is used instead (E17, `game/render/FontAtlas.cpp`).

## From a build

```bash
build/game/Penumbra.exe
```

A build finds the original's files in `extracted/app` and the enhanced edition's data in `game/data`, as
absolute paths baked in by CMake. It finds the engine's shaders in `engine/assets/shaders`. It
can therefore be started from any directory. The Linux, Android and Apple builds are described in
[building.md](building.md).

## Where the files are found

At startup the game looks for two folders, and logs where it found each one:

| | What marks it | Looked for, in order |
|---|---|---|
| The original game | `data.enml` | `--original <dir>`; `original\` beside the executable; the build's `extracted/app` |
| The enhanced edition's data | `strings.json` | `--data <dir>`; `data\` beside the executable; the build's `game/data` |

- If a folder named with `--original` or `--data` does not hold the file that marks it, the game
  refuses to start. It does not fall back to another folder.
- Without the original, the game stops with a message.
- Without the enhanced edition's data, the game still runs, in Portuguese with the original's images.
- The engine finds its own shaders by a similar rule: the executable's folder if it holds
  `assets\shaders`, else the working directory, else the engine checkout the build names
  (`engine/README.md`, "Building a game against the engine").
- The game opens these files by narrow (code page) paths, as the original did. If the path to the
  original's folder has characters outside the system code page, the game refuses to start and
  says so; move the folder to a plainer path.

## Settings and saves

Everything the game writes for the player goes to `%APPDATA%\Penumbra\` on Windows. Nothing is
ever written into the original's folder.

| File | What it is |
|---|---|
| `settings.json` | Language (`"en"`/`"pt"`); the window (`window`: `width`/`height`, `0` for a window fitted to the monitor; `fullscreen`, true on a first launch; the fullscreen mode, `fullscreenWidth`/`fullscreenHeight`, `0` for the desktop's resolution; `fullscreenRefresh` in Hz, `0` for the highest the monitor offers at that resolution); widescreen, volumes, pixel shaders, `smoothMotion`, `pauseOnFocusLoss`, `touchControls` (`"auto"`, `"on"`, `"off"`); and the controls: `joystickLayout`, `keyboardPlayer2`, `firstPadIsPlayer1`, `rawJoysticks`, `stickDeadzone`, and the `player1`/`player2` key lists. A broken or missing field falls back to its default, field by field. |
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
| `--original <dir>`, `--data <dir>` | Where the original's files and the enhanced edition's data are ([above](#where-the-files-are-found)) |
| `--hold <KEY>@<a>-<b>` | Hold an Ethanon key from tick *a* to tick *b*, for scripted captures. KEY is one of `UP DOWN LEFT RIGHT CTRL ALT SHIFT SPACE ENTER ESC BACKSPACE PAGEUP PAGEDOWN J S D 1 2 3 LMOUSE RMOUSE`. |
| `--cursor <x>,<y>` | Pin the scripts' cursor at a point of the 1024x768 menu screen, for menu captures |
| `--touch [on\|off]` | This run's touch controls (E16); on by itself. On a desktop the held left mouse button is the finger. It is not saved. |
| `--refresh auto\|<Hz>` | This run's fullscreen refresh rate (E23). It is not saved; a pick on the options screen replaces it. A rate the monitor does not offer at the fullscreen resolution runs at the highest. |
| `--modes <W>x<H>@<Hz>,...` | List these display modes on the options screen instead of the monitor's, a `*` after one marking the desktop's (captures). A pick still goes to the real monitor. |

The engine's options that matter here:

| Option | What it does |
|---|---|
| `--window <W>x<H>` | Open a window of this size. This implies a windowed run unless `--fullscreen` is also given. |
| `--fullscreen`, `--windowed` | This run only; not saved. `--fullscreen` runs at the saved fullscreen mode, or the automatic one (E23). |
| `--frames <n>` | Run *n* frames, then exit |
| `--fixed-step` | Simulate at exactly 1/60 s per frame, so a run reproduces |
| `--screenshot <path>` | Write a PNG of the last frame and exit. Give an **absolute** path: the engine changes the working directory at startup. |
| `--help` | List the options |

For example, a headless capture of level 1 after five seconds:

```bash
build/game/Penumbra.exe --start level1 --window 1280x720 --fixed-step --frames 300 \
    --screenshot "$PWD/out/shots/level1.png"
```
