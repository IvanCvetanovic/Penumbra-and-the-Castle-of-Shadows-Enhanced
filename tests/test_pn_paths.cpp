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
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <system_error>
#include <vector>

#include "TestHarness.hpp"
#include "core/Log.hpp"   // E38
#include "eth/AssetUnpack.hpp"   // E49
#include "eth/Audio.hpp"
#include "eth/Machine.hpp"
#include "eth/Paths.hpp"
#include "eth/StartupErrors.hpp"

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

// A player who double-clicks Penumbra.exe INSIDE the downloaded zip: Explorer
// extracts that one file into %TEMP%\Temp1_<zip>\Penumbra\ and runs it there,
// and the build's baked path names a checkout on another machine. Neither root
// is found, and not through a flag - which is what makes main.cpp tell the
// player to extract the whole folder (eth/StartupErrors.hpp).
void TestRunFromInsideTheZip(const Layout& l) {
    const fs::path temp = l.root / "Temp1_Penumbra-Windows.zip" / "Penumbra";
    Touch(temp / "Penumbra.exe");
    const fs::path elsewhere = l.root / "D" / "a" / "Penumbra" / "extracted" / "app";
    const FoundRoot original = FindOriginalRoot({}, temp, elsewhere);
    CHECK(original.source == RootSource::NotFound);
    CHECK(!original.found);
    const FoundRoot data = FindDataRoot({}, temp, l.root / "D" / "a" / "Penumbra" / "game" / "data");
    CHECK(data.source == RootSource::NotFound);
    CHECK(!data.found);
    // Extracted whole, the same exe finds both beside it.
    Touch(temp / "original" / "data.enml");
    Touch(temp / "data" / "strings.json");
    CHECK(FindOriginalRoot({}, temp, elsewhere).source == RootSource::BesideExecutable);
    CHECK(FindDataRoot({}, temp, elsewhere).source == RootSource::BesideExecutable);
}

// Every byte sequence well-formed UTF-8 (the message box converts from it).
bool ValidUtf8(const std::string& text) {
    for (std::size_t i = 0; i < text.size();) {
        const auto c = static_cast<unsigned char>(text[i]);
        std::size_t extra = 0;
        if (c < 0x80) extra = 0;
        else if ((c & 0xE0) == 0xC0 && c >= 0xC2) extra = 1;
        else if ((c & 0xF0) == 0xE0) extra = 2;
        else if ((c & 0xF8) == 0xF0 && c <= 0xF4) extra = 3;
        else return false;
        if (i + extra >= text.size()) return false;   // cut short
        for (std::size_t k = 1; k <= extra; ++k) {
            if ((static_cast<unsigned char>(text[i + k]) & 0xC0) != 0x80) return false;
        }
        i += extra + 1;
    }
    return true;
}

bool Has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

