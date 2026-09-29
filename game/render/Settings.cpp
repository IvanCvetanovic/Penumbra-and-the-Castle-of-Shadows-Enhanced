#include "render/Settings.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <system_error>

#include <GLFW/glfw3.h>

#include "core/Json.hpp"

// Only for the UI language. Never include eth/Eth.hpp in this file: windows.h
// defines DrawText as a macro, which would rename the script API's DrawText.
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Penumbra::Render {

namespace {

namespace fs = std::filesystem;
using Supersonic::Json::Value;

constexpr const char* kActionNames[kControlActionCount] = {
    "left", "right", "up", "down", "jump", "sword", "fire", "light", "confirm", "cancel",
};

struct NamedKey {
    int code;
    const char* name;
};

// The names written to the file. Letters, digits and F-keys are generated in
// KeyName/KeyFromName; everything else a player might bind is here.
constexpr NamedKey kNamedKeys[] = {
    {GLFW_KEY_SPACE, "Space"},
    {GLFW_KEY_APOSTROPHE, "Apostrophe"},
    {GLFW_KEY_COMMA, "Comma"},
    {GLFW_KEY_MINUS, "Minus"},
    {GLFW_KEY_PERIOD, "Period"},
    {GLFW_KEY_SLASH, "Slash"},
    {GLFW_KEY_SEMICOLON, "Semicolon"},
    {GLFW_KEY_EQUAL, "Equal"},
    {GLFW_KEY_LEFT_BRACKET, "LeftBracket"},
    {GLFW_KEY_BACKSLASH, "Backslash"},
    {GLFW_KEY_RIGHT_BRACKET, "RightBracket"},
    {GLFW_KEY_GRAVE_ACCENT, "GraveAccent"},
    {GLFW_KEY_ESCAPE, "Escape"},
    {GLFW_KEY_ENTER, "Enter"},
    {GLFW_KEY_TAB, "Tab"},
    {GLFW_KEY_BACKSPACE, "Backspace"},
    {GLFW_KEY_INSERT, "Insert"},
    {GLFW_KEY_DELETE, "Delete"},
    {GLFW_KEY_RIGHT, "Right"},
    {GLFW_KEY_LEFT, "Left"},
    {GLFW_KEY_DOWN, "Down"},
    {GLFW_KEY_UP, "Up"},
    {GLFW_KEY_PAGE_UP, "PageUp"},
    {GLFW_KEY_PAGE_DOWN, "PageDown"},
    {GLFW_KEY_HOME, "Home"},
    {GLFW_KEY_END, "End"},
    {GLFW_KEY_CAPS_LOCK, "CapsLock"},
    {GLFW_KEY_SCROLL_LOCK, "ScrollLock"},
    {GLFW_KEY_NUM_LOCK, "NumLock"},
    {GLFW_KEY_PRINT_SCREEN, "PrintScreen"},
    {GLFW_KEY_PAUSE, "Pause"},
    {GLFW_KEY_KP_DECIMAL, "KeypadDecimal"},
    {GLFW_KEY_KP_DIVIDE, "KeypadDivide"},
    {GLFW_KEY_KP_MULTIPLY, "KeypadMultiply"},
    {GLFW_KEY_KP_SUBTRACT, "KeypadSubtract"},
    {GLFW_KEY_KP_ADD, "KeypadAdd"},
    {GLFW_KEY_KP_ENTER, "KeypadEnter"},
    {GLFW_KEY_KP_EQUAL, "KeypadEqual"},
    {GLFW_KEY_LEFT_SHIFT, "LeftShift"},
    {GLFW_KEY_LEFT_CONTROL, "LeftCtrl"},
    {GLFW_KEY_LEFT_ALT, "LeftAlt"},
    {GLFW_KEY_LEFT_SUPER, "LeftSuper"},
    {GLFW_KEY_RIGHT_SHIFT, "RightShift"},
    {GLFW_KEY_RIGHT_CONTROL, "RightCtrl"},
    {GLFW_KEY_RIGHT_ALT, "RightAlt"},
    {GLFW_KEY_RIGHT_SUPER, "RightSuper"},
    {GLFW_KEY_MENU, "Menu"},
};

// The window's sane range. Wider than any display, narrower than a typo.
constexpr int kMinWindowWidth = 640;
constexpr int kMaxWindowWidth = 15360;
constexpr int kMinWindowHeight = 480;
constexpr int kMaxWindowHeight = 8640;
constexpr float kMaxStickDeadzone = 0.9f;

bool EqualsIgnoreCase(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

// The codes the engine's InputPolling walks (validKeys), so the only ones a
// binding can ever see held.
bool IsValidKeyCode(int code) {
    return code == GLFW_KEY_SPACE || code == GLFW_KEY_APOSTROPHE ||
           (code >= GLFW_KEY_COMMA && code <= GLFW_KEY_9) || code == GLFW_KEY_SEMICOLON ||
           code == GLFW_KEY_EQUAL || (code >= GLFW_KEY_A && code <= GLFW_KEY_RIGHT_BRACKET) ||
           code == GLFW_KEY_GRAVE_ACCENT ||
           (code >= GLFW_KEY_ESCAPE && code <= GLFW_KEY_END) ||
           (code >= GLFW_KEY_CAPS_LOCK && code <= GLFW_KEY_PAUSE) ||
           (code >= GLFW_KEY_F1 && code <= GLFW_KEY_F25) ||
           (code >= GLFW_KEY_KP_0 && code <= GLFW_KEY_KP_EQUAL) ||
           (code >= GLFW_KEY_LEFT_SHIFT && code <= GLFW_KEY_MENU);
}

void Warn(std::string* warning, const std::string& what) {
    if (warning == nullptr) return;
    if (!warning->empty()) *warning += "; ";
    *warning += what;
}

std::string FormatFloat(float value) {
    // Shortest text that reads back as the same float, so a round trip through
    // the file changes nothing.
    char buffer[32];
    const auto result = std::to_chars(std::begin(buffer), std::end(buffer), value);
    if (result.ec != std::errc()) return "0";
    return std::string(buffer, result.ptr);
}

const char* FormatBool(bool value) { return value ? "true" : "false"; }

// A present field of the wrong type is worth a warning; an absent one is just
// an older file.
void ReadBool(const Value& object, const char* key, bool& out, std::string* warning) {
    if (!object.Has(key)) return;
    const Value& value = object[key];
    if (!value.IsBool()) {
        Warn(warning, std::string(key) + " is not true/false");
        return;
    }
    out = value.AsBool();
}

void ReadInt(const Value& object, const char* key, int minimum, int maximum, int& out, std::string* warning) {
    if (!object.Has(key)) return;
    const Value& value = object[key];
    const double number = value.AsNumber(std::nan(""));
    if (!value.IsNumber() || !std::isfinite(number)) {
        Warn(warning, std::string(key) + " is not a number");
        return;
    }
    const double clamped = std::clamp(std::round(number), static_cast<double>(minimum), static_cast<double>(maximum));
    out = static_cast<int>(clamped);
}

// window.fullscreenWidth/Height: 0 x 0, or a whole size in the window's range.
// Anything else - a typo, a fraction, half a size - leaves the default (the
// desktop's mode) rather than being clamped: a clamped width is a guess at a
// mode no monitor may have, where the desktop's is always there.
void ReadFullscreenMode(const Value& window, int& width, int& height, std::string* warning) {
    if (!window.Has("fullscreenWidth") && !window.Has("fullscreenHeight")) return;
    const auto read = [&window](const char* key, int minimum, int maximum, int& out) {
        if (!window.Has(key)) return false;
        const Value& value = window[key];
        const double number = value.AsNumber(std::nan(""));
        if (!value.IsNumber() || !std::isfinite(number) || number != std::floor(number)) return false;
        if (number != 0.0 && (number < minimum || number > maximum)) return false;
        out = static_cast<int>(number);
        return true;
    };
    int readWidth = 0;
    int readHeight = 0;
    if (!read("fullscreenWidth", kMinWindowWidth, kMaxWindowWidth, readWidth) ||
        !read("fullscreenHeight", kMinWindowHeight, kMaxWindowHeight, readHeight) ||
        (readWidth == 0) != (readHeight == 0)) {
        Warn(warning, "window.fullscreenWidth/fullscreenHeight is not a display mode; using the desktop's");
        return;
    }
    width = readWidth;
    height = readHeight;
}

void ReadFloat(const Value& object, const char* key, float minimum, float maximum, float& out,
               std::string* warning) {
    if (!object.Has(key)) return;
    const Value& value = object[key];
    const double number = value.AsNumber(std::nan(""));
    if (!value.IsNumber() || !std::isfinite(number)) {
        Warn(warning, std::string(key) + " is not a number");
        return;
    }
    out = std::clamp(static_cast<float>(number), minimum, maximum);
}

void ReadBindings(const Value& object, const char* player, KeyBindings& bindings, std::string* warning) {
    if (!object.Has(player)) return;
    const Value& table = object[player];
    if (!table.IsObject()) {
        Warn(warning, std::string(player) + " is not an object");
        return;
    }
    for (int i = 0; i < kControlActionCount; ++i) {
        const char* action = kActionNames[i];
        // An absent action keeps its default; a present empty list unbinds it.
        if (!table.Has(action)) continue;
        const Value& list = table[action];
        if (!list.IsArray()) {
            Warn(warning, std::string(player) + "." + action + " is not a list");
            continue;
        }
        std::vector<int> keys;
        for (const Value& entry : list.AsArray()) {
            int code = -1;
            if (entry.IsString()) {
                code = KeyFromName(entry.AsString());
            } else if (entry.IsNumber()) {
                const double number = entry.AsNumber();
                if (std::isfinite(number) && number == std::floor(number) && number >= 0.0 && number <= 512.0) {
                    code = static_cast<int>(number);
                    if (!IsValidKeyCode(code)) code = -1;
                }
            }
            if (code < 0) {
                Warn(warning, std::string(player) + "." + action + " has an unknown key");
                continue;
            }
            if (std::find(keys.begin(), keys.end(), code) == keys.end()) keys.push_back(code);
        }
        bindings.keys[static_cast<std::size_t>(i)] = std::move(keys);
    }
}

void WriteBindings(std::ostringstream& out, const char* player, const KeyBindings& bindings, bool last) {
    out << "    \"" << player << "\": {\n";
    for (int i = 0; i < kControlActionCount; ++i) {
        out << "      \"" << kActionNames[i] << "\": [";
        const std::vector<int>& keys = bindings.keys[static_cast<std::size_t>(i)];
        for (std::size_t k = 0; k < keys.size(); ++k) {
            if (k > 0) out << ", ";
            // Names, not codes, so the file can be edited by hand; a code
            // without a name would go in as its number, which reads back.
            out << "\"" << Supersonic::Json::Escape(KeyName(keys[k])) << "\"";
        }
        out << "]" << (i + 1 < kControlActionCount ? "," : "") << "\n";
    }
    out << "    }" << (last ? "" : ",") << "\n";
}

} // namespace

const char* ControlActionName(ControlAction action) {
    const int index = static_cast<int>(action);
    return index >= 0 && index < kControlActionCount ? kActionNames[index] : "";
}

std::string KeyName(int glfwKey) {
    // Nothing for a code GLFW never reports: "5" would read back as the 5 key.
    if (!IsValidKeyCode(glfwKey)) return {};
    if (glfwKey >= GLFW_KEY_A && glfwKey <= GLFW_KEY_Z) return std::string(1, static_cast<char>(glfwKey));
    if (glfwKey >= GLFW_KEY_0 && glfwKey <= GLFW_KEY_9) return std::string(1, static_cast<char>(glfwKey));
    if (glfwKey >= GLFW_KEY_F1 && glfwKey <= GLFW_KEY_F25) return "F" + std::to_string(glfwKey - GLFW_KEY_F1 + 1);
    if (glfwKey >= GLFW_KEY_KP_0 && glfwKey <= GLFW_KEY_KP_9) return "Keypad" + std::to_string(glfwKey - GLFW_KEY_KP_0);
    for (const NamedKey& named : kNamedKeys) {
        if (named.code == glfwKey) return named.name;
    }
    return std::to_string(glfwKey);
}

int KeyFromName(const std::string& name) {
    if (name.empty()) return -1;
    if (name.size() == 1) {
        const unsigned char c = static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(name[0])));
        if (c >= 'A' && c <= 'Z') return c;
        if (c >= '0' && c <= '9') return c;
    }
    for (const NamedKey& named : kNamedKeys) {
        if (EqualsIgnoreCase(named.name, name)) return named.code;
    }
    const auto numberAfter = [&name](std::size_t prefix) -> int {
        if (name.size() <= prefix || name.size() > prefix + 3) return -1;
        int value = 0;
        for (std::size_t i = prefix; i < name.size(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(name[i]))) return -1;
            value = value * 10 + (name[i] - '0');
        }
        return value;
    };
    if ((name[0] == 'F' || name[0] == 'f')) {
        const int n = numberAfter(1);
        if (n >= 1 && n <= 25) return GLFW_KEY_F1 + n - 1;
    }
    if (name.size() == 7 && EqualsIgnoreCase(name.substr(0, 6), "Keypad")) {
        const int n = numberAfter(6);
        if (n >= 0 && n <= 9) return GLFW_KEY_KP_0 + n;
    }
    // A raw code, as a key without a name is written.
    const int code = numberAfter(0);
    return code >= 0 && IsValidKeyCode(code) ? code : -1;
}

