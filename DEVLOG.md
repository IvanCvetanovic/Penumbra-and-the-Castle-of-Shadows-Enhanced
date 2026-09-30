# DEVLOG — Penumbra on Supersonic

Append-only. One entry per stretch of work: what was built, what broke, dead ends, decisions,
numbers. (On 2026-09-30 the earlier entries were reworded in place for the public repository:
conversational phrasing removed, every fact kept. See the last entry.)

---

## 2026-09-27 — understand, scaffold, contract

**Built.** Mapped the original with an 8-reader workflow plus a critic (docs/spec, ~92k words).
Found the exact engine build the game shipped on — Ethanon 0.7.12, SourceForge SVN tag v0-7-12 —
and GS2D r485 as a stand-in for the closed GameSpaceLib; both in gitignored reference/. Ran the
original from a scratch copy (it works on Windows 11 with a HIGHDPIAWARE compat flag) and captured
the menu. Scaffolded the repo on the Magic Portals pattern and wrote the Eth contract headers.

**Decisions.** Port the AngelScript to C++ over an emulated 0.7.12 runtime (not embed AngelScript,
not re-derive gameplay from data). One Ethanon frame per 60 Hz tick. Snapshot taken at 0.7.12's
render point, before callbacks. Enhanced from the start, PT+EN, assets in place.

**Dead ends.** The original's menu ignores Enter unless the (hidden) cursor entity is over a button:
it is mouse-driven through CollideDynamic. First capture was offset by DPI virtualisation.

---

## 2026-09-27 (continued) — the game runs, whole

**Built.** eth runtime + 25 scripts (9c64f8a); render/ + layer (abbc062); engine Ogg Vorbis (abb9e8a)
and window control (a516b7c), both additive and announced to the Magic Portals project, which
shares the engine; English image variants; live shadows to the light's reach; ten headless
gameplay scenarios.

**Numbers.** 13 suites, 2593 checks, 0 failures. Level 1 at 1024x768 against the original's capture:
sampled pixels within 2 levels. Scenarios: 458 checks, 0 script aborts, no port bug.

**Broke / dead ends.** Smart App Control refused freshly linked executables (Penumbra.exe twice in a
row) until every exe carried an icon, VERSIONINFO and a manifest; with them, none was refused again.
The first lighting comparison looked "too dark" - it was the widescreen framing and a different
moment, not the lighting: a pixel-aligned 1024x768 capture matched. A menu capture depends on the
live mouse (cursor.ent follows it) - hence --cursor.

**Decisions.** E7 (lv20 / lv31+), E9 (shadow length), R4 exception for generated English art.

---

## 2026-09-27 (night) — standing statues, highlights, options, a package

**Built.** Engine f30df7c + b999491 (vertical 2D sprites, gloss highlights; opt-in, bit-identical
for everything else; announced to the Magic Portals project and cleared by it before the push). The
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

## 2026-09-28 — a pause, pad menus, six more scenarios, one test executable

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
never play, the off-screen second summon, the 50-mana refusal) are pinned, not fixed, pending a
decision.

**Later the same day.** E15 (the misnamed horror markers play). The round's gates: test_pn_all once,
16 suites, 3719 checks, 0 failures. Mistakes, owned: a capture loop launched a freshly relinked
Penumbra.exe eight times after Smart App Control had refused it (eight notifications), and later a
build-then-run chain relaunched a refused test_pn_all because the edit meant to relink it had not
applied. Rule written down (CLAUDE.md rule 5 practice): launch a new binary once, alone, and only
after the build log shows it was relinked.

**The pause, live.** Driven with real input events: Esc in level 1 opened the pause, froze the game
(two frames 1.5 s apart byte-identical), and Main menu reached the main menu. Two anomalies came
from the test, not the game: the window moved under a resting mouse, and the Magic Portals
project's game windows, opening on the same desktop, took the focus and some of the rig's input.
The pointer now selects a pause row only when it moves 2 logical pixels or more in a tick (against
a pixel of rounding wobble; the rig's window move is larger and is a test artifact, and a pointer
creeping slower than that does not highlight), and every pause transition is logged to
penumbra.log. Live desktop tests stopped while the other project drives windows on this desktop;
E13/E14 by hand is the remaining check. test_pn_all once: 16 suites, 3719 checks, 0 failures.