// What a player is told, and when (eth/StartupErrors.hpp). Never the box
// itself: ShowStartupDialog would stop the suite at a modal window.
void TestStartupErrors() {
    using Penumbra::Eth::ErrorStream;
    using Penumbra::Eth::ShouldShowStartupDialog;
    using Penumbra::Eth::StartupMessage;
    using Penumbra::Eth::StartupProblem;

    // Only a run with nowhere to write stderr, and never a headless one.
    CHECK(ShouldShowStartupDialog(false, ErrorStream::Nowhere));
    CHECK(!ShouldShowStartupDialog(true, ErrorStream::Nowhere));
    for (const ErrorStream stream : {ErrorStream::Console, ErrorStream::Pipe, ErrorStream::File}) {
        CHECK(!ShouldShowStartupDialog(false, stream));
        CHECK(!ShouldShowStartupDialog(true, stream));
    }
    // This suite's own stderr is a console, a pipe or a file - never nowhere
    // under a runner - so nothing it runs would ever raise a box.
    CHECK(Penumbra::Eth::CurrentErrorStream() != ErrorStream::Nowhere);

    const std::string notTilde = "n\xC3\xA3o";   // "nao" with its tilde, as UTF-8
    const struct {
        StartupProblem problem;
        const char* english;
        const char* portuguese;
    } cases[] = {
        {StartupProblem::GameFilesMissing, "Penumbra could not find its game files.", "encontrou os arquivos do jogo."},
        {StartupProblem::FolderNameUnusable, "Penumbra cannot run from this folder", "pode ser aberto desta pasta"},
        {StartupProblem::NoGraphics, "Penumbra could not start its graphics.", "iniciar os gr\xC3\xA1" "ficos."},
        {StartupProblem::StoppedByError, "Penumbra stopped because of an unexpected error.", "O Penumbra parou"},
    };
    for (const auto& c : cases) {
        const std::string english = StartupMessage(c.problem, false, "", "");
        const std::string portuguese = StartupMessage(c.problem, true, "", "");
        CHECK(ValidUtf8(english));
        CHECK(ValidUtf8(portuguese));
        // Both languages, whichever comes first.
        CHECK(Has(english, c.english) && Has(english, c.portuguese));
        CHECK(Has(portuguese, c.english) && Has(portuguese, c.portuguese));
        CHECK(english.find(c.english) < english.find(c.portuguese));
        CHECK(portuguese.find(c.portuguese) < portuguese.find(c.english));
        // Neither line when there is nothing to say.
        CHECK(!Has(english, "Details:") && !Has(english, "Log:"));
        // The English half is plain ASCII; the Portuguese keeps its accents.
        const std::string englishHalf = english.substr(0, english.find("\n\n----"));
        bool ascii = true;
        for (const char ch : englishHalf) ascii = ascii && static_cast<unsigned char>(ch) < 0x80;
        CHECK(ascii);
        if (c.problem != StartupProblem::StoppedByError) CHECK(Has(portuguese, notTilde));
        // No '?' where a letter failed to encode, no stray escape.
        CHECK(!Has(portuguese, "?"));
    }
    // The steps Explorer names, in both languages.
    const std::string missing = StartupMessage(StartupProblem::GameFilesMissing, false, "", "");
    CHECK(Has(missing, "\"Extract All\""));
    CHECK(Has(missing, "\"Extrair Tudo\""));
    CHECK(Has(missing, "extract (unzip) the whole folder first"));
    CHECK(Has(StartupMessage(StartupProblem::NoGraphics, false, "", ""), "Vulkan 1.2"));
    CHECK(Has(StartupMessage(StartupProblem::StoppedByError, true, "", ""),
              "github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/issues"));

    // The detail and the log, once each, at the end, in any order of languages.
    const std::string detailed = StartupMessage(StartupProblem::NoGraphics, true, "Failed to find GPUs with Vulkan support!",
                                                "C:\\Users\\Jo\xC3\xA3o\\AppData\\Roaming\\Penumbra\\penumbra.log");
    CHECK(ValidUtf8(detailed));
    CHECK(Has(detailed, "\n\nDetails: Failed to find GPUs with Vulkan support!\nLog: C:\\Users\\Jo\xC3\xA3o\\"));
    CHECK(detailed.rfind("penumbra.log") == detailed.size() - std::string("penumbra.log").size());
    const std::string logOnly = StartupMessage(StartupProblem::StoppedByError, false, "", "/tmp/penumbra.log");
    CHECK(Has(logOnly, "\n\nLog: /tmp/penumbra.log"));
    CHECK(!Has(logOnly, "Details:"));

    // The validator itself: a cp1252 byte on its own is not UTF-8.
    CHECK(!ValidUtf8("n\xE3o"));
    CHECK(!ValidUtf8("\xC3"));
    CHECK(ValidUtf8(notTilde));

    // The loader probe answers without a window (always true off Windows,
    // where there is nothing to probe; on Windows whatever this machine has -
    // asked, not required).
    const bool loader = Penumbra::Eth::VulkanLoaderAvailable();
    std::printf("  (VulkanLoaderAvailable: %s)\n", loader ? "yes" : "no");
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
    const auto allCrlf = [](const std::string& batch) {
        bool crlf = batch.find('\n') != std::string::npos;
        for (std::size_t i = 0; i < batch.size(); ++i) {
            if (batch[i] == '\n' && (i == 0 || batch[i - 1] != '\r')) crlf = false;
        }
        return crlf;
    };
    CHECK_MSG(allCrlf(text), "package.bat must have CRLF line ends (.gitattributes converts only on checkout)");

    // The release zip's script: CRLF as well, and it packages through
    // package.bat rather than a copy of its rules.
    const fs::path tools = built / ".." / ".." / "tools";
    const std::string release = ReadText(tools / "make_release.bat");
    CHECK_MSG(allCrlf(release), "make_release.bat must have CRLF line ends");
    CHECK(Has(release, "package.bat"));
    CHECK(Has(release, "make_release.py"));

    // One release version everywhere a player's system reads one: the Windows
    // resources take the top-level project VERSION (penumbra_windows_resources),
    // and the Windows and Android manifests must say the same (make_release.py
    // refuses a release otherwise; this catches it on every push).
    const std::string cmake = ReadText(built / ".." / ".." / "CMakeLists.txt");
    const std::string marker = "project(Penumbra VERSION ";
    const std::size_t at = cmake.find(marker);
    CHECK_MSG(at != std::string::npos, "the top-level CMakeLists.txt must name the release: project(Penumbra VERSION x.y.z ...)");
    if (at != std::string::npos) {
        const std::size_t start = at + marker.size();
        const std::string version = cmake.substr(start, cmake.find(' ', start) - start);
        const std::string windowsManifest = ReadText(built / ".." / ".." / "game" / "windows" / "Penumbra.manifest");
        const std::string androidManifest = ReadText(built / ".." / ".." / "game" / "android" / "AndroidManifest.xml");
        CHECK_MSG(Has(windowsManifest, "version=\"" + version + ".0\""), "Penumbra.manifest's assemblyIdentity version must be " + version + ".0");
        CHECK_MSG(Has(androidManifest, "android:versionName=\"" + version + "\""), "AndroidManifest.xml's versionName must be " + version);

        // E40: the report of a last run that ended unexpectedly (engine/.../PostMortem.java) reads the game's log by
        // the path the manifest names; the game writes it to the user directory ("Penumbra", main.cpp's manifest
        // title) as penumbra.log, which on Android is files/Penumbra/. A renamed log or title would make the report
        // say "no game log" on a phone nobody can look inside.
        CHECK_MSG(Has(androidManifest, "android:name=\"supersonic.reportUnexpectedExit\" android:value=\"true\""),
                  "AndroidManifest.xml must opt in to the report of an unexpected end (E40)");
        CHECK_MSG(Has(androidManifest, "android:name=\"supersonic.reportLog\" android:value=\"Penumbra/penumbra.log\""),
                  "AndroidManifest.xml's reportLog must be the user directory's log: Penumbra/penumbra.log (E40)");
        CHECK_MSG(Has(androidManifest, "android:name=\"supersonic.reportSkipWhenFile\" android:value=\"penumbra_args.txt\""),
                  "AndroidManifest.xml's reportSkipWhenFile must be the development-flags file AndroidMain.cpp reads (E40)");
        const std::string main = ReadText(built / ".." / ".." / "game" / "main.cpp");
        CHECK_MSG(Has(main, "manifest.title = \"Penumbra\"") && Has(main, "\"penumbra.log\""),
                  "main.cpp must keep the manifest title Penumbra and the log name penumbra.log that the Android report reads (E40)");
        const std::string androidMain = ReadText(built / ".." / ".." / "game" / "android" / "AndroidMain.cpp");
        CHECK_MSG(Has(androidMain, "kArgsFile = \"penumbra_args.txt\""),
                  "AndroidMain.cpp's development-flags file must stay penumbra_args.txt, which the manifest's reportSkipWhenFile names (E40)");
        // E49: the unpack opens the same log before main.cpp does, so a start stopped during it leaves a log for the report.
        CHECK_MSG(Has(androidMain, "UserDataDirectory(\"Penumbra\")") && Has(androidMain, "\"penumbra.log\""),
                  "AndroidMain.cpp must open the log main.cpp's title and name give, for the unpack's lines (E49)");
    }
}

