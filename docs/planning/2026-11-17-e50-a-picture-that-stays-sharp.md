# E50: a picture that stays sharp on a slow GPU: the HUD at the window's resolution, a 30 fps tier and a size per scene

Built 2026-11-17, in the engine (`engine/`, additive and opt-in: `renderer/MeshRegistry.{hpp,cpp}`; `core/ScreenOverlay.{hpp,cpp}`, the
new `renderer/WindowOverlay.hpp`, `renderer/VulkanRenderer.{hpp,cpp}`, `renderer/VulkanPipeline.{hpp,cpp}`, `editor/EditorLayer.{hpp,cpp}`,
`core/GameRuntime.hpp`; `core/DynamicResolution.{hpp,cpp}`, `core/RenderScaleControl.hpp`, `core/SupersonicApp.{hpp,cpp}`,
`core/PerfOverlay.{hpp,cpp}`) and in the game (`game/main.cpp`, `game/render/ShadowRenderer.*`, the new `game/render/PictureSize.{hpp,cpp}`,
`game/PenumbraLayer.*`, `game/render/Settings.hpp`, `game/script/Script.hpp`, `tests/test_pn_render_{touch,pause}.cpp`,
`docs/playing.md`, `docs/building.md`), after [E47](2026-10-16-e47-a-picture-that-keeps-its-sharpness.md) left two things open that this
builds: the text and the touch buttons drawn at the window's resolution ("the real cure for legibility at a small size"), and a menu that
no longer sets the size the first level meets. Before the four changes, the engine pin moved on its own, and was checked on its own, to the
engine's main with the MIT licence text and 34 commits of the engine's own hardening (under "Measured and tested").

## The report

A tester's Samsung Galaxy Tab A7 Lite (the model of [E49](2026-11-10-e49-a-first-start-that-goes-on.md)'s report) found the picture blurry
and the game laggy. The app version was not confirmed. What the tablet is, from published sources (not measured here):

- a MediaTek Helio P22T with a PowerVR GE8320 GPU, a 1340x800 panel, Android 14;
- a Vulkan 1.1 driver with no GPU timestamps, whose swapchain formats include R8G8B8A8_UNORM (the public hardware database, gpuinfo report
  31369);
- a GPU 9 to 16 times slower than the Xiaomi Pad 5's Adreno 640, on which E41 was measured, by published GFXBench and 3DMark figures.

## What was found

A diagnosis on lavapipe (Mesa's software Vulkan driver, under Xvfb on Linux) at the tablet's 1340x800. Lavapipe is not the tablet: it says
what the controller does with a GPU that is too slow, where a frame's pixels go and how often the game waits, not the tablet's frame rates.

1. **The blur is the dynamic resolution.** E41's controller, with E47's rules, has one target, 58 frames a second, and it chased that target
   down past E47's 360-line floor (0.45 of 800 lines, which gives way under 30 fps) to its emergency floor, 0.30: a **402x240 picture
   stretched 3.3 times** over the panel, for a frame rate it still did not reach.
2. **Everything was in that picture.** The HUD, every text, the touch buttons and the pause panel are the screen overlay, and the overlay was
   drawn into the picture and stretched with it. In captures at 0.30 the hp/mp numbers, the signs and the hints could not be read.
3. **One size for every scene.** The picture's size was never reset per scene, so the menu, the heaviest scene, decided how sharp the levels
   were. E47 saw the same on the Galaxy A05 and left it open.
4. **Where a frame's pixels go.** Lighting is about **50-60% of the menu's pixel cost and about 36% of level 1's**, on lavapipe. Its share on
   the GE8320 is not measured.
5. **A finger in the menu drained the GPU.** The menu's cursor light follows a finger and casts shadows, so a moving finger rebuilds the
   shadow strips it projects, and every rebuild staged its geometry through two copies that each waited for the GPU queue to go idle: **2 to
   7 waits a frame, 34 to 254 a second** on lavapipe. Each wait drains the frames in flight while the player touches the screen, which is
   also when the resolution controller measures. Lavapipe counts the waits; what one costs on the tablet is not measured.

By the estimates, the tablet cannot hold 58 frames a second in the menu at any picture worth looking at. The choice there is a soft picture
at a frame rate it does not reach, or a sharper one at a steady lower rate: E50 takes the second where it can, and draws the words at the
window's resolution whatever size the picture is.

## What changed

Engine: four opt-ins, each off by default and none of them in `game.manifest`'s text. A game that asks for none of them runs as before.

1. **Host-visible meshes** (`MeshRegistry`, `MeshResidency::HostVisible`). `Upload` takes an optional residency. `HostVisible` puts the
   vertex and index buffers in mapped memory the CPU writes and the GPU reads, written and flushed with no staging, no copy and no wait.
   `Replace` keeps a mesh's residency and still writes fresh buffers (a frame in flight may read the old ones, and the shadow pass tells
   meshes apart by buffer), queues the old ones for the deferred destroy and bumps the generation, as before. The default, `DeviceLocal`, is
   the old path, and every existing caller passes nothing.
