#include "render/TouchControls.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <system_error>
#include <utility>

#include "core/Json.hpp"

namespace Penumbra::Render {

namespace {

using Supersonic::Json::Value;

// In TouchControl's order.
constexpr const char* kControlIds[kTouchControlCount] = {"dpad",       "jump",       "sword",    "fire",  "light",
                                                         "swordCombo", "spellCombo", "exitDown", "pause", "back"};

// What each action presses: what playerInput.as reads for player 0.
constexpr Eth::KEY kActionKeys[kTouchActionCount] = {
    Eth::K_LEFT,    // getLeftButtonStatus, getPlayerXYAxis (playerInput.as:55, :135)
    Eth::K_RIGHT,   // getRightButtonStatus (playerInput.as:75)
    Eth::K_DOWN,    // getDownButtonStatus (playerInput.as:115): combos, the next_level door (E25's button)
    Eth::K_CTRL,    // getJumpButtonStatus (playerInput.as:176)
    Eth::K_S,       // getAttack01ButtonStatus, the sword (playerInput.as:201)
    Eth::K_D,       // getAttack02ButtonStatus, the fire ball (playerInput.as:221)
    Eth::K_SPACE,   // getAttack03ButtonStatus, the light spell (playerInput.as:241)
    Eth::K_ESC,     // getCancelButtonStatus (playerInput.as:289), escToGoToMenu, E13's pause
};

// One tick of a combo's macro: the one key it presses, or none. Side is the
// way the wizard faces. The timelines are the header's table.
enum class ComboKey { None, Side, Down, Sword, Fire };
constexpr ComboKey kSwordComboTicks[] = {ComboKey::None, ComboKey::Side, ComboKey::None, ComboKey::Side,
                                         ComboKey::Sword};   // CMD side, side, SWORD (playerInput.as:361-362)
constexpr ComboKey kSpellComboTicks[] = {ComboKey::None, ComboKey::Down, ComboKey::Side,
                                         ComboKey::Fire};   // CMD DOWN, side, SPELL (playerInput.as:380-381)

std::size_t ComboLength(TouchCombo combo) {
    switch (combo) {
        case TouchCombo::Sword: return std::size(kSwordComboTicks);
        case TouchCombo::Spell: return std::size(kSpellComboTicks);
        case TouchCombo::None: break;
    }
    return 0;
}

ComboKey ComboTick(TouchCombo combo, std::size_t tick) {
    if (tick >= ComboLength(combo)) return ComboKey::None;
    return combo == TouchCombo::Sword ? kSwordComboTicks[tick] : kSpellComboTicks[tick];
}

// What the combo buffer reads for player 1 (combo.as:63-112 through
// playerInput.as:55-239), as bits: his keys - all up without the focus, as
// InputState reads them - and his pad, stepped with or without it, its stick
// past InputState's 0.8 as JK_LEFT/RIGHT/UP/DOWN.
enum ComboInput : unsigned {
    kComboLeft = 1u << 0,
    kComboRight = 1u << 1,
    kComboUp = 1u << 2,
    kComboDown = 1u << 3,
    kComboSword = 1u << 4,
    kComboSpell = 1u << 5,
};
constexpr float kStickArrow = 0.8f;   // eth/Input.cpp kArrowThreshold

unsigned ComboInputs(const Eth::InputFrame& frame, int player1Pad) {
    unsigned bits = 0;
    if (frame.hasFocus) {
        if (frame.keys[Eth::K_LEFT]) bits |= kComboLeft;
        if (frame.keys[Eth::K_RIGHT]) bits |= kComboRight;
        if (frame.keys[Eth::K_UP]) bits |= kComboUp;
        if (frame.keys[Eth::K_DOWN]) bits |= kComboDown;
        if (frame.keys[Eth::K_S]) bits |= kComboSword;
        if (frame.keys[Eth::K_D]) bits |= kComboSpell;
    }
    if (player1Pad >= 0 && player1Pad < Eth::kMaxJoysticks) {
        const Eth::InputFrame::Pad& pad = frame.pads[static_cast<std::size_t>(player1Pad)];
        if (pad.connected) {
            if (pad.xy.x <= -kStickArrow) bits |= kComboLeft;
            if (pad.xy.x >= kStickArrow) bits |= kComboRight;
            if (pad.xy.y <= -kStickArrow) bits |= kComboUp;
            if (pad.xy.y >= kStickArrow) bits |= kComboDown;
            if (pad.buttons[Eth::JK_04]) bits |= kComboSword;
            if (pad.buttons[Eth::JK_02]) bits |= kComboSpell;
        }
    }
    return bits;
}

TouchControlSpec Spec(const char* image, TouchAnchor anchor, glm::vec2 offset, glm::vec2 size, TouchShape shape,
                      float hitPadding, glm::vec2 overhang = glm::vec2(0.0f)) {
    TouchControlSpec spec;
    spec.image = image;
    spec.anchor = anchor;
    spec.offset = offset;
    spec.size = size;
    spec.shape = shape;
    spec.hitPadding = hitPadding;
    spec.overhang = overhang;
    return spec;
}

// tan(22.5 degrees): the edge between a direction and its diagonals.
constexpr float kTan22 = 0.41421356f;

constexpr float kMinScale = 0.25f;
constexpr float kMaxScale = 4.0f;
constexpr float kMaxDeadZone = 0.95f;
// A missing image is drawn as a square this grey, at the control's alpha.
constexpr Eth::uint8 kPlainGrey = 90;

bool EqualsIgnoreCase(const std::string& a, const char* b) {
    std::size_t i = 0;
    for (; i < a.size() && b[i] != '\0'; ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return i == a.size() && b[i] == '\0';
}

void Warn(std::string* warning, const std::string& what) {
    if (warning == nullptr) return;
    if (!warning->empty()) *warning += "; ";
    *warning += what;
}

void ReadString(const Value& object, const char* key, const std::string& where, std::string& out,
                std::string* warning) {
    if (!object.Has(key)) return;
    const Value& value = object[key];
    if (!value.IsString()) {
        Warn(warning, where + "." + key + " is not a string");
        return;
    }
    out = value.AsString();
}

void ReadFloat(const Value& object, const char* key, const std::string& where, float minimum, float maximum,
               float& out, std::string* warning) {
    if (!object.Has(key)) return;
    const Value& value = object[key];
    const double number = value.AsNumber(std::numeric_limits<double>::quiet_NaN());
    if (!value.IsNumber() || !std::isfinite(number)) {
        Warn(warning, where + "." + key + " is not a number");
        return;
    }
    out = std::clamp(static_cast<float>(number), minimum, maximum);
}

// [x, y], each finite and at least `minimum` on its axis.
void ReadPair(const Value& object, const char* key, const std::string& where, const glm::vec2& minimum,
              glm::vec2& out, std::string* warning) {
    if (!object.Has(key)) return;
    const Value& value = object[key];
    const auto& items = value.AsArray();
    if (!value.IsArray() || items.size() != 2 || !items[0].IsNumber() || !items[1].IsNumber() ||
        !std::isfinite(items[0].AsNumber()) || !std::isfinite(items[1].AsNumber())) {
        Warn(warning, where + "." + key + " is not [x, y]");
        return;
    }
    const glm::vec2 pair(static_cast<float>(items[0].AsNumber()), static_cast<float>(items[1].AsNumber()));
    const bool lowX = pair.x < minimum.x;
    if (lowX || pair.y < minimum.y) {
        const float floor = lowX ? minimum.x : minimum.y;
        Warn(warning, where + "." + key + " is below " + std::to_string(static_cast<int>(floor)));
        return;
    }
    out = pair;
}

void ReadAnchor(const Value& object, const std::string& where, TouchAnchor& out, std::string* warning) {
    if (!object.Has("anchor")) return;
    const std::string name = object["anchor"].AsString();
    if (EqualsIgnoreCase(name, "topLeft")) out = TouchAnchor::TopLeft;
    else if (EqualsIgnoreCase(name, "topRight")) out = TouchAnchor::TopRight;
    else if (EqualsIgnoreCase(name, "bottomLeft")) out = TouchAnchor::BottomLeft;
    else if (EqualsIgnoreCase(name, "bottomRight")) out = TouchAnchor::BottomRight;
    else Warn(warning, where + ".anchor is not topLeft, topRight, bottomLeft or bottomRight");
}

void ReadShape(const Value& object, const std::string& where, TouchShape& out, std::string* warning) {
    if (!object.Has("shape")) return;
    const std::string name = object["shape"].AsString();
    if (EqualsIgnoreCase(name, "circle")) out = TouchShape::Circle;
    else if (EqualsIgnoreCase(name, "rect")) out = TouchShape::Rect;
    else Warn(warning, where + ".shape is not circle or rect");
}

Eth::uint8 AlphaByte(float alpha) {
    return static_cast<Eth::uint8>(std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255.0f));
}

Eth::HudCmd Sprite(const std::string& path, const glm::vec2& min, const glm::vec2& size, Eth::uint8 alpha) {
    Eth::HudCmd cmd;
    // Stretched to the box: the art has at least the box's logical pixels
    // (Magic Rampage's 128 px buttons for 96-120), for the phones whose
    // screens have more pixels than the logical 768.
    cmd.kind = Eth::HudCmd::Kind::ShapedSprite;
    cmd.sprite = path;
    cmd.pos = min;
    cmd.size = size;
    cmd.color = Eth::ARGB(alpha, 255, 255, 255);
    return cmd;
}

Eth::HudCmd Plain(const glm::vec2& min, const glm::vec2& size, Eth::uint8 alpha) {
    Eth::HudCmd cmd;
    cmd.kind = Eth::HudCmd::Kind::Rectangle;
    cmd.pos = min;
    cmd.size = size;
    // Every corner: HudCmd's corners default to opaque white.
    cmd.color = cmd.color1 = cmd.color2 = cmd.color3 = Eth::ARGB(alpha, kPlainGrey, kPlainGrey, kPlainGrey);
    // E28: a square that meets the screen's left or right edge (the direction control's, in the editor's wide
    // area) stays the square it is, not a bar out to the edge of what the view shows.
    cmd.stretchToSides = false;   // E28
    return cmd;
}

bool Contains(const std::vector<TouchContact>& contacts, int id) {
    return std::any_of(contacts.begin(), contacts.end(), [id](const TouchContact& c) { return c.id == id; });
}

} // namespace

const char* TouchControls::ControlId(TouchControl control) {
    const int index = static_cast<int>(control);
    return index >= 0 && index < kTouchControlCount ? kControlIds[index] : "";
}

Eth::KEY TouchControls::KeyFor(TouchAction action) {
    const int index = static_cast<int>(action);
    return index >= 0 && index < kTouchActionCount ? kActionKeys[index] : Eth::K_COUNT;
}

TouchManifest TouchControls::DefaultManifest() {
    // game/data/touch_controls.json holds the same; see it for why each is where it is. The art is
    // Magic Rampage's screen pad (tools/art/make_mr_touch_art.py): square buttons, so they are
    // touched as squares - all but the direction control, one round control whose two buttons
    // are drawn where its sectors are.
    TouchManifest m;
    constexpr glm::vec2 kButton{120.0f, 120.0f};
    constexpr glm::vec2 kComboButton{100.0f, 100.0f};
    constexpr TouchShape kSquare = TouchShape::Rect;
    // The disc's two buttons are drawn 21.7 above its centre and its lower half is empty, so the
    // box hangs 138 below the screen's bottom edge: the buttons' centres are then 200 - 138 + 21.7
    // = 83.7 above it, level with the sword button's (24 + 60); the jump button is 30 higher. overhang is the allowance for that.   // E29
    // The box is 400 across (the art's 406 px): each button 126 units, a face of about 118.
    m[TouchControl::Dpad] = Spec("images/touch/dpad.png", TouchAnchor::BottomLeft, {24.0f, -138.0f}, {400.0f, 400.0f},
                                 TouchShape::Circle, 30.0f, {0.0f, 150.0f});
    // E29: the six actions are two columns of three, each button 16 from its neighbours and the right
    // column's edge 24 in from the screen's, as the cluster always was. From the bottom: left sword, the   // E29
    // sword combo, light; right jump, the spell combo, fire. The right column stands 30 higher, so the   // E29
    // two bottom buttons follow the arc of a thumb pivoting at the screen's corner, and each combo (100   // E29
    // wide) is centred in its column of 120.   // E29
    m[TouchControl::Jump] =
        Spec("images/touch/jump.png", TouchAnchor::BottomRight, {24.0f, 54.0f}, kButton, kSquare, 4.0f);   // E29
    m[TouchControl::Sword] =
        Spec("images/touch/sword.png", TouchAnchor::BottomRight, {160.0f, 24.0f}, kButton, kSquare, 4.0f);   // E29
    m[TouchControl::Fire] =
        Spec("images/touch/fire.png", TouchAnchor::BottomRight, {24.0f, 306.0f}, kButton, kSquare, 4.0f);   // E29
    m[TouchControl::Light] =
        Spec("images/touch/light.png", TouchAnchor::BottomRight, {160.0f, 276.0f}, kButton, kSquare, 4.0f);   // E29
    // E16 combos, E29: each in the middle of its column.
    m[TouchControl::SwordCombo] =
        Spec("images/touch/combo_sword.png", TouchAnchor::BottomRight, {170.0f, 160.0f}, kComboButton, kSquare, 6.0f);   // E29
    m[TouchControl::SpellCombo] =
        Spec("images/touch/combo_spell.png", TouchAnchor::BottomRight, {34.0f, 190.0f}, kComboButton, kSquare, 6.0f);   // E29
    // E25's down button: the arrows' size, centred over the gap between the left and right buttons
    // (the disc's centre, 24 + 200 across) and about 17 px above their faces (the disc's centre,
    // 768 + 138 - 200 = 706, less 21.7 to theirs, less 59): x 161-287, y 482-608.
    m[TouchControl::ExitDown] =
        Spec("images/touch/exit_down.png", TouchAnchor::BottomLeft, {161.0f, 160.0f}, {126.0f, 126.0f}, kSquare, 6.0f);
    // Magic Rampage's pause is a pill, 128x86.
    m[TouchControl::Pause] =
        Spec("images/touch/pause.png", TouchAnchor::TopRight, {20.0f, 44.0f}, {96.0f, 64.5f}, kSquare, 12.0f);
    m[TouchControl::Back] =
        Spec("images/touch/back.png", TouchAnchor::TopRight, {20.0f, 44.0f}, {96.0f, 96.0f}, kSquare, 12.0f);
    m.dpadLeft = "images/touch/dpad_left.png";
    m.dpadRight = "images/touch/dpad_right.png";
    m.dpadDown.clear();   // E25: no down arrow on the disc, and no down sector
    m.downSector = false;
    m.knobImage = "images/touch/dpad_knob.png";
    m.knobSize = {128.0f, 128.0f};   // the art's own size: the brackets just frame a 126 px button
    m.knobAtRest = false;   // the brackets only on the button the thumb holds
    m.deadZone = 0.25f;
    m.idleAlpha = 0.45f;
    m.pressedAlpha = 0.9f;
    m.scale = 1.0f;
    return m;
}

TouchManifest TouchControls::ManifestFromJson(const std::string& text, std::string* warning) {
    TouchManifest manifest = DefaultManifest();
    Value root;
    std::string parseError;
    if (!Supersonic::Json::Parse(text, root, parseError)) {
        Warn(warning, std::string(kManifestFile) + " is not valid JSON (" + parseError + "); using the defaults");
        return manifest;
    }
    if (!root.IsObject()) {
        Warn(warning, std::string(kManifestFile) + " is not an object; using the defaults");
        return manifest;
    }

    ReadFloat(root, "idleAlpha", "manifest", 0.0f, 1.0f, manifest.idleAlpha, warning);
    ReadFloat(root, "pressedAlpha", "manifest", 0.0f, 1.0f, manifest.pressedAlpha, warning);
    ReadFloat(root, "scale", "manifest", kMinScale, kMaxScale, manifest.scale, warning);

    if (!root.Has("controls")) return manifest;
    const Value& controls = root["controls"];
    if (!controls.IsObject()) {
        Warn(warning, "controls is not an object");
        return manifest;
    }
    for (const auto& [id, unused] : controls.AsObject()) {
        (void)unused;
        const bool known = std::any_of(std::begin(kControlIds), std::end(kControlIds),
                                       [&id](const char* name) { return id == name; });
        // Notes are keys starting with '_', as strings.json has them.
        if (!known && (id.empty() || id[0] != '_')) Warn(warning, "controls." + id + " is not a control");
    }
    for (int i = 0; i < kTouchControlCount; ++i) {
        const char* id = kControlIds[i];
        if (!controls.Has(id)) continue;
        const Value& entry = controls[id];
        const std::string where = std::string("controls.") + id;
        if (!entry.IsObject()) {
            Warn(warning, where + " is not an object");
            continue;
        }
        TouchControlSpec& spec = manifest.controls[static_cast<std::size_t>(i)];
        ReadString(entry, "image", where, spec.image, warning);
        ReadAnchor(entry, where, spec.anchor, warning);
        ReadPair(entry, "overhang", where, glm::vec2(0.0f), spec.overhang, warning);
        // Negative only as far as the control may hang past its edge (read just above).
        ReadPair(entry, "offset", where, -spec.overhang, spec.offset, warning);
        ReadPair(entry, "size", where, glm::vec2(1.0f), spec.size, warning);
        ReadShape(entry, where, spec.shape, warning);
        ReadFloat(entry, "hitPadding", where, 0.0f, 1000.0f, spec.hitPadding, warning);
        if (entry.Has("enabled")) {
            if (entry["enabled"].IsBool()) spec.enabled = entry["enabled"].AsBool();
            else Warn(warning, where + ".enabled is not true/false");
        }

        if (static_cast<TouchControl>(i) != TouchControl::Dpad) continue;
        ReadFloat(entry, "deadZone", where, 0.0f, kMaxDeadZone, manifest.deadZone, warning);
        if (entry.Has("downSector")) {
            if (entry["downSector"].IsBool()) manifest.downSector = entry["downSector"].AsBool();
            else Warn(warning, where + ".downSector is not true/false");
        }
        if (entry.Has("arrows")) {
            const Value& arrows = entry["arrows"];
            if (arrows.IsObject()) {
                ReadString(arrows, "left", where + ".arrows", manifest.dpadLeft, warning);
                ReadString(arrows, "right", where + ".arrows", manifest.dpadRight, warning);
                ReadString(arrows, "down", where + ".arrows", manifest.dpadDown, warning);
            } else {
                Warn(warning, where + ".arrows is not an object");
            }
        }
        if (entry.Has("knob")) {
            const Value& knob = entry["knob"];
            if (knob.IsObject()) {
                ReadString(knob, "image", where + ".knob", manifest.knobImage, warning);
                ReadPair(knob, "size", where + ".knob", glm::vec2(1.0f), manifest.knobSize, warning);
                if (knob.Has("atRest")) {
                    if (knob["atRest"].IsBool()) manifest.knobAtRest = knob["atRest"].AsBool();
                    else Warn(warning, where + ".knob.atRest is not true/false");
                }
            } else {
                Warn(warning, where + ".knob is not an object");
            }
        }
    }
    return manifest;
}

TouchManifest TouchControls::LoadManifest(const std::filesystem::path& file, std::string* warning) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        Warn(warning, file.generic_string() + ": cannot open; using the built-in layout");
        return DefaultManifest();
    }
    std::ostringstream contents;
    contents << in.rdbuf();
    // Named: Json::Parser keeps a reference to the text it parses.
    const std::string text = contents.str();
    return ManifestFromJson(text, warning);
}

