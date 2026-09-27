#include "eth/Paths.hpp"

#include <system_error>

namespace Penumbra::Eth {

namespace {

namespace fs = std::filesystem;

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

} // namespace Penumbra::Eth
