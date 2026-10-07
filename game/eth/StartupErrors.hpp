#pragma once

// WHAT A PLAYER IS TOLD when the game cannot start, or stops on an error
// (main.cpp).
//
// Penumbra.exe has no console (game/CMakeLists.txt: WIN32_EXECUTABLE), so a
// player who starts it from Explorer never sees stderr: before these, a failed
// start looked like nothing at all - the game just did not open. The two
// likeliest failures for someone who downloaded the game are:
//
//   - running it from INSIDE the zip. Explorer shows a zip as a folder and, on
//     a double-click, extracts only the clicked exe into a temporary folder, so
//     original/ and data/ are not beside it (eth/Paths.hpp) and nothing is at
//     the build's baked path on a player's machine;
//   - a PC without a usable Vulkan driver: no vulkan-1.dll at all (Penumbra.exe
//     delay-loads it so that it gets as far as saying so), or a window or
//     device the engine cannot create.
//
// For those, a folder name the code page cannot spell, and an error that stops
// a running game, a message box in English and Portuguese, the system's
// language first.
//
// ONLY WHEN NOBODY ELSE IS LISTENING. A headless capture (--frames,
// --screenshot, --fixed-step) or a run whose stderr goes to a pipe or a file (a
// shell, a script, an agent) keeps the stderr line alone, exactly as before:
// automation must never meet a modal window it cannot close.
//
// In the Eth library for the reason Paths.hpp is: the lowest one every suite
// links, so test_pn_paths checks the texts and the rule without opening a
// window. ShowStartupDialog is the one function a suite must never call.

#include <filesystem>
#include <string>

namespace Penumbra::Eth {

enum class StartupProblem {
    GameFilesMissing,     // no original/ (data.enml) beside the executable nor at the build's path,
                          // or no engine shaders anywhere: the game was not extracted
    FolderNameUnusable,   // the folder's name has characters this system's code page cannot spell
    NoGraphics,           // no vulkan-1.dll, or the window or the Vulkan device could not be made
    StoppedByError,       // an exception out of a game that had started
};

// Where this process's stderr goes.
enum class ErrorStream {
    Nowhere,   // no handle: a GUI program started from Explorer, a shortcut, the Start menu
    Console,   // a console (nobody reads a GUI program's console output reliably)
    Pipe,      // a shell, a script, a test harness
    File,      // redirected to a file
};

// The stream stderr is attached to now. Windows asks GetFileType; elsewhere a
// terminal is Console and anything else a Pipe (no dialog is ever shown there).
ErrorStream CurrentErrorStream();

// Whether to show a message box: never for a headless run, never when stderr
// goes to a pipe or a file, where a person or a program already reads it.
bool ShouldShowStartupDialog(bool headless, ErrorStream stderrStream);

// The box's text, UTF-8: the system's language first, a rule, then the other.
// `detail` (an exception's what(), a folder; may be empty) and `logPath` (may
// be empty) follow once, untranslated, for whoever is asked to help.
std::string StartupMessage(StartupProblem problem, bool portugueseFirst, const std::string& detail,
                           const std::string& logPath);

// The box's caption.
inline constexpr const char* kStartupDialogTitle = "Penumbra";

// Whether the Vulkan loader (vulkan-1.dll) can be loaded. Windows only; true
// elsewhere, where the executable links the loader outright. Call it before
// anything calls Vulkan: Penumbra.exe delay-loads the DLL, and without it the
// first Vulkan call would end the process.
bool VulkanLoaderAvailable();

// A modal message box with `utf8Text`, the caption above and an error icon, on
// Windows; nothing elsewhere. Returns when the player closes it.
void ShowStartupDialog(const std::string& utf8Text);

// ---- A start that did not finish (E38) -------------------------------------------------------------------------
//
// A window that stops answering is ended by the player, a PC loses power, the game crashes before it draws: the
// next start should not do again what the last one did not survive. A normal desktop start writes a marker in the
// user folder before the engine starts (BeginStartRecord), and the layer takes it away once the first frame is
// drawn and the display switch the launch asked for has been applied (PenumbraLayer::FinishLaunch). A start that
// finds it was ended before that, so it opens in a window this once (the fullscreen setting is not changed), and
// the log of the start that did not finish is kept beside the new one (KeepUnfinishedLog): that log, with its
// times, is the evidence of where the time went.
//
// Pure file operations with no engine behind them, so test_pn_paths drives them in a folder of its own. A
// headless run (a capture, a test), a phone and a start with no user folder keep no marker at all.
inline constexpr const char* kStartMarkerFile = "start-unfinished";
inline constexpr const char* kUnfinishedLogFile = "penumbra-unfinished.log";

struct StartRecord {
    // Where this start's marker goes: empty when this start keeps none.
    std::filesystem::path marker;
    // The marker was there when this start began: the last start did not finish.
    bool lastUnfinished = false;
};

// Reads the folder; writes nothing. `userDir` empty, `headless` or `mobile`: no marker, and the last start is not
// judged (a capture must neither inherit nor leave a player's state).
StartRecord ReadStartRecord(const std::filesystem::path& userDir, bool headless, bool mobile);

// Whether this start opens in a window instead of covering the monitor: the last one did not finish, and the
// player named no mode of their own (--fullscreen, --windowed, --window).
bool OpenWindowedAfterUnfinishedStart(const StartRecord& record, bool playerNamedAMode);

// penumbra.log of the start that did not finish becomes penumbra-unfinished.log (an older one is replaced); the
// new start's log would otherwise replace it. Before the log file is opened. False when there was no log to keep.
bool KeepUnfinishedLog(const std::filesystem::path& userDir);

// Writes the marker; true when it exists afterwards. EndStartRecord takes it away (a start that failed and told
// the player is not an unfinished one).
bool BeginStartRecord(const StartRecord& record);
void EndStartRecord(const StartRecord& record);

} // namespace Penumbra::Eth
