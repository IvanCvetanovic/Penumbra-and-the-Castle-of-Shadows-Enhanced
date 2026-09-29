#pragma once

// What the devices said, turned into the one Eth::InputFrame an Ethanon frame
// reads (eth/Input.hpp). This is where the input enhancements live, so the
// ported scripts keep reading the original's keys and winmm buttons:
//
//  - KEYBOARD. Physical keys become Eth KEYs by the original's VK table
//    (G:Input/Win/gs2dWinInput.cpp:78-166): K_CTRL/K_ALT/K_SHIFT are either
//    side, K_ENTER either Enter. Player 1's actions are rebindable: the KEY an
//    action drives (K_S for the sword...) comes only from that action's keys.
//  - GAMEPADS (E3), mapped by MEANING onto the winmm numbers the scripts read
//    (playerInput.as:176-305): A = JK_03 jump, X = JK_04 sword, B = JK_02 fire,
//    Y = JK_01 light, Start = JK_10 confirm/summon, Back = JK_09 cancel. Left
//    stick or D-pad is xy, y DOWN, with a per-axis dead zone.
//  - A KEYBOARD SECOND PLAYER (E4) presented as the joystick player 2 reads
//    (getPlayerJoystick(1): index 0 under the default g_controls,
//    playerInput.as:43-53), merged into a real pad on that index if there is
//    one, and connected whenever it is enabled so hasASecondController()
//    (playerInput.as:159-164) sees a second controller. A key bound to player 2
//    no longer produces its own KEY, so J walking the princess left does not
//    also hold K_J, which setupScene.as:393 reads to show the joystick icons.
//  - TOUCH IS PLAYER 1 (E22, SetTouchPlaysPlayer1): while the on-screen
//    touch controls are on they play player 1, and real pads start at the
//    index player 2 reads (PadOrder), so a phone with one Bluetooth pad has
//    two players: Versus opens, the co-op princess can be summoned.
//  - MENU MODE (E14, SetMenuMode): in the screens laid out as menus a real
//    gamepad's A ALSO presses JK_10 and its B ALSO JK_09. The original's
//    menus confirm only on Start and cancel only on Back
//    (getConfirmButtonStatus/getCancelButtonStatus, playerInput.as:267-305);
//    a player with a modern pad presses A and B.
//
// Map() is pure: raw states in, frame out, so a suite checks the mapping
// without a window. InputMapper around it adds what spans frames: the latch
// that hands a press made during a frame that ran no tick to the next tick,
// typed text handed to exactly one tick, and the virtual cursor that the
// scripts' SetCursorPos moves (menu.as:239, videoModes.as:61).

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "core/Input.hpp"
#include "eth/Input.hpp"
#include "render/Settings.hpp"
#include "render/View.hpp"

namespace Penumbra::Render {

// One joystick as GLFW reported it.
struct RawPad {
    // GLFW had a gamepad mapping: the fields below are its standard layout
    // (Supersonic::Pad mirrors GLFW_GAMEPAD_*). Otherwise the raw* fields hold
    // the device's own numbering, which is what winmm reported in 2010.
    bool gamepad = true;
    std::array<bool, Supersonic::Pad::ButtonCount> buttons{};
    std::array<float, Supersonic::Pad::AxisCount> axes{};   // triggers rest at -1
    std::array<bool, 32> rawButtons{};
    glm::vec2 rawAxes{0.0f};                                 // axes 0 and 1, y down
    glm::ivec2 rawHat{0};                                    // hat 0 as -1/0/1, y down
};

// One frame of the devices, as the engine and GLFW reported them.
struct RawDevices {
    std::array<bool, Supersonic::Key::Last + 1> keys{};     // by GLFW key code
    std::array<bool, 3> mouse{};                             // left, right, middle
    glm::vec2 mouseWindow{0.0f};                             // window pixels (glfwGetCursorPos)
    bool focused = true;
    std::vector<RawPad> pads;                                // present joysticks, GLFW id order
    std::vector<std::uint32_t> typed;                        // codepoints typed this frame
};

// E14: what menu mode adds to one frame. Only MAPPED gamepads (RawPad::gamepad)
// are aliased: an unmapped joystick's numbering is the device's own, and the
// keyboard second player's keys are not a pad's A and B.
struct MenuButtons {
    bool on = false;
    // Bit i: the i-th present joystick (RawDevices::pads order) has held A
    // (heldA) or B (heldB) since before menu mode began, and that button is
    // not aliased until it is released. Without this, a scene change made
    // with A held - Main menu picked with A in the pause, a death into
    // gameover.esc with B held - would press JK_10/JK_09 out of nowhere, and
    // the new screen's first frame would read it as a fresh confirm or cancel.
    std::uint32_t heldA = 0;
    std::uint32_t heldB = 0;
};

class InputMapper {
public:
    // ---- The pure mapping ---------------------------------------------------

    // One frame from the devices. `cursor` is the (virtual) cursor in logical
    // screen pixels; `player2Pad` is the index player 2 reads,
    // getPlayerJoystick(1); `menu` is E14's menu mode (off by default);
    // `touchIsPlayer1` is E22 (PadOrder).
    static Eth::InputFrame Map(const RawDevices& raw, const ControlSettings& controls, const glm::vec2& cursor,
                               int player2Pad, const MenuButtons& menu = {}, bool touchIsPlayer1 = false);

    // PadOrder's entry for "no index": the real pads from there on are not
    // presented to the scripts at all.
    static constexpr int kNoPad = -1;

