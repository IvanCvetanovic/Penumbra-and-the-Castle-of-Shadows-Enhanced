// Particles as engine quads (render/ParticleRenderer).
//
// The per-particle mapping is checked against the original's own arithmetic
// rather than against a restatement of the renderer: the rotation against
// GS2D's transformSprite (the oracle below), the cells against GS2D's
// SetupSpriteRects integer strides, the colours against 0.7.12's blend states
// (docs/spec/30 §3.3, docs/spec/21 §1.5). Then the pool, on a bare registry
// with the original's particle bitmaps: grown once, reused, hidden, never
// recreated, and the same picture for the same snapshot.
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include "TestHarness.hpp"
#include "core/Components.hpp"
#include "render/ParticleRenderer.hpp"

using namespace Penumbra;
using namespace Penumbra::Render;
using Supersonic::MaterialComponent;

namespace {

Eth::ParticleDraw MakeParticle(const std::string& bitmap, Eth::ALPHA_MODE mode, glm::vec2 position, float size,
                               float angle, glm::vec4 color) {
    Eth::ParticleDraw particle;
    particle.bitmap = bitmap;
    particle.alphaMode = mode;
    particle.position = position;
    particle.size = size;
    particle.angle = angle;
    particle.color = color;
    return particle;
}

// GS2D's transformSprite, as the 2010 GameSpace.dll ran it: an offset from the
// sprite's centre in y-DOWN pixels, times RotateZ's rows (cos, sin) /
// (-sin, cos) uploaded row-major and applied as mul(rotationMatrix, pos)
// (G:gs2dmath.h:699-708, G:gs2dcgshadercode.h, G:Video/Direct3D9/gs2dD3D9CgShader.cpp:289).
glm::vec2 Gs2dRotate(glm::vec2 offset, float degrees) {
    const float t = glm::radians(degrees);
    return {offset.x * std::cos(t) + offset.y * std::sin(t), -offset.x * std::sin(t) + offset.y * std::cos(t)};
}

// Where the engine draws the quad's local point (the quad is one unit, +y up),
// back in Ethanon pixels.
glm::vec2 EngineCorner(const ParticleQuad& quad, glm::vec2 local) {
    Supersonic::TransformComponent transform;
    transform.position = quad.position;
    transform.rotation = glm::vec3(0.0f, 0.0f, quad.rotationZ);
    transform.scale = quad.scale;
    const glm::vec4 world = transform.getModelMatrix() * glm::vec4(local, 0.0f, 1.0f);
    return ToEthanon(glm::vec3(world));
}

void CheckRotation(float angle) {
    const glm::vec2 centre(300.0f, 200.0f);
    const float size = 40.0f;
    const ParticleQuad quad = ParticleRenderer::ComputeQuad(
        MakeParticle("particle.png", Eth::AM_PIXEL, centre, size, angle, glm::vec4(1.0f)), 0, glm::ivec2(32));
    // The four corners. An engine corner (lx, ly) carries the same texel as
    // the Ethanon offset (lx, -ly) * size: the quad's uv (0,0) is at its top
    // left, and so was GS2D's.
    const glm::vec2 corners[] = {{-0.5f, 0.5f}, {0.5f, 0.5f}, {0.5f, -0.5f}, {-0.5f, -0.5f}};
    for (const glm::vec2 local : corners) {
        const glm::vec2 expected = centre + Gs2dRotate(glm::vec2(local.x, -local.y) * size, angle);
        const glm::vec2 got = EngineCorner(quad, local);
        CHECK_MSG(std::fabs(got.x - expected.x) < 1e-3f && std::fabs(got.y - expected.y) < 1e-3f,
                  "angle " + std::to_string(angle) + ": corner (" + std::to_string(local.x) + "," +
                      std::to_string(local.y) + ") at (" + std::to_string(got.x) + "," + std::to_string(got.y) +
                      "), GS2D put it at (" + std::to_string(expected.x) + "," + std::to_string(expected.y) + ")");
    }
}

void CheckUv(const ParticleQuad& quad, glm::vec2 scale, glm::vec2 offset) {
    CHECK_NEAR(quad.uvScale.x, scale.x);
    CHECK_NEAR(quad.uvScale.y, scale.y);
    CHECK_NEAR(quad.uvOffset.x, offset.x);
    CHECK_NEAR(quad.uvOffset.y, offset.y);
}

ParticleQuad Cell(glm::ivec2 bitmap, int cutX, int cutY, Eth::uint frame) {
    Eth::ParticleDraw particle =
        MakeParticle("explosion.png", Eth::AM_ADD, glm::vec2(0.0f), 64.0f, 0.0f, glm::vec4(1.0f));
    particle.spriteCutX = cutX;
    particle.spriteCutY = cutY;
    particle.frame = frame;
    return ParticleRenderer::ComputeQuad(particle, 0, bitmap);
}

bool Same(const glm::vec4& a, const glm::vec4& b) { return glm::all(glm::lessThan(glm::abs(a - b), glm::vec4(1e-5f))); }

// The drawn quads of a registry, by their rank's z.
struct Drawn {
    entt::entity entity = entt::null;
    glm::vec3 position{0.0f};
    float rotation = 0.0f;
    glm::vec4 color{0.0f};
    glm::vec2 uvScale{0.0f};
    glm::vec2 uvOffset{0.0f};
    std::string texture;
    MaterialComponent::BlendMode blend = MaterialComponent::BlendMode::Alpha;
};

std::vector<Drawn> Visible(entt::registry& registry) {
    std::vector<Drawn> out;
    auto view = registry.view<Supersonic::TransformComponent, MaterialComponent, Supersonic::RenderableComponent>();
    for (const entt::entity e : view) {
        if (!view.get<Supersonic::RenderableComponent>(e).isVisible) continue;
        const auto& transform = view.get<Supersonic::TransformComponent>(e);
        const auto& material = view.get<MaterialComponent>(e);
        out.push_back(Drawn{e, transform.position, transform.rotation.z, material.albedoColor, material.uvScale,
                            material.uvOffset, material.albedoTexturePath, material.blend});
    }
    return out;
}

const Drawn* AtRank(const std::vector<Drawn>& drawn, int rank) {
    for (const Drawn& d : drawn) {
        if (std::fabs(d.position.z - RankZ(rank)) < 1e-4f) return &d;
    }
    return nullptr;
}

std::size_t Quads(entt::registry& registry) { return registry.view<Supersonic::TagComponent>().size(); }

void CheckMapping() {
    // ---- rotation: the original's sign, not its mirror ----
    for (const float angle : {0.0f, 30.0f, 90.0f, -45.0f, 200.0f}) CheckRotation(angle);
    {
        // Named once, so the sign is visible without the oracle: at +90 the
        // image's top-left corner lands at the bottom-left - counter-clockwise
        // on the screen.
        const ParticleQuad quad = ParticleRenderer::ComputeQuad(
            MakeParticle("particle.png", Eth::AM_PIXEL, glm::vec2(300.0f, 200.0f), 40.0f, 90.0f, glm::vec4(1.0f)), 0,
            glm::ivec2(32));
        const glm::vec2 topLeft = EngineCorner(quad, glm::vec2(-0.5f, 0.5f));
        CHECK_NEAR(topLeft.x, 280.0f);
        CHECK_NEAR(topLeft.y, 220.0f);
        CHECK(quad.rotationZ > 0.0f);
    }

    // ---- place, size and order ----
    {
        const ParticleQuad quad = ParticleRenderer::ComputeQuad(
            MakeParticle("fog.dds", Eth::AM_PIXEL, glm::vec2(100.0f, 200.0f), 24.5f, 0.0f, glm::vec4(1.0f)), 7,
            glm::ivec2(256));
        CHECK_NEAR(quad.position.x, 100.0f);
        CHECK_NEAR(quad.position.y, -200.0f);
        CHECK_NEAR(quad.position.z, RankZ(7));
        CHECK_NEAR(quad.scale.x, 24.5f);
        CHECK_NEAR(quad.scale.y, 24.5f);
        CHECK_NEAR(quad.scale.z, 1.0f);
        CHECK(quad.visible);
    }

    // ---- cells of a 4x3 cut (explosion.png, 256x192 -> 64x64 frames) ----
    const float third = 1.0f / 3.0f;
    CheckUv(Cell({256, 192}, 4, 3, 0), {0.25f, third}, {0.0f, 0.0f});
    CheckUv(Cell({256, 192}, 4, 3, 3), {0.25f, third}, {0.75f, 0.0f});
    CheckUv(Cell({256, 192}, 4, 3, 6), {0.25f, third}, {0.5f, third});         // column 2, row 1: row-major
    CheckUv(Cell({256, 192}, 4, 3, 11), {0.25f, third}, {0.75f, 2.0f * third});
    CheckUv(Cell({256, 192}, 4, 3, 40), {0.25f, third}, {0.75f, 2.0f * third}); // past the sheet: the last cell
    // A size the cut does not divide: GS2D's integer strides (62 x 63), not quarters.
    CheckUv(Cell({250, 190}, 4, 3, 7), {62.0f / 250.0f, 63.0f / 190.0f}, {186.0f / 250.0f, 63.0f / 190.0f});
    // Size unknown: even cells.
    CheckUv(Cell({0, 0}, 4, 3, 5), {0.25f, third}, {0.25f, third});
    // One column, four rows.
    CheckUv(Cell({64, 256}, 1, 4, 2), {1.0f, 0.25f}, {0.0f, 0.5f});
    // 1x1: the whole bitmap whatever the frame, so no UV slot is taken.
    CheckUv(Cell({211, 211}, 1, 1, 3), {1.0f, 1.0f}, {0.0f, 0.0f});

    // ---- AM_ADD: One,One as Additive over an alpha-1 texture ----
    {
        const Eth::ParticleDraw spark = MakeParticle("flash.bmp", Eth::AM_ADD, glm::vec2(0.0f), 44.0f, 0.0f,
                                                     glm::vec4(0.6f, 0.7f, 1.0f, 0.5f));
        const ParticleQuad faithful = ParticleRenderer::ComputeQuad(spark, 0, glm::ivec2(265, 253));
        CHECK(faithful.blend == MaterialComponent::BlendMode::Additive);
        CHECK(faithful.variant == TextureVariant::Additive);
        CHECK(faithful.visible);
        // The vertex alpha did not weight One,One: light_spell pops, it does not fade.
        CHECK(Same(faithful.color, glm::vec4(0.6f, 0.7f, 1.0f, 1.0f)));
        // The enhancement: premultiplied into rgb, since SrcAlpha,One is then
        // handed alpha 1 and the texture's alpha is forced to 1.
        const ParticleQuad faded = ParticleRenderer::ComputeQuad(spark, 0, glm::ivec2(265, 253), true);
        CHECK(Same(faded.color, glm::vec4(0.3f, 0.35f, 0.5f, 1.0f)));
        CHECK(faded.blend == MaterialComponent::BlendMode::Additive);

        // The alpha test (ALPHAREF 1, GREATER, on the truncated byte).
        Eth::ParticleDraw dim = spark;
        dim.color.a = 1.0f / 255.0f;
        CHECK(!ParticleRenderer::ComputeQuad(dim, 0, glm::ivec2(265, 253)).visible);
        dim.color.a = 0.0f;
        CHECK(!ParticleRenderer::ComputeQuad(dim, 0, glm::ivec2(265, 253)).visible);
        dim.color.a = 3.0f / 255.0f;
        CHECK(ParticleRenderer::ComputeQuad(dim, 0, glm::ivec2(265, 253)).visible);
        // A black Color1 (big_explosion) is how an ADD particle really faded.
        dim.color = glm::vec4(0.1f, 0.1f, 0.25f, 0.25f);
        CHECK(Same(ParticleRenderer::ComputeQuad(dim, 0, glm::ivec2(256, 192)).color,
                   glm::vec4(0.1f, 0.1f, 0.25f, 1.0f)));
    }

    // ---- AM_PIXEL: straight alpha, the colour as it came ----
    {
        const ParticleQuad fog = ParticleRenderer::ComputeQuad(
            MakeParticle("fog.dds", Eth::AM_PIXEL, glm::vec2(0.0f), 300.0f, 10.0f, glm::vec4(0.4f, 0.4f, 0.4f, 0.6f)), 0,
            glm::ivec2(256));
        CHECK(fog.blend == MaterialComponent::BlendMode::Alpha);
        CHECK(fog.variant == TextureVariant::Sprite);
        CHECK(Same(fog.color, glm::vec4(0.4f, 0.4f, 0.4f, 0.6f)));
        CHECK_NEAR(fog.alphaCutoff, 0.0f);
        const ParticleQuad hot = ParticleRenderer::ComputeQuad(
            MakeParticle("blood.png", Eth::AM_PIXEL, glm::vec2(0.0f), 30.0f, 0.0f, glm::vec4(1.5f, -0.2f, 0.5f, 2.0f)), 0,
            glm::ivec2(41));
        CHECK(Same(hot.color, glm::vec4(1.0f, 0.0f, 0.5f, 1.0f)));
    }

    // ---- AM_MODULATE: black over the Modulate variant ----
    {
        const Eth::ParticleDraw aura =
            MakeParticle("black_opaque.png", Eth::AM_MODULATE, glm::vec2(0.0f), 38.0f, 0.0f, glm::vec4(1.0f));
        const ParticleQuad quad = ParticleRenderer::ComputeQuad(aura, 0, glm::ivec2(48));
        CHECK(quad.blend == MaterialComponent::BlendMode::Alpha);
        CHECK(quad.variant == TextureVariant::Modulate);
        CHECK(Same(quad.color, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)));
        CHECK(quad.visible);
        Eth::ParticleDraw gone = aura;
        gone.color.a = 0.0f;
        CHECK(!ParticleRenderer::ComputeQuad(gone, 0, glm::ivec2(48)).visible);
    }

