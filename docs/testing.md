# Testing

The suites are `tests/test_pn_*.cpp`, one executable each, built by the engine's
`supersonic_add_test`. Five of them boot the real game headless, from the original's own files,
and play it through its ported scripts. A suite that needs the original's files exits 77 (reported
as skipped) when they are missing.

```bash
ctest --test-dir build --output-on-failure    # all of them, each through its own executable
build/tests/test_pn_all.exe                   # all of them, through one executable
build/tests/test_pn_all.exe --suite boot      # one, as build/tests/test_pn_boot.exe runs it
build/tests/test_pn_all.exe --list            # the suites test_pn_all holds
build/tests/test_pn_scenarios.exe             # one
```

On Linux, `bash tools/build_linux.sh --test` builds everything and runs `test_pn_all` once. On
GitHub, `.github/workflows/ci.yml` builds and runs it on Linux and Windows, and `apple.yml` on macOS
(see [building.md](building.md#macos-and-ios)). `release.yml` runs it once more, in the Ubuntu
22.04 container the Linux download is built in, and runs the Linux and Mac downloads
([The release workflow](#the-release-workflow)).

## test_pn_all

`test_pn_all` exists for Smart App Control, which on the development laptop judges every freshly
linked executable on its first launch and refuses some. A build relinks every suite and so gives
it one chance to refuse per suite; `test_pn_all` is one executable, so one judgement per build.

It holds every `tests/test_pn_*.cpp`, found by the same glob as the suites' own executables, which
stay as they are. Each suite is compiled from a generated wrapper (`tests/all/WrapSuite.cmake`,
rewritten whenever the suite changes): the suite's own `#include`s, then the suite inside a
namespace of its own, so that suites keep their `main()` and their same-named helpers apart. A
suite must include its headers at the top of the file, outside any `#if`; the wrapper refuses
otherwise.

Every suite still runs in a process of its own, a child of `test_pn_all` itself (the same file, so
the same verdict). The ported script's globals live for the whole program, as they did in the
original, so the suites that boot the real game cannot share one. Each suite prints what its own
executable prints, its summary line included; `test_pn_all` adds a line per suite and a total. It
exits 0 when nothing failed, 1 when a suite failed, crashed, timed out (`--timeout <seconds>`,
default 1500) or could not be started (it then starts no further suite), and 77 when every suite
skipped. It is not registered with ctest: ctest already runs every suite through its own
executable, and a second registration would run each twice.

## The suites

A full run of `test_pn_all` on Linux is 17 suites and 39,542 checks at E34 (39,290 at E32, 39,140 at E31, 28,556 at E28, 22,109 before it); every suite prints its own count.

| Suite | What it checks |
|---|---|
| `test_pn_smoke` | The build finds the original |
| `test_pn_paths` | Where the original and the data are found: flag, beside the executable, the build's path |
| `test_pn_formats` | The original's files: every scene and entity parses, the 0.7.12 writer reproduces the shipped scenes byte for byte, ENML, image headers, cp1252 text |
| `test_pn_runtime` | The emulated Ethanon runtime: buckets, custom data, callbacks, frame order, input, samples |
| `test_pn_particles` | The Ethanon particle manager |
| `test_pn_boot` | The real game boots headless: the menu, level 1, the wizard walks, jumps and swings |
| `test_pn_scenarios` | The real game played headless: combat, spells and both combos, potions, hazards, checkpoints, game over, level changes, the king, Versus, the co-op creature; the options screen, its resolution list and refresh-rate row (E23); the "Adjust controls" button of E28 in E20's art-less phone layout, drawn under the touch controls' switch and raised by a click on it and by none of its neighbours; the menu's song kept through the settings and the arena select, and started again after a level (E30); E31's large options screen on a phone, with the art on: the exact texts of a frame, a tap on every cell, button, gap and corner in four window shapes (a 4:3 window, a 20:9 phone, a notched phone, a 4:3 window with 88 px cut-outs), steppers and choosers stopping at their ends, a card lit only while it is pressed, the Adjust cell gone and dead with the touch controls off, the layout without the refresh row (iOS), a narrow body's text fitted into its rooms, no rectangle meeting the scene's edge, and the menu's song going on through Back, the screen's corner and Esc |
| `test_pn_audio` | Every sound the original ships decodes (19 Ogg Vorbis, 16 MP3); the sample bank's request to keep one sample through the next scene load (E30), against a device whose unload stops every voice: carried through one load only, its volume and loop kept, the music volume reaching it, not carried when it has ended or was never played |
| `test_pn_render_textures` | Colour keys, DDS, normal maps, blend variants |
| `test_pn_render_lights` | Lights, halos and projected shadows |
| `test_pn_render_particles` | Particles drawn as pooled quads |
| `test_pn_render_hud` | `strings.json` against every string the scripts draw, the control hints' touch wording (every hint has it, it fits, touch off changes nothing), the font layout, the Credits panel with the enhanced edition's credit and its c with acute in every face (E21), the HUD's quads; E25's menu panel on a phone, every panel in every language scaled to fit what the window shows of it, the long ones in two columns (at least 1.2 times E1's size on a 20:9 phone, 1.24 to 1.60 measured) and clear of a gamepad's icon; the two columns' break, order (the first at the right in Arabic) and avoid box on a text of the suite's own; E26's `DrawSpritePart` through the runtime and the renderer (a part keeps its rectangle, clamped to the image, and its texture coordinates), and the HUD's stone plaque through the real game (the six pieces of each player's missing top and left stone cut from `frame.png`, none overlapping another or the frame's own stone, the bars 16 px inside the plaque's corner, the message lines clear of it; without the plaque the panel is the original's), E27's `DrawShapedSpritePart` (a part stretched to its size, the destination cut in step with a rectangle cut to the image, an empty part drawn as nothing) and the helpers for the options art (every file of `game/data/images/options/` loaded at its PNG's size and none named as a file of the original is, a panel's nine slices filling its rectangle exactly in the art and on the screen, also a panel smaller than two slices, icons at their proportions, nothing drawn without a data folder); E28's two strings (the "Adjust controls" button and the editor's title) in every language and each in its room, and the real `PenumbraLayer` driven on a bare registry through its own frame loop (the entry opens the touch controls' editor, the Machine stands still under it, a synthetic finger's drag of a button is kept in the settings, Esc closes it without the options screen reading the key, and a finger that was down at that moment clicks nothing); E31's phone options screen: every text the layout draws has a `phone` room in `tests/data/l10n_rooms.json` and fits it in all eleven languages (an options text without one fails the suite, a check proves the measurement fails on a room one pixel too narrow, and the rooms agree with the layout's own numbers), and the editor of E28 shares the top-left corner with the options Back arrow without leaking into it (a tap on the editor's, of one tick or three, and a finger held across the Esc that closes it, leave the options screen where it is, and a fresh tap there leaves) |
| `test_pn_render_english` | The English images: each variant exists at the original's size, and the renderers swap them with the language |
| `test_pn_render_input` | Keys, pads and the keyboard second player mapped onto the original's input; A and B in the menus (E14); the pads' order with the touch controls on (E22); the settings file; the automatic display mode and the refresh rates offered (E23); E25's zoom (its setting, its rules, the campaign's screen at each) and the larger menu on a phone (where each window puts it, inside a notched phone's safe area, and a click or a tap landing on every button); E26's HUD frame (its setting, the margin asked, the frame from the margin and the safe area) and the message rule on ten phone and tablet shapes at every zoom they offer; E28's touch tuning in the settings (the round trip, the one-line JSON, a tolerance for every wrong field, the limits and steps of size and opacity, `--touch-tuning`'s grammar); E34's layout number (a block with the current number keeps everything; one with none, as before E34, or another loses its moves and keeps its size and opacity; a number that is not one warns, and the number 3 of an intermediate build loses its moves like the others; the migration through a whole settings text and a real file, and the next save writing the current number); E31's `ComputeFixedLayoutArea` (what a window shows of a 1024x768 scene and the frame inside it: five shapes pinned, and agreement with the camera's own view in 20 window and side combinations) and the phone options layout rect by rect in eight shapes (a phone, a tablet, a notch, a bottom bar, a 4:3 window with cut-outs, no published area), with its invariants (every hit box inside the area, no two overlapping, every cell inside the panel, the panel inside 12 to 1012) |
| `test_pn_render_interp` | Smooth motion (E8): the blend between two ticks, whole pixels kept whole, never across a scene load, a frame gap or a jump |
| `test_pn_render_pause` | The pause (E13): when it opens, the frozen ticks, the menu, the one-tick cancel to the main menu, focus loss, the overlay, the input held back after it; E28's editor of the touch controls: its layout on 4:3, a wide area and a notch, the fingers (taps on release, the finger of the entry dead, grabs, locked drags, the size step's re-lock, restore, close, a dropped control kept out from under a tile), the overlay's order and counts, the number pulse and the outlines' breathing, the tiles' art, the title and numbers through `HudRenderer` in every language, and `HoldPressed` |
| `test_pn_render_touch` | The touch controls (E16): the key each control presses, the direction disc, several fingers at once, the pause opened and tapped, a tap in a menu as a click, the knob only while a direction is held, the corner button screen by screen, the layout on 4:3 and widescreen with a safe area, the manifest and its art, the setting; the combo buttons' key timelines, and the combos firing through the ported combo buffer and in level 1, where the first help sign is drawn in touch wording; a phone's Versus with one gamepad through the real game (the touchscreen moves the wizard only, the pad the princess only) and the co-op princess summoned with its Start (E22); E25's down button (shown only while the exit door offers the way on, a tap is down alone), the direction control's box hanging below the screen's edge (the arrows 25 units under the middle of the action buttons' bottom row on three screens and several insets, the overhang's clamp rules and parsing, a finger on a lowered arrow, the exit button taking priority over the disc) and the layout at a zoomed screen's scale, and in level 1 at 175% the camera, the door's offer and the button taking the wizard to level 2; the princess taking a zoomed level back to E1's screen, where she survives 540 px below the wizard (and dies without it); E26's pause button in step with the HUD frame, and the HUD's commands in level 1 moved by the frame and restyled on touch only, the original's otherwise; E28's tuning laid over the manifest (the default is the manifest bit for bit, the boxes at each size, opacity, moves by anchor, the Pause held below the timer, the exit button following the direction control), `MoveFor` as the layout run backwards (over 11,000 round trips on every control, screen shape, inset, scale and size), `GrabBox` against the art's opaque pixels, the editor's scene (nothing a finger does presses a key or clicks, now or in the scene after it), and the largest size at which the shipped layout draws no control over another in play (1.1 on every case since E33; E32's level grid held to 1.0, E29's columns to 1.2, E16's arrangement to 1.1, 1.0 with a 100 px bottom bar, and 1.2 unzoomed; the pairs that meet at each size from 1.2 are pinned, and the pause button's clearance at each size); E29's, E32's and E33's two columns of action buttons (every box on 4:3, 16:9, 20:9 and a notched 16:9, the sizes and paddings, a finger on each button's centre, the gaps between neighbours and the strip right of the cluster fingerless, the relations the columns obey (equal columns, the right one exactly 30 higher at every row, exactly 16 between neighbours, the right column 24 to 60 from the edge, evenly spaced rows, each combo halfway between its buttons, the top clear of the pause button) and the rules every arrangement obeys apart from the ones the columns add, the direction control's two drawn buttons held clear of every control a finger could reach), and `TouchTuning::kLayoutVersion` (E34) pinned beside the table of the default's places: whoever changes a default offset bumps it |

## The release workflow

`.github/workflows/release.yml` ([building.md](building.md#release-downloads)) checks each download
before it can be added to a release. A check that fails stops its job, and the files are published
only when all three jobs passed.

- **Linux**, in the Ubuntu 22.04 container the download is built in:
  - `test_pn_all` once, whose summary must say "0 failed, 0 skipped, 0 not run".
  - The program's libraries and its newest glibc symbol
    ([building.md](building.md#the-linux-release-build)).
  - The package as a player gets it: `Penumbra-Linux.tar.gz` unpacked into a folder that is then
    made read-only, and run as an ordinary user (not root) on Mesa's lavapipe under Xvfb, for 300
    ticks of level 1 with a capture. It must exit 0 and write the capture.
  - The same package with no display at all: it must end at once with exit code 1 (the 30-second
    `timeout` around it would give 124). A start that fails this way used to print its error and
    never end, until `game/main.cpp` shut the job pool down in its `catch`.
- **Mac**:
  - `tools/apple/make_release.sh` unpacks the zip it wrote and verifies the app with
    `codesign --verify --deep --strict`.
  - The workflow unpacks the zip again as Finder would (`ditto -x -k`) and runs the app from there,
    started from another folder, for 300 ticks of level 1 with a capture: on arm64, then its
    x86_64 half under Rosetta (`arch -x86_64`). Both runs use the runner's Apple GPU, so they show
    that the Intel half runs, not that an Intel Mac's graphics draw it.
  - Only installing Rosetta may fail, because not every runner image can. Once it is installed,
    the x86_64 run gates the job like the arm64 one.
  - The suites are not run here: `apple.yml` runs them on macOS for every push.
- **iPhone/iPad**: `make_release.sh` checks that the program links only the system's libraries and
  is not encrypted, verifies its signature (`codesign --verify --strict`), and checks that the
  `.ipa` starts with `Payload/` and holds no AppleDouble files. Nothing runs it: the simulator
  cannot draw the game.
- The Linux, Mac and iPhone/iPad jobs each check that `extracted/` and `engine/` are as the
  checkout left them.
- **Publishing** checks the release's existing files against its published `SHA256SUMS.txt`, and
  the rewritten file's first two lines (Windows, Android) against the published ones.
