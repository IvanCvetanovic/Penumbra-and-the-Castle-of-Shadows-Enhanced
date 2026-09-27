// The original's XML data: .ent entity definitions, .esc scenes, .par particle
// systems. A port of the Ethanon 0.7.12 readers and the scene writer, over the
// same TinyXML 2.5 the original used:
//   E:ETHEntityFile.cpp   scene properties, lights, entities, scene placements
//   E:ETHParticleManager.cpp:105-448   particle systems
//   E:ETHCommon.cpp:68-104             collision boxes
//   E:ETHDataManager.cpp:364-497       custom data
//   E:ETHScene.cpp:182-311             scene files
//
// What the port keeps from those readers, because the data depends on it:
//  - TIXML_ENCODING_LEGACY: bytes pass through as cp1252 (level1.esc's help
//    messages hold 0xE3/0xE7/0xF3), entities such as &apos; are decoded.
//  - Line ends are normalised as TiXmlDocument::LoadFile does, so CRLF and LF
//    files read alike.
//  - A missing attribute or child keeps the 0.7.12 default. Those defaults are
//    the constructors' Reset() values below, which Defs.hpp's struct
//    initialisers now also carry; the Reset functions stay so each reader
//    cites the 0.7.12 line it follows.
//  - Numbers are written with "%g" (TiXmlAttribute::SetDoubleValue) and custom
//    data with an ostream's default six digits, so a saved scene holds six
//    significant digits, as 0.7.12's checkpoint.esc did.
//  - Sibling loops take ANY next element, not only the expected name, as the
//    originals' NextSiblingElement() did.

#include "eth/Defs.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <locale>
#include <sstream>