// ENHANCEMENT E49: THE UNPACK THAT A STOPPED START DOES NOT LOSE (eth/AssetUnpack.hpp). The APK is a map of paths to bytes
// behind the unpack's hooks; "a start that was stopped" is the event loop saying the app is closing, or a copy that fails
// halfway. Nothing here knows Android: the point is that every way of being stopped, at every file, leaves a state the next
// start finishes into exactly the tree a clean unpack makes, and that a tree of another build is never trusted.
struct FakeApk {
    std::string tag = "A";                        // the build: its bytes differ from another build's, its lengths need not
    std::map<std::string, std::string> assets;    // path -> bytes
    std::vector<std::string> list;                // the APK's list, in order (it may name an asset that is not there)
    int copies = 0;                               // files written whole, over every run
    int lengthQueries = 0;
    int stopAfter = -1;                           // keepGoing says no once it has been asked this many times
    int asked = 0;
    std::string cutAt;                            // a copy of this path writes half of it and fails
    std::vector<std::string> said;

    Penumbra::Eth::UnpackHooks Hooks() {
        Penumbra::Eth::UnpackHooks hooks;
        hooks.length = [this](const std::string& path) -> std::int64_t {
            ++lengthQueries;
            const auto it = assets.find(path);
            return it == assets.end() ? -1 : static_cast<std::int64_t>(it->second.size());
        };
        hooks.copy = [this](const std::string& path, const fs::path& target) {
            const auto it = assets.find(path);
            if (it == assets.end()) return false;
            std::ofstream out(target, std::ios::binary | std::ios::trunc);
            if (path == cutAt) {
                out << it->second.substr(0, it->second.size() / 2);
                return false;
            }
            out << it->second;
            ++copies;
            return static_cast<bool>(out);
        };
        hooks.keepGoing = [this] { return stopAfter < 0 || ++asked <= stopAfter; };
        hooks.log = [this](const std::string& line) { said.push_back(line); };
        return hooks;
    }
};

FakeApk MakeApk(const std::string& tag, int count) {
    FakeApk apk;
    apk.tag = tag;
    static const char* const kRoots[] = {"original", "data", "engine"};
    for (int i = 0; i < count; ++i) {
        const std::string path = std::string(kRoots[i % 3]) + "/folder" + std::to_string(i % 4) + "/file" + std::to_string(i) + ".bin";
        std::string bytes;
        for (int repeat = 0; repeat < (i % 5) + 1; ++repeat) bytes += tag + path + ";";   // lengths differ from file to file
        apk.assets[path] = bytes;
        apk.list.push_back(path);
    }
    return apk;
}

