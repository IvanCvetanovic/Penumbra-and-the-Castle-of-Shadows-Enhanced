// render/Lighting.cpp, render/LightRenderer and render/ShadowRenderer.
//
// What each part pins, and why it could come back:
//   - ComputeSpriteLighting's ambient is min(1, ambient + emissive) for EVERY
//     sprite (E:ETHRenderEntity.cpp:687-691): an applyLight=0 sprite is drawn
//     at the ambient, not at full white - the easy mistake, since "unlit"
//     reads like "not darkened".
//   - The vertical sprites standing up (engine f30df7c): base line at the
//     entity's own y, base height z + ZAxisDirection.y * z, with and without
//     the menus' ZAxisDirection - the easy mistake is to stand it on the row
//     it is drawn from. And the gloss highlight's key and strength
//     (brightness / lightIntensity), off wherever 0.7.12 drew none.
//   - ComputeShadow against numbers worked out independently from
//     dynaShadowVS.cg's own operations (GetAngle, RotateZ uploaded row-major,
//     extrude, push back): a straight-down case, and a diagonal one that only
//     comes out right with the rotation's sign convention right. Then the
//     length rules (x2 live, x8 baked, the 2.2h stretch, the enhanced range
//     cap on the visible end), each early-out, and the alpha byte; and one
//     menu barrel with menu.esc's own numbers.
//   - 0.7.12's lightmap-bake eye for a static light on a static glossy sprite
//     (the menu devil's (110, 1312, 768), whatever the camera), asked for only
//     by such a sprite; through the engine's transliteration, a static
//     highlight that the live eye slides with the camera and the bake eye
//     holds still. The light pass's alpha test asked for by every lit sprite,
//     and a faint glow texel whose pass it drops.
//   - The light mapping: intensity x particle ratio only for dynamic lights,
//     the halo's ratio for all, ConvertToDW, the flicker's bounds.
//   - The pools on a bare registry: the same snapshot drawn twice changes
//     nothing, a light that leaves keeps its slot, the 64-light cap.
//
// Needs no file of the original except where a check says so (the normal
// map, the halo bitmap, shadow.dds), and those checks follow what the
// TextureCache could read.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include "TestHarness.hpp"
#include "core/Components.hpp"
#include "core/Light2D.hpp"
#include "eth/Snapshot.hpp"
#include "render/DrawOrder.hpp"
#include "render/LightRenderer.hpp"
#include "render/Lighting.hpp"
#include "render/ShadowRenderer.hpp"
#include "render/TextureCache.hpp"
#include "render/View.hpp"

using namespace Penumbra;
using Render::LightRenderer;
using Render::ShadowRenderer;

