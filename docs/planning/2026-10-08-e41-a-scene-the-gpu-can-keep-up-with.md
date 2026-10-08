# E41: a scene the GPU can keep up with (sprite-only scene shaders, dynamic resolution)

Built 2026-10-08, in the engine (`engine/`, additive and opt-in: `DynamicResolution.{hpp,cpp}`, the scene fragment shader's constant 0, `VulkanPipelineOptions::spritesOnly`,
the manifest's two fields) and in the game (`game/main.cpp`: the defaults and three developer flags).

## What and why

A reviewer said the game ran at very low frame rates on the main screen, and the owner saw the same on his Xiaomi Pad 5 (Snapdragon 860, Adreno 640, a 2560x1600 panel at
120 Hz, Android 13). With the tablet on a cable the game was measured, not guessed at (`dumpsys SurfaceFlinger --latency` for the frames the compositor showed, `top` for
where the CPU time went):

- The released 1.0.6 drew the **main menu at 8.8 frames a second** (117 ms a frame) and level 1 at 27. The game's thread used about 20% of one core with the other seven
  idle: the GPU was the limit. The offscreen target was the window's 2560x1600 at 4x MSAA in 16-bit float, and the picture is a 2010 game's art.
- What did not matter, each measured on its own: the sample count (4x to 1x: 8.8 to 9.3 fps), the shadow passes (skipped: no change), MAILBOX against FIFO, the game's
  pixel-shader option (normal and specular maps off: no change), and the lights' per-pixel data being a storage buffer or a uniform buffer (a uniform buffer was a little
  slower). What did: **the pixel count** (half the width and height: the menu at 29.6 fps, level 1 at 86; the frame time scaled with the pixels almost exactly) and the
  **lights**: with none, the menu took 50 ms instead of 117, and a light's own shadow strips (a loop per light per pixel) were about 25 of the 66 ms.
- The one scene shader is the engine's general one (physically based surfaces, clustered lights, cascades, probes) with the sprite path in front of it. Compiled that way
  the GPU schedules a very large shader even for a sprite. Folding the unlit path in at pipeline creation (a specialisation constant) took the menu from 117 to 75 ms and
  level 1 from 33 to 25 ms.

## What changed

Engine (a game that sets neither field behaves exactly as before):

