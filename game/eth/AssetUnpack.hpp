#pragma once

// ENHANCEMENT E49 (not in the original, which read its files straight from its folder): the first start on a phone unpacks the
// game's files out of the APK (game/android/AndroidMain.cpp), and a start that is cut short does not have to begin again.
//
// Before, the unpack wiped what it owned, copied all of it, and wrote the stamp that says "done" last, so a process stopped
// anywhere in between left nothing the next start could use, and the next start deleted the partial tree and copied it all
// again: on a phone whose system stops the game early on its first starts (a Galaxy Tab A7 Lite's Samsung stop, "due to SPEG",
// came with five tries before one got through) every try repeated the same work and could be stopped at the same point.
//
// Now a marker file holds the stamp of the unpack in progress. A start that finds the marker equal to the APK's stamp is
// continuing that very unpack: every file it finds whole (its size is the asset's length) is kept and only the rest is
// copied, so each short life adds to the last. Any other state (no marker, another build's stamp, an interrupted wipe) is a
// clean start that deletes everything the unpack owns first, which is what an APK update needs: the stamp is a hash of the
// files' paths and bytes, so only a tree of the SAME stamp may be trusted, and a file whose size alone matches across two
// builds is never kept.
//
// A file is written under its own name, not under a temporary one: a copy that was stopped is shorter than its asset, so the
// size check recopies it, and one that was not stopped is whole, which is all a rename would have said. Nothing is synced:
// a stopped process loses none of what it wrote (the kernel has it), and a sync per file would cost seconds on a phone's flash.
//
// Portable (the APK is behind hooks), so a computer's test suite runs the whole of it (tests/test_pn_paths.cpp).

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "eth/AtomicWrite.hpp"

