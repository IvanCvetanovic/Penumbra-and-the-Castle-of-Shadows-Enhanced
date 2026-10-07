# E38: a start that cannot look like a crash

Built 2026-10-07, in the engine (`engine/`, additive and opt-in) and in the game (`game/main.cpp`, `game/PenumbraLayer.*`, `game/eth/StartupErrors.*`). The
player's view is in [`../playing.md`](../playing.md) ("A start that takes long, or stalls") and [`../install.md`](../install.md) ("It didn't start?"); the
row is in [`../enhancements.md`](../enhancements.md).

## What and why

A tester downloaded the Windows release, double-clicked `Penumbra.exe`, and saw a black (or white) window that did not answer; Windows offered "Wait"
or "Close the program". They sent no log, no version, no hardware.

What the code shows, read at 1.0.5 (cb49562) and not run on their machine:

- The engine makes a visible window at once (`Window.cpp`: GLFW shows it inside `glfwCreateWindow`; nothing hides it) and the first read of the window's
  messages is `glfwPollEvents` at the top of the first frame (`SupersonicApp::Run`). Between the two come the Vulkan instance and device, the swapchain,
  the renderer (shadow maps, the ImGui font atlas), every pipeline the driver compiles from SPIR-V (about a dozen on a first run: the release ships no
  cache), the game layer's whole `OnAttach`, and a first-launch fullscreen. Windows calls a window "Not responding" after about five seconds without a
  message read, and GLFW paints nothing into it, so any stretch of five seconds there is exactly the report.
- A first launch is fullscreen (E23: `Settings::fullscreen = true`), and the game asked the monitor for its highest refresh rate at the top of the
  first frame (`ApplyPending`, before the first poll): a real display-mode switch, which blanks the display and holds the thread in the driver's call
  for seconds on some monitors and TVs. A laptop panel offers one rate, so no machine the game had run on ever did it.
- Nothing in the code proves which stretch was slow on their PC, and the log could not say: its lines had no times, and the last line was all it named.

Not found, and looked for: a deadlock, an unbounded loop, an infinite wait that is satisfiable only by a device (every fence and acquire wait was read: they
are satisfiable, and a lost device throws), a missing file, a missing DLL, an instruction the exe needs and an older CPU lacks (the release exe has no
unguarded AVX), a baked developer path that wins over the files beside the exe. The other possible reading of "the exe from GitHub" (the 2010 original's
`machine.exe` or its installer, which the repository also holds) cannot be told apart from the report; the questions that do are under Open.

## What changed

Engine (additive: nothing changes for a game that does not ask):

- `GameManifest::pumpEventsDuringStartup` (default false, set by a game's main, never in game.manifest's text). True makes `SupersonicApp` read the
  window's events between the stages of its constructor (after the display start, the Vulkan instance, the device, the swapchain, the renderer, the
  editor layer, the pipelines, the asset scan) and around the layer's `OnAttach` (`PushLayer`), and `VulkanRenderer::SetStartupPump` makes it read them
  after each pipeline is built. So the longest a window goes unread is the longest single stage. GLFW windows only (a phone's `PollEvents` blocks in the
  background). It stops when `Run` begins (`m_running`), and the renderer's hook is cleared once the pipelines are built (it captures `this`).
- Before the swapchain is made (`settleWindowForSwapchain`): a window minimised during the start, or iconified by the focus loss of a fullscreen one, is
  waited out as `RecreateSwapchain` waits at run time (a swapchain cannot be made on a window of no size), or restored when a close was asked for, and the
  resized flag the pumps raised is cleared. Without it the new pumps would have turned an alt-tab during a slow start into a failed one: the old start
  read no message until its swapchain existed. A close asked for during the start also skips the layer's attach, and `Run` ends at once.
- `Log::SetElapsedTimestamps(bool)` (default off): the file sink's lines carry the seconds since the first line, `INFO +1.234s [Window] ...`. The console
  and the in-memory buffer are not stamped.
- New log lines, each a stage that can stall: "Renderer ready", "Layer '...' is ready" (after `OnAttach`), "First DrawFrame returned", "Entering
  fullscreen on ... (the desktop's mode)" before the call that lists the monitor's video modes, "Switching <monitor> to WxH @ R Hz (it runs ...)" before a
  real display switch (the existing line after it is written only when it returns), and "Looking for gamepads" / "Gamepad scan done in N ms" around GLFW's
  first joystick call, which finds every game controller on the PC on the window's thread. No existing line changed.

Game:

- `main.cpp` turns both on (`pumpEventsDuringStartup = true`, timestamps from the moment the log file opens).
- With the intro on, the launch's fullscreen request (the monitor's highest refresh rate) is made on the second `OnUpdate`, when the first `DrawFrame` has
  returned, instead of in `OnAttach` (`PenumbraLayer::FinishLaunch`). A start with a development flag has no intro and asks at once, as before.
