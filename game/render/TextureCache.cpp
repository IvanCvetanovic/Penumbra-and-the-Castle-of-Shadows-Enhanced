#include "render/TextureCache.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <utility>

// Declarations only: the engine compiles stb_image's implementation once, in
// TextureRegistry.cpp, with every format on and no global flags set (nothing
// in the engine or the port calls stbi_set_flip_vertically_on_load).
#include <stb_image.h>

#include "core/Log.hpp"
#include "eth/Paths.hpp"
#include "render/TextureDecode.hpp"
#include "renderer/TextureRegistry.hpp"

namespace Penumbra::Render {

namespace {

// ---- file and byte helpers ---------------------------------------------------

bool ReadWholeFile(const std::string& path, std::vector<std::uint8_t>& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size <= 0) return false;
    file.seekg(0, std::ios::beg);
    out.resize(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(size));
    return static_cast<bool>(file);
}

std::uint32_t LittleEndian32(const std::uint8_t* p) {
    return std::uint32_t{p[0]} | (std::uint32_t{p[1]} << 8) | (std::uint32_t{p[2]} << 16) |
           (std::uint32_t{p[3]} << 24);
}

std::uint16_t LittleEndian16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(std::uint32_t{p[0]} | (std::uint32_t{p[1]} << 8));
}

std::string NormalisedPath(const std::string& path) {
    std::string out = path;
    std::replace(out.begin(), out.end(), '\\', '/');
    return out;
}

const char* VariantName(TextureVariant variant) {
    switch (variant) {
    case TextureVariant::Sprite: return "sprite";
    case TextureVariant::Halo: return "halo";
    case TextureVariant::Additive: return "additive";
    case TextureVariant::Plain: return "plain";
    case TextureVariant::Normal: return "normal";
    case TextureVariant::Modulate: return "modulate";
    }
    return "plain";
}

// ---- DDS ---------------------------------------------------------------------
//
// The DirectDraw Surface header as D3DX wrote it: "DDS ", then a 124-byte
// DDS_HEADER whose pixel format block starts at byte 76 of the file. Only the
// uncompressed layouts are read - the port ships no compressed DDS, and a
// FourCC file is refused by name rather than decoded as garbage.

constexpr std::size_t kDdsHeaderBytes = 128;
constexpr std::uint32_t kDdsdPitch = 0x8;
constexpr std::uint32_t kDdpfAlphaPixels = 0x1;
constexpr std::uint32_t kDdpfAlpha = 0x2;
constexpr std::uint32_t kDdpfFourCc = 0x4;
constexpr std::uint32_t kDdpfRgb = 0x40;
constexpr std::uint32_t kDdpfLuminance = 0x20000;
// Far past anything the original ships (its largest image is 1024 wide), and
// small enough that width * height * 4 cannot overflow a size_t on any target.
constexpr std::uint32_t kMaxDimension = 16384;

bool IsDds(const std::uint8_t* data, std::size_t size) {
    return size >= 4 && std::memcmp(data, "DDS ", 4) == 0;
}

// One channel of a masked pixel, widened to 8 bits the way D3DX expands a
// narrower channel: scaled so the mask's maximum is 255. An 8-bit mask - every
// channel of every shipped DDS - comes through unchanged.
struct MaskedChannel {
    std::uint32_t mask = 0;
    int shift = 0;
    std::uint32_t maximum = 0;

    explicit MaskedChannel(std::uint32_t m) : mask(m) {
        if (mask == 0) return;
        shift = std::countr_zero(mask);
        maximum = mask >> shift;
    }

    std::uint8_t Read(std::uint32_t value, std::uint8_t absent) const {
        if (mask == 0) return absent;
        const std::uint64_t raw = (value & mask) >> shift;
        return static_cast<std::uint8_t>((raw * 255u + maximum / 2u) / maximum);
    }
};

