// Image dimensions from headers, the format told by the bytes and never by the
// name: entities/normalmaps/nm_white_ground.jpg is a BMP, and D3DX (which
// loaded the original's textures) sniffed content the same way.
//
// PNG, JPEG, BMP and DDS are recognised by their signatures; TGA has none and is
// accepted only when its header is self-consistent. Anything else is offered to
// stb_image's stbi_info_from_memory (the engine links its implementation).

#include "eth/ImageInfo.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <vector>

#include <stb_image.h>

namespace Penumbra::Eth {
namespace {

using Bytes = std::vector<unsigned char>;

struct Size {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

std::uint32_t Be16(const Bytes& b, const std::size_t at) {
    return (std::uint32_t(b[at]) << 8) | b[at + 1];
}
std::uint32_t Be32(const Bytes& b, const std::size_t at) {
    return (std::uint32_t(b[at]) << 24) | (std::uint32_t(b[at + 1]) << 16) | (std::uint32_t(b[at + 2]) << 8) |
           b[at + 3];
}
std::uint32_t Le16(const Bytes& b, const std::size_t at) {
    return std::uint32_t(b[at]) | (std::uint32_t(b[at + 1]) << 8);
}
std::uint32_t Le32(const Bytes& b, const std::size_t at) {
    return std::uint32_t(b[at]) | (std::uint32_t(b[at + 1]) << 8) | (std::uint32_t(b[at + 2]) << 16) |
           (std::uint32_t(b[at + 3]) << 24);
}

// Signature, then IHDR's width and height (big-endian) as the first chunk.
bool PngSize(const Bytes& b, Size& out) {
    static constexpr unsigned char kSignature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (b.size() < 24) return false;
    for (std::size_t i = 0; i < 8; ++i) {
        if (b[i] != kSignature[i]) return false;
    }
    if (b[12] != 'I' || b[13] != 'H' || b[14] != 'D' || b[15] != 'R') return false;
    out.width = Be32(b, 16);
    out.height = Be32(b, 20);
    return true;
}

// Walks the marker segments to the first start-of-frame: SOF0 (baseline) and
// SOF2 (progressive: face.jpg, face_norm.jpg) and the rarer SOFn, which all
// share the layout. C4 (DHT), C8 (JPG) and CC (DAC) are in that range and are
// not frames.
bool JpegSize(const Bytes& b, Size& out) {
    if (b.size() < 4 || b[0] != 0xFF || b[1] != 0xD8) return false;
    std::size_t at = 2;
    while (at + 1 < b.size()) {
        if (b[at] != 0xFF) return false;
        const unsigned char marker = b[at + 1];
        if (marker == 0xFF) {       // fill byte
            ++at;
            continue;
        }
        if (marker == 0x01 || marker == 0xD8 || (marker >= 0xD0 && marker <= 0xD7)) {
            at += 2;                // markers without a length
            continue;
        }
        if (marker == 0xD9 || marker == 0xDA) return false;   // end of image / scan before any frame
        if (at + 4 > b.size()) return false;
        const std::size_t length = Be16(b, at + 2);
        const bool frame = marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC;
        if (frame) {
            if (length < 7 || at + 9 > b.size()) return false;
            out.height = Be16(b, at + 5);
            out.width = Be16(b, at + 7);
            return true;
        }
        if (length < 2) return false;
        at += 2 + length;
    }
    return false;
}

// BITMAPFILEHEADER then the DIB header: OS/2's 12-byte core header holds 16-bit
// sizes; every later one (40, 52, 56, 108, 124 bytes; OS/2 2.x's 16 and 64)
// holds signed 32-bit sizes, the height negative for a top-down image. The
// bit depth (the 8-bit paletted halos, the 32-bit nm_white_ground) does not
// move them.
bool BmpSize(const Bytes& b, Size& out) {
    if (b.size() < 26 || b[0] != 'B' || b[1] != 'M') return false;
    const std::uint32_t dibSize = Le32(b, 14);
    if (dibSize == 12) {
        out.width = Le16(b, 18);
        out.height = Le16(b, 20);
        return true;
    }
    if (dibSize < 16) return false;
    const auto width = static_cast<std::int32_t>(Le32(b, 18));
    const auto height = static_cast<std::int32_t>(Le32(b, 22));
    if (width <= 0 || height == std::numeric_limits<std::int32_t>::min()) return false;
    out.width = static_cast<std::uint32_t>(width);
    out.height = static_cast<std::uint32_t>(height < 0 ? -height : height);
    return true;
}

// "DDS " then DDS_HEADER (dwSize 124): height at byte 12, width at 16.
bool DdsSize(const Bytes& b, Size& out) {
    if (b.size() < 128 || b[0] != 'D' || b[1] != 'D' || b[2] != 'S' || b[3] != ' ') return false;
    if (Le32(b, 4) != 124) return false;
    out.height = Le32(b, 12);
    out.width = Le32(b, 16);
    return true;
}

// TGA has no signature. The header must describe a real image: a colour-map
// flag of 0 or 1 matching the image type, a known pixel depth, non-zero sizes,
// and a file long enough to hold the header and the ID field.
bool TgaSize(const Bytes& b, Size& out) {
    if (b.size() < 18) return false;
    const unsigned idLength = b[0];
    const unsigned colorMapType = b[1];
    const unsigned imageType = b[2];
    const unsigned depth = b[16];
    const bool mapped = imageType == 1 || imageType == 9;
    const bool unmapped = imageType == 2 || imageType == 3 || imageType == 10 || imageType == 11;
    if (colorMapType > 1 || !(mapped || unmapped)) return false;
    if (mapped && colorMapType != 1) return false;
    if (depth != 8 && depth != 15 && depth != 16 && depth != 24 && depth != 32) return false;
    if (b.size() < 18 + idLength) return false;
    const std::uint32_t width = Le16(b, 12);
    const std::uint32_t height = Le16(b, 14);
    if (width == 0 || height == 0) return false;
    out.width = width;
    out.height = height;
    return true;
}

bool StbSize(const Bytes& b, Size& out) {
    if (b.empty() || b.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return false;
    int width = 0;
    int height = 0;
    int components = 0;
    if (!stbi_info_from_memory(b.data(), static_cast<int>(b.size()), &width, &height, &components)) return false;
    if (width <= 0 || height <= 0) return false;
    out.width = static_cast<std::uint32_t>(width);
    out.height = static_cast<std::uint32_t>(height);
    return true;
}

} // namespace

vector2 ReadImageSize(const string& absolutePath) {
    std::ifstream file(std::filesystem::path(absolutePath), std::ios::binary);
    if (!file) return vector2(0.0f);
    const Bytes bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    Size size;
    const bool known = PngSize(bytes, size) || JpegSize(bytes, size) || BmpSize(bytes, size) ||
                       DdsSize(bytes, size) || TgaSize(bytes, size) || StbSize(bytes, size);
    if (!known || size.width == 0 || size.height == 0) return vector2(0.0f);
    return vector2(static_cast<float>(size.width), static_cast<float>(size.height));
}

} // namespace Penumbra::Eth
