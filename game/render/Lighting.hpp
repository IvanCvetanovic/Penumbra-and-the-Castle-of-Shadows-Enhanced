#pragma once

// How one drawn entity is lit, decided once so the sprite renderer and the
// light renderer agree. Implemented in render/Lighting.cpp (the lights' side).
//
// The original (docs/spec/21-formats-particles-shaders.md): an ambient pass
// clamp(T*C*min(1, ambient+emissive)), then per light an additive pass
// T*C*(N.L)*attenuation*lightColour*lightIntensity through the renormalised
// normal map; static lights reached static sprites through a lightmap baked at
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
    // Sprite2DLight::height: the lighting height of its surface.
    float height = 0.0f;
    // Sprite2DLight::normalYDown for the original's (DirectX-convention) maps.
    bool normalYDown = true;
    // TextureCache key of its normal map ("" = flat).
    std::string normalKey;
};

SpriteLighting ComputeSpriteLighting(const Eth::SpriteDraw& sprite, const Eth::RenderSnapshot& snapshot,
                                     TextureCache& textures);

} // namespace Penumbra::Render
