#pragma once

// The runtime's one random stream: the scripts' rand()/randF() and every
// particle system draw from it, as they drew from GameSpaceLib's single MT19937
// (DLL@0x10004bd0). Seeded by the Machine: a fixed seed under --fixed-step and
// in the suites (so runs reproduce), a clock seed otherwise.
//
// Ranges are INCLUSIVE, as the original's were: rand(n) is 0..n, randF(x) is
// 0..x (docs/spec/30-ethanon-runtime.md §2.11).

#include <cstdint>
#include <random>

namespace Penumbra::Eth {

class Random {
public:
    explicit Random(std::uint32_t seed = 5489u) : m_mt(seed) {}

    void Seed(std::uint32_t seed) { m_mt.seed(seed); }

    // [0, max] inclusive; max <= 0 returns 0.
    int RandI(int max) {
        if (max <= 0) return 0;
        return static_cast<int>(m_mt() % static_cast<std::uint32_t>(max + 1));
    }
    // [min, max] inclusive.
    int RandI(int min, int max) { return max <= min ? min : min + RandI(max - min); }

    // [0, max] (max may be negative: then [max, 0]).
    float RandF(float max) { return Unit() * max; }
    // [min, max].
    float RandF(float min, float max) { return min + Unit() * (max - min); }

private:
    // MT19937's own [0,1] real, as gsRandF computed it: a 32-bit draw over 2^32-1.
    float Unit() { return static_cast<float>(static_cast<double>(m_mt()) / 4294967295.0); }

    std::mt19937 m_mt;
};

} // namespace Penumbra::Eth
