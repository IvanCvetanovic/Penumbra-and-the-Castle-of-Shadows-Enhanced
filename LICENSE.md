# Licences

This repository holds several things under different terms.

| What | Where | Terms |
|---|---|---|
| The original game, *Penumbra e o Castelo das Sombras* (2010), by Andre Santee / Asantee: its art, sounds, scenes, data and scripts | `extracted/`, `penumbra_setup.exe` | The original authors'. Remade here with their permission, given to Ivan Cvetanovic; its terms are his to state. |
| The original's scripts' own licence | `extracted/app/*.as` (header) | LGPL-3.0-or-later |
| The port of those scripts to C++ | `game/script/` | LGPL-3.0-or-later, as a work derived from them |
| The emulation of the Ethanon 0.7.12 runtime, parts of it ported from Ethanon's source (e.g. `ETHParticleManager`, `enml.h`) | `game/eth/` | LGPL-3.0-or-later, as a work derived from Ethanon (LGPL) |
| TinyXML 2.5 | `game/third_party/tinyxml/` | zlib (notice in each file) |
| dr_mp3 (David Reid, based on minimp3), for MP3 where the engine has no decoder | `game/third_party/dr_mp3/` | Public domain (Unlicense) or MIT No Attribution, the user's choice (end of `dr_mp3.h`) |
| Liberation Sans Bold 2.1.5, stand-in for Arial and Arial Narrow | `game/data/fonts/LiberationSans-Bold.ttf` | SIL Open Font License 1.1 (`game/data/fonts/LICENSE-LiberationFonts.txt`) |
| DejaVu Sans Bold 2.37, stand-in for Arial Black and Verdana | `game/data/fonts/DejaVuSans-Bold.ttf` | Bitstream Vera and Arev font licences, DejaVu changes public domain (`game/data/fonts/LICENSE-DejaVuFonts.txt`) |
| The Supersonic Engine | `engine/` (submodule) | MIT, see `engine/LICENSE` and `engine/THIRD_PARTY_LICENSES.md` |
| Everything else written for this remake (`game/render/`, the layer, tools, tests, docs, the generated English art) | | © Ivan Cvetanovic. No licence chosen yet - that is his decision. |

The GNU licence texts are in `licenses/LGPL-3.0.txt` and `licenses/GPL-3.0.txt` (the LGPL is a set
of additional permissions on top of the GPL, so both are needed). A packaged build
(`tools/package.bat`) carries them in its `licenses/` folder.

Fonts: on Windows the game draws text with the Microsoft fonts installed there (Arial, Arial
Narrow, Arial Black, Verdana) and never ships them. Where a face is missing, and on every other
platform, it draws two open-licence stand-ins bundled in `game/data/fonts/`. That folder's
README.md gives their sources, versions and checksums, and each licence text sits beside its font
there. A packaged build carries that folder whole. The generated English menu art was rendered from
Courier New and Matura MT Script Capitals; the images, not the fonts, are in the repository.
