#pragma once

// The original's projected sprite shadows: for each (caster, light), a
// five-vertex strip textured with data/shadow.dds, drawn black and alpha
// blended behind its caster.
//
// 0.7.12 (E:ETHRenderEntity.cpp:818-949, data/dynaShadowVS.cg:83-124,
// docs/spec/21 §2.6) drew shadow.dds with RM_THREE_TRIANGLES - a strip over
// (0,0) (0,1) (0.5,0) (1,1) (1,0), position = texture coordinate - on a quad
// W x 1 about the origin (0.5, 0.79), turned by GetAngle(light - caster) so its
// base runs across the light direction, and the vertex shader then:
//   - pushed the three v = 0 vertices radially away from the light by
//     shadowLength (so the far end fans out);
//   - moved all five towards the light by shadowLength/6 - entityZ (the
//     texture's first 5/32 is transparent, so the dark band starts at the
//     caster).
// Its numbers: W = w x 0.8 x shadowScale; the length h x 8 into a lightmap,
// h x 2 in real time, or, when the light is above the caster's top,
// min(2.2 h, the ground-plane projection); at least h; x shadowLengthScale.
// Real-time alpha = (1 - d^2/r^2) x min(max colour, 1) x (1 - mean ambient)
// x clamp(1 - z/h) x shadowOpacity, in bytes, nothing below 8. The shader is
// replaced by the CPU here: ComputeShadow is the whole of it.
//
// WHICH PAIRS, and how long. 0.7.12 had two paths:
//   - BAKED: a static caster and a static light (castShadows) were drawn into
//     every static receiver's lightmap at load, length factor 8, at full
//     opacity, darkening only that light's term (E:ETHRenderEntity.cpp:500-519,
//     :514 passes maxOpacity and drawToTarget). No applyLight needed.
//   - REAL TIME: every other pair, drawn after the caster's own light pass, so
//     only for a caster that applies light (E:ETHScene.cpp:924-948,
//     E:ETHShaderManager.cpp:217), length factor 2, the alpha formula above.
// The enhanced port lights everything live, so both are drawn live as one
// darkening overlay. The overlay darkens ambient and every light under it, not
// just its own light's term, so:
//   - ALPHA: the real-time formula for both. It scales with the light's
//     falloff at the caster and with how much the ambient leaves to darken,
//     which is what "remove this light's contribution" amounts to. 0.7.12's
//     baked alpha of 1 x opacity over everything would paint the ambient black.
//   - LENGTH: factor 8 for baked pairs - the long menu barrel shadows - but
//     (ENHANCEMENT, switchable, on) cut so the shadow's VISIBLE end - where
//     shadow.dds turns clear, v = kFadedV - stops where the light stops
//     reaching, never shorter than the caster is tall. A baked shadow faded
//     out with its light, because it only removed that light; a live one
//     would darken plain ambient past the light's range. Cutting the visible
//     end rather than the strip's apex is the longest cut that darkens
//     nothing out there (the last 5/32 of the strip is clear anyway), and
//     along the menu barrels' axes it follows the original's baked falloff:
//     modelled with hPixelLight, a strip cut at its apex fades 0.08-0.12 (in
//     shadowed/lit ratio) too early, one cut at its visible end is within
//     0.02-0.06, and the uncut x8 drawn live is 0.23-0.34 too dark. It binds
//     on every baked pair shipped (70 of them): the menu's barrels and devils
//     (58 x 8 = 464 px against torches reaching 255 and blue lights 231:
//     barrel 117 by torch 107 keeps 246), tile_shadow in level1-3 and pvp_lv2
//     (64 x 8 x 1.5 = 768 px against torches and crystals: 64-505 kept) and
//     pvp_lv2's single_tile_shadow (x 9.5: 2432 px, or 669 by the projection
//     rule; 32-307 kept). Real-time shadows are not cut: 0.7.12 drew them
//     over everything too.
//
// DRAWN as one MeshRegistry mesh per shadow (MeshComponent::meshKey), black
// (MaterialComponent::albedoColor (0,0,0,alpha)) through shadow.dds's alpha,
// Alpha blend, alpha test > 1/255, unlit, ranked at DrawOrder::shadowRankBase
// of its caster (every shadow of one caster shares it: black over black
// composites the same in any order).
//
// POOLED per (caster entity id, light owner id, occurrence) - the occurrence
// only tells apart a pair a snapshot repeats, so it is 0 in practice. The mesh is kept in the
// caster's frame (vertices relative to its position, the transform at the
// caster), so a static pair never rebuilds, and a moving one rebuilds only when
// a vertex moves more than kRebuildEpsilon. A rebuild is MeshRegistry::Replace,
// which stages through the graphics queue and waits for it (twice) and makes
// every renderable re-resolve its resources that frame: fine for the handful a
// moving light casts at once, and the reason static pairs never pay it. The
// same snapshot drawn twice rebuilds nothing.
//
// How the layer calls it:
//   Attach(registry, textures)                   once, after TextureCache::Attach
//   Draw(registry, snapshot, view, order)        every frame, after ComputeDrawOrder
//                                                (E8: the blend, with &tick as `shapes`)
//   Detach(registry)                             on shutdown / before the registry goes

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include "eth/Snapshot.hpp"
#include "render/DrawOrder.hpp"
#include "render/TextureCache.hpp"
#include "render/View.hpp"

namespace Supersonic {
class MeshRegistry;
}