namespace {

bool Near(float a, float b, float eps = 1e-3f) { return test::nearly(a, b, eps); }
bool Near(const glm::vec2& a, const glm::vec2& b, float eps = 1e-3f) { return Near(a.x, b.x, eps) && Near(a.y, b.y, eps); }
bool Near(const glm::vec3& a, const glm::vec3& b, float eps = 1e-3f) {
    return Near(a.x, b.x, eps) && Near(a.y, b.y, eps) && Near(a.z, b.z, eps);
}
std::string Str(const glm::vec2& v) { return "(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ")"; }
std::string Str(const glm::vec3& v) {
    return "(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
}

bool HaveOriginal() { return std::filesystem::exists(PENUMBRA_ORIGINAL_DIR "/data/shadow.dds"); }

Eth::SpriteDraw Tile(float z) {
    Eth::SpriteDraw sprite;
    sprite.entityId = 1;
    sprite.sprite = "tile.png";
    sprite.type = Eth::ET_HORIZONTAL;
    sprite.applyLight = true;
    sprite.position = glm::vec3(300.0f, 200.0f, z);
    sprite.size = glm::vec2(64.0f);
    sprite.origin = glm::vec2(268.0f, 168.0f);
    return sprite;
}

// ---- ComputeSpriteLighting --------------------------------------------------------

void AmbientIsAmbientPlusEmissiveForEverySprite(Render::TextureCache& textures) {
    Eth::RenderSnapshot snapshot;
    snapshot.ambient = glm::vec3(0.2f, 0.1f, 0.2f);   // level1's

    Eth::SpriteDraw lit = Tile(-84.0f);
    lit.emissive = glm::vec4(0.5f, 0.2f, 0.9f, 0.7f);   // alpha is ignored
    const Render::SpriteLighting a = Render::ComputeSpriteLighting(lit, snapshot, textures);
    CHECK_MSG(Near(a.ambient, glm::vec3(0.7f, 0.3f, 1.0f)), Str(a.ambient));
    CHECK(a.lit);
    CHECK_EQ(static_cast<int>(a.lightMask), static_cast<int>(LightRenderer::kAllLayers));
    CHECK(Near(a.height, -84.0f));
    CHECK(a.normalYDown);
    CHECK(a.normalKey.empty());   // no <Normal>: the engine's flat default

    // applyLight=0 with emissive 0: the AMBIENT, not 1 - a tree in level1 is dark.
    Eth::SpriteDraw unlit = Tile(0.0f);
    unlit.applyLight = false;
    unlit.normal = "Bloco_Normal.png";   // named, but no light reaches it: never read
    const Render::SpriteLighting b = Render::ComputeSpriteLighting(unlit, snapshot, textures);
    CHECK_MSG(Near(b.ambient, glm::vec3(0.2f, 0.1f, 0.2f)), Str(b.ambient));
    CHECK(!b.lit);
    CHECK_EQ(static_cast<int>(b.lightMask), 0);
    CHECK(b.normalKey.empty());

    // Emissive (1,1,1): fully bright whatever the room, lit or not.
    unlit.emissive = glm::vec4(1.0f);
    CHECK(Near(Render::ComputeSpriteLighting(unlit, snapshot, textures).ambient, glm::vec3(1.0f)));

    // min(1, ...) per channel.
    snapshot.ambient = glm::vec3(0.9f, 0.1f, 0.6f);
    lit.emissive = glm::vec4(0.5f, 0.5f, 0.5f, 0.0f);
    CHECK_MSG(Near(Render::ComputeSpriteLighting(lit, snapshot, textures).ambient, glm::vec3(1.0f, 0.6f, 1.0f)),
              Str(Render::ComputeSpriteLighting(lit, snapshot, textures).ambient));

    // Pixel shaders off: 0.7.12 still lit it, per vertex, without the map.
    snapshot.pixelShaders = false;
    lit.normal = "Bloco_Normal.png";
    const Render::SpriteLighting c = Render::ComputeSpriteLighting(lit, snapshot, textures);
    CHECK(c.lit);
    CHECK_EQ(static_cast<int>(c.lightMask), static_cast<int>(LightRenderer::kAllLayers));
    CHECK(c.normalKey.empty());
}

void NormalMapComesFromEntitiesNormalmaps(Render::TextureCache& textures) {
    if (!HaveOriginal()) {
        std::printf("  (normal map check skipped: the original is not at %s)\n", PENUMBRA_ORIGINAL_DIR);
        return;
    }
    Eth::RenderSnapshot snapshot;
    Eth::SpriteDraw sprite = Tile(0.0f);
    sprite.normal = "barril_nm.bmp";
    const Render::SpriteLighting lighting = Render::ComputeSpriteLighting(sprite, snapshot, textures);
    CHECK(lighting.normalKey == textures.Key("entities/normalmaps/barril_nm.bmp", Render::TextureVariant::Normal));
}

void VerticalSpritesStandOnTheirBaseLine(Render::TextureCache& textures) {
    // barrel.ent: 48x58, origin at the centre-bottom plus pivot (0,-14), so the
    // entity's point is 44 px below the image top. 0.7.12 lit its rows at
    // (x, y, z + (S.y - drawn y)); the engine stands the quad up on the line
    // y = verticalBaseY with the base line's texels at `height`.
    Eth::RenderSnapshot level;
    Eth::SpriteDraw barrel;
    barrel.type = Eth::ET_VERTICAL;
    barrel.sprite = "barril.png";
    barrel.applyLight = true;
    barrel.size = glm::vec2(48.0f, 58.0f);
    barrel.position = glm::vec3(100.0f, 400.0f, 0.0f);
    barrel.origin = glm::vec2(76.0f, 400.0f - 44.0f);
    Render::SpriteLighting a = Render::ComputeSpriteLighting(barrel, level, textures);
    CHECK(a.vertical);
    CHECK_MSG(Near(a.verticalBaseY, -400.0f), std::to_string(a.verticalBaseY));   // ToWorld's y
    CHECK_MSG(Near(a.height, 0.0f), std::to_string(a.height));                    // drawn from its own y
    // The top row, drawn at engine y -356, is lit at height 0 + (-356 + 400) =
    // 44, and the bottom row at -14: 0.7.12's z + oy - v h.
    CHECK(Near(a.height + ((-barrel.origin.y) - a.verticalBaseY), 44.0f));
    CHECK(Near(a.height + ((-(barrel.origin.y + barrel.size.y)) - a.verticalBaseY), -14.0f));
    barrel.position.z = 10.0f;
    CHECK(Near(Render::ComputeSpriteLighting(barrel, level, textures).height, 10.0f));

    // The menus draw it z pixels higher (ZAxisDirection (0,-1)), and 0.7.12
    // still lit it at its own y: the drawn anchor row, z higher on screen, is
    // at height z, so the base line (its own y) is at 0 whatever z is.
    Eth::RenderSnapshot menu;
    menu.zAxisDirection = glm::vec2(0.0f, -1.0f);
    barrel.origin = glm::vec2(76.0f, (400.0f - 10.0f) - 44.0f);
    a = Render::ComputeSpriteLighting(barrel, menu, textures);
    CHECK(a.vertical);
    CHECK(Near(a.verticalBaseY, -400.0f));
    CHECK_MSG(Near(a.height, 0.0f), std::to_string(a.height));
    CHECK(Near(a.height + (-(400.0f - 10.0f) - a.verticalBaseY), 10.0f));   // the anchor row: z

    // No light reaches it: nothing to stand up.
    barrel.applyLight = false;
    CHECK(!Render::ComputeSpriteLighting(barrel, menu, textures).vertical);

    // Anything else lies flat, lit at its z, where the drawing is.
    Eth::SpriteDraw layer = Tile(-12.0f);
    layer.type = Eth::ET_LAYERABLE;
    const Render::SpriteLighting flat = Render::ComputeSpriteLighting(layer, menu, textures);
    CHECK(Near(flat.height, -12.0f));
    CHECK(!flat.vertical);
    CHECK(Near(flat.verticalBaseY, 0.0f));
}

void GlossMapsGiveAHighlight(Render::TextureCache& textures) {
    // cano.ent: <Gloss>white.bmp</Gloss>, specularPower 60, brightness 1, lit.
    Eth::RenderSnapshot snapshot;   // pixel shaders on, lightIntensity 2 (the default)
    Eth::SpriteDraw pipe = Tile(0.0f);
    pipe.gloss = "white.bmp";
    pipe.specularPower = 60.0f;
    pipe.specularBrightness = 1.0f;

    // None of it without the original's file, as 0.7.12 drew no highlight
    // for a gloss map that would not load.
    Render::SpriteLighting a = Render::ComputeSpriteLighting(pipe, snapshot, textures);
    if (!HaveOriginal()) {
        CHECK(a.glossKey.empty());
        CHECK(Near(a.specularStrength, 0.0f));
        std::printf("  (gloss checks skipped: the original is not at %s)\n", PENUMBRA_ORIGINAL_DIR);
        return;
    }
    const std::string key = textures.Key("entities/white.bmp", Render::TextureVariant::Plain);
    CHECK(!key.empty());
    CHECK_MSG(a.glossKey == key, a.glossKey);
    // The engine's light colour is colour x lightIntensity; 0.7.12's highlight
    // took the colour alone.
    CHECK_MSG(Near(a.specularStrength, 0.5f), std::to_string(a.specularStrength));
    CHECK(Near(a.specularPower, 60.0f));
    snapshot.lightIntensity = 4.0f;
    pipe.specularBrightness = 2.0f;
    CHECK(Near(Render::ComputeSpriteLighting(pipe, snapshot, textures).specularStrength, 0.5f));
    snapshot.lightIntensity = 2.0f;
    pipe.specularBrightness = 1.0f;

    // Looked up by file name in entities\, as Scene.cpp loads it.
    pipe.gloss = "some\\folder/white.bmp";
    CHECK(Render::ComputeSpriteLighting(pipe, snapshot, textures).glossKey == key);
    pipe.gloss = "white.bmp";

    // Off: pixel shaders off (the per-vertex fallback had none), no light
    // reaching it, no gloss, a file that does not read, no brightness, and a
    // scene without light intensity (its lights add nothing to divide).
    const auto off = [&](const Eth::SpriteDraw& sprite, const Eth::RenderSnapshot& shot) {
        const Render::SpriteLighting l = Render::ComputeSpriteLighting(sprite, shot, textures);
        return Near(l.specularStrength, 0.0f) && l.glossKey.empty();
    };
    Eth::RenderSnapshot noShaders = snapshot;
    noShaders.pixelShaders = false;
    CHECK(off(pipe, noShaders));
    Eth::SpriteDraw unlit = pipe;
    unlit.applyLight = false;
    CHECK(off(unlit, snapshot));
    Eth::SpriteDraw plain = pipe;
    plain.gloss.clear();
    CHECK(off(plain, snapshot));
    Eth::SpriteDraw missing = pipe;
    missing.gloss = "no_such_gloss.bmp";
    CHECK(off(missing, snapshot));
    Eth::SpriteDraw dull = pipe;
    dull.specularBrightness = 0.0f;
    CHECK(off(dull, snapshot));
    Eth::RenderSnapshot dark = snapshot;
    dark.lightIntensity = 0.0f;
    CHECK(off(pipe, dark));

    // A standing glossy sprite (the menu's devil statues) takes both.
    Eth::SpriteDraw devil = pipe;
    devil.type = Eth::ET_VERTICAL;
    devil.gloss = "white_ground.jpg";
    devil.specularPower = 30.0f;
    a = Render::ComputeSpriteLighting(devil, snapshot, textures);
    CHECK(a.vertical);
    CHECK(Near(a.specularPower, 30.0f));
    CHECK(a.glossKey == textures.Key("entities/white_ground.jpg", Render::TextureVariant::Plain));
}

// ---- ComputeShadow ----------------------------------------------------------------

Eth::SpriteDraw Caster() {
    Eth::SpriteDraw caster;
    caster.entityId = 42;
    caster.sprite = "crate.png";
    caster.type = Eth::ET_HORIZONTAL;
    caster.isStatic = true;
    caster.applyLight = true;
    caster.castShadow = true;
    caster.shadowScale = 1.0f;
    caster.shadowLengthScale = 1.0f;
    caster.shadowOpacity = 1.0f;
    caster.position = glm::vec3(100.0f, 100.0f, 0.0f);
    caster.size = glm::vec2(40.0f, 50.0f);
    caster.origin = glm::vec2(80.0f, 75.0f);
    return caster;
}

Eth::LightDraw Light(const glm::vec3& at, bool isStatic) {
    Eth::LightDraw light;
    light.ownerId = 7;
    light.position = at;
    light.color = glm::vec3(1.0f);
    light.range = 200.0f;
    light.isStatic = isStatic;
    light.castShadows = true;
    return light;
}

void ShadowStripMatchesTheShader() {
    // A dynamic light 100 px straight up the screen, 10 in front.
    const Eth::SpriteDraw caster = Caster();
    const Render::ShadowGeometry g =
        ShadowRenderer::ComputeShadow(caster, Light({100.0f, 0.0f, 10.0f}, false), glm::vec3(0.0f));
    CHECK(g.visible);
    CHECK(!g.baked);
    CHECK(Near(g.width, 32.0f));      // 40 x 0.8
    CHECK(Near(g.length, 100.0f));    // 50 x 2, the real-time factor
    // (1 - 10100/40000) x 255 = 190.6, truncated.
    CHECK_EQ(g.alpha8, 190);
    CHECK(Near(g.alpha, 190.0f / 255.0f, 1e-6f));
    CHECK(Near(g.anchor, glm::vec2(100.0f, 100.0f)));

    // Worked out from dynaShadowVS.cg's operations with GetAngle = atan2(dx, dy)
    // and RotateZ uploaded row-major (reference: this suite's header).
    const glm::vec2 expected[5] = {{131.6783f, 182.8866f}, {116.0f, 83.1233f}, {100.0f, 184.1233f},
                                   {84.0f, 83.1233f}, {68.3217f, 182.8866f}};
    for (int i = 0; i < 5; ++i) {
        CHECK_MSG(Near(g.vertices[static_cast<std::size_t>(i)], expected[i]),
                  "vertex " + std::to_string(i) + " " + Str(g.vertices[static_cast<std::size_t>(i)]));
    }

    // The same by its properties: the base (v = 1) is W across and square to
    // the light direction; the far apex is length + 1 from the base's middle,
    // straight away from the light; the far corners fan wider than the base.
    const glm::vec2 base = g.vertices[3] - g.vertices[1];
    CHECK(Near(glm::length(base), g.width));
    CHECK(Near(glm::dot(base, glm::vec2(0.0f, -1.0f)), 0.0f));
    const glm::vec2 baseMiddle = 0.5f * (g.vertices[1] + g.vertices[3]);
    CHECK(Near(g.vertices[2] - baseMiddle, glm::vec2(0.0f, g.length + 1.0f)));
    CHECK(glm::length(g.vertices[0] - g.vertices[4]) > g.width);
}

void ShadowTurnsWithTheLight() {
    // Up and to the right: only the right sign convention for the turn puts
    // the base across the light and these numbers out.
    const Render::ShadowGeometry g =
        ShadowRenderer::ComputeShadow(Caster(), Light({160.0f, 20.0f, 10.0f}, false), glm::vec3(0.0f));
    CHECK(g.visible);
    CHECK_EQ(g.alpha8, 190);   // the same 3-D distance as above
    const glm::vec2 expected[5] = {{75.6106f, 185.3163f}, {122.9260f, 96.0987f}, {49.5260f, 167.2987f},
                                   {97.3260f, 76.8987f}, {24.9254f, 147.3024f}};
    for (int i = 0; i < 5; ++i) {
        CHECK_MSG(Near(g.vertices[static_cast<std::size_t>(i)], expected[i]),
                  "vertex " + std::to_string(i) + " " + Str(g.vertices[static_cast<std::size_t>(i)]));
    }
    const glm::vec2 towardLight = glm::normalize(glm::vec2(60.0f, -80.0f));
    CHECK(Near(glm::dot(g.vertices[3] - g.vertices[1], towardLight), 0.0f));
    // The apex lies away from the light.
    CHECK(glm::dot(g.vertices[2] - g.anchor, towardLight) < 0.0f);
}

void ShadowLengths() {
    Eth::SpriteDraw caster = Caster();
    const glm::vec3 above(100.0f, 0.0f, 10.0f);

    // Static caster, static light: 0.7.12 baked it at x8 (400) ...
    const Render::ShadowGeometry baked =
        ShadowRenderer::ComputeShadow(caster, Light(above, true), glm::vec3(0.0f), false);
    CHECK(baked.visible);
    CHECK(baked.baked);
    CHECK(Near(baked.length, 400.0f));
    // ... which the enhanced port cuts so that its visible end - v = 5/32,
    // where shadow.dds turns clear - is where the light stops reaching:
    // (sqrt(200^2 - 10^2) - 100 - 0 - 0.79 + 5/32) x 96/65 = 146.39, above the
    // live 100.
    const Render::ShadowGeometry capped =
        ShadowRenderer::ComputeShadow(caster, Light(above, true), glm::vec3(0.0f), true);
    CHECK_MSG(Near(capped.length, 146.3868f), std::to_string(capped.length));
    // That visible end, 5/32 of the way from the apex to the base's middle,
    // is then at the light's reach, and only the clear tail lies past it.
    const float reach = std::sqrt(200.0f * 200.0f - 100.0f);
    const glm::vec2 cappedBase = 0.5f * (capped.vertices[1] + capped.vertices[3]);
    const glm::vec2 visibleEnd = capped.vertices[2] + ShadowRenderer::kFadedV * (cappedBase - capped.vertices[2]);
    CHECK_MSG(Near(glm::length(visibleEnd - glm::vec2(100.0f, 0.0f)), reach, 1e-2f),
              std::to_string(glm::length(visibleEnd - glm::vec2(100.0f, 0.0f))));
    CHECK(glm::length(capped.vertices[2] - glm::vec2(100.0f, 0.0f)) > reach);
    // The cap may go under the live length - range 150 leaves
    // (sqrt(150^2 - 10^2) - 100.79 + 5/32) x 96/65 = 72.42 of the live 100 ...
    Eth::LightDraw shortReach = Light(above, true);
    shortReach.range = 150.0f;
    const Render::ShadowGeometry shorter = ShadowRenderer::ComputeShadow(caster, shortReach, glm::vec3(0.0f), true);
    CHECK(shorter.visible);
    CHECK_MSG(Near(shorter.length, 72.4173f), std::to_string(shorter.length));
    CHECK_EQ(shorter.alpha8, 140);   // (1 - 10100/22500) x 255 = 140.5
    // ... but not under the caster's height, 0.7.12's own minimum: range 134
    // leaves 48.73, and the shadow keeps 50.
    shortReach.range = 134.0f;
    const Render::ShadowGeometry floor = ShadowRenderer::ComputeShadow(caster, shortReach, glm::vec3(0.0f), true);
    CHECK(floor.visible);
    CHECK_MSG(Near(floor.length, 50.0f), std::to_string(floor.length));
    CHECK_EQ(floor.alpha8, 111);     // (1 - 10100/17956) x 255 = 111.6
    // A huge shadowLengthScale is cut all the same (pvp_lv2's 9.5): 400 x 9.5
    // uncapped, 146.39 capped, as at scale 1.
    caster.shadowLengthScale = 9.5f;
    CHECK(Near(ShadowRenderer::ComputeShadow(caster, Light(above, true), glm::vec3(0.0f), false).length, 3800.0f));
    CHECK(Near(ShadowRenderer::ComputeShadow(caster, Light(above, true), glm::vec3(0.0f), true).length, 146.3868f));
    caster.shadowLengthScale = 1.0f;
    // Right at the range's edge the alpha is under 8 and nothing is drawn:
    // (1 - 10100/10201) x 255 = 2.5.
    shortReach.range = 101.0f;
    CHECK(!ShadowRenderer::ComputeShadow(caster, shortReach, glm::vec3(0.0f), true).visible);
    // The baked pair asks no applyLight; a live pair does.
    caster.applyLight = false;
    CHECK(ShadowRenderer::ComputeShadow(caster, Light(above, true), glm::vec3(0.0f)).visible);
    CHECK(!ShadowRenderer::ComputeShadow(caster, Light(above, false), glm::vec3(0.0f)).visible);
    caster.applyLight = true;

    // Light above the caster's top (50 < 60): the ground-plane projection,
    // 100/10 x 60 - 100 = 500, clamped to 2.2 x 50 = 110. The same baked or
    // live in 0.7.12: only the other branch asks drawToTarget
    // (E:ETHRenderEntity.cpp:884-897) ...
    const Render::ShadowGeometry stretched =
        ShadowRenderer::ComputeShadow(caster, Light({100.0f, 0.0f, 60.0f}, false), glm::vec3(0.0f));
    CHECK(stretched.visible);
    CHECK_MSG(Near(stretched.length, 110.0f), std::to_string(stretched.length));
    const Render::ShadowGeometry stretchedBaked =
        ShadowRenderer::ComputeShadow(caster, Light({100.0f, 0.0f, 60.0f}, true), glm::vec3(0.0f), false);
    CHECK(stretchedBaked.baked);
    CHECK_MSG(Near(stretchedBaked.length, 110.0f), std::to_string(stretchedBaked.length));
    // ... and the enhanced cap leaves it: the light reaches sqrt(200^2 - 60^2)
    // = 190.79 at this height, (190.79 - 100.79 + 5/32) x 96/65 = 133.15.
    const Render::ShadowGeometry stretchedCapped =
        ShadowRenderer::ComputeShadow(caster, Light({100.0f, 0.0f, 60.0f}, true), glm::vec3(0.0f), true);
    CHECK_MSG(Near(stretchedCapped.length, 110.0f), std::to_string(stretchedCapped.length));
    // Projection shorter than the caster: at least h.
    const Render::ShadowGeometry minimum =
        ShadowRenderer::ComputeShadow(caster, Light({100.0f, 90.0f, 150.0f}, false), glm::vec3(0.0f));
    CHECK(minimum.visible);
    CHECK_MSG(Near(minimum.length, 50.0f), std::to_string(minimum.length));

    // shadowScale 0 means 1; shadowLengthScale multiplies after the minimum.
    caster.shadowScale = 0.0f;
    caster.shadowLengthScale = 1.5f;
    const Render::ShadowGeometry scaled =
        ShadowRenderer::ComputeShadow(caster, Light(above, false), glm::vec3(0.0f));
    CHECK(Near(scaled.width, 32.0f));
    CHECK(Near(scaled.length, 150.0f));
    caster.shadowScale = 1.5f;
    CHECK(Near(ShadowRenderer::ComputeShadow(caster, Light(above, false), glm::vec3(0.0f)).width, 48.0f));
}

void MenuBarrelShadow() {
    // menu.esc's barrel 117 (barrel.ent: vertical, static, 48x58, shadowScale
    // 0) by ground_fire 107's torch: owner (110, 326, 0) + (0, 0, 16), static,
    // castShadows, range 256, colour (1, 0.5, 0.3); ambient (0.2, 0, 0.2).
    // The torch is under the barrel's top (16 < 58), so 0.7.12 baked it x8.
    Eth::SpriteDraw barrel;
    barrel.entityId = 117;
    barrel.sprite = "barril.png";
    barrel.type = Eth::ET_VERTICAL;
    barrel.isStatic = true;
    barrel.applyLight = true;
    barrel.castShadow = true;
    barrel.shadowScale = 0.0f;
    barrel.shadowLengthScale = 1.0f;
    barrel.shadowOpacity = 1.0f;
    barrel.position = glm::vec3(69.0f, 404.0f, 0.0f);
    barrel.size = glm::vec2(48.0f, 58.0f);
    Eth::LightDraw torch;
    torch.ownerId = 107;
    torch.position = glm::vec3(110.0f, 326.0f, 16.0f);
    torch.color = glm::vec3(1.0f, 0.5f, 0.3f);
    torch.range = 256.0f;
    torch.isStatic = true;
    torch.castShadows = true;
    const glm::vec3 ambient(0.2f, 0.0f, 0.2f);

    const Render::ShadowGeometry original = ShadowRenderer::ComputeShadow(barrel, torch, ambient, false);
    CHECK(original.visible);
    CHECK(original.baked);
    CHECK(Near(original.width, 38.4f));
    CHECK_MSG(Near(original.length, 464.0f), std::to_string(original.length));
    // (1 - 8021/65536) x 1 x (1 - 0.4/3) x 255 = 193.95.
    CHECK_EQ(original.alpha8, 193);
    // Cut to the torch's reach, sqrt(256^2 - 16^2) = 255.50, 88.12 away:
    // (255.50 - 88.12 - 0.79 + 5/32) x 96/65 = 246.27 - not the 200 of a
    // strip cut at its apex, nor the live 116.
    const Render::ShadowGeometry live = ShadowRenderer::ComputeShadow(barrel, torch, ambient, true);
    CHECK_MSG(Near(live.length, 246.2718f, 1e-2f), std::to_string(live.length));
    CHECK_EQ(live.alpha8, 193);
    const glm::vec2 base = 0.5f * (live.vertices[1] + live.vertices[3]);
    const glm::vec2 visibleEnd = live.vertices[2] + ShadowRenderer::kFadedV * (base - live.vertices[2]);
    CHECK_MSG(Near(glm::length(visibleEnd - glm::vec2(110.0f, 326.0f)), std::sqrt(256.0f * 256.0f - 256.0f), 1e-2f),
              Str(visibleEnd));
}

void ShadowEarlyOutsAndAlpha() {
    Eth::SpriteDraw caster = Caster();
    const glm::vec3 above(100.0f, 0.0f, 10.0f);

    // The light behind the caster (z below it): no shadow on the floor.
    caster.position.z = 20.0f;
    CHECK(!ShadowRenderer::ComputeShadow(caster, Light(above, false), glm::vec3(0.0f)).visible);
    caster.position.z = 0.0f;
    // Out of range.
    Eth::LightDraw far = Light(above, false);
    far.range = 50.0f;
    CHECK(!ShadowRenderer::ComputeShadow(caster, far, glm::vec3(0.0f)).visible);
    // A light that casts none, an entity that casts none, nothing to draw.
    Eth::LightDraw noCast = Light(above, false);
    noCast.castShadows = false;
    CHECK(!ShadowRenderer::ComputeShadow(caster, noCast, glm::vec3(0.0f)).visible);
    caster.castShadow = false;
    CHECK(!ShadowRenderer::ComputeShadow(caster, Light(above, false), glm::vec3(0.0f)).visible);
    caster.castShadow = true;
    caster.sprite.clear();
    CHECK(!ShadowRenderer::ComputeShadow(caster, Light(above, false), glm::vec3(0.0f)).visible);
    caster.sprite = "crate.png";
    // Straight overhead: no direction to cast along.
    CHECK(!ShadowRenderer::ComputeShadow(caster, Light({100.0f, 100.0f, 50.0f}, false), glm::vec3(0.0f)).visible);

    // THE ALPHA. Ambient (0.2, 0.1, 0.2) leaves 1 - 0.5/3; opacity 0.5:
    // 0.7475 x 0.8333 x 255 x 0.5 = 79.4 -> 79. The colour's largest channel is 1.
    Eth::LightDraw torch = Light(above, false);
    torch.color = glm::vec3(1.0f, 0.7f, 0.3f);
    caster.shadowOpacity = 0.5f;
    CHECK_EQ(ShadowRenderer::ComputeShadow(caster, torch, glm::vec3(0.2f, 0.1f, 0.2f)).alpha8, 79);
    caster.shadowOpacity = 1.0f;
    // A dynamic light's colour is dimmed by its owner's particles: x 0.5 -> 95.
    Eth::LightDraw spell = Light(above, false);
    spell.particleRatio = 0.5f;
    CHECK_EQ(ShadowRenderer::ComputeShadow(caster, spell, glm::vec3(0.0f)).alpha8, 95);
    // A static one's is not.
    spell.isStatic = true;
    CHECK_EQ(ShadowRenderer::ComputeShadow(caster, spell, glm::vec3(0.0f)).alpha8, 190);
    // A caster above the floor fades by 1 - z/h: z 10 of 50 -> x 0.8 -> 152.
    caster.position.z = 10.0f;
    CHECK_EQ(ShadowRenderer::ComputeShadow(caster, Light({100.0f, 0.0f, 20.0f}, false), glm::vec3(0.0f)).alpha8, 152);
    caster.position.z = 0.0f;
    // A bright ambient leaves nothing to darken.
    CHECK(!ShadowRenderer::ComputeShadow(caster, Light(above, false), glm::vec3(1.0f)).visible);
}

// ---- LightRenderer's mapping -------------------------------------------------------

// ---- the bake eye and the light pass's alpha test (render/Lighting.cpp) ----------

// The menu's left devil statue, menu.esc id 104: devil.ent at (112, 292, 0),
// ET_VERTICAL, 139 x 145, pivot (0, -13), static, <Gloss>white_ground.jpg</Gloss>
// at power 30; drawn from floor(112, 292) - (69.5, 132) under ZAxisDirection (0, -1).
Eth::SpriteDraw MenuDevil() {
    Eth::SpriteDraw devil;
    devil.entityId = 104;
    devil.sprite = "devil_statue.png";
    devil.gloss = "white_ground.jpg";
    devil.type = Eth::ET_VERTICAL;
    devil.isStatic = true;
    devil.applyLight = true;
    devil.specularPower = 30.0f;
    devil.specularBrightness = 1.0f;
    devil.position = glm::vec3(112.0f, 292.0f, 0.0f);
    devil.size = glm::vec2(139.0f, 145.0f);
    devil.origin = glm::vec2(42.5f, 160.0f);
    return devil;
}

void StaticHighlightsAreSeenFromTheBakeEye(Render::TextureCache& textures) {
    // ETHScene::GenerateLightmaps moved the devil so its origin sat at the
    // target's corner at z 0, and the eye there was (L.x, 1.5 screenH, 768)
    // (ETHShaderManager::SetFakeEyePosition, drawToTarget): in the world, its
    // top edge 292 - 132 = 160 plus 1152, at z 0 + 768. The camera is no part
    // of it.
    Eth::RenderSnapshot menu;
    menu.zAxisDirection = glm::vec2(0.0f, -1.0f);
    const glm::vec3 fire(110.0f, 326.0f, 16.0f);   // ground_fire.ent 107's light
    const Eth::SpriteDraw devil = MenuDevil();
    CHECK_MSG(Near(Render::LightmapBakeEye(devil, menu, fire), glm::vec3(110.0f, 1312.0f, 768.0f)),
              Str(Render::LightmapBakeEye(devil, menu, fire)));
    Eth::RenderSnapshot moved = menu;
    moved.camera = glm::vec2(0.0f, 240.0f);
    CHECK(Near(Render::LightmapBakeEye(devil, moved, fire), glm::vec3(110.0f, 1312.0f, 768.0f)));

    // A flat tile at z -4 under the menus' axis, drawn 4 pixels lower than it
    // lies: the bake saw its unshifted top, 200 - 32, and its own z.
    Eth::SpriteDraw tile = Tile(-4.0f);
    tile.origin = glm::vec2(268.0f, 172.0f);
    CHECK_MSG(Near(Render::LightmapBakeEye(tile, menu, fire), glm::vec3(110.0f, 1320.0f, 764.0f)),
              Str(Render::LightmapBakeEye(tile, menu, fire)));

    // The lights: every static one is marked baked, no dynamic one.
    CHECK(LightRenderer::ComputeLight(Light(fire, true), 2.0f).baked);
    CHECK(!LightRenderer::ComputeLight(Light(fire, false), 2.0f).baked);

    if (!HaveOriginal()) {
        std::printf("  (bake eye sprite checks skipped: the original is not at %s)\n", PENUMBRA_ORIGINAL_DIR);
        return;
    }
    // The sprite asks for it only when it is static and has a highlight; the
    // engine's y is Ethanon's flipped.
    const Render::SpriteLighting a = Render::ComputeSpriteLighting(devil, menu, textures);
    CHECK(a.specularStrength > 0.0f);
    CHECK(a.bakedEye);
    CHECK_MSG(Near(a.bakedEyeY, -1312.0f), std::to_string(a.bakedEyeY));
    Eth::SpriteDraw moving = devil;
    moving.isStatic = false;
    CHECK_MSG(!Render::ComputeSpriteLighting(moving, menu, textures).bakedEye,
              "a dynamic sprite's lights were all live: the live eye");
    Eth::SpriteDraw matte = devil;
    matte.gloss.clear();
    CHECK_MSG(!Render::ComputeSpriteLighting(matte, menu, textures).bakedEye, "no highlight, no eye");
    Eth::RenderSnapshot noShaders = menu;
    noShaders.pixelShaders = false;
    CHECK(!Render::ComputeSpriteLighting(devil, noShaders, textures).bakedEye);
}

// Through the engine's transliteration of the light loop: pvp_lv5's glossy
// block (bloco_mario.ent at (352, 32, 0), 64 x 64, <Gloss>white_ground.jpg</Gloss>,
// power 50, static) under the arena's static huge_ambient_light (962, -225, 44),
// colour 0.4, range 3530, at two camera heights. The live eye slides the
// highlight with the camera; the bake eye holds it, and matches mainSpecular
// seen from 0.7.12's bake eye.
void BakedHighlightsHoldStillAsTheCameraMoves() {
    using Supersonic::GpuLight2D;
    namespace Light2D = Supersonic::Light2D;
    const float lightIntensity = 2.0f;
    const glm::vec3 lamp(962.0f, -225.0f, 44.0f);
    GpuLight2D light;
    light.position = glm::vec3(lamp.x, -lamp.y, lamp.z);
    light.range = 3530.0f;
    light.color = glm::vec3(0.4f) * lightIntensity;
    light.layers = LightRenderer::kStaticLayer | Supersonic::kLight2DBakedBit;

    Eth::SpriteDraw block = Tile(0.0f);
    block.position = glm::vec3(352.0f, 32.0f, 0.0f);
    block.origin = glm::vec2(320.0f, 0.0f);
    block.isStatic = true;
    Eth::RenderSnapshot arena;
    const glm::vec3 bakeEye = Render::LightmapBakeEye(block, arena, lamp);
    CHECK(Near(bakeEye, glm::vec3(962.0f, 1152.0f, 768.0f)));

    const glm::vec3 tint(0.5f);
    float liveMoved = 0.0f;
    float bakedMoved = 0.0f;
    float worst = 0.0f;
    // Flat, two bumps, and two facing the light's live half vector at the
    // first pixel (engine y): a white_ground.jpg bump caught the glints.
    for (const glm::vec3& facing : {glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.3f, -0.4f, 0.87f),
                                    glm::vec3(-0.2f, -0.6f, 0.77f), glm::vec3(0.883f, -0.311f, 0.350f),
                                    glm::vec3(0.8f, -0.45f, 0.4f)}) {
        const glm::vec3 normal = glm::normalize(facing);   // world axes, engine y
        for (const glm::vec2 pixel : {glm::vec2(330.0f, 10.0f), glm::vec2(370.0f, 50.0f)}) {
            const glm::vec3 surface(pixel.x, -pixel.y, 0.0f);
            glm::vec3 live[2];
            glm::vec3 baked[2];
            for (int c = 0; c < 2; ++c) {
                const float cameraY = c == 0 ? 0.0f : 200.0f;
                Light2D::Highlight highlight;
                highlight.gloss = glm::vec3(1.0f) / lightIntensity;
                highlight.power = 50.0f;
                highlight.eyeMirrorY = -(cameraY + 0.75f * 768.0f);
                highlight.eyeHeight = 768.0f;
                live[c] = Light2D::SpecularContribution(light, LightRenderer::kStaticLayer, surface, normal, tint,
                                                        highlight);
                highlight.bakedEye = true;
                highlight.bakedEyeY = -bakeEye.y;
                highlight.spriteHeight = 0.0f;
                baked[c] = Light2D::SpecularContribution(light, LightRenderer::kStaticLayer, surface, normal, tint,
                                                         highlight);
            }
            liveMoved = std::max(liveMoved, glm::length(live[1] - live[0]));
            bakedMoved = std::max(bakedMoved, glm::length(baked[1] - baked[0]));

            // mainSpecular (hPixelLight.cg) from the bake eye, in Ethanon's axes.
            const glm::vec3 p(pixel.x, pixel.y, 0.0f);
            const glm::vec3 n(normal.x, -normal.y, normal.z);
            const glm::vec3 toLight = glm::normalize(lamp - p);
            const glm::vec3 toEye = glm::normalize(bakeEye - p);
            const float d2 = glm::dot(lamp - p, lamp - p);
            const float att = 1.0f - d2 / (3530.0f * 3530.0f);
            const float diffuse = glm::dot(toLight, n);
            const float shine = std::pow(std::max(0.0f, glm::dot(n, glm::normalize(toLight + toEye))), 50.0f);
            const glm::vec3 expected = glm::clamp(
                (tint * diffuse * glm::vec3(0.4f) * lightIntensity + glm::vec3(0.4f) * shine * 1.0f) * att, 0.0f, 1.0f);
            worst = std::max(worst, glm::length(baked[0] - expected));
        }
    }
    CHECK_MSG(liveMoved > 1.0f / 255.0f, "the live eye moves a static highlight with the camera by " +
                                             std::to_string(liveMoved * 255.0f) + " levels");
    CHECK_MSG(bakedMoved == 0.0f, "the bake eye does not move it at all");
    CHECK_MSG(worst < 1e-4f, "and it is mainSpecular from 0.7.12's bake eye: " + std::to_string(worst));
    std::printf("  a camera 200 px lower moves pvp_lv5's block highlight by up to %.1f levels live, 0 baked\n",
                liveMoved * 255.0f);
}

