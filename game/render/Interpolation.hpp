#pragma once

// ENHANCEMENT E8: the world drawn between ticks, not on them.
//
// The game thinks once per 60 Hz tick (one Ethanon frame, CLAUDE.md rule 7)
// and 0.7.12 drew once per 60 Hz vsync, so every picture was one tick. On a
// 120 or 144 Hz display the layer draws the same snapshot two or three times
// in a row, and a walking wizard and the camera following him move in steps.
// This blends the world - sprites, lights and their halos, shadows,
// particles, the camera - between the last two snapshots by the fraction of a
// tick the frame arrived at (Supersonic::SimulationClock::alpha). The HUD is
// not blended: it is screen text and panels, and is drawn from the current
// snapshot as before.
//
// ONE TICK BEHIND, BY DESIGN. alpha is how far the frame is INTO the next
// tick, which has not run yet; the only honest picture between two known
// states is between the previous one and the current one. So alpha 0 draws
// the previous snapshot's positions and alpha 1 the current one's - the
// standard fixed-step interpolation, and the engine's own InterpolationSystem
// (engine/src/core/InterpolationSystem.hpp: previous + (current - previous) *
// alpha) does exactly the same for its transforms. Nothing inside a tick ever
// reads what is drawn: the blend is a copy of the snapshot, the Machine's own
// is never touched, so the simulation is exactly what it was.
//
// WHAT IS BLENDED, and what is not. Only where things are: a sprite's
// position and drawn corner, a light's position (and so its halo), a
// particle's position, size and angle, the camera. Everything else - which
// sprite, which frame of it, its colour, the draw order, the lights' colours -
// is the current snapshot's, so a frame never shows an animation cell or a
// colour that no tick produced.
//
// IDENTITY. Things are matched between the two snapshots by who they are, not
// where they are in the list: a sprite by entityId, a light by ownerId, a
// particle by (ownerId, system, particleId, lifeStartMs) (Snapshot.hpp - a
// particle respawned at its emitter starts a new life and is a new particle).
// Something with no previous pose - just created, just come into view, or an
// id seen twice in the previous snapshot - is drawn where the current snapshot
// has it.
//
// NEVER ACROSS A JUMP. The whole frame is drawn as the current snapshot when
// the two are not consecutive ticks of one scene: a gap in frameIndex, another
// sceneSerial (a death reloading the same level counts), another scene file,
// or a change of SetPositionRoundUp or the z axis. Within a scene, one thing
// that moved more than kMaxStepPixels in a tick - a respawn, a teleport, the
// camera sent to a checkpoint - is drawn where it is now rather than slid
// across the screen. The fastest legitimate motion is far under it: walking is
// 2.5 px a tick, a spell 1.5 x its caster's speed, a fall at GRAVITY 1200 px/s^2
// about 20 px a tick after a second, and the largest earthquake step 20 px.
//
// THE PIXEL GRID, kept. 0.7.12 floored a sprite's position before drawing it
// (SetPositionRoundUp, SpriteDraw::origin) and cameraManager.as floors the
// camera, so every sprite sat on a whole pixel. A blend of two whole-pixel
// positions is put back on a whole pixel (WholePixel), for sprites and camera
// ALIKE: a sprite the camera follows keeps its exact place on screen, since
// both are rounded from values the same whole number apart. A camera with a
// fractional end (a scene that never rounds it) and particles and lights,
// which 0.7.12 never rounded, are blended as they are.
//
// FIXED STEP. Under --fixed-step every frame runs exactly one tick and alpha is
// 0, so a blend would only ever draw the previous tick - every capture one
// tick late. The layer turns the blend off for such runs (the wiring in the
// layer, not here); SetEnabled(false) returns the current snapshot untouched.
//
// How the layer uses it:
//   BeginTick(machine.Snapshot())      every tick, BEFORE Machine::Frame replaces it
//   Frame(machine.Snapshot(), alpha)   every frame: the snapshot to draw the world from
//   SetEnabled(settings.smoothMotion)  the switch (render/Settings.hpp)

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

#include "eth/Snapshot.hpp"

namespace Penumbra::Render {

// Where one snapshot drew everything that moves, by identity. Compact: no
// names or paths, only what a blend reads.
struct SnapshotPoses {
    struct Sprite {
        glm::vec3 position{0.0f};   // SpriteDraw::position (unrounded)
        glm::vec2 anchor{0.0f};     // SnapshotInterpolator::DrawnAnchor
        bool duplicate = false;     // its id came twice: nothing to match against
    };
    struct Light {
        glm::vec3 position{0.0f};
        bool duplicate = false;
    };
    struct Particle {
        glm::vec2 position{0.0f};
        float size = 0.0f;
        float angle = 0.0f;
        bool duplicate = false;
    };
    struct ParticleKey {
        int ownerId = -1;
        int system = 0;
        int particleId = -1;
        float lifeStartMs = 0.0f;
        bool operator==(const ParticleKey& other) const = default;
    };
    struct ParticleKeyHash {
        std::size_t operator()(const ParticleKey& key) const;
    };

