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
//   ET_VERTICAL stands up. 0.7.12 lit the sprite in the XZ plane: pixel (u, v)
//   at (x - ox + u w, y, z + oy - v h), with the normal swizzled to
//   (n.x, n.z, -n.y) so a flat map faces +y, down the screen
//   (pixelLightVS.cg verticalSprite_ppl, vPixelLight.cg:67-69). The engine's
//   Sprite2DLight::vertical (engine f30df7c) turns the flat sprite's frame a
//   quarter turn about x through the line y = verticalBaseY:
//     P = (fragment x, verticalBaseY, height + (fragment y - verticalBaseY))
//     N = (n.x, -n.z, n.y)
//   which through the Y FLIP above is exactly 0.7.12's P and N, given
//     verticalBaseY = -position.y   (every texel lit at the entity's own y,
//                                    not where the menus' ZAxisDirection drew it)
//     height = z + (S.y - y) = z + ZAxisDirection.y * z,
//   S = ToScreenPos(position), the point it is drawn from: the row drawn at
//   screen y_d is at 0.7.12's height z + (S.y - y_d), and the fragment there
//   has engine y = -y_d. Before engine f30df7c the port lit the whole sprite
//   at its middle row's height, which left menu.esc's devil statues dark
//   where the original shows them bronze.
//
// THE MASK. Lights reach a sprite when it applies light. With pixel shaders
// off (the menu's "Desativa pixel shaders"), 0.7.12 did NOT stop lighting: it
// switched to per-vertex light with a fixed normal and no map
// (E:ETHShaderManager.cpp:327-341, h/vVertexLightShader.cg). The closest the
// engine has is per-pixel light through the flat normal, so that is what
// kLightWithoutPixelShaders keeps.
//
// THE HIGHLIGHT. An entity with a <Gloss> was drawn with mainSpecular
// (h/vPixelLight.cg), which adds to each light's diffuse term, the two then
// attenuated together,
//   lightColor * pow(saturate(N.H), specularPower) * T.a * gloss * specularBrightness
// with H halfway between the light and the fake eye (ETHShaderManager.cpp:
// 75-90; PenumbraLayer publishes it as Light2DEye). The engine's
// Sprite2DLight::specularStrength is that sum's gloss multiplier; its light
// colour is colour x lightIntensity where 0.7.12's highlight took the colour
// alone, so the strength is specularBrightness / lightIntensity. Only with
// pixel shaders on: the per-vertex fallback had no highlight
// (h/vVertexLightShader.cg). A gloss file that does not read gives no
// highlight, as a null m_pGloss chose the plain light shader
// (ETHShaderManager.cpp:237). Not modelled: the light pass's alpha test (a
// texel at alpha <= 1/255 got no light at all, highlight included), and the
// highlights 0.7.12 baked into static sprites' lightmaps from the first
// frame's eye - every light is live here (E9), so they follow the camera.

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

// 0.7.12 looked normal maps up in entities\normalmaps\ (E:ETHCommon.h:77), and
// gloss maps in entities\ (ETHRenderEntity.cpp:335, ETH_ENTITY_FOLDER).
const std::string kNormalFolder = "entities/normalmaps/";
const std::string kGlossFolder = "entities/";

float LightingHeight(const Eth::SpriteDraw& sprite, const Eth::RenderSnapshot& snapshot) {
    if (sprite.type != Eth::ET_VERTICAL) return sprite.position.z;
    // z + (S.y - y), S.y being where 0.7.12 drew the entity from (ToScreenPos,
    // E:ETHCommon.h:423-426): the height of the texels on the base line.
    return sprite.position.z + snapshot.zAxisDirection.y * sprite.position.z;
}

// The file name alone, as Scene.cpp loads a <Gloss> (AddSpriteResource).
std::string BaseName(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

// `relativePath` in the current language when strings.json has a variant
// (the menu buttons' maps carry their labels), else as it is.
std::string InLanguage(std::string relativePath, const Localization* localization) {
    if (localization != nullptr) {
        std::string variant = localization->ImageVariant(relativePath, localization->CurrentLanguage());
        if (!variant.empty()) return variant;
    }
    return relativePath;
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
    if (lighting.lit && sprite.type == Eth::ET_VERTICAL) {
        lighting.vertical = true;
        lighting.verticalBaseY = -sprite.position.y;   // ToWorld's y
    }
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
        lighting.normalKey = textures.Key(InLanguage(kNormalFolder + sprite.normal, localization),
                                          TextureVariant::Normal);
    }
    // THE HIGHLIGHT above. Plain: no gloss map shipped holds a magenta texel,
    // so 0.7.12's colour key changed none of them, and the engine samples the
    // map as data. The menu buttons' gloss masks their labels, so it follows
    // the language as the normal map does.
    if (lighting.lit && snapshot.pixelShaders && !sprite.gloss.empty() && sprite.specularBrightness > 0.0f &&
        sprite.specularPower > 0.0f && snapshot.lightIntensity > 0.0f) {
        lighting.glossKey =
            textures.Key(InLanguage(kGlossFolder + BaseName(sprite.gloss), localization), TextureVariant::Plain);
        if (!lighting.glossKey.empty()) {
            lighting.specularStrength = sprite.specularBrightness / snapshot.lightIntensity;
            lighting.specularPower = sprite.specularPower;
        }
    }
    return lighting;
}

} // namespace Penumbra::Render
