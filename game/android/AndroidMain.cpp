// Android's way into the game: the engine's android_main (engine/src/platform/
// android/AndroidApp.cpp) calls SupersonicMain once the activity has a window,
// and this does what a desktop's folder layout and command line did before
// handing on to PenumbraMain (main.cpp):
//
//  - THE FILES. The APK carries the original game, the port's data and the
//    engine's shaders as assets (tools/build_android.sh), but the game reads
//    files by path, and an APK's assets have none. So they are unpacked once
//    into the app's private storage - again only when the APK's list says they
//    changed - and the game is pointed at them with --original and --data,
//    exactly as a packaged desktop game can be. The working directory is the
//    unpacked engine root, which is where the engine looks for assets/shaders.
//  - DEVELOPMENT FLAGS. A phone has no command line, so a run's flags come from
//    penumbra_args.txt in the app's private files directory, or failing that
//    its external one: whitespace-separated, double quotes around an argument
//    with spaces. Read once and renamed to penumbra_args.used, so one scripted
//    run does not become every later launch. adb writes the private one
//    through run-as (the APK is debuggable); from Android 11 the shell may not
//    create files in another app's external directory at all.
//    tools/build_android.sh --run writes it. A --screenshot path should be in
//    the private directory too (/data/user/0/<package>/files/...), read back
//    with `adb exec-out run-as <package> cat files/<name>.png`.

#include <android/asset_manager.h>
#include <android/configuration.h>
#include <android/native_activity.h>
#include <android_native_app_glue.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "core/Log.hpp"
#include "platform/android/AndroidApp.hpp"
#include "render/Settings.hpp"

int PenumbraMain(int argc, char** argv);

namespace {

namespace fs = std::filesystem;

// Written by tools/build_android.sh into the APK's assets: the first line is a
// stamp that changes whenever any packed file does, every other line a path
// under assets/.
constexpr const char* kAssetList = "penumbra_assets.txt";
constexpr const char* kStampFile = "penumbra_assets.stamp";
constexpr const char* kArgsFile = "penumbra_args.txt";
constexpr const char* kArgsUsed = "penumbra_args.used";

// The folders unpacking owns. The user's own folder (settings, scores, the
// log) sits beside them and is never touched.
constexpr const char* kUnpackedRoots[] = {"original", "data", "engine"};

bool readAsset(AAssetManager* manager, const std::string& path, std::string& out) {
    AAsset* asset = AAssetManager_open(manager, path.c_str(), AASSET_MODE_STREAMING);
    if (asset == nullptr) return false;
    out.clear();
    char buffer[64 * 1024];
    int n = 0;
    while ((n = AAsset_read(asset, buffer, sizeof buffer)) > 0) out.append(buffer, static_cast<std::size_t>(n));
    AAsset_close(asset);
    return n == 0;
}

bool copyAsset(AAssetManager* manager, const std::string& path, const fs::path& target) {
    AAsset* asset = AAssetManager_open(manager, path.c_str(), AASSET_MODE_STREAMING);
    if (asset == nullptr) return false;
    std::error_code ec;
    fs::create_directories(target.parent_path(), ec);
    FILE* file = std::fopen(target.c_str(), "wb");
    bool ok = file != nullptr;
    char buffer[64 * 1024];
    int n = 0;
    while (ok && (n = AAsset_read(asset, buffer, sizeof buffer)) > 0) {
        ok = std::fwrite(buffer, 1, static_cast<std::size_t>(n), file) == static_cast<std::size_t>(n);
    }
    if (n < 0) ok = false;
    if (file != nullptr) ok = std::fclose(file) == 0 && ok;
    AAsset_close(asset);
    return ok;
}

std::string readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

// Unpacks the APK's files into `files` unless the last unpack was of this very
// list. False when the list is missing or a file would not unpack.
bool unpackAssets(const fs::path& files) {
    android_app* app = Supersonic::Android::App();
    AAssetManager* manager = app->activity->assetManager;

    std::string list;
    if (!readAsset(manager, kAssetList, list)) {
        SUPERSONIC_LOG_ERROR("Penumbra") << "The APK has no " << kAssetList << "; nothing to unpack.";
        return false;
    }
    std::istringstream lines(list);
    std::string stamp;
    std::getline(lines, stamp);
    std::vector<std::string> paths;
    for (std::string line; std::getline(lines, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) paths.push_back(line);
    }

    const fs::path stampPath = files / kStampFile;
    if (readFile(stampPath) == stamp) {
        SUPERSONIC_LOG_INFO("Penumbra") << "Game files already unpacked (" << paths.size() << ", " << stamp << ").";
        return true;
    }

    SUPERSONIC_LOG_INFO("Penumbra") << "Unpacking " << paths.size() << " game files into " << files.string() << "...";
    std::error_code ec;
    fs::remove(stampPath, ec);
    for (const char* root : kUnpackedRoots) fs::remove_all(files / root, ec);

    std::size_t done = 0;
    for (const std::string& path : paths) {
        if (!copyAsset(manager, path, files / path)) {
            SUPERSONIC_LOG_ERROR("Penumbra") << "Could not unpack " << path << ".";
            return false;
        }
        // The UI thread waits for this one to read its lifecycle commands;
        // an unpack that stopped reading for seconds would be an ANR dialog.
        if (++done % 16 == 0 && !Supersonic::Android::PumpEvents()) return false;
    }

    std::ofstream(stampPath, std::ios::binary | std::ios::trunc) << stamp;
    SUPERSONIC_LOG_INFO("Penumbra") << "Unpacked " << done << " files.";
    return true;
}

// The development flags, if a file of them was left for this launch.
std::vector<std::string> takeDevelopmentFlags(const std::vector<fs::path>& folders) {
    std::vector<std::string> flags;
    for (const fs::path& folder : folders) {
        if (folder.empty()) continue;
        const fs::path file = folder / kArgsFile;
        std::error_code ec;
        if (!fs::is_regular_file(file, ec)) continue;

        const std::string text = readFile(file);
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
        break;
    }
    return flags;
}

} // namespace