    // ---- the modes no shipped system uses, and the degenerate ones ----
    {
        Eth::ParticleDraw none =
            MakeParticle("particle.png", Eth::AM_NONE, glm::vec2(0.0f), 10.0f, 0.0f, glm::vec4(0.5f, 0.5f, 0.5f, 0.0f));
        const ParticleQuad solid = ParticleRenderer::ComputeQuad(none, 0, glm::ivec2(32));
        CHECK(solid.visible);   // NONE had no alpha test
        CHECK_NEAR(solid.color.a, 1.0f);
        Eth::ParticleDraw test = none;
        test.alphaMode = Eth::AM_ALPHA_TEST;
        test.color.a = 1.0f;
        CHECK(ParticleRenderer::ComputeQuad(test, 0, glm::ivec2(32)).alphaCutoff > 0.0f);
        Eth::ParticleDraw odd = none;
        odd.alphaMode = static_cast<Eth::ALPHA_MODE>(9);
        odd.color.a = 1.0f;
        CHECK(ParticleRenderer::ComputeQuad(odd, 0, glm::ivec2(32)).variant == TextureVariant::Sprite);
        Eth::ParticleDraw empty = none;
        empty.alphaMode = Eth::AM_PIXEL;
        empty.color.a = 1.0f;
        empty.size = 0.0f;
        CHECK(!ParticleRenderer::ComputeQuad(empty, 0, glm::ivec2(32)).visible);
    }

