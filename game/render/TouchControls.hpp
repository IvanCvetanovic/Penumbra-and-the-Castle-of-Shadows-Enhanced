#pragma once

// ENHANCEMENT E16: on-screen touch controls, for the Android and iOS builds. The
// original was a keyboard-and-joystick game (playerInput.as); a phone has
// neither.
//
// WHAT A FINGER PRESSES. The keys player 1's keyboard presses, into the same
// Eth::InputFrame InputMapper builds, before the pause and the game read it -
// so the ported scripts read the original's keys and change nothing:
//
//   direction control   K_LEFT, K_RIGHT, K_DOWN   held: walking reads KeyDown
//                                                 (getPlayerXYAxis), combos and
//                                                 the menus' edges read KS_HIT
//   jump                K_CTRL                    KS_HIT (getJumpButtonStatus)
//   sword               K_S                       KS_HIT (getAttack01ButtonStatus)
//   fire                K_D                       KS_HIT (getAttack02ButtonStatus)
//   light               K_SPACE                   KS_HIT (getAttack03ButtonStatus)
//   pause / back        K_ESC                     KS_HIT: E13's pause in play,
//                                                 escToGoToMenu / waitForInputToMenu
//                                                 elsewhere (menu.as:374-400)
//
// Keys, not a virtual joystick: a connected pad would put the joystick icon on
// the menus (detectJoysticks, menu.as:74) and could make hasASecondController()
// true (playerInput.as:159). K_CTRL and not K_UP for the jump: K_UP is also
// the combos' CMD_UP and the pause's Up, and a pad's stick-up does not jump
// either (JK_03 does). There is no up on the direction control: nothing in
// play needs it (up only jumps, and the jump has its own button). Down is
// needed: held at the next_level door (main.as:208) and first in the spell
// combo (playerInput.as:376). Every action fires on its KS_HIT and none repeats
// while held, so a button is simply held while a finger is on it.
//
// THE DIRECTION CONTROL is a disc the thumb slides on without lifting: left and
// right by which side of the centre it is, down within 67.5 degrees of
// straight down, nothing in the dead zone at the centre or within 22.5 degrees
// of straight up. Straight down is down alone, so a thumb held down at the
// door does not walk off it.
//
// FINGERS. A contact belongs to what it first landed on, until it lifts:
// sliding off a button keeps it held, sliding onto one presses nothing, and a
// contact that lands on no control does nothing in play. Two fingers on one
// button hold it once (one KS_HIT); a second finger on the direction control
// is ignored. A contact whose control is hidden under it - the pause opens,
// a death loads game over - is dead until it lifts: it neither keeps its key
// down nor turns into a click.
//
// MENUS (TouchScene::Menu: the menus, the options, game over, and the pause's
// rows). The direction control and the action buttons are hidden, and a
// finger on no control is the mouse: the cursor goes where it is and the left
// button is held while it stays - cursor and click in the same tick, which
// every reader of the cursor (the menu's cursor.ent, Switch, the video modes,
// the pause) takes as a click at that point. Only the first such finger; the
// others are ignored. A platform may make a finger the mouse as well - on the
// desktop the held mouse is contact 0 (Input::SynthesiseMouseContact), on
// Android the first finger of a gesture holds the left button and moves the
// pointer - so while any finger is down the left button is the touch
// controls' to say: a finger on a button never clicks under it, and a thumb
// resting on the direction control does not hold the button down through a
// second finger's tap. With no finger down the real mouse is left as it is.
//
// THE CORNER BUTTON sends K_ESC; its picture says what that does. Hidden in
// the main menu (cancel does nothing there) and in the pause (its rows are the
// way on); the only way out of the arena select and game over, whose screens
// read cancel and not a click.
//
// THE LOOK is data, not code: game/data/touch_controls.json names each
// control's image (under game/data, images/touch/ for the placeholders
// tools/art/make_touch_art.py draws) and places it - the corner of the
// logical screen it hangs from, its distance from that corner, its size, in
// logical pixels (the logical screen is always 768 tall, so these are already
// relative to the screen). Replacing the art is replacing the PNGs and editing
// the manifest. Images go through the HUD's TextureCache as the scripts' HUD
// images do, magenta (#FF00FF) keyed out.
//
// Pure, like PauseMenu: fed this tick's contacts, it answers what is held. No
// window, no Machine.

#include <array>
#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "eth/Input.hpp"
#include "eth/Snapshot.hpp"
#include "render/View.hpp"

