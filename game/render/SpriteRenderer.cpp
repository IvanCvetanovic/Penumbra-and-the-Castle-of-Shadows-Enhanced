#include "render/SpriteRenderer.hpp"

#include <algorithm>
#include <cmath>

#include "core/Components.hpp"
#include "render/Lighting.hpp"

namespace Penumbra::Render {

namespace {

using Supersonic::MaterialComponent;

constexpr float kDegreesToRadians = 3.14159265358979323846f / 180.0f;

std::uint64_t PoolKey(int entityId, std::uint32_t occurrence) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(entityId)) << 16) | occurrence;
}

// Each field written only when it differs: a still frame writes nothing.
template <typename T>
void Assign(T& field, const T& value) {
    if (field != value) field = value;
}

// What the material of one drawn sprite says, apart from the lighting.
struct Look {
    MaterialComponent::BlendMode blend = MaterialComponent::BlendMode::Alpha;
    bool transparent = true;
    float alphaCutoff = SpriteRenderer::kAlphaTestCutoff;
    glm::vec4 colour{1.0f};
};

Look LookFor(const Eth::SpriteDraw& sprite, const SpriteLighting& lighting) {
    Look look;
    look.colour = sprite.color;
    switch (sprite.blendMode) {
    case Eth::AM_ADD:
        look.blend = MaterialComponent::BlendMode::Additive;
        // One, One ignores alpha except as the alpha test's gate (T.a * C.a >
        // 1/255); the Additive image's alpha is 1, so the colour's alpha only
        // gates.
        look.colour.a = sprite.color.a > 1.0f / 255.0f ? 1.0f : 0.0f;
        break;
    case Eth::AM_NONE:
        // No blend and no test: the image drawn solid, keyed texels black.
        look.transparent = false;
        look.alphaCutoff = 0.0f;
        break;
    case Eth::AM_MODULATE: // the image is black at 1 - luminance: mixed, not added
    case Eth::AM_ALPHA_TEST:
    case Eth::AM_PIXEL:
    default:
        look.blend = MaterialComponent::BlendMode::Alpha;
        break;
    }

    // THE LIGHT'S WEIGHT on a partly transparent texel. The 2010 horizontal
    // diffuse pass multiplies its output by the texel's alpha
    // (hPixelLight.cg:main, out = T*C*d*att*Lc*LI*T.a), which is Alpha: the
    // engine gives (base + lit) * alpha. The vertical pass (vPixelLight.cg:main)
    // and the gloss pass (mainSpecular, both files) do not weight it, which is
    // Premultiplied: base * alpha + lit. So unlike MPR's tint(), which turns
    // every lit sprite Premultiplied, only those two are (docs/spec/40 §2.8).
    // Only a mixed sprite has the choice: an additive one adds either way.
    if (lighting.lit && look.blend == MaterialComponent::BlendMode::Alpha && look.transparent &&
        sprite.blendMode != Eth::AM_MODULATE && (sprite.type == Eth::ET_VERTICAL || !sprite.gloss.empty())) {
        look.blend = MaterialComponent::BlendMode::Premultiplied;
    }
    return look;
}

} // namespace

// ---- the pure halves ------------------------------------------------------------

TextureVariant SpriteRenderer::VariantFor(Eth::ALPHA_MODE mode) {
    switch (mode) {
    case Eth::AM_ADD: return TextureVariant::Additive;
    case Eth::AM_MODULATE: return TextureVariant::Modulate;
    default: return TextureVariant::Sprite;
    }
}

void SpriteRenderer::FrameUv(const Eth::SpriteDraw& sprite, const glm::vec2& bitmap, glm::vec2& uvScale,
                             glm::vec2& uvOffset) {
    uvScale = glm::vec2(1.0f);
    uvOffset = glm::vec2(0.0f);
    if (!(bitmap.x > 0.0f && bitmap.y > 0.0f) || !(sprite.size.x > 0.0f && sprite.size.y > 0.0f)) return;

    // Cells run left to right, then top to bottom (ETHRenderEntity::SetFrame);
    // the cell's corner is its column and row times the frame size, which is
    // 0.7.12's rectPos, whatever rounding the runtime gave the frame size.
    const std::uint32_t columns = static_cast<std::uint32_t>(std::max(1, sprite.spriteCutX));
    const std::uint32_t rows = static_cast<std::uint32_t>(std::max(1, sprite.spriteCutY));
    const std::uint32_t cell = sprite.frame % (columns * rows);
    const glm::vec2 corner(static_cast<float>(cell % columns) * sprite.size.x,
                           static_cast<float>(cell / columns) * sprite.size.y);
    uvScale = sprite.size / bitmap;
    uvOffset = corner / bitmap;
    // A whole image names no transform, so it takes no UV slot.
    if (std::fabs(uvScale.x - 1.0f) < 1e-6f && std::fabs(uvScale.y - 1.0f) < 1e-6f) uvScale = glm::vec2(1.0f);
}

