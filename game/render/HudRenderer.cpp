#include "render/HudRenderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <utility>

#include "core/Log.hpp"
#include "eth/Text.hpp"
#include "render/CameraRig.hpp"
#include "render/FontAtlas.hpp"
#include "render/Localization.hpp"
#include "render/TextureCache.hpp"
#include "renderer/TextureRegistry.hpp"

namespace Penumbra::Render {
namespace {

using Quad = Supersonic::ScreenOverlay::Quad;

// Strips per axis when a gradient has no texture: a 391x768 panel in 48 rows
// steps its alpha by about three levels a row, under what the eye reads as a band.
constexpr int kStripsOneAxis = 48;
constexpr int kStripsGrid = 12;

// E25: fit factors remembered before the memo starts again (a menu has a
// dozen panels a language).
constexpr std::size_t kMaxFitMemo = 512;
// E25: how far past the box a fitted text may reach, logical px - under a
// rounding's worth - and the steps a fit takes down from its estimate.
constexpr float kFitSlack = 0.5f;
constexpr float kFitStep = 0.98f;
constexpr int kFitSteps = 32;
// E25: the gap between two columns, in the text's own size, and how much
// larger two columns must set a group than one before they are used.
constexpr float kColumnGapEm = 0.6f;
constexpr float kColumnGain = 1.1f;
// E25: of the breaks within this much of the largest factor, the one that
// leaves the two columns nearest in height is taken (near the middle).
constexpr float kColumnBalance = 0.97f;
// E25: the halvings that refine a fit once a step down has found one.
constexpr int kFitRefinements = 6;

// E25: a line's box, logical px.
struct LineBox {
    glm::vec2 min{0.0f};
    glm::vec2 max{0.0f};
    // Whether its right end must be in the box: not a right-to-left text's,
    // which is set against rtlRight - the box's right, or its shadow's past it.
    bool rightInBox = true;
    bool Crosses(const glm::vec2& otherMin, const glm::vec2& otherMax) const {
        return min.x < otherMax.x && max.x > otherMin.x && min.y < otherMax.y && max.y > otherMin.y;
    }
};

// E25: a text's lines as FontAtlas breaks them - CR LF once, a lone CR, LF -
// without the breaks.
std::vector<std::u32string> SplitLines(const std::u32string& text) {
    std::vector<std::u32string> lines(1);
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char32_t c = text[i];
        if (c == U'\r' && i + 1 < text.size() && text[i + 1] == U'\n') continue;
        if (c == U'\r' || c == U'\n') {
            lines.emplace_back();
            continue;
        }
        lines.back().push_back(c);
    }
    return lines;
}

bool BlankLine(const std::u32string& line) {
    return std::all_of(line.begin(), line.end(), [](const char32_t c) { return c <= U' '; });
}

// E25: where a text may be broken into two columns: the first blank line of
// each run of them that has a line with something on it before and after.
std::vector<int> ColumnBreaks(const std::u32string& text) {
    const std::vector<std::u32string> lines = SplitLines(text);
    std::vector<int> breaks;
    bool before = false;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (!BlankLine(lines[i])) {
            before = true;
            continue;
        }
        if (!before || (i > 0 && BlankLine(lines[i - 1]))) continue;
        const bool after =
            std::any_of(lines.begin() + static_cast<std::ptrdiff_t>(i) + 1, lines.end(),
                        [](const std::u32string& line) { return !BlankLine(line); });
        if (after) breaks.push_back(static_cast<int>(i));
    }
    return breaks;
}

// E25: the two columns of a text broken at blank line `at`: the lines before
// it, and those after it from the first with something on it; each with its
// hand-made breaks.
std::array<std::u32string, 2> SplitColumns(const std::u32string& text, const int at) {
    const std::vector<std::u32string> lines = SplitLines(text);
    const std::size_t split = std::min(lines.size(), static_cast<std::size_t>(std::max(0, at)));
    std::size_t from = split + 1;
    while (from < lines.size() && BlankLine(lines[from])) ++from;
    const auto join = [&lines](const std::size_t begin, const std::size_t end) {
        std::u32string out;
        for (std::size_t i = begin; i < end; ++i) {
            if (i > begin) out.push_back(U'\n');
            out += lines[i];
        }
        return out;
    };
    return {join(0, split), join(std::min(from, lines.size()), lines.size())};
}

