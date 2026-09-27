// Penumbra e o Castelo das Sombras - Enhanced, on the Supersonic Engine.
//
// Game flags (taken out before the engine parses the rest):
//   --start <scene>        skip the menu: start scenes/<scene>(.esc), e.g. level1 or pvp_lv2
//   --hold <KEY>@<a>-<b>   hold an Ethanon key from tick a to tick b (headless captures);
//                          KEY is a K_ name without the prefix: RIGHT, UP, S, D, SPACE, CTRL...
//   --lang pt|en           this run's language, over the settings
//   --widescreen on|off    this run's view, over the settings
//   --cursor <x>,<y>       pin the scripts' cursor at a logical-screen point (menu captures)
// --lang and --widescreen are never saved; --window implies a windowed run
// unless --fullscreen is given too.
//
// Engine flags that matter here: --window WxH (the windowed size, over the
// settings), --fullscreen / --windowed (this run only, over the settings; never
// saved), --frames N and --screenshot <absolute path> for headless captures.

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "PenumbraLayer.hpp"

#include "core/GameRuntime.hpp"
#include "core/LaunchOptions.hpp"
#include "core/Log.hpp"
#include "core/SupersonicApp.hpp"
#include "platform/ExecutablePath.hpp"
#include "renderer/VulkanContext.hpp"

namespace {

// The Ethanon key names --hold accepts.
const std::map<std::string, Penumbra::Eth::KEY>& KeyNames() {
    using namespace Penumbra::Eth;
    static const std::map<std::string, KEY> names = {
        {"UP", K_UP}, {"DOWN", K_DOWN}, {"LEFT", K_LEFT}, {"RIGHT", K_RIGHT},
        {"CTRL", K_CTRL}, {"ALT", K_ALT}, {"SHIFT", K_SHIFT}, {"SPACE", K_SPACE},
        {"ENTER", K_ENTER}, {"RETURN", K_ENTER}, {"ESC", K_ESC}, {"BACKSPACE", K_BACK},
        {"PAGEUP", K_PAGEUP}, {"PAGEDOWN", K_PAGEDOWN}, {"J", K_J}, {"S", K_S}, {"D", K_D},
        {"1", K_1}, {"2", K_2}, {"3", K_3}, {"LMOUSE", K_LMOUSE}, {"RMOUSE", K_RMOUSE},
    };
    return names;
}

bool ParseHold(const std::string& text, Penumbra::PenumbraLayer::DevHold& hold) {
    const std::size_t at = text.find('@');
    const std::size_t dash = text.find('-', at == std::string::npos ? 0 : at);
    if (at == std::string::npos) return false;
    const auto key = KeyNames().find(text.substr(0, at));
    if (key == KeyNames().end()) return false;
    try {
        hold.key = key->second;
        if (dash == std::string::npos) {
            hold.from = hold.to = static_cast<unsigned>(std::stoul(text.substr(at + 1)));
        } else {
            hold.from = static_cast<unsigned>(std::stoul(text.substr(at + 1, dash - at - 1)));
            hold.to = static_cast<unsigned>(std::stoul(text.substr(dash + 1)));
        }
    } catch (const std::exception&) {
        return false;
    }
    return hold.to >= hold.from;
}

} // namespace

