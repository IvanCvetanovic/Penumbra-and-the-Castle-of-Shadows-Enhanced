#include "render/ShadowRenderer.hpp"

#include <algorithm>
#include <cmath>

#include "core/Components.hpp"
#include "core/Log.hpp"
#include "core/MeshData.hpp"
#include "render/Lighting.hpp"
#include "render/TextureDecode.hpp"
#include "renderer/MeshRegistry.hpp"

namespace Penumbra::Render {

namespace {

// AM_PIXEL alpha-tests alpha > 1/255 (ALPHAREF 1, GREATER; GameSpace.dll
// SetAlphaMode @0x10006170): a byte of 1 is discarded, 2 is kept.
constexpr float kAlphaTestCutoff = 1.5f / 255.0f;

const std::string kShadowTexture = "data/shadow.dds";

// DrawProjShadow's length before shadowLengthScale, for one length factor
// (E:ETHRenderEntity.cpp:881-901).
float ShadowLength(const glm::vec3& pe, const glm::vec3& pl, float height, float planarDist, float factor) {
    float length = 0.0f;
    if ((pe.z + height) < pl.z) {
        // The light is above the caster's top: project the top onto the
        // ground plane z = 0, then clamp - "not realistic, but it looks better
        // for the real-time shadows" (:891-893).
        const float verticalDist = std::abs((pe.z + height) - pl.z);
        const float totalDist = (planarDist / verticalDist) * std::abs(pl.z);
        length = std::min(height * ShadowRenderer::kFakeStretch, totalDist - planarDist);
    } else {
        length = height * factor;
    }
    return std::max(length, height);
}

} // namespace

ShadowRenderer::ShadowRenderer() : m_bakedOwnLight(BakedShadowsOwnLight()) {}

// ---- the pure mapping -------------------------------------------------------------

ShadowGeometry ShadowRenderer::ComputeShadow(const Eth::SpriteDraw& caster, const Eth::LightDraw& light,
                                             const glm::vec3& sceneAmbient, bool capBakedLength,
                                             bool bakedOwnLight) {
    ShadowGeometry g;
    // BeginShadowPass (E:ETHShaderManager.cpp:369-371): the light casts and the
    // entity casts; DrawProjShadow needs a sprite (E:ETHRenderEntity.cpp:824).
    if (!caster.castShadow || !light.castShadows || caster.sprite.empty()) return g;
    g.baked = caster.isStatic && light.isStatic;
    // A real-time shadow is drawn inside the caster's light pass, which
    // BeginLightPass refuses to an entity that does not apply light
    // (E:ETHShaderManager.cpp:217). The lightmap bake asks no such thing.
    if (!g.baked && !caster.applyLight) return g;

    const glm::vec3 pe = caster.position;
    const glm::vec3 pl = light.position;
    // "if the object is higher than the light, then the shadow shouldn't be
    // cast on the floor" (:840-844).
    if (pl.z < pe.z) return g;
    const glm::vec3 diff = pe - pl;
    const float squaredDist = glm::dot(diff, diff);
    const float squaredRange = light.range * light.range;
    if (squaredDist > squaredRange) return g;   // :876-879
    const glm::vec2 toLight = glm::vec2(pl) - glm::vec2(pe);
    const float planarDist = glm::length(toLight);
    if (planarDist < kMinPlanarDistance) return g;

    const float scale = (caster.shadowScale <= 0.0f) ? 1.0f : caster.shadowScale;      // :847
    const float opacity = (caster.shadowOpacity <= 0.0f) ? 1.0f : caster.shadowOpacity; // :848
    const float height = caster.size.y;
    g.width = caster.size.x * kScaleX * scale;
    g.length = ShadowLength(pe, pl, height, planarDist, g.baked ? kScaleYBaked : kScaleYRealTime) *
               caster.shadowLengthScale;
    // entityZ = max(m_shadowZ, z), m_shadowZ being 0 for every entity (:905, :68).
    const float entityZ = std::max(0.0f, pe.z);
    // Drawn into a lightmap: the length stays the bake's, and the alpha is
    // maxOpacity's (below).
    const bool asBaked = g.baked && bakedOwnLight;

    if (g.baked && capBakedLength && !asBaked) {
        // ENHANCEMENT (ShadowRenderer.hpp, LENGTH): the visible end at most
        // where the light still reaches, at the caster's height. Along the
        // strip's middle the texture's v runs from 1 at the base, planarDist -
        // 0.21 - push from the light, to 0 at the apex, planarDist + 0.79 +
        // length - push, with push = length/6 - entityZ; so v = kFadedV lies
        // planarDist + entityZ + 0.79 - kFadedV + length (1 - 1/6 - kFadedV)
        // out, and length <= (reach - planarDist - entityZ - 0.79 + kFadedV)
        // / (5/6 - kFadedV), that is x 96/65 for shadow.dds. Never shorter than the
        // caster is tall, 0.7.12's own minimum (:901) - and not floored at the
        // real-time length, which shadowLengthScale can put far past the light
        // (pvp_lv2's single_tile_shadow: 9.5 x 64 = 608 against a crystal
        // reaching 236).
        const float dz = pl.z - pe.z;
        const float reach = std::sqrt(std::max(0.0f, squaredRange - dz * dz));
        const float fits =
            (reach - planarDist - entityZ - kOriginY + kFadedV) / (5.0f / 6.0f - kFadedV);
        g.length = std::min(g.length, std::max(height, fits));
    }

    // THE ALPHA. Into a lightmap maxOpacity kept attenBias at 1 (:919-921,
    // GenerateLightmap passes true, E:ETHRenderEntity.cpp:514). Otherwise the
    // real-time formula (:918-935), for the overlaid baked pairs too
    // (ShadowRenderer.hpp, ALPHA). The light's colour is the one the frame's
    // light list held, dimmed by the particle ratio when it is not static
    // (E:ETHScene.cpp:821-826).
    float attenBias = 1.0f;
    if (!asBaked) {
        const glm::vec3 color = light.color * (light.isStatic ? 1.0f : light.particleRatio);
        attenBias = 1.0f - squaredDist / std::max(squaredDist, squaredRange);
        attenBias *= std::min(std::max({color.r, color.g, color.b}), 1.0f);
        const float ambientColorLen = 1.0f - (sceneAmbient.r + sceneAmbient.g + sceneAmbient.b) / 3.0f;
        attenBias = std::min(attenBias * ambientColorLen, 1.0f);
        attenBias *= std::clamp(1.0f - pe.z / std::max(height, 1.0f), 0.0f, 1.0f);
    }
    // GS_BYTE(attenBias * 255 * opacity): truncated (:935), and nothing under 8 (:937).
    const float byteValue = attenBias * 255.0f * opacity;
    g.alpha8 = byteValue <= 0.0f ? 0 : static_cast<int>(std::min(byteValue, 255.0f));
    if (g.alpha8 < kMinAlpha8) return g;
    g.alpha = static_cast<float>(g.alpha8) / 255.0f;

    // THE STRIP (dynaShadowVS.cg transformSprite). GetAngle(light - caster)
    // turns the quad so its local +y points at the light and its +x across it
    // (gs2d uploads RotateZ row-major, cgSetMatrixParameterfr, and the shader
    // multiplies mul(rotationMatrix, v)).
    g.anchor = glm::vec2(pe);
    const glm::vec2 towardLight = toLight / planarDist;
    const glm::vec2 across(towardLight.y, -towardLight.x);
    const float pushBack = g.length / 6.0f - entityZ;
    for (std::size_t i = 0; i < kStripUv.size(); ++i) {
        const glm::vec2 local = kStripUv[i] * glm::vec2(g.width, 1.0f) - glm::vec2(kOriginX * g.width, kOriginY);
        glm::vec2 q = g.anchor + across * local.x + towardLight * local.y;
        // The v = 0 vertices go radially away from the light, measured from
        // where they stand before the push (shadowDir is taken first).
        glm::vec2 radial(0.0f);
        const float extrude = 1.0f - kStripUv[i].y;
        if (extrude > 0.0f) {
            const glm::vec2 fromLight = q - glm::vec2(pl);
            const float reachLength = glm::length(fromLight);
            if (reachLength > 0.0f) radial = fromLight / reachLength * (g.length * extrude);
        }
        // "push back the shadow a little bit so it won't look odd": towards the
        // light, -lightVec with lightVec = normalize(caster - light).
        q += towardLight * pushBack;
        q += radial;
        g.vertices[i] = q;
    }
    g.visible = true;
    return g;
}

Supersonic::Light2DShadowMask ShadowRenderer::DecodeMask(const std::string& shadowDdsPath) {
    Supersonic::Light2DShadowMask mask;
    // Plain: no colour key, the alpha as the file holds it - the channel
    // shadow.dds's A8L8 carries the shape in (its luminance is 0 throughout).
    const DecodedImage image = DecodeTexture(shadowDdsPath, TextureVariant::Plain);
    if (!image.Valid()) return mask;
    const auto texels = static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height);
    if (texels > Supersonic::kMaxShadowMask2DTexels) return mask;
    mask.width = static_cast<std::uint32_t>(image.width);
    mask.height = static_cast<std::uint32_t>(image.height);
    mask.alpha.resize(texels);
    for (std::size_t t = 0; t < texels; ++t) mask.alpha[t] = static_cast<float>(image.rgba[t * 4 + 3]) / 255.0f;
    return mask;
}

