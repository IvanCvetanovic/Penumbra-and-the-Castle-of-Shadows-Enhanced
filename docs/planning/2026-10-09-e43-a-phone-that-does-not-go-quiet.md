# E43: a phone that does not go quiet (background sounds, a focus that heals, a smaller memory footprint)

Built 2026-10-09, in the engine (`engine/`: `AndroidApp.{hpp,cpp}`, `GameManifest::shadowMapResolution`, the performance overlay's touch line) and in the game
(`game/eth/AudioPrefetch.*`, `game/render/AudioOutEngine.*`, `game/main.cpp`).

## What and why

The Oppo A74 5G tester, on 1.0.7, said the game now runs smoothly but "sometimes the game becomes stuck like my touchscreen completely stops working. I think the ram gets overloaded
suddenly." Nothing was measured on that phone; a seven-lens review of the code (every finding re-read by a second reader) said:

- **No cause can be named from the code**, and the RAM theory is not supported: the game's footprint is about 0.4 to 0.6 GB (an estimate), nothing grows without bound, no step is above
  about 60 MiB, and a kill by the low-memory manager would close the game and show the report of E40, not freeze it.
- On Android the game's one thread reads the touch queue once per rendered frame and also runs the tick, the scene loads and the GPU waits, so **anything that holds it for seconds
  freezes the picture and the touch together**. The largest block that always happens is the first level start of each launch: setupScene asks for 13 MP3s (about 249 s of audio) and
  13 Ogg files (about 39 s), decoded whole on the game thread inside one tick with no event read (the decoded clips are kept for the process, so a death or a level change does not
  repeat it). Its length on the phone is unmeasured; it is the one stall that is certain to happen, once per cold start, at the first New Game.
- 1.0.7's resize path (a controller that could hunt, a `waitIdle`, a free and allocation of a ~100 MB target and three uncached bloom pipelines per resize) was fixed by E42.
- **A latent defect that fits "the picture runs and the touch is dead"**: `APP_CMD_PAUSE` clears the focus flag and `APP_CMD_RESUME` does not restore it, only a later `GAINED_FOCUS`
  does; the game reads every key and click as released while the flag is false (`InputState::Update`). A phone that resumes the activity without a focus-gain callback would leave the
  touch dead, with the picture still animating, until the next focus change. Whether any ColorOS build does is unknown.
- Memory that is not needed: 120 MiB of shadow maps (4x 2048 cascades, point and spot cubes) allocated whatever the game draws, in a game that casts no shadow; and 4x MSAA, about 92 MiB
  more than one sample at the A74's 2208x992 start target. A phone's GPU memory is the system's.

## What changed

Engine:

- **A finger landing on the window restores a lost focus** (`AndroidApp.cpp`, `ACTION_DOWN`): Android delivers a touch to the window being touched, so a finger on the game is proof that it has
  the input. When the app is resumed, has a window and believes itself unfocused, the flag is set again and a warning is logged (at most one in ten seconds), which also shows in a log that it
  happened. It does not change the order of the lifecycle callbacks.
- `Android::GetInputDiagnostics()` (fingers tracked, seconds since the last touch event of any kind, focus and resume flags) and a line in the performance overlay, its report and its log
  lines: "touch 0 fingers, last event 3.2 s ago, focus yes, resumed yes". A screenshot taken during a freeze then says whether touches still arrive and whether the game thinks it has the input.
- `GameManifest::shadowMapResolution` (0, the default: the engine's sizes; another number sets the cascades, the point cubes and the spot array to it, clamped 16 to 4096; never in the
  manifest's text). Every consumer reads `GetResolution()`.

Game:

- **`Eth::AudioPrefetch`** (`game/eth/`): a worker thread that decodes a list of files in order and hands each clip over when the game thread asks. A clip the worker has finished is moved
  out at once; one it is decoding is waited for (at most that one file's remainder); one it has not started is taken off the queue and decoded by the caller, as before: it is never
  slower than the inline decode. The decoder is a parameter (`Eth::LoadSound`), so the class is tested with no files. `AudioOutEngine` starts it the first time a sound is asked for from
  a folder named `soundfx` (the menu's `menu.mp3`), on every platform but Windows (whose MP3 decoder is the engine's Media Foundation), queueing the rest of that folder by size, largest first
  (the two pieces of level music); the clips are added with the same `AddClip` the inline path used.
- On a phone: **small shadow maps** (16x16) and **one sample per pixel**, in the release as well as the test build (E42 took one sample in the test build only). A computer keeps both as they were.
  Developer flags `--shadow-maps <16-4096>|default` and `--scene-samples 1|2|4`.

## Measured and tested

- Linux: `test_pn_all` 17 of 17 suites (48,435 checks; `test_pn_audio` has 17 new checks for the worker: order, a failure's reason, spellings of one path, a duplicate request, a file the caller
  claims is never decoded twice, a file being decoded is waited for, the destructor joins); the engine's `test_gameruntime` 1,324 checks. MSVC `/W4` is clean for every changed file that MSVC compiles.
- An Android 11 emulator (x86_64; its numbers say nothing of a phone): the log says `Shadow maps at 16x16`, `1x MSAA` and `Decoding 34 more sounds ... in the background`; **tapping New Game
  after about 36 s on the menu took all 24 of the level's sounds, 120 s and 82 s of music among them, from the worker within 3 ms**; the panel shows the touch line and the log lines carry it.
- **Not measured**: how long the first level start blocked on a real phone before this (the claim "seconds" is unmeasured), whether it explains the tester's freeze, and whether the focus
  healing ever fires on a real ColorOS phone. The focus path cannot be driven from `adb`: it needs a PAUSE and RESUME without a GAINED_FOCUS.

## Open

- The pause panel (auto-pause on any focus loss) takes only two 316x44 rows as taps; a tap anywhere else is ignored. On a touch build a tap outside the panel could resume. Not done.
- Three unbounded GPU and swapchain waits in `DrawFrame` (`UINT64_MAX`: the frame fence, the acquire, the image fence) would be a permanent freeze and an ANR if the GPU or the compositor
  ever stopped answering; nothing shows that they do. Not changed (slicing them needs the frame counter undone on a timeout).
- The scene is parsed again at every death and checkpoint, inside a tick; the cost on a phone is unmeasured.