---

## 2026-09-28 — macOS and iOS (branch apple-port)

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
width. The simulator's base-instance limit is left to the renderer.

---

## 2026-09-28 — every platform, touch controls, the open polish

**Goal.** The open polish; the game playing on Android, iOS, Windows, Linux and macOS where
possible; controls for mobile, with the buttons from Magic Rampage (not on this laptop at first:
placeholder art until its package was available); Apple builds verified on GitHub Actions (with,
at the time, a read-only deploy key on the engine for the CI).

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

**Broke.** A docs script dropped the planning doc's "Open" heading (restored a commit later).
Ubuntu's glslc, installed in WSL by the polish work, rewrote every committed .spv through the
engine's build; caught before the push, removed, and build_linux.sh now refuses a configured
shader compiler. Smart App Control refused test_pn_all once (exit 126, one notification), at the
polish build; the next relink ran.

**Not verified.** A real phone (Android arm64, any iPhone), a frame in the iOS simulator
(base-instance drawing), sound by ear on any new platform, two-finger touch on a device, a person
playing on each.

**Last.** The packaged Windows game (tools/package.bat, the new fonts and touch art in its data)
launched once, headless and without input, with the go-ahead of the Magic Portals project, which
shares the desktop: level 1 at frame 300 on the laptop's Radeon, lights, torches, HUD and English
text drawn, no touch controls (off on the desktop by default), exit 0.

## 2026-09-29 — small fixes, the pause with real input

**Built.** Magic Rampage's buttons for E16 (from Magic Rampage 7.8.7's XAPK), then the small fixes:
touch-worded hints, the knob only while held, one back button on Options, and the shadows of baked
lights that remove only their own light (engine 6cb4036, which also sizes the descriptor pool from
the layout).

**Numbers.** Windows test_pn_all 17 suites, 8024 checks, 0 failures; Linux 7935; engine 57/57; the
APK starts and plays on the emulator. The pause with real OS input on Windows (planning Step 22):
Esc opens and closes it, a focus loss opens it, the refocus click does not choose, a click resumes.

**Broke.** A descriptor pool one storage buffer short (a literal 7 against the new binding 13):
SwiftShader refused it at startup, desktop drivers did not notice; caught on the emulator before
the push, now sized from the layout and guarded. Smart App Control refused one freshly linked
Penumbra.exe (one notification). The live test's first arrow keys were sent without scan codes.

---

## 2026-09-30 — the public release: the README, the licences, screenshots

**Goal.** The repository ready for its public release, with a professional README that says Claude
Code was used in the development and which platforms were playtested and which only built.
Decided: everything is published, the original's files and the Magic Rampage art included (they
stay their authors'); the engine goes public too; the enhanced edition's own code and the engine
become free to use (MIT No Attribution); the full history stays.

