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
//    E49: a start that is stopped while it unpacks (by the system, or the
//    player) does not cost the next one the work: eth/AssetUnpack.hpp continues
//    it, and the game's log is opened for it, so the report of a stopped start
//    says how far the unpack got.
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

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "core/Log.hpp"
#include "eth/AssetUnpack.hpp"   // E49
#include "eth/StartupErrors.hpp"   // E39
#include "platform/ExecutablePath.hpp"
#include "platform/android/AndroidApp.hpp"
#include "render/Settings.hpp"

int PenumbraMain(int argc, char** argv);

namespace {

namespace fs = std::filesystem;

// Written by tools/build_android.sh into the APK's assets: the first line is a
// stamp that changes whenever any packed file does, every other line a path
// under assets/.
constexpr const char* kAssetList = "penumbra_assets.txt";
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
    // The folder exists: the unpack makes it (eth/AssetUnpack.hpp).
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

// The length of an asset, or -1 when the APK has none: how the unpack tells a whole file of an earlier start from a
// stopped copy.
std::int64_t assetLength(AAssetManager* manager, const std::string& path) {
    AAsset* asset = AAssetManager_open(manager, path.c_str(), AASSET_MODE_STREAMING);
    if (asset == nullptr) return -1;
    const std::int64_t length = static_cast<std::int64_t>(AAsset_getLength64(asset));
    AAsset_close(asset);
    return length;
}

// One line of the unpack's story, for logcat and (E49) for the game's own log. That log (main.cpp) is opened after the
// files are unpacked, so a start stopped during the unpack used to leave none, and the report of it could only say "No game
// log: the game stopped before it opened one". The unpack opens the log itself, the first time it has something to say (a
// start whose files are in place says nothing here and leaves the log to main.cpp), under the name main.cpp uses; main.cpp
// then opens it again, which replaces it, and writes the unpack's summary (Eth::LastUnpackSummary) among its first lines.
void sayUnpack(const std::string& line) {
    static bool logOpened = false;
    if (!logOpened) {
        logOpened = true;
        const fs::path folder = Supersonic::UserDataDirectory("Penumbra");   // main.cpp's manifest.title
        if (!folder.empty()) {
            std::error_code ignored;
            fs::create_directories(folder, ignored);
            Supersonic::Log::SetFileSink((folder / "penumbra.log").string());
            Supersonic::Log::SetElapsedTimestamps(true);
            SUPERSONIC_LOG_INFO("Penumbra") << "Penumbra " << PENUMBRA_VERSION << " (mobile build): unpacking the game's files";
        }
    }
    SUPERSONIC_LOG_INFO("Penumbra") << line;
}

// Unpacks the APK's files into `files` unless the last unpack was of this very list, and continues one that an earlier
// start left unfinished (E49, eth/AssetUnpack.hpp). Failed when the list is missing or a file would not unpack.
Penumbra::Eth::UnpackOutcome unpackAssets(const fs::path& files) {
    using Penumbra::Eth::UnpackOutcome;
    android_app* app = Supersonic::Android::App();
    AAssetManager* manager = app->activity->assetManager;

    std::string list;
    if (!readAsset(manager, kAssetList, list)) {
        SUPERSONIC_LOG_ERROR("Penumbra") << "The APK has no " << kAssetList << "; nothing to unpack.";
        return UnpackOutcome::Failed;
    }
    std::istringstream lines(list);
    std::string stamp;
    std::getline(lines, stamp);
    if (!stamp.empty() && stamp.back() == '\r') stamp.pop_back();
    std::vector<std::string> paths;
    for (std::string line; std::getline(lines, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) paths.push_back(line);
    }

    Penumbra::Eth::UnpackHooks hooks;
    hooks.length = [manager](const std::string& path) { return assetLength(manager, path); };
    hooks.copy = [manager](const std::string& path, const fs::path& target) { return copyAsset(manager, path, target); };
    // The UI thread waits for this one to read its lifecycle commands; an unpack that stopped reading for seconds would be
    // an ANR dialog. Asked before each file: a file takes a few milliseconds, the whole unpack a few seconds on a slow phone.
    hooks.keepGoing = [] { return Supersonic::Android::PumpEvents(); };
    hooks.log = sayUnpack;

    const std::vector<std::string> roots(std::begin(kUnpackedRoots), std::end(kUnpackedRoots));
    const Penumbra::Eth::UnpackResult result = Penumbra::Eth::UnpackAssets(files, stamp, paths, roots, hooks);
    if (result.outcome == UnpackOutcome::UpToDate) {
        SUPERSONIC_LOG_INFO("Penumbra") << "Game files already unpacked (" << paths.size() << ", " << stamp << ").";
    } else if (result.outcome == UnpackOutcome::Unpacked) {
        Penumbra::Eth::LastUnpackSummary() = Penumbra::Eth::DescribeUnpack(result);
        SUPERSONIC_LOG_INFO("Penumbra") << Penumbra::Eth::LastUnpackSummary();
    }
    return result.outcome;
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
    // E19: the device's language, which an activity's environment does not
    // carry (no LANG), for the first launch's default before settings.json, and (E39) for the
    // language of a dialog about a failed unpack, which comes before anything else.
    if (android_app* app = Supersonic::Android::App(); app != nullptr && app->config != nullptr) {
        char language[2] = {0, 0};
        AConfiguration_getLanguage(app->config, language);
        if (language[0] != 0) Penumbra::Render::Settings::SetSystemLocale(std::string(language, 2));
    }

    const fs::path files = Supersonic::Android::InternalDataPath();
    const Penumbra::Eth::UnpackOutcome unpacked =
        files.empty() ? Penumbra::Eth::UnpackOutcome::Failed : unpackAssets(files);
    if (unpacked == Penumbra::Eth::UnpackOutcome::Cancelled) {
        // E49: the player left (Back) while the files were being unpacked. That is neither an error the next start
        // should report nor a dialog for an activity that is going: what was copied stays, and the next start goes on.
        return EXIT_SUCCESS;
    }
    if (unpacked == Penumbra::Eth::UnpackOutcome::Failed) {
        SUPERSONIC_LOG_ERROR("Penumbra") << "The game's files are not available; stopping.";
        // E39: said to the player; before, the activity closed with nothing on the screen. The usual cause is a
        // full phone (about 21 MB are unpacked). Not for a scripted run (a file of development flags is left for
        // it): automation must never meet a dialog nobody closes.
        std::error_code ignored;
        const bool scripted = (!files.empty() && fs::is_regular_file(files / kArgsFile, ignored)) ||
                              fs::is_regular_file(fs::path(Supersonic::Android::ExternalDataPath()) / kArgsFile, ignored);
        if (!scripted) {
            Penumbra::Eth::ShowStartupDialog(Penumbra::Eth::StartupMessage(
                Penumbra::Eth::StartupProblem::GameFilesMissing,
                Penumbra::Render::Settings::SystemLanguageIsPortuguese(), "", "", true));
        }
        return EXIT_FAILURE;
    }

    // The window may have gone while the files were unpacked (the player pressed Home on the black screen of a
    // first run): wait for it to come back, or end quietly if the app was closed. Without it the engine's
    // window would throw "no window to draw on" and the player would be told their graphics are at fault.
    Supersonic::Android::WaitUntilDrawable();
    if (Supersonic::Android::DestroyRequested()) return EXIT_SUCCESS;

    // Where the engine finds assets/shaders (ChooseAssetRoot's working
    // directory), and where relative writes land: the app's own storage.
    std::error_code ec;
    fs::current_path(files / "engine", ec);
    if (ec) SUPERSONIC_LOG_ERROR("Penumbra") << "Cannot enter " << (files / "engine").string() << ": " << ec.message();

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
    // PenumbraMain tells the player about a std::exception of its own start; this is for whatever it does
    // not catch (an exception outside its try blocks, or one that is not a std::exception): thrown out of
    // the game's thread it would be std::terminate, a black screen and the launcher with nothing said.
    // Not for a scripted run (development flags beyond the two paths above): automation never meets a
    // dialog nobody closes.
    constexpr std::size_t kBuiltInArguments = 5;   // argv[0], --original <dir>, --data <dir>, as pushed above
    const bool scripted = args.size() > kBuiltInArguments;
    const auto fail = [scripted](const std::string& what) {
        SUPERSONIC_LOG_ERROR("Penumbra") << "fatal: " << what;
        if (scripted) return;
        Penumbra::Eth::ShowStartupDialog(Penumbra::Eth::StartupMessage(
            Penumbra::Eth::StartupProblem::StoppedByError, Penumbra::Render::Settings::SystemLanguageIsPortuguese(),
            what, "", true));
    };
    try {
        return PenumbraMain(static_cast<int>(args.size()), pointers.data());
    } catch (const std::exception& e) {
        fail(e.what());
    } catch (...) {
        fail("an exception that is not a std::exception");
    }
    return EXIT_FAILURE;
}
