#pragma once

// The snapshot's lights, as the engine's normal-mapped 2D point lights, and
// their halos, as additive quads.
//
// THE LIGHTS. 0.7.12 added one One,One pass per (lit entity, light):
//   T * C * dot(normalize(L - P), N) * max(0, 1 - |L - P|^2 / r^2) * lightColour * lightIntensity
// with P the pixel's 3-D position and L the light's (docs/spec/21 §2.3,
// E:ETHShaderManager.cpp:213-359). The engine's Light2DComponent adds exactly
// that per sprite fragment, clamped per light the way the 8-bit target clamped
// each pass (shader.frag shadeSprite2D), with P = (fragment x, fragment y,
// Sprite2DLight::height). So a snapshot light becomes one Light2DComponent:
//   - at ToWorld(light world xy): the LIGHTING position, not the drawn one.
//     ZAxisDirection never entered 0.7.12's lighting (docs/spec/21 §2.3);
//   - height = the light's Ethanon z, in the same units as the sprite heights
//     render/Lighting.cpp hands out, so the 3-D distance and the facing match;
//   - colour = the raw light colour, intensity = the scene's lightIntensity
//     (0.7.12 kept them apart, BeginLightPass @0x43a1b0; the engine folds them);
//   - x the owner's slot-0 particle ratio when the light is NOT static
//     (E:ETHScene.cpp:821: a static light is never dimmed, a spell's is).
// The enhanced port lights everything live: static lights reach static sprites
// too, where 0.7.12 had baked them into lightmaps at load (docs/spec/21 §3).
// That is what lets a torch flicker.
//
// THE HALOS (E:ETHRenderEntity.cpp:736-765). An AM_ADD (One,One) quad of
// haloSize x haloSize, centred on ToScreenPos(light position, ZAxisDirection),
// coloured ConvertToDW(light colour x haloBrightness x particle ratio) - the
// ratio applies to static owners too (E:ETHRenderEntity.cpp:751-756) - with the
// halo bitmap loaded from entities/ with a black colour key
// (E:ETHRenderEntity.cpp:334). Drawn after everything at depth 1.0, so they
// take DrawOrder::haloRank onwards; their order among themselves does not
// matter, additions commute.
//
// ENHANCEMENT (switchable, on by default): torches flicker. A static light
// whose owner draws AM_ADD particles this frame - a flame: torch, foggy_torch,
// torch_higher_range, ground_fire, the lava shooters; not a checkpoint's dark
// AM_MODULATE aura or a portal's AM_PIXEL swirl - varies its intensity, and
// its halo, by a few percent - a sum of three incommensurate sines of
// snapshot.timeMs with phases from the owner's id, so it is deterministic, the
// same snapshot drawn twice looks the same, and neighbouring torches do not
// pulse together. 0.7.12's torches were steady.
//
// POOLED by ownerId: one light entity and at most one halo quad per slot, a
// slot kept by its owner while it stays in the snapshot and handed to a new
// owner when it leaves. Nothing is created or destroyed per frame once the
// pool has grown to the most lights ever shown at once.
//
// How the layer calls it:
//   Attach(registry, textures)                   once, after TextureCache::Attach
//   Draw(registry, snapshot, view, order)        every frame, after ComputeDrawOrder
//   Detach(registry)                             on shutdown / before the registry goes

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include "core/Light2D.hpp"
#include "eth/Snapshot.hpp"
#include "render/DrawOrder.hpp"
#include "render/TextureCache.hpp"
#include "render/View.hpp"

namespace Penumbra::Render {

// What one snapshot light puts on its Light2DComponent. Pure data.
struct LightState {
    bool enabled = false;          // false when it could add nothing (zero colour or range)
    glm::vec2 position{0.0f};      // engine world xy: ToWorld(light world xy)
    float height = 0.0f;           // Light2DComponent::height: the light's Ethanon z
    glm::vec3 color{0.0f};         // Light2DComponent::color: the raw light colour
    float intensity = 0.0f;        // Light2DComponent::intensity
    float range = 0.0f;            // Light2DComponent::range, world units = pixels
    std::uint8_t layers = 0;       // Light2DComponent::layers
    bool baked = false;            // Light2DComponent::baked: a static light, which 0.7.12 baked into
                                   // static sprites (render/Lighting.cpp, THE BAKED EYE)
};

// What one snapshot light puts on its halo quad. Pure data.
struct HaloState {
    bool visible = false;
    std::string bitmapPath;        // "entities/<haloBitmap>", for TextureCache (Additive variant)
    glm::vec3 position{0.0f};      // engine world centre; z = RankZ(rank)
    glm::vec2 size{0.0f};          // haloSize x haloSize (the engine quad is one unit)
    glm::vec4 color{0.0f};         // MaterialComponent::albedoColor, alpha 1
};

class LightRenderer {
public:
    // Which Light2D layers a light is on (Light2DComponent::layers) and which
    // a sprite takes (Sprite2DLight::lightMask, render/Lighting.cpp). Two
    // layers, as 0.7.12 had two kinds: a static light on a static sprite was a
    // lightmap, everything else a live pass. The enhanced port lights both
    // live, so a lit sprite takes kAllLayers today; the split keeps a baked
    // path possible without touching either side's numbers.
    static constexpr std::uint8_t kDynamicLayer = 1u << 0;
    static constexpr std::uint8_t kStaticLayer = 1u << 1;
    static constexpr std::uint8_t kAllLayers = kDynamicLayer | kStaticLayer;