// Whether `files` holds exactly the APK's files and the stamp, and nothing of the unpack's marker.
std::string UnpackedTreeProblem(const fs::path& files, const FakeApk& apk, const std::string& stamp) {
    using namespace Penumbra::Eth;
    std::size_t found = 0;
    for (const char* root : {"original", "data", "engine"}) {
        std::error_code ec;
        if (!fs::exists(files / root, ec)) continue;
        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(files / root, ec)) {
            if (entry.is_regular_file()) ++found;
        }
    }
    if (found != apk.assets.size()) return "holds " + std::to_string(found) + " files, wants " + std::to_string(apk.assets.size());
    for (const auto& [path, bytes] : apk.assets) {
        if (ReadText(files / path) != bytes) return path + " differs";
    }
    if (ReadText(files / kUnpackStampFile) != stamp) return "the stamp is not " + stamp;
    if (fs::exists(files / kUnpackPartialFile)) return "the marker is still there";
    return {};
}

void TestAssetUnpack(const Layout& layout) {   // E49
    using namespace Penumbra::Eth;
    const std::vector<std::string> roots = {"original", "data", "engine"};
    const auto run = [&](const fs::path& files, FakeApk& apk, const std::string& stamp) {
        return UnpackAssets(files, stamp, apk.list, roots, apk.Hooks());
    };
    const auto fresh = [&](const std::string& name) {
        const fs::path files = layout.root / ("unpack_" + name);
        fs::remove_all(files);
        fs::create_directories(files);
        return files;
    };

    // A first start unpacks all of it, leaves the player's own folder alone, and the next start has nothing to do.
    {
        const fs::path files = fresh("first");
        Touch(files / "Penumbra" / "settings.json");
        FakeApk apk = MakeApk("A", 130);
        const UnpackResult first = run(files, apk, "stampA");
        CHECK(first.outcome == UnpackOutcome::Unpacked);
        CHECK(!first.resumed);
        CHECK_EQ(first.copied, std::size_t(130));
        CHECK_EQ(first.kept, std::size_t(0));
        CHECK_MSG(UnpackedTreeProblem(files, apk, "stampA").empty(), UnpackedTreeProblem(files, apk, "stampA"));
        CHECK_MSG(fs::exists(files / "Penumbra" / "settings.json"), "the player's folder is never touched");
        CHECK_MSG(apk.lengthQueries == 0, "a clean start trusts nothing on the disk, so it measures nothing");
        bool saidStart = false, saidProgress = false;
        for (const std::string& line : apk.said) {
            if (Has(line, "Unpacking 130 game files") && !Has(line, "continuing")) saidStart = true;
            if (Has(line, "Unpack: 64 of 130")) saidProgress = true;
        }
        CHECK_MSG(saidStart && saidProgress, "the log says what the unpack is doing, sparsely");
        const std::string described = DescribeUnpack(first);
        CHECK_MSG(Has(described, "unpacked 130 game files, 130 copied") && !Has(described, "kept"), described);

        const int copiesBefore = apk.copies;
        const UnpackResult second = run(files, apk, "stampA");
        CHECK(second.outcome == UnpackOutcome::UpToDate);
        CHECK_MSG(apk.copies == copiesBefore && apk.lengthQueries == 0, "a finished unpack is not touched again");
        CHECK_MSG(DescribeUnpack(second).empty(), "nothing is said of a start that unpacked nothing");
    }

    // The core of it: stopped after any number of the event loop's questions (inside the wipe, between any two files, after
    // the last one), then started again. The second start finishes into the clean tree, copies only what the first did not,
    // and is a continuation exactly when the first left its marker.
    for (int stop = 0; stop <= 46; ++stop) {
        const fs::path files = fresh("stop");
        FakeApk apk = MakeApk("A", 40);
        apk.stopAfter = stop;
        const UnpackResult stopped = run(files, apk, "stampA");
        const bool markerLeft = fs::exists(files / kUnpackPartialFile);
        const bool stampLeft = fs::exists(files / kUnpackStampFile) && ReadText(files / kUnpackStampFile) == "stampA";
        const int copiedByFirst = apk.copies;
        apk.stopAfter = -1;
        const UnpackResult resumed = run(files, apk, "stampA");

        std::string problem;
        if (stopped.outcome == UnpackOutcome::Cancelled) {
            if (stampLeft) problem += "a stopped start must not leave the stamp; ";
            if (resumed.outcome != UnpackOutcome::Unpacked) problem += "second start did not unpack; ";
            if (resumed.resumed != markerLeft) problem += "resumed must follow the marker; ";
            if (resumed.kept != static_cast<std::size_t>(copiedByFirst)) problem += "kept != what the first copied; ";
            if (apk.copies != 40) problem += "every file must be copied exactly once over both starts, got " + std::to_string(apk.copies) + "; ";
        } else {
            if (stopped.outcome != UnpackOutcome::Unpacked) problem += "a start that was not stopped must finish; ";
            if (resumed.outcome != UnpackOutcome::UpToDate) problem += "after a finished start: up to date; ";
        }
        const std::string tree = UnpackedTreeProblem(files, apk, "stampA");
        if (!tree.empty()) problem += tree + "; ";
        CHECK_MSG(problem.empty(), "stopped after " + std::to_string(stop) + " questions: " + problem);
    }

    // A copy that fails halfway (no room left, or a process killed inside a file): the half file is gone, the others stay,
    // and the retry copies that file and the rest, not the ones already whole.
    {
        const fs::path files = fresh("cut");
        FakeApk apk = MakeApk("A", 40);
        apk.cutAt = apk.list[17];
        const UnpackResult failed = run(files, apk, "stampA");
        CHECK(failed.outcome == UnpackOutcome::Failed);
        CHECK_MSG(Has(failed.why, apk.list[17]), "the failure names the file: " + failed.why);
        CHECK_MSG(!fs::exists(files / apk.list[17]), "the half-written file is removed");
        CHECK_MSG(fs::exists(files / apk.list[16]) && !fs::exists(files / kUnpackStampFile) && fs::exists(files / kUnpackPartialFile),
                  "earlier files stay, there is no stamp, the marker stays");
        apk.cutAt.clear();
        const UnpackResult retry = run(files, apk, "stampA");
        CHECK(retry.outcome == UnpackOutcome::Unpacked && retry.resumed);
        CHECK_EQ(retry.kept, std::size_t(17));
        CHECK_EQ(retry.copied, std::size_t(23));
        CHECK_MSG(UnpackedTreeProblem(files, apk, "stampA").empty(), UnpackedTreeProblem(files, apk, "stampA"));
        const std::string described = DescribeUnpack(retry);
        CHECK_MSG(Has(described, "23 copied") && Has(described, "17 kept from a start that was cut short"), described);
    }

    // A file left under its name by a process that was killed inside it (shorter than its asset, or longer: anything but
    // the asset's length) is copied again, and a whole one beside it is not.
    {
        const fs::path files = fresh("killed");
        FakeApk apk = MakeApk("A", 40);
        apk.stopAfter = 20;
        run(files, apk, "stampA");
        CHECK(fs::exists(files / kUnpackPartialFile));
        // The edges of "as long as its asset": a stub of 5 bytes, 0 bytes (stopped between the open and the first write: the
        // likeliest state of a copy that was stopped), one byte short, one byte long, and 5 bytes long.
        const auto damage = [&](int index, const std::string& bytes) {
            std::ofstream out(files / apk.list[static_cast<std::size_t>(index)], std::ios::binary | std::ios::trunc);
            out << bytes;
        };
        const auto asset = [&](int index) { return apk.assets[apk.list[static_cast<std::size_t>(index)]]; };
        damage(3, asset(3).substr(0, 5));
        damage(4, asset(4) + "extra");
        damage(5, "");
        damage(6, asset(6).substr(0, asset(6).size() - 1));
        damage(7, asset(7) + "x");
        apk.stopAfter = -1;
        const int copiesBefore = apk.copies;
        const UnpackResult resumed = run(files, apk, "stampA");
        CHECK(resumed.outcome == UnpackOutcome::Unpacked);
        CHECK_MSG(UnpackedTreeProblem(files, apk, "stampA").empty(), UnpackedTreeProblem(files, apk, "stampA"));
        CHECK_MSG(apk.copies - copiesBefore == 40 - 17 + 5, "the five damaged files are copied again with the 23 not yet copied");
    }

    // Another build: never continued. A tree stopped halfway and then an APK of the SAME file sizes but other bytes, a file
    // more and a file less: every file is the new build's, none of the old build's remains.
    {
        const fs::path files = fresh("update");
        FakeApk oldApk = MakeApk("A", 40);
        oldApk.assets["original/old_only.bin"] = "gone in the next build";
        oldApk.list.insert(oldApk.list.begin() + 10, "original/old_only.bin");   // among the 27 files the stopped start copies
        oldApk.stopAfter = 30;
        CHECK(run(files, oldApk, "stampA").outcome == UnpackOutcome::Cancelled);
        CHECK_MSG(fs::exists(files / "original" / "old_only.bin"), "the old build's own file is on the disk before the update");
        FakeApk newApk = MakeApk("B", 40);   // "B" and "A" are one byte each: every length is the old build's
        newApk.assets["data/new_only.bin"] = "arrives in this build";
        newApk.list.push_back("data/new_only.bin");
        CHECK_EQ(newApk.assets[newApk.list[5]].size(), oldApk.assets[oldApk.list[5]].size());
        const UnpackResult updated = run(files, newApk, "stampB");
        CHECK(updated.outcome == UnpackOutcome::Unpacked);
        CHECK_MSG(!updated.resumed && updated.kept == 0, "a tree of another stamp is never continued");
        CHECK_MSG(UnpackedTreeProblem(files, newApk, "stampB").empty(), UnpackedTreeProblem(files, newApk, "stampB"));
        CHECK_MSG(!fs::exists(files / "original" / "old_only.bin"), "a file the new build lacks does not stay behind");

        // A finished older build updated: the same.
        FakeApk finished = MakeApk("A", 40);
        const fs::path done = fresh("update_done");
        CHECK(run(done, finished, "stampA").outcome == UnpackOutcome::Unpacked);
        const UnpackResult over = run(done, newApk, "stampB");
        CHECK(over.outcome == UnpackOutcome::Unpacked && !over.resumed);
        CHECK_MSG(UnpackedTreeProblem(done, newApk, "stampB").empty(), UnpackedTreeProblem(done, newApk, "stampB"));
    }

    // A start of another build stopped INSIDE its wipe, over a finished tree of the build before: some of the old files are gone and
    // the rest are still there. The marker must not exist yet (else the restart would "continue" over the old build's files,
    // keeping the ones whose size happens to match), and the old stamp must already be gone (else going back to the old build
    // would be "up to date" over a gutted tree).
    for (int q = 0; q <= 3; ++q) {
        const bool wipeOver = q >= 3;   // three roots, so three questions are the wipe's
        FakeApk a = MakeApk("A", 40);
        FakeApk b = MakeApk("B", 40);   // the same lengths, other bytes

        const fs::path forward = fresh("wipecut_forward");
        std::string problem;
        if (run(forward, a, "stampA").outcome != UnpackOutcome::Unpacked) problem += "A did not unpack; ";
        b.stopAfter = q;
        if (run(forward, b, "stampB").outcome != UnpackOutcome::Cancelled) problem += "B was not stopped; ";
        if (fs::exists(forward / kUnpackStampFile)) problem += "the old stamp survived the start of the wipe; ";
        if (fs::exists(forward / kUnpackPartialFile) != wipeOver) problem += "the marker must exist exactly when the wipe is over; ";
        b.stopAfter = -1;
        const UnpackResult again = run(forward, b, "stampB");
        if (again.outcome != UnpackOutcome::Unpacked || again.resumed != wipeOver) problem += "B's restart; ";
        const std::string tree = UnpackedTreeProblem(forward, b, "stampB");
        if (!tree.empty()) problem += "B's tree: " + tree + "; ";
        CHECK_MSG(problem.empty(), "B stopped after " + std::to_string(q) + " questions of its wipe: " + problem);

        const fs::path backward = fresh("wipecut_backward");
        FakeApk a2 = MakeApk("A", 40);
        FakeApk b2 = MakeApk("B", 40);
        problem.clear();
        if (run(backward, a2, "stampA").outcome != UnpackOutcome::Unpacked) problem += "A did not unpack; ";
        b2.stopAfter = q;
        run(backward, b2, "stampB");
        const UnpackResult back = run(backward, a2, "stampA");   // the player is back on the old build
        if (back.outcome != UnpackOutcome::Unpacked) problem += "going back to A was not an unpack; ";
        const std::string oldTree = UnpackedTreeProblem(backward, a2, "stampA");
        if (!oldTree.empty()) problem += "A's tree: " + oldTree + "; ";
        CHECK_MSG(problem.empty(), "back to A after B stopped at " + std::to_string(q) + ": " + problem);
    }

    // The stamp cannot be written (no room, the rename refused) when every file is in place: the marker stays, so the next start
    // finds the tree whole in one pass and writes the stamp, instead of wiping 21 MB to copy it again.
    {
        const fs::path files = fresh("stampfail");
        FakeApk apk = MakeApk("A", 40);
        apk.stopAfter = 20;
        CHECK(run(files, apk, "stampA").outcome == UnpackOutcome::Cancelled);
        apk.stopAfter = -1;
        const fs::path blocker = files / (std::string(kUnpackStampFile) + ".tmp");
        fs::create_directories(blocker);   // WriteFileAtomic cannot open its temporary
        const UnpackResult failed = run(files, apk, "stampA");
        CHECK(failed.outcome == UnpackOutcome::Failed);
        CHECK_MSG(fs::exists(files / kUnpackPartialFile) && !fs::exists(files / kUnpackStampFile),
                  "a failed stamp leaves the marker and no stamp");
        fs::remove_all(blocker);
        const UnpackResult again = run(files, apk, "stampA");
        CHECK_MSG(again.outcome == UnpackOutcome::Unpacked && again.resumed && again.copied == 0 && again.kept == 40,
                  "the next start only writes the stamp: copied " + std::to_string(again.copied) + ", kept " + std::to_string(again.kept));
        CHECK_MSG(UnpackedTreeProblem(files, apk, "stampA").empty(), UnpackedTreeProblem(files, apk, "stampA"));
    }

    // An asset the list names and the APK does not have: a failure that names it, the rest kept; found later, it finishes.
    {
        const fs::path files = fresh("missing");
        FakeApk apk = MakeApk("A", 12);
        apk.list.push_back("original/absent.bin");
        const UnpackResult failed = run(files, apk, "stampA");
        CHECK(failed.outcome == UnpackOutcome::Failed && Has(failed.why, "original/absent.bin"));
        CHECK(!fs::exists(files / kUnpackStampFile));
        apk.assets["original/absent.bin"] = "found";
        CHECK(run(files, apk, "stampA").outcome == UnpackOutcome::Unpacked);
        CHECK_MSG(UnpackedTreeProblem(files, apk, "stampA").empty(), UnpackedTreeProblem(files, apk, "stampA"));
    }

    // A list with no stamp cannot tell a finished tree from none: nothing is written. And a start with no way to stop it or
    // to say anything (no hooks for either) still unpacks.
    {
        const fs::path files = fresh("nostamp");
        FakeApk apk = MakeApk("A", 5);
        CHECK(run(files, apk, "").outcome == UnpackOutcome::Failed);
        CHECK(fs::is_empty(files));
        UnpackHooks bare = apk.Hooks();
        bare.keepGoing = nullptr;
        bare.log = nullptr;
        CHECK(UnpackAssets(files, "stampA", apk.list, roots, bare).outcome == UnpackOutcome::Unpacked);
        CHECK_MSG(UnpackedTreeProblem(files, apk, "stampA").empty(), UnpackedTreeProblem(files, apk, "stampA"));
    }
}

