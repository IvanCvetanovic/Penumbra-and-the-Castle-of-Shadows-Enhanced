#pragma once

// ENHANCEMENT E28: the player's own touch-control layout (size, opacity, where each control sits), kept in
// settings.json and applied over the manifest (TouchControls::WithTuning). Pure data: no Eth, no glm,
// no Json include (Settings.hpp must stay free of windows.h's DrawText macro, Settings.cpp:19-20).

#include <array>
#include <string>

namespace Supersonic::Json {
class Value;
}

namespace Penumbra::Render {

// One entry per TouchControl, in its order; TouchControls.cpp static_asserts the count and a test the names.
inline constexpr int kTuningControls = 10;
inline constexpr int kTuningBackIndex = 9;
inline constexpr const char* kTuningControlKeys[kTuningControls] = {
    "dpad", "jump", "sword", "fire", "light", "swordCombo", "spellCombo", "exitDown", "pause", "back"};

// How far a control is moved from where the manifest puts it: manifest pixels, SCREEN direction (+x right, +y down).
struct TouchMove {
    float x = 0.0f;
    float y = 0.0f;
    bool operator==(const TouchMove& other) const = default;
    bool IsZero() const { return x == 0.0f && y == 0.0f; }
};

struct TouchTuning {
    static constexpr float kMinSize = 0.4f;
    static constexpr float kMaxSize = 1.4f;       // Magic Rampage's own ceiling; above the sizes testSizeCeiling pins (docs/planning/2026-10-01-e28-adjustable-touch-controls.md) the shipped layout draws controls over each other and the player moves them apart
    static constexpr float kSizeStep = 0.1f;
    static constexpr float kMinOpacity = 0.2f;
    static constexpr float kMaxOpacity = 1.8f;
    static constexpr float kOpacityStep = 0.2f;
    static constexpr float kMaxMove = 2048.0f;    // |x| and |y|
    static constexpr float kMoveQuantum = 0.1f;

    // ENHANCEMENT E34: which arrangement of the shipped default (touch_controls.json, TouchControls::DefaultManifest)
    // the saved moves were made against. A move is a delta from where the default puts a control, so a move saved
    // against another arrangement would be laid on this one's places (a wide, staggered cluster, until Restore).
    // settings.json's touchTuning carries the value it was written under as "layout"; ReadFrom keeps the saved
    // moves only when it is this number and drops them otherwise (a missing "layout" is every file written before
    // E34), keeping the size and the opacity, which do not depend on the arrangement. The numbers: 1 is E29's
    // arrangement (two columns, the right one higher), 2 is E32's (a level grid, 117 in from the edge), 3 is an
    // intermediate build of E33 (two staggered columns 40 in from the edge, the LEFT one the higher), which was
    // never in a release, and 4 is E33's (the same columns with the RIGHT one the higher). WHOEVER CHANGES ANY OF
    // THE SIX ACTION BUTTONS' DEFAULT OFFSETS (or the direction control's, the down button's or the pause button's)
    // BUMPS THIS BY ONE; tests/test_pn_render_touch.cpp pins the value beside the table of the default places
    // (testTuningIdentity).
    static constexpr int kLayoutVersion = 4;

    float size = 1.0f;                            // every control but Pause and Back
    float opacity = 1.0f;                         // times the manifest's idleAlpha (held: see WithTuning)
    std::array<TouchMove, kTuningControls> move{};

    bool operator==(const TouchTuning& other) const = default;
    bool IsDefault() const;                       // == TouchTuning{}
    bool Moved() const;                           // any move non-zero
    TouchTuning Clamped() const;                  // ranges, snapping, NaN -> default, move[back] zeroed

    static float SnapSize(float value);           // clamp 0.4..1.4, round(x*10)/10
    static float SnapOpacity(float value);        // clamp 0.2..1.8, round(x*5)/5
    static float StepSize(float current, int direction);      // SnapSize(current + direction*0.1); `current` itself at a limit
    static float StepOpacity(float current, int direction);   // likewise, 0.2 steps
    static bool IsMovableIndex(int index);        // 0..8 (everything but back)
    static const char* ControlKey(int index);     // kTuningControlKeys[index], "" out of range

    // The settings.json block, `block` = root["touchTuning"]. `out` starts as the caller's default; every wrong
    // field is left as it is and described in `warning` (may be null). Never throws. E34: the block's "layout" must be
    // kLayoutVersion for its "move" to be read; a missing or different one (an expected migration, no warning) leaves
    // the moves as the caller had them, and a "layout" that is not a number warns and counts as different.
    static void ReadFrom(const Supersonic::Json::Value& block, TouchTuning& out, std::string* warning);
    // The object text written after "touchTuning": (no trailing comma), one line; E34: it starts with the current layout.
    std::string ToJson() const;
    // --touch-tuning's text (key=value pairs, see game/main.cpp's usage). False with `error` set when it is not that grammar; `out` is left Clamped().
    static bool ParseFlag(const std::string& text, TouchTuning& out, std::string* error);
};

} // namespace Penumbra::Render
