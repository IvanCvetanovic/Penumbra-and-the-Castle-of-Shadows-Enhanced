#pragma once

// DrawText's fonts: the system TrueType faces 0.7.12 asked GDI for, rasterised
// with stb_truetype into atlases a ScreenOverlay quad can name.
//
// What the original did (docs/spec/30-ethanon-runtime.md §3.6, disassembled
// from GameSpace.dll): D3DXCreateFontA(Height = (int)size, Weight = 1000,
// ANSI_CHARSET, ANTIALIASED_QUALITY, face) - a positive height is the GDI CELL
// height (ascent + descent, not the em), and weight 1000 means every face came
// out bold - then ID3DXFont::DrawTextA(text, {(int)x, (int)y}, DT_EXPANDTABS |
// DT_NOCLIP, argb): top-left anchored at the truncated position, '\n' starting
// a line one tmHeight lower, tabs to every 8 average character widths, nothing
// clipped. So:
//   "Arial Narrow" -> ARIALNB.TTF (Arial Narrow Bold)
//   "Arial Black"  -> ariblk.ttf
//   "Arial"        -> arialbd.ttf
//   "Verdana"      -> verdanab.ttf
// read from the system's Fonts folder at run time (never copied into the
// repository), each falling back to another bold sans and finally arial.ttf.
// The cell height is scaled against the OS/2 table's usWinAscent+usWinDescent,
// which is what GDI maps a positive lfHeight onto.
//
// ENHANCED: the glyphs are rasterised at the WINDOW's resolution (the logical
// size times View::scale), not at the original's 1024x768, so text stays sharp
// at 1080p and 4K; its layout is the original's, scaled. Glyph advances are
// whole pixels at that resolution, as GDI's were at its own.
//
// An atlas is baked per (face file, pixel size) on first use: every cp1252 byte
// 0x20-0xFF (the undefined ones as U+FFFD, through Eth::Cp1252CodePoint). Above
// kFullAtlasMaxPx only the characters actually drawn are baked (the 256-pixel
// clock is digits and ':'), and a new character rebakes that atlas. An atlas
// unused for a few seconds - a size left behind by a window resize - is freed.

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace Supersonic {
class TextureRegistry;
}

namespace Penumbra::Render {

// One visible glyph, in LOGICAL screen pixels (the snapshot's screen space,
// +y down), for View::HudToFraction.
struct TextGlyph {
    glm::vec2 min{0.0f};       // the glyph bitmap's top-left
    glm::vec2 max{0.0f};
    glm::vec2 uvMin{0.0f};
    glm::vec2 uvMax{0.0f};
    unsigned char byte = 0;    // the cp1252 byte drawn
    int line = 0;              // 0-based
    glm::vec2 pen{0.0f};       // x: the glyph's pen position; y: its line's baseline
};

struct TextLayout {
    std::string texture;             // atlas key for ScreenOverlay::Quad::texture ("" = no font found)
    std::vector<TextGlyph> glyphs;   // only glyphs with pixels: spaces, tabs and breaks make none
    int lines = 0;                   // lines of the text (0 for "")
    float lineHeight = 0.0f;         // logical px from one line to the next (GDI tmHeight)
    float ascent = 0.0f;             // logical px from a line's top to its baseline (tmAscent)
    float width = 0.0f;              // the widest line's advance, logical px
};

class FontAtlas {
public:
    // Characters baked on demand above this raster cell height.
    static constexpr int kFullAtlasMaxPx = 128;
    // Frames (BeginFrame calls) an atlas may go unused before it is freed.
    static constexpr std::uint64_t kEvictAfterFrames = 600;

    FontAtlas();
    ~FontAtlas();
    FontAtlas(const FontAtlas&) = delete;
    FontAtlas& operator=(const FontAtlas&) = delete;

    // Finds the TextureRegistry in registry.ctx(). Without one (a suite on a
    // bare registry) layout and baking still work and nothing is uploaded.
    void Attach(entt::registry& registry);
    void Detach();

    // Window pixels per logical pixel (View::scale). Glyphs are rasterised at
    // size * scale; 1 reproduces the original's pixels.
    void SetRasterScale(float windowPixelsPerLogical);
    float RasterScale() const { return m_scale; }

    // Once per drawn frame, before any Layout: frees the atlases nobody has
    // used for kEvictAfterFrames, and the ones replaced by a rebake last frame
    // (not at once: quads already queued this frame still name them).
    void BeginFrame();

    // Lays a cp1252 string out as DrawText did, top-left at (int)pos, in the
    // named face at `size` (the GDI cell height, logical px). Bakes and
    // uploads what it needs.
    TextLayout Layout(const std::string& cp1252, const std::string& face, float size, glm::vec2 pos);

    // The font file a face resolves to on this machine ("" when none of its
    // candidates exist, in which case Layout returns no glyphs).
    std::string FaceFile(const std::string& face);

    // %WINDIR%\Fonts.
    static std::string FontsDirectory();

    std::size_t AtlasCount() const { return m_atlases.size(); }

private:
    struct Font;
    struct Atlas;

    Font* fontFor(const std::string& face);
    Atlas& atlasFor(Font& font, int rasterPx);
    void bake(Font& font, Atlas& atlas);
    void retire(const std::string& key);

    Supersonic::TextureRegistry* m_textures = nullptr;
    float m_scale = 1.0f;
    std::uint64_t m_frame = 0;
    std::uint64_t m_serial = 0;
    std::map<std::string, std::string> m_faceFiles;             // lower-case face -> file ("" = none)
    std::map<std::string, std::unique_ptr<Font>> m_fonts;       // by file
    std::map<std::string, std::unique_ptr<Atlas>> m_atlases;    // by file + "|" + raster px
    std::vector<std::string> m_retired;                          // keys to free at the next BeginFrame
};

} // namespace Penumbra::Render