// ---- the pool ----------------------------------------------------------------------

void ShadowRenderer::Attach(entt::registry& registry, TextureCache& textures) {
    m_textures = &textures;
    if (auto* const* meshes = registry.ctx().find<Supersonic::MeshRegistry*>()) m_meshes = *meshes;
    m_shadowKeyResolved = false;
}

void ShadowRenderer::Detach(entt::registry& registry) {
    Supersonic::MeshRegistry* meshes = nullptr;
    if (auto* const* found = registry.ctx().find<Supersonic::MeshRegistry*>()) meshes = *found;
    for (Slot& slot : m_slots) {
        if (slot.entity != entt::null && registry.valid(slot.entity)) registry.destroy(slot.entity);
        // Hands the buffers to the device's deferred-destroy queue.
        if (meshes != nullptr && slot.hasMesh) meshes->Invalidate(slot.meshKey);
    }
    m_slots.clear();
    m_byPair.clear();
    m_seen.clear();
    m_bakedStrips.clear();
    m_bakedStripCount = 0;
    registry.ctx().erase<Supersonic::Light2DShadowMask>();
    m_textures = nullptr;
    m_meshes = nullptr;
    m_shadowKey.clear();
    m_shadowKeyResolved = false;
    m_shown = 0;
    m_rebuilds = 0;
}

