// The 0.7.12 particle simulation (game/eth/Particles.cpp) against what the
// original's code says it must do. This is gameplay: a sprite-less entity whose
// systems are all finite is deleted when they finish, so a sword's hit box,
// a fireball's flight and the light spell last exactly as long as their
// particles (docs/spec/11-logic-player-combat.md).
//
// The definitions are pasted from extracted/app/entities/*.ent (no files are
// read), every field set by hand: Defs.hpp's defaults are not 0.7.12's (maxSize
// 0 would clamp every particle to nothing).
//
// The clock is the Machine's: tick k is at floor(k*1000/60) ms, one Update per
// tick with frameSpeed 1. A system created at tick k0 (in that frame's
// callbacks) gets its first Update at tick k0+1.
//
// Particles are always looked up by id: CollectDraws sorts them in place, as
// 0.7.12's draw did.

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "TestHarness.hpp"
#include "eth/Particles.hpp"

using namespace Penumbra::Eth;

namespace {

float TickMs(const int k) { return static_cast<float>((static_cast<std::int64_t>(k) * 1000) / 60); }

// sword0.ent (the player's sword; swords.as:81 spawns it, doDamage.as uses its box).
ParticleSystemDef Sword0() {
    ParticleSystemDef s;
    s.bitmap = "sword.png";
    s.soundEffect = "sword01.mp3";
    s.nParticles = 16;
    s.allAtOnce = false;
    s.alphaMode = AM_ADD;
    s.repeat = 1;
    s.animationMode = 1;
    s.boundingSphere = 409.6f;
    s.lifeTime = 150.0f;
    s.randomizeLifeTime = 0.0f;
    s.angleDir = 0.0f;
    s.randAngle = 0.0f;
    s.size = 66.4f;
    s.randomizeSize = 0.0f;
    s.growth = 1.2f;
    s.minSize = 0.0f;
    s.maxSize = 800.0f;
    s.angleStart = 0.0f;
    s.randAngleStart = 0.0f;
    s.gravity = vector2(-3.04f, 0.0f);
    s.direction = vector2(12.56f, 2.16f);
    s.randomizeDir = vector2(3.92f, 0.4f);
    s.spriteCutX = 1;
    s.spriteCutY = 1;
    s.startPoint = vector3(-13.0f, 0.0f, 0.0f);
    s.randStartPoint = vector2(0.0f, 0.0f);
    s.color0 = glm::vec4(0.5f, 0.5f, 1.0f, 1.0f);
    s.color1 = glm::vec4(0.5f, 0.5f, 1.0f, 1.0f);
    s.luminance = vector3(1.0f, 1.0f, 1.0f);
    return s;
}

// fire_ball.ent (playerInput.as:428, and the impy's and paladin's).
ParticleSystemDef FireBall() {
    ParticleSystemDef s;
    s.bitmap = "explosion.JPG";
    s.soundEffect = "";
    s.nParticles = 19;
    s.allAtOnce = false;
    s.alphaMode = AM_ADD;
    s.repeat = 5;
    s.animationMode = 1;
    s.boundingSphere = 64.0f;
    s.lifeTime = 250.0f;
    s.randomizeLifeTime = 150.0f;
    s.angleDir = 0.0f;
    s.randAngle = 20.0f;
    s.size = 20.0f;
    s.randomizeSize = 0.0f;
    s.growth = -1.5f;
    s.minSize = 2.0f;
    s.maxSize = 1000.0f;
    s.angleStart = 0.0f;
    s.randAngleStart = 360.0f;
    s.gravity = vector2(0.0f, 0.0f);
    s.direction = vector2(0.0f, 0.0f);
    s.randomizeDir = vector2(0.8f, 0.8f);
    s.spriteCutX = 1;
    s.spriteCutY = 1;
    s.startPoint = vector3(0.0f, 0.0f, 0.0f);
    s.randStartPoint = vector2(3.0f, 0.0f);
    s.color0 = glm::vec4(1.0f, 0.3f, 0.0f, 1.0f);
    s.color1 = glm::vec4(1.0f, 0.8f, 0.8f, 1.0f);
    s.luminance = vector3(1.0f, 1.0f, 1.0f);
    return s;
}

// light_spell.ent (playerInput.as:413).
ParticleSystemDef LightSpell() {
    ParticleSystemDef s;
    s.bitmap = "flash.bmp";
    s.soundEffect = "";
    s.nParticles = 15;
    s.allAtOnce = false;
    s.alphaMode = AM_ADD;
    s.repeat = 36;
    s.animationMode = 1;
    s.boundingSphere = 409.6f;
    s.lifeTime = 600.0f;
    s.randomizeLifeTime = 500.0f;
    s.angleDir = 0.0f;
    s.randAngle = 0.0f;
    s.size = 35.2f;
    s.randomizeSize = 0.0f;
    s.growth = -0.96f;
    s.minSize = 0.0f;
    s.maxSize = 800.0f;
    s.angleStart = 0.0f;
    s.randAngleStart = 235.0f;
    s.gravity = vector2(0.0f, 0.0f);
    s.direction = vector2(0.0f, 0.0f);
    s.randomizeDir = vector2(1.6f, 1.6f);
    s.spriteCutX = 1;
    s.spriteCutY = 1;
    s.startPoint = vector3(0.0f, 0.0f, 0.0f);
    s.randStartPoint = vector2(0.0f, 0.0f);
    s.color0 = glm::vec4(0.6f, 0.7f, 1.0f, 1.0f);
    s.color1 = glm::vec4(0.6f, 0.7f, 1.0f, 0.0f);
    s.luminance = vector3(1.0f, 1.0f, 1.0f);
    return s;
}

// torch.ent (placed 10/1/9 in the levels).
ParticleSystemDef Torch() {
    ParticleSystemDef s;
    s.bitmap = "explosion.JPG";
    s.soundEffect = "";
    s.nParticles = 18;
    s.allAtOnce = false;
    s.alphaMode = AM_ADD;
    s.repeat = 0;
    s.animationMode = 1;
    s.boundingSphere = 76.8f;
    s.lifeTime = 850.0f;
    s.randomizeLifeTime = 700.0f;
    s.angleDir = 0.0f;
    s.randAngle = 20.0f;
    s.size = 24.0f;
    s.randomizeSize = 0.0f;
    s.growth = -0.6f;
    s.minSize = 0.0f;
    s.maxSize = 1200.0f;
    s.angleStart = 0.0f;
    s.randAngleStart = 360.0f;
    s.gravity = vector2(0.0f, -0.048f);
    s.direction = vector2(0.0f, 0.0f);
    s.randomizeDir = vector2(0.36f, 0.96f);
    s.spriteCutX = 1;
    s.spriteCutY = 1;
    s.startPoint = vector3(0.0f, -8.0f, 8.0f);
    s.randStartPoint = vector2(3.6f, 0.0f);
    s.color0 = glm::vec4(1.0f, 0.5f, 0.1f, 1.0f);
    s.color1 = glm::vec4(1.0f, 0.8f, 0.8f, 1.0f);
    s.luminance = vector3(1.0f, 1.0f, 1.0f);
    return s;
}

const Particle* ById(const ParticleManager& m, const int id) {
    for (const Particle& p : m.Particles()) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

// The oracle. Before every Update it predicts, from each particle's own state
// (its stamp, its drawn life) and the rules of ETHParticleManager.cpp:660-721,
// what must happen on this tick - release when age > (life + randLife) * id / n,
// death when age > life (then repeat+1 and, unless killed, a new stamp even on
// the last repeat), finished when every particle had used its repeats before
// the tick - and checks it after. The finish time is then not estimated but
// derived from the lifetimes the RNG actually drew.
struct OracleRun {
    int finishTicks = -1;       // Updates from creation to the one that reported finished
    float finishMs = 0.0f;      // its clock minus the creation clock
    int mismatches = 0;
    int releasedAfterKill = 0;  // first releases on ticks where the system was already killed
};

OracleRun RunWithOracle(const ParticleSystemDef& def, const std::uint32_t seed, const int k0, const int maxTicks,
                        const int killAfterTicks = -1) {
    OracleRun run;
    Random rng(seed);
    const vector2 owner(640.0f, 360.0f);
    const vector3 owner3(owner, 0.0f);
    ParticleManager m(def, owner, owner3, 0.0f, 1.0f, TickMs(k0), rng);
    const int n = def.nParticles;

    for (int tick = 1; tick <= maxTicks; ++tick) {
        if (tick == killAfterTicks) m.Kill(true);
        const float now = TickMs(k0 + tick);

        struct Expect {
            int repeat = 0;
            bool released = false;
            bool releasing = false;
            bool restamp = false;
            float lastTime = 0.0f;
        };
        std::vector<Expect> expect(static_cast<std::size_t>(n));
        bool expectFinished = true;
        for (const Particle& p : m.Particles()) {
            Expect e;
            e.repeat = p.repeat;
            e.released = p.released;
            e.lastTime = p.lastTime;
            if (!(def.repeat > 0 && p.repeat >= def.repeat)) {
                expectFinished = false;
                const float age = now - p.lastTime;
                if (!p.released) {
                    const float releaseTime = (def.lifeTime + def.randomizeLifeTime) *
                                              (static_cast<float>(p.id) / static_cast<float>(n));
                    if (age > releaseTime) {
                        e.released = true;
                        e.releasing = true;
                        e.restamp = true;   // age 0 after release: every life here is > 0
                    }
                } else if (age > p.lifeTime) {
                    e.repeat = p.repeat + 1;
                    e.restamp = !m.Killed();
                }
            }
            expect[static_cast<std::size_t>(p.id)] = e;
        }

        const bool alive = m.Update(owner, owner3, 0.0f, now, 1.0f, rng);

        for (const Particle& p : m.Particles()) {
            const Expect& e = expect[static_cast<std::size_t>(p.id)];
            const float stamp = e.restamp ? now : e.lastTime;
            if (p.repeat != e.repeat || p.released != e.released || p.lastTime != stamp) {
                if (run.mismatches < 5) {
                    std::printf("    oracle: tick %d particle %d: repeat %d/%d released %d/%d stamp %.1f/%.1f\n", tick,
                                p.id, p.repeat, e.repeat, p.released ? 1 : 0, e.released ? 1 : 0, p.lastTime, stamp);
                }
                ++run.mismatches;
            }
            if (m.Killed() && e.releasing && p.released) ++run.releasedAfterKill;
        }
        if (alive != !expectFinished || m.Finished() != expectFinished) {
            if (run.mismatches < 5) std::printf("    oracle: tick %d finished %d, expected %d\n", tick, m.Finished() ? 1 : 0,
                                                expectFinished ? 1 : 0);
            ++run.mismatches;
        }
        if (expectFinished) {
            run.finishTicks = tick;
            run.finishMs = now - TickMs(k0);
            return run;
        }
    }
    return run;
}

// (b) sword0: exactly 20 Updates, 333 ms. The spec's ~291 ms is the continuous
// figure: the last particle (id 15) waits 150*15/16 = 140.625 ms, then lives
// 150 ms = 290.6 ms. On the 60 Hz clock it is released on the first tick past
// 140.6 ms (tick 9, 150 ms), dies on the first tick where its age exceeds 150
// (tick 18 is age exactly 150, not more: tick 19, 316.7 ms), and Finished() is
// reported by the NEXT Update, which sees every repeat used (tick 20, 333 ms).
// The entity is deleted in that frame, so its hit box lived 20 frames.
void TestSwordFinishes() {
    for (int k0 = 0; k0 < 3; ++k0) {
        const OracleRun run = RunWithOracle(Sword0(), 5489u, k0, 200);
        CHECK_EQ(run.mismatches, 0);
        CHECK_EQ(run.finishTicks, 20);
        CHECK_MSG(run.finishMs >= 333.0f && run.finishMs <= 334.0f, "sword0 finished at " + std::to_string(run.finishMs));
    }

    // The tick-by-tick story of the last particle, at phase 0.
    Random rng(5489u);
    const vector2 owner(100.0f, 100.0f);
    ParticleManager m(Sword0(), owner, vector3(owner, 0.0f), 0.0f, 0.7f, TickMs(0), rng);
    int releasedAt = -1;
    int diedAt = -1;
    int finishedAt = -1;
    for (int k = 1; k <= 30 && finishedAt < 0; ++k) {
        const bool alive = m.Update(owner, vector3(owner, 0.0f), 0.0f, TickMs(k), 1.0f, rng);
        const Particle* last = ById(m, 15);
        if (releasedAt < 0 && last->released) releasedAt = k;
        if (diedAt < 0 && last->repeat == 1) diedAt = k;
        if (!alive) finishedAt = k;
    }
    CHECK_EQ(releasedAt, 9);
    CHECK_EQ(diedAt, 19);
    CHECK_EQ(finishedAt, 20);
    CHECK(m.Finished());
}

// (c) fire_ball (repeat 5) and light_spell (repeat 36). The exact check is the
// oracle; around it, the bounds that follow from the definitions alone:
//  - particle i is released on the first tick past (life+randLife)*i/n ms from
//    creation (fire_ball 378.9 ms for i=18, light_spell 1026.7 ms for i=14);
//  - each life L is uniform in life +/- randLife/2 (fire_ball 175..325,
//    light_spell 350..850) and ends on the first tick past L, so it lasts
//    (L, L+17] ms; Finished() comes one tick after the last death;
//  - so fire_ball ends in (378.9 + 5*175 + 16, 395.6 + 5*342 + 17] = (1270, 2123]
//    and light_spell in (1026.7 + 36*350 + 16, 1043.3 + 36*867 + 17] = (13642, 32273].
// The distribution (a Python transcription of the same rules over 200 seeds):
// fire_ball 1773 +/- 59 ms (1633..1900), light_spell 24028 +/- 459 ms
// (22966..25400). The spec's ~1.6-1.8 s holds; its ~23 s is low: ~24 s. The
// mean over 32 seeds must sit in a window of about 6 standard errors.
void TestFiniteSystemsFinish() {
    struct Case {
        const char* name;
        ParticleSystemDef def;
        float lo, hi;           // hard bounds, ms
        float meanLo, meanHi;   // 32-seed mean window, ms
        int maxTicks;
        float twin5489;         // what the transcription gives for seed 5489 (printed, not asserted)
    };
    const Case cases[] = {
        {"fire_ball", FireBall(), 1270.0f, 2124.0f, 1713.0f, 1833.0f, 200, 1750.0f},
        {"light_spell", LightSpell(), 13642.0f, 32274.0f, 23578.0f, 24478.0f, 2400, 24583.0f},
    };
    for (const Case& c : cases) {
        const OracleRun first = RunWithOracle(c.def, 5489u, 0, c.maxTicks);
        std::printf("  %s, seed 5489: finished after %d updates, %.0f ms (the transcription says %.0f)\n", c.name,
                    first.finishTicks, first.finishMs, c.twin5489);

        double sum = 0.0;
        int mismatches = 0;
        int outOfBounds = 0;
        int unfinished = 0;
        for (std::uint32_t seed = 1; seed <= 32; ++seed) {
            for (int k0 = 0; k0 < 2; ++k0) {
                const OracleRun run = RunWithOracle(c.def, seed, static_cast<int>(seed % 3) + k0, c.maxTicks);
                mismatches += run.mismatches;
                if (run.finishTicks < 0) {
                    ++unfinished;
                    continue;
                }
                if (!(run.finishMs > c.lo && run.finishMs <= c.hi)) ++outOfBounds;
                if (k0 == 0) sum += run.finishMs;
            }
        }
        const double mean = sum / 32.0;
        std::printf("  %s: mean finish over 32 seeds %.1f ms\n", c.name, mean);
        CHECK_EQ(mismatches, 0);
        CHECK_EQ(unfinished, 0);
        CHECK_EQ(outOfBounds, 0);
        CHECK_MSG(mean >= c.meanLo && mean <= c.meanHi, std::string(c.name) + " mean " + std::to_string(mean));
    }
}

// (a) torch: endless. After the stagger (the last of 18 waits 1550*17/18 =
// 1463.9 ms: tick 88) every particle is released and recycled forever, and the
// system never finishes. NumActiveParticles counts only particles with size
// > 0: a torch particle shrinks 0.6 px/frame from 24, so it is empty after 40
// frames (667 ms) of a 500..1200 ms life - about 75% are active on average
// (13.6 of 18 in the transcription, 7..18), not all 18.
void TestTorchIsEndless() {
    Random rng(5489u);
    const vector2 owner(1000.0f, 400.0f);
    const vector3 owner3(1000.0f, 400.0f, -40.0f);
    ParticleManager m(Torch(), owner, owner3, 0.0f, 1.0f, TickMs(0), rng);
    CHECK(m.Endless());
    bool everFinished = false;
    int allReleasedAt = -1;
    double activeSum = 0.0;
    int samples = 0;
    int minActive = 1000;
    int maxActive = -1;
    for (int k = 1; k <= 3600; ++k) {
        if (!m.Update(owner, owner3, 0.0f, TickMs(k), 1.0f, rng)) everFinished = true;
        if (allReleasedAt < 0) {
            bool all = true;
            for (const Particle& p : m.Particles()) all = all && p.released;
            if (all) allReleasedAt = k;
        }
        if (k > 120) {
            const int active = m.NumActiveParticles();
            activeSum += active;
            ++samples;
            minActive = active < minActive ? active : minActive;
            maxActive = active > maxActive ? active : maxActive;
        }
    }
    const double meanActive = activeSum / samples;
    std::printf("  torch: all released at tick %d; active after warm-up %d..%d, mean %.2f of 18\n", allReleasedAt,
                minActive, maxActive, meanActive);
    CHECK(!everFinished);
    CHECK(!m.Finished());
    CHECK_EQ(allReleasedAt, 88);
    CHECK(maxActive <= 18);
    CHECK(minActive >= 1);
    CHECK_MSG(meanActive >= 12.5 && meanActive <= 14.7, "torch mean active " + std::to_string(meanActive));
    // Particles are recycled, not lost: every one still released, still counted.
    int released = 0;
    for (const Particle& p : m.Particles()) released += p.released ? 1 : 0;
    CHECK_EQ(released, 18);
}

// (d) Kill. On a warm endless torch: nothing is reset any more, the active
// count falls to 0 within one maximum life (1200 ms), nothing is drawn, and the
// system still never finishes (its repeat is 0, so no particle is ever skipped).
// On a fire_ball killed in its first wave (tick 5, 83 ms: ids 0..3 are out),
// the rest are STILL released on their stagger - ETHParticleManager.cpp:676-689
// has no Killed() test - but none lives twice, and the system finishes a few
// ticks after the last first life ends, as each dead particle's repeat climbs by
// one per frame.
void TestKill() {
    Random rng(77u);
    const vector2 owner(300.0f, 300.0f);
    const vector3 owner3(owner, 0.0f);
    ParticleManager torch(Torch(), owner, owner3, 0.0f, 1.0f, TickMs(0), rng);
    int k = 1;
    for (; k <= 200; ++k) torch.Update(owner, owner3, 0.0f, TickMs(k), 1.0f, rng);
    CHECK(torch.NumActiveParticles() > 0);

    std::vector<float> stamps(18, 0.0f);
    for (const Particle& p : torch.Particles()) stamps[static_cast<std::size_t>(p.id)] = p.lastTime;
    torch.Kill(true);
    CHECK(torch.Killed());
    int restamped = 0;
    bool finished = false;
    for (int end = k + 90; k < end; ++k) {
        if (!torch.Update(owner, owner3, 0.0f, TickMs(k), 1.0f, rng)) finished = true;
        for (const Particle& p : torch.Particles()) {
            if (p.lastTime != stamps[static_cast<std::size_t>(p.id)]) ++restamped;
        }
    }
    CHECK_EQ(restamped, 0);
    CHECK(!finished);
    CHECK_EQ(torch.NumActiveParticles(), 0);
    std::vector<ParticleDraw> draws;
    torch.CollectDraws(1, vector3(1.0f), 0.0f, 768.0f, ET_HORIZONTAL, vector2(0.0f), 0.0f, draws);
    CHECK_EQ(static_cast<int>(draws.size()), 0);

    // Killed mid-wave: the oracle checks every release and that no death is
    // followed by a new life.
    const OracleRun run = RunWithOracle(FireBall(), 5489u, 0, 300, 5);
    CHECK_EQ(run.mismatches, 0);
    CHECK(run.finishTicks > 23);
    CHECK_MSG(run.releasedAfterKill == 15, "released after the kill: " + std::to_string(run.releasedAfterKill));
    // One life (175..325 ms, +17 ms of tick) after the last release (tick 23,
    // 383 ms), then repeat 1 -> 5 over four more ticks, then the finishing tick.
    CHECK_MSG(run.finishMs > 383.0f + 175.0f && run.finishMs <= 383.0f + 342.0f + 5 * 17.0f,
              "killed fire_ball finished at " + std::to_string(run.finishMs));
}

// (e) MirrorX(true), as swords.as:85 does right after AddEntity: the system's
// start point, direction, randomisation and gravity flip on x, and so does
// every particle's already drawn direction. The first particle is then released
// at owner + (13, 0) and moves left.
void TestMirrorX() {
    Random rng(2024u);
    const vector2 owner(500.0f, 300.0f);
    const vector3 owner3(owner, 18.0f);
    ParticleManager m(Sword0(), owner, owner3, 0.0f, 0.7f, TickMs(0), rng);
    std::vector<float> dirX(16, 0.0f);
    for (const Particle& p : m.Particles()) dirX[static_cast<std::size_t>(p.id)] = p.dir.x;

    m.MirrorX(true);
    CHECK_EQ(m.System().startPoint.x, 13.0f);
    CHECK_EQ(m.System().direction.x, -12.56f);
    CHECK_EQ(m.System().randomizeDir.x, -3.92f);
    CHECK_EQ(m.System().gravity.x, 3.04f);
    CHECK_EQ(m.System().direction.y, 2.16f);
    int flipped = 0;
    for (const Particle& p : m.Particles()) flipped += (p.dir.x == -dirX[static_cast<std::size_t>(p.id)]) ? 1 : 0;
    CHECK_EQ(flipped, 16);

    m.Update(owner, owner3, 0.0f, TickMs(1), 1.0f, rng);
    const Particle* first = ById(m, 0);
    CHECK(first->released);
    // The release placed it at owner + start, then the same Update moved it by
    // its (gravity-bent) direction once.
    CHECK_NEAR(first->pos.x - first->dir.x, owner.x + 13.0f);
    CHECK_NEAR(first->pos.y - first->dir.y, owner.y);
    CHECK(first->dir.x < 0.0f);
    CHECK_NEAR(first->startPoint.x, owner.x + 13.0f);
    CHECK_NEAR(first->startPoint.z, 18.0f);

    // Without gravity mirroring, and mirrored back.
    Random rng2(2024u);
    ParticleManager plain(Sword0(), owner, owner3, 0.0f, 0.7f, TickMs(0), rng2);
    plain.MirrorX(false);
    CHECK_EQ(plain.System().gravity.x, -3.04f);
    plain.MirrorX(false);
    CHECK_EQ(plain.System().startPoint.x, -13.0f);
}

// (f) CollectDraws: nothing before the first Update, then exactly the released,
// non-empty particles still inside their repeats, in the sorted order, with
// 0.7.12's colour bytes and depth; nothing once finished.
struct Filter {
    static bool Drawn(const ParticleManager& m, const Particle& p) {
        const ParticleSystemDef& s = m.System();
        if (s.repeat > 0 && p.repeat >= s.repeat) return false;
        if (p.size <= 0.0f || !p.released) return false;
        if (m.Killed() && p.elapsed > p.lifeTime) return false;
        return true;
    }
};

float Byte(const float v) { return static_cast<float>(static_cast<int>(v)) / 255.0f; }

void TestCollectDraws() {
    Random rng(5489u);
    const vector2 owner(200.0f, 150.0f);
    const vector3 owner3(owner, 10.0f);
    ParticleManager m(Sword0(), owner, owner3, 0.0f, 0.7f, TickMs(0), rng);
    std::vector<ParticleDraw> draws;
    m.CollectDraws(7, vector3(0.2f), -100.0f, 768.0f, ET_HORIZONTAL, vector2(0.0f), 0.0f, draws);
    CHECK_EQ(static_cast<int>(draws.size()), 0);

    int badDraw = 0;
    int badCount = 0;
    int badOrder = 0;
    bool finished = false;
    for (int k = 1; k <= 25; ++k) {
        finished = !m.Update(owner, owner3, 0.0f, TickMs(k), 1.0f, rng);
        draws.clear();
        m.CollectDraws(7, vector3(0.2f), -100.0f, 768.0f, ET_HORIZONTAL, vector2(0.0f), 0.0f, draws);
        std::size_t i = 0;
        float lastOffset = -1e30f;
        for (const Particle& p : m.Particles()) {
            const float offset = p.startPoint.y - p.pos.y;
            if (offset < lastOffset) ++badOrder;
            lastOffset = offset;
            if (!Filter::Drawn(m, p)) continue;
            if (i >= draws.size()) {
                ++badCount;
                break;
            }
            const ParticleDraw& d = draws[i++];
            // ADD ignores the ambient: (0.5, 0.5, 1, 1) -> bytes 127, 127, 255, 255.
            const bool ok = d.ownerId == 7 && d.bitmap == "sword.png" && d.alphaMode == AM_ADD &&
                            d.position == p.pos && d.size == p.size && d.angle == p.angle &&
                            d.color == glm::vec4(127.0f / 255.0f, 127.0f / 255.0f, 1.0f, 1.0f) &&
                            test::nearly(d.depth, (10.0f + 100.0f) / 868.0f, 1e-6f) && d.frame == 0;
            if (!ok) ++badDraw;
        }
        if (i != draws.size()) ++badCount;
        // Particle i waits 9.375*i ms: ticks 1, 2, 3 (16, 33, 50 ms) release
        // ids 0-1, 2-3, 4-5, each drawn on the tick it is released.
        if (k == 1) CHECK_EQ(static_cast<int>(draws.size()), 2);
        if (k == 3) CHECK_EQ(static_cast<int>(draws.size()), 6);
        if (finished) break;
    }
    CHECK_EQ(badDraw, 0);
    CHECK_EQ(badCount, 0);
    CHECK_EQ(badOrder, 0);
    CHECK(finished);
    draws.clear();
    m.CollectDraws(7, vector3(0.2f), -100.0f, 768.0f, ET_HORIZONTAL, vector2(0.0f), 0.0f, draws);
    CHECK_EQ(static_cast<int>(draws.size()), 0);
}

// The colour and depth rules on the other owner types: a PIXEL system is lit
// by min(luminance + ambient, 1); a vertical owner's particle comes nearer by
// its rise plus ETH_PARTICLE_DEPTH_SHIFT; a layerable owner's is the owner's.
void TestDrawRules() {
    ParticleSystemDef def = Torch();
    def.alphaMode = AM_PIXEL;
    def.luminance = vector3(0.0f, 0.25f, 1.0f);
    Random rng(99u);
    const vector2 owner(64.0f, 64.0f);
    const vector3 owner3(owner, -40.0f);
    ParticleManager m(def, owner, owner3, 0.0f, 1.0f, TickMs(0), rng);
    for (int k = 1; k <= 30; ++k) m.Update(owner, owner3, 0.0f, TickMs(k), 1.0f, rng);

    const vector3 ambient(0.5f, 0.5f, 0.5f);
    const vector2 zAxis(0.0f, -1.0f);
    std::vector<ParticleDraw> vertical;
    m.CollectDraws(3, ambient, -500.0f, 900.0f, ET_VERTICAL, zAxis, 0.25f, vertical);
    std::vector<ParticleDraw> layerable;
    m.CollectDraws(3, ambient, -500.0f, 900.0f, ET_LAYERABLE, zAxis, 0.25f, layerable);
    CHECK(!vertical.empty());
    CHECK_EQ(vertical.size(), layerable.size());

    int bad = 0;
    std::size_t i = 0;
    for (const Particle& p : m.Particles()) {
        if (!Filter::Drawn(m, p) || i >= vertical.size()) continue;
        const ParticleDraw& v = vertical[i];
        const ParticleDraw& l = layerable[i];
        ++i;
        const glm::vec4 lit(Byte(p.color.x * 0.5f * 255.0f), Byte(p.color.y * 0.75f * 255.0f),
                            Byte(p.color.z * 1.0f * 255.0f), Byte(p.color.w * 255.0f));
        const float rise = p.startPoint.y - p.pos.y;
        const float depth = ((-40.0f + 8.0f) + (rise + 10.0f) - -500.0f) / (900.0f - -500.0f);
        if (v.color != lit || l.color != lit) ++bad;
        if (!test::nearly(v.depth, depth, 1e-6f)) ++bad;
        if (l.depth != 0.25f) ++bad;
        // ToScreenPos with the system's start z (8) on the menus' (0,-1) axis.
        if (v.position != p.pos + zAxis * 8.0f) ++bad;
    }
    CHECK_EQ(bad, 0);
}

// HandleSoundPlayback on sword0's sample (repeat 1: never loops), called after
// every Update as 0.7.12 did. The first call always sees 0 active particles -
// the count (ETHParticleManager.cpp:667) precedes the release (:676) - so it
// only records the emitter's position: the never-initialised m_v2Move cannot
// damp a spawn. Then the volume is soundVolume x the damp, which falls by
// distance moved / screen diagonal and recovers 0.01 a frame; the pan is the
// emitter's x across the screen.
void TestSound() {
    Random rng(5489u);
    vector2 owner(3000.0f, 500.0f);
    const vector2 camera(2500.0f, 200.0f);
    const vector2 screen(1024.0f, 768.0f);
    ParticleManager m(Sword0(), owner, vector3(owner, 8.0f), 0.0f, 0.7f, TickMs(0), rng);
    CHECK(m.HasSoundEffect());

    m.Update(owner, vector3(owner, 8.0f), 0.0f, TickMs(1), 1.0f, rng);
    SoundDecision d = m.SoundPlayback(owner, camera, screen, 1.0f);
    CHECK(!d.touch);
    CHECK_EQ(m.NumActiveParticles(), 0);

    m.Update(owner, vector3(owner, 8.0f), 0.0f, TickMs(2), 1.0f, rng);
    d = m.SoundPlayback(owner, camera, screen, 1.0f);
    CHECK(d.touch);
    CHECK(!d.stop);
    CHECK(!d.loop);
    CHECK_NEAR(d.volume, 0.7f);
    CHECK_NEAR(d.pan, ((3000.0f - 13.0f - 2500.0f) / 1024.0f - 0.5f) * 2.0f);

    // Move 128 px: a tenth of the 1280 px diagonal (through gsSqrt's
    // approximation, hence the tolerance), from a damp of 1.01.
    owner.x += 128.0f;
    m.Update(owner, vector3(owner, 8.0f), 0.0f, TickMs(3), 1.0f, rng);
    d = m.SoundPlayback(owner, camera, screen, 1.0f);
    CHECK_MSG(test::nearly(d.volume, (1.01f - 0.1f) * 0.7f, 2e-3f), "damped volume " + std::to_string(d.volume));
    CHECK(!m.IsSoundLooping());

    // A repeat >= 4 system asks for a loop, is scaled by active/total from the
    // next frame on, and is stopped once nothing is active.
    Random rng2(5u);
    const vector2 at(640.0f, 360.0f);
    ParticleManager spell(LightSpell(), at, vector3(at, 0.0f), 0.0f, 1.0f, TickMs(0), rng2);
    std::vector<SoundDecision> decisions;
    int k = 1;
    for (; k <= 3; ++k) {
        spell.Update(at, vector3(at, 0.0f), 0.0f, TickMs(k), 1.0f, rng2);
        decisions.push_back(spell.SoundPlayback(at, camera, screen, 1.0f));
    }
    // Tick 1 releases id 0 (the next waits 73.3 ms), counted from tick 2.
    CHECK(!decisions[0].touch);
    CHECK(decisions[1].touch && decisions[1].loop);
    CHECK_NEAR(decisions[1].volume, 1.0f);
    CHECK(spell.IsSoundLooping());
    CHECK_NEAR(decisions[2].volume, 1.0f / 15.0f);
    spell.Kill(true);
    for (int end = k + 120; k < end; ++k) {
        spell.Update(at, vector3(at, 0.0f), 0.0f, TickMs(k), 1.0f, rng2);
        d = spell.SoundPlayback(at, camera, screen, 1.0f);
    }
    CHECK_EQ(spell.NumActiveParticles(), 0);
    CHECK(d.touch && d.stop);
}

} // namespace

int main() {
    TestSwordFinishes();
    TestFiniteSystemsFinish();
    TestTorchIsEndless();
    TestKill();
    TestMirrorX();
    TestCollectDraws();
    TestDrawRules();
    TestSound();
    return test::summary("test_pn_particles", 60);
}
