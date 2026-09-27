#pragma once

// ETHInput: Ethanon's key and joystick state machines, fed once per Ethanon
// frame from what the layer read off the devices.
//
// Keys and buttons go UP -> HIT -> DOWN -> RELEASE -> UP, one step per frame
// (DLL@0x10007290). Joysticks are winmm-style: up to 4, axes in [-1,1] with a
// 0.01 dead zone, buttons numbered JK_01..JK_32, and JK_UP/DOWN/LEFT/RIGHT
// synthesised from the stick at |axis| >= 0.8 with their own state machines
// (docs/spec/30-ethanon-runtime.md §4.1).
//
// The layer decides what physical input becomes which KEY or JK_n. That is
// where the enhancements live - modern pads mapped by meaning (A = jump...),
// a keyboard second player presented to the scripts as joystick 1 - so the
// ported scripts keep reading the original's buttons.

#include <array>

#include "eth/EthTypes.hpp"

namespace Penumbra::Eth {

inline constexpr int kMaxJoysticks = 4;

// What the devices said this frame.
struct InputFrame {
    std::array<bool, K_COUNT> keys{};
    vector2 cursor{0.0f};           // client-area pixels, in the logical screen
    vector2 cursorAbsolute{0.0f};   // desktop pixels (only compared, never shown)
    bool hasFocus = true;
    struct Pad {
        bool connected = false;
        vector2 xy{0.0f};           // raw, [-1,1], y DOWN
        std::array<bool, 32> buttons{};
    };
    std::array<Pad, kMaxJoysticks> pads{};
    string typed;                   // characters typed this frame (WM_CHAR), cp1252
};

class InputState {
public:
    // Advance every state machine by one frame.
    void Update(const InputFrame& frame);

    KEY_STATE GetKeyState(KEY key) const;
    bool KeyDown(KEY key) const;

    J_STATUS GetJoystickStatus(uint pad) const;
    bool DetectJoysticks();                 // re-enumerate: true if any is connected
    KEY_STATE JoyButtonState(uint pad, J_KEY key) const;
    bool JoyButtonDown(uint pad, J_KEY key) const;
    vector2 GetJoystickXY(uint pad) const;  // dead zone applied
    uint GetNumJoyButtons(uint pad) const;
    uint GetMaxJoysticks() const { return kMaxJoysticks; }

    vector2 GetCursorPos() const { return m_cursor; }
    vector2 GetCursorAbsolutePos() const { return m_cursorAbsolute; }
    // SetCursorPos takes DESKTOP coordinates and applies at the next Update;
    // the layer reads the request and moves its virtual cursor.
    void SetCursorPos(const vector2& absolute);
    bool TakeCursorRequest(vector2& absolute);
    string GetLastCharInput() const { return m_lastChar; }

private:
    static KEY_STATE Step(KEY_STATE previous, bool down);

    std::array<KEY_STATE, K_COUNT> m_keys{};
    struct PadState {
        bool connected = false;
        vector2 xy{0.0f};
        std::array<KEY_STATE, JK_COUNT> buttons{};
    };
    std::array<PadState, kMaxJoysticks> m_pads{};
    vector2 m_cursor{0.0f};
    vector2 m_cursorAbsolute{0.0f};
    bool m_hasCursorRequest = false;
    vector2 m_cursorRequest{0.0f};
    string m_lastChar;
};

} // namespace Penumbra::Eth
