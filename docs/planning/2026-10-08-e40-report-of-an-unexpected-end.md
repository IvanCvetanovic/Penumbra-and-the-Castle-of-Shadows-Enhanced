# E40: a report of how the last run ended, for a phone whose game closed with nothing said

Built 2026-10-08, in the engine (`engine/`, additive and opt-in: `SupersonicActivity.java`, `PostMortem.java`, `AndroidApp.cpp`, the renderer's log lines) and in the game
(`game/main.cpp`, `game/android/*`). The player's view is in [`../install.md`](../install.md) ("It didn't work?") and the owner's in [`../building.md`](../building.md).

## What and why

Release 1.0.6 (E39) made the game accept a phone whose graphics report Vulkan 1.1, and put a dialog in front of every start that *throws*. The Oppo A74 5G then ran it
(slowly). The Oppo A54 (the 4G model: a MediaTek Helio P35 with a PowerVR GE8320, Android 11) went on closing "the same way": the window opens, the screen goes black, the
phone shows its home screen.

That is not a failure the dialog can see. It speaks only when a `std::exception` reaches `PenumbraMain`'s catch, and a start that dies on a native signal inside a vendor
driver, is killed by the system (not responding, memory), or stops on a thread that has no catch, throws nothing the game can show. What the phone keeps of such an end is
the system's record of how the process ended and the game's own log, both unreadable to a player and to us: the log is in the app's private storage (a release APK has no
`run-as`), and nothing reads the record. So the next report needed a way out of the phone that asks nothing of the tester but a second launch.

Measured on an Android 11 (API 30) x86_64 emulator with the released 1.0.6 APK: a native crash (`kill -11`) of the foreground game sends the phone straight to the
launcher with **no** "has stopped" dialog of the system's either (the A54's symptom, exactly), and `dumpsys activity exit-info` then reads `reason=5 (APP CRASH(NATIVE))`,
`status=11`, `importance=100`, `trace=null`. Android 11 keeps no stack for a native crash; the log's last line, which the engine flushes per line, names the stage.

## What changed

