# PENUMBRA AND THE CASTLE OF SHADOWS — Enhanced, on the Supersonic Engine

> The working brief for Claude Code sessions on the author's machine: its absolute paths, emulator names and rules are that machine's. To build from any clone, see README.md.

## What this is

An enhanced edition of **Penumbra and the Castle of Shadows** (2010, PC, by Andre Santee / Asantee), a 2D
side-view action platformer (a wizard, a sword, fireballs and a light spell; three campaign levels,
a king to beat, local co-op and 2-player Versus in six arenas). The original ran on the **Ethanon
Engine 0.7.12** (D3D9 + NVIDIA Cg, AngelScript gameplay). The enhanced edition runs on the author's own engine,
**Supersonic** (C++20, Vulkan, EnTT), which is a git submodule at `engine/`.

Decisions (2026-09-27): **enhanced from the start** — the original's scripts are the gameplay
spec, but visuals, controls, resolution and balance may be modernised freely; **eleven languages**
(Portuguese, the original's; English; and since E24 the nine others of the author's Magic Rampage
Companion app), selectable in the game; the original's assets are **read in place** from
`extracted/app` (committed, and public since 2026-09-30) and never copied or converted into
the repository.

The original's AngelScript SOURCE is in `extracted/app/*.as` (LGPL-3). It is ported to C++ close to
line by line, on top of a small game-side emulation of the Ethanon runtime ("the Eth layer"), so
that the gameplay behaves as the original did. `docs/spec/` is the decoded knowledge of the original
— scripts, formats, the Ethanon 0.7.12 runtime, the engine's capabilities — with citations. Read the
relevant spec file before touching a system; where the spec and the original disagree, the original
(the `.as` source, then `reference/eth-0.7.12`) wins and the spec gets fixed.

## Layout

```
CMakeLists.txt       engine/ as a subproject, then game/ and tests/ (its project VERSION is the release's)
LICENSE, LICENSE.md  MIT-0 for the enhanced edition and the engine, and the table of every part's terms;
                     licenses/ holds the GPL and LGPL texts
engine/              Supersonic, git submodule pinned to a commit
game/
  third_party/       TinyXML and dr_mp3, vendored
  eth/               PenumbraEth: the Ethanon 0.7.12 runtime emulation (no renderer)
  script/            the .as files ported to C++, one .cpp per .as, on the Eth API
  render/            presentation on the engine: sprites, lights, shadows, particles, text, HUD
  data/              the port's own JSON (strings.json = English + the language list, strings/<id>.json =
                     the other languages, settings defaults, touch layout), fonts, images
  android/           AndroidMain.cpp (unpack, flags, locale) and the manifest
  macos/ ios/        MacMain.mm (main on a Mac) and IOSMain.mm (SupersonicMain on iOS), Info.plists
  windows/ linux/ macos/  how-to-play.txt: the "HOW TO PLAY.txt" each release download carries
  PenumbraLayer.*    the EngineLayer that runs the Eth frame on the tick and draws it
  main.cpp
tests/               test_pn_*.cpp suites (supersonic_add_test)
tests/all/           test_pn_all: RunAll.cpp (the runner), WrapSuite.cmake (writes each suite's wrapper), SuiteRegistry.hpp
tools/build.bat      configure + build with MSVC (Build Tools 18, found by tools/msvc_env.bat) and the Vulkan SDK's glslc
tools/check.bat      lock-free compile check of single files (parallel agents)
tools/package.bat    a playable folder in out/package/Penumbra (never committed)
tools/build_linux.sh Linux (WSL Ubuntu-24.04) build in ~/pn-build-linux, --test runs test_pn_all
tools/build_android.sh  debug APK without Gradle (android_package.py): out/android/Penumbra-debug.apk;
                     --release signs with the offline release key (never in the repository)
tools/make_release.* the release files: `windows` (the zip, from make_release.bat), `linux` (the tar.gz),
                     `sums` (SHA256SUMS.txt)
tools/apple/         make_app.sh (Penumbra.app for macOS or iOS), make_release.sh (the Mac zip, the .ipa);
                     Apple builds run only in CI (this laptop has no Apple SDK)
.github/workflows/   ci.yml (Linux, the gate; Windows, best-effort) and apple.yml: on pushes to main, PRs,
                     by hand; release.yml (by hand): the Linux, Mac and iPhone/iPad downloads, added to a
                     release; pages.yml: site/, the Pages download page (still deployed)
site/                the download page (EN/PT) on GitHub Pages: still published, but the README no longer
                     links to it (it links docs/install.md instead); its retirement awaits the author's decision
docs/install.md      the install guide, linked from the README: every download's steps, the controls, the credits;
                     the steps also live in game/*/how-to-play.txt and site/index.html: change them together
docs/install.pt.md   the same in Portuguese; docs/*.md ship in the Windows and Linux downloads, so both travel there
tools/art/           make_localized_art.py: the image variants with translated words, game/data/images/<id>
                     (make_english_art.py: the same for English only)
tools/l10n/          make_fonts.py: the bundled Noto Sans JP / Arabic subsets (rerun when ja.json changes)
docs/spec/           what the original is and does (read-only knowledge base, cited)
docs/planning/       the port's step record
DEVLOG.md            append-only work log
extracted/app/       THE ORIGINAL GAME, read-only. Never write into it.
reference/           gitignored: Ethanon 0.7.12 source (eth-0.7.12), GS2D r485, disassembly, scripts
```

