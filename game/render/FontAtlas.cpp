#include "render/FontAtlas.hpp"

#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

// The C library stb_truetype would include, included first so that its own
// #includes inside the namespace below are no-ops.
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "core/Log.hpp"
#include "eth/Text.hpp"
#include "renderer/TextureRegistry.hpp"

// stb_truetype, the copy ImGui vendors (engine/third_party/imgui), compiled
// here too. imgui_draw.cpp compiles its own with STBTT_STATIC, and so does this
// file, so every function has internal linkage in both and nothing clashes at
// link time; the namespace keeps even the types apart. Its warnings are the
// library's, not ours.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100 4127 4189 4244 4245 4267 4305 4389 4456 4457 4505 4701 4703 4706)
#endif
namespace Penumbra::Render::Stbtt {
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "imstb_truetype.h"
} // namespace Penumbra::Render::Stbtt
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace Penumbra::Render {
namespace {

using namespace Stbtt;

constexpr int kPad = 2;             // texels between glyphs: linear filtering and mips never mix two
constexpr int kMaxAtlasSide = 4096; // what every Vulkan device can sample
constexpr int kMaxRasterPx = 1024;  // a 256-px clock at 4x; beyond, the atlas stops fitting

std::string Lower(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return text;
}

int RoundToInt(const float value) { return static_cast<int>(std::lround(value)); }

// Which files stand in for a face, best first. The bold file first, because
// D3DXCreateFontA asked for weight 1000; for Arial Narrow the regular narrow cut
// before a wider bold one, because its width is what the layouts were made with.
const std::vector<const char*>& Candidates(const std::string& lowerFace) {
    static const std::vector<const char*> kArialNarrow = {"ARIALNB.TTF", "ARIALN.TTF", "arialbd.ttf",
                                                          "tahomabd.ttf", "segoeuib.ttf", "arial.ttf"};
    static const std::vector<const char*> kArialBlack = {"ariblk.ttf", "arialbd.ttf", "segoeuib.ttf",
                                                         "tahomabd.ttf", "arial.ttf"};
    static const std::vector<const char*> kVerdana = {"verdanab.ttf", "verdana.ttf", "tahomabd.ttf",
                                                      "arialbd.ttf", "arial.ttf"};
    static const std::vector<const char*> kArial = {"arialbd.ttf", "segoeuib.ttf", "tahomabd.ttf", "arial.ttf"};
    if (lowerFace == "arial narrow") return kArialNarrow;
    if (lowerFace == "arial black") return kArialBlack;
    if (lowerFace == "verdana") return kVerdana;
    return kArial;
}

bool ReadFile(const std::string& path, std::vector<unsigned char>& out) {
    std::ifstream file(std::filesystem::path(path), std::ios::binary);
    if (!file) return false;
    out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return !out.empty();
}

} // namespace

struct FontAtlas::Font {
    std::string file;
    std::vector<unsigned char> data;   // stbtt reads from it for the font's lifetime
    stbtt_fontinfo info{};
    int winAscent = 0;                 // font units
    int winDescent = 0;
    int avgCharWidth = 0;
    std::array<int, 256> glyph{};      // glyph index per cp1252 byte
};

struct FontAtlas::Atlas {
    struct Slot {
        int advance = 0;               // raster px, whole
        int x0 = 0;                    // bitmap box relative to the pen and the baseline
        int y0 = 0;
        int w = 0;
        int h = 0;
        glm::vec2 uvMin{0.0f};
        glm::vec2 uvMax{0.0f};
    };
    std::string stem;                  // the font file's name, for the key
    std::string key;                   // "" until baked
    int rasterPx = 0;
    float scale = 0.0f;                // stbtt: raster px per font unit
    int ascent = 0;                    // raster px
    int lineHeight = 0;
    int tab = 8;
    bool partial = false;
    std::bitset<256> wanted;
    std::bitset<256> baked;
    std::array<Slot, 256> slots{};
    std::uint64_t lastUsed = 0;
};

FontAtlas::FontAtlas() = default;
FontAtlas::~FontAtlas() = default;

void FontAtlas::Attach(entt::registry& registry) {
    auto* const* slot = registry.ctx().find<Supersonic::TextureRegistry*>();
    m_textures = slot != nullptr ? *slot : nullptr;
    // Keys uploaded into another registry mean nothing to this one.
    m_atlases.clear();
    m_retired.clear();
}

void FontAtlas::Detach() {
    m_textures = nullptr;
    m_atlases.clear();
    m_retired.clear();
}

void FontAtlas::SetRasterScale(const float windowPixelsPerLogical) {
    m_scale = windowPixelsPerLogical > 0.0f ? windowPixelsPerLogical : 1.0f;
}

std::string FontAtlas::FontsDirectory() {
#ifdef _MSC_VER
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, "WINDIR") == 0 && value != nullptr) {
        std::string dir(value);
        std::free(value);
        return dir + "/Fonts";
    }
    std::free(value);
