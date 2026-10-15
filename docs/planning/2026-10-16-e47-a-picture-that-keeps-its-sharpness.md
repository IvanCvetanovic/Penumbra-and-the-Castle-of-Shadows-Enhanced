# E47: a picture that keeps its sharpness on a weak phone, and a row to choose it

Built 2026-10-16, in the engine (`engine/`, additive and opt-in: `DynamicResolution.{hpp,cpp}`, the new `RenderScaleControl.hpp`, `SupersonicApp.{hpp,cpp}`) and in the game (`game/main.cpp`, `game/render/Settings.*`,
`game/script/optionsPhone.cpp`, `Script.hpp`, `videoModes.cpp`, `game/PenumbraLayer.*`, `game/data/strings*.json`), after [E41](2026-10-08-e41-a-scene-the-gpu-can-keep-up-with.md) left "a setting that pins the scale" and the rise rule open.

## What was wrong

A tester's report (a Samsung Galaxy A05, Android 15: "it ran normally, but the more I played the more pixelated it got, so that I could not see the enemies, the damage or what was written") came with two screenshots, one of the options screen
and one of a level, 22 minutes apart. Both were measured (not judged by eye): each was drawn from a **480x216** scene target stretched to the phone's 1600x720, which is **0.30, the floor of E41's controller**. Four independent estimators
(the comb a bilinear stretch leaves in the second difference, a template match of the options screen against captures at known scales, the shape of a down-then-up residual curve, the gradient autocorrelation), each calibrated on captures of
this build at known scales through the tester's own JPEG quality (libjpeg 70, 4:2:0), gave 0.30 for both pictures, and nothing near it fitted (0.31: 8.4 against 6.6 in the template match; 0.7 and 0.35 are ruled out by the autocorrelation). The HUD, the
text and the touch buttons are drawn into that target (the game draws no ImGui), so they blur with it: nothing on the screen stays sharp.

Why it stayed there is the controller's rule, read from the code and simulated, **not measured on the phone**: the scale rose only after eight seconds on end at **110 frames a second**, and any window between the shortfall line (54.5) and 110 reset that count. A
frame with a fixed share of 9 ms or more (a slow CPU, a driver) can never show 110, so a size once lost to a heavy scene, a load hitch or heat was lost for the run; the menu is the heaviest scene and the levels inherit what it settled at; nothing persists
the scale, so a relaunch started sharp and fell again. The floor, 30% of the window, was chosen on a 2560x1600 tablet (where it is 768x480); on a 1600x720 phone it is 480x216, under what text needs. What drove the first fall (a slow GPU, heat, a hitch) is not
known: both screenshots are already at the floor. The A05 is a Helio G85 with a Mali-G52 MC2, about 4.8 times slower than the Pad 5's Adreno 640 by the median of six benchmarks (and 2.3 times faster than the A54's PowerVR GE8320), a 60 Hz panel by every secondary
source; the Android loader lists MAILBOX for it (two gpuinfo reports), so its loop is unpaced.

## What changed

Engine, all opt-in, off by default, never in `game.manifest`'s text (a game that sets none of them has the controller exactly as E41 made it: a differential run of the old and the new controller on identical traces gave identical decisions, below):

- **A rise on room** (`relaxedRise`, `riseMargin` 1.25): the way up opens at 1.25 x the target (72.5 fps) instead of 110, and the step is still the one `ScaleAfterHeadroom` predicts to keep 1.2 x the target (the prediction follows the pixel count
  entirely, so a frame with a fixed share lands above it). A rise is **on trial for 20 s**: a shortfall inside it means the rise did not hold, and the next one waits 60 s, doubling to 480, so a rise and a fall cannot take turns every few seconds (each
  costs a resize). **Never on a loop the display paces**: a window cannot read above the refresh rate there, so room is invisible and the rule would only guess; `SupersonicApp` tells the controller (`SetLoopPaced`) from the swapchain's present mode
  ("Fifo", "FifoRelaxed": MoltenVK, a device with no MAILBOX), and there E41's rule stands.
