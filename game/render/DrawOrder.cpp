#include "render/DrawOrder.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// WHAT THE PAINTER'S ORDER IS, and where it departs from DrawOrder.hpp's
// comment ("sprites in snapshot order").
//
// 0.7.12 drew with the depth buffer on and z-write on for every pass, blended
// or not, LESSEQUAL against z = 1 - depth (docs/spec/30 §3.1). So what showed
// at a pixel was the NEAREST piece there, and among equal depths the one drawn
// LAST; the drawHash order decided only the ties. The engine sorts blended
// quads by z alone, so the port has to hand it the z-buffer's answer, and that
// is a stable sort by depth, ties in draw order - not the drawHash order
// itself. The two differ wherever the hash is not monotonic in depth, and the
// shipped content has such a case in every level: a vertical sprite's hash is
// 0.5 + depth + (y - camY) in raw pixels (a barrel on screen hashes in the
// hundreds), so it is drawn after the black layerables (hash = depth = 1.0)
// that cover pits and dead ends - and the z-buffer still hid it behind them.
// Painted in snapshot order, every barrel would sit on top of the covers.
//
// The whole order is one sort over three kinds of piece, by
// (depth, phase, sequence):
//   - a sprite:   (its depth, 0, 3i + 1)
//   - its shadow: horizontal-style caster (ETHRenderEntity.cpp:910-911):
//                     (max(0, depth - 0.001), 0, 3i + 2) - just BEHIND the
//                     caster, drawn right after it, so it darkens what lies
//                     behind and never the caster or its same-depth neighbours
//                     (they fail the LESSEQUAL test against it in the original);
//                 vertical caster: (depth of z = shadowZ = 0, 0, 3i) - the
//                     caster's own base depth with no bias, placed just before
//                     the caster: it darkens the same-depth tiles drawn earlier
//                     (LESSEQUAL passes), while the caster, whose rows stand
//                     nearer than its base, covers it;
//   - a particle: (its depth, 1, k) - particles were drawn after every entity
//                 (ETHScene.cpp:1091-1132), so at equal depth they win, and
//                 among themselves they keep their order, which is one system
//                 after another.
// This is "each particle after the last sprite whose depth does not exceed its
// own" from the header, and it also sorts particles of different depths
// against each other, as the depth test did.
//
// Halos follow everything: depth 1.0 and ADD, after the particles
// (ETHRenderEntity.cpp:747), so haloRank + k is the halo of snapshot.lights[k].
//
// CONTRACT FOR THE OTHER RENDERERS: every shadow cast by sprites[i] is drawn at
// RankZ(shadowRankBase[i]) (one rank a sprite, reserved whether or not it casts;
// several shadows of one caster share it and are ordered by sortKey), the halo
// of lights[k] at RankZ(haloRank + k), and total is one past the last rank.

namespace Penumbra::Render {

namespace {

// ETHRenderEntity::m_layrableMinimumDepth, which DrawShadow subtracts from a
// non-vertical caster's depth (ETHRenderEntity.cpp:55, :911).
constexpr float kShadowDepthBias = 0.001f;

enum class Piece : std::uint8_t { Sprite, Shadow, Particle };

struct Item {
    float depth = 0.0f;
    int phase = 0;
    std::int64_t sequence = 0;
    Piece piece = Piece::Sprite;
    int index = 0;
};

float Finite(float value) { return std::isfinite(value) ? value : 0.0f; }

bool UsesPositionDepth(Eth::ENTITY_TYPE type) { return type == Eth::ET_HORIZONTAL || type == Eth::ET_VERTICAL; }

// A vertical caster's shadow sits at ComputeDepth(shadowZ = 0), which the
// snapshot does not carry. Horizontal and vertical sprites share one linear
// map depth = (z - minH) / (maxH - minH) in a frame, so it is read back from
// the two of them lying furthest apart in z. Unknown when every such sprite
// shares one z.
struct DepthLine {
    bool known = false;
    float slope = 0.0f;
    float intercept = 0.0f;
};

DepthLine FitDepthLine(const Eth::RenderSnapshot& snapshot) {
    const Eth::SpriteDraw* lowest = nullptr;
    const Eth::SpriteDraw* highest = nullptr;
    for (const Eth::SpriteDraw& sprite : snapshot.sprites) {
        if (!UsesPositionDepth(sprite.type) || !std::isfinite(sprite.position.z) || !std::isfinite(sprite.depth)) {
            continue;
        }
        if (lowest == nullptr || sprite.position.z < lowest->position.z) lowest = &sprite;
        if (highest == nullptr || sprite.position.z > highest->position.z) highest = &sprite;
    }
    DepthLine line;
    if (lowest == nullptr || highest == nullptr || highest->position.z <= lowest->position.z) return line;
    line.known = true;
    line.slope = (highest->depth - lowest->depth) / (highest->position.z - lowest->position.z);
    line.intercept = lowest->depth - line.slope * lowest->position.z;
    return line;
}

float VerticalShadowDepth(const Eth::SpriteDraw& caster, const DepthLine& line) {
    if (caster.position.z == 0.0f || !line.known) return Finite(caster.depth);
    return Finite(line.intercept);
}

} // namespace

DrawOrder ComputeDrawOrder(const Eth::RenderSnapshot& snapshot) {
    const std::size_t spriteCount = snapshot.sprites.size();
    const std::size_t particleCount = snapshot.particles.size();

    const DepthLine line = FitDepthLine(snapshot);

    std::vector<Item> items;
    items.reserve(spriteCount * 2 + particleCount);
    for (std::size_t i = 0; i < spriteCount; ++i) {
        const Eth::SpriteDraw& sprite = snapshot.sprites[i];
        const std::int64_t base = static_cast<std::int64_t>(i) * 3;
        const float depth = Finite(sprite.depth);
        items.push_back(Item{depth, 0, base + 1, Piece::Sprite, static_cast<int>(i)});
        if (sprite.type == Eth::ET_VERTICAL) {
            items.push_back(Item{VerticalShadowDepth(sprite, line), 0, base, Piece::Shadow, static_cast<int>(i)});
        } else {
            items.push_back(Item{std::max(0.0f, depth - kShadowDepthBias), 0, base + 2, Piece::Shadow,
                                 static_cast<int>(i)});
        }
    }
    for (std::size_t k = 0; k < particleCount; ++k) {
        items.push_back(Item{Finite(snapshot.particles[k].depth), 1, static_cast<std::int64_t>(k), Piece::Particle,
                             static_cast<int>(k)});
    }

    // The key is unique (a sequence is unique within its phase), so the order
    // is total and the same snapshot always ranks the same way.
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        if (a.depth != b.depth) return a.depth < b.depth;
        if (a.phase != b.phase) return a.phase < b.phase;
        return a.sequence < b.sequence;
    });

    DrawOrder order;
    order.spriteRank.assign(spriteCount, 0);
    order.shadowRankBase.assign(spriteCount, 0);
    order.particleRank.assign(particleCount, 0);
    int rank = 0;
    for (const Item& item : items) {
        const auto index = static_cast<std::size_t>(item.index);
        switch (item.piece) {
        case Piece::Sprite: order.spriteRank[index] = rank; break;
        case Piece::Shadow: order.shadowRankBase[index] = rank; break;
        case Piece::Particle: order.particleRank[index] = rank; break;
        }
        ++rank;
    }
    order.haloRank = rank;
    order.total = rank + static_cast<int>(snapshot.lights.size());
    return order;
}

} // namespace Penumbra::Render
