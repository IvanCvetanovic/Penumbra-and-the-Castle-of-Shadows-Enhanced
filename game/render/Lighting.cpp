// How one drawn entity is lit (render/Lighting.hpp), the lights' side of it.
//
// THE AMBIENT. 0.7.12 drew EVERY entity's ambient pass with
//   colour = min(1, sceneAmbient + emissive.rgb) per channel, times v4Color
// (E:ETHRenderEntity.cpp:687-691), whether or not it applied light: applyLight
// only decides whether light passes are added on top (BeginLightPass,
// E:ETHShaderManager.cpp:217). So an applyLight=0 tree with emissive 0 renders
// at the ambient alone - very dark in level1's (0.2, 0.1, 0.2) - and an
// emissive (1,1,1) sprite (crystals, lava, the logo) is fully bright whatever
// the room (docs/spec/21 §2.2). Emissive's alpha is ignored.
//
// THE Y FLIP, verified. Ethanon lights in its world axes: +x right, +y DOWN
// the screen, +z towards the viewer, with the normal map decoded as
// N = normalize(2 nm - 1) in those same axes - red +x, green +y (down the
// image, the DirectX convention), blue +z - and the diffuse term
//   dot(normalize(P - L), -N) = dot(normalize(L - P), N)
// (hPixelLight.cg:69,113; docs/spec/21 §2.3). The engine is the mirror image in
// y: M(x, y, z) = (x, -y, z), render/View.hpp's ToWorld. M is orthogonal, so
//   dot(M(L - P), M N) = dot(L - P, N)   and   |M(L - P)| = |L - P|,
// i.e. the diffuse and the attenuation are unchanged PROVIDED the engine sees
//   L_eng = (lx, -ly, lz)   (LightRenderer: ToWorld(light xy), height = light z)
//   P_eng = (px, -py, z)    (the fragment's world xy; height below)
//   N_eng = (Nx, -Ny, Nz).
// shadeSprite2D builds n = normalize(model[0]) * c.x + normalize(model[1]) * c.y
// + (0, 0, c.z) from c = 2 tex - 1, with c.y negated under normalYDown. For a
// sprite quad drawn unrotated and unmirrored, model[0] and model[1] point along
// +x and +y, so normalYDown = true gives exactly (Nx, -Ny, Nz) = N_eng. That
// holds because the engine's Quad maps texture v = 0 (the image's top row) to
// its +y edge (ModelLoader::GenerateQuad), so "down the image" is engine -y.
// TextureCache's Normal variant renormalises each texel at load, as the 2010
// Cg did per pixel; shadeSprite2D does not renormalise.
//
// THE HEIGHT.
//   Every type but ET_VERTICAL: 0.7.12's light pass placed every pixel at the
//   entity's z (pixelLightVS.cg sprite_ppl: topLeft3DPos + (u w, v h, 0)), so
//   Sprite2DLight::height = position.z, exactly.
//
//   ET_VERTICAL cannot be exact. 0.7.12 stood the sprite up in the XZ plane:
//   pixel (u, v) at (x - ox + u w, y, z + oy - v h), with the normal swizzled to
//   (n.x, n.z, -n.y) so a flat map faces +y, down the screen
//   (pixelLightVS.cg verticalSprite_ppl, vPixelLight.cg:67-69). The engine has
//   one plane and one height per sprite. The row drawn at screen y is at height
//     z + (S.y - drawnY),  S = ToScreenPos(position) = the point it is drawn from,
//   so the port uses the MIDDLE row's height,
//     height = z + (S.y - (origin.y + size.y / 2)).
//   Why the middle and not the base: it is the mean of the heights 0.7.12 lit
//   the sprite with, so the light's distance to the sprite (and the falloff) is
//   right on average; and the face-on term the engine computes, (lz - height),
//   comes out near the one 0.7.12 computed, (ly - y), for the lights that sit
//   near a vertical sprite's base in the shipped scenes. menu.esc's barrel at
//   (45, 336, 0), pivot -14, next to ground_fire's light at (110, 326, 16):
//   0.7.12's face-on factor is (326 - 336) = -10, unlit; the middle row's
//   height is 15, so the engine's is (16 - 15) = 1, nearly edge-on; the base's
//   would be 16, lit full face. Its sides, lit through the map's x, match in
//   both. What differs: 0.7.12's vertical rows also shifted the facing through
//   the map's green (-n.y times the row's height), which the engine replaces
//   with the row's screen offset.
//
// THE MASK. Lights reach a sprite when it applies light. With pixel shaders
// off (the menu's "Desativa pixel shaders"), 0.7.12 did NOT stop lighting: it
// switched to per-vertex light with a fixed normal and no map
// (E:ETHShaderManager.cpp:327-341, h/vVertexLightShader.cg). The closest the
// engine has is per-pixel light through the flat normal, so that is what
// kLightWithoutPixelShaders keeps. Specular (<Gloss>, 253 definitions) has no
// engine counterpart and is left out.