#else
    if (const char* value = std::getenv("WINDIR")) return std::string(value) + "/Fonts";
#endif
    return "C:/Windows/Fonts";
}

std::string FontAtlas::FaceFile(const std::string& face) {
    const std::string lower = Lower(face);
    if (const auto it = m_faceFiles.find(lower); it != m_faceFiles.end()) return it->second;
    const std::filesystem::path dir(FontsDirectory());
    std::string found;
    for (const char* name : Candidates(lower)) {
        std::error_code ec;
        const std::filesystem::path candidate = dir / name;
        if (std::filesystem::is_regular_file(candidate, ec)) {
            found = candidate.generic_string();
            break;
        }
    }
    if (found.empty()) {
        SUPERSONIC_LOG_WARN("Penumbra") << "fonts: no file for \"" << face << "\" in " << dir.generic_string()
                                        << "; its text is not drawn";
    }
    m_faceFiles.emplace(lower, found);
    return found;
}

FontAtlas::Font* FontAtlas::fontFor(const std::string& face) {
    const std::string file = FaceFile(face);
    if (file.empty()) return nullptr;
    if (const auto it = m_fonts.find(file); it != m_fonts.end()) return it->second.get();

    auto font = std::make_unique<Font>();
    font->file = file;
    const int offset = ReadFile(file, font->data) ? stbtt_GetFontOffsetForIndex(font->data.data(), 0) : -1;
    if (offset < 0 || stbtt_InitFont(&font->info, font->data.data(), offset) == 0) {
        SUPERSONIC_LOG_WARN("Penumbra") << "fonts: cannot read " << file;
        m_fonts.emplace(file, nullptr);
        return nullptr;
    }

    // GDI maps a positive lfHeight onto usWinAscent + usWinDescent, and takes
    // tmAveCharWidth (the tab unit) from xAvgCharWidth: both in the OS/2 table,
    // which stb_truetype reads only the typographic half of.
    const stbtt_uint32 os2 = stbtt__find_table(font->data.data(), static_cast<stbtt_uint32>(font->info.fontstart), "OS/2");
    if (os2 != 0 && os2 + 78 <= font->data.size()) {
        unsigned char* table = font->data.data() + os2;
        font->avgCharWidth = ttSHORT(table + 2);
        font->winAscent = ttUSHORT(table + 74);
        font->winDescent = ttUSHORT(table + 76);
    }
    if (font->winAscent + font->winDescent <= 0) {
        int ascent = 0;
        int descent = 0;
        int lineGap = 0;
        stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &lineGap);
        font->winAscent = ascent;
        font->winDescent = -descent;
    }
    if (font->avgCharWidth <= 0) {
        int advance = 0;
        int bearing = 0;
        stbtt_GetGlyphHMetrics(&font->info, stbtt_FindGlyphIndex(&font->info, 'x'), &advance, &bearing);
        font->avgCharWidth = advance;
    }
    for (int byte = 0; byte < 256; ++byte) {
        const auto codePoint = static_cast<int>(Eth::Cp1252CodePoint(static_cast<unsigned char>(byte)));
        font->glyph[static_cast<std::size_t>(byte)] = stbtt_FindGlyphIndex(&font->info, codePoint);
    }

    Font* raw = font.get();
    m_fonts.emplace(file, std::move(font));
    return raw;
}

