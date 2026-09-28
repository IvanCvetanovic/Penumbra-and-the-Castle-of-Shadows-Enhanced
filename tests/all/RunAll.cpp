// test_pn_all: every test_pn_* suite in one executable, beside the suites' own
// executables, which stay exactly as they were and which ctest keeps running.
//
// Smart App Control judges a freshly linked executable when it is first
// launched, and a build that relinks sixteen suites gives it sixteen chances to
// refuse one (test_pn_boot and test_pn_formats were refused build after
// build). Launching this one instead is one judgement per build.
//
// Every suite still runs in a process of its own: a child of this same
// executable, the same file, so the verdict this launch already got. The
// ported script's globals (g_frameTimers, g_camera, g_gameData, the options
// screen's Switches) live for the whole program, as they lived for the whole of
// machine.exe, and nothing re-initialises them for a new Machine. test_pn_boot,
// test_pn_render_interp and test_pn_scenarios each boot the real game; in one
// process each would inherit the game the one before it left. A process each
// makes a suite's run here the run its own executable makes, a crash included.
//
//   test_pn_all                         every suite, a child each, in name order
//   test_pn_all --suite boot            that suite, in this process: exactly its
//                                       own executable's run (test_pn_ optional)
//   test_pn_all --suite boot --suite formats    those suites, a child each
//   test_pn_all --list                  the suites it holds; nothing runs
//   --timeout <seconds>                 a child still running then is killed and
//                                       failed (default 1500, ctest's; 0: never)
//
// Each suite prints its own lines and its own summary line, unchanged. Exit:
// 0 when nothing failed; 1 when a suite failed, crashed, timed out or could not
// be started; 77 when every suite skipped (a suite's own "skipped"); 2 for a
// command line it does not understand.

#include "SuiteRegistry.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>
#include <system_error>
#include <vector>

#include "TestHarness.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <thread>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <TargetConditionals.h>
#endif
extern char** environ;   // POSIX: the environment a spawned child inherits
#endif

namespace {

constexpr int kSkipped = 77;   // the suites' SKIP_RETURN_CODE (tests/CMakeLists.txt)
constexpr int kUsage = 2;
constexpr unsigned kDefaultTimeoutSeconds = 1500;   // ctest's default for one test

// What became of one child process.
struct Child {
    bool started = false;
    bool timedOut = false;
    unsigned long code = 0;
    int signal = 0;      // POSIX: the signal that ended it (code is then 128 + it)
    std::string error;   // why it did not start
};

enum class Outcome { Passed, Failed, Skipped, NotRun };

struct Result {
    const char* name;
    Outcome outcome;
    std::string detail;
    double seconds = 0.0;
};

const char* OutcomeText(const Outcome outcome) {
    switch (outcome) {
    case Outcome::Passed: return "PASS";
    case Outcome::Failed: return "FAIL";
    case Outcome::Skipped: return "SKIPPED";
    case Outcome::NotRun: return "NOT RUN";
    }
    return "?";
}

std::string ExitText(const Child& child) {
    char text[96] = "";
    if (child.signal != 0) {
#ifndef _WIN32
        // POSIX: ended by a signal (SIGSEGV, SIGABRT...) rather than returning.
        const char* name = strsignal(child.signal);
        std::snprintf(text, sizeof text, "killed by signal %d (%s), crashed", child.signal,
                      name != nullptr ? name : "?");
#endif
    } else if (child.code <= 255) {
        std::snprintf(text, sizeof text, "exit %lu", child.code);
    } else {
        // A Windows exception code (0xC0000005 is an access violation): the
        // child crashed rather than returning.
        std::snprintf(text, sizeof text, "exit 0x%08lX, crashed", child.code);
    }
    return text;
}

void Usage(std::FILE* to) {
    std::fprintf(to,
                 "usage: test_pn_all [--list] [--suite <name>]... [--timeout <seconds>]\n"
                 "  (no --suite)     every suite, each in a child process of this executable\n"
                 "  --suite <name>   that suite (the test_pn_ prefix is optional); one alone runs\n"
                 "                   in this process, exactly as its own executable would\n"
                 "  --list           print the suites and run nothing\n"
                 "  --timeout <s>    kill and fail a child still running after s seconds\n"
                 "                   (default %u; 0: never)\n",
                 kDefaultTimeoutSeconds);
}

// One suite, in this process. The harness's counters are program-wide inline
// variables (TestHarness.hpp), so they are set to zero first, as a suite's own
// executable starts them.
int RunHere(const PnAll::Suite& suite) {
    test::g_checks = 0;
    test::g_failures = 0;
    try {
        return suite.run();
    } catch (const std::exception& e) {
        std::printf("  FAIL %s threw: %s\n", suite.name, e.what());
    } catch (...) {
        std::printf("  FAIL %s threw\n", suite.name);
    }
    return 1;
}

#ifdef _WIN32

std::string WindowsMessage(const DWORD error) {
    char* text = nullptr;
    const DWORD length = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error,
        0, reinterpret_cast<char*>(&text), 0, nullptr);
    std::string message = (length != 0 && text != nullptr) ? std::string(text, length) : std::string();
    if (text != nullptr) LocalFree(text);
    while (!message.empty() && std::strchr("\r\n .", message.back()) != nullptr) message.pop_back();
    return "Windows error " + std::to_string(error) + (message.empty() ? "" : ": " + message);
}

