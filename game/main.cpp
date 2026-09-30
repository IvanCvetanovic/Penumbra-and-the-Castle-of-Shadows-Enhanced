// Penumbra and the Castle of Shadows - Enhanced, on the Supersonic Engine.
//
// Game flags (taken out before the engine parses the rest):
//   --start <scene>        skip the menu: start scenes/<scene>(.esc), e.g. level1 or pvp_lv2
//                          (arena_select, gameover and videoModes start as the scripts start them)
//   --tour <a,b,...>@<N>   after the menu, start each scene in turn for N ticks (one launch, many screens)
//   --hold <KEY>@<a>-<b>   hold an Ethanon key from tick a to tick b (headless captures);
//                          KEY is a K_ name without the prefix: RIGHT, UP, S, D, SPACE, CTRL...
//   --lang <id>            this run's language, over the settings (E24: en de es fr it pt ru tr
//                          uk ja ar)
//   --widescreen on|off    this run's view, over the settings
//   --smooth on|off        this run's motion between ticks (E8), over the settings;
//                          off under --fixed-step unless given as on
//   --cursor <x>,<y>       pin the scripts' cursor at a logical-screen point (menu captures)
//   --pointer <x>,<y>      pin the scripts' cursor at a window pixel, mapped as the real mouse is
//   --spawn <x>,<y>        put the wizard at a scene point once he exists (captures far from a start)
//   --touch [on|off]       this run's on-screen touch controls (E16); on by itself. On the
//                          desktop the held left mouse button is the finger
//   --refresh auto|<Hz>    this run's fullscreen refresh rate (E23), over the settings
//   --modes <WxH@R,...>    the display modes the options screen lists, instead of the monitor's
//                          (captures); a '*' after one makes it the desktop's
//   --original <dir>       the original game's files (the folder holding data.enml)
//   --data <dir>           the port's own data (the folder holding strings.json)
// --lang and --widescreen are never saved; --window implies a windowed run
// unless --fullscreen is given too. Without --original/--data, a packaged
// game's original/ and data/ beside the executable win over the build's paths
// (eth/Paths.hpp).
//
// Engine flags that matter here: --window WxH (the windowed size, over the
// settings, and over E23's automatic window), --fullscreen / --windowed (this
// run only, over the settings; never saved; --fullscreen runs at the saved
// fullscreen mode, or E23's automatic one, as Alt+Enter would), --frames N and
// --screenshot <absolute path> for headless captures.

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
#include "eth/StartupErrors.hpp"
#include "render/Languages.hpp"
#include "render/WindowMode.hpp"

#include "core/GameRuntime.hpp"
#include "core/JobSystem.hpp"
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
    "  --lang <id>            this run's language (not saved): en de es fr it pt ru tr uk ja ar\n"
    "  --widescreen on|off    this run's view (not saved)\n"
    "  --smooth on|off        this run's motion between ticks (not saved; off under --fixed-step)\n"
    "  --original <dir>       the original game's files: the folder holding data.enml\n"
    "  --data <dir>           the port's data: the folder holding strings.json\n"
    "  --hold <KEY>@<a>-<b>   hold a key from tick a to tick b (RIGHT, UP, CTRL, S, D, SPACE, ENTER...)\n"
    "  --cursor <x>,<y>       pin the menu cursor at a point of the 1024x768 screen\n"
    "  --pointer <x>,<y>      pin the menu cursor at a pixel of the window (mapped as the real mouse)\n"
    "  --spawn <x>,<y>        put the wizard at a point of the scene (its pixels) once he appears\n"
    "  --touch [on|off]       this run's on-screen touch controls (not saved; on by itself);\n"
    "                         on a desktop the held left mouse button is the finger\n"
    "  --refresh auto|<Hz>    this run's fullscreen refresh rate (not saved)\n"
    "  --modes <WxH@R,...>    the display modes the options screen lists (captures; not saved);\n"
    "                         a '*' after one makes it the desktop's mode, else the largest is\n";

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