std::size_t ShadowRenderer::SlotFor(entt::registry& registry, const PairKey& pair) {
    // Keys are unique within a Draw (the occurrence sees to it), so a mapped
    // slot is never already taken.
    if (const auto it = m_byPair.find(pair); it != m_byPair.end()) return it->second;

    // A new pair. Hidden slots stay with their pairs (so the camera scrolling
    // back rebuilds nothing) until the pool is at its cap; then the one unused
    // longest goes to the newcomer.
    if (m_slots.size() >= kPoolSoftCap) {
        std::size_t oldest = m_slots.size();
        for (std::size_t s = 0; s < m_slots.size(); ++s) {
            if (m_slots[s].used) continue;
            if (oldest == m_slots.size() || m_slots[s].lastUsed < m_slots[oldest].lastUsed) oldest = s;
        }
        if (oldest < m_slots.size()) {
            Slot& slot = m_slots[oldest];
            if (const auto it = m_byPair.find(slot.pair); it != m_byPair.end() && it->second == oldest) {
                m_byPair.erase(it);
            }
            slot.pair = pair;
            m_byPair[pair] = oldest;
            return oldest;
        }
    }

    Slot slot;
    slot.meshKey = "penumbra:shadow:" + std::to_string(m_nextKey++);
    slot.pair = pair;
    slot.entity = CreateEntity(registry, slot.meshKey);
    m_slots.push_back(std::move(slot));
    m_byPair[pair] = m_slots.size() - 1;
    return m_slots.size() - 1;
}

entt::entity ShadowRenderer::CreateEntity(entt::registry& registry, const std::string& meshKey) {
    using namespace Supersonic;
    const entt::entity e = registry.create();
    registry.emplace<TagComponent>(e, std::string("Penumbra Shadow"));
    registry.emplace<TransformComponent>(e);
    registry.emplace<MeshComponent>(e).meshKey = meshKey;
    auto& material = registry.emplace<MaterialComponent>(e);
    // Black through shadow.dds's alpha, mixed over what is behind: AM_PIXEL.
    // Unlit - no light reaches a shadow - and no ambient either.
    material.unlit = true;
    material.transparent = true;
    material.blend = MaterialComponent::BlendMode::Alpha;
    material.alphaCutoff = kAlphaTestCutoff;
    material.albedoColor = glm::vec4(0.0f);
    auto& renderable = registry.emplace<RenderableComponent>(e);
    renderable.castsShadow = false;
    // Invisible until its mesh exists: a meshKey that names nothing leaves the
    // renderable's meshID at whatever it was.
    renderable.isVisible = false;
    return e;
}

