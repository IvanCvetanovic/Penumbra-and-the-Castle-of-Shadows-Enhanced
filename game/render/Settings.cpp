#include "render/Settings.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string_view>
#include <system_error>

#include <GLFW/glfw3.h>

#include "core/Json.hpp"
#include "render/Languages.hpp"

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
// E23: window.fullscreenRefresh, in Hz. Wider than any display's rate, and
// never a guess: anything outside is automatic.
constexpr int kMaxRefreshRate = 1000;

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

// window.width/height (E23): 0 x 0 is automatic, and so is a zero in either,
// which only half a size or a typo leaves; any other value is read and clamped
// to the window's range, as it always was. One extent alone is read against
// the other's current value, and an automatic other makes it half a size.
void ReadWindowedSize(const Value& window, int& width, int& height, std::string* warning) {
    if (!window.Has("width") && !window.Has("height")) return;
    const auto zero = [&window](const char* key) {
        return window.Has(key) && window[key].IsNumber() && window[key].AsNumber(1.0) == 0.0;
    };
    if (zero("width") && zero("height")) {
        width = 0;
        height = 0;
        return;
    }
    int readWidth = width;
    int readHeight = height;
    if (!zero("width")) ReadInt(window, "width", kMinWindowWidth, kMaxWindowWidth, readWidth, warning);
    if (!zero("height")) ReadInt(window, "height", kMinWindowHeight, kMaxWindowHeight, readHeight, warning);
    if (zero("width") || zero("height") || (readWidth == 0) != (readHeight == 0)) {
        Warn(warning, "window.width/height is half a size; the window is automatic");
        width = 0;
        height = 0;
        return;
    }
    width = readWidth;
    height = readHeight;
}

// zoom (E25): "auto" (any case) is 0; a number is a percentage, rounded and
// held to 100..200. Anything else leaves the default, with a warning.
void ReadZoom(const Value& root, int& out, std::string* warning) {
    if (!root.Has("zoom")) return;
    const Value& value = root["zoom"];
    if (value.IsString() && EqualsIgnoreCase(value.AsString(), "auto")) {
        out = 0;
        return;
    }
    const double number = value.AsNumber(std::nan(""));
    if (!value.IsNumber() || !std::isfinite(number)) {
        Warn(warning, "zoom is not \"auto\" or a percentage from 100 to 200");
        return;
    }
    out = static_cast<int>(std::clamp(std::round(number), 100.0, 200.0));
}

// edgeMargin (E26): "auto" (any case) is negative; a number is a percentage,
// held to 0..8. Anything else leaves the default, with a warning.
void ReadEdgeMargin(const Value& root, float& out, std::string* warning) {
    if (!root.Has("edgeMargin")) return;
    const Value& value = root["edgeMargin"];
    if (value.IsString() && EqualsIgnoreCase(value.AsString(), "auto")) {
        out = -1.0f;
        return;
    }
    const double number = value.AsNumber(std::nan(""));
    if (!value.IsNumber() || !std::isfinite(number)) {
        Warn(warning, "edgeMargin is not \"auto\" or a percentage from 0 to 8");
        return;
    }
    out = static_cast<float>(std::clamp(number, 0.0, 8.0));
}

