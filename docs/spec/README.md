# docs/spec — what the original is and does

Decoded on 2026-09-27 by eight parallel readers plus a completeness critic, from the original's
AngelScript source (`extracted/app/*.as`), its data, the Ethanon 0.7.12 engine source the game
shipped on, disassembly of `machine.exe` / `GameSpace.dll`, and the Supersonic engine at 4bfcf67.

| File | What |
|---|---|
| `10-logic-flow-ui.md` | boot, menu, scenes, setupScene/doLoop, checkpoint, lives, HUD, messages, camera, environment, scores |
| `11-logic-player-combat.md` | movement, physics, animation, input, combos, swords, spells, potions, damage, XP, death, PvP |
| `12-logic-ai-world.md` | every ETHCallback_*, enemy AI, bosses, hazards, lava shooters, falling bridges, util.as |
| `20-formats-scene-entity.md` | .esc/.ent/.enml schema and census of all 13 scenes and 144 entities |
| `21-formats-particles-shaders.md` | .par particles, the 2010 Cg lighting/shadow/lightmap math |
| `30-ethanon-runtime.md` | the 0.7.12 frame loop, buckets, ids, callbacks, custom data, draw order, blend modes, input, audio |
| `40-engine-capabilities.md` | what Supersonic offers and lacks for this game |
| `41-magic-portals-reuse.md` | what the Magic Portals port (also Ethanon) solved that applies here |
| `42-magic-rampage-screenpad.md` | what Magic Rampage 7.8.7's screen that adjusts the on-screen pad does (size, transparency, lock, move, restore, storage), decoded on 2026-10-01 from its Android package as the reference for E28; its paths are inside that package, not in this repository |
| `90-synthesis.md` | corrections to the above, gaps, engine-gap list, module breakdown and build order |

**Precedence.** Where these files disagree with each other, `90-synthesis.md`'s corrections win;
where anything here disagrees with the original's files or `reference/eth-0.7.12`, the original wins.
Known correction: text is Windows-1252, not Latin-1 (bytes 0x95 in menu.as/switch.as).

**Citation keys.** `A:` or a bare `file.as:line` = `extracted/app/`; `E:` = `reference/eth-0.7.12/src/`;
`G:` = `reference/gs2d-r485/`; `DLL@0x…` = a virtual address in `extracted/app/GameSpace.dll`
(listing: `reference/analysis/gs.asm`); `reference/analysis/*.py` are the census scripts. Engine
citations are relative to `engine/`. `reference/` is gitignored and lives on the development machine only; it
is recreated from SourceForge SVN (`https://svn.code.sf.net/p/ethanon/code/tags/v0-7-12`, via
`reference/analysis/svncrawl.py`).

**Superseded by the project's decisions** (see `CLAUDE.md`): the reports assume a faithful 1024x768 port;
the remake is *enhanced from the start* — widescreen, gamepads, English — so the reports' parity
details are the baseline the enhancements depart from on purpose, not a mandate.