void LitSpritesAlphaTestTheirPasses(Render::TextureCache& textures) {
    Eth::RenderSnapshot snapshot;
    const Eth::SpriteDraw tile = Tile(0.0f);
    CHECK(Render::ComputeSpriteLighting(tile, snapshot, textures).lightAlphaTest);
    Eth::SpriteDraw unlit = tile;
    unlit.applyLight = false;
    CHECK_MSG(!Render::ComputeSpriteLighting(unlit, snapshot, textures).lightAlphaTest, "no pass, no test");
    Eth::RenderSnapshot noShaders = snapshot;
    noShaders.pixelShaders = false;
    CHECK_MSG(!Render::ComputeSpriteLighting(tile, noShaders, textures).lightAlphaTest,
              "the per-vertex fallback's pass is not modelled");

    // The pass's alpha test through the engine's transliteration: the menu's
    // "Novo jogo" letters' faint glow (alpha 8/255, flat, no highlight) 150
    // pixels from green_light_menu - a pass alpha of 8/255 x 8/255 x facing x
    // falloff x 2, far under 1.5/255: 0.7.12 added none of the green, and the
    // capture showed the letters' edge grey, not tinted.
    namespace Light2D = Supersonic::Light2D;
    Supersonic::GpuLight2D green;
    green.position = glm::vec3(239.0f, -54.0f, 16.0f);
    green.range = 284.5f;
    green.color = glm::vec3(0.3f, 1.0f, 0.7f) * 2.0f;
    green.layers = 1u;
    Light2D::PassAlpha glow;
    glow.albedoAlpha = 8.0f / 255.0f;
    glow.intensity = 2.0f;
    const glm::vec3 surface(239.0f + 100.0f, -(54.0f + 110.0f), 10.0f);
    const glm::vec3 flat(0.0f, 0.0f, 1.0f);
    const glm::vec3 tint(0.2f);
    CHECK(Light2D::Contribution(green, 1, surface, flat, tint) != glm::vec3(0.0f));
    CHECK(Light2D::Contribution(green, 1, surface, flat, tint, &glow) == glm::vec3(0.0f));
}