#include "core/Log.hpp"

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include "tinyxml.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace Penumbra::Eth {
namespace {

constexpr const char* kLog = "Penumbra";

// ETH_MAX_PARTICLE_SYS_PER_ENTITY (E:ETHCommon.h:69).
constexpr int kMaxParticleSystems = 2;

#ifdef _MSC_VER
#define PENUMBRA_DEFS_SSCANF sscanf_s
#else
#define PENUMBRA_DEFS_SSCANF std::sscanf
#endif

// --- 0.7.12 defaults ---------------------------------------------------------

// ETHEntityFile::Reset's light (E:ETHEntityFile.cpp:721-729).
LightDef ResetLight() {
    LightDef light;
    light.active = false;
    light.isStatic = false;
    light.castShadows = true;
    light.range = 256.0f;
    light.haloBrightness = 1.0f;
    light.haloSize = 64.0f;
    light.position = vector3(0.0f);
    light.color = vector3(1.0f);
    light.haloBitmap.clear();
    return light;
}

// ETHEntityFile::Reset (E:ETHEntityFile.cpp:699-736).
EntityDef ResetEntity() {
    EntityDef def;
    def.type = ET_HORIZONTAL;
    def.isStatic = false;
    def.collidable = false;
    def.startFrame = 0;
    def.applyLight = true;
    def.castShadow = false;
    def.blendMode = AM_PIXEL;
    def.layerDepth = 0.0f;
    def.soundVolume = 1.0f;
    def.shadowScale = 0.0f;
    def.shadowLengthScale = 1.0f;
    def.shadowOpacity = 1.0f;
    def.specularPower = 50.0f;
    def.specularBrightness = 1.0f;
    def.emissiveColor = glm::vec4(0.0f);
    def.spriteCutX = 1;
    def.spriteCutY = 1;
    def.pivotAdjust = vector2(0.0f);
    def.light = ResetLight();
    def.collisionPos = vector3(0.0f);
    def.collisionSize = vector3(0.0f);
    return def;
}

// ETH_PARTICLE_SYSTEM::Reset (E:ETHParticleManager.cpp:105-126). The vectors,
// colours, angleDir and randAngle are not reset there; they keep Defs.hpp's
// zeros and white.
ParticleSystemDef ResetParticleSystem() {
    ParticleSystemDef system;
    system.alphaMode = AM_PIXEL;
    system.nParticles = 0;
    system.lifeTime = 0.0f;
    system.randomizeLifeTime = 0.0f;
    system.size = 1.0f;
    system.growth = 0.0f;
    system.minSize = 0.0f;
    system.maxSize = 99999.0f;
    system.repeat = 0;
    system.randomizeSize = 0.0f;
    system.randAngleStart = 0.0f;
    system.angleStart = 0.0f;
    system.luminance = vector3(0.0f);
    system.allAtOnce = false;
    system.boundingSphere = 512.0f;
    system.soundEffect.clear();
    system.spriteCutX = 1;
    system.spriteCutY = 1;
    system.animationMode = ParticleSystemDef::kPlayAnimation;
    return system;
}

// ETH_SCENE_PROPERTIES::Reset (E:ETHEntityFile.cpp:111-116, ETHCommon.h:91-92).
SceneProperties ResetSceneProperties() {
    SceneProperties properties;
    properties.ambient = vector3(0.3f);
    properties.lightIntensity = 2.0f;
    properties.zAxisDirection = vector2(0.0f, -1.0f);
    return properties;
}

// ETH_ENTITY_DATA's constructor (E:ETHEntityFile.cpp:742-747).
ScenePlacement ResetPlacement() {
    ScenePlacement placement;
    placement.id = 0;
    placement.spriteFrame = 0;
    placement.color = glm::vec4(1.0f);
    placement.position = vector3(0.0f);
    placement.angle = 0.0f;
    placement.def = ResetEntity();
    return placement;
}

// --- Reading helpers ---------------------------------------------------------

// pElement->FirstChild("X")->ToElement(), without the originals' null
// dereference when X is missing (ETH_LIGHT's Position, the collision box).
const TiXmlElement* Child(const TiXmlElement& parent, const char* name) {
    const TiXmlNode* node = parent.FirstChild(name);
    return node != nullptr ? node->ToElement() : nullptr;
}

// QueryFloatAttribute's own path (tinyxml.h: "%lf" into a double, then a cast),
// through the out-of-line QueryDoubleAttribute.
void QueryFloat(const TiXmlElement& element, const char* name, float& value) {
    double parsed = 0.0;
    if (element.QueryDoubleAttribute(name, &parsed) == TIXML_SUCCESS) value = static_cast<float>(parsed);
}

void QueryInt(const TiXmlElement& element, const char* name, int& value) {
    int parsed = 0;
    if (element.QueryIntAttribute(name, &parsed) == TIXML_SUCCESS) value = parsed;
}

// The flags went through an int into ETH_BOOL, an unsigned char
// (E:ETHCommon.h:131). A missing flag read an uninitialised int in 0.7.12;
// here it keeps the default.
void QueryBool(const TiXmlElement& element, const char* name, bool& value) {
    int parsed = 0;
    if (element.QueryIntAttribute(name, &parsed) == TIXML_SUCCESS) value = static_cast<unsigned char>(parsed) != 0;
}

template <typename Enum>
void QueryEnum(const TiXmlElement& element, const char* name, Enum& value) {
    int parsed = static_cast<int>(value);
    QueryInt(element, name, parsed);
    value = static_cast<Enum>(parsed);
}

void QueryUintViaInt(const TiXmlElement& element, const char* name, uint& value) {
    int parsed = static_cast<int>(value);
    QueryInt(element, name, parsed);
    value = static_cast<uint>(parsed);
}

// <Name>text</Name>: GetText(), kept only when there is text.
void QueryText(const TiXmlElement& parent, const char* name, string& value) {
    const TiXmlElement* element = Child(parent, name);
    if (element == nullptr) return;
    const char* text = element->GetText();
    if (text != nullptr) value = text;
}

void QueryXY(const TiXmlElement* element, vector2& value) {
    if (element == nullptr) return;
    QueryFloat(*element, "x", value.x);
    QueryFloat(*element, "y", value.y);
}

void QueryXYZ(const TiXmlElement* element, vector3& value) {
    if (element == nullptr) return;
    QueryFloat(*element, "x", value.x);
    QueryFloat(*element, "y", value.y);
    QueryFloat(*element, "z", value.z);
}

void QueryRGB(const TiXmlElement* element, vector3& value) {
    if (element == nullptr) return;
    QueryFloat(*element, "r", value.x);
    QueryFloat(*element, "g", value.y);
    QueryFloat(*element, "b", value.z);
}

void QueryRGBA(const TiXmlElement* element, glm::vec4& value) {
    if (element == nullptr) return;
    QueryFloat(*element, "r", value.x);
    QueryFloat(*element, "g", value.y);
    QueryFloat(*element, "b", value.z);
    QueryFloat(*element, "a", value.w);
}

void QueryCut(const TiXmlElement* element, int& x, int& y) {
    if (element == nullptr) return;
    QueryInt(*element, "x", x);
    QueryInt(*element, "y", y);
}

// ETHGlobal::ParseFloat/ParseInt/ParseUint (E:ETHCommon.h:571-590): sscanf
// into a zero, so text that does not scan reads as 0.
float ParseFloat(const string& text) {
    float value = 0.0f;
    static_cast<void>(PENUMBRA_DEFS_SSCANF(text.c_str(), "%f", &value));
    return value;
}

int ParseInt(const string& text) {
    int value = 0;
    static_cast<void>(PENUMBRA_DEFS_SSCANF(text.c_str(), "%d", &value));
    return value;
}

uint ParseUint(const string& text) {
    unsigned int value = 0;
    static_cast<void>(PENUMBRA_DEFS_SSCANF(text.c_str(), "%u", &value));
    return value;
}

// ETHGlobal::szDataName (E:ETHDataManager.h:82-89), indexed by DATA_TYPE.
const char* DataTypeName(const DATA_TYPE type) {
    switch (type) {
    case DT_FLOAT: return "float";
    case DT_INT: return "int";
    case DT_UINT: return "uint";
    case DT_STRING: return "string";
    case DT_NODATA: break;
    }
    return "";
}

// ETH_LIGHT::ReadFromXMLFile (E:ETHEntityFile.cpp:150-209).
void ReadLight(const TiXmlElement& element, LightDef& light) {
    QueryBool(element, "active", light.active);
    QueryBool(element, "static", light.isStatic);
    QueryBool(element, "castShadows", light.castShadows);
    QueryFloat(element, "range", light.range);
    QueryFloat(element, "haloBrightness", light.haloBrightness);
    QueryFloat(element, "haloSize", light.haloSize);
    QueryXYZ(Child(element, "Position"), light.position);
    QueryRGB(Child(element, "Color"), light.color);
    QueryText(element, "HaloBitmap", light.haloBitmap);
}

// ETHCustomDataManager::ReadFromXMLFile (E:ETHDataManager.cpp:364-454). A
// variable with an empty type, name or value is skipped, so an empty string
// does not survive a save and load; an unknown type is skipped too.
void ReadCustomData(const TiXmlElement& entity, CustomData& data) {
    const TiXmlElement* customData = Child(entity, "CustomData");
    if (customData == nullptr) return;
    for (const TiXmlElement* variable = Child(*customData, "Variable"); variable != nullptr;
         variable = variable->NextSiblingElement()) {
        string type;
        string name;
        string text;
        QueryText(*variable, "Type", type);
        QueryText(*variable, "Name", name);
        QueryText(*variable, "Value", text);
        if (type.empty() || name.empty() || text.empty()) continue;

        CustomValue value;
        if (type == "float") {
            value.type = DT_FLOAT;
            value.f = ParseFloat(text);
        } else if (type == "int") {
            value.type = DT_INT;
            value.i = ParseInt(text);
        } else if (type == "uint") {
            value.type = DT_UINT;
            value.u = ParseUint(text);
        } else if (type == "string") {
            value.type = DT_STRING;
            value.s = text;
        } else {
            continue;
        }
        data[name] = value;     // Add*Data: a repeated name overwrites value and type
    }
}

// ETH_ENTITY_DATA::ReadFromXMLFile (E:ETHEntityFile.cpp:794-852).
ScenePlacement ReadPlacement(const TiXmlElement& element) {
    ScenePlacement placement = ResetPlacement();
    QueryInt(element, "id", placement.id);
    int spriteFrame = 0;
    QueryInt(element, "spriteFrame", spriteFrame);
    placement.spriteFrame = static_cast<uint>(spriteFrame);
    QueryText(element, "EntityName", placement.entityName);
    QueryRGBA(Child(element, "Color"), placement.color);
    if (const TiXmlElement* position = Child(element, "Position")) {
        QueryFloat(*position, "x", placement.position.x);
        QueryFloat(*position, "y", placement.position.y);
        QueryFloat(*position, "z", placement.position.z);
        QueryFloat(*position, "angle", placement.angle);
    }
    if (const TiXmlElement* entity = Child(element, "Entity")) placement.def = ReadEntityElement(*entity);
    return placement;
}

// ETH_SCENE_PROPERTIES::ReadFromXMLFile (E:ETHEntityFile.cpp:55-88).
void ReadSceneProperties(const TiXmlElement& root, SceneProperties& properties) {
    const TiXmlElement* element = Child(root, "SceneProperties");
    if (element == nullptr) return;
    QueryFloat(*element, "lightIntensity", properties.lightIntensity);
    QueryRGB(Child(*element, "Ambient"), properties.ambient);
    QueryXY(Child(*element, "ZAxisDirection"), properties.zAxisDirection);
}

bool ReadFileBytes(const string& path, string& out) {
    std::ifstream file(std::filesystem::path(path), std::ios::binary);
    if (!file) return false;
    out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

// TiXmlDocument::LoadFile(path, TIXML_ENCODING_LEGACY), with the file read here:
// the same CR/CRLF-to-LF pass it makes before parsing (tinyxml.cpp LoadFile),
// then Parse. Logs why a file was refused.
bool LoadDocument(const string& path, TiXmlDocument& document) {
    string bytes;
    if (!ReadFileBytes(path, bytes)) {
        SUPERSONIC_LOG_WARN(kLog) << "cannot open " << path;
        return false;
    }
    string text;
    text.reserve(bytes.size());
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (bytes[i] == '\r') {
            text += '\n';
            if (i + 1 < bytes.size() && bytes[i + 1] == '\n') ++i;
        } else {
            text += bytes[i];
        }
    }
    document.Parse(text.c_str(), nullptr, TIXML_ENCODING_LEGACY);
    if (document.Error()) {
        SUPERSONIC_LOG_WARN(kLog) << "malformed XML in " << path << " (row " << document.ErrorRow() << ", col "
                                  << document.ErrorCol() << "): " << document.ErrorDesc();
        return false;
    }
    return true;
}

// The document's root (<Ethanon>) and its first child element, which is what
// ETHEntityFile::LoadFromFile and ETH_PARTICLE_SYSTEM::ReadFromFile hand to
// their readers.
const TiXmlElement* FirstInnerElement(const TiXmlDocument& document, const string& path) {
    const TiXmlElement* root = document.FirstChildElement();
    const TiXmlElement* inner = root != nullptr ? root->FirstChildElement() : nullptr;
    if (inner == nullptr) SUPERSONIC_LOG_WARN(kLog) << path << " has no definition inside its root";
    return inner;
}

// --- Writing helpers -----------------------------------------------------------

TiXmlElement* AddChild(TiXmlNode& parent, const char* name) {
    auto* element = new TiXmlElement(name);
    parent.LinkEndChild(element);
    return element;
}

void AddTextChild(TiXmlNode& parent, const char* name, const string& text) {
    TiXmlElement* element = AddChild(parent, name);
    element->LinkEndChild(new TiXmlText(text));
}

void SetXY(TiXmlElement& element, const vector2& value) {
    element.SetDoubleAttribute("x", value.x);
    element.SetDoubleAttribute("y", value.y);
}

void SetXYZ(TiXmlElement& element, const vector3& value) {
    element.SetDoubleAttribute("x", value.x);
    element.SetDoubleAttribute("y", value.y);
    element.SetDoubleAttribute("z", value.z);
}

void SetRGB(TiXmlElement& element, const vector3& value) {
    element.SetDoubleAttribute("r", value.x);
    element.SetDoubleAttribute("g", value.y);
    element.SetDoubleAttribute("b", value.z);
}

void SetRGBA(TiXmlElement& element, const glm::vec4& value) {
    element.SetDoubleAttribute("r", value.x);
    element.SetDoubleAttribute("g", value.y);
    element.SetDoubleAttribute("b", value.z);
    element.SetDoubleAttribute("a", value.w);
}

// ETHGlobal::GetFileName (E:ETHCommon.h:267-281): the part after the last
// separator - one at index 0 is left alone, as the original's loop stopped at 1.
string FileName(const string& path) {
    for (std::size_t t = path.size() > 0 ? path.size() - 1 : 0; t > 0; --t) {
        if (path[t] == '\\' || path[t] == '/') return path.substr(t + 1);
    }
    return path;
}

template <typename T>
string Streamed(const T& value) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << value;
    return stream.str();
}