bool TouchControls::EnabledBySetting(const std::string& setting) {
    if (EqualsIgnoreCase(setting, "on")) return true;
    if (EqualsIgnoreCase(setting, "off")) return false;
    return kMobileBuild;   // "auto"
}

TouchCorner TouchControls::CornerFor(const TouchScreen& screen) {
    // The pause's own rows lead on: Resume, Main menu.
    if (screen.paused) return TouchCorner::Hidden;
    // A level or an arena opens E13's pause; their end screens have no pause
    // and leave on cancel (doLoop's waitForInputToMenu, escToGoToMenu).
    if (screen.level) return screen.gameFinished ? TouchCorner::Back : TouchCorner::Pause;
    // The main menu: cancel does nothing there (goToMenu, menu.as:383).
    // Before the first scene: nothing to leave.
    if (screen.sceneFile.empty() || screen.sceneFile == "scenes/menu.esc") return TouchCorner::Hidden;
    // The options: the original's own Back arrow is on the screen
    // (putBackButton, videoModes.as:65), clicked by a finger as by the mouse;
    // a second one beside it would only say the same twice.
    if (screen.sceneFile == "scenes/videoModes.esc") return TouchCorner::Hidden;
    // The arena select and game over read cancel alone (waitForInputToMenu,
    // menu.as:374) and draw no button for it: this is their way out.
    return TouchCorner::Back;
}

