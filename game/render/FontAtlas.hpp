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
// PORTABLE: those faces are Microsoft's and exist only on Windows. Everywhere
// else - and on a Windows without the face, before any other system font -
// an open-licence STAND-IN bundled in game/data/fonts (README.md there has
// each file's source, version and licence) is drawn with the metrics of the
// face it replaces, so every layout the scripts compute still fits:
//   "Arial Narrow" -> LiberationSans-Bold.ttf, advances x 0.82
//   "Arial"        -> LiberationSans-Bold.ttf
//   "Arial Black"  -> DejaVuSans-Bold.ttf
//   "Verdana"      -> DejaVuSans-Bold.ttf
// Liberation Sans is metric-compatible with Arial, and Arial Narrow is Arial
// condensed to 82%: LiberationSans-Bold's advances x 0.82 are ARIALNB.TTF's to
// within 1/2048 em for every cp1252 character but six the game never draws
// (no-break space, macron, degree, plus-minus, micro, division sign), and
// FontAtlas's whole-pixel advances make the game's lines exactly as wide as
// ARIALNB's at 16, 30 and 40 px (fontTools, Windows 11's files). The cell (usWinAscent,
// usWinDescent) and the tab unit (xAvgCharWidth) are the Windows face's own,
// measured from Windows 11's files, so a stand-in's text is as tall, sits on
// the same baseline and tabs to the same stops. DejaVu Sans is not
// metric-compatible with Arial Black or Verdana, only close (their advances
// within a few percent on average); given their cells, its glyphs are drawn
// at their size.
//
// ENHANCED: the glyphs are rasterised at the WINDOW's resolution (the logical
// size times View::scale), not at the original's 1024x768, so text stays sharp
// at 1080p and 4K; its layout is the original's, scaled. Glyph advances are
// whole pixels at that resolution, as GDI's were at its own.
//
// ONE BYTE BEYOND CP1252 (ENHANCEMENT E21, eth/Text.hpp): 0x8D, which cp1252
// leaves undefined, is drawn as U+0107 (c with acute), for the enhanced
// edition's credit ("Cvetanovi" + c-acute, with the diacritic, as decided).
// Every face the scripts name has it: the Windows files (ARIALNB.TTF,
// arialbd.ttf, ariblk.ttf, verdanab.ttf) and both stand-ins (Liberation Sans
// Bold, DejaVu Sans Bold) map U+0107 in their cmap (test_pn_render_hud checks
// whichever files this machine resolves each face to).
//
// An atlas is baked per (face file, pixel size) on first use: every cp1252 byte
// 0x20-0xFF (the other four undefined ones as U+FFFD, through
// Eth::Cp1252CodePoint). Above
// kFullAtlasMaxPx only the characters actually drawn are baked (the 256-pixel
// clock is digits and ':'), and a new character rebakes that atlas. An atlas
// unused for a few seconds - a size left behind by a window resize - is freed.
//
// CODE POINTS (ENHANCEMENT E24). Text is laid out by Unicode code point: the
// translations of E24's languages are UTF-8 (render/Localization.hpp). A code
// point cp1252 has is drawn from its byte's slot, exactly as before, so
// Portuguese and English come out pixel for pixel as they did; any other is
// baked on demand into the same atlas, beside them - the whole of a string's
// new characters at once, the first time it is drawn, so a sign costs one
// rebake, and the new characters of every text of a frame at once when they
// are handed to Prepare first (HudRenderer does), so a screen costs one. A
// glyph that no longer fits the atlas's 4096x4096 is logged, not silently
// dropped.
//
// FONTS BY SCRIPT, the same on every platform: hiragana, katakana, CJK
// ideographs, CJK punctuation and the full-width forms from the bundled
// NotoSansJP-Bold.ttf, Arabic from the bundled NotoSansArabic-Bold.ttf (subsets
// of Noto made by tools/l10n/make_fonts.py; game/data/fonts/README.md), since
// no Windows face the scripts name has Japanese and only one has Arabic.
// Everything else - Latin, Turkish, Cyrillic - comes from the face itself as
// above. A glyph from one of the two is scaled em to em to the line's face
// (the same em, whatever the files' units), sits on its baseline, and is not
// condensed as Arial Narrow's stand-in is: its advances are its own.

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
    unsigned char byte = 0;    // the cp1252 byte drawn (0 for a code point cp1252 does not have)
    char32_t codePoint = 0;    // the code point drawn
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

// E24: where each line of a block starts. Right: each line flush with the
// right end of the block's widest line, which starts at the text's position -
// how a right-to-left paragraph is drawn at the left-anchored places the
// scripts give (render/ArabicShaping.hpp). RightEdge: the same, the text's x
// being where the widest line ENDS (rounded, not truncated as a left anchor
// is) - a right-to-left paragraph set in a box (Eth::HudCmd::rtlRight).
enum class LineAlign { Left, Right, RightEdge };

class FontAtlas {
public:
    // Characters baked on demand above this raster cell height.
    static constexpr int kFullAtlasMaxPx = 128;
    // Frames (BeginFrame calls) an atlas may go unused before it is freed.
    static constexpr std::uint64_t kEvictAfterFrames = 600;
    // E24's bundled faces, in the bundled fonts folder.
    static constexpr const char* kJapaneseFile = "NotoSansJP-Bold.ttf";
    static constexpr const char* kArabicFile = "NotoSansArabic-Bold.ttf";

