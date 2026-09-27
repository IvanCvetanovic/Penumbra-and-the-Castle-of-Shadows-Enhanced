#include "render/Interpolation.hpp"

#include <bit>
#include <cmath>

namespace Penumbra::Render {

namespace {

constexpr float kMaxStepSquared = SnapshotInterpolator::kMaxStepPixels * SnapshotInterpolator::kMaxStepPixels;

// The two ends exactly, not from + (to - from) * t: at 1 that sum can miss
// `to` by an ulp, and alpha 1 must be the current snapshot to the bit.
template <typename T>
T Lerp(const T& from, const T& to, float t) {
    if (t <= 0.0f) return from;
    if (t >= 1.0f) return to;
    return from + (to - from) * t;
}

// Written so that a distance that is not a number counts as a jump.
bool Jumped(const glm::vec2& from, const glm::vec2& to) {
    const glm::vec2 d = to - from;
    return !(glm::dot(d, d) <= kMaxStepSquared);
}

bool Jumped(const glm::vec3& from, const glm::vec3& to) {
    const glm::vec3 d = to - from;
    return !(glm::dot(d, d) <= kMaxStepSquared);
}

bool IsWhole(const glm::vec2& v) { return v.x == std::floor(v.x) && v.y == std::floor(v.y); }

// Two whole-pixel ends give a whole-pixel blend (THE PIXEL GRID in the
// header); anything else is left where the blend put it. The one rule for the
// camera and the sprites, so a followed sprite never drifts a pixel against
// the screen.
//
// Measured back from the current end, and rounded as an OFFSET: the step
// (from - to) is a small whole number, exact in a float, and (1 - t) is the
// same float for every point of the frame, so a sprite and the camera that
// moved the same whole number of pixels get the same rounded offset to the
// bit. Rounding the blended coordinates themselves would not: a sprite near
// x 100 and the camera near x -514 carry different float precision, and at
// the odd alpha one of them rounds across a half the other does not.
glm::vec2 BlendPoint(const glm::vec2& from, const glm::vec2& to, float t) {
    if (t <= 0.0f) return from;
    if (t >= 1.0f) return to;
    const glm::vec2 back = (from - to) * (1.0f - t);
    return to + ((IsWhole(from) && IsWhole(to)) ? SnapshotInterpolator::WholePixel(back) : back);
}

// splitmix64's finaliser.
std::uint64_t Mix(std::uint64_t x) {
    x ^= x >> 30;
    x *= 0xBF58476D1CE4E5B9ull;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBull;
    x ^= x >> 31;
    return x;
}

} // namespace

// ---- SnapshotPoses ----------------------------------------------------------------

std::size_t SnapshotPoses::ParticleKeyHash::operator()(const ParticleKey& key) const {
    // -0 and 0 are equal keys and must hash alike (the clock is never
    // negative, but the hash should not be the one place that assumes it).
    const float life = key.lifeStartMs == 0.0f ? 0.0f : key.lifeStartMs;
    std::uint64_t h = Mix((static_cast<std::uint64_t>(static_cast<std::uint32_t>(key.ownerId)) << 32) |
                          static_cast<std::uint32_t>(key.particleId));
    h = Mix(h ^ ((static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(life)) << 8) |
                 static_cast<std::uint8_t>(key.system)));
    return static_cast<std::size_t>(h);
}

SnapshotPoses::ParticleKey SnapshotPoses::KeyOf(const Eth::ParticleDraw& particle) {
    return ParticleKey{particle.ownerId, particle.system, particle.particleId, particle.lifeStartMs};
}

void SnapshotPoses::Clear() {
    valid = false;
    sprites.clear();
    lights.clear();
    particles.clear();
}

