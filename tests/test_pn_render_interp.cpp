// render/Interpolation (enhancement E8): the world blended between two ticks.
//
// What each part pins, and why it could come back:
//   - The ends: alpha 0 is the previous tick exactly, alpha 1 (and anything
//     past it) the current one to the bit, NaN counts as 0. Only positions are
//     blended; the frame, colour and every other field are the current tick's.
//   - The midpoint of a sprite, a light, a particle (position, size, angle)
//     and the camera, and the pixel grid: two whole-pixel ends give a
//     whole-pixel blend, a fractional end does not.
//   - A sprite the camera follows keeps its place on screen at EVERY alpha -
//     the jitter a naive per-coordinate rounding brings back.
//   - Jumps: past kMaxStepPixels a thing is drawn where it is now; just under
//     it, blended.
//   - Identity: a new id, a particle's new life, another system slot, a
//     reordered particle list, an id drawn twice.
//   - Continuity: a gap in frameIndex, another sceneSerial, another scene
//     file, a change of round-up or z axis - the frame is the current
//     snapshot, the very object.
//   - The interpolator's per-frame flow: nothing before the first tick, the
//     copy refreshed once a tick and rewritten every frame, the switch.
//   - The renderers take the blend as it is: SpriteRenderer::Placement puts
//     the quad at the blended corner; ShadowRenderer places a strip at the
//     blended caster while its shape stays the current tick's.
//   - Settings: smoothMotion defaults on, round-trips, and an old file keeps
//     the default.
//   - The real game (needs extracted/app): the Machine fills the particles'
//     identity and counts scene loads; a walking wizard is blended between his
//     two ticks, never snapped, and never drifts against the camera.

#include "script/Script.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include "TestHarness.hpp"
#include "core/Components.hpp"
#include "eth/Machine.hpp"
#include "eth/Snapshot.hpp"
#include "render/DrawOrder.hpp"
#include "render/Interpolation.hpp"
#include "render/Settings.hpp"
#include "render/ShadowRenderer.hpp"
#include "render/SpriteRenderer.hpp"
#include "render/TextureCache.hpp"
#include "render/View.hpp"

namespace Eth = Penumbra::Eth;
namespace Render = Penumbra::Render;
namespace Script = Penumbra::Script;
using Render::BlendStats;
using Render::SnapshotInterpolator;
using Render::SnapshotPoses;

