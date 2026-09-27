#pragma once

// One back-to-front order for everything the snapshot draws in the world.
//
// 0.7.12 drew entities in drawHash order and then every particle, all
// depth-tested (LESSEQUAL, z-write on, z = 1 - depth), so what showed at a
// pixel was the nearest piece there, ties going to the one drawn last. The
// engine has no such depth test for blended quads - it sorts them by view
// depth - so the port hands it the z-buffer's answer as one painter's order:
// a stable sort of every sprite, shadow slot and particle by (depth, phase,
// sequence) - sprites and their shadow slots in snapshot order at equal depth,
// particles after the sprites of their depth and in their own order. NOT the
// snapshot's drawHash order: a vertical sprite hashes in raw pixels, so a
// barrel comes after the black layerable covers it lies behind
// (render/DrawOrder.cpp). Halos (depth 1.0, always in front) come after
// everything. rank -> RankZ(rank) (render/View.hpp).
//
// THE CONTRACT the renderers share: sprites[i] at RankZ(spriteRank[i]); every
// shadow cast by sprites[i] at RankZ(shadowRankBase[i]) (one rank reserved per
// sprite, caster or not; a caster's several shadows share it); particles[k] at
// RankZ(particleRank[k]); the halo of lights[k] at RankZ(haloRank + k); total
// is one past the last rank. The background image is RankZ(-1).
//
// Implemented in render/DrawOrder.cpp (sprite renderer's side).

#include <vector>

#include "eth/Snapshot.hpp"

namespace Penumbra::Render {

struct DrawOrder {
    std::vector<int> spriteRank;     // one per snapshot.sprites
    std::vector<int> particleRank;   // one per snapshot.particles
    std::vector<int> shadowRankBase; // one per snapshot.sprites: just behind a horizontal-style caster, just
                                     // before a vertical one (ETHRenderEntity.cpp:910-911)
    int haloRank = 0;                // first rank for halos (they follow all)
    int total = 0;
};

DrawOrder ComputeDrawOrder(const Eth::RenderSnapshot& snapshot);

} // namespace Penumbra::Render