2. **The overlay at the window's resolution** (`GameManifest::screenOverlayPlacement`: `Picture`, the default; `WindowWhenScaled`;
   `Window`). The overlay's quads are recorded inside the swapchain pass, with the same shaders and blend, through a draw callback placed
   after the picture's image (`renderer/WindowOverlay.hpp`) and a second pipeline built for the swapchain's render pass. This is done only
   on an 8-bit UNORM swapchain, whose blend is the overlay's own; any other format keeps the picture path and says so once in the log.
   `WindowWhenScaled` draws on the window only while the picture is smaller than the window; `Window` always does. With the overlay on the
   window, `--screenshot` holds the picture alone and says so once, and `--screenshot-ui` holds the overlay. `Picture` builds nothing new and
   records the same commands as before. The E42 readout's size line ends with ", hud window" when the window path ran.
3. **Two tiers and a memory per scene** (`DynamicResolutionConfig::lowTierFps`, `qualityTargetHeight`, `qualityMaxShare`;
   `RenderScaleControl::NoteScene`, which the controller reads as `DynamicResolution::EnterScene`).
   - The **upper tier** is E47's controller aiming at the target, 58 fps, with its floor raised to a quality floor: `qualityTargetHeight`
     lines of picture, never more than `qualityMaxShare` (three quarters) of the window, nor more than the lines of art the scene shows.
   - Where the target cannot be held at the quality floor, or a smaller picture does not help (the CPU sets the frame), the controller goes
     to the **lower tier**: E47's controller aiming at `lowTierFps`, 30, with the 360-line and emergency floors. The picture is then kept
     as sharp as 30 fps allows. The lower tier rises by at least 0.03 a step: a rise sized as if the whole frame were the GPU's stalled a
     CPU-heavy level at the menu's 0.31 (found in the first run of the tests below).
   - From the lower tier the controller **probes back up** to 58 when the target is seen holding at the quality floor, or predicted to from
     the frame's non-waiting part and the pixel count. A failed probe is held 60 s, doubling; after two failures a scene is not probed again
     this session. Decisions are made from window medians, at the scale each window was measured at, so a hitch moves nothing.
   - **Each scene**, by a key the game chooses, keeps its tier, size, holds and failed probes for the session. A first visit starts where
     the last scene was and rises faster. An entry, a resume (`NoteResume`) and a window-height change **settle** for 30 frames and 1 s
     (a load frame counts as 0.1 s) before anything is judged, and an unfocused window feeds the controller nothing.
   - The log names each scene and each change of tier, and the E42 readout's size line ends with the tier (", tier 30" or ", tier 58"). A
     game that sets none of this and notes no scene runs E47's controller byte for byte: no new state is read and no new log line written.
4. **A ceiling** (`RenderScaleControl::RequestCapped`, `DynamicResolution::Capped`). A game could only pin the picture (`RequestFixed`),
   and a pin has no fall: on a slow GPU a pick of half the size held that size at a lower frame rate than the controller, which fell further
   and ran faster. A ceiling keeps the controller (its floors, its emergency floor, its relaxed rise and its target), starts it at the
   ceiling and gives it no lower tier, since a ceiling is asked for frames, not for a steady 30. On a loop the display paces, a ceiling under
   1.25 times the pixel floor is that floor, so a slow window falls straight to the emergency floor. A ceiling is kept across a
   window-height change. A game that never asks for one is unchanged.

Game:

- **`game/main.cpp`**, on Android and iOS (`kMobileBuild`): the shadow strips host-visible; the overlay `WindowWhenScaled`; and the tiers,
  `lowTierFps` 30 and `qualityTargetHeight` 540 (0.7 of the art's 768 lines; 0.68 on the tablet's 800 lines; the three-quarter share binds
  only on a window under 720 lines). The overlay's placement follows the build, not whether dynamic resolution is on: `--render-scale` turns
  it on for a computer's captures, and those keep the HUD in the picture `--screenshot` reads. On a computer, where dynamic resolution is off
  unless asked for, nothing changes.
