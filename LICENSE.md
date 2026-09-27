# Licences

This repository holds several things under different terms.

| What | Where | Terms |
|---|---|---|
| The original game, *Penumbra e o Castelo das Sombras* (2010), by Andre Santee / Asantee: its art, sounds, scenes, data and scripts | `extracted/`, `penumbra_setup.exe` | The original authors'. Remade here with their permission, given to Ivan Cvetanovic; its terms are his to state. |
| The original's scripts' own licence | `extracted/app/*.as` (header) | LGPL-3.0-or-later |
| The port of those scripts to C++ | `game/script/` | LGPL-3.0-or-later, as a work derived from them |
| The emulation of the Ethanon 0.7.12 runtime, parts of it ported from Ethanon's source (e.g. `ETHParticleManager`, `enml.h`) | `game/eth/` | LGPL-3.0-or-later, as a work derived from Ethanon (LGPL) |
| TinyXML 2.5 | `game/third_party/tinyxml/` | zlib (notice in each file) |
| The Supersonic Engine | `engine/` (submodule) | MIT, see `engine/LICENSE` and `engine/THIRD_PARTY_LICENSES.md` |
| Everything else written for this remake (`game/render/`, the layer, tools, tests, docs, the generated English art) | | © Ivan Cvetanovic. No licence chosen yet - that is his decision. |

The GNU licence texts are in `licenses/LGPL-3.0.txt` and `licenses/GPL-3.0.txt` (the LGPL is a set
of additional permissions on top of the GPL, so both are needed). A packaged build
(`tools/package.bat`) carries them in its `licenses/` folder.

Fonts: the game draws text with fonts installed on the player's system (Arial, Arial Narrow,
Arial Black, Verdana) and never ships them. The generated English menu art was rendered from
Courier New and Matura MT Script Capitals; the images, not the fonts, are in the repository.