    bool valid = false;             // false: nothing captured (every frame draws the current snapshot)
    Eth::uint frameIndex = 0;
    Eth::uint sceneSerial = 0;
    std::string sceneFile;
    bool roundUp = true;
    glm::vec2 zAxisDirection{0.0f};
    glm::vec2 camera{0.0f};
    std::unordered_map<int, Sprite> sprites;                                  // by entityId
    std::unordered_map<int, Light> lights;                                    // by ownerId
    std::unordered_map<ParticleKey, Particle, ParticleKeyHash> particles;

    // Replaces what is held with `snapshot`'s poses (the maps keep their
    // storage, so a tick allocates nothing once they have grown).
    void Capture(const Eth::RenderSnapshot& snapshot);
    void Clear();

    static ParticleKey KeyOf(const Eth::ParticleDraw& particle);
};

// What one blend did, for suites and a debug overlay.
struct BlendStats {
    bool blended = false;           // false: the frame is the current snapshot, untouched
    std::size_t sprites = 0;        // sprites drawn between two poses
    std::size_t lights = 0;
    std::size_t particles = 0;
    std::size_t snapped = 0;        // matched, but moved more than kMaxStepPixels: drawn where they are now
    bool cameraSnapped = false;
};

class SnapshotInterpolator {
public:
    // A thing that moved further than this in one tick jumped (see NEVER
    // ACROSS A JUMP above). World pixels.
    static constexpr float kMaxStepPixels = 64.0f;

    // Off: Frame returns the current snapshot itself and BeginTick keeps
    // nothing. On again, the first frame after it draws the current snapshot
    // (the pose it would blend from is gone) and the next tick blends again.
    void SetEnabled(bool enabled);
    bool Enabled() const { return m_enabled; }

    // Once per tick, BEFORE Machine::Frame overwrites `outgoing` (the Machine
    // rebuilds its snapshot in place): remembers where it drew everything.
    void BeginTick(const Eth::RenderSnapshot& outgoing);

    // Once per frame: the snapshot to draw the world from. `alpha` is
    // SimulationClock::alpha (clamped to [0, 1]; not a number counts as 0).
    // Returns `current` itself when disabled or when the two ticks may not be
    // blended (Continuous), otherwise this object's blended copy - index for
    // index the same sprites, lights and particles as `current`, so
    // ComputeDrawOrder(current) ranks it - valid until the next Frame or
    // BeginTick. `current` is expected to be the Machine's snapshot: the copy
    // is refreshed when its frameIndex or sceneSerial changes.
    const Eth::RenderSnapshot& Frame(const Eth::RenderSnapshot& current, float alpha);

    // Forgets the previous pose (the next frame draws the current snapshot).
    void Reset();

    const BlendStats& Stats() const { return m_stats; }
    const SnapshotPoses& Previous() const { return m_previous; }

    // ---- the pure halves, for suites ------------------------------------------

    // Whether `current` is the tick right after the one `previous` holds, in
    // the same scene, rounded and projected the same way.
    static bool Continuous(const SnapshotPoses& previous, const Eth::RenderSnapshot& current);

    // Writes the blended positions of `current` into `out`, which must hold
    // `current`'s entries index for index (a copy of it; it is copied first if
    // its lists are not the same sizes). Every positional field of `out` is
    // written - blended, or current's - so a copy reused across the frames of
    // one tick needs no refresh. Blends whatever matches even when
    // Continuous is false: that test is Frame's.
    static BlendStats Blend(const SnapshotPoses& previous, const Eth::RenderSnapshot& current, float alpha,
                            Eth::RenderSnapshot& out);

    // `current` copied and blended: Blend into a fresh copy.
    static Eth::RenderSnapshot Blended(const SnapshotPoses& previous, const Eth::RenderSnapshot& current,
                                       float alpha);

    // The point a sprite is drawn from, as 0.7.12 rounded it: ToScreenPos
    // (xy + zAxisDirection * z), floored when round-up is on - the Machine's
    // own arithmetic (Machine::Render), so SpriteDraw::origin is this minus the
    // sprite's fixed offset (centre plus pivot).
    static glm::vec2 DrawnAnchor(const Eth::SpriteDraw& sprite, const glm::vec2& zAxisDirection, bool roundUp);

    // The nearest whole pixel (halves up, the same on either side of zero).
    static float WholePixel(float value);
    static glm::vec2 WholePixel(const glm::vec2& value);

    // alpha as the blend reads it: [0, 1], NaN as 0.
    static float SanitiseAlpha(float alpha);

private:
    bool m_enabled = true;
    SnapshotPoses m_previous;
    Eth::RenderSnapshot m_blended;
    bool m_blendedValid = false;
    Eth::uint m_blendedFrame = 0;
    Eth::uint m_blendedSerial = 0;
    BlendStats m_stats;
};

} // namespace Penumbra::Render
