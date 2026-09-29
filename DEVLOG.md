# DEVLOG — Penumbra on Supersonic

Append-only. One entry per session: what was built, what broke, dead ends, decisions, numbers.

---

## 2026-09-27 — session 1: understand, scaffold, contract

**Built.** Mapped the original with an 8-reader workflow plus a critic (docs/spec, ~92k words).
Found the exact engine build the game shipped on — Ethanon 0.7.12, SourceForge SVN tag v0-7-12 —
and GS2D r485 as a stand-in for the closed GameSpaceLib; both in gitignored reference/. Ran the
original from a scratch copy (it works on Windows 11 with a HIGHDPIAWARE compat flag) and captured
the menu. Scaffolded the repo on the Magic Portals pattern and wrote the Eth contract headers.

**Decisions.** Port the AngelScript to C++ over an emulated 0.7.12 runtime (not embed AngelScript,
not re-derive gameplay from data). One Ethanon frame per 60 Hz tick. Snapshot taken at 0.7.12's
render point, before callbacks. Ivan: enhanced from the start, PT+EN, assets in place.

**Dead ends.** The original's menu ignores Enter unless the (hidden) cursor entity is over a button:
it is mouse-driven through CollideDynamic. First capture was offset by DPI virtualisation.

---

## 2026-09-27 — session 1 (continued): the game runs, whole

**Built.** eth runtime + 25 scripts (9c64f8a); render/ + layer (abbc062); engine Ogg Vorbis (abb9e8a)
and window control (a516b7c), both additive and announced to the Magic Portals session; English
image variants; live shadows to the light's reach; ten headless gameplay scenarios.

**Numbers.** 13 suites, 2593 checks, 0 failures. Level 1 at 1024x768 against the original's capture:
sampled pixels within 2 levels. Scenarios: 458 checks, 0 script aborts, no port bug.

**Broke / dead ends.** Smart App Control refused freshly linked executables (Penumbra.exe twice in a
row) until every exe carried an icon, VERSIONINFO and a manifest; with them, none was refused again.
The first lighting comparison looked "too dark" - it was the widescreen framing and a different
moment, not the lighting: a pixel-aligned 1024x768 capture matched. A menu capture depends on the
live mouse (cursor.ent follows it) - hence --cursor.

**Decisions.** E7 (lv20 / lv31+), E9 (shadow length), R4 exception for generated English art.

---

## 2026-09-27 — session 1 (night): standing statues, highlights, options, a package

**Built.** Engine f30df7c + b999491 (vertical 2D sprites, gloss highlights; opt-in, bit-identical
for everything else; announced and green-lit by the Magic Portals session before the push). The
original's Settings screen gained the enhanced rows (E10). Interpolation between ticks (E8). A
runtime path resolver and tools/package.bat; README.md; LICENSE.md + the LGPL/GPL texts;
Penumbra.exe without a console, logging to %APPDATA%\Penumbra\penumbra.log.

**Numbers.** Menu against the original, per-region mean error: left pedestal 24.4 -> 12.9, lower-left
barrel 27.9 -> 3.9. Level 1 at 1024x768 byte-identical through the round. 13 of 15 suites run,
2345 checks, 0 failures.

**Broke.** Smart App Control keeps refusing test_pn_boot and test_pn_formats specifically (three
links each), and test_materials in the engine's own build - whatever it keys on, relinking does not
move them. Recorded as not run; their ground is covered by test_pn_scenarios.

---

## 2026-09-28 — session 2: a pause, pad menus, six more scenarios, one test executable

**Built.** E13, a pause (render/PauseMenu): Esc or player 1's Back in a level or an arena freezes
the Machine under Resume / Main menu, the music at 40%, and opens by itself on a focus loss; Main
menu feeds the original's own Esc for one tick. E14: a pad's A and B confirm and cancel in the
menu-like screens. The options screen gained a smooth-motion row (E8, E10). test_pn_scenarios gained
scenarios 15-20. tests/all builds every suite into test_pn_all, which runs each in a child process
of itself.

**Numbers.** test_pn_all: 16 suites, 3716 checks, 0 failures (render_pause 312, scenarios 916).
Paused capture run: every pixel outside the pause panel identical from frame 130 to 180 (paused
from tick 120 to 190).

**Broke.** The full build's Penumbra.exe was refused by Smart App Control (exit 126); one relink
with a real change (--help lists --tour) was accepted. test_pn_all was accepted on its first launch,
and with it test_pn_boot and test_pn_formats ran for the first time since Step 5.

**Decisions.** Original bugs found by the new scenarios (the `play_sound.ent` horror markers that
never play, the off-screen second summon, the 50-mana refusal) are pinned, not fixed, pending Ivan.

**Session 2, later.** E15 (the misnamed horror markers play). The round's gates: test_pn_all once,
16 suites, 3719 checks, 0 failures. Mistakes, owned: a capture loop launched a freshly relinked
Penumbra.exe eight times after Smart App Control had refused it (eight notifications), and later a
build-then-run chain relaunched a refused test_pn_all because the edit meant to relink it had not
applied. Rule written down (memory + CLAUDE.md rule 5 practice): launch a new binary once, alone,
and only after the build log shows it was relinked.

**Session 2, the pause live.** Driven with real input events: Esc in level 1 opened the pause,
froze the game (two frames 1.5 s apart byte-identical), and Main menu reached the main menu. Two
anomalies came from the test, not the game: the window moved under a resting mouse, and the Magic
Portals session's game windows, opening on the same desktop, took the focus and some of the rig's
input. The pointer now selects a pause row only when it moves 2 logical pixels or more in a tick
(against a pixel of rounding wobble; the rig's window move is larger and is a test artifact, and a
pointer creeping slower than that does not highlight), and every pause transition is logged to
penumbra.log. Live desktop tests stopped while another session drives
windows here; E13/E14 by hand (Ivan) is the remaining check. test_pn_all once: 16 suites, 3719
checks, 0 failures.

