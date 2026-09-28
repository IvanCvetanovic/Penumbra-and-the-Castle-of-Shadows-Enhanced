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
//
// THE ROWS. A standing sprite's depth was not one number. 0.7.12's vertical
// vertex shaders lowered each vertex's z by (1 - v) rectSize.y / (maxH - minH)
// (defaultStaticAmbientVS.cg:132, pixelLightVS.cg:129, vVertexLightShader.cg:115),
// so the row k texels above the frame's bottom edge lay at the depth of z + k
// (docs/spec/21 2.1): a particle or a sprite whose depth fell between the
// sprite's base and its top was hidden by the rows above that height and drawn
// over the rows below it. The painter's order holds one depth a piece, so such
// a sprite is cut into bands of rows at those heights, each band a piece at
// the depth of its lowest row - its bottom band keeps the whole sprite's depth
// and sequence, so it ties with what it tied with before. It is cut only where
// the piece overlaps it on screen and only at a depth strictly inside its rows
// (the arena select's thumbnails, z 4 and 64 rows tall, under the cursor's
// sparkles, which sit near z 20: 0.7.12 hid them behind the thumbnail's upper
// rows; the menu logo, z 294 and 145 rows, under the fog at 324). Shadows do
// not cut: a vertical caster's lies at z 0, a horizontal one's at its depth,
// neither inside a standing sprite's rows in the shipped scenes. A standing
// sprite over another is not cut either: the shipped ones that overlap share
// their z, and one of equal z whose bottom edge is lower is nearer in every row.
// kPerRowVerticalDepth off draws every sprite whole, as the port did before.

namespace Penumbra::Render {

namespace {

// THE ROWS above.
constexpr bool kPerRowVerticalDepth = true;

// ETHRenderEntity::m_layrableMinimumDepth, which DrawShadow subtracts from a
// non-vertical caster's depth (ETHRenderEntity.cpp:55, :911).
constexpr float kShadowDepthBias = 0.001f;

enum class Piece : std::uint8_t { Sprite, Shadow, Particle, Band };

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

// A drawn footprint in the snapshot's world pixels (+y down).
struct Footprint {
    glm::vec2 min{0.0f};
    glm::vec2 max{0.0f};
};

bool Overlap(const Footprint& a, const Footprint& b) {
    return a.min.x < b.max.x && b.min.x < a.max.x && a.min.y < b.max.y && b.min.y < a.max.y;
}

bool Drawn(const Eth::SpriteDraw& sprite) {
    return !sprite.sprite.empty() && sprite.size.x > 0.0f && sprite.size.y > 0.0f;
}

Footprint SpriteFootprint(const Eth::SpriteDraw& sprite) {
    return Footprint{sprite.origin, sprite.origin + sprite.size};
}

// A particle's square turned by any angle stays inside the circle through its
// corners.
Footprint ParticleFootprint(const Eth::ParticleDraw& particle) {
    const float reach = particle.size * 0.70710678f;
    return Footprint{particle.position - glm::vec2(reach), particle.position + glm::vec2(reach)};
}

// THE ROWS: the rows of standing sprite `sprite` at which it is cut, from its
// top, ascending. Row r's centre stands H - r - 0.5 above the bottom edge, at
// depth base + (H - r - 0.5) slope; a piece of depth d covers the rows at or
// below its height k = (d - base) / slope (a tie goes to the piece: a particle
// was drawn after every sprite, LESSEQUAL), so the rows above it end at
// ceil(H - 0.5 - k).
std::vector<int> CutRows(const Eth::RenderSnapshot& snapshot, std::size_t index, const DepthLine& line) {
    std::vector<int> cuts;
    const Eth::SpriteDraw& sprite = snapshot.sprites[index];
    if (sprite.type != Eth::ET_VERTICAL || !Drawn(sprite) || !std::isfinite(sprite.depth)) return cuts;
    const int rows = static_cast<int>(sprite.size.y);
    if (rows < 2) return cuts;
    const float base = sprite.depth;
    const float top = base + static_cast<float>(rows) * line.slope;
    const Footprint self = SpriteFootprint(sprite);
    const auto consider = [&](float depth, const Footprint& other) {
        if (!(depth > base && depth < top) || !Overlap(self, other)) return;
        const float height = (depth - base) / line.slope;
        const int row = static_cast<int>(std::ceil(static_cast<float>(rows) - 0.5f - height));
        if (row > 0 && row < rows) cuts.push_back(row);
    };
    for (std::size_t j = 0; j < snapshot.sprites.size(); ++j) {
        const Eth::SpriteDraw& other = snapshot.sprites[j];
        if (j == index || !Drawn(other) || other.type == Eth::ET_VERTICAL) continue;
        consider(other.depth, SpriteFootprint(other));
    }
    for (const Eth::ParticleDraw& particle : snapshot.particles) {
        if (particle.size > 0.0f) consider(particle.depth, ParticleFootprint(particle));
    }
    std::sort(cuts.begin(), cuts.end());
    cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
    return cuts;
}

} // namespace

DrawOrder ComputeDrawOrder(const Eth::RenderSnapshot& snapshot) {
    const std::size_t spriteCount = snapshot.sprites.size();
    const std::size_t particleCount = snapshot.particles.size();

    const DepthLine line = FitDepthLine(snapshot);

    DrawOrder order;
    order.firstBand.assign(spriteCount, -1);

    std::vector<Item> items;
    items.reserve(spriteCount * 2 + particleCount);
    for (std::size_t i = 0; i < spriteCount; ++i) {
        const Eth::SpriteDraw& sprite = snapshot.sprites[i];
        const std::int64_t base = static_cast<std::int64_t>(i) * 3;
        const float depth = Finite(sprite.depth);
        items.push_back(Item{depth, 0, base + 1, Piece::Sprite, static_cast<int>(i)});
        // THE ROWS above: the bottom band is the sprite's own item; each band
        // above it goes in at the depth of its lowest row, with the sprite's
        // sequence (its bands' depths all differ).
        if (kPerRowVerticalDepth && line.known && line.slope > 0.0f) {
            const std::vector<int> cuts = CutRows(snapshot, i, line);
            if (!cuts.empty()) {
                const int rows = static_cast<int>(sprite.size.y);
                order.firstBand[i] = static_cast<int>(order.bands.size());
                int begin = 0;
                for (std::size_t c = 0; c <= cuts.size(); ++c) {
                    const int end = c < cuts.size() ? cuts[c] : rows;
                    order.bands.push_back(SpriteBand{static_cast<int>(i), begin, end, 0});
                    if (end < rows) {
                        const float lowest = static_cast<float>(rows - (end - 1)) - 0.5f;
                        items.push_back(Item{depth + lowest * line.slope, 0, base + 1, Piece::Band,
                                             static_cast<int>(order.bands.size() - 1)});
                    }
                    begin = end;
                }
            }
        }
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
        case Piece::Band: order.bands[index].rank = rank; break;
        }
        ++rank;
    }
    // A cut sprite's bottom band is the sprite's own item.
    for (SpriteBand& band : order.bands) {
        if (band.rowEnd == static_cast<int>(snapshot.sprites[static_cast<std::size_t>(band.sprite)].size.y)) {
            band.rank = order.spriteRank[static_cast<std::size_t>(band.sprite)];
        }
    }
    order.haloRank = rank;
    order.total = rank + static_cast<int>(snapshot.lights.size());
    return order;
}

} // namespace Penumbra::Render