KeyBindings Settings::DefaultPlayer1Keys() {
    // The original's player 0 keys (playerInput.as:56-305, the como_jogar text
    // at menu.as): arrows, Ctrl (or Up, which the script itself treats as
    // jump), S, D, Space, Enter, Esc. K_CTRL was VK_CONTROL, either Ctrl, and
    // K_RETURN was VK_RETURN, either Enter (G:Input/Win/gs2dWinInput.cpp:82,88).
    KeyBindings keys;
    keys[ControlAction::Left] = {GLFW_KEY_LEFT};
    keys[ControlAction::Right] = {GLFW_KEY_RIGHT};
    keys[ControlAction::Up] = {GLFW_KEY_UP};
    keys[ControlAction::Down] = {GLFW_KEY_DOWN};
    keys[ControlAction::Jump] = {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_RIGHT_CONTROL};
    keys[ControlAction::Sword] = {GLFW_KEY_S};
    keys[ControlAction::Fire] = {GLFW_KEY_D};
    keys[ControlAction::Light] = {GLFW_KEY_SPACE};
    keys[ControlAction::Confirm] = {GLFW_KEY_ENTER, GLFW_KEY_KP_ENTER};
    keys[ControlAction::Cancel] = {GLFW_KEY_ESCAPE};
    return keys;
}