std::array<std::uint8_t, 4> Bytes(const Eth::uint argb) {
    return {static_cast<std::uint8_t>((argb >> 16) & 0xFFu), static_cast<std::uint8_t>((argb >> 8) & 0xFFu),
            static_cast<std::uint8_t>(argb & 0xFFu), static_cast<std::uint8_t>((argb >> 24) & 0xFFu)};
}

Quad Plain(const glm::vec2& min, const glm::vec2& max, const glm::vec4& color) {
    Quad quad;
    quad.min = min;
    quad.max = max;
    quad.color = color;
    return quad;
}

} // namespace

glm::vec4 HudRenderer::ToColor(const Eth::uint argb) {
    const auto bytes = Bytes(argb);
    return glm::vec4(bytes[0], bytes[1], bytes[2], bytes[3]) / 255.0f;
}

void HudRenderer::Attach(entt::registry& registry, TextureCache& textures, FontAtlas& fonts,
                         Localization& localization) {
    m_textures = &textures;
    m_fonts = &fonts;
    m_localization = &localization;
    auto* const* slot = registry.ctx().find<Supersonic::TextureRegistry*>();
    m_registryTextures = slot != nullptr ? *slot : nullptr;
    m_gradients.clear();
}

void HudRenderer::Detach() {
    m_textures = nullptr;
    m_fonts = nullptr;
    m_localization = nullptr;
    m_registryTextures = nullptr;
    m_gradients.clear();
}

void HudRenderer::Draw(entt::registry& registry, const Eth::RenderSnapshot& snapshot, const View& view,
                       const std::vector<Eth::HudCmd>* extra) {
    auto* const* slot = registry.ctx().find<Supersonic::ScreenOverlay*>();
    if (slot == nullptr || *slot == nullptr) return;
    Supersonic::ScreenOverlay& overlay = **slot;

    // The previous frame's overlay has been drawn by now, so atlases retired
    // while it was built can go.
    if (m_fonts != nullptr) m_fonts->BeginFrame();

    m_quads.clear();
    Build(snapshot, view, m_quads, extra);
    for (Quad& quad : m_quads) overlay.Add(std::move(quad));

    if (overlay.DroppedQuads() > 0 && !m_loggedDrops) {
        m_loggedDrops = true;
        SUPERSONIC_LOG_WARN("Penumbra") << "hud: the screen overlay is full (" << Supersonic::ScreenOverlay::kMaxQuads
                                        << " quads); " << overlay.DroppedQuads() << " dropped this frame";
    }
}

void HudRenderer::Build(const Eth::RenderSnapshot& snapshot, const View& view, std::vector<Quad>& out,
                        const std::vector<Eth::HudCmd>* extra) {
    if (m_fonts != nullptr) m_fonts->SetRasterScale(view.scale);
    // E25: the fit groups' factors, at this frame's raster scale, before any
    // text is prepared at its size.
    computeFits(snapshot.hud);
    // E24: every text's characters first, so each atlas this frame grows is
    // baked once for all of them rather than once per text that brings some.
    for (const Eth::HudCmd& cmd : snapshot.hud) prepareText(cmd);
    if (extra != nullptr) {
        for (const Eth::HudCmd& cmd : *extra) prepareText(cmd);
    }
    for (const Eth::HudCmd& cmd : snapshot.hud) addCommand(cmd, view, out);
    // The layer's own commands, over the scripts' and under the bars, which
    // stay last.
    if (extra != nullptr) {
        for (const Eth::HudCmd& cmd : *extra) addCommand(cmd, view, out);
    }
    addBars(view, out);
}

void HudRenderer::addCommand(const Eth::HudCmd& cmd, const View& view, std::vector<Quad>& out) {
    switch (cmd.kind) {
        case Eth::HudCmd::Kind::Text: addText(cmd, view, out); break;
        case Eth::HudCmd::Kind::Sprite: addSprite(cmd, view, false, out); break;
        case Eth::HudCmd::Kind::ShapedSprite: addSprite(cmd, view, true, out); break;
        case Eth::HudCmd::Kind::Rectangle: addRectangle(cmd, view, out); break;
    }
}

