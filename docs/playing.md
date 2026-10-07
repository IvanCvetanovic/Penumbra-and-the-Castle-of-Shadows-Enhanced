# Playing

How to run the game once it is built ([building.md](building.md)) or downloaded, where it looks
for its files, what it saves, and its command line. The controls are in [controls.md](controls.md).
Installing a download, step by step: [install.md](install.md) (English), [install.pt.md](install.pt.md) (Português).

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

## The Linux download

`Penumbra-Linux.tar.gz` ([download](https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/releases/latest/download/Penumbra-Linux.tar.gz))
is made by the release workflow (`.github/workflows/release.yml`, `tools/make_release.py linux`)
in an Ubuntu 22.04 container. It holds the Windows package's layout, with the Linux program:

```
HOW TO PLAY.txt            game/linux/how-to-play.txt, English and Portuguese
Penumbra/
  Penumbra                 the program
  assets/shaders/*.spv     the engine's shaders
  data/                    the enhanced edition's own data
  original/                the original game's data files
  README.md, LICENSE.txt, docs/, licenses/
```

Extract it (the file manager's Extract Here, or `tar xzf Penumbra-Linux.tar.gz`), open the
`Penumbra` folder and double-click `Penumbra` (choose Run if the file manager asks), or run
`./Penumbra` there in a terminal.

You need:

- A 64-bit PC (x86_64) with glibc 2.35 or newer. The program is built in Ubuntu 22.04, so this is
  most desktop Linux from about 2022 on: Ubuntu 22.04+, Debian 12+, Fedora, Arch, Linux Mint 21+,
  SteamOS.
- A graphics driver with Vulkan 1.2: `mesa-vulkan-drivers` on Ubuntu, Debian and Fedora (AMD and
  Intel graphics), `vulkan-radeon` or `vulkan-intel` on Arch. NVIDIA's own driver includes it.
- An X11 desktop, or Wayland with XWayland.

Every test suite passes in the release build, and the package itself, unpacked into a read-only
folder and run as an ordinary user, plays level 1 on a software Vulkan driver (lavapipe) under a
virtual screen. With no screen it exits with an error at once. Nobody has played it on a real Linux
PC yet. Settings, saves and the log go to `~/.local/share/Penumbra` ([below](#settings-and-saves)).

## The Mac download

`Penumbra-macOS.zip` ([download](https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/releases/latest/download/Penumbra-macOS.zip))
is made by the release workflow (`tools/apple/make_release.sh macos`). It holds one folder:

```
Penumbra/
  Penumbra.app             the game, one program for Apple silicon and Intel Macs, with its data inside
  HOW TO PLAY.txt          game/macos/how-to-play.txt, English and Portuguese
```

It needs macOS 13.3 Ventura or newer. Safari unzips the download by itself; otherwise double-click
the zip. Open the `Penumbra` folder and drag the `Penumbra` app into Applications.

The app is signed ad hoc only and not notarised by Apple ([code-signing.md](code-signing.md)), so
the first open takes one more step:

- macOS 15 Sequoia and newer: double-click it, and at "Penumbra" Not Opened click Done (not Move to
  Trash). Then Apple menu, System Settings, Privacy & Security, scroll down to Security, click Open
  Anyway, enter the Mac's password, and click Open Anyway again.
- macOS 13 Ventura and 14 Sonoma: close the message (don't choose Move to Trash), then Control-click
  (or right-click) the app, choose Open, then Open.

After that it opens with a double-click. If macOS says it "is damaged and can't be opened",
download it again and unzip it with Finder.

It has been built and run from the unpacked zip on GitHub's Apple silicon Macs (level 1 drawn),
and its Intel half run there through Rosetta (level 1 drawn); the automated suites pass on macOS
in CI. Nobody has played it on a real Mac, and it has never run on a real Intel Mac's graphics.

## iPhone and iPad (experimental)

`Penumbra-iOS.ipa` ([download](https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/releases/latest/download/Penumbra-iOS.ipa))
is the device build (arm64) for iOS and iPadOS 16.3 or newer, made by the release workflow
(`tools/apple/make_release.sh ios`). **It has never run on any iPhone or iPad**: Apple's simulator
cannot draw it, and real devices should be able to, but nobody has tried, so it may not start.

It is not on the App Store and cannot be installed by tapping it. It needs a Windows PC or a Mac
and a free sideloading tool, Sideloadly (on the computer) or AltStore or SideStore, which signs it
with the player's Apple ID (a spare Apple ID is fine):

1. Download the `.ipa` on the computer.
2. Install it with the tool.
3. On the device, turn on Settings, Privacy & Security, Developer Mode, and restart.
4. Trust the Apple ID under Settings, General, VPN & Device Management.
5. Open Penumbra and hold the device sideways. The game draws its own buttons on the screen; a
   controller should work too.

With a free Apple ID the app stops opening after 7 days, until it is refreshed (AltStore,
SideStore) or installed again (Sideloadly). Progress stays unless the app is deleted. A free Apple
ID can have 3 sideloaded apps at a time.

## From a build

```bash
build/game/Penumbra.exe
```

A build finds the original's files in `extracted/app` and the enhanced edition's data in `game/data`, as
absolute paths baked in by CMake. It finds the engine's shaders in `engine/assets/shaders`. It
can therefore be started from any directory. The Linux, Android and Apple builds are described in
[building.md](building.md).

## When the game starts

A normal start opens with the logo of the Supersonic Engine, which the game runs on, for two seconds (E35), and then the menu.
Any key, mouse button, tap or gamepad button skips it once its first 0.3 seconds are over: the logo fades out and the menu
follows, and the press is not also a click or a confirm on the menu. `--splash off` removes the intro and `--splash on` plays it
with any other option; a start with an option that is not a player's own (`--start`, `--frames`, `--screenshot`, `--fixed-step`,
`--hold`...) has none, so scripted captures are as they were. A copy of the game without its `data\images\splash\` folder starts
without it and says so in `penumbra.log`.

### A start that takes long, or stalls (E38)

This is in the builds after 1.0.5; 1.0.5 itself starts as it did before (its log has no times, and it opens fullscreen after a start that froze).

The window is on the screen from the moment it is made, and the start behind it (the graphics device, every shader the driver compiles
on a first run, the game's own load) can take seconds on a slow PC. Windows calls a window that reads no messages for about five
seconds "Not responding", so the game now reads the window's messages between the engine's stages of that start and around the
game's own load: a long start is a blank window that can be moved and closed, and Windows does not offer to end it unless one single
stage takes that long (a display switch, say). A window minimised during the start is waited out before the swapchain is made, and a
close asked for during it is heard: the game's own load is skipped and the window closes when the engine's stages are through.

A first start is fullscreen, and asks the monitor for its highest refresh rate; with the intro on, that switch is asked for once the
first frame has been handed to the GPU instead of being the first thing the game does. A start that did not get as far as the end of
that (the player ended a window that did not answer, or the PC lost power) leaves the file `start-unfinished` in the user folder
(`%APPDATA%\Penumbra` on Windows). The file goes on the third frame, after the display switch has been applied. The next start sees
it, opens in a window this once (the fullscreen setting is not changed, and the log says so), and keeps the log of the start that
did not finish as `penumbra-unfinished.log` beside the new `penumbra.log`. A start with `--fullscreen`, `--windowed` or `--window`
keeps the mode it was given, and a capture or a test run (`--frames`, `--screenshot`, `--fixed-step`) keeps no such file, nor does a phone.

`penumbra.log` carries the seconds since its first line (`INFO +1.234s [Window] ...`) and names the stages that can stall ("Entering
fullscreen", "Switching ... to ... Hz", "Looking for gamepads", "First DrawFrame returned"), so a log that stops says where.

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
| `settings.json` | Language (`language`: `auto`, the default, or one of `en`, `de`, `es`, `fr`, `it`, `pt`, `ru`, `tr`, `uk`, `ja`, `ar`); the window (`window`: `width`/`height`, `0` for a window fitted to the monitor; `fullscreen`, true on a first launch; the fullscreen mode, `fullscreenWidth`/`fullscreenHeight`, `0` for the desktop's resolution; `fullscreenRefresh` in Hz, `0` for the highest the monitor offers at that resolution); widescreen, volumes, pixel shaders, `smoothMotion`, `pauseOnFocusLoss`, `difficulty` (E36: the campaign's difficulty, `"normal"`, the default, or `"hard"`, set on the options screen's Difficulty row; read in any case, and anything else is Normal with a warning), `touchControls` (`"auto"`, `"on"`, `"off"`); `zoom` (E25: `"auto"` or a percentage from 100 to 200, the campaign camera's zoom while the touch controls are on); `edgeMargin` (E26: `"auto"` or a percentage from 0 to 8, how far in from a curved screen's edges the HUD is drawn while the touch controls are on); `touchTuning` (E28: the touch controls' own layout, set on the options screen's Adjust controls: `size` 0.4 to 1.4, `opacity` 0.2 to 1.8 times the normal look, and `move`, a `[dx, dy]` for each moved control in the manifest's pixels from where `touch_controls.json` puts it, +x right and +y down, and `layout` (E34), the number of the default arrangement the moves were made against, 4 since E33: a file with another number or none keeps its `size` and `opacity` and loses its `move`, and the next save writes the current number); and the controls: `joystickLayout`, `keyboardPlayer2`, `firstPadIsPlayer1`, `rawJoysticks`, `stickDeadzone`, and the `player1`/`player2` key lists. A broken or missing field falls back to its default, field by field. |
| `hs.enml` | The best times, written after a new record. It holds both difficulties' lists (E36): the original's entity `hs` is Normal's five, and `hsHard` is Hard's five. Until the first record the original's `hs.enml` is read, and its `hs` is Normal's list. A key or an entity that is missing reads 59:59. See [Difficulty and best times](#difficulty-and-best-times). |
| `scenes\checkpoint.esc` | The level saved at the last checkpoint. |

The language is automatic until it is chosen: it follows the system when the game speaks it
(Windows' display language, the POSIX locale (`LC_ALL`, `LC_MESSAGES`, `LANG`), or the device's
language on Android, macOS and iOS) and is English otherwise, looked up again at every start, so a
device switched to another language is drawn in it the next time. The options screen's Language row
names each language in its own script, with Automatic first; picking a language fixes it (`language`
holds its id), and picking Automatic goes back to following the system (`language` is `auto`). On a
phone's options screen the Language chooser, with its globe, is in the top-right corner. A
`settings.json` written by an earlier version names a language and keeps it. On Linux the
files go to `$XDG_DATA_HOME/Penumbra`, else `~/.local/share/Penumbra`. On macOS they go to
`~/Library/Application Support/Penumbra`, and the graphics cache to
`~/Library/Caches/com.ivancvetanovic.penumbra`. On an iPhone or iPad they are inside the app's own
container: deleting the app deletes them.

## Difficulty and best times

The campaign has two difficulties (E36), chosen on the options screen's Difficulty row, "Difficulty:
Normal" or "Difficulty: Hard" (on a desktop beside the language chooser, on a phone its own cell). **Normal**
is the original's game. **Hard** is the same game with every enemy's hp doubled when it is spawned:

| Enemy | Normal | Hard |
|---|---|---|
| Warrior (also the ones the king summons) | 75 | 150 |
| Minion | 45 | 90 |
| Knight | 150 | 300 |
| Impy | 75 | 150 |
| Paladin | 400 | 800 |
| Master knight | 1700 | 3400 |
| King | 3500 | 7000 |

- A kill gives the same experience in both (the enemy's base hp), so in Hard each kill takes twice the
  damage for the same experience, and the wizard levels up more slowly. The players, the potions'
  healing and the co-op princess are the same in both. Versus never plays Hard.
- A New Game reads the choice when its fade ends. The run keeps it through deaths, checkpoints and the
  next-level doors, and a change made during a run applies from the next New Game. Hovering New Game
  shows the difficulty the run will be played at and where it is changed. `--difficulty normal|hard`
  ([below](#command-line)) sets it for one run and saves nothing.
- The five best campaign times are kept for each difficulty. The Best Times panel lists Normal's five and
  then Hard's five, each under its name. The campaign's end screen adds the time to the list of the
  difficulty that was played and shows only that list, headed "Best times (Normal):" or "Best times
  (Hard):".
- In `hs.enml` the original's entity `hs` (keys `hs0` to `hs4`, in milliseconds, best first) is Normal's
  list, unchanged, so records made before this version stay valid and count as Normal. Hard's list is a
  second entity, `hsHard`, in the same file with the same keys. A key or an entity that is missing reads
  59:59 (3,599,000 ms, what the shipped file holds). The first record written creates both entities, and
  a time never changes the other difficulty's list.
- A locked arena (Templo Sagrado at 12:00, Neblina at 15:00) opens when the better of the two lists' best
  times is below its score, so a finish in either difficulty opens it. With no record yet, both stay locked.

## Command line

The game's own options:

| Option | What it does |
|---|---|
| `--start <scene>` | Skip the menu and start `scenes/<scene>.esc`: `level1`–`level3` or `pvp_lv1`–`pvp_lv6`; `arena_select`, `gameover` and `videoModes` start as the scripts start them |
| `--tour <a,b,...>@<N>` | After the menu, start each scene in turn for *N* ticks: many screens in one launch |
| `--lang <id>` | This run's language: `en`, `de`, `es`, `fr`, `it`, `pt`, `ru`, `tr`, `uk`, `ja` or `ar`. It is not saved. |
| `--difficulty normal\|hard` | This run's campaign difficulty (E36), over the setting. It is not saved, and a pick on the options screen's row replaces it. A campaign level started with `--start` is played at it. Any other value is an error. Like every option a player does not use, it removes the intro. |
| `--widescreen on\|off` | This run's view. It is not saved. |
| `--smooth on\|off` | This run's motion between ticks (E8). It is not saved. Off under `--fixed-step` unless given as `on`, so fixed-step captures show the ticks themselves. |
| `--splash on\|off` | The Supersonic Engine's intro (E35). `on` plays it with any other option (a capture of the intro: `--splash on --fixed-step --frames 30 --screenshot <path>`, where `--frames 30` is the 30th tick of the intro and the menu's first frame is `--frames 121`); `off` removes it. Without it, it plays on a normal start only. |
| `--original <dir>`, `--data <dir>` | Where the original's files and the enhanced edition's data are ([above](#where-the-files-are-found)) |
| `--hold <KEY>@<a>-<b>` | Hold an Ethanon key from tick *a* to tick *b*, for scripted captures. KEY is one of `UP DOWN LEFT RIGHT CTRL ALT SHIFT SPACE ENTER ESC BACKSPACE PAGEUP PAGEDOWN J S D 1 2 3 LMOUSE RMOUSE`. |
| `--cursor <x>,<y>` | Pin the scripts' cursor at a point of the 1024x768 menu screen, for menu captures |
| `--touch [on\|off]` | This run's touch controls (E16); on by itself. On a desktop the held left mouse button is the finger. It is not saved. |
| `--zoom auto\|<percent>` | This run's campaign zoom while the touch controls are on (E25), 100 to 200. It is not saved. |
| `--mobile-layout on\|off` | This run's phone layout of the options screen (E20, with E25's Zoom row), for captures on a desktop. With the data folder's options art it is E31's large two-column screen, and without it E20's. It is not saved. |
| `--edge-margin auto\|<percent>` | This run's HUD edge margin while the touch controls are on (E26), 0 to 8. It is not saved. |
| `--safe-area <l>,<t>,<r>,<b>` | The display's safe-area insets, in window pixels, in place of what the platform reports (none on a desktop), for captures of a notched phone. |
| `--touch-tuning <list>` | This run's touch controls' size, opacity and places (E28), in place of the saved ones: comma-separated `key=value` items, `size` (0.4 to 1.4) and `opacity` (0.2 to 1.8) as numbers, and any of `dpad`, `jump`, `sword`, `fire`, `light`, `swordCombo`, `spellCombo`, `exitDown` and `pause` as `dx:dy` in the manifest's pixels (+x right, +y down), for example `size=1.2,opacity=0.6,light=-60:-20,pause=-30:20,dpad=12:0`. A run with this flag, `--touch-editor` or `--finger` saves nothing. |
| `--touch-editor [locked\|unlocked]` | Opens E28's editor of the touch controls on the first tick the options screen is up (with `--start videoModes`), locked unless told otherwise; it turns the touch controls on unless `--touch` says otherwise. For captures. |
| `--finger <id>:<x>,<y>@<from>-<to>[/<x2>,<y2>]` | A synthetic finger at a point of the screen's logical pixels (the menus' 1024x768), down from tick *from* to tick *to* (counted from the first tick, as `--hold`'s are) and moving in a straight line to (*x2*, *y2*) over that span; `@<tick>` alone is a one-tick tap. It can be given more than once. For captures. |
| `--princess` | Puts player 2's princess beside the wizard in a campaign level, as a pad's Start would, for captures of co-op. |
| `--hp <n>` | Sets the wizard's hp once he appears, for captures. |
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
