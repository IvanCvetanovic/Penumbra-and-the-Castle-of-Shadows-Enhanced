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
