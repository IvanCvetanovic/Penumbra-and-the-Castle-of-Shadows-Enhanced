#pragma once

// The value types and enums of the Ethanon 0.7.12 script API, under the names
// the original's AngelScript used, so a ported line reads like the line it came
// from. Values are the engine's own (reference/eth-0.7.12/src/ETHScriptObjRegister.cpp:406-587,
// reference/gs2d-r485/gs2d.h), because a few of them are written into scenes
// and compared numerically by the scripts.

#include <cstdint>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>

namespace Penumbra::Eth {

using uint = std::uint32_t;
using uint8 = std::uint8_t;
using string = std::string;       // cp1252 bytes, exactly as the scripts held them
using vector2 = glm::vec2;
using vector3 = glm::vec3;

struct collisionBox {
    vector3 pos{0.0f};
    vector3 size{0.0f};
};

enum PIXEL_FORMAT : int { PF32BIT = 0, PF16BIT = 1, PFUNKNOWN = 2 };

struct videoMode {
    uint width = 0;
    uint height = 0;
    PIXEL_FORMAT format = PF32BIT;
};

// ETHEntity::GetType and the <Entity type="..."> attribute.
enum ENTITY_TYPE : int {
    ET_HORIZONTAL = 0,
    ET_GROUND_DECAL = 1,
    ET_VERTICAL = 2,
    ET_OVERALL = 3,
    ET_OPAQUE_DECAL = 4,
    ET_LAYERABLE = 5,
};

// CheckCustomData.
enum DATA_TYPE : int { DT_NODATA = 0, DT_FLOAT = 1, DT_INT = 2, DT_UINT = 3, DT_STRING = 4 };

// Every key and joystick button goes UP -> HIT (first frame down) -> DOWN ->
// RELEASE (first frame up) -> UP. KeyDown() is HIT or DOWN.
enum KEY_STATE : int { KS_UP = 0, KS_HIT = 1, KS_DOWN = 2, KS_RELEASE = 3 };

enum J_STATUS : int { JS_DETECTED = 0, JS_NOTDETECTED = 1, JS_INVALID = 2 };

// Joystick buttons: winmm button bits 1..32, and four more synthesised from the
// stick crossing +/-0.8 (DLL@0x10007200).
enum J_KEY : int {
    JK_01 = 0, JK_02, JK_03, JK_04, JK_05, JK_06, JK_07, JK_08, JK_09, JK_10,
    JK_11, JK_12, JK_13, JK_14, JK_15, JK_16, JK_17, JK_18, JK_19, JK_20,
    JK_21, JK_22, JK_23, JK_24, JK_25, JK_26, JK_27, JK_28, JK_29, JK_30,
    JK_31, JK_32,
    JK_UP, JK_DOWN, JK_LEFT, JK_RIGHT,
    JK_COUNT
};

// The keys the scripts can name. Only the values matter to the port, and only
// as indices into the input state; the layer maps physical keys onto them.
enum KEY : int {
    K_UP = 0, K_DOWN, K_LEFT, K_RIGHT,
    K_PAGEDOWN, K_PAGEUP, K_SPACE, K_ENTER, K_DELETE, K_HOME, K_END, K_INSERT,
    K_PAUSE, K_ESC, K_BACK, K_TAB, K_PRINTSCREEN, K_SUBTRACT, K_ADD,
    K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10, K_F11, K_F12,
    K_A, K_B, K_C, K_D, K_E, K_F, K_G, K_H, K_I, K_J, K_K, K_L, K_M,
    K_N, K_O, K_P, K_Q, K_R, K_S, K_T, K_U, K_V, K_W, K_X, K_Y, K_Z,
    K_0, K_1, K_2, K_3, K_4, K_5, K_6, K_7, K_8, K_9,
    K_MINUS, K_PLUS, K_COMMA, K_DOT,
    K_CTRL, K_ALT, K_SHIFT,
    K_LMOUSE, K_RMOUSE, K_MMOUSE,
    K_COUNT,
    // Aliases the original registered under a second name.
    K_RETURN = K_ENTER,
    K_BACKSPACE = K_BACK,
};

// GS_ALPHA_MODE: an entity's blendMode and a particle system's alphaMode.
// Verified against GameSpace.dll's SetAlphaMode (DLL@0x10006170): every mode
// but NONE also alpha-tests at > 1/255.
enum ALPHA_MODE : int {
    AM_PIXEL = 0,       // SrcAlpha, InvSrcAlpha
    AM_ADD = 1,         // One, One
    AM_ALPHA_TEST = 2,  // no blending, hard cutout
    AM_NONE = 3,
    AM_MODULATE = 4,    // Zero, SrcColor
};

// The ARGB colour the drawing functions take.
inline uint ARGB(const uint8 a, const uint8 r, const uint8 g, const uint8 b) {
    return (uint(a) << 24) | (uint(r) << 16) | (uint(g) << 8) | uint(b);
}

// What AngelScript does on a null handle access, an out-of-range array index or
// a division by zero: it aborts the script function that is running, and the
// engine carries on with the next callback. The ported code throws this at the
// same points (ETHEntity::operator-> does it for handles), and the Machine
// catches it around each callback, each loop and each preLoop, logs it once per
// site, and moves on - exactly the original's failure mode, which some of its
// behaviour depends on (docs/spec/90-synthesis.md, doDamage.as:176).
class ScriptException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

} // namespace Penumbra::Eth
