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
#include "eth/Paths.hpp"
#include "eth/Text.hpp"
#include "renderer/TextureRegistry.hpp"

// A compile definition on PenumbraGame (game/CMakeLists.txt); the fallback only
// keeps a stray translation unit compiling, and finds nothing.
#ifndef PENUMBRA_DATA_DIR
#define PENUMBRA_DATA_DIR ""
#endif

// stb_truetype, the copy ImGui vendors (engine/third_party/imgui), compiled
// here too. imgui_draw.cpp compiles its own with STBTT_STATIC, and so does this
// file, so every function has internal linkage in both and nothing clashes at
// link time; the namespace keeps even the types apart. Its warnings are the
// library's, not ours.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100 4127 4189 4244 4245 4267 4305 4389 4456 4457 4505 4701 4703 4706)
#elif defined(__GNUC__)
// GCC and Clang both read these. STBTT_STATIC leaves every function this file
// does not call defined and unused.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
namespace Penumbra::Render::Stbtt {
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "imstb_truetype.h"
} // namespace Penumbra::Render::Stbtt
#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
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

// A bundled file standing in for a Windows face (FontAtlas.hpp), with that
// face's GDI metrics: usWinAscent, usWinDescent and xAvgCharWidth of its
// Windows 11 file (ARIALNB.TTF 2.40, arialbd.ttf 7.06, ariblk.ttf 5.23,
// verdanab.ttf 5.33), per 2048 em units - which is what all four use.
struct StandIn {
    const char* name;        // for the atlas key
    const char* file;        // in the bundled fonts folder
    float widthScale;
    int winAscent;
    int winDescent;
    int avgCharWidth;
};

constexpr StandIn kNarrowStandIn = {"arial-narrow", "LiberationSans-Bold.ttf", 0.82f, 1910, 431, 803};
constexpr StandIn kArialStandIn = {"arial", "LiberationSans-Bold.ttf", 1.0f, 1854, 434, 980};
constexpr StandIn kBlackStandIn = {"arial-black", "DejaVuSans-Bold.ttf", 1.0f, 2254, 634, 1131};
constexpr StandIn kVerdanaStandIn = {"verdana", "DejaVuSans-Bold.ttf", 1.0f, 2059, 430, 1163};

// Which files stand in for a face, best first: the face's own bold file
// (D3DXCreateFontA asked for weight 1000), then the bundled stand-in, which
// has the face's metrics, then other system faces, which do not - for Arial
// Narrow the regular narrow cut before a wider bold one, because its width is
// what the layouts were made with.
struct Candidates {
    std::vector<const char*> own;      // in the system's Fonts folder
    StandIn standIn;
    std::vector<const char*> others;   // in the system's Fonts folder
};

const Candidates& CandidatesFor(const std::string& lowerFace) {
    static const Candidates kArialNarrow = {
        {"ARIALNB.TTF"}, kNarrowStandIn, {"ARIALN.TTF", "arialbd.ttf", "tahomabd.ttf", "segoeuib.ttf", "arial.ttf"}};
    static const Candidates kArialBlack = {
        {"ariblk.ttf"}, kBlackStandIn, {"arialbd.ttf", "segoeuib.ttf", "tahomabd.ttf", "arial.ttf"}};
    static const Candidates kVerdana = {
        {"verdanab.ttf"}, kVerdanaStandIn, {"verdana.ttf", "tahomabd.ttf", "arialbd.ttf", "arial.ttf"}};
    static const Candidates kArial = {{"arialbd.ttf"}, kArialStandIn, {"segoeuib.ttf", "tahomabd.ttf", "arial.ttf"}};
    if (lowerFace == "arial narrow") return kArialNarrow;
    if (lowerFace == "arial black") return kArialBlack;
    if (lowerFace == "verdana") return kVerdana;
    return kArial;
}