    // The engine's per-frame cap (core/Light2D.hpp). The snapshot holds only
    // visible-bucket lights, which the shipped scenes keep well under it;
    // past it the later lights in snapshot order are left off (logged once)
    // rather than letting the engine drop in registry order, which is not the
    // snapshot's.
    static constexpr std::size_t kMaxLights = Supersonic::kMaxLights2D;

    // The torch flicker's largest deviation from the steady intensity.
    static constexpr float kTorchFlickerAmplitude = 0.07f;

    void Attach(entt::registry& registry, TextureCache& textures);
    void Detach(entt::registry& registry);
    // `bakedStrips` (optional): ShadowRenderer::BakedStrips() for this same
    // snapshot, index for index its lights. A light with strips gets them as
    // its Light2DShadowsComponent (render/Lighting.cpp, THE BAKED SHADOWS);
    // every other light, and every light without the argument, has none.
    // Handed over here, not by the shadow renderer, because the slots - which
    // entity is which light - are this Draw's.
    void Draw(entt::registry& registry, const Eth::RenderSnapshot& snapshot, const View& view,
              const DrawOrder& order,
              const std::vector<std::vector<Supersonic::Light2DShadowsComponent::Strip>>* bakedStrips = nullptr);

    // ENHANCEMENT switch (see the header comment). On by default.
    void SetTorchFlicker(bool flicker) { m_torchFlicker = flicker; }
    bool TorchFlicker() const { return m_torchFlicker; }

    // The whole light mapping. `flicker` multiplies the intensity (1 = steady).
    static LightState ComputeLight(const Eth::LightDraw& light, float lightIntensity, float flicker = 1.0f);

    // The whole halo mapping. `rank` is the halo's draw rank (DrawOrder::haloRank
    // + its index); `flicker` multiplies the colour after ConvertToDW.
    static HaloState ComputeHalo(const Eth::LightDraw& light, const glm::vec2& zAxisDirection, int rank,
                                 float flicker = 1.0f);

    // ConvertToDW's rule for a colour (E:ETHCommon.h:376-383): a colour with
    // any channel above 1 is normalised to unit length, not clamped.
    static glm::vec3 ConvertToDW(const glm::vec3& color);

    // The torch flicker at `timeMs` for the light owned by `ownerId`: within
    // 1 +/- kTorchFlickerAmplitude, a pure function of its arguments.
    static float TorchFlickerFactor(int ownerId, Eth::uint timeMs);

    // A torch, for the flicker: a static light whose owner draws AM_ADD
    // particles this frame. `flameOwners` is sorted (FlameOwners).
    static bool IsTorch(const Eth::LightDraw& light, const std::vector<int>& flameOwners);
    // The owners of this frame's AM_ADD particles, sorted, once each.
    static std::vector<int> FlameOwners(const Eth::RenderSnapshot& snapshot);

    // For suites and the debug overlay.
    std::size_t SlotCount() const { return m_slots.size(); }
    std::size_t EnabledCount() const { return m_enabled; }       // lights left on by the last Draw
    std::size_t HaloCount() const { return m_halos; }            // halos shown by the last Draw
    std::size_t DroppedCount() const { return m_dropped; }       // over kMaxLights in the last Draw

private:
    struct Slot {
        entt::entity light = entt::null;
        entt::entity halo = entt::null;
        int ownerId = -1;
        bool used = false;              // taken by the Draw in progress
        std::string haloPath;           // what haloKey was resolved from
        std::string haloKey;            // TextureCache key, "" = unreadable
    };

    std::size_t SlotFor(entt::registry& registry, int ownerId);
    static entt::entity CreateLight(entt::registry& registry);
    static entt::entity CreateHalo(entt::registry& registry);
    static void DisableSlot(entt::registry& registry, const Slot& slot);

    TextureCache* m_textures = nullptr;
    std::vector<Slot> m_slots;
    std::unordered_map<int, std::size_t> m_byOwner;
    std::vector<std::size_t> m_assigned;    // per snapshot light, scratch
    std::vector<int> m_flameOwners;         // scratch
    bool m_torchFlicker = true;
    std::size_t m_enabled = 0;
    std::size_t m_halos = 0;
    std::size_t m_dropped = 0;
    bool m_warnedCap = false;
};

} // namespace Penumbra::Render
