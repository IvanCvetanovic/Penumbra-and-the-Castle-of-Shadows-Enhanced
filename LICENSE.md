# Licences

## No warranty, no liability

Everything in this repository - the code, the data, the original game's files, the art, and any
build or package made from it - is provided **as is, without warranty of any kind**, express or
implied, including the warranties of merchantability, fitness for a particular purpose and
non-infringement. **Ivan Cvetanović is not responsible for anything that happens through its
use.** In no event shall he, or any other author or copyright holder of any part, be liable for
any claim, damages or other liability - including loss of data, damage to hardware, problems with
displays or drivers (the game can change a monitor's resolution and refresh rate), or any other
harm - whether in an action of contract, tort or otherwise, arising from, out of or in connection
with this repository or its use. You use it entirely at your own risk.

The licences of the individual parts carry their own disclaimers as well: MIT-0 (`LICENSE`), the
GNU LGPL and GPL (sections 15 and 16 of `licenses/GPL-3.0.txt`), and each third-party licence.

The top-level `LICENSE` file is the MIT No Attribution licence for the parts written by Ivan
Cvetanović for the enhanced edition; the table below says which parts those are and which parts are
under other terms.

This repository holds several things under different terms. In short: the code written for the
enhanced edition, and the Supersonic Engine, are free to use under the **MIT No Attribution**
licence (MIT-0), with no conditions; the port of the original's scripts and the Ethanon runtime
emulation stay **LGPL-3.0-or-later**, as the code they derive from is; the original game's files
and the Magic Rampage art **belong to their authors**.

| What | Where | Terms |
|---|---|---|
| Everything written for this enhanced edition by Ivan Cvetanovic: the drawing, input, audio and platform code (`game/render/`, `PenumbraLayer.*`, `main.cpp`, `game/android/`, `game/ios/`, `game/macos/`, `game/windows/`), the port's own data (`game/data/*.json`), the tools, the tests and the documentation | | **MIT No Attribution** (MIT-0), Copyright (c) 2026 Ivan Cvetanovic. The text is in [`LICENSE`](LICENSE) and at the end of this file. |
| The Supersonic Engine | `engine/` (git submodule) | **MIT No Attribution** (MIT-0), Copyright (c) 2026 Ivan Cvetanovic; see `engine/LICENSE`. Its vendored third-party components keep their own licences, listed in `engine/THIRD_PARTY_LICENSES.md`. |
| The Supersonic Engine's logo as a picture for the game's intro (E35): the engine's own `engine/assets/branding/supersonic-logo.svg`, drawn at 2x without its decorative bars and cropped around the lettering | `game/data/images/splash/supersonic-logo.png` | **MIT No Attribution** (MIT-0), Copyright (c) 2026 Ivan Cvetanovic, as the engine's branding is (`engine/LICENSE`). Not the original's art and not Magic Rampage's. `game/data/images/splash/README.md` says how it was made. |
| The port of the original's scripts to C++ | `game/script/` | LGPL-3.0-or-later, as a work derived from them |
| The emulation of the Ethanon 0.7.12 runtime, parts of it ported from Ethanon's source (e.g. `ETHParticleManager`, `enml.h`) | `game/eth/` | LGPL-3.0-or-later, as a work derived from Ethanon (LGPL) |
| The original game, *Penumbra and the Castle of Shadows* (2010), by André Santee / Asantee: its art, music, sounds, scenes, data, scripts and executables, and its installer | `extracted/`, `docs/original-installer/penumbra_setup.exe` | **The original authors' property.** No licence to them is granted here. |
| The original's scripts' own licence | `extracted/app/*.as` (header) | LGPL-3.0-or-later |
| The runtime libraries the original shipped with: D3DX9 (`d3dx9_42.dll`, Microsoft), NVIDIA Cg (`cg.dll`, `cgD3D9.dll`) and Audiere (`audiere.dll`, LGPL) | `extracted/app/` | Their makers' redistribution terms, included as the original's installer distributed them. The enhanced edition neither uses them nor puts them in its packages. |
| The English variants of the original's menu and interface images (the original's art with English lettering, made by `tools/art/make_english_art.py`) | `game/data/images/en/` | Derived from the original authors' art, which stays theirs. The script that makes them is MIT-0. |
| Magic Rampage's on-screen buttons (Magic Rampage 7.8.7, Asantee Games, from its Android package), some composed from its parts, for the touch controls | `game/data/images/touch/*.png` (not `placeholder/`), made by `tools/art/make_mr_touch_art.py` | **Asantee Games' property.** `game/data/images/touch/README.md` says which of its files each image is and what was composed. The placeholder buttons in `placeholder/` are Ivan's (MIT-0). |
| Magic Rampage's user-interface art (Magic Rampage 7.8.7, Asantee Games, from its Android package) for the options screen: the stone frames, check boxes, arrow buttons, speaker, music and gamepad icons, the touch-control editor's size, opacity, lock and restore buttons, some composed from its parts; and a globe and a monitor drawn for the same screens | `game/data/images/options/*.png`, made by `tools/art/make_options_art.py` | **Asantee Games' property**, except `globe.png` and `monitor.png`, which Ivan drew (MIT-0, with the script that draws them). `game/data/images/options/README.md` says which of its files each image is and what was composed or drawn. |
| TinyXML 2.6.1 | `game/third_party/tinyxml/` | zlib (notice in each file) |
| dr_mp3 (David Reid, based on minimp3), for MP3 where the engine has no decoder | `game/third_party/dr_mp3/` | Public domain (Unlicense) or MIT No Attribution, the user's choice (end of `dr_mp3.h`) |
| Liberation Sans Bold 2.1.5, stand-in for Arial and Arial Narrow | `game/data/fonts/LiberationSans-Bold.ttf` | SIL Open Font License 1.1 (`game/data/fonts/LICENSE-LiberationFonts.txt`) |
| DejaVu Sans Bold 2.37, stand-in for Arial Black and Verdana | `game/data/fonts/DejaVuSans-Bold.ttf` | Bitstream Vera and Arev font licences, DejaVu changes public domain (`game/data/fonts/LICENSE-DejaVuFonts.txt`) |
| Noto Sans JP Bold, a subset, the face that draws Japanese | `game/data/fonts/NotoSansJP-Bold.ttf` | SIL Open Font License 1.1 (`game/data/fonts/LICENSE-NotoSansJP.txt`) |
| Noto Sans Arabic Bold, a subset, the face that draws Arabic | `game/data/fonts/NotoSansArabic-Bold.ttf` | SIL Open Font License 1.1 (`game/data/fonts/LICENSE-NotoSansArabic.txt`) |