void LightMapping() {
    Eth::LightDraw spell;
    spell.ownerId = 3;
    spell.position = glm::vec3(320.0f, 240.0f, -10.0f);
    spell.color = glm::vec3(0.6f, 0.6f, 1.0f);
    spell.range = 319.0f;
    spell.isStatic = false;
    spell.particleRatio = 0.5f;
    const Render::LightState a = LightRenderer::ComputeLight(spell, 2.0f);
    CHECK(a.enabled);
    CHECK(Near(a.position, glm::vec2(320.0f, -240.0f)));   // y flipped
    CHECK(Near(a.height, -10.0f));                          // the Ethanon z
    CHECK(Near(a.color, spell.color));                      // raw; intensity apart
    CHECK(Near(a.intensity, 1.0f));                         // 2 x 0.5
    CHECK(Near(a.range, 319.0f));
    CHECK_EQ(static_cast<int>(a.layers), static_cast<int>(LightRenderer::kDynamicLayer));

    Eth::LightDraw torch = spell;
    torch.isStatic = true;
    const Render::LightState b = LightRenderer::ComputeLight(torch, 2.0f, 1.05f);
    CHECK(Near(b.intensity, 2.1f));                         // not dimmed; flicker applied
    CHECK_EQ(static_cast<int>(b.layers), static_cast<int>(LightRenderer::kStaticLayer));

    torch.color = glm::vec3(0.0f);
    CHECK(!LightRenderer::ComputeLight(torch, 2.0f).enabled);
}

