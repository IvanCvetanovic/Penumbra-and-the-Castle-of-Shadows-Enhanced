# Bundled fonts: stand-ins for the Windows faces, and the Japanese and Arabic faces

The original drew its text with GDI in four Microsoft faces (Arial Narrow, Arial, Arial Black,
Verdana, all bold). Those exist only on Windows and may not be redistributed, so the port reads them
from `%WINDIR%\Fonts` when they are there and otherwise draws these open-licence stand-ins with the
Windows face's metrics (`game/render/FontAtlas.hpp` says how). Off Windows they are the only fonts
the game uses. The Japanese and Arabic faces of ENHANCEMENT E24, subsets of Noto, are at the end.

| File | Stands in for | Version | Licence |
|---|---|---|---|
| `LiberationSans-Bold.ttf` | Arial Bold; Arial Narrow Bold (advances x 0.82) | Liberation Fonts 2.1.5 | SIL Open Font License 1.1 (`LICENSE-LiberationFonts.txt`) |
| `DejaVuSans-Bold.ttf` | Arial Black; Verdana Bold | DejaVu Fonts 2.37 | Bitstream Vera licence + Arev licence, DejaVu changes public domain (`LICENSE-DejaVuFonts.txt`) |

Both stand-ins are unmodified copies from the upstream releases. The Arial Narrow stand-in is condensed
at run time, not in the file, so no renamed derivative is needed under either licence. Both licences
allow the fonts to be bundled and redistributed with software, including sold as part of a larger
package, but not sold on their own. Each licence text must travel with its font file. `tools/package.bat`
copies this whole folder.

## Where they came from (downloaded 2026-09-28)

- **Liberation Fonts 2.1.5** (released 2021-09-30), https://github.com/liberationfonts/liberation-fonts/releases/tag/2.1.5
  - archive: https://github.com/liberationfonts/liberation-fonts/files/7261482/liberation-fonts-ttf-2.1.5.tar.gz
    (sha256 `7191c669bf38899f73a2094ed00f7b800553364f90e2637010a69c0e268f25d0`)
  - `LiberationSans-Bold.ttf` sha256 `788abee4c806d660e8aee46689dd8540cd4bb98da03dcc9d171ce3efd99a9173`
    (name table: "Liberation Sans Bold", "Version 2.1.5", "Licensed under the SIL Open Font License, Version 1.1")
  - `LICENSE-LiberationFonts.txt` is the archive's `LICENSE`, verbatim.
- **DejaVu Fonts 2.37** (released 2016-07-30), https://github.com/dejavu-fonts/dejavu-fonts/releases/tag/version_2_37
  - archive: https://github.com/dejavu-fonts/dejavu-fonts/releases/download/version_2_37/dejavu-fonts-ttf-2.37.tar.bz2
    (sha256 `fa9ca4d13871dd122f61258a80d01751d603b4d3ee14095d65453b4e846e17d7`)
  - `DejaVuSans-Bold.ttf` sha256 `e6476c1b80502924294eed40894c5b18e06c181444ca953e5334262df9c27724`
    (name table: "DejaVu Sans Bold", "Version 2.37")
  - `LICENSE-DejaVuFonts.txt` is the archive's `LICENSE`, verbatim.

## Not used, and why

- **Liberation Sans Narrow** (liberationfonts/liberation-sans-narrow 1.07.6) matches Arial Narrow
  exactly, but it is licensed GPLv2 with a font exception and a further clause about source access,
  not under the OFL. Liberation Fonts 2.x dropped the Narrow cut for that reason. LiberationSans-Bold's
  advances x 0.82 are ARIALNB.TTF's to within 1/2048 em on every glyph checked, because Arial Narrow
  is Arial condensed to 82%.

## How close they are (measured with fontTools against Windows 11's files)

- LiberationSans-Bold vs arialbd.ttf 7.06: identical advances, usWinAscent/usWinDescent 1854/434 on
  both.
- LiberationSans-Bold x 0.82 vs ARIALNB.TTF 2.40: advances equal to within 1 unit in 2048 for every
  cp1252 character except six the game never draws: no-break space, macron, degree, plus-minus,
  micro and division sign. Laid out with FontAtlas's whole-pixel advances, the game's lines ("hp:
  100", "Carregando...", "Pressione Alt+Enter para trocar entre fullscreen e modo janela", ...)
  come out exactly as wide as ARIALNB's at 16, 30 and 40 px. The cell is taken from ARIALNB:
  1910/431, xAvgCharWidth 803.
- DejaVuSans-Bold vs ariblk.ttf 5.23: advances 0.89-1.20x, 1.03x on average. The cell is taken from
  ariblk: 2254/634, xAvgCharWidth 1131.
- DejaVuSans-Bold vs verdanab.ttf 5.33: advances 0.96-1.05x, 1.00x on average. The cell is taken
  from verdanab: 2059/430, xAvgCharWidth 1163.

<!-- make_fonts.py: begin -->
## Script faces (ENHANCEMENT E24): Japanese and Arabic

Written by `tools/l10n/make_fonts.py` (rerun it whenever `game/data/strings/ja.json` changes; this section is its own and is rewritten). `game/render/FontAtlas` draws hiragana, katakana, CJK ideographs, CJK punctuation and the full-width forms from the first, Arabic from the second, on every platform, scaled to the em of the line's face. Each is an OFL Modified Version of the upstream file - a subset, instanced at bold - under its own name (neither upstream declares a Reserved Font Name the file uses), with its licence beside it.

| File | Source | Made | Licence |
|---|---|---|---|
| `NotoSansJP-Bold.ttf` | google/fonts `ofl/notosansjp/NotoSansJP[wght].ttf` at commit `66a36c8c94b1a5d992ee4e7f392fccfe4945767c` (sha256 `c2f3b4d463500a2ddcd3849cded1fceeb9fd6d1c32e6cbecd568453ba50fc68f`) | subset + instance (wght 700): 934 characters, 932 glyphs, 161,696 bytes, sha256 `881d6c6e15382e1c31eb59f7a73c47ff634fab7716ba3a6cf1d2eb189f067789` | SIL Open Font License 1.1 (`LICENSE-NotoSansJP.txt`) |
| `NotoSansArabic-Bold.ttf` | google/fonts `ofl/notosansarabic/NotoSansArabic[wdth,wght].ttf` at commit `66a36c8c94b1a5d992ee4e7f392fccfe4945767c` (sha256 `63111b5b2e074dd48cc67692e0a2726d86ee94c1c37fe8598257b7b4e87e869e`) | subset + instance (wght 700, wdth 100): 1171 characters, 1161 glyphs, 96,724 bytes, sha256 `f53dc0274d87962a931c21d995970a74b68c5958930c11bf85c143bffe598e7e` | SIL Open Font License 1.1 (`LICENSE-NotoSansArabic.txt`) |

Japanese characters from the language files (ar.json, de.json, es.json, fr.json, it.json, ja.json, ru.json, tr.json, uk.json) and the language names: 369, of them 250 kanji.
Made with fontTools 4.63.0.
<!-- make_fonts.py: end -->
