#include "eth/Paths.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <system_error>
#include <unordered_map>

#include "core/Log.hpp"

namespace Penumbra::Eth {

namespace {

namespace fs = std::filesystem;

// --- The case-folded index -----------------------------------------------------------

// A root's files and folders, keyed by Fold(path relative to the root). More
// than this many entries means the root is not a game folder (a drive, a home
// directory named by mistake): the index stops there rather than walk it all.
constexpr std::size_t kMaxIndexEntries = 200000;

struct AssetIndex {
    std::unordered_map<std::string, std::string> byFolded;   // folded -> relative, as on disk ('/')
};

// '\' as '/', A-Z as a-z: how Windows compared the names the original used.
// ASCII only, deliberately - every name the original ships is ASCII, and a
// fold of cp1252 bytes against UTF-8 names on disk would be a guess.
std::string Fold(std::string path) {
    for (char& c : path) {
        if (c == '\\') c = '/';
        else if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return path;
}

// The relative part tidied as the index keys it: separators unified, "." and
// ".." segments applied, no leading or trailing slash. Empty for a path that
// leaves the root or names the root itself.
std::string IndexKey(const std::string& relative) {
    std::string slashed = relative;
    std::replace(slashed.begin(), slashed.end(), '\\', '/');
    std::string key = fs::path(slashed).lexically_normal().generic_string();
    while (!key.empty() && key.back() == '/') key.pop_back();
    while (key.size() >= 2 && key[0] == '.' && key[1] == '/') key.erase(0, 2);
    if (key.empty() || key == "." || key == ".." || key.rfind("../", 0) == 0 || key[0] == '/') return {};
    return Fold(key);
}

std::shared_ptr<const AssetIndex> BuildIndex(const std::string& root) {
    auto index = std::make_shared<AssetIndex>();
    const fs::path base(root);
    std::error_code ec;
    if (!fs::is_directory(base, ec)) return index;
    fs::recursive_directory_iterator it(base, fs::directory_options::skip_permission_denied, ec);
    const fs::recursive_directory_iterator end;
    for (; !ec && it != end; it.increment(ec)) {
        if (index->byFolded.size() >= kMaxIndexEntries) {
            SUPERSONIC_LOG_WARN("Penumbra") << "paths: " << root << " holds more than " << kMaxIndexEntries
                                            << " entries; only those are looked up by case";
            break;
        }
        const std::string relative = it->path().lexically_relative(base).generic_string();
        if (relative.empty()) continue;
        // Two names that differ only in case (possible off Windows): the
        // smaller one, so the answer never depends on the directory's order.
        auto [slot, inserted] = index->byFolded.try_emplace(Fold(relative), relative);
        if (!inserted && relative < slot->second) slot->second = relative;
    }
    return index;
}

std::mutex g_indexMutex;
std::map<std::string, std::shared_ptr<const AssetIndex>>& Indexes() {
    static std::map<std::string, std::shared_ptr<const AssetIndex>> indexes;   // by the root as given
    return indexes;
}

std::shared_ptr<const AssetIndex> IndexFor(const std::string& root) {
    {
        const std::lock_guard<std::mutex> lock(g_indexMutex);
        const auto it = Indexes().find(root);
        if (it != Indexes().end()) return it->second;
    }
    // Built outside the lock (the walk is the slow part); the first one stored wins.
    std::shared_ptr<const AssetIndex> built = BuildIndex(root);
    const std::lock_guard<std::mutex> lock(g_indexMutex);
    return Indexes().try_emplace(root, std::move(built)).first->second;
}

bool Exists(const std::string& path) {
    std::error_code ec;
    return fs::exists(fs::path(path), ec) && !ec;
}

bool HoldsFile(const fs::path& directory, const char* marker) {
    if (directory.empty()) return false;
    std::error_code ec;
    return fs::is_regular_file(directory / marker, ec) && !ec;
}

// Absolute and lexically tidied ("a/./b/.." -> "a"), without touching the
// filesystem: the answer must not change with symlinks or with whether the
// folder exists yet, only with the text and the working directory.
fs::path Absolute(const fs::path& path) {
    std::error_code ec;
    const fs::path absolute = fs::absolute(path, ec);
    return (ec ? path : absolute).lexically_normal();
}

FoundRoot Find(const fs::path& flag, const fs::path& executableDirectory, const char* packagedFolder,
               const fs::path& built, bool (*holds)(const fs::path&)) {
    if (!flag.empty()) {
        const fs::path named = Absolute(flag);
        return {named, RootSource::Flag, holds(named)};
    }
    if (!executableDirectory.empty()) {
        const fs::path beside = Absolute(executableDirectory / packagedFolder);
        if (holds(beside)) return {beside, RootSource::BesideExecutable, true};
    }
    if (!built.empty()) {
        const fs::path baked = Absolute(built);
        if (holds(baked)) return {baked, RootSource::Build, true};
        return {baked, RootSource::NotFound, false};
    }
    return {fs::path(), RootSource::NotFound, false};
}

} // namespace

bool HoldsOriginal(const std::filesystem::path& directory) { return HoldsFile(directory, kOriginalMarker); }

bool HoldsPortData(const std::filesystem::path& directory) { return HoldsFile(directory, kDataMarker); }

FoundRoot FindOriginalRoot(const std::filesystem::path& flag, const std::filesystem::path& executableDirectory,
                           const std::filesystem::path& built) {
    return Find(flag, executableDirectory, kPackagedOriginalFolder, built, &HoldsOriginal);
}

FoundRoot FindDataRoot(const std::filesystem::path& flag, const std::filesystem::path& executableDirectory,
                       const std::filesystem::path& built) {
    return Find(flag, executableDirectory, kPackagedDataFolder, built, &HoldsPortData);
}

const char* DescribeRootSource(const RootSource source) {
    switch (source) {
    case RootSource::Flag: return "the command line";
    case RootSource::BesideExecutable: return "beside the executable";
    case RootSource::Build: return "the build's path";
    case RootSource::NotFound: return "not found";
    }
    return "not found";
}

std::string ResolveUnder(const std::string& root, const std::string& relative) {
    // Joined exactly as every loader joined it before there was a resolver.
    const std::string joined = root + "/" + relative;
    if (root.empty() || relative.empty() || Exists(joined)) return joined;

    const std::string key = IndexKey(relative);
    if (key.empty()) return joined;
    const std::shared_ptr<const AssetIndex> index = IndexFor(root);
    const auto it = index->byFolded.find(key);
    if (it == index->byFolded.end()) return joined;
    return root + "/" + it->second;
}

std::size_t AssetIndexSize(const std::string& root) { return IndexFor(root)->byFolded.size(); }

} // namespace Penumbra::Eth