bool ShadowRenderer::Upload(entt::registry& registry, Slot& slot, const std::array<glm::vec2, 5>& local) {
    using namespace Supersonic;
    if (m_meshes == nullptr) return false;

    MeshData data;
    data.vertices.reserve(local.size());
    for (std::size_t i = 0; i < local.size(); ++i) {
        Vertex v{};
        v.pos = glm::vec3(local[i], 0.0f);
        v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        v.color = glm::vec3(1.0f);       // the shader multiplies by it
        v.texCoord = kStripUv[i];
        data.vertices.push_back(v);
    }
    data.indices.assign(kIndices.begin(), kIndices.end());
    // Frustum culling reads these; a mesh without them is culled by its centre.
    data.computeBounds();

    bool ok = false;
    const std::uint32_t id = m_meshes->Find(slot.meshKey);
    if (id == MeshRegistry::kInvalidMesh) {
        const std::uint32_t uploaded = m_meshes->Upload(slot.meshKey, data);
        ok = uploaded != MeshRegistry::kInvalidMesh && uploaded != m_meshes->GetCubeMesh();
    } else {
        ok = m_meshes->Replace(id, data);
    }
    if (!ok) return false;

    slot.built = local;
    slot.hasMesh = true;
    ++m_rebuilds;
    // SyncResources re-reads meshKey only when the renderable's signature moves,
    // and an Upload moves no generation (a Replace does): make it look again.
    registry.get<RenderableComponent>(slot.entity).resourceSignature = 0;
    return true;
}

void ShadowRenderer::Hide(entt::registry& registry, const Slot& slot) {
    if (slot.entity == entt::null || !registry.valid(slot.entity)) return;
    auto& renderable = registry.get<Supersonic::RenderableComponent>(slot.entity);
    if (renderable.isVisible) renderable.isVisible = false;
}

