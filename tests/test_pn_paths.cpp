// Where the game finds the original's files and its own data (eth/Paths.hpp):
// the flag, then original/ and data/ beside the executable (a packaged game,
// tools/package.bat), then the build's paths. Asked of folders made in the
// temp directory, never of the real layout, except to confirm that the build's
// own path still holds the original and that package.bat still writes the
// folder names the resolver looks for.
//
// And how a file named as the original named it is found (ResolveUnder): the
// case and the slashes of a Windows-only game on a filesystem that minds them.

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

#include "TestHarness.hpp"
#include "eth/Audio.hpp"
#include "eth/Machine.hpp"
#include "eth/Paths.hpp"

namespace {

namespace fs = std::filesystem;
using Penumbra::Eth::DescribeRootSource;
using Penumbra::Eth::FindDataRoot;
using Penumbra::Eth::FindOriginalRoot;
using Penumbra::Eth::FoundRoot;
using Penumbra::Eth::HoldsOriginal;
using Penumbra::Eth::HoldsPortData;
using Penumbra::Eth::RootSource;

void Touch(const fs::path& file) {
    fs::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << "x";
}

bool Same(const fs::path& a, const fs::path& b) {
    std::error_code ec;
    return fs::equivalent(a, b, ec) && !ec;
}

std::string ReadText(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// Restores the working directory whatever a check does in between.
class WorkingDirectory {
public:
    explicit WorkingDirectory(const fs::path& to) : m_previous(fs::current_path()) { fs::current_path(to); }
    ~WorkingDirectory() {
        std::error_code ec;
        fs::current_path(m_previous, ec);
    }
    WorkingDirectory(const WorkingDirectory&) = delete;
    WorkingDirectory& operator=(const WorkingDirectory&) = delete;

private:
    fs::path m_previous;
};

struct Layout {
    fs::path root;
    fs::path packaged;      // an executable's folder with original/ and data/ beside it
    fs::path hollow;        // an executable's folder whose original/ and data/ lack the markers
    fs::path bare;          // an executable's folder with nothing beside it
    fs::path builtOriginal; // what PENUMBRA_ORIGINAL_DIR would name
    fs::path builtData;     // what PENUMBRA_DATA_DIR would name
    fs::path flagOriginal;  // a folder a player names with --original
    fs::path flagData;
    fs::path empty;         // exists, holds nothing
};

Layout MakeLayout() {
    Layout l;
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    l.root = fs::temp_directory_path() / ("penumbra_test_paths_" + std::to_string(stamp));
    l.packaged = l.root / "Penumbra";
    l.hollow = l.root / "Hollow";
    l.bare = l.root / "Bare";
    l.builtOriginal = l.root / "repo" / "extracted" / "app";
    l.builtData = l.root / "repo" / "game" / "data";
    l.flagOriginal = l.root / "elsewhere" / "Penumbra 2010";
    l.flagData = l.root / "elsewhere" / "translations";
    l.empty = l.root / "empty";

    Touch(l.packaged / "original" / "data.enml");
    Touch(l.packaged / "data" / "strings.json");
    fs::create_directories(l.hollow / "original" / "scenes");
    Touch(l.hollow / "original" / "hs.enml");
    fs::create_directories(l.hollow / "data" / "strings.json");   // a folder by the marker's name is not it
    fs::create_directories(l.bare);
    Touch(l.builtOriginal / "data.enml");
    Touch(l.builtData / "strings.json");
    Touch(l.flagOriginal / "data.enml");
    Touch(l.flagData / "strings.json");
    fs::create_directories(l.empty);
    return l;
}

void TestMarkers(const Layout& l) {
    CHECK(HoldsOriginal(l.builtOriginal));
    CHECK(HoldsOriginal(l.packaged / "original"));
    CHECK(!HoldsOriginal(l.hollow / "original"));        // hs.enml and scenes/ are not enough
    CHECK(!HoldsOriginal(l.builtData));
    CHECK(!HoldsOriginal(l.root / "missing"));
    CHECK(!HoldsOriginal(fs::path()));
    CHECK(HoldsPortData(l.builtData));
    CHECK(HoldsPortData(l.packaged / "data"));
    CHECK(!HoldsPortData(l.hollow / "data"));            // strings.json there is a folder
    CHECK(!HoldsPortData(l.builtOriginal));
    CHECK(!HoldsPortData(fs::path()));
}

void TestOriginal(const Layout& l) {
    // 1. The flag wins over both, and is taken as named.
    FoundRoot r = FindOriginalRoot(l.flagOriginal, l.packaged, l.builtOriginal);
    CHECK(r.source == RootSource::Flag);
    CHECK(r.found);
    CHECK(Same(r.path, l.flagOriginal));

    // A flag naming a folder without data.enml is reported as such - never
    // replaced by the packaged or the built one behind the player's back.
    r = FindOriginalRoot(l.empty, l.packaged, l.builtOriginal);
    CHECK(r.source == RootSource::Flag);
    CHECK(!r.found);
    CHECK(Same(r.path, l.empty));
    r = FindOriginalRoot(l.root / "no such folder", l.packaged, l.builtOriginal);
    CHECK(r.source == RootSource::Flag);
    CHECK(!r.found);
    CHECK(r.path.is_absolute());

    // 2. A packaged folder: original/ beside the executable, over the build's.
    r = FindOriginalRoot({}, l.packaged, l.builtOriginal);
    CHECK(r.source == RootSource::BesideExecutable);
    CHECK(r.found);
    CHECK(Same(r.path, l.packaged / "original"));
    // ...even when the build's path is gone, as on any other machine.
    r = FindOriginalRoot({}, l.packaged, l.root / "someone else's checkout");
    CHECK(r.source == RootSource::BesideExecutable);
    CHECK(r.found);

    // 3. The build tree: nothing usable beside the executable.
    r = FindOriginalRoot({}, l.bare, l.builtOriginal);
    CHECK(r.source == RootSource::Build);
    CHECK(r.found);
    CHECK(Same(r.path, l.builtOriginal));
    r = FindOriginalRoot({}, l.hollow, l.builtOriginal);    // original/ without data.enml is skipped
    CHECK(r.source == RootSource::Build);
    CHECK(Same(r.path, l.builtOriginal));

    // 4. Nowhere: not found, and the build's path is kept for the message.
    r = FindOriginalRoot({}, l.bare, l.root / "gone");
    CHECK(r.source == RootSource::NotFound);
    CHECK(!r.found);
    CHECK(r.path == (l.root / "gone").lexically_normal());
    r = FindOriginalRoot({}, {}, {});
    CHECK(r.source == RootSource::NotFound);
    CHECK(!r.found);
    CHECK(r.path.empty());
}

void TestData(const Layout& l) {
    FoundRoot r = FindDataRoot(l.flagData, l.packaged, l.builtData);
    CHECK(r.source == RootSource::Flag);
    CHECK(r.found);
    CHECK(Same(r.path, l.flagData));
    r = FindDataRoot(l.flagOriginal, l.packaged, l.builtData);   // the original's folder is not the data
    CHECK(r.source == RootSource::Flag);
    CHECK(!r.found);

    r = FindDataRoot({}, l.packaged, l.builtData);
    CHECK(r.source == RootSource::BesideExecutable);
    CHECK(Same(r.path, l.packaged / "data"));

    r = FindDataRoot({}, l.hollow, l.builtData);
    CHECK(r.source == RootSource::Build);
    CHECK(Same(r.path, l.builtData));

    r = FindDataRoot({}, l.bare, l.root / "gone");
    CHECK(r.source == RootSource::NotFound);
    CHECK(!r.found);

    // The two roots are decided apart: a package with only data/ beside it
    // still takes the original from the build.
    const fs::path half = l.root / "Half";
    Touch(half / "data" / "strings.json");
    CHECK(FindDataRoot({}, half, l.builtData).source == RootSource::BesideExecutable);
    CHECK(FindOriginalRoot({}, half, l.builtOriginal).source == RootSource::Build);
}

void TestWorkingDirectory(const Layout& l) {
    // An unknown executable directory is never asked about: "" / "original"
    // would be the relative "original", found in whatever folder the game was
    // launched from. Here the working directory holds one, and it is ignored.
    const fs::path launchedFrom = l.root / "launched";
    Touch(launchedFrom / "original" / "data.enml");
    Touch(launchedFrom / "data" / "strings.json");
    {
        WorkingDirectory cwd(launchedFrom);
        FoundRoot r = FindOriginalRoot({}, {}, l.builtOriginal);
        CHECK(r.source == RootSource::Build);
        CHECK(Same(r.path, l.builtOriginal));
        r = FindDataRoot({}, {}, l.builtData);
        CHECK(r.source == RootSource::Build);
    }

    // A relative flag is the player's: resolved against the directory they
    // launched from, made absolute before the engine moves the working
    // directory, and tidied.
    {
        WorkingDirectory cwd(l.root);
        FoundRoot r = FindOriginalRoot(fs::path("elsewhere") / "Penumbra 2010", l.packaged, l.builtOriginal);
        CHECK(r.source == RootSource::Flag);
        CHECK(r.found);
        CHECK(r.path.is_absolute());
        CHECK(Same(r.path, l.flagOriginal));
        r = FindOriginalRoot(fs::path("elsewhere") / "." / "x" / ".." / "Penumbra 2010", l.packaged, l.builtOriginal);
        CHECK(r.found);
        CHECK(Same(r.path, l.flagOriginal));
        bool tidy = true;
        for (const fs::path& part : r.path) tidy = tidy && part != "." && part != "..";
        CHECK(tidy);
        r = FindDataRoot(fs::path("elsewhere") / "translations", {}, {});
        CHECK(r.found);
        CHECK(r.path.is_absolute());
        // A relative build path (never the case: CMake bakes absolute ones)
        // comes back absolute too.
        r = FindOriginalRoot({}, {}, fs::path("repo") / "extracted" / "app");
        CHECK(r.source == RootSource::Build);
        CHECK(r.path.is_absolute());
    }
}

void TestDescriptions() {
    const std::string flag = DescribeRootSource(RootSource::Flag);
    const std::string beside = DescribeRootSource(RootSource::BesideExecutable);
    const std::string build = DescribeRootSource(RootSource::Build);
    const std::string missing = DescribeRootSource(RootSource::NotFound);
    CHECK(!flag.empty() && !beside.empty() && !build.empty() && !missing.empty());
    CHECK(flag != beside && beside != build && build != missing && flag != missing);
}

// Whether this filesystem tells names apart by case (Linux, a case-sensitive
// APFS volume) - asked of it, not assumed from the platform.
bool CaseSensitive(const fs::path& directory) {
    Touch(directory / "Probe.Case");
    std::error_code ec;
    const bool sensitive = !fs::exists(directory / "probe.case", ec);
    fs::remove(directory / "Probe.Case", ec);
    return sensitive;
}

bool IsFile(const std::string& path) {
    std::error_code ec;
    return fs::is_regular_file(fs::path(path), ec);
}

void TestAssetResolver(const Layout& l) {
    using Penumbra::Eth::AssetIndexSize;
    using Penumbra::Eth::ResolveUnder;
    const fs::path base = l.root / "assets";
    Touch(base / "Scenes" / "Level1.esc");
    Touch(base / "entities" / "STONE03A.JPG");
    Touch(base / "soundfx" / "Hit01.ogg");
    Touch(l.root / "Outside.txt");
    const bool sensitive = CaseSensitive(base);
    std::printf("  (this filesystem is case-%s)\n", sensitive ? "sensitive" : "insensitive");
    const std::string root = base.generic_string();

    // Spelled as on disk: exactly the path the loaders always joined.
    CHECK(ResolveUnder(root, "Scenes/Level1.esc") == root + "/Scenes/Level1.esc");
    // Another case, a backslash, both: found, and off Windows spelled as the
    // disk spells it. Where the filesystem ignores case the joined path already
    // opens, and comes back unchanged.
    const std::string lower = ResolveUnder(root, "scenes/level1.esc");
    CHECK(IsFile(lower));
    CHECK(lower == (sensitive ? root + "/Scenes/Level1.esc" : root + "/scenes/level1.esc"));
    const std::string slashed = ResolveUnder(root, "entities\\STONE03A.JPG");
    CHECK(IsFile(slashed));
    const std::string both = ResolveUnder(root, "SOUNDFX\\hit01.OGG");
    CHECK(IsFile(both));
    const std::string dotted = ResolveUnder(root, "./Entities/../ENTITIES/stone03a.jpg");
    CHECK(IsFile(dotted));
    if (sensitive) {
        CHECK(slashed == root + "/entities/STONE03A.JPG");
        CHECK(both == root + "/soundfx/Hit01.ogg");
        CHECK(dotted == root + "/entities/STONE03A.JPG");
    }
    // Nothing by that name, or not under the root at all: the joined path, so
    // the caller's "cannot open" names what was asked for.
    CHECK(ResolveUnder(root, "scenes/missing.esc") == root + "/scenes/missing.esc");
    CHECK(ResolveUnder(root, "../OUTSIDE.TXT") == root + "/../OUTSIDE.TXT");
    CHECK(ResolveUnder(root, "") == root + "/");
    CHECK(ResolveUnder("", "Scenes/Level1.esc") == "/Scenes/Level1.esc");
    // Three folders, three files: the probe was gone before the index was made.
    CHECK_EQ(AssetIndexSize(root), std::size_t{6});

    // The loaders go through it: a Machine's reads, a sample bank's loads.
    Penumbra::Eth::MachineConfig config;
    config.gameRoot = root;
    Penumbra::Eth::Machine machine(config);
    CHECK(IsFile(machine.ReadPath("SCENES\\level1.ESC")));
    CHECK(IsFile(machine.ReadPath(root + "/scenes/LEVEL1.esc")));
    Penumbra::Eth::SampleBank samples(root);
    CHECK(samples.LoadSoundEffect("SoundFX/HIT01.ogg"));
    CHECK(!samples.LoadSoundEffect("soundfx/none.ogg"));
}

// The real tree: the build's own original, and tools/package.bat writing the
// folder names the resolver asks for (the engine checks its packager against
// LooksLikeAPackagedFolder the same way, test_executable_path).
void TestRepository() {
    const fs::path built = PENUMBRA_ORIGINAL_DIR;
    if (!fs::exists(built)) {
        std::printf("  (the original is not at %s; its checks are skipped)\n", PENUMBRA_ORIGINAL_DIR);
        return;
    }
    CHECK(HoldsOriginal(built));
    const FoundRoot r = FindOriginalRoot({}, fs::path(), built);
    CHECK(r.source == RootSource::Build);
    // The original's own files, named as a Windows script might have named them.
    CHECK(IsFile(Penumbra::Eth::ResolveUnder(PENUMBRA_ORIGINAL_DIR, "SCENES\\Menu.ESC")));
    CHECK(IsFile(Penumbra::Eth::ResolveUnder(PENUMBRA_ORIGINAL_DIR, "soundfx/MENU.mp3")));
    CHECK(IsFile(Penumbra::Eth::ResolveUnder(PENUMBRA_ORIGINAL_DIR, "entities/stone03a.jpg")));

    const fs::path script = built / ".." / ".." / "tools" / "package.bat";
    if (!fs::exists(script)) {
        std::printf("  (no tools/package.bat beside %s; its checks are skipped)\n", PENUMBRA_ORIGINAL_DIR);
        return;
    }
    const std::string text = ReadText(script);
    CHECK_MSG(text.find(std::string("%PKG%\\") + Penumbra::Eth::kPackagedOriginalFolder) != std::string::npos,
              "package.bat must copy the original into <package>\\original");
    CHECK_MSG(text.find(std::string("%PKG%\\") + Penumbra::Eth::kPackagedDataFolder) != std::string::npos,
              "package.bat must copy game\\data into <package>\\data");
    CHECK_MSG(text.find("%PKG%\\assets\\shaders") != std::string::npos,
              "package.bat must copy the engine's shaders, which anchor the working directory");
    // cmd.exe mis-parses a batch file with bare LF line ends.
    bool crlf = text.find('\n') != std::string::npos;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n' && (i == 0 || text[i - 1] != '\r')) crlf = false;
    }
    CHECK_MSG(crlf, "package.bat must have CRLF line ends (.gitattributes converts only on checkout)");
}

} // namespace

int main() {
    const Layout layout = MakeLayout();
    TestMarkers(layout);
    TestOriginal(layout);
    TestData(layout);
    TestWorkingDirectory(layout);
    TestDescriptions();
    TestAssetResolver(layout);
    TestRepository();
    std::error_code ec;
    fs::remove_all(layout.root, ec);
    return test::summary("test_pn_paths", 60);
}