// ENHANCEMENT E39: WHAT A PHONE IS TOLD when the game cannot start. Before, an Android start that failed (the Oppo A54
// and A74 5G, whose drivers report Vulkan 1.1) showed a black screen and the launcher. Here the text, in both languages:
// it names what the game needs of the phone, carries the GPU and version the engine reported, says where to write, and
// has none of the desktop's steps (a zip, Explorer, Windows Update). The dialog itself is the activity's.
void TestPhoneStartupMessages() {   // E39
    using namespace Penumbra::Eth;
    const std::string reported = "Failed to find a suitable Vulkan physical GPU! Adreno (TM) 619: reports Vulkan 1.0, the game needs 1.1 or newer. ";
    for (const bool portugueseFirst : {false, true}) {
        const std::string graphics = StartupMessage(StartupProblem::NoGraphics, portugueseFirst, reported, "", true);
        CHECK(ValidUtf8(graphics));
        CHECK(Has(graphics, "Vulkan 1.1"));
        CHECK_MSG(Has(graphics, "phone") && Has(graphics, "celular"), "worded for a phone, in both languages");
        CHECK_MSG(Has(graphics, "\n\nDetails: " + reported), "the GPU and the version it reported are shown: " + graphics);
        // The details come right after the first language, before the rule and the second: a small dialog
        // scrolls, and the line must be on the first screen of a screenshot.
        CHECK_MSG(graphics.find("Details: ") < graphics.find("----------------"), "details before the rule: " + graphics);
        // The words of the Portuguese: "voc\xC3\xAA" is e with a circumflex, never "voc\xC3\xA9".
        CHECK(!Has(graphics, "voc\xC3\xA9"));
        CHECK_MSG(Has(graphics, "send a screenshot") && Has(graphics, "envie uma captura"), "asks for a screenshot, in both languages");
        // Hedged: it must not claim the phone lacks Vulkan 1.1 for every failure.
        CHECK(!Has(graphics, "does not offer"));
        CHECK(Has(graphics, "github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/issues"));
        CHECK_MSG(!Has(graphics, "Windows") && !Has(graphics, "NVIDIA") && !Has(graphics, "Explorer"), "none of the desktop's steps");
        CHECK_MSG(!Has(graphics, "Log:"), "a phone's log cannot be opened by a player: none is offered");
        // The same problem on a desktop is the desktop's text, unchanged.
        const std::string desktop = StartupMessage(StartupProblem::NoGraphics, portugueseFirst, "", "", false);
        CHECK_MSG(Has(desktop, "NVIDIA") && Has(desktop, "Vulkan 1.2"), "a desktop keeps its own wording");
    }
    const std::string files = StartupMessage(StartupProblem::GameFilesMissing, false, "", "", true);
    CHECK(Has(files, "storage") && Has(files, "armazenamento"));
    CHECK_MSG(!Has(files, "zip") && !Has(files, "Penumbra.exe") && !Has(files, "Extract"), "no desktop steps for a phone");
    // What has no phone wording reads as the desktop's: an unexpected error is the same on any device.
    CHECK(Has(StartupMessage(StartupProblem::StoppedByError, false, "", "", true), "unexpected error"));
}