void SpriteRenderer::Placement(const Eth::SpriteDraw& sprite, const glm::vec2& zAxisDirection, float engineZ,
                               glm::vec3& position, glm::vec3& scale, float& rotationZ) {
    // The engine's quad is centred on its position; 0.7.12's origin is the
    // top-left corner it drew at.
    const glm::vec2 centre = sprite.origin + sprite.size * 0.5f;
    position = ToWorld(centre, engineZ);
    scale = glm::vec3(sprite.size.x, sprite.size.y, 1.0f);
    rotationZ = 0.0f;
    if (sprite.angle == 0.0f || sprite.type == Eth::ET_VERTICAL || !std::isfinite(sprite.angle)) return;

    // Turned about the entity's screen position (ToScreenPos: xy + ZAxisDirection
    // * z), the point transformSprite rotates the corner offsets about. The
    // SENSE is the one render/View.hpp (ANGLE) derives from GS2D's RotateZ and
    // transformSprite, which the particles use too: a positive angle turns
    // counter-clockwise on screen, engine +radians. No shipped entity is ever
    // turned (docs/spec/20 Key facts: angle is always 0).
    const glm::vec2 pivot = glm::vec2(sprite.position.x, sprite.position.y) + zAxisDirection * sprite.position.z;
    const glm::vec3 pivotWorld = ToWorld(pivot, engineZ);
    rotationZ = sprite.angle * kDegreesToRadians;
    const float c = std::cos(rotationZ);
    const float s = std::sin(rotationZ);
    const glm::vec2 arm(position.x - pivotWorld.x, position.y - pivotWorld.y);
    position.x = pivotWorld.x + c * arm.x - s * arm.y;
    position.y = pivotWorld.y + s * arm.x + c * arm.y;
}

// ---- the pool --------------------------------------------------------------------

void SpriteRenderer::Attach(entt::registry& registry, TextureCache& textures) {
    m_textures = &textures;
    textures.Attach(registry);
}

void SpriteRenderer::Detach(entt::registry& registry) {
    const auto destroy = [&registry](entt::entity e) {
        if (e != entt::null && registry.valid(e)) registry.destroy(e);
    };
    for (auto& [key, slot] : m_slots) destroy(slot.quad);
    for (const entt::entity e : m_free) destroy(e);
    destroy(m_background);
    m_slots.clear();
    m_free.clear();
    m_background = entt::null;
    m_backgroundSource.clear();
    m_backgroundKey.clear();
    m_visible = 0;
    m_textures = nullptr;
}

std::string SpriteRenderer::imagePath(const std::string& relativePath) const {
    if (m_localization != nullptr) {
        std::string variant = m_localization->ImageVariant(relativePath, m_localization->CurrentLanguage());
        if (!variant.empty()) return variant;
    }
    return relativePath;
}

entt::entity SpriteRenderer::QuadFor(int entityId) const {
    const auto it = m_slots.find(PoolKey(entityId, 0));
    return (it != m_slots.end() && it->second.used) ? it->second.quad : entt::null;
}

entt::entity SpriteRenderer::makeQuad(entt::registry& registry, const char* tag) {
    using namespace Supersonic;
    const entt::entity e = registry.create();
    registry.emplace<TagComponent>(e, tag);
    registry.emplace<TransformComponent>(e);
    registry.emplace<MeshComponent>(e).primitiveType = "Quad";
    auto& material = registry.emplace<MaterialComponent>(e);
    // Unlit: the engine's PBR lights never reach a sprite. Its 2D record
    // (sprite2D) is what lights it.
    material.unlit = true;
    material.transparent = true;
    auto& renderable = registry.emplace<RenderableComponent>(e);
    renderable.castsShadow = false;
    renderable.isVisible = false;
    return e;
}

SpriteRenderer::Slot& SpriteRenderer::slotFor(entt::registry& registry, int entityId) {
    for (std::uint32_t occurrence = 0;; ++occurrence) {
        const std::uint64_t key = PoolKey(entityId, occurrence);
        auto it = m_slots.find(key);
        if (it == m_slots.end()) {
            Slot slot;
            slot.entityId = entityId;
            while (!m_free.empty() && slot.quad == entt::null) {
                const entt::entity reused = m_free.back();
                m_free.pop_back();
                if (registry.valid(reused)) slot.quad = reused;
            }
            if (slot.quad == entt::null) slot.quad = makeQuad(registry, "Penumbra Sprite");
            it = m_slots.emplace(key, std::move(slot)).first;
            return it->second;
        }
        if (it->second.used) continue;
        // Something cleared the registry under the pool (a scene load): start
        // the quad again rather than write into an entity that is gone.
        if (!registry.valid(it->second.quad)) {
            it->second = Slot{};
            it->second.entityId = entityId;
            it->second.quad = makeQuad(registry, "Penumbra Sprite");
        }
        return it->second;
    }
}

