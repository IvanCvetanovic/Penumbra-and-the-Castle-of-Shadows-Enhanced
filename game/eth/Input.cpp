// ETHInput: the key and joystick state machines (docs/spec/30-ethanon-runtime.md
// §4.1). GameSpaceLib's own code is closed; the behaviour here is what its
// disassembly shows (DLL@0x10007290 the key machine, DLL@0x10007200 the
// synthesised stick buttons, DLL@0x100077b0 KeyDown).

#include "eth/Input.hpp"

#include <cmath>

namespace Penumbra::Eth {

namespace {

// winmm axes pass through a 0.01 dead zone and are clamped to [-1, 1].
constexpr float kDeadZone = 0.01f;
// JK_UP/DOWN/LEFT/RIGHT fire at |axis| >= 0.8 (constants at DLL@0x1002f1d0).
constexpr float kArrowThreshold = 0.8f;

float ApplyDeadZone(const float value) {
    if (!(value == value)) return 0.0f;   // a NaN from a broken driver reads as centred
    if (std::fabs(value) < kDeadZone) return 0.0f;
    return value < -1.0f ? -1.0f : (value > 1.0f ? 1.0f : value);
}

} // namespace

KEY_STATE InputState::Step(const KEY_STATE previous, const bool down) {
    if (down) return (previous == KS_HIT || previous == KS_DOWN) ? KS_DOWN : KS_HIT;
    return (previous == KS_HIT || previous == KS_DOWN) ? KS_RELEASE : KS_UP;
}

void InputState::Update(const InputFrame& frame) {
    // Without focus GameSpaceLib read every key as up AND reset its counters,
    // so a key held while the window lost focus does not report a RELEASE.
    for (int k = 0; k < K_COUNT; ++k) {
        const auto index = static_cast<std::size_t>(k);
        m_keys[index] = frame.hasFocus ? Step(m_keys[index], frame.keys[index]) : KS_UP;
    }

    for (std::size_t p = 0; p < m_pads.size(); ++p) {
        const InputFrame::Pad& in = frame.pads[p];
        PadState& pad = m_pads[p];
        pad.connected = in.connected;
        pad.xy = in.connected ? vector2(ApplyDeadZone(in.xy.x), ApplyDeadZone(in.xy.y)) : vector2(0.0f);
        for (std::size_t b = 0; b < in.buttons.size(); ++b) {
            pad.buttons[b] = Step(pad.buttons[b], in.connected && in.buttons[b]);
        }
        // y is DOWN, as winmm reported it: up is the negative end.
        pad.buttons[JK_UP] = Step(pad.buttons[JK_UP], pad.xy.y <= -kArrowThreshold);
        pad.buttons[JK_DOWN] = Step(pad.buttons[JK_DOWN], pad.xy.y >= kArrowThreshold);
        pad.buttons[JK_LEFT] = Step(pad.buttons[JK_LEFT], pad.xy.x <= -kArrowThreshold);
        pad.buttons[JK_RIGHT] = Step(pad.buttons[JK_RIGHT], pad.xy.x >= kArrowThreshold);
    }

    m_cursor = frame.cursor;
    m_cursorAbsolute = frame.cursorAbsolute;

    // GetLastCharInput: the frame's last WM_CHAR, Backspace, Escape and Enter
    // excluded, Tab expanded to four spaces (docs/spec/30-ethanon-runtime.md §4.1).
    m_lastChar.clear();
    for (const char c : frame.typed) {
        if (c == '\b' || c == '\x1b' || c == '\r' || c == '\n') continue;
        m_lastChar = (c == '\t') ? string("    ") : string(1, c);
    }
}

KEY_STATE InputState::GetKeyState(const KEY key) const {
    if (key < 0 || key >= K_COUNT) return KS_UP;
    return m_keys[static_cast<std::size_t>(key)];
}

bool InputState::KeyDown(const KEY key) const {
    const KEY_STATE state = GetKeyState(key);
    return state == KS_HIT || state == KS_DOWN;
}

J_STATUS InputState::GetJoystickStatus(const uint pad) const {
    if (pad >= m_pads.size()) return JS_INVALID;
    return m_pads[pad].connected ? JS_DETECTED : JS_NOTDETECTED;
}

bool InputState::DetectJoysticks() {
    // The layer re-reads the devices every frame, so the enumeration the
    // scripts ask for is already current.
    for (const PadState& pad : m_pads) {
        if (pad.connected) return true;
    }
    return false;
}

KEY_STATE InputState::JoyButtonState(const uint pad, const J_KEY key) const {
    if (pad >= m_pads.size() || key < 0 || key >= JK_COUNT) return KS_UP;
    return m_pads[pad].buttons[static_cast<std::size_t>(key)];
}

bool InputState::JoyButtonDown(const uint pad, const J_KEY key) const {
    const KEY_STATE state = JoyButtonState(pad, key);
    return state == KS_HIT || state == KS_DOWN;
}

vector2 InputState::GetJoystickXY(const uint pad) const {
    if (pad >= m_pads.size()) return vector2(0.0f);
    return m_pads[pad].xy;
}

uint InputState::GetNumJoyButtons(const uint pad) const {
    // The layer does not report a pad's own button count; a connected pad
    // offers all 32 winmm buttons (the unused ones simply read UP).
    if (pad >= m_pads.size() || !m_pads[pad].connected) return 0;
    return 32;
}

void InputState::SetCursorPos(const vector2& absolute) {
    m_hasCursorRequest = true;
    m_cursorRequest = absolute;
}

bool InputState::TakeCursorRequest(vector2& absolute) {
    if (!m_hasCursorRequest) return false;
    absolute = m_cursorRequest;
    m_hasCursorRequest = false;
    return true;
}

} // namespace Penumbra::Eth