namespace Penumbra::Render {

// One (caster, light) shadow. Pure data.
struct ShadowGeometry {
    bool visible = false;
    bool baked = false;                 // static caster x static light: 0.7.12 baked it
    float width = 0.0f;                 // W = w x 0.8 x shadowScale
    float length = 0.0f;                // the shader's shadowLength (x shadowLengthScale, after the cap)
    float alpha = 0.0f;                 // alpha8 / 255
    int alpha8 = 0;                     // the byte 0.7.12 drew with
    glm::vec2 anchor{0.0f};             // the caster's world xy, Ethanon pixels (y down)
    // The strip, in Ethanon world pixels (y down), in 0.7.12's vertex order;
    // vertex i's texture coordinate is ShadowRenderer::kStripUv[i].
    std::array<glm::vec2, 5> vertices{};
};

class ShadowRenderer {
public:
    // RM_THREE_TRIANGLES: position = texture coordinate (the 2013 gs2d,
    // D3D9VideoInfo.cpp:181-188; the mode is in 2010 too, G:gs2d.h:103).
    static constexpr std::array<glm::vec2, 5> kStripUv = {
        glm::vec2(0.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec2(0.5f, 0.0f),
        glm::vec2(1.0f, 1.0f), glm::vec2(1.0f, 0.0f)};
    // The strip as a list, each triangle wound as the first.
    static constexpr std::array<std::uint32_t, 9> kIndices = {0, 1, 2, 2, 1, 3, 2, 3, 4};

    // E:ETHCommon.h:87-89, E:ETHRenderEntity.cpp:897, :937, :942.
    static constexpr float kScaleX = 0.8f;          // _ETH_SHADOW_SCALEX
    static constexpr float kFakeStretch = 2.2f;     // _ETH_SHADOW_FAKE_STRETCH
    static constexpr float kScaleYBaked = 8.0f;     // _ETH_SHADOW_SCALEY
    static constexpr float kScaleYRealTime = 2.0f;  // _ETH_SHADOW_SCALEY / 4
    static constexpr float kOriginX = 0.5f;
    static constexpr float kOriginY = 0.79f;
    static constexpr int kMinAlpha8 = 8;
    // shadow.dds's alpha is 0 in rows 0-4 (docs/spec/21 §2.6): the strip is
    // clear for v < 5/32, so its visible end lies that far short of its apex.
    static constexpr float kFadedV = 5.0f / 32.0f;
    // Below this the light is straight over the caster: no direction to cast
    // along (the shader would normalise a zero vector).
    static constexpr float kMinPlanarDistance = 1e-3f;
    // A rebuild waits on the GPU; a shadow whose vertices moved less than this
    // (in pixels) keeps the mesh it has.
    static constexpr float kRebuildEpsilon = 1e-3f;
    // Pairs a hidden slot is kept for before it may be handed to a new pair.
    static constexpr std::size_t kPoolSoftCap = 256;

    void Attach(entt::registry& registry, TextureCache& textures);
    void Detach(entt::registry& registry);
    // `shapes` (optional): the snapshot the strips' SHAPE is computed from,
    // index for index the same sprites and lights as `snapshot`, which then
    // only places each strip at its caster. For the enhanced interpolation
    // between ticks (E8, render/Interpolation.hpp): `snapshot` is the blend
    // and `shapes` the current tick, so a caster moving past a light moves its
    // shadow every frame but rebuilds its mesh - a GPU wait - once a tick, as
    // it did before. Ignored when its lists do not line up with `snapshot`'s.
    void Draw(entt::registry& registry, const Eth::RenderSnapshot& snapshot, const View& view,
              const DrawOrder& order, const Eth::RenderSnapshot* shapes = nullptr);

    // ENHANCEMENT switch (see LENGTH above). On by default.
    void SetCapBakedLength(bool cap) { m_capBakedLength = cap; }
    bool CapBakedLength() const { return m_capBakedLength; }

    // The whole of one shadow: who casts, the strip, the alpha. Not visible
    // when 0.7.12 would have drawn nothing. `sceneAmbient` is the snapshot's.
    static ShadowGeometry ComputeShadow(const Eth::SpriteDraw& caster, const Eth::LightDraw& light,
                                        const glm::vec3& sceneAmbient, bool capBakedLength = true);

    // For suites and the debug overlay.
    std::size_t SlotCount() const { return m_slots.size(); }
    std::size_t ShownCount() const { return m_shown; }          // shown by the last Draw
    std::size_t RebuildCount() const { return m_rebuilds; }     // meshes (re)built by the last Draw

private:
    using PairKey = std::tuple<int, int, int>;

    struct Slot {
        entt::entity entity = entt::null;
        std::string meshKey;
        bool hasMesh = false;                    // uploaded, and `built` is what is on the GPU
        std::array<glm::vec2, 5> built{};        // local (caster-relative, engine axes) vertices
        PairKey pair{-1, -1, 0};                 // (caster entity id, light owner id, occurrence)
        std::uint64_t lastUsed = 0;
        bool used = false;
    };

    std::size_t SlotFor(entt::registry& registry, const PairKey& pair);
    static entt::entity CreateEntity(entt::registry& registry, const std::string& meshKey);
    bool Upload(entt::registry& registry, Slot& slot, const std::array<glm::vec2, 5>& local);
    static void Hide(entt::registry& registry, const Slot& slot);

    TextureCache* m_textures = nullptr;
    Supersonic::MeshRegistry* m_meshes = nullptr;
    std::string m_shadowKey;
    bool m_shadowKeyResolved = false;
    bool m_warnedNoTexture = false;
    bool m_warnedNoRank = false;
    std::vector<Slot> m_slots;
    std::map<PairKey, std::size_t> m_byPair;
    std::map<std::pair<int, int>, int> m_seen;     // per Draw: occurrences of a (caster, owner) so far
    std::uint64_t m_draws = 0;
    std::uint32_t m_nextKey = 0;
    bool m_capBakedLength = true;
    std::size_t m_shown = 0;
    std::size_t m_rebuilds = 0;
};

} // namespace Penumbra::Render
