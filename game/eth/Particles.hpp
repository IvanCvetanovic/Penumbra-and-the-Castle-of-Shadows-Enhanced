#pragma once

// One Ethanon 0.7.12 particle system, simulated: a line-by-line port of
// ETHParticleManager (reference/eth-0.7.12/src/ETHParticleManager.cpp/.h),
// stepped once per Ethanon frame on the 60 Hz tick.
//
// This is GAMEPLAY, not decoration. An entity with no sprite whose particle
// systems all have repeat > 0 is "temporary" and is deleted by the runtime when
// its particles are over - and that is how long a sword's hit box, a fireball's
// flight and the light spell last (docs/spec/11-logic-player-combat.md). So
// the timing - lifeTime, randomizeLifeTime, repeat counting, the staggered
// release when allAtOnce is 0, Kill() stopping the re-release of spent
// particles - must be the original's to the millisecond.
//
// Differences from 0.7.12 that are deliberate:
//  - Time. 0.7.12 reads an absolute millisecond clock for particle age and a
//    frame timer for motion, frameSpeed = min(dt*60, 2) (ETHParticleManager.cpp:650-654).
//    Here both come from the Machine's tick: nowMs is GetTime() as a float,
//    frameSpeed is 1 on a normal tick.
//  - Randomness comes from the Machine's Random, not a global.
//  - Drawing is not done here: CollectDraws turns the particles into the
//    snapshot's ParticleDraws, the layer draws those.
//  - Sound: HandleSoundPlayback's volume/pan/loop decision is computed here and
//    applied by the caller to the sample bank (SoundDecision).

#include <vector>

#include "eth/Defs.hpp"
#include "eth/Random.hpp"
#include "eth/Snapshot.hpp"

namespace Penumbra::Eth {

struct Particle {
    vector2 pos{0.0f};
    vector2 dir{0.0f};
    glm::vec4 color{1.0f};
    vector3 startPoint{0.0f};
    float angle = 0.0f;
    float angleDir = 0.0f;
    float size = 0.0f;
    float lifeTime = 0.0f;
    float elapsed = 0.0f;
    float lastTime = 0.0f;
    int repeat = 0;
    bool released = false;
    int id = 0;
    uint currentFrame = 0;
};

// What HandleSoundPlayback (ETHParticleManager.cpp:727-780) would do to the
// system's sample this frame. Applied in this order, as 0.7.12 did: stop; else
// pan, volume, then the loop rule.
struct SoundDecision {
    bool touch = false;     // false: leave the sample alone this frame
    bool stop = false;      // stop a looping sample (no active particles left); volume/pan repeat the last ones
    // The loop rule (:761-772): if the sample is not playing, or is playing
    // without its loop flag, Play() it from the start and set its loop flag.
    // Only systems with repeat >= 4 or endless ask for it; every shipped
    // <SoundEffect> system has repeat 1, so the content never does.
    bool loop = false;
    float volume = 1.0f;    // motion damp (0.2..1) x active ratio (when looping) x entity volume; not clamped here
    float pan = 0.0f;       // -1..1 from the emitter's x on screen
};

class ParticleManager {
public:
    // CreateParticleSystem (ETHParticleManager.cpp:564-605): every particle is
    // drawn its life, size, direction and start (RNG) and stamped with nowMs;
    // nothing is released until Update. `ownerPos2` is the owner's xy (0.7.12
    // passed GetPositionXY()); `ownerPos3` is accepted for symmetry but unused,
    // because 0.7.12 reset the particles with (ownerPos2, 0) here. The entity
    // volume is taken as given (only SetSoundVolume clamped it). A system with
    // nParticles <= 0 is refused by 0.7.12; here it simply has no particles.
    // nowMs is the particle clock at creation: a system created in a callback
    // sees its first Update one tick later, where 0.7.12's GetTickCount had
    // barely moved (see Update).
    ParticleManager(const ParticleSystemDef& system, const vector2& ownerPos2, const vector3& ownerPos3,
                    float ownerAngle, float entityVolume, float nowMs, Random& rng);

    // UpdateParticleSystem (:650-727). `ownerPos2` is ToScreenPos(owner
    // position, zAxisDirection), `ownerPos3` the owner's position, as
    // ETHRenderEntity::Update passed them. `frameSpeed` is PER MANAGER in
    // 0.7.12: each has its own timer, reset at creation and read at every
    // Update, so it is min(secondsSinceThisManagersLastUpdate*60, 2): 1 on a
    // normal tick, ~0 for a second update in the same frame (RenderParticleList
    // updating a system CheckTemporaryEntities already did), up to 2 after a gap.
    // Returns false when the system has finished (0.7.12 returned true always;
    // Finished() is what it exposed).
    bool Update(const vector2& ownerPos2, const vector3& ownerPos3, float ownerAngle, float nowMs,
                float frameSpeed, Random& rng);

    // Play (:786-800): every repeat to 0, every particle unreleased and redrawn.
    // It does NOT clear Kill: PlayParticleSystem called Kill(false) first
    // (ETHRenderEntity.cpp:1264), and it restarted the sample itself.
    void Play(const vector2& ownerPos2, const vector3& ownerPos3, float ownerAngle, float nowMs, Random& rng);

