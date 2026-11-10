# E49: a first start that was stopped goes on where it stopped

Built 2026-11-10, in the game only (`game/eth/AssetUnpack.hpp`, new; `game/android/AndroidMain.cpp`, `game/main.cpp`,
`tests/test_pn_paths.cpp`, the two install guides). No engine change, so no engine pin and no heads-up to the other project that
shares the engine.

## What was reported

A tester's Samsung Galaxy Tab A7 Lite (SM-T225N, MediaTek MT8768WT, Android 14, 1.0.11 build 13) showed the game's own report
window ("Penumbra closed unexpectedly last time", E40) and, by the tester's account, the game opened only on the fifth try. The
window said:

- `Last run ended: other (13), status 0, in the foreground, 2026-11-08 12:52:00 (19 s ago)`
- `Description: stop com.ivancvetanovic.penumbra due to SPEG`
- `No game log: the game stopped before it opened one.`

## What that record says, and what it does not

- Reason 13 is `REASON_OTHER` with a description of the form "stop <package> due to <tag>": the system's ActivityManager stopping
  the package on behalf of a component that names itself in the tag. Status 0 and "in the foreground" are what that kind of stop
  records. It is not a crash of the game (that would be reason 4 or 5), not a memory kill (3) and not a not-responding (6).
- "No game log" means the process was stopped before `PenumbraMain` opened `penumbra.log` (`game/main.cpp`), which is after the
  unpack of the game's files: so during the process start, the report window's hold, the unpack, or the wait for a window.
- **What SPEG is, is not established.** Two searches for the tag found nothing. The one precedent found is a 2022 issue of another
  app (Neo-Backup, issue 569, a rooted Galaxy Tab S6 Lite, Android 12), read verbatim for 17 lines out of the page (one stretch of it
  could not be read): a Samsung component launches a just-installed app on a `SpegVirtualDisplay`, logs `Force stopping <app>
  appid=10280 user=0: SPEG` and `Killing ...: stop <app> due to SPEG` 2.1 s after the process's `Start proc`, 0.26 s after Google Play's
  install-time verification completed, and `dex2oat` then runs with `--compilation-reason=install-speg-dm`. Nobody in that thread knew
  what SPEG stands for. The reading that fits (an inference, from one log on another device and another Android): Samsung starts a
  newly installed app for about two seconds on a virtual display, without anyone looking, to collect what it needs to compile it, and
  then stops it. If that is what happened here, the game was started and stopped by the system without the tester's doing, and
  possibly again on later starts.
- **The screenshot is not necessarily a failed try.** The report window is built at the start AFTER the one it describes, and the
  game goes on once OK is tapped. It may be the first real open after such a stop, or the fifth, successful, one. Which of the first
  four tries ended how is not known.

## What was wrong in our code (confirmed; whatever SPEG is)