void SpriteRenderer::hide(entt::registry& registry, entt::entity quad) {
    if (quad == entt::null || !registry.valid(quad)) return;
    Assign(registry.get<Supersonic::RenderableComponent>(quad).isVisible, false);
    // A hidden quad still takes a UV slot if it names a transform
    // (MaterialSystem::GatherUvTransforms walks every material), and the frame
    // has 4095 of them.
    auto& material = registry.get<MaterialComponent>(quad);
    Assign(material.uvScale, glm::vec2(1.0f));
    Assign(material.uvOffset, glm::vec2(0.0f));
}

// ---- drawing ------------------------------------------------------------------------

void SpriteRenderer::Draw(entt::registry& registry, const Eth::RenderSnapshot& snapshot, const View& view,
                          const DrawOrder& order) {
    if (m_textures == nullptr) return;
    m_visible = 0;
    for (auto& [key, slot] : m_slots) slot.used = false;
    const Language language =
        m_localization != nullptr ? m_localization->CurrentLanguage() : Language::Portuguese;

    for (std::size_t i = 0; i < snapshot.sprites.size(); ++i) {
        const Eth::SpriteDraw& sprite = snapshot.sprites[i];
        // A trigger label, a spawn marker: nothing to draw.
        if (sprite.sprite.empty() || !(sprite.size.x > 0.0f && sprite.size.y > 0.0f)) continue;

        Slot& slot = slotFor(registry, sprite.entityId);
        const TextureVariant variant = VariantFor(sprite.blendMode);
        if (slot.albedoKey.empty() || slot.sprite != sprite.sprite || slot.variant != variant ||
            slot.language != language) {
            slot.sprite = sprite.sprite;
            slot.variant = variant;
            slot.language = language;
            slot.albedoPath = imagePath("entities/" + sprite.sprite);
            slot.albedoKey = m_textures->Key(slot.albedoPath, variant);
        }
        // An image that would not read is drawn as nothing (TextureCache logged
        // it once); the quad stays in the pool, unused.
        if (slot.albedoKey.empty()) continue;

        slot.used = true;
        slot.idleDraws = 0;
        drawSprite(registry, snapshot, i, order, slot, slot.albedoKey);
        ++m_visible;
    }

    // What nobody drew this frame: hidden now, recycled after a while.
    for (auto it = m_slots.begin(); it != m_slots.end();) {
        Slot& slot = it->second;
        if (slot.used) {
            ++it;
            continue;
        }
        hide(registry, slot.quad);
        if (++slot.idleDraws > kIdleDrawsBeforeRecycle) {
            if (slot.quad != entt::null && registry.valid(slot.quad)) m_free.push_back(slot.quad);
            it = m_slots.erase(it);
        } else {
            ++it;
        }
    }

    drawBackground(registry, snapshot, view);
}

void SpriteRenderer::drawSprite(entt::registry& registry, const Eth::RenderSnapshot& snapshot, std::size_t index,
                                const DrawOrder& order, Slot& slot, const std::string& albedoKey) {
    using namespace Supersonic;
    const Eth::SpriteDraw& sprite = snapshot.sprites[index];

    // A stale order (not this snapshot's) still draws, in snapshot order.
    const int rank = index < order.spriteRank.size() ? order.spriteRank[index] : static_cast<int>(index);
    glm::vec3 position(0.0f);
    glm::vec3 scale(1.0f);
    float rotationZ = 0.0f;
    Placement(sprite, snapshot.zAxisDirection, RankZ(rank), position, scale, rotationZ);

    auto& transform = registry.get<TransformComponent>(slot.quad);
    Assign(transform.position, position);
    Assign(transform.scale, scale);
    Assign(transform.rotation, glm::vec3(0.0f, 0.0f, rotationZ));

    const SpriteLighting lighting = ComputeSpriteLighting(sprite, snapshot, *m_textures, m_localization);
    const Look look = LookFor(sprite, lighting);

    // The 2D record exactly as MPR's tint() writes it: always enabled, so the
    // ambient multiplies and the base clamps as one fixed-point draw did; the
    // light mask and the normal map only when lights reach it (a sprite that
    // takes none keeps the flat map, and so the material set its image's other
    // copies share).
    MaterialComponent::Sprite2DLight record;
    record.enabled = true;
    record.ambient = lighting.ambient;
    record.height = lighting.height;
    record.lightMask = lighting.lit ? lighting.lightMask : std::uint8_t{0};
    record.normalYDown = lighting.normalYDown;
    const std::string& normal = lighting.lit ? lighting.normalKey : std::string();

    glm::vec2 bitmap = sprite.bitmapSize;
    if (!(bitmap.x > 0.0f && bitmap.y > 0.0f)) bitmap = glm::vec2(m_textures->Size(slot.albedoPath));
    glm::vec2 uvScale(1.0f);
    glm::vec2 uvOffset(0.0f);
    FrameUv(sprite, bitmap, uvScale, uvOffset);

    auto& material = registry.get<MaterialComponent>(slot.quad);
    Assign(material.unlit, true);
    Assign(material.transparent, look.transparent);
    Assign(material.blend, look.blend);
    Assign(material.alphaCutoff, look.alphaCutoff);
    Assign(material.albedoColor, look.colour);
    Assign(material.albedoTexturePath, albedoKey);
    Assign(material.normalTexturePath, normal);
    Assign(material.sprite2D, record);
    Assign(material.uvScale, uvScale);
    Assign(material.uvOffset, uvOffset);

    Assign(registry.get<RenderableComponent>(slot.quad).isVisible, true);
}

