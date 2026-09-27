# Penumbra on Supersonic — the port's step record

Started 2026-09-27. Ivan asked for Penumbra e o Castelo das Sombras to be remade on his own engine,
in this repository, with the engine improved where the game needs it.

## Rulings (Ivan, 2026-09-27)

- **R1 Enhanced from the start.** The original is the base; visuals, controls, resolution and balance
  may be modernised freely. The scripts remain the gameplay spec.
- **R2 Portuguese + English**, selectable in the game.
- **R3 The original may be run** (from a scratch copy) for reference captures.
- **R4 Assets are read in place** from `extracted/app` (committed; the repository is private). No
  converted copies are committed.
- **R5 Commits as Ivan, no AI trailers**, here and in the engine; checkpoints at Claude's discretion.
- **R6 A second Claude session (Magic Portals) also edits Supersonic.** Engine changes are additive,
  made from this repo's `engine/`, rebased before push, announced to that session. Agreed with it on
  2026-09-27: keep existing enum values and their order (append new ones).

## Architecture (decided 2026-09-27, see CLAUDE.md)

- `game/eth` — the Ethanon 0.7.12 runtime the scripts ran on, emulated game-side: files, buckets,
  custom data, callbacks, the frame order, particles, input, samples. One Ethanon frame per 60 Hz
  engine tick. Produces a `RenderSnapshot` per frame.
- `game/script` — the 25 `.as` files ported to C++ close to line by line against `eth/Eth.hpp`.
- `game/render` + `PenumbraLayer` — the snapshot drawn with the engine: sprite quads, Light2D with
  normal maps, projected shadows, particles as pooled quads, HUD via ScreenOverlay, TrueType text.
- Alternatives rejected: embedding AngelScript and running the original scripts (exact, but no room
  for the enhancements Ivan asked for and no C++ to test); a data-only rewrite in the Magic Portals
  style (the scripts ARE the spec here, so re-deriving them would only add error).

## Enhancements (planned; each switchable where it changes gameplay)

| # | Enhancement | Original |
|---|---|---|
| E1 | Widescreen: logical view 768 px tall, width by aspect; menus pillarboxed at 1024x768 | 1024x768 only |
| E2 | Any window size / fullscreen, rendered at native resolution | 1024x768 or listed video modes |
| E3 | Modern gamepads (XInput via GLFW) mapped by meaning, for both players | winmm button numbers |
| E4 | Keyboard second player (presented to the scripts as joystick 1) | P2 needs a joystick |
| E5 | English text alongside Portuguese, switchable | Portuguese only |
| E6 | Settings persisted (language, window, volumes, controls) | options reset every launch |
| E7 | Original bugs fixed where they are plainly bugs (listed per step) | — |
| E8 | Smoother presentation: interpolation between ticks at high refresh rates | vsync 60 Hz |

## Steps

### Step 0 — scaffold (done 2026-09-27)
Engine submodule at 4bfcf67; CMake (engine subproject, game/eth, game/script, PenumbraGame, tests);
tools/build.bat; CLAUDE.md; docs/spec (eight mapping reports + synthesis); reference/ (gitignored
Ethanon 0.7.12 source, GS2D r485, disassembly, scripts); the Eth contract headers.

### Step 1 — the Eth runtime (done 2026-09-27, 9c64f8a)
game/eth implemented from reference/eth-0.7.12: TinyXML legacy readers/writer (all 13 scenes rewrite
byte-exact), ENML (enml.h's missing-key rule), buckets, typed custom data, soft delete, callbacks by
name, the 0.7.12 frame order, the HUD queue, drawHash order, ETHParticleManager, input state
machines, the sample bank, the snapshot. Gates: test_pn_runtime 277, test_pn_formats 589,
test_pn_particles 81 checks, 0 failures.

### Step 2 — the scripts (done 2026-09-27, 9c64f8a)
All 25 .as files ported to C++ (game/script), each adversarially reviewed against its .as: one
discrepancy found (interface.as's float divisions must abort on 0, as AngelScript's asBC_DIVf did).
Gate: test_pn_boot 46 checks - the real game boots headless, menu -> level1, the wizard spawns,
walks 150 px/s, jumps 111 px for 50 ticks, his sword lives 20 ticks, 0 script aborts.
- **E7 (first bug fix):** data.enml has no lv20, and the original read the thresholds directly, so a
  player skipped level 20 and, past lv30, addToExp never returned. expForLevel (Script.hpp) falls back
  to the last level that has a threshold; drawPlayerStatus uses it too.

### Step 3 — drawing it (done 2026-09-27)
game/render: TextureCache (colour keys, DDS, renormalised normal maps, additive/modulate variants),
DrawOrder (one painter's order from 0.7.12's depths), CameraRig (ortho camera, DisplayEncoded,
pillarbox), SpriteRenderer, Lighting + LightRenderer (every light live through Light2D, halos,
torch flicker), ShadowRenderer (the projected strips of dynaShadowVS), ParticleRenderer, FontAtlas
(system TrueType faces, bold, GDI cell height), HudRenderer, Localization + game/data/strings.json
(English), InputMapper (gamepads by meaning, keyboard player 2), AudioOutEngine, Settings. Engine
abb9e8a: Ogg Vorbis (e955663 pins it). PenumbraLayer wires them; main.cpp takes --start, --hold,
--lang, --widescreen.
Gates: build zero warnings; test_pn_render_textures 144, _particles 137, _hud 258, _input 166,
test_pn_audio 177 (all 35 sounds decode), test_pn_render_lights 132 checks with 3 failures (shadow
edge cases on a bare registry; open). First headless level1 frame compared with
reference/captures/05_level1_after14s.png: walls, torches, crystals, HUD and English text in place.
- **Smart App Control.** Freshly linked executables with no version resource were refused at random
  (Penumbra.exe twice in a row, two suites). With game/windows' icon, VERSIONINFO and manifest on
  every executable (penumbra_windows_resources), the next launches of all of them were accepted.