---

## 2026-09-28 — session 3: macOS and iOS (branch apple-port)

**Built.** The port on Apple platforms, compiled and run only on GitHub's macOS runners
(.github/workflows/apple.yml: apple-port pushes and manual runs, never main, 40-minute jobs).
Engine (its apple-port branch): a CoreAudio backend, a UIKit backend in src/platform/ios on the
native-surface seam Android made, MoltenVK linked into the program, GLFW fixes for macOS. Game:
macos/MacMain.mm and ios/IOSMain.mm (the language, the bundle's files, the engine's writable
folder, iOS dev flags), tools/apple/make_app.sh for Penumbra.app, extracted/ CRLF on every
checkout. Planning Step 15 has the whole of it.

**Numbers.** macOS (macos-15 arm64, Xcode 26.3, MoltenVK 1.4.2): every target builds; test_pn_all
17 suites, 4914 checks, 0 failures; Penumbra.app renders level 1 on the runner's Apple
Paravirtual GPU; CoreAudio pulled 384000 frames in 750 callbacks. iOS: simulator and device
builds link; in the simulator the engine initialises and the game loads level 1, CoreAudio's
RemoteIO pulls, then the first draw fails (no base-instance drawing on the simulator's GPU
family Apple 2). CI: 4 runs to this point, 19.4 minutes of macOS runner time.

**Broke, and why.** GLFW's .m files compiled as Objective-C++ (OBJCXX enabled without OBJC). The
simulator refused array views of attachment images (no layered rendering) and its Metal service
died under MoltenVK's Tier 1 argument-buffer path writing ImGui's sampler descriptor; both fixed
in the engine, Apple-only. test_pn_formats failed its round trips on the Mac because a non-Windows
checkout of extracted/ is LF.

**Decisions.** MoltenVK statically linked on both platforms (no loader to find, nothing to
rewrite in a bundle; no validation layers). A bundle is never written: the engine runs from
~/Library/Caches (Library/Caches on iOS). The game's view on a notched phone is the safe area's
width (orchestrator's ruling). The simulator's base-instance limit is left to the renderer.

---

## 2026-09-28 — session 3: every platform, touch controls, the open polish

**Asked.** Ivan: do the open polish; make the game play on Android, iOS, Windows, Linux and macOS
"if possible"; controls for mobile, the buttons from Magic Rampage (not on this laptop: placeholder
art until he gives a path); Apple verified on GitHub Actions (his choice; a read-only deploy key
on the engine for the CI, approved).

**Built.** The pause's focus-loss row. Linux (WSL): one path resolver, stand-in fonts (E17),
dr_mp3 (E18), the POSIX locale (E19), posix_spawn suites. E16 touch controls, then combo buttons.
The engine on Android (android_main, the ANativeWindow surface, touch, pads, AAudio, an immersive
Java activity, SafeArea) and the APK without Gradle; E20, the options screen on a phone. The
rendering fidelity items (the baked highlight eye, the light pass's alpha test, standing sprites'
rows). macOS and iOS (above). Planning steps 11-18.

**Numbers.** Windows, the final tree: build zero warnings; test_pn_all once, 17 suites, 5685
checks, 0 failures. Linux 5596, macOS (CI) 5005 at the Apple merge. Engine suites 57 of 57 on the
desktop path (Linux) at every engine push. Android: both ABIs, an 18 MB APK; on the emulator the
menu, taps, level 1 with the controls, Back, Home and resume, the music, the device's language.

**Broke.** A docs script of mine dropped the planning doc's "Open" heading (restored a commit
later). Ubuntu's glslc, installed in WSL by the polish work, rewrote every committed .spv through
the engine's build; caught before the push, removed, and build_linux.sh now refuses a configured
shader compiler. Smart App Control refused test_pn_all once (exit 126, one notification), at the
polish build; the next relink ran.

**Not verified.** A real phone (Android arm64, any iPhone), a frame in the iOS simulator
(base-instance drawing), sound by ear on any new platform, two-finger touch on a device, a person
playing on each.

**Session 3, last.** The packaged Windows game (tools/package.bat, the new fonts and touch art
in its data) launched once, headless and without input, with the Magic Portals session's
go-ahead: level 1 at frame 300 on the laptop's Radeon, lights, torches, HUD and English text
drawn, no touch controls (off on the desktop by default), exit 0.

## 2026-09-29 — session 3, the end: small fixes, the pause with real input

**Built.** Magic Rampage's buttons for E16 (from Ivan's XAPK), then the small fixes: touch-worded
hints, the knob only while held, one back button on Options, and the shadows of baked lights that
remove only their own light (engine 6cb4036, which also sizes the descriptor pool from the layout).

**Numbers.** Windows test_pn_all 17 suites, 8024 checks, 0 failures; Linux 7935; engine 57/57; the
APK starts and plays on the emulator. The pause with real OS input on Windows (planning Step 22):
Esc opens and closes it, a focus loss opens it, the refocus click does not choose, a click resumes.

**Broke.** A descriptor pool one storage buffer short (a literal 7 against the new binding 13):
SwiftShader refused it at startup, desktop drivers did not notice; caught on the emulator before
the push, now sized from the layout and guarded. Smart App Control refused one freshly linked
Penumbra.exe (one notification). The live test's first arrow keys were sent without scan codes.

