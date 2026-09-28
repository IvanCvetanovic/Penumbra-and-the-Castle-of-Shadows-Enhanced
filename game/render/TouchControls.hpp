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
// combo (playerInput.as:380). Every action fires on its KS_HIT and none repeats
// while held, so a button is simply held while a finger is on it.
//
// THE DIRECTION CONTROL is one round control the thumb slides on without
// lifting: left and right by which side of the centre it is, down within 67.5
// degrees of straight down, nothing in the dead zone at the centre or within
// 22.5 degrees of straight up. Straight down is down alone, so a thumb held
// down at the door does not walk off it. It is drawn as three buttons, left,
// right and down, each where its sector is (tools/art/make_mr_touch_art.py
// checks the art against these sectors).
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
// COMBOS. Two buttons play the original's combos (playerInput.as:358-395),
// which a thumb on a disc can hardly make: a tap runs a short macro of player
// 1's keys, one press a tick, the side being the way the wizard faces
// (TouchInput::facing, his currentDir; else the side the disc was last pushed;
// else right, as a currentDir never written reads):
//
//   sword combo   tick 0 -, 1 side, 2 -, 3 side, 4 K_S    CMD side, side, SWORD
//   spell combo   tick 0 -, 1 K_DOWN, 2 side, 3 K_D       CMD DOWN, side, SPELL
//
// Tick 0 lets go of every key the combos read, so the first press is a fresh
// KS_HIT whatever the disc held. The two presses of the same side need the
// release between them; a different key's KS_HIT is recorded while the last
// one is still releasing (combo.as records the first HIT of a frame). THE
// BUFFER MUST BE EMPTY for checkSequence, which matches only the first three
// commands since it was last emptied, and combo.as:116 empties it only on a
// frame without a new command more than BUTTON_STRIDE (210 ms) after the last
// one: 13 ticks at GetTime's 1000/60 ms (216-217 ms; 12 are 200). So the first
// press waits until the game has run kComboQuietTicks ticks in a row in which
// nothing the combos read was newly pressed, by any device (ObserveFrame): at
// once when nothing was pressed for a while, 14 ticks after a step. While a combo
// runs, the disc's directions and the sword and fire buttons are held back
// (their presses would land in the buffer), jump, light and pause are not; a
// sword or fire finger held through a combo stays held back until it lifts
// (it would swing again), the disc steers again at once. A second tap on
// either combo button is ignored until the combo ends; leaving play (the
// pause, a menu) or a scene load (TouchInput::sceneSerial) cancels it, and
// its keys are simply not pressed any more.
//
// THE CORNER BUTTON sends K_ESC; its picture says what that does. Which one a
// screen gets is CornerFor's, one reason a screen:
//   the pause               hidden: its rows are the way on
//   a level, an arena       pause (E13)
//   their end screens       back: doLoop's waitForInputToMenu reads cancel
//   the arena select,       back: they read cancel (waitForInputToMenu) and
//   game over                     no click, and draw no button of their own
//   the options             hidden: the original's own Back arrow is there
//                                   (putBackButton), and a finger clicks it
//   the main menu           hidden: cancel does nothing there (goToMenu)
//
// THE LOOK is data, not code: game/data/touch_controls.json names each
// control's image (under game/data: images/touch/ holds Magic Rampage's
// screen-pad buttons, which tools/art/make_mr_touch_art.py makes from its
// package, and images/touch/placeholder/ the first look, with its own
// manifest) and places it - the corner of the logical screen it hangs from,
// its distance from that corner, its size, in logical pixels (the logical
// screen is always 768 tall, so these are already relative to the screen).
// Replacing the art is replacing the PNGs and editing the manifest; a control
// the manifest marks "enabled": false is not there at all (a layout without
// the combo buttons, say). Images go through the HUD's TextureCache as the
// scripts' HUD images do, magenta (#FF00FF) keyed out.
//
// Pure, like PauseMenu: fed this tick's contacts (and, for the combos, the
// frames the game ran), it answers what is held. No window, no Machine.

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
    Hidden,   // the main menu, the options (a Back of their own), the pause
    Pause,    // in play: K_ESC opens E13's pause
    Back,     // the arena select, game over, the end screens: K_ESC leaves
};