bool HudRenderer::drawsText(const Eth::HudCmd& cmd) const {
    // Every blend mode 0.7.12 set alpha-tested at > 1/255 (docs/spec/30 §3.3):
    // a transparent text draws nothing, and costs no quads here.
    return cmd.kind == Eth::HudCmd::Kind::Text && m_fonts != nullptr && !cmd.text.empty() && cmd.fontSize > 0.0f &&
           ToColor(cmd.color).a > 0.0f;
}

const std::u32string& HudRenderer::visualText(const Eth::HudCmd& cmd) {
    // E24: the translation is UTF-8, laid out by code point; Arabic shaped and
    // put in drawing order first (render/ArabicShaping.hpp). Both lookups are
    // remembered, so asking twice a frame (prepareText, addText) costs two
    // hash lookups. The reference is used at once: the memo may be cleared by
    // the next call.
    const std::string text =
        m_localization != nullptr ? m_localization->Translate(cmd.text) : Eth::Cp1252ToUtf8(cmd.text);
    return m_visual.Of(text, rightToLeft());
}

bool HudRenderer::rightToLeft() const { return m_localization != nullptr && m_localization->RightToLeft(); }

void HudRenderer::prepareText(const Eth::HudCmd& cmd) {
    if (cmd.fit.group != 0 && m_fitScales.count(cmd.fit.group) != 0) {
        Eth::HudCmd scaled = Fitted(cmd);
        scaled.fit.group = 0;   // E25: prepared at the size it is drawn at
        prepareText(scaled);
        return;
    }
    if (drawsText(cmd)) m_fonts->Prepare(visualText(cmd), cmd.font, cmd.fontSize);
}

float HudRenderer::FitScale(const Eth::uint group) const {
    const auto it = m_fitScales.find(group);
    return it != m_fitScales.end() ? it->second.scale : 0.0f;
}

int HudRenderer::FitColumnBreak(const Eth::uint group) const {
    const auto it = m_fitScales.find(group);
    return it != m_fitScales.end() ? it->second.columnBreak : -1;
}

Eth::HudCmd HudRenderer::Fitted(const Eth::HudCmd& cmd) const {
    const auto it = m_fitScales.find(cmd.fit.group);
    if (cmd.kind != Eth::HudCmd::Kind::Text || cmd.fit.group == 0 || it == m_fitScales.end()) return cmd;
    const float f = it->second.scale;
    Eth::HudCmd scaled = cmd;
    scaled.fontSize = cmd.fontSize * f;
    scaled.pos = cmd.fit.min + (cmd.pos - cmd.fit.min) * f;
    // A right-to-left text ends at rtlRight: as far in from the box's right
    // as its left inset, scaled, so the panel's lines keep one edge.
    if (cmd.rtlRight > 0.0f) scaled.rtlRight = cmd.fit.max.x - (cmd.fit.max.x - cmd.rtlRight) * f;
    return scaled;
}

void HudRenderer::computeFits(const std::vector<Eth::HudCmd>& cmds) {
    m_fitScales.clear();
    if (m_fonts == nullptr) return;
    std::map<Eth::uint, std::vector<const Eth::HudCmd*>> groups;
    for (const Eth::HudCmd& cmd : cmds) {
        if (cmd.fit.group != 0 && drawsText(cmd)) groups[cmd.fit.group].push_back(&cmd);
    }
    for (const auto& [group, members] : groups) {
        // Everything the factor follows from: the texts as they are drawn
        // (translated, shaped), their faces, sizes and places, the box and
        // its bounds, and the raster scale the metrics are taken at.
        const Eth::TextFit& fit = members.front()->fit;
        std::string key = std::to_string(m_fonts->RasterScale()) + (rightToLeft() ? "R" : "L");
        for (const float value : {fit.min.x, fit.min.y, fit.max.x, fit.max.y, fit.minScale, fit.maxScale,
                                  fit.avoidMin.x, fit.avoidMin.y, fit.avoidMax.x, fit.avoidMax.y}) {
            key += '|' + std::to_string(value);
        }
        for (const Eth::HudCmd* cmd : members) {
            key += '|' + cmd->font + '|' + std::to_string(cmd->fontSize) + ',' + std::to_string(cmd->pos.x) + ',' +
                   std::to_string(cmd->pos.y) + ',' + std::to_string(cmd->rtlRight) + ',' +
                   std::to_string(cmd->fit.columns) + '|';
            const std::u32string& text = visualText(*cmd);
            key.append(reinterpret_cast<const char*>(text.data()), text.size() * sizeof(char32_t));
        }
        auto memo = m_fitMemo.find(key);
        if (memo == m_fitMemo.end()) {
            if (m_fitMemo.size() >= kMaxFitMemo) m_fitMemo.clear();
            memo = m_fitMemo.emplace(std::move(key), fitChoice(members)).first;
        }
        m_fitScales[group] = memo->second;
    }
}

