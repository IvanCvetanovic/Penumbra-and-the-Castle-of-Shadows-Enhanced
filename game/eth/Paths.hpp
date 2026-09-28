#pragma once

// WHERE THE GAME'S FILES ARE, decided once at startup (main.cpp) and handed to
// the layer: the original's files (data.enml, scenes/, entities/, soundfx/...)
// and the port's own data (strings.json, images/en).
//
// Built from this repository, both are the absolute paths the build baked in
// (PENUMBRA_ORIGINAL_DIR -> extracted/app, PENUMBRA_DATA_DIR -> game/data). A
// packaged copy (tools/package.bat) cannot use those: they name a checkout on
// the machine that built it. So a folder beside the executable wins over them,
// and a flag wins over both. First match, each root on its own:
//
//   1. the flag (--original <dir>, --data <dir>), whatever it holds - a player
//      who names a folder gets told it is wrong rather than silently handed
//      another one (main.cpp refuses a flag that does not hold the marker);
//   2. <executable dir>/original holding data.enml, <executable dir>/data
//      holding strings.json - the packaged layout;
//   3. the build's path, when it holds the marker;
//   4. none: `found` is false and `path` is the build's, for the message.
//
// The same shape as the engine's ChooseAssetRoot (platform/ExecutablePath.hpp),
// which settles where the ENGINE's files (assets/shaders) come from by the
// same rule - the executable's folder before the build's configured root - and
// for the same reason. And like it, an empty candidate is never asked about:
// an unknown executable directory must not become the relative "original",
// looked up in whatever the working directory happens to be.
//
// Every path returned is ABSOLUTE. The engine moves the working directory to
// its asset root when the app starts (AnchorAssetRoot), so a relative flag has
// to be resolved against the directory the player launched from, before that.
//
// In the Eth library, not the layer's, because it is the lowest one every
// suite links; the build's data path (a PenumbraGame definition) is therefore
// a parameter, never read here.

#include <cstddef>
#include <filesystem>
#include <string>

namespace Penumbra::Eth {

// The folder names the packaged layout uses beside the executable. tools/package.bat
// writes the same names; test_pn_paths checks that it still does.
inline constexpr const char* kPackagedOriginalFolder = "original";
inline constexpr const char* kPackagedDataFolder = "data";
// What makes a folder the one asked for.
inline constexpr const char* kOriginalMarker = "data.enml";
inline constexpr const char* kDataMarker = "strings.json";

enum class RootSource {
    Flag,               // --original / --data
    BesideExecutable,   // <exe dir>/original, <exe dir>/data: a packaged game
    Build,              // the path the build baked in
    NotFound,           // none of them holds the marker
};

struct FoundRoot {
    std::filesystem::path path;         // absolute; the build's path when NotFound
    RootSource source = RootSource::NotFound;
    // The marker is there. False for NotFound, and for a flag naming a folder
    // without it (main.cpp refuses that run).
    bool found = false;
};

// Whether `directory` holds the original game (data.enml) / the port's data
// (strings.json). False for an empty path.
bool HoldsOriginal(const std::filesystem::path& directory);
bool HoldsPortData(const std::filesystem::path& directory);

// The rule above for each root. `flag` is empty when not given; a relative flag
// is made absolute against the CURRENT working directory, so call these before
// the engine moves it. `executableDirectory` may be empty (unknown).
FoundRoot FindOriginalRoot(const std::filesystem::path& flag, const std::filesystem::path& executableDirectory,
                           const std::filesystem::path& built);
FoundRoot FindDataRoot(const std::filesystem::path& flag, const std::filesystem::path& executableDirectory,
                       const std::filesystem::path& built);

// "the command line", "beside the executable", "the build's path", "not
// found": for the startup log.
const char* DescribeRootSource(RootSource source);

// --- A file named as the original named it ---------------------------------------------
//
// The original's scripts, scenes and entity files were only ever read on
// Windows, whose filesystem ignores case and takes either slash. On a
// case-sensitive one (Linux, Android, iOS, a case-sensitive APFS volume) a
// name spelled differently from the file on disk, or written with a
// backslash - an ordinary character in a POSIX name - would not be found.
// Every loader therefore opens root + "/" + relative through this ONE
// function, which answers:
//
//   1. root + "/" + relative exactly as the loaders always joined it, when
//      that exists: every lookup on Windows (so there the answer is the old
//      path, unchanged) and every correctly spelled one elsewhere;
//   2. otherwise the file the case-folded index of `root` names: every file
//      and folder under it, keyed by its path relative to `root` with '\'
//      as '/' and A-Z as a-z, built on the first miss under that root and
//      kept for the process. The roots are read-only (extracted/app is never
//      written; game/data is the port's own), so an index cannot go stale -
//      and a file spelled correctly is found by step 1 whatever the index
//      holds;
//   3. neither: the joined path of step 1, so the caller's "cannot open"
//      names what was asked for.
//
// Thread-safe. A path that leaves `root` ("../x") is never looked up.
std::string ResolveUnder(const std::string& root, const std::string& relative);

// How many entries the index of `root` holds (building it if need be), for a
// suite.
std::size_t AssetIndexSize(const std::string& root);

} // namespace Penumbra::Eth