void HaloMapping() {
    // torch.ent's: (1, 0.7, 0.3) x 0.5, 144 px, halo1.bmp; the ratio applies
    // to the static owner's halo.
    Eth::LightDraw torch;
    torch.ownerId = 9;
    torch.position = glm::vec3(110.0f, 326.0f, 16.0f);
    torch.color = glm::vec3(1.0f, 0.7f, 0.3f);
    torch.isStatic = true;
    torch.haloBitmap = "halo1.bmp";
    torch.haloBrightness = 0.5f;
    torch.haloSize = 144.0f;
    torch.particleRatio = 0.5f;
    const Render::HaloState level = LightRenderer::ComputeHalo(torch, glm::vec2(0.0f), 12);
    CHECK(level.visible);
    CHECK(level.bitmapPath == "entities/halo1.bmp");
    CHECK(Near(level.position, glm::vec3(110.0f, -326.0f, Render::RankZ(12))));
    CHECK(Near(level.size, glm::vec2(144.0f)));
    CHECK_MSG(Near(glm::vec3(level.color), glm::vec3(0.25f, 0.175f, 0.075f)), Str(glm::vec3(level.color)));
    CHECK(Near(level.color.a, 1.0f));
    // In the menus it is drawn where the screen shows the light: z higher.
    const Render::HaloState menu = LightRenderer::ComputeHalo(torch, glm::vec2(0.0f, -1.0f), 12);
    CHECK(Near(menu.position, glm::vec3(110.0f, -(326.0f - 16.0f), Render::RankZ(12))));

    // ConvertToDW: over 1 normalises, it does not clamp.
    CHECK(Near(LightRenderer::ConvertToDW(glm::vec3(2.0f, 0.0f, 0.0f)), glm::vec3(1.0f, 0.0f, 0.0f)));
    CHECK(Near(LightRenderer::ConvertToDW(glm::vec3(1.2f, 0.9f, 0.0f)), glm::vec3(0.8f, 0.6f, 0.0f)));
    CHECK(Near(LightRenderer::ConvertToDW(glm::vec3(0.4f, 1.0f, 0.2f)), glm::vec3(0.4f, 1.0f, 0.2f)));

    torch.haloSize = 0.0f;
    CHECK(!LightRenderer::ComputeHalo(torch, glm::vec2(0.0f), 12).visible);
    torch.haloSize = 144.0f;
    torch.haloBitmap.clear();
    CHECK(!LightRenderer::ComputeHalo(torch, glm::vec2(0.0f), 12).visible);
}

