// The English variants of the images with Portuguese baked into their pixels
// (E5): game/data/strings.json "images" against the files
// tools/art/make_english_art.py generated into game/data/images/en, and the
// renderers swapping them with the language at run time.
//   - every variant strings.json names resolves (Localization::ImageVariant) to
//     an existing file of the original's pixel size, and differs from it;
//   - Portuguese, an image with no entry and an empty "en" resolve to "";
//   - the menu sheet keeps its 1x8 frame grid (frames 0-6 inked, 7 empty) and
//     its normal map is flat (127,127,255) away from the letters;
//   - SpriteRenderer draws the menu button's image AND its normal map from the
//     variants in English, the originals in Portuguese, and swaps both when the
//     language changes between two draws of the same sprite; HudRenderer does
//     the same for a DrawSprite of the back arrow.
//   - ENHANCEMENT E24: another language's own art, images/<id>/<the original's
//     path>, where there is one - every such file an image strings.json lists,
//     at the original's size - and the English art where there is none; and a
//     switch from one language to another (German to French and back) hands
//     each renderer that language's file, not the one it cached before.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <entt/entt.hpp>

#include "TestHarness.hpp"
#include "core/Components.hpp"
#include "core/Json.hpp"
#include "eth/Snapshot.hpp"
#include "render/CameraRig.hpp"
#include "render/DrawOrder.hpp"
#include "render/FontAtlas.hpp"
#include "render/HudRenderer.hpp"
#include "render/Lighting.hpp"
#include "render/Localization.hpp"
#include "render/SpriteRenderer.hpp"
#include "render/TextureCache.hpp"
#include "render/TextureDecode.hpp"
#include "render/View.hpp"

using namespace Penumbra;
using namespace Penumbra::Render;