- **A floor in pixels** (`minTargetHeight`, `DynamicResolution::ForWindow`, `FloorForTargetHeight`): the scene target is never fewer lines high than that (rounded up to the scale's two decimals), 360 for Penumbra: 0.5 on 720 lines, and still 0.3 where
  that is already more (1600 lines). Worked out where the controller is built, like E41's start budget, and again when the window's height changes; the controller is reconfigured only if the floor is not what it was (a `Reconfigure` forgets the
  holds and the judgement).
- **An emergency floor** (`emergencyBelowFps` 30, `emergencyScale`): the pixel floor gives way to the old floor only on a window that ran under 30 fps AT the comfortable floor (a fall is measured there first, and a floor raised under a picture
  that had fallen lifts it), so the phones that were at 0.3 for speed (a PowerVR GE8320) stay there. Without `emergencyBelowFps` the raised floor is a hard one.
- **`RenderScaleControl`**, published in the registry context like `WindowControl`: a game asks for Automatic (the controller restarts at the launch size) or Fixed (a share of the size the controller started at, held by `DynamicResolution::Pinned`: min =
  max = start, no rise, no emergency floor, no pixel floor); latched, applied at the top of the next frame, available only where there is a controller (not for a capture or a fixed-step run, except a fixed `--render-scale`); a request for what the picture
  already is changes nothing.

Game:

- `main.cpp`: all three on for every phone build (`kMobileBuild`, where the controller is on).
- **The picture's row** on the phone's options screen (`g_pictureSize`, label "Picture sharpness", values **Automatic** (the controller), **Sharp**, **Balanced**, **Fast**, held at 100%, 70% and 50% of the largest size the controller would
  start at, never under the floor): saved as `pictureSize` in `settings.json` (absent or unknown: auto, with a warning for the latter), asked for once at the first tick the controller is there and at once on a pick. The row exists only where a controller can be
  asked: a computer's picture is the window's. With the refresh rate's row (Android) it is a chooser of its own under Refresh | Zoom, which takes the rows from six to seven (88 px to about 70 on a 1600x720 phone, 60 at the least: a 3120x1440 screen with an
  8% frame needs it); without it (iOS) it sits beside Zoom and the six rows are as they were. Eleven languages (the four texts; the label and value rooms are in `tests/data/l10n_rooms.json`); the Japanese uses only characters the bundled face has, so the
  face is as it was.

## Measured and tested

- **The engine's own suite** (`test_gameruntime`, 3,283 checks, 0 failures): E41's controller tests pass unmodified; new checks state each rule on stand-in GPUs (a frame of 9.5 ms + 20 ms x scale squared that E41 holds for ever and the new rule lifts from 0.3 to about
  0.5 in a few steps, then holds; a paced loop that never rises; a rise that does not hold waits 60, 120, 240, 480 and 480 s; a trusted rise is not held against the next failure; the margin; the pixel floor, with the target's height checked line by line for
  every window height from 100 to 3000 and five floors; the emergency floor reached from one hundredth over the floor; a pinned size that no GPU moves; a floor that rose lifts a picture that had fallen under it; the request channel). The manifest's text carries
  none of the new fields.
- **The old controller against the new, option by option** (a differential harness over the controller alone, g++ 13 on x86-64 with `-ffp-contract=off`, on the tree as it was before the last small change to `FloorForTargetHeight`, which the runs with a pixel floor
  of 0 never reach): with every new option off, or `relaxedRise` on a paced loop, or an emergency scale that mends to nothing, the return value, the six public getters and all 16 private members were **bit-identical after every `Observe`** over 100,000 random traces
  (3.6e9 frames: steady, stand-in GPUs, hitches, bursts, thermal drift, pauses, zero, negative and non-finite frame times, random and contradictory configurations, random `Reconfigure` calls), 10.9e9 comparisons; `Pinned` with every option declared equals the old
  min = max = start, and `ForWindow` with only a pixel floor equals the old controller with the raised floor. With the options on: no rise above x1.25 a step, none outside [min, max], none inside a hold after a failed rise (10,646 failed rises, each followed by at
  least 60 s with none), the emergency floor never below 0.3 and `AtTheFloor()` only there; 17 deliberately broken copies of the new controller were all caught. Known and left: the steady 30-minute worst case is 27 resizes (old 26), and under a load that
  alternates 60 s light with 5 s heavy the new controller makes up to 166 resizes in 30 minutes where the old one made 58 (one in 11 s: it follows the load, which the old one could not; with a light phase of 29 s or more the rise survives its trial and the hold never
  engages). The re-run on the final tree was stopped by the machine running short of memory and was not repeated.
- **The real controller on simulated phones** (the simulation of the A05 report's investigation, rebuilt against `engine/src/core/DynamicResolution.cpp` as it was in the tree; 216 + 272 + 120 paired scenarios, each run old against new; the frame costs are
  assumptions, nothing here was measured on a Mali phone): on an unpaced loop at costs like the A05's the picture ends at **0.85 where E41 left it at 0.52** (a menu of 60 ms at full size, a CPU part of 12 ms: 69 fps against 82) and at **0.60 where E41 left 0.37** (120 ms,
  12 ms: 72 fps against 82); with load hitches at each scene change the mean scale of the last five minutes is **0.48 to 0.66** and none of 16 ends at the floor (5 of 16 before); the weakest GPUs (a menu of 480 ms) are as E41 left them, at 0.3, and at 240 ms they are
  0.41 to 0.43 at 67 to 72 fps instead of 0.30 to 0.38; **no scenario ends under 30 fps**; on a loop the display paces the result is E41's (the same scale and fps, as designed); the tablet stand-in calibrated on E41's numbers (4.1 MP, start 0.73) ends 0.75 to 0.76 at 69
  to 71 fps where it ended 0.73 at 71 to 75, and is identical on a paced loop. The cost is resizes: 6.7 against 5.4 in 30 minutes on the main set, 20 against 11 with heavy episodes, at most 63 in one run (one in 29 s); a quick reversal (a direction change inside 10 s)
  0.57 a run with the episodes against 0.07.