HudRenderer::FitChoice HudRenderer::fitChoice(const std::vector<const Eth::HudCmd*>& group) {
    const Eth::TextFit& fit = group.front()->fit;
    const float least = std::max(0.01f, fit.minScale);
    const float most = std::max(least, fit.maxScale);
    const glm::vec2 box = fit.max - fit.min;
    if (!(box.x > 0.0f) || !(box.y > 0.0f)) return FitChoice{least, -1, 0.0f};
    const bool rtl = rightToLeft();
    const bool avoiding = fit.avoidMax.x > fit.avoidMin.x && fit.avoidMax.y > fit.avoidMin.y;
    // The texts as drawn, kept: visualText's memo may be cleared by the next call.
    std::vector<std::u32string> texts;
    texts.reserve(group.size());
    for (const Eth::HudCmd* cmd : group) texts.push_back(visualText(*cmd));
    // The texts that may take two columns, which must be one text (a body and
    // its shadow) to be broken at one line; how far the highest of them is
    // below the box's top, which a second column may rise by.
    int columnText = -1;
    bool oneColumnText = true;
    float rise = 0.0f;
    for (std::size_t i = 0; i < group.size(); ++i) {
        if (group[i]->fit.columns < 2) continue;
        if (columnText < 0) {
            columnText = static_cast<int>(i);
            rise = group[i]->pos.y - fit.min.y;
        } else if (texts[i] != texts[static_cast<std::size_t>(columnText)]) {
            oneColumnText = false;
        }
        rise = std::min(rise, group[i]->pos.y - fit.min.y);
    }
    rise = std::max(0.0f, rise);

    // Everything laid out at a factor, the column texts broken at `at` (-1:
    // whole), their second column raised to the box's top or not: each line's
    // box - from its text's scaled place, flush left, or for a right-to-left
    // language flush with its widest and set against rtlRight - and whether
    // all of them lie in the box, clear of the avoid box, and a raised second
    // column clear of every line that is not a column text's (the title).
    std::vector<float> widths;
    std::vector<LineBox> lines;
    std::vector<LineBox> raised;
    const auto fits = [&](const float f, const int at, const bool up) {
        lines.clear();
        raised.clear();
        for (std::size_t m = 0; m < group.size(); ++m) {
            const Eth::HudCmd& cmd = *group[m];
            const bool split = at >= 0 && cmd.fit.columns >= 2;
            std::array<std::u32string, 2> parts;
            if (split) parts = SplitColumns(texts[m], at);
            else parts[0] = texts[m];
            const float size = cmd.fontSize * f;
            const bool fromRight = rtl && cmd.rtlRight > 0.0f;
            const glm::vec2 pos = fit.min + (cmd.pos - fit.min) * f;
            const float right = fit.max.x - (fit.max.x - cmd.rtlRight) * f;
            float offset = 0.0f;   // the column's start from the text's own, across
            for (std::size_t c = 0; c < (split ? 2u : 1u); ++c) {
                const FontAtlas::Extent extent = m_fonts->Measure(parts[c], cmd.font, size, &widths);
                const float top = pos.y - (c == 1 && up ? rise * f : 0.0f);
                for (std::size_t i = 0; i < widths.size(); ++i) {
                    if (!(widths[i] > 0.0f)) continue;
                    float x0 = pos.x + offset;
                    if (fromRight) x0 = right - offset - widths[i];
                    else if (rtl) x0 += extent.width - widths[i];
                    const float y0 = top + static_cast<float>(i) * extent.lineHeight;
                    const LineBox line{{x0, y0}, {x0 + widths[i], y0 + extent.lineHeight}, !fromRight};
                    (c == 1 && up ? raised : lines).push_back(line);
                }
                offset += extent.width + kColumnGapEm * size;
            }
        }
        const glm::vec2 lo = fit.min - glm::vec2(kFitSlack);
        const glm::vec2 hi = fit.max + glm::vec2(kFitSlack);
        for (const std::vector<LineBox>* set : {&lines, &raised}) {
            for (const LineBox& line : *set) {
                if (line.min.x < lo.x || line.min.y < lo.y || (line.rightInBox && line.max.x > hi.x) ||
                    line.max.y > hi.y) {
                    return false;
                }
                if (avoiding && line.Crosses(fit.avoidMin, fit.avoidMax)) return false;
            }
        }
        for (const LineBox& line : raised) {
            for (const LineBox& other : lines) {
                if (line.Crosses(other.min, other.max)) return false;
            }
        }
        return true;
    };
    // Sizes scale all but linearly (whole raster pixels): an estimate from
    // the least, then down in small steps until everything fits, then the
    // last step halved a few times. The least is kept even where nothing
    // fits: never smaller than without E25.
    const auto solve = [&](const int at, const bool up) {
        float f = most;
        fits(least, at, up);   // the lines at the least, whether or not they fit
        glm::vec2 reach(0.0f);
        for (const std::vector<LineBox>* set : {&lines, &raised}) {
            for (const LineBox& line : *set) {
                reach.x = std::max(reach.x, rtl ? fit.max.x - line.min.x : line.max.x - fit.min.x);
                reach.y = std::max(reach.y, line.max.y - fit.min.y);
            }
        }
        if (reach.x > 0.0f) f = std::min(f, least * box.x / reach.x);
        if (reach.y > 0.0f) f = std::min(f, least * box.y / reach.y);
        f = std::clamp(f, least, most);
        float over = f;
        for (int i = 0; i < kFitSteps && f > least && !fits(f, at, up); ++i) {
            over = f;
            f = std::max(least, f * kFitStep);
        }
        bool ok = fits(f, at, up);
        if (ok && over > f) {
            for (int i = 0; i < kFitRefinements; ++i) {
                const float mid = 0.5f * (f + over);
                (fits(mid, at, up) ? f : over) = mid;
            }
        }
        return std::make_pair(f, ok);
    };

    const auto [single, singleFits] = solve(-1, false);
    FitChoice choice{single, -1, 0.0f};
    if (columnText < 0 || !oneColumnText || (singleFits && single >= most)) return choice;
    // Two columns, broken at a blank line, the second raised to the box's top
    // or not: of those that set the group within kColumnBalance of the
    // largest, the one whose columns are nearest in lines - used where they
    // fit and one does not, or set it clearly larger.
    struct Candidate {
        int at = -1;
        bool up = false;
        float scale = 0.0f;
        bool fits = false;
        int imbalance = 0;
    };
    std::vector<Candidate> candidates;
    const std::u32string& columnWhole = texts[static_cast<std::size_t>(columnText)];
    for (const int at : ColumnBreaks(columnWhole)) {
        const std::array<std::u32string, 2> parts = SplitColumns(columnWhole, at);
        const auto count = [](const std::u32string& part) {
            return static_cast<int>(std::count(part.begin(), part.end(), U'\n')) + 1;
        };
        for (const bool up : {false, true}) {
            if (up && !(rise > 0.0f)) continue;
            const auto [f, ok] = solve(at, up);
            candidates.push_back(Candidate{at, up, f, ok, std::abs(count(parts[0]) - count(parts[1]))});
        }
    }
    bool anyFits = false;
    for (const Candidate& c : candidates) anyFits = anyFits || c.fits;
    float largest = 0.0f;
    for (const Candidate& c : candidates) {
        if (c.fits == anyFits) largest = std::max(largest, c.scale);
    }
    const Candidate* pick = nullptr;
    for (const Candidate& c : candidates) {
        if (c.fits != anyFits || c.scale < largest * kColumnBalance) continue;
        if (pick == nullptr || c.imbalance < pick->imbalance ||
            (c.imbalance == pick->imbalance && c.scale > pick->scale)) {
            pick = &c;
        }
    }
    if (pick != nullptr &&
        ((pick->fits && !singleFits) || (pick->fits == singleFits && pick->scale >= single * kColumnGain))) {
        choice = FitChoice{pick->scale, pick->at, pick->up ? rise : 0.0f};
    }
    return choice;
}

