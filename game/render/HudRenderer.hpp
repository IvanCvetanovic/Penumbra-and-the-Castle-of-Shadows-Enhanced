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
//   Text          translated (Localization), laid out (FontAtlas), one quad
//                 per visible glyph in the text's colour. shadowText is
//                 already two Text commands in the snapshot.
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

    // Adds snapshot.hud, then the pillarbox, to the registry's ScreenOverlay.
    // Call on every frame, after the world renderers.
    void Draw(entt::registry& registry, const Eth::RenderSnapshot& snapshot, const View& view);

    // The quads Draw adds, appended to `out` in drawing order; no overlay needed.
    void Build(const Eth::RenderSnapshot& snapshot, const View& view,
               std::vector<Supersonic::ScreenOverlay::Quad>& out);

    // The scripts' ARGB as the overlay's rgba, 0..1.
    static glm::vec4 ToColor(Eth::uint argb);

private:
    using Quad = Supersonic::ScreenOverlay::Quad;

    void addText(const Eth::HudCmd& cmd, const View& view, std::vector<Quad>& out);
    void addSprite(const Eth::HudCmd& cmd, const View& view, bool stretched, std::vector<Quad>& out);
    void addRectangle(const Eth::HudCmd& cmd, const View& view, std::vector<Quad>& out);
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
    bool m_loggedDrops = false;
};

} // namespace Penumbra::Render
