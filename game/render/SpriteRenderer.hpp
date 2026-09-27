#pragma once

// The snapshot's entities, and the scene's background image, as engine quads.
//
// One pooled quad a drawn sprite, keyed by its entity id so a sprite keeps its
// quad (and the material the engine resolved for it) from frame to frame. A
// quad nobody drew this frame is hidden, not destroyed; one left idle for
// kIdleDrawsBeforeRecycle draws goes back to a free list for the next new id,
// so spells and projectiles, which get a fresh id each, do not grow the pool
// without bound. Every field is written only when it changes, so drawing the
// same snapshot twice writes nothing the second time (and the texture paths,
// which are in SyncResources' signature, never churn).
//
// The per-sprite mapping (docs/spec/30 §3.2-3.5, docs/spec/21 §2):
//   position   quad centre = origin + size/2 (origin is the rounded top-left
//              0.7.12 drew), y flipped (View.hpp), z = RankZ(spriteRank)
//   scale      the frame size in pixels (one world unit a pixel)
//   frame      uvScale = frame / bitmap, uvOffset = cell corner / bitmap:
//              0.7.12's rect, rectPos/bitmapSize + uv * rectSize/bitmapSize
//              (defaultStaticAmbientVS.cg transformCoord)
//   colour     albedoColor = v4Color; the ambient term min(1, A + E) goes in
//              sprite2D.ambient (render/Lighting.hpp)
//   blend      AM_PIXEL      Alpha,    image keyed magenta
//              AM_ALPHA_TEST Alpha,    image keyed magenta, cut at alpha 1/255
//              AM_ADD        Additive, image keyed black with alpha 1
//              AM_MODULATE   Alpha,    the Modulate image (black at 1 - luminance)
//              AM_NONE       opaque,   no cut
//              every mode but NONE discards alpha <= 1/255, as GameSpace.dll's
//              SetAlphaMode alpha-tests all of them (docs/spec/30 §3.3).
//
// The background image (SetBackgroundImage / PositionBackgroundImage /
// SetBackgroundAlphaAdd) is one more quad, fixed to the camera in screen
// pixels, white, unlit, drawn behind rank 0 (ETHEngine.cpp:594-606 drew it
// before the scene with the camera at 0,0). The clear colour behind it is
// CameraRig's.

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include "eth/Snapshot.hpp"
#include "render/DrawOrder.hpp"
#include "render/TextureCache.hpp"
#include "render/View.hpp"

namespace Penumbra::Render {

class SpriteRenderer {
public:
    // Draws a quad sat idle this many times goes back to the free list.
    static constexpr int kIdleDrawsBeforeRecycle = 120;

    // D3D9's alpha test is ALPHAREF 1, GREATER: a texel of alpha 1/255 is
    // dropped and one of 2/255 kept. The engine discards below its cutoff.
    static constexpr float kAlphaTestCutoff = 1.5f / 255.0f;

    // Where the background image lies: one step behind rank 0.
    static constexpr int kBackgroundRank = -1;

    void Attach(entt::registry& registry, TextureCache& textures);
    void Detach(entt::registry& registry);

    // Once a frame (idempotent: any number of frames a tick). `order` is
    // ComputeDrawOrder(snapshot), computed once a frame and shared with the
    // shadow, particle and halo renderers.
    void Draw(entt::registry& registry, const Eth::RenderSnapshot& snapshot, const View& view,
              const DrawOrder& order);

    // The quad drawing that entity this frame, entt::null if none.
    entt::entity QuadFor(int entityId) const;
    entt::entity BackgroundQuad() const { return m_background; }
    std::size_t PooledQuads() const { return m_slots.size() + m_free.size(); }
    std::size_t VisibleQuads() const { return m_visible; }

    // ---- the pure halves, for suites ----------------------------------------

    // The texture variant a blend mode draws its image with.
    static TextureVariant VariantFor(Eth::ALPHA_MODE mode);

    // The frame's texture rectangle as the engine's uvScale/uvOffset. `bitmap`
    // is the whole image in pixels.
    static void FrameUv(const Eth::SpriteDraw& sprite, const glm::vec2& bitmap, glm::vec2& uvScale,
                        glm::vec2& uvOffset);

    // The quad's world centre (z = `engineZ`), scale and z rotation for a
    // sprite. A rotated sprite turns about its entity position (the vertex
    // shader's entityPos, the pivot its centre offset is measured from);
    // a vertical one never turns (ETHRenderEntity.cpp:707, :721).
    static void Placement(const Eth::SpriteDraw& sprite, const glm::vec2& zAxisDirection, float engineZ,
                          glm::vec3& position, glm::vec3& scale, float& rotationZ);

private:
    struct Slot {
        entt::entity quad = entt::null;
        int idleDraws = 0;
        bool used = false;
        int entityId = -1;
        // What the albedo key was last resolved from, so a still sprite asks
        // the texture cache nothing.
        std::string sprite;
        TextureVariant variant = TextureVariant::Sprite;
        std::string albedoKey;
    };

    entt::entity makeQuad(entt::registry& registry, const char* tag);
    Slot& slotFor(entt::registry& registry, int entityId);
    void drawSprite(entt::registry& registry, const Eth::RenderSnapshot& snapshot, std::size_t index,
                    const DrawOrder& order, Slot& slot, const std::string& albedoKey);
    void drawBackground(entt::registry& registry, const Eth::RenderSnapshot& snapshot, const View& view);
    void hide(entt::registry& registry, entt::entity quad);

    TextureCache* m_textures = nullptr;
    // Keyed by entity id, and by an occurrence count above it on the (never
    // expected) frame one id is drawn twice, so neither steals the other's quad.
    std::unordered_map<std::uint64_t, Slot> m_slots;
    std::vector<entt::entity> m_free;
    entt::entity m_background = entt::null;
    std::string m_backgroundSource;
    bool m_backgroundAdditive = false;
    std::string m_backgroundKey;
    std::size_t m_visible = 0;
};

} // namespace Penumbra::Render
