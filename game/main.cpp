// Penumbra e o Castelo das Sombras - Enhanced, on the Supersonic Engine.
//
// Game flags (taken out before the engine parses the rest):
//   --start <scene>        skip the menu: start scenes/<scene>(.esc), e.g. level1 or pvp_lv2
//                          (arena_select, gameover and videoModes start as the scripts start them)
//   --tour <a,b,...>@<N>   after the menu, start each scene in turn for N ticks (one launch, many screens)
//   --hold <KEY>@<a>-<b>   hold an Ethanon key from tick a to tick b (headless captures);
//                          KEY is a K_ name without the prefix: RIGHT, UP, S, D, SPACE, CTRL...
//   --lang pt|en           this run's language, over the settings
//   --widescreen on|off    this run's view, over the settings
//   --smooth on|off        this run's motion between ticks (E8), over the settings;
//                          off under --fixed-step unless given as on
//   --cursor <x>,<y>       pin the scripts' cursor at a logical-screen point (menu captures)
//   --original <dir>       the original game's files (the folder holding data.enml)
//   --data <dir>           the port's own data (the folder holding strings.json)
// --lang and --widescreen are never saved; --window implies a windowed run
// unless --fullscreen is given too. Without --original/--data, a packaged
// game's original/ and data/ beside the executable win over the build's paths
// (eth/Paths.hpp).
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
#include "eth/Paths.hpp"

#include "core/GameRuntime.hpp"
#include "core/LaunchOptions.hpp"
#include "core/Log.hpp"
#include "core/SupersonicApp.hpp"
#include "platform/ExecutablePath.hpp"
#include "renderer/VulkanContext.hpp"

namespace {

constexpr const char* kGameUsage =
    "Usage: Penumbra [options]\n"
    "  --start <scene>        skip the menu and start scenes/<scene>.esc (level1..level3, pvp_lv1..pvp_lv6;\n"
    "                         arena_select, gameover and videoModes start as the scripts start them)\n"
    "  --tour <a,b,...>@<N>   after the menu, start each scene in turn for N ticks (many screens, one launch)\n"
    "  --lang pt|en           this run's language (not saved)\n"
    "  --widescreen on|off    this run's view (not saved)\n"
    "  --smooth on|off        this run's motion between ticks (not saved; off under --fixed-step)\n"
    "  --original <dir>       the original game's files: the folder holding data.enml\n"
    "  --data <dir>           the port's data: the folder holding strings.json\n"
    "  --hold <KEY>@<a>-<b>   hold a key from tick a to tick b (RIGHT, UP, CTRL, S, D, SPACE, ENTER...)\n"
    "  --cursor <x>,<y>       pin the menu cursor at a point of the 1024x768 screen\n";

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

// Logs where a root was found, or says on stderr why it was not. False when
// it was not.
bool ReportRoot(const char* what, const char* flag, const char* folder, const char* marker,
                const Penumbra::Eth::FoundRoot& root) {
    // The Eth layer opens files by narrow (code page) strings, as the original
    // did. A folder the code page cannot spell would come back as another
    // name, or '?', and every file under it would quietly fail to open.
    if (root.found && std::filesystem::path(root.path.string()) != root.path) {
        std::cerr << "[Penumbra] the " << what << " folder's name has characters this system's code page"
                  << " cannot spell; move it to a plain path or name one with " << flag << " <dir>." << std::endl;
        return false;
    }
    if (root.found) {
        SUPERSONIC_LOG_INFO("Penumbra") << what << ": " << root.path.string() << " ("
                                        << Penumbra::Eth::DescribeRootSource(root.source) << ")";
        return true;
    }
    if (root.source == Penumbra::Eth::RootSource::Flag) {
        std::cerr << "[Penumbra] " << flag << " " << root.path.string() << ": there is no " << marker << " there"
                  << std::endl;
    } else {
        std::cerr << "[Penumbra] " << what << " not found: no " << marker << " in " << folder
                  << "/ beside the executable, nor at " << root.path.string() << ". Name its folder with " << flag
                  << " <dir>." << std::endl;
    }
    return false;
}

} // namespace