// ETH_PARTICLE_SYSTEM::WriteToXMLFile (E:ETHParticleManager.cpp:380-448).
void WriteParticleSystem(const ParticleSystemDef& system, TiXmlNode& parent) {
    TiXmlElement* root = AddChild(parent, "ParticleSystem");
    if (!system.soundEffect.empty()) AddTextChild(*root, "SoundEffect", FileName(system.soundEffect));
    if (!system.bitmap.empty()) AddTextChild(*root, "Bitmap", FileName(system.bitmap));
    SetXY(*AddChild(*root, "Gravity"), system.gravity);
    SetXY(*AddChild(*root, "Direction"), system.direction);
    SetXY(*AddChild(*root, "RandomizeDir"), system.randomizeDir);
    TiXmlElement* cut = AddChild(*root, "SpriteCut");
    cut->SetDoubleAttribute("x", system.spriteCutX);
    cut->SetDoubleAttribute("y", system.spriteCutY);
    SetXYZ(*AddChild(*root, "StartPoint"), system.startPoint);
    SetXY(*AddChild(*root, "RandStartPoint"), system.randStartPoint);
    SetRGBA(*AddChild(*root, "Color0"), system.color0);
    SetRGBA(*AddChild(*root, "Color1"), system.color1);
    SetRGB(*AddChild(*root, "Luminance"), system.luminance);

    root->SetAttribute("particles", system.nParticles);
    root->SetAttribute("allAtOnce", system.allAtOnce ? 1 : 0);
    root->SetAttribute("alphaMode", static_cast<int>(system.alphaMode));
    root->SetAttribute("repeat", system.repeat);
    root->SetAttribute("animationMode", system.animationMode);
    root->SetDoubleAttribute("boundingSphere", system.boundingSphere);
    root->SetDoubleAttribute("lifeTime", system.lifeTime);
    root->SetDoubleAttribute("randomLifeTime", system.randomizeLifeTime);
    root->SetDoubleAttribute("angleDir", system.angleDir);
    root->SetDoubleAttribute("randAngle", system.randAngle);
    root->SetDoubleAttribute("size", system.size);
    root->SetDoubleAttribute("randomizeSize", system.randomizeSize);
    root->SetDoubleAttribute("growth", system.growth);
    root->SetDoubleAttribute("minSize", system.minSize);
    root->SetDoubleAttribute("maxSize", system.maxSize);
    root->SetDoubleAttribute("angleStart", system.angleStart);
    root->SetDoubleAttribute("randAngleStart", system.randAngleStart);
}