- **`game/render/ShadowRenderer.*`** uploads the strips with `MeshResidency::HostVisible` when the layer's `hostVisibleStrips` option is set.
  The geometry is the same either way.
- **`game/render/PictureSize.{hpp,cpp}`** (new) holds the picture row's four values and what each asks of the engine. Automatic is the
  game's own configuration, the two tiers. Sharp is a pin at the start size, as in E47. **Balanced and Fast are ceilings** at 70% and 50% of
  the start size, aiming at 58 fps with no 30 fps tier; in E47 they were pins. The ids written to `settings.json`, the order and the labels
  are E47's, so a saved choice reads as before, no language needs a new word, and the settings file's spellings and version are unchanged.
  **For a player:** a saved Balanced or Fast can now go below 70% or 50% on a phone too slow for 58 fps there; only Sharp holds one size.
- The same file gives **the scene the controller is told of**: its key is the scene file as the game names it, and its lines are the
  logical screen's height, which is the number of the art's lines on screen whatever the window's size: 768 for a menu or an unzoomed
  scene, 768 divided by the zoom for a zoomed campaign level (439 at 175%). A level with fewer lines of art on screen has less detail to
  lose, so its quality floor asks for no more lines than it shows. `game/PenumbraLayer.*` tells the controller when the scene or its lines
  change (a load, or a zoomed level's end screen going back to 768 lines), not every tick, and only while there is a controller.
- `docs/playing.md` (`pictureSize`) and `docs/building.md` (the three flags) say the same.

The menu's own words, its logo and its cursor are scene sprites (the words are baked into `menu_buttons.png`), not overlay: they stay as
soft as the picture.

## The flags

Each one is over the build's default, for a measurement; `docs/building.md` lists them with the other developer flags.

| Flag | Values | A phone or tablet (Android, iOS) | A computer |
|---|---|---|---|
| `--shadow-strips` | `host`, `staged` | `host` | `staged`, as before |
| `--hud-res` | `picture`, `scaled`, `window` | `scaled` | `picture`, as before |
| `--low-tier` | `on`, `off` | `on` | `on`, inert: there is no controller unless `--dynamic-res on` |

- `--shadow-strips staged` is the old path, two queue waits a rebuild; the strips' geometry is the same either way.
- `--hud-res scaled` draws the overlay at the window's resolution whenever the picture is smaller than the window, `window` always,
  `picture` into the picture as before. On the window, `--screenshot` (the scene's picture, at the picture's own size) holds no HUD and the
  log says so once; `--screenshot-ui` holds the window as the player sees it.
- `--low-tier off` is one 58 fps target chased down to the floor, as in E47, with each scene's size still remembered: the A/B for the tiers.
- `--hud-res` and `--low-tier` remove the intro, as every flag a player would not type.

## Measured and tested

Three rigs, none of them the tablet: **lavapipe** under Xvfb on Linux (WSL, GCC) at 1340x800, for the counts and the captures; an **x86_64
Android emulator** on SwiftShader (another software Vulkan), for the Android build; and the engine's own **`test_gameruntime`**, which runs
the real controller against stand-in devices, for the tiers and the ceilings. The emulator's figures below are for the strips and the HUD;
the figures for the tiers and the ceilings are the simulation's.

The shadow strips:

- **Lavapipe**, the menu at 0.45 under a synthetic finger moving across it: from **34-254 waits a second** with the strips staged to **0**
  host-visible, except in the windows where a strip slot is made for the first time (6-9 a second there: the caster's texture uploaded on
  first sight, left for later).
- **The emulator**: from 22-46 waits a second to 0 in every window but one (21).
- Fifteen fixed-step captures byte-identical with the strips staged and host-visible, and to the build before; the finger sweep under
  the validation layers: no message. `test_device`: 26 checks (no wait for a host-visible upload or replace, fresh buffers, the old ones
  queued, the residency kept; the default path's 4 waits as the negative control).

The HUD at the window's resolution:

- **The emulator at 0.3**: the hp/mp numbers, the level, the hints, the pause panel and the touch buttons are crisp, and the frame rate is the
  same within a tenth (15.3 and 15.4 fps on SwiftShader).
- **Lavapipe**: at 0.45 and 0.30 the hp/mp text, the hints and the buttons are drawn with 1-pixel edges; at full size the window path is
  byte-identical to the old one, and with `Picture` fifteen fixed-step captures are byte-identical to before; the validation layers report
  nothing on the window path in level 1 and in the menu under a finger.
- The engine's suites: `test_screenoverlay` 40 more checks (the placement table, the window rectangle at 1340x800 and at a framebuffer
  scale of 2, the blend equal to ImGui's), `test_uilayer` 13 more, `test_gameruntime` 6 more.
- After each of these two changes the game's suites passed on Linux, `test_pn_all` 17 of 17, with no compiler warning.

The tiers and the memory per scene, **simulated**:

- The stand-in device gives each frame a CPU and a GPU time from per-megapixel costs that are **estimates**, the wait between them, seeded
  jitter and display-paced rounding. With the tiers off, the controller equals E47's over 1,000 random traces.
- The **tablet-like** device's menu rests in the 30 tier at **0.41 and 32 fps** (0.31 and 32 fps for a heavier estimate), where E47 fell to
  0.30; its levels at **0.49-0.67 and 33-37 fps**, including with a CPU-heavy 26 ms frame.
- A **Galaxy A05-like** phone's menu rests in the 30 tier at 0.56; its level probes up to 58 at 0.67 and 69 fps.
- A **Pad 5-like** tablet keeps 58 fps at 0.41 with no tier change through slow launch windows.
- **CPU-bound** phones keep the full size in the 30 tier and never probe.
- The suite also covers a display-paced loop, heat with failing probes, the scene memory, holds kept over five reloads, bursts of hitches,
  the load frame and the resume. `test_gameruntime`: 3,480 checks; GCC and MSVC, no warning.

The ceilings, **simulated**:

- A grid of 96 stand-in devices (a fixed GPU part of 0, 9 or 20 ms; 10, 40, 120 or 480 ms per megapixel; 720, 800, 1080 or 1600 lines;
  paced and unpaced; 180 s each): **no ceiling is exceeded**, and Balanced and Fast are never under 0.94 of Automatic's frame rate (the
  tightest, 0.978 and 0.983); a pin is never resized.
- On the tablet-like device's heaviest menu, E47's Fast held 0.5 at 12.6 fps; the ceiling falls to the emergency floor at about 33 fps.
- `test_gameruntime`: 3,628 checks; GCC and MSVC, no warning. `test_pn_render_touch` checks the row's ids, order and labels, byte for
  byte, and what each value asks of the engine (Automatic, a pin, two ceilings).

The whole, at the end (the four engine changes and the game's use of them, built together):

- **The engine**, built on its own on Linux: all 67 of its tests pass, the device-backed ones on lavapipe with a missing device counted as
  a failure; its documented suite counts agree with the build (66 suites registered); no compiler warning.
- **The game** on Linux: `test_pn_all` 17 of 17, no compiler warning; the fifteen fixed-step captures byte-identical to those of the build
  before E50, so a computer's picture is unchanged.
- **A live run** on lavapipe at one thread, at 1340x800 with the phone's flags, through level 1, level 2 and level 1 again: the controller
  tried 58 at the quality floor (0.68), went to the 30 tier in one log line, gave each level its own size, and found level 1 again
  "remembered". That device reaches 30 at no size (its frame is mostly the CPU's), so both it and the same run with `--low-tier off` end at
  the emergency floor: the run shows the tiers and the memory working, not a sharper picture.
- **A saved Fast** in `settings.json`: the game asks for it at the first tick and the engine logs "capped at 0.5"; level 1 is noted at 614
  lines (1340x800 at a zoom of 125%).
- **Windows** (MSVC, zero warnings, the laptop with Smart App Control enforcing; the build had been stopped twice by Claude Code for low
  memory at step 178 of 240 and was finished with one compile job at a time): `test_pn_all`, launched once with no refusal, 17 of 17
  suites and 0 failures (render_pause 8,381 checks and render_touch 9,357, the two that E50 changed; the others as before).
- **Android**, the debug APK of the final tree on the API 33 x86_64 emulator (SwiftShader, 1280x720; three ABIs built, 178 steps, zero
  warnings; the emulator's host process segfaulted once, exit 139, as the first run dumped its log, while the game was logging normal frames,
  so the other three checks were run again after a restart):
  - *Live policy*, menu, level 1, level 2, level 1 again, with the phone's defaults: the controller started at 1.0 on the 58 tier, fell to
    0.75, went to the 30 tier ("58 fps did not hold at 0.75") and rested near 0.56 at 29 to 32 fps; level 2 was entered at 0.56 on its
    first visit and fell to 0.44 at 23 to 25 fps (the software driver cannot hold 30 there); level 1 was found "remembered" at 0.56 and moved between 0.50 and 0.56 at 31 to 38 fps. The size line ends with ", hud window, tier 30".
  - *A saved Fast* (a settings file made for the run and removed after): the game logged "capped at 0.5 (asked by the game)" and level 1 at
    614 lines; SwiftShader cannot hold 58 at that size, so the ceiling fell to the 0.30 floor, as designed (30.7 fps there).
  - *A finger swiping across the menu at 0.45*: 28 of 31 one-second windows with 0.0 drain and 0 waits; the other three are the scene's
    load (62 waits) and the first two windows of the swipes (15 and 6), where a strip slot is made for the first time: the first-sight
    texture uploads this change leaves out (below). Before: 22 to 46 waits a second.
  - *The HUD at 0.3 in level 1*: the hp/mp numbers, the level, the hint text and every touch button are drawn sharp over a soft picture.

The engine pin, moved before the four changes, on its own: Linux `test_pn_all` 17 of 17 with the same check counts as before, and fifteen
fixed-step captures on lavapipe byte-identical to two runs of the build before. Among its commits, the Android audio stream is closed at
Home and a new one opened on the return: on the emulator it closed, and a new stream opened in 54 and 117 ms.

## Not verified

- **Nothing has run on the GE8320, or on any real phone.** The report's app version is not known either.
- **Every per-megapixel cost is an estimate.** Where the tablet's menu rests is the simulation's answer: probably still 0.30-0.41, a soft
  world under a sharp HUD. The levels' 0.49-0.67 is the same kind of answer.
- **How play feels at 30 frames a second on a touch screen.**
- **Host-visible vertex buffers on PowerVR, Mali and Adreno**: proven only on lavapipe and SwiftShader, and what a queue wait costs on a
  real GPU is not measured.
- The window overlay on a phone whose driver offers no 8-bit UNORM swapchain: such a phone keeps the picture path, which was not seen on a
  real one. The tablet's driver offers R8G8B8A8_UNORM (by the gpuinfo report, not on the device).

## What was left out, and why

- **E51: a steady 30 by pacing the frames.** The lower tier reaches 30 fps by the picture's size alone; its frames are not paced to the
  display. Making the swapchain FIFO for the whole run would mark every phone's loop as paced, which turns off E47's relaxed rise: a
  regression for the phones that hold 60. A paced loop also hides the GPU's time, and the tablet has no GPU timestamps to show it another
  way. Neither the software rig nor the emulator can check steadiness. It needs, first, a signal of a busy GPU that works on a paced loop,
  and a proof on a real phone.
- **The first-sight texture uploads** (3 queue waits each): the 6-9 waits a second that remain where a strip slot is made for the first time.
- **The re-resolve of every renderable on each `Replace`.**
- **A floor of its own for the menu**, and **keeping the scene memory across launches**: each launch starts every scene afresh.
- **A cheaper lighting tier.** The original's "pixel shaders off" lit each sprite per vertex. The port's row of that name does not do that:
  off, it drops the normal and specular maps and still lights every pixel, so it saves no time (E41 measured no change on the Pad 5). A
  cheaper lighting tier for slow GPUs, in the original's way, was considered and not done; lighting is 50-60% of the menu's pixel cost on
  lavapipe, and its share on the GE8320 is not measured.

## Risks

- **Host-visible vertex data on a tiled phone GPU** may cost more to read than device-local data. The strips are small, and this is not
  measured on any phone GPU.
- **A new pipeline inside the swapchain pass draws a phone's whole HUD.** A driver fault there would show in the HUD on every phone that takes
  the path. `--hud-res picture` is the way back for a measurement; there is no row for it on the options screen.
- **`--screenshot` on a phone build no longer holds the HUD** while the picture is scaled: a capture that needs it uses `--screenshot-ui`.
- **The tiers can move.** A phone that holds 58 at the quality floor only part of the time can go between the tiers; the 60 s hold, doubling,
  and the two-failure rule limit that, and a scene that failed two probes stays in the 30 tier for the session even if the phone later
  cools. A first visit to a scene starts at the size the last scene was at.
- **A saved Balanced or Fast can be smaller than before** on a slow phone: a player who wants one size held has Sharp only.
- **The simulation is only as right as its estimates**; the tablet may rest elsewhere than 0.41.
- **The engine pin crossed 34 commits** of the engine's own work, checked here by the suites and by byte-identical captures on Linux, not on
  a real phone.