    // E24: which face a code point is drawn from: the text's own, or one of
    // the two above (CODE POINTS and FONTS BY SCRIPT at the top).
    enum class Script { Face, Japanese, Arabic };
    static Script ScriptOf(char32_t codePoint);

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

    // Lays text out as DrawText did, top-left at (int)pos (RightEdge: top-right), in the named face
    // at `size` (the GDI cell height, logical px). Bakes and uploads what it
    // needs. The code points in the order they are drawn (left to right: an
    // Arabic line is shaped and reordered first, render/ArabicShaping.hpp).
    TextLayout LayoutCodePoints(const std::u32string& text, const std::string& face, float size, glm::vec2 pos,
                                LineAlign align = LineAlign::Left);
    // The same for UTF-8 (a translation), read as it is: no shaping.
    TextLayout LayoutUtf8(const std::string& utf8, const std::string& face, float size, glm::vec2 pos);
    // The same for the scripts' cp1252 (0x8D as U+0107, E21).
    TextLayout LayoutCp1252(const std::string& cp1252, const std::string& face, float size, glm::vec2 pos);

    // E24: the characters a text will need, wanted now and baked by the next
    // Layout of that face and size. HudRenderer hands every text of a frame
    // here before laying any out, so an atlas the frame's texts grow is baked
    // (and uploaded) once, not once per text: the first frame of a Japanese
    // screen brings a dozen texts of new characters into one atlas. The code
    // points as LayoutCodePoints will get them (an Arabic line shaped first).
    void Prepare(const std::u32string& text, const std::string& face, float size);
    // For the suites: the bakes so far, each one an atlas rasterised and uploaded.
    std::uint64_t BakeCount() const { return m_serial; }

    // The font file a face resolves to on this machine ("" when none of its
    // candidates exist, in which case Layout returns no glyphs).
    std::string FaceFile(const std::string& face);
    // Whether that file is a bundled stand-in rather than the face itself.
    bool FaceIsStandIn(const std::string& face);

    // For the suites: the glyph (the font file's own index; 0 is .notdef,
    // i.e. none) this atlas draws a cp1252 byte with in a face, and the glyph
    // the same file has for a Unicode code point - so a suite can see E21's
    // 0x8D drawn as U+0107 in whichever file the face resolved to.
    int GlyphForByte(const std::string& face, unsigned char byte);
    int GlyphForCodePoint(const std::string& face, unsigned codePoint);
    // E24: the glyph a code point is drawn with in text of this face - from
    // the face, or from the bundled face its script goes to - and that file
    // ("" when there is none); 0 when the file has no glyph for it.
    int RoutedGlyph(const std::string& face, char32_t codePoint);
    std::string RoutedFile(const std::string& face, char32_t codePoint);

    // %WINDIR%\Fonts on Windows; "" elsewhere, where no system face is looked
    // for and the bundled stand-ins are the only candidates.
    static std::string FontsDirectory();

    // Where the stand-ins are: <data>/fonts. PENUMBRA_DATA_DIR's until the
    // layer names the data folder it found (a packaged game's is beside the
    // executable).
    void SetBundledFontsDirectory(const std::string& directory);
    const std::string& BundledFontsDirectory() const { return m_bundledDir; }
    static std::string DefaultBundledFontsDirectory();
    // Off: the system's faces are never asked for, only the stand-ins - so a
    // suite on Windows can measure a stand-in against the face it replaces.
    void SetSystemFontsEnabled(bool enabled);

    std::size_t AtlasCount() const { return m_atlases.size(); }

private:
    struct Font;
    struct Atlas;

    // What a face resolved to.
    struct FaceChoice {
        std::string file;            // "" = none found
        std::string standIn;         // "" for a system file; the stand-in's name (its atlas key) otherwise
        float widthScale = 1.0f;     // applied to every advance and glyph width
        int winAscent = 0;           // the replaced face's, per 2048 em units; 0 = the file's own
        int winDescent = 0;
        int avgCharWidth = 0;
    };

    const FaceChoice& choiceFor(const std::string& face);
    Font* fontFor(const std::string& face);
    // E24: the bundled face for a script (null for Script::Face, or when the
    // file is missing, which is logged once).
    Font* scriptFont(Script script);
    Font* loadFont(const std::string& file, const std::string& id, const std::string& stem);
    Atlas& atlasFor(Font& font, int rasterPx);
    // The raster cell a text of `size` logical px is drawn at.
    int rasterPxFor(float size) const;
    // E24: a slot for a code point cp1252 does not have, measured.
    void addExtra(Font& font, Atlas& atlas, char32_t codePoint);
    // E24: marks what `text` needs that the atlas lacks as wanted (and
    // measures the new extras); true when that was anything.
    bool want(Font& font, Atlas& atlas, const std::u32string& text);
    void bake(Font& font, Atlas& atlas);
    void retire(const std::string& key);

    Supersonic::TextureRegistry* m_textures = nullptr;
    float m_scale = 1.0f;
    std::uint64_t m_frame = 0;
    std::uint64_t m_serial = 0;
    std::string m_bundledDir;
    bool m_systemFonts = true;
    std::map<std::string, FaceChoice> m_faceChoices;            // lower-case face -> what it resolved to
    std::map<Script, std::string> m_scriptFiles;                 // E24: the bundled file each script resolved to
    std::map<std::string, std::unique_ptr<Font>> m_fonts;       // by file + "#" + stand-in name
    std::map<std::string, std::unique_ptr<Atlas>> m_atlases;    // by that + "|" + raster px
    std::vector<std::string> m_retired;                          // keys to free at the next BeginFrame
};

} // namespace Penumbra::Render