// `name` in `directory`, as the disk spells it; "" when it is not there.
std::string FileIn(const std::string& directory, const char* name) {
    if (directory.empty()) return {};
    const std::filesystem::path candidate(Eth::ResolveUnder(directory, name));
    std::error_code ec;
    return std::filesystem::is_regular_file(candidate, ec) ? candidate.generic_string() : std::string();
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
    std::string id;                    // file, + "#" + the stand-in's name for a stand-in
    std::string stem;                  // for the atlas key: the file's name, or the stand-in's
    std::vector<unsigned char> data;   // stbtt reads from it for the font's lifetime
    stbtt_fontinfo info{};
    float widthScale = 1.0f;           // horizontal scale on top of the raster scale
    int winAscent = 0;                 // font units
    int winDescent = 0;
    int avgCharWidth = 0;              // font units, widthScale already applied
    int unitsPerEm = 0;                // head.unitsPerEm
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
    // E24: a code point cp1252 does not have, from this face or from the
    // bundled face of its script. Baked the first time it is drawn.
    struct Extra {
        Slot slot;
        const Font* font = nullptr;
        int glyph = 0;
        float scaleX = 0.0f;
        float scale = 0.0f;
    };
    std::string stem;                  // the font file's name, for the key
    std::string key;                   // "" until baked
    int rasterPx = 0;
    float scale = 0.0f;                // stbtt: raster px per font unit
    float scaleX = 0.0f;               // the same, across: scale x the font's widthScale
    float emPx = 0.0f;                 // E24: the face's em, raster px (a script face's glyphs are scaled to it)
    int ascent = 0;                    // raster px
    int lineHeight = 0;
    int tab = 8;
    bool partial = false;
    bool dirty = false;                // E24: wanted has grown since the last bake (Prepare)
    std::bitset<256> wanted;
    std::bitset<256> baked;
    std::array<Slot, 256> slots{};
    std::map<char32_t, Extra> extras;  // E24: ordered, so a bake packs them the same way every time
    std::uint64_t lastUsed = 0;
};

FontAtlas::Script FontAtlas::ScriptOf(const char32_t codePoint) {
    const auto value = static_cast<unsigned>(codePoint);
    const auto in = [value](const unsigned first, const unsigned last) { return value >= first && value <= last; };
    // CJK symbols and punctuation, hiragana, katakana (and its phonetic
    // extensions), the CJK ideographs with extension A, the full-width forms.
    if (in(0x3000, 0x30FF) || in(0x31F0, 0x31FF) || in(0x3400, 0x4DBF) || in(0x4E00, 0x9FFF) ||
        in(0xFF00, 0xFFEF)) {
        return Script::Japanese;
    }
    // Arabic, its supplement, and both presentation-form blocks (what
    // render/ArabicShaping.hpp turns the letters into).
    if (in(0x0600, 0x06FF) || in(0x0750, 0x077F) || in(0xFB50, 0xFDFF) || in(0xFE70, 0xFEFF)) return Script::Arabic;
    return Script::Face;
}

FontAtlas::FontAtlas() : m_bundledDir(DefaultBundledFontsDirectory()) {}
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
#if defined(_MSC_VER)
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, "WINDIR") == 0 && value != nullptr) {
        std::string dir(value);
        std::free(value);
        return dir + "/Fonts";
    }
    std::free(value);
    return "C:/Windows/Fonts";
#elif defined(_WIN32)
    if (const char* value = std::getenv("WINDIR")) return std::string(value) + "/Fonts";
    return "C:/Windows/Fonts";
#else
    // No system face is asked for off Windows: Microsoft's faces are not
    // there, and whatever a distribution substitutes under their names has
    // other metrics. The bundled stand-ins give the same layout everywhere.
    return {};
#endif
}

std::string FontAtlas::DefaultBundledFontsDirectory() {
    const std::string data = PENUMBRA_DATA_DIR;
    return data.empty() ? std::string() : data + "/fonts";
}

void FontAtlas::SetBundledFontsDirectory(const std::string& directory) {
    if (directory == m_bundledDir) return;
    m_bundledDir = directory;
    m_faceChoices.clear();   // fonts and atlases already made are keyed by file and stay valid
    m_scriptFiles.clear();
}

void FontAtlas::SetSystemFontsEnabled(const bool enabled) {
    if (enabled == m_systemFonts) return;
    m_systemFonts = enabled;
    m_faceChoices.clear();
}