    // HandleSoundPlayback (:729-779), which 0.7.12 ran at the end of every
    // Update of a system whose sample loaded: call it once right after each
    // Update, only when HasSoundEffect() and the sample exists, with the same
    // frameSpeed. `ownerPos2` is the position Update was given (world), `camera`
    // the world position of the screen's top-left: the motion damp compares
    // successive WORLD positions and the pan uses x - camera.x.
    SoundDecision SoundPlayback(const vector2& ownerPos2, const vector2& camera, const vector2& screenSize,
                                float frameSpeed);
    // The same with camera (0,0), for a caller holding the position on screen:
    // the pan is identical, but the damp then measures motion on screen rather
    // than in the world (a camera-followed emitter is not damped). Prefer the
    // overload above.
    SoundDecision SoundPlayback(const vector2& ownerScreenPos, const vector2& screenSize, float frameSpeed);
    // StopSFX (:500-503): a silenced system never (re)starts its looping sample.
    void StopSFX(bool stopped) { m_isSoundStopped = stopped; }
    bool IsSoundLooping() const { return m_isSoundLooping; }

    // Kill (:530-533) stops the RE-release of particles whose life ends: they
    // are not reset, so their repeat climbs by one per frame until the system
    // finishes (an endless system never does). It does NOT stop the first
    // release of particles still waiting in the allAtOnce=0 stagger (:676-689
    // have no Killed() test), so a system killed early still emits the rest of
    // its first wave. Killed particles past their life are neither active nor drawn.
    void Kill(bool kill);
    bool Killed() const { return m_killed; }
    bool Finished() const { return m_finished; }
    bool Endless() const { return m_system.repeat <= 0; }
    // MirrorX/MirrorY (:939-957): the system's start point, direction and
    // randomisation (and gravity when asked) flip, and so do every particle's
    // direction and position component - the position of an already released
    // particle is ABSOLUTE, so it flips about world x = 0, as in 0.7.12. The
    // scripts only mirror right after AddEntity, before any release.
    void MirrorX(bool mirrorGravity);
    void MirrorY(bool mirrorGravity);
    // Scale (:66-79): the length fields, but not the start point (commented
    // out in 0.7.12). The scripts never call it.
    void Scale(float scale);

    // DrawParticleSystem's per-particle output (ETHParticleManager.cpp:821-907,
    // the draw half): every released, live particle as a ParticleDraw, in draw
    // order, with the colour rule (luminance / ambient) and the particle's depth
    // (owner z + start z, plus the vertical-owner rise and ETH_PARTICLE_DEPTH_SHIFT)
    // as 0.7.12 computed them. Appends.
    //
    // Like 0.7.12 it first bubble-sorts the particles in place by
    // (start y - y): the video's alpha mode was PIXEL during the particle pass,
    // so every DRAWN system was sorted, and the sort changes the order later
    // Updates walk the particles and so draw from the RNG. Call it only for a
    // system that passes IsSphereInScreen, after that frame's Update, as
    // RenderParticleList did (ETHScene.cpp:1091-1132); Particles() reflects the
    // order. `minHeight`/`maxHeight` are the scene's running z extremes (note
    // DrawParticleSystem took them as max, min); `ownerDepth` is used only for a
    // layerable owner.
    void CollectDraws(int ownerId, const vector3& ambient, float minHeight, float maxHeight,
                      ENTITY_TYPE ownerType, const vector2& zAxisDirection, float ownerDepth,
                      std::vector<ParticleDraw>& out) const;

    int NumParticles() const { return m_system.nParticles; }
    int NumActiveParticles() const { return m_nActiveParticles; }
    const ParticleSystemDef& System() const { return m_system; }
    const std::vector<Particle>& Particles() const { return m_particles; }
    bool HasSoundEffect() const { return !m_system.soundEffect.empty(); }
    // GetBoundingRadius (:515-518) and GetStartPos (:505-508) of the manager's
    // copy, which MirrorX and Scale change. ETHScene culled and grew its height
    // range with the ENTITY's copy (unmirrored), and passed boundingSphere
    // itself, not this radius, to IsSphereInScreen (ETHScene.cpp:1105-1115).
    float BoundingRadius() const { return m_system.boundingSphere / 2.0f; }
    vector3 StartPoint() const { return m_system.startPoint; }

private:
    // gsm4x4RotateZ's 2x2 part (DLL@0x10002e60): a11=cos, a12=sin, a21=-sin,
    // a22=cos, applied by ETHGlobal::Multiply as x' = x*a11 + y*a12,
    // y' = x*a21 + y*a22 (ETHCommon.h:343-351).
    struct Rotation {
        float a11 = 1.0f, a12 = 0.0f, a21 = 0.0f, a22 = 1.0f;
    };
    static Rotation RotateZ(float degrees);

    void ResetParticle(Particle& particle, const vector2& ownerPos2, const vector3& ownerPos3, float ownerAngle,
                       const Rotation& rotation, float nowMs, Random& rng);
    void PositionParticle(Particle& particle, const vector2& ownerPos2, float ownerAngle, const Rotation& rotation,
                          const vector3& ownerPos3, Random& rng);

    ParticleSystemDef m_system;
    // Mutable because the draw sorts it in place (see CollectDraws): 0.7.12's
    // draw reordered the very array its update walked.
    mutable std::vector<Particle> m_particles;
    bool m_finished = false;
    bool m_killed = false;
    int m_nActiveParticles = 0;
    // m_v2Move, never initialised by 0.7.12's constructor (the open question
    // in docs/spec/90-synthesis.md). It cannot matter: the first Update always
    // counts 0 active particles, so the first HandleSoundPlayback only stores
    // the position here, whatever was in it.
    vector2 m_move{0.0f};
    float m_soundVolume = 1.0f;
    float m_entityVolume = 1.0f;
    float m_generalVolume = 1.0f;   // never changed in 0.7.12
    bool m_isSoundLooping = false;
    bool m_isSoundStopped = false;
    float m_lastVolume = 1.0f;      // what the last touching decision set, for a stop decision to repeat
    float m_lastPan = 0.0f;
};

} // namespace Penumbra::Eth