// ETH_LIGHT::WriteToXMLFile (E:ETHEntityFile.cpp:211-243).
void WriteLight(const LightDef& light, TiXmlNode& parent) {
    TiXmlElement* root = AddChild(parent, "Light");
    SetXYZ(*AddChild(*root, "Position"), light.position);
    SetRGB(*AddChild(*root, "Color"), light.color);
    if (!light.haloBitmap.empty()) AddTextChild(*root, "HaloBitmap", light.haloBitmap);
    root->SetAttribute("active", light.active ? 1 : 0);
    root->SetAttribute("static", light.isStatic ? 1 : 0);
    root->SetAttribute("castShadows", light.castShadows ? 1 : 0);
    root->SetDoubleAttribute("range", light.range);
    root->SetDoubleAttribute("haloBrightness", light.haloBrightness);
    root->SetDoubleAttribute("haloSize", light.haloSize);
}

// ETH_COLLISION_BOX::WriteToXMLFile (E:ETHCommon.cpp:88-104).
void WriteCollision(const vector3& position, const vector3& size, TiXmlNode& parent) {
    TiXmlElement* root = AddChild(parent, "Collision");
    SetXYZ(*AddChild(*root, "Position"), position);
    SetXYZ(*AddChild(*root, "Size"), size);
}

