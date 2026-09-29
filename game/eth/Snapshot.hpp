#pragma once

// What one Ethanon frame put on screen, captured at the point in the frame
// where 0.7.12 rendered (after the loop and any scene load, BEFORE the
// callbacks - docs/spec/30-ethanon-runtime.md §1). The layer draws only this;
// it never reads live entities, so what is shown is exactly the state the
// original showed, and a frame drawn twice between ticks draws the same thing.
//
// Everything is in the original's coordinates: world pixels, +x right, +y DOWN,
// the camera being the world position of the screen's top-left. The layer
// converts to the engine's +y-up world.

#include <string>
#include <vector>

#include "eth/EthTypes.hpp"

namespace Penumbra::Eth {

// One entity drawn this frame, in draw order (ascending drawHash; ties in
// visible-bucket order, then bucket-list order).
struct SpriteDraw {
    int entityId = -1;
    string entityName;
    string sprite;              // file in entities/ ("" = nothing to draw, e.g. a trigger label)
    string normal;              // file in entities/normalmaps/ ("" = flat)
    string gloss;               // file in entities/ ("" = no specular)
    ENTITY_TYPE type = ET_HORIZONTAL;
    ALPHA_MODE blendMode = AM_PIXEL;
    bool isStatic = false;
    bool applyLight = false;
    bool castShadow = false;
    float shadowScale = 1.0f;
    float shadowLengthScale = 1.0f;
    float shadowOpacity = 1.0f;
    float specularPower = 50.0f;
    float specularBrightness = 1.0f;
    glm::vec4 emissive{0.0f};
    glm::vec4 color{1.0f};      // v4Color: tint and alpha
    vector3 position{0.0f};     // world, unrounded
    vector2 origin{0.0f};       // world xy of the sprite's top-left corner, already rounded
                                // as 0.7.12 drew it (floor when round-up is on)
    vector2 size{0.0f};         // frame size in pixels
    vector2 bitmapSize{0.0f};   // whole image in pixels
    int spriteCutX = 1;
    int spriteCutY = 1;
    uint frame = 0;             // row-major cell index
    float angle = 0.0f;         // degrees (always 0 in the shipped content)
    float depth = 0.0f;         // 0.7.12 depth: larger is nearer
    float drawHash = 0.0f;
    // A layerable/overall/decal and z the renderer may want for its own order.
    float layerDepth = 0.0f;
};

struct LightDraw {
    int ownerId = -1;
    vector3 position{0.0f};     // world: owner position + light position
    vector3 color{1.0f};        // raw light colour (lightIntensity is separate)
    float range = 0.0f;
    bool isStatic = true;
    bool castShadows = false;
    float haloBrightness = 0.0f;
    float haloSize = 0.0f;
    string haloBitmap;          // "" = no halo
    // Dimming by the owner's slot-0 particle system (active/total), which 0.7.12
    // applies to the light and its halo (ETHScene.cpp:814-829).
    float particleRatio = 1.0f;
};

struct ParticleDraw {
    int ownerId = -1;
    string bitmap;              // file in particles/
    ALPHA_MODE alphaMode = AM_PIXEL;
    vector2 position{0.0f};     // world, centre of the particle
    float size = 0.0f;          // square side in pixels
    float angle = 0.0f;         // degrees
    glm::vec4 color{1.0f};      // final vertex colour (luminance/ambient rule applied)
    int spriteCutX = 1;
    int spriteCutY = 1;
    uint frame = 0;
    float depth = 0.0f;         // 0.7.12 depth of this particle (owner depth + shift)
    // Who this particle is from one frame to the next, for the enhanced
    // interpolation between ticks (E8, render/Interpolation.hpp); 0.7.12 had
    // no use for it. (ownerId, system, particleId) names a particle slot, and
    // lifeStartMs - the particle clock when its current life began - tells one
    // life of the slot from the next, since a spent particle is respawned at
    // its emitter under the same index.
    int system = 0;             // the owner's particle system slot, 0 or 1
    int particleId = -1;        // the particle's index in its system
    float lifeStartMs = 0.0f;
};

// A top-layer primitive (DrawText / DrawSprite / DrawShapedSprite /
// DrawRectangle), in screen pixels, drawn over the scene in submission order.
struct HudCmd {
    enum class Kind { Text, Sprite, ShapedSprite, Rectangle };
    Kind kind = Kind::Rectangle;
    vector2 pos{0.0f};
    vector2 size{0.0f};         // ShapedSprite/Rectangle; Sprite: the bitmap size
    string text;                // Text: cp1252 bytes, '\n' breaks lines, tabs expand
    string font;                // Text: "Arial Narrow", "Arial Black", "Arial", "Verdana"
    float fontSize = 0.0f;      // Text: GDI cell height in pixels (always drawn bold)
    string sprite;              // Sprite/ShapedSprite: the path the script loaded (relative to the game root)
    vector2 spriteRectMin{0.0f};// Sprite: the sub-rectangle drawn, in pixels of the image
    vector2 spriteRectMax{0.0f};
    uint color = 0xFFFFFFFFu;   // ARGB (Text/Sprite/ShapedSprite, and the Rectangle's top-left)
    uint color1 = 0xFFFFFFFFu;  // Rectangle: top-right
    uint color2 = 0xFFFFFFFFu;  // Rectangle: bottom-left
    uint color3 = 0xFFFFFFFFu;  // Rectangle: bottom-right
};

struct RenderSnapshot {
    uint frameIndex = 0;        // Ethanon frames since boot
    uint timeMs = 0;            // GetTime() at the render point
    string sceneFile;           // as passed to LoadScene
    vector2 camera{0.0f};       // world position of the screen's top-left
    vector2 screenSize{1024.0f, 768.0f};
    vector3 ambient{1.0f};
    float lightIntensity = 2.0f;
    vector2 zAxisDirection{0.0f};
    bool pixelShaders = true;   // UsePixelShaders
    bool roundUp = true;        // SetPositionRoundUp
    uint backgroundColor = 0xFF000000u;
    string backgroundImage;     // "" = none (path relative to the game root)
    vector2 backgroundMin{0.0f};
    vector2 backgroundMax{0.0f};
    bool backgroundAdditive = false;
    std::vector<SpriteDraw> sprites;
    std::vector<LightDraw> lights;
    std::vector<ParticleDraw> particles;
    std::vector<HudCmd> hud;
    bool cursorHidden = true;
    // ENHANCEMENT E1 (the wide menus, Machine::SideMargin): how far past the
    // logical screen's left and right edges this frame's world was collected,
    // in logical pixels; 0 = the screen only, as 0.7.12 drew it. The view
    // shows that far past the screen instead of barring it (View::openSides).
    float sideMargin = 0.0f;
    // How many LoadScene requests have been served. A change means every
    // entity id and position before it belonged to another scene - even when
    // the file is the same one, as a death reloading the level is - so the
    // enhanced interpolation (E8) never blends across it.
    uint sceneSerial = 0;
};

} // namespace Penumbra::Eth
