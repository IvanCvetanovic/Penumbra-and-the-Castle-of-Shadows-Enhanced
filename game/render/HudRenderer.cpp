#include "render/HudRenderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
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
    if (drawsText(cmd)) m_fonts->Prepare(visualText(cmd), cmd.font, cmd.fontSize);
}

void HudRenderer::addText(const Eth::HudCmd& cmd, const View& view, std::vector<Quad>& out) {
    if (!drawsText(cmd)) return;
    const glm::vec4 color = ToColor(cmd.color);

    // A right-to-left paragraph's lines flush with its widest.
    const TextLayout layout = m_fonts->LayoutCodePoints(visualText(cmd), cmd.font, cmd.fontSize, cmd.pos,
                                                        rightToLeft() ? LineAlign::Right : LineAlign::Left);
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