TouchInsets TouchControls::WindowInsetsToLogical(const TouchInsets& windowPixels, const View& view) {
    TouchInsets out;
    if (!(view.scale > 0.0f)) return out;
    const glm::vec2 image(view.windowPixels);
    // What the bars already keep clear is not asked of the controls again.
    // The bars are what is not shown: under E1's open sides, less than the
    // pillarbox's.
    const glm::vec2 shownMin = view.ShownMin();
    const glm::vec2 shownMax = view.ShownMax();
    out.left = std::max(0.0f, windowPixels.left - shownMin.x) / view.scale;
    out.top = std::max(0.0f, windowPixels.top - shownMin.y) / view.scale;
    out.right = std::max(0.0f, windowPixels.right - (image.x - shownMax.x)) / view.scale;
    out.bottom = std::max(0.0f, windowPixels.bottom - (image.y - shownMax.y)) / view.scale;
    return out;
}

namespace {   // E28

// E28: ComputeLayout's prelude and its loop body, split out so that MoveFor can run the same placement
// backwards (one set of clamps, never a second copy of them). Every expression is the one ComputeLayout
// had, in the same order, so that the default layout is bit for bit what it was (lines moved without a
// change of text carry no marker).
struct Bounds {   // E28
    glm::vec2 safeLo{0.0f};   // E28
    glm::vec2 safeHi{0.0f};   // E28
    glm::vec2 frameLo{0.0f};   // E28: E26: the pause button's corner, the HUD frame's, when that lies further in
    glm::vec2 frameHi{0.0f};   // E28
    float manifestScale = 1.0f;   // E28
    float unitScale = 1.0f;   // E28
};   // E28

Bounds BoundsFor(const TouchManifest& manifest, const glm::vec2& areaMin, const glm::vec2& areaMax,   // E28
                 const TouchInsets& safe, const float unit, const TouchInsets& hudFrame) {   // E28
    Bounds bounds;   // E28
    bounds.manifestScale = std::clamp(manifest.scale, kMinScale, kMaxScale);   // E28
    // E25: the manifest's pixels in the screen's; 1 but for a zoomed level or a phone's larger menu.
    bounds.unitScale = unit > 0.0f && std::isfinite(unit) ? unit : 1.0f;   // E28
    bounds.safeLo = areaMin + glm::vec2(std::max(0.0f, safe.left), std::max(0.0f, safe.top));   // E28
    bounds.safeHi =   // E28
        glm::max(bounds.safeLo, areaMax - glm::vec2(std::max(0.0f, safe.right), std::max(0.0f, safe.bottom)));   // E28
    // E26: the pause button's corner is the HUD frame's, where the timer is
    // drawn, when that lies further in.
    bounds.frameLo = glm::max(bounds.safeLo, areaMin + glm::vec2(hudFrame.left, hudFrame.top));   // E28
    bounds.frameHi = glm::max(   // E28
        bounds.frameLo, glm::min(bounds.safeHi, areaMax - glm::vec2(hudFrame.right, hudFrame.bottom)));   // E28
    return bounds;   // E28
}   // E28

// The same for one tick's geometry: an area whose max is not above its min is no area (the screen), as
// Update reads it.
Bounds BoundsFor(const TouchManifest& manifest, const TouchGeometry& geometry) {   // E28
    return BoundsFor(manifest, geometry.AreaMin(), geometry.AreaMax(), geometry.safeArea, geometry.unit,   // E28
                     geometry.hudFrame);   // E28
}   // E28

// Where one control goes: its size, how far it may hang, where its anchor and offset put it, and the
// limits that put it back inside the safe area.
struct Placement {   // E28
    glm::vec2 size{0.0f};   // E28
    glm::vec2 hang{0.0f};   // E28
    glm::vec2 unclampedMin{0.0f};   // E28
    glm::vec2 minLimit{0.0f};   // E28
    glm::vec2 maxLimit{0.0f};   // E28
};   // E28

Placement PlaceControl(const TouchControlSpec& spec, const float manifestScale, const float unitScale,   // E28
                       const glm::vec2& lo, const glm::vec2& hi) {   // E28
    const float scale = manifestScale * unitScale;
    Placement placed;   // E28
    const glm::vec2 size = glm::max(spec.size, glm::vec2(1.0f)) * scale;
    // How far the box may lie past the edges it hangs from: what the manifest allows, and never
    // more than half of it, so a hand-edited manifest cannot take a control off the screen.
    const glm::vec2 hang = glm::min(glm::max(spec.overhang, glm::vec2(0.0f)) * scale, size * 0.5f);
    glm::vec2 offset = glm::max(spec.offset * scale, -hang);
    const bool left = spec.anchor == TouchAnchor::TopLeft || spec.anchor == TouchAnchor::BottomLeft;
    const bool top = spec.anchor == TouchAnchor::TopLeft || spec.anchor == TouchAnchor::TopRight;
    // E25: a zoom enlarges the run's timer at the top (its 25 px are the
    // screen's), and a control hanging below it keeps below it: only the
    // part of its offset past the timer's row is scaled down.
    const float fromTop = std::max(0.0f, spec.offset.y) * manifestScale;
    if (top && unitScale < 1.0f && fromTop > TouchControls::kTimerRowHeight) {   // E28
        offset.y = TouchControls::kTimerRowHeight + (fromTop - TouchControls::kTimerRowHeight) * unitScale;   // E28
    }
    placed.size = size;   // E28
    placed.hang = hang;   // E28
    placed.unclampedMin = glm::vec2(left ? lo.x + offset.x : hi.x - offset.x - size.x,   // E28
                                    top ? lo.y + offset.y : hi.y - offset.y - size.y);   // E28
    // Inside the safe area whatever the offsets say: a hand-edited
    // manifest, or a screen smaller than it was laid out for - but for
    // the overhang past the edges the control hangs from.
    placed.minLimit = lo;   // E28
    placed.maxLimit = glm::max(lo, hi - size);   // E28
    if (left) placed.minLimit.x -= hang.x;   // E28
    else placed.maxLimit.x += hang.x;   // E28
    if (top) placed.minLimit.y -= hang.y;   // E28
    else placed.maxLimit.y += hang.y;   // E28
    return placed;   // E28
}   // E28

glm::vec2 ClampedMin(const Placement& placed) {   // E28
    return glm::clamp(placed.unclampedMin, placed.minLimit, glm::max(placed.minLimit, placed.maxLimit));   // E28
}   // E28

TouchLayout LayOut(const TouchManifest& manifest, const Bounds& bounds) {   // E28
    TouchLayout layout;
    const float scale = bounds.manifestScale * bounds.unitScale;   // E28
    for (int i = 0; i < kTouchControlCount; ++i) {
        const auto index = static_cast<std::size_t>(i);
        const TouchControlSpec& spec = manifest.controls[index];
        const bool framed = static_cast<TouchControl>(i) == TouchControl::Pause;
        const Placement placed = PlaceControl(spec, bounds.manifestScale, bounds.unitScale,   // E28
                                              framed ? bounds.frameLo : bounds.safeLo,   // E28
                                              framed ? bounds.frameHi : bounds.safeHi);   // E28
        const glm::vec2 min = ClampedMin(placed);   // E28
        layout.boxes[index].min = min;
        layout.boxes[index].max = min + placed.size;   // E28
        layout.hitPadding[index] = std::max(0.0f, spec.hitPadding) * scale;
    }
    return layout;
}

// E28: one tick's geometry as a layout: over the area it names, or the screen when it names none (Update's rule).
TouchLayout LayoutFor(const TouchManifest& manifest, const TouchGeometry& geometry) {   // E28
    return TouchControls::ComputeLayout(manifest, geometry.AreaMin(), geometry.AreaMax(), geometry.safeArea,   // E28
                                        geometry.unit, geometry.hudFrame);   // E28
}   // E28

} // namespace   // E28

