#include "render/LightRenderer.hpp"

#include <algorithm>
#include <cmath>

#include "core/Components.hpp"
#include "core/DetMath.hpp"
#include "core/Log.hpp"

namespace Penumbra::Render {

namespace {

constexpr double kTwoPi = 6.283185307179586;

// A torch flame's flicker is a few hertz; three partials that share no simple
// ratio never line up into a visible period, and weights summing to 1 keep the
// sum within +/-1 so the amplitude is the whole bound.
constexpr double kFlickerHz[3] = {1.3, 2.9, 7.1};
constexpr float kFlickerWeight[3] = {0.5f, 0.3f, 0.2f};

// The murmur3 finaliser: an owner id to well-spread bits, so ids 7 and 8 do not
// get neighbouring phases.
std::uint32_t Mix(std::uint32_t h) {
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

// 0.7.12 loaded halos from entities\ (ETH_HALOS_FOLDER, E:ETHCommon.h:78).
const std::string kHaloFolder = "entities/";

} // namespace

// ---- the pure mappings ---------------------------------------------------------

LightState LightRenderer::ComputeLight(const Eth::LightDraw& light, float lightIntensity, float flicker) {
    LightState state;
    state.position = glm::vec2(ToWorld(glm::vec2(light.position), 0.0f));
    state.height = light.position.z;
    state.color = light.color;
    // E:ETHScene.cpp:821: only a light that is not static is dimmed by its
    // owner's slot-0 particles (a spell's light fading with its sparks). A
    // torch's is not - its HALO is (ComputeHalo).
    const float ratio = light.isStatic ? 1.0f : light.particleRatio;
    state.intensity = lightIntensity * ratio * flicker;
    state.range = light.range;
    state.layers = light.isStatic ? kStaticLayer : kDynamicLayer;
    // Only a sprite that asks for its bake eye reads it (Sprite2DLight::bakedEye).
    state.baked = light.isStatic;
    // Nothing to add: the engine would skip it anyway (Light2D::GatherLights2D),
    // but it must not take one of the 64 places a real light needs.
    const glm::vec3 folded = state.color * state.intensity;
    state.enabled = state.range > 0.0f && (folded.r != 0.0f || folded.g != 0.0f || folded.b != 0.0f);
    return state;
}

glm::vec3 LightRenderer::ConvertToDW(const glm::vec3& color) {
    if (color.r > 1.0f || color.g > 1.0f || color.b > 1.0f) return glm::normalize(color);
    return color;
}

HaloState LightRenderer::ComputeHalo(const Eth::LightDraw& light, const glm::vec2& zAxisDirection, int rank,
                                     float flicker) {
    HaloState halo;
    // DrawHalo returns early without a bitmap or with a size of zero or less
    // (E:ETHRenderEntity.cpp:738).
    if (light.haloBitmap.empty() || light.haloSize <= 0.0f) return halo;
    halo.visible = true;
    halo.bitmapPath = kHaloFolder + light.haloBitmap;
    // ToScreenPos(entity + light position): the halo is DRAWN where the screen
    // shows the light, which in the menus (ZAxisDirection (0,-1)) is z pixels
    // above where it lights.
    const glm::vec2 centre = glm::vec2(light.position) + zAxisDirection * light.position.z;
    halo.position = ToWorld(centre, RankZ(rank));
    halo.size = glm::vec2(light.haloSize);
    // Colour x brightness x the slot-0 ratio, for static owners too
    // (E:ETHRenderEntity.cpp:751-760). The flicker goes on after ConvertToDW,
    // which would otherwise read a flickering 1.05 as "normalise" and halve it.
    const glm::vec3 color = ConvertToDW(light.color * light.haloBrightness * light.particleRatio) * flicker;
    halo.color = glm::vec4(color, 1.0f);
    return halo;
}

float LightRenderer::TorchFlickerFactor(int ownerId, Eth::uint timeMs) {
    const std::uint32_t bits = Mix(static_cast<std::uint32_t>(ownerId));
    // In double: GetTime counts milliseconds for the whole run, and a float
    // loses the millisecond after about four and a half hours.
    const double seconds = static_cast<double>(timeMs) / 1000.0;
    float sum = 0.0f;
    for (int k = 0; k < 3; ++k) {
        const double phase = static_cast<double>((bits >> (8 * k)) & 0xFFu) / 256.0 * kTwoPi;
        const double angle = std::fmod(seconds * kFlickerHz[k] * kTwoPi + phase, kTwoPi);
        // DetMath, so a capture is the same picture on every machine.
        sum += kFlickerWeight[k] * Supersonic::DetMath::sin(static_cast<float>(angle));
    }
    return 1.0f + kTorchFlickerAmplitude * sum;
}

std::vector<int> LightRenderer::FlameOwners(const Eth::RenderSnapshot& snapshot) {
    std::vector<int> owners;
    for (const Eth::ParticleDraw& particle : snapshot.particles) {
        if (particle.alphaMode == Eth::AM_ADD) owners.push_back(particle.ownerId);
    }
    std::sort(owners.begin(), owners.end());
    owners.erase(std::unique(owners.begin(), owners.end()), owners.end());
    return owners;
}

bool LightRenderer::IsTorch(const Eth::LightDraw& light, const std::vector<int>& flameOwners) {
    return light.isStatic && std::binary_search(flameOwners.begin(), flameOwners.end(), light.ownerId);
}

// ---- the pool ----------------------------------------------------------------------

void LightRenderer::Attach(entt::registry& /*registry*/, TextureCache& textures) {
    m_textures = &textures;
}

void LightRenderer::Detach(entt::registry& registry) {
    for (Slot& slot : m_slots) {
        if (slot.light != entt::null && registry.valid(slot.light)) registry.destroy(slot.light);
        if (slot.halo != entt::null && registry.valid(slot.halo)) registry.destroy(slot.halo);
    }
    m_slots.clear();
    m_byOwner.clear();
    m_enabled = 0;
    m_halos = 0;
    m_dropped = 0;
    m_textures = nullptr;
}

entt::entity LightRenderer::CreateLight(entt::registry& registry) {
    using namespace Supersonic;
    const entt::entity e = registry.create();
    registry.emplace<TagComponent>(e, std::string("Penumbra Light"));
    registry.emplace<TransformComponent>(e);
    registry.emplace<Light2DComponent>(e).enabled = false;
    return e;
}

entt::entity LightRenderer::CreateHalo(entt::registry& registry) {
    using namespace Supersonic;
    const entt::entity e = registry.create();
    registry.emplace<TagComponent>(e, std::string("Penumbra Halo"));
    registry.emplace<TransformComponent>(e);
    registry.emplace<MeshComponent>(e).primitiveType = "Quad";
    auto& material = registry.emplace<MaterialComponent>(e);
    // Unlit: nothing the engine lights may reach a halo, and it takes no
    // ambient - it IS light. Additive over a texture whose alpha the
    // TextureCache forced to 1 is One,One, 0.7.12's AM_ADD.
    material.unlit = true;
    material.transparent = true;
    material.blend = MaterialComponent::BlendMode::Additive;
    auto& renderable = registry.emplace<RenderableComponent>(e);
    renderable.castsShadow = false;
    renderable.isVisible = false;
    return e;
}

void LightRenderer::DisableSlot(entt::registry& registry, const Slot& slot) {
    if (slot.light != entt::null && registry.valid(slot.light)) {
        auto& component = registry.get<Supersonic::Light2DComponent>(slot.light);
        if (component.enabled) component.enabled = false;
    }
    if (slot.halo != entt::null && registry.valid(slot.halo)) {
        auto& renderable = registry.get<Supersonic::RenderableComponent>(slot.halo);
        if (renderable.isVisible) renderable.isVisible = false;
    }
}

std::size_t LightRenderer::SlotFor(entt::registry& registry, int ownerId) {
    // A slot no light of this snapshot has claimed, lowest first, so the
    // choice depends on the snapshot alone.
    for (std::size_t s = 0; s < m_slots.size(); ++s) {
        Slot& slot = m_slots[s];
        if (slot.used) continue;
        if (const auto it = m_byOwner.find(slot.ownerId); it != m_byOwner.end() && it->second == s) {
            m_byOwner.erase(it);
        }
        slot.ownerId = ownerId;
        m_byOwner[ownerId] = s;
        return s;
    }
    Slot slot;
    slot.light = CreateLight(registry);
    slot.ownerId = ownerId;
    m_slots.push_back(std::move(slot));
    m_byOwner[ownerId] = m_slots.size() - 1;
    return m_slots.size() - 1;
}

void LightRenderer::Draw(entt::registry& registry, const Eth::RenderSnapshot& snapshot, const View& /*view*/,
                         const DrawOrder& order) {
    using namespace Supersonic;
    const std::vector<Eth::LightDraw>& lights = snapshot.lights;

    // Owners that keep their slot first, so a light that stays keeps its
    // entities; then the newcomers take what is left.
    for (Slot& slot : m_slots) slot.used = false;
    constexpr std::size_t kNone = static_cast<std::size_t>(-1);
    m_assigned.assign(lights.size(), kNone);
    for (std::size_t i = 0; i < lights.size(); ++i) {
        const auto it = m_byOwner.find(lights[i].ownerId);
        if (it == m_byOwner.end() || m_slots[it->second].used) continue;
        m_assigned[i] = it->second;
        m_slots[it->second].used = true;
    }
    for (std::size_t i = 0; i < lights.size(); ++i) {
        if (m_assigned[i] != kNone) continue;
        m_assigned[i] = SlotFor(registry, lights[i].ownerId);
        m_slots[m_assigned[i]].used = true;
    }

    // The intensity a light pass's ALPHA carried in 0.7.12 (BeginLightPass kept
    // lightIntensity apart from the colour, @0x43a1b0), for the sprites that
    // alpha-test their passes (render/Lighting.cpp, THE LIGHT PASS'S ALPHA
    // TEST). Unflickered: the flicker is the port's, and never reached an alpha.
    registry.ctx().insert_or_assign(Light2DAlphaTest{snapshot.lightIntensity});

    m_flameOwners = FlameOwners(snapshot);
    m_enabled = 0;
    m_halos = 0;
    m_dropped = 0;
    for (std::size_t i = 0; i < lights.size(); ++i) {
        const Eth::LightDraw& light = lights[i];
        Slot& slot = m_slots[m_assigned[i]];
        const float flicker = (m_torchFlicker && IsTorch(light, m_flameOwners))
                                  ? TorchFlickerFactor(light.ownerId, snapshot.timeMs)
                                  : 1.0f;

        LightState state = ComputeLight(light, snapshot.lightIntensity, flicker);
        if (state.enabled) {
            if (m_enabled >= kMaxLights) {
                state.enabled = false;
                ++m_dropped;
            } else {
                ++m_enabled;
            }
        }
        if (slot.light == entt::null || !registry.valid(slot.light)) slot.light = CreateLight(registry);
        registry.get<TransformComponent>(slot.light).position = glm::vec3(state.position, 0.0f);
        auto& component = registry.get<Light2DComponent>(slot.light);
        component.color = state.color;
        component.intensity = state.intensity;
        component.range = state.range;
        component.height = state.height;
        component.layers = state.layers;
        component.baked = state.baked;
        component.enabled = state.enabled;

        const HaloState halo = ComputeHalo(light, snapshot.zAxisDirection, order.haloRank + static_cast<int>(i),
                                           flicker);
        if (halo.visible && m_textures != nullptr && slot.haloPath != halo.bitmapPath) {
            slot.haloPath = halo.bitmapPath;
            slot.haloKey = m_textures->Key(halo.bitmapPath, TextureVariant::Additive);
        }
        // An unreadable bitmap draws nothing, as DrawHalo does without m_pHalo.
        const bool showHalo = halo.visible && m_textures != nullptr && !slot.haloKey.empty();
        if (!showHalo) {
            if (slot.halo != entt::null && registry.valid(slot.halo)) {
                registry.get<RenderableComponent>(slot.halo).isVisible = false;
            }
            continue;
        }
        if (slot.halo == entt::null || !registry.valid(slot.halo)) slot.halo = CreateHalo(registry);
        auto& transform = registry.get<TransformComponent>(slot.halo);
        transform.position = halo.position;
        transform.scale = glm::vec3(halo.size, 1.0f);
        auto& material = registry.get<MaterialComponent>(slot.halo);
        // Written only when they change: the texture path is in SyncResources'
        // signature, and rewriting the same string every frame is churn.
        if (material.albedoTexturePath != slot.haloKey) material.albedoTexturePath = slot.haloKey;
        if (material.albedoColor != halo.color) material.albedoColor = halo.color;
        registry.get<RenderableComponent>(slot.halo).isVisible = true;
        ++m_halos;
    }

    for (const Slot& slot : m_slots) {
        if (!slot.used) DisableSlot(registry, slot);
    }

    if (m_dropped > 0 && !m_warnedCap) {
        m_warnedCap = true;
        SUPERSONIC_LOG_WARN("Penumbra") << "LightRenderer: " << (m_enabled + m_dropped)
                                        << " lights in one frame; the engine lights at most " << kMaxLights
                                        << ", so the last " << m_dropped
                                        << " in snapshot order are left off (logged once)." << std::endl;
    }
}

} // namespace Penumbra::Render
