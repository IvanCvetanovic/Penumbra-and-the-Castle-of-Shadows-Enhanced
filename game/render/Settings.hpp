#pragma once

// The enhanced game's persisted settings, in <userDir>/settings.json.
//
// The original persisted none of its options: g_enablePS, g_windowed and
// g_controls are script globals that reset on every launch (videoModes.as:43-45,
// docs/spec/90-synthesis.md "Persistence"). Saving them, and the enhancements'
// own switches, is enhancement E6 (docs/planning/2026-09-27-penumbra-port.md).
//
// The file is read tolerantly: a missing file, a broken one, or a missing or
// mistyped field each falls back to the default for exactly what is missing,
// so a hand-edited file degrades field by field rather than all at once. It is
// written atomically (a temporary, then a rename), because a settings file cut
// short by a crash would otherwise cost the player every binding they made.

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace Penumbra::Render {

// What a player can do, by meaning. The scripts read these as fixed keys for
// player 0 and fixed joystick buttons for everyone (playerInput.as:56-305).
enum class ControlAction : int {
    Left = 0,
    Right,
    Up,
    Down,
    Jump,       // P1: K_CTRL (K_UP jumps too, in the script)   pad: JK_03
    Sword,      // P1: K_S                                       pad: JK_04
    Fire,       // P1: K_D                                       pad: JK_02
    Light,      // P1: K_SPACE                                   pad: JK_01
    Confirm,    // P1: K_ENTER (K_LMOUSE/K_RMOUSE too, in the script)  pad: JK_10 (also P2's summon)
    Cancel,     // P1: K_ESC                                     pad: JK_09
    Count
};
inline constexpr int kControlActionCount = static_cast<int>(ControlAction::Count);

// The JSON name of an action ("left", "jump", ...).
const char* ControlActionName(ControlAction action);

// One player's keyboard: for each action, the GLFW key codes (Supersonic::Key
// mirrors them) that perform it. Any one of them held is the action held; an
// empty list leaves the action unbound.
struct KeyBindings {
    std::array<std::vector<int>, kControlActionCount> keys{};

    std::vector<int>& operator[](ControlAction action) { return keys[static_cast<int>(action)]; }
    const std::vector<int>& operator[](ControlAction action) const { return keys[static_cast<int>(action)]; }
    bool operator==(const KeyBindings& other) const { return keys == other.keys; }
};

struct ControlSettings {
    // g_controls (videoModes.as:45), which the original forgot at every launch:
    // 0 = keyboard for player 1, joystick 0 for player 2 (the default);
    // 1 = joystick 0 for player 1, joystick 1 for player 2. The layer seeds the
    // ported script's switch with it and saves it back when the menu changes it.
    int joystickLayout = 0;
    // Player 1 (the wizard). The defaults are the original's fixed keys.
    KeyBindings player1;
    // The keyboard second player (enhancement E4), presented to the scripts as
    // the joystick player 2 reads. Off: player 2 needs a real pad, as in 2010.
    // Off by default on a phone, which has no keyboard: on, a Versus arena
    // would open with a player 2 nobody can move.
#ifdef PENUMBRA_MOBILE
    bool keyboardPlayer2 = false;
#else
    bool keyboardPlayer2 = true;
#endif
    KeyBindings player2;
    // Per-axis stick dead zone, applied before the Eth layer's own 0.01: a
    // modern stick rests a few percent off centre, and getInputDirection walks
    // on ANY nonzero x (playerInput.as:166-174).
    float stickDeadzone = 0.25f;
    // Where real pads go. true (E12, the default): the first pad goes to the
    // index player 1 reads - so one player with one pad plays the wizard and
    // drives the menu - and the second to player 2's. false (the original):
    // the first pad is joystick 0, which player 2 reads under the default
    // g_controls, so a lone pad played the princess (who must be summoned) and
    // could not drive the menu (it reads player 0). While the touch controls
    // are on, E22 decides instead: they are player 1 and the first pad is
    // player 2's (InputMapper::PadOrder); off again, this applies again.
    bool firstPadIsPlayer1 = true;
    // Joysticks GLFW has no gamepad mapping for, read by their own button
    // numbers (what winmm reported). Off by default: an unmapped HID device at
    // index 0 would otherwise shift every pad and make hasASecondController()
    // true on its own.
    bool rawJoysticks = false;

    bool operator==(const ControlSettings& other) const = default;

    // getPlayerJoystick(1) for this layout (playerInput.as:43-53): the index
    // player 2 reads.
    int Player2Pad() const { return joystickLayout == 0 ? 0 : 1; }
};

struct Settings {
    // 2: E23's automatic window (window.width/height 0 x 0) and
    // window.fullscreenRefresh. Nothing reads the number: a version-1 file is
    // read as it always was, and a field it lacks is automatic.
    static constexpr int kVersion = 2;

    // A render/Languages.hpp id (E24: "en", "de", "es", "fr", "it", "pt",
    // "ru", "tr", "uk", "ja" or "ar"); a file written before E24 holds "pt" or
    // "en" and reads as it always did.
    std::string language = "en";
    // The windowed size: a line of the options screen's mode list picked in a
    // window. 0 x 0 is automatic (E23, the default): the window fitted to the
    // monitor it opens on - the largest of the monitor's own shape within
    // kAutoWindowFraction of its work area, centred (render/WindowMode.hpp) -
    // and fitted again each time the game leaves fullscreen. Half a size is
    // automatic too; any other value is clamped to the window's range.
    int windowWidth = 0;
    int windowHeight = 0;
    // E23: a first launch covers the monitor, at the automatic mode below. A
    // file that says false keeps its window.
    bool fullscreen = true;
    // The display mode fullscreen runs at: a line of the options screen's mode
    // list picked while fullscreen, which switches the monitor to it as 0.7.12
    // did (Step 23). 0 x 0 is automatic: the desktop's own size - the monitor's
    // native one as the system runs it, which switches nothing but the rate
    // (E23: "Autom\xE1tico (melhor)", the list's first line, and the default,
    // so a desktop that changes resolution later is followed). Alt+Enter, the
    // options' switch and a fullscreen launch all use it; a monitor that does
    // not offer it gets the desktop's size instead, and the setting is kept for
    // one that does.
    int fullscreenWidth = 0;
    int fullscreenHeight = 0;
    // E23: the refresh rate fullscreen runs at, in Hz. 0 is automatic (the
    // default): the highest the monitor offers at the fullscreen size. A rate
    // the monitor does not offer at that size runs at the highest and is kept
    // for one that does. In a window the rate is the compositor's; this waits
    // for the next fullscreen. On a phone it is the display's own rate
    // (WindowControl::SetPreferredRefreshRate): 0 the highest, 60 to save the
    // battery.
    int fullscreenRefresh = 0;
    bool widescreen = true;             // E1: the logical view widens with the window
    float musicVolume = 1.0f;           // master volumes, 0..1, on top of the scripts' own
    float effectsVolume = 1.0f;
    bool pixelShaders = true;           // g_enablePS: 0 ("Ativa pixel shaders") is the default
    // E8: the world drawn between ticks on displays faster than 60 Hz
    // (render/Interpolation.hpp). Off: every frame shows the last tick, as
    // 0.7.12 showed one tick per 60 Hz vsync.
    bool smoothMotion = true;
    // E13: the pause (render/PauseMenu.hpp) opens by itself when the window
    // loses focus during play. Off: only Esc/Back open it. The original had
    // no pause, and played on behind another window.
    bool pauseOnFocusLoss = true;
    // E16: the on-screen touch controls (render/TouchControls.hpp). "auto" is
    // on in a mobile build (PENUMBRA_MOBILE) and off on the desktop; "on" and
    // "off" force them. The original was played with a keyboard and pads.
    std::string touchControls = "auto";
    ControlSettings controls;

    bool operator==(const Settings& other) const = default;

    // Everything at its default, in `language` (a Languages.hpp id; anything
    // else is English). main() passes SystemLanguage(): the language the
    // system is set to when the game speaks it, English otherwise (E24).
    static Settings Defaults(const std::string& language = "en");
    // The system's UI language as a Languages.hpp id, "en" when the game does
    // not speak it:
    //   a locale supplied with SetSystemLocale, when one was - the hook for a
    //     platform whose language is not in the environment: an Android
    //     activity (AConfiguration_getLanguage), an iOS or macOS app bundle
    //     (NSLocale.preferredLanguages), called before Defaults;
    //   Windows: the user's UI language (GetUserDefaultUILanguage's primary
    //     language, LanguageOfPrimaryLangId);
    //   elsewhere: the POSIX message locale, the first of LC_ALL, LC_MESSAGES
    //     and LANG that is set (a desktop session sets them).
    static std::string SystemLanguage();
    // SystemLanguage() == "pt": the start-up error boxes put Portuguese first
    // on a Portuguese system and English first everywhere else (eth/StartupErrors).
    static bool SystemLanguageIsPortuguese();
    // The hook above: a BCP 47 tag or a POSIX locale ("pt-BR", "pt_PT.UTF-8",
    // "en"). An empty string forgets it.
    static void SetSystemLocale(const std::string& locale);
    // A locale name's language: the part before the first '_', '-', '.' or
    // '@', any case, when it is a Languages.hpp id ("pt_BR" pt, "uk-UA" uk,
    // "en_GB.UTF-8" en); "en" for any other ("zh_CN", "C", "POSIX", "").
    static std::string LanguageOfLocale(const std::string& locale);
    // Windows' PRIMARYLANGID of a LANGID (LANG_GERMAN 0x07, ...) as a
    // Languages.hpp id; "en" for a language the game does not speak. Numbers,
    // so the table is tested off Windows too (Settings.cpp checks them against
    // winnt.h's LANG_* on Windows).
    static std::string LanguageOfPrimaryLangId(unsigned primaryLangId);
    // Whether a locale name is Portuguese: LanguageOfLocale(locale) == "pt".
    static bool LocaleIsPortuguese(const std::string& locale);
    static KeyBindings DefaultPlayer1Keys();
    static KeyBindings DefaultPlayer2Keys();

    static std::filesystem::path FilePath(const std::filesystem::path& userDir);

    // Reads <userDir>/settings.json over `defaults`. A missing file is not an
    // error; anything unreadable is reported through `warning` (may be null)
    // and replaced by its default.
    static Settings Load(const std::filesystem::path& userDir, const Settings& defaults,
                         std::string* warning = nullptr);
    // The same, from text.
    static Settings FromJson(const std::string& text, const Settings& defaults, std::string* warning = nullptr);

    // Writes <userDir>/settings.json atomically, creating the folder. False
    // (with `error`) when it cannot, e.g. an empty userDir.
    bool Save(const std::filesystem::path& userDir, std::string* error = nullptr) const;
    std::string ToJson() const;
};

// Key names for the file and a controls menu: "A", "Left", "LeftCtrl",
// "KeypadEnter", ... A raw code ("341") is read too. "" for a code the
// engine never polls.
std::string KeyName(int glfwKey);
// -1 when the name is neither a known name nor a valid key number.
int KeyFromName(const std::string& name);

} // namespace Penumbra::Render