const FontAtlas::FaceChoice& FontAtlas::choiceFor(const std::string& face) {
    const std::string lower = Lower(face);
    if (const auto it = m_faceChoices.find(lower); it != m_faceChoices.end()) return it->second;

    const Candidates& candidates = CandidatesFor(lower);
    const std::string system = m_systemFonts ? FontsDirectory() : std::string();
    FaceChoice choice;
    for (const char* name : candidates.own) {
        if (!(choice.file = FileIn(system, name)).empty()) break;
    }
    if (choice.file.empty()) {
        const StandIn& standIn = candidates.standIn;
        choice.file = FileIn(m_bundledDir, standIn.file);
        if (!choice.file.empty()) {
            choice.standIn = standIn.name;
            choice.widthScale = standIn.widthScale;
            choice.winAscent = standIn.winAscent;
            choice.winDescent = standIn.winDescent;
            choice.avgCharWidth = standIn.avgCharWidth;
        }
    }
    if (choice.file.empty()) {
        for (const char* name : candidates.others) {
            if (!(choice.file = FileIn(system, name)).empty()) break;
        }
    }
    if (choice.file.empty()) {
        SUPERSONIC_LOG_WARN("Penumbra") << "fonts: no file for \"" << face << "\" (system fonts: "
                                        << (system.empty() ? std::string("none") : system) << "; bundled: "
                                        << (m_bundledDir.empty() ? std::string("none") : m_bundledDir)
                                        << "); its text is not drawn";
    }
    return m_faceChoices.emplace(lower, std::move(choice)).first->second;
}

std::string FontAtlas::FaceFile(const std::string& face) { return choiceFor(face).file; }

bool FontAtlas::FaceIsStandIn(const std::string& face) { return !choiceFor(face).standIn.empty(); }

int FontAtlas::GlyphForByte(const std::string& face, const unsigned char byte) {
    Font* font = fontFor(face);
    return font != nullptr ? font->glyph[byte] : 0;
}

int FontAtlas::GlyphForCodePoint(const std::string& face, const unsigned codePoint) {
    Font* font = fontFor(face);
    return font != nullptr ? stbtt_FindGlyphIndex(&font->info, static_cast<int>(codePoint)) : 0;
}

int FontAtlas::RoutedGlyph(const std::string& face, const char32_t codePoint) {
    const Script script = ScriptOf(codePoint);
    Font* font = script == Script::Face ? fontFor(face) : scriptFont(script);
    return font != nullptr ? stbtt_FindGlyphIndex(&font->info, static_cast<int>(codePoint)) : 0;
}

std::string FontAtlas::RoutedFile(const std::string& face, const char32_t codePoint) {
    const Script script = ScriptOf(codePoint);
    Font* font = script == Script::Face ? fontFor(face) : scriptFont(script);
    return font != nullptr ? font->file : std::string();
}

FontAtlas::Font* FontAtlas::scriptFont(const Script script) {
    if (script == Script::Face) return nullptr;
    auto it = m_scriptFiles.find(script);
    if (it == m_scriptFiles.end()) {
        const char* name = script == Script::Japanese ? kJapaneseFile : kArabicFile;
        std::string file = FileIn(m_bundledDir, name);
        if (file.empty()) {
            SUPERSONIC_LOG_WARN("Penumbra") << "fonts: no " << name << " in "
                                            << (m_bundledDir.empty() ? std::string("(no bundled folder)") : m_bundledDir)
                                            << "; its script is drawn from the text's own face";
        }
        it = m_scriptFiles.emplace(script, std::move(file)).first;
    }
    if (it->second.empty()) return nullptr;
    if (const auto font = m_fonts.find(it->second); font != m_fonts.end()) return font->second.get();
    return loadFont(it->second, it->second, std::filesystem::path(it->second).stem().string());
}