// ETHCustomDataManager::WriteToFile (E:ETHDataManager.cpp:456-497): variables in
// map order (alphabetical), values through an ostream.
void WriteCustomData(const CustomData& data, TiXmlNode& parent) {
    TiXmlElement* root = AddChild(parent, "CustomData");
    for (const auto& [name, value] : data) {
        TiXmlElement* variable = AddChild(*root, "Variable");
        AddTextChild(*variable, "Type", DataTypeName(value.type));
        AddTextChild(*variable, "Name", name);
        string text;
        switch (value.type) {
        case DT_FLOAT: text = Streamed(value.f); break;
        case DT_INT: text = Streamed(value.i); break;
        case DT_UINT: text = Streamed(value.u); break;
        case DT_STRING: text = value.s; break;
        case DT_NODATA: break;
        }
        AddTextChild(*variable, "Value", text);
    }
}

// ETHEntityFile::WriteToXMLFile (E:ETHEntityFile.cpp:427-499). Only systems
// with particles are written, so a system with none drops out on a round trip.
void WriteEntity(const EntityDef& def, TiXmlNode& parent) {
    TiXmlElement* root = AddChild(parent, "Entity");
    SetRGBA(*AddChild(*root, "EmissiveColor"), def.emissiveColor);
    TiXmlElement* cut = AddChild(*root, "SpriteCut");
    cut->SetDoubleAttribute("x", def.spriteCutX);
    cut->SetDoubleAttribute("y", def.spriteCutY);
    SetXY(*AddChild(*root, "PivotAdjust"), def.pivotAdjust);
    if (!def.sprite.empty()) AddTextChild(*root, "Sprite", def.sprite);
    if (!def.normal.empty()) AddTextChild(*root, "Normal", def.normal);
    if (!def.gloss.empty()) AddTextChild(*root, "Gloss", def.gloss);

    TiXmlElement* particles = AddChild(*root, "Particles");
    for (std::size_t t = 0; t < def.particles.size() && t < static_cast<std::size_t>(kMaxParticleSystems); ++t) {
        if (def.particles[t].nParticles > 0) WriteParticleSystem(def.particles[t], *particles);
    }
    WriteLight(def.light, *root);
    WriteCollision(def.collisionPos, def.collisionSize, *root);

    root->SetAttribute("type", static_cast<int>(def.type));
    root->SetAttribute("static", def.isStatic ? 1 : 0);
    root->SetAttribute("collidable", def.collidable ? 1 : 0);
    root->SetAttribute("startFrame", static_cast<int>(def.startFrame));
    root->SetAttribute("applyLight", def.applyLight ? 1 : 0);
    root->SetAttribute("castShadow", def.castShadow ? 1 : 0);
    root->SetAttribute("blendMode", static_cast<int>(def.blendMode));
    root->SetDoubleAttribute("layerDepth", def.layerDepth);
    root->SetDoubleAttribute("soundVolume", def.soundVolume);
    root->SetDoubleAttribute("shadowScale", def.shadowScale);
    root->SetDoubleAttribute("shadowLengthScale", def.shadowLengthScale);
    root->SetDoubleAttribute("shadowOpacity", def.shadowOpacity);
    root->SetDoubleAttribute("specularPower", def.specularPower);
    root->SetDoubleAttribute("specularBrightness", def.specularBrightness);

    WriteCustomData(def.customData, *root);
}