- `GameManifest::spritesOnlyScenePipelines` (not in the manifest's text): the four scene pipelines built from `frag.spv` get specialisation constant 0 of `shader.frag`
  (`SPRITES_ONLY`, default false), which only widens the unlit test: every draw takes the unlit block (the sprite exit or the plain unlit exit, unchanged), and the driver
  removes everything after it. A draw that is not flagged unlit would be drawn unlit, so a game that draws a lit mesh through those pipelines must not ask. `frag.spv` is
  the SDK `glslc`'s output for the new source (the unmodified source reproduces the old blob byte for byte).
- `GameManifest::dynamicResolution` (`DynamicResolution.hpp`, not in the manifest's text): the scene target's size, chosen by how fast the GPU draws it. The median frame of
  each 1.2 s window against 58 frames a second; below 94% of it the scale falls by the square root of the shortfall (cost follows the pixel count) less 3%, by at most
  half in a step; above 110 fps for eight seconds on end it rises, by a quarter at most and never past where 20% of the target is spare; frames longer than 1.5 s (a load or
  a long hitch) count for nothing; the floor is 0.3. **A step down must pay for itself**: the controller sees the length of a frame, not what it waits for, and a frame the CPU or
  the driver limits does not follow the pixel count, so every step down is judged by the window after it, and when less than a quarter of the gain the pixel count predicted
  arrived (and the target is still missed) the step is taken back and not tried again for 30 s, then 60, then 120 (up to 480): a phone that is not GPU-bound keeps its picture.
  A much heavier scene (the frames three tenths slower than at the revert) ends the hold early. The target is **created** at the start size, so the first frame is not a
  full-window one and no resize waits for the second. The target is the window's size times the scale (even, at least 64), stretched to the window by the same `ImGui::Image`;
  what a finger or a HUD element is placed by is the window, so a tap lands where it did. Never for a capture or a fixed-step run (their pictures must reproduce), except
  a fixed scale (floor equals ceiling), which is how `--render-scale` works.

Game (`main.cpp`):

- The sprite-only pipelines are on for every build (the game draws no lit mesh); a computer's pictures are asserted unchanged by byte-identical captures (below).
- Dynamic resolution is on for a phone or tablet (`kMobileBuild`) and off for a computer, starting at the largest scale that fits 2.2 megapixels (a 2010 game has nothing
  more to show on a 4-megapixel panel) with the controller refining it from there. A PowerVR GE8320 phone needs a far smaller picture than the Adreno 640, and each now gets
  the largest one it keeps 58 frames a second at.
- Developer flags (they remove the intro, like every flag a player does not type): `--render-scale <0.1-1>` (fixed), `--dynamic-res on|off`, `--sprites-only on|off`.

## Measured and tested

- The Pad 5 (Adreno 640, 2560x1600), the build of this change installed beside the player's copy under another package name: menu 8.7 to **61.7 fps**, level 2 61.7, level 1 27
  to **76** (the controller's scale settled at 0.39 in the menu, 0.64 and 0.69 in the levels, in about six seconds from a start at 0.73); the sprite-only shader at full size
  alone: menu 13 fps, level 1 43. A tap on New Game with the scene at half size started the level.
- Fixed-step captures (`--fixed-step --frames 150 --screenshot`) of the menu, level 1 and level 2 with `--sprites-only on` and `off` are **byte-identical** on that GPU.
- Linux (GCC, WSL): `test_pn_all` 17 of 17 suites pass; the engine's own suites `test_gameruntime` (1,238 checks: the controller on stand-in GPUs, the opt-ins off by
  default and never in the manifest's text), `test_materials` (454: the constant's spelling and its single use) and `test_light2d` (281) pass, 0 failures. Fixed-step
  captures of the menu and levels 1 and 2 with `--sprites-only on` and `off` are byte-identical on lavapipe too, so a computer's pictures do not change; a capture with
  `--render-scale 0.5` is the half-size target (640x360 from a 1280x720 window). Windows: `tools/check.bat` (MSVC /W4) is clean for every changed C++ file; `test_pn_all.exe`
  was not built or launched there.
- Reviewed by three independent readers (the controller, the shader and pipelines, the game side) with a skeptic per finding before it was committed. A first version of the
  constant made every draw take the sprite path, which would have drawn a particle, a halo or a shadow blob with lights: it now widens only the unlit test (a second skeptic,
  of the GE8320 analysis, had found the same). The review's real findings were fixed: a step down that does not pay is taken back (above); the target is created at its start
  size; the capture guard also covers `--screenshot-ui`, `--screenshot-every`, `--record` and `--replay`; the new pipeline option is the struct's last member; the shader
  test counts the constant in code, not comments, and checks that `frag.spv` carries it. Final controller tests on stand-in GPUs: a CPU-bound one spends under 10% of five
  simulated minutes below its start size, a tablet-like one (10 ms fixed + 81 ms following the pixel count) settles in a few steps with no revert.
- The final build on the Pad 5: the target is created at 1868x1168 (0.73), the menu settles at 0.39 and 60.7 to 61.0 fps, level 1 71 and level 2 63 fps (steady state).
  A reading taken in the middle of a resize reads lower (54.5 once): a resize is a frame or two.

## Open

- **A display-paced loop never gets a larger picture.** The scale rises only on proof of room, which is frames far above the target (110 fps for eight seconds), and a loop
  paced by the display's refresh (a FIFO present: MoltenVK, or a device with no MAILBOX) cannot show that: there 2.2 megapixels is the most the picture has, and only the
  fall works. The engine's mailbox present is unpaced.
- **A scripted run is not at full size.** A phone build started without `--frames`, `--screenshot` or `--fixed-step` (the `--splash off` plus `adb shell input` plus
  `adb screencap` recipe) has the controller on and its first seconds at a smaller scale: add `--dynamic-res off` (or `--render-scale 1`) for a full-size picture.
- **A guard against another cause:** a step down that is taken back is logged ("the last step down did not speed the frame up, taken back"), which names a phone whose slow
  frame is not the picture's size (the CPU, the driver, a display's pacing). Whether the Oppo A74 5G is one is not known.
- **The Pad 5 only.** The Oppo A74 5G (Adreno 619) and the PowerVR GE8320 phones are expected to gain far more, and none has run it. The A54 and A04e reports are, first of all,
  a question of which version they run (their database entries read Vulkan 1.1.131, which closed release 1.0.5 and which 1.0.6 accepts).
- The menu is the heaviest scene (the lights and their shadow strips are about half of its frame at any size); a cheaper light loop would let it keep a larger picture. Not
  attempted: it must reproduce the same picture.
- At the settled scale the HUD's text and the touch buttons are softer than at full size; the menu at 0.39 of a 2560x1600 panel is about 1000x620 pixels. A setting that pins
  the scale, or a native-resolution overlay, is not built.