namespace Penumbra::Render {

// PENUMBRA_MOBILE is defined (with any value, or none) by the Android and iOS
// builds and never by the desktop's: it means "the device is a touchscreen
// with no keyboard", so touchControls "auto" turns the controls on. Tested
// with #ifdef only, so -DPENUMBRA_MOBILE and -DPENUMBRA_MOBILE=1 agree.
#ifdef PENUMBRA_MOBILE
inline constexpr bool kMobileBuild = true;
#else
inline constexpr bool kMobileBuild = false;
#endif

// One finger this tick, as the layer hands it over.
struct TouchContact {
    int id = -1;                    // stable while the finger stays down (Supersonic::Contact::id)
    glm::vec2 position{0.0f};       // logical screen pixels, +y down
    bool down = true;               // false: it lifted this frame (ContactPhase::Ended)
};

// The edges of the screen the controls keep clear of (a notch, rounded
// corners, a gesture bar), in logical pixels.
struct TouchInsets {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    bool operator==(const TouchInsets& other) const = default;
};

enum class TouchScene {
    Play,   // a level or an arena: the direction control and the action buttons
    Menu,   // the menus, the options, game over and the pause: a finger is the mouse
};

enum class TouchCorner {
    Hidden,   // the main menu, the pause
    Pause,    // in play: K_ESC opens E13's pause
    Back,     // the arena select, the options, game over, the end screens: K_ESC leaves
};

struct TouchInput {
    std::vector<TouchContact> contacts;
    glm::vec2 screen{1024.0f, 768.0f};   // the logical screen (GetScreenSize)
    TouchInsets safeArea;
    TouchScene scene = TouchScene::Play;
    TouchCorner corner = TouchCorner::Hidden;
};

// What the controls hold down, by what it does; each is one of player 1's keys
// (KeyFor).
enum class TouchAction : int { Left = 0, Right, Down, Jump, Sword, Fire, Light, Cancel, Count };
inline constexpr int kTouchActionCount = static_cast<int>(TouchAction::Count);

// One tick's answer.
struct TouchStep {
    std::array<bool, kTouchActionCount> held{};
    // Menu: a finger on no control. The cursor goes to pointerPos and the left
    // mouse button is held.
    bool pointer = false;
    glm::vec2 pointerPos{0.0f};
    // A finger is on the screen. The left mouse button is then the touch
    // controls' alone (ApplyToFrame).
    bool touching = false;

    bool Held(TouchAction action) const { return held[static_cast<std::size_t>(action)]; }
};

// What is drawn and touched; the manifest's ids, in drawing order.
enum class TouchControl : int { Dpad = 0, Jump, Sword, Fire, Light, Pause, Back, Count };
inline constexpr int kTouchControlCount = static_cast<int>(TouchControl::Count);

enum class TouchAnchor { TopLeft, TopRight, BottomLeft, BottomRight };
enum class TouchShape { Circle, Rect };

// One control in the manifest.
struct TouchControlSpec {
    std::string image;              // under the data folder; "" is drawn as a plain square
    TouchAnchor anchor = TouchAnchor::BottomRight;
    glm::vec2 offset{0.0f};         // from the anchor corner to the control's nearest edges, inward
    glm::vec2 size{100.0f};
    TouchShape shape = TouchShape::Circle;
    float hitPadding = 0.0f;        // how far past its edge a finger still lands on it
    bool operator==(const TouchControlSpec& other) const = default;
};

struct TouchManifest {
    std::array<TouchControlSpec, kTouchControlCount> controls{};
    // The direction control's arrows, each drawn over its image (the same box)
    // and brightened while its direction is held, and the knob that follows
    // the thumb. Any may be "" (not drawn).
    std::string dpadLeft;
    std::string dpadRight;
    std::string dpadDown;
    std::string knobImage;
    glm::vec2 knobSize{0.0f};
    float deadZone = 0.25f;         // of the direction control's radius: the thumb there means nothing
    float idleAlpha = 0.45f;        // what a control is drawn at, 0..1
    float pressedAlpha = 0.9f;      // and while it is held
    float scale = 1.0f;             // every size, offset and padding
    bool operator==(const TouchManifest& other) const = default;

    TouchControlSpec& operator[](TouchControl control) { return controls[static_cast<std::size_t>(control)]; }
    const TouchControlSpec& operator[](TouchControl control) const {
        return controls[static_cast<std::size_t>(control)];
    }
};

// Where each control is on one screen, logical pixels.
struct TouchLayout {
    struct Box {
        glm::vec2 min{0.0f};
        glm::vec2 max{0.0f};
        glm::vec2 Centre() const { return (min + max) * 0.5f; }
        glm::vec2 Size() const { return max - min; }
    };
    std::array<Box, kTouchControlCount> boxes{};
    std::array<float, kTouchControlCount> hitPadding{};