void TorchFlicker() {
    float lowest = 2.0f;
    float highest = 0.0f;
    bool bounded = true;
    for (Eth::uint t = 0; t < 20000; t += 17) {
        const float f = LightRenderer::TorchFlickerFactor(5, t);
        bounded = bounded && f >= 1.0f - LightRenderer::kTorchFlickerAmplitude - 1e-6f &&
                  f <= 1.0f + LightRenderer::kTorchFlickerAmplitude + 1e-6f;
        lowest = std::min(lowest, f);
        highest = std::max(highest, f);
    }
    CHECK(bounded);
    CHECK(highest - lowest > LightRenderer::kTorchFlickerAmplitude);   // it does move
    CHECK(LightRenderer::TorchFlickerFactor(5, 123456) == LightRenderer::TorchFlickerFactor(5, 123456));
    CHECK(LightRenderer::TorchFlickerFactor(5, 1000) != LightRenderer::TorchFlickerFactor(6, 1000));
    // Late in a long run the millisecond still moves it.
    CHECK(LightRenderer::TorchFlickerFactor(5, 400000000u) != LightRenderer::TorchFlickerFactor(5, 400000033u));

    Eth::RenderSnapshot snapshot;
    Eth::ParticleDraw flame;
    flame.ownerId = 5;
    flame.alphaMode = Eth::AM_ADD;
    Eth::ParticleDraw aura;             // checkpoint.ent's dark smoke: not a flame
    aura.ownerId = 6;
    aura.alphaMode = Eth::AM_MODULATE;
    snapshot.particles = {flame, aura, flame};
    const std::vector<int> owners = LightRenderer::FlameOwners(snapshot);
    CHECK_EQ(owners.size(), std::size_t{1});
    Eth::LightDraw light;
    light.ownerId = 5;
    light.isStatic = true;
    CHECK(LightRenderer::IsTorch(light, owners));
    light.isStatic = false;   // a spell with sparks: dimmed by its ratio, not flickered
    CHECK(!LightRenderer::IsTorch(light, owners));
    light.isStatic = true;
    light.ownerId = 6;    // the aura's owner
    CHECK(!LightRenderer::IsTorch(light, owners));
}