**Built.** README.md rewritten for readers outside the project: the pitch, four screenshots, About
(the original, its authors, how the port works), highlights grouped from E1-E23, a platform status
table (build, automated tests, playtested: only Windows was played by a person), getting started
per platform, short controls and settings, testing, the layout, how this was made (directed and
reviewed by Ivan Cvetanović, done with Claude Code), credits and a licence summary. Its long
reference moved to docs/: enhancements.md (the player's table, now with E15 and E17-E20),
controls.md, playing.md, building.md, testing.md. Licences: a top-level LICENSE (MIT-0 for Ivan
Cvetanović's code); LICENSE.md's table with that code and the engine as MIT-0, game/script and
game/eth LGPL-3.0-or-later, the English art as derived from the original's, and the original's
runtime DLLs under their makers' terms; the engine's LICENSE, README (badge, licence section) and
CONTRIBUTING from MIT to MIT-0. Screenshots in docs/images as JPEG (114-319 KB): the widescreen
menu (Step 25's Linux capture), the Android emulator's touch controls (Step 21), and two new Linux
captures from the existing ~/pn-build-linux binary on lavapipe, level 2 with a fireball and the
options screen at 1920x1080 (out/readme_captures.sh).

**Numbers.** No code changed; nothing was built or launched on Windows. The platform facts come
from the last runs before this step: Windows test_pn_all 17 suites, 10056 checks, 0 failures;
Linux 9967 checks.

**Found.** extracted/app holds the original's runtime DLLs (D3DX9, NVIDIA Cg, Audiere), which are
not the original authors' and which no package carries; LICENSE.md says whose they are. The old
README's font fallback (Arial Bold for a missing Arial Narrow) predated E17: FontAtlas tries the
face's own file, then the bundled stand-in. Left for the release: CLAUDE.md and planning R4 still
call the repository private; tools/package.bat copies LICENSE.md but not LICENSE; the engine's
THIRD_PARTY_LICENSES.md asks for the provenance of assets/audio/ambient.wav before a public release.

---

## 2026-09-30 — the download page, the release files, neutral docs

**Built.** The download page (site/index.html, English and Portuguese): a first Windows step for
the browser's "isn't commonly downloaded" warning (Edge: "...", Keep, Show more, Keep anyway;
Chrome: Keep), Chrome's "Download anyway" in Android step 1, a note for in-app browsers ("Nothing
happens when you tap Download? Open this page in Chrome"), a note for grown-ups on newer Samsung
phones (Settings, Security and privacy, Auto Blocker: off, install, back on), Android's
requirement as "Android 8 or newer with Vulkan 1.2 graphics - most phones from about 2020 on", a
note that the Android version is new and not yet tested on real phones, and two more Windows
help lines ("VCRUNTIME140.dll / MSVCP140.dll was not found": started inside the zip; "MFPlat.DLL
was not found": an N edition of Windows, which needs the Media Feature Pack). The step pictures
show the game's real icon, the original's grey skull (site/images/icon.png, from
extracted/app/penumbra.ico's 48 px image), not a flame. The footer links the code signing policy.
game/windows/how-to-play.txt says the same, in both languages. The SmartScreen warning is
explained as "because the game is new and not signed yet".

**Found.** Penumbra.exe imports MFPlat.DLL and MFReadWrite.dll in its regular import table, not
the delay-load one (read with objdump -p in WSL): on a Windows N edition without the Media
Feature Pack the game cannot start at all. docs/playing.md claimed the game decodes its music
itself there; corrected (E18's decoder is for the other platforms, and on Windows only for a file
Media Foundation refuses).

**Docs.** The shipped notes (game/data/touch_controls.json, game/data/strings.json,
tools/art/make_mr_touch_art.py) now name the game in English and say only whose the art is. DEVLOG.md, the planning record and docs/spec were reworded for a public repository: no
quoted requests, no personal attributions of decisions, no references to working sessions; every
number, commit, step and date kept. README: "over 10,000 automated checks" (Linux measured 10040
after test_pn_paths grew from 92 to 165 checks; no Windows run since). Comment-only C++ changes
(the same line counts).

**Numbers.** Windows: tools\build.bat zero warnings; check.bat clean at /W4 on the seven files
whose comments changed; nothing launched (the desktop was in use). Linux (WSL, GCC): test_pn_all
17 suites, 10040 checks, 0 failures. The release files rebuilt: the APK repackaged with the new
data (build_android.sh --release --package-only; apksigner: Verifies, v2 and v3, the release
key's certificate), then tools\make_release.bat (490 files in the zip, 15.7 MB). The page rendered
headless (Playwright, Chromium) in both languages at desktop and phone width: every
Portuguese key present, no horizontal scroll, the icon loaded (out/shots/site/).

---

## 2026-09-30 — Linux, Mac and iPhone/iPad downloads

**Built.** .github/workflows/release.yml, run by hand from the Actions tab and a dry run unless
"publish" is on, builds three more downloads and adds them to an existing release, next to the
Windows zip and the APK, which it never replaces: Penumbra-Linux.tar.gz (64-bit PCs; built in an
Ubuntu 22.04 container for a glibc 2.35 floor, GCC 13, libstdc++ and libgcc linked statically, the
engine's vendored Vulkan headers, no validation layers), Penumbra-macOS.zip (one app for Apple
silicon and Intel Macs, macOS 13.3 or newer, signed ad hoc, not notarised) and Penumbra-iOS.ipa
(the device build for iOS and iPadOS 16.3 or newer, signed ad hoc for a sideloading tool;
experimental). tools/make_release.py gains `linux` (package.bat's layout, a reproducible tar that
keeps the executable bit) and `sums`; tools/apple/make_release.sh makes the Mac zip (ditto,
verified after unpacking) and the .ipa. Each new package carries its own HOW TO PLAY.txt in
English and Portuguese (game/linux, game/macos). The publish job checks the release's files
against its published checksums and keeps the Windows and Android lines of SHA256SUMS.txt first
and unchanged. The files come from aae1a29, later than the tag's 2b5cdff: the same game, with the
packaging and the start-up fix below.

**Found.** A start that failed inside SupersonicApp's constructor (no display, no Vulkan driver)
printed its fatal line and then never ended on Linux: exit() blocked destroying the job pool's
condition variable while its workers still waited on it, because only ~SupersonicApp joins them.
The catch in game/main.cpp now shuts the pool down (JobSystem::Shutdown, idempotent). Measured in
WSL: with no DISPLAY, and with a missing ICD, the game ran into the 20 s timeout (exit 124)
before, and now exits 1 at once. The workflow's review found three more. `ldd --version | head -1`
could fail its step under pipefail: head stops reading while ldd, a script of several writes, is
still writing, and ldd dies of SIGPIPE (measured: 2458 of 3000 runs in WSL); `sed -n 1p` reads to
the end instead. The Apple bundles carried no licence texts; make_app.sh now copies the ones
package.bat puts beside Penumbra.exe. The Rosetta step could hide a failing x86_64 run; now only
installing Rosetta may fail, and once it is installed the x86_64 run gates the job like the arm64
one. The first dry run (36715036864) stopped at the Linux library check: with libstdc++ linked in
statically, the program names ld-linux-x86-64.so.2, glibc's own loader, which every glibc system
has. It is on the allowed list since aae1a29.

**Docs.** docs/building.md: the release downloads (the workflow and its jobs, the Linux release
build with its library and glibc checks, make_release.py linux and sums, make_release.sh macos and
ios); its Apple section now points to the release's Mac zip and the experimental .ipa instead of
a CI artifact and a quarantine command. docs/testing.md: the release workflow's checks. The
planning record: Step 29, and its Open list (the iOS item revised; a Linux playtest, a Mac
playtest and notarisation added). The header comments of ci.yml and pages.yml name release.yml.
README.md (a line for the other downloads, the platform table), docs/playing.md (the Linux, Mac
and iPhone/iPad downloads, where each keeps its saves), docs/code-signing.md (what each download
is signed with), CLAUDE.md's layout, and the download page: a section for Mac, Linux and
iPhone/iPad, the Mac and Linux buttons marked for those visitors (not an iPad, which says
Macintosh but has a touch screen, nor an Android tablet asking for the desktop site), and the
line addressed to children about asking an adult removed. The HOW TO PLAY texts no longer claim
the Linux version "draws the game correctly" (only level 1 on a software driver was seen).

**Numbers.** Windows: zero warnings, test_pn_all 17 suites, 10129 checks, 0 failures. WSL: the
Linux package byte-identical over two runs (480 files, 16.2 MB); unpacked into a read-only folder
and run as an ordinary user it plays level 1 (exit 0, capture drawn). Dry run 36715036864: Linux
test_pn_all 17 of 17 suites in the 22.04 container, 0 failed (the package itself was not built:
the library check stopped the job first); the Mac app, run from the unpacked zip, drew level 1 on
arm64 and on x86_64 under Rosetta; the .ipa holds 496 entries, Payload/ first. Nobody has played
the Linux, Mac or iPhone/iPad download on a real machine; the .ipa has never run anywhere.
Published: run 36721535557 (every job green: Linux test_pn_all 17 of 17 and the package run -
level 1 drawn from a read-only folder as an ordinary user, and exit 1 at once with no display;
the Mac app on arm64 and on x86_64 under Rosetta). SHA256SUMS.txt now lists all five files; the
Windows and Android lines are unchanged, and every file fetched back through
releases/latest/download matches it. In that run's 22.04 container, lavapipe (Mesa 23, LLVM 15)
drew faint diagonal dotted lines by the torches at frame 300; the same published program on
Mesa 25.2 in WSL draws the frame without them, so they are that driver's, not the game's.