void ShadowRenderer::Draw(entt::registry& registry, const Eth::RenderSnapshot& snapshot, const View& /*view*/,
                          const DrawOrder& order, const Eth::RenderSnapshot* shapes) {
    using namespace Supersonic;
    // The shape's source: `snapshot` itself unless an aligned `shapes` is given.
    const Eth::RenderSnapshot& shape = (shapes != nullptr && shapes->sprites.size() == snapshot.sprites.size() &&
                                        shapes->lights.size() == snapshot.lights.size())
                                           ? *shapes
                                           : snapshot;
    ++m_draws;
    m_shown = 0;
    m_rebuilds = 0;
    for (Slot& slot : m_slots) slot.used = false;
    m_seen.clear();
    // One list per light of this snapshot, emptied (their storage kept), so a
    // light no strip reaches this frame hands the engine none.
    m_bakedStrips.resize(snapshot.lights.size());
    for (LightStrips& strips : m_bakedStrips) strips.clear();
    m_bakedStripCount = 0;

    MeshRegistry* meshes = nullptr;
    if (auto* const* found = registry.ctx().find<MeshRegistry*>()) meshes = *found;
    if (meshes != m_meshes) {
        // A different registry holds none of what was built: build again.
        for (Slot& slot : m_slots) slot.hasMesh = false;
        m_meshes = meshes;
    }
    if (!m_shadowKeyResolved && m_textures != nullptr) {
        m_shadowKey = m_textures->Key(kShadowTexture, TextureVariant::Plain);
        m_shadowKeyResolved = true;
    }
    if (m_shadowKey.empty()) {
        // Without the texture the strip would draw as a hard grey polygon,
        // which is worse than no shadow.
        if (!m_warnedNoTexture && m_shadowKeyResolved) {
            m_warnedNoTexture = true;
            SUPERSONIC_LOG_WARN("Penumbra") << "ShadowRenderer: " << kShadowTexture
                                            << " did not load; projected shadows are off." << std::endl;
        }
        for (const Slot& slot : m_slots) Hide(registry, slot);
        return;
    }

    // The mask every baked strip samples: shadow.dds's alpha, as the overlay
    // draws it. Decoded once; published whenever the context lacks it (it is
    // runtime state, and Detach takes it away).
    if (m_bakedOwnLight) {
        if (!m_maskDecoded && m_textures != nullptr) {
            m_mask = DecodeMask(m_textures->GameRoot() + "/" + kShadowTexture);
            m_maskDecoded = true;
        }
        if (!m_mask.alpha.empty() && registry.ctx().find<Light2DShadowMask>() == nullptr) {
            registry.ctx().emplace<Light2DShadowMask>(m_mask);
        }
    }

    for (std::size_t i = 0; i < snapshot.sprites.size(); ++i) {
        const Eth::SpriteDraw& caster = shape.sprites[i];
        if (!caster.castShadow) continue;
        // Where the strips stand: this caster in `snapshot`. The same point as
        // the shape's anchor when `shapes` is not given.
        const glm::vec2 placedAt(snapshot.sprites[i].position.x, snapshot.sprites[i].position.y);

        float z = 0.0f;
        if (i < order.shadowRankBase.size()) {
            z = RankZ(order.shadowRankBase[i]);
        } else if (i < order.spriteRank.size()) {
            // Half a rank behind the caster: between it and whatever precedes it.
            z = RankZ(order.spriteRank[i]) - 0.5f * kLayerStep;
        } else {
            if (!m_warnedNoRank) {
                m_warnedNoRank = true;
                SUPERSONIC_LOG_WARN("Penumbra") << "ShadowRenderer: the draw order has no rank for sprite " << i
                                                << "; its shadows are not drawn." << std::endl;
            }
            continue;
        }

        for (std::size_t li = 0; li < shape.lights.size(); ++li) {
            const Eth::LightDraw& light = shape.lights[li];
            const ShadowGeometry g = ComputeShadow(caster, light, shape.ambient, m_capBakedLength, m_bakedOwnLight);
            if (!g.visible) continue;

            // BAKED, AS BAKED (ShadowRenderer.hpp): the strip goes to its light,
            // placed where the caster stands, in engine axes; nothing is drawn.
            if (g.baked && m_bakedOwnLight) {
                Supersonic::Light2DShadowsComponent::Strip strip;
                for (std::size_t k = 0; k < g.vertices.size(); ++k) {
                    strip.corners[k] = glm::vec2(ToWorld(g.vertices[k] - g.anchor + placedAt, 0.0f));
                }
                strip.opacity = g.alpha;
                m_bakedStrips[li].push_back(strip);
                ++m_bakedStripCount;
                continue;
            }

            const int occurrence = m_seen[{caster.entityId, light.ownerId}]++;
            // Taken before the reference: SlotFor may grow the pool.
            const std::size_t index = SlotFor(registry, PairKey{caster.entityId, light.ownerId, occurrence});
            Slot& slot = m_slots[index];
            slot.used = true;
            slot.lastUsed = m_draws;
            // Something cleared the registry under the pool (a queued engine
            // scene load clears and refills it, SupersonicApp.cpp:1608), as the
            // other renderers' pools allow for. The mesh lives in the
            // MeshRegistry, not the registry, so it is still there to name.
            if (slot.entity == entt::null || !registry.valid(slot.entity)) {
                slot.entity = CreateEntity(registry, slot.meshKey);
            }

            // The caster's frame, engine axes: a pair whose relative geometry
            // holds still keeps its mesh wherever the two go.
            std::array<glm::vec2, 5> local{};
            for (std::size_t k = 0; k < local.size(); ++k) {
                const glm::vec2 offset = g.vertices[k] - g.anchor;
                local[k] = glm::vec2(offset.x, -offset.y);
            }
            bool changed = !slot.hasMesh;
            for (std::size_t k = 0; !changed && k < local.size(); ++k) {
                const glm::vec2 delta = glm::abs(local[k] - slot.built[k]);
                changed = delta.x > kRebuildEpsilon || delta.y > kRebuildEpsilon;
            }
            if (changed) Upload(registry, slot, local);

            auto& transform = registry.get<TransformComponent>(slot.entity);
            transform.position = ToWorld(placedAt, z);
            auto& material = registry.get<MaterialComponent>(slot.entity);
            if (material.albedoTexturePath != m_shadowKey) material.albedoTexturePath = m_shadowKey;
            const glm::vec4 color(0.0f, 0.0f, 0.0f, g.alpha);
            if (material.albedoColor != color) material.albedoColor = color;
            auto& renderable = registry.get<RenderableComponent>(slot.entity);
            if (renderable.isVisible != slot.hasMesh) renderable.isVisible = slot.hasMesh;
            if (slot.hasMesh) ++m_shown;
        }
    }

    for (const Slot& slot : m_slots) {
        if (!slot.used) Hide(registry, slot);
    }
}

} // namespace Penumbra::Render