void HudRenderer::addText(const Eth::HudCmd& cmd, const View& view, std::vector<Quad>& out) {
    if (cmd.fit.group != 0 && m_fitScales.count(cmd.fit.group) != 0) {
        Eth::HudCmd scaled = Fitted(cmd);
        scaled.fit.group = 0;   // E25: drawn at its group's factor
        const int at = m_fitScales[cmd.fit.group].columnBreak;
        if (at < 0 || cmd.fit.columns < 2 || !drawsText(scaled)) {
            addText(scaled, view, out);
            return;
        }
        // E25: in two columns, the first where the text is, the second a
        // gap past its widest line: to the right, or for a right-to-left
        // language (rtlRight) to the left, so they read in its order.
        const FitChoice& choice = m_fitScales[cmd.fit.group];
        const std::array<std::u32string, 2> columns = SplitColumns(visualText(cmd), at);
        const float gap = kColumnGapEm * scaled.fontSize;
        const float first = m_fonts->Measure(columns[0], scaled.font, scaled.fontSize).width;
        Eth::HudCmd second = scaled;
        if (rightToLeft() && scaled.rtlRight > 0.0f) second.rtlRight = scaled.rtlRight - first - gap;
        else second.pos.x = scaled.pos.x + first + gap;
        // Raised by as much as the column texts are below the box's top.
        second.pos.y -= choice.columnRise * choice.scale;
        addTextCodePoints(scaled, columns[0], view, out);
        addTextCodePoints(second, columns[1], view, out);
        return;
    }
    if (!drawsText(cmd)) return;
    addTextCodePoints(cmd, visualText(cmd), view, out);
}