Where a file of Ivan's quotes, translates or shows the original (the English text in
`game/data/strings.json`, the specification in `docs/spec/`, the screenshots in `docs/images/`),
the MIT-0 grant covers his own work only. The original's words and art in it stay the authors',
and so do the Magic Rampage buttons in the touch-controls screenshot.

**A built game combines both.** `Penumbra` links the LGPL parts (`game/script/`, `game/eth/`) with
the MIT-0 parts and the engine. Anyone who distributes a build distributes those LGPL parts too, and
must meet the LGPL's conditions for them; the source is this repository. The GNU licence texts are
in `licenses/LGPL-3.0.txt` and `licenses/GPL-3.0.txt` (the LGPL is a set of additional permissions
on top of the GPL, so both are needed). A packaged build (`tools/package.bat`) carries them in its
`licenses/` folder.

**Fonts.** On Windows the game draws text with the Microsoft fonts installed there (Arial, Arial
Narrow, Arial Black, Verdana) and never ships them. Where a face is missing, and on every other
platform, it draws open-licence fonts bundled in `game/data/fonts/`: Liberation and DejaVu stand-ins for the Microsoft faces, and Noto subsets for the Japanese and Arabic text on every platform. That folder's
README.md gives their sources, versions and checksums, and each licence text sits beside its font
there. A packaged build carries that folder whole. The generated English menu art was rendered from
Courier New and Matura MT Script Capitals, and the other languages' from Aref Ruqaa, Kurale, Yuji Syuku and the Noto faces (their
licence texts are in `tools/art/fonts/`); the images, not those fonts, are in the repository.

## MIT No Attribution

```
MIT No Attribution

Copyright (c) 2026 Ivan Cvetanovic

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
