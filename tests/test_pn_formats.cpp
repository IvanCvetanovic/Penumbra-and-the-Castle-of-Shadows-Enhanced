// The original's data files through the Eth layer's readers and writers:
// every scene and entity parses and matches the census in
// docs/spec/20-formats-scene-entity.md, the 0.7.12 writer reproduces the
// shipped scenes byte for byte, ENML reads data.enml/hs.enml as enml.h did and
// writes only to the user directory, image headers are read by content, and
// the text helpers convert cp1252 and format numbers as AngelScript did.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "TestHarness.hpp"
#include "eth/Defs.hpp"
#include "eth/Eth.hpp"
#include "eth/ImageInfo.hpp"
#include "eth/Machine.hpp"
#include "eth/Text.hpp"

using namespace Penumbra::Eth;
namespace fs = std::filesystem;

namespace {

const string kApp = PENUMBRA_ORIGINAL_DIR;

string ReadBytes(const string& path) {
    std::ifstream file(fs::path(path), std::ios::binary);
    return string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

void WriteBytes(const fs::path& path, const string& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

std::vector<string> FilesWithExtension(const string& directory, const string& extension) {
    std::vector<string> names;
    for (const auto& entry : fs::directory_iterator(fs::path(directory))) {
        if (entry.is_regular_file() && entry.path().extension().string() == extension) {
            names.push_back(entry.path().filename().string());
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

// Exact case, as a case-sensitive filesystem would demand: the name must be in
// its directory's listing byte for byte (Windows' own lookup ignores case).
bool ExistsExactCase(const string& relativePath) {
    const fs::path path = fs::path(kApp) / fs::path(relativePath);
    if (!fs::exists(path.parent_path())) return false;
    const string name = path.filename().string();
    for (const auto& entry : fs::directory_iterator(path.parent_path())) {
        if (entry.path().filename().string() == name) return true;
    }
    return false;
}

// Where the first difference is, for a failed byte comparison.
string FirstDifference(const string& a, const string& b) {
    std::size_t i = 0;
    while (i < a.size() && i < b.size() && a[i] == b[i]) ++i;
    const std::size_t from = i > 40 ? i - 40 : 0;
    return "sizes " + std::to_string(a.size()) + "/" + std::to_string(b.size()) + ", first difference at " +
           std::to_string(i) + ": [" + a.substr(from, 80) + "] vs [" + b.substr(from, 80) + "]";
}

bool Same(const glm::vec2& a, const glm::vec2& b) { return a == b; }
bool Same(const glm::vec3& a, const glm::vec3& b) { return a == b; }
bool Same(const glm::vec4& a, const glm::vec4& b) { return a == b; }

bool SameCustomData(const CustomData& a, const CustomData& b) {
    if (a.size() != b.size()) return false;
    for (const auto& [name, value] : a) {
        const auto it = b.find(name);
        if (it == b.end()) return false;
        const CustomValue& other = it->second;
        if (value.type != other.type) return false;
        switch (value.type) {
        case DT_FLOAT: if (value.f != other.f) return false; break;
        case DT_INT: if (value.i != other.i) return false; break;
        case DT_UINT: if (value.u != other.u) return false; break;
        case DT_STRING: if (value.s != other.s) return false; break;
        case DT_NODATA: break;
        }
    }
    return true;
}

bool SameParticles(const ParticleSystemDef& a, const ParticleSystemDef& b) {
    return a.bitmap == b.bitmap && a.soundEffect == b.soundEffect && a.allAtOnce == b.allAtOnce &&
           a.boundingSphere == b.boundingSphere && a.alphaMode == b.alphaMode && a.nParticles == b.nParticles &&
           Same(a.gravity, b.gravity) && Same(a.direction, b.direction) && Same(a.randomizeDir, b.randomizeDir) &&
           Same(a.startPoint, b.startPoint) && Same(a.randStartPoint, b.randStartPoint) &&
           a.spriteCutX == b.spriteCutX && a.spriteCutY == b.spriteCutY && Same(a.color0, b.color0) &&
           Same(a.color1, b.color1) && a.lifeTime == b.lifeTime && a.randomizeLifeTime == b.randomizeLifeTime &&
           a.angleDir == b.angleDir && a.randAngle == b.randAngle && a.size == b.size &&
           a.randomizeSize == b.randomizeSize && a.growth == b.growth && a.minSize == b.minSize &&
           a.maxSize == b.maxSize && a.repeat == b.repeat && a.animationMode == b.animationMode &&
           Same(a.luminance, b.luminance) && a.angleStart == b.angleStart && a.randAngleStart == b.randAngleStart;
}

bool SameLight(const LightDef& a, const LightDef& b) {
    return a.active == b.active && a.isStatic == b.isStatic && a.castShadows == b.castShadows &&
           a.range == b.range && a.haloBrightness == b.haloBrightness && a.haloSize == b.haloSize &&
           Same(a.position, b.position) && Same(a.color, b.color) && a.haloBitmap == b.haloBitmap;
}

bool SameDef(const EntityDef& a, const EntityDef& b) {
    if (a.particles.size() != b.particles.size()) return false;
    for (std::size_t i = 0; i < a.particles.size(); ++i) {
        if (!SameParticles(a.particles[i], b.particles[i])) return false;
    }
    return a.type == b.type && a.isStatic == b.isStatic && a.collidable == b.collidable &&
           a.startFrame == b.startFrame && a.applyLight == b.applyLight && a.castShadow == b.castShadow &&
           a.blendMode == b.blendMode && a.layerDepth == b.layerDepth && a.soundVolume == b.soundVolume &&
           a.shadowScale == b.shadowScale && a.shadowLengthScale == b.shadowLengthScale &&
           a.shadowOpacity == b.shadowOpacity && a.specularPower == b.specularPower &&
           a.specularBrightness == b.specularBrightness && Same(a.emissiveColor, b.emissiveColor) &&
           a.spriteCutX == b.spriteCutX && a.spriteCutY == b.spriteCutY && Same(a.pivotAdjust, b.pivotAdjust) &&
           a.sprite == b.sprite && a.normal == b.normal && a.gloss == b.gloss && SameLight(a.light, b.light) &&
           Same(a.collisionPos, b.collisionPos) && Same(a.collisionSize, b.collisionSize) &&
           SameCustomData(a.customData, b.customData);
}

bool SamePlacement(const ScenePlacement& a, const ScenePlacement& b) {
    return a.id == b.id && a.spriteFrame == b.spriteFrame && a.entityName == b.entityName &&
           Same(a.color, b.color) && Same(a.position, b.position) && a.angle == b.angle && SameDef(a.def, b.def);
}

const ScenePlacement* FindById(const SceneFile& scene, const int id) {
    for (const ScenePlacement& placement : scene.entities) {
        if (placement.id == id) return &placement;
    }
    return nullptr;
}

// docs/spec/20-formats-scene-entity.md §5.1: placements per scene, 2607 in all.
const std::map<string, std::size_t> kCensus = {
    {"arena_select.esc", 41}, {"gameover.esc", 1},  {"level1.esc", 630},  {"level2.esc", 429},
    {"level3.esc", 866},      {"menu.esc", 69},     {"pvp_lv1.esc", 90},  {"pvp_lv2.esc", 144},
    {"pvp_lv3.esc", 69},      {"pvp_lv4.esc", 57},  {"pvp_lv5.esc", 97},  {"pvp_lv6.esc", 88},
    {"videoModes.esc", 26},
};

// --- Scenes -------------------------------------------------------------------

void TestScenes(std::map<string, SceneFile>& scenes) {
    const std::vector<string> files = FilesWithExtension(kApp + "/scenes", ".esc");
    CHECK_EQ(files.size(), std::size_t(13));
    std::size_t total = 0;
    for (const string& name : files) {
        const auto scene = ReadSceneFile(kApp + "/scenes/" + name);
        CHECK_MSG(scene.has_value(), name + " did not parse");
        if (!scene) continue;
        total += scene->entities.size();
        const auto expected = kCensus.find(name);
        CHECK_MSG(expected != kCensus.end() && expected->second == scene->entities.size(),
                  name + ": " + std::to_string(scene->entities.size()) + " placements");
        scenes[name] = *scene;
    }
    CHECK_EQ(total, std::size_t(2607));
}

void TestLevel1(const SceneFile& level1) {
    // SceneProperties: ambient 0.2,0.1,0.2, intensity 2, and ZAxisDirection 0,0
    // (not the 0,-1 default).
    CHECK(Same(level1.properties.ambient, vector3(0.2f, 0.1f, 0.2f)));
    CHECK(level1.properties.lightIntensity == 2.0f);
    CHECK(Same(level1.properties.zAxisDirection, vector2(0.0f, 0.0f)));

    // The player's spawn marker (setupScene.as:273-280 reads its 'name').
    const ScenePlacement* spawn = FindById(level1, 55);
    CHECK(spawn != nullptr);
    if (spawn != nullptr) {
        CHECK(spawn->entityName == "spawn");
        CHECK(Same(spawn->position, vector3(410.0f, -42.0f, 0.0f)));
        CHECK(spawn->spriteFrame == 0u);
        const auto name = spawn->def.customData.find("name");
        CHECK(name != spawn->def.customData.end() && name->second.type == DT_STRING && name->second.s == "bruxo");
        const auto complete = spawn->def.customData.find("complete");
        CHECK(complete != spawn->def.customData.end() && complete->second.type == DT_INT && complete->second.i == 0);
        CHECK(spawn->def.particles.empty());
        CHECK(spawn->def.sprite.empty());
    }

    std::size_t spawns = 0;
    bool apostrophe = false;
    bool accented = false;
    for (const ScenePlacement& placement : level1.entities) {
        if (placement.entityName == "spawn") ++spawns;
        if (placement.entityName != "help") continue;
        const auto message = placement.def.customData.find("message");
        if (message == placement.def.customData.end()) continue;
        // &apos; decoded (level1.esc:6397); the cp1252 byte passed through.
        if (message->second.s == "Golpe de espada: tecla 'S'") apostrophe = true;
        if (message->second.s == "Alguns inimigos s\xE3o mais fortes que outros") accented = true;
    }
    CHECK_EQ(spawns, std::size_t(48));
    CHECK(apostrophe);
    CHECK(accented);

    // The inline copy wins over the .ent (spec §4.1): blue_light id 423 still
    // has the halo it was placed with.
    const ScenePlacement* blue = FindById(level1, 423);
    CHECK(blue != nullptr && blue->entityName == "blue_light.ent");
    if (blue != nullptr) {
        CHECK(blue->def.light.active);
        CHECK(blue->def.light.haloBitmap == "halo.bmp");
        CHECK(blue->def.light.haloSize == 146.0f);
        CHECK(blue->def.light.haloBrightness == 0.0f);
        CHECK(blue->def.light.range == 232.0f);
    }
}

// The 0.7.12 writer over what the reader produced must give the shipped
// bytes back: same TinyXML printer, same "%g" numbers, same CRLF.
void TestSceneRoundTrip(const std::map<string, SceneFile>& scenes, const fs::path& scratch) {
    for (const auto& [name, scene] : scenes) {
        const string original = ReadBytes(kApp + "/scenes/" + name);
        const string written = WriteSceneFile(scene);
        CHECK_MSG(written == original, name + " rewritten differs: " + FirstDifference(written, original));
    }

    // Parsed back, level1's placements are equal field by field - ids, names,
    // positions, frames, custom data and particle systems.
    const auto level1 = scenes.find("level1.esc");
    if (level1 == scenes.end()) return;
    const fs::path path = scratch / "level1_roundtrip.esc";
    WriteBytes(path, WriteSceneFile(level1->second));
    const auto back = ReadSceneFile(path.string());
    CHECK(back.has_value());
    if (!back) return;
    CHECK_EQ(back->entities.size(), level1->second.entities.size());
    std::size_t equal = 0;
    for (std::size_t i = 0; i < back->entities.size() && i < level1->second.entities.size(); ++i) {
        if (SamePlacement(back->entities[i], level1->second.entities[i])) ++equal;
    }
    CHECK_EQ(equal, level1->second.entities.size());
    CHECK(Same(back->properties.ambient, level1->second.properties.ambient));
    CHECK(Same(back->properties.zAxisDirection, level1->second.properties.zAxisDirection));

    // Runtime custom data (SaveScene's checkpoint) survives with its types.
    SceneFile edited = level1->second;
    CustomValue u;
    u.type = DT_UINT;
    u.u = 4294967295u;
    edited.entities[0].def.customData["lastTimeAlive"] = u;
    CustomValue f;
    f.type = DT_FLOAT;
    f.f = -12.5f;
    edited.entities[0].def.customData["forceX"] = f;
    CustomValue s;
    s.type = DT_STRING;
    s.s = "a<b>&\"c'\xE9";
    edited.entities[0].def.customData["hitBy"] = s;
    WriteBytes(path, WriteSceneFile(edited));
    const auto editedBack = ReadSceneFile(path.string());
    CHECK(editedBack.has_value() && SamePlacement(editedBack->entities[0], edited.entities[0]));
}

// --- Entities -------------------------------------------------------------------

void TestEntities(std::map<string, EntityDef>& entities) {
    const std::vector<string> files = FilesWithExtension(kApp + "/entities", ".ent");
    CHECK_EQ(files.size(), std::size_t(144));
    std::size_t parsed = 0;
    for (const string& name : files) {
        const auto def = ReadEntityFile(kApp + "/entities/" + name);
        CHECK_MSG(def.has_value(), name + " did not parse");
        if (!def) continue;
        ++parsed;
        entities[name] = *def;
    }
    CHECK_EQ(parsed, std::size_t(144));

    const auto torch = entities.find("torch.ent");
    CHECK(torch != entities.end());
    if (torch != entities.end()) {
        const EntityDef& def = torch->second;
        CHECK(def.sprite == "torch_small.png" && def.normal == "light01_normal.png");
        CHECK(def.isStatic && def.applyLight && def.blendMode == AM_PIXEL);
        CHECK_EQ(def.particles.size(), std::size_t(1));
        if (!def.particles.empty()) {
            const ParticleSystemDef& p = def.particles[0];
            CHECK_EQ(p.nParticles, 18);
            CHECK(p.lifeTime == 850.0f);
            CHECK(p.randomizeLifeTime == 700.0f);
            CHECK(p.alphaMode == AM_ADD);
            CHECK(p.repeat == 0 && !p.allAtOnce);
            CHECK(Same(p.startPoint, vector3(0.0f, -8.0f, 8.0f)));
            CHECK(p.bitmap == "explosion.JPG");
            CHECK(p.boundingSphere == 76.8f);
            CHECK(p.gravity.y == -0.048f);
            CHECK(Same(p.luminance, vector3(1.0f)));
        }
        CHECK(def.light.active && def.light.isStatic && def.light.castShadows);
        CHECK(def.light.range == 227.5f);
        CHECK(def.light.haloBitmap == "halo1.bmp");
        CHECK(def.light.haloSize == 144.0f && def.light.haloBrightness == 0.5f);
        CHECK(Same(def.light.color, vector3(1.0f, 0.7f, 0.3f)));
        CHECK(Same(def.light.position, vector3(0.0f, -14.0f, 8.0f)));
        CHECK(def.customData.empty());
    }

    const auto bruxo = entities.find("bruxo.ent");
    CHECK(bruxo != entities.end());
    if (bruxo != entities.end()) {
        const EntityDef& def = bruxo->second;
        CHECK(def.spriteCutX == 4 && def.spriteCutY == 4);
        CHECK(Same(def.collisionPos, vector3(0.0f, 4.0f, 0.0f)));
        CHECK(Same(def.collisionSize, vector3(14.0f, 36.0f, 8.0f)));
        CHECK(def.collidable && !def.isStatic && def.startFrame == 8u && def.blendMode == AM_ALPHA_TEST);
        CHECK(def.particles.size() == 1 && def.particles[0].alphaMode == AM_MODULATE);
        CHECK_EQ(def.customData.size(), std::size_t(21));
        const auto speed = def.customData.find("speed");
        CHECK(speed != def.customData.end() && speed->second.type == DT_FLOAT && speed->second.f == 150.0f);
        const auto coolDown = def.customData.find("coolDown");
        CHECK(coolDown != def.customData.end() && coolDown->second.type == DT_UINT && coolDown->second.u == 200u);
        const auto hp = def.customData.find("hp");
        CHECK(hp != def.customData.end() && hp->second.type == DT_INT && hp->second.i == 100);
        CHECK(!def.light.active);
    }

    // The .ent the level1 placement 423 differs from (spec §4.1).
    const auto blue = entities.find("blue_light.ent");
    CHECK(blue != entities.end() && blue->second.light.haloBitmap == "halo1.bmp" &&
          blue->second.light.haloSize == 288.0f);
}

void TestParticleFiles() {
    const std::vector<string> files = FilesWithExtension(kApp + "/effects", ".par");
    CHECK_EQ(files.size(), std::size_t(43));
    std::size_t parsed = 0;
    for (const string& name : files) {
        if (ReadParticleFile(kApp + "/effects/" + name).has_value()) ++parsed;
    }
    CHECK_EQ(parsed, files.size());

    // A legacy file without animationMode and <SpriteCut> keeps the defaults.
    const auto sword = ReadParticleFile(kApp + "/effects/sword.par");
    CHECK(sword.has_value() && sword->animationMode == ParticleSystemDef::kPlayAnimation &&
          sword->spriteCutX == 1 && sword->spriteCutY == 1 && sword->nParticles == 16);

    CHECK(!ReadSceneFile(kApp + "/scenes/no_such_scene.esc").has_value());
    CHECK(!ReadEntityFile(kApp + "/entities/no_such_entity.ent").has_value());
}

// --- ENML -------------------------------------------------------------------------

void TestEnmlData() {
    // The raw CRLF bytes, as a GetStringFromFile that does not fold line ends
    // would hand them over; the parser folds them as the original's text-mode
    // read did.
    enmlFile data;
    CHECK_EQ(data.parseString(ReadBytes(kApp + "/data.enml")), 0u);
    CHECK(data.exists("global") && data.exists("warrior") && data.exists("king") && !data.exists("bruxo"));

    int lives = 0;
    CHECK(data.getInt("global", "lives", lives) && lives == 13);
    CHECK(data.get("warrior", "hp") == "75");
    int hp = 0;
    CHECK(data.getInt("warrior", "hp", hp) && hp == 75);
    uint coolDown = 0;
    CHECK(data.getUint("master_knight", "coolDown", coolDown) && coolDown == 420u);
    float bias = 0.0f;
    CHECK(data.getFloat("knight", "pushBackBias", bias) && bias == 0.1f);
    double biasD = 0.0;
    CHECK(data.getDouble("paladin", "pushBackBias", biasD) && biasD == 0.05);
    CHECK(data.get("minion", "chaseSfx") == "minion.ogg");

    // global.lv20 is missing: false, and the out value is left as it was
    // (addToExp keeps lv19's 11000, util.as:406-418).
    int next = 12345;
    CHECK(!data.getInt("global", "lv20", next) && next == 12345);
    CHECK(data.getInt("global", "lv19", next) && next == 11000);
    CHECK(data.getInt("global", "lv21", next) && next == 11000);
    float nextF = 7.0f;
    CHECK(!data.getFloat("global", "lv20", nextF) && nextF == 7.0f);
    CHECK(data.get("global", "lv20").empty() && data.get("nosection", "lives").empty());

    // A multi-line value: the leading line break skipped, the inner ones kept
    // (as LF - the text-mode read folded them), the cp1252 accents intact.
    // The original's wording is "muito a aten\xE7\xE3o" (data.enml, story01).
    CHECK(data.get("global", "story01") ==
          "Pelos corredores subterr\xE2neos\nde penumbra \xE9 poss\xEDvel chegar\n"
          "perto do Castelo das Sombras\nsem chamar muito a aten\xE7\xE3o");

    enmlFile fromFile;
    CHECK(fromFile.parseFromFile(kApp + "/data.enml"));
    CHECK(fromFile.get("global", "story01") == data.get("global", "story01"));

    const string hsBytes = ReadBytes(kApp + "/hs.enml");
    enmlFile hs;
    CHECK_EQ(hs.parseString(hsBytes), 0u);
    uint best = 0;
    CHECK(hs.getUint("hs", "hs0", best) && best == 3599000u);
    CHECK(hs.getUint("hs", "hs4", best) && best == 3599000u);
    // generateString, with the text-mode write's CRLF, is the shipped file.
    string crlf;
    for (const char c : hs.generateString()) {
        if (c == '\n') crlf += '\r';
        crlf += c;
    }
    CHECK_MSG(crlf == hsBytes, FirstDifference(crlf, hsBytes));
}

void TestEnmlGrammar() {
    enmlFile f;
    // Escapes: \; and \\ only; trailing whitespace kept, leading skipped.
    CHECK_EQ(f.parseString("a\n{\n\tk =   x\\;y\\\\z;\n\tt = v  ;\n}\n"), 0u);
    CHECK(f.get("a", "k") == "x;y\\z");
    CHECK(f.get("a", "t") == "v  ");

    // '/' comments outside values; a '/' inside a value is a byte of it.
    CHECK_EQ(f.parseString("/ header\na { k = 1; / note\n j = a/b; }"), 0u);
    CHECK(f.get("a", "k") == "1" && f.get("a", "j") == "a/b");

    // Any other backslash fails the parse, reports the line and empties the file.
    CHECK_EQ(f.parseString("a {\n k = x\\qy;\n}"), 2u);
    CHECK(!f.exists("a"));
    // Missing '=' on line 3.
    CHECK_EQ(f.parseString("a\n{\n k 1;\n}"), 3u);
    // An empty value is invalid.
    CHECK_EQ(f.parseString("a { k = ; }"), 1u);
    // A section without its '}' is dropped silently.
    CHECK_EQ(f.parseString("a { k = 1; }\nb { j = 2;"), 0u);
    CHECK(f.exists("a") && !f.exists("b"));
    // A repeated section replaces the first; a repeated key keeps the last.
    CHECK_EQ(f.parseString("a { k = 1; j = 1; j = 2; }\na { k = 3; }"), 0u);
    CHECK(f.get("a", "k") == "3" && f.get("a", "j").empty());

    // generateString: map order, the backslashes restored.
    enmlFile out;
    enmlEntity entity;
    entity.add("z", "last");
    entity.add("a", "x;y\\z");
    out.addEntity("section", entity);
    enmlEntity empty;
    out.addEntity("nothing", empty);    // an empty entity adds no section
    CHECK(!out.exists("nothing"));
    CHECK(out.generateString() == "section\n{\n\ta = x\\;y\\\\z;\n\tz = last;\n}\n\n");
    enmlFile again;
    CHECK_EQ(again.parseString(out.generateString()), 0u);
    CHECK(again.get("section", "a") == "x;y\\z");
    // addEntity over an existing section replaces its attributes.
    enmlEntity replacement;
    replacement.add("b", "1");
    out.addEntity("section", replacement);
    CHECK(out.get("section", "a").empty() && out.get("section", "b") == "1");
}

// writeToFile never writes into the original's folder: a game path goes to the
// machine's user directory, and without one nothing is written.
void TestEnmlWriteRedirect(const fs::path& scratch) {
    const string hsPath = kApp + "/hs.enml";
    const string original = ReadBytes(hsPath);

    enmlFile hs;
    hs.parseString(original);
    enmlEntity scores;
    for (int i = 0; i < 5; ++i) scores.add("hs" + std::to_string(i), std::to_string(1000 + i));
    hs.addEntity("hs", scores);

    // No machine: refused.
    if (Machine::CurrentOrNull() == nullptr) {
        hs.writeToFile(hsPath);
        CHECK(ReadBytes(hsPath) == original);
    }

    {
        MachineConfig config;           // no user root
        Machine machine(config);
        Machine::Scope scope(machine);
        hs.writeToFile(hsPath);
        CHECK(ReadBytes(hsPath) == original);
    }

    MachineConfig config;
    config.userRoot = (scratch / "user").generic_string();
    Machine machine(config);
    Machine::Scope scope(machine);
    hs.writeToFile(hsPath);
    CHECK(ReadBytes(hsPath) == original);
    const string target = machine.WritePath("hs.enml");
    CHECK(!target.empty() && fs::exists(fs::path(target)));
    CHECK(ReadBytes(target) ==
          "hs\r\n{\r\n\ths0 = 1000;\r\n\ths1 = 1001;\r\n\ths2 = 1002;\r\n\ths3 = 1003;\r\n\ths4 = 1004;\r\n}\r\n\r\n");

    // Reading the game path now finds the user's copy.
    enmlFile back;
    CHECK(back.parseFromFile(hsPath));
    uint best = 0;
    CHECK(back.getUint("hs", "hs0", best) && best == 1000u);
}

// --- Images -----------------------------------------------------------------------

void TestImageSizes(const fs::path& scratch) {
    CHECK(ReadImageSize(kApp + "/entities/bruxo.png") == vector2(128.0f, 192.0f));
    // A BMP under a .jpg name.
    CHECK(ReadImageSize(kApp + "/entities/normalmaps/nm_white_ground.jpg") == vector2(256.0f, 256.0f));
    CHECK(ReadImageSize(kApp + "/entities/thorn.dds") == vector2(256.0f, 64.0f));
    // 8-bit paletted BMPs.
    CHECK(ReadImageSize(kApp + "/entities/halo1.bmp") == vector2(123.0f, 123.0f));
    CHECK(ReadImageSize(kApp + "/entities/halo.bmp") == vector2(265.0f, 253.0f));
    // A progressive JPEG (SOF2) and a baseline one.
    CHECK(ReadImageSize(kApp + "/entities/face.jpg") == vector2(256.0f, 256.0f));
    CHECK(ReadImageSize(kApp + "/entities/lava.jpg") == vector2(128.0f, 128.0f));
    CHECK(ReadImageSize(kApp + "/entities/mario_BG.png") == vector2(1024.0f, 768.0f));
    CHECK(ReadImageSize(kApp + "/entities/no_such_image.png") == vector2(0.0f));

    // No TGA ships; a minimal uncompressed one: 3x5, 24 bpp.
    string tga(18, '\0');
    tga[2] = 2;
    tga[12] = 3;
    tga[14] = 5;
    tga[16] = 24;
    tga += string(3 * 5 * 3, '\x7F');
    const fs::path tgaPath = scratch / "tiny.tga";
    WriteBytes(tgaPath, tga);
    CHECK(ReadImageSize(tgaPath.string()) == vector2(3.0f, 5.0f));
}

// Every bitmap a definition names resolves, with exact case, to an image whose
// header reads: Sprite/Gloss/HaloBitmap under entities/, Normal under
// entities/normalmaps/, particle Bitmap under particles/ (spec §12.1).
void TestImageReferences(const std::map<string, SceneFile>& scenes, const std::map<string, EntityDef>& entities) {
    std::set<string> references;
    const auto collect = [&references](const EntityDef& def) {
        if (!def.sprite.empty()) references.insert("entities/" + def.sprite);
        if (!def.gloss.empty()) references.insert("entities/" + def.gloss);
        if (!def.light.haloBitmap.empty()) references.insert("entities/" + def.light.haloBitmap);
        if (!def.normal.empty()) references.insert("entities/normalmaps/" + def.normal);
        for (const ParticleSystemDef& system : def.particles) {
            if (!system.bitmap.empty()) references.insert("particles/" + system.bitmap);
        }
    };
    for (const auto& [name, def] : entities) collect(def);
    for (const auto& [name, scene] : scenes) {
        for (const ScenePlacement& placement : scene.entities) collect(placement.def);
    }

    CHECK_EQ(references.size(), std::size_t(130));
    for (const string& reference : references) {
        CHECK_MSG(ExistsExactCase(reference), reference + " does not exist with that case");
        const vector2 size = ReadImageSize(kApp + "/" + reference);
        CHECK_MSG(size.x > 0.0f && size.y > 0.0f, reference + " has no readable size");
    }
}

// --- Text -------------------------------------------------------------------------

void TestText() {
    CHECK(Cp1252ToUtf8("a\xE3\xE7") == "a\xC3\xA3\xC3\xA7");
    CHECK(Cp1252ToUtf8("\x95") == "\xE2\x80\xA2");      // the bullet in menu.as
    CHECK(Cp1252ToUtf8("\x80") == "\xE2\x82\xAC");
    CHECK(Cp1252ToUtf8("\x81") == "\xEF\xBF\xBD");      // undefined in cp1252
    CHECK(Cp1252CodePoint(0x95) == 0x2022u && Cp1252CodePoint(0xE9) == 0xE9u && Cp1252CodePoint(0x9D) == 0xFFFDu);
    CHECK(Utf8ToCp1252("\xE2\x80\xA2") == "\x95");
    CHECK(Utf8ToCp1252("S\xC3\xA3o \xE4\xB8\xAD") == "S\xE3o ?");
    CHECK(Utf8ToCp1252("\xC2\x81") == "?");             // a C1 control is not cp1252's 0x81
    CHECK(Utf8ToCp1252("a\xC3") == "a?");               // truncated
    // E21: 0x8D, undefined in cp1252, is the port's U+0107 (c with acute),
    // both ways; the other four undefined bytes stay U+FFFD.
    CHECK_EQ(Cp1252CodePoint(0x8D), 0x0107u);
    CHECK_EQ(Cp1252CodePoint(kCAcuteByte), kCAcuteCodePoint);
    CHECK(Cp1252ToUtf8("Cvetanovi\x8D") == "Cvetanovi\xC4\x87");
    CHECK(Utf8ToCp1252("Ivan Cvetanovi\xC4\x87") == "Ivan Cvetanovi\x8D");
    CHECK(Utf8ToCp1252("\xC4\x86") == "?");             // the capital, U+0106: not held
    for (const unsigned b : {0x81u, 0x8Fu, 0x90u, 0x9Du}) {
        CHECK_EQ(Cp1252CodePoint(static_cast<unsigned char>(b)), 0xFFFDu);
    }
    std::size_t roundTrips = 0;
    for (int b = 1; b < 256; ++b) {
        if (b == 0x81 || b == 0x8F || b == 0x90 || b == 0x9D) continue;
        const string one(1, static_cast<char>(b));
        if (Utf8ToCp1252(Cp1252ToUtf8(one)) == one) ++roundTrips;
    }
    CHECK_EQ(roundTrips, std::size_t(251));   // 0x8D included (E21)

    // AngelScript's string + number (ostringstream, six significant digits).
    CHECK(Str(1.5f) == "1.5");
    CHECK(Str(0.1f) == "0.1");
    CHECK(Str(100.0f) == "100");
    CHECK(Str(1234567.0f) == "1.23457e+06");
    CHECK(Str(-0.048f) == "-0.048");
    CHECK(Str(1.0 / 3.0) == "0.333333");
    CHECK(Str(-3) == "-3");
    CHECK(Str(4294967295u) == "4294967295");
    CHECK(Str(true) == "1" && Str(false) == "0");
    CHECK(Str(vector2(1.5f, -2.0f)) == "(1.5, -2)");
}

} // namespace

int main() {
    if (!fs::exists(fs::path(kApp) / "main.as")) {
        std::printf("SKIP: the original is not at %s\n", kApp.c_str());
        return 77;
    }

    const fs::path scratch = fs::temp_directory_path() / "penumbra_test_pn_formats";
    std::error_code ignored;
    fs::remove_all(scratch, ignored);
    fs::create_directories(scratch, ignored);

    std::map<string, SceneFile> scenes;
    TestScenes(scenes);
    const auto level1 = scenes.find("level1.esc");
    CHECK(level1 != scenes.end());
    if (level1 != scenes.end()) TestLevel1(level1->second);
    TestSceneRoundTrip(scenes, scratch);

    std::map<string, EntityDef> entities;
    TestEntities(entities);
    TestParticleFiles();

    TestEnmlData();
    TestEnmlGrammar();
    TestImageSizes(scratch);
    TestImageReferences(scenes, entities);
    TestText();
    TestEnmlWriteRedirect(scratch);

    fs::remove_all(scratch, ignored);
    return test::summary("test_pn_formats", 350);
}