- An unfinished-start record (`eth/StartupErrors`: `ReadStartRecord`, `BeginStartRecord`, `OpenWindowedAfterUnfinishedStart`, `KeepUnfinishedLog`,
  `EndStartRecord`): a normal desktop start reads `start-unfinished` in the user folder before the log file is opened, and writes it just before the engine
  starts. The layer removes it on the third `OnUpdate`: by then the first frame has been drawn and the display switch, applied at the top of the third
  frame, has returned, so a hang in that very switch leaves the mark. A start that finds it, and was given no `--fullscreen`, `--windowed` or `--window`,
  opens in a window this once (`options.windowed`, as the flag), writes a line in the log, and changes no setting; and the log of the unfinished start is
  kept as `penumbra-unfinished.log` before the new one replaces it. A capture or test run (`--frames`, `--screenshot`, `--fixed-step`) keeps no marker
  and does not read one; neither does a phone; a failure the player was told of removes it.

## Review

Four reviewers, each trying to break the change from one side (event-pump safety, "additive" for the other project that uses the engine, the game's launch
logic, the tests), read the diff before it was built. What they found, and was changed: the new pumps could turn a minimise during a slow start into a failed
start (the swapchain wait above); the marker was removed one frame before the display switch it guards was applied; the recovery start overwrote the log of
the failed one, which is the evidence this exists to collect; the first log test passed against a `Log::Submit` that never wrote the time (found by the
engine test itself, before the review; the review's checks then also required that the times be real and rising, and that the console buffer stay
unstamped); the layer test had a block that could not fail, and counted frames and ticks alike; the marker's rules were in `main.cpp`, where no suite
reaches them. What they found and was left: the layer's `OnAttach` is one unread stretch (nothing heavy in it was found, and the engine's own stages around it
are covered); "First DrawFrame returned" is the call returning, not proof that pixels reached the screen.

## Measured

On the development machine (AMD Radeon 780M, a 1920x1200 panel with one mode, Windows 11, a warm driver cache), from `penumbra.log` of the real runs of the
first build of this change (the later fixes are covered by the suites):