1. **The first-run unpack was all or nothing** (`AndroidMain.cpp`): it deleted the stamp and the three folders it owns, copied
   all 624 files (21.3 MB, 66 folders; 439 of them the original's, 141 the port's data, 44 the engine's), and wrote the stamp last.
   A process stopped anywhere in between left nothing the next start could use, and the next start deleted the partial tree and
   copied it all again, so a phone whose system kept stopping the first starts early would repeat the same work every time and
   could be stopped at the same point every time. A retry cost more than the first try.
2. **The game's log opened after the unpack**, so a start stopped during it left no log, and the report could only say "No game
   log" instead of how far it had got.
3. **Cancelling was an error.** If the player left while the files were being unpacked (the app's task removed), the unpack
   returned false, the "game files missing" dialog was shown on an activity that was going, and the game ended with a failure status,
   which the next start's report called an unexpected end.

## What changed

- **`game/eth/AssetUnpack.hpp`** (new, portable, header only; the APK is behind four hooks, so a computer's test suite runs all of
  it). A marker file, `penumbra_assets.partial`, holds the stamp of the unpack in progress.
  - A start whose APK stamp equals the stamp file does nothing, as before (the one extra cost on every later start is the same single
    file read it always was).
  - A start that finds the marker equal to the APK's stamp **continues that unpack**: each file already on the disk whose size equals
    the asset's length is kept, the rest are copied. Each short life adds to the last.
  - Anything else (no marker, another build's stamp, a wipe that was cut short) is a **clean start** as before: the stamp and the marker
    are removed first, then the three folders, then the marker is written. A tree is only ever trusted if it is of the very stamp (a hash
    of every packed path and byte), so a file whose size alone matches across two builds is never kept, and nothing of an older build
    stays behind.
  - A file is written under its own name, with no temporary and no rename: a copy that was stopped is shorter than its asset, so the
    size check copies it again, and one that was not stopped is whole. Nothing is synced (a stopped process loses none of what it
    wrote; a sync per file would cost seconds on a phone's flash). The stamp is written last, atomically (`WriteFileAtomic`, E45), and
    then the marker is removed, in that order, so a stamp that cannot be written leaves everything in place for a one-pass finish.
  - The event loop is asked before every file (before: every 16th), and between the folders of a wipe. A "no" stops the unpack where it
    is and keeps what it has (`Cancelled`).
- **`AndroidMain.cpp`** wires the hooks to the APK (`AAsset_getLength64`, the old `copyAsset`, `PumpEvents`), opens the game's log
  itself the first time the unpack has anything to say (the same `files/Penumbra/penumbra.log` the report reads; a start with the files in
  place says nothing here and leaves the log to `main.cpp`), and treats a cancel as a quiet end (`EXIT_SUCCESS`, no dialog, nothing
  for the next start's report). The "game files missing" dialog stays for a real failure (no room, an unreadable asset).
- **`game/main.cpp`** writes the unpack's one-line summary among the first lines of the log it then opens ("First start: unpacked 624
  game files, 320 copied, 304 kept from a start that was cut short, in 0.14 s."), because opening that log replaces the unpack's lines.
- **`docs/install.md`, `docs/install.pt.md`**: one bullet under "It didn't work?" for a game that closes by itself a few times
  and then works, and for a report window that says "stop ... due to SPEG": it is the phone's system, not a crash; an interrupted first
  start keeps what it has done; versions up to 1.0.11 began again every time.

## Measured and tested

- **Linux (GCC, WSL), `test_pn_all`, one run: 17 suites.** `test_pn_paths` went from 235 to 339 checks (0 failures): the stop loop
  stops an unpack after every possible number of the event loop's questions (inside the wipe, between any two files, after the last)
  and restarts it; a copy that fails halfway; a file left under its name by a killed process, 5 bytes, 0 bytes, one short, one long, one
  extra byte; another build's stamp over a partial tree and over a finished one (same lengths, other bytes; a file the new build lacks;
  a file it adds); a new build stopped inside its wipe, then restarted, and then the old build restarted; a stamp that cannot be
  written; a missing asset; an empty stamp; the marker's and stamp's order.
- **Mutation check of those tests** (13 deliberate breakages of the unit, one at a time, `test_pn_paths` rebuilt and run for each):
  marker written before the wipe, no wipe, only the first folder wiped, the old stamp kept through the wipe or removed after it, the
  marker removed before the stamp, a file one byte short / one byte long / empty / half-length kept, a half file kept after a failed
  copy, resume without the stamp comparison: all 13 caught. The first version of the tests let most of these through (an
  independent reviewer, who ran 48 such breakages, found five gaps); the cases above are what closes them.
- **A ptrace harness** (an independent reviewer's, scratch only) killed the real unit with SIGKILL at each state-changing system call
  (before it, after it, and in the middle of a write), for builds A and B of the same file sizes and different bytes, under libstdc++ and
  libc++, in several thousand runs and 450 random histories (kills, cancels, finished starts, a 1.0.11-style tree): every next start
  ended in exactly the clean tree.
- **Android 13 (API 33) x86_64 emulator, the debug APK from this tree (the stamp is the same as 1.0.11's, `7f2460f4e2f29ed9`, so an
  installed game keeps its fast path):** a clean first start unpacks 624 files in 0.50 s and reaches the menu (a screenshot); a second start
  does nothing; a start killed once the Nth file existed (57, 254, 305, 438 and 566 files) leaves a log ending in the unpack's progress
  lines (never "No game log") and the next start finishes with copied + kept = 624 and a tree of 624 files whose sizes all equal the
  APK's (a partly written file is recopied: one more copied than the files kept); the report window after a qualifying end shows the
  unpack's lines under "End of the game's log"; a marker of another stamp with a truncated file and a junk file gives a clean start and
  the junk is gone; a marker of the real stamp with a file truncated and a file deleted gives "continuing ... 2 copied, 622 kept"; the same
  APK installed over itself keeps the fast path. After a last edit of the unit's file reader the checks were repeated on the final three-ABI debug APK (the x86_64 library ran): a clean start in 0.44 s to the menu, the fast path, a crash with 412 files in place then the report window holding the start with the unpack's progress lines in it and, after OK, 213 copied and 411 kept, a marker of another stamp (clean start, junk gone), a marker of the real stamp (2 copied, 622 kept) and the install over itself: all passed. The emulator itself died three times in the session (host graphics, its log ends mid-line in the Vulkan instance's creation); the tree was whole after the death that came between runs.
- **Compile:** MSVC `/W4` (`tools/check.bat` on `game/main.cpp` and `tests/test_pn_paths.cpp`) clean; the NDK's clang builds the x86_64
  APK with no warning in the changed files, and an independent `-fsyntax-only` with `-Wall -Wextra -Wpedantic` for arm64-v8a and
  armeabi-v7a is clean for `AndroidMain.cpp` and `main.cpp`.
- **Review:** four independent readers (the state machine, the Android integration, the tests by mutation, robustness and the player's
  view) and two skeptics per finding; no defect in the unit or the integration; six findings, five of them gaps in the tests (all
  closed), one a power-loss caveat (below).

## Not done / open

- **Nothing was run on the tablet, or on any Samsung phone.** Whether SPEG still stops the first starts of this build, how long the unpack
  takes on that tablet (0.5 s on the emulator, which says little about a 3 GB phone's flash), and whether the fix makes the game open
  at the first try there are unknown. What the change guarantees is narrower: a first start that is stopped at any moment, however many
  times, makes progress each time instead of none, and the report of a stopped start now says how far the unpack got.
- **A cancel inside the unpack was not run on a device.** Back is mapped to the game's own input and does not end the activity, and the
  unpack takes about a second, so the only ends that reach the unpack are the task being removed or the process being stopped: the
  emulator runs of those ended after it had finished, or before it began. The quiet-cancel path is exercised by the host tests (a "no"
  from the event loop at every point) and by reading the end of `android_main`.
- **No sync, as before.** A stamp is written atomically but not synced; on a power loss the stamp can come back empty (seen once, when
  the emulator itself died: the next start did a clean unpack and recovered). A valid stamp over a file that lost its bytes would not be
  repaired (the fast path trusts the stamp), as it was not before. A resume does not know which boot wrote the files; a boot id in the
  marker would close that and costs a hook; not done.
- **The report window still says "closed unexpectedly" for a stop by the system.** Wording it as the phone's own stop (reason 13 with a
  "stop ... due to" description) is a change in the engine's `PostMortem.java` and `ReportText.java`; not done here (the install guide
  explains it).
- **Still a black screen for the whole first unpack.** A progress view would be Java-side and an engine change (drawing on the native window
  from here would collide with the Vulkan swapchain made on it later); the unpack is the part a slower phone makes long, so measuring it there
  comes first. Other ways to shorten it, if the tablet's number asks for them: copying on more than one thread, or not copying what the
  game could read straight from the APK.
- **Docs say "versions up to 1.0.11 began again every time"**; 1.0.12 (published 2026-11-10 as Latest, tag v1.0.12 = 8a75fe2) is the
  first release that carries this, so the sentence holds. The install guides' wording was softened after the tag (the unpack's duration
  on a slower phone is unmeasured, and "should get it through" became "may"); see DEVLOG's entry for 1.0.12.
- **Questions for the tester,** to settle the rest: a screenshot (and the Share text) of every window it showed, and what the screen did
  after each try (black, back to the home screen, closed by itself) and for about how long; whether the first one came right after
  installing; whether it now opens at once on every later start and after a restart of the tablet; the free storage; whether battery
  saver or "put unused apps to sleep" is on; and, from a computer with `adb`, `adb shell dumpsys activity exit-info
  com.ivancvetanovic.penumbra` and `adb logcat -b all -d` filtered for `SPEG`.