namespace Penumbra::Eth {

// In the folder the files are unpacked into. The stamp is written last and says the tree is whole; the marker is written
// first and says which unpack the tree belongs to.
inline constexpr const char* kUnpackStampFile = "penumbra_assets.stamp";
inline constexpr const char* kUnpackPartialFile = "penumbra_assets.partial";

// What the unpack asks of its surroundings: the APK, the event loop, the log.
struct UnpackHooks {
    // The length in bytes of an asset, or a negative number when there is no such asset. Asked only about a file that is
    // already on the disk, to decide whether it is whole.
    std::function<std::int64_t(const std::string& path)> length;
    // Writes the whole asset to `target` (its folder exists). False when it could not be read or written.
    std::function<bool(const std::string& path, const std::filesystem::path& target)> copy;
    // Called before each file and between the folders of a wipe, so the activity keeps reading its events (a thread that
    // stops reading for five seconds is "not responding"). False once the app is being closed: the unpack then stops where
    // it is and keeps what it has. Absent: never stops.
    std::function<bool()> keepGoing;
    // One line of the log. Absent: nothing is said.
    std::function<void(const std::string& line)> log;
};

enum class UnpackOutcome {
    UpToDate,    // the stamp matched: nothing to do
    Unpacked,    // every file is in place and the stamp is written
    Cancelled,   // the app is closing: what was copied stays for the next start
    Failed,      // a file could not be unpacked (`why` says which): what was copied stays too
};

struct UnpackResult {
    UnpackOutcome outcome = UnpackOutcome::Failed;
    bool resumed = false;        // continued an unpack that an earlier start cut short
    std::size_t total = 0;       // files in the list
    std::size_t kept = 0;        // already whole from an earlier start
    std::size_t copied = 0;      // written by this one
    double seconds = 0.0;
    std::string why;             // for Failed: what went wrong
};

namespace unpack_detail {

// The file's bytes, or nothing when it is missing or unreadable. Never throws (a stamp that is a folder must not end the start).
inline std::string ReadWhole(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return {};
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

inline std::string Seconds(double value, int decimals) {
    char text[32];
    std::snprintf(text, sizeof text, "%.*f", decimals, value);
    return text;
}

}   // namespace unpack_detail

// The whole of the unpack. `stamp` and `paths` are the APK's list (the stamp changes whenever any packed file does); `roots`
// are the folders under `files` that the unpack owns (everything in them is the unpack's to delete; the player's own folder
// sits beside them and is never touched).
inline UnpackResult UnpackAssets(const std::filesystem::path& files, const std::string& stamp,
                                 const std::vector<std::string>& paths, const std::vector<std::string>& roots,
                                 const UnpackHooks& hooks) {
    namespace fs = std::filesystem;
    using unpack_detail::ReadWhole;

    UnpackResult result;
    result.total = paths.size();
    const auto started = std::chrono::steady_clock::now();
    const auto seconds = [&started] {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    };
    const auto say = [&hooks](const std::string& line) {
        if (hooks.log) hooks.log(line);
    };
    const auto proceed = [&hooks] { return !hooks.keepGoing || hooks.keepGoing(); };
    const auto fail = [&](const std::string& why) {
        result.outcome = UnpackOutcome::Failed;
        result.why = why;
        result.seconds = seconds();
        say("Unpack failed: " + why);
        return result;
    };
    const auto cancelled = [&] {
        result.outcome = UnpackOutcome::Cancelled;
        result.seconds = seconds();
        say("Unpack stopped, the app is closing: " + std::to_string(result.kept + result.copied) + " of " +
            std::to_string(result.total) + " files are in place for the next start.");
        return result;
    };

    // An empty stamp could not tell a finished tree from a missing one.
    if (stamp.empty()) return fail("the asset list has no stamp");

    const fs::path stampPath = files / kUnpackStampFile;
    const fs::path partialPath = files / kUnpackPartialFile;
    if (ReadWhole(stampPath) == stamp) {
        result.outcome = UnpackOutcome::UpToDate;
        return result;
    }

    // Only a tree of this very stamp may be continued. Anything else (no marker, another build's stamp, a wipe that was cut
    // short before the marker was written) is cleared first, in an order that leaves no state a later start could mistake:
    // the stamp and the marker go first (so a cut here is a clean start again), the marker comes back last.
    const bool resume = ReadWhole(partialPath) == stamp;
    result.resumed = resume;
    std::error_code ec;
    if (!resume) {
        fs::remove(stampPath, ec);
        fs::remove(partialPath, ec);
        for (const std::string& root : roots) {
            fs::remove_all(files / root, ec);
            if (!proceed()) return cancelled();
        }
        std::string why;
        fs::create_directories(files, ec);
        if (!WriteFileAtomic(partialPath, stamp, why)) return fail(why);
    }
    say(std::string("Unpacking ") + std::to_string(paths.size()) + " game files into " + files.string() +
        (resume ? ", continuing an unpack that an earlier start cut short." : "."));

    fs::path lastFolder;
    std::size_t processed = 0;
    for (const std::string& path : paths) {
        if (!proceed()) return cancelled();
        const fs::path target = files / path;

        // A file of the same stamp is whole when it is as long as its asset: a copy that was stopped is shorter.
        bool whole = false;
        if (resume && hooks.length) {
            const std::uintmax_t size = fs::file_size(target, ec);   // an error for a missing file and for a folder
            if (!ec) {
                const std::int64_t want = hooks.length(path);
                whole = want >= 0 && static_cast<std::uintmax_t>(want) == size;
            }
            ec.clear();
        }
        if (whole) {
            ++result.kept;
        } else {
            const fs::path folder = target.parent_path();
            if (folder != lastFolder) {
                fs::create_directories(folder, ec);
                lastFolder = folder;
            }
            if (!hooks.copy || !hooks.copy(path, target)) {
                // A half-written file left under its name would only be recopied; it is removed to give its room back.
                fs::remove(target, ec);
                return fail("could not unpack " + path);
            }
            ++result.copied;
        }
        // Sparse on purpose: each line is flushed, and the last one is what a report of a stopped start shows.
        if (++processed % 64 == 0) {
            say("Unpack: " + std::to_string(processed) + " of " + std::to_string(result.total) + " files (" +
                std::to_string(result.copied) + " copied, " + std::to_string(result.kept) + " kept), " +
                unpack_detail::Seconds(seconds(), 1) + " s.");
        }
    }

    // Every file is in place: from here the stamp is all that is missing, so a cancel is no longer asked.
    std::string why;
    if (!WriteFileAtomic(stampPath, stamp, why)) return fail(why);
    fs::remove(partialPath, ec);
    result.outcome = UnpackOutcome::Unpacked;
    result.seconds = seconds();
    return result;
}

// One line for the log of a start that unpacked: how much was copied and how long it took. Empty for any other outcome.
inline std::string DescribeUnpack(const UnpackResult& result) {
    if (result.outcome != UnpackOutcome::Unpacked) return {};
    std::string line = "First start: unpacked " + std::to_string(result.total) + " game files, " +
                       std::to_string(result.copied) + " copied";
    if (result.resumed) line += ", " + std::to_string(result.kept) + " kept from a start that was cut short";
    return line + ", in " + unpack_detail::Seconds(result.seconds, 2) + " s.";
}

// What the unpack of this process said about itself, for the game's own log (main.cpp writes it once that log is open).
inline std::string& LastUnpackSummary() {
    static std::string summary;
    return summary;
}

}   // namespace Penumbra::Eth