void HudRenderer::addTextCodePoints(const Eth::HudCmd& cmd, const std::u32string& text, const View& view,
                                    std::vector<Quad>& out) {
    const glm::vec4 color = ToColor(cmd.color);

    // A right-to-left paragraph's lines flush with its widest; set in a box
    // (rtlRight), the widest ends at the box's right inset.
    glm::vec2 pos = cmd.pos;
    LineAlign align = LineAlign::Left;
    if (rightToLeft()) {
        align = LineAlign::Right;
        if (cmd.rtlRight > 0.0f) {
            pos.x = cmd.rtlRight;
            align = LineAlign::RightEdge;
        }
    }
    const TextLayout layout = m_fonts->LayoutCodePoints(text, cmd.font, cmd.fontSize, pos, align);
    if (layout.texture.empty()) return;
    for (const TextGlyph& glyph : layout.glyphs) {
        Quad quad;
        quad.min = view.HudToFraction(glyph.min);
        quad.max = view.HudToFraction(glyph.max);
        // Wholly off the image: nothing to see, and the overlay's quad budget
        // is better spent. The lore signs (ETHCallback_story) are world text
        // drawn while their bucket is, often half off-screen.
        if (quad.max.x <= 0.0f || quad.max.y <= 0.0f || quad.min.x >= 1.0f || quad.min.y >= 1.0f) continue;
        quad.uvMin = glyph.uvMin;
        quad.uvMax = glyph.uvMax;
        quad.color = color;
        quad.texture = layout.texture;
        out.push_back(std::move(quad));
    }
}