// ETH_ENTITY_DATA::WriteToXMLFile (E:ETHEntityFile.cpp:759-792).
void WritePlacement(const ScenePlacement& placement, TiXmlNode& parent) {
    TiXmlElement* root = AddChild(parent, "Entity");
    if (!placement.entityName.empty()) AddTextChild(*root, "EntityName", placement.entityName);
    SetRGBA(*AddChild(*root, "Color"), placement.color);
    TiXmlElement* position = AddChild(*root, "Position");
    SetXYZ(*position, placement.position);
    position->SetDoubleAttribute("angle", placement.angle);
    root->SetAttribute("id", placement.id);
    root->SetAttribute("spriteFrame", static_cast<int>(placement.spriteFrame));
    WriteEntity(placement.def, *root);
}

// ETH_SCENE_PROPERTIES::WriteToXMLFile (E:ETHEntityFile.cpp:90-109).
void WriteSceneProperties(const SceneProperties& properties, TiXmlNode& parent) {
    TiXmlElement* root = AddChild(parent, "SceneProperties");
    SetRGB(*AddChild(*root, "Ambient"), properties.ambient);
    SetXY(*AddChild(*root, "ZAxisDirection"), properties.zAxisDirection);
    root->SetDoubleAttribute("lightIntensity", properties.lightIntensity);
}

} // namespace