// window.fullscreenRefresh (E23): 0, or a whole number of Hz up to
// kMaxRefreshRate. Anything else is automatic, with a warning.
void ReadRefreshRate(const Value& window, int& out, std::string* warning) {
    if (!window.Has("fullscreenRefresh")) return;
    const Value& value = window["fullscreenRefresh"];
    const double number = value.AsNumber(std::nan(""));
    if (!value.IsNumber() || !std::isfinite(number) || number != std::floor(number) || number < 0.0 ||
        number > kMaxRefreshRate) {
        Warn(warning, "window.fullscreenRefresh is not a refresh rate; using the automatic one");
        out = 0;
        return;
    }
    out = static_cast<int>(number);
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

Settings Settings::Defaults(const std::string& language) {
    Settings settings;
    Language known = Language::English;
    settings.language = LanguageFromId(language, known) ? LanguageId(known) : "en";
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

std::string Settings::LanguageOfLocale(const std::string& locale) {
    const std::size_t end = locale.find_first_of("_-.@");
    Language language = Language::English;
    return LanguageFromId(std::string_view(locale).substr(0, end), language) ? LanguageId(language) : "en";
}

bool Settings::LocaleIsPortuguese(const std::string& locale) { return LanguageOfLocale(locale) == "pt"; }

namespace {

// winnt.h's primary language ids of the languages the game speaks.
struct PrimaryLangId {
    unsigned id;
    Language language;
};
constexpr PrimaryLangId kPrimaryLangIds[] = {
    {0x09, Language::English},  {0x07, Language::German},  {0x0A, Language::Spanish},
    {0x0C, Language::French},   {0x10, Language::Italian}, {0x16, Language::Portuguese},
    {0x19, Language::Russian},  {0x1F, Language::Turkish}, {0x22, Language::Ukrainian},
    {0x11, Language::Japanese}, {0x01, Language::Arabic},
};
#ifdef _WIN32
static_assert(kPrimaryLangIds[0].id == LANG_ENGLISH && kPrimaryLangIds[1].id == LANG_GERMAN &&
                  kPrimaryLangIds[2].id == LANG_SPANISH && kPrimaryLangIds[3].id == LANG_FRENCH &&
                  kPrimaryLangIds[4].id == LANG_ITALIAN && kPrimaryLangIds[5].id == LANG_PORTUGUESE &&
                  kPrimaryLangIds[6].id == LANG_RUSSIAN && kPrimaryLangIds[7].id == LANG_TURKISH &&
                  kPrimaryLangIds[8].id == LANG_UKRAINIAN && kPrimaryLangIds[9].id == LANG_JAPANESE &&
                  kPrimaryLangIds[10].id == LANG_ARABIC,
              "kPrimaryLangIds agrees with winnt.h");
#endif

} // namespace

std::string Settings::LanguageOfPrimaryLangId(const unsigned primaryLangId) {
    for (const PrimaryLangId& entry : kPrimaryLangIds) {
        if (entry.id == primaryLangId) return LanguageId(entry.language);
    }
    return "en";
}

std::string Settings::SystemLanguage() {
    if (!SuppliedLocale().empty()) return LanguageOfLocale(SuppliedLocale());
#ifdef _WIN32
    return LanguageOfPrimaryLangId(PRIMARYLANGID(GetUserDefaultUILanguage()));
#else
    // POSIX precedence for messages: LC_ALL overrides LC_MESSAGES, which
    // overrides LANG; an empty variable counts as unset.
    for (const char* name : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        const char* value = std::getenv(name);
        if (value != nullptr && *value != '\0') return LanguageOfLocale(value);
    }
    return "en";
#endif
}

bool Settings::SystemLanguageIsPortuguese() { return SystemLanguage() == "pt"; }

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
        Language known = Language::English;
        if (LanguageFromId(language, known)) {
            settings.language = LanguageId(known);
        } else {
            Warn(warning, "language \"" + language + "\" is not one the game speaks");
        }
    }

    if (root.Has("window")) {
        const Value& window = root["window"];
        if (window.IsObject()) {
            ReadWindowedSize(window, settings.windowWidth, settings.windowHeight, warning);
            ReadBool(window, "fullscreen", settings.fullscreen, warning);
            ReadFullscreenMode(window, settings.fullscreenWidth, settings.fullscreenHeight, warning);
            ReadRefreshRate(window, settings.fullscreenRefresh, warning);   // E23
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
    ReadZoom(root, settings.zoom, warning);   // E25
    ReadEdgeMargin(root, settings.edgeMargin, warning);   // E26

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
        << ", \"fullscreenHeight\": " << fullscreenHeight << ", \"fullscreenRefresh\": " << fullscreenRefresh
        << " },\n";
    out << "  \"widescreen\": " << FormatBool(widescreen) << ",\n";
    out << "  \"volume\": { \"music\": " << FormatFloat(musicVolume) << ", \"effects\": "
        << FormatFloat(effectsVolume) << " },\n";
    out << "  \"pixelShaders\": " << FormatBool(pixelShaders) << ",\n";
    out << "  \"smoothMotion\": " << FormatBool(smoothMotion) << ",\n";
    out << "  \"pauseOnFocusLoss\": " << FormatBool(pauseOnFocusLoss) << ",\n";
    out << "  \"touchControls\": \"" << Supersonic::Json::Escape(touchControls) << "\",\n";   // E16
    out << "  \"zoom\": " << (zoom == 0 ? std::string("\"auto\"") : std::to_string(zoom)) << ",\n";   // E25
    out << "  \"edgeMargin\": " << (edgeMargin < 0.0f ? std::string("\"auto\"") : FormatFloat(edgeMargin))
        << ",\n";   // E26
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
