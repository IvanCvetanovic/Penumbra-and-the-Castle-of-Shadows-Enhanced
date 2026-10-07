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
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <system_error>
#include <vector>

#include "TestHarness.hpp"
#include "core/Log.hpp"   // E38
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
    }
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
    TestStartRecord(layout);   // E38
    TestLogTimes(layout);   // E38
    TestWorkingDirectory(layout);
    TestDescriptions();
    TestAssetResolver(layout);
    TestRepository();
    std::error_code ec;
    fs::remove_all(layout.root, ec);
    return test::summary("test_pn_paths", 140);
}