    // Which pad index each real pad takes, in order (kNoPad ends the list):
    //  - 0,1,2,3 as winmm numbered them;
    //  - with controls.firstPadIsPlayer1 (E12), player 1's index first, then
    //    player 2's, then the rest;
    //  - while the touch controls are on (touchIsPlayer1, E22, whatever E12
    //    says), player 2's index first, then the indices neither player reads,
    //    and never player 1's: player 1 plays on the touchscreen and the first
    //    pad plays player 2 (Versus on a phone, the co-op princess). A second
    //    pad changes nothing for player 1 either: it would put the wizard
    //    under a finger and a pad at once, and a pad set down on a sofa would
    //    walk him; two pad players turn the touch controls off (E20's row) and
    //    E12 gives them one each.
    static std::array<int, Eth::kMaxJoysticks> PadOrder(const ControlSettings& controls, int player2Pad,
                                                        bool touchIsPlayer1 = false);

    // Input::MousePosition() pixels -> logical screen pixels:
    // (window - imageOrigin - viewportMin) / scale (render/View.hpp, POINTER).
    static glm::vec2 WindowToLogical(const glm::vec2& window, const View& view);

    // Whether a pointer at `window` (Input::MousePosition() coordinates) is on
    // the image but outside what is shown in it: over the bars of a
    // pillarboxed or letterboxed frame (CameraRig::Bars), where the scripts'
    // cursor sprite is drawn under a bar and the player would have no pointer
    // at all. What is shown is the logical screen's box, or wider under E1's
    // open sides (View::ShownMin/ShownMax), where the cursor sprite is seen.
    // Both boxes are half-open - the shown box's min is its first pixel, its
    // max the bar's - and a pointer off the image (an editor's panels) is not
    // over the bars.
    static bool PointerOverBars(const glm::vec2& window, const View& view);

    // Typed codepoints as the WM_CHAR bytes GetLastCharInput read: cp1252,
    // '?' for what cp1252 lacks, Tab kept (InputState::Update expands it to
    // four spaces), Backspace/Esc/Enter and other controls dropped
    // (docs/spec/30-ethanon-runtime.md §4.1).
    static std::string ToCp1252(const std::vector<std::uint32_t>& codepoints);

    // Per-axis dead zone, rescaled so full deflection is still 1.
    static float ApplyDeadzone(float value, float deadzone);

    // ---- The devices ----------------------------------------------------------

    // The engine's raw snapshot (Supersonic::Input: keys, mouse, focus, typed
    // characters) and every present GLFW joystick. Main thread, GLFW
    // initialised, i.e. inside the app's frame.
    static RawDevices PollDevices();

    // ---- Per tick and per frame ------------------------------------------------

    void SetControls(const ControlSettings& controls) { m_controls = controls; }
    const ControlSettings& Controls() const { return m_controls; }
    // getPlayerJoystick(1) of the ported playerInput: 0 while g_controls is 0.
    void SetPlayer2Pad(int index);
    int Player2Pad() const { return m_player2Pad; }
    // E22: the touch controls are on, so they are player 1 and the first pad
    // is player 2's (PadOrder). The layer sets it wherever the controls go on
    // or off, E20's options row included; off, E12's order is back from the
    // next tick.
    void SetTouchPlaysPlayer1(bool on);
    bool TouchPlaysPlayer1() const { return m_touchIsPlayer1; }
    // E14: on in the screens laid out as menus (the layer's
    // IsFixedLayoutScene: menu.esc, arena_select.esc, videoModes.esc,
    // gameover.esc), before BuildTick. A or B already held when it turns on
    // is aliased only from its next press (MenuButtons).
    void SetMenuMode(bool on) { m_menuMode = on; }
    bool MenuMode() const { return m_menuMode; }

    // OnFixedUpdate, once per tick, before InputState::Update.
    Eth::InputFrame BuildTick(const View& view);
    Eth::InputFrame BuildTick(const RawDevices& raw, const View& view);

    // OnUpdate, once per frame, after the frame's ticks: latches what a frame
    // that ran no tick would otherwise lose.
    void EndFrame();
    void EndFrame(const RawDevices& raw);

    // An InputState::TakeCursorRequest, in the same logical pixels the frame
    // reports (cursorAbsolute is the cursor). Clamped to the logical screen,
    // which the OS cursor the original moved could not leave either - to what
    // is shown of it under E1's open sides (View::ShownLogicalMin/Max).
    void WarpCursor(const glm::vec2& logical, const View& view);
    glm::vec2 Cursor() const { return m_cursor; }

private:
    static constexpr std::size_t kMaxPendingTyped = 64;

    // E14: steps the held-since-before bits against these devices and returns
    // what Map applies (nothing while menu mode is off).
    MenuButtons stepMenuButtons(const RawDevices& raw);

    ControlSettings m_controls;
    int m_player2Pad = 0;
    bool m_touchIsPlayer1 = false;
    bool m_menuMode = false;
    bool m_menuWasOn = false;       // menu mode was on at the last step
    std::uint32_t m_menuHeldA = 0;
    std::uint32_t m_menuHeldB = 0;

    glm::vec2 m_cursor{0.0f};
    glm::vec2 m_lastMouseWindow{0.0f};
    bool m_haveMouse = false;

    int m_ticksThisFrame = 0;
    Eth::InputFrame m_lastTick;
    std::array<bool, Eth::K_COUNT> m_latchedKeys{};
    std::array<std::array<bool, 32>, Eth::kMaxJoysticks> m_latchedButtons{};
    std::vector<std::uint32_t> m_pendingTyped;
};

} // namespace Penumbra::Render
