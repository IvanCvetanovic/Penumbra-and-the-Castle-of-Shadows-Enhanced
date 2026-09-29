#pragma once

// The pure half of TextureCache: one of the original's images decoded the way
// 0.7.12 decoded it and processed for one TextureVariant, with no device and no
// registry. TextureCache uploads what this returns; a suite checks it directly.
//
// Decoding is by CONTENT, never by extension (nm_white_ground.jpg is a BMP):
// "DDS " files go through the reader in TextureCache.cpp (the nine shipped DDS
// are uncompressed: eight A8R8G8B8 and data/shadow.dds A8L8, docs/spec/20 §12.3),
// everything else through the engine's stb_image, which sniffs its own formats.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "render/TextureCache.hpp"

namespace Penumbra::Render {

struct DecodedImage {
    int width = 0;
    int height = 0;
    // width * height texels, RGBA8, rows top-down (the order the engine's quad
    // samples them: v = 0 is the image's top).
    std::vector<std::uint8_t> rgba;

    bool Valid() const {
        return width > 0 && height > 0 &&
               rgba.size() == static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
    }
};

// Reads `absolutePath`, decodes it and applies `variant`. An invalid image on
// failure, with the reason in `error` when one is given.
DecodedImage DecodeTexture(const std::string& absolutePath, TextureVariant variant, std::string* error = nullptr);

// The same from bytes already in memory.
DecodedImage DecodeTextureMemory(const std::uint8_t* data, std::size_t size, TextureVariant variant,
                                 std::string* error = nullptr);

// The variant's per-texel rule alone, on RGBA8 already decoded (exposed so a
// suite can hold each rule to a hand-made texel).
void ApplyTextureVariant(std::vector<std::uint8_t>& rgba, TextureVariant variant);

// The pixel size from the header alone; (0,0) when unreadable.
glm::ivec2 ProbeImageSize(const std::string& absolutePath);

// "penumbra:<variant>:<relative path>", backslashes turned to slashes: the path
// a MaterialComponent or ScreenOverlay::Quad names for that upload.
std::string VirtualTextureKey(const std::string& relativePath, TextureVariant variant);

// Whether TextureCache uploads its images sampled CLAMPED to their edges, as
// 0.7.12 sampled every texture, rather than repeating (TextureCache.cpp, THE
// EDGES; the switch kClampToEdge).
bool TexturesClampToEdge();

} // namespace Penumbra::Render
