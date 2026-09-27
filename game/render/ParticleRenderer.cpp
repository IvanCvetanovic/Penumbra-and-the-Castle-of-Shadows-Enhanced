#include "render/ParticleRenderer.hpp"

#include <algorithm>

#include "core/Log.hpp"

namespace Penumbra::Render {

namespace {

using Supersonic::MaterialComponent;

// GS_ALPHA_MODE as a particle system can carry it; anything else is drawn as
// PIXEL, the default a <ParticleSystem> without the attribute had.
Eth::ALPHA_MODE Normalised(const Eth::ALPHA_MODE mode) {
    switch (mode) {
    case Eth::AM_PIXEL:
    case Eth::AM_ADD:
    case Eth::AM_ALPHA_TEST:
    case Eth::AM_NONE:
    case Eth::AM_MODULATE:
        return mode;
    }
    return Eth::AM_PIXEL;
}

// ALPHAREF 1, GREATER (GameSpace.dll SetAlphaMode, DLL@0x10006170): a byte
// alpha of 2 survives, 1 does not. As a float cut-off, halfway between.
constexpr float kAlphaTestCutoff = 1.5f / 255.0f;

} // namespace

TextureVariant ParticleRenderer::VariantFor(const Eth::ALPHA_MODE mode) {
    // 0.7.12 keyed a particle bitmap with black only for AM_ADD, magenta for
    // every other mode (E:ETHRenderEntity.cpp:365-370 -> ETHResourceManager.cpp:106).
    // Its resource cache was keyed by file name, so a bitmap first loaded by an
    // ADD system kept the black key for a PIXEL one too; no shipped bitmap is
    // used under two modes (docs/spec/21 §1.2), so each mode's own key is exact.
    switch (Normalised(mode)) {
    case Eth::AM_ADD:
        return TextureVariant::Additive;
    case Eth::AM_MODULATE:
        return TextureVariant::Modulate;
    default:
        return TextureVariant::Sprite;
    }
}

std::string ParticleRenderer::BitmapPath(const std::string& bitmap) {
    const std::size_t slash = bitmap.find_last_of("/\\");
    return "particles/" + (slash == std::string::npos ? bitmap : bitmap.substr(slash + 1));
}

ParticleQuad ParticleRenderer::ComputeQuad(const Eth::ParticleDraw& particle, const int rank,
                                           const glm::ivec2& bitmapSize, const bool fadeAdditiveByAlpha) {
    ParticleQuad quad;
    const Eth::ALPHA_MODE mode = Normalised(particle.alphaMode);
    quad.variant = VariantFor(mode);

    // The vertex colour as 0.7.12 made it: (GS_BYTE)(c * 255), truncated
    // (E:ETHParticleManager.cpp:863-867). Only the alpha test needs the byte.
    const glm::vec4 colour = glm::clamp(particle.color, glm::vec4(0.0f), glm::vec4(1.0f));
    const int alphaByte = static_cast<int>(colour.a * 255.0f);
    // Every mode but NONE alpha-tests tex.a x vertex.a > 1/255, and a vertex
    // alpha that fails it fails for every texel: the particle is not drawn.
    const bool passesAlphaTest = mode == Eth::AM_NONE || alphaByte > 1;
    quad.visible = particle.size > 0.0f && passesAlphaTest;

    switch (mode) {
    case Eth::AM_ADD:
        // One, One (docs/spec/30 §3.3). The engine's Additive is SrcAlpha, One;
        // with the Additive variant's alpha forced to 1 and the colour's alpha
        // 1 the two are the same sum, dst + tex.rgb * colour.rgb. The vertex
        // alpha fed nothing but the test above, so the original faded an ADD
        // particle only through its rgb (big_explosion's Color1 is black).
        quad.blend = MaterialComponent::BlendMode::Additive;
        quad.color = glm::vec4(fadeAdditiveByAlpha ? glm::vec3(colour) * colour.a : glm::vec3(colour), 1.0f);
        break;
    case Eth::AM_MODULATE:
        // APPROXIMATE. Zero, SrcColor multiplies what is behind by
        // tex.rgb * colour.rgb; the engine has no multiply blend. The Modulate
        // variant carries (1 - luminance) * texel alpha in a black texel, so
        // Alpha blending black over dst leaves dst * luminance(tex): exact for
        // a grey bitmap under a white vertex colour, which is every shipped
        // AM_MODULATE system (black_opaque.png, Color0 = Color1 = white; the
        // luminance rule gives MODULATE (1,1,1), docs/spec/21 §1.5). A tinted
        // vertex colour is dropped. Its alpha fed only the test, so it is 1.
        quad.blend = MaterialComponent::BlendMode::Alpha;
        quad.color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        break;
    case Eth::AM_ALPHA_TEST:
        // Blending off, texels past the test drawn solid. No particle system
        // ships with it; drawn as a cut-out that still blends what survives.
        quad.blend = MaterialComponent::BlendMode::Alpha;
        quad.color = glm::vec4(glm::vec3(colour), 1.0f);
        quad.alphaCutoff = kAlphaTestCutoff;
        break;
    case Eth::AM_NONE:
        // Blending and the test both off. No particle system ships with it.
        quad.blend = MaterialComponent::BlendMode::Alpha;
        quad.color = glm::vec4(glm::vec3(colour), 1.0f);
        break;
    default:
        // AM_PIXEL: SrcAlpha, InvSrcAlpha of tex x colour, which is the
        // engine's Alpha exactly.
        quad.blend = MaterialComponent::BlendMode::Alpha;
        quad.color = colour;
        break;
    }

    // Centred on its position, unrounded (docs/spec/30 §3.4: particles skip
    // the floor; the -0.5 was D3D9's texel alignment, which Vulkan does not need).
    quad.position = ToWorld(particle.position, RankZ(rank));
    quad.scale = glm::vec3(particle.size, particle.size, 1.0f);

    // The SAME sign, not the opposite one. GS2D builds RotateZ as the rows
    // (cos, sin) / (-sin, cos) (G:gs2dmath.h:699-708), uploads it row-major
    // (cgSetMatrixParameterfr, G:Video/Direct3D9/gs2dD3D9CgShader.cpp:289) and
    // applies mul(rotationMatrix, pos) in y-DOWN pixels before the screen flip
    // (G:gs2dcgshadercode.h transformSprite); 2010's GameSpace.dll carries the
    // same shader text and the same import (reference/analysis/gs_strings.txt
    // 1153, 1859). So x' = x cos + y sin, y' = -x sin + y cos: at +90 degrees
    // screen-down (0,1) turns to screen-right (1,0) - counter-clockwise on
    // screen, which is also why GetAngle = atan2(x, y) calls +x 90 degrees.
    // The engine's +z rotation is counter-clockwise on screen too (y up,
    // looking down -Z). Magic Portals' Particles::WorldRotation lands on the
    // same +radians through two negations.
    quad.rotationZ = glm::radians(particle.angle);

    // The cell. 0.7.12 set a rect only when the cut is more than 1x1, cutting
    // the bitmap's ORIGINAL size into integer strides, row-major from the top
    // left (E:ETHParticleManager.cpp:889-893; G:Video/Direct3D9/gs2dD3D9Sprite.cpp:217-247).
    const int cutX = std::max(1, particle.spriteCutX);
    const int cutY = std::max(1, particle.spriteCutY);
    if (cutX > 1 || cutY > 1) {
        const Eth::uint cells = static_cast<Eth::uint>(cutX) * static_cast<Eth::uint>(cutY);
        // Out of range, 0.7.12 refused the SetRect and drew the previous
        // particle's rect; the last cell is the nearest well-defined answer.
        const Eth::uint cell = std::min(particle.frame, cells - 1u);
        const float column = static_cast<float>(cell % static_cast<Eth::uint>(cutX));
        const float row = static_cast<float>(cell / static_cast<Eth::uint>(cutX));
        const int strideX = bitmapSize.x / cutX;
        const int strideY = bitmapSize.y / cutY;
        if (strideX > 0 && strideY > 0) {
            const glm::vec2 bitmap(bitmapSize);
            const glm::vec2 stride(static_cast<float>(strideX), static_cast<float>(strideY));
            quad.uvScale = stride / bitmap;
            quad.uvOffset = glm::vec2(column, row) * stride / bitmap;
        } else {
            // Size unknown: even cells, exact for every shipped sheet.
            quad.uvScale = glm::vec2(1.0f / static_cast<float>(cutX), 1.0f / static_cast<float>(cutY));
            quad.uvOffset = glm::vec2(column, row) * quad.uvScale;
        }
    }
    return quad;
}

void ParticleRenderer::Attach(entt::registry& /*registry*/, TextureCache& textures) {
    // Quads are made on first need, so an idle scene owns none.
    m_textures = &textures;
}

void ParticleRenderer::Detach(entt::registry& registry) {
    for (auto& [key, pool] : m_pools) {
        for (const entt::entity quad : pool.quads) {
            if (registry.valid(quad)) registry.destroy(quad);
        }
    }
    m_pools.clear();
    m_textures = nullptr;
    m_shown = 0;
    m_droppedCut = 0;
}

std::size_t ParticleRenderer::QuadCount() const {
    std::size_t count = 0;
    for (const auto& [key, pool] : m_pools) count += pool.quads.size();
    return count;
}

ParticleRenderer::Pool& ParticleRenderer::PoolFor(const std::string& bitmap, const Eth::ALPHA_MODE mode) {
    std::string path = BitmapPath(bitmap);
    auto [it, created] = m_pools.try_emplace(std::make_pair(path, static_cast<int>(mode)));
    Pool& pool = it->second;
    if (created) {
        pool.mode = mode;
        if (m_textures != nullptr) {
            // Once per pool: the key never changes, and an unreadable bitmap
            // (TextureCache logs it once) leaves the pool drawing nothing.
            pool.textureKey = m_textures->Key(path, VariantFor(mode));
            pool.bitmapSize = m_textures->Size(path);
        }
    }
    return pool;
}

entt::entity ParticleRenderer::CreateQuad(entt::registry& registry, const Pool& pool, const ParticleQuad& quad) {
    using namespace Supersonic;
    const entt::entity e = registry.create();
    registry.emplace<TagComponent>(e, "Particle");
    registry.emplace<TransformComponent>(e);
    registry.emplace<MeshComponent>(e).primitiveType = "Quad";
    auto& material = registry.emplace<MaterialComponent>(e);
    // Plain unlit: no Sprite2DLight, so no Light2D and no ambient reach it.
    material.unlit = true;
    material.transparent = true;
    material.blend = quad.blend;
    material.alphaCutoff = quad.alphaCutoff;
    material.albedoTexturePath = pool.textureKey;
    auto& renderable = registry.emplace<RenderableComponent>(e);
    renderable.castsShadow = false;
    renderable.isVisible = false;
    return e;
}

entt::entity ParticleRenderer::Acquire(entt::registry& registry, Pool& pool, const ParticleQuad& quad) {
    if (pool.used == pool.quads.size()) {
        pool.quads.push_back(CreateQuad(registry, pool, quad));
    } else if (!registry.valid(pool.quads[pool.used])) {
        // Something cleared the registry under the pool.
        pool.quads[pool.used] = CreateQuad(registry, pool, quad);
    }
    return pool.quads[pool.used++];
}

void ParticleRenderer::Hide(entt::registry& registry, const entt::entity quad) {
    using namespace Supersonic;
    if (!registry.valid(quad)) return;
    registry.get<RenderableComponent>(quad).isVisible = false;
    // The UV-slot gather counts every material, drawn or not: a hidden quad
    // left on a cell would hold a slot for nothing.
    auto& material = registry.get<MaterialComponent>(quad);
    material.uvScale = glm::vec2(1.0f);
    material.uvOffset = glm::vec2(0.0f);
}

void ParticleRenderer::Draw(entt::registry& registry, const Eth::RenderSnapshot& snapshot, const View& /*view*/,
                            const DrawOrder& order) {
    using namespace Supersonic;
    for (auto& [key, pool] : m_pools) pool.used = 0;
    m_shown = 0;
    m_droppedCut = 0;
    if (m_textures == nullptr) return;

    std::size_t cutQuads = 0;
    // Particles come system by system, so the pool of the previous one is
    // almost always the pool of this one.
    Pool* pool = nullptr;
    const std::string* poolBitmap = nullptr;
    Eth::ALPHA_MODE poolMode = Eth::AM_PIXEL;

    const std::size_t count = std::min(snapshot.particles.size(), order.particleRank.size());
    for (std::size_t i = 0; i < count; ++i) {
        const Eth::ParticleDraw& particle = snapshot.particles[i];
        const Eth::ALPHA_MODE mode = Normalised(particle.alphaMode);
        if (pool == nullptr || mode != poolMode || particle.bitmap != *poolBitmap) {
            pool = &PoolFor(particle.bitmap, mode);
            poolBitmap = &particle.bitmap;
            poolMode = mode;
        }
        if (pool->textureKey.empty()) continue;

        const ParticleQuad quad = ComputeQuad(particle, order.particleRank[i], pool->bitmapSize, m_fadeAdditiveByAlpha);
        if (!quad.visible) continue;
        if (quad.uvScale != glm::vec2(1.0f) || quad.uvOffset != glm::vec2(0.0f)) {
            if (cutQuads >= kMaxCutParticles) {
                ++m_droppedCut;
                if (!m_warnedCutBudget) {
                    m_warnedCutBudget = true;
                    SUPERSONIC_LOG_WARN("Penumbra") << "particles: more than " << kMaxCutParticles
                                                    << " sprite-sheet particles in one frame; the rest are not drawn"
                                                    << std::endl;
                }
                continue;
            }
            ++cutQuads;
        }

        const entt::entity e = Acquire(registry, *pool, quad);
        auto& transform = registry.get<TransformComponent>(e);
        transform.position = quad.position;
        transform.rotation = glm::vec3(0.0f, 0.0f, quad.rotationZ);
        transform.scale = quad.scale;
        auto& material = registry.get<MaterialComponent>(e);
        material.albedoColor = quad.color;
        material.uvScale = quad.uvScale;
        material.uvOffset = quad.uvOffset;
        registry.get<RenderableComponent>(e).isVisible = true;
        ++m_shown;
    }

    // Only what the last Draw showed and this one did not: the rest of a pool
    // is already hidden, and a frame drawn twice touches nothing here.
    for (auto& [key, p] : m_pools) {
        for (std::size_t q = p.used; q < p.shown && q < p.quads.size(); ++q) Hide(registry, p.quads[q]);
        p.shown = p.used;
    }
}

} // namespace Penumbra::Render
