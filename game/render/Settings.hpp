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
    bool keyboardPlayer2 = true;
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
    // could not drive the menu (it reads player 0).
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
    static constexpr int kVersion = 1;

    std::string language = "en";        // "pt" or "en"
    int windowWidth = 1366;
    int windowHeight = 768;
    bool fullscreen = false;
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
    ControlSettings controls;

    bool operator==(const Settings& other) const = default;

    // Everything at its default. The language follows the system's: Portuguese
    // for a Portuguese Windows UI, English otherwise.
    static Settings Defaults(bool systemIsPortuguese);
    // Whether the Windows UI language is Portuguese (any region). False
    // elsewhere and on other platforms.
    static bool SystemLanguageIsPortuguese();
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