TouchLayout TouchControls::ComputeLayout(const TouchManifest& manifest, const glm::vec2& screen,
                                         const TouchInsets& safe, const float unit, const TouchInsets& hudFrame) {
    return ComputeLayout(manifest, glm::vec2(0.0f), screen, safe, unit, hudFrame);
}

TouchLayout TouchControls::ComputeLayout(const TouchManifest& manifest, const glm::vec2& areaMin,
                                         const glm::vec2& areaMax, const TouchInsets& safe, const float unit,
                                         const TouchInsets& hudFrame) {
    return LayOut(manifest, BoundsFor(manifest, areaMin, areaMax, safe, unit, hudFrame));   // E28: split, same arithmetic
}

void TouchControls::SetManifest(TouchManifest manifest) {
    m_base = std::move(manifest);   // E28: the file's (or the defaults') layout; the player's tuning goes over it
    m_manifest = WithTuning(m_base, m_tuning);   // E28: the base itself while the tuning is the default
    SetImageRoot(m_imageRoot);
}

void TouchControls::SetImageRoot(const std::filesystem::path& dataDir) {
    m_imageRoot = dataDir;
    m_images.clear();
    if (m_imageRoot.empty()) return;
    std::vector<std::string> names = {m_manifest.dpadLeft, m_manifest.dpadRight, m_manifest.dpadDown,
                                      m_manifest.knobImage};
    for (const TouchControlSpec& spec : m_manifest.controls) names.push_back(spec.image);
    for (const std::string& name : names) {
        if (name.empty() || m_images.count(name) != 0) continue;
        const std::filesystem::path path = m_imageRoot / name;
        std::error_code ec;
        // Absolute: TextureCache takes an absolute path as it is (the English
        // art reaches it the same way, Localization::ImageVariant).
        m_images.emplace(name, std::filesystem::is_regular_file(path, ec) ? path.generic_string() : std::string());
    }
}

