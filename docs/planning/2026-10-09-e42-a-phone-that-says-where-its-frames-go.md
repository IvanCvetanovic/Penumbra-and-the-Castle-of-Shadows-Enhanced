# E42: a phone that says where its frames go (frame statistics, an overlay with Share, and the cheap costs removed)

Built 2026-10-09, in the engine (`engine/`, additive and opt-in unless it says otherwise: `FramePerf.{hpp,cpp}`, `PerfOverlay.{hpp,cpp}`, the profiler's `QueueDrain` zone, four
manifest fields, `Android::ShareText`) and in the game (`game/main.cpp`: three developer flags and the test build's defaults; `tools/build_android.sh --perf-diag`).

## What and why

Release 1.0.7 (E40 and E41) was run by the Oppo A54 tester, who opened the game and found it **too laggy to play**; the A74 5G tester had said the same of 1.0.6. The A54 4G is a
MediaTek Helio P35 with eight Cortex-A53 cores and a PowerVR GE8320, 720x1600: the weakest phone the game has met, and the one class nobody here can run (the 1.0.7 numbers
are from a Xiaomi Pad 5 and a Pixel 9 Pro XL). A review of the code and of the hardware (a seven-lens analysis, every finding re-read by a second reader) said, in short:

- The GE8320 is about **15 times slower** than the Pad 5's Adreno 640 at per-pixel work (range 10 to 24: 3DMark Sling Shot 739 against 10,766 for a Poco X3 Pro, the same SoC
  as the Pad 5; Manhattan 3.1 offscreen 6.7 fps against 69) and its CPU two to three times slower. The Redmi 10A (Helio G25, the same GPU) scores 736: its tester probably
  lags too and has only said the game does not crash. From the Pad 5's measured costs the model predicts 1.0.7 at about **9 fps in level 1** at the A54's starting size and
  about 3 in the menu, and about 19 and 15 after the size controller has done what it can: **60 fps is out of reach by shrinking the picture alone**.
- The size controller (E41) can only remove the part of a frame that follows the pixel count. Two real defects: after a revert the hold was released against the speed at the
  *smaller* size, so a step that had bought 43% or more cancelled its own hold and the picture hunted between two sizes with a resize every second or two (a frame of 100 ms
  fixed plus 100 ms of pixels: 64 changes in two minutes, ending at full size and 5 fps); and a step needed a quarter of the gain the pixel count predicts (75% for a halving), so
  a frame with a fixed share of 40% was held at the slower size (15 fps, where the next size gave 20).
- Costs that do not follow the pixel count, none of them measured on a weak phone: four empty 2048x2048 cascade depth passes cleared and stored on every frame a scrolling camera
  crossed a shadow texel (64 MB of writes a frame in a game with no shadow casters: the Pad 5's "no effect" test was on a static menu, where the cache already held); a 4x
  RGBA16F colour and depth target (55 MB at 1600x720, on a tile-based GPU whose tiles shrink as the samples rise); a window-sized swapchain pass presented on a rotated surface;
  idle waits on the game thread (a texture's first use is three queue waits, a moving real-time shadow strip two); a per-frame stat of every watched file (virtual keys on a
  phone, so every one fails); a pipeline cache that never reached the bloom chain or ImGui.
- **And no way to know which of them matters on the A54**: the log of an installed Android app is in private storage (Android 11 hides `Android/data` from file managers), the
  profiler's zones were printed only at the end of a `--frames` run, nothing counted the idle waits, and the size controller sees a frame's length, not what the frame waited for.

## What changed

Engine (a game that sets none of the fields behaves as before; the two fixes in the first list change what a game does, and not what it draws):

- **An empty shadow pass stays cached** (`RenderSystem::ShadowPassSignature`): with no caster in a pass's view the pass is a clear to the far depth whatever the light matrix is, so
  its signature is the seed and the count alone. The first frame and the frame the last caster leaves still record. Output unchanged.
- **`DynamicResolution`**: the hold is released against the speed before the step; a step that bought at least 15% is kept (`kEnoughBenefit`); `Reconfigure()` takes a new
  configuration while running; `WindowMedianMilliseconds`, `WindowMissedTheTarget` and `AtTheFloor` for a log line (the floor is no longer silent).
- **The renderer's pipeline cache is created first**, so the bloom chain (rebuilt at every resize of the scene target) and ImGui use it.
- `GameManifest::perfOverlay`, `perfLogSeconds` (`PerfOverlay.hpp`, `FramePerf.hpp`): the frame rate, the median, 95th percentile and worst frame, the part of each frame spent
  waiting for the GPU or display, in the game's own layers, recording the scene, the shadows, resource sync and idle waits, a one-line verdict (GPU or display bound, CPU bound,
  mixed, stalls: idle waits count as waiting), a **Share** button (Android's share sheet with the end of the game's log appended, `Android::ShareText`; the clipboard elsewhere),
  and buttons that **fix the scene target's scale** (Auto, 1.0, 0.6, 0.4, 0.3) so a player can measure, in one place, the part of the frame that follows the pixel count and the part
  that does not. The zones are read at the top of the next frame, before `Profiler::BeginFrame` zeroes them. A log line of the same numbers every `perfLogSeconds`.