bool DecodeDds(const std::uint8_t* data, std::size_t size, DecodedImage& out, std::string& error) {
    if (size < kDdsHeaderBytes) {
        error = "is a DDS shorter than its header";
        return false;
    }
    const std::uint32_t headerFlags = LittleEndian32(data + 8);
    const std::uint32_t height = LittleEndian32(data + 12);
    const std::uint32_t width = LittleEndian32(data + 16);
    const std::uint32_t pitch = LittleEndian32(data + 20);
    const std::uint32_t formatFlags = LittleEndian32(data + 80);
    const std::uint32_t fourCc = LittleEndian32(data + 84);
    const std::uint32_t bitCount = LittleEndian32(data + 88);

    if ((formatFlags & kDdpfFourCc) != 0) {
        const char code[5] = {static_cast<char>(fourCc & 0xFFu), static_cast<char>((fourCc >> 8) & 0xFFu),
                              static_cast<char>((fourCc >> 16) & 0xFFu), static_cast<char>((fourCc >> 24) & 0xFFu),
                              '\0'};
        error = std::string("is a compressed DDS (") + code + "), which the port does not read";
        return false;
    }
    if (bitCount != 8 && bitCount != 16 && bitCount != 24 && bitCount != 32) {
        error = "is a DDS of " + std::to_string(bitCount) + " bits a pixel, which the port does not read";
        return false;
    }
    if (width == 0 || height == 0 || width > kMaxDimension || height > kMaxDimension) {
        error = "is a DDS of an unusable size";
        return false;
    }
    const bool luminance = (formatFlags & kDdpfLuminance) != 0;
    const bool rgb = (formatFlags & kDdpfRgb) != 0;
    const bool alphaOnly = !luminance && !rgb && (formatFlags & kDdpfAlpha) != 0;
    if (!luminance && !rgb && !alphaOnly) {
        error = "is a DDS in a pixel format the port does not read";
        return false;
    }

    const std::size_t bytesPerPixel = bitCount / 8u;
    const std::size_t rowBytes = static_cast<std::size_t>(width) * bytesPerPixel;
    // The pitch is honoured only when the header says it is there; D3DX wrote
    // the shipped files tightly packed with no pitch (flags 0x1007).
    const std::size_t stride = ((headerFlags & kDdsdPitch) != 0 && pitch >= rowBytes) ? pitch : rowBytes;
    const std::size_t needed = kDdsHeaderBytes + stride * (height - 1u) + rowBytes;
    if (size < needed) {
        error = "is a DDS cut short of its pixels";
        return false;
    }

    const MaskedChannel red(LittleEndian32(data + 92));
    const MaskedChannel green(LittleEndian32(data + 96));
    const MaskedChannel blue(LittleEndian32(data + 100));
    const MaskedChannel alpha(LittleEndian32(data + 104));
    const bool hasAlpha = (formatFlags & (kDdpfAlphaPixels | kDdpfAlpha)) != 0;

    out.width = static_cast<int>(width);
    out.height = static_cast<int>(height);
    out.rgba.assign(static_cast<std::size_t>(width) * height * 4u, 0);
    for (std::uint32_t y = 0; y < height; ++y) {
        const std::uint8_t* row = data + kDdsHeaderBytes + stride * y;
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::uint8_t* p = row + x * bytesPerPixel;
            std::uint32_t value = 0;
            for (std::size_t b = 0; b < bytesPerPixel; ++b) value |= std::uint32_t{p[b]} << (8u * b);

            std::uint8_t* texel = &out.rgba[(static_cast<std::size_t>(y) * width + x) * 4u];
            if (luminance) {
                // A8L8 (data/shadow.dds): L in the red mask, as D3DFMT_A8L8
                // samples - (L, L, L, A).
                const std::uint8_t l = red.Read(value, 0);
                texel[0] = texel[1] = texel[2] = l;
            } else if (rgb) {
                texel[0] = red.Read(value, 0);
                texel[1] = green.Read(value, 0);
                texel[2] = blue.Read(value, 0);
            } // D3DFMT_A8 samples (0, 0, 0, A): the zeros assign() left.
            texel[3] = hasAlpha ? alpha.Read(value, 255) : std::uint8_t{255};
        }
    }
    return true;
}

// ---- everything else, through stb_image ---------------------------------------