// --- Element readers -------------------------------------------------------------

// ETH_PARTICLE_SYSTEM::ReadFromXMLFile (E:ETHParticleManager.cpp:128-315).
ParticleSystemDef ReadParticleElement(const TiXmlElement& element) {
    ParticleSystemDef system = ResetParticleSystem();
    QueryInt(element, "particles", system.nParticles);
    QueryBool(element, "allAtOnce", system.allAtOnce);
    QueryEnum(element, "alphaMode", system.alphaMode);
    QueryInt(element, "repeat", system.repeat);
    QueryFloat(element, "boundingSphere", system.boundingSphere);
    QueryFloat(element, "lifeTime", system.lifeTime);
    if (element.Attribute("randomLifeTime") != nullptr) {
        QueryFloat(element, "randomLifeTime", system.randomizeLifeTime);
    } else {
        // Not a 0.7.12 name and in none of the shipped files; later Ethanon
        // files spell it this way, and Defs.hpp's contract accepts both.
        QueryFloat(element, "randLifeTime", system.randomizeLifeTime);
    }
    QueryFloat(element, "angleDir", system.angleDir);
    QueryFloat(element, "randAngle", system.randAngle);
    QueryFloat(element, "size", system.size);
    QueryFloat(element, "randomizeSize", system.randomizeSize);
    QueryFloat(element, "growth", system.growth);
    QueryFloat(element, "minSize", system.minSize);
    QueryFloat(element, "maxSize", system.maxSize);
    QueryFloat(element, "angleStart", system.angleStart);
    QueryFloat(element, "randAngleStart", system.randAngleStart);
    QueryInt(element, "animationMode", system.animationMode);

    QueryText(element, "Bitmap", system.bitmap);
    QueryText(element, "SoundEffect", system.soundEffect);
    QueryXY(Child(element, "Gravity"), system.gravity);
    QueryXY(Child(element, "Direction"), system.direction);
    QueryXY(Child(element, "RandomizeDir"), system.randomizeDir);
    QueryXYZ(Child(element, "StartPoint"), system.startPoint);
    QueryXY(Child(element, "RandStartPoint"), system.randStartPoint);
    // explosion_particles.par and sword.par have no <SpriteCut> and no
    // animationMode: the defaults (1x1, play) stand.
    QueryCut(Child(element, "SpriteCut"), system.spriteCutX, system.spriteCutY);
    QueryRGBA(Child(element, "Color0"), system.color0);
    QueryRGBA(Child(element, "Color1"), system.color1);
    QueryRGB(Child(element, "Luminance"), system.luminance);
    return system;
}

