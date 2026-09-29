#pragma once

// Where the original's screen meets the engine's world. Shared by every
// renderer in render/.
//
// ETHANON: pixels, +x right, +y DOWN; the camera is the world position of the
// screen's TOP-LEFT; the logical screen is GetScreenSize() (1024x768 in the
// original, wider in the enhanced widescreen view).
//
// ENGINE: one world unit per original pixel, +x right, +y UP, an orthographic
// camera looking down -Z whose orthoHeight is the world height the image shows
// (CameraRig), and transparent quads ordered back to front by view depth, i.e.
// by world z (a larger z is nearer and drawn later), then by
// RenderableComponent::sortKey (RenderSystem::SortTransparentDraws).
//
// THE ONE CONVENTION every renderer here follows (checked across all of them):
//   POSITION  Ethanon (x, y) is engine (x, -y): ToWorld. Every snapshot
//             position a renderer places is already where 0.7.12 DREW it
//             (SpriteDraw::origin and ParticleDraw::position include
//             ZAxisDirection * z; a halo adds it itself), except the two that
//             0.7.12 did not shift either: a light's lighting position
//             (LightRenderer) and a shadow's anchor (ShadowRenderer,
//             E:ETHRenderEntity.cpp:851 draws at the entity's own xy).
//   SIZE      quads are the engine's unit "Quad" (ModelLoader::GenerateQuad:
//             centred, v = 0 at its +y edge, i.e. the image's top row up)
//             scaled by POSITIVE pixel sizes, never mirrored, so an image
//             decoded top row first (TextureDecode.hpp) shows upright and a
//             normal map's "down the image" is engine -y (normalYDown = true).
//   ANGLE     a positive Ethanon angle turns COUNTER-CLOCKWISE on screen - GS2D
//             builds RotateZ as rows (cos, sin)/(-sin, cos), uploads it
//             row-major and multiplies in y-down pixels, so +90 turns
//             screen-down into screen-right (ParticleRenderer.cpp). The
//             engine's +z rotation is counter-clockwise on screen too, so
//             TransformComponent::rotation.z = +radians(angle), no negation.
//             Shadows need no angle: ShadowRenderer builds the strip from the
//             light direction in Ethanon pixels and flips each vertex.
//   ORDER     draw order is world z: z = RankZ(rank), ranks from
//             ComputeDrawOrder (DrawOrder.hpp), the background image at
//             RankZ(-1). The Ethanon z of an entity is NOT the engine z.
//   HEIGHT    the Ethanon z is the lighting height (Sprite2DLight::height,
//             Light2DComponent::height), in pixels like x and y, so the light
//             to fragment vector keeps its length and the diffuse its value
//             under the mirror (render/Lighting.cpp, THE Y FLIP).
//   SCREEN    HUD primitives and pillarbox bars are ScreenOverlay quads in
//             fractions of the image (+y down, no flip): HudToFraction.
//             The bars cover what is not shown (ShownMin/ShownMax): the
//             logical screen's box, or wider under E1's open sides.
//   POINTER   Input::MousePosition() is in the same coordinates as
//             ViewportInfo::rect; the image starts at imageOrigin there, and a
//             logical pixel is (mouse - imageOrigin - viewportMin) / scale
//             (InputMapper::WindowToLogical).

#include <algorithm>

#include <glm/glm.hpp>

#include "eth/EthTypes.hpp"

namespace Penumbra::Render {

inline constexpr float kLayerBase = -500.0f;   // behind everything drawn by rank
inline constexpr float kLayerStep = 0.01f;     // 100000 ranks fit before kLayerBase+1000

inline glm::vec3 ToWorld(const glm::vec2& ethanon, float engineZ) { return {ethanon.x, -ethanon.y, engineZ}; }
inline glm::vec2 ToEthanon(const glm::vec3& world) { return {world.x, -world.y}; }
inline float RankZ(int rank) { return kLayerBase + static_cast<float>(rank) * kLayerStep; }

// The frame's view: what the layer computed from the window and the snapshot
// (CameraRig::Update / CameraRig::ComputeView).
struct View {
    glm::vec2 camera{0.0f};            // Ethanon camera (top-left of the logical screen)
    glm::vec2 logicalScreen{1024.0f, 768.0f};
    // The image the game is drawn into this frame (ViewportInfo's size).
    glm::uvec2 windowPixels{1024, 768};
    // Image pixels per logical pixel: the largest that fits when pillarboxed,
    // otherwise windowPixels.y / logicalScreen.y.
    float scale = 1.0f;
    // The logical screen's rectangle inside the image, in image pixels: the
    // whole image in the widescreen view, a centred 4:3 box when pillarboxed.
    // viewportMin is a whole pixel (CameraRig rounds it and places the camera
    // to match), so a glyph FontAtlas puts on a whole pixel stays on one.
    glm::vec2 viewportMin{0.0f};
    glm::vec2 viewportMax{1024.0f, 768.0f};
    // Where the image's top-left is in Input::MousePosition() coordinates
    // (ViewportInfo::rect.min; 0 in a packaged game, whose image is the
    // window). Only the pointer mapping reads it.
    glm::vec2 imageOrigin{0.0f};
    // ENHANCEMENT E1 for the menus (RenderSnapshot::sideMargin): how far past
    // the logical screen's left and right edges the scene is shown instead of
    // barred, in logical pixels. The logical screen stays exactly where the
    // pillarbox put it (the same scale and viewportMin), so the pointer and the
    // HUD map as before; only the bars give way. 0: bars wherever the logical
    // screen leaves the image, as before.
    float openSides = 0.0f;

    // The part of the image that is not barred (CameraRig::Bars), image
    // pixels: the logical screen's box, widened across by openSides as far as
    // the image goes. Exactly viewportMin/viewportMax when nothing is open.
    glm::vec2 ShownMin() const {
        if (!(openSides > 0.0f)) return viewportMin;
        return {std::max(0.0f, viewportMin.x - openSides * scale), viewportMin.y};
    }
    glm::vec2 ShownMax() const {
        if (!(openSides > 0.0f)) return viewportMax;
        return {std::min(static_cast<float>(windowPixels.x), viewportMax.x + openSides * scale), viewportMax.y};
    }
    // The same, in logical pixels (x may run below 0 and past the screen's
    // width). Exactly (0, 0) and logicalScreen when nothing is open.
    glm::vec2 ShownLogicalMin() const {
        if (!(openSides > 0.0f) || !(scale > 0.0f)) return glm::vec2(0.0f);
        return {(ShownMin().x - viewportMin.x) / scale, 0.0f};
    }
    glm::vec2 ShownLogicalMax() const {
        if (!(openSides > 0.0f) || !(scale > 0.0f)) return logicalScreen;
        return {(ShownMax().x - viewportMin.x) / scale, logicalScreen.y};
    }

    // Screen-space HUD position (logical pixels, y down) -> ScreenOverlay
    // fraction of the whole image.
    glm::vec2 HudToFraction(const glm::vec2& logical) const {
        const glm::vec2 window = viewportMin + logical * scale;
        return window / glm::vec2(windowPixels);
    }
};

} // namespace Penumbra::Render
