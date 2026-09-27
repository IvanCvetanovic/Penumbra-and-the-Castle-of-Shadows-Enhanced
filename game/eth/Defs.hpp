#pragma once

// The original's data files, as parsed values: an entity definition (.ent, or
// the full copy every .esc placement carries inline), its particle systems and
// light, a scene (.esc), and the custom data both hold.
//
// The whole schema is in docs/spec/20-formats-scene-entity.md; the reader is
// the 0.7.12 one (reference/eth-0.7.12/src/ETHEntityFile.cpp, ETHParticleManager.cpp,
// ETHScene.cpp) over TinyXML with TIXML_ENCODING_LEGACY: bytes pass through as
// cp1252, &apos; &amp; &lt; &gt; &quot; are decoded, CRLF and LF both accepted.
// A missing element or attribute keeps the default written here, which is the
// 0.7.12 constructor's default.

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "eth/EthTypes.hpp"

class TiXmlElement;

namespace Penumbra::Eth {

// One custom-data value: Ethanon's ETHCustomData. The type is part of the value
// - a getter of the wrong type reads 0 or "" (ETHDataManager.cpp:123-189).
struct CustomValue {
    DATA_TYPE type = DT_NODATA;
    float f = 0.0f;
    int i = 0;
    uint u = 0;
    string s;
};

// Ordered by name: SaveScene writes the variables alphabetically, as 0.7.12's
// std::map did.
using CustomData = std::map<string, CustomValue>;

// <ParticleSystem>: ETH_PARTICLE_SYSTEM (ETHParticleManager.h), 0.7.12 fields.
struct ParticleSystemDef {
    string bitmap;              // <Bitmap>, a file in particles/
    string soundEffect;         // <SoundEffect>, a file in soundfx/ ("" = none)
    bool allAtOnce = false;
    float boundingSphere = 0.0f;
    ALPHA_MODE alphaMode = AM_PIXEL;
    int nParticles = 0;         // particles="..."
    vector2 gravity{0.0f};
    vector2 direction{0.0f};
    vector2 randomizeDir{0.0f};
    vector3 startPoint{0.0f};
    vector2 randStartPoint{0.0f};
    int spriteCutX = 1;
    int spriteCutY = 1;
    glm::vec4 color0{1.0f};
    glm::vec4 color1{1.0f};
    float lifeTime = 0.0f;
    float randomizeLifeTime = 0.0f;   // randLifeTime / randomLifeTime
    float angleDir = 0.0f;
    float randAngle = 0.0f;
    float size = 0.0f;
    float randomizeSize = 0.0f;
    float growth = 0.0f;
    float minSize = 0.0f;
    float maxSize = 0.0f;
    int repeat = 0;             // 0 = endless
    int animationMode = 1;      // 1 PLAY_ANIMATION, 2 PICK_RANDOM_FRAME
    vector3 luminance{1.0f};
    float angleStart = 0.0f;
    float randAngleStart = 0.0f;

    static constexpr int kPlayAnimation = 1;
    static constexpr int kPickRandomFrame = 2;
};

// <Light>: ETH_LIGHT.
struct LightDef {
    bool active = false;
    bool isStatic = true;
    bool castShadows = false;
    float range = 256.0f;
    float haloBrightness = 1.0f;
    float haloSize = 64.0f;
    vector3 position{0.0f};
    vector3 color{1.0f};
    string haloBitmap;          // <HaloBitmap>, a file in entities/ ("" = no halo)
};

// <Entity type=... > ... </Entity>: ETHEntityProperties plus the file-level
// fields. Every placement in a scene carries a full copy; an AddEntity'd one is
// read from entities/<name> (docs/spec/20-formats-scene-entity.md: 4 of 2254
// inline copies differ from the file on disk, and the inline copy wins).
struct EntityDef {
    ENTITY_TYPE type = ET_HORIZONTAL;
    bool isStatic = false;
    bool collidable = false;
    uint startFrame = 0;
    bool applyLight = false;
    bool castShadow = false;
    ALPHA_MODE blendMode = AM_PIXEL;
    float layerDepth = 0.0f;
    float soundVolume = 1.0f;
    float shadowScale = 1.0f;
    float shadowLengthScale = 1.0f;
    float shadowOpacity = 1.0f;
    float specularPower = 50.0f;
    float specularBrightness = 1.0f;
    glm::vec4 emissiveColor{0.0f};
    int spriteCutX = 1;
    int spriteCutY = 1;
    vector2 pivotAdjust{0.0f};
    string sprite;              // <Sprite>, a file in entities/ ("" = no sprite)
    string normal;              // <Normal>, a file in entities/normalmaps/
    string gloss;               // <Gloss>, a file in entities/
    std::vector<ParticleSystemDef> particles;   // at most 2
    LightDef light;
    vector3 collisionPos{0.0f};
    vector3 collisionSize{0.0f};
    CustomData customData;
};

// One <Entity id=... spriteFrame=...> in <EntitiesInScene>.
struct ScenePlacement {
    int id = -1;
    uint spriteFrame = 0;
    string entityName;          // <EntityName>: a .ent file name or a bare label ("spawn", "help"...)
    glm::vec4 color{1.0f};      // <Color> (always 1,1,1,1 in the shipped scenes)
    vector3 position{0.0f};
    float angle = 0.0f;
    EntityDef def;              // the inline <Entity> block
};

struct SceneProperties {
    float lightIntensity = 2.0f;
    vector3 ambient{1.0f};
    vector2 zAxisDirection{0.0f, -1.0f};   // 0.7.12 default; levels say (0,0)
};

struct SceneFile {
    SceneProperties properties;
    std::vector<ScenePlacement> entities;   // file order
};

// Readers. Each returns nullopt (and logs why) on a missing or malformed file.
// `path` is absolute. The 0.7.12 reader's quirks are the contract: attribute
// names as in the shipped files (randomLifeTime or randLifeTime), missing
// children keep defaults, <Variable><Type>int|uint|float|string</Type>.
std::optional<EntityDef> ReadEntityFile(const string& path);
std::optional<SceneFile> ReadSceneFile(const string& path);
std::optional<ParticleSystemDef> ReadParticleFile(const string& path);

// The element-level readers the above share, for a caller holding a TinyXML
// element already (the scene reader, the checkpoint round trip).
EntityDef ReadEntityElement(const TiXmlElement& entity);
ParticleSystemDef ReadParticleElement(const TiXmlElement& system);

// Writers, for SaveScene. The output parses back to equal values.
string WriteSceneFile(const SceneFile& scene);

} // namespace Penumbra::Eth