namespace {

const std::string kApp = PENUMBRA_ORIGINAL_DIR;

// The images the generator makes (tools/art/make_english_art.py) that
// strings.json must wire: the same font as the original's, so no judgement call.
const std::vector<std::string> kRequired = {
    "entities/menu_buttons.png",
    "entities/normalmaps/menu_nm_buttons.png",
    "entities/menu_buttons_gloss.png",
    "interface/input_options1.png",
    "interface/input_options2.png",
};
// Made in a stand-in typeface (the original's uncial is not installed): wired
// today, but emptying their "en" to keep the Portuguese must not fail a suite.
const std::vector<std::string> kOptional = {
    "entities/gamelogo.png",
    "interface/arrow_button.png",
};

bool Contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

std::string ReadText(const std::string& path) {
    std::ifstream file(std::filesystem::path(path), std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

// One variant against its original: there, the same pixel size, decodable,
// not a copy, and English only.
void CheckVariant(const Localization& loc, const std::string& relative) {
    const std::string en = loc.ImageVariant(relative, Language::English);
    CHECK_MSG(!en.empty(), relative);
    if (en.empty()) return;
    CHECK_MSG(std::filesystem::is_regular_file(std::filesystem::path(en)), en);
    CHECK_MSG(Contains(en, "/images/en/" + relative), en);
    CHECK_MSG(loc.ImageVariant(relative, Language::Portuguese).empty(), relative);

    const std::string original = kApp + "/" + relative;
    const glm::ivec2 size = ProbeImageSize(original);
    const glm::ivec2 variantSize = ProbeImageSize(en);
    CHECK_MSG(size.x > 0 && size.y > 0, original);
    CHECK_MSG(variantSize == size, relative + " is " + std::to_string(variantSize.x) + "x" +
                                       std::to_string(variantSize.y) + ", the original " +
                                       std::to_string(size.x) + "x" + std::to_string(size.y));

    // Decodes as the renderer will, and is not the original copied.
    const DecodedImage a = DecodeTexture(original, TextureVariant::Plain);
    const DecodedImage b = DecodeTexture(en, TextureVariant::Plain);
    CHECK_MSG(a.Valid() && b.Valid(), relative);
    if (a.Valid() && b.Valid()) CHECK_MSG(a.rgba != b.rgba, relative + " is the original unchanged");
}

void TestVariantsResolve() {
    Localization loc;
    CHECK(loc.Load());

    for (const std::string& relative : kRequired) CheckVariant(loc, relative);
    for (const std::string& relative : kOptional) {
        if (!loc.ImageVariant(relative, Language::English).empty()) CheckVariant(loc, relative);
        CHECK_MSG(loc.ImageVariant(relative, Language::Portuguese).empty(), relative);
    }

    // Lookup ignores case and slash direction, as the scripts' paths vary.
    CHECK(loc.ImageVariant("Entities\\Menu_Buttons.PNG", Language::English) ==
          loc.ImageVariant("entities/menu_buttons.png", Language::English));
    // No entry: the original.
    CHECK(loc.ImageVariant("entities/bruxo.png", Language::English).empty());
    CHECK(loc.ImageVariant("entities/gameover.png", Language::English).empty());

    // Every entry strings.json fills resolves, so a typo in a path fails here
    // rather than silently drawing Portuguese.
    Supersonic::Json::Value root;
    std::string error;
    std::string text = ReadText(Localization::DefaultPath());
    if (text.rfind("\xEF\xBB\xBF", 0) == 0) text.erase(0, 3);
    CHECK_MSG(Supersonic::Json::Parse(text, root, error), error);
    std::vector<std::string> filled;
    for (const auto& [path, entry] : root["images"].AsObject()) {
        if (path.empty() || path[0] == '_') continue;
        if (entry["en"].AsString().empty()) continue;
        filled.push_back(path);
        CHECK_MSG(!loc.ImageVariant(path, Language::English).empty(), path + " names a file that is not there");
    }
    for (const std::string& relative : kRequired) {
        bool listed = false;
        for (const std::string& path : filled) listed = listed || path == relative;
        CHECK_MSG(listed, relative + " has no English in strings.json");
    }
}

void TestMenuSheetLayout() {
    Localization loc;
    loc.Load();
    const std::string sheetPath = loc.ImageVariant("entities/menu_buttons.png", Language::English);
    const std::string normalPath = loc.ImageVariant("entities/normalmaps/menu_nm_buttons.png", Language::English);
    const DecodedImage sheet = DecodeTexture(sheetPath, TextureVariant::Plain);
    const DecodedImage normal = DecodeTexture(normalPath, TextureVariant::Plain);
    CHECK(sheet.Valid() && sheet.width == 512 && sheet.height == 512);
    CHECK(normal.Valid() && normal.width == 512 && normal.height == 512);
    if (!sheet.Valid() || !normal.Valid()) return;

    // buttons.ent's SpriteCut 1x8: seven labels in 512x64 frames, the eighth
    // empty; the label is all alpha, its colour black, as the original's.
    const auto texel = [](const DecodedImage& image, int x, int y, int c) {
        return image.rgba[(static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                           static_cast<std::size_t>(x)) * 4u + static_cast<std::size_t>(c)];
    };
    bool black = true;
    for (int frame = 0; frame < 8; ++frame) {
        int inked = 0;
        for (int y = frame * 64; y < frame * 64 + 64; ++y) {
            for (int x = 0; x < 512; ++x) {
                if (texel(sheet, x, y, 3) == 255) ++inked;
                const int colour = texel(sheet, x, y, 0) | texel(sheet, x, y, 1) | texel(sheet, x, y, 2);
                if (texel(sheet, x, y, 3) > 0 && colour != 0) black = false;
            }
        }
        if (frame < 7) CHECK_MSG(inked > 500, "frame " + std::to_string(frame) + " has " + std::to_string(inked));
        else CHECK_MSG(inked == 0, "frame 7 should be empty, has " + std::to_string(inked));
    }
    CHECK(black);

    // Flat where there are no letters (the empty eighth frame), as the
    // original's (127,127,255).
    CHECK_EQ(static_cast<int>(texel(normal, 256, 500, 0)), 127);
    CHECK_EQ(static_cast<int>(texel(normal, 256, 500, 1)), 127);
    CHECK_EQ(static_cast<int>(texel(normal, 256, 500, 2)), 255);
}

Eth::SpriteDraw MenuButton(int id, std::uint32_t frame) {
    Eth::SpriteDraw s;
    s.entityId = id;
    s.sprite = "menu_buttons.png";
    s.normal = "menu_nm_buttons.png";
    s.gloss = "menu_buttons_gloss.png";
    s.type = Eth::ET_HORIZONTAL;
    s.blendMode = Eth::AM_PIXEL;
    s.applyLight = true;
    s.position = glm::vec3(484.0f, 267.0f, 10.0f);
    s.origin = glm::vec2(228.0f, 235.0f);
    s.size = glm::vec2(512.0f, 64.0f);
    s.bitmapSize = glm::vec2(512.0f, 512.0f);
    s.spriteCutX = 1;
    s.spriteCutY = 8;
    s.frame = frame;
    s.depth = 0.5f;
    return s;
}

void TestSpriteRendererSwitch() {
    entt::registry registry;
    TextureCache textures(kApp);
    SpriteRenderer renderer;
    renderer.Attach(registry, textures);
    Localization loc;
    loc.Load();
    loc.SetLanguage(Language::English);
    renderer.SetLocalization(&loc);

    Eth::RenderSnapshot snapshot;
    snapshot.pixelShaders = true;
    snapshot.sprites = {MenuButton(99, 6)};
    Eth::SpriteDraw logo;
    logo.entityId = 108;
    logo.sprite = "gamelogo.png";
    logo.type = Eth::ET_VERTICAL;
    logo.position = glm::vec3(237.0f, 44.0f, 16.0f);
    logo.origin = glm::vec2(-46.0f, -28.0f);
    logo.size = glm::vec2(567.0f, 145.0f);
    logo.bitmapSize = logo.size;
    logo.depth = 0.6f;
    snapshot.sprites.push_back(logo);

    const View view = CameraRig::ComputeView(snapshot, glm::uvec2(1024, 768), false);
    const DrawOrder order = ComputeDrawOrder(snapshot);

    const std::string ptAlbedo = VirtualTextureKey("entities/menu_buttons.png", TextureVariant::Sprite);
    const std::string ptNormal = VirtualTextureKey("entities/normalmaps/menu_nm_buttons.png", TextureVariant::Normal);
    const std::string ptGloss = VirtualTextureKey("entities/menu_buttons_gloss.png", TextureVariant::Plain);
    const std::string ptLogo = VirtualTextureKey("entities/gamelogo.png", TextureVariant::Sprite);

    const auto material = [&](int id) -> const Supersonic::MaterialComponent* {
        const entt::entity quad = renderer.QuadFor(id);
        if (quad == entt::null || !registry.valid(quad)) return nullptr;
        return &registry.get<Supersonic::MaterialComponent>(quad);
    };

    // English: the variants.
    renderer.Draw(registry, snapshot, view, order);
    const Supersonic::MaterialComponent* button = material(99);
    CHECK(button != nullptr);
    if (button != nullptr) {
        CHECK_MSG(Contains(button->albedoTexturePath, "images/en/entities/menu_buttons.png"),
                  button->albedoTexturePath);
        CHECK_MSG(Contains(button->normalTexturePath, "images/en/entities/normalmaps/menu_nm_buttons.png"),
                  button->normalTexturePath);
        // The gloss masks the labels too: its highlight must not show the
        // Portuguese letters under the English ones. Brightness 1 over the
        // scene's lightIntensity 2 (render/Lighting.cpp THE HIGHLIGHT).
        CHECK_MSG(Contains(button->glossTexturePath, "images/en/entities/menu_buttons_gloss.png"),
                  button->glossTexturePath);
        CHECK_NEAR(button->sprite2D.specularStrength, 0.5f);
        CHECK_NEAR(button->sprite2D.specularPower, 50.0f);
        CHECK(!button->sprite2D.vertical);
        // The frame is still the seventh of eight.
        CHECK_NEAR(button->uvScale.y, 0.125f);
        CHECK_NEAR(button->uvOffset.y, 0.75f);
    }
    // The logo is optional (kOptional): the variant when strings.json names one.
    const bool logoWired = !loc.ImageVariant("entities/gamelogo.png", Language::English).empty();
    const Supersonic::MaterialComponent* title = material(108);
    CHECK(title != nullptr);
    if (title != nullptr) {
        CHECK_MSG(Contains(title->albedoTexturePath, "images/en/entities/gamelogo.png") == logoWired,
                  title->albedoTexturePath);
        // Vertical, but no light reaches it (emissive, applyLight 0): flat,
        // no highlight - the record it always had.
        CHECK(!title->sprite2D.vertical);
        CHECK(title->glossTexturePath.empty());
        CHECK_NEAR(title->sprite2D.specularStrength, 0.0f);
    }

    // Switched to Portuguese between two draws of the same snapshot: the
    // originals, on the same quads.
    loc.SetLanguage(Language::Portuguese);
    renderer.Draw(registry, snapshot, view, order);
    button = material(99);
    CHECK(button != nullptr);
    if (button != nullptr) {
        CHECK_MSG(button->albedoTexturePath == ptAlbedo, button->albedoTexturePath);
        CHECK_MSG(button->normalTexturePath == ptNormal, button->normalTexturePath);
        CHECK_MSG(button->glossTexturePath == ptGloss, button->glossTexturePath);
    }
    title = material(108);
    if (title != nullptr) CHECK_MSG(title->albedoTexturePath == ptLogo, title->albedoTexturePath);

    // And back.
    loc.SetLanguage(Language::English);
    renderer.Draw(registry, snapshot, view, order);
    button = material(99);
    if (button != nullptr) {
        CHECK(Contains(button->albedoTexturePath, "images/en/"));
        CHECK(Contains(button->normalTexturePath, "images/en/"));
    }

    // Without a Localization every image is the original's.
    renderer.SetLocalization(nullptr);
    renderer.Draw(registry, snapshot, view, order);
    button = material(99);
    if (button != nullptr) {
        CHECK(button->albedoTexturePath == ptAlbedo);
        CHECK(button->normalTexturePath == ptNormal);
        CHECK(button->glossTexturePath == ptGloss);
    }

    // The lighting half alone: the normal and gloss maps follow the language;
    // a sprite whose map has no variant keeps its own.
    const Eth::SpriteDraw sprite = MenuButton(1, 0);
    CHECK(Contains(ComputeSpriteLighting(sprite, snapshot, textures, &loc).normalKey, "images/en/"));
    CHECK(ComputeSpriteLighting(sprite, snapshot, textures).normalKey == ptNormal);
    CHECK(Contains(ComputeSpriteLighting(sprite, snapshot, textures, &loc).glossKey, "images/en/"));
    CHECK(ComputeSpriteLighting(sprite, snapshot, textures).glossKey == ptGloss);
    Eth::SpriteDraw wizard = sprite;
    wizard.sprite = "bruxo.png";
    wizard.normal = "bruxo_nm.png";
    CHECK(ComputeSpriteLighting(wizard, snapshot, textures, &loc).normalKey ==
          VirtualTextureKey("entities/normalmaps/bruxo_nm.png", TextureVariant::Normal));

    renderer.Detach(registry);
}

void TestHudSwitch() {
    entt::registry registry;
    TextureCache textures(kApp);
    FontAtlas fonts;
    Localization loc;
    loc.Load();
    HudRenderer hud;
    hud.Attach(registry, textures, fonts, loc);

    View view;
    view.logicalScreen = glm::vec2(1024.0f, 768.0f);
    view.windowPixels = glm::uvec2(1024, 768);
    view.scale = 1.0f;
    view.viewportMin = glm::vec2(0.0f);
    view.viewportMax = glm::vec2(1024.0f, 768.0f);

    // A DrawSprite of the image switch.as:98 draws for g_controls (required),
    // and of videoModes.as:81's back arrow (optional, kOptional).
    const auto textureOf = [&](const std::string& image) {
        Eth::RenderSnapshot snapshot;
        Eth::HudCmd sprite;
        sprite.kind = Eth::HudCmd::Kind::Sprite;
        sprite.sprite = image;
        sprite.pos = glm::vec2(40.0f, 600.0f);
        sprite.color = 0xFFCBCBE4u;
        snapshot.hud.push_back(sprite);
        std::vector<Supersonic::ScreenOverlay::Quad> quads;
        hud.Build(snapshot, view, quads);
        for (const auto& quad : quads) {
            if (!quad.texture.empty()) return quad.texture;
        }
        return std::string();
    };
    for (const std::string image : {"interface/input_options1.png", "interface/arrow_button.png"}) {
        const bool wired = !loc.ImageVariant(image, Language::English).empty();
        CHECK_MSG(wired || image == "interface/arrow_button.png", image);
        loc.SetLanguage(Language::English);
        const std::string en = textureOf(image);
        CHECK_MSG(Contains(en, "images/en/" + image) == wired, en);
        loc.SetLanguage(Language::Portuguese);
        const std::string pt = textureOf(image);
        CHECK_MSG(pt == VirtualTextureKey(image, TextureVariant::Sprite), pt);
    }
    hud.Detach();
}

// E24: the language folders that exist beside images/en (the art tool writes
// them): each file is an image strings.json lists, at its original's size and
// not the original; and every language resolves each listed image to its own
// file or else to the English one.
void TestLanguageFolders() {
    Localization loc;
    CHECK(loc.Load());
    const std::filesystem::path data = std::filesystem::path(Localization::DefaultPath()).parent_path();
    Supersonic::Json::Value root;
    std::string error;
    std::string text = ReadText(Localization::DefaultPath());
    if (text.rfind("\xEF\xBB\xBF", 0) == 0) text.erase(0, 3);
    CHECK_MSG(Supersonic::Json::Parse(text, root, error), error);
    std::vector<std::string> listed;
    for (const auto& [path, entry] : root["images"].AsObject()) {
        if (!path.empty() && path[0] != '_') listed.push_back(path);
    }
    int own = 0;
    for (const LanguageInfo& info : kLanguages) {
        if (info.language == Language::Portuguese) continue;
        for (const std::string& relative : listed) {
            const std::string resolved = loc.ImageVariant(relative, info.language);
            const std::string english = loc.ImageVariant(relative, Language::English);
            if (Contains(resolved, std::string("/images/") + info.id + "/")) {
                ++own;
                const glm::ivec2 size = ProbeImageSize(kApp + "/" + relative);
                CHECK_MSG(ProbeImageSize(resolved) == size, resolved);
                const DecodedImage a = DecodeTexture(kApp + "/" + relative, TextureVariant::Plain);
                const DecodedImage b = DecodeTexture(resolved, TextureVariant::Plain);
                CHECK_MSG(a.Valid() && b.Valid() && a.rgba != b.rgba, resolved);
            } else {
                CHECK_MSG(resolved == english, std::string(info.id) + " " + relative + ": " + resolved);
            }
        }
        // Nothing in the folder that no one draws.
        const std::filesystem::path folder = data / "images" / info.id;
        std::error_code ec;
        if (info.language == Language::English || !std::filesystem::is_directory(folder, ec)) continue;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(folder, ec)) {
            if (!entry.is_regular_file()) continue;
            const std::string relative = std::filesystem::relative(entry.path(), folder).generic_string();
            bool known = false;
            for (const std::string& path : listed) known = known || path == relative;
            CHECK_MSG(known, entry.path().generic_string() + " is not an image strings.json lists");
        }
    }
    std::printf("  %d images drawn from a language's own folder (beside English)\n", own);
}

// E24: German, French and back, on a data folder of our own: the text, the
// image each language resolves to, and the two renderers that cache them.
void TestLanguageSwitch() {
    namespace fs = std::filesystem;
    const fs::path data = fs::path(Localization::DefaultPath()).parent_path();
    const fs::path dir = fs::temp_directory_path() / ("penumbra-l10n-" + std::to_string(std::random_device{}()));
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir / "strings", ec);
    fs::create_directories(dir / "images", ec);
    fs::copy_file(data / "strings.json", dir / "strings.json", ec);
    CHECK_MSG(!ec, ec.message());
    fs::copy(data / "images" / "en", dir / "images" / "en", fs::copy_options::recursive, ec);
    CHECK_MSG(!ec, ec.message());
    // The German and French menu sheets: the English one's pixels, under each
    // language's name (what matters here is which file is asked for).
    for (const char* id : {"de", "fr"}) {
        fs::create_directories(dir / "images" / id / "entities", ec);
        fs::copy_file(data / "images" / "en" / "entities" / "menu_buttons.png",
                      dir / "images" / id / "entities" / "menu_buttons.png", ec);
        CHECK_MSG(!ec, ec.message());
    }
    const auto write = [](const fs::path& path, const std::string& text) {
        std::ofstream file(path, std::ios::binary);
        file << text;
    };
    write(dir / "strings" / "de.json", "{\"strings\": {\"Novo jogo\": \"Neues Spiel\"}}");
    write(dir / "strings" / "fr.json", "{\"strings\": {\"Novo jogo\": \"Nouvelle partie\"}}");

    Localization loc;
    CHECK(loc.Load((dir / "strings.json").generic_string()));
    CHECK(loc.HasLanguageFile(Language::German) && loc.HasLanguageFile(Language::French));
    CHECK(!loc.HasLanguageFile(Language::Italian));
    const std::string sheet = "entities/menu_buttons.png";
    for (int round = 0; round < 2; ++round) {
        CHECK(Contains(loc.ImageVariant(sheet, Language::German), "/images/de/entities/menu_buttons.png"));
        CHECK(Contains(loc.ImageVariant(sheet, Language::French), "/images/fr/entities/menu_buttons.png"));
        CHECK(loc.Translate("Novo jogo", Language::German) == "Neues Spiel");
        CHECK(loc.Translate("Novo jogo", Language::French) == "Nouvelle partie");
    }
    // No art of its own: the English; no file: the English text.
    CHECK(loc.ImageVariant(sheet, Language::Italian) == loc.ImageVariant(sheet, Language::English));
    CHECK(Contains(loc.ImageVariant(sheet, Language::English), "/images/en/entities/menu_buttons.png"));
    CHECK(loc.ImageVariant("entities/gamelogo.png", Language::German) ==
          loc.ImageVariant("entities/gamelogo.png", Language::English));
    CHECK(loc.ImageVariant(sheet, Language::Portuguese).empty());
    CHECK(loc.Translate("Novo jogo", Language::Italian) == "New Game");

    // The sprite renderer re-resolves the sheet at each switch.
    entt::registry registry;
    TextureCache textures(kApp);
    SpriteRenderer renderer;
    renderer.Attach(registry, textures);
    renderer.SetLocalization(&loc);
    Eth::RenderSnapshot snapshot;
    snapshot.pixelShaders = true;
    snapshot.sprites = {MenuButton(99, 0)};
    const View view = CameraRig::ComputeView(snapshot, glm::uvec2(1024, 768), false);
    const DrawOrder order = ComputeDrawOrder(snapshot);
    const auto albedo = [&]() {
        renderer.Draw(registry, snapshot, view, order);
        const entt::entity quad = renderer.QuadFor(99);
        return quad != entt::null && registry.valid(quad)
                   ? registry.get<Supersonic::MaterialComponent>(quad).albedoTexturePath
                   : std::string();
    };
    const std::pair<Language, const char*> steps[] = {
        {Language::German, "images/de/"}, {Language::French, "images/fr/"}, {Language::German, "images/de/"},
        {Language::Italian, "images/en/"}};
    for (const auto& [language, folder] : steps) {
        loc.SetLanguage(language);
        const std::string drawn = albedo();
        CHECK_MSG(Contains(drawn, folder), std::string(LanguageId(language)) + ": " + drawn);
    }
    renderer.Detach(registry);
    fs::remove_all(dir, ec);
}

} // namespace

int main() {
    if (!std::filesystem::exists(PENUMBRA_ORIGINAL_DIR "/main.as")) {
        std::printf("SKIP: the original is not at %s\n", PENUMBRA_ORIGINAL_DIR);
        return 77;
    }
    TestVariantsResolve();
    TestMenuSheetLayout();
    TestSpriteRendererSwitch();
    TestHudSwitch();
    TestLanguageFolders();   // E24
    TestLanguageSwitch();
    return test::summary("test_pn_render_english", 90);
}