void HudRenderer::addSprite(const Eth::HudCmd& cmd, const View& view, const bool stretched, std::vector<Quad>& out) {
    if (m_textures == nullptr || cmd.sprite.empty()) return;
    const glm::vec4 color = ToColor(cmd.color);
    if (color.a <= 0.0f) return;

    // ENHANCED: an image with Portuguese in its pixels has an English variant
    // once one is made (strings.json "images"); the same size, so the scripts'
    // rectangles still fit it.
    std::string path = cmd.sprite;
    if (m_localization != nullptr) {
        std::string variant = m_localization->ImageVariant(path, m_localization->CurrentLanguage());
        if (!variant.empty()) path = std::move(variant);
    }
    const std::string key = m_textures->Key(path, TextureVariant::Sprite);
    if (key.empty()) return;

    const glm::vec2 image(m_textures->Size(path));
    const bool hasRect = cmd.spriteRectMax.x > cmd.spriteRectMin.x && cmd.spriteRectMax.y > cmd.spriteRectMin.y;
    const glm::vec2 rectMin = hasRect ? cmd.spriteRectMin : glm::vec2(0.0f);
    const glm::vec2 rectMax = hasRect ? cmd.spriteRectMax : image;

    glm::vec2 size = cmd.size;
    if (!stretched) {
        // DrawSprite: the current rectangle at bitmap size.
        if (hasRect) size = rectMax - rectMin;
        else if (size.x <= 0.0f || size.y <= 0.0f) size = image;
    }
    if (size.x == 0.0f || size.y == 0.0f) return;

    Quad quad;
    quad.min = view.HudToFraction(cmd.pos);
    quad.max = view.HudToFraction(cmd.pos + size);
    if (image.x > 0.0f && image.y > 0.0f) {
        quad.uvMin = rectMin / image;
        quad.uvMax = rectMax / image;
    }
    quad.color = color;
    quad.texture = key;
    out.push_back(std::move(quad));
}

std::string HudRenderer::gradientTexture(const Eth::HudCmd& cmd) {
    if (m_registryTextures == nullptr) return {};
    char name[64];
    std::snprintf(name, sizeof(name), "penumbra:hud:gradient:%08X%08X%08X%08X", cmd.color, cmd.color1, cmd.color2,
                  cmd.color3);
    std::string key(name);
    if (m_gradients.count(key) != 0) return key;
    if (m_gradients.size() >= kMaxGradients) return {};

    // Row-major, top row first: c0 c1 / c2 c3.
    std::array<std::uint8_t, 16> pixels{};
    const Eth::uint corners[4] = {cmd.color, cmd.color1, cmd.color2, cmd.color3};
    for (std::size_t i = 0; i < 4; ++i) {
        const auto bytes = Bytes(corners[i]);
        for (std::size_t c = 0; c < 4; ++c) pixels[i * 4 + c] = bytes[c];
    }
    // "data:" and UNORM: how the overlay acquires its textures.
    m_registryTextures->UploadRGBA("data:" + key, pixels.data(), 2, 2, false);
    m_gradients.insert(key);
    return key;
}

void HudRenderer::addRectangle(const Eth::HudCmd& cmd, const View& view, std::vector<Quad>& out) {
    addRectangleQuads(cmd, view, out);
    if (!(view.openSides > 0.0f) || cmd.size.x == 0.0f || cmd.size.y == 0.0f) return;

    // E1's open sides: a rectangle that meets the logical screen's left or
    // right edge - a fade (fadeIn/fadeOut, util.as:379-403), drawRect's black,
    // showData's panel (menu.as:217) - goes on to the edge of what is shown,
    // in the colours of the edge it meets, as if that edge were clamped.
    // Otherwise a fade would darken the 4:3 box and leave the sides lit, and
    // the panel would stop in mid-floor. Nothing else is stretched.
    constexpr float kEdge = 0.5f;   // showData's right edge is 1024 - 0.382 x 1024 + 0.382 x 1024, in floats
    const float left = std::min(cmd.pos.x, cmd.pos.x + cmd.size.x);
    const float right = std::max(cmd.pos.x, cmd.pos.x + cmd.size.x);
    const float top = std::min(cmd.pos.y, cmd.pos.y + cmd.size.y);
    const float height = std::abs(cmd.size.y);
    // The corners' colours as the rectangle has them on screen, whichever
    // way its size runs: c0 top-left, c1 top-right, c2 bottom-left, c3 bottom-right.
    const bool flipX = cmd.size.x < 0.0f;
    const bool flipY = cmd.size.y < 0.0f;
    const auto corner = [&cmd, flipX, flipY](bool rightSide, bool bottom) {
        const bool r = rightSide != flipX;
        const bool b = bottom != flipY;
        return b ? (r ? cmd.color3 : cmd.color2) : (r ? cmd.color1 : cmd.color);
    };
    const auto strip = [&](float from, float to, bool rightSide) {
        if (!(to - from > 0.0f)) return;
        Eth::HudCmd edge = cmd;
        edge.pos = {from, top};
        edge.size = {to - from, height};
        edge.color = edge.color1 = corner(rightSide, false);
        edge.color2 = edge.color3 = corner(rightSide, true);
        addRectangleQuads(edge, view, out);
    };
    const glm::vec2 shownMin = view.ShownLogicalMin();
    const glm::vec2 shownMax = view.ShownLogicalMax();
    // It must reach the edge from inside the screen, not lie wholly past it.
    if (left <= kEdge && right > 0.0f) strip(shownMin.x, left, false);
    if (right >= view.logicalScreen.x - kEdge && left < view.logicalScreen.x) strip(right, shownMax.x, true);
}

