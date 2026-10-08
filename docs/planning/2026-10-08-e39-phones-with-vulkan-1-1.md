# E39: phones whose graphics report Vulkan 1.1, and a start that fails on a phone says why

Built 2026-10-07 and 08, in the engine (`engine/`, additive and opt-in) and in the game (`game/main.cpp`, `game/eth/StartupErrors.*`, `game/android/*`). The
player's view is in [`../install.md`](../install.md) ("Android phone or tablet") and the row is in [`../enhancements.md`](../enhancements.md).

## What and why

Two testers, on an Oppo A54 (Android 11) and an Oppo A74 5G (Android 12), wrote the same thing: the game opens, the screen goes black, and the phone
returns to its home screen. No log, no message.

The cause, found by reading the code and checking two public hardware reports first-hand:

- The engine rejected any GPU that reports a Vulkan version below 1.2 (`VulkanDevice::isDeviceSuitable`, `kRequiredApiVersion`).
- The Vulkan hardware database (vulkan.gpuinfo.org) has a report for each model: report 36238, OPPO CPH2239 (the A54, PowerVR GE8320), Android 11,
  **Vulkan 1.1.131**; report 18569, OPPO CPH2197 (the A74 5G, Adreno 619), Android 12, **Vulkan 1.1.128**. The stock Android 11 and 12 loader is itself capped
  at 1.1. Across the database's Android reports 1.2 or newer is about 5% of the Android 11 ones and 15% of the Android 12 ones (the sample skews towards
  enthusiasts; the real share is lower), which is why the install guide's "most phones from about 2020 on" was wrong.
- So no GPU was acceptable, `pickPhysicalDevice` threw, `main.cpp` caught it and returned, `ShowStartupDialog` was an empty function off Windows, and
  `android_main` finished the activity: a black window, then the launcher, with nothing said. The author's own phone (a Pixel 9 Pro XL, Vulkan 1.4) and the
  emulator's software GPU both pass the gate, so it was never seen.
- The manifest's `uses-feature android.hardware.vulkan.version 0x402000` did not stop the install: Android's own documentation calls uses-feature informational;
  only Google Play filters on it. (And 0x402000 is not a value Android defines.)

Not known: that these two phones' own drivers report what the database shows for other units of the same models and Android versions (no log came from them),
and what happens after the gate on a PowerVR or Adreno driver: nothing has run on either.

## What changed

Engine (additive: nothing changes for a game that does not ask):

- `GameManifest::minimumVulkanMinor` (default 2, so Vulkan 1.2 as ever; clamped into 1 to 2 by `GameRuntime::ResolveVulkanMinor`; never in game.manifest's text).
  `VulkanDevice` takes it and uses it in `isDeviceSuitable`. The memory allocator (VMA) is given the version the chosen GPU really reports, capped at 1.2
  (`GameRuntime::AllocatorVulkanMinor`): the same 1.2 as before on a 1.2 or newer GPU, 1.1 on a 1.1 one. Nothing in the engine, the shaders (SPIR-V 1.0) or
  VMA's 1.1 entry points needs more than Vulkan 1.1 core (read across the engine and the game; no `*2` queries, no `renderPass2`, no timeline semaphores, no
  1.2-only layouts or enums).
- The thrown message says why each GPU was passed over: `Failed to find a suitable Vulkan physical GPU! Adreno (TM) 619: reports Vulkan 1.0, the game needs 1.1
  or newer.` One new log line, `The GPU reports Vulkan 1.1.131 (driver ...); the allocator is told 1.1.`, so the first log from a phone shows it. A throwing
  `VulkanDevice` constructor now releases its surface (it leaked it, and an instance was destroyed with a live child). The instance's error names the `VkResult`.
- Android: `Supersonic::Android::ShowMessage` and `SupersonicActivity.showMessage` / `isMessageOpen`: an `AlertDialog` on the UI thread that closes only with
  its OK button (a tap outside it or Back does not: they are what a player does to a black screen), whose text can be selected and whose address can be tapped,
  while the engine's thread keeps reading its looper (an activity that stops reading for five seconds is "not responding") for at most five minutes.

Game:

- `main.cpp`: `manifest.minimumVulkanMinor` is 1 on a phone and 2 on a computer (nothing has run a 1.1 GPU there, and the computer guides say 1.2).
  `--vulkan 1.1|1.2` sets it for one run (a developer flag: it removes the intro).