- **Review**: a hostile reader of the engine diff found four real defects, all fixed with a test for each: a pinned size was overridden by the emergency floor (the row's "Sharp" would not have held), every window-height change reset what the controller had learned
  (a rule 2 breach for a game that asked for nothing), the emergency floor was unreachable from one hundredth over the floor (a step the controller's own 0.01 test swallows), and a raised floor left a picture below it. The differential run above found a fifth, in the
  float arithmetic of the pixel floor (a 150-line window at 0.84 is 125.99999 lines): fixed and tested for every height.
- **Linux**: 17 of 17 suites (`test_pn_all`) on the tree before the last three small changes (the pixel floor's rounding, the seven rows' least height of 60 and the restart of Automatic at the launch size); after them `test_gameruntime` (3,283 checks) and
  `test_pn_render_input` (19,756) pass, and the full run was not repeated (the machine was short of memory). `test_pn_render_hud` lays every language's four texts out in their rooms with the game's own font atlas (and checks every glyph is in the bundled faces),
  `test_pn_render_input` pins the phone geometry (the six rows exactly as before, the seven rows' structure, every shape, every cell inside the panel and no two hit boxes overlapping), `test_pn_render_touch` the setting's round trip and its defaults. MSVC `/W4`
  (`tools/check.bat`) is clean for the ten changed files on the final tree (read for "warning C": none); the restart of Automatic in `SupersonicApp.cpp` was not compiled by GCC.
- **Looked at and run** (Linux, software Vulkan): the options screen with the row in all eleven languages and in all four values, at 1600x720 with the A05's inset, at 2400x1080, 1920x1200, 1024x768, with the touch controls off, and at the extreme frame (a 3120x1440 screen with an 8% edge margin):
  every text inside its cell, nothing under the Back arrow or the language chooser. A live run (`--dynamic-res on`, a synthetic finger) walked the row Automatic, Sharp, Balanced, Fast and back: each pick logs "held at 1 / 0.7 / 0.5 (asked by the game)", writes
  `pictureSize` to `settings.json`, a relaunch applies the saved size at the first tick without a tap, and Automatic hands the picture back to the controller.

## Not verified, and open

- **Nothing was run on an A05 or any Mali phone, nor on an Android build of this change** (no device was attached; the Android package was not built). Every figure about the A05's speed is an estimate; whether the relaxed rise ever opens on it depends on its frames being able to
  show 72.5 fps, and the first fall's cause is still not known. The tester's version is unknown too (E41 is in 1.0.7 and later). A `PENUMBRA_PERF_DIAG` build on the phone (E42) would say.
- The seventh row makes every cell about 20% shorter on a 1600x720 phone (a finger-sized 86 px button box stays 86 wide; the row is 70 tall, 60 at the least). On iOS the six rows are kept, and that layout was tested by geometry only (`kMobileBuild` is false on Linux).
- "Fast" is not the smallest picture: it is half the start size and never under the floor (0.5 on 720 lines); only the controller, in an emergency, goes lower. "Sharp" is the start size, not the window: at most about 2.2 megapixels (about 0.92 on 2400x1080, 0.74 on 2992x1344).
- A load hitch is not ignored (the controller counts any frame under 1.5 s as the GPU's speed): the rise on room is what heals it, in about 30 s on a GPU with room, as simulated; no hitch rule was built, for a rule not relative to the window's own baseline would blind the
  controller on the slowest phones, and nothing here shows the engine sees the game's scene loads.
- The menu is still the heaviest scene and sets the size the first level meets; a hint that a scene is a menu (a lower speed target) would let it keep a sharper picture. Not built.
- The text and touch buttons are still drawn inside the scaled target (a native-resolution overlay, the real cure for legibility at a small size, is engine work not attempted). The render-path review of this investigation found no single large saving in the render path;
  the first thing it asks for is per-pass GPU timestamps in the performance log, so that the next report says where a phone's frame goes.
- The translations (ten languages) were not read by a native speaker; the Japanese "Fast" is スムーズ ("smooth") because the characters of a more natural word are not in the bundled face and the font tool cannot run offline here.
