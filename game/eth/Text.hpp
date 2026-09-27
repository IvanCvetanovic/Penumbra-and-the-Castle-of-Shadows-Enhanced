#pragma once

// The original's text is Windows-1252 bytes (not Latin-1: menu.as and switch.as
// hold 0x95 bullets). It stays bytes everywhere in the port; these convert at
// the edges - logs, the font rasteriser, JSON.

#include <string>

#include "eth/EthTypes.hpp"

namespace Penumbra::Eth {

// cp1252 bytes -> UTF-8. Bytes cp1252 leaves undefined (0x81, 0x8D, 0x8F,
// 0x90, 0x9D) map to U+FFFD.
string Cp1252ToUtf8(const string& text);
// UTF-8 -> cp1252; characters cp1252 cannot hold become '?'.
string Utf8ToCp1252(const string& text);
// One cp1252 byte -> its Unicode code point.
unsigned Cp1252CodePoint(unsigned char byte);

} // namespace Penumbra::Eth