- `eth/StartupErrors`: on Android `CurrentErrorStream()` is "nowhere" (stderr is logcat, which no player reads) and `ShowStartupDialog` calls the dialog.
  `StartupMessage(..., phone)` words the message for a phone, in English and Portuguese: the graphics text is hedged on purpose (it is shown for any failure
  while the graphics start, and only the "Details" line knows whether the phone reports Vulkan 1.0 or something else went wrong), it asks for the phone's model
  and a screenshot, and the details come right after the first language, on the dialog's first screen. A phone's log is in private storage no player can open, so
  none is offered.
- `AndroidMain.cpp`: the same dialog when the first-run unpack fails (not for a scripted run: a file of development flags is left for those); the device's
  language is read before the unpack, so that dialog is in it; after the unpack it waits for the window if the player pressed Home during it (the engine would
  otherwise throw "no window to draw on" and blame the graphics).
- The manifest asks for Vulkan 1.1 (`0x401000`, which Android defines).

## Measured and tested

- The failure path on a real Android runtime (the project's x86_64 API 33 emulator): with the engine's shaders deleted from the app's private storage the
  start throws inside the engine; the catch hands the message to the activity; the dialog appears with the details on its first screen; a tap outside it and
  Back leave it up (the screen is byte-identical afterwards); OK ends the game ("The game returned 1; finishing the activity.") and the launcher is shown.
  A normal start on the same build reaches its first frame in under 5 s.
- The Vulkan 1.1 path on Linux, under a test-only Vulkan layer that caps the version a GPU reports (Mesa's software driver ignores
  `MESA_VK_VERSION_OVERRIDE`; the layer is `vkcap.c`, kept out of the repository): capped at 1.1 with `--vulkan 1.1` the game is accepted ("The GPU reports
  Vulkan 1.1.0 ...; the allocator is told 1.1.") and its capture is **byte-identical** to the uncapped one; capped at 1.1 on a computer's default it is refused with
  "llvmpipe ...: reports Vulkan 1.1, the game needs 1.2 or newer."; capped at 1.0 with `--vulkan 1.1` it is refused with "reports Vulkan 1.0, the game needs 1.1 or
  newer."; capped at 1.2 it is as ever. The layer does not remove any entry point from the driver, so it shows that the game's own decisions are right, not that a
  1.1 driver has every function the game calls.
- Linux (GCC): `test_pn_all`, 17 suites, 48,038 checks, 0 failures (`test_pn_paths` 230, with the new phone-message and record checks); the engine's
  `test_gameruntime` 308 checks, 0 failures (the minimum's default and clamp, the allocator's minor). Windows: built with zero warnings (MSVC /W4); the first build of
  this change ran `test_pn_all` (its only failures were two stale expectations of the new test, since fixed, and the 18 known E25 glyph-fit ones); the final build's
  `test_pn_all.exe` and the engine suite's exe were refused by Smart App Control (exit 126) and are **not run** on Windows.
- Reviewed by three readers before the last fixes (the JNI and Java, the Vulkan change, the player-facing text). What they found and was changed: the dialog could be
  dismissed by a tap outside it; the phone graphics text blamed the Vulkan version for any failure while the graphics start; "você" was spelled with the wrong accent;
  the minimum was lowered on computers too, ahead of the evidence and the guides; the app suggestion in the guide named a label that does not exist; the first-run
  dialog ignored scripted runs and the language of the phone; a JNI lookup with an exception pending (CheckJNI aborts on it); the details were at the bottom of two
  languages; the surface leak.

## Open

- **No real phone has run it.** The two testers' drivers and what happens after the gate on a PowerVR GE8320 or an Adreno 619 (the first pipeline compile on those
  drivers, the allocator on a 1.1 device, 4x multisampling, the shadow maps' formats) are unmeasured; the dialog will say what happens if it fails there. Where a
  limit is tight it is recorded: the scene's fragment stage uses 7 storage buffers and one colour output, 8 of the GE8320's 8 combined resources.
- **The release.** Released as 1.0.6 (build 7) on 2026-10-08 with E38: the guides' Android lines now say Vulkan 1.1, the version is bumped everywhere `tools/make_release.py check` looks, and a build
  signed with the release key is the one phones update to (same package name and key as 1.0.5). Versions up to 1.0.5 stay published and still close on a 1.1 phone.
- A computer still needs Vulkan 1.2; `--vulkan 1.1` lets one try a 1.1 GPU. Nothing has run a 1.1 GPU on a computer.
- Android-only code that no suite reaches (the JNI call, the Java dialog) is checked by hand on the emulator, as above; the Windows engine suite ran on Linux.
