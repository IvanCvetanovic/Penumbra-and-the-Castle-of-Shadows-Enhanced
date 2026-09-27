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
