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
(see [building.md](building.md#macos-and-ios)).

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

| Suite | What it checks |
|---|---|
| `test_pn_smoke` | The build finds the original |
| `test_pn_paths` | Where the original and the data are found: flag, beside the executable, the build's path |
| `test_pn_formats` | The original's files: every scene and entity parses, the 0.7.12 writer reproduces the shipped scenes byte for byte, ENML, image headers, cp1252 text |
| `test_pn_runtime` | The emulated Ethanon runtime: buckets, custom data, callbacks, frame order, input, samples |
| `test_pn_particles` | The Ethanon particle manager |
| `test_pn_boot` | The real game boots headless: the menu, level 1, the wizard walks, jumps and swings |
| `test_pn_scenarios` | The real game played headless: combat, spells and both combos, potions, hazards, checkpoints, game over, level changes, the king, Versus, the co-op creature; the options screen, its resolution list and refresh-rate row (E23) |
| `test_pn_audio` | Every sound the original ships decodes (19 Ogg Vorbis, 16 MP3) |
| `test_pn_render_textures` | Colour keys, DDS, normal maps, blend variants |
| `test_pn_render_lights` | Lights, halos and projected shadows |
| `test_pn_render_particles` | Particles drawn as pooled quads |
| `test_pn_render_hud` | `strings.json` against every string the scripts draw, the control hints' touch wording (every hint has it, it fits, touch off changes nothing), the font layout, the Credits panel with the enhanced edition's credit and its c with acute in every face (E21), the HUD's quads |
| `test_pn_render_english` | The English images: each variant exists at the original's size, and the renderers swap them with the language |
| `test_pn_render_input` | Keys, pads and the keyboard second player mapped onto the original's input; A and B in the menus (E14); the pads' order with the touch controls on (E22); the settings file; the automatic display mode and the refresh rates offered (E23) |
| `test_pn_render_interp` | Smooth motion (E8): the blend between two ticks, whole pixels kept whole, never across a scene load, a frame gap or a jump |
| `test_pn_render_pause` | The pause (E13): when it opens, the frozen ticks, the menu, the one-tick cancel to the main menu, focus loss, the overlay, the input held back after it |
| `test_pn_render_touch` | The touch controls (E16): the key each control presses, the direction disc, several fingers at once, the pause opened and tapped, a tap in a menu as a click, the knob only while a direction is held, the corner button screen by screen, the layout on 4:3 and widescreen with a safe area, the manifest and its art, the setting; the combo buttons' key timelines, and the combos firing through the ported combo buffer and in level 1, where the first help sign is drawn in touch wording; a phone's Versus with one gamepad through the real game (the touchscreen moves the wizard only, the pad the princess only) and the co-op princess summoned with its Start (E22) |