void SpriteRenderer::drawBackground(entt::registry& registry, const Eth::RenderSnapshot& snapshot, const View& view) {
    using namespace Supersonic;
    if (snapshot.backgroundImage.empty()) {
        if (m_background != entt::null && registry.valid(m_background)) {
            Assign(registry.get<RenderableComponent>(m_background).isVisible, false);
        }
        return;
    }
    if (m_background == entt::null || !registry.valid(m_background)) {
        m_background = makeQuad(registry, "Penumbra Background");
        m_backgroundSource.clear();
    }

    // Loaded with the magenta key (ETHEngine.cpp:1439, AddFile without the
    // black cut-out); SetBackgroundAlphaAdd draws it One, One, which the
    // Additive image reproduces (its black key changes nothing on the one
    // additive background shipped, planets.png, which holds no magenta).
    const TextureVariant variant =
        snapshot.backgroundAdditive ? TextureVariant::Additive : TextureVariant::Sprite;
    if (m_backgroundSource != snapshot.backgroundImage || m_backgroundAdditive != snapshot.backgroundAdditive) {
        m_backgroundSource = snapshot.backgroundImage;
        m_backgroundAdditive = snapshot.backgroundAdditive;
        m_backgroundKey = m_textures->Key(snapshot.backgroundImage, variant);
    }
    if (m_backgroundKey.empty()) {
        Assign(registry.get<RenderableComponent>(m_background).isVisible, false);
        return;
    }

    // Screen pixels, min and max sorted per component (PositionBackgroundImage);
    // an image never positioned covers (0,0)..its size (SetBackgroundImage).
    glm::vec2 lo = glm::min(snapshot.backgroundMin, snapshot.backgroundMax);
    glm::vec2 hi = glm::max(snapshot.backgroundMin, snapshot.backgroundMax);
    if (!(hi.x > lo.x && hi.y > lo.y)) {
        lo = glm::vec2(0.0f);
        hi = glm::vec2(m_textures->Size(snapshot.backgroundImage));
    }
    if (!(hi.x > lo.x && hi.y > lo.y)) {
        Assign(registry.get<RenderableComponent>(m_background).isVisible, false);
        return;
    }

    // Drawn with the camera at (0, 0): fixed to the screen, so in the world it
    // rides with the camera.
    const glm::vec2 centre = view.camera + (lo + hi) * 0.5f;
    auto& transform = registry.get<TransformComponent>(m_background);
    Assign(transform.position, ToWorld(centre, RankZ(kBackgroundRank)));
    Assign(transform.scale, glm::vec3(hi.x - lo.x, hi.y - lo.y, 1.0f));
    Assign(transform.rotation, glm::vec3(0.0f));

    // GS_WHITE on every corner and no ambient: the plain unlit path.
    auto& material = registry.get<MaterialComponent>(m_background);
    Assign(material.unlit, true);
    Assign(material.transparent, true);
    Assign(material.blend, snapshot.backgroundAdditive ? MaterialComponent::BlendMode::Additive
                                                       : MaterialComponent::BlendMode::Alpha);
    Assign(material.alphaCutoff, kAlphaTestCutoff);
    Assign(material.albedoColor, glm::vec4(1.0f));
    Assign(material.albedoTexturePath, m_backgroundKey);
    Assign(material.normalTexturePath, std::string());
    Assign(material.sprite2D, MaterialComponent::Sprite2DLight{});
    Assign(registry.get<RenderableComponent>(m_background).isVisible, true);
}

} // namespace Penumbra::Render