// ETHEntityFile::ReadFromXMLFile (E:ETHEntityFile.cpp:268-425).
EntityDef ReadEntityElement(const TiXmlElement& element) {
    EntityDef def = ResetEntity();
    QueryEnum(element, "type", def.type);
    QueryBool(element, "static", def.isStatic);
    QueryBool(element, "collidable", def.collidable);
    QueryBool(element, "applyLight", def.applyLight);
    QueryBool(element, "castShadow", def.castShadow);
    QueryUintViaInt(element, "startFrame", def.startFrame);
    QueryEnum(element, "blendMode", def.blendMode);
    QueryFloat(element, "layerDepth", def.layerDepth);
    QueryFloat(element, "soundVolume", def.soundVolume);
    QueryFloat(element, "shadowScale", def.shadowScale);
    QueryFloat(element, "shadowLengthScale", def.shadowLengthScale);
    QueryFloat(element, "shadowOpacity", def.shadowOpacity);
    QueryFloat(element, "specularPower", def.specularPower);
    QueryFloat(element, "specularBrightness", def.specularBrightness);

    QueryRGBA(Child(element, "EmissiveColor"), def.emissiveColor);
    QueryCut(Child(element, "SpriteCut"), def.spriteCutX, def.spriteCutY);
    QueryXY(Child(element, "PivotAdjust"), def.pivotAdjust);
    QueryText(element, "Sprite", def.sprite);
    QueryText(element, "Normal", def.normal);
    QueryText(element, "Gloss", def.gloss);

    // Up to two systems, in file order, each kept even with particles="0" -
    // 0.7.12 filled its fixed slots the same way and only started the systems
    // that have particles (E:ETHRenderEntity.cpp:359).
    if (const TiXmlElement* particles = Child(element, "Particles")) {
        const TiXmlElement* system = Child(*particles, "ParticleSystem");
        for (int t = 0; t < kMaxParticleSystems && system != nullptr; ++t) {
            def.particles.push_back(ReadParticleElement(*system));
            system = system->NextSiblingElement();
        }
    }

    if (const TiXmlElement* light = Child(element, "Light")) ReadLight(*light, def.light);

    // ETH_COLLISION_BOX::ReadFromXMLFile (E:ETHCommon.cpp:68-86). The box is read
    // whatever `collidable` says (7 .ent carry a box with collidable=0).
    if (const TiXmlElement* collision = Child(element, "Collision")) {
        QueryXYZ(Child(*collision, "Position"), def.collisionPos);
        QueryXYZ(Child(*collision, "Size"), def.collisionSize);
    }

    ReadCustomData(element, def.customData);
    return def;
}

// --- File readers ------------------------------------------------------------------

std::optional<EntityDef> ReadEntityFile(const string& path) {
    TiXmlDocument document;
    if (!LoadDocument(path, document)) return std::nullopt;
    const TiXmlElement* entity = FirstInnerElement(document, path);
    if (entity == nullptr) return std::nullopt;
    return ReadEntityElement(*entity);
}

std::optional<ParticleSystemDef> ReadParticleFile(const string& path) {
    TiXmlDocument document;
    if (!LoadDocument(path, document)) return std::nullopt;
    const TiXmlElement* system = FirstInnerElement(document, path);
    if (system == nullptr) return std::nullopt;
    return ReadParticleElement(*system);
}

// ETHScene::LoadFromFile / ReadFromXMLFile (E:ETHScene.cpp:222-311).
std::optional<SceneFile> ReadSceneFile(const string& path) {
    TiXmlDocument document;
    if (!LoadDocument(path, document)) return std::nullopt;
    const TiXmlElement* root = document.FirstChildElement();
    if (root == nullptr) {
        SUPERSONIC_LOG_WARN(kLog) << path << " has no root element";
        return std::nullopt;
    }
    SceneFile scene;
    scene.properties = ResetSceneProperties();
    ReadSceneProperties(*root, scene.properties);
    if (const TiXmlElement* entities = Child(*root, "EntitiesInScene")) {
        for (const TiXmlElement* entity = Child(*entities, "Entity"); entity != nullptr;
             entity = entity->NextSiblingElement()) {
            scene.entities.push_back(ReadPlacement(*entity));
        }
    }
    return scene;
}

// --- Writer ------------------------------------------------------------------------

// ETHScene::SaveToFile (E:ETHScene.cpp:182-221), as the bytes it left on disk:
// TinyXML's printer (four-space indent) and a text-mode FILE on Windows, so
// every line ends CRLF - the layout of every shipped .esc. The placements are
// written in the order given; 0.7.12 wrote its buckets' order, which is the
// caller's to reproduce. (0.7.12 refused to save a scene with no entities; that
// check belongs to SaveScene.)
string WriteSceneFile(const SceneFile& scene) {
    TiXmlDocument document;
    document.LinkEndChild(new TiXmlDeclaration("1.0", "", ""));
    TiXmlElement* root = AddChild(document, "Ethanon");
    WriteSceneProperties(scene.properties, *root);
    TiXmlElement* entities = AddChild(*root, "EntitiesInScene");
    for (const ScenePlacement& placement : scene.entities) WritePlacement(placement, *entities);

    TiXmlPrinter printer;
    printer.SetIndent("    ");
    printer.SetLineBreak("\r\n");
    document.Accept(&printer);
    return printer.Str();
}

#undef PENUMBRA_DEFS_SSCANF

} // namespace Penumbra::Eth