// Where the game is, for TouchControls::CornerFor.
struct TouchScreen {
    std::string sceneFile;       // GetSceneFileName(): "scenes/menu.esc", "" before the first load
    bool level = false;          // levelLoop or pvpLoop runs: a level, an arena, or their end screen
    bool gameFinished = false;   // g_gameFinished: that end screen (the campaign's, a Versus win)
    bool paused = false;         // E13's pause is open
};

// Which way the wizard faces (his currentDir), when there is a wizard.
enum class TouchFacing { Unknown, Left, Right };

// A combo button's macro.
enum class TouchCombo { None, Sword, Spell };

struct TouchInput {
    std::vector<TouchContact> contacts;
    glm::vec2 screen{1024.0f, 768.0f};   // the logical screen (GetScreenSize)
    TouchInsets safeArea;
    TouchScene scene = TouchScene::Play;
    TouchCorner corner = TouchCorner::Hidden;
    TouchFacing facing = TouchFacing::Unknown;
    // RenderSnapshot::sceneSerial: a change (a load, a death's reload) cancels a combo.
    unsigned sceneSerial = 0;
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
    // The combo that ran this tick (waiting or pressing), or None.
    TouchCombo combo = TouchCombo::None;

    bool Held(TouchAction action) const { return held[static_cast<std::size_t>(action)]; }
};

// What is drawn and touched; the manifest's ids, in drawing order.
enum class TouchControl : int { Dpad = 0, Jump, Sword, Fire, Light, SwordCombo, SpellCombo, Pause, Back, Count };
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
    bool enabled = true;            // false: the layout has no such control
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
    // The knob with no thumb pointing a direction: at the control's centre
    // (true, the placeholder's disc and its knob) or not drawn (false, the
    // shipped look: Magic Rampage's brackets there frame the empty gap
    // between the left and right buttons, which reads as a missing button).
    bool knobAtRest = true;
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

    // "dpad", "jump", "sword", "fire", "light", "swordCombo", "spellCombo",
    // "pause", "back".
    static const char* ControlId(TouchControl control);
    // How many ticks in a row with nothing newly pressed that the combos read
    // empty the combo buffer: the first whose GetTime (frame * 1000 / 60) is
    // more than BUTTON_STRIDE, 210 ms (combo.as:44, :116), after the last press.
    static constexpr unsigned kComboQuietTicks = 13;
    // A combo that has waited this long for the quiet gives up: something else
    // keeps pressing.
    static constexpr unsigned kComboMaxWaitTicks = 30;
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

    // The corner button a screen gets (THE CORNER BUTTON above).
    static TouchCorner CornerFor(const TouchScreen& screen);

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

    // Once for every tick the game runs, with the frame it runs (after the
    // pause's filter): whether anything the combo buffer reads for player 1
    // was newly pressed - his six keys, and his pad (`player1Pad`,
    // getPlayerJoystick(0)): the stick past 0.8, JK_04, JK_02
    // (playerInput.as:55-239). Touch or not: a combo tapped later must know.
    void ObserveFrame(const Eth::InputFrame& frame, int player1Pad);
    // Stops a running combo; its keys are not pressed from the next Update.
    void CancelCombo();
    TouchCombo RunningCombo() const { return m_combo.combo; }

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
    enum class Owner { None, Dpad, Jump, Sword, Fire, Light, SwordCombo, SpellCombo, Corner, Pointer };

    struct Held {
        Owner owner = Owner::None;
        glm::vec2 position{0.0f};
        bool heldBack = false;   // a sword or fire finger a combo ran under: nothing until it lifts
    };

    struct Combo {
        TouchCombo combo = TouchCombo::None;
        TouchAction side = TouchAction::Right;
        std::size_t next = 0;    // the next tick of its timeline
        bool pressed = false;    // its first press is made
        unsigned waited = 0;     // ticks held back for the buffer to empty
    };

    void startCombo(TouchCombo combo, TouchFacing facing);

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
    // The thumb on the direction control points a direction: past the dead
    // zone and not straight up. By where it is, not by what is held, which a
    // combo holds back or presses with no thumb there.
    bool m_dpadPointing = false;

    Combo m_combo;
    TouchAction m_lastSide = TouchAction::Right;   // the disc's last left or right
    unsigned m_sceneSerial = 0;
    // ObserveFrame: what the combos read that was down, and for how many run
    // ticks in a row nothing of it was newly pressed. Quiet until told.
    unsigned m_observed = 0;
    unsigned m_quietTicks = kComboQuietTicks;
};

} // namespace Penumbra::Render
