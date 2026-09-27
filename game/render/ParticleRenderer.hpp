#pragma once

// The snapshot's particles, drawn as pooled engine quads.
//
// 0.7.12 drew every particle as one DrawShapedSprite: a size x size quad about
// its centre, rotated by the particle's angle, textured with one cell of its
// bitmap, tinted by one vertex colour and blended by the system's alphaMode
// (E:ETHParticleManager.cpp:821-906, docs/spec/21-formats-particles-shaders.md
// §1.5). Nothing lit a particle: the particle vertex shader's light uniforms
// are commented out (defaultVS.cg:138-147), so here they are plain unlit quads
// and no Light2D reaches them.
//
// POOLED. The layer may draw one snapshot several times between ticks, and a
// burst of sparks comes and goes in a quarter of a second, so quads are never
// made and unmade per frame: one pool per (bitmap, alphaMode), grown to the
// most particles of that pair ever drawn at once, the surplus hidden. A pool
// fixes its quads' texture, blend and cutoff once, so a frame only writes the
// transform, the colour, the cell and the visibility - and every particle of
// one bitmap and mode names the same texture, which is one material
// descriptor set however many there are (the engine has 1024,
// renderer/MaterialSetLedger.hpp).
//
// IDEMPOTENT. Draw reads nothing but its arguments: the same snapshot drawn
// twice leaves every quad as it was.
//
// How the layer calls it:
//   Attach(registry, textures)                   once, after TextureCache::Attach
//   Draw(registry, snapshot, view, order)        every frame, after ComputeDrawOrder
//   Detach(registry)                             on shutdown / before the registry goes

#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include "core/Components.hpp"
#include "eth/Snapshot.hpp"
#include "render/DrawOrder.hpp"
#include "render/TextureCache.hpp"
#include "render/View.hpp"

namespace Penumbra::Render {

// Everything one snapshot particle puts on its quad. Pure data, so a suite can
// check the mapping without a registry.
struct ParticleQuad {
    // False when 0.7.12 would have drawn nothing: a size of zero, or a vertex
    // alpha the alpha test (ALPHAREF 1, GREATER) threw away whole.
    bool visible = false;
    glm::vec3 position{0.0f};          // engine world: the centre, z = RankZ(rank)
    glm::vec3 scale{1.0f};             // size x size (the engine quad is one unit)
    float rotationZ = 0.0f;            // engine radians, counter-clockwise on screen
    glm::vec2 uvScale{1.0f};           // the cell: MaterialComponent::uvScale / uvOffset
    glm::vec2 uvOffset{0.0f};
    glm::vec4 color{1.0f};             // MaterialComponent::albedoColor
    Supersonic::MaterialComponent::BlendMode blend = Supersonic::MaterialComponent::BlendMode::Alpha;
    TextureVariant variant = TextureVariant::Sprite;
    float alphaCutoff = 0.0f;          // MaterialComponent::alphaCutoff
};

class ParticleRenderer {
public:
    // How many particles cut from a sprite sheet may take a UV-transform slot
    // in one frame. The engine has 4095 for the whole scene (twelve bits,
    // Components.hpp kUvSlotMask), counted over every MaterialComponent visible
    // or not, and the sprite renderer's animated sheets need theirs. The
    // shipped content peaks far below this (explosion.png, 4x3, at most 12 a
    // burst); one over the budget is not drawn rather than drawn as its whole
    // sheet, which is what a dropped slot would show.
    static constexpr std::size_t kMaxCutParticles = 1024;

    void Attach(entt::registry& registry, TextureCache& textures);
    void Detach(entt::registry& registry);
    void Draw(entt::registry& registry, const Eth::RenderSnapshot& snapshot, const View& view,
              const DrawOrder& order);

    // ENHANCEMENT, off by default: fade AM_ADD particles by their vertex alpha
    // (rgb x alpha). 0.7.12's One,One ignored that alpha except for the alpha
    // test, so a system that fades only its alpha - light_spell,
    // light_spell_infinite (the menu cursor) - popped off at full brightness,
    // and that is the original's look.
    void SetFadeAdditiveByAlpha(bool fade) { m_fadeAdditiveByAlpha = fade; }
    bool FadeAdditiveByAlpha() const { return m_fadeAdditiveByAlpha; }

    // The whole per-particle mapping. `rank` is DrawOrder::particleRank's
    // entry for it; `bitmapSize` the bitmap's original pixel size
    // (TextureCache::Size, (0,0) if unknown).
    static ParticleQuad ComputeQuad(const Eth::ParticleDraw& particle, int rank, const glm::ivec2& bitmapSize,
                                    bool fadeAdditiveByAlpha = false);

    // The texture variant 0.7.12 loaded a particle bitmap with, by alphaMode.
    static TextureVariant VariantFor(Eth::ALPHA_MODE mode);

    // "particles/<basename>": 0.7.12 took the file name of <Bitmap> and looked
    // for it in particles\ (E:ETHRenderEntity.cpp:361-364).
    static std::string BitmapPath(const std::string& bitmap);

    // For suites and the debug overlay.
    std::size_t QuadCount() const;                         // quads owned, shown or not
    std::size_t ShownCount() const { return m_shown; }     // shown by the last Draw
    std::size_t DroppedCutCount() const { return m_droppedCut; }  // over kMaxCutParticles in the last Draw

private:
    struct Pool {
        std::string textureKey;        // "" = the bitmap could not be read: draw nothing
        glm::ivec2 bitmapSize{0};
        Eth::ALPHA_MODE mode = Eth::AM_PIXEL;
        std::vector<entt::entity> quads;
        std::size_t used = 0;          // taken by the Draw in progress
        std::size_t shown = 0;         // left visible by the last Draw
    };

    Pool& PoolFor(const std::string& bitmap, Eth::ALPHA_MODE mode);
    entt::entity Acquire(entt::registry& registry, Pool& pool, const ParticleQuad& quad);
    static entt::entity CreateQuad(entt::registry& registry, const Pool& pool, const ParticleQuad& quad);
    static void Hide(entt::registry& registry, entt::entity quad);

    TextureCache* m_textures = nullptr;
    std::map<std::pair<std::string, int>, Pool> m_pools;
    bool m_fadeAdditiveByAlpha = false;
    std::size_t m_shown = 0;
    std::size_t m_droppedCut = 0;
    bool m_warnedCutBudget = false;
};

} // namespace Penumbra::Render
