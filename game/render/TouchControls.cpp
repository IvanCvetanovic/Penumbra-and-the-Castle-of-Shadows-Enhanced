#include "render/TouchControls.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <system_error>
#include <utility>

#include "core/Json.hpp"

namespace Penumbra::Render {

namespace {

using Supersonic::Json::Value;

constexpr const char* kControlIds[kTouchControlCount] = {"dpad", "jump", "sword", "fire", "light", "pause", "back"};

// What each action presses: what playerInput.as reads for player 0.
constexpr Eth::KEY kActionKeys[kTouchActionCount] = {
    Eth::K_LEFT,    // getLeftButtonStatus, getPlayerXYAxis (playerInput.as:55, :135)
    Eth::K_RIGHT,   // getRightButtonStatus (playerInput.as:75)
    Eth::K_DOWN,    // getDownButtonStatus (playerInput.as:115): combos, the next_level door
    Eth::K_CTRL,    // getJumpButtonStatus (playerInput.as:176)
    Eth::K_S,       // getAttack01ButtonStatus, the sword (playerInput.as:201)
    Eth::K_D,       // getAttack02ButtonStatus, the fire ball (playerInput.as:221)
    Eth::K_SPACE,   // getAttack03ButtonStatus, the light spell (playerInput.as:241)
    Eth::K_ESC,     // getCancelButtonStatus (playerInput.as:289), escToGoToMenu, E13's pause
};

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

// [x, y], each finite and at least `minimum`.
void ReadPair(const Value& object, const char* key, const std::string& where, float minimum, glm::vec2& out,
              std::string* warning) {
    if (!object.Has(key)) return;
    const Value& value = object[key];
    const auto& items = value.AsArray();
    if (!value.IsArray() || items.size() != 2 || !items[0].IsNumber() || !items[1].IsNumber() ||
        !std::isfinite(items[0].AsNumber()) || !std::isfinite(items[1].AsNumber())) {
        Warn(warning, where + "." + key + " is not [x, y]");
        return;
    }
    const glm::vec2 pair(static_cast<float>(items[0].AsNumber()), static_cast<float>(items[1].AsNumber()));
    if (pair.x < minimum || pair.y < minimum) {
        Warn(warning, where + "." + key + " is below " + std::to_string(static_cast<int>(minimum)));
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
    // Stretched to the box: the art is drawn at twice the logical size, for
    // the phones whose screens have more pixels than the logical 768.
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
    // game/data/touch_controls.json holds the same; see it for why each is where it is.
    TouchManifest m;
    m[TouchControl::Dpad] = {"images/touch/dpad.png", TouchAnchor::BottomLeft, {40.0f, 40.0f}, {260.0f, 260.0f},
                             TouchShape::Circle, 60.0f};
    m[TouchControl::Jump] = {"images/touch/jump.png", TouchAnchor::BottomRight, {142.0f, 24.0f}, {120.0f, 120.0f},
                             TouchShape::Circle, 16.0f};
    m[TouchControl::Sword] = {"images/touch/sword.png", TouchAnchor::BottomRight, {260.0f, 142.0f}, {120.0f, 120.0f},
                              TouchShape::Circle, 16.0f};
    m[TouchControl::Fire] = {"images/touch/fire.png", TouchAnchor::BottomRight, {24.0f, 142.0f}, {120.0f, 120.0f},
                             TouchShape::Circle, 16.0f};
    m[TouchControl::Light] = {"images/touch/light.png", TouchAnchor::BottomRight, {142.0f, 260.0f}, {120.0f, 120.0f},
                              TouchShape::Circle, 16.0f};
    m[TouchControl::Pause] = {"images/touch/pause.png", TouchAnchor::TopRight, {20.0f, 44.0f}, {84.0f, 84.0f},
                              TouchShape::Circle, 12.0f};
    m[TouchControl::Back] = {"images/touch/back.png", TouchAnchor::TopRight, {20.0f, 44.0f}, {84.0f, 84.0f},
                             TouchShape::Circle, 12.0f};
    m.dpadLeft = "images/touch/dpad_left.png";
    m.dpadRight = "images/touch/dpad_right.png";
    m.dpadDown = "images/touch/dpad_down.png";
    m.knobImage = "images/touch/dpad_knob.png";
    m.knobSize = {104.0f, 104.0f};
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
        ReadPair(entry, "offset", where, 0.0f, spec.offset, warning);
        ReadPair(entry, "size", where, 1.0f, spec.size, warning);
        ReadShape(entry, where, spec.shape, warning);
        ReadFloat(entry, "hitPadding", where, 0.0f, 1000.0f, spec.hitPadding, warning);

        if (static_cast<TouchControl>(i) != TouchControl::Dpad) continue;
        ReadFloat(entry, "deadZone", where, 0.0f, kMaxDeadZone, manifest.deadZone, warning);
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
                ReadPair(knob, "size", where + ".knob", 1.0f, manifest.knobSize, warning);
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

TouchInsets TouchControls::WindowInsetsToLogical(const TouchInsets& windowPixels, const View& view) {
    TouchInsets out;
    if (!(view.scale > 0.0f)) return out;
    const glm::vec2 image(view.windowPixels);
    // What the bars already keep clear is not asked of the controls again.
    out.left = std::max(0.0f, windowPixels.left - view.viewportMin.x) / view.scale;
    out.top = std::max(0.0f, windowPixels.top - view.viewportMin.y) / view.scale;
    out.right = std::max(0.0f, windowPixels.right - (image.x - view.viewportMax.x)) / view.scale;
    out.bottom = std::max(0.0f, windowPixels.bottom - (image.y - view.viewportMax.y)) / view.scale;
    return out;
}

TouchLayout TouchControls::ComputeLayout(const TouchManifest& manifest, const glm::vec2& screen,
                                         const TouchInsets& safe) {
    TouchLayout layout;
    const float scale = std::clamp(manifest.scale, kMinScale, kMaxScale);
    const glm::vec2 lo(std::max(0.0f, safe.left), std::max(0.0f, safe.top));
    const glm::vec2 hi = glm::max(lo, screen - glm::vec2(std::max(0.0f, safe.right), std::max(0.0f, safe.bottom)));
    for (int i = 0; i < kTouchControlCount; ++i) {
        const auto index = static_cast<std::size_t>(i);
        const TouchControlSpec& spec = manifest.controls[index];
        const glm::vec2 size = glm::max(spec.size, glm::vec2(1.0f)) * scale;
        const glm::vec2 offset = glm::max(spec.offset, glm::vec2(0.0f)) * scale;
        const bool left = spec.anchor == TouchAnchor::TopLeft || spec.anchor == TouchAnchor::BottomLeft;
        const bool top = spec.anchor == TouchAnchor::TopLeft || spec.anchor == TouchAnchor::TopRight;
        glm::vec2 min(left ? lo.x + offset.x : hi.x - offset.x - size.x,
                      top ? lo.y + offset.y : hi.y - offset.y - size.y);
        // Inside the safe area whatever the offsets say: a hand-edited
        // manifest, or a screen smaller than it was laid out for.
        min = glm::clamp(min, lo, glm::max(lo, hi - size));
        layout.boxes[index].min = min;
        layout.boxes[index].max = min + size;
        layout.hitPadding[index] = std::max(0.0f, spec.hitPadding) * scale;
    }
    return layout;
}

void TouchControls::SetManifest(TouchManifest manifest) {
    m_manifest = std::move(manifest);
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

TouchStep TouchControls::Update(const TouchInput& input) {
    m_scene = input.scene;
    m_corner = input.corner;
    m_layout = ComputeLayout(m_manifest, input.screen, input.safeArea);
    const bool play = m_scene == TouchScene::Play;
    m_visible.fill(false);
    for (const TouchControl control :
         {TouchControl::Dpad, TouchControl::Jump, TouchControl::Sword, TouchControl::Fire, TouchControl::Light}) {
        m_visible[static_cast<std::size_t>(control)] = play;
    }
    m_visible[static_cast<std::size_t>(TouchControl::Pause)] = m_corner == TouchCorner::Pause;
    m_visible[static_cast<std::size_t>(TouchControl::Back)] = m_corner == TouchCorner::Back;

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
        if (!ownerVisible(held.owner)) held.owner = Owner::None;
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
        switch (hit(contact.position)) {
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
    const TouchLayout::Box& dpad = m_layout[TouchControl::Dpad];
    const glm::vec2 dpadSize = dpad.Size();
    const float radius = 0.5f * std::min(dpadSize.x, dpadSize.y);
    m_knob = dpad.Centre();
    m_dpadHeld = false;
    const auto hold = [&step](TouchAction action) { step.held[static_cast<std::size_t>(action)] = true; };
    for (const auto& entry : m_contacts) {
        const Held& held = entry.second;
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
                    if (!nearVertical) hold(d.x > 0.0f ? TouchAction::Right : TouchAction::Left);
                    // Down and both of its diagonals (y is down).
                    if (d.y > 0.0f && d.y >= across * kTan22) hold(TouchAction::Down);
                }
                // The knob follows the thumb, never past the disc's rim.
                const float knobRadius = 0.5f * std::min(m_manifest.knobSize.x, m_manifest.knobSize.y) *
                                         std::clamp(m_manifest.scale, kMinScale, kMaxScale);
                const float travel = std::max(0.0f, radius - knobRadius);
                const float length = glm::length(d);
                m_knob = dpad.Centre() + (length > travel && length > 0.0f ? d * (travel / length) : d);
                break;
            }
            case Owner::Jump: hold(TouchAction::Jump); break;
            case Owner::Sword: hold(TouchAction::Sword); break;
            case Owner::Fire: hold(TouchAction::Fire); break;
            case Owner::Light: hold(TouchAction::Light); break;
            case Owner::Corner: hold(TouchAction::Cancel); break;
            case Owner::Pointer:
                step.pointer = true;
                step.pointerPos = held.position;
                break;
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
                const glm::vec2 knob = m_manifest.knobSize * std::clamp(m_manifest.scale, kMinScale, kMaxScale);
                if (knob.x > 0.0f && knob.y > 0.0f) {
                    draw(m_manifest.knobImage, m_knob - knob * 0.5f, knob, m_dpadHeld ? pressed : idle, false);
                }
                break;
            }
            case TouchControl::Jump:
            case TouchControl::Sword:
            case TouchControl::Fire:
            case TouchControl::Light:
            case TouchControl::Pause:
            case TouchControl::Back: {
                const TouchAction action = control == TouchControl::Jump    ? TouchAction::Jump
                                           : control == TouchControl::Sword ? TouchAction::Sword
                                           : control == TouchControl::Fire  ? TouchAction::Fire
                                           : control == TouchControl::Light ? TouchAction::Light
                                                                            : TouchAction::Cancel;
                draw(spec.image, box.min, box.Size(), held(action) ? pressed : idle, true);
                break;
            }
            case TouchControl::Count: break;
        }
    }
}

} // namespace Penumbra::Render