std::wstring ThisExecutable() {
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0) return {};
        if (length < path.size()) {
            path.resize(length);
            return path;
        }
        path.resize(path.size() * 2);
    }
}

// The child writes to this process's stdout and stderr, whatever they are (a
// console, a file, Git Bash's pipe), so its lines land between ours.
void InheritStandardHandles(STARTUPINFOW& startup) {
    startup.dwFlags |= STARTF_USESTDHANDLES;
    HANDLE* const slots[] = {&startup.hStdInput, &startup.hStdOutput, &startup.hStdError};
    const DWORD which[] = {STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE};
    for (int i = 0; i < 3; ++i) {
        const HANDLE handle = GetStdHandle(which[i]);
        if (handle != nullptr && handle != INVALID_HANDLE_VALUE) {
            SetHandleInformation(handle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        }
        *slots[i] = handle;
    }
}

Child Spawn(const PnAll::Suite& suite, const unsigned timeoutSeconds) {
    Child child;
    const std::wstring self = ThisExecutable();
    if (self.empty()) {
        child.error = "this executable's own path is unknown (" + WindowsMessage(GetLastError()) + ")";
        return child;
    }
    std::wstring commandLine = L"\"" + self + L"\" --suite ";
    for (const char* c = suite.name; *c != '\0'; ++c) commandLine += static_cast<wchar_t>(static_cast<unsigned char>(*c));

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    InheritStandardHandles(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(self.c_str(), commandLine.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup,
                        &process)) {
        child.error = WindowsMessage(GetLastError());
        return child;
    }
    child.started = true;
    CloseHandle(process.hThread);

    const unsigned long long milliseconds = static_cast<unsigned long long>(timeoutSeconds) * 1000ull;
    const DWORD wait = timeoutSeconds == 0 ? INFINITE
                                           : static_cast<DWORD>(std::min<unsigned long long>(milliseconds, INFINITE - 1));
    if (WaitForSingleObject(process.hProcess, wait) == WAIT_TIMEOUT) {
        child.timedOut = true;
        TerminateProcess(process.hProcess, 1);
        WaitForSingleObject(process.hProcess, INFINITE);
    }
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    child.code = code;
    CloseHandle(process.hProcess);
    return child;
}

#else

// This executable's own file, to start again with --suite: /proc/self/exe on
// Linux and Android, _NSGetExecutablePath on macOS. Empty when unknown.
std::string ThisExecutable() {
#if defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string path(size + 1, '\0');
    if (_NSGetExecutablePath(path.data(), &size) != 0) return {};
    path.resize(std::strlen(path.c_str()));
    return path;
#else
    std::string path(256, '\0');
    for (;;) {
        const ssize_t length = readlink("/proc/self/exe", path.data(), path.size());
        if (length <= 0) return {};
        if (static_cast<std::size_t>(length) < path.size()) {
            path.resize(static_cast<std::size_t>(length));
            return path;
        }
        path.resize(path.size() * 2);
    }
#endif
}

// The same protocol as on Windows: this file started again as
// `<self> --suite <name>`, a fresh process image that has run nothing, with
// this one's stdout and stderr (posix_spawn passes the descriptors on). The
// timeout is the parent's to enforce, as TerminateProcess enforces it there:
// the child is polled and killed, never trusted to stop itself.
//
// iOS lets an app start no process at all; there the suites run one at a time
// with --suite, or all in one process through each suite's own entry.
Child Spawn(const PnAll::Suite& suite, const unsigned timeoutSeconds) {
    Child child;
#if defined(__APPLE__) && TARGET_OS_IPHONE
    (void)suite;
    (void)timeoutSeconds;
    child.error = "iOS does not let an app start a process; run one suite at a time with --suite";
    return child;
#else
    std::string self = ThisExecutable();
    if (self.empty()) {
        child.error = "this executable's own path is unknown (" + std::string(std::strerror(errno)) + ")";
        return child;
    }
    std::string flag = "--suite";
    std::string name = suite.name;
    char* const argv[] = {self.data(), flag.data(), name.data(), nullptr};
    pid_t pid = 0;
    const int spawned = posix_spawn(&pid, self.c_str(), nullptr, nullptr, argv, environ);
    if (spawned != 0) {
        child.error = self + ": " + std::strerror(spawned);
        return child;
    }
    child.started = true;

    using Clock = std::chrono::steady_clock;
    const Clock::time_point deadline = Clock::now() + std::chrono::seconds(timeoutSeconds);
    int status = 0;
    for (;;) {
        const pid_t done = waitpid(pid, &status, timeoutSeconds == 0 ? 0 : WNOHANG);
        if (done == pid) break;
        if (done < 0) {
            if (errno == EINTR) continue;
            child.code = 1;
            return child;
        }
        if (Clock::now() >= deadline) {
            child.timedOut = true;
            kill(pid, SIGKILL);
            while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
            }
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (WIFEXITED(status)) {
        child.code = static_cast<unsigned long>(WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        child.signal = WTERMSIG(status);
        child.code = 128ul + static_cast<unsigned long>(child.signal);
    } else {
        child.code = 1;
    }
    return child;
#endif
}

#endif

int RunChildren(const std::vector<const PnAll::Suite*>& selected, const unsigned timeoutSeconds) {
#ifdef _WIN32
    // An unattended run must not wait on a "has stopped working" box: a child
    // that crashes exits with its exception code instead. Children inherit it.
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
    using Clock = std::chrono::steady_clock;
    const Clock::time_point start = Clock::now();
    std::printf("test_pn_all: %zu suites, each in a child process of this executable\n", selected.size());

    std::vector<Result> results;
    bool stopped = false;
    for (std::size_t i = 0; i < selected.size(); ++i) {
        const PnAll::Suite& suite = *selected[i];
        if (stopped) {
            results.push_back({suite.name, Outcome::NotRun, "not started", 0.0});
            continue;
        }
        std::printf("\n== %s (%zu/%zu)\n", suite.name, i + 1, selected.size());
        std::fflush(stdout);
        std::fflush(stderr);
        const Clock::time_point began = Clock::now();
        const Child child = Spawn(suite, timeoutSeconds);
        const double seconds = std::chrono::duration<double>(Clock::now() - began).count();

        Result result{suite.name, Outcome::Failed, {}, seconds};
        if (!child.started) {
            // Never on to the next: whatever kept this child from starting would
            // keep the next one, and each refused start may notify the user
            // (Smart App Control, CLAUDE.md rule 5).
            result.outcome = Outcome::NotRun;
            result.detail = "could not be started: " + child.error;
            stopped = true;
        } else if (child.timedOut) {
            result.detail = "killed after " + std::to_string(timeoutSeconds) + " s (--timeout)";
        } else if (child.code == 0) {
            result.outcome = Outcome::Passed;
        } else if (child.code == static_cast<unsigned long>(kSkipped)) {
            result.outcome = Outcome::Skipped;
        } else {
            result.detail = ExitText(child);
        }
        std::printf("-- %s: %s%s%s (%.1f s)\n", suite.name, OutcomeText(result.outcome),
                    result.detail.empty() ? "" : ", ", result.detail.c_str(), seconds);
        if (stopped) std::printf("   test_pn_all stops here and starts no further suite.\n");
        results.push_back(result);
    }

    int passed = 0;
    int failed = 0;
    int skipped = 0;
    int notRun = 0;
    for (const Result& r : results) {
        switch (r.outcome) {
        case Outcome::Passed: ++passed; break;
        case Outcome::Failed: ++failed; break;
        case Outcome::Skipped: ++skipped; break;
        case Outcome::NotRun: ++notRun; break;
        }
    }
    const double total = std::chrono::duration<double>(Clock::now() - start).count();
    std::printf("\ntest_pn_all: %zu suites in %.1f s: %d passed, %d failed, %d skipped, %d not run\n", results.size(),
                total, passed, failed, skipped, notRun);
    // Every suite, so one glance says what ran and where the time went (each
    // suite's own check count is in its section above).
    for (const Result& r : results) {
        std::printf("  %-7s %-28s %6.1f s%s%s\n", OutcomeText(r.outcome), r.name, r.seconds,
                    r.detail.empty() ? "" : " - ", r.detail.c_str());
    }
    std::fflush(stdout);
    if (failed > 0 || notRun > 0) return 1;
    return passed == 0 ? kSkipped : 0;
}

const PnAll::Suite* Find(const std::vector<PnAll::Suite>& suites, const std::string& name) {
    for (const PnAll::Suite& suite : suites) {
        if (name == suite.name || "test_pn_" + name == suite.name) return &suite;
    }
    return nullptr;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<PnAll::Suite> suites = PnAll::Suites();
    std::sort(suites.begin(), suites.end(),
              [](const PnAll::Suite& a, const PnAll::Suite& b) { return std::strcmp(a.name, b.name) < 0; });

    bool list = false;
    unsigned timeoutSeconds = kDefaultTimeoutSeconds;
    std::vector<const PnAll::Suite*> selected;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            Usage(stdout);
            return 0;
        }
        if (arg == "--list") {
            list = true;
            continue;
        }
        if (arg != "--suite" && arg != "--timeout") {
            std::fprintf(stderr, "test_pn_all: unknown argument '%s'\n", arg.c_str());
            Usage(stderr);
            return kUsage;
        }
        if (i + 1 >= argc) {
            std::fprintf(stderr, "test_pn_all: %s needs a value\n", arg.c_str());
            return kUsage;
        }
        const std::string value = argv[++i];
        if (arg == "--suite") {
            const PnAll::Suite* suite = Find(suites, value);
            if (suite == nullptr) {
                std::fprintf(stderr, "test_pn_all: there is no suite '%s' (--list shows them)\n", value.c_str());
                return kUsage;
            }
            if (std::find(selected.begin(), selected.end(), suite) == selected.end()) selected.push_back(suite);
        } else {
            const char* const end = value.data() + value.size();
            const auto [stop, error] = std::from_chars(value.data(), end, timeoutSeconds);
            if (error != std::errc() || stop != end) {
                std::fprintf(stderr, "test_pn_all: --timeout wants whole seconds, not '%s'\n", value.c_str());
                return kUsage;
            }
        }
    }

    if (list) {
        for (const PnAll::Suite& suite : suites) std::printf("%s\n", suite.name);
        return 0;
    }
    if (suites.empty()) {
        std::printf("test_pn_all: no suite was linked in\n");
        return 1;
    }
    // One suite named: it runs here, exactly as its own executable runs it.
    // This is also how each child of a full run starts.
    if (selected.size() == 1) return RunHere(*selected.front());
    if (selected.empty()) {
        for (const PnAll::Suite& suite : suites) selected.push_back(&suite);
    }
    return RunChildren(selected, timeoutSeconds);
}