std::string TouchControls::resolved(const std::string& image) const {
    const auto it = m_images.find(image);
    return it != m_images.end() ? it->second : std::string();
}

bool TouchControls::ownerVisible(Owner owner) const {
    switch (owner) {
        case Owner::None: return true;
        case Owner::Dpad: return Visible(TouchControl::Dpad);
        case Owner::Jump: return Visible(TouchControl::Jump);
        case Owner::Sword: return Visible(TouchControl::Sword);
        case Owner::Fire: return Visible(TouchControl::Fire);
        case Owner::Light: return Visible(TouchControl::Light);
        case Owner::SwordCombo: return Visible(TouchControl::SwordCombo);
        case Owner::SpellCombo: return Visible(TouchControl::SpellCombo);
        case Owner::ExitDown: return Visible(TouchControl::ExitDown);
        case Owner::Corner: return Visible(TouchControl::Pause) || Visible(TouchControl::Back);
        case Owner::Pointer: return m_scene == TouchScene::Menu;
    }
    return false;
}

TouchControl TouchControls::hit(const glm::vec2& point) const {
    TouchControl best = TouchControl::Count;
    float bestDistance = std::numeric_limits<float>::max();
    for (int i = 0; i < kTouchControlCount; ++i) {
        const auto index = static_cast<std::size_t>(i);
        if (!m_visible[index]) continue;
        const TouchLayout::Box& box = m_layout.boxes[index];
        const float pad = m_layout.hitPadding[index];
        const glm::vec2 centre = box.Centre();
        const glm::vec2 toPoint = point - centre;
        const float distance = glm::dot(toPoint, toPoint);
        bool inside = false;
        if (m_manifest.controls[index].shape == TouchShape::Circle) {
            const glm::vec2 size = box.Size();
            const float radius = 0.5f * std::min(size.x, size.y) + pad;
            inside = distance <= radius * radius;
        } else {
            inside = point.x >= box.min.x - pad && point.x <= box.max.x + pad && point.y >= box.min.y - pad &&
                     point.y <= box.max.y + pad;
        }
        // Where two reach the finger (padding on a small screen), the nearer centre.
        if (inside && distance < bestDistance) {
            best = static_cast<TouchControl>(i);
            bestDistance = distance;
        }
    }
    return best;
}

void TouchControls::LatchFrame(const std::vector<TouchContact>& contacts) {
    for (const TouchContact& contact : contacts) {
        // Only a finger no tick has seen: one already held is read again by
        // the next tick if it is still down, and latching it would stretch
        // its lift by a tick.
        if (!contact.down || contact.id < 0 || m_contacts.count(contact.id) != 0) continue;
        m_latched.emplace(contact.id, contact.position);
    }
}

void TouchControls::ObserveFrame(const Eth::InputFrame& frame, int player1Pad) {
    const unsigned now = ComboInputs(frame, player1Pad);
    const bool pressed = (now & ~m_observed) != 0u;
    m_observed = now;
    if (pressed) m_quietTicks = 0;
    else if (m_quietTicks < kComboQuietTicks) ++m_quietTicks;
}

void TouchControls::CancelCombo() { m_combo = Combo{}; }

void TouchControls::startCombo(TouchCombo combo, TouchFacing facing) {
    m_combo = Combo{};
    m_combo.combo = combo;
    // His currentDir; without a wizard, where the disc last pointed.
    m_combo.side = facing == TouchFacing::Left    ? TouchAction::Left
                   : facing == TouchFacing::Right ? TouchAction::Right
                                                  : m_lastSide;
}