namespace {

std::string Str(const glm::vec2& v) { return "(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ")"; }
std::string Str(const glm::vec3& v) {
    return "(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
}

// A sprite placed as Machine::Render places one with round-up on, no z axis
// and no pivot: its corner is floor(position) - size / 2.
Eth::SpriteDraw Walker(int id, const glm::vec3& position) {
    Eth::SpriteDraw sprite;
    sprite.entityId = id;
    sprite.entityName = "walker.ent";
    sprite.sprite = "walker.png";
    sprite.size = glm::vec2(32.0f, 48.0f);
    sprite.position = position;
    sprite.origin = glm::vec2(std::floor(position.x), std::floor(position.y)) - sprite.size * 0.5f;
    return sprite;
}

Eth::LightDraw Lamp(int owner, const glm::vec3& position) {
    Eth::LightDraw light;
    light.ownerId = owner;
    light.position = position;
    light.range = 200.0f;
    return light;
}

Eth::ParticleDraw Spark(int owner, int system, int id, float lifeStartMs, const glm::vec2& position, float size,
                        float angle) {
    Eth::ParticleDraw particle;
    particle.ownerId = owner;
    particle.system = system;
    particle.particleId = id;
    particle.lifeStartMs = lifeStartMs;
    particle.bitmap = "spark.png";
    particle.position = position;
    particle.size = size;
    particle.angle = angle;
    return particle;
}

// An empty tick of level 1 (scene serial 3, rounded, no z axis).
Eth::RenderSnapshot Tick(Eth::uint frame) {
    Eth::RenderSnapshot snapshot;
    snapshot.frameIndex = frame;
    snapshot.timeMs = frame * 1000u / 60u;
    snapshot.sceneFile = "scenes/level1.esc";
    snapshot.sceneSerial = 3;
    snapshot.roundUp = true;
    snapshot.ambient = glm::vec3(0.2f, 0.1f, 0.2f);
    return snapshot;
}

SnapshotPoses PosesOf(const Eth::RenderSnapshot& snapshot) {
    SnapshotPoses poses;
    poses.Capture(snapshot);
    return poses;
}

// Tick 10 and tick 11 of one walker, his light and one of his sparks, the
// camera moving with him: every value to blend is 10 apart.
struct Pair {
    Eth::RenderSnapshot previous = Tick(10);
    Eth::RenderSnapshot current = Tick(11);
    Pair() {
        previous.camera = glm::vec2(100.0f, 50.0f);
        previous.sprites = {Walker(5, {300.0f, 400.0f, 0.0f})};
        previous.lights = {Lamp(5, {300.0f, 380.0f, 12.0f})};
        previous.particles = {Spark(5, 0, 3, 1000.0f, {310.0f, 390.0f}, 8.0f, 30.0f)};
        current.camera = glm::vec2(110.0f, 50.0f);
        current.sprites = {Walker(5, {310.0f, 400.0f, 0.0f})};
        current.sprites[0].frame = 7;
        current.sprites[0].color = glm::vec4(1.0f, 0.5f, 0.5f, 0.75f);
        current.lights = {Lamp(5, {310.0f, 380.0f, 12.0f})};
        current.lights[0].color = glm::vec3(0.5f, 0.25f, 1.0f);
        current.particles = {Spark(5, 0, 3, 1000.0f, {320.0f, 380.0f}, 10.0f, 40.0f)};
        current.particles[0].frame = 2;
    }
};

// ---- the helpers -------------------------------------------------------------------

void AlphaAndWholePixels() {
    CHECK_EQ(SnapshotInterpolator::SanitiseAlpha(0.25f), 0.25f);
    CHECK_EQ(SnapshotInterpolator::SanitiseAlpha(-1.0f), 0.0f);
    CHECK_EQ(SnapshotInterpolator::SanitiseAlpha(2.0f), 1.0f);
    CHECK_EQ(SnapshotInterpolator::SanitiseAlpha(std::numeric_limits<float>::quiet_NaN()), 0.0f);
    CHECK_EQ(SnapshotInterpolator::SanitiseAlpha(std::numeric_limits<float>::infinity()), 1.0f);

    // Halves up, on both sides of zero.
    CHECK_EQ(SnapshotInterpolator::WholePixel(1.5f), 2.0f);
    CHECK_EQ(SnapshotInterpolator::WholePixel(-1.5f), -1.0f);
    CHECK_EQ(SnapshotInterpolator::WholePixel(2.49f), 2.0f);
    CHECK_EQ(SnapshotInterpolator::WholePixel(-2.51f), -3.0f);

    // DrawnAnchor is Machine::Render's point: ToScreenPos, floored under round-up.
    Eth::SpriteDraw sprite = Walker(1, {100.7f, 50.2f, 20.0f});
    CHECK(SnapshotInterpolator::DrawnAnchor(sprite, glm::vec2(0.0f), true) == glm::vec2(100.0f, 50.0f));
    // The menus' z axis (0, -1): drawn z pixels higher.
    CHECK(SnapshotInterpolator::DrawnAnchor(sprite, glm::vec2(0.0f, -1.0f), true) == glm::vec2(100.0f, 30.0f));
    CHECK(SnapshotInterpolator::DrawnAnchor(sprite, glm::vec2(0.0f), false) == glm::vec2(100.7f, 50.2f));
    // And the Walker helper agrees with it, so the checks below mean what they say.
    CHECK(sprite.origin == SnapshotInterpolator::DrawnAnchor(sprite, glm::vec2(0.0f), true) - sprite.size * 0.5f);
}

// ---- the blend ------------------------------------------------------------------------

void MidpointAndEnds() {
    const Pair pair;
    const SnapshotPoses poses = PosesOf(pair.previous);
    CHECK(poses.valid);
    CHECK(SnapshotInterpolator::Continuous(poses, pair.current));

    // Halfway: everything halfway, on whole pixels where both ends were.
    Eth::RenderSnapshot half = pair.current;
    const BlendStats stats = SnapshotInterpolator::Blend(poses, pair.current, 0.5f, half);
    CHECK(stats.blended);
    CHECK_EQ(stats.sprites, std::size_t{1});
    CHECK_EQ(stats.lights, std::size_t{1});
    CHECK_EQ(stats.particles, std::size_t{1});
    CHECK_EQ(stats.snapped, std::size_t{0});
    CHECK(!stats.cameraSnapped);
    CHECK_MSG(half.camera == glm::vec2(105.0f, 50.0f), Str(half.camera));
    CHECK_MSG(half.sprites[0].position == glm::vec3(305.0f, 400.0f, 0.0f), Str(half.sprites[0].position));
    // Exactly where 0.7.12 would have drawn a walker standing at x 305.
    CHECK_MSG(half.sprites[0].origin == Walker(5, {305.0f, 400.0f, 0.0f}).origin, Str(half.sprites[0].origin));
    CHECK_MSG(half.lights[0].position == glm::vec3(305.0f, 380.0f, 12.0f), Str(half.lights[0].position));
    CHECK_MSG(half.particles[0].position == glm::vec2(315.0f, 385.0f), Str(half.particles[0].position));
    CHECK_EQ(half.particles[0].size, 9.0f);
    CHECK_EQ(half.particles[0].angle, 35.0f);
    // Everything that is not a position is the current tick's.
    CHECK_EQ(half.sprites[0].frame, 7u);
    CHECK(half.sprites[0].color == pair.current.sprites[0].color);
    CHECK(half.lights[0].color == pair.current.lights[0].color);
    CHECK_EQ(half.particles[0].frame, 2u);
    CHECK_EQ(half.frameIndex, 11u);
    CHECK_EQ(half.timeMs, pair.current.timeMs);

    // alpha 0: the previous tick, exactly (one tick behind, by design).
    const Eth::RenderSnapshot zero = SnapshotInterpolator::Blended(poses, pair.current, 0.0f);
    CHECK(zero.camera == pair.previous.camera);
    CHECK(zero.sprites[0].position == pair.previous.sprites[0].position);
    CHECK(zero.sprites[0].origin == pair.previous.sprites[0].origin);
    CHECK(zero.lights[0].position == pair.previous.lights[0].position);
    CHECK(zero.particles[0].position == pair.previous.particles[0].position);
    CHECK_EQ(zero.particles[0].size, pair.previous.particles[0].size);
    CHECK_EQ(zero.particles[0].angle, pair.previous.particles[0].angle);
    CHECK_EQ(zero.sprites[0].frame, 7u);   // still the current frame's cell
    // NaN and below zero read as 0.
    const Eth::RenderSnapshot nan =
        SnapshotInterpolator::Blended(poses, pair.current, std::numeric_limits<float>::quiet_NaN());
    CHECK(nan.sprites[0].origin == pair.previous.sprites[0].origin);
    CHECK(nan.camera == pair.previous.camera);

    // alpha 1, and past it: the current tick to the bit.
    for (const float alpha : {1.0f, 1.5f}) {
        const Eth::RenderSnapshot one = SnapshotInterpolator::Blended(poses, pair.current, alpha);
        CHECK(one.camera == pair.current.camera);
        CHECK(one.sprites[0].position == pair.current.sprites[0].position);
        CHECK(one.sprites[0].origin == pair.current.sprites[0].origin);
        CHECK(one.lights[0].position == pair.current.lights[0].position);
        CHECK(one.particles[0].position == pair.current.particles[0].position);
        CHECK_EQ(one.particles[0].size, pair.current.particles[0].size);
    }

    // A blend written into a reused copy rewrites every position: from alpha
    // 0.5 to alpha 1 leaves nothing of the half behind.
    SnapshotInterpolator::Blend(poses, pair.current, 1.0f, half);
    CHECK(half.camera == pair.current.camera);
    CHECK(half.sprites[0].origin == pair.current.sprites[0].origin);
    CHECK(half.lights[0].position == pair.current.lights[0].position);
    CHECK(half.particles[0].position == pair.current.particles[0].position);

    // A copy of another size is replaced by the current snapshot first.
    Eth::RenderSnapshot empty;
    SnapshotInterpolator::Blend(poses, pair.current, 0.5f, empty);
    CHECK_EQ(empty.sprites.size(), std::size_t{1});
    CHECK(empty.sprites[0].origin == half.sprites[0].origin - glm::vec2(5.0f, 0.0f));
}

void PixelGrid() {
    // A walker from x 300.6 (drawn from 300) to 303.1 (drawn from 303).
    Eth::RenderSnapshot previous = Tick(20);
    Eth::RenderSnapshot current = Tick(21);
    previous.sprites = {Walker(1, {300.6f, 200.0f, 0.0f})};
    current.sprites = {Walker(1, {303.1f, 200.0f, 0.0f})};
    const SnapshotPoses poses = PosesOf(previous);
    const float left = current.sprites[0].origin.x;   // 303 - 16

    // A quarter of the way: 300.75, drawn from the whole pixel 301.
    Eth::RenderSnapshot quarter = SnapshotInterpolator::Blended(poses, current, 0.25f);
    CHECK_EQ(quarter.sprites[0].origin.x, left - 2.0f);
    CHECK(test::nearly(quarter.sprites[0].position.x, 301.225f, 1e-4f));   // the position itself is not rounded
    // Every alpha lands the corner on a whole pixel, and it never runs backwards.
    float last = previous.sprites[0].origin.x;
    bool whole = true;
    bool monotonic = true;
    for (int k = 0; k <= 100; ++k) {
        const Eth::RenderSnapshot b = SnapshotInterpolator::Blended(poses, current, static_cast<float>(k) / 100.0f);
        const float x = b.sprites[0].origin.x;
        whole = whole && x == std::floor(x);
        monotonic = monotonic && x >= last;
        last = x;
    }
    CHECK(whole);
    CHECK(monotonic);
    CHECK_EQ(last, left);

    // The camera: whole ends, whole blend (halves up)...
    previous.camera = glm::vec2(0.0f, 0.0f);
    current.camera = glm::vec2(3.0f, -3.0f);
    const SnapshotPoses withCamera = PosesOf(previous);
    CHECK(SnapshotInterpolator::Blended(withCamera, current, 0.5f).camera == glm::vec2(2.0f, -1.0f));
    CHECK(SnapshotInterpolator::Blended(withCamera, current, 0.4f).camera == glm::vec2(1.0f, -1.0f));
    // ...a fractional end (a scene that never rounds it): blended as it is.
    previous.camera = glm::vec2(0.5f, 0.0f);
    const glm::vec2 fractional = SnapshotInterpolator::Blended(PosesOf(previous), current, 0.5f).camera;
    CHECK_MSG(test::nearly(fractional.x, 1.75f, 1e-5f) && test::nearly(fractional.y, -1.5f, 1e-5f), Str(fractional));

    // Without round-up the corner is not put on a whole pixel either.
    previous.roundUp = false;
    current.roundUp = false;
    previous.sprites[0].origin = glm::vec2(300.6f, 200.0f) - previous.sprites[0].size * 0.5f;
    current.sprites[0].origin = glm::vec2(303.1f, 200.0f) - current.sprites[0].size * 0.5f;
    const Eth::RenderSnapshot unrounded = SnapshotInterpolator::Blended(PosesOf(previous), current, 0.5f);
    CHECK_MSG(test::nearly(unrounded.sprites[0].origin.x, 301.85f - 16.0f, 1e-3f), Str(unrounded.sprites[0].origin));
}

// cameraManager.as's dead zone puts the camera floor(player - 614.4) - a
// constant whole number of pixels from the player's floored anchor - so a
// followed player stands still on screen. The blend must keep that at every
// alpha, not only at the ticks.
void FollowedSpriteHoldsItsPlaceOnScreen() {
    const auto frameOf = [](Eth::uint frame, float x) {
        Eth::RenderSnapshot snapshot = Tick(frame);
        snapshot.sprites = {Walker(1, {x, 300.0f, 0.0f})};
        snapshot.camera = glm::vec2(std::floor(x - 614.4f), 0.0f);
        return snapshot;
    };
    const auto screenX = [](const Eth::RenderSnapshot& s) { return s.sprites[0].origin.x - s.camera.x; };

    // 100.5 -> 102.9: the anchor and the camera both move 2.
    const Eth::RenderSnapshot previous = frameOf(30, 100.5f);
    const Eth::RenderSnapshot current = frameOf(31, 102.9f);
    CHECK_EQ(screenX(previous), screenX(current));
    const SnapshotPoses poses = PosesOf(previous);
    Eth::RenderSnapshot drawn = current;
    int drifted = 0;
    for (int k = 0; k <= 4000; ++k) {
        SnapshotInterpolator::Blend(poses, current, static_cast<float>(k) / 4000.0f, drawn);
        if (screenX(drawn) != screenX(current)) ++drifted;
    }
    // The alphas where a rounded coordinate would slip: a hair under a half.
    for (const float alpha : {0.25f, 0.2499999f, 0.2500001f, 0.49999f, 0.5000001f, 0.75f, 0.7499999f}) {
        SnapshotInterpolator::Blend(poses, current, alpha, drawn);
        if (screenX(drawn) != screenX(current)) ++drifted;
    }
    CHECK_EQ(drifted, 0);

    // When the offset itself changes between the ticks (the floors fall
    // differently: floor(p) - floor(p - 614.4) is 615 for a fraction under
    // 0.4, else 614), every frame lies between the two ticks' offsets.
    const Eth::RenderSnapshot a = frameOf(40, 100.9f);   // anchor 100, camera -514: 614
    const Eth::RenderSnapshot b = frameOf(41, 103.2f);   // anchor 103, camera -512: 615
    CHECK_EQ(screenX(b) - screenX(a), 1.0f);
    const float lo = std::min(screenX(a), screenX(b));
    const float hi = std::max(screenX(a), screenX(b));
    const SnapshotPoses abPoses = PosesOf(a);
    int outside = 0;
    for (int k = 0; k <= 1000; ++k) {
        SnapshotInterpolator::Blend(abPoses, b, static_cast<float>(k) / 1000.0f, drawn);
        if (screenX(drawn) < lo || screenX(drawn) > hi) ++outside;
    }
    CHECK_EQ(outside, 0);
}

void Jumps() {
    Pair pair;
    // The walker respawns 500 px away, the camera is sent 300 px, the light
    // goes with the walker, the spark moves 65: all drawn where they are now.
    pair.current.sprites[0] = Walker(5, {800.0f, 400.0f, 0.0f});
    pair.current.camera = glm::vec2(400.0f, 50.0f);
    pair.current.lights[0].position = glm::vec3(800.0f, 380.0f, 12.0f);
    pair.current.particles[0].position = glm::vec2(310.0f + 65.0f, 390.0f);
    const SnapshotPoses poses = PosesOf(pair.previous);
    Eth::RenderSnapshot drawn = pair.current;
    const BlendStats stats = SnapshotInterpolator::Blend(poses, pair.current, 0.5f, drawn);
    CHECK_EQ(stats.snapped, std::size_t{3});
    CHECK(stats.cameraSnapped);
    CHECK_EQ(stats.sprites, std::size_t{0});
    CHECK(drawn.camera == pair.current.camera);
    CHECK(drawn.sprites[0].origin == pair.current.sprites[0].origin);
    CHECK(drawn.sprites[0].position == pair.current.sprites[0].position);
    CHECK(drawn.lights[0].position == pair.current.lights[0].position);
    CHECK(drawn.particles[0].position == pair.current.particles[0].position);
    // At alpha 0 too: a jump is never drawn from where it left.
    SnapshotInterpolator::Blend(poses, pair.current, 0.0f, drawn);
    CHECK(drawn.sprites[0].origin == pair.current.sprites[0].origin);
    CHECK(drawn.camera == pair.current.camera);

    // Just under the threshold is motion: blended.
    const float step = SnapshotInterpolator::kMaxStepPixels - 1.0f;
    pair.current.sprites[0] = Walker(5, {300.0f + step, 400.0f, 0.0f});
    pair.current.camera = glm::vec2(100.0f + step, 50.0f);
    pair.current.particles[0].position = glm::vec2(310.0f + step, 390.0f);
    const BlendStats under = SnapshotInterpolator::Blend(poses, pair.current, 0.5f, drawn);
    CHECK_EQ(under.snapped, std::size_t{1});   // only the light, still 500 away
    CHECK(!under.cameraSnapped);
    CHECK_EQ(under.sprites, std::size_t{1});
    CHECK_EQ(under.particles, std::size_t{1});
    CHECK_MSG(drawn.camera == glm::vec2(132.0f, 50.0f), Str(drawn.camera));   // 100 + 63/2 = 131.5, rounded up
    CHECK(drawn.particles[0].position == glm::vec2(341.5f, 390.0f));

    // A jump in z alone (a thing lifted far) snaps as well.
    pair.current.sprites[0] = Walker(5, {300.0f, 400.0f, 200.0f});
    SnapshotInterpolator::Blend(poses, pair.current, 0.5f, drawn);
    CHECK(drawn.sprites[0].position == pair.current.sprites[0].position);
}

void Identity() {
    Pair pair;
    // A newcomer of each kind, with no previous pose: drawn where it is.
    pair.current.sprites.push_back(Walker(9, {500.0f, 100.0f, 0.0f}));
    pair.current.lights.push_back(Lamp(9, {500.0f, 90.0f, 5.0f}));
    // The spark's slot starts a new life (respawned at its emitter): new.
    pair.current.particles[0].lifeStartMs = 1400.0f;
    // The same index in the owner's OTHER system: another particle.
    pair.current.particles.push_back(Spark(5, 1, 3, 1000.0f, {330.0f, 380.0f}, 8.0f, 0.0f));
    const SnapshotPoses poses = PosesOf(pair.previous);
    Eth::RenderSnapshot drawn = pair.current;
    const BlendStats stats = SnapshotInterpolator::Blend(poses, pair.current, 0.0f, drawn);
    CHECK_EQ(stats.sprites, std::size_t{1});
    CHECK_EQ(stats.lights, std::size_t{1});
    CHECK_EQ(stats.particles, std::size_t{0});
    CHECK_EQ(stats.snapped, std::size_t{0});
    CHECK(drawn.sprites[0].origin == pair.previous.sprites[0].origin);   // the walker: from where he was
    CHECK(drawn.sprites[1].origin == pair.current.sprites[1].origin);
    CHECK(drawn.sprites[1].position == pair.current.sprites[1].position);
    CHECK(drawn.lights[1].position == pair.current.lights[1].position);
    CHECK(drawn.particles[0].position == pair.current.particles[0].position);
    CHECK(drawn.particles[1].position == pair.current.particles[1].position);

    // The key tells lives, slots and systems apart and nothing else.
    const auto key = [](const Eth::ParticleDraw& p) { return SnapshotPoses::KeyOf(p); };
    const Eth::ParticleDraw a = Spark(5, 0, 3, 1000.0f, {0.0f, 0.0f}, 1.0f, 0.0f);
    CHECK(key(a) == key(Spark(5, 0, 3, 1000.0f, {50.0f, 9.0f}, 4.0f, 90.0f)));
    CHECK(!(key(a) == key(Spark(6, 0, 3, 1000.0f, {0.0f, 0.0f}, 1.0f, 0.0f))));
    CHECK(!(key(a) == key(Spark(5, 1, 3, 1000.0f, {0.0f, 0.0f}, 1.0f, 0.0f))));
    CHECK(!(key(a) == key(Spark(5, 0, 4, 1000.0f, {0.0f, 0.0f}, 1.0f, 0.0f))));
    CHECK(!(key(a) == key(Spark(5, 0, 3, 1016.0f, {0.0f, 0.0f}, 1.0f, 0.0f))));
    const SnapshotPoses::ParticleKeyHash hash;
    CHECK(hash(key(a)) == hash(key(Spark(5, 0, 3, 1000.0f, {50.0f, 9.0f}, 4.0f, 90.0f))));
    CHECK(hash(key(Spark(5, 0, 3, 0.0f, {}, 1.0f, 0.0f))) == hash(key(Spark(5, 0, 3, -0.0f, {}, 1.0f, 0.0f))));

    // CollectDraws bubble-sorts its particles in place, so the list comes back
    // in another order: matched by key, not by index.
    Eth::RenderSnapshot previous = Tick(50);
    Eth::RenderSnapshot current = Tick(51);
    previous.particles = {Spark(2, 0, 0, 500.0f, {10.0f, 10.0f}, 4.0f, 0.0f),
                          Spark(2, 0, 1, 500.0f, {40.0f, 10.0f}, 4.0f, 0.0f)};
    current.particles = {Spark(2, 0, 1, 500.0f, {40.0f, 20.0f}, 4.0f, 0.0f),
                         Spark(2, 0, 0, 500.0f, {10.0f, 30.0f}, 4.0f, 0.0f)};
    const Eth::RenderSnapshot swapped = SnapshotInterpolator::Blended(PosesOf(previous), current, 0.5f);
    CHECK_MSG(swapped.particles[0].position == glm::vec2(40.0f, 15.0f), Str(swapped.particles[0].position));
    CHECK_MSG(swapped.particles[1].position == glm::vec2(10.0f, 20.0f), Str(swapped.particles[1].position));

    // An id drawn twice in the previous tick: which of the two continues
    // cannot be told, so neither is blended.
    previous = Tick(60);
    current = Tick(61);
    previous.sprites = {Walker(3, {100.0f, 100.0f, 0.0f}), Walker(3, {120.0f, 100.0f, 0.0f})};
    previous.lights = {Lamp(3, {0.0f, 0.0f, 0.0f}), Lamp(3, {10.0f, 0.0f, 0.0f})};
    current.sprites = {Walker(3, {110.0f, 100.0f, 0.0f})};
    current.lights = {Lamp(3, {5.0f, 0.0f, 0.0f})};
    const SnapshotPoses twice = PosesOf(previous);
    CHECK(twice.sprites.at(3).duplicate);
    const Eth::RenderSnapshot dup = SnapshotInterpolator::Blended(twice, current, 0.0f);
    CHECK(dup.sprites[0].origin == current.sprites[0].origin);
    CHECK(dup.lights[0].position == current.lights[0].position);
}

void Continuity() {
    const Pair pair;
    const SnapshotPoses poses = PosesOf(pair.previous);
    CHECK(SnapshotInterpolator::Continuous(poses, pair.current));
    CHECK(!SnapshotInterpolator::Continuous(SnapshotPoses{}, pair.current));   // nothing captured

    const auto breaks = [&](auto change) {
        Eth::RenderSnapshot other = pair.current;
        change(other);
        return !SnapshotInterpolator::Continuous(poses, other);
    };
    CHECK(breaks([](Eth::RenderSnapshot& s) { s.frameIndex = 12; }));     // a tick went by unseen
    CHECK(breaks([](Eth::RenderSnapshot& s) { s.frameIndex = 10; }));     // the same tick again
    CHECK(breaks([](Eth::RenderSnapshot& s) { s.sceneSerial = 4; }));     // a death reloading level 1
    CHECK(breaks([](Eth::RenderSnapshot& s) { s.sceneFile = "scenes/level2.esc"; }));
    CHECK(breaks([](Eth::RenderSnapshot& s) { s.roundUp = false; }));
    CHECK(breaks([](Eth::RenderSnapshot& s) { s.zAxisDirection = glm::vec2(0.0f, -1.0f); }));

    // Through the interpolator: the frame IS the current snapshot, untouched.
    SnapshotInterpolator interp;
    interp.BeginTick(pair.previous);
    Eth::RenderSnapshot reloaded = pair.current;
    reloaded.sceneSerial = 4;
    CHECK(&interp.Frame(reloaded, 0.5f) == &reloaded);
    CHECK(!interp.Stats().blended);
    Eth::RenderSnapshot late = pair.current;
    late.frameIndex = 12;
    CHECK(&interp.Frame(late, 0.5f) == &late);
    CHECK(&interp.Frame(pair.current, 0.5f) != &pair.current);
    CHECK(interp.Stats().blended);
}

void InterpolatorFlow() {
    const Pair pair;
    SnapshotInterpolator interp;
    CHECK(interp.Enabled());
    // Before the first tick there is nothing to blend from.
    CHECK(&interp.Frame(pair.current, 0.5f) == &pair.current);

    interp.BeginTick(pair.previous);
    const Eth::RenderSnapshot& half = interp.Frame(pair.current, 0.5f);
    CHECK(&half != &pair.current);
    CHECK(half.sprites[0].position == glm::vec3(305.0f, 400.0f, 0.0f));
    CHECK(half.camera == glm::vec2(105.0f, 50.0f));
    CHECK_EQ(interp.Stats().sprites, std::size_t{1});
    // The same copy, rewritten, frame after frame of one tick.
    const Eth::RenderSnapshot& one = interp.Frame(pair.current, 1.0f);
    CHECK(&one == &half);
    CHECK(one.sprites[0].origin == pair.current.sprites[0].origin);
    const Eth::RenderSnapshot& zero = interp.Frame(pair.current, 0.0f);
    CHECK(zero.sprites[0].origin == pair.previous.sprites[0].origin);
    CHECK(zero.camera == pair.previous.camera);
    // The HUD rides along untouched (the layer draws it from the current one).
    CHECK_EQ(zero.hud.size(), pair.current.hud.size());

    // The next tick: the copy is refreshed, so its other fields are the new tick's.
    Eth::RenderSnapshot next = Tick(12);
    next.camera = glm::vec2(120.0f, 50.0f);
    next.sprites = {Walker(5, {320.0f, 400.0f, 0.0f})};
    next.sprites[0].frame = 3;
    next.sprites[0].sprite = "walker_attack.png";
    interp.BeginTick(pair.current);
    const Eth::RenderSnapshot& third = interp.Frame(next, 0.5f);
    CHECK_EQ(third.sprites[0].frame, 3u);
    CHECK(third.sprites[0].sprite == "walker_attack.png");
    CHECK(third.lights.empty());
    CHECK(third.sprites[0].position == glm::vec3(315.0f, 400.0f, 0.0f));

    // Off: the current snapshot itself, and nothing is kept...
    interp.SetEnabled(false);
    CHECK(!interp.Enabled());
    CHECK(&interp.Frame(next, 0.5f) == &next);
    interp.BeginTick(next);
    CHECK(!interp.Previous().valid);
    // ...so turned on again, the first frame draws the current tick, and the
    // next tick blends again.
    interp.SetEnabled(true);
    CHECK(&interp.Frame(next, 0.5f) == &next);
    Eth::RenderSnapshot after = next;
    after.frameIndex = 13;
    after.sprites[0] = Walker(5, {330.0f, 400.0f, 0.0f});
    interp.BeginTick(next);
    CHECK(interp.Frame(after, 0.5f).sprites[0].position == glm::vec3(325.0f, 400.0f, 0.0f));
    interp.Reset();
    CHECK(&interp.Frame(after, 0.5f) == &after);
}

// ---- the renderers take the blend as it is ---------------------------------------------

void SpritePlacementFollowsTheBlend() {
    const Pair pair;
    const Eth::RenderSnapshot half = SnapshotInterpolator::Blended(PosesOf(pair.previous), pair.current, 0.5f);
    glm::vec3 blended(0.0f), expected(0.0f), scale(0.0f);
    float rotation = 0.0f;
    Render::SpriteRenderer::Placement(half.sprites[0], half.zAxisDirection, 0.0f, blended, scale, rotation);
    Render::SpriteRenderer::Placement(Walker(5, {305.0f, 400.0f, 0.0f}), glm::vec2(0.0f), 0.0f, expected, scale,
                                      rotation);
    CHECK_MSG(blended == expected, Str(blended) + " vs " + Str(expected));
    // The order ranks the blend by the current tick's indices.
    const Render::DrawOrder order = Render::ComputeDrawOrder(pair.current);
    CHECK_EQ(order.spriteRank.size(), half.sprites.size());
    CHECK_EQ(order.particleRank.size(), half.particles.size());
}

Eth::SpriteDraw Crate(const glm::vec3& at) {
    Eth::SpriteDraw caster = Walker(42, at);
    caster.sprite = "crate.png";
    caster.isStatic = false;
    caster.applyLight = true;
    caster.castShadow = true;
    caster.size = glm::vec2(40.0f, 50.0f);
    return caster;
}

void ShadowShapeFromTheTickPlacedByTheBlend(Render::TextureCache& textures) {
    // A crate walking past a torch 100 px above it. The strip's shape comes
    // from the tick (`shapes`), where it stands from the blend.
    Eth::RenderSnapshot current = Tick(70);
    current.sprites = {Crate({100.0f, 100.0f, 0.0f})};
    Eth::LightDraw torch = Lamp(7, {100.0f, 0.0f, 10.0f});
    torch.color = glm::vec3(1.0f);
    torch.castShadows = true;
    current.lights = {torch};
    Render::DrawOrder order;
    order.spriteRank = {1};
    order.shadowRankBase = {0};
    CHECK(Render::ShadowRenderer::ComputeShadow(current.sprites[0], torch, current.ambient).visible);

    entt::registry registry;
    Render::ShadowRenderer renderer;
    renderer.Attach(registry, textures);
    const Render::View view;
    const bool textureReadable = !textures.Key("data/shadow.dds", Render::TextureVariant::Plain).empty();
    std::size_t placedRight = 0;
    std::size_t draws = 0;
    for (const float x : {96.0f, 98.0f, 100.0f}) {
        Eth::RenderSnapshot blended = current;
        blended.sprites[0].position.x = x;
        renderer.Draw(registry, blended, view, order, &current);
        ++draws;
        // One slot for the one pair, however the blend moves.
        CHECK_EQ(renderer.SlotCount(), textureReadable ? std::size_t{1} : std::size_t{0});
        for (const entt::entity e : registry.view<Supersonic::TagComponent, Supersonic::TransformComponent>()) {
            if (registry.get<Supersonic::TagComponent>(e).tag != "Penumbra Shadow") continue;
            const glm::vec3 at = registry.get<Supersonic::TransformComponent>(e).position;
            if (at == Render::ToWorld(glm::vec2(x, 100.0f), Render::RankZ(0))) ++placedRight;
        }
    }
    CHECK_EQ(placedRight, textureReadable ? draws : std::size_t{0});
    // Lists that do not line up are ignored: the blend alone decides.
    Eth::RenderSnapshot misaligned = current;
    misaligned.lights.push_back(torch);
    Eth::RenderSnapshot blended = current;
    blended.sprites[0].position.x = 90.0f;
    renderer.Draw(registry, blended, view, order, &misaligned);
    CHECK_EQ(renderer.SlotCount(), textureReadable ? std::size_t{1} : std::size_t{0});
    renderer.Detach(registry);
    if (!textureReadable) std::printf("  (shadow.dds unreadable: the shadow placement checks ran empty)\n");
}

// ---- settings ----------------------------------------------------------------------------

void SmoothMotionSetting() {
    const Render::Settings defaults = Render::Settings::Defaults(false);
    CHECK(defaults.smoothMotion);
    CHECK(defaults.ToJson().find("\"smoothMotion\": true") != std::string::npos);

    Render::Settings off = defaults;
    off.smoothMotion = false;
    std::string warning;
    const Render::Settings back = Render::Settings::FromJson(off.ToJson(), defaults, &warning);
    CHECK(back == off);
    CHECK(!back.smoothMotion);
    CHECK(warning.empty());

    // An older file without the field keeps the default; a mistyped one warns.
    CHECK(Render::Settings::FromJson(R"({"language": "en"})", defaults).smoothMotion);
    warning.clear();
    const Render::Settings typo = Render::Settings::FromJson(R"({"smoothMotion": "no"})", defaults, &warning);
    CHECK(typo.smoothMotion);
    CHECK(!warning.empty());
    CHECK(!Render::Settings::FromJson(R"({"smoothMotion": false})", defaults).smoothMotion);
}

// ---- the real game -------------------------------------------------------------------------

struct Run {
    Eth::Machine& machine;
    SnapshotInterpolator interp;