    const Box& operator[](TouchControl control) const { return boxes[static_cast<std::size_t>(control)]; }
};

class TouchControls {
public:
    // The manifest, in the data folder (eth/Paths.hpp's data root).
    static constexpr const char* kManifestFile = "touch_controls.json";

    // "dpad", "jump", "sword", "fire", "light", "pause", "back".
    static const char* ControlId(TouchControl control);
    // The key an action presses (the table at the top of this file).
    static Eth::KEY KeyFor(TouchAction action);

    // The layout and the art the port ships, as touch_controls.json has them:
    // what is used when the manifest is missing, and under each field it lacks.
    static TouchManifest DefaultManifest();
    // Over the defaults, field by field; what is wrong goes to `warning`.
    static TouchManifest ManifestFromJson(const std::string& text, std::string* warning = nullptr);
    static TouchManifest LoadManifest(const std::filesystem::path& file, std::string* warning = nullptr);

    // settings.touchControls: "on", "off", or "auto" (on in a mobile build).
    static bool EnabledBySetting(const std::string& setting);

    // Safe-area insets as a platform reports them (window pixels, from each
    // edge of the window) -> logical pixels, less whatever the pillarbox bars
    // already keep clear.
    static TouchInsets WindowInsetsToLogical(const TouchInsets& windowPixels, const View& view);

    // Each control's box: from its anchor corner of the safe area, clamped
    // into it.
    static TouchLayout ComputeLayout(const TouchManifest& manifest, const glm::vec2& screen, const TouchInsets& safe);

    void SetManifest(TouchManifest manifest);
    const TouchManifest& Manifest() const { return m_manifest; }
    // Where the manifest's images are (the data folder). Each is looked for
    // once, here; one that is missing is drawn as a plain square.
    void SetImageRoot(const std::filesystem::path& dataDir);

    // A frame that ran no tick: fingers that came down in it are handed to
    // the next tick, so a tap shorter than a tick still presses (InputMapper
    // latches keys the same way).
    void LatchFrame(const std::vector<TouchContact>& contacts);

    // Once a tick, before the frame is read.
    TouchStep Update(const TouchInput& input);

    // Presses the step's keys in `frame` (never releases one the keyboard
    // holds) and, for a pointer, moves the cursor and holds the left button.
    // While a finger is down the left button is the pointer's and nothing
    // else's: a platform makes one finger the mouse too (Android's first
    // finger, the desktop's held mouse as contact 0), and that finger on a
    // button must not also click under it, nor hold the button down so that
    // another finger's tap is no fresh click.
    static void ApplyToFrame(const TouchStep& step, Eth::InputFrame& frame);

    // The controls as the last Update left them, appended to `out` for
    // HudRenderer::Draw's `extra`: over the scripts' HUD, under the pause.
    void AppendOverlay(std::vector<Eth::HudCmd>& out) const;

    bool Visible(TouchControl control) const { return m_visible[static_cast<std::size_t>(control)]; }
    const TouchLayout& Layout() const { return m_layout; }

private:
    enum class Owner { None, Dpad, Jump, Sword, Fire, Light, Corner, Pointer };

    struct Held {
        Owner owner = Owner::None;
        glm::vec2 position{0.0f};
    };

    // The visible control a new finger at `point` lands on, or Count.
    TouchControl hit(const glm::vec2& point) const;
    bool ownerVisible(Owner owner) const;
    std::string resolved(const std::string& image) const;

    TouchManifest m_manifest = DefaultManifest();
    std::filesystem::path m_imageRoot;
    // Each image name, resolved to an absolute path, or "" when it is not there.
    std::map<std::string, std::string> m_images;

    TouchScene m_scene = TouchScene::Play;
    TouchCorner m_corner = TouchCorner::Hidden;
    TouchLayout m_layout;
    std::array<bool, kTouchControlCount> m_visible{};

    std::map<int, Held> m_contacts;          // every finger down, by id
    std::map<int, glm::vec2> m_latched;      // down in a frame no tick saw
    TouchStep m_last;
    glm::vec2 m_knob{0.0f};                  // where the knob is drawn (the thumb, within the disc)
    bool m_dpadHeld = false;
};

} // namespace Penumbra::Render