// ---- the pools, on a bare registry -------------------------------------------------

std::size_t Count2DLights(entt::registry& registry, bool enabledOnly) {
    std::size_t n = 0;
    for (auto e : registry.view<Supersonic::Light2DComponent>()) {
        if (!enabledOnly || registry.get<Supersonic::Light2DComponent>(e).enabled) ++n;
    }
    return n;
}

void LightPool(Render::TextureCache& textures) {
    entt::registry registry;
    LightRenderer renderer;
    renderer.Attach(registry, textures);
    Render::View view;
    Render::DrawOrder order;
    order.haloRank = 50;

    Eth::RenderSnapshot snapshot;
    snapshot.timeMs = 5000;
    for (int i = 0; i < 3; ++i) {
        Eth::LightDraw light;
        light.ownerId = 100 + i;
        light.position = glm::vec3(100.0f * static_cast<float>(i), 50.0f, 8.0f);
        light.color = glm::vec3(1.0f, 0.7f, 0.3f);
        light.range = 227.5f;
        light.isStatic = true;
        snapshot.lights.push_back(light);
    }
    snapshot.lights[0].haloBitmap = "halo1.bmp";
    snapshot.lights[0].haloSize = 144.0f;
    snapshot.lights[0].haloBrightness = 0.5f;
    Eth::ParticleDraw flame;
    flame.ownerId = 100;   // light 0 is a torch
    flame.alphaMode = Eth::AM_ADD;
    snapshot.particles.push_back(flame);

    renderer.Draw(registry, snapshot, view, order);
    CHECK_EQ(renderer.SlotCount(), std::size_t{3});
    CHECK_EQ(renderer.EnabledCount(), std::size_t{3});
    CHECK_EQ(Count2DLights(registry, true), std::size_t{3});
    const bool haloReadable = !textures.Key("entities/halo1.bmp", Render::TextureVariant::Additive).empty();
    CHECK_EQ(renderer.HaloCount(), haloReadable ? std::size_t{1} : std::size_t{0});

    // Every intensity as the mapping says; the torch's flickers.
    std::vector<float> first;
    for (auto e : registry.view<Supersonic::Light2DComponent>()) {
        first.push_back(registry.get<Supersonic::Light2DComponent>(e).intensity);
    }
    bool flickered = false;
    for (float intensity : first) flickered = flickered || !Near(intensity, 2.0f, 1e-6f);
    CHECK(flickered);

    // Static lights are the baked ones (render/Lighting.cpp, THE BAKED EYE),
    // and the frame carries the scene's unflickered lightIntensity for the
    // light pass's alpha test.
    bool allBaked = true;
    for (auto e : registry.view<Supersonic::Light2DComponent>()) {
        allBaked = allBaked && registry.get<Supersonic::Light2DComponent>(e).baked;
    }
    CHECK(allBaked);
    const auto* alphaTest = registry.ctx().find<Supersonic::Light2DAlphaTest>();
    CHECK(alphaTest != nullptr && Near(alphaTest->intensity, snapshot.lightIntensity, 1e-6f));

    // Drawn again: nothing made, nothing changed.
    const std::size_t entities = registry.storage<entt::entity>().free_list();
    renderer.Draw(registry, snapshot, view, order);
    CHECK_EQ(registry.storage<entt::entity>().free_list(), entities);
    std::vector<float> second;
    for (auto e : registry.view<Supersonic::Light2DComponent>()) {
        second.push_back(registry.get<Supersonic::Light2DComponent>(e).intensity);
    }
    CHECK(first == second);

    // Flicker off: the steady lightIntensity.
    renderer.SetTorchFlicker(false);
    renderer.Draw(registry, snapshot, view, order);
    bool steady = true;
    for (auto e : registry.view<Supersonic::Light2DComponent>()) {
        steady = steady && registry.get<Supersonic::Light2DComponent>(e).intensity == 2.0f;
    }
    CHECK(steady);

    // A light leaves: its slot is kept, switched off.
    snapshot.lights.erase(snapshot.lights.begin() + 1);
    renderer.Draw(registry, snapshot, view, order);
    CHECK_EQ(renderer.SlotCount(), std::size_t{3});
    CHECK_EQ(Count2DLights(registry, true), std::size_t{2});
    // A newcomer takes it rather than growing the pool.
    Eth::LightDraw newcomer = snapshot.lights[1];
    newcomer.ownerId = 200;
    snapshot.lights.push_back(newcomer);
    renderer.Draw(registry, snapshot, view, order);
    CHECK_EQ(renderer.SlotCount(), std::size_t{3});
    CHECK_EQ(Count2DLights(registry, true), std::size_t{3});

    // Past the engine's 64: the rest, in snapshot order, are left off.
    snapshot.lights.clear();
    for (int i = 0; i < 70; ++i) {
        Eth::LightDraw light;
        light.ownerId = 1000 + i;
        light.color = glm::vec3(1.0f);
        light.range = 100.0f;
        snapshot.lights.push_back(light);
    }
    renderer.Draw(registry, snapshot, view, order);
    CHECK_EQ(renderer.EnabledCount(), LightRenderer::kMaxLights);
    CHECK_EQ(renderer.DroppedCount(), std::size_t{6});
    CHECK_EQ(Count2DLights(registry, true), LightRenderer::kMaxLights);

    renderer.Detach(registry);
    CHECK_EQ(Count2DLights(registry, false), std::size_t{0});
}

