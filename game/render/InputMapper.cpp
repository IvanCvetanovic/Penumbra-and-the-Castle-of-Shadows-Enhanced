#include "render/InputMapper.hpp"

#include <algorithm>
#include <cmath>

#include <GLFW/glfw3.h>

#if defined(__ANDROID__)
#include "platform/Gamepads.hpp"
#endif

namespace Penumbra::Render {

namespace {

using namespace Penumbra::Eth;
namespace Pad = Supersonic::Pad;

// The KEY each physical key produces when nothing rebinds it: GS2D's VK table
// (G:Input/Win/gs2dWinInput.cpp:78-166) in GLFW's key codes. VK_CONTROL,
// VK_MENU, VK_SHIFT and VK_RETURN were either side; K_SUBTRACT/K_ADD were the
// keypad's; K_PLUS was VK_OEM_PLUS, the '=' key; K_0..K_9 the top row only.
struct IdentityKey {
    KEY key;
    int codes[2];
};
constexpr IdentityKey kIdentity[] = {
    {K_UP, {GLFW_KEY_UP, -1}},
    {K_DOWN, {GLFW_KEY_DOWN, -1}},
    {K_LEFT, {GLFW_KEY_LEFT, -1}},
    {K_RIGHT, {GLFW_KEY_RIGHT, -1}},
    {K_PAGEDOWN, {GLFW_KEY_PAGE_DOWN, -1}},
    {K_PAGEUP, {GLFW_KEY_PAGE_UP, -1}},
    {K_SPACE, {GLFW_KEY_SPACE, -1}},
    {K_ENTER, {GLFW_KEY_ENTER, GLFW_KEY_KP_ENTER}},
    {K_DELETE, {GLFW_KEY_DELETE, -1}},
    {K_HOME, {GLFW_KEY_HOME, -1}},
    {K_END, {GLFW_KEY_END, -1}},
    {K_INSERT, {GLFW_KEY_INSERT, -1}},
    {K_PAUSE, {GLFW_KEY_PAUSE, -1}},
    {K_ESC, {GLFW_KEY_ESCAPE, -1}},
    {K_BACK, {GLFW_KEY_BACKSPACE, -1}},
    {K_TAB, {GLFW_KEY_TAB, -1}},
    {K_PRINTSCREEN, {GLFW_KEY_PRINT_SCREEN, -1}},
    {K_SUBTRACT, {GLFW_KEY_KP_SUBTRACT, -1}},
    {K_ADD, {GLFW_KEY_KP_ADD, -1}},
    {K_MINUS, {GLFW_KEY_MINUS, -1}},
    {K_PLUS, {GLFW_KEY_EQUAL, -1}},
    {K_COMMA, {GLFW_KEY_COMMA, -1}},
    {K_DOT, {GLFW_KEY_PERIOD, -1}},
    {K_CTRL, {GLFW_KEY_LEFT_CONTROL, GLFW_KEY_RIGHT_CONTROL}},
    {K_ALT, {GLFW_KEY_LEFT_ALT, GLFW_KEY_RIGHT_ALT}},
    {K_SHIFT, {GLFW_KEY_LEFT_SHIFT, GLFW_KEY_RIGHT_SHIFT}},
};

// The KEY each of player 1's actions drives: what playerInput.as reads for
// player 0. K_UP also jumps and the mouse buttons also confirm, in the script
// itself (playerInput.as:182-189, 267-277), so those need nothing here.
constexpr KEY kPlayer1Key[kControlActionCount] = {
    K_LEFT, K_RIGHT, K_UP, K_DOWN, K_CTRL, K_S, K_D, K_SPACE, K_ENTER, K_ESC,
};

// The winmm button each action is on a joystick (playerInput.as:195-303).
// Directions are the stick, so they have none.
constexpr int kNoButton = -1;
constexpr int kActionButton[kControlActionCount] = {
    kNoButton, kNoButton, kNoButton, kNoButton, JK_03, JK_04, JK_02, JK_01, JK_10, JK_09,
};

// A mapped gamepad's buttons by meaning. The six the scripts read follow the
// actions above. The rest follow the numbering those six imply - the common
// PS-style USB pad, where 1-4 are triangle/circle/cross/square, 5-8 the
// shoulders, 9 select, 10 start, 11-12 the stick clicks - so nothing the
// scripts do not read lands on a button they do.
struct PadButton {
    int glfwButton;
    J_KEY key;
};
constexpr PadButton kPadButtons[] = {
    {Pad::A, JK_03},             // jump
    {Pad::B, JK_02},             // fire
    {Pad::X, JK_04},             // sword
    {Pad::Y, JK_01},             // light spell
    {Pad::LeftBumper, JK_07},
    {Pad::RightBumper, JK_08},
    {Pad::Back, JK_09},          // cancel
    {Pad::Start, JK_10},         // confirm, summon, start
    {Pad::LeftThumb, JK_11},
    {Pad::RightThumb, JK_12},
};
constexpr float kTriggerPressed = 0.0f;   // triggers rest at -1: past half travel

bool KeyDown(const RawDevices& raw, int code) {
    return code >= 0 && code < static_cast<int>(raw.keys.size()) && raw.keys[static_cast<std::size_t>(code)];
}

bool AnyDown(const RawDevices& raw, const std::vector<int>& codes) {
    return std::any_of(codes.begin(), codes.end(), [&raw](int code) { return KeyDown(raw, code); });
}

void Claim(std::array<bool, Supersonic::Key::Last + 1>& claimed, const KeyBindings& bindings) {
    for (const std::vector<int>& codes : bindings.keys) {
        for (const int code : codes) {
            if (code >= 0 && code < static_cast<int>(claimed.size())) claimed[static_cast<std::size_t>(code)] = true;
        }
    }
}

glm::vec2 ClampXY(const glm::vec2& xy) { return glm::clamp(xy, glm::vec2(-1.0f), glm::vec2(1.0f)); }

InputFrame::Pad MapRealPad(const RawPad& raw, float deadzone) {
    InputFrame::Pad pad;
    pad.connected = true;
    glm::vec2 dpad{0.0f};
    glm::vec2 stick{0.0f};
    if (raw.gamepad) {
        for (const PadButton& button : kPadButtons) {
            if (raw.buttons[static_cast<std::size_t>(button.glfwButton)]) pad.buttons[button.key] = true;
        }
        if (raw.axes[Pad::LeftTrigger] > kTriggerPressed) pad.buttons[JK_05] = true;
        if (raw.axes[Pad::RightTrigger] > kTriggerPressed) pad.buttons[JK_06] = true;
        dpad.x = (raw.buttons[Pad::DpadRight] ? 1.0f : 0.0f) - (raw.buttons[Pad::DpadLeft] ? 1.0f : 0.0f);
        dpad.y = (raw.buttons[Pad::DpadDown] ? 1.0f : 0.0f) - (raw.buttons[Pad::DpadUp] ? 1.0f : 0.0f);
        // GLFW's gamepad y is already down-positive, as winmm's was.
        stick = {raw.axes[Pad::LeftX], raw.axes[Pad::LeftY]};
    } else {
        pad.buttons = raw.rawButtons;
        dpad = glm::vec2(raw.rawHat);
        stick = raw.rawAxes;
    }
    // The D-pad wins when held: it is digital and exact, the stick is not.
    if (dpad != glm::vec2(0.0f)) {
        pad.xy = ClampXY(dpad);
    } else {
        pad.xy = ClampXY({InputMapper::ApplyDeadzone(stick.x, deadzone), InputMapper::ApplyDeadzone(stick.y, deadzone)});
    }
    return pad;
}

// cp1252's 0x80..0x9F, as Unicode (0 = unassigned).
constexpr std::uint32_t kCp1252High[32] = {
    0x20AC, 0,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0,      0x017D, 0,
    0,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0,      0x017E, 0x0178,
};

} // namespace

float InputMapper::ApplyDeadzone(float value, float deadzone) {
    if (!std::isfinite(value)) return 0.0f;
    const float magnitude = std::fabs(value);
    const float zone = std::clamp(deadzone, 0.0f, 0.99f);
    if (magnitude <= zone) return 0.0f;
    const float scaled = std::min(1.0f, (magnitude - zone) / (1.0f - zone));
    return value < 0.0f ? -scaled : scaled;
}

std::array<int, kMaxJoysticks> InputMapper::PadOrder(const ControlSettings& controls, int player2Pad) {
    std::array<int, kMaxJoysticks> order{};
    for (int i = 0; i < kMaxJoysticks; ++i) order[static_cast<std::size_t>(i)] = i;
    if (!controls.firstPadIsPlayer1) return order;

    // getPlayerJoystick gives the two players the two lowest indices, one
    // each (playerInput.as:43-53), so player 1's is whichever of 0 and 1
    // player 2's is not.
    const int player2 = std::clamp(player2Pad, 0, kMaxJoysticks - 1);
    const int player1 = player2 == 0 ? 1 : 0;
    std::size_t next = 0;
    order[next++] = player1;
    order[next++] = player2;
    for (int i = 0; i < kMaxJoysticks; ++i) {
        if (i != player1 && i != player2) order[next++] = i;
    }
    return order;
}

glm::vec2 InputMapper::WindowToLogical(const glm::vec2& window, const View& view) {
    if (!(view.scale > 0.0f)) return window;
    // Input::MousePosition() is in ViewportInfo::rect's coordinates, where the
    // game's image starts at imageOrigin (Magic Portals maps its pointer the
    // same way, ViewportInfo::ToLocal); 0 when the image is the window.
    return (window - view.imageOrigin - view.viewportMin) / view.scale;
}

std::string InputMapper::ToCp1252(const std::vector<std::uint32_t>& codepoints) {
    std::string out;
    for (const std::uint32_t c : codepoints) {
        if (c == '\t') {
            // Passed as WM_CHAR delivered it: InputState::Update expands it to
            // four spaces, as GS2D's GetLastCharInput did.
            out += '\t';
            continue;
        }
        if (c < 0x20 || (c >= 0x7F && c < 0xA0)) continue;   // controls: Backspace, Esc, Enter...
        if (c < 0x7F || (c >= 0xA0 && c <= 0xFF)) {
            out += static_cast<char>(static_cast<unsigned char>(c));
            continue;
        }
        char mapped = '?';
        for (std::uint32_t i = 0; i < 32; ++i) {
            if (kCp1252High[i] == c) {
                mapped = static_cast<char>(static_cast<unsigned char>(0x80 + i));
                break;
            }
        }
        out += mapped;
    }
    return out;
}

InputFrame InputMapper::Map(const RawDevices& raw, const ControlSettings& controls, const glm::vec2& cursor,
                            int player2Pad, const MenuButtons& menu) {
    InputFrame frame;
    frame.hasFocus = raw.focused;
    // The scripts only compare the two (menu.as:239 moves the cursor by
    // absolute + delta), so one logical position serves as both.
    frame.cursor = cursor;
    frame.cursorAbsolute = cursor;
    frame.typed = ToCp1252(raw.typed);

    // Keys a binding claims stop producing their own KEY, and the KEYs player
    // 1's actions drive come only from those actions' keys - which is what
    // makes a rebinding move a control rather than add a second one.
    std::array<bool, Supersonic::Key::Last + 1> claimed{};
    Claim(claimed, controls.player1);
    if (controls.keyboardPlayer2) Claim(claimed, controls.player2);
    std::array<bool, K_COUNT> owned{};
    for (const KEY key : kPlayer1Key) owned[key] = true;

    for (const IdentityKey& identity : kIdentity) {
        if (owned[identity.key]) continue;
        for (const int code : identity.codes) {
            if (code >= 0 && !claimed[static_cast<std::size_t>(code)] && KeyDown(raw, code)) frame.keys[identity.key] = true;
        }
    }
    for (int letter = 0; letter < 26; ++letter) {
        const KEY key = static_cast<KEY>(K_A + letter);
        const int code = GLFW_KEY_A + letter;
        if (!owned[key] && !claimed[static_cast<std::size_t>(code)] && KeyDown(raw, code)) frame.keys[key] = true;
    }
    for (int digit = 0; digit < 10; ++digit) {
        const KEY key = static_cast<KEY>(K_0 + digit);
        const int code = GLFW_KEY_0 + digit;
        if (!owned[key] && !claimed[static_cast<std::size_t>(code)] && KeyDown(raw, code)) frame.keys[key] = true;
    }
    for (int f = 0; f < 12; ++f) {
        const KEY key = static_cast<KEY>(K_F1 + f);
        const int code = GLFW_KEY_F1 + f;
        if (!owned[key] && !claimed[static_cast<std::size_t>(code)] && KeyDown(raw, code)) frame.keys[key] = true;
    }
    for (int action = 0; action < kControlActionCount; ++action) {
        if (AnyDown(raw, controls.player1.keys[static_cast<std::size_t>(action)])) {
            frame.keys[kPlayer1Key[action]] = true;
        }
    }
    frame.keys[K_LMOUSE] = raw.mouse[0];
    frame.keys[K_RMOUSE] = raw.mouse[1];
    frame.keys[K_MMOUSE] = raw.mouse[2];

    // Real pads, in winmm's order unless the settings put player 1 first.
    const std::array<int, kMaxJoysticks> order = PadOrder(controls, player2Pad);
    std::size_t next = 0;
    for (std::size_t i = 0; i < raw.pads.size(); ++i) {
        const RawPad& pad = raw.pads[i];
        if (!pad.gamepad && !controls.rawJoysticks) continue;
        if (next >= order.size()) break;
        InputFrame::Pad mapped = MapRealPad(pad, controls.stickDeadzone);
        // E14: A and B keep their own buttons (jump, fire) and add the
        // menus' confirm and cancel.
        if (menu.on && pad.gamepad) {
            const std::uint32_t bit = i < 32 ? (1u << i) : 0u;
            if (pad.buttons[Pad::A] && (menu.heldA & bit) == 0) mapped.buttons[JK_10] = true;
            if (pad.buttons[Pad::B] && (menu.heldB & bit) == 0) mapped.buttons[JK_09] = true;
        }
        frame.pads[static_cast<std::size_t>(order[next++])] = mapped;
    }

    // The keyboard second player, on the index player 2 reads. MERGED into a
    // real pad already there, never moved to the next free index: the next
    // index is player 1's pad. Connected with nothing held, or
    // hasASecondController() would flicker with every key.
    if (controls.keyboardPlayer2 && player2Pad >= 0 && player2Pad < kMaxJoysticks) {
        InputFrame::Pad& pad = frame.pads[static_cast<std::size_t>(player2Pad)];
        pad.connected = true;
        const KeyBindings& keys = controls.player2;
        const glm::vec2 xy{
            (AnyDown(raw, keys[ControlAction::Right]) ? 1.0f : 0.0f) - (AnyDown(raw, keys[ControlAction::Left]) ? 1.0f : 0.0f),
            (AnyDown(raw, keys[ControlAction::Down]) ? 1.0f : 0.0f) - (AnyDown(raw, keys[ControlAction::Up]) ? 1.0f : 0.0f),
        };
        pad.xy = ClampXY(pad.xy + xy);
        for (int action = 0; action < kControlActionCount; ++action) {
            const int button = kActionButton[action];
            if (button == kNoButton) continue;
            if (AnyDown(raw, keys.keys[static_cast<std::size_t>(action)])) pad.buttons[static_cast<std::size_t>(button)] = true;
        }
    }
    return frame;
}

RawDevices InputMapper::PollDevices() {
    using Supersonic::Input;
    RawDevices raw;
    // The engine's snapshot for this frame (InputPolling::Poll ran before the
    // ticks). The raw queries, not actions: bindings do not touch them.
    for (int code = 0; code < static_cast<int>(raw.keys.size()); ++code) {
        raw.keys[static_cast<std::size_t>(code)] = Input::IsKeyDown(code);
    }
    raw.mouse[0] = Input::IsMouseButtonDown(Supersonic::MouseButton::Left);
    raw.mouse[1] = Input::IsMouseButtonDown(Supersonic::MouseButton::Right);
    raw.mouse[2] = Input::IsMouseButtonDown(Supersonic::MouseButton::Middle);
    raw.mouseWindow = Input::MousePosition();
    raw.focused = Input::WindowFocused();
    const unsigned int* typed = Input::TypedCharacters();
    const int typedCount = Input::TypedCharacterCount();
    for (int i = 0; i < typedCount; ++i) raw.typed.push_back(typed[i]);

    // Every joystick, not only the first gamepad the engine polls: two players
    // need two pads, and PvP needs both at once (docs/spec/90-synthesis.md).
#if defined(__ANDROID__)
    // No GLFW on Android: the engine's pads (platform/Gamepads.hpp), already in
    // GLFW's standard layout, in the order they were first heard from. Android
    // maps every pad it lists, so there is no unmapped (raw) case.
    for (int index = 0; index < Supersonic::Gamepads::Count(); ++index) {
        Supersonic::GamepadState state;
        if (!Supersonic::Gamepads::Get(index, state)) continue;
        RawPad pad;
        pad.gamepad = true;
        for (int b = 0; b < Pad::ButtonCount; ++b) pad.buttons[static_cast<std::size_t>(b)] = state.buttons[b];
        for (int a = 0; a < Pad::AxisCount; ++a) pad.axes[static_cast<std::size_t>(a)] = state.axes[a];
        raw.pads.push_back(pad);
    }
#else
    for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid) {
        if (glfwJoystickPresent(jid) != GLFW_TRUE) continue;
        RawPad pad;
        GLFWgamepadstate state{};
        if (glfwJoystickIsGamepad(jid) == GLFW_TRUE && glfwGetGamepadState(jid, &state) == GLFW_TRUE) {
            pad.gamepad = true;
            for (int b = 0; b < Pad::ButtonCount; ++b) pad.buttons[static_cast<std::size_t>(b)] = state.buttons[b] == GLFW_PRESS;
            for (int a = 0; a < Pad::AxisCount; ++a) pad.axes[static_cast<std::size_t>(a)] = state.axes[a];
        } else {
            pad.gamepad = false;
            int buttonCount = 0;
            const unsigned char* buttons = glfwGetJoystickButtons(jid, &buttonCount);
            for (int b = 0; buttons != nullptr && b < buttonCount && b < 32; ++b) {
                pad.rawButtons[static_cast<std::size_t>(b)] = buttons[b] == GLFW_PRESS;
            }
            int axisCount = 0;
            const float* axes = glfwGetJoystickAxes(jid, &axisCount);
            if (axes != nullptr && axisCount >= 2) pad.rawAxes = {axes[0], axes[1]};
            int hatCount = 0;
            const unsigned char* hats = glfwGetJoystickHats(jid, &hatCount);
            if (hats != nullptr && hatCount >= 1) {
                const unsigned char hat = hats[0];
                pad.rawHat.x = ((hat & GLFW_HAT_RIGHT) ? 1 : 0) - ((hat & GLFW_HAT_LEFT) ? 1 : 0);
                pad.rawHat.y = ((hat & GLFW_HAT_DOWN) ? 1 : 0) - ((hat & GLFW_HAT_UP) ? 1 : 0);
            }
        }
        raw.pads.push_back(pad);
    }
#endif
    return raw;
}