bool DecodeStb(const std::uint8_t* data, std::size_t size, DecodedImage& out, std::string& error) {
    if (size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        error = "is too large to decode";
        return false;
    }
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(data, static_cast<int>(size), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr || width <= 0 || height <= 0) {
        const char* reason = stbi_failure_reason();
        error = std::string("could not be decoded: ") + (reason != nullptr ? reason : "unknown format");
        if (pixels != nullptr) stbi_image_free(pixels);
        return false;
    }
    out.width = width;
    out.height = height;
    out.rgba.assign(pixels, pixels + static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
    stbi_image_free(pixels);

    // A 32-bit BI_RGB bitmap has no alpha channel to D3DX, which loads it as
    // X8R8G8B8; stb_image instead believes the fourth byte whenever any of them
    // is non-zero (entities/normalmaps/barril_nm.bmp carries 249..255 there).
    // Opaque, as the original drew it.
    if (size >= 34 && data[0] == 'B' && data[1] == 'M' && LittleEndian16(data + 28) == 32 &&
        LittleEndian32(data + 30) == 0) {
        for (std::size_t i = 3; i < out.rgba.size(); i += 4) out.rgba[i] = 255;
    }
    return true;
}

// ---- the variants ---------------------------------------------------------------

// D3DX's ColorKey is an exact ARGB compare, alpha included, done after the
// source is widened to ARGB (a format with no alpha reads as 0xFF), and a match
// becomes transparent BLACK, 0x00000000 (ETHResourceManager.cpp:106).
void ColourKey(std::vector<std::uint8_t>& rgba, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
        if (rgba[i] == r && rgba[i + 1] == g && rgba[i + 2] == b && rgba[i + 3] == 255) {
            rgba[i] = rgba[i + 1] = rgba[i + 2] = rgba[i + 3] = 0;
        }
    }
}

std::uint8_t ToByte(float unit) {
    return static_cast<std::uint8_t>(std::clamp(std::lround(unit * 255.0f), 0L, 255L));
}

// THE EDGES. 0.7.12 sampled every texture CLAMPED: the device's start sets
// SetClamp(true) (G:Video/Direct3D9/gs2dD3D9.cpp:1677; :623-643 put
// D3DTADDRESS_CLAMP on U and V of every stage) and nothing in Ethanon turns
// it off. It also drew 1:1 with the half-texel alignment (gs2dD3D9Sprite.cpp:581,
// "subtract 0.5 to align pixel-texel"), so its samples fell on texel centres
// and the wrap would not have shown either way. The port magnifies (1.5625
// image pixels a texel at 1920x1200) and blends between ticks, so a pixel at a
// sprite's border samples up to half a texel outside the image; under the
// engine's default REPEAT that half is the OPPOSITE border. ground.png and
// cliff_left.png are transparent in their top two rows and opaque along the
// bottom, so every ground top and cliff top had a thin dark line hanging a
// few pixels above it, wider than the rock over a pit's edge (Step 24). Off,
// the images repeat as the engine's default does.
constexpr bool kClampToEdge = true;

} // namespace

bool TexturesClampToEdge() { return kClampToEdge; }