FontAtlas::Atlas& FontAtlas::atlasFor(Font& font, const int rasterPx) {
    const std::string id = font.file + "|" + std::to_string(rasterPx);
    if (const auto it = m_atlases.find(id); it != m_atlases.end()) return *it->second;

    auto atlas = std::make_unique<Atlas>();
    atlas->stem = std::filesystem::path(font.file).stem().string();
    atlas->rasterPx = rasterPx;
    atlas->scale = static_cast<float>(rasterPx) / static_cast<float>(font.winAscent + font.winDescent);
    atlas->ascent = RoundToInt(static_cast<float>(font.winAscent) * atlas->scale);
    atlas->lineHeight = atlas->ascent + RoundToInt(static_cast<float>(font.winDescent) * atlas->scale);
    // DT_EXPANDTABS without DT_TABSTOP: a stop every 8 average character widths.
    atlas->tab = 8 * std::max(1, RoundToInt(static_cast<float>(font.avgCharWidth) * atlas->scale));
    atlas->partial = rasterPx > kFullAtlasMaxPx;
    atlas->lastUsed = m_frame;

    for (int byte = 0x20; byte < 256; ++byte) {
        Atlas::Slot& slot = atlas->slots[static_cast<std::size_t>(byte)];
        const int glyph = font.glyph[static_cast<std::size_t>(byte)];
        int advance = 0;
        int bearing = 0;
        stbtt_GetGlyphHMetrics(&font.info, glyph, &advance, &bearing);
        // Whole pixels, as GDI's advance widths were at the size it drew.
        slot.advance = RoundToInt(static_cast<float>(advance) * atlas->scale);
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        stbtt_GetGlyphBitmapBox(&font.info, glyph, atlas->scale, atlas->scale, &x0, &y0, &x1, &y1);
        slot.x0 = x0;
        slot.y0 = y0;
        slot.w = std::max(0, x1 - x0);
        slot.h = std::max(0, y1 - y0);
        if (!atlas->partial) atlas->wanted.set(static_cast<std::size_t>(byte));
    }
    if (atlas->partial) {
        for (const char c : std::string("0123456789:")) atlas->wanted.set(static_cast<unsigned char>(c));
    }

    Atlas& ref = *atlas;
    m_atlases.emplace(id, std::move(atlas));
    return ref;
}

void FontAtlas::bake(Font& font, Atlas& atlas) {
    std::vector<int> order;
    long long area = 0;
    int widest = 1;
    for (int byte = 0x20; byte < 256; ++byte) {
        const Atlas::Slot& slot = atlas.slots[static_cast<std::size_t>(byte)];
        if (!atlas.wanted.test(static_cast<std::size_t>(byte)) || slot.w <= 0 || slot.h <= 0) continue;
        order.push_back(byte);
        area += static_cast<long long>(slot.w + kPad) * (slot.h + kPad);
        widest = std::max(widest, slot.w + 2 * kPad);
    }
    // Tallest first onto shelves.
    std::stable_sort(order.begin(), order.end(), [&atlas](const int a, const int b) {
        return atlas.slots[static_cast<std::size_t>(a)].h > atlas.slots[static_cast<std::size_t>(b)].h;
    });

    int width = 64;
    while (width < kMaxAtlasSide &&
           (static_cast<long long>(width) * width < area + area / 5 || width < widest)) {
        width *= 2;
    }
    std::vector<glm::ivec2> at(order.size());
    int height = 0;
    for (;;) {
        int x = kPad;
        int y = kPad;
        int shelf = 0;
        for (std::size_t i = 0; i < order.size(); ++i) {
            const Atlas::Slot& slot = atlas.slots[static_cast<std::size_t>(order[i])];
            if (x + slot.w + kPad > width) {
                y += shelf + kPad;
                x = kPad;
                shelf = 0;
            }
            at[i] = glm::ivec2(x, y);
            x += slot.w + kPad;
            shelf = std::max(shelf, slot.h);
        }
        height = std::max(1, y + shelf + kPad);
        if (height <= kMaxAtlasSide || width >= kMaxAtlasSide) break;
        width *= 2;
    }
    height = std::min(height, kMaxAtlasSide);

    std::vector<unsigned char> coverage(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0);
    for (std::size_t i = 0; i < order.size(); ++i) {
        Atlas::Slot& slot = atlas.slots[static_cast<std::size_t>(order[i])];
        if (at[i].y + slot.h > height) {
            slot.w = 0;   // did not fit: drawn as nothing rather than as a neighbour
            continue;
        }
        unsigned char* target =
            coverage.data() + static_cast<std::size_t>(at[i].y) * static_cast<std::size_t>(width) + at[i].x;
        stbtt_MakeGlyphBitmap(&font.info, target, slot.w, slot.h, width, atlas.scale, atlas.scale,
                              font.glyph[static_cast<std::size_t>(order[i])]);
        slot.uvMin = glm::vec2(at[i]) / glm::vec2(static_cast<float>(width), static_cast<float>(height));
        slot.uvMax = glm::vec2(at[i] + glm::ivec2(slot.w, slot.h)) /
                     glm::vec2(static_cast<float>(width), static_cast<float>(height));
    }

    // White, with the coverage as alpha: the quad's colour is the text colour,
    // as D3DX modulated its glyph texture by DrawText's argb.
    std::vector<std::uint8_t> rgba(coverage.size() * 4);
    for (std::size_t i = 0; i < coverage.size(); ++i) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = coverage[i];
    }

    // A fresh key per bake: an invalidated key read again would make the
    // registry look for a file by that name and cache the failure.
    if (!atlas.key.empty()) retire(atlas.key);
    atlas.key = "penumbra:font:" + atlas.stem + ":" + std::to_string(atlas.rasterPx) + ":" + std::to_string(++m_serial);
    if (m_textures != nullptr) {
        // "data:" and UNORM, which is how the overlay acquires every texture
        // it draws (VulkanRenderer.cpp, the screen overlay pass).
        m_textures->UploadRGBA("data:" + atlas.key, rgba.data(), static_cast<std::uint32_t>(width),
                               static_cast<std::uint32_t>(height), false);
    }
    atlas.baked = atlas.wanted;
}