// ENHANCEMENT E38: A START THAT DID NOT FINISH (eth/StartupErrors.hpp). The marker, what reads it, and the log kept
// beside the next one, asked of folders made in the temp directory. main.cpp only calls these; the layer's side (when
// the marker goes) is checked in test_pn_render_hud.
void TestStartRecord(const Layout& layout) {   // E38
    using namespace Penumbra::Eth;
    const fs::path user = layout.root / "start_record_user";
    fs::create_directories(user);
    const fs::path marker = user / kStartMarkerFile;

    // A first start: a marker to write, nothing found, and reading wrote nothing.
    const StartRecord first = ReadStartRecord(user, false, false);
    CHECK_MSG(first.marker == marker, "the marker is in the user folder");
    CHECK(!first.lastUnfinished);
    CHECK_MSG(!fs::exists(marker), "reading the record writes nothing");
    CHECK(!OpenWindowedAfterUnfinishedStart(first, false));
    CHECK(BeginStartRecord(first));
    CHECK(fs::exists(marker));

    // The next start finds it: a window this once, unless the player named a mode.
    const StartRecord second = ReadStartRecord(user, false, false);
    CHECK(second.lastUnfinished);
    CHECK(OpenWindowedAfterUnfinishedStart(second, false));
    CHECK_MSG(!OpenWindowedAfterUnfinishedStart(second, true), "--fullscreen, --windowed and --window are kept");

    // A capture or a test run (headless), a phone and a start with no user folder keep no marker and do not judge
    // the last start, though the file is there; and they cannot take it away.
    for (const StartRecord& other : {ReadStartRecord(user, true, false), ReadStartRecord(user, false, true),
                                     ReadStartRecord(fs::path{}, false, false)}) {
        CHECK(other.marker.empty());
        CHECK(!other.lastUnfinished);
        CHECK(!OpenWindowedAfterUnfinishedStart(other, false));
        CHECK(!BeginStartRecord(other));
        EndStartRecord(other);
        CHECK_MSG(fs::exists(marker), "a run that keeps no marker leaves a player's alone");
    }

    // A folder that cannot be written to gives no marker (the start goes on without one).
    CHECK(!BeginStartRecord(ReadStartRecord(user / "nowhere", false, false)));

    // A failure the player was told of takes it away.
    EndStartRecord(second);
    CHECK(!fs::exists(marker));
    EndStartRecord(second);   // and again: nothing to take

    // The log of the start that did not finish is kept, replacing an older one; there is nothing to keep twice.
    std::ofstream(user / "penumbra.log", std::ios::binary) << "the start that froze";
    std::ofstream(user / kUnfinishedLogFile, std::ios::binary) << "an older one";
    CHECK(KeepUnfinishedLog(user));
    CHECK(!fs::exists(user / "penumbra.log"));
    CHECK_MSG(ReadText(user / kUnfinishedLogFile) == "the start that froze", "the older kept log is replaced");
    CHECK(!KeepUnfinishedLog(user));
    CHECK(!KeepUnfinishedLog(fs::path{}));
    CHECK_MSG(ReadText(user / kUnfinishedLogFile) == "the start that froze", "and a second call changes nothing");
}