TouchStep TouchControls::Update(const TouchInput& input) {
    m_scene = input.scene;
    m_corner = input.corner;
    m_unit = input.unit > 0.0f && std::isfinite(input.unit) ? input.unit : 1.0f;
    // E28: this tick's geometry, kept so that SetTuning can lay the controls out again at once.
    m_geometry = TouchGeometry::From(input);   // E28
    m_haveGeometry = true;   // E28
    // E1's wide menus lay the controls out across what is shown.
    m_layout = LayoutFor(m_manifest, m_geometry);   // E28: the one place a geometry becomes a layout (SetTuning's too)
    const bool play = m_scene == TouchScene::Play;
    const bool edit = m_scene == TouchScene::Edit;   // E28
    const auto show = [this](TouchControl control, bool shown) {
        m_visible[static_cast<std::size_t>(control)] = shown && m_manifest[control].enabled;
    };
    for (const TouchControl control : {TouchControl::Dpad, TouchControl::Jump, TouchControl::Sword, TouchControl::Fire,
                                       TouchControl::Light, TouchControl::SwordCombo, TouchControl::SpellCombo}) {
        show(control, play || edit);   // E28: the editor shows every control that moves
    }
    // E25: down, only where it takes the wizard on.
    show(TouchControl::ExitDown, edit || (play && input.nextLevelOffered));   // E28
    show(TouchControl::Pause, edit || m_corner == TouchCorner::Pause);   // E28: whatever CornerFor said
    show(TouchControl::Back, !edit && m_corner == TouchCorner::Back);   // E28: Back never moves, so never shown there

    // A combo ends with the play - the pause, a menu, a load (a death's
    // reload too) - or with its button gone from the layout.
    const TouchControl comboButton =
        m_combo.combo == TouchCombo::Spell ? TouchControl::SpellCombo : TouchControl::SwordCombo;
    if (m_combo.combo != TouchCombo::None && (!play || input.sceneSerial != m_sceneSerial || !Visible(comboButton))) {
        CancelCombo();
    }
    m_sceneSerial = input.sceneSerial;

    // The fingers down this tick: the input's, and those a frame without a
    // tick saw come down - held for this one tick where they were last seen.
    std::vector<TouchContact> down;
    for (const TouchContact& contact : input.contacts) {
        if (contact.down && contact.id >= 0 && !Contains(down, contact.id)) down.push_back(contact);
    }
    for (const auto& [id, position] : m_latched) {
        if (Contains(down, id)) continue;
        TouchContact latched{id, position, true};
        for (const TouchContact& contact : input.contacts) {
            if (contact.id == id) latched.position = contact.position;
        }
        down.push_back(latched);
    }
    m_latched.clear();
    m_down = down;   // E28: every scene's: the editor reads exactly the fingers this tick used

    // Lifted: whatever is not down any more lets go of what it held.
    for (auto it = m_contacts.begin(); it != m_contacts.end();) {
        if (Contains(down, it->first)) ++it;
        else it = m_contacts.erase(it);
    }
    // Hidden under the finger: dead until it lifts, never a key and never a click.
    bool dpadTaken = false;
    bool pointerTaken = false;
    for (auto& entry : m_contacts) {
        Held& held = entry.second;
        // E28: in the editor nothing a finger does presses a key, so a finger a level or a menu gave an owner
        // (the pause's, the direction control's) lets go of it too; it keeps none after the editor closes.
        if (edit || !ownerVisible(held.owner)) held.owner = Owner::None;   // E28
        dpadTaken = dpadTaken || held.owner == Owner::Dpad;
        pointerTaken = pointerTaken || held.owner == Owner::Pointer;
    }

    // Each finger belongs to what it first landed on.
    for (const TouchContact& contact : down) {
        const auto known = m_contacts.find(contact.id);
        if (known != m_contacts.end()) {
            known->second.position = contact.position;
            continue;
        }
        Held held;
        held.position = contact.position;
        // E28: in the editor every new finger lands on no control (an owner of None: dead until it lifts, never a
        // key or a click, in this scene or the next), whatever is under it.
        switch (edit ? TouchControl::Count : hit(contact.position)) {   // E28
            case TouchControl::Dpad:
                // One thumb steers; a second finger on it is ignored.
                if (!dpadTaken) {
                    held.owner = Owner::Dpad;
                    dpadTaken = true;
                }
                break;
            case TouchControl::Jump: held.owner = Owner::Jump; break;
            case TouchControl::Sword: held.owner = Owner::Sword; break;
            case TouchControl::Fire: held.owner = Owner::Fire; break;
            case TouchControl::Light: held.owner = Owner::Light; break;
            // A tap starts its combo; one while a combo runs is ignored, not queued.
            case TouchControl::SwordCombo:
                held.owner = Owner::SwordCombo;
                if (m_combo.combo == TouchCombo::None) startCombo(TouchCombo::Sword, input.facing);
                break;
            case TouchControl::SpellCombo:
                held.owner = Owner::SpellCombo;
                if (m_combo.combo == TouchCombo::None) startCombo(TouchCombo::Spell, input.facing);
                break;
            case TouchControl::ExitDown: held.owner = Owner::ExitDown; break;
            case TouchControl::Pause:
            case TouchControl::Back: held.owner = Owner::Corner; break;
            case TouchControl::Count:
                // On no control: the mouse, in a menu; nothing, in play.
                if (m_scene == TouchScene::Menu && !pointerTaken) {
                    held.owner = Owner::Pointer;
                    pointerTaken = true;
                }
                break;
        }
        m_contacts.emplace(contact.id, held);
    }

    TouchStep step;
    step.touching = !down.empty();
    // While a combo runs, its keys are the only ones of the buffer's it may see.
    const bool comboRuns = m_combo.combo != TouchCombo::None;
    const TouchLayout::Box& dpad = m_layout[TouchControl::Dpad];
    const glm::vec2 dpadSize = dpad.Size();
    const float radius = 0.5f * std::min(dpadSize.x, dpadSize.y);
    m_knob = dpad.Centre();
    m_dpadHeld = false;
    m_dpadPointing = false;
    const auto hold = [&step](TouchAction action) { step.held[static_cast<std::size_t>(action)] = true; };
    for (auto& entry : m_contacts) {
        Held& held = entry.second;
        switch (held.owner) {
            case Owner::None: break;
            case Owner::Dpad: {
                m_dpadHeld = true;
                const glm::vec2 d = held.position - dpad.Centre();
                const float dead = std::clamp(m_manifest.deadZone, 0.0f, kMaxDeadZone) * radius;
                if (glm::dot(d, d) > dead * dead) {
                    const float across = std::fabs(d.x);
                    // Within 22.5 degrees of straight up or down: no side.
                    const bool nearVertical = across < std::fabs(d.y) * kTan22;
                    // Down and both of its diagonals (y is down), on a disc
                    // that has them (E16's; E25's has its down button instead).
                    const bool downward = m_manifest.downSector && d.y > 0.0f && d.y >= across * kTan22;
                    m_dpadPointing = !nearVertical || downward;
                    if (!comboRuns) {
                        if (!nearVertical) {
                            m_lastSide = d.x > 0.0f ? TouchAction::Right : TouchAction::Left;
                            hold(m_lastSide);
                        }
                        if (downward) hold(TouchAction::Down);
                    }
                }
                // The knob follows the thumb, never past the disc's rim.
                const float knobRadius = 0.5f * std::min(m_manifest.knobSize.x, m_manifest.knobSize.y) *
                                         std::clamp(m_manifest.scale, kMinScale, kMaxScale) * m_unit;
                const float travel = std::max(0.0f, radius - knobRadius);
                const float length = glm::length(d);
                m_knob = dpad.Centre() + (length > travel && length > 0.0f ? d * (travel / length) : d);
                break;
            }
            case Owner::Jump: hold(TouchAction::Jump); break;
            case Owner::Sword:
            case Owner::Fire:
                // Held back under a combo, and after it until the finger lifts:
                // held on, it would press again as the combo let go.
                held.heldBack = held.heldBack || comboRuns;
                if (!held.heldBack) hold(held.owner == Owner::Sword ? TouchAction::Sword : TouchAction::Fire);
                break;
            case Owner::Light: hold(TouchAction::Light); break;
            // E25: down, as the keyboard's, for as long as the button shows.
            case Owner::ExitDown: hold(TouchAction::Down); break;
            case Owner::SwordCombo:
            case Owner::SpellCombo: break;   // the tap started it; held, it does nothing
            case Owner::Corner: hold(TouchAction::Cancel); break;
            case Owner::Pointer:
                step.pointer = true;
                step.pointerPos = held.position;
                break;
        }
    }

    // The combo's tick.
    if (comboRuns) {
        step.combo = m_combo.combo;
        const ComboKey key = ComboTick(m_combo.combo, m_combo.next);
        if (key != ComboKey::None && !m_combo.pressed && m_quietTicks < kComboQuietTicks) {
            // The buffer may still hold a command, which checkSequence would
            // read first: nothing until combo.as:116 has emptied it.
            if (++m_combo.waited > kComboMaxWaitTicks) CancelCombo();
        } else {
            switch (key) {
                case ComboKey::None: break;
                case ComboKey::Side: hold(m_combo.side); break;
                case ComboKey::Down: hold(TouchAction::Down); break;
                case ComboKey::Sword: hold(TouchAction::Sword); break;
                case ComboKey::Fire: hold(TouchAction::Fire); break;
            }
            m_combo.pressed = m_combo.pressed || key != ComboKey::None;
            // Its last tick: from the next one the fingers have the keys again.
            if (++m_combo.next >= ComboLength(m_combo.combo)) m_combo = Combo{};
        }
    }
    m_last = step;
    return step;
}

