#pragma once

// Image dimensions without decoding pixels, for GetSize(), frame rectangles
// and GetSpriteSize(). The FORMAT is sniffed from the content, not the name
// (entities/normalmaps/nm_white_ground.jpg is a BMP): PNG, JPEG (baseline and
// progressive), BMP, TGA and DDS (a 128-byte header, width and height at 16/12).

#include <string>

#include "eth/EthTypes.hpp"

namespace Penumbra::Eth {

// (0,0) when the file is missing or unreadable.
vector2 ReadImageSize(const string& absolutePath);

} // namespace Penumbra::Eth