int SupersonicMain(int argc, char** argv) {
    const fs::path files = Supersonic::Android::InternalDataPath();
    if (files.empty() || !unpackAssets(files)) {
        SUPERSONIC_LOG_ERROR("Penumbra") << "The game's files are not available; stopping.";
        return EXIT_FAILURE;
    }

    // Where the engine finds assets/shaders (ChooseAssetRoot's working
    // directory), and where relative writes land: the app's own storage.
    std::error_code ec;
    fs::current_path(files / "engine", ec);
    if (ec) SUPERSONIC_LOG_ERROR("Penumbra") << "Cannot enter " << (files / "engine").string() << ": " << ec.message();

    // E19: the device's language, which an activity's environment does not
    // carry (no LANG), for the first launch's default before settings.json.
    if (android_app* app = Supersonic::Android::App(); app != nullptr && app->config != nullptr) {
        char language[2] = {0, 0};
        AConfiguration_getLanguage(app->config, language);
        if (language[0] != 0) Penumbra::Render::Settings::SetSystemLocale(std::string(language, 2));
    }

    std::vector<std::string> args;
    args.emplace_back(argc > 0 ? argv[0] : "Penumbra");
    args.emplace_back("--original");
    args.emplace_back((files / "original").string());
    args.emplace_back("--data");
    args.emplace_back((files / "data").string());
    for (std::string& flag : takeDevelopmentFlags({files, Supersonic::Android::ExternalDataPath()})) {
        args.push_back(std::move(flag));
    }

    std::vector<char*> pointers;
    for (std::string& arg : args) pointers.push_back(arg.data());
    pointers.push_back(nullptr);
    return PenumbraMain(static_cast<int>(args.size()), pointers.data());
}
