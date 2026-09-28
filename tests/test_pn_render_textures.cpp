// The original's images as the port decodes them (render/TextureDecode.hpp):
// colour keys, the DDS reader, content sniffing, the normal-map renormalise;
// the texture cache on a bare registry; the painter's order (render/DrawOrder);
// and the sprite renderer's quads on a bare registry.
//
// Reference values were read from the files with PIL/numpy (docs/spec/20 §12.3,
// docs/spec/21 §2.6 for shadow.dds).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <entt/entt.hpp>

#include "TestHarness.hpp"
#include "core/Components.hpp"
#include "core/RenderSettings.hpp"
#include "render/CameraRig.hpp"
#include "render/DrawOrder.hpp"
#include "render/SpriteRenderer.hpp"
#include "render/TextureCache.hpp"
#include "render/TextureDecode.hpp"

using namespace Penumbra;
using namespace Penumbra::Render;

namespace {

const std::string kRoot = PENUMBRA_ORIGINAL_DIR;

struct Rgba {
    int r, g, b, a;
};

Rgba At(const DecodedImage& image, int x, int y) {
    const std::size_t i = (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                           static_cast<std::size_t>(x)) * 4u;
    return {image.rgba[i], image.rgba[i + 1], image.rgba[i + 2], image.rgba[i + 3]};
}

bool Is(const Rgba& c, int r, int g, int b, int a) { return c.r == r && c.g == g && c.b == b && c.a == a; }

std::string Show(const Rgba& c) {
    return "(" + std::to_string(c.r) + "," + std::to_string(c.g) + "," + std::to_string(c.b) + "," +
           std::to_string(c.a) + ")";
}

int CountAlpha(const DecodedImage& image, int alpha) {
    int n = 0;
    for (std::size_t i = 3; i < image.rgba.size(); i += 4) n += image.rgba[i] == alpha ? 1 : 0;
    return n;
}

void Put32(std::vector<std::uint8_t>& out, std::size_t at, std::uint32_t v) {
    for (int b = 0; b < 4; ++b) out[at + static_cast<std::size_t>(b)] = static_cast<std::uint8_t>(v >> (8 * b));
}

// A DDS as D3DX writes one: "DDS ", DDS_HEADER, the pixel format at 76.
std::vector<std::uint8_t> MakeDds(std::uint32_t width, std::uint32_t height, std::uint32_t formatFlags,
                                  std::uint32_t fourCc, std::uint32_t bitCount, std::uint32_t r, std::uint32_t g,
                                  std::uint32_t b, std::uint32_t a, const std::vector<std::uint8_t>& pixels) {
    std::vector<std::uint8_t> out(128, 0);
    out[0] = 'D';
    out[1] = 'D';
    out[2] = 'S';
    out[3] = ' ';
    Put32(out, 4, 124);
    Put32(out, 8, 0x1007);
    Put32(out, 12, height);
    Put32(out, 16, width);
    Put32(out, 76, 32);
    Put32(out, 80, formatFlags);
    Put32(out, 84, fourCc);
    Put32(out, 88, bitCount);
    Put32(out, 92, r);
    Put32(out, 96, g);
    Put32(out, 100, b);
    Put32(out, 104, a);
    Put32(out, 108, 0x1000);
    out.insert(out.end(), pixels.begin(), pixels.end());
    return out;
}

void Images() {
    // skull.png: 18x20 RGBA, 177 exact-magenta texels and no alpha-0 texel
    // before the key.
    {
        const DecodedImage plain = DecodeTexture(kRoot + "/entities/skull.png", TextureVariant::Plain);
        CHECK(plain.Valid() && plain.width == 18 && plain.height == 20);
        if (plain.Valid()) {
            CHECK_MSG(Is(At(plain, 0, 0), 255, 0, 255, 255), Show(At(plain, 0, 0)));
            CHECK_EQ(CountAlpha(plain, 0), 0);
        }
        const DecodedImage keyed = DecodeTexture(kRoot + "/entities/skull.png", TextureVariant::Sprite);
        CHECK(keyed.Valid());
        if (keyed.Valid()) {
            CHECK_MSG(Is(At(keyed, 0, 0), 0, 0, 0, 0), Show(At(keyed, 0, 0)));
            CHECK_EQ(CountAlpha(keyed, 0), 177);
        }
    }

    // halo1.bmp: 123x123 8-bit grey, 3960 black texels, white at the centre.
    {
        const DecodedImage halo = DecodeTexture(kRoot + "/entities/halo1.bmp", TextureVariant::Halo);
        CHECK(halo.Valid() && halo.width == 123 && halo.height == 123);
        if (halo.Valid()) {
            CHECK_MSG(Is(At(halo, 0, 0), 0, 0, 0, 0), Show(At(halo, 0, 0)));
            CHECK_MSG(Is(At(halo, 61, 61), 255, 255, 255, 255), Show(At(halo, 61, 61)));
            CHECK_EQ(CountAlpha(halo, 0), 3960);
        }
        // Additive: the same key, then every texel opaque (the keyed ones black).
        const DecodedImage add = DecodeTexture(kRoot + "/entities/halo1.bmp", TextureVariant::Additive);
        CHECK(add.Valid());
        if (add.Valid()) {
            CHECK_MSG(Is(At(add, 0, 0), 0, 0, 0, 255), Show(At(add, 0, 0)));
            CHECK_EQ(CountAlpha(add, 255), 123 * 123);
        }
    }

    // data/shadow.dds: 32x32 A8L8, luminance 0 everywhere; the alpha ramp down
    // column 16 (docs/spec/21 §2.6) and the soft edge along row 24.
    {
        const DecodedImage shadow = DecodeTexture(kRoot + "/data/shadow.dds", TextureVariant::Sprite);
        CHECK(shadow.Valid() && shadow.width == 32 && shadow.height == 32);
        if (shadow.Valid()) {
            int lit = 0;
            for (std::size_t i = 0; i < shadow.rgba.size(); i += 4) {
                lit += (shadow.rgba[i] | shadow.rgba[i + 1] | shadow.rgba[i + 2]) != 0 ? 1 : 0;
            }
            CHECK_EQ(lit, 0);
            const int ramp[32] = {0,   0,   0,   0,   0,   3,   9,   17,  25,  35,  45,  58,  71,  86,  100, 118,
                                  134, 154, 173, 196, 217, 238, 255, 255, 255, 255, 255, 0,   0,   0,   0,   0};
            bool rampOk = true;
            for (int y = 0; y < 32; ++y) rampOk = rampOk && At(shadow, 16, y).a == ramp[y];
            CHECK(rampOk);
            CHECK_EQ(At(shadow, 0, 24).a, 4);
            CHECK_EQ(At(shadow, 1, 24).a, 86);
            CHECK_EQ(At(shadow, 2, 24).a, 253);
            CHECK_EQ(At(shadow, 31, 24).a, 3);
        }
    }

    // entities/thorn.dds: 256x64 A8R8G8B8, black with a soft alpha; the
    // magenta key finds nothing in it.
    {
        const DecodedImage thorn = DecodeTexture(kRoot + "/entities/thorn.dds", TextureVariant::Sprite);
        CHECK(thorn.Valid() && thorn.width == 256 && thorn.height == 64);
        if (thorn.Valid()) {
            CHECK_MSG(Is(At(thorn, 0, 0), 0, 0, 0, 1), Show(At(thorn, 0, 0)));
            CHECK_MSG(Is(At(thorn, 128, 32), 0, 0, 0, 2), Show(At(thorn, 128, 32)));
            CHECK_EQ(CountAlpha(thorn, 255), 251);
        }
        // particles/fog.dds carries grey: (195, 195, 195, 192) at its centre.
        const DecodedImage fog = DecodeTexture(kRoot + "/particles/fog.dds", TextureVariant::Plain);
        CHECK(fog.Valid() && fog.width == 256 && fog.height == 256);
        if (fog.Valid()) CHECK_MSG(Is(At(fog, 128, 128), 195, 195, 195, 192), Show(At(fog, 128, 128)));
    }

    // nm_white_ground.jpg is a 32-bit BMP whose fourth bytes are all 0: opaque.
    {
        const DecodedImage nm = DecodeTexture(kRoot + "/entities/normalmaps/nm_white_ground.jpg", TextureVariant::Plain);
        CHECK(nm.Valid() && nm.width == 256 && nm.height == 256);
        if (nm.Valid()) {
            CHECK_MSG(Is(At(nm, 0, 0), 127, 125, 255, 255), Show(At(nm, 0, 0)));
            CHECK_EQ(CountAlpha(nm, 255), 256 * 256);
        }
        // Renormalised: every texel decodes (2n - 1) to unit length, within the
        // 8-bit quantisation (the file's own lengths run 0.9951 .. 1.0046).
        const DecodedImage normal =
            DecodeTexture(kRoot + "/entities/normalmaps/nm_white_ground.jpg", TextureVariant::Normal);
        CHECK(normal.Valid());
        if (normal.Valid()) {
            float worst = 0.0f;
            for (std::size_t i = 0; i < normal.rgba.size(); i += 4) {
                const float x = normal.rgba[i] / 255.0f * 2.0f - 1.0f;
                const float y = normal.rgba[i + 1] / 255.0f * 2.0f - 1.0f;
                const float z = normal.rgba[i + 2] / 255.0f * 2.0f - 1.0f;
                worst = std::max(worst, std::fabs(std::sqrt(x * x + y * y + z * z) - 1.0f));
            }
            CHECK_MSG(worst < 0.01f, "worst |length - 1| = " + std::to_string(worst));
        }
        // barril_nm.bmp is a 32-bit BI_RGB bitmap whose fourth bytes run
        // 249..255: X8R8G8B8 to D3DX, so opaque.
        const DecodedImage barrel = DecodeTexture(kRoot + "/entities/normalmaps/barril_nm.bmp", TextureVariant::Plain);
        CHECK(barrel.Valid());
        if (barrel.Valid()) {
            CHECK_EQ(CountAlpha(barrel, 255), barrel.width * barrel.height);
        }
    }

    // Sizes from the header alone.
    CHECK(ProbeImageSize(kRoot + "/entities/skull.png") == glm::ivec2(18, 20));
    CHECK(ProbeImageSize(kRoot + "/data/shadow.dds") == glm::ivec2(32, 32));
    CHECK(ProbeImageSize(kRoot + "/entities/thorn.dds") == glm::ivec2(256, 64));
    CHECK(ProbeImageSize(kRoot + "/entities/normalmaps/nm_white_ground.jpg") == glm::ivec2(256, 256));
    CHECK(ProbeImageSize(kRoot + "/entities/no_such_file.png") == glm::ivec2(0, 0));
    std::string why;
    CHECK(!DecodeTexture(kRoot + "/entities/no_such_file.png", TextureVariant::Sprite, &why).Valid());
    CHECK(!why.empty());
}

void HandMade() {
    // A8R8G8B8, stored BGRA: channel order, and the magenta key on a DDS.
    {
        const std::vector<std::uint8_t> bytes = MakeDds(2, 1, 0x41, 0, 32, 0xFF0000u, 0xFF00u, 0xFFu, 0xFF000000u,
                                                        {10, 20, 30, 40, 255, 0, 255, 255});
        const DecodedImage plain = DecodeTextureMemory(bytes.data(), bytes.size(), TextureVariant::Plain);
        CHECK(plain.Valid() && plain.width == 2 && plain.height == 1);
        if (plain.Valid()) {
            CHECK_MSG(Is(At(plain, 0, 0), 30, 20, 10, 40), Show(At(plain, 0, 0)));
            CHECK_MSG(Is(At(plain, 1, 0), 255, 0, 255, 255), Show(At(plain, 1, 0)));
        }
        const DecodedImage keyed = DecodeTextureMemory(bytes.data(), bytes.size(), TextureVariant::Sprite);
        if (keyed.Valid()) CHECK_MSG(Is(At(keyed, 1, 0), 0, 0, 0, 0), Show(At(keyed, 1, 0)));
    }
    // A8L8: (L, L, L, A).
    {
        const std::vector<std::uint8_t> bytes = MakeDds(1, 1, 0x20001, 0, 16, 0xFFu, 0, 0, 0xFF00u, {77, 200});
        const DecodedImage image = DecodeTextureMemory(bytes.data(), bytes.size(), TextureVariant::Plain);
        CHECK(image.Valid());
        if (image.Valid()) CHECK_MSG(Is(At(image, 0, 0), 77, 77, 77, 200), Show(At(image, 0, 0)));
    }
    // R5G6B5 without alpha: channels widened to 8 bits, opaque.
    {
        const std::vector<std::uint8_t> bytes = MakeDds(2, 1, 0x40, 0, 16, 0xF800u, 0x07E0u, 0x001Fu, 0,
                                                        {0x00, 0xF8, 0x1F, 0x00});
        const DecodedImage image = DecodeTextureMemory(bytes.data(), bytes.size(), TextureVariant::Plain);
        CHECK(image.Valid());
        if (image.Valid()) {
            CHECK_MSG(Is(At(image, 0, 0), 255, 0, 0, 255), Show(At(image, 0, 0)));
            CHECK_MSG(Is(At(image, 1, 0), 0, 0, 255, 255), Show(At(image, 1, 0)));
        }
    }
    // Refused by name, not decoded as garbage: a compressed DDS, and one cut short.
    {
        std::string why;
        const std::vector<std::uint8_t> dxt = MakeDds(4, 4, 0x4, 0x31545844u /*DXT1*/, 0, 0, 0, 0, 0,
                                                      std::vector<std::uint8_t>(8, 0));
        CHECK(!DecodeTextureMemory(dxt.data(), dxt.size(), TextureVariant::Plain, &why).Valid());
        CHECK_MSG(why.find("DXT1") != std::string::npos, why);
        const std::vector<std::uint8_t> shortDds = MakeDds(2, 2, 0x41, 0, 32, 0xFF0000u, 0xFF00u, 0xFFu,
                                                           0xFF000000u, std::vector<std::uint8_t>(8, 0));
        CHECK(!DecodeTextureMemory(shortDds.data(), shortDds.size(), TextureVariant::Plain).Valid());
    }

    // The variant rules on single texels.
    {
        std::vector<std::uint8_t> t = {255, 0, 255, 254};    // magenta, not opaque: no key match
        ApplyTextureVariant(t, TextureVariant::Sprite);
        CHECK(t == (std::vector<std::uint8_t>{255, 0, 255, 254}));

        t = {10, 20, 30, 0, 10, 20, 30, 128, 0, 0, 0, 255};
        ApplyTextureVariant(t, TextureVariant::Additive);
        CHECK(t == (std::vector<std::uint8_t>{0, 0, 0, 255, 10, 20, 30, 255, 0, 0, 0, 255}));

        // Modulate: white darkens nothing, opaque black everything, a keyed
        // texel nothing, and half-grey at half alpha about a quarter.
        t = {255, 255, 255, 255, 0, 0, 0, 255, 255, 0, 255, 255, 128, 128, 128, 128};
        ApplyTextureVariant(t, TextureVariant::Modulate);
        CHECK(t[0] == 0 && t[1] == 0 && t[2] == 0 && t[3] == 0);
        CHECK(t[7] == 255);
        CHECK(t[11] == 0);
        CHECK(t[12] == 0 && t[15] >= 62 && t[15] <= 65);

        // Normal: (255, 128, 128) is already +x; a short vector is lengthened.
        t = {255, 128, 128, 255, 160, 128, 160, 255};
        ApplyTextureVariant(t, TextureVariant::Normal);
        CHECK(t[0] == 255 && t[1] == 128 && t[2] == 128);
        const float x = t[4] / 255.0f * 2.0f - 1.0f;
        const float y = t[5] / 255.0f * 2.0f - 1.0f;
        const float z = t[6] / 255.0f * 2.0f - 1.0f;
        // Within the 8-bit quantisation, as above (it was 0.36 long).
        CHECK(std::fabs(std::sqrt(x * x + y * y + z * z) - 1.0f) < 0.01f);
        CHECK(t[4] == t[6] && t[4] > 200);
    }

    CHECK(VirtualTextureKey("entities\\x.png", TextureVariant::Sprite) == "penumbra:sprite:entities/x.png");
    CHECK(VirtualTextureKey("entities/halo.bmp", TextureVariant::Halo) == "penumbra:halo:entities/halo.bmp");
}

void CacheWithoutADevice() {
    // A bare registry: no TextureRegistry, so nothing uploads, but keys and
    // sizes still answer.
    entt::registry registry;
    TextureCache cache(kRoot);
    cache.Attach(registry);
    CHECK(cache.Key("entities/skull.png", TextureVariant::Sprite) == "penumbra:sprite:entities/skull.png");
    CHECK(cache.Key("entities/no_such_file.png", TextureVariant::Sprite).empty());
    CHECK(cache.Key("", TextureVariant::Sprite).empty());
    CHECK(cache.Size("entities/skull.png") == glm::ivec2(18, 20));
    CHECK(cache.Size("entities\\normalmaps\\nm_white_ground.jpg") == glm::ivec2(256, 256));
    // Spelled as a Windows-only game could spell it, found on any filesystem
    // (eth/Paths.hpp): the original's STONE03A.JPG, and a skull in capitals.
    CHECK(cache.Size("ENTITIES\\Skull.PNG") == glm::ivec2(18, 20));
    CHECK(cache.Key("Entities/SKULL.png", TextureVariant::Sprite) == "penumbra:sprite:Entities/SKULL.png");
    CHECK(cache.Size("entities/stone03a.jpg") == cache.Size("entities/STONE03A.JPG"));
    CHECK(cache.Size("entities/STONE03A.JPG").x > 0);
}

Eth::SpriteDraw Sprite(int id, Eth::ENTITY_TYPE type, float z, float depth) {
    Eth::SpriteDraw s;
    s.entityId = id;
    s.sprite = "skull.png";
    s.type = type;
    s.position = glm::vec3(0.0f, 0.0f, z);
    s.size = glm::vec2(18.0f, 20.0f);
    s.bitmapSize = s.size;
    s.depth = depth;
    return s;
}

Eth::ParticleDraw Particle(float depth) {
    Eth::ParticleDraw p;
    p.depth = depth;
    return p;
}

void Order() {
    // Snapshot (drawHash) order, as 0.7.12 drew level1: a wall at z -84, a
    // tile at 0, a black layerable cover (hash = depth = 1.0), then a barrel -
    // vertical, hash 0.5 + depth + (y - camY), in the hundreds - standing at
    // the tiles' z.
    Eth::RenderSnapshot snapshot;
    snapshot.sprites = {Sprite(1, Eth::ET_HORIZONTAL, -84.0f, 0.10f), Sprite(2, Eth::ET_HORIZONTAL, 0.0f, 0.15f),
                        Sprite(3, Eth::ET_LAYERABLE, 0.0f, 1.0f), Sprite(4, Eth::ET_VERTICAL, 0.0f, 0.15f)};
    snapshot.particles = {Particle(0.12f), Particle(0.15f), Particle(0.5f)};
    snapshot.lights.resize(2);

    const DrawOrder order = ComputeDrawOrder(snapshot);
    CHECK_EQ(order.spriteRank.size(), std::size_t{4});
    CHECK_EQ(order.particleRank.size(), std::size_t{3});
    CHECK_EQ(order.shadowRankBase.size(), std::size_t{4});
    if (order.spriteRank.size() == 4 && order.particleRank.size() == 3 && order.shadowRankBase.size() == 4) {
        const auto& s = order.spriteRank;
        const auto& p = order.particleRank;
        const auto& h = order.shadowRankBase;
        // THE BARREL BEHIND THE COVER, as the z-buffer had it, though it was
        // drawn after it.
        CHECK(s[3] < s[2]);
        // Back to front by depth; the barrel after the tile at the same depth
        // (drawn later, LESSEQUAL wins).
        CHECK(s[0] < s[1] && s[1] < s[3]);
        // Particles interleave by depth, after sprites of equal depth.
        CHECK(s[0] < p[0] && p[0] < s[1]);
        CHECK(s[3] < p[1] && p[1] < p[2] && p[2] < s[2]);
        // A horizontal caster's shadow just behind it and whatever shares its
        // depth; the vertical barrel's at its base depth, after the tile it
        // darkens and before the barrel that covers it.
        CHECK(h[0] < s[0]);
        CHECK(p[0] < h[1] && h[1] < s[1]);
        CHECK(s[1] < h[3] && h[3] < s[3]);
        CHECK(p[2] < h[2] && h[2] < s[2]);
        // Halos after everything, one rank a light.
        const int last = std::max({*std::max_element(s.begin(), s.end()), *std::max_element(p.begin(), p.end()),
                                   *std::max_element(h.begin(), h.end())});
        CHECK(order.haloRank > last);
        CHECK_EQ(order.haloRank, 11);
        CHECK_EQ(order.total, order.haloRank + 2);
        // Every rank used once.
        std::vector<int> all(s.begin(), s.end());
        all.insert(all.end(), p.begin(), p.end());
        all.insert(all.end(), h.begin(), h.end());
        std::sort(all.begin(), all.end());
        CHECK(std::adjacent_find(all.begin(), all.end()) == all.end());
        CHECK(all.front() == 0 && all.back() == order.haloRank - 1);
    }
    // The same snapshot ranks the same way.
    const DrawOrder again = ComputeDrawOrder(snapshot);
    CHECK(again.spriteRank == order.spriteRank && again.particleRank == order.particleRank &&
          again.shadowRankBase == order.shadowRankBase && again.haloRank == order.haloRank);

    // A vertical caster off z = 0: its shadow sits at the depth of z = 0,
    // read back from the frame's depth line (here exactly 1/256 a unit, 0.5 at
    // z = 0).
    Eth::RenderSnapshot raised;
    raised.sprites = {Sprite(1, Eth::ET_HORIZONTAL, 0.0f, 0.5f), Sprite(2, Eth::ET_HORIZONTAL, -64.0f, 0.25f),
                      Sprite(3, Eth::ET_VERTICAL, 64.0f, 0.75f)};
    const DrawOrder r = ComputeDrawOrder(raised);
    if (r.spriteRank.size() == 3 && r.shadowRankBase.size() == 3) {
        CHECK_EQ(r.spriteRank[0], 3);
        CHECK_EQ(r.shadowRankBase[2], 4);
        CHECK_EQ(r.spriteRank[2], 5);
    }

    // Empty: nothing but the halos.
    Eth::RenderSnapshot empty;
    empty.lights.resize(3);
    const DrawOrder none = ComputeDrawOrder(empty);
    CHECK(none.spriteRank.empty() && none.haloRank == 0 && none.total == 3);
}

void Camera() {
    Eth::RenderSnapshot snapshot;
    snapshot.camera = glm::vec2(100.0f, 50.0f);
    snapshot.screenSize = glm::vec2(1024.0f, 768.0f);

    // 4:3 in 16:9, pillarboxed: 1440 wide, 240-pixel bars.
    const View boxed = CameraRig::ComputeView(snapshot, glm::uvec2(1920, 1080), true);
    CHECK_NEAR(boxed.scale, 1080.0f / 768.0f);
    CHECK_NEAR(boxed.viewportMin.x, 240.0f);
    CHECK_NEAR(boxed.viewportMax.x, 1680.0f);
    CHECK_NEAR(boxed.viewportMin.y, 0.0f);
    const std::vector<CameraRig::Bar> bars = CameraRig::Bars(boxed);
    CHECK_EQ(bars.size(), std::size_t{2});
    if (bars.size() == 2) {
        CHECK_NEAR(bars[0].max.x, 0.125f);
        CHECK_NEAR(bars[1].min.x, 0.875f);
    }
    // The HUD's logical (0, 0) lands at the box's corner.
    CHECK_NEAR(boxed.HudToFraction(glm::vec2(0.0f)).x, 0.125f);

    // Widescreen: the logical screen sized to the window fills it, no bars.
    snapshot.screenSize = glm::vec2(1365.3334f, 768.0f);
    const View wide = CameraRig::ComputeView(snapshot, glm::uvec2(1920, 1080), false);
    CHECK(CameraRig::Bars(wide).empty());
    CHECK(std::fabs(wide.viewportMin.x) < 1.0f);

    // The rig on a bare registry: an orthographic camera at the logical
    // screen's centre, y flipped; the clear colour from the snapshot.
    entt::registry registry;
    CameraRig rig;
    rig.Attach(registry);
    snapshot.screenSize = glm::vec2(1024.0f, 768.0f);
    snapshot.backgroundColor = 0xFF4D4D4Du;
    const View view = rig.Update(registry, snapshot, true);
    CHECK(registry.valid(rig.Camera()));
    if (registry.valid(rig.Camera())) {
        const auto& camera = registry.get<Supersonic::CameraComponent>(rig.Camera());
        CHECK(camera.isOrthographic());
        CHECK(!camera.flyControlsEnabled);
        CHECK_NEAR(camera.position.x, 100.0f + 512.0f);
        CHECK_NEAR(camera.position.y, -(50.0f + 384.0f));
        CHECK_NEAR(camera.orthoHeight, 768.0f);
    }
    CHECK_NEAR(view.scale, 1.0f);
    const auto* rendering = registry.ctx().find<Supersonic::RenderSettings>();
    CHECK(rendering != nullptr);
    if (rendering != nullptr) {
        CHECK(!rendering->decodesColourTextures());
        CHECK(rendering->background == Supersonic::RenderSettings::Background::Color);
        CHECK_NEAR(rendering->backgroundColor[0], 0x4D / 255.0f);
    }
}

void Sprites() {
    entt::registry registry;
    TextureCache cache(kRoot);
    SpriteRenderer renderer;
    renderer.Attach(registry, cache);

    Eth::RenderSnapshot snapshot;
    snapshot.camera = glm::vec2(0.0f);
    Eth::SpriteDraw skull = Sprite(7, Eth::ET_HORIZONTAL, 0.0f, 0.5f);
    skull.blendMode = Eth::AM_ALPHA_TEST;
    skull.origin = glm::vec2(100.0f, 200.0f);
    skull.color = glm::vec4(1.0f, 1.0f, 1.0f, 0.5f);
    // bruxo.png is 128x192, cut 4x4 into 32x48 frames; frame 5 is column 1, row 1.
    Eth::SpriteDraw wizard = Sprite(9, Eth::ET_HORIZONTAL, 0.0f, 0.6f);
    wizard.sprite = "bruxo.png";
    wizard.size = glm::vec2(32.0f, 48.0f);
    wizard.bitmapSize = glm::vec2(128.0f, 192.0f);
    wizard.spriteCutX = 4;
    wizard.spriteCutY = 4;
    wizard.frame = 5;
    Eth::SpriteDraw label = Sprite(11, Eth::ET_HORIZONTAL, 0.0f, 0.7f);
    label.sprite.clear();
    snapshot.sprites = {skull, wizard, label};
    snapshot.backgroundImage = "entities/planets.png";
    snapshot.backgroundAdditive = true;
    snapshot.backgroundMin = glm::vec2(943.4f, 429.0f);
    snapshot.backgroundMax = glm::vec2(614.4f, 100.0f);

    const View view = CameraRig::ComputeView(snapshot, glm::uvec2(1024, 768), false);
    const DrawOrder order = ComputeDrawOrder(snapshot);
    renderer.Draw(registry, snapshot, view, order);

    const entt::entity quad = renderer.QuadFor(7);
    CHECK(quad != entt::null && registry.valid(quad));
    CHECK(renderer.QuadFor(11) == entt::null);
    CHECK_EQ(renderer.VisibleQuads(), std::size_t{2});
    if (quad != entt::null && registry.valid(quad)) {
        const auto& transform = registry.get<Supersonic::TransformComponent>(quad);
        CHECK_NEAR(transform.position.x, 109.0f);
        CHECK_NEAR(transform.position.y, -210.0f);
        CHECK_NEAR(transform.position.z, RankZ(order.spriteRank[0]));
        CHECK_NEAR(transform.scale.x, 18.0f);
        CHECK_NEAR(transform.scale.y, 20.0f);
        const auto& material = registry.get<Supersonic::MaterialComponent>(quad);
        CHECK(material.albedoTexturePath == "penumbra:sprite:entities/skull.png");
        CHECK(material.unlit && material.transparent);
        CHECK(material.blend == Supersonic::MaterialComponent::BlendMode::Alpha);
        CHECK_NEAR(material.alphaCutoff, SpriteRenderer::kAlphaTestCutoff);
        CHECK_NEAR(material.albedoColor.a, 0.5f);
        CHECK(material.sprite2D.enabled);
        CHECK(!material.HasUvTransform());
        CHECK(registry.get<Supersonic::RenderableComponent>(quad).isVisible);
    }
    const entt::entity sheet = renderer.QuadFor(9);
    if (sheet != entt::null && registry.valid(sheet)) {
        const auto& material = registry.get<Supersonic::MaterialComponent>(sheet);
        CHECK_NEAR(material.uvScale.x, 0.25f);
        CHECK_NEAR(material.uvScale.y, 0.25f);
        CHECK_NEAR(material.uvOffset.x, 0.25f);
        CHECK_NEAR(material.uvOffset.y, 0.25f);
    } else {
        CHECK_MSG(false, "no quad for the sheet sprite");
    }
    const entt::entity background = renderer.BackgroundQuad();
    CHECK(background != entt::null && registry.valid(background));
    if (background != entt::null && registry.valid(background)) {
        const auto& material = registry.get<Supersonic::MaterialComponent>(background);
        CHECK(material.albedoTexturePath == "penumbra:additive:entities/planets.png");
        CHECK(material.blend == Supersonic::MaterialComponent::BlendMode::Additive);
        const auto& transform = registry.get<Supersonic::TransformComponent>(background);
        CHECK_NEAR(transform.scale.x, 329.0f);
        CHECK_NEAR(transform.position.x, 778.9f);
        CHECK_NEAR(transform.position.y, -264.5f);
        CHECK(transform.position.z < RankZ(0));
    }

    // Drawn again: the same quads, nothing new.
    const std::size_t pooled = renderer.PooledQuads();
    renderer.Draw(registry, snapshot, view, order);
    CHECK(renderer.QuadFor(7) == quad);
    CHECK_EQ(renderer.PooledQuads(), pooled);

    // The skull gone: its quad hidden, not destroyed.
    snapshot.sprites = {wizard};
    renderer.Draw(registry, snapshot, view, ComputeDrawOrder(snapshot));
    CHECK(renderer.QuadFor(7) == entt::null);
    CHECK(registry.valid(quad));
    if (registry.valid(quad)) CHECK(!registry.get<Supersonic::RenderableComponent>(quad).isVisible);
    CHECK_EQ(renderer.PooledQuads(), pooled);

    renderer.Detach(registry);
    CHECK(!registry.valid(quad));
}

} // namespace

int main() {
    if (!std::filesystem::exists(PENUMBRA_ORIGINAL_DIR)) {
        std::printf("SKIP: the original is not at %s\n", PENUMBRA_ORIGINAL_DIR);
        return 77;
    }
    Images();
    HandMade();
    CacheWithoutADevice();
    Order();
    Camera();
    Sprites();
    return test::summary("test_pn_render_textures", 100);
}