## Rules

1. **Never write into `extracted/`.** The game reads it in place; saves (high scores, checkpoint
   scene, settings) go to `Supersonic::UserDataDirectory("Penumbra")`.
2. **Never touch the `Supersonic-Engine` checkout on the author's Desktop** — another project's development session
   (Magic Portals) works from it. Engine changes are made in THIS repo's `engine/` checkout: `git -C engine
   pull --ff-only` first, keep changes additive (new values/APIs/opt-in flags; never change existing
   behaviour), add checks to an existing engine suite rather than a new suite where possible (CI
   checks the documented suite counts), build and run the touched engine suites, `git pull --rebase`
   right before `git push`, then commit the new pin here (`git add engine`) in a separate commit.
   Tell the Magic Portals development session (SendMessage to `magic-portals-remake-93`) before each engine push.
3. **Commits and pushes as `IvanCvetanovic <icvetanovic99@gmail.com>`, with no AI trailers** (no
   Co-Authored-By, no session lines) — here and in the engine. The use of Claude Code is disclosed
   once, in README.md's "How this was made", not per commit. Never force-push,
   rewrite history, or `reset --hard` / `checkout --` over uncommitted work. Commit title
   `Penumbra: <what changed>`; body says what was wrong before and what was measured.
4. **Paths are absolute.** A game built on the engine changes its working directory at startup
   (`AnchorAssetRoot`), so every `--screenshot` path must be absolute. The original and game/data are
   found at run time (game/eth/Paths.hpp): `--original`/`--data`, then `original/` and `data/` beside
   the exe (a package, tools/package.bat), then the baked PENUMBRA_ORIGINAL_DIR / PENUMBRA_DATA_DIR.
5. **Smart App Control is enforcing on this laptop.** A freshly linked exe is sometimes refused
   ("An Application Control policy has blocked this file", exit 126, ctest "Not Run") and EVERY
   refusal pops a notification on the desktop. Launch each suite/exe at most once per build; never loop
   ctest or relink-and-retry; report a refused exe as "not run". `ctest --test-dir build -N` lists
   without launching. Every executable gets `penumbra_windows_resources` (icon, VERSIONINFO,
   manifest): refusals stopped once they carried them. Prefer `build/tests/test_pn_all.exe` to
   ctest: one executable holds every suite (tests/all/), so there is one Smart App Control
   judgement per build instead of one per suite. Each suite runs in a child process of that same
   file, and the runner stops at the first child that cannot start. It is not registered with
   ctest, because ctest already runs each suite's own exe and registering it would run every
   suite twice.
6. **Builds are serialised.** Only one agent builds at a time, in the one `build/` directory:
   `cmd //c "tools\build.bat --target <T>"` from Git Bash at the repo root. Zero warnings (/W4).
   Parallel agents write code; an integration step builds and runs.
7. **The tick.** One Ethanon frame per 60 Hz engine tick. `GetTime()` is simulated ms, one
   app-lifetime uint32 counter never reset by a scene load; `UnitsPerSecond(x) = x/60` (0 on the
   first tick after a scene load); `GetFPSRate() = 60`. No wall clock, no libm-dependent RNG in the
   tick: use the Eth layer's seeded RNG and `DetMath` where results must reproduce.
