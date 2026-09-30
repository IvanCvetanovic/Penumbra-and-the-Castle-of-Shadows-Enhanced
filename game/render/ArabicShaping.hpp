#pragma once

// ENHANCEMENT E24: Arabic, drawn by a font rasteriser that knows neither
// shaping nor direction (stb_truetype maps one code point to one glyph and
// lays glyphs left to right). HarfBuzz and FriBidi are not vendored, and a
// hundred-odd UI strings need only this:
//
// SHAPING. Each Arabic letter takes its isolated, final, initial or medial
// form from its neighbours' joining types (Unicode's ArabicShaping.txt: U
// non-joining, R right-joining, D dual-joining, C join-causing - the tatweel -
// and T transparent - the harakat, skipped when looking for a neighbour), as
// the Arabic Presentation Forms-B code points, which the bundled Noto Sans
// Arabic maps to the joined glyphs. Lam followed by alef (plain, madda, hamza
// above or below) becomes the one lam-alef ligature, isolated or final. The
// translations carry no harakat: nothing positions a mark over its letter.
//
// DIRECTION, one line at a time (a simplified Unicode Bidirectional
// Algorithm, UAX #9, at a single paragraph level): the letters' own direction,
// digits after Arabic kept as a number, a separator between two digits
// ("12:05") part of the number, neutrals between two runs of one direction
// taking it and others the paragraph's, a bracket pair taking the direction of
// what it encloses in its context (so "CTRL (joystick 3)" and "(Supersonic
// Engine)" stay whole), then the runs reordered - Latin words, key names and
// numbers read left to right inside a right-to-left line - and ( ) [ ] { } < >
// << >> mirrored where they read right to left. What comes out is what the
// font lays out from left to right.
//
// In a right-to-left paragraph (the Arabic UI) the options screen's switch
// marker "[x] " stays at the left in its own order, as UI chrome, and the rest
// of its line is the paragraph; a line with no letter in it - the chooser's
// "[<]" and "[>]", a number, a time - is drawn as it is. In a
// left-to-right one (every other language) an Arabic word - the chooser's name
// for Arabic - is still shaped and reads right to left in place. FontAtlas
// right-aligns a right-to-left paragraph's lines to its widest
// (LineAlign::Right); a paragraph set in a box - showData's panel, the pause's
// rows (Eth::HudCmd::rtlRight) - ends at the box's right inset
// (LineAlign::RightEdge).

#include <string>
#include <unordered_map>

namespace Penumbra::Render {

namespace ArabicShaping {

// A code point of the Arabic blocks or presentation forms (strong right to left).
bool IsArabic(char32_t codePoint);
// Joined forms and lam-alef ligatures, in logical order.
std::u32string Shape(const std::u32string& logical);
// One line, logical order in, the order to draw it from left to right out:
// shaped, resolved, reordered and mirrored. No line breaks in it.
std::u32string VisualLine(const std::u32string& logical, bool rightToLeft);
// Every line of a text, each alone; the breaks are kept where they are.
std::u32string Visual(const std::u32string& logical, bool rightToLeft);

} // namespace ArabicShaping

// What the HUD lays out for a text: its code points in drawing order,
// remembered (the HUD draws the same texts every frame). A left-to-right text
// with no Arabic in it is its own code points.
class VisualText {
public:
    const std::u32string& Of(const std::string& utf8, bool rightToLeft);

private:
    std::unordered_map<std::string, std::u32string> m_memo[2];
};

} // namespace Penumbra::Render