FontAtlas::Font* FontAtlas::loadFont(const std::string& file, const std::string& id, const std::string& stem) {
    auto font = std::make_unique<Font>();
    font->file = file;
    font->id = id;
    font->stem = stem;
    const int offset = ReadFile(file, font->data) ? stbtt_GetFontOffsetForIndex(font->data.data(), 0) : -1;
    if (offset < 0 || stbtt_InitFont(&font->info, font->data.data(), offset) == 0) {
        SUPERSONIC_LOG_WARN("Penumbra") << "fonts: cannot read " << file;
        m_fonts.emplace(id, nullptr);
        return nullptr;
    }
    const stbtt_uint32 head = stbtt__find_table(font->data.data(), static_cast<stbtt_uint32>(font->info.fontstart), "head");
    font->unitsPerEm = (head != 0 && head + 20 <= font->data.size()) ? ttUSHORT(font->data.data() + head + 18) : 0;
    if (font->unitsPerEm <= 0) font->unitsPerEm = 2048;
    Font* raw = font.get();
    m_fonts.emplace(id, std::move(font));
    return raw;
}

FontAtlas::Font* FontAtlas::fontFor(const std::string& face) {
    const FaceChoice& choice = choiceFor(face);
    if (choice.file.empty()) return nullptr;
    const std::string id = choice.standIn.empty() ? choice.file : choice.file + "#" + choice.standIn;
    if (const auto it = m_fonts.find(id); it != m_fonts.end()) return it->second.get();

    Font* font = loadFont(choice.file, id,
                          choice.standIn.empty()
                              ? std::filesystem::path(choice.file).stem().string()
                              : std::filesystem::path(choice.file).stem().string() + "-" + choice.standIn);
    if (font == nullptr) return nullptr;
    font->widthScale = choice.widthScale;

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
    font->avgCharWidth = RoundToInt(static_cast<float>(font->avgCharWidth) * font->widthScale);
    // A stand-in takes the metrics of the face it replaces, from 2048ths of
    // an em into this file's units (head.unitsPerEm).
    if (choice.winAscent > 0) {
        const float toFile = static_cast<float>(font->unitsPerEm) / 2048.0f;
        font->winAscent = RoundToInt(static_cast<float>(choice.winAscent) * toFile);
        font->winDescent = RoundToInt(static_cast<float>(choice.winDescent) * toFile);
        font->avgCharWidth = RoundToInt(static_cast<float>(choice.avgCharWidth) * toFile);
    }
    for (int byte = 0; byte < 256; ++byte) {
        const auto codePoint = static_cast<int>(Eth::Cp1252CodePoint(static_cast<unsigned char>(byte)));
        font->glyph[static_cast<std::size_t>(byte)] = stbtt_FindGlyphIndex(&font->info, codePoint);
    }
    return font;
}