void HudRenderer::addRectangleQuads(const Eth::HudCmd& cmd, const View& view, std::vector<Quad>& out) {
    const Eth::uint c[4] = {cmd.color, cmd.color1, cmd.color2, cmd.color3};
    if (((c[0] | c[1] | c[2] | c[3]) >> 24) == 0) return;   // all four corners transparent
    if (cmd.size.x == 0.0f || cmd.size.y == 0.0f) return;

    const glm::vec2 min = view.HudToFraction(cmd.pos);
    const glm::vec2 max = view.HudToFraction(cmd.pos + cmd.size);
    if (c[0] == c[1] && c[0] == c[2] && c[0] == c[3]) {
        out.push_back(Plain(min, max, ToColor(c[0])));
        return;
    }

    if (const std::string key = gradientTexture(cmd); !key.empty()) {
        Quad quad;
        quad.min = min;
        quad.max = max;
        // Texel centres: at the corners the filter reads one colour whole,
        // between them it blends exactly as the vertex colours did.
        quad.uvMin = glm::vec2(0.25f);
        quad.uvMax = glm::vec2(0.75f);
        quad.texture = key;
        out.push_back(std::move(quad));
        return;
    }

    // Strips, each the blend at its centre.
    const glm::vec4 k[4] = {ToColor(c[0]), ToColor(c[1]), ToColor(c[2]), ToColor(c[3])};
    const auto at = [&k](const float u, const float v) {
        return glm::mix(glm::mix(k[0], k[1], u), glm::mix(k[2], k[3], u), v);
    };
    const bool vertical = c[0] == c[1] && c[2] == c[3];
    const bool horizontal = c[0] == c[2] && c[1] == c[3];
    const int columns = vertical ? 1 : (horizontal ? kStripsOneAxis : kStripsGrid);
    const int rows = horizontal ? 1 : (vertical ? kStripsOneAxis : kStripsGrid);
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            const glm::vec2 a(static_cast<float>(column) / static_cast<float>(columns),
                              static_cast<float>(row) / static_cast<float>(rows));
            const glm::vec2 b(static_cast<float>(column + 1) / static_cast<float>(columns),
                              static_cast<float>(row + 1) / static_cast<float>(rows));
            const glm::vec2 centre = (a + b) * 0.5f;
            out.push_back(Plain(glm::mix(min, max, a), glm::mix(min, max, b), at(centre.x, centre.y)));
        }
    }
}

void HudRenderer::addBars(const View& view, std::vector<Quad>& out) const {
    // CameraRig::Bars is the one rule for where the bars are (left, right,
    // then top and bottom between them; slivers under a pixel dropped), so the
    // HUD and anything else that draws them cannot disagree. The layer does
    // not add CameraRig::AddBars on top of this.
    const glm::vec4 black(0.0f, 0.0f, 0.0f, 1.0f);
    for (const CameraRig::Bar& bar : CameraRig::Bars(view)) out.push_back(Plain(bar.min, bar.max, black));
}

} // namespace Penumbra::Render