// --modes: "1920x1200@60*,1920x1200@165,1280x800@60" - each size and rate, the
// desktop's marked with '*' (else the largest), listed and sorted as the
// engine lists a monitor's (WindowControl::SelectDisplayModes).
bool ParseModes(const std::string& text, std::vector<Supersonic::DisplayMode>& modes,
                Supersonic::DisplayMode& desktop) {
    std::vector<Supersonic::WindowControl::VideoMode> reported;
    bool haveDesktop = false;
    for (std::size_t start = 0; start <= text.size();) {
        const std::size_t comma = text.find(',', start);
        std::string item = text.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        const bool isDesktop = !item.empty() && item.back() == '*';
        if (isDesktop) item.pop_back();
        const std::size_t x = item.find('x');
        const std::size_t at = item.find('@');
        if (x == std::string::npos || at == std::string::npos || at < x) return false;
        try {
            const int width = std::stoi(item.substr(0, x));
            const int height = std::stoi(item.substr(x + 1, at - x - 1));
            const int rate = std::stoi(item.substr(at + 1));
            if (width <= 0 || height <= 0 || rate < 0) return false;
            reported.push_back({width, height, 8, 8, 8, rate});
            if (isDesktop) {
                desktop = Supersonic::DisplayMode{static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                                                  static_cast<uint32_t>(rate)};
                haveDesktop = true;
            }
        } catch (const std::exception&) {
            return false;
        }
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    modes = Supersonic::WindowControl::SelectDisplayModes(reported);
    if (modes.empty()) return false;
    if (!haveDesktop) desktop = modes.back();
    return true;
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

enum class RootReport { Found, Unspellable, Missing };

// Logs where a root was found, or says on stderr why it was not.
RootReport ReportRoot(const char* what, const char* flag, const char* folder, const char* marker,
                      const Penumbra::Eth::FoundRoot& root) {
    // The Eth layer opens files by narrow (code page) strings, as the original
    // did. A folder the code page cannot spell would come back as another
    // name, or '?', and every file under it would quietly fail to open.
    if (root.found && std::filesystem::path(root.path.string()) != root.path) {
        std::cerr << "[Penumbra] the " << what << " folder's name has characters this system's code page"
                  << " cannot spell; move it to a plain path or name one with " << flag << " <dir>." << std::endl;
        return RootReport::Unspellable;
    }
    if (root.found) {
        SUPERSONIC_LOG_INFO("Penumbra") << what << ": " << root.path.string() << " ("
                                        << Penumbra::Eth::DescribeRootSource(root.source) << ")";
        return RootReport::Found;
    }
    if (root.source == Penumbra::Eth::RootSource::Flag) {
        std::cerr << "[Penumbra] " << flag << " " << root.path.string() << ": there is no " << marker << " there"
                  << std::endl;
    } else {
        std::cerr << "[Penumbra] " << what << " not found: no " << marker << " in " << folder
                  << "/ beside the executable, nor at " << root.path.string() << ". Name its folder with " << flag
                  << " <dir>." << std::endl;
    }
    return RootReport::Missing;
}

// A path for a message box, which takes UTF-8 whatever the code page spells.
std::string Utf8(const std::filesystem::path& path) {
    try {
        const std::u8string text = path.u8string();
        return std::string(text.begin(), text.end());
    } catch (const std::exception&) {
        return {};
    }
}

} // namespace

// The whole of the game's start, callable from every entry: main() below on
// Windows and Linux, SupersonicMain on Android (android/AndroidMain.cpp) and
// iOS (ios/IOSMain.mm), which add the paths to the game's files and any
// development flags to argv first, and main() in macos/MacMain.mm on a Mac.
int PenumbraMain(int argc, char** argv);

int PenumbraMain(int argc, char** argv) {
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
        } else if (arg == "--touch") {
            // E16. The value is optional: only "on" or "off" is taken as one,
            // so `--touch --start level1` still starts level 1.
            layerOptions.touchOverride = true;
            if (hasValue && (std::string(argv[i + 1]) == "on" || std::string(argv[i + 1]) == "off")) {
                layerOptions.touchOverride = std::string(argv[++i]) == "on";
            }
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
        } else if (arg == "--pointer" && hasValue) {
            const std::string value = argv[++i];
            const std::size_t comma = value.find(',');
            try {
                if (comma == std::string::npos) throw std::invalid_argument("no comma");
                layerOptions.devPointer = glm::vec2(std::stof(value.substr(0, comma)), std::stof(value.substr(comma + 1)));
            } catch (const std::exception&) {
                std::cerr << "[Penumbra] --pointer wants x,y in the window's pixels, got " << value << std::endl;
                return EXIT_FAILURE;
            }
        } else if (arg == "--spawn" && hasValue) {
            const std::string value = argv[++i];
            const std::size_t comma = value.find(',');
            try {
                if (comma == std::string::npos) throw std::invalid_argument("no comma");
                layerOptions.devSpawn = glm::vec2(std::stof(value.substr(0, comma)), std::stof(value.substr(comma + 1)));
            } catch (const std::exception&) {
                std::cerr << "[Penumbra] --spawn wants x,y in the scene's pixels, got " << value << std::endl;
                return EXIT_FAILURE;
            }
        } else if (arg == "--refresh" && hasValue) {
            // E23: "auto" or a whole number of Hz, as window.fullscreenRefresh.
            const std::string value = argv[++i];
            try {
                const unsigned long rate = value == "auto" ? 0ul : std::stoul(value);
                if (rate > 1000ul || (value != "auto" && value.find_first_not_of("0123456789") != std::string::npos)) {
                    throw std::invalid_argument("not a rate");
                }
                layerOptions.refreshOverride = static_cast<uint32_t>(rate);
            } catch (const std::exception&) {
                std::cerr << "[Penumbra] --refresh wants auto or a rate in Hz (e.g. 144), got " << value << std::endl;
                return EXIT_FAILURE;
            }
        } else if (arg == "--modes" && hasValue) {
            const std::string value = argv[++i];
            if (!ParseModes(value, layerOptions.devModes, layerOptions.devDesktop)) {
                std::cerr << "[Penumbra] --modes wants WxH@R,... with an optional '*' for the desktop's "
                             "(e.g. 1920x1200@60*,1920x1200@165), got " << value << std::endl;
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

    // A failure a player must be TOLD about (eth/StartupErrors.hpp): a message
    // box when the game was started from Explorer, where stderr goes nowhere;
    // never for a capture, a shell or a script, which read the stderr line as
    // before.
    const bool headless = options.maxFrames > 0 || !options.screenshotPath.empty() || options.fixedDelta > 0.0f;
    const bool showDialogs = Penumbra::Eth::ShouldShowStartupDialog(headless, Penumbra::Eth::CurrentErrorStream());
    std::filesystem::path logFile;   // once the log has one
    const auto tellPlayer = [&](Penumbra::Eth::StartupProblem problem, const std::string& detail) {
        if (!showDialogs) return;
        Penumbra::Eth::ShowStartupDialog(Penumbra::Eth::StartupMessage(
            problem, Penumbra::Render::Settings::SystemLanguageIsPortuguese(), detail, Utf8(logFile)));
    };

    // WHERE THE GAME'S FILES ARE, before SupersonicApp moves the working
    // directory (a relative --original is the player's, from where they
    // launched). The original is required; without the port's data the game
    // still runs, in Portuguese with the original's images.
    try {
        const std::filesystem::path exeDir = Supersonic::ExecutableDirectory();
        const Penumbra::Eth::FoundRoot original =
            Penumbra::Eth::FindOriginalRoot(originalFlag, exeDir, PENUMBRA_ORIGINAL_DIR);
        const RootReport originalReport = ReportRoot("original game", "--original",
                                                     Penumbra::Eth::kPackagedOriginalFolder,
                                                     Penumbra::Eth::kOriginalMarker, original);
        if (originalReport != RootReport::Found) {
            // Not for a flag, which a developer typed: a player's copy without
            // original/ beside it is one run from inside the zip (Explorer
            // extracts the clicked exe alone) or never extracted whole.
            if (original.source != Penumbra::Eth::RootSource::Flag) {
                tellPlayer(originalReport == RootReport::Unspellable ? Penumbra::Eth::StartupProblem::FolderNameUnusable
                                                                     : Penumbra::Eth::StartupProblem::GameFilesMissing,
                           "Penumbra.exe is in " + Utf8(exeDir));
            }
            return EXIT_FAILURE;
        }
        const Penumbra::Eth::FoundRoot data = Penumbra::Eth::FindDataRoot(dataFlag, exeDir, PENUMBRA_DATA_DIR);
        layerOptions.originalDir = original.path;
        if (ReportRoot("port data", "--data", Penumbra::Eth::kPackagedDataFolder, Penumbra::Eth::kDataMarker, data) ==
            RootReport::Found) {
            layerOptions.dataDir = data.path;
        } else {
            if (data.source == Penumbra::Eth::RootSource::Flag) return EXIT_FAILURE;
            std::cerr << "[Penumbra] continuing without it: no English, the original's images only." << std::endl;
        }
    } catch (const std::exception& e) {
        // path::string() throws on some names the code page cannot spell,
        // where others come back altered (ReportRoot).
        std::cerr << "[Penumbra] cannot use the game's folders: " << e.what() << std::endl;
        tellPlayer(Penumbra::Eth::StartupProblem::FolderNameUnusable, e.what());
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
        logFile = layerOptions.userDir / "penumbra.log";
    }

    std::string warning;
    Penumbra::Render::Settings settings = Penumbra::Render::Settings::Load(
        layerOptions.userDir,
        Penumbra::Render::Settings::Defaults(Penumbra::Render::Settings::SystemLanguage()), &warning);
    if (!warning.empty()) std::cerr << "[Penumbra] settings: " << warning << std::endl;
    // Run-only: the layer shows them and never saves them into settings.json.
    if (auto language = Penumbra::Render::Language::English;
        Penumbra::Render::LanguageFromId(languageOverride, language)) {
        layerOptions.languageOverride = Penumbra::Render::LanguageId(language);
    } else if (!languageOverride.empty()) {
        std::cerr << "[Penumbra] --lang: \"" << languageOverride << "\" is not a language the game speaks; ignored"
                  << std::endl;
    }
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

    // E23: an automatic windowed size (0 x 0) states none, and asks the engine
    // to fit the window to the monitor before the first swapchain - so it does
    // not open at the engine's default and jump. --window and a fullscreen
    // start win over it (SupersonicApp).
    const bool automaticWindow = settings.windowWidth <= 0 || settings.windowHeight <= 0;
    manifest.width = automaticWindow ? 0u : static_cast<uint32_t>(settings.windowWidth);
    manifest.height = automaticWindow ? 0u : static_cast<uint32_t>(settings.windowHeight);
    if (automaticWindow) manifest.fitWindowToMonitor = Penumbra::Render::kAutoWindowFraction;
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

    // Penumbra.exe delay-loads vulkan-1.dll (game/CMakeLists.txt), so a PC with
    // no Vulkan driver at all gets here, not to a Windows error naming a DLL.
    // Nothing has called Vulkan yet; nothing will without the loader.
    if (!Penumbra::Eth::VulkanLoaderAvailable()) {
        std::cerr << "[Penumbra] fatal: no Vulkan loader (vulkan-1.dll); a graphics driver with Vulkan 1.2 is needed"
                  << std::endl;
        tellPlayer(Penumbra::Eth::StartupProblem::NoGraphics, "vulkan-1.dll could not be loaded");
        Supersonic::Log::CloseFileSink();
        return EXIT_FAILURE;
    }
    // Whether the engine will find its shaders (the rule SupersonicApp anchors
    // by): if it will not, a failed start below is missing files, not graphics.
    std::error_code launchDirError;
    const std::filesystem::path launchDir = std::filesystem::current_path(launchDirError);
    const bool engineFilesFound =
        Supersonic::ChooseAssetRoot(Supersonic::ExecutableDirectory(), launchDir, Supersonic::ConfiguredAssetRoot(),
                                    Supersonic::HoldsEngineAssets)
            .source != Supersonic::AssetRootSource::Unresolved;

    bool started = false;   // the window and the Vulkan device exist
    try {
        Supersonic::SupersonicApp app(options, &manifest);
        started = true;
        SUPERSONIC_LOG_INFO("Penumbra")
            << "Vulkan validation layers: "
            << (Supersonic::VulkanContext::ValidationLayersActive() ? "ACTIVE"
                                                                    : "NOT LOADED - nothing is checking this run")
            << std::endl;
        app.PushLayer(std::make_unique<Penumbra::PenumbraLayer>(layerOptions));
        app.Run();
    } catch (const std::exception& e) {
        // A throw from SupersonicApp's constructor skips its destructor, the
        // one place the job workers are joined: on glibc, exit() then blocks
        // for ever destroying the pool's condition variable while they wait on
        // it, and the player's failed start never ends. Shutdown is idempotent.
        Supersonic::JobSystem::Shutdown();
        std::cerr << "[Penumbra] fatal: " << e.what() << std::endl;
        tellPlayer(started            ? Penumbra::Eth::StartupProblem::StoppedByError
                   : engineFilesFound ? Penumbra::Eth::StartupProblem::NoGraphics
                                      : Penumbra::Eth::StartupProblem::GameFilesMissing,
                   e.what());
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

#if !defined(__ANDROID__) && !defined(__APPLE__)
int main(int argc, char** argv) { return PenumbraMain(argc, argv); }
#endif