int main(int argc, char** argv) {
    Penumbra::PenumbraLayer::Options layerOptions;
    std::string languageOverride;
    std::string widescreenOverride;
    std::vector<char*> engineArgs{argv[0]};
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--start" && hasValue) {
            std::string scene = argv[++i];
            if (scene.size() < 4 || scene.substr(scene.size() - 4) != ".esc") scene += ".esc";
            layerOptions.startScene = scene;
        } else if (arg == "--hold" && hasValue) {
            Penumbra::PenumbraLayer::DevHold hold;
            if (!ParseHold(argv[++i], hold)) {
                std::cerr << "[Penumbra] --hold wants KEY@from-to (e.g. RIGHT@60-180), got " << argv[i] << std::endl;
                return EXIT_FAILURE;
            }
            layerOptions.holds.push_back(hold);
        } else if (arg == "--lang" && hasValue) {
            languageOverride = argv[++i];
        } else if (arg == "--widescreen" && hasValue) {
            widescreenOverride = argv[++i];
        } else if (arg == "--cursor" && hasValue) {
            const std::string value = argv[++i];
            const std::size_t comma = value.find(',');
            try {
                if (comma == std::string::npos) throw std::invalid_argument("no comma");
                layerOptions.devCursor = glm::vec2(std::stof(value.substr(0, comma)), std::stof(value.substr(comma + 1)));
            } catch (const std::exception&) {
                std::cerr << "[Penumbra] --cursor wants x,y in the logical screen, got " << value << std::endl;
                return EXIT_FAILURE;
            }
        } else {
            engineArgs.push_back(argv[i]);
        }
    }

    Supersonic::LaunchOptions options;
    try {
        options = Supersonic::LaunchOptions::Parse(static_cast<int>(engineArgs.size()), engineArgs.data());
    } catch (const std::exception& e) {
        std::cerr << "[Penumbra] " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    Supersonic::GameManifest manifest;
    manifest.isGame = true;
    manifest.title = "Penumbra";
    manifest.startupScene.clear();     // the layer builds the world

    // WHERE THIS GAME MAY WRITE, resolved once here: settings, high scores and
    // the checkpoint scene. Empty when the platform will not say; then nothing
    // is kept, and the game says so rather than writing somewhere unexpected.
    layerOptions.userDir = Supersonic::UserDataDirectory(manifest.title);
    if (layerOptions.userDir.empty()) {
        std::cerr << "[Penumbra] no writable user directory; settings and scores will not be kept.\n";
    }

    std::string warning;
    Penumbra::Render::Settings settings = Penumbra::Render::Settings::Load(
        layerOptions.userDir,
        Penumbra::Render::Settings::Defaults(Penumbra::Render::Settings::SystemLanguageIsPortuguese()), &warning);
    if (!warning.empty()) std::cerr << "[Penumbra] settings: " << warning << std::endl;
    // Run-only: the layer shows them and never saves them into settings.json.
    if (languageOverride == "pt" || languageOverride == "en") layerOptions.languageOverride = languageOverride;
    if (widescreenOverride == "on") layerOptions.widescreenOverride = true;
    if (widescreenOverride == "off") layerOptions.widescreenOverride = false;
    layerOptions.settings = settings;

    // --window names a window: without --fullscreen it is a windowed run
    // whatever the settings say, so a player's saved fullscreen never turns a
    // headless capture into a 1920x1200 one.
    if (options.windowWidth > 0 && !options.fullscreen) options.windowed = true;

    manifest.width = static_cast<uint32_t>(settings.windowWidth);
    manifest.height = static_cast<uint32_t>(settings.windowHeight);
    manifest.fullscreen = settings.fullscreen;
    uint32_t width = 0;
    uint32_t height = 0;
    Supersonic::GameRuntime::ResolveWindowSize(manifest, options.windowWidth, options.windowHeight, width, height);
    layerOptions.windowPixels = {width, height};
    // The same answer SupersonicApp reaches (--windowed, --fullscreen, then the
    // settings): the scripts' own idea of the window must start from what the
    // window really is, or the menu's switch undoes a flag on its first frame.
    layerOptions.startFullscreen =
        Supersonic::GameRuntime::ResolveFullscreen(manifest, options.fullscreen, options.windowed);

    try {
        Supersonic::SupersonicApp app(options, &manifest);
        SUPERSONIC_LOG_INFO("Penumbra")
            << "Vulkan validation layers: "
            << (Supersonic::VulkanContext::ValidationLayersActive() ? "ACTIVE"
                                                                    : "NOT LOADED - nothing is checking this run")
            << std::endl;
        app.PushLayer(std::make_unique<Penumbra::PenumbraLayer>(layerOptions));
        app.Run();
    } catch (const std::exception& e) {
        std::cerr << "[Penumbra] fatal: " << e.what() << std::endl;
        Supersonic::Log::CloseFileSink();
        return EXIT_FAILURE;
    }

    const unsigned validationErrors = Supersonic::VulkanContext::ValidationErrorCount();
    Supersonic::Log::CloseFileSink();
    if (validationErrors > 0) {
        std::cerr << "[Penumbra] " << validationErrors << " Vulkan validation error(s); failing the run." << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