// ENHANCEMENT E38: THE LOG'S TIMES. main.cpp turns them on and docs/playing.md promises the form; the engine's own
// suite checks the clock, this one the contract the game relies on (this suite is in the game's gate, which builds
// no engine suite): "INFO +1.234s [Category] message", the time rising, and nothing stamped when it is off.
void TestLogTimes(const Layout& layout) {   // E38
    const fs::path file = layout.root / "log_times.txt";
    CHECK(Supersonic::Log::SetFileSink(file.string()));
    Supersonic::Log::SetElapsedTimestamps(true);
    Supersonic::Log::Submit(Supersonic::Log::Level::Info, "PnProbe", "one");
    std::this_thread::sleep_for(std::chrono::milliseconds(70));
    Supersonic::Log::Submit(Supersonic::Log::Level::Info, "PnProbe", "two");
    Supersonic::Log::SetElapsedTimestamps(false);
    Supersonic::Log::Submit(Supersonic::Log::Level::Info, "PnProbe", "three");
    Supersonic::Log::CloseFileSink();

    std::vector<std::string> lines;
    {
        std::istringstream in(ReadText(file));
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.find("[PnProbe]") != std::string::npos) lines.push_back(line);
        }
    }
    CHECK_MSG(lines.size() == 3, "three lines were logged, got " + std::to_string(lines.size()));
    if (lines.size() != 3) return;
    const std::regex stamped(R"(INFO \+([0-9]+\.[0-9]{3})s \[PnProbe\] (one|two))");
    std::smatch a;
    std::smatch b;
    CHECK_MSG(std::regex_match(lines[0], a, stamped), "stamped: " + lines[0]);
    CHECK_MSG(std::regex_match(lines[1], b, stamped), "stamped: " + lines[1]);
    CHECK_MSG(lines[2] == "INFO [PnProbe] three", "off again is the old form: " + lines[2]);
    if (a.size() == 3 && b.size() == 3) {
        CHECK_MSG(std::stod(b[1]) - std::stod(a[1]) >= 0.04, "the second line is later by about the 70 ms slept");
    }
}

} // namespace

int main() {
    const Layout layout = MakeLayout();
    TestMarkers(layout);
    TestOriginal(layout);
    TestData(layout);
    TestRunFromInsideTheZip(layout);
    TestStartupErrors();
    TestPhoneStartupMessages();   // E39
    TestStartRecord(layout);   // E38
    TestAssetUnpack(layout);   // E49
    TestLogTimes(layout);   // E38
    TestWorkingDirectory(layout);
    TestDescriptions();
    TestAssetResolver(layout);
    TestRepository();
    std::error_code ec;
    fs::remove_all(layout.root, ec);
    return test::summary("test_pn_paths", 300);
}
