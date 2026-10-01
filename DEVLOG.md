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

---

## 2026-09-30 — E24: eleven languages

**Built.** The game spoke Portuguese (the original) and English; it now also speaks German,
Spanish, French, Italian, Russian, Turkish, Ukrainian, Japanese and Arabic, the languages of the
author's Magic Rampage Companion app (its picker: en de es fr it pt ru tr uk ja ar). The scripts
keep the original's cp1252 bytes; translation at the draw boundary (game/render/Localization)
now yields UTF-8 from strings.json (English and the language list) or game/data/strings/<id>.json
(the nine others; a missing entry falls back to English), and FontAtlas lays out and bakes by code
point, all of a frame's new characters in one bake (Prepare). Japanese and Arabic always draw from
bundled Noto Sans JP and Noto Sans Arabic Bold subsets (tools/l10n/make_fonts.py: google/fonts at
a pinned commit, sha256-checked, instanced at weight 700, OFL), scaled em-to-em onto the line's
face. Arabic is joined to its presentation forms and laid out right to left with a simplified bidi
(game/render/ArabicShaping); a block is right-aligned, and the menu panels' title and body and the
pause menu share their box's right edge (HudCmd::rtlRight, an E24 overload of DrawText and
shadowText; ignored left to right). `language` in settings.json takes the eleven ids; a first
launch follows Windows' display language, the POSIX locale or the device's language on Android,
macOS and iOS, else English; `--lang` takes any id. The options screen's two-row language switch is
a chooser naming each language in its own script. The words baked into images come from
tools/art/make_localized_art.py (make_english_art.py is now its English wrapper and regenerates
the committed English files byte for byte): the menu buttons, the back arrow, the player labels
and the logo's subtitle per language, each menu label within the room its button has in the menu
scene (the statue and the torches), Matura MT Script Capitals for Latin where it has the glyphs
(its capital I reads as J, so a word with one goes to Kurale), Kurale for Cyrillic, Yuji Syuku for
Japanese, Aref Ruqaa and Noto Sans Arabic for Arabic (OFL faces pinned like the fonts).

**Found.** The first fitting rule for the translations - no line wider than the Portuguese or
English it replaces - was far too strict and forced unnatural short words ("Completa" for
"Pantalla completa", "Hades" for "Infierno", "Стела" for "Обелиск", titles that no longer matched
their buttons). Every text now has its real room where the game draws it
(tests/data/l10n_rooms.json: face, size, width to the next element or edge, lines by height, and a
visual limit of 1.15 x the wider of Portuguese and English for texts drawn in the world), and
TestRooms lays every language out with the game's own layout against it; a second pass restored
the natural wordings. Key names follow the local keyboard where it prints its own (Strg,
Alt+Entrée, Alt+Intro, Alt+Invio). Japanese uses ステージ for the original's "fase", because the
HUD's "lv:" is the character's level. An adversarial review of the code found per-language test
loops that could pass over no files at all (the nine files are now required to exist and be
complete) and a rebake per text on a new script's first frame (now one per frame). A visual
review of 90 captures found the Arabic panel titles on the wrong side, the Italian "Indietro"
reading "Jndietro" in Matura, the English logo subtitle in the new languages, and wording nits;
all fixed.

**Docs.** CLAUDE.md (the languages decision, rule 8 restated: cp1252 in the scripts, Unicode on
the screen; the layout), docs/enhancements.md (E24), docs/playing.md (the settings value, the
first language, --lang), README.md (the languages row), game/data/fonts/README.md (the two Noto
subsets), and the art tool's and font tool's own documentation. The 1.0.0 downloads still carry
Portuguese and English only; the other languages reach players with the next release.