void TouchControls::ApplyToFrame(const TouchStep& step, Eth::InputFrame& frame) {
    for (int i = 0; i < kTouchActionCount; ++i) {
        if (step.held[static_cast<std::size_t>(i)]) frame.keys[static_cast<std::size_t>(kActionKeys[i])] = true;
    }
    if (step.touching) frame.keys[Eth::K_LMOUSE] = step.pointer;
    if (step.pointer) {
        // The scripts only compare the two (InputMapper::Map), so one point
        // serves as both, as it does for the mouse.
        frame.cursor = step.pointerPos;
        frame.cursorAbsolute = step.pointerPos;
        frame.keys[Eth::K_LMOUSE] = true;
    }
}

void TouchControls::AppendOverlay(std::vector<Eth::HudCmd>& out) const {
    const Eth::uint8 idle = AlphaByte(m_manifest.idleAlpha);
    const Eth::uint8 pressed = AlphaByte(m_manifest.pressedAlpha);
    const auto draw = [&](const std::string& image, const glm::vec2& min, const glm::vec2& size, Eth::uint8 alpha,
                          bool plainIfMissing) {
        const std::string path = resolved(image);
        if (!path.empty()) out.push_back(Sprite(path, min, size, alpha));
        else if (plainIfMissing) out.push_back(Plain(min, size, alpha));
    };
    const auto held = [this](TouchAction action) { return m_last.Held(action); };

    for (int i = 0; i < kTouchControlCount; ++i) {
        const auto index = static_cast<std::size_t>(i);
        if (!m_visible[index]) continue;
        const TouchControl control = static_cast<TouchControl>(i);
        const TouchLayout::Box& box = m_layout.boxes[index];
        const TouchControlSpec& spec = m_manifest.controls[index];
        switch (control) {
            case TouchControl::Dpad: {
                // The disc stays at rest; its arrows light with their direction.
                draw(spec.image, box.min, box.Size(), idle, true);
                draw(m_manifest.dpadLeft, box.min, box.Size(), held(TouchAction::Left) ? pressed : idle, false);
                draw(m_manifest.dpadRight, box.min, box.Size(), held(TouchAction::Right) ? pressed : idle, false);
                draw(m_manifest.dpadDown, box.min, box.Size(), held(TouchAction::Down) ? pressed : idle, false);
                const glm::vec2 knob = m_manifest.knobSize * std::clamp(m_manifest.scale, kMinScale, kMaxScale) * m_unit;
                // At rest (no thumb, or one in the dead zone or pointing up)
                // only where the manifest keeps it there.
                if (knob.x > 0.0f && knob.y > 0.0f && (m_manifest.knobAtRest || m_dpadPointing)) {
                    draw(m_manifest.knobImage, m_knob - knob * 0.5f, knob, m_dpadHeld ? pressed : idle, false);
                }
                break;
            }
            case TouchControl::Jump:
            case TouchControl::Sword:
            case TouchControl::Fire:
            case TouchControl::Light:
            case TouchControl::ExitDown:
            case TouchControl::Pause:
            case TouchControl::Back: {
                const TouchAction action = control == TouchControl::Jump       ? TouchAction::Jump
                                           : control == TouchControl::Sword    ? TouchAction::Sword
                                           : control == TouchControl::Fire     ? TouchAction::Fire
                                           : control == TouchControl::Light    ? TouchAction::Light
                                           : control == TouchControl::ExitDown ? TouchAction::Down
                                                                               : TouchAction::Cancel;
                draw(spec.image, box.min, box.Size(), held(action) ? pressed : idle, true);
                break;
            }
            case TouchControl::SwordCombo:
            case TouchControl::SpellCombo: {
                // Lit while its combo runs.
                const TouchCombo mine = control == TouchControl::SwordCombo ? TouchCombo::Sword : TouchCombo::Spell;
                draw(spec.image, box.min, box.Size(), m_last.combo == mine ? pressed : idle, true);
                break;
            }
            case TouchControl::Count: break;
        }
    }
}

// E28 ------------------------------------------------------------------------------------------------------
// The player's tuning over the manifest: one transform of a copy of the base manifest, so that the
// layout, the hit tests, the knob and the art all read one manifest, and the default tuning is that manifest
// exactly.
static_assert(kTuningControls == kTouchControlCount, "one tuning entry per TouchControl");   // E28
static_assert(kTuningBackIndex == static_cast<int>(TouchControl::Back), "the tuning's back entry is TouchControl::Back");   // E28

TouchGeometry TouchGeometry::From(const TouchInput& input) {   // E28
    TouchGeometry geometry;   // E28
    geometry.screen = input.screen;   // E28
    geometry.areaMin = input.areaMin;   // E28
    geometry.areaMax = input.areaMax;   // E28
    geometry.safeArea = input.safeArea;   // E28
    geometry.hudFrame = input.hudFrame;   // E28
    geometry.unit = input.unit;   // E28
    return geometry;   // E28
}   // E28

// As Update reads it: an area whose max is not above its min is no area (the screen).   // E28
glm::vec2 TouchGeometry::AreaMin() const {   // E28
    const bool area = areaMax.x > areaMin.x && areaMax.y > areaMin.y;   // E28
    return area ? areaMin : glm::vec2(0.0f);   // E28
}   // E28

glm::vec2 TouchGeometry::AreaMax() const {   // E28
    const bool area = areaMax.x > areaMin.x && areaMax.y > areaMin.y;   // E28
    return area ? areaMax : screen;   // E28
}   // E28

bool TouchControls::SizeExempt(const TouchControl control) {   // E28
    return control == TouchControl::Pause || control == TouchControl::Back;   // E28
}   // E28

bool TouchControls::Movable(const TouchControl control) { return TouchTuning::IsMovableIndex(static_cast<int>(control)); }   // E28