void SnapshotPoses::Capture(const Eth::RenderSnapshot& snapshot) {
    valid = true;
    frameIndex = snapshot.frameIndex;
    sceneSerial = snapshot.sceneSerial;
    sceneFile = snapshot.sceneFile;
    roundUp = snapshot.roundUp;
    zAxisDirection = snapshot.zAxisDirection;
    camera = snapshot.camera;

    sprites.clear();
    for (const Eth::SpriteDraw& sprite : snapshot.sprites) {
        const auto [it, inserted] = sprites.try_emplace(sprite.entityId);
        if (!inserted) {
            // Two draws of one id (SpriteRenderer calls it never expected):
            // which of them a later one continues cannot be told.
            it->second.duplicate = true;
            continue;
        }
        it->second.position = sprite.position;
        it->second.anchor = SnapshotInterpolator::DrawnAnchor(sprite, snapshot.zAxisDirection, snapshot.roundUp);
    }

    lights.clear();
    for (const Eth::LightDraw& light : snapshot.lights) {
        const auto [it, inserted] = lights.try_emplace(light.ownerId);
        if (!inserted) {
            it->second.duplicate = true;
            continue;
        }
        it->second.position = light.position;
    }

    particles.clear();
    for (const Eth::ParticleDraw& particle : snapshot.particles) {
        const auto [it, inserted] = particles.try_emplace(KeyOf(particle));
        if (!inserted) {
            it->second.duplicate = true;
            continue;
        }
        it->second.position = particle.position;
        it->second.size = particle.size;
        it->second.angle = particle.angle;
    }
}

// ---- the pure halves --------------------------------------------------------------

float SnapshotInterpolator::SanitiseAlpha(float alpha) {
    if (!(alpha > 0.0f)) return 0.0f;   // NaN too
    return alpha >= 1.0f ? 1.0f : alpha;
}

float SnapshotInterpolator::WholePixel(float value) { return std::floor(value + 0.5f); }

glm::vec2 SnapshotInterpolator::WholePixel(const glm::vec2& value) {
    return glm::vec2(WholePixel(value.x), WholePixel(value.y));
}

glm::vec2 SnapshotInterpolator::DrawnAnchor(const Eth::SpriteDraw& sprite, const glm::vec2& zAxisDirection,
                                            bool roundUp) {
    // Machine::Render: at = ToScreenPos(position, zAxisDirection), floored
    // when round-up is on; origin = at - (centre + pivotAdjust).
    glm::vec2 at = glm::vec2(sprite.position.x, sprite.position.y) + zAxisDirection * sprite.position.z;
    if (roundUp) at = glm::vec2(std::floor(at.x), std::floor(at.y));
    return at;
}

bool SnapshotInterpolator::Continuous(const SnapshotPoses& previous, const Eth::RenderSnapshot& current) {
    return previous.valid && current.frameIndex == previous.frameIndex + 1u &&
           current.sceneSerial == previous.sceneSerial && current.sceneFile == previous.sceneFile &&
           current.roundUp == previous.roundUp && current.zAxisDirection == previous.zAxisDirection;
}