KeyBindings Settings::DefaultPlayer2Keys() {
    // The right hand, clear of every key player 1 uses: I/J/K/L to move with I
    // also jumping (as Up does for player 1), U sword, O fire, P light.
    // Backspace is START, which summons the princess in the campaign
    // (controlCharacters.as:679); K_BACKSPACE itself is read only by the
    // unused PlaceInput (eth_util.as:100). Nothing reads player 2's cancel
    // (menu.as:376 asks player 0 only), so it is left unbound.
    KeyBindings keys;
    keys[ControlAction::Left] = {GLFW_KEY_J};
    keys[ControlAction::Right] = {GLFW_KEY_L};
    keys[ControlAction::Up] = {GLFW_KEY_I};
    keys[ControlAction::Down] = {GLFW_KEY_K};
    keys[ControlAction::Jump] = {GLFW_KEY_I};
    keys[ControlAction::Sword] = {GLFW_KEY_U};
    keys[ControlAction::Fire] = {GLFW_KEY_O};
    keys[ControlAction::Light] = {GLFW_KEY_P};
    keys[ControlAction::Confirm] = {GLFW_KEY_BACKSPACE};
    keys[ControlAction::Cancel] = {};
    return keys;
}

Settings Settings::Defaults(bool systemIsPortuguese) {
    Settings settings;
    settings.language = systemIsPortuguese ? "pt" : "en";
    settings.controls.player1 = DefaultPlayer1Keys();
    settings.controls.player2 = DefaultPlayer2Keys();
    return settings;
}

