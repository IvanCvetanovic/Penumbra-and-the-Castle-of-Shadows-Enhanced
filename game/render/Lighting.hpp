#pragma once

// How one drawn entity is lit, decided once so the sprite renderer and the
// light renderer agree. Implemented in render/Lighting.cpp (the lights' side).
//
// The original (docs/spec/21-formats-particles-shaders.md): an ambient pass
// clamp(T*C*min(1, ambient+emissive)), then per light an additive pass
// T*C*(N.L)*attenuation*lightColour*lightIntensity through the renormalised
// normal map (stood up for ET_VERTICAL, plus a gloss-map highlight for a
// <Gloss>); static lights reached static sprites through a lightmap baked at
// load. The enhanced port lights everything live with the engine's
// normal-mapped Light2D (static lights included - no lightmap), which is also
// what lets torches flicker.

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

#include "eth/Snapshot.hpp"
#include "render/TextureCache.hpp"

namespace Penumbra::Render {

struct SpriteLighting {
    // MaterialComponent::sprite2D.ambient: min(1, sceneAmbient + emissive) per
    // channel (MPR's Lighting::AmbientTerm) for EVERY sprite, lit or not:
    // 0.7.12 drew each entity's ambient pass that way whatever its applyLight
    // (E:ETHRenderEntity.cpp:687-691).
    glm::vec3 ambient{1.0f};
    // Whether lights reach it at all: applyLight. With pixel shaders off
    // 0.7.12 still lit it, per vertex without the normal map
    // (E:ETHShaderManager.cpp:327-341), so pixelShaders only drops normalKey.
    bool lit = false;
    // Sprite2DLight::lightMask: which Light2D layers reach it.
    std::uint8_t lightMask = 0;
    // Sprite2DLight::height: the lighting height of its surface (for a
    // vertical sprite, of the texels on its base line).
    float height = 0.0f;
    // Sprite2DLight::normalYDown for the original's (DirectX-convention) maps.
    bool normalYDown = true;
    // TextureCache key of its normal map ("" = flat).
    std::string normalKey;
    // Sprite2DLight::vertical / verticalBaseY: an ET_VERTICAL entity lit as a
    // plane standing on the engine line y = verticalBaseY (vPixelLight.cg),
    // only when lights reach it.
    bool vertical = false;
    float verticalBaseY = 0.0f;
    // TextureCache key of its gloss map ("" = no highlight) and the highlight's
    // Sprite2DLight::specularStrength (0 = off) and specularPower.
    std::string glossKey;
    float specularStrength = 0.0f;
    float specularPower = 50.0f;
};

class Localization;

// `localization` (optional) swaps a normal or gloss map with words in it (the
// menu buttons') for its variant in the current language, as the sprite
// renderer swaps the image itself (E5).
SpriteLighting ComputeSpriteLighting(const Eth::SpriteDraw& sprite, const Eth::RenderSnapshot& snapshot,
                                     TextureCache& textures, const Localization* localization = nullptr);

} // namespace Penumbra::Render