void ApplyTextureVariant(std::vector<std::uint8_t>& rgba, TextureVariant variant) {
    switch (variant) {
    case TextureVariant::Sprite:
        // Every sprite, HUD image and background: ColorKey 0xFFFF00FF.
        ColourKey(rgba, 255, 0, 255);
        break;

    case TextureVariant::Halo:
        // Halos (and AM_ADD particles, via Additive below): ColorKey 0xFF000000.
        ColourKey(rgba, 0, 0, 0);
        break;

    case TextureVariant::Additive:
        // Ethanon's AM_ADD is One, One with the alpha test > 1/255 still on
        // (GameSpace.dll SetAlphaMode, docs/spec/30 §3.3); the engine's Additive
        // is SrcAlpha, One. Alpha forced to 1 makes the two equal, and a texel
        // the alpha test would have thrown away is made black so it adds
        // nothing, as it added nothing then.
        ColourKey(rgba, 0, 0, 0);
        for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
            if (rgba[i + 3] <= 1) rgba[i] = rgba[i + 1] = rgba[i + 2] = 0;
            rgba[i + 3] = 255;
        }
        break;

    case TextureVariant::Plain:
        break;

    case TextureVariant::Normal:
        // The 2010 Cg renormalised every fetch, n = normalize(2 * (nm - 0.5))
        // (hPixelLight.cg:69, vPixelLight.cg:67-69); the engine's shadeSprite2D
        // decodes without renormalising, so the texel is renormalised here and
        // stored back. Re-encoded to 8 bits, so a unit vector comes back within
        // about 1/255 of unit length.
        for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
            float n[3] = {rgba[i] / 255.0f * 2.0f - 1.0f, rgba[i + 1] / 255.0f * 2.0f - 1.0f,
                          rgba[i + 2] / 255.0f * 2.0f - 1.0f};
            const float length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            if (length > 1e-6f) {
                for (float& c : n) c /= length;
            } else {
                // No direction at all: straight out of the screen, as a flat map.
                n[0] = 0.0f;
                n[1] = 0.0f;
                n[2] = 1.0f;
            }
            for (int c = 0; c < 3; ++c) rgba[i + static_cast<std::size_t>(c)] = ToByte(n[c] * 0.5f + 0.5f);
        }
        break;

    case TextureVariant::Modulate:
        // AM_MODULATE is Zero, SrcColor: dest *= texel. The engine has no such
        // blend, so it is drawn as black mixed in at alpha = 1 - luminance,
        // which leaves dest * luminance - the multiply, through the texel's
        // luminance rather than per channel. Weighted by the texel's own alpha
        // so a keyed or transparent texel darkens nothing (the original's alpha
        // test dropped those; a partly transparent texel darkens partly, which
        // the original did not do - it modulated fully above alpha 1/255).
        // Particles other than AM_ADD loaded with the magenta key.
        ColourKey(rgba, 255, 0, 255);
        for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
            const float luminance = (0.299f * rgba[i] + 0.587f * rgba[i + 1] + 0.114f * rgba[i + 2]) / 255.0f;
            const float texelAlpha = rgba[i + 3] <= 1 ? 0.0f : rgba[i + 3] / 255.0f;
            rgba[i] = rgba[i + 1] = rgba[i + 2] = 0;
            rgba[i + 3] = ToByte((1.0f - luminance) * texelAlpha);
        }
        break;
    }
}

DecodedImage DecodeTextureMemory(const std::uint8_t* data, std::size_t size, TextureVariant variant,
                                 std::string* error) {
    DecodedImage image;
    std::string why;
    const bool ok = data != nullptr && size > 0 &&
                    (IsDds(data, size) ? DecodeDds(data, size, image, why) : DecodeStb(data, size, image, why));
    if (!ok) {
        if (error != nullptr) *error = why.empty() ? std::string("is empty") : why;
        return DecodedImage{};
    }
    ApplyTextureVariant(image.rgba, variant);
    return image;
}

DecodedImage DecodeTexture(const std::string& absolutePath, TextureVariant variant, std::string* error) {
    std::vector<std::uint8_t> bytes;
    if (!ReadWholeFile(absolutePath, bytes)) {
        if (error != nullptr) *error = "cannot be read";
        return DecodedImage{};
    }
    return DecodeTextureMemory(bytes.data(), bytes.size(), variant, error);
}

glm::ivec2 ProbeImageSize(const std::string& absolutePath) {
    std::vector<std::uint8_t> bytes;
    if (!ReadWholeFile(absolutePath, bytes)) return glm::ivec2(0);
    if (IsDds(bytes.data(), bytes.size())) {
        if (bytes.size() < kDdsHeaderBytes) return glm::ivec2(0);
        const std::uint32_t height = LittleEndian32(bytes.data() + 12);
        const std::uint32_t width = LittleEndian32(bytes.data() + 16);
        if (width == 0 || height == 0 || width > kMaxDimension || height > kMaxDimension) return glm::ivec2(0);
        return glm::ivec2(static_cast<int>(width), static_cast<int>(height));
    }
    if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return glm::ivec2(0);
    int width = 0;
    int height = 0;
    int channels = 0;
    if (stbi_info_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels) == 0 ||
        width <= 0 || height <= 0) {
        return glm::ivec2(0);
    }
    return glm::ivec2(width, height);
}

std::string VirtualTextureKey(const std::string& relativePath, TextureVariant variant) {
    return std::string("penumbra:") + VariantName(variant) + ":" + NormalisedPath(relativePath);
}