namespace {

// SetSystemLocale's. Set once at startup, before any thread reads it.
std::string& SuppliedLocale() {
    static std::string locale;
    return locale;
}

} // namespace

void Settings::SetSystemLocale(const std::string& locale) { SuppliedLocale() = locale; }

bool Settings::LocaleIsPortuguese(const std::string& locale) {
    if (locale.size() < 2) return false;
    const auto lower = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
    if (lower(locale[0]) != 'p' || lower(locale[1]) != 't') return false;
    return locale.size() == 2 || locale[2] == '_' || locale[2] == '-' || locale[2] == '.' || locale[2] == '@';
}

bool Settings::SystemLanguageIsPortuguese() {
    if (!SuppliedLocale().empty()) return LocaleIsPortuguese(SuppliedLocale());
#ifdef _WIN32
    return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_PORTUGUESE;
#else
    // POSIX precedence for messages: LC_ALL overrides LC_MESSAGES, which
    // overrides LANG; an empty variable counts as unset.
    for (const char* name : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        const char* value = std::getenv(name);
        if (value != nullptr && *value != '\0') return LocaleIsPortuguese(value);
    }
    return false;
#endif
}

std::filesystem::path Settings::FilePath(const std::filesystem::path& userDir) {
    return userDir / "settings.json";
}

