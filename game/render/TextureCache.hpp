#pragma once

// The original's images, decoded the way 0.7.12 decoded them and handed to the
// engine's TextureRegistry under keys a MaterialComponent or ScreenOverlay::Quad
// can name.
//
// Why not just point a material at extracted/app/entities/x.png:
//  - COLOUR KEYS. D3DX loaded every sprite with ColorKey 0xFFFF00FF (exact
//    magenta -> transparent black) and halos and additive particles with
//    0xFF000000 (ETHResourceManager.cpp:106); skull.png has 177 magenta pixels.
//  - DDS. Nine uncompressed DDS files, which stb_image cannot read.
//  - CONTENT, NOT EXTENSION. nm_white_ground.jpg is a BMP.
//  - NORMAL MAPS are renormalised per texel at load (the 2010 Cg renormalised,
//    the engine's shadeSprite2D does not).
//  - ADDITIVE. Ethanon's AM_ADD is One,One; the engine's Additive is
//    SrcAlpha,One - equal when alpha is 1, so the additive variant forces alpha
//    to 1 after the black key.
// Each variant is uploaded once with TextureRegistry::UploadRGBA under
// "data:" + a virtual path ("penumbra:<variant>:<relative path>") as UNORM, and
// the virtual path is the key handed out: RenderSystem acquires albedos with
// srgb = false only in a DisplayEncoded scene (CameraRig::Attach sets it),
// normal maps and ScreenOverlay textures always. The registry-free decode is
// in render/TextureDecode.hpp.
//
// ORDER: Attach before any renderer asks for a key. A key handed out while
// no TextureRegistry was attached is never uploaded, and the particle pools,
// the shadow texture, the halo slots and the sprite slots all keep the key
// they were first given.

#include <map>
#include <string>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace Supersonic {
class TextureRegistry;
}

namespace Penumbra::Render {

enum class TextureVariant {
    Sprite,        // magenta key (entities, HUD images, background)
    Halo,          // black key, alpha kept. The halo renderer draws halos with
                   // Additive instead (One,One ignored alpha); the two are the
                   // same image for the shipped halos, which are BMPs
    Additive,      // black key, alpha <= 1/255 made black, alpha forced to 1
                   // (AM_ADD particles and sprites, halos, an additive background)
    Plain,         // no key (normal maps are separate; this is for anything else)
    Normal,        // renormalised normal map, UNORM
    // AM_MODULATE (Zero, SrcColor: dest *= texel) emulated with Alpha blending:
    // magenta key, then rgb 0 and alpha = (1 - luminance(texel)) * texelAlpha
    // (0 at texelAlpha <= 1/255), so a black quad drawn with it darkens by
    // what the multiply would have removed.
    Modulate,
};

class TextureCache {
public:
    explicit TextureCache(std::string gameRoot);

    // Finds the TextureRegistry in registry.ctx(). Without one (a suite on a
    // bare registry), Key() still returns keys and Size() still works; nothing
    // is uploaded.
    void Attach(entt::registry& registry);

    // The path to give MaterialComponent::albedoTexturePath / normalTexturePath /
    // ScreenOverlay::Quad::texture for an original image, e.g.
    // Key("entities/bruxo.png", TextureVariant::Sprite). Decodes and uploads on
    // first use. "" when the file cannot be read (logged once).
    std::string Key(const std::string& relativePath, TextureVariant variant);

    // The image's original pixel size (0,0 if unreadable).
    glm::ivec2 Size(const std::string& relativePath);

    const std::string& GameRoot() const { return m_gameRoot; }

private:
    struct Entry {
        std::string key;
        glm::ivec2 size{0};
        bool failed = false;
    };
    std::string m_gameRoot;
    Supersonic::TextureRegistry* m_textures = nullptr;
    std::map<std::string, Entry> m_entries;
    std::map<std::string, glm::ivec2> m_sizes;
};

} // namespace Penumbra::Render
