#pragma once

// The top layer: what the scripts drew with DrawText, DrawSprite,
// DrawShapedSprite and DrawRectangle (snapshot.hud), as ScreenOverlay quads.
//
// 0.7.12 drew these after the scene in screen space at depth 0, in the order
// they were called, alpha-blended (docs/spec/30-ethanon-runtime.md §3.6). The
// ScreenOverlay is exactly that: drawn over the finished image, in display
// values, in the order added. It is IMMEDIATE and cleared after every frame,
// so Draw runs on every frame (OnUpdate) and re-emits the same snapshot's HUD
// on frames without a new tick; it holds no entities.
//
//   Text          translated (Localization), shaped and ordered for drawing
//                 (E24's Arabic, ArabicShaping), laid out (FontAtlas), one
//                 quad per visible glyph in the text's colour. shadowText is
//                 already two Text commands in the snapshot. Every text's
//                 characters go to FontAtlas::Prepare before the first is laid
//                 out, so a frame that brings new ones (E24's scripts) bakes
//                 each atlas once.
//   Sprite        the loaded image (TextureCache, magenta key), its current
//                 rectangle at bitmap size, the colour multiplied in.
//   ShapedSprite  the same stretched to the command's size.
//   Rectangle     untextured. Corner colours c0 top-left, c1 top-right, c2
//                 bottom-left, c3 bottom-right, blended across: one plain quad
//                 when they agree, else a 2x2 texture of the four colours
//                 sampled between its texel centres, which the bilinear filter
//                 turns into the exact blend. Without a TextureRegistry (or past
//                 kMaxGradients) the gradient is approximated by strips.
// Then the pillarbox: black over whatever of the window lies outside
// View::viewportMin..viewportMax (CameraRig::Bars, the one rule for them),
// last, so nothing drawn past the original's 4:3 screen (DT_NOCLIP text, the
// scene) shows in the bars. The layer does not call CameraRig::AddBars too.
// Under E1's open sides (a menu in a wide window) the sides are not barred:
// the scripts' HUD stays where their 1024x768 put it, and a rectangle that
// meets the screen's left or right edge (a fade, the menu's panel) goes on
// to the edge of what is shown, in that edge's colours (addRectangle).
//
// The overlay holds ScreenOverlay::kMaxQuads (4096) a frame; text is a quad a
// glyph. The busiest screen, "How to Play" (460 glyphs, twice for the shadow)
// with its title and the Alt+Enter line, is about 1100.

#include <cstddef>
#include <string>
#include <unordered_set>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include "core/ScreenOverlay.hpp"
#include "eth/Snapshot.hpp"
#include "render/ArabicShaping.hpp"
#include "render/View.hpp"

namespace Supersonic {
class TextureRegistry;
}

namespace Penumbra::Render {

class FontAtlas;
class Localization;
class TextureCache;

class HudRenderer {
public:
    // Distinct corner-colour sets given their own gradient texture; the
    // original draws one (the menu panel), a fade could make more.
    static constexpr std::size_t kMaxGradients = 64;

    // The collaborators are the layer's and must outlive this. Finds the
    // TextureRegistry in registry.ctx() for gradient textures (none: strips).
    void Attach(entt::registry& registry, TextureCache& textures, FontAtlas& fonts, Localization& localization);
    void Detach();

    // Adds snapshot.hud, then `extra` (may be null), then the pillarbox, to the
    // registry's ScreenOverlay. Call on every frame, after the world renderers.
    // `extra` is the layer's own top layer - the pause's overlay (E13,
    // render/PauseMenu.hpp) - drawn over everything the scripts drew and
    // translated and laid out exactly as their commands are.
    void Draw(entt::registry& registry, const Eth::RenderSnapshot& snapshot, const View& view,
              const std::vector<Eth::HudCmd>* extra = nullptr);

    // The quads Draw adds, appended to `out` in drawing order; no overlay needed.
    void Build(const Eth::RenderSnapshot& snapshot, const View& view,
               std::vector<Supersonic::ScreenOverlay::Quad>& out, const std::vector<Eth::HudCmd>* extra = nullptr);

    // The scripts' ARGB as the overlay's rgba, 0..1.
    static glm::vec4 ToColor(Eth::uint argb);

private:
    using Quad = Supersonic::ScreenOverlay::Quad;

    void addCommand(const Eth::HudCmd& cmd, const View& view, std::vector<Quad>& out);
    // E24: whether a command is a text that draws anything; its code points
    // in drawing order; and those handed to FontAtlas::Prepare before any text
    // of the frame is laid out.
    bool drawsText(const Eth::HudCmd& cmd) const;
    const std::u32string& visualText(const Eth::HudCmd& cmd);
    bool rightToLeft() const;
    void prepareText(const Eth::HudCmd& cmd);
    void addText(const Eth::HudCmd& cmd, const View& view, std::vector<Quad>& out);
    void addSprite(const Eth::HudCmd& cmd, const View& view, bool stretched, std::vector<Quad>& out);
    // The rectangle, and under E1's open sides its continuation past a
    // screen edge it meets (addRectangleQuads draws one rectangle).
    void addRectangle(const Eth::HudCmd& cmd, const View& view, std::vector<Quad>& out);
    void addRectangleQuads(const Eth::HudCmd& cmd, const View& view, std::vector<Quad>& out);
    void addBars(const View& view, std::vector<Quad>& out) const;
    // The 2x2 texture of a rectangle's corners, uploaded once; "" when there is
    // nowhere to upload it or the budget is spent.
    std::string gradientTexture(const Eth::HudCmd& cmd);

    TextureCache* m_textures = nullptr;
    FontAtlas* m_fonts = nullptr;
    Localization* m_localization = nullptr;
    Supersonic::TextureRegistry* m_registryTextures = nullptr;
    std::unordered_set<std::string> m_gradients;
    std::vector<Quad> m_quads;
    VisualText m_visual;   // E24: each text's code points in drawing order
    bool m_loggedDrops = false;
};

} // namespace Penumbra::Render