    // ---- which file and which key ----
    CHECK(ParticleRenderer::VariantFor(Eth::AM_PIXEL) == TextureVariant::Sprite);
    CHECK(ParticleRenderer::VariantFor(Eth::AM_ADD) == TextureVariant::Additive);
    CHECK(ParticleRenderer::VariantFor(Eth::AM_MODULATE) == TextureVariant::Modulate);
    CHECK(ParticleRenderer::BitmapPath("explosion.png") == "particles/explosion.png");
    CHECK(ParticleRenderer::BitmapPath("particles\\fog.dds") == "particles/fog.dds");
    CHECK(ParticleRenderer::BitmapPath("a/b/flash.bmp") == "particles/flash.bmp");
}

void CheckPool() {
    entt::registry registry;
    TextureCache textures(PENUMBRA_ORIGINAL_DIR);
    textures.Attach(registry);   // a bare registry: keys, no uploads
    ParticleRenderer renderer;
    renderer.Attach(registry, textures);
    const View view;

    const std::string explosionKey = textures.Key("particles/explosion.png", TextureVariant::Additive);
    const std::string particleKey = textures.Key("particles/particle.png", TextureVariant::Sprite);
    CHECK(!explosionKey.empty());
    CHECK(!particleKey.empty());

    auto burst = [](Eth::uint frame, glm::vec2 at) {
        Eth::ParticleDraw p = MakeParticle("explosion.png", Eth::AM_ADD, at, 64.0f, 45.0f,
                                           glm::vec4(1.0f, 0.4f, 0.2f, 1.0f));
        p.spriteCutX = 4;
        p.spriteCutY = 3;
        p.frame = frame;
        return p;
    };
    Eth::RenderSnapshot snapshot;
    snapshot.particles = {burst(5, {10.0f, 20.0f}), burst(6, {30.0f, 40.0f}),
                          MakeParticle("particle.png", Eth::AM_PIXEL, {50.0f, 60.0f}, 16.0f, 0.0f,
                                       glm::vec4(0.0f, 0.0f, 0.0f, 1.0f))};
    DrawOrder order;
    order.particleRank = {3, 5, 9};

    renderer.Draw(registry, snapshot, view, order);
    CHECK_EQ(renderer.QuadCount(), std::size_t{3});
    CHECK_EQ(renderer.ShownCount(), std::size_t{3});
    const std::vector<Drawn> first = Visible(registry);
    CHECK_EQ(first.size(), std::size_t{3});
    const Drawn* a = AtRank(first, 3);
    const Drawn* c = AtRank(first, 9);
    CHECK(a != nullptr && c != nullptr);
    if (a != nullptr && c != nullptr) {
        CHECK(a->texture == explosionKey);
        CHECK(a->blend == MaterialComponent::BlendMode::Additive);
        CHECK_NEAR(a->position.x, 10.0f);
        CHECK_NEAR(a->position.y, -20.0f);
        CHECK_NEAR(a->rotation, glm::radians(45.0f));
        CHECK_NEAR(a->uvOffset.x, 0.25f);   // frame 5: column 1, row 1
        CHECK_NEAR(a->uvOffset.y, 1.0f / 3.0f);
        CHECK(c->texture == particleKey);
        CHECK(c->blend == MaterialComponent::BlendMode::Alpha);
        CHECK_NEAR(c->uvScale.x, 1.0f);
        const MaterialComponent& material = registry.get<MaterialComponent>(a->entity);
        CHECK(material.unlit);
        CHECK(material.transparent);
        CHECK(!material.sprite2D.enabled);   // no Light2D reaches a particle
        CHECK(!registry.get<Supersonic::RenderableComponent>(a->entity).castsShadow);
    }

    // The same snapshot again: the same quads, untouched.
    renderer.Draw(registry, snapshot, view, order);
    const std::vector<Drawn> second = Visible(registry);
    CHECK_EQ(second.size(), first.size());
    CHECK_EQ(Quads(registry), std::size_t{3});
    bool same = second.size() == first.size();
    for (std::size_t i = 0; same && i < first.size(); ++i) {
        same = first[i].entity == second[i].entity && first[i].position == second[i].position &&
               first[i].color == second[i].color && first[i].uvOffset == second[i].uvOffset;
    }
    CHECK_MSG(same, "a snapshot drawn twice must draw the same quads");

    // Fewer particles: the surplus hidden, kept, and off its cell.
    std::map<entt::entity, bool> before;
    for (const Drawn& d : first) before[d.entity] = true;
    Eth::RenderSnapshot fewer;
    fewer.particles = {snapshot.particles[2]};
    DrawOrder fewerOrder;
    fewerOrder.particleRank = {1};
    renderer.Draw(registry, fewer, view, fewerOrder);
    CHECK_EQ(renderer.ShownCount(), std::size_t{1});
    CHECK_EQ(renderer.QuadCount(), std::size_t{3});
    CHECK_EQ(Quads(registry), std::size_t{3});
    CHECK_EQ(Visible(registry).size(), std::size_t{1});
    if (a != nullptr) {
        CHECK(registry.valid(a->entity));
        const MaterialComponent& hidden = registry.get<MaterialComponent>(a->entity);
        CHECK(!hidden.HasUvTransform());   // holds no UV slot while hidden
        CHECK(!registry.get<Supersonic::RenderableComponent>(a->entity).isVisible);
    }

    // More of one pool: it grows by the difference and no further.
    Eth::RenderSnapshot more;
    more.particles = {burst(0, {0.0f, 0.0f}), burst(1, {1.0f, 0.0f}), burst(2, {2.0f, 0.0f}), burst(3, {3.0f, 0.0f})};
    DrawOrder moreOrder;
    moreOrder.particleRank = {0, 1, 2, 3};
    renderer.Draw(registry, more, view, moreOrder);
    CHECK_EQ(renderer.QuadCount(), std::size_t{5});
    CHECK_EQ(renderer.ShownCount(), std::size_t{4});
    std::size_t reused = 0;
    for (const Drawn& d : Visible(registry)) reused += before.count(d.entity);
    CHECK_EQ(reused, std::size_t{2});   // both explosion quads came back

    // A rank list shorter than the particles: the unranked are not guessed at.
    moreOrder.particleRank = {0, 1};
    renderer.Draw(registry, more, view, moreOrder);
    CHECK_EQ(renderer.ShownCount(), std::size_t{2});
    CHECK_EQ(renderer.QuadCount(), std::size_t{5});

    // An unreadable bitmap draws nothing and makes no quads.
    Eth::RenderSnapshot missing;
    missing.particles = {MakeParticle("no_such_particle.png", Eth::AM_PIXEL, {0.0f, 0.0f}, 8.0f, 0.0f, glm::vec4(1.0f))};
    DrawOrder missingOrder;
    missingOrder.particleRank = {0};
    renderer.Draw(registry, missing, view, missingOrder);
    CHECK_EQ(renderer.ShownCount(), std::size_t{0});
    CHECK_EQ(Visible(registry).size(), std::size_t{0});
    CHECK_EQ(renderer.QuadCount(), std::size_t{5});

    renderer.Detach(registry);
    CHECK_EQ(Quads(registry), std::size_t{0});
    CHECK_EQ(renderer.QuadCount(), std::size_t{0});
}

} // namespace

int main() {
    if (!std::filesystem::exists(PENUMBRA_ORIGINAL_DIR "/particles/explosion.png")) {
        std::printf("SKIP: the original is not at %s\n", PENUMBRA_ORIGINAL_DIR);
        return 77;
    }
    CheckMapping();
    CheckPool();
    return test::summary("test_pn_render_particles", 100);
}