void FontAtlas::retire(const std::string& key) { m_retired.push_back(key); }

void FontAtlas::BeginFrame() {
    ++m_frame;
    if (m_textures != nullptr) {
        for (const std::string& key : m_retired) m_textures->Invalidate(key);
    }
    m_retired.clear();
    for (auto it = m_atlases.begin(); it != m_atlases.end();) {
        if (m_frame - it->second->lastUsed > kEvictAfterFrames) {
            // Nothing queued this frame yet, so it can go at once.
            if (m_textures != nullptr && !it->second->key.empty()) m_textures->Invalidate(it->second->key);
            it = m_atlases.erase(it);
        } else {
            ++it;
        }
    }
}

TextLayout FontAtlas::Layout(const std::string& cp1252, const std::string& face, const float size,
                             const glm::vec2 pos) {
    TextLayout layout;
    if (cp1252.empty()) return layout;
    Font* font = fontFor(face);
    if (font == nullptr) return layout;

    const float scale = m_scale;
    const int rasterPx = std::clamp(RoundToInt(size * scale), 1, kMaxRasterPx);
    Atlas& atlas = atlasFor(*font, rasterPx);
    atlas.lastUsed = m_frame;

    if (atlas.partial) {
        bool grew = false;
        for (const char c : cp1252) {
            const auto byte = static_cast<unsigned char>(c);
            if (byte < 0x20 || atlas.wanted.test(byte)) continue;
            atlas.wanted.set(byte);
            grew = true;
        }
        if (grew || atlas.key.empty()) bake(*font, atlas);
    } else if (atlas.key.empty()) {
        bake(*font, atlas);
    }

    layout.texture = atlas.key;
    layout.lineHeight = static_cast<float>(atlas.lineHeight) / scale;
    layout.ascent = static_cast<float>(atlas.ascent) / scale;

    // (int) the position as DrawTextA's RECT did, then into window pixels,
    // where every glyph edge below lands on a whole pixel.
    const int anchorX = RoundToInt(static_cast<float>(static_cast<int>(pos.x)) * scale);
    const int anchorY = RoundToInt(static_cast<float>(static_cast<int>(pos.y)) * scale);
    int penX = 0;
    int line = 0;
    int widest = 0;
    const auto newLine = [&]() {
        widest = std::max(widest, penX);
        ++line;
        penX = 0;
    };
    for (std::size_t i = 0; i < cp1252.size(); ++i) {
        const auto byte = static_cast<unsigned char>(cp1252[i]);
        if (byte == '\r') {
            // CR LF is one break; a lone CR breaks too, as DrawText reads it.
            if (i + 1 < cp1252.size() && cp1252[i + 1] == '\n') continue;
            newLine();
            continue;
        }
        if (byte == '\n') {
            newLine();
            continue;
        }
        if (byte == '\t') {
            penX = (penX / atlas.tab + 1) * atlas.tab;
            continue;
        }
        if (byte < 0x20) continue;
        const Atlas::Slot& slot = atlas.slots[byte];
        if (slot.w > 0 && slot.h > 0) {
            const int baseline = anchorY + line * atlas.lineHeight + atlas.ascent;
            const int left = anchorX + penX + slot.x0;
            const int top = baseline + slot.y0;
            TextGlyph glyph;
            glyph.min = glm::vec2(static_cast<float>(left), static_cast<float>(top)) / scale;
            glyph.max = glm::vec2(static_cast<float>(left + slot.w), static_cast<float>(top + slot.h)) / scale;
            glyph.uvMin = slot.uvMin;
            glyph.uvMax = slot.uvMax;
            glyph.byte = byte;
            glyph.line = line;
            glyph.pen = glm::vec2(static_cast<float>(anchorX + penX), static_cast<float>(baseline)) / scale;
            layout.glyphs.push_back(glyph);
        }
        penX += slot.advance;
    }
    widest = std::max(widest, penX);
    layout.lines = line + 1;
    layout.width = static_cast<float>(widest) / scale;
    return layout;
}

} // namespace Penumbra::Render