    // One tick as the layer runs it: the outgoing snapshot remembered, then
    // the frame.
    void Step(const Eth::InputFrame& input = Eth::InputFrame{}) {
        interp.BeginTick(machine.Snapshot());
        machine.Frame(input);
    }
};

void RealGame() {
    std::printf("-- the real game\n");
    Eth::MachineConfig config;   // no user directory: nothing is written
    Eth::Machine machine(config);
    Eth::Machine::Scope scope(machine);
    Script::RegisterAll(machine);
    machine.Boot(Script::ScriptMain);

    Run run{machine};
    run.Step();
    const Eth::RenderSnapshot& snap = machine.Snapshot();
    CHECK(snap.sceneFile == "scenes/menu.esc");
    CHECK_EQ(snap.sceneSerial, 1u);
    // The first frame of a scene is never blended.
    CHECK(&run.interp.Frame(snap, 0.5f) == &snap);

    // The menu's torches: every particle carries who it is.
    std::size_t blendedParticles = 0;
    std::size_t badIds = 0;
    std::size_t notFinite = 0;
    std::size_t drawnParticles = 0;
    for (int i = 0; i < 60; ++i) {
        run.Step();
        for (const Eth::ParticleDraw& p : snap.particles) {
            ++drawnParticles;
            if (p.particleId < 0 || (p.system != 0 && p.system != 1) || !(p.lifeStartMs >= 0.0f)) ++badIds;
        }
        const Eth::RenderSnapshot& drawn = run.interp.Frame(snap, 0.5f);
        blendedParticles += run.interp.Stats().particles;
        for (const Eth::ParticleDraw& p : drawn.particles) {
            if (!std::isfinite(p.position.x) || !std::isfinite(p.position.y) || !std::isfinite(p.size)) ++notFinite;
        }
    }
    std::printf("  menu: %zu particles drawn over 60 ticks, %zu blended\n", drawnParticles, blendedParticles);
    CHECK(drawnParticles > 0);
    CHECK_EQ(badIds, std::size_t{0});
    CHECK_EQ(notFinite, std::size_t{0});
    CHECK(blendedParticles > 0);
    CHECK_EQ(snap.sceneSerial, 1u);

    // New Game: the level loads (a new serial, not blended), the wizard spawns.
    try {
        Script::newGame("CAMPAIGN");
    } catch (const Eth::ScriptException& e) {
        CHECK_MSG(false, std::string("newGame threw: ") + e.what());
    }
    bool sawLoad = false;
    for (int i = 0; i < 240; ++i) {
        const Eth::uint serial = snap.sceneSerial;
        run.Step();
        if (snap.sceneSerial != serial) {
            sawLoad = true;
            CHECK(&run.interp.Frame(snap, 0.5f) == &snap);
        }
    }
    CHECK(sawLoad);
    CHECK(snap.sceneFile == "scenes/level1.esc");
    CHECK(snap.sceneSerial >= 2u);

    const auto findWizard = [](const Eth::RenderSnapshot& s) -> const Eth::SpriteDraw* {
        for (const Eth::SpriteDraw& sprite : s.sprites) {
            if (sprite.entityName == "bruxo.ent") return &sprite;
        }
        return nullptr;
    };
    CHECK(findWizard(snap) != nullptr);

    // Walk right for 120 ticks (test_pn_boot's walk), each drawn at three
    // points between the tick before and the tick itself.
    Eth::InputFrame right;
    right.keys[Eth::K_RIGHT] = true;
    std::size_t walked = 0;          // frames the wizard was drawn in
    std::size_t between = 0;         // ...between his two ticks
    std::size_t blended = 0;         // ...on a tick he moved, somewhere other than where he is now
    std::size_t moving = 0;          // frames of ticks he moved on
    std::size_t drifted = 0;         // ...off his place on screen
    std::size_t cameraSnapped = 0;
    std::size_t cameraBetween = 0;
    std::size_t cameraMoved = 0;
    std::size_t ticks = 0;           // ticks he was drawn on both sides of (a hurt blink hides him)
    // Where on screen he is drawn FROM (his anchor, not his corner: a new
    // animation may bring another frame size, which moves the corner alone).
    const auto anchorX = [](const Eth::SpriteDraw& sprite, const Eth::RenderSnapshot& s) {
        return SnapshotInterpolator::DrawnAnchor(sprite, s.zAxisDirection, s.roundUp).x;
    };
    for (int i = 0; i < 120; ++i) {
        const Eth::SpriteDraw* before = findWizard(snap);
        const bool hadBefore = before != nullptr;
        // Read now: the next frame rebuilds the snapshot under the pointer.
        const glm::vec3 from = hadBefore ? before->position : glm::vec3(0.0f);
        const float fromScreen = hadBefore ? anchorX(*before, snap) - snap.camera.x : 0.0f;
        const glm::vec2 fromCamera = snap.camera;
        run.Step(right);
        const Eth::SpriteDraw* after = findWizard(snap);
        if (!hadBefore || after == nullptr) continue;
        ++ticks;
        const float toScreen = anchorX(*after, snap) - snap.camera.x;
        // The current frame's corner-to-anchor offset, which the blend keeps.
        const float offset = anchorX(*after, snap) - after->origin.x;
        if (snap.camera != fromCamera) ++cameraMoved;
        for (const float alpha : {0.25f, 0.5f, 0.75f}) {
            const Eth::RenderSnapshot& drawn = run.interp.Frame(snap, alpha);
            if (run.interp.Stats().cameraSnapped) ++cameraSnapped;
            const float cx = drawn.camera.x;
            if (cx >= std::min(fromCamera.x, snap.camera.x) && cx <= std::max(fromCamera.x, snap.camera.x)) {
                ++cameraBetween;
            }
            const Eth::SpriteDraw* wizard = findWizard(drawn);
            if (wizard == nullptr) continue;
            ++walked;
            const float x = wizard->position.x;
            if (x >= std::min(from.x, after->position.x) - 1e-3f && x <= std::max(from.x, after->position.x) + 1e-3f) {
                ++between;
            }
            // A real step, not an ulp of collision push-back while he is
            // blocked, which a lerp can round straight back onto the tick.
            if (glm::length(after->position - from) > 0.01f) {
                ++moving;
                if (wizard->position != after->position) ++blended;
            }
            const float screen = wizard->origin.x + offset - drawn.camera.x;
            if (screen < std::min(fromScreen, toScreen) || screen > std::max(fromScreen, toScreen)) ++drifted;
        }
    }
    std::printf("  walk: %zu wizard frames, %zu between his ticks, %zu of %zu moving ones blended, %zu drifted on "
                "screen; camera moved on %zu ticks, snapped on %zu frames\n",
                walked, between, blended, moving, drifted, cameraMoved, cameraSnapped);
    CHECK(ticks >= 100u);
    CHECK_EQ(walked, 3 * ticks);
    CHECK_EQ(between, walked);
    CHECK(moving >= 60u);    // he walks (test_pn_boot: over 100 px in 120 ticks); the count only has to be real
    CHECK_EQ(blended, moving);
    CHECK_EQ(drifted, std::size_t{0});
    CHECK_EQ(cameraSnapped, std::size_t{0});
    CHECK_EQ(cameraBetween, 3 * ticks);
    CHECK(cameraMoved > 0);
    CHECK_EQ(machine.ScriptAborts(), 0u);
}

} // namespace

int main() {
    Render::TextureCache textures(PENUMBRA_ORIGINAL_DIR);
    entt::registry bare;
    textures.Attach(bare);

    AlphaAndWholePixels();
    MidpointAndEnds();
    PixelGrid();
    FollowedSpriteHoldsItsPlaceOnScreen();
    Jumps();
    Identity();
    Continuity();
    InterpolatorFlow();
    SpritePlacementFollowsTheBlend();
    ShadowShapeFromTheTickPlacedByTheBlend(textures);
    SmoothMotionSetting();

    if (std::filesystem::exists(PENUMBRA_ORIGINAL_DIR "/scenes/level1.esc")) {
        RealGame();
    } else {
        std::printf("SKIP: the original is not at %s\n", PENUMBRA_ORIGINAL_DIR);
        return 77;
    }
    return test::summary("test_pn_render_interp", 150);
}
