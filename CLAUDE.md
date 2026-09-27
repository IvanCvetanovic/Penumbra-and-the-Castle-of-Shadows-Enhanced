# PENUMBRA E O CASTELO DAS SOMBRAS — Enhanced, on the Supersonic Engine

## What this is

A remake of **Penumbra e o Castelo das Sombras** (2010, PC, by Andre Santee / Asantee), a 2D
side-view action platformer (a wizard, a sword, fireballs and a light spell; three campaign levels,
a king to beat, local co-op and 2-player Versus in six arenas). The original ran on the **Ethanon
Engine 0.7.12** (D3D9 + NVIDIA Cg, AngelScript gameplay). The remake runs on Ivan's own engine,
**Supersonic** (C++20, Vulkan, EnTT), which is a git submodule at `engine/`.

Ivan's rulings (2026-09-27): **enhanced from the start** — the original's scripts are the gameplay
spec, but visuals, controls, resolution and balance may be modernised freely; **Portuguese and
English** text, selectable in the game; the original's assets are **read in place** from
`extracted/app` (already committed; this repository is PRIVATE) and never copied or converted into
the repository.

The original's AngelScript SOURCE is in `extracted/app/*.as` (LGPL-3). It is ported to C++ close to
line by line, on top of a small game-side emulation of the Ethanon runtime ("the Eth layer"), so
that the gameplay behaves as the original did. `docs/spec/` is the decoded knowledge of the original
— scripts, formats, the Ethanon 0.7.12 runtime, the engine's capabilities — with citations. Read the
relevant spec file before touching a system; where the spec and the original disagree, the original
(the `.as` source, then `reference/eth-0.7.12`) wins and the spec gets fixed.

## Layout

```
CMakeLists.txt       engine/ as a subproject, then game/ and tests/
engine/              Supersonic, git submodule pinned to a commit
game/
  eth/               PenumbraEth: the Ethanon 0.7.12 runtime emulation (no renderer)
  script/            the .as files ported to C++, one .cpp per .as, on the Eth API
  render/            presentation on the engine: sprites, lights, shadows, particles, text, HUD
  data/              the port's own JSON (translations, settings defaults), with provenance
  PenumbraLayer.*    the EngineLayer that runs the Eth frame on the tick and draws it
  main.cpp
tests/               test_pn_*.cpp suites (supersonic_add_test)
tools/build.bat      configure + build with MSVC (Build Tools 18) and the Vulkan SDK's glslc
tools/check.bat      lock-free compile check of single files (parallel agents)
tools/package.bat    a playable folder in out/package/Penumbra (never committed)
tools/art/           make_english_art.py: the English image variants in game/data/images/en
docs/spec/           what the original is and does (read-only knowledge base, cited)
docs/planning/       the port's step record
DEVLOG.md            append-only session log
extracted/app/       THE ORIGINAL GAME, read-only. Never write into it.
reference/           gitignored: Ethanon 0.7.12 source (eth-0.7.12), GS2D r485, disassembly, scripts
```

## Rules

1. **Never write into `extracted/`.** The game reads it in place; saves (high scores, checkpoint
   scene, settings) go to `Supersonic::UserDataDirectory("Penumbra")`.
2. **Never touch `C:\Users\icvet\Desktop\Supersonic-Engine`** — another Claude session (Magic
   Portals) works from it. Engine changes are made in THIS repo's `engine/` checkout: `git -C engine
   pull --ff-only` first, keep changes additive (new values/APIs/opt-in flags; never change existing
   behaviour), add checks to an existing engine suite rather than a new suite where possible (CI
   checks the documented suite counts), build and run the touched engine suites, `git pull --rebase`
   right before `git push`, then commit the new pin here (`git add engine`) in a separate commit.
   Tell the Magic Portals session (SendMessage to `magic-portals-remake-93`) before each engine push.
3. **Commits and pushes as `IvanCvetanovic <icvetanovic99@gmail.com>`, with NO AI trailers** (no
   Co-Authored-By, no session lines, no mention of AI) — here and in the engine. Never force-push,
   rewrite history, or `reset --hard` / `checkout --` over uncommitted work. Commit title
   `Penumbra: <what changed>`; body says what was wrong before and what was measured.
4. **Paths are absolute.** A game built on the engine changes its working directory at startup
   (`AnchorAssetRoot`), so every `--screenshot` path must be absolute. The original and game/data are
   found at run time (game/eth/Paths.hpp): `--original`/`--data`, then `original/` and `data/` beside
   the exe (a package, tools/package.bat), then the baked PENUMBRA_ORIGINAL_DIR / PENUMBRA_DATA_DIR.
5. **Smart App Control is enforcing on this laptop.** A freshly linked exe is sometimes refused
   ("An Application Control policy has blocked this file", exit 126, ctest "Not Run") and EVERY
   refusal pops a notification for Ivan. Launch each suite/exe at most once per build; never loop
   ctest or relink-and-retry; report a refused exe as "not run". `ctest --test-dir build -N` lists
   without launching. Every executable gets `penumbra_windows_resources` (icon, VERSIONINFO,
   manifest): refusals stopped once they carried them.
6. **Builds are serialised.** Only one agent builds at a time, in the one `build/` directory:
   `cmd //c "tools\build.bat --target <T>"` from Git Bash at the repo root. Zero warnings (/W4).
   Parallel agents write code; an integration step builds and runs.
7. **The tick.** One Ethanon frame per 60 Hz engine tick. `GetTime()` is simulated ms, one
   app-lifetime uint32 counter never reset by a scene load; `UnitsPerSecond(x) = x/60` (0 on the
   first tick after a scene load); `GetFPSRate() = 60`. No wall clock, no libm-dependent RNG in the
   tick: use the Eth layer's seeded RNG and `DetMath` where results must reproduce.
8. **Text is cp1252.** The original's strings are Windows-1252 bytes. Keep them as bytes in the
   ported code (write non-ASCII in C++ literals as `\xE3` escapes); translate at the draw boundary
   (`game/data/strings.json`, keyed by the original string) and convert to UTF-8 only for logs.
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
```

The original runs from a scratch copy (never from `extracted/app`, which it would write into):
`reference/analysis/orig_rig.ps1` drives a copy of `machine.exe` (start / shot / keys / stop). An
HKCU AppCompatFlags `HIGHDPIAWARE` entry is set for that scratch exe only.