TouchManifest TouchControls::WithTuning(const TouchManifest& base, const TouchTuning& tuningIn) {   // E28
    const TouchTuning tuning = tuningIn.Clamped();   // E28
    // The default is the base itself, not a copy that was multiplied by one: bit for bit what the file gave.   // E28
    if (tuning == TouchTuning{}) return base;   // E28
    TouchManifest out = base;   // E28
    const float baseScale = std::clamp(base.scale, kMinScale, kMaxScale);   // E28
    for (int i = 0; i < kTouchControlCount; ++i) {   // E28
        const auto index = static_cast<std::size_t>(i);   // E28
        const TouchControl control = static_cast<TouchControl>(i);   // E28
        TouchControlSpec& spec = out.controls[index];   // E28
        // Size: every control's own size, its distances from its corner and its reach grow together, so that the   // E28
        // whole pack grows from its corners (the way `scale` does). The Pause and Back keep what the manifest says:   // E28
        // the pause column feeds the message room, the zoom's limit and the phone menu's panel.   // E28
        if (!SizeExempt(control)) {   // E28
            spec.size *= tuning.size;   // E28
            spec.offset *= tuning.size;   // E28
            spec.overhang *= tuning.size;   // E28
            spec.hitPadding *= tuning.size;   // E28
        }   // E28
        if (!Movable(control)) continue;   // E28
        // Moves are in the manifest's pixels, in the SCREEN's direction, and are not scaled by the size (offsets   // E28
        // are inward from the anchor corner: right and bottom controls take the opposite sign). The down button   // E28
        // sits over the direction control's gap, so it goes where that goes.   // E28
        TouchMove moved = tuning.move[index];   // E28
        if (control == TouchControl::ExitDown) {   // E28
            moved.x += tuning.move[static_cast<std::size_t>(TouchControl::Dpad)].x;   // E28
            moved.y += tuning.move[static_cast<std::size_t>(TouchControl::Dpad)].y;   // E28
        }   // E28
        const bool left = spec.anchor == TouchAnchor::TopLeft || spec.anchor == TouchAnchor::BottomLeft;   // E28
        const bool top = spec.anchor == TouchAnchor::TopLeft || spec.anchor == TouchAnchor::TopRight;   // E28
        spec.offset.x += left ? moved.x : -moved.x;   // E28
        spec.offset.y += top ? moved.y : -moved.y;   // E28
        // A moved Pause keeps below the run's timer (its row and a margin; E25's rule scales only what is past it).   // E28
        if (control == TouchControl::Pause && top && !tuning.move[index].IsZero()) {   // E28
            spec.offset.y = std::max(spec.offset.y, kPauseTopClear / baseScale);   // E28
        }   // E28
    }   // E28
    out.knobSize *= tuning.size;   // E28: the direction control's
    if (tuning.opacity != 1.0f) {   // E28
        out.idleAlpha = std::clamp(base.idleAlpha * tuning.opacity, 0.0f, 1.0f);   // E28
        // The held look is the only cue that a finger is on a button (there is no pressed art): never fainter   // E28
        // than the manifest's, and always a little stronger than the idle one, at any opacity.   // E28
        out.pressedAlpha = std::clamp(std::max(base.pressedAlpha, out.idleAlpha + 0.15f), 0.0f, 1.0f);   // E28
    }   // E28
    return out;   // E28
}   // E28

TouchMove TouchControls::MoveFor(const TouchManifest& base, const TouchTuning& tuning, const TouchControl control,   // E28
                                 const glm::vec2& wantedMin, const TouchGeometry& geometry) {   // E28
    if (!Movable(control)) return TouchMove{};   // E28
    const auto index = static_cast<std::size_t>(control);   // E28
    // The manifest as it would be with every other move and the size, and none of this control's own.   // E28
    TouchTuning rest = tuning.Clamped();   // E28
    rest.move[index] = TouchMove{};   // E28
    const TouchManifest probe = WithTuning(base, rest);   // E28
    // Placed by the very code ComputeLayout runs, so every clamp (the safe area, the overhang, the HUD frame   // E28
    // for the Pause, E25's timer rule) is there and none is written twice.   // E28
    const Bounds bounds = BoundsFor(probe, geometry);   // E28
    const bool framed = control == TouchControl::Pause;   // E28
    const Placement placed = PlaceControl(probe.controls[index], bounds.manifestScale, bounds.unitScale,   // E28
                                          framed ? bounds.frameLo : bounds.safeLo,   // E28
                                          framed ? bounds.frameHi : bounds.safeHi);   // E28
    glm::vec2 wanted = wantedMin;   // E28
    if (!std::isfinite(wanted.x)) wanted.x = placed.unclampedMin.x;   // E28: a point that is no point is no move
    if (!std::isfinite(wanted.y)) wanted.y = placed.unclampedMin.y;   // E28
    const glm::vec2 held = glm::clamp(wanted, placed.minLimit, glm::max(placed.minLimit, placed.maxLimit));   // E28
    // A move of m in the screen's direction shifts the box's unclamped corner by m x (manifest scale x unit)   // E28
    // on every anchor (left and top add it to the offset, right and bottom take it from the edge), and E25's   // E28
    // timer rule only adds a constant, so the inverse is one division...   // E28
    const float scale = bounds.manifestScale * bounds.unitScale;   // E28
    const TouchControlSpec& spec = probe.controls[index];   // E28
    // ...but for one thing: PlaceControl holds an offset at -hang, so an offset the probe already has below that (the   // E28
    // down button, with a direction control hand-edited to move further than the button's own distance from the   // E28
    // edge) sits flat there, and the move has to cross the gap first. Nothing the manifests allow is below it.   // E28
    const glm::vec2 gap = glm::max(glm::vec2(0.0f), -placed.hang - spec.offset * scale);   // E28
    const bool left = spec.anchor == TouchAnchor::TopLeft || spec.anchor == TouchAnchor::BottomLeft;   // E28
    const bool top = spec.anchor == TouchAnchor::TopLeft || spec.anchor == TouchAnchor::TopRight;   // E28
    TouchMove move{(held.x - placed.unclampedMin.x + (left ? gap.x : -gap.x)) / scale,   // E28
                   (held.y - placed.unclampedMin.y + (top ? gap.y : -gap.y)) / scale};   // E28
    if (framed && top) {   // E28
        // WithTuning's floor, in move terms: below the timer's row.   // E28
        move.y = std::max(move.y, kPauseTopClear / bounds.manifestScale - spec.offset.y);   // E28
    }   // E28
    // Quantised and held by the one rule that does it (Clamped), then taken back out.   // E28
    TouchTuning out;   // E28
    out.move[index] = move;   // E28
    return out.Clamped().move[index];   // E28
}   // E28

TouchLayout::Box TouchControls::GrabBox(const TouchControl control, const TouchLayout::Box& box) {   // E28
    if (control != TouchControl::Dpad) return box;   // E28
    // The art's two 126-unit buttons sit centred 21.7 above the 400-unit box's centre (see DefaultManifest): the   // E28
    // lower half of the disc is empty, and half of it hangs below the screen's edge.   // E28
    const float height = box.Size().y;   // E28
    return TouchLayout::Box{{box.min.x, box.min.y + 0.2883f * height}, {box.max.x, box.min.y + 0.6033f * height}};   // E28
}   // E28

void TouchControls::SetTuning(const TouchTuning& tuning) {   // E28
    m_tuning = tuning.Clamped();   // E28
    // Not SetManifest: no image is looked for again (the names do not change), so a drag costs no file system call.   // E28
    m_manifest = WithTuning(m_base, m_tuning);   // E28
    if (!m_haveGeometry) return;   // E28
    m_layout = LayoutFor(m_manifest, m_geometry);   // E28
    // The knob at rest sits at the direction control's centre, which just moved with it (a thumb on the control, in   // E28
    // a level, drags the knob; the editor has none).   // E28
    if (!m_dpadHeld) m_knob = m_layout[TouchControl::Dpad].Centre();   // E28
}   // E28

} // namespace Penumbra::Render