8. **Text is cp1252 in the scripts, Unicode on the screen.** The original's strings are
   Windows-1252 bytes. Keep them as bytes in the ported code (write non-ASCII in C++ literals as
   `\xE3` escapes). Translation happens at the draw boundary (game/render/Localization):
   `game/data/strings.json` (English, and the language list) and `game/data/strings/<id>.json` (the
   nine others), keyed by the original string, give UTF-8, which FontAtlas draws by code point;
   Portuguese passes through as cp1252 converted to UTF-8. Japanese and Arabic always draw from the
   bundled Noto fonts (tools/l10n/make_fonts.py; rerun it whenever ja.json changes), Arabic shaped
   and laid out right to left by game/render/ArabicShaping. Every text has a room on screen
   (tests/data/l10n_rooms.json) that test_pn_render_hud checks in every language; the words baked
   into images come from tools/art/make_localized_art.py (game/data/images/<id>/).
   One byte goes beyond cp1252: 0x8D, which cp1252 leaves undefined and none of the original's
   files holds, is the port's U+0107 (c with acute) for the enhanced edition's credit (E21):
   `\x8D` in C++, `ć` in strings.json (eth/Text.hpp). The other four undefined bytes stay undefined.
9. **Port faithfully, then enhance deliberately.** A ported function keeps the original's name,
   order of operations and numbers, with `// file.as:line` comments. Enhancements (widescreen, gamepads,
   keyboard P2, English, settings, bug fixes) are explicit, switchable where it matters, and listed
   in `docs/planning/` with what the original did.
10. **Comments explain why**, not what. Match the engine's style (`engine/CONTRIBUTING.md`):
    four spaces, PascalCase types and public methods, camelCase locals, `m_` members, `k` constants.

## Running

```bash
cmd //c "tools\build.bat --target Penumbra"
build/game/Penumbra.exe                                   # the menu
build/game/Penumbra.exe --start level1 --window 1280x720 --fixed-step --frames 300 \
    --screenshot "$PWD/out/shots/level1.png"             # a headless capture (absolute path!)
build/tests/test_pn_<suite>.exe                           # one suite, once
build/tests/test_pn_all.exe                               # every suite in one exe: launch it ONCE per build
build/tests/test_pn_all.exe --suite boot                  # one suite, in-process, as test_pn_boot.exe runs it
build/tests/test_pn_all.exe --list                        # the suites it holds; runs nothing
```

Linux (WSL) and Android, from Git Bash. No Smart App Control there: a Linux binary or an APK
can be launched as often as needed. Put `MSYS_NO_PATHCONV=1` before any `wsl` call. Never install
a shader compiler (glslc, glslang) in WSL: the engine's build writes SPIR-V into
engine/assets/shaders, and Ubuntu's glslc rewrote every committed blob with different bytes once.
Shader edits are compiled on Windows with the Vulkan SDK's glslc (`glslc <src> -o <out>`), which
reproduces the committed blobs; tools/build_linux.sh stops if a build dir has a compiler cached.

```bash
MSYS_NO_PATHCONV=1 wsl -d Ubuntu-24.04 -u root -- bash /mnt/c/<path to the repository>/tools/build_linux.sh --test
# headless capture on lavapipe (from the build's game/ folder; absolute --screenshot path):
cd ~/pn-build-linux/game && VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json xvfb-run -a -s "-screen 0 1920x1080x24" \
    ./Penumbra --start level1 --window 1280x720 --fixed-step --frames 300 --screenshot /mnt/c/.../out/shots/linux/level1.png
bash tools/build_android.sh --abi all                     # APK in out/android/
```

The Android emulator is ours only as `Penumbra_API33_x86_64` on port 5560, headless
(`emulator -avd Penumbra_API33_x86_64 -port 5560 -no-window -no-audio -gpu swiftshader_indirect
-memory 2048 -cores 2`), always addressed with `adb -s emulator-5560`, input only through `adb shell
input`. Never start or change `MP_Original_API30`: the Magic Portals project's parity rig.

The original runs from a scratch copy (never from `extracted/app`, which it would write into):
`reference/analysis/orig_rig.ps1` drives a copy of `machine.exe` (start / shot / keys / stop). An
HKCU AppCompatFlags `HIGHDPIAWARE` entry is set for that scratch exe only.