int main(int argc, char** argv) {
    Penumbra::PenumbraLayer::Options layerOptions;
    std::string languageOverride;
    std::string widescreenOverride;
    std::string smoothOverride;
    std::filesystem::path originalFlag;
    std::filesystem::path dataFlag;
    std::vector<char*> engineArgs{argv[0]};
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--start" && hasValue) {
            std::string scene = argv[++i];
            if (scene.size() < 4 || scene.substr(scene.size() - 4) != ".esc") scene += ".esc";
            layerOptions.startScene = scene;
        } else if (arg == "--tour" && hasValue) {
            const std::string value = argv[++i];
            const std::size_t at = value.rfind('@');
            try {
                if (at == std::string::npos) throw std::invalid_argument("no @");
                layerOptions.tourTicks = static_cast<unsigned>(std::stoul(value.substr(at + 1)));
                std::string list = value.substr(0, at);
                for (std::size_t start = 0; start <= list.size();) {
                    const std::size_t comma = list.find(',', start);
                    std::string scene = list.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
                    if (!scene.empty()) {
                        if (scene.size() < 4 || scene.substr(scene.size() - 4) != ".esc") scene += ".esc";
                        layerOptions.tour.push_back(scene);
                    }
                    if (comma == std::string::npos) break;
                    start = comma + 1;
                }
                if (layerOptions.tour.empty() || layerOptions.tourTicks == 0) throw std::invalid_argument("empty");
            } catch (const std::exception&) {
                std::cerr << "[Penumbra] --tour wants scene,scene,...@ticks (e.g. pvp_lv1,gameover@300), got " << value << std::endl;
                return EXIT_FAILURE;
            }
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
        } else if (arg == "--smooth" && hasValue) {
            smoothOverride = argv[++i];
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
        } else if (arg == "--original" && hasValue) {
            originalFlag = argv[++i];
        } else if (arg == "--data" && hasValue) {
            dataFlag = argv[++i];
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
    // Parse carries its failures rather than throwing them. Ignored, a typo
    // started the game anyway and dropped every flag after it.
    if (options.helpRequested) {
        std::cout << kGameUsage << "\nEngine options:\n" << Supersonic::LaunchOptions::Usage();
        return EXIT_SUCCESS;
    }
    if (!options.ok) {
        std::cerr << "[Penumbra] " << options.error << "\n\n" << kGameUsage << std::endl;
        return EXIT_FAILURE;
    }

    // WHERE THE GAME'S FILES ARE, before SupersonicApp moves the working
    // directory (a relative --original is the player's, from where they
    // launched). The original is required; without the port's data the game
    // still runs, in Portuguese with the original's images.
    try {
        const std::filesystem::path exeDir = Supersonic::ExecutableDirectory();
        const Penumbra::Eth::FoundRoot original =
            Penumbra::Eth::FindOriginalRoot(originalFlag, exeDir, PENUMBRA_ORIGINAL_DIR);
        if (!ReportRoot("original game", "--original", Penumbra::Eth::kPackagedOriginalFolder,
                        Penumbra::Eth::kOriginalMarker, original)) {
            return EXIT_FAILURE;
        }
        const Penumbra::Eth::FoundRoot data = Penumbra::Eth::FindDataRoot(dataFlag, exeDir, PENUMBRA_DATA_DIR);
        layerOptions.originalDir = original.path;
        if (ReportRoot("port data", "--data", Penumbra::Eth::kPackagedDataFolder, Penumbra::Eth::kDataMarker, data)) {
            layerOptions.dataDir = data.path;
        } else {
            if (data.source == Penumbra::Eth::RootSource::Flag) return EXIT_FAILURE;
            std::cerr << "[Penumbra] continuing without it: no English, the original's images only." << std::endl;
        }
    } catch (const std::exception& e) {
        // path::string() throws on some names the code page cannot spell,
        // where others come back altered (ReportRoot).
        std::cerr << "[Penumbra] cannot use the game's folders: " << e.what() << std::endl;
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
    } else {
        // Penumbra.exe has no console (a game started from Explorer should not
        // open one), so the log also goes to a file a player can send along.
        // Overwritten each run: the last run is the one worth reading.
        std::error_code ignored;
        std::filesystem::create_directories(layerOptions.userDir, ignored);
        Supersonic::Log::SetFileSink((layerOptions.userDir / "penumbra.log").string());
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
    // E8 blends by SimulationClock::alpha, which --fixed-step at the tick pins
    // at 0: every frame would draw the tick before, and every capture would
    // be one tick late. So a fixed-step run draws the ticks themselves, as
    // every capture so far was taken, unless --smooth on asks for the blend
    // (a capture of E8 itself: --fixed-step 0.0083333 --smooth on
    // --screenshot-every 1, about half a tick a frame).
    if (options.fixedDelta > 0.0f) layerOptions.smoothMotionOverride = false;
    // E13: nor does it pause itself when its window has no focus - a capture's
    // window often never has it.
    if (options.fixedDelta > 0.0f) layerOptions.pauseOnFocusLossOverride = false;
    if (smoothOverride == "on") layerOptions.smoothMotionOverride = true;
    if (smoothOverride == "off") layerOptions.smoothMotionOverride = false;
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