| | |
|---|---|
| A flagless start, fullscreen (first-launch path) | window made at +0.29 s; "Entering fullscreen" to "Fullscreen on": 22 ms; renderer ready +0.88 s; layer ready +0.94 s; gamepad scan 42 ms (no pad); first frame +1.01 s; the launch's fullscreen request at +1.010 s, after it ("already at" the desktop's mode: this panel offers 60 Hz only) |
| A windowed start with the intro (1024x768) | window made +0.30 s; the Vulkan instance +0.60 s; the swapchain +0.85 s; the renderer +1.02 s; first frame +1.12 s; the unread stretch before this change, window to first poll: about 0.8 s |
| The same with no pipeline cache | the pipelines: 21 ms on this driver (it keeps a cache of its own; not the tester's case) |
| A start with the marker planted | "The last start did not get as far as ...: opening in a window this time"; a window of the saved size, no "Fullscreen on" line; marker gone after the first frames; `settings.json` byte-identical before and after |

## Checked

- Engine: `test_gameruntime` 296 checks, 0 failures on the final build (new: the flag is off by default and never in the manifest's text; the log's time prefix, its form, that the time is the
  clock's and rises, that nothing is stamped when off, and that the buffer is unstamped).
- Game: `test_pn_paths` (the record: written, found, a window unless the player named a mode, no marker for a capture, a phone or no folder, the unfinished
  log kept and an older one replaced; the log's times as `main.cpp` and the guides rely on them) and `test_pn_render_hud`'s `TestLaunchAfterTheFirstFrame` on
  a stand-in `WindowControl` (with the intro: nothing at attach or in the first frame, one request on the second; the marker through it and gone on the third;
  frames, not ticks; a window the player left is not put back; without the intro the request at attach; a layer given no marker leaves another's alone).
- Linux (WSL, GCC): `test_pn_all`, 17 suites, 48,011 checks, 0 failures, from the working tree (which also held the unreleased E36 rework) (`test_pn_paths` 203 checks, `test_pn_render_hud` 15,068). Windows: the
  first build of this change ran `test_pn_all` once, 16 of 17 suites passing, the only failures the 18 known E25 glyph-fit ones of `test_pn_render_hud`
  with the real Arial Narrow (unchanged); the final build's `test_pn_all.exe` was refused by Smart App Control (exit 126) and is **not run** on Windows.
- Built with zero warnings (MSVC /W4); the real runs above, on the first build.

## Open

- The tester's cause is unknown. This change makes a slow start answer (a window that moves and closes, a log that says where the time went, and a second
  start that opens windowed and keeps the first one's log), and cannot make one single stage shorter than it is: a display switch or a driver call of five
  seconds still ghosts the window. Their next report should carry `penumbra-unfinished.log` (or `penumbra.log`) from `%APPDATA%\Penumbra` and the answers to:
  the exact file name and where it came from (the green Windows button; "Download ZIP"; "Source code"), the window's title bar ("Penumbra" is this edition),
  whether a second black console window opened (the 2010 original opens one), whether the picture covered the whole screen, whether it came alive after a
  minute, and the monitor and graphics card.
- Reading the tester's next log. If it comes from the shipped 1.0.5 (no times, none of this entry's lines), its last line names the stage:

  | Last line of `penumbra.log` | Where the start stopped or was slow |
  |---|---|
  | `[Window] GLFW Window created: ...` | The first fullscreen's listing of the monitor's video modes (a driver question per mode on some adapters), or the Vulkan instance |
  | `[Window] Fullscreen on <monitor> at ...` | `vkCreateInstance`: the loader, or an overlay's implicit Vulkan layer (the dev log shows two stale Steam ones failing harmlessly) |
  | `[VulkanDevice] Selected Physical GPU: ...` | Creating the device: the driver, a laptop waking its second GPU |
  | `[PipelineCache] Starting empty; ...` | Compiling the first eight pipelines (a slow driver, a cold cache); `Saved N bytes` as the last line: the last two |
  | `[SupersonicApp] Attached layer 'Penumbra'.` | The game's own load (`OnAttach`) |
  | `[SupersonicApp] Starting Main 3D Game Loop...` and an `E23 fullscreen (launch): ... rates at this size: 60, 144` line but no `Fullscreen on ... switched from the desktop's` | The display switch to the highest refresh rate |
  | `Starting Main 3D Game Loop...` and one rate only | The first gamepad scan (Bluetooth, virtual or many controllers) or the first frame itself |
  | `[AudioEngine] Loaded ... menu.mp3` or `Supersonic intro: finished` | After the intro: the menu's load, Media Foundation's first use, or XAudio2's voices (an odd audio device) |
  | No `penumbra.log` at all | `main` was never reached (SmartScreen, antivirus, a missing DLL) or a different exe ran: the 2010 original writes none |

  From this build on the same stages carry times, so slow and stuck can be told apart. One experiment for the tester that splits the first launch from the rest: a shortcut to
  `Penumbra.exe` whose Target ends with ` --windowed --splash off`.
- The display switch ("Switching ... to ... Hz") has not run on a real monitor that offers a second refresh rate at its size; the single-rate panel here
  never takes that branch. Its log line is read from code only. The wait for a minimised window before the swapchain has not been run either (it needs a
  window minimised during a slow start).
- Not changed, on purpose: the waits with no timeout on the GPU (`vkAcquireNextImageKHR`, the fences: satisfiable, and an unhealthy device throws), the
  compile of the pipelines on the main thread (21 ms here; moving them to workers is the next step if a log shows seconds), and the choice of the
  highest refresh rate as the automatic one (E23's, kept).
- The release exe has never been launched on any Windows machine other than the development one. `test_pn_all.exe` of the final build was refused by Windows
  Smart App Control (exit 126) and is not run on Windows; Linux ran every suite.
- The new engine commits (the other project's `--hidden` and `ResolveStartWindow` among them) came with `git -C engine pull --ff-only`, and the game
  builds and passes against them. The engine changes of this entry are one engine commit, and this repository's pin to it is a commit of its own.
- The suites ran on a tree that also held the unreleased rework of E36 (the difficulty chosen at New Game); the commit of this entry holds only E38's hunks,
  so the committed tree was not itself built here: CI's Linux job is its first full build.
- The Build Tools update moved the compiler (14.50.35717 to 14.51.36231), so the old `build/` could not build; its `CMakeCache.txt` was removed and the
  tree reconfigured; the engine's own suites were made available for the run (`-DSUPERSONIC_BUILD_TESTS=ON`; they are off in a subproject) and the option put back to OFF afterwards.