Settings Settings::FromJson(const std::string& text, const Settings& defaults, std::string* warning) {
    Settings settings = defaults;

    Value root;
    std::string parseError;
    if (!Supersonic::Json::Parse(text, root, parseError)) {
        Warn(warning, "settings.json is not valid JSON (" + parseError + "); using the defaults");
        return settings;
    }
    if (!root.IsObject()) {
        Warn(warning, "settings.json is not an object; using the defaults");
        return settings;
    }

    if (root.Has("language")) {
        const std::string language = root["language"].AsString();
        if (EqualsIgnoreCase(language, "pt")) {
            settings.language = "pt";
        } else if (EqualsIgnoreCase(language, "en")) {
            settings.language = "en";
        } else {
            Warn(warning, "language is neither \"pt\" nor \"en\"");
        }
    }

    if (root.Has("window")) {
        const Value& window = root["window"];
        if (window.IsObject()) {
            ReadInt(window, "width", kMinWindowWidth, kMaxWindowWidth, settings.windowWidth, warning);
            ReadInt(window, "height", kMinWindowHeight, kMaxWindowHeight, settings.windowHeight, warning);
            ReadBool(window, "fullscreen", settings.fullscreen, warning);
            ReadFullscreenMode(window, settings.fullscreenWidth, settings.fullscreenHeight, warning);
        } else {
            Warn(warning, "window is not an object");
        }
    }

    ReadBool(root, "widescreen", settings.widescreen, warning);
    ReadBool(root, "pixelShaders", settings.pixelShaders, warning);
    ReadBool(root, "smoothMotion", settings.smoothMotion, warning);
    ReadBool(root, "pauseOnFocusLoss", settings.pauseOnFocusLoss, warning);

    // E16. A hand-written true/false is taken as "on"/"off".
    if (root.Has("touchControls")) {
        const Value& touch = root["touchControls"];
        const std::string mode = touch.AsString();
        if (touch.IsBool()) {
            settings.touchControls = touch.AsBool() ? "on" : "off";
        } else if (EqualsIgnoreCase(mode, "auto") || EqualsIgnoreCase(mode, "on") || EqualsIgnoreCase(mode, "off")) {
            settings.touchControls = mode;
            for (char& c : settings.touchControls) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else {
            Warn(warning, "touchControls is not \"auto\", \"on\" or \"off\"");
        }
    }

    if (root.Has("volume")) {
        const Value& volume = root["volume"];
        if (volume.IsObject()) {
            ReadFloat(volume, "music", 0.0f, 1.0f, settings.musicVolume, warning);
            ReadFloat(volume, "effects", 0.0f, 1.0f, settings.effectsVolume, warning);
        } else {
            Warn(warning, "volume is not an object");
        }
    }

    if (root.Has("controls")) {
        const Value& controls = root["controls"];
        if (controls.IsObject()) {
            ReadInt(controls, "joystickLayout", 0, 1, settings.controls.joystickLayout, warning);
            ReadBool(controls, "keyboardPlayer2", settings.controls.keyboardPlayer2, warning);
            ReadBool(controls, "firstPadIsPlayer1", settings.controls.firstPadIsPlayer1, warning);
            ReadBool(controls, "rawJoysticks", settings.controls.rawJoysticks, warning);
            ReadFloat(controls, "stickDeadzone", 0.0f, kMaxStickDeadzone, settings.controls.stickDeadzone, warning);
            ReadBindings(controls, "player1", settings.controls.player1, warning);
            ReadBindings(controls, "player2", settings.controls.player2, warning);
        } else {
            Warn(warning, "controls is not an object");
        }
    }

    return settings;
}

Settings Settings::Load(const std::filesystem::path& userDir, const Settings& defaults, std::string* warning) {
    if (userDir.empty()) return defaults;
    const fs::path path = FilePath(userDir);
    std::error_code ec;
    if (!fs::exists(path, ec)) return defaults;   // the first launch: nothing to report

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        Warn(warning, path.string() + ": cannot open; using the defaults");
        return defaults;
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    // Named: Json::Parser keeps a reference to the text it parses.
    const std::string text = contents.str();
    return FromJson(text, defaults, warning);
}

std::string Settings::ToJson() const {
    std::ostringstream out;
    out << "{\n";
    out << "  \"version\": " << kVersion << ",\n";
    out << "  \"language\": \"" << Supersonic::Json::Escape(language) << "\",\n";
    out << "  \"window\": { \"width\": " << windowWidth << ", \"height\": " << windowHeight
        << ", \"fullscreen\": " << FormatBool(fullscreen) << ", \"fullscreenWidth\": " << fullscreenWidth
        << ", \"fullscreenHeight\": " << fullscreenHeight << " },\n";
    out << "  \"widescreen\": " << FormatBool(widescreen) << ",\n";
    out << "  \"volume\": { \"music\": " << FormatFloat(musicVolume) << ", \"effects\": "
        << FormatFloat(effectsVolume) << " },\n";
    out << "  \"pixelShaders\": " << FormatBool(pixelShaders) << ",\n";
    out << "  \"smoothMotion\": " << FormatBool(smoothMotion) << ",\n";
    out << "  \"pauseOnFocusLoss\": " << FormatBool(pauseOnFocusLoss) << ",\n";
    out << "  \"touchControls\": \"" << Supersonic::Json::Escape(touchControls) << "\",\n";   // E16
    out << "  \"controls\": {\n";
    out << "    \"joystickLayout\": " << controls.joystickLayout << ",\n";
    out << "    \"keyboardPlayer2\": " << FormatBool(controls.keyboardPlayer2) << ",\n";
    out << "    \"firstPadIsPlayer1\": " << FormatBool(controls.firstPadIsPlayer1) << ",\n";
    out << "    \"rawJoysticks\": " << FormatBool(controls.rawJoysticks) << ",\n";
    out << "    \"stickDeadzone\": " << FormatFloat(controls.stickDeadzone) << ",\n";
    WriteBindings(out, "player1", controls.player1, false);
    WriteBindings(out, "player2", controls.player2, true);
    out << "  }\n";
    out << "}\n";
    return out.str();
}

bool Settings::Save(const std::filesystem::path& userDir, std::string* error) const {
    const auto fail = [error](const std::string& why) {
        if (error != nullptr) *error = why;
        return false;
    };
    if (userDir.empty()) return fail("no user folder to save settings in");

    std::error_code ec;
    fs::create_directories(userDir, ec);
    if (ec) return fail(userDir.string() + ": " + ec.message());

    const fs::path path = FilePath(userDir);
    fs::path temp = path;
    temp += ".tmp";
    {
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        if (!file) return fail(temp.string() + ": cannot open for writing");
        file << ToJson();
        file.flush();
        if (!file.good()) return fail(temp.string() + ": write failed");
    }

    // The temporary is complete before the real file is touched, so a crash
    // leaves either the old settings or the new ones, never half of either.
    ec.clear();
    fs::rename(temp, path, ec);
    if (ec) {
        // Some filesystems refuse a rename onto an existing file (MagicPortals
        // Scores.cpp met the same): remove and retry.
        ec.clear();
        fs::remove(path, ec);
        ec.clear();
        fs::rename(temp, path, ec);
    }
    if (ec) return fail(path.string() + ": " + ec.message());
    return true;
}

} // namespace Penumbra::Render
