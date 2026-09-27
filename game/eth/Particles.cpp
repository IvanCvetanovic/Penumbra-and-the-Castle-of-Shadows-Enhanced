// ETHParticleManager from Ethanon 0.7.12 (reference/eth-0.7.12/src/ETHParticleManager.cpp),
// ported line by line. Citations without a file are to ETHParticleManager.cpp;
// DLL@ addresses are GameSpace.dll (reference/analysis/gs.asm), the closed
// GameSpaceLib 1.6.4.1 the game shipped with, whose gsRandF, gsm4x4RotateZ and
// gsGetDist2 this file reproduces.

#include "eth/Particles.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include "core/DetMath.hpp"

namespace Penumbra::Eth {

namespace {

// ETHParticleManager.h:54-55.
constexpr int kMinimumParticleRepeatsToLoopSound = 4;
constexpr float kParticleDepthShift = 10.0f;

// ETHGlobal::Min/Max (ETHCommon.h:211-220). Written out rather than std::min
// so a NaN falls through the same side it did in 0.7.12.
float EthMin(const float a, const float b) { return (a < b) ? a : b; }
float EthMax(const float a, const float b) { return (a > b) ? a : b; }

// gsRandF(min, max) (DLL@0x10004fc0): it orders its bounds and draws over the
// width, so a range written backwards - a mirrored randomisation, whose half
// width is negative - still yields min + U*width from the LOWER bound. Random's
// own RandF(min, max) would walk from `min` instead and give a different value
// for the same draw. One draw either way.
float GsRandF(Random& rng, const float a, const float b) {
    const float lo = EthMin(a, b);
    const float hi = EthMax(a, b);
    return rng.RandF(hi - lo) + lo;
}

// gsDegreeToRadian (DLL@0x100037f0): times the double 0.01745329238474369,
// which is (float)(pi/180) widened, then stored as a float.
float GsDegreeToRadian(const float degrees) {
    return static_cast<float>(static_cast<double>(degrees) * 0.01745329238474369);
}

// gsSqrt (DLL@0x10002b30) is Quake's reciprocal square root, one Newton step,
// inverted - not sqrtf. The sound damp divides two of these, so the error
// mostly cancels, but the first frame's damp is decided by one.
float GsSqrt(const float x) {
    const float half = x * 0.5f;
    const std::int32_t bits = 0x5f3759df - (std::bit_cast<std::int32_t>(x) >> 1);
    const float y = std::bit_cast<float>(bits);
    const float r = y * (1.5f - (half * y) * y);
    return 1.0f / r;
}

// gsGetDist2 (DLL@0x10004b90): gsSqrt(gsDP2(a - b, a - b)).
float GsGetDist2(const vector2& a, const vector2& b) {
    const vector2 d = a - b;
    return GsSqrt(d.y * d.y + d.x * d.x);
}

// A float cast to an integer in 0.7.12 went through MSVC's _ftol2: truncation
// toward zero to 64 bits, of which the cast kept the low bits. NaN and values
// past 2^63 give the "integer indefinite" 0x8000000000000000, low bits 0.
std::int64_t Ftol(const float value) {
    if (!(std::fabs(value) < 9.2e18f)) return 0;
    return static_cast<std::int64_t>(value);
}
uint8 ToByte(const float value) { return static_cast<uint8>(Ftol(value) & 0xFF); }
uint ToUint(const float value) { return static_cast<uint>(Ftol(value) & 0xFFFFFFFF); }

bool HasSpriteCut(const ParticleSystemDef& system) { return system.spriteCutX > 1 || system.spriteCutY > 1; }

// ETHParticleManager::BubbleSort (:802-819), with ETH_PARTICLE::operator< on
// GetOffset() = v3StartPoint.y - v2Pos.y (ETHParticleManager.h:60-67). Stable:
// only a strictly smaller neighbour moves.
void BubbleSort(std::vector<Particle>& v) {
    const auto offset = [](const Particle& p) { return p.startPoint.y - p.pos.y; };
    const int len = static_cast<int>(v.size());
    for (int j = len - 1; j > 0; j--) {
        bool leave = true;
        for (int i = 0; i < j; i++) {
            if (offset(v[i + 1]) < offset(v[i])) {
                leave = false;
                std::swap(v[i + 1], v[i]);
            }
        }
        if (leave) return;
    }
}

} // namespace

ParticleManager::Rotation ParticleManager::RotateZ(const float degrees) {
    // gsm4x4RotateZ(&rot, gsDegreeToRadian(angle)). sin/cos from DetMath so a
    // rotated emitter reproduces on every machine; at angle 0 (every emitter in
    // the shipped content) they are exactly 0 and 1 anyway.
    float s = 0.0f;
    float c = 1.0f;
    Supersonic::DetMath::sincos(GsDegreeToRadian(degrees), s, c);
    Rotation r;
    r.a11 = c;
    r.a12 = s;
    r.a21 = -s;
    r.a22 = c;
    return r;
}

namespace {

// ETHGlobal::Multiply(GS_VECTOR2, GS_M4X4) (ETHCommon.h:343-351).
vector2 Multiply(const vector2& v, const float a11, const float a12, const float a21, const float a22) {
    return vector2(v.x * a11 + v.y * a12, v.x * a21 + v.y * a22);
}

} // namespace

// CreateParticleSystem (:564-605).
ParticleManager::ParticleManager(const ParticleSystemDef& system, const vector2& ownerPos2,
                                 [[maybe_unused]] const vector3& ownerPos3, const float ownerAngle,
                                 const float entityVolume, const float nowMs, Random& rng)
    : m_system(system), m_entityVolume(entityVolume) {
    if (m_system.nParticles <= 0) {
        // :569-573 refused the system; ETHRenderEntity never built one (:359).
        return;
    }

    m_nActiveParticles = m_system.allAtOnce ? m_system.nParticles : 0;
    m_particles.resize(static_cast<std::size_t>(m_system.nParticles));

    const Rotation rot = RotateZ(ownerAngle);
    for (int t = 0; t < m_system.nParticles; t++) {
        Particle& particle = m_particles[static_cast<std::size_t>(t)];
        particle.id = t;
        particle.released = false;
        // :594 - the owner's z is not passed here, (v2Pos, 0) is.
        ResetParticle(particle, ownerPos2, vector3(ownerPos2, 0.0f), ownerAngle, rot, nowMs, rng);
    }
}

// ResetParticle (ETHParticleManager.h:273-299). The draws are in this order,
// one each whatever the range: angleDir, lifeTime, size, dir.x, dir.y, then
// PositionParticle's three, then a frame when PICK_RANDOM_FRAME.
void ParticleManager::ResetParticle(Particle& particle, const vector2& ownerPos2, const vector3& ownerPos3,
                                    const float ownerAngle, const Rotation& rotation, const float nowMs,
                                    Random& rng) {
    particle.angleDir = m_system.angleDir + GsRandF(rng, -m_system.randAngle / 2, m_system.randAngle / 2);
    particle.elapsed = 0.0f;
    particle.lastTime = nowMs;
    particle.lifeTime = m_system.lifeTime +
                        GsRandF(rng, -m_system.randomizeLifeTime / 2, m_system.randomizeLifeTime / 2);
    particle.size = m_system.size + GsRandF(rng, -m_system.randomizeSize / 2, m_system.randomizeSize / 2);
    particle.dir.x = m_system.direction.x + GsRandF(rng, -m_system.randomizeDir.x / 2, m_system.randomizeDir.x / 2);
    particle.dir.y = m_system.direction.y + GsRandF(rng, -m_system.randomizeDir.y / 2, m_system.randomizeDir.y / 2);
    particle.dir = Multiply(particle.dir, rotation.a11, rotation.a12, rotation.a21, rotation.a22);
    particle.color = m_system.color0;
    PositionParticle(particle, ownerPos2, ownerAngle, rotation, ownerPos3, rng);

    // setup sprite frame
    if (HasSpriteCut(m_system)) {
        if (m_system.animationMode == ParticleSystemDef::kPlayAnimation) {
            particle.currentFrame = 0;
        } else if (m_system.animationMode == ParticleSystemDef::kPickRandomFrame) {
            particle.currentFrame = static_cast<uint>(rng.RandI(m_system.spriteCutX * m_system.spriteCutY - 1));
        }
    }
}

// PositionParticle (ETHParticleManager.h:301-310). randAngleStart is the one
// one-sided draw: [0, randAngleStart], not centred.
void ParticleManager::PositionParticle(Particle& particle, const vector2& ownerPos2, const float ownerAngle,
                                       const Rotation& rotation, const vector3& ownerPos3, Random& rng) {
    particle.angle = m_system.angleStart + rng.RandF(m_system.randAngleStart) + ownerAngle;
    particle.pos.x = m_system.startPoint.x +
                     GsRandF(rng, -m_system.randStartPoint.x / 2, m_system.randStartPoint.x / 2);
    particle.pos.y = m_system.startPoint.y +
                     GsRandF(rng, -m_system.randStartPoint.y / 2, m_system.randStartPoint.y / 2);
    particle.pos = Multiply(particle.pos, rotation.a11, rotation.a12, rotation.a21, rotation.a22);
    particle.pos = particle.pos + ownerPos2;
    particle.startPoint = vector3(ownerPos2, ownerPos3.z) + m_system.startPoint;
}

// UpdateParticleSystem (:650-727), without its HandleSoundPlayback call (see
// SoundPlayback). Particle age is the absolute clock (nowMs - lastTime),
// motion is per frame scaled by frameSpeed: the two only agree at 60 Hz.
bool ParticleManager::Update(const vector2& ownerPos2, const vector3& ownerPos3, const float ownerAngle,
                             const float nowMs, const float frameSpeed, Random& rng) {
    bool anythingDrawn = false;
    const Rotation rot = RotateZ(ownerAngle);
    m_nActiveParticles = 0;
    const float elapsedTime = nowMs;
    const bool animated = HasSpriteCut(m_system);
    const float frameCount = static_cast<float>(m_system.spriteCutX * m_system.spriteCutY);

    for (Particle& particle : m_particles) {
        if (m_system.repeat > 0 && particle.repeat >= m_system.repeat) continue;

        // check how many particles are active (with last frame's size and age)
        if (particle.size > 0.0f && particle.released) {
            if (!Killed() || (Killed() && particle.elapsed < particle.lifeTime)) m_nActiveParticles++;
        }

        // A particle still inside its repeat count keeps the system alive,
        // released or not: Finished() only turns true on the update AFTER the
        // last particle's last death.
        anythingDrawn = true;
        particle.elapsed = elapsedTime - particle.lastTime;

        if (!particle.released) {
            // The allAtOnce=0 stagger: particle `id` waits (lifeTime +
            // randomizeLifeTime) * id / nParticles ms from its creation stamp,
            // so the whole first wave spreads over one maximum life. No
            // Killed() test here (see Kill).
            const float releaseTime = (m_system.lifeTime + m_system.randomizeLifeTime) *
                                      (static_cast<float>(particle.id) / static_cast<float>(m_system.nParticles));
            if (particle.elapsed > releaseTime || m_system.allAtOnce) {
                particle.lastTime = elapsedTime;
                particle.elapsed = 0.0f;
                particle.released = true;
                PositionParticle(particle, ownerPos2, ownerAngle, rot, ownerPos3, rng);
            }
        }

        if (particle.released) {
            particle.dir = particle.dir + (m_system.gravity * frameSpeed);
            particle.pos = particle.pos + (particle.dir * frameSpeed);
            particle.angle += (particle.angleDir * frameSpeed);
            particle.size += (m_system.growth * frameSpeed);
            const float w = particle.elapsed / particle.lifeTime;
            particle.color = m_system.color0 + (m_system.color1 - m_system.color0) * w;

            // update particle animation if there is any
            if (animated && m_system.animationMode == ParticleSystemDef::kPlayAnimation) {
                particle.currentFrame = ToUint(frameCount * w);
            }

            particle.size = EthMin(particle.size, m_system.maxSize);
            particle.size = EthMax(particle.size, m_system.minSize);

            if (particle.elapsed > particle.lifeTime) {
                particle.repeat++;
                // Reset even on the last repeat (the draws are spent and
                // lastTime moves); the repeat test above then skips it for good.
                if (!Killed()) ResetParticle(particle, ownerPos2, ownerPos3, ownerAngle, rot, nowMs, rng);
            }
        }
    }
    m_finished = !anythingDrawn;
    return !m_finished;
}

// HandleSoundPlayback (:729-779).
SoundDecision ParticleManager::SoundPlayback(const vector2& ownerPos2, const vector2& camera,
                                             const vector2& screenSize, const float frameSpeed) {
    SoundDecision decision;
    const vector2 finalPos = ownerPos2 + vector2(m_system.startPoint);
    if (m_nActiveParticles <= 0) {
        if (IsSoundLooping()) {
            decision.touch = true;
            decision.stop = true;
            decision.volume = m_lastVolume;
            decision.pan = m_lastPan;
        }
    } else {
        // The damp: a moving emitter is quieter (-distance/screen diagonal per
        // frame, floor 0.2), recovering 0.01 per frame. m_v2Move is never
        // (0,0) here: the first Update always counts 0 active particles (the
        // count at :667 precedes the release at :676), so the first call took
        // the branch above and only recorded the position.
        const float move = GsGetDist2(finalPos, m_move);
        const float screenDiagonal = GsGetDist2(vector2(0.0f), screenSize);
        const float moveVolume = move / screenDiagonal;
        m_soundVolume -= moveVolume;
        m_soundVolume = EthMax(m_soundVolume, 0.2f);
        m_soundVolume = EthMin(m_soundVolume, 1.0f);

        const float screenWidth = screenSize.x;
        const float pan = ((EthMax(EthMin(finalPos.x - camera.x, screenWidth), 0.0f) / screenWidth) - 0.5f) * 2;
        decision.touch = true;
        decision.pan = pan;

        // The looping state of the PREVIOUS frame decides the ratio, as the
        // flag is only updated below.
        float volume = 1.0f;
        if (m_isSoundLooping) {
            volume = static_cast<float>(m_nActiveParticles) / static_cast<float>(m_system.nParticles);
        }
        decision.volume = m_soundVolume * volume * m_entityVolume * m_generalVolume;

        // :761-772. The sample half of the test, (!IsPlaying() || !GetLoop()),
        // is the bank's to evaluate when it applies `loop`; the flag is set
        // here whenever the repeat half holds, which differs from 0.7.12 only
        // if another system already had the shared sample looping.
        if (m_system.repeat >= kMinimumParticleRepeatsToLoopSound || m_system.repeat <= 0) {
            m_isSoundLooping = true;
            decision.loop = !m_isSoundStopped;
        }
        if (m_system.repeat < kMinimumParticleRepeatsToLoopSound && m_system.repeat > 0) m_isSoundLooping = false;

        m_soundVolume += 0.01f * frameSpeed;
        m_lastVolume = decision.volume;
        m_lastPan = decision.pan;
    }
    m_move = finalPos;
    return decision;
}

SoundDecision ParticleManager::SoundPlayback(const vector2& ownerScreenPos, const vector2& screenSize,
                                             const float frameSpeed) {
    return SoundPlayback(ownerScreenPos, vector2(0.0f), screenSize, frameSpeed);
}

// Play (:786-800). The sample restart is the caller's (:788-789).
void ParticleManager::Play(const vector2& ownerPos2, const vector3& ownerPos3, const float ownerAngle,
                           const float nowMs, Random& rng) {
    const Rotation rot = RotateZ(ownerAngle);
    m_finished = false;
    for (Particle& particle : m_particles) {
        particle.repeat = 0;
        particle.released = false;
        ResetParticle(particle, ownerPos2, ownerPos3, ownerAngle, rot, nowMs, rng);
    }
}

void ParticleManager::Kill(const bool kill) { m_killed = kill; }

// ETHParticleManager::MirrorX (:939-947) over ETH_PARTICLE_SYSTEM::MirrorX (:81-89).
void ParticleManager::MirrorX(const bool mirrorGravity) {
    if (mirrorGravity) m_system.gravity.x *= -1;
    m_system.startPoint.x *= -1;
    m_system.direction.x *= -1;
    m_system.randomizeDir.x *= -1;
    m_system.randStartPoint.x *= -1;
    for (Particle& particle : m_particles) {
        particle.dir.x *= -1;
        particle.pos.x *= -1;
    }
}

// MirrorY (:949-957) over ETH_PARTICLE_SYSTEM::MirrorY (:91-99).
void ParticleManager::MirrorY(const bool mirrorGravity) {
    if (mirrorGravity) m_system.gravity.y *= -1;
    m_system.startPoint.y *= -1;
    m_system.direction.y *= -1;
    m_system.randomizeDir.y *= -1;
    m_system.randStartPoint.y *= -1;
    for (Particle& particle : m_particles) {
        particle.dir.y *= -1;
        particle.pos.y *= -1;
    }
}

// ScaleParticleSystem (:934-937) over ETH_PARTICLE_SYSTEM::Scale (:66-79).
void ParticleManager::Scale(const float scale) {
    m_system.boundingSphere *= scale;
    m_system.gravity *= scale;
    m_system.direction *= scale;
    m_system.randomizeDir *= scale;
    // v3StartPoint is not scaled: the line is commented out in 0.7.12 (:72).
    m_system.randStartPoint *= scale;
    m_system.size *= scale;
    m_system.randomizeSize *= scale;
    m_system.growth *= scale;
    m_system.minSize *= scale;
    m_system.maxSize *= scale;
}

// DrawParticleSystem (:821-907), minus the device calls.
void ParticleManager::CollectDraws(const int ownerId, const vector3& ambient, const float minHeight,
                                   const float maxHeight, const ENTITY_TYPE ownerType, const vector2& zAxisDirection,
                                   const float ownerDepth, std::vector<ParticleDraw>& out) const {
    // :832-839: sorted when the device's alpha mode on entry is PIXEL, which it
    // always is in the particle pass (BeginSpriteScene sets PIXEL and every
    // ambient/light pass and every system restores what it found).
    BubbleSort(m_particles);

    // :854-860. Luminance lifts a blended particle over a dark scene; additive
    // and multiplicative particles ignore the scene's ambient entirely.
    vector3 finalAmbient(1.0f);
    if (m_system.alphaMode == AM_PIXEL || m_system.alphaMode == AM_ALPHA_TEST) {
        finalAmbient.x = EthMin(m_system.luminance.x + ambient.x, 1.0f);
        finalAmbient.y = EthMin(m_system.luminance.y + ambient.y, 1.0f);
        finalAmbient.z = EthMin(m_system.luminance.z + ambient.z, 1.0f);
    }

    const bool animated = HasSpriteCut(m_system);
    const uint lastFrame = static_cast<uint>(m_system.spriteCutX * m_system.spriteCutY - 1);

    for (const Particle& particle : m_particles) {
        if (m_system.repeat > 0 && particle.repeat >= m_system.repeat) continue;
        if (particle.size <= 0.0f || !particle.released) continue;
        if (Killed() && particle.elapsed > particle.lifeTime) continue;

        ParticleDraw draw;
        draw.ownerId = ownerId;
        draw.bitmap = m_system.bitmap;
        draw.alphaMode = m_system.alphaMode;

        // :862-866: the vertex colour is bytes, truncated.
        const uint8 a = ToByte(particle.color.w * 255.0f);
        const uint8 r = ToByte(particle.color.x * finalAmbient.x * 255.0f);
        const uint8 g = ToByte(particle.color.y * finalAmbient.y * 255.0f);
        const uint8 b = ToByte(particle.color.z * finalAmbient.z * 255.0f);
        draw.color = glm::vec4(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b),
                               static_cast<float>(a)) / 255.0f;

        // :869: ToScreenPos with the SYSTEM's start z; the owner's z is already
        // in the position through the ownerPos2 Update was given.
        draw.position = particle.pos + zAxisDirection * m_system.startPoint.z;

        // :871-884: owner z + start z; a vertical owner's particles also come
        // nearer as they rise above where they started, plus the fixed shift.
        if (ownerType != ET_LAYERABLE) {
            float offsetYZ = particle.startPoint.z;
            if (ownerType == ET_VERTICAL) {
                offsetYZ += (particle.startPoint.y - particle.pos.y) + kParticleDepthShift;
            }
            draw.depth = (offsetYZ - minHeight) / (maxHeight - minHeight);
        } else {
            draw.depth = ownerDepth;
        }

        draw.spriteCutX = m_system.spriteCutX;
        draw.spriteCutY = m_system.spriteCutY;
        // :887-893. A PLAY_ANIMATION frame reaches cutX*cutY only when the age
        // equals the life exactly; GS2D's SetRect refused that index and the
        // bitmap kept its previous rect. The last cell stands in for it.
        if (animated) draw.frame = (particle.currentFrame > lastFrame) ? lastFrame : particle.currentFrame;

        draw.size = particle.size;
        draw.angle = particle.angle;
        // Identity for E8 (Snapshot.hpp). lastTime is written only when a life
        // begins - ResetParticle and the release - so it is constant for one
        // life and differs for the next.
        draw.particleId = particle.id;
        draw.lifeStartMs = particle.lastTime;
        out.push_back(draw);
    }
}

} // namespace Penumbra::Eth