Engine (opt-in by the game's manifest meta-data, so a game that sets none is unchanged):

- `PostMortem.java` (new) and `SupersonicActivity.java`: before `super.onCreate` (which starts the native thread, which opens and overwrites the log) the activity reads
  `ActivityManager.getHistoricalProcessExitReasons` (Android 11 and later: nothing happens below) and the end of the game's log. The newest end it has not shown is an
  unexpected one when it was a crash (Java or native), an ANR, an initialisation failure, excessive resource use, the game ending itself with a non-zero status (a start
  that could not finish), or a kill (signal, memory, unknown, other) of a process that was on the screen or close to it (importance 125 or lower). Not reported: a clean
  quit (status 0), the player's swipe from Recents or force-stop, the system's routine trimming of a background process, an app update.
- The dialog (an `AlertDialog` like E39's, closing only with a button): a line of instructions in both languages, the build, how the last run ended, when the log was last
  written relative to that end (a log is replaced only when the game gets as far as opening it, so one that died earlier leaves an earlier run's: the report says when the
  log was written, and when later runs ended normally), the last seven lines of the log, then the phone (model, device, hardware, SoC on Android 12 and later, Android
  version and the vendor's build id). **Share** sends the longer text (RAM, screen, memory at the end, the system's trace when it kept one, 60 log lines, none cut short) to
  any app. OK lets the game start.
- `nativeSetHoldStart` (a static native method of `SupersonicActivity`, `AndroidApp.cpp`): while the report is open `android_main` waits before it enters the game, reading
  its events (an activity that stops reading for five seconds is "not responding"), for at most ten minutes, and waits for a window again after it, so a start that fails
  again cannot take the report off the screen. With no report the only added work is one atomic read.
- An end is marked seen once its dialog is on the screen (`markShown`), not before; one whose dialog never got that far (the start died first) is given up after three
  offers, so a report that itself took the process down cannot come back for ever.
- A scripted run never meets it: with `supersonic.reportSkipWhenFile` set (Penumbra: `penumbra_args.txt`, the development flags `tools/build_android.sh --run` leaves) the
  end is let go as seen and the start is not held.
- Log lines (engine, observation only): every GPU the loader offers (name, Vulkan version, vendor, device, driver), the chosen one's limits (the ones this renderer leans
  on: per-stage descriptors, the fragment stage's combined output resources, push constants, sample counts), the offscreen target before it is created, every graphics
  pipeline before the driver compiles it and how long it took (`VulkanPipeline`, `BloomPass`, ImGui's backend), so the last line of a log from a start that died names what
  the driver was asked to do. ImGui's backend now logs a failing Vulkan result instead of dropping it. `Android::DeviceSummary()` (system properties) for a log line.
- `waitIdle` in `~SupersonicApp` and `VulkanRenderer::ReleaseSurface` no longer throws (a lost device made it `std::terminate` before the game's message about the failure was
  shown); at the end of `Run` it logs and rethrows, as before.

Game:

- `main.cpp`: the first lines of the log are the version and, on Android, the device; the fatal paths go through the log (so the file holds the cause too); a
  `catch (...)` beside `catch (const std::exception&)`. `AndroidMain.cpp` wraps the call to `PenumbraMain` in the same two catches and shows the E39 dialog for what it did not
  catch (not for a scripted run). The manifest names the meta-data, and the log's path (`Penumbra/penumbra.log`), which `test_pn_paths` checks against `main.cpp`.
- `tools/check.bat` defines `PENUMBRA_VERSION`, which `main.cpp` now uses.

## Measured and tested

- Android 11 (API 30) x86_64 emulator, a debug APK built from a clean clone: a normal start shows no report; a native crash and a launch show the report, hold the start
  (the log of the crashed run is untouched while it is open), and OK releases it and the game starts; Share opens the system's chooser with the whole text and Back returns
  to the dialog; a force-stop and a kill of a cached background process (importance 400) show nothing; a launch with development flags after a crash shows nothing and does
  not hold, and the next crash is reported again. `kill -9` of the foreground game wedged the emulator itself (its host-side graphics, not this code), so that case is not
  run.
- Linux (GCC, WSL): `test_pn_all`, 17 suites, 17 passed, 0 failed (`test_pn_paths` 235 checks, five of them the manifest's report meta-data against what `main.cpp` and
  `AndroidMain.cpp` write and read); the build's 21 warnings are all in third-party code (entt, stb). Windows: `tools/check.bat` (MSVC `/W4`) is clean for `game/main.cpp`,
  `tests/test_pn_paths.cpp` and every changed engine C++ file; `test_pn_all.exe` was not built or launched there. Apple builds run only in CI.
- The file that goes to a tester is a repack of that build with another version label (`1.0.6-diag1`, build 8, debug-signed and debuggable, so it cannot be installed over a
  release-signed 1.0.6: the tester uninstalls first). Its arm64 library has exactly the released 1.0.6's 374 undefined symbols and the same needed libraries, so the phone
  loads nothing new; `apksigner verify` passes (v2 and v3); installed on the API 30 emulator and crashed, its report's first line reads `1.0.6-diag1 (build 8)`.
- Reviewed by four independent readers and a skeptic per finding before the build was used (the Java, the native hold, the renderer's edits, the game's); the real findings
  (a log line cut short, a dialog too long for one screen, an end marked seen too early, a scripted run meeting the report, a shared-engine shutdown path that changed
  behaviour, a missing `PENUMBRA_VERSION` for `check.bat`) are fixed.

## Open

- **No real phone has run it.** Whether the A54's drivers, ColorOS 11's exit records, or its dialog behave as the emulator's: unmeasured. On Android 11 the record carries no
  stack for a native crash, so the report names the stage (the log) and the kind of end, not the library.
- It reports the **second** launch: the first one is the one that failed. A phone that cannot get as far as the Java activity (a library that does not load) shows nothing.
- A start that ended itself after the player was already told why (E39's graphics dialog, a full phone) is reported once more at the next start: the log tail then carries the
  cause. Left as it is for the diagnostic.
- This changes the privacy statement (`code-signing.md`): the report is text shown to the player, sent nowhere unless they tap Share.
