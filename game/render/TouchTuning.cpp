// ENHANCEMENT E28: the player's own touch-control layout (render/TouchTuning.hpp): the ranges and the grid
// every value snaps to, the settings.json block (read tolerantly, written in one line) and the
// --touch-tuning flag's text.

#include "render/TouchTuning.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <system_error>

#include "core/Json.hpp"

namespace Penumbra::Render {

namespace {

using Supersonic::Json::Value;

// The three grids. A value is held as a whole number of steps and turned back with ONE division: n / 10.0f
// and n / 5.0f are correctly rounded quotients, so a snapped float is exactly the literal (9 / 10.0f == 0.9f,
// 2 / 5.0f == 0.4f) and a chain of steps ends on the limits and stops there, with no drift to compare away.
constexpr double kSizeSteps = 10.0;        // per 1.0 of size: 0.1
constexpr double kOpacitySteps = 5.0;      // per 1.0 of opacity: 0.2
constexpr double kMoveSteps = 10.0;        // per manifest pixel: 0.1
constexpr long kMinSizeStep = 4;           // 0.4
constexpr long kMaxSizeStep = 14;          // 1.4
constexpr long kMinOpacityStep = 1;        // 0.2
constexpr long kMaxOpacityStep = 9;        // 1.8
constexpr long kMaxMoveStep = 20480;       // 2048.0

// `value` as steps of 1/perUnit, held to [lo, hi]. The product is taken in double (a float times 10 is
// exact there), so what is rounded is what the float really holds. NaN is `fallback`, which std::clamp
// would pass through; an infinity is held to the nearest limit.
long StepsOf(const float value, const double perUnit, const long lo, const long hi, const long fallback) {
    if (std::isnan(value)) return fallback;
    const double steps = std::round(static_cast<double>(value) * perUnit);
    return static_cast<long>(std::clamp(steps, static_cast<double>(lo), static_cast<double>(hi)));
}

// Whole steps cannot be a negative zero, which ToJson would write as "-0".
float FromSteps(const long steps, const double perUnit) {
    return static_cast<float>(steps) / static_cast<float>(perUnit);
}

long SizeSteps(const float value) { return StepsOf(value, kSizeSteps, kMinSizeStep, kMaxSizeStep, 10); }
long OpacitySteps(const float value) { return StepsOf(value, kOpacitySteps, kMinOpacityStep, kMaxOpacityStep, 5); }

float SnapMove(const float value) {
    return FromSteps(StepsOf(value, kMoveSteps, -kMaxMoveStep, kMaxMoveStep, 0), kMoveSteps);
}

void Warn(std::string* warning, const std::string& what) {
    if (warning == nullptr) return;
    if (!warning->empty()) *warning += "; ";
    *warning += what;
}

// Shortest text that reads back as the same float (Settings.cpp's FormatFloat; a pure file keeps its own).
std::string Number(const float value) {
    char buffer[32];
    const auto result = std::to_chars(std::begin(buffer), std::end(buffer), value);
    if (result.ec != std::errc()) return "0";
    return std::string(buffer, result.ptr);
}

// A JSON number held to [lo, hi] BEFORE it is narrowed: a double past float's range would be undefined to
// cast, and 1e40 should read as the limit, not as an infinity that Clamped() then takes for a typo.
float NarrowTo(const double number, const double lo, const double hi) {
    return static_cast<float>(std::clamp(number, lo, hi));
}

// A finite JSON number, else false.
bool FiniteNumber(const Value& value, double& out) {
    if (!value.IsNumber()) return false;
    out = value.AsNumber();
    return std::isfinite(out);
}

// One whole token of a flag as a finite number (strtod, as core/Json does: the C locale's point).
bool ParseNumber(const std::string& token, float& out) {
    const std::size_t first = token.find_first_not_of(" \t");
    if (first == std::string::npos) return false;
    const std::size_t last = token.find_last_not_of(" \t");
    const std::string trimmed = token.substr(first, last - first + 1);
    char* end = nullptr;
    const double number = std::strtod(trimmed.c_str(), &end);
    if (end != trimmed.c_str() + trimmed.size() || !std::isfinite(number)) return false;
    out = NarrowTo(number, -1.0e6, 1.0e6);
    return true;
}

std::string Trimmed(const std::string& text) {
    const std::size_t first = text.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    const std::size_t last = text.find_last_not_of(" \t");
    return text.substr(first, last - first + 1);
}

std::string KeyList() {
    std::string keys = "size, opacity";
    for (int i = 0; i < kTuningBackIndex; ++i) keys += std::string(", ") + kTuningControlKeys[i];
    return keys;
}

// The control a flag or file key names, or -1 (also for "back": it never moves).
int MovableIndexOf(const std::string& key) {
    for (int i = 0; i < kTuningBackIndex; ++i) {
        if (key == kTuningControlKeys[i]) return i;
    }
    return -1;
}

} // namespace

bool TouchTuning::IsDefault() const { return *this == TouchTuning{}; }

bool TouchTuning::Moved() const {
    for (const TouchMove& entry : move) {
        if (!entry.IsZero()) return true;
    }
    return false;
}

TouchTuning TouchTuning::Clamped() const {
    TouchTuning out;
    out.size = SnapSize(size);
    out.opacity = SnapOpacity(opacity);
    for (int i = 0; i < kTuningBackIndex; ++i) {
        const auto index = static_cast<std::size_t>(i);
        out.move[index] = TouchMove{SnapMove(move[index].x), SnapMove(move[index].y)};
    }
    // Back never moves (its left edge feeds the phone menu's panel): its entry stays {}.
    return out;
}

float TouchTuning::SnapSize(const float value) { return FromSteps(SizeSteps(value), kSizeSteps); }

float TouchTuning::SnapOpacity(const float value) { return FromSteps(OpacitySteps(value), kOpacitySteps); }

float TouchTuning::StepSize(const float current, const int direction) {
    const long step = direction > 0 ? 1 : direction < 0 ? -1 : 0;
    // Snapped first, so a value off the grid (a hand-edited 2.0) steps from where it lands.
    return FromSteps(std::clamp(SizeSteps(current) + step, kMinSizeStep, kMaxSizeStep), kSizeSteps);
}

float TouchTuning::StepOpacity(const float current, const int direction) {
    const long step = direction > 0 ? 1 : direction < 0 ? -1 : 0;
    return FromSteps(std::clamp(OpacitySteps(current) + step, kMinOpacityStep, kMaxOpacityStep), kOpacitySteps);
}

bool TouchTuning::IsMovableIndex(const int index) { return index >= 0 && index < kTuningBackIndex; }

const char* TouchTuning::ControlKey(const int index) {
    return index >= 0 && index < kTuningControls ? kTuningControlKeys[index] : "";
}

void TouchTuning::ReadFrom(const Value& block, TouchTuning& out, std::string* warning) {
    if (!block.IsObject()) {
        Warn(warning, "touchTuning is not an object");
        return;
    }
    double number = 0.0;
    // E34: looked up first, whatever order the file has its keys in. Compared as the double it is: a number past
    // int's range cannot be narrowed to it, and 3.5 is not layout 3.
    bool sameLayout = false;
    if (block.Has("layout")) {
        if (FiniteNumber(block["layout"], number)) sameLayout = number == static_cast<double>(kLayoutVersion);
        else Warn(warning, "touchTuning.layout is not a number");
    }
    if (block.Has("size")) {
        if (FiniteNumber(block["size"], number)) out.size = SnapSize(NarrowTo(number, -1.0e6, 1.0e6));
        else Warn(warning, "touchTuning.size is not a number");
    }
    if (block.Has("opacity")) {
        if (FiniteNumber(block["opacity"], number)) out.opacity = SnapOpacity(NarrowTo(number, -1.0e6, 1.0e6));
        else Warn(warning, "touchTuning.opacity is not a number");
    }
    // E34: the moves are deltas from an arrangement of the default that is not this one's: not read, not warned about.
    if (!sameLayout || !block.Has("move")) return;
    const Value& moves = block["move"];
    if (!moves.IsObject()) {
        Warn(warning, "touchTuning.move is not an object");
        return;
    }
    // In the file's own order, so the warnings read in it.
    for (const std::string& key : moves.OrderedKeys()) {
        // Notes are keys starting with '_', as touch_controls.json and strings.json have them.
        if (!key.empty() && key[0] == '_') continue;
        const int index = MovableIndexOf(key);
        if (index < 0) {
            Warn(warning, "touchTuning.move." + key + " is not a control that moves");
            continue;
        }
        const Value& pair = moves[key];
        const auto& items = pair.AsArray();
        double x = 0.0;
        double y = 0.0;
        if (!pair.IsArray() || items.size() != 2 || !FiniteNumber(items[0], x) || !FiniteNumber(items[1], y)) {
            Warn(warning, "touchTuning.move." + key + " is not [x, y]");
            continue;
        }
        const double limit = static_cast<double>(kMaxMove);
        out.move[static_cast<std::size_t>(index)] =
            TouchMove{SnapMove(NarrowTo(x, -limit, limit)), SnapMove(NarrowTo(y, -limit, limit))};
    }
}

std::string TouchTuning::ToJson() const {
    // What is held, not what was asked: a NaN would be written as "nan", which is not JSON.
    const TouchTuning held = Clamped();
    std::string text = "{ \"layout\": " + std::to_string(kLayoutVersion) + ", \"size\": " + Number(held.size) +
                       ", \"opacity\": " + Number(held.opacity) + ", \"move\": {";   // E34: the layout first
    bool first = true;
    for (int i = 0; i < kTuningBackIndex; ++i) {
        const TouchMove& entry = held.move[static_cast<std::size_t>(i)];
        if (entry.IsZero()) continue;
        text += first ? " \"" : ", \"";
        text += std::string(kTuningControlKeys[i]) + "\": [" + Number(entry.x) + ", " + Number(entry.y) + "]";
        first = false;
    }
    text += first ? "} }" : " } }";
    return text;
}

bool TouchTuning::ParseFlag(const std::string& text, TouchTuning& out, std::string* error) {
    const auto fail = [&out, error](const std::string& why) {
        if (error != nullptr) *error = why;
        out = out.Clamped();
        return false;
    };
    // Replacing, not merging: the flag is the tuning of the run.
    TouchTuning parsed;
    if (Trimmed(text).empty()) return fail("no key=value pairs");
    std::size_t at = 0;
    while (at <= text.size()) {
        const std::size_t comma = std::min(text.find(',', at), text.size());
        const std::string item = Trimmed(text.substr(at, comma - at));
        at = comma + 1;
        if (item.empty()) return fail("an empty item (a stray comma?)");
        const std::size_t equals = item.find('=');
        if (equals == std::string::npos) return fail("\"" + item + "\" is not key=value");
        const std::string key = Trimmed(item.substr(0, equals));
        const std::string value = Trimmed(item.substr(equals + 1));
        float x = 0.0f;
        if (key == "size" || key == "opacity") {
            if (!ParseNumber(value, x)) return fail(key + " needs a number, not \"" + value + "\"");
            (key == "size" ? parsed.size : parsed.opacity) = x;
            continue;
        }
        const int index = MovableIndexOf(key);
        if (index < 0) {
            const std::string why = key == "back" ? "back does not move" : "unknown key \"" + key + "\"";
            return fail(why + " (keys: " + KeyList() + ")");
        }
        const std::size_t colon = value.find(':');
        float y = 0.0f;
        if (colon == std::string::npos || !ParseNumber(value.substr(0, colon), x) ||
            !ParseNumber(value.substr(colon + 1), y)) {
            return fail(key + " needs dx:dy, not \"" + value + "\"");
        }
        parsed.move[static_cast<std::size_t>(index)] = TouchMove{x, y};
    }
    out = parsed.Clamped();
    return true;
}

} // namespace Penumbra::Render