#include "render/Lighting.hpp"

#include <algorithm>
#include <utility>

#include "render/LightRenderer.hpp"
#include "render/Localization.hpp"

namespace Penumbra::Render {

namespace {

// See THE MASK above: false would draw a sprite at its ambient alone once pixel
// shaders are off, which 0.7.12 never did.
constexpr bool kLightWithoutPixelShaders = true;

// 0.7.12 looked normal maps up in entities\normalmaps\ (E:ETHCommon.h:77).
const std::string kNormalFolder = "entities/normalmaps/";

float LightingHeight(const Eth::SpriteDraw& sprite, const Eth::RenderSnapshot& snapshot) {
    if (sprite.type != Eth::ET_VERTICAL) return sprite.position.z;
    // S.y: where 0.7.12 drew the entity from (ToScreenPos, E:ETHCommon.h:423-426).
    const float anchorY = sprite.position.y + snapshot.zAxisDirection.y * sprite.position.z;
    const float middleRowY = sprite.origin.y + 0.5f * sprite.size.y;
    return sprite.position.z + (anchorY - middleRowY);
}

} // namespace

SpriteLighting ComputeSpriteLighting(const Eth::SpriteDraw& sprite, const Eth::RenderSnapshot& snapshot,
                                     TextureCache& textures, const Localization* localization) {
    SpriteLighting lighting;
    lighting.ambient = glm::min(glm::vec3(1.0f), snapshot.ambient + glm::vec3(sprite.emissive));
    lighting.lit = sprite.applyLight && (snapshot.pixelShaders || kLightWithoutPixelShaders);
    lighting.lightMask = lighting.lit ? LightRenderer::kAllLayers : std::uint8_t{0};
    lighting.height = LightingHeight(sprite, snapshot);
    lighting.normalYDown = true;
    // A sprite no light reaches has no use for a normal map (and naming none
    // keeps it in the material set its image's other copies share). Without
    // pixel shaders 0.7.12 read no map at all. A missing <Normal>, or one that
    // does not read, lights flat - 0.7.12 fell back to data/default_nm.png,
    // (127, 127, 255), which is the engine's flat default too.
    if (lighting.lit && snapshot.pixelShaders && !sprite.normal.empty()) {
        // ENHANCED (E5): the menu buttons' map embosses their Portuguese; its
        // English variant (strings.json "images") embosses the English labels
        // the albedo shows. The key is the variant's own path, so a language
        // switch lands on a different texture.
        std::string path = kNormalFolder + sprite.normal;
        if (localization != nullptr) {
            std::string variant = localization->ImageVariant(path, localization->CurrentLanguage());
            if (!variant.empty()) path = std::move(variant);
        }
        lighting.normalKey = textures.Key(path, TextureVariant::Normal);
    }
    return lighting;
}

} // namespace Penumbra::Render
