// The Mac's way into the game: main() for a macOS build, doing what a Mac
// needs before PenumbraMain (main.cpp) and nothing else.
//
//  - THE LANGUAGE. An app started from the Finder or the Dock has no LANG in
//    its environment, so the POSIX rule Settings::SystemLanguageIsPortuguese
//    falls back to would see none; the first language the user put in System
//    Settings is handed over instead (E19's hook), before anything reads it.
//  - THE FILES, inside Penumbra.app (tools/apple/make_app.sh). Its
//    Contents/Resources holds original/ (the original game), data/ (the port's
//    own) and engine/assets/shaders (the engine's): the game is pointed at the
//    first two with --original and --data, as a packaged desktop folder would
//    be found beside the executable, and the engine at a working directory it
//    may write in (its pipeline cache, assets/scenes) - a folder under
//    ~/Library/Caches holding a copy of the shaders, which ChooseAssetRoot's
//    working-directory rule then takes. A bundle's own contents are never
//    written: a signed app that changed itself would no longer open.
//    A player's own --original / --data still win, being later on the line,
//    and are made absolute first, against the folder they were typed in.
//
// Outside a bundle - build/game/Penumbra, as the suites and captures run it -
// only the language is added; the build's baked paths find everything, as on
// Linux.

#import <Foundation/Foundation.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include "render/Settings.hpp"

int PenumbraMain(int argc, char** argv);

namespace {

namespace fs = std::filesystem;

std::string PreferredLanguage() {
    NSString* language = NSLocale.preferredLanguages.firstObject;
    return language != nil ? std::string(language.UTF8String) : std::string();
}

// Contents/Resources when this is Penumbra.app holding the game; empty for a
// build-tree run.
fs::path BundleResources() {
    NSBundle* bundle = NSBundle.mainBundle;
    if (![bundle.bundlePath.pathExtension isEqualToString:@"app"] || bundle.resourcePath == nil) return {};
    const fs::path resources(bundle.resourcePath.UTF8String);
    std::error_code ec;
    return fs::is_regular_file(resources / "original" / "data.enml", ec) ? resources : fs::path();
}

// ~/Library/Caches/<bundle id>/engine, with the bundle's shaders copied into
// assets/shaders. Copied every launch: seventeen small files, and a newer
// bundle's shaders must never lose to an older copy. False if it cannot be
// made; the engine then looks where it always does (the build's engine
// checkout, which a bundle run on another Mac does not have).
bool PrepareEngineRoot(const fs::path& resources, fs::path& root) {
    NSArray<NSString*>* caches = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
    if (caches.count == 0) return false;
    NSString* identifier = NSBundle.mainBundle.bundleIdentifier;
    root = fs::path(caches.firstObject.UTF8String) / (identifier != nil ? identifier.UTF8String : "Penumbra") / "engine";

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

} // namespace

int main(int argc, char** argv) {
    @autoreleasepool {
        const std::string language = PreferredLanguage();
        if (!language.empty()) Penumbra::Render::Settings::SetSystemLocale(language);

        std::vector<std::string> args(argv, argv + argc);
        const fs::path resources = BundleResources();
        if (!resources.empty()) {
            std::error_code ec;
            for (std::size_t i = 1; i + 1 < args.size(); ++i) {
                if (args[i] == "--original" || args[i] == "--data") {
                    const fs::path absolute = fs::absolute(args[i + 1], ec);
                    if (!ec) args[i + 1] = absolute.string();
                    ++i;
                }
            }
            const std::vector<std::string> located = {"--original", (resources / "original").string(), "--data",
                                                      (resources / "data").string()};
            args.insert(args.begin() + 1, located.begin(), located.end());

            fs::path engineRoot;
            if (!PrepareEngineRoot(resources, engineRoot)) {
                std::cerr << "[Penumbra] could not copy the engine's shaders to " << engineRoot.string() << std::endl;
            } else if (fs::current_path(engineRoot, ec); ec) {
                std::cerr << "[Penumbra] cannot enter " << engineRoot.string() << ": " << ec.message() << std::endl;
            }
        }

        std::vector<char*> pointers;
        pointers.reserve(args.size() + 1);
        for (std::string& arg : args) pointers.push_back(arg.data());
        pointers.push_back(nullptr);
        return PenumbraMain(static_cast<int>(args.size()), pointers.data());
    }
}
