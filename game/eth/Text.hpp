#pragma once

// The original's text is Windows-1252 bytes (not Latin-1: menu.as and switch.as
// hold 0x95 bullets). It stays bytes everywhere in the port; these convert at
// the edges - logs, the font rasteriser, JSON.
//
// ONE PRIVATE BYTE (ENHANCEMENT E21). cp1252 has no c with acute, and the
// enhanced edition's credit ends its name in U+0107 (decided: with the
// diacritic, "Cvetanovi" + c-acute). 0x8D, one of the five bytes cp1252 leaves undefined, is the
// port's own code for U+0107, both ways: a script literal writes it as \x8D,
// strings.json writes the character itself (the loader converts it to 0x8D),
// FontAtlas draws the font's U+0107 glyph for it and a log shows U+0107. None
// of the original's 261 text files (.as .enml .esc .ent .par .txt .cg .ethproj) holds 0x8D or any
// other undefined byte, so nothing the original draws changes; the other four
// (0x81, 0x8F, 0x90, 0x9D) stay undefined. Typed characters are not text the
// port draws: InputMapper::ToCp1252 keeps its own table, so a typed U+0107 is
// '?' as an ANSI window's WM_CHAR made it.
//
// BEYOND CP1252 (ENHANCEMENT E24). The nine languages E24 adds are not all
// cp1252 (Turkish, Cyrillic, Japanese, Arabic): their translations stay UTF-8
// from game/data/strings/<id>.json to the font, which lays out code points.
// The scripts' own text is still cp1252; only the draw boundary changes.

#include <string>

#include "eth/EthTypes.hpp"

namespace Penumbra::Eth {

// E21: the port's byte for U+0107 (LATIN SMALL LETTER C WITH ACUTE).
constexpr unsigned char kCAcuteByte = 0x8D;
constexpr unsigned kCAcuteCodePoint = 0x0107u;

// cp1252 bytes -> UTF-8. The four bytes cp1252 leaves undefined and the port
// does not use (0x81, 0x8F, 0x90, 0x9D) map to U+FFFD; 0x8D is U+0107 (E21).
string Cp1252ToUtf8(const string& text);
// UTF-8 -> cp1252; characters cp1252 cannot hold become '?', but for U+0107,
// which becomes 0x8D (E21).
string Utf8ToCp1252(const string& text);
// One cp1252 byte -> its Unicode code point (0x8D: U+0107, E21).
unsigned Cp1252CodePoint(unsigned char byte);
// E24: the cp1252 byte whose Cp1252CodePoint is `codePoint` (the lowest such
// byte for U+FFFD); false when cp1252 has none. U+0080..U+009F are C1
// controls, not the cp1252 characters at those bytes, and have none.
bool Cp1252ByteOf(unsigned codePoint, unsigned char& byte);
// E24: UTF-8 -> code points, a stray or truncated sequence as one U+FFFD (the
// text that follows it survives); and back.
std::u32string Utf8ToCodePoints(const string& text);
string CodePointsToUtf8(const std::u32string& text);

} // namespace Penumbra::Eth