// ---- the cache ---------------------------------------------------------------------

TextureCache::TextureCache(std::string gameRoot) : m_gameRoot(NormalisedPath(gameRoot)) {
    while (!m_gameRoot.empty() && m_gameRoot.back() == '/') m_gameRoot.pop_back();
}

void TextureCache::Attach(entt::registry& registry) {
    auto* const* textures = registry.ctx().find<Supersonic::TextureRegistry*>();
    m_textures = textures != nullptr ? *textures : nullptr;
}

namespace {

std::string Absolute(const std::string& gameRoot, const std::string& relativePath) {
    const std::string path = NormalisedPath(relativePath);
    // Already absolute (a drive or a root): taken as it is.
    if ((path.size() >= 2 && path[1] == ':') || (!path.empty() && path[0] == '/')) return path;
    // As the original spelled it, found as the disk spells it (eth/Paths.hpp).
    return gameRoot.empty() ? path : Eth::ResolveUnder(gameRoot, path);
}

} // namespace

std::string TextureCache::Key(const std::string& relativePath, TextureVariant variant) {
    if (relativePath.empty()) return {};
    std::string key = VirtualTextureKey(relativePath, variant);
    if (const auto it = m_entries.find(key); it != m_entries.end()) {
        return it->second.failed ? std::string() : it->second.key;
    }

    // No registry (a suite on a bare registry): the key is still the key, and
    // nothing is remembered, so a later Attach uploads on the next request.
    if (m_textures == nullptr) {
        const glm::ivec2 size = Size(relativePath);
        return size.x > 0 && size.y > 0 ? key : std::string();
    }

    Entry entry;
    std::string error;
    const DecodedImage image = DecodeTexture(Absolute(m_gameRoot, relativePath), variant, &error);
    if (!image.Valid()) {
        // Once: the failure is remembered below, so the next frame asks the map.
        SUPERSONIC_LOG_ERROR("Penumbra") << relativePath << " " << error << "; drawn as nothing." << std::endl;
        entry.failed = true;
        m_entries.emplace(std::move(key), std::move(entry));
        return {};
    }

    // UNDER "data:" AND AS UNORM, because that is the key every reader builds:
    // TextureRegistry::Acquire looks up (srgb ? "srgb:" : "data:") + path, and
    //  - an albedo is acquired with srgb = decodesColourTextures(), which is
    //    false in the DisplayEncoded scene CameraRig sets (RenderSystem.cpp:481),
    //  - a normal map always with srgb = false (RenderSystem.cpp:487),
    //  - a ScreenOverlay::Quad texture always with srgb = false (VulkanRenderer.cpp:1956).
    // So a material naming `key` finds this upload and never reaches stbi_load.
    // Linear filtering, as D3D9 sampled every texture (docs/spec/30 §3.5), and
    // clamped at the edges as it was (THE EDGES, above).
    const std::uint32_t id = m_textures->UploadRGBA(
        "data:" + key, image.rgba.data(), static_cast<std::uint32_t>(image.width),
        static_cast<std::uint32_t>(image.height), /*srgb*/ false, vk::Filter::eLinear,
        kClampToEdge ? vk::SamplerAddressMode::eClampToEdge : vk::SamplerAddressMode::eRepeat);
    if (id == m_textures->GetCheckerTexture()) {
        SUPERSONIC_LOG_ERROR("Penumbra") << relativePath << " could not be uploaded; drawn as nothing." << std::endl;
        entry.failed = true;
        m_entries.emplace(std::move(key), std::move(entry));
        return {};
    }

    entry.key = key;
    entry.size = glm::ivec2(image.width, image.height);
    m_sizes[NormalisedPath(relativePath)] = entry.size;
    m_entries.emplace(key, std::move(entry));
    return key;
}

glm::ivec2 TextureCache::Size(const std::string& relativePath) {
    const std::string path = NormalisedPath(relativePath);
    if (const auto it = m_sizes.find(path); it != m_sizes.end()) return it->second;
    const glm::ivec2 size = ProbeImageSize(Absolute(m_gameRoot, path));
    m_sizes.emplace(path, size);
    return size;
}

} // namespace Penumbra::Render