void ShadowPool(Render::TextureCache& textures) {
    // No MeshRegistry on a bare registry: slots are made and kept, nothing is
    // shown (a shadow is visible only once its mesh exists).
    entt::registry registry;
    ShadowRenderer renderer;
    renderer.Attach(registry, textures);
    Render::View view;

    Eth::RenderSnapshot snapshot;
    // A room with something left to darken: the snapshot's default ambient is
    // (1, 1, 1), under which 0.7.12's alpha, x (1 - mean ambient), is 0 and
    // no shadow - and so no slot - is made at all.
    snapshot.ambient = glm::vec3(0.2f, 0.1f, 0.2f);   // level1's
    snapshot.sprites = {Caster(), Tile(0.0f)};
    snapshot.lights = {Light({100.0f, 0.0f, 10.0f}, true), Light({180.0f, 60.0f, 10.0f}, false)};
    snapshot.lights[1].ownerId = 8;
    Render::DrawOrder order;
    order.spriteRank = {1, 2};
    order.shadowRankBase = {0, 1};

    // Both pairs are drawn: 158 and 169 of 255.
    CHECK(ShadowRenderer::ComputeShadow(snapshot.sprites[0], snapshot.lights[0], snapshot.ambient).visible);
    CHECK(ShadowRenderer::ComputeShadow(snapshot.sprites[0], snapshot.lights[1], snapshot.ambient).visible);

    const bool textureReadable = !textures.Key("data/shadow.dds", Render::TextureVariant::Plain).empty();
    renderer.Draw(registry, snapshot, view, order);
    // The caster with both lights; the tile casts none.
    CHECK_EQ(renderer.SlotCount(), (textureReadable ? std::size_t{2} : std::size_t{0}));
    CHECK_EQ(renderer.ShownCount(), std::size_t{0});
    const std::size_t entities = registry.storage<entt::entity>().free_list();
    renderer.Draw(registry, snapshot, view, order);
    CHECK_EQ(registry.storage<entt::entity>().free_list(), entities);
    CHECK_EQ(renderer.RebuildCount(), std::size_t{0});

    // A duplicated pair gets its own slot, once - not a new one every frame.
    snapshot.sprites.push_back(Caster());
    order.spriteRank.push_back(3);
    order.shadowRankBase.push_back(2);
    renderer.Draw(registry, snapshot, view, order);
    renderer.Draw(registry, snapshot, view, order);
    CHECK_EQ(renderer.SlotCount(), (textureReadable ? std::size_t{4} : std::size_t{0}));

    renderer.Detach(registry);
    CHECK_EQ(registry.storage<entt::entity>().free_list(), std::size_t{0});
}

} // namespace

int main() {
    // Game root = the original's folder; nothing here needs it but the checks
    // that say so, and a missing folder only empties their keys.
    Render::TextureCache textures(PENUMBRA_ORIGINAL_DIR);
    entt::registry bare;
    textures.Attach(bare);

    AmbientIsAmbientPlusEmissiveForEverySprite(textures);
    NormalMapComesFromEntitiesNormalmaps(textures);
    VerticalSpritesStandOnTheirBaseLine(textures);
    GlossMapsGiveAHighlight(textures);
    ShadowStripMatchesTheShader();
    ShadowTurnsWithTheLight();
    ShadowLengths();
    MenuBarrelShadow();
    ShadowEarlyOutsAndAlpha();
    LightMapping();
    StaticHighlightsAreSeenFromTheBakeEye(textures);
    BakedHighlightsHoldStillAsTheCameraMoves();
    LitSpritesAlphaTestTheirPasses(textures);
    HaloMapping();
    TorchFlicker();
    LightPool(textures);
    ShadowPool(textures);
    return test::summary("test_pn_render_lights", 100);
}