void InputMapper::SetPlayer2Pad(int index) { m_player2Pad = std::clamp(index, 0, kMaxJoysticks - 1); }

MenuButtons InputMapper::stepMenuButtons(const RawDevices& raw) {
    if (!m_menuMode) {
        m_menuWasOn = false;
        m_menuHeldA = 0;
        m_menuHeldB = 0;
        return {};
    }
    std::uint32_t heldA = 0;
    std::uint32_t heldB = 0;
    for (std::size_t i = 0; i < raw.pads.size() && i < 32; ++i) {
        if (!raw.pads[i].gamepad) continue;
        const std::uint32_t bit = 1u << i;
        if (raw.pads[i].buttons[Pad::A]) heldA |= bit;
        if (raw.pads[i].buttons[Pad::B]) heldB |= bit;
    }
    if (!m_menuWasOn) {
        // Menu mode has just begun: what is held now was pressed before it.
        m_menuWasOn = true;
        m_menuHeldA = heldA;
        m_menuHeldB = heldB;
    } else {
        // Released since: aliased from its next press.
        m_menuHeldA &= heldA;
        m_menuHeldB &= heldB;
    }
    return MenuButtons{true, m_menuHeldA, m_menuHeldB};
}

InputFrame InputMapper::BuildTick(const View& view) { return BuildTick(PollDevices(), view); }