**Numbers.** WSL (GCC 13): zero warnings; test_pn_all 17 suites, 17547 checks, 0 failures,
including every language's text against its room, the coverage of every character by the face
it is routed to, Arabic shaping golden tests, and a German -> French -> German switch of text,
touch hints and image variants. Portuguese and English captures (menu panels, options, level 1,
pause, arena select) are pixel-identical to before E24 except the language row itself. Windows:
tools\build.bat --parallel 2 built with zero warnings; Smart App Control refused the new
test_pn_all.exe (exit 126), so it was not run there (CI's Windows job runs it). The Japanese font
is 934 characters, 162 KB; the Arabic one 1171 characters, 97 KB; the nine languages' images add
about 2.3 MB. The translations are machine-made and machine-reviewed (each by an independent
back-translation), not checked by native speakers.

---

## 2026-09-30 — Version 1.0.1

**Built.** The version is 1.0.1 everywhere make_release.py checks (CMakeLists.txt, the Windows
manifest, the Android manifest with versionCode 2, and both Info.plists with build number 2), so
a 1.0.1 APK installs over 1.0.0 as an update. tools/build_android.sh takes `--jobs <n>` (default
6, as before) for machines short of memory.

**Numbers.** `tools/build_android.sh --release --jobs 3`: zero warnings; the APK (arm64-v8a and
x86_64, 21 MB) verifies with v2 and v3 signatures from the release key (certificate SHA-256
ac1d43bd...66da), package versionCode 2, versionName 1.0.1, and carries the nine language files,
both Noto fonts and every language's images. Not run on the emulator (memory was short); it is
for a test on a real phone. No release was made.

---

## 2026-09-30 — E25: a phone-sized UI

**Why.** Played on a phone, 1.0.1 was small everywhere. The levels showed a wide stretch of the
castle around a small wizard. The menu sat in the middle of the screen, with the panel's text at a
4:3 monitor's size. The direction control's down arrow did something only at a level's exit. All
three now change while the touch controls are on (E16), and nowhere else.

**Built.**
- **The zoom.** render/PhoneUi gives the campaign's levels and checkpoint.esc a logical screen the
  zoom times smaller than E1's, so the scripts' camera, HUD, messages and pause work in it:
  - automatic is 150% on a phone-shaped screen (18:9 or longer) and 125% elsewhere;
  - `zoom` in settings.json, `--zoom`, and a Zoom row on a phone's options screen (x 540, y 424-474);
  - never narrower than 908 px, the pause button's edge at 1024, short of which E24's rooms cap
    every message: on 20:9 the row offers up to 175% (a value set higher is drawn at about 188%),
    a 4:3 tablet's automatic 125% comes out at about 113%, and the row offers no step past the limit;
  - a finished campaign's end screen goes back to E1's screen (Machine::SetScreenSize).
- **The menu.** menu.esc and arena_select.esc keep 1024x768 for the scripts. On a screen wider than
  4:3, the view scales them until the logo and the seven buttons fill the height, set at the left
  (CameraRig's MenuFrame). showData sets its title and body in what the window shows of the panel,
  scaled by one factor (HudCmd::fit, measured by FontAtlas::Measure without baking), from E1's size
  up to 1.6 times it.
- **The down button.** The direction control is left and right only. A down button
  (images/touch/exit_down.png, the former arrow alone) shows above them only while
  Script::g_nextLevelOffered is up; ETHCallback_next_level raises it where player 1 stands at the
  door. The disc's down sector is a manifest flag; the placeholder look keeps it. The touch layout
  is scaled by TouchInput::unit, so the buttons keep their size under the zoom and the larger menu.

**Numbers.**
- WSL (GCC 13): zero warnings; test_pn_all 17 suites, 19522 checks, 0 failures.
- The new checks:
  - the zoom's setting and rules: ten cases across six window shapes;
  - where the larger menu goes on seven window shapes (x1.32 on 20:9 and 16:9, x1.22 on 16:10,
    x1.12 on a 2360x1640 tablet; a 4:3 tablet unchanged);
  - a click and a tap on every menu button landing in its collision box;
  - every panel (the arenas and both Versus texts included) in all eleven languages, fitted inside
    its box at 2400x1080, 1920x1080 and 2560x1600: from 1.01 to 1.60 times E1's size;
  - the down button hidden away from the exit and a tap on it pressing down alone;
  - the layout at a zoomed screen's scale;
  - level 1 at 150% through the real game: a 1138x512 screen, the door's offer 19 px from it, and
    level 2 loaded 179 ticks after the button's tap.
- Measured with the touch wording on, the widest message line in any language is 759 px (Arial 30).
- A desktop without the touch controls draws exactly as before: the Portuguese and English
  1024x768 capture set (menu panels, options, level 1, pause, arena select) is byte-identical to
  the captures taken before E25, 20 of 20 files.
- Captures at 2400x1080 in pt, en, ja and ar: the menu, How to Play, Credits, the arena select,
  the phone options screen, level 1, and level 1 at the exit and a step short of it. In English
  also a jump, the pause, and a drop from 2000 px above the exit. The drop starts above level 1's
  geometry, where nothing is lit, so it shows only that the camera keeps the wizard on the zoomed
  screen as he falls. The same at 2048x1536, a 4:3 tablet: the menu as before, the
  levels at 113%. The ported camera has no level bounds (cameraManager.as); at level 1's left edge
  the zoomed screen shows the black past the wall, as E1's does.

**Open.**
- The nine other languages' touch wording for "Próxima fase: seta para baixo" still says the down
  arrow at the bottom left. The room allows 861 px.
- The end screen's revert and the layer's composition of the zoomed screen are layer code; no
  suite runs them, and only the captures saw them.
- docs/images/touch-controls.jpg still shows the down arrow.
- Not played on a phone.

## 2026-10-01 — E25's review; E26: the HUD in a safe frame

**Why.** E25's review found five things. The zoom tightened co-op's leash. A message could run
under the pause button on a zoomed tablet. The gamepad icon sat over the enlarged panel text. How
to Play and the credits were no larger on a phone. The phone menu ignored the safe area's top and
bottom. And on a phone with curved edges the hp and mp values could not be read: the bars start
at the screen's top-left corner, the values ride the bars' ends in the bars' own dark colours,
and the timer sits in the top-right corner.

**Built.**
- **The message rule** (render/PhoneUi.hpp). A message line starts 10 px past the HUD frame's left
  edge and keeps 785 + 10 px up to the pause button, on every screen a touch screen draws a level
  on. The zoom stops where the rule would break; this replaces E25's 908 px floor. E24's rooms cap
  every message a touch screen draws at 785 px. Desktop-only wordings keep 888.
- **Co-op.** A zoomed campaign scene goes back to E1's screen once the princess exists, until the
  next load (CampaignUnzooms, with the end screen's case).
- **The menu in the safe area.** The larger menu fills the safe area's height and keeps the logo
  and the Quit button inside it; the panel's text box stops at its top and bottom. Without a
  safe area the frame is E25's exactly, checked on seven window shapes.
- **Two columns.** showData's body may be broken at a blank line into two columns, the second
  0.6 em past the first (to its left in Arabic), raised to the title's row where it clears the
  title. HudRenderer picks the break, keeping hand-made breaks.
- **The gamepad icons** on a phone's panel: half E1's size, in a row in the top corner the text
  leaves free, with the text kept out of their box (TextFit::avoid).
- **E26.** In a level or an arena, with the touch controls on, the bars, their values, the lives,
  the timer, the message lines and the loading message are drawn inside a frame. The frame is the safe area or
  `edgeMargin` (auto 3.5% of the width at the sides and of the height at the top on a phone, 1% on
  a tablet; 0-8), whichever is more. The pause button hangs from the frame's corner. The values are
  light text over the lives counter's dark shadow, and never leave the bar's left end.
- Dev flags for captures: `--edge-margin`, `--safe-area`, `--princess`, `--hp`.

**Numbers.**
- WSL (GCC 13): zero warnings. test_pn_all: 17 suites, 21254 checks, 16 suites pass. The only
  failures are 4 in test_pn_render_hud's font coverage: the Japanese touch wording for "Próxima
  fase" (rewritten in another session) uses U+73FE, which the bundled Noto Sans JP lacks.
  TestRooms passes with every language file as it stands.
- The message rule on ten shapes, widescreen on and off, at every zoom offered and E1's: the
  smallest room is 785 px. Offered steps with the automatic frame: 20:9 up to 175% (limit 182%),
  18:9 150%, a notched 19.5:9 iPhone 150%, 16:9 150%, 16:10 125%, 3:2 125%. A 4:3 tablet's
  automatic 125% is drawn at 110%.
- Co-op in the real game: summoned into a zoomed level 1 and held 540 px below the wizard, the
  princess is alive after 5 s on E1's 1707x768 screen. Without the rule she dies on the 1138x512
  one.
- Level 1's HUD in the real game: with no frame every command is the original's (the values dark
  at the bars' ends, "lv: 1" at x -27 at no experience). With a frame the bars, values, lives,
  timer, messages and the loading message move by it and nothing else does. On touch the values are two texts, light
  over dark, at least 2 px inside the bar's left end at hp 10.
- How to Play and the credits on 2400x1080, in eleven languages: 1.24 (the Japanese credits) to
  1.60 times E1's size. Only that one and the Portuguese How to Play (1.29) are under 1.3; the
  earlier E25 build drew both at about 1.0. On a notched 2532x1170 iPhone: 1.00 to 1.27. 16:9 and
  16:10 keep one column at about 1.0. No glyph lies on a gamepad's icon in any language or window.
- A desktop without the touch controls draws exactly as before: the pt/en 1024x768 capture set is
  byte-identical to out/shots/l10n/final, 20 of 20 files.
- Captures (out/shots/phone/e26): 2400x1080, 2532x1170 with safe insets 132,0,132,63, and
  2048x1536, in en, ja, ar and pt. The set is the menu, How to Play, Credits, the arena select,
  level 1, level 1 at the exit, and co-op (--princess). In English also level 1 at hp 12 and the
  pause.

**Open.**
- The edge margin has no options row: its label would need nine translations. A phone player
  cannot edit settings.json, so only the automatic margin reaches them.
- MSVC /W4 not checked (WSL only).
- (Resolved below: the Noto Sans JP subset regenerated with U+73FE.)

---

## 2026-10-01 — E25/E26: the review's last fixes

**Built.** A review of E25 and E26 found six things; five are fixed. The Japanese next-level
touch wording used 現 (U+73FE), missing from the bundled Noto Sans JP subset: the subset is
regenerated (tools/l10n/make_fonts.py, 935 characters). On a phone with a side cutout the loading
message started inside it: the phone panel's shown area now starts at the safe area's left. At
exactly the fill scale, float rounding could put std::clamp's high bound a hair under its low
(undefined behaviour) in ComputeMenuFrame: the high is held at the low. The zoom chooser's list
now follows a touch switch and safe insets that arrive after start-up (RefreshZoomChoices). The
search for player 2's princess, a walk of the whole scene, runs only in a zoomed campaign scene.

**Open.** On a 4:3 touch tablet (no phone panel) the gamepad icons still sit over the panel's
title row, as in the original; a half-size row there would need its own right-to-left placement.

**Numbers.** WSL: zero warnings; test_pn_all 17 suites, 21250 checks, 0 failures. Without touch
the twenty Portuguese and English reference captures are pixel-identical to before E24's polish.

---

## 2026-10-01 — The HUD panel as a plaque, lower direction buttons, an adaptive icon

**Why.** Three things on a phone. E26's hp/mp/xp panel floated inside its frame: `frame.png` is the
original's border for a screen corner, with stone along its bottom and right only, so once the panel
stood in from the edge its top and left ended in bare black. The left and right buttons were drawn
207 px above the screen's bottom edge, higher than a thumb rests. And a recent Android drew the icon
on a white plate, because the APK carried only plain square mipmaps, which launchers from Android 8
put on a white background.

**Built.**
- *The plaque.* `DrawSpritePart` (Eth): a sprite command carrying a sub-rectangle of its image, which the
  queue keeps clamped to the image. With the touch controls on the panel stands at the safe area's
  corner (the display's own insets, no margin) with stone on all four sides: `drawPlaqueStone`
  (interface.cpp) cuts six pieces per player out of `frame.png`'s own strips, none overlapping another or
  the frame's stone, so the bars start 16 logical px in; from one player's panel to the next the stone
  runs on. `TouchHud` gained `plaque`, `panelLeft` and `panelTop`, which the layer fills from
  `ComputeHudFrame(..., 0)`. The lives counter keeps the bars' row, and `hudMessagesTopLeft` keeps the
  message lines below the plaque. The timer, the pause button, the messages' left edge and the loading
  message keep the margin's frame; the other controls hang from the safe area as before. Without the
  touch controls the commands are unchanged.
- *The arrows.* The direction control draws its two buttons above the disc's centre and its lower half is
  empty, so the box now hangs below the screen's edge: `TouchControlSpec::overhang` (a per-control
  manifest field: how far a box may lie past the edges it hangs from, and how far negative its offset may
  go; at most half the control) and the shipped `dpad` at offset y -99 put the buttons' centres 84 px above
  the edge, level with the jump button's. The disc, its dead zone, sectors, knob and hit test are
  unchanged. The down button moved down by the same 123 px (offset 150).
- *The icon.* tools/android_package.py also writes `res/mipmap-anydpi-v26/ic_launcher.xml`: an adaptive
  icon with a deep-violet gradient background (`res/drawable/ic_launcher_background.xml`, #2A1536 to
  #0F0716, the iOS tile's colours), a foreground and a monochrome layer for Android 13's themed icons,
  both layers at five densities (108 dp, 108 to 432 px). A transparent background was tried first and
  the system draws it as a black disc (an adaptive icon's layers are composed over black), so a
  transparent launcher icon is not possible on Android: the skull gets a deliberate tile. The skull's smallest enclosing circle over its
  visible pixels (alpha 16 or more of 255) is fitted to the 66 dp circle no launcher mask cuts, and the
  stage stops if a visible pixel lies outside it. The plain `ic_launcher.png` files are unchanged. iOS fills
  what is transparent and the App Store refuses an alpha channel, so the AppIcon PNGs are now the icon over
  an opaque deep-violet gradient (#2A1536 to #0F0716, from the menus' cave): tools/apple/ico_to_png.py
  takes `--flatten TOP[,BOTTOM]` (standard library only) and make_app.sh uses it for iOS only; macOS's
  `.icns` stays transparent.

**Numbers.** WSL: zero warnings; test_pn_all 17 suites, 21586 checks, 0 failures (21250 before). The twenty
Portuguese and English desktop captures are pixel-identical to before. Phone-sized captures (2400x1080):
the arrow buttons moved 173 px down (123 logical px), the gap between the exit button and the arrows is
unchanged (53 px), and at 12 hp "hp: 12" stays on the bar's left end, never on the stone.
The screenshots docs/images/touch-controls.jpg and site/images/touch-controls.jpg are retaken with the new
layout (1280x720). With a 132 px side and a 63 px bottom inset (2532x1170) the plaque starts at the inset
line and both arrows lie above the bottom inset. A review by four independent readers, each finding checked by two skeptics, found two cosmetic seams in the plaque's soft
shadow: the top-right sliver restarts the frame's own shadow ramp (a notch of about 7 px), and player 2's
left stone lies over the first panel's shadow (about 3 px darker). Neither shows over the dark stretch
behind the HUD; the cuts were not changed. Android: build_android.sh --package-only, zero warnings; aapt2
dump badging gives `res/mipmap-anydpi-v26/ic_launcher.xml` for the application icon at every density, the
farthest visible pixel 32.5 dp from the layer's centre (limit 33). On the Android 13 emulator
(Penumbra_API33_x86_64, Pixel launcher) the 1.0.0 icon sits on a white circle, the transparent-background
adaptive icon on a black one, and the violet-tile icon on its own tile; the launch splash shows the same
tile on the window's black, with no white disc.

**Open.**
- The icon was seen on the emulator's Pixel launcher only, not on a phone (another maker's launcher may
  mask or tint it differently, and Android 13's themed colours were previewed in Pillow only); the iOS PNGs
  were not run through sips here (no Apple tools on this machine).
- The two shadow seams above.

---

## 2026-10-01 — A closer camera, larger direction buttons; version 1.0.2

**Why.** Played again on a phone, the levels still showed more than needed and the left and right
buttons were smaller than the action buttons beside the thumb.

**Built.**
- *The zoom.* The automatic campaign zoom on a phone-shaped screen is 175% (was 150%), the next of
  the options screen's steps. A tablet-shaped screen stays at 125%. The existing rule that holds the
  zoom where a message line would lose its room still applies: a 20:9 phone (2400x1080, a 976x439
  screen) gets the full 175%, an 18:9 one about 162%.
- *The direction buttons.* The direction control is 400 logical px across (was 330): its art is the
  same 406 px canvas, drawn larger, so each button is 126 px with a face of about 118, as large as
  the action buttons (120). The box hangs 138 px below the screen's edge (`dpad` offset y -138,
  `overhang` y 150), which keeps the buttons' centres 83.7 px above it, level with the jump button's.
  The down button is the buttons' new size (126), centred over them and about 17 px above their
  faces (offset [161, 160]). The finger padding around the disc is 30 (was 60), so the larger disc
  still never reaches the sword button on a notched 4:3 screen; the knob's brackets are the art's own
  128 px. tools/art/make_mr_touch_art.py has the new box and centres; run on the Magic Rampage
  package it writes the same fourteen PNGs byte for byte (only the drawn size changed).
- *Version 1.0.2.* CMakeLists.txt (and with it the Windows resources), the Windows manifest
  (1.0.2.0), the Android manifest (versionName 1.0.2, versionCode 3) and both Info.plists (1.0.2,
  build 3) agree (`tools/make_release.py check`); the release workflow's default tag is v1.0.2.

**Numbers.** WSL: zero warnings; test_pn_all 17 suites, 21587 checks, 0 failures. The suites that
quote the old numbers (the automatic zoom, the direction control's box and the down button's place)
were updated; test_pn_render_touch now also says that the box lies in the screen's left half "at
most" (a 4:3 screen with an 88 px inset has it end exactly at the middle).

---

## 2026-10-01 — E27: an automatic language, a way to find it without reading, a framed options screen

**Why.** Every button of the main menu is a picture of words in the current language, and the options
screen's rows are translated too: a player in a language they cannot read had nothing to look for
to change it. The language was also chosen once, at the first run, and then fixed. And the options
screen was a column of text rows with "[x]" and "[<]" marks over the menu's background.

**Built.**
- *Automatic language.* `Settings::languageAuto` is the default: `settings.json` holds `"language":
  "auto"` (or no key), and each start resolves it again - the system's language when the game speaks it
  (Windows' display language, the POSIX locale, the device's language on Android, macOS and iOS),
  English otherwise. A named id is a choice, so a file from an earlier version keeps its language. The
  options screen's Language chooser lists Automatic first, then the eleven languages each in its own
  script (index 0, then `kLanguages[i]` at i + 1); a pick is a row other than the one last seeded, so
  a `--lang` run never reads as one.
- *Shared art and calls.* `tools/art/make_options_art.py` makes game/data/images/options/ from Magic
  Rampage's package: a stone-frame panel, check boxes, arrow, minus and plus buttons, a speaker, a note
  and a controller, plus a globe and a monitor drawn in the same style. `DrawShapedSpritePart` (Eth) is
  a stretched sub-rectangle of an image; the script helpers `drawPanel` (nine slices that tile the
  rectangle exactly), `drawOptionsIcon`, `loadOptionsArt` and `optionsArtReady` draw it, and
  `Script::g_artDir` (set by the layer) says where the folder is. Without the art, or without a data
  folder as in the suites, every screen is exactly the original's.
- *The options screen.* On a stone-framed panel (the video mode list on one of its own), "[x]" is a
  check box, "[<]" and "[>]" arrow buttons, a stepper's arrows minus and plus; rules divide the groups
  and each has an icon at the right (a monitor, a controller, a globe beside the Language row, a note
  and a speaker beside the volumes). Every row keeps its place and its box, so the rooms in
  tests/data/l10n_rooms.json hold (the switch rows' check box is 26 px where the brackets were 24:
  the room test reserves 2.5 px for it; the mode list's own mark is 24 px wide, its column being 200).
  The Back arrow moves to the top right, out of the panel's way.
- *The main menu's language button.* A globe at the bottom left of what the window shows (a phone's
  larger menu shows less; the right side is where the panels' text runs) opens a list of the chooser's rows, each in its own script with the current
  one ticked; a row sets the chooser, which the layer applies as it does on the options screen, and a
  click outside the list, or cancel, closes it. It is modal: the menu's buttons do not answer while it
  is open. Scenario 23 drives it (opening, picking, cancelling, clicking outside, the first and last
  rows, and no button without the art).

**Numbers.** WSL: zero warnings; test_pn_all 17 suites, 22109 checks, 0 failures (21587 before). The
languages' automatic label fits the chooser's 176 px value box in all eleven (widest, German, about
109 px). The desktop's level, pause and arena-select captures are pixel-identical to before; the menu
(the globe) and the options screen differ on purpose. A review by four independent readers, each
finding checked by a skeptic, found and had fixed: a click on the globe during the New Game fade froze
the start (the globe now draws only when no start is running); the globe covered the side panels' text
(it moved to the bottom left); the menu's own cursor is under everything the list draws (the list draws
a pointer mark); Alt+Enter was dead while the list was open; a few right-hand labels (Italian, Arabic)
ran past the options panel (it is 680 px wide now, the Back arrow and the right-hand icons moved with
it, and the room data holds those rows to its inner edge); the scenario now checks that the buttons
behind the list do not answer; the credits name the options screen's Magic Rampage art.

**Open.**
- The icons and panel are Magic Rampage's (Asantee Games') with two drawn icons; LICENSE.md and the
  folder's README say which.
- The globe and the list read the window's shown area (`g_phonePanel`), which starts at the safe area's
  left, so a notch on that side keeps clear of them.

---

## 2026-10-01 (evening) — E28: adjustable touch controls

**Why.** The touch buttons had one size, one opacity and one place, set only in `touch_controls.json`, which a player cannot edit on a phone. Magic
Rampage has a screen for this (its own pad, decoded for the purpose: docs/spec/42-magic-rampage-screenpad.md), and its ranges and its look were taken as
the reference; the record of what was built and where it differs is docs/planning/2026-10-01-e28-adjustable-touch-controls.md.

**Built.**
- *The editor.* On a touch-enabled options screen an "Adjust controls" button under the touch switch (videoModes.cpp, marked E28) opens a full-screen
  editor (render/TouchEditor, pure like PauseMenu) over the frozen options scene: the real controls, size tiles (one global size, 0.4 to 1.4 by 0.1; Pause
  and Back keep theirs), opacity tiles (0.2 to 1.8 by 0.2, a multiplier of the manifest's 0.45), a padlock (opens locked; unlock and drag; a size step or a
  restore re-locks), a circular arrow (Restore: the moves only, nothing asked), and the back arrow at the top left (Esc leaves too). The icons are Magic
  Rampage's symbols on its stone frames (tools/art/make_options_art.py writes game/data/images/options/edit_*.png; LICENSE.md and the folder's README say so).
- *The model.* render/TouchTuning: size, opacity and a move per control in manifest pixels, in the screen's direction, from the control's anchor, so they
  survive a window or rotation change; snapped to their grids so a chain of steps ends exactly on the limits. `TouchControls::WithTuning` lays it over the
  manifest as one derived manifest (the default returns the base itself), `MoveFor` runs `ComputeLayout`'s own placement backwards for the drag (that
  function was split into `BoundsFor`, `PlaceControl` and `LayOut` with every expression kept), `GrabBox` is what a finger can take (the direction control's
  strip, not its 400-unit disc). `settings.json` holds `touchTuning`, read as tolerantly as the other fields.
- *The layer.* `ApplyTouch`'s first half is `BuildTouchInput()`; with the editor open the scene is `TouchScene::Edit`. Four things keep the closing Esc, a click
  or a finger from reaching the options screen: the Machine does not tick while the editor is open, `step.touching` forces `K_LMOUSE` false, fingers in the
  Edit scene have no owner, and `PauseMenu::HoldPressed()` arms the input filter on close. `HudCmd::stretchToSides` (default true) lets the outlines stay
  thin on a wide menu. Three dev flags, `--touch-tuning`, `--touch-editor` and `--finger`, set `noSave`: a run with any of them writes no settings.
- *Words.* Two strings (the button, the title) in eleven languages, each with a room in tests/data/l10n_rooms.json.
- *Tools.* tools/check.bat defines `PENUMBRA_TESTS_DATA_DIR`, so the two suites that read tests/data can be compile-checked on their own.

**Numbers.** WSL: zero warnings; test_pn_all 17 suites, 28556 checks, 0 failures (22109 before). Per suite now: render_touch 6936, render_input 2438,
render_pause 4083, render_hud 11440, scenarios 1085 (scenario 24 added); the others are as they were.
- *The size ceiling.* Magic Rampage pushes overlapping buttons apart and Penumbra does not, so `testSizeCeiling` measures, in play (E26's frame, E25's
  automatic zoom, a notch, a gesture bar, a tablet's bars, the Pause in every overlap check), the largest size at which the shipped layout draws no control
  over another: **1.1** on the nine standard cases, **1.0** once a 100 px bottom bar is added (at 1.1 the spell combo button meets the Pause on the 20:9, the
  16:9 and the 4:3 shape), **1.2** on the unzoomed screens the layout was always checked on. Above the ceiling the spell combo button grows into the pause
  button on every screen. The editor offers every size to 1.4 and every one keeps every control inside the safe area; only overlap is left to the player.
- *MoveFor:* 11232 round trips through the real layout (every control, three screen shapes, three insets, two scales, with and without the HUD frame, the wide
  menu's area, four sizes, eight targets each), the result always inside the room it must keep to.
- *Byte identity.* The default captures are byte-identical to those taken at ff3cde8 before any E28 code: `--touch on --mobile-layout on --lang en --fixed-step
  --start level1 --frames 120` at 2400x1080 (SHA-256 3da7437d...) and at 1024x768 (481bff92...).
- *Captures* (headless, Linux/lavapipe, each in a throwaway user directory; none committed): the entry button at 1280x720, 1024x768 and in Portuguese; the
  editor locked at 1024x768 and 2400x1080; unlocked and tuned (`size=1.2,opacity=0.6,jump=-40:30,pause=-30:20,dpad=12:0`) at both; a notch
  (`--safe-area 132,0,132,48 --edge-margin 3.5`); a drag by `--finger` through the layer; the entry tap, with the options scene frozen between frame 12 and
  frame 30; the tuning in level 1; the title in de, ja, ar and ru; size 1.4 and size 0.4 with
  opacity 0.2, in the editor and in play. A pair meant to show a finger pressing a moved jump button (and not its old place) came out identical at frame 60 and shows
  nothing; that the hit areas follow the drawn ones is pinned by test_pn_render_touch instead.
- *Windows* (MSVC, this laptop, with the real Arial Narrow installed): Penumbra and test_pn_all build with zero warnings; `test_pn_all.exe` was launched once
  and ran: 16 suites pass.

**Open.**
- Windows: `test_pn_render_hud` has 18 failures, all of one check, E25's panels check, in Spanish, French and Italian at 2400x1080, 2532x1170 and 1920x1080 (the same 18 with the
  language files and the rooms file put back to their pre-E28 text, so the data is not the cause):
  the first glyph of a panel's line starts up to 0.115 px left of the 2 px slack the check allows, which looks like the side bearing of the real face. It
  passes on Linux with the stand-in fonts. E28 does not touch text layout; the check is not changed here.
- The translations of the two strings are drafts in the vocabulary of the existing touch strings; Arabic, Japanese, Turkish and Ukrainian have not been read by
  a native speaker.
- Android's system Back closed the editor in the emulator (an adb KEYCODE_BACK, back on the options screen); a real phone is not checked. A pad's Back or B cannot close the editor: with the
  touch controls on, no pad is player 1.
- Real touch contacts: one pass on the Android emulator (`adb shell input`, debug APK, x86_64): the button opens the editor, the padlock unlocks, a swipe moves the jump
  button, a size step is taken, Back closes it with the options screen still showing, `settings.json` holds `touchTuning` (size 1.1, jump -69,-69.8) and, after
  force-stopping and relaunching the app, level 1 draws the controls so (the swipe's last sample is lost at the lift, so the button stops short of the swipe's end).
  A real phone has not been tried. The headless captures use `--finger`'s synthetic fingers.
- Not built: sounds, per-control size, a restore confirmation, a flash on a drag while locked, an entry in the pause menu, Magic Rampage's push-apart.

---

## 2026-10-01 (night) — E29 to E31: two columns of buttons, a quiet menu song, a settings screen for a phone

**Why.** Three things were wrong on a phone once E28 let the touch buttons grow. The six action buttons were a diamond with the two combos in a row above it,
400 px wide and 512 tall, and enlarged, the spell combo button ran into the pause button (E28's size ceiling was 1.1, and 1.0 with a tall bottom bar). Going into the
settings and back, or into the arena select and back, started the menu song again from its first note: the original released every sample on every scene load and
started `menu.mp3` again in each menu screen's preLoop (`loopMenuSong`, menu.as:43). And the phone's settings screen was E27's panel over the original's single
column: 25 px rows with 22 px check boxes and 24 px arrows (about 13 dp on a 2400x1080 screen at density 2.625), the unselected half of every pair at 100 of 255 alpha,
and the Back arrow at the top right of the 4:3 box.

**Built.**
- *E29, the buttons.* The six action buttons are two columns of three at the bottom right. From the bottom, the left column is the sword (at the very bottom), the sword
  combo (the heavy attack) and the light; the right column, to its right and 30 px higher, is jump, the spell combo (the strong fire attack) and fire. Offsets from the
  bottom right in manifest pixels, with the old ones after: jump 24, 54 (152, 24); sword 160, 24 (280, 152); fire 24, 306 (24, 152); light 160, 276 (152, 280); sword
  combo 170, 160 (222, 412); spell combo 34, 190 (102, 412). The rules: the right column keeps the 24 px margin the cluster always had, 16 px between the columns and
  between neighbours, each combo (100) centred over its column (120), the sword 24 px above the edge and the right column 30 higher, the arc of a thumb pivoting at the
  corner. Sizes, art and hit paddings are as they were (4 for the four buttons, 6 for the combos), so neighbouring hit areas stay at least 6 px apart. The cluster is 256 px
  wide (was 400) and 426 tall (was 512), and the direction control's two buttons are now level with the sword button. The offsets are in game/data/touch_controls.json and
  `TouchControls::DefaultManifest()` (a test compares them); the E28 model, the editor and the layout rules work on top unchanged, and the placeholder look keeps E16's
  arrangement with its own manifest. A saved tuning's moves are deltas from where the manifest puts each control, so they now apply from the new places and Restore
  returns to them; nothing is migrated. One sentence became false and is replaced in all eleven languages: How to Play's "Combos: the two buttons above them" is now "the two
  middle buttons".
- *E30, the menu song.* `SampleBank::KeepOnNextLoad(path)` (eth/Audio.hpp, `KeepSampleOnNextLoad` in the Eth API) asks for one sample to go through the next `ReleaseAll`
  whole: the same voice, volume, pan and loop flag, still keyed by its name, so the music volume reaches it. `Machine::DoLoad` calls `ReleaseAll` before any preLoop runs,
  so skipping the play in `loopMenuSong` could not work alone. The request is consumed by that load and counts only for a voice that is sounding; while a sample is carried,
  `ReleaseAll` does not call the output's `UnloadAll`, which stops every voice of every clip it loaded (every other voice is stopped one by one, as before). menu.cpp asks
  (`keepMenuSong`) only before the loads to a menu screen, Settings, Versus and `goToMenu`, which is the way back from both; never before a New Game and not on a level's way
  back, so a level begins in its own music and a menu entered from a level starts the song as the original did. `loopMenuSong` finds a kept song playing, sets its volume to 1
  explicitly (a song carried while faded, by Esc during a New Game's fade-out, comes back at full volume) and returns; a cold start is the original's three calls. The code and
  scenario 25 of this carry the tag E30.
- *E30, the main menu's language button (E27) is removed:* the globe, its list, their hooks and the `globe_button` art (with its row in the art folder's README, the LICENSE.md
  wording and its generation in tools/art/make_options_art.py). The options screen's Language row, with its globe icon, is as it was, and so is the chooser.
- *E31, the settings screen on a phone.* With the phone layout up and the options art loaded, `screenModesLoop` hands the frame to `phoneOptionsLoop()`
  (game/script/optionsPhone.cpp): a stone panel of two columns of 474 px cells, rows 88 px tall (82 under an iPhone's home indicator, down to 68 under a tall bottom bar), 30 px
  text, 44 px check boxes, arrow, minus and plus buttons drawn at 56 px and hit over 86 x 88 px. A two-way Switch is one cell, a ticked box and the wording of the state it is in, and a
  tap anywhere in it flips it, so the six pairs and E28's Adjust button take four rows; Refresh rate and Zoom, and Music and Effects, sit side by side. The Back arrow is at the top left of
  what the window shows (inside the safe area or E26's edge margin, whichever is more, in every language) and the language chooser with its globe at the top right. A cell lights only
  while it is pressed, a gap between cells does nothing, and a pressed button is drawn at its art's own 64 px. All 14 controls and all eleven languages stay, with no new string. Back
  and Esc leave through `goToMenu()`, so the menu song goes on. The geometry is pure (`phoneOptionsLayout`, Script.hpp); the layer publishes what the window shows of the scene as
  `Script::g_optionsArea` from a pure `Render::ComputeFixedLayoutArea` (PhoneUi); the cells read and write the globals the old rows did. The desktop's layout and E20's art-less phone
  layout are untouched. docs/planning/2026-10-01-e31-phone-options-screen.md has the geometry and what differs from the first model.
- *Docs.* docs/enhancements.md has rows E29 to E31, and E27's row no longer claims the main-menu globe; playing.md, testing.md and controls.md follow the three changes, and the E28
  planning page notes the new place of its button and the new size ceiling.

**Numbers.** Linux (WSL, gcc): test_pn_all 17 suites, 39140 checks, 0 failures (28556 at E28). Per suite now: render_touch 7307 (6936), render_pause 4088 (4083), render_input 10224
(2438), render_hud 13112 (11440), scenarios 1788 (1085; scenario 23 removed, 25 and 26 added), audio 304 (257); the others are as they were. MSVC `tools\check.bat`: zero warnings at /W4
on the touched files.
- *E29's size ceiling.* `testSizeCeiling`, measured with the real layout (E26's frame, E25's zoom, a notch, a gesture bar, a tablet's bars, the Pause in every check): the largest size at
  which no control is drawn over another in play is **1.2** in every case, where it was 1.1 on the nine standard cases and 1.0 once a 100 px bottom bar is added; unzoomed screens
  stay at 1.2. The spell combo no longer reaches the Pause. At 1.3 exactly these pairs overlap: on a 4:3 screen with an 88 px notch, the direction control's right button and the sword
  button; under the 100 px bottom bar, the fire button and the Pause (a 20:9 phone, a 16:9 and a 4:3 tablet); at 1.4 the fire button also meets the Pause on a 20:9 phone with a notch. The
  fire button's least clearance of the Pause over the 15 in-play cases, in 768ths of the screen, is 87.9 at size 1.0, 2.7 at 1.2, -39.9 at 1.3 and -82.5 at 1.4 (bottom bars read in
  E1's pixels). `kMaxSize` stays 1.4: above 1.2 the player moves buttons apart by hand, as in E28.
- *E29's checks.* `CheckLayoutFits` keeps the rules every arrangement obeys (inside the safe area, nothing drawn over anything, no finger reaching two controls, the halves, the
  Pause's clearance of the combos), the new `CheckColumns` holds the columns' own and runs on the built-in and the shipped manifests, and `testButtonColumns` pins every box on 4:3, 16:9,
  20:9 and a notched 16:9, the sizes and paddings, a finger on each button's centre, and the six gaps between neighbours as fingerless. Every number the old arrangement owned was
  recomputed with the real code. Two finger checks in `testTuningSize` moved from the jump to the fire button, the jump's old centre being inside the sword combo's padded box at size 0.5.
- *E30's checks.* Scenario 23 (the language list) is removed. Scenario 25 plays menu, settings, menu by Esc and by the Back arrow, and menu, arena select, menu, and asserts on the stand-in
  speakers: the same voice id, never stopped, no new play of menu.mp3, volume 1, the music volume setting reaching the carried voice, and the faded case (an arena confirmed, Esc 60
  frames into the 3 s fade-out: the song at 0.67 through the load, back to 1 by `loopMenuSong`); then a New Game (the song fades below 0.05, its voice is stopped by the level's load, the
  sample is forgotten) and Esc back to the menu (a new voice, looping, volume 1). `SoundLog` records the volume per voice and its `UnloadAll` stops every voice, as the device's does, so a fix that only
  handled `ReleaseAll`'s own `Stop` calls could not pass. With `keepMenuSong` emptied (the original's behaviour) scenario 25 fails 3 to 4 checks per screen. test_pn_audio gained a bank-level
  test (47 checks): carried through one load only, volume and loop kept, the music volume reaching it, not carried when it ended or was never played, no effect without an output.
- *E31's checks.* The layout is pinned from three sides. Rooms: every options text carries a `phone` room in tests/data/l10n_rooms.json (22 rooms, 6 null), measured in all eleven languages
  like the others and tied to the layout's own numbers; the tightest are Italian "Pausa quando il gioco perde il focus" 372 of 396 px and German "Automática" 130 of 148 px, and a check proves
  the measurement fails on a room one pixel too narrow. test_pn_render_input pins `ComputeFixedLayoutArea` in five shapes and a zero window and against the camera's own shown rectangle in 20
  window and side combinations, and `phoneOptionsLayout` rect by rect in eight shapes with its invariants. Scenario 26 (606 checks), with the art on, clicks every cell, button, gap and corner
  through the Machine in four window shapes (a 4:3 window, a 20:9 phone, a notched phone, a 4:3 window with 88 px cut-outs), counts the frame's 18 front texts, checks the narrow body's fit, the
  iOS slot without the refresh row, no rectangle meeting x 0 or 1024, and the menu song through Back, the corner and Esc. The layer-driven test keeps E28's editor and the options Back arrow
  apart: a tap on the editor's arrow (one tick or three) and a finger held across the Esc that closes it leave the options screen where it is, and a fresh tap there leaves. Mutations tried in
  a scratch copy, each caught: the Back box not reaching past the corner, a card lit on hover, the Adjust cell live with touch off, a hit not strictly inside, Back by a bare `LoadScene`, a
  toggle flipping every frame the confirm is held. Scenarios 21 and 24 run art-less and keep pinning E20's layout.
- *Found in review.* In a narrow body (a 4:3 window with cut-outs, or a hand-set edge margin) the options text is drawn through a fit box, and every text there was
  smaller than it needed to be: the box was exactly one font size tall while its shadow sits a tenth lower (about 0.91 for every language), and `HudRenderer`'s fit solver measured a
  right-to-left line from the box's right even when the Text has no `rtlRight`, which the placement just above it anchors at the left, so every Arabic text came out at the
  minimum 0.7. The box is now a line and a shadow tall (`optionsPhone.cpp`), and the solver reaches each line from the side it is anchored at (`line.rightInBox`); only a right-to-left
  Text without `rtlRight` changes. Tests in test_pn_render_hud: the same untranslated text gets the same factor in English and Arabic in wide and narrowing boxes, the
  largest-fit search agrees with the solver within 6 % (its estimate is never refined upward), a right-anchored line is as before, and the real narrow-body screen in all eleven
  languages never goes below 0.7, keeps a text that fits at full size, and fits its box; each fails on the old line or the old box. Linux: 39140 checks, render_hud 13112, scenarios 1788.
- *Byte identity.* The desktop's options screen (`--mobile-layout off`) in five captures (1280x720, 1024x768 in Portuguese, 1920x1080 in German with touch off, a hover and a click) is
  byte-identical to the build before E31 (SHA-256 of the 1280x720 one begins 599060e7).
- *Captures* (headless, Linux/lavapipe, none committed): E29, level 1 at 2400x1080, 1280x720 and 1024x768 and the editor locked and unlocked at 2400x1080 (the columns at the bottom right, the
  right one higher; the Pause and the top-right level sign clear of the cluster); E30, the main menu at 1280x720 and 2400x1080 with no globe, and the settings screen with its Language row and
  globe; E31, 2400x1080 in all eleven languages, 1920x1080, 1280x720, 1280x800, 1920x1200, 3120x1440, 2532x1170 with `--safe-area 132,0,132,63`, 1024x768, 1024x768 with 88/0/88/24 px cut-outs
  (the 848 px body), touch off, widescreen off and a 150 px bottom bar, and presses by `--finger`, `--pointer` and `--cursor` on every kind of cell, button and gap, the editor opened from Adjust
  and Back and Esc leaving; they match the mockups the geometry was worked out on.
- *Windows* (this laptop, real Arial Narrow): test_pn_all still shows the 18 failures described under E28's Open.

**Open.**
- No real phone has been tried. E31's dp figures are model values (a row is 47.1 dp at density 2.625, 44.2 at 2.8, 41.2 at 3.0), and the new button places have been seen only in headless
  captures.
- The menu song's survival through a scene load is verified through the stand-in speakers only (a headless run has no audio device); a listen on a phone and on a desktop is worth one pass.
- Arabic, Japanese, Turkish and Ukrainian wordings have not been read by a native speaker, including E29's one line.
- The collapsed switch shows only the wording of its current state. Where a narrow body (a 4:3 window with cut-outs) shrinks a text, the line's top stays where it was, so it sits a little
  high. A boot straight into the options screen (`--start videoModes`) lays its first frame out for the 4:3 box, the window not being known yet.
- A player who moved buttons to suit the old arrangement may want Restore once.
- The screenshots docs/images/menu.jpg (also the site's) show the old main-menu globe, and touch-controls.jpg (also the site's) and touch-editor.jpg the old arrangement of the buttons; they
  are retaken with the next release, and the README and the download page still describe 1.0.2.
