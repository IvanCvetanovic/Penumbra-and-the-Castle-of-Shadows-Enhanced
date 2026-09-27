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
// release when allAtOnce is 0, Kill() stopping new releases - must be the
// original's to the millisecond.
//
// Differences from 0.7.12 that are deliberate:
//  - Time. 0.7.12 reads an absolute millisecond clock for particle age and a
//    frame timer for motion, frameSpeed = min(dt*60, 2) (ETHParticleManager.cpp:650-654).
//    Here both come from the Machine's tick: nowMs is GetTime() as a float,
//    frameSpeed is 1 on a normal tick.
//  - Randomness comes from the Machine's Random, not a global.
//  - Drawing is not done here: Particles() is read by the snapshot.
//  - Sound: HandleSoundPlayback's volume/pan/loop decision is computed here and
//    applied by the caller to the sample bank (SoundDecision).

#include <vector>

#include "eth/Defs.hpp"
#include "eth/Random.hpp"

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
// system's sample this frame.
struct SoundDecision {
    bool touch = false;     // false: leave the sample alone this frame
    bool stop = false;      // stop a looping sample (no active particles left)
    bool loop = false;      // the sample should loop (repeat >= 4 or endless)
    float volume = 1.0f;    // motion damp x active ratio x entity volume, clamped
    float pan = 0.0f;       // -1..1 from the screen x
};

class ParticleManager {
public:
    // CreateParticleSystem: the system starts at the owner's position, with
    // every particle's repeat at 0; nothing is released until Update.
    ParticleManager(const ParticleSystemDef& system, const vector2& ownerPos2, const vector3& ownerPos3,
                    float ownerAngle, float entityVolume, float nowMs, Random& rng);

    // UpdateParticleSystem: once per Ethanon frame. `frameSpeed` is
    // min(frameSeconds*60, 2). Returns false when the system has finished.
    bool Update(const vector2& ownerPos2, const vector3& ownerPos3, float ownerAngle, float nowMs,
                float frameSpeed, Random& rng);

    // Play: restart (every repeat count to zero).
    void Play(const vector2& ownerPos2, const vector3& ownerPos3, float ownerAngle, float nowMs, Random& rng);

    // HandleSoundPlayback's decision for this frame. `ownerScreenPos` is the
    // owner's position on screen, `screenSize` the logical screen.
    SoundDecision SoundPlayback(const vector2& ownerScreenPos, const vector2& screenSize, float frameSpeed);

    void Kill(bool kill);
    bool Killed() const { return m_killed; }
    bool Finished() const { return m_finished; }
    bool Endless() const { return m_system.repeat <= 0; }
    void MirrorX(bool mirrorGravity);
    void MirrorY(bool mirrorGravity);
    void Scale(float scale);

    int NumParticles() const { return m_system.nParticles; }
    int NumActiveParticles() const { return m_nActiveParticles; }
    const ParticleSystemDef& System() const { return m_system; }
    const std::vector<Particle>& Particles() const { return m_particles; }
    bool HasSoundEffect() const { return !m_system.soundEffect.empty(); }

private:
    ParticleSystemDef m_system;
    std::vector<Particle> m_particles;
    bool m_finished = false;
    bool m_killed = false;
    int m_nActiveParticles = 0;
    vector2 m_move{0.0f};
    float m_soundVolume = 1.0f;
    float m_entityVolume = 1.0f;
    float m_generalVolume = 1.0f;
    bool m_isSoundLooping = false;
    bool m_isSoundStopped = false;
};

} // namespace Penumbra::Eth