- The profiler's **`QueueDrain` zone and a count** (`Profiler::DrainScope` around the five queue waits behind a one-off command buffer and the device waits a running game can reach).
- `GameManifest::assetWatching` (default true): false skips the per-frame stat of every watched path and the hash of every material's paths.
- `GameManifest::sceneSamples` (0, the default: the device's best up to 4, as ever; 1, 2 and 4 cap it; applied when the target is made).

Game: `--perf-overlay on|off`, `--perf-log <seconds>`, `--scene-samples 1|2|4` (developer flags, they remove the intro); on a phone `assetWatching` is false. A **test build**
(`tools/build_android.sh --perf-diag`, the CMake option `PENUMBRA_PERF_DIAG`, stated explicitly on every run so it cannot carry into a release) has the overlay on, a log line every
five seconds and one sample per pixel: the build sent to a tester's phone, so that what comes back says where the frames go. A release has none of those on.

## Measured and tested

- Linux (GCC, WSL): `test_pn_all` 17 of 17 suites pass (48,114 checks); the engine's `test_gameruntime` 1,312 checks, `test_materials` 456, `test_light2d` 281, `test_shadowcache` 87, 0 failures.
  The controller changes are tested on stand-in GPUs and were replayed in a Python port that reproduces the old numbers (64 changes ending at full size; 7 ending at 0.5 and 8 fps
  now). MSVC `/W4` (`tools/check.bat`) is clean for every changed C++ file.
- A headless run on lavapipe with the overlay: the log line (`Perf: 28.1 fps, frame 35.5 ms ... wait 0.2 game 0.6 scene 28.9 ... drain 0.0/0, ticks 2.14, scale 1.00 1280x720 of 1280x720,
  4x, Mailbox`) and the panel are as designed. On that software renderer the first second of level 1 shows 113 idle waits (about 19 ms a frame): the textures of a level being
  decoded and uploaded on first sight, which a phone will show too.
- **An Android 11 emulator** (x86_64, SwiftShader: its numbers say nothing about a phone's), the test build (`--perf-diag`, 1.0.8-test1 as a temporary version label that is not
  committed: `test_pn_paths` insists the manifest's versionName equals the CMake version): the log says `1x MSAA` and a `Perf:` line every five seconds; the panel draws over the menu with
  finger-sized buttons (the first version's were 2 mm); tapping 1.0 and then 0.3 fixed the scene target at those scales (`Perf: size 1.0, scale 1`); **Share opens the system's share sheet**
  with the report text and the Copy button. Building the test APK found that the share code's two Java string literals had been committed with real newlines (the Linux suites and the
  MSVC check never compile Java): repaired in the same commit as the buttons.
- `--scene-samples 1` against 4: 3.4%, 3.1% and 4.5% of the pixels of the menu and levels 1 and 2 differ, by about 2 levels on average, at hairline seams between tiles and at sprite
  outlines (lavapipe has no 2x: it gives 1x).

## Open

- **Nothing here is measured on a PowerVR phone or an A53 CPU.** What the test build is for.
- Not built, in the order the analysis ranks them for a weak GPU: an unlit pipeline variant (the scene shader is 950 SPIR-V instructions for every quad, an unlit-only fold is 132;
  the menu's fog is about three unlit layers a pixel); a cheaper light loop (the strips loop alone is 506 of the 950); batching the one-off uploads of textures and shadow strips
  (three and two queue waits each); swapchain pre-rotation; 8-bit scene colour; fusing the HUD overlay pass; the first New Game's synchronous decode of about 250 s of MP3.
- The overlay is English-only tester text (`verbatim`, outside the localisation rule), drawn by ImGui over the picture, and a release has no way to switch it on yet.