BlendStats SnapshotInterpolator::Blend(const SnapshotPoses& previous, const Eth::RenderSnapshot& current,
                                       float alpha, Eth::RenderSnapshot& out) {
    if (out.sprites.size() != current.sprites.size() || out.lights.size() != current.lights.size() ||
        out.particles.size() != current.particles.size()) {
        out = current;
    }
    BlendStats stats;
    stats.blended = true;
    const float t = SanitiseAlpha(alpha);
    const bool matchable = previous.valid;

    // The camera: SnapshotPoses always has one, so "no previous" is only an
    // empty capture.
    out.camera = current.camera;
    if (matchable) {
        if (Jumped(previous.camera, current.camera)) {
            stats.cameraSnapped = true;
        } else {
            out.camera = BlendPoint(previous.camera, current.camera, t);
        }
    }

    for (std::size_t i = 0; i < current.sprites.size(); ++i) {
        const Eth::SpriteDraw& sprite = current.sprites[i];
        Eth::SpriteDraw& drawn = out.sprites[i];
        drawn.position = sprite.position;
        drawn.origin = sprite.origin;
        if (!matchable) continue;
        const auto it = previous.sprites.find(sprite.entityId);
        if (it == previous.sprites.end() || it->second.duplicate) continue;
        const glm::vec2 anchor = DrawnAnchor(sprite, current.zAxisDirection, current.roundUp);
        if (Jumped(it->second.anchor, anchor) || Jumped(it->second.position, sprite.position)) {
            ++stats.snapped;
            continue;
        }
        ++stats.sprites;
        // The corner moves with the rounded anchor, keeping the current
        // frame's own offset (its size and pivot): a sprite that changed image
        // between the ticks is drawn whole, at the blended place.
        drawn.origin = sprite.origin + (BlendPoint(it->second.anchor, anchor, t) - anchor);
        drawn.position = Lerp(it->second.position, sprite.position, t);
    }

    for (std::size_t i = 0; i < current.lights.size(); ++i) {
        const Eth::LightDraw& light = current.lights[i];
        Eth::LightDraw& drawn = out.lights[i];
        drawn.position = light.position;
        if (!matchable) continue;
        const auto it = previous.lights.find(light.ownerId);
        if (it == previous.lights.end() || it->second.duplicate) continue;
        if (Jumped(it->second.position, light.position)) {
            ++stats.snapped;
            continue;
        }
        ++stats.lights;
        drawn.position = Lerp(it->second.position, light.position, t);
    }

    for (std::size_t i = 0; i < current.particles.size(); ++i) {
        const Eth::ParticleDraw& particle = current.particles[i];
        Eth::ParticleDraw& drawn = out.particles[i];
        drawn.position = particle.position;
        drawn.size = particle.size;
        drawn.angle = particle.angle;
        if (!matchable) continue;
        const auto it = previous.particles.find(SnapshotPoses::KeyOf(particle));
        if (it == previous.particles.end() || it->second.duplicate) continue;
        if (Jumped(it->second.position, particle.position)) {
            ++stats.snapped;
            continue;
        }
        ++stats.particles;
        drawn.position = Lerp(it->second.position, particle.position, t);
        drawn.size = Lerp(it->second.size, particle.size, t);
        // Unwrapped: a particle's angle only ever accumulates angleDir
        // (Particles.cpp Update), so the straight line is the way it turned.
        drawn.angle = Lerp(it->second.angle, particle.angle, t);
    }
    return stats;
}

Eth::RenderSnapshot SnapshotInterpolator::Blended(const SnapshotPoses& previous, const Eth::RenderSnapshot& current,
                                                  float alpha) {
    Eth::RenderSnapshot out = current;
    Blend(previous, current, alpha, out);
    return out;
}

// ---- the per-frame half -------------------------------------------------------------

void SnapshotInterpolator::SetEnabled(bool enabled) {
    if (enabled == m_enabled) return;
    m_enabled = enabled;
    Reset();
}

void SnapshotInterpolator::Reset() {
    m_previous.Clear();
    m_blendedValid = false;
    m_stats = BlendStats{};
}

void SnapshotInterpolator::BeginTick(const Eth::RenderSnapshot& outgoing) {
    if (!m_enabled) {
        if (m_previous.valid) m_previous.Clear();
        return;
    }
    m_previous.Capture(outgoing);
}

const Eth::RenderSnapshot& SnapshotInterpolator::Frame(const Eth::RenderSnapshot& current, float alpha) {
    m_stats = BlendStats{};
    if (!m_enabled || !Continuous(m_previous, current)) return current;
    // Copied once a tick: the frames between ticks only rewrite the positions
    // (Blend writes every one of them).
    if (!m_blendedValid || m_blendedFrame != current.frameIndex || m_blendedSerial != current.sceneSerial) {
        m_blended = current;
        m_blendedValid = true;
        m_blendedFrame = current.frameIndex;
        m_blendedSerial = current.sceneSerial;
    }
    m_stats = Blend(m_previous, current, alpha, m_blended);
    return m_blended;
}

} // namespace Penumbra::Render
