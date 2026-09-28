// iOS's way into the game: the engine's main() (engine/src/platform/ios/
// IOSApp.mm) starts UIKit and calls SupersonicMain once the app's scene shows a
// view to draw on, and this does what a desktop's folder layout and command
// line did before handing on to PenumbraMain (main.cpp), as Android's entry
// does (android/AndroidMain.cpp):
//
//  - THE FILES. Penumbra.app carries original/ (the original game), data/ (the
//    port's own) and engine/assets/shaders (tools/apple/make_app.sh). An app
//    bundle has real paths, unlike an APK's assets, so nothing is unpacked:
//    the game is pointed at the first two with --original and --data. The
//    bundle is read-only, and the engine writes where it runs (its pipeline
//    cache, assets/scenes), so its working directory is Library/Caches/engine
//    with a copy of the shaders in assets/shaders - ChooseAssetRoot's
//    working-directory rule. Saves and settings go to Library/Application
//    Support (the engine's UserDataDirectory), as on a Mac.
//  - THE LANGUAGE (E19): the first of the device's preferred languages, since
//    an app's environment carries no LANG.
//  - DEVELOPMENT FLAGS. A phone has no command line, so a run's flags come from
//    penumbra_args.txt in the app's Documents folder: whitespace-separated,
//    double quotes around an argument with spaces. Read once and renamed to
//    penumbra_args.used, so one scripted run does not become every later
//    launch. On a simulator the file is written from the Mac into the folder
//    `xcrun simctl get_app_container <device> <bundle id> data` names
//    (.github/workflows/apple.yml does), and a --screenshot path in that same
//    folder is read back from there.

#import <Foundation/Foundation.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "core/Log.hpp"
#include "platform/ios/IOSApp.hpp"
#include "render/Settings.hpp"

int PenumbraMain(int argc, char** argv);

namespace {

namespace fs = std::filesystem;

constexpr const char* kArgsFile = "penumbra_args.txt";
constexpr const char* kArgsUsed = "penumbra_args.used";

fs::path UserFolder(NSSearchPathDirectory which) {
    NSArray<NSString*>* folders = NSSearchPathForDirectoriesInDomains(which, NSUserDomainMask, YES);
    return folders.count > 0 ? fs::path(folders.firstObject.UTF8String) : fs::path();
}

std::string ReadFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

// The engine's working directory: somewhere it may write, holding the
// bundle's shaders. Copied every launch - seventeen small files - so an
// updated app never runs an older copy.
bool PrepareEngineRoot(const fs::path& resources, const fs::path& root) {
    const fs::path from = resources / "engine" / "assets" / "shaders";
    const fs::path to = root / "assets" / "shaders";
    std::error_code ec;
    fs::create_directories(to, ec);
    if (ec) return false;
    for (fs::directory_iterator it(from, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        fs::copy_file(it->path(), to / it->path().filename(), fs::copy_options::overwrite_existing, ec);
        if (ec) return false;
    }
    return !ec;
}

// The development flags, if a file of them was left for this launch.
std::vector<std::string> TakeDevelopmentFlags(const fs::path& folder) {
    std::vector<std::string> flags;
    if (folder.empty()) return flags;
    const fs::path file = folder / kArgsFile;
    std::error_code ec;
    if (!fs::is_regular_file(file, ec)) return flags;

    const std::string text = ReadFile(file);
    std::string current;
    bool quoted = false;
    bool any = false;
    for (const char c : text) {
        if (c == '"') {
            quoted = !quoted;
            any = true;
        } else if (!quoted && (c == ' ' || c == '\t' || c == '\n' || c == '\r')) {
            if (any) flags.push_back(current);
            current.clear();
            any = false;
        } else {
            current += c;
            any = true;
        }
    }
    if (any) flags.push_back(current);

    fs::rename(file, folder / kArgsUsed, ec);
    std::string joined;
    for (const std::string& flag : flags) joined += " " + flag;
    SUPERSONIC_LOG_INFO("Penumbra") << "Flags from " << file.string() << ":" << joined;
    return flags;
}

} // namespace

int SupersonicMain(int argc, char** argv) {
    // No @autoreleasepool around the game: the engine drains one of its own
    // each frame (IOSApp.mm), and a pool here, popped when the game returns,
    // would take that one with it before the engine pops it again.
    NSString* resourcePath = NSBundle.mainBundle.resourcePath;
    if (resourcePath == nil) {
        SUPERSONIC_LOG_ERROR("Penumbra") << "The app bundle has no resource folder; stopping.";
        return EXIT_FAILURE;
    }
    const fs::path resources(resourcePath.UTF8String);

    const fs::path engineRoot = UserFolder(NSCachesDirectory) / "engine";
    if (engineRoot.has_parent_path() && PrepareEngineRoot(resources, engineRoot)) {
        std::error_code ec;
        fs::current_path(engineRoot, ec);
        if (ec) SUPERSONIC_LOG_ERROR("Penumbra") << "Cannot enter " << engineRoot.string() << ": " << ec.message();
    } else {
        SUPERSONIC_LOG_ERROR("Penumbra") << "Could not copy the engine's shaders to " << engineRoot.string() << ".";
    }

    NSString* language = NSLocale.preferredLanguages.firstObject;
    if (language != nil) Penumbra::Render::Settings::SetSystemLocale(language.UTF8String);

    std::vector<std::string> args;
    args.emplace_back(argc > 0 ? argv[0] : "Penumbra");
    args.emplace_back("--original");
    args.emplace_back((resources / "original").string());
    args.emplace_back("--data");
    args.emplace_back((resources / "data").string());
    for (std::string& flag : TakeDevelopmentFlags(UserFolder(NSDocumentDirectory))) args.push_back(std::move(flag));

    std::vector<char*> pointers;
    pointers.reserve(args.size() + 1);
    for (std::string& arg : args) pointers.push_back(arg.data());
    pointers.push_back(nullptr);
    return PenumbraMain(static_cast<int>(args.size()), pointers.data());
}