InputFrame InputMapper::BuildTick(const RawDevices& raw, const View& view) {
    // The real mouse wins whenever it moves; otherwise the cursor stays where
    // the scripts last put it.
    if (!m_haveMouse || raw.mouseWindow != m_lastMouseWindow) {
        m_cursor = WindowToLogical(raw.mouseWindow, view);
        m_lastMouseWindow = raw.mouseWindow;
        m_haveMouse = true;
    }

    // What was typed goes to the first tick of the frame and to no other: the
    // engine's characters stay readable for the whole frame, so a frame
    // running two ticks would type everything twice.
    RawDevices adjusted = raw;
    if (m_ticksThisFrame == 0) {
        adjusted.typed = m_pendingTyped;
        adjusted.typed.insert(adjusted.typed.end(), raw.typed.begin(), raw.typed.end());
    } else {
        adjusted.typed.clear();
    }
    m_pendingTyped.clear();

    InputFrame frame = Map(adjusted, m_controls, m_cursor, m_player2Pad, stepMenuButtons(raw));

    // A press made in a frame that ran no tick is handed to this one, so it is
    // seen for at least one Ethanon frame (HIT, then RELEASE).
    for (int k = 0; k < K_COUNT; ++k) {
        if (m_latchedKeys[static_cast<std::size_t>(k)]) frame.keys[static_cast<std::size_t>(k)] = true;
    }
    for (std::size_t p = 0; p < m_latchedButtons.size(); ++p) {
        if (!frame.pads[p].connected) continue;
        for (std::size_t b = 0; b < m_latchedButtons[p].size(); ++b) {
            if (m_latchedButtons[p][b]) frame.pads[p].buttons[b] = true;
        }
    }
    m_latchedKeys = {};
    m_latchedButtons = {};

    m_lastTick = frame;
    ++m_ticksThisFrame;
    return frame;
}