FontAtlas::Atlas& FontAtlas::atlasFor(Font& font, const int rasterPx) {
    const std::string id = font.id + "|" + std::to_string(rasterPx);
    if (const auto it = m_atlases.find(id); it != m_atlases.end()) return *it->second;

    auto atlas = std::make_unique<Atlas>();
    atlas->stem = font.stem;
    atlas->rasterPx = rasterPx;
    atlas->scale = static_cast<float>(rasterPx) / static_cast<float>(font.winAscent + font.winDescent);
    atlas->scaleX = atlas->scale * font.widthScale;
    atlas->emPx = atlas->scale * static_cast<float>(font.unitsPerEm);
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
        slot.advance = RoundToInt(static_cast<float>(advance) * atlas->scaleX);
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        stbtt_GetGlyphBitmapBox(&font.info, glyph, atlas->scaleX, atlas->scale, &x0, &y0, &x1, &y1);
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

void FontAtlas::addExtra(Font& font, Atlas& atlas, const char32_t codePoint) {
    Atlas::Extra extra;
    const Script script = ScriptOf(codePoint);
    const Font* from = script == Script::Face ? nullptr : scriptFont(script);
    if (from != nullptr) {
        // Em to em, on the face's baseline, never condensed.
        const float scale = atlas.emPx / static_cast<float>(from->unitsPerEm);
        extra.scale = scale;
        extra.scaleX = scale;
    } else {
        from = &font;
        extra.scale = atlas.scale;
        extra.scaleX = atlas.scaleX;
    }
    extra.font = from;
    extra.glyph = stbtt_FindGlyphIndex(&from->info, static_cast<int>(codePoint));
    int advance = 0;
    int bearing = 0;
    stbtt_GetGlyphHMetrics(&from->info, extra.glyph, &advance, &bearing);
    extra.slot.advance = RoundToInt(static_cast<float>(advance) * extra.scaleX);
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    stbtt_GetGlyphBitmapBox(&from->info, extra.glyph, extra.scaleX, extra.scale, &x0, &y0, &x1, &y1);
    extra.slot.x0 = x0;
    extra.slot.y0 = y0;
    extra.slot.w = std::max(0, x1 - x0);
    extra.slot.h = std::max(0, y1 - y0);
    atlas.extras.emplace(codePoint, extra);
}

void FontAtlas::bake(Font& font, Atlas& atlas) {
    // What goes onto the atlas: the cp1252 slots wanted, in byte order, then
    // E24's extras, in code point order (none at all for Portuguese and
    // English, so their atlases are packed exactly as they always were).
    struct Item {
        Atlas::Slot* slot;
        const stbtt_fontinfo* info;
        int glyph;
        float scaleX;
        float scale;
    };
    std::vector<Item> order;
    long long area = 0;
    int widest = 1;
    const auto add = [&](Atlas::Slot& slot, const stbtt_fontinfo& info, const int glyph, const float scaleX,
                         const float scale) {
        if (slot.w <= 0 || slot.h <= 0) return;
        order.push_back(Item{&slot, &info, glyph, scaleX, scale});
        area += static_cast<long long>(slot.w + kPad) * (slot.h + kPad);
        widest = std::max(widest, slot.w + 2 * kPad);
    };
    for (int byte = 0x20; byte < 256; ++byte) {
        if (!atlas.wanted.test(static_cast<std::size_t>(byte))) continue;
        add(atlas.slots[static_cast<std::size_t>(byte)], font.info, font.glyph[static_cast<std::size_t>(byte)],
            atlas.scaleX, atlas.scale);
    }
    for (auto& entry : atlas.extras) {
        Atlas::Extra& extra = entry.second;
        add(extra.slot, extra.font->info, extra.glyph, extra.scaleX, extra.scale);
    }
    // Tallest first onto shelves.
    std::stable_sort(order.begin(), order.end(), [](const Item& a, const Item& b) { return a.slot->h > b.slot->h; });

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
            const Atlas::Slot& slot = *order[i].slot;
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
    int dropped = 0;
    for (std::size_t i = 0; i < order.size(); ++i) {
        Atlas::Slot& slot = *order[i].slot;
        if (at[i].y + slot.h > height) {
            slot.w = 0;   // did not fit: drawn as nothing rather than as a neighbour
            ++dropped;
            continue;
        }
        unsigned char* target =
            coverage.data() + static_cast<std::size_t>(at[i].y) * static_cast<std::size_t>(width) + at[i].x;
        stbtt_MakeGlyphBitmap(order[i].info, target, slot.w, slot.h, width, order[i].scaleX, order[i].scale,
                              order[i].glyph);
        slot.uvMin = glm::vec2(at[i]) / glm::vec2(static_cast<float>(width), static_cast<float>(height));
        slot.uvMax = glm::vec2(at[i] + glm::ivec2(slot.w, slot.h)) /
                     glm::vec2(static_cast<float>(width), static_cast<float>(height));
    }

    if (dropped > 0) {
        SUPERSONIC_LOG_WARN("Penumbra") << "fonts: the " << atlas.stem << " atlas at " << atlas.rasterPx << " px is full ("
                                        << kMaxAtlasSide << "x" << kMaxAtlasSide << "); " << dropped
                                        << " glyph(s) will not be drawn";
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
    atlas.dirty = false;
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

TextLayout FontAtlas::LayoutCp1252(const std::string& cp1252, const std::string& face, const float size,
                                   const glm::vec2 pos) {
    std::u32string text;
    text.reserve(cp1252.size());
    for (const char c : cp1252) text += static_cast<char32_t>(Eth::Cp1252CodePoint(static_cast<unsigned char>(c)));
    return LayoutCodePoints(text, face, size, pos);
}

TextLayout FontAtlas::LayoutUtf8(const std::string& utf8, const std::string& face, const float size,
                                 const glm::vec2 pos) {
    return LayoutCodePoints(Eth::Utf8ToCodePoints(utf8), face, size, pos);
}

int FontAtlas::rasterPxFor(const float size) const { return std::clamp(RoundToInt(size * m_scale), 1, kMaxRasterPx); }

bool FontAtlas::want(Font& font, Atlas& atlas, const std::u32string& text) {
    bool grew = false;
    for (const char32_t codePoint : text) {
        if (codePoint < 0x20) continue;
        unsigned char byte = 0;
        if (Eth::Cp1252ByteOf(static_cast<unsigned>(codePoint), byte)) {
            if (!atlas.partial || atlas.wanted.test(byte)) continue;
            atlas.wanted.set(byte);
            grew = true;
        } else if (atlas.extras.find(codePoint) == atlas.extras.end()) {
            addExtra(font, atlas, codePoint);
            grew = true;
        }
    }
    return grew;
}

void FontAtlas::Prepare(const std::u32string& text, const std::string& face, const float size) {
    if (text.empty()) return;
    Font* font = fontFor(face);
    if (font == nullptr) return;
    Atlas& atlas = atlasFor(*font, rasterPxFor(size));
    atlas.lastUsed = m_frame;
    if (want(*font, atlas, text)) atlas.dirty = true;
}

FontAtlas::Extent FontAtlas::Measure(const std::u32string& text, const std::string& face, const float size,
                                     std::vector<float>* lineWidths) {
    Extent extent;
    if (lineWidths != nullptr) lineWidths->clear();
    if (text.empty()) return extent;
    Font* font = fontFor(face);
    if (font == nullptr) return extent;
    Atlas& atlas = atlasFor(*font, rasterPxFor(size));
    atlas.lastUsed = m_frame;
    // An extra's advance is known once it is added; the next Layout bakes it.
    if (want(*font, atlas, text)) atlas.dirty = true;

    // LayoutCodePoints' first pass: whole raster pixels a line, the same breaks and tabs.
    int penX = 0;
    int widest = 0;
    int line = 0;
    int lastWithAdvance = -1;
    const auto endLine = [&]() {
        widest = std::max(widest, penX);
        if (lineWidths != nullptr) lineWidths->push_back(static_cast<float>(penX) / m_scale);
        if (penX > 0) lastWithAdvance = line;
        ++line;
        penX = 0;
    };
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char32_t codePoint = text[i];
        if (codePoint == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') continue;
            endLine();
            continue;
        }
        if (codePoint == '\n') {
            endLine();
            continue;
        }
        if (codePoint == '\t') {
            penX = (penX / atlas.tab + 1) * atlas.tab;
            continue;
        }
        if (codePoint < 0x20) continue;
        unsigned char byte = 0;
        if (Eth::Cp1252ByteOf(static_cast<unsigned>(codePoint), byte)) {
            penX += atlas.slots[byte].advance;
        } else if (const auto extra = atlas.extras.find(codePoint); extra != atlas.extras.end()) {
            penX += extra->second.slot.advance;
        }
    }
    endLine();
    extent.width = static_cast<float>(widest) / m_scale;
    extent.lines = lastWithAdvance + 1;
    extent.lineHeight = static_cast<float>(atlas.lineHeight) / m_scale;
    return extent;
}

TextLayout FontAtlas::LayoutCodePoints(const std::u32string& text, const std::string& face, const float size,
                                       const glm::vec2 pos, const LineAlign align) {
    TextLayout layout;
    if (text.empty()) return layout;
    Font* font = fontFor(face);
    if (font == nullptr) return layout;

    const float scale = m_scale;
    Atlas& atlas = atlasFor(*font, rasterPxFor(size));
    atlas.lastUsed = m_frame;

    // Everything this text needs that the atlas lacks, all at once, with
    // whatever Prepare asked for since the last bake: one rebake however many
    // new characters a sign - or a frame's texts, prepared first - bring. The
    // same wanted set packs the same way, so a text drawn alone or after a
    // Prepare lands on the same pixels.
    if (want(*font, atlas, text) || atlas.dirty || atlas.key.empty()) bake(*font, atlas);

    layout.texture = atlas.key;
    layout.lineHeight = static_cast<float>(atlas.lineHeight) / scale;
    layout.ascent = static_cast<float>(atlas.ascent) / scale;

    const auto slotOf = [&atlas](const char32_t codePoint, unsigned char& byte) -> const Atlas::Slot& {
        byte = 0;
        if (Eth::Cp1252ByteOf(static_cast<unsigned>(codePoint), byte)) return atlas.slots[byte];
        byte = 0;
        return atlas.extras.find(codePoint)->second.slot;
    };
    // One pass per line of pen advances: the glyphs' places (`place`) or only
    // each line's width (the first pass, for LineAlign::Right).
    std::vector<int> lineWidths;
    const auto walk = [&](const bool place, const int anchorX, const int anchorY, const int widest) {
        int penX = 0;
        int line = 0;
        const auto offset = [&]() {
            return (place && align != LineAlign::Left && static_cast<std::size_t>(line) < lineWidths.size())
                       ? widest - lineWidths[static_cast<std::size_t>(line)]
                       : 0;
        };
        int lineStart = offset();
        const auto newLine = [&]() {
            if (!place) lineWidths.push_back(penX);
            ++line;
            penX = 0;
            lineStart = offset();
        };
        for (std::size_t i = 0; i < text.size(); ++i) {
            const char32_t codePoint = text[i];
            if (codePoint == '\r') {
                // CR LF is one break; a lone CR breaks too, as DrawText reads it.
                if (i + 1 < text.size() && text[i + 1] == '\n') continue;
                newLine();
                continue;
            }
            if (codePoint == '\n') {
                newLine();
                continue;
            }
            if (codePoint == '\t') {
                penX = (penX / atlas.tab + 1) * atlas.tab;
                continue;
            }
            if (codePoint < 0x20) continue;
            unsigned char byte = 0;
            const Atlas::Slot& slot = slotOf(codePoint, byte);
            if (place && slot.w > 0 && slot.h > 0) {
                const int baseline = anchorY + line * atlas.lineHeight + atlas.ascent;
                const int left = anchorX + lineStart + penX + slot.x0;
                const int top = baseline + slot.y0;
                TextGlyph glyph;
                glyph.min = glm::vec2(static_cast<float>(left), static_cast<float>(top)) / scale;
                glyph.max = glm::vec2(static_cast<float>(left + slot.w), static_cast<float>(top + slot.h)) / scale;
                glyph.uvMin = slot.uvMin;
                glyph.uvMax = slot.uvMax;
                glyph.byte = byte;
                glyph.codePoint = codePoint;
                glyph.line = line;
                glyph.pen = glm::vec2(static_cast<float>(anchorX + lineStart + penX), static_cast<float>(baseline)) / scale;
                layout.glyphs.push_back(glyph);
            }
            penX += slot.advance;
        }
        if (!place) lineWidths.push_back(penX);
        return line;
    };

    // (int) the position as DrawTextA's RECT did, then into window pixels,
    // where every glyph edge below lands on a whole pixel.
    const int anchorX = RoundToInt(static_cast<float>(static_cast<int>(pos.x)) * scale);
    const int anchorY = RoundToInt(static_cast<float>(static_cast<int>(pos.y)) * scale);
    walk(false, anchorX, anchorY, 0);
    const int widest = lineWidths.empty() ? 0 : *std::max_element(lineWidths.begin(), lineWidths.end());
    // RightEdge: the block's left in whole window pixels, so its glyphs land
    // on them as a left-anchored text's do.
    const int startX = align == LineAlign::RightEdge ? RoundToInt(pos.x * scale) - widest : anchorX;
    const int lines = walk(true, startX, anchorY, widest);
    layout.lines = lines + 1;
    layout.width = static_cast<float>(widest) / scale;
    return layout;
}

} // namespace Penumbra::Render
