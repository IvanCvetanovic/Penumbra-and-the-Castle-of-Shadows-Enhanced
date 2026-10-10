# E45: a save that cannot end the run (and what the checks of 2026-10-10 found out)

Built 2026-10-10, in the game (`game/eth/AtomicWrite.hpp`, `Machine::SaveScene`, `Machine::DoLoad`, `Enml.cpp`), with scenarios 29 to 31 of `test_pn_scenarios`.

## The defect

The game saves a checkpoint (`scenes/checkpoint.esc`, in the user's folder) when the wizard touches one, and loads it at his next death. The save opened the file truncated and wrote into it, with no check that the write went
through, and the scripts set `hasCheckpoint` whether it did or not (`main.as:183`, ported as it was). A write that is cut short - the app killed in the middle of it, which a phone does to a game it needs the memory of, or a storage with no
room left - left a file that could not be read, and had already replaced the last good one. The next death then asked for it: `ReadSceneFile` failed ("malformed XML ... Couldn't load the scene scenes/checkpoint.esc") and 0.7.12's rule for
a scene it cannot read (`ETHEngine.cpp:835-839`, ported as it was) is to leave the new scene empty and the old loop running. The wizard was gone with nothing to play or to leave: an empty screen. Reproduced by scenario 31 in the code of
release 1.0.8, which wrote its checkpoint this way as every release since 1.0.0 did: 0 entities, no wizard, a life spent. How often it met a player is not known; it needs a save interrupted, and low storage on a cheap phone makes that likelier.

The list of best times (`hs.enml`) was written the same way. A cut-short one reads as never played, which the code handles (59:59) but which loses the times, and with them the arenas they unlock.

## What changed

- **Atomic writes** (`WriteFileAtomic`): the bytes go to `<file>.tmp` and the temporary is renamed over the file, so a save that fails (the temporary cannot be written, the disk is full, the rename is refused) leaves the old file as it was and no
  temporary behind; a reader sees the whole of either file. Used by `Machine::SaveScene` (the checkpoint) and the Enml save (best times). The settings file was already written this way. The unpack of the APK's files on Android ends by writing a stamp,
  so a cut-short unpack is redone at the next start, as it was.
- **A scene that cannot be read starts the one that was running again** (`Machine::DoLoad`), with the request's own scripts: for a damaged or missing checkpoint that is what a death with no checkpoint does, the level from its start, with
  the life already spent. Only when the request goes on with the loop that is running (a checkpoint's `levelLoop` after the level's, so a load that changes what is played stays as it was) and the running scene is another file, so one that cannot be read twice stops as it always did. The original's empty scene under the old loop had no use for the player.
- Nothing else is touched: a save that works writes the same bytes to the same place.

## Measured and tested

- Scenario 31: a checkpoint is taken; a save that cannot be written (a directory where the temporary goes) and one that cannot replace the file (a folder with something in it stands there) answer false, leave the file byte for byte as it was and no
  temporary beside it; a save that works leaves no temporary; then, three times, in a fresh level 1: the checkpoint is taken, the file is cut to half its size, emptied, or replaced by text that is not a scene, and the wizard dies: with the old code (half size) 0 entities and no
  wizard, with the new one a playable wizard 245 frames after (3 s of fade and the load) at the level's start, 628 entities, one life spent, no checkpoint, each time. Linux, 17 of 17 suites, 48,210 checks;
  `tools/check.bat` compiles the changed files with /W4 clean. Not run: the Windows and Android builds (CI), and a real interrupted write.
- **Not covered:** a power cut after the rename and before the storage has committed it (no `fsync`: what the settings file never had either), and a checkpoint that reads but is wrong.

## The other checks of the day (no real phone, no real keyboard in any of them)

- **Combo assist, from the keys to the recorder** (scenario 29, which makes `test_pn_scenarios` link the whole game): each tick's keys through a real `InputMapper` into the game. With the assist off, the second RIGHT with S, DOWN with RIGHT, and all
  three of the spell's keys pressed together make no combo (a plain fireball); with it on each makes it (5 and 25 mana); three clean taps make it both ways. The Windows report ("keys pressed too fast or at once do not register") is the
  original's one command per tick, and the assist fixes it in the game's own machine. See [E44](2026-10-09-e44-combo-assist.md).
- **A soak** (scenario 30): 30,000 ticks each of seeded random keys, pad buttons, sticks, pause and confirm, leaning right, in level 1, level 1 with both characters at level 15, level 2, level 3, level 3 at level 15, and Versus arenas 1 and 4: 210,000
  ticks, 0 script aborts, no wizard that is not a number, entity counts flat (level 1 about 640, level 2 about 460, level 3 about 890, an arena about 110). The game's own logic costs 0.05 to 0.16 ms a tick on average here (no drawing), so the CPU
  of a slow phone is not what the logic of the game asks for; a scene load costs about 35 ms to read and fill and, in this setup (the files on a Windows drive seen from WSL), about 150 ms in all. What a phone's drawing costs is not measured by any of this.
  The soak found nothing but what scenario 31 then pinned down, which it did not reach by itself (random play seldom takes a checkpoint and dies).
- **Memory of the phone settings** (E43): the peak resident memory of level 1 at 2400x1080 on a software Vulkan driver with dynamic resolution off, three runs each: 542 MiB with 1.0.7's settings (default shadow maps, 4 samples), 422 with 16x16 shadow maps
  only, 433 with 1 sample only, 310 with both (1.0.8's phone settings): 232 MiB less, repeatable to 1 MiB. A software driver keeps its pictures in the process, so this is what the settings cost there; a phone's GPU drivers account their memory
  differently, so the figure says the saving is real and not what a given phone sees. (A first try that sampled the process every half second missed the peak and gave numbers 100 MiB apart for one setting; it was thrown away.)
- **The update to build 10** (Android 13 emulator): 1.0.7 (versionCode 8) was installed and set to Hard and German through its own Settings screen, then 1.0.8's first build (9) and the rebuilt one (10) were installed over it with `adb install -r`, each
  accepted (same release key); the language and the difficulty were kept, build 10's New Game question opened in German with Hard lit, Android's Back button closed it with the game still running, and a Hard game started level 1 with the touch controls and
  nothing fatal in the log. Not tried: a phone maker's own installer.

## Open

- Whether this is what any tester met is not known. The "stuck, as if the touch screen stopped" report of an Oppo A74 and the reports of a lag on an Oppo A54 still need a test build's panel from the phones themselves.
- `SaveStringToFile` (the API of the Eth layer, not called by any script) still writes in place.