void InputMapper::EndFrame() {
    // A frame that ran a tick has nothing to latch: skip polling every pad a
    // second time (at 144 Hz that would double the XInput reads for nothing).
    if (m_ticksThisFrame > 0) {
        m_ticksThisFrame = 0;
        return;
    }
    EndFrame(PollDevices());
}

void InputMapper::EndFrame(const RawDevices& raw) {
    if (m_ticksThisFrame == 0) {
        // No tick saw this frame. Only NEW presses are latched: a key already
        // down at the last tick is held, and the next tick sees it anyway if
        // it still is - latching it would stretch every release by a tick.
        const InputFrame now = Map(raw, m_controls, m_cursor, m_player2Pad, stepMenuButtons(raw));
        for (std::size_t k = 0; k < now.keys.size(); ++k) {
            if (now.keys[k] && !m_lastTick.keys[k]) m_latchedKeys[k] = true;
        }
        for (std::size_t p = 0; p < now.pads.size(); ++p) {
            for (std::size_t b = 0; b < now.pads[p].buttons.size(); ++b) {
                if (now.pads[p].buttons[b] && !m_lastTick.pads[p].buttons[b]) m_latchedButtons[p][b] = true;
            }
        }
        for (const std::uint32_t c : raw.typed) {
            if (m_pendingTyped.size() >= kMaxPendingTyped) break;
            m_pendingTyped.push_back(c);
        }
    }
    m_ticksThisFrame = 0;
}

void InputMapper::WarpCursor(const glm::vec2& logical, const View& view) {
    m_cursor = glm::clamp(logical, glm::vec2(0.0f), view.logicalScreen);
}

} // namespace Penumbra::Render
