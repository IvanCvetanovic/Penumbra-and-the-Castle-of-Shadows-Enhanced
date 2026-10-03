// E16, the on-screen touch controls (render/TouchControls): which key each
// control presses and that the Eth runtime sees it as the keyboard's (KS_HIT,
// then KS_DOWN, then KS_RELEASE); the direction control's sectors and a thumb
// sliding between them; several fingers at once; a finger's control being the
// one it landed on; the pause opened and tapped through E13's own pieces; a
// tap in a menu as a click at that point; what is shown where (the knob only
// while the thumb points a direction, the placeholder's at rest as before);
// the corner button for every scene the original has, the options screen's
// own Back included; the layout on 4:3 and widescreen screens with and without
// a safe area; the manifest (the built-in one the shipped file's) and its art;
// the touchControls setting. Then the combo buttons: each macro's keys tick by
// tick for both facings, the wait for the combo buffer to empty, a double
// tap, the fingers held back under a combo, its cancelling - and the combos
// firing through the ported Combo in a bare Machine, and through the real game
// in level 1, whose first help sign the HUD draws in touch wording (skipped
// without the original's files, the rest of the suite is not). E25: the
// direction control without its down arrow, the down button shown only while
// the next_level door offers the way on, the layout at a zoomed screen's
// scale, and level 1 zoomed as on a phone, left through the button; the
// princess taking a zoomed level back to E1's screen. E26: the pause button
// in step with the HUD's frame, and level 1's HUD moved and restyled by it
// on touch only. E29, E32, E33: the six action buttons as two staggered columns of three (the sword at the
// bottom of the left column, the jump button at the bottom of the right one, which is 30 higher than the left; above   // E33
// them the sword combo and the light, the spell combo and the fire, the combos halfway between, 16 between   // E33
// neighbours everywhere): every box on three screens, the columns' alignment, stagger and gaps, the order from the bottom,   // E33
// the clearance of the pause button, the gaps no finger lands in, and the size ceilings that follow from it. E34: the   // E33
// layout the saved moves belong to (the value pinned in testTuningIdentity).   // E34
// E37: the combo buttons' cooldown: its length and clock, the refused taps and their flash and cue, a finger that stays down,
// the buttons resting apart, what cancels a macro or a load clears, the look in play and not in the editor, and the combos
// typed by hand, which have no limit.   // E37
// No window. Only the combo checks run a Machine.

#include "script/Script.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <initializer_list>
#include <iterator>
#include <limits>   // E28
#include <random>
#include <set>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

#include <entt/entt.hpp>

#include "TestHarness.hpp"

#include "eth/Input.hpp"
#include "eth/Machine.hpp"
#include "eth/Snapshot.hpp"
#include "eth/Text.hpp"
#include "render/FontAtlas.hpp"
#include "render/HudRenderer.hpp"
#include "render/InputMapper.hpp"
#include "render/Localization.hpp"
#include "render/PauseMenu.hpp"
#include "render/PhoneUi.hpp"
#include "render/Settings.hpp"
#include "render/TextureCache.hpp"
#include "render/TextureDecode.hpp"
#include "render/TouchControls.hpp"
#include "render/View.hpp"
#include "render/WideMenus.hpp"

namespace {

namespace fs = std::filesystem;
namespace Script = Penumbra::Script;
using namespace Penumbra::Eth;
using Penumbra::Render::HudRenderer;
using Penumbra::Render::PauseMenu;
using Penumbra::Render::PauseStep;
using Penumbra::Render::Settings;
using Penumbra::Render::TouchAction;
using Penumbra::Render::TouchAnchor;
using Penumbra::Render::TouchCombo;
using Penumbra::Render::TouchContact;
using Penumbra::Render::TouchControl;
using Penumbra::Render::TouchControlSpec;
using Penumbra::Render::TouchControls;
using Penumbra::Render::TouchCorner;
using Penumbra::Render::TouchFacing;
using Penumbra::Render::TouchGeometry;   // E28
using Penumbra::Render::TouchInput;
using Penumbra::Render::TouchInsets;
using Penumbra::Render::TouchLayout;
using Penumbra::Render::TouchManifest;
using Penumbra::Render::TouchMove;   // E28
using Penumbra::Render::TouchScene;
using Penumbra::Render::TouchStep;
using Penumbra::Render::TouchTuning;   // E28
using Penumbra::Render::View;
using Penumbra::Render::kTouchActionCount;
using Penumbra::Render::kTouchControlCount;

const glm::vec2 kFourThree{1024.0f, 768.0f};
const glm::vec2 kWide{1366.0f, 768.0f};

constexpr TouchAction kButtons[] = {TouchAction::Jump, TouchAction::Sword, TouchAction::Fire, TouchAction::Light};
constexpr TouchControl kButtonControls[] = {TouchControl::Jump, TouchControl::Sword, TouchControl::Fire,
                                            TouchControl::Light};

TouchContact Finger(int id, glm::vec2 position, bool down = true) { return TouchContact{id, position, down}; }

TouchInput Play(std::vector<TouchContact> contacts = {}, glm::vec2 screen = kFourThree) {
    TouchInput input;
    input.contacts = std::move(contacts);
    input.screen = screen;
    input.scene = TouchScene::Play;
    input.corner = TouchCorner::Pause;
    return input;
}

TouchInput Menu(std::vector<TouchContact> contacts = {}, TouchCorner corner = TouchCorner::Hidden) {
    TouchInput input;
    input.contacts = std::move(contacts);
    input.screen = kFourThree;
    input.scene = TouchScene::Menu;
    input.corner = corner;
    return input;
}

// Where each control is on the 4:3 screen, from the defaults.
TouchLayout Default(glm::vec2 screen = kFourThree) {
    return TouchControls::ComputeLayout(TouchControls::DefaultManifest(), screen, TouchInsets{});
}
glm::vec2 Centre(TouchControl control, glm::vec2 screen = kFourThree) { return Default(screen)[control].Centre(); }
glm::vec2 Dpad(glm::vec2 offset) { return Centre(TouchControl::Dpad) + offset; }
// The same in another manifest's layout: a finger `offset` from the direction control's centre where
// that manifest puts it (E16's disc sits on the screen, the shipped box hangs below its edge).
glm::vec2 DpadIn(const TouchManifest& manifest, glm::vec2 offset) {
    return TouchControls::ComputeLayout(manifest, kFourThree, TouchInsets{})[TouchControl::Dpad].Centre() + offset;
}

// Exactly these are held (and no pointer).
bool Only(const TouchStep& step, const std::vector<TouchAction>& actions) {
    for (int i = 0; i < kTouchActionCount; ++i) {
        const TouchAction action = static_cast<TouchAction>(i);
        const bool wanted = std::find(actions.begin(), actions.end(), action) != actions.end();
        if (step.Held(action) != wanted) return false;
    }
    return !step.pointer;
}

InputFrame FrameOf(const TouchStep& step) {
    InputFrame frame;
    TouchControls::ApplyToFrame(step, frame);
    return frame;
}

int KeysDown(const InputFrame& frame) {
    return static_cast<int>(std::count(frame.keys.begin(), frame.keys.end(), true));
}

// The Eth runtime fed one tick of touches.
KEY_STATE Tick(InputState& state, const TouchStep& step, KEY key) {
    state.Update(FrameOf(step));
    return state.GetKeyState(key);
}

std::uint8_t Alpha(uint argb) { return static_cast<std::uint8_t>(argb >> 24); }

// How many of an overlay's commands draw the direction control's knob.
int Knobs(const std::vector<HudCmd>& out) {
    return static_cast<int>(std::count_if(out.begin(), out.end(), [](const HudCmd& cmd) {
        return cmd.sprite.find("dpad_knob.png") != std::string::npos;
    }));
}

void testKeys() {
    // What playerInput.as reads for player 0 (the header's table).
    CHECK(TouchControls::KeyFor(TouchAction::Left) == K_LEFT);
    CHECK(TouchControls::KeyFor(TouchAction::Right) == K_RIGHT);
    CHECK(TouchControls::KeyFor(TouchAction::Down) == K_DOWN);
    CHECK(TouchControls::KeyFor(TouchAction::Jump) == K_CTRL);
    CHECK(TouchControls::KeyFor(TouchAction::Sword) == K_S);
    CHECK(TouchControls::KeyFor(TouchAction::Fire) == K_D);
    CHECK(TouchControls::KeyFor(TouchAction::Light) == K_SPACE);
    CHECK(TouchControls::KeyFor(TouchAction::Cancel) == K_ESC);

    for (int i = 0; i < kTouchActionCount; ++i) {
        TouchStep step;
        step.held[static_cast<std::size_t>(i)] = true;
        const InputFrame frame = FrameOf(step);
        CHECK_EQ(KeysDown(frame), 1);
        CHECK(frame.keys[TouchControls::KeyFor(static_cast<TouchAction>(i))]);
        // Keys, never a pad: a connected pad would change what the menus show.
        for (const InputFrame::Pad& pad : frame.pads) CHECK(!pad.connected);
    }

    // Pressing only adds: the keyboard's keys stay down.
    InputFrame keyboard;
    keyboard.keys[K_UP] = true;
    keyboard.keys[K_S] = true;
    TouchControls::ApplyToFrame(TouchStep{}, keyboard);
    CHECK(keyboard.keys[K_UP]);
    CHECK(keyboard.keys[K_S]);
    CHECK_EQ(KeysDown(keyboard), 2);

    // A pointer: the cursor there, and the left button.
    TouchStep pointer;
    pointer.pointer = true;
    pointer.pointerPos = glm::vec2(300.0f, 200.0f);
    const InputFrame click = FrameOf(pointer);
    CHECK(click.keys[K_LMOUSE]);
    CHECK_EQ(KeysDown(click), 1);
    CHECK(click.cursor == glm::vec2(300.0f, 200.0f));
    CHECK(click.cursorAbsolute == glm::vec2(300.0f, 200.0f));

    // A finger down and no pointer: the left button is let go of.
    TouchStep touching;
    touching.touching = true;
    InputFrame mouse;
    mouse.keys[K_LMOUSE] = true;
    TouchControls::ApplyToFrame(touching, mouse);
    CHECK(!mouse.keys[K_LMOUSE]);
    // No finger: it is left as it is.
    mouse.keys[K_LMOUSE] = true;
    TouchControls::ApplyToFrame(TouchStep{}, mouse);
    CHECK(mouse.keys[K_LMOUSE]);
}

void testEachButton() {
    for (std::size_t b = 0; b < std::size(kButtons); ++b) {
        const TouchAction action = kButtons[b];
        const KEY key = TouchControls::KeyFor(action);
        const glm::vec2 at = Centre(kButtonControls[b]);
        TouchControls touch;
        InputState state;
        TouchStep step = touch.Update(Play({Finger(1, at)}));
        CHECK(Only(step, {action}));
        CHECK_EQ(KeysDown(FrameOf(step)), 1);
        CHECK(Tick(state, step, key) == KS_HIT);
        step = touch.Update(Play({Finger(1, at)}));
        CHECK(Tick(state, step, key) == KS_DOWN);        // held: one press, no repeat
        step = touch.Update(Play({Finger(1, at, false)}));   // lifted (ContactPhase::Ended)
        CHECK(Only(step, {}));
        CHECK(Tick(state, step, key) == KS_RELEASE);
        step = touch.Update(Play());
        CHECK(Tick(state, step, key) == KS_UP);
    }

    // The pause button: the original's cancel.
    TouchControls touch;
    TouchStep step = touch.Update(Play({Finger(1, Centre(TouchControl::Pause))}));
    CHECK(Only(step, {TouchAction::Cancel}));
    CHECK(FrameOf(step).keys[K_ESC]);
    step = touch.Update(Play());
    CHECK(Only(step, {}));

    // A near miss counts (hitPadding), a far one does not.
    const TouchManifest manifest = TouchControls::DefaultManifest();
    const float reach = 0.5f * manifest[TouchControl::Jump].size.x + manifest[TouchControl::Jump].hitPadding;
    TouchControls edge;
    step = edge.Update(Play({Finger(1, Centre(TouchControl::Jump) + glm::vec2(0.0f, reach - 1.0f))}));
    CHECK(Only(step, {TouchAction::Jump}));
    TouchControls miss;
    step = miss.Update(Play({Finger(1, Centre(TouchControl::Jump) + glm::vec2(0.0f, reach + 1.0f))}));
    CHECK(Only(step, {}));
}

// E16's disc, with its down sector: the built-in layout before E25 (the
// placeholder look's still has it).
TouchManifest DiscWithDown() {
    TouchManifest manifest = TouchControls::DefaultManifest();
    manifest.downSector = true;
    manifest.dpadDown = "images/touch/dpad_down.png";
    manifest[TouchControl::ExitDown].enabled = false;
    // E16's disc sat on the screen: its down arrow is drawn below the centre,
    // which the shipped box, hanging below the edge, would take off the screen.
    manifest[TouchControl::Dpad].offset = glm::vec2(24.0f, 24.0f);
    manifest[TouchControl::Dpad].overhang = glm::vec2(0.0f);
    return manifest;
}

void testDirections() {
    const TouchManifest manifest = TouchControls::DefaultManifest();
    const float radius = 0.5f * manifest[TouchControl::Dpad].size.x;
    const float dead = manifest.deadZone * radius;
    const auto steerWith = [](const TouchManifest& with, glm::vec2 offset) {
        TouchControls touch;
        touch.SetManifest(with);
        return touch.Update(Play({Finger(1, DpadIn(with, offset))}));
    };
    const auto steer = [&](glm::vec2 offset) { return steerWith(manifest, offset); };
    CHECK(!manifest.downSector);   // E25
    CHECK(Only(steer({0.0f, 0.0f}), {}));
    CHECK(Only(steer({dead - 2.0f, 0.0f}), {}));                 // the dead zone
    CHECK(Only(steer({-dead - 2.0f, 0.0f}), {TouchAction::Left}));
    CHECK(Only(steer({-100.0f, 0.0f}), {TouchAction::Left}));
    CHECK(Only(steer({100.0f, 0.0f}), {TouchAction::Right}));
    // E25: left and right only. Straight down, as straight up, is nothing;
    // down-left is left, down-right right.
    CHECK(Only(steer({0.0f, 100.0f}), {}));
    CHECK(Only(steer({30.0f, 100.0f}), {}));                     // 16.7 degrees off: still straight
    CHECK(Only(steer({-80.0f, 80.0f}), {TouchAction::Left}));
    CHECK(Only(steer({80.0f, 80.0f}), {TouchAction::Right}));
    CHECK(Only(steer({-100.0f, 30.0f}), {TouchAction::Left}));
    CHECK(Only(steer({-100.0f, 45.0f}), {TouchAction::Left}));
    // E16's disc (a manifest's "downSector": true): straight down is down
    // alone, so a thumb held down at the next_level door (main.as:208) does
    // not walk off it.
    const TouchManifest withDown = DiscWithDown();
    CHECK(Only(steerWith(withDown, {0.0f, 100.0f}), {TouchAction::Down}));
    CHECK(Only(steerWith(withDown, {30.0f, 100.0f}), {TouchAction::Down}));    // 16.7 degrees off: still straight
    CHECK(Only(steerWith(withDown, {-80.0f, 80.0f}), {TouchAction::Left, TouchAction::Down}));
    CHECK(Only(steerWith(withDown, {80.0f, 80.0f}), {TouchAction::Right, TouchAction::Down}));
    CHECK(Only(steerWith(withDown, {-100.0f, 30.0f}), {TouchAction::Left}));   // 16.7 degrees below: still level
    CHECK(Only(steerWith(withDown, {-100.0f, 45.0f}), {TouchAction::Left, TouchAction::Down}));
    // No up: straight up is nothing, up-left is left.
    CHECK(Only(steer({0.0f, -100.0f}), {}));
    CHECK(Only(steer({-80.0f, -80.0f}), {TouchAction::Left}));
    CHECK(Only(steer({80.0f, -80.0f}), {TouchAction::Right}));
    CHECK(Only(steerWith(withDown, {0.0f, -100.0f}), {}));
    // Inside the padding past the rim: still the control's.
    CHECK(Only(steer({-radius - 30.0f, 0.0f}), {TouchAction::Left}));

    // The thumb slides from left to right without lifting.
    TouchControls touch;
    InputState state;
    TouchStep step = touch.Update(Play({Finger(3, Dpad({-100.0f, 0.0f}))}));
    CHECK(Only(step, {TouchAction::Left}));
    state.Update(FrameOf(step));
    CHECK(state.GetKeyState(K_LEFT) == KS_HIT);
    step = touch.Update(Play({Finger(3, Dpad({-20.0f, 0.0f}))}));   // through the dead zone
    CHECK(Only(step, {}));
    state.Update(FrameOf(step));
    CHECK(state.GetKeyState(K_LEFT) == KS_RELEASE);
    step = touch.Update(Play({Finger(3, Dpad({100.0f, 0.0f}))}));
    CHECK(Only(step, {TouchAction::Right}));
    state.Update(FrameOf(step));
    CHECK(state.GetKeyState(K_RIGHT) == KS_HIT);
    CHECK(state.GetKeyState(K_LEFT) == KS_UP);
    // Straight across, skipping the centre: left lets go as right comes down.
    step = touch.Update(Play({Finger(3, Dpad({-100.0f, 0.0f}))}));
    state.Update(FrameOf(step));
    CHECK(state.GetKeyState(K_LEFT) == KS_HIT);
    CHECK(state.GetKeyState(K_RIGHT) == KS_RELEASE);
    // And far past the rim it still steers: the thumb owns the control until it lifts.
    step = touch.Update(Play({Finger(3, Dpad({-400.0f, -200.0f}))}));
    CHECK(Only(step, {TouchAction::Left}));
    step = touch.Update(Play({Finger(3, Dpad({-400.0f, -200.0f}), false)}));
    CHECK(Only(step, {}));

    // The sword combo (Left, Left, Sword within 210 ms each, combo.as): left,
    // back to the centre, left again - two presses a tick apart.
    TouchControls combo;
    InputState comboState;
    const glm::vec2 path[] = {{-100.0f, 0.0f}, {0.0f, 0.0f}, {-100.0f, 0.0f}};
    const KEY_STATE expected[] = {KS_HIT, KS_RELEASE, KS_HIT};
    for (std::size_t i = 0; i < std::size(path); ++i) {
        comboState.Update(FrameOf(combo.Update(Play({Finger(4, Dpad(path[i]))}))));
        CHECK(comboState.GetKeyState(K_LEFT) == expected[i]);
    }

    // The spell combo starts with Down, then a side (combo.as records one
    // command a frame, Left before Down): down first, then down-left, on E16's
    // disc (E25's makes it with its combo button).
    TouchControls spell;
    spell.SetManifest(withDown);
    InputState spellState;
    spellState.Update(FrameOf(spell.Update(Play({Finger(5, DpadIn(withDown, {0.0f, 100.0f}))}))));
    CHECK(spellState.GetKeyState(K_DOWN) == KS_HIT);
    CHECK(spellState.GetKeyState(K_LEFT) == KS_UP);
    spellState.Update(FrameOf(spell.Update(Play({Finger(5, DpadIn(withDown, {-80.0f, 80.0f}))}))));
    CHECK(spellState.GetKeyState(K_DOWN) == KS_DOWN);
    CHECK(spellState.GetKeyState(K_LEFT) == KS_HIT);

    // One thumb steers: a second finger landing on the control is ignored.
    TouchControls two;
    step = two.Update(Play({Finger(1, Dpad({-100.0f, 0.0f}))}));
    step = two.Update(Play({Finger(1, Dpad({-100.0f, 0.0f})), Finger(2, Dpad({100.0f, 0.0f}))}));
    CHECK(Only(step, {TouchAction::Left}));
    // Nor does it take over when the first lifts: it landed as nobody's.
    step = two.Update(Play({Finger(2, Dpad({100.0f, 0.0f}))}));
    CHECK(Only(step, {}));
}

void testManyFingers() {
    const glm::vec2 right = Dpad({100.0f, 0.0f});
    const glm::vec2 sword = Centre(TouchControl::Sword);
    const glm::vec2 jump = Centre(TouchControl::Jump);

    // Right held and the sword tapped in the same tick.
    TouchControls same;
    InputState sameState;
    TouchStep step = same.Update(Play({Finger(1, right), Finger(2, sword)}));
    CHECK(Only(step, {TouchAction::Right, TouchAction::Sword}));
    sameState.Update(FrameOf(step));
    CHECK(sameState.GetKeyState(K_RIGHT) == KS_HIT);
    CHECK(sameState.GetKeyState(K_S) == KS_HIT);

    // Across ticks: right held, the sword tapped on the third, the walk unbroken.
    TouchControls touch;
    InputState state;
    const std::vector<std::vector<TouchContact>> ticks = {
        {Finger(1, right)},
        {Finger(1, right)},
        {Finger(1, right), Finger(2, sword)},
        {Finger(1, right), Finger(2, sword, false)},
        {Finger(1, right)},
        {Finger(1, right), Finger(3, sword), Finger(4, jump)},   // sword and jump at once
        {Finger(1, right, false)},
    };
    const KEY_STATE rightStates[] = {KS_HIT, KS_DOWN, KS_DOWN, KS_DOWN, KS_DOWN, KS_DOWN, KS_RELEASE};
    const KEY_STATE swordStates[] = {KS_UP, KS_UP, KS_HIT, KS_RELEASE, KS_UP, KS_HIT, KS_RELEASE};
    const KEY_STATE jumpStates[] = {KS_UP, KS_UP, KS_UP, KS_UP, KS_UP, KS_HIT, KS_RELEASE};
    for (std::size_t i = 0; i < ticks.size(); ++i) {
        step = touch.Update(Play(ticks[i]));
        state.Update(FrameOf(step));
        CHECK_MSG(state.GetKeyState(K_RIGHT) == rightStates[i], "right, tick " + std::to_string(i));
        CHECK_MSG(state.GetKeyState(K_S) == swordStates[i], "sword, tick " + std::to_string(i));
        CHECK_MSG(state.GetKeyState(K_CTRL) == jumpStates[i], "jump, tick " + std::to_string(i));
    }

    // Two fingers on one button hold it once: one press, released when both lift.
    TouchControls both;
    InputState bothState;
    const std::vector<std::vector<TouchContact>> fingers = {
        {Finger(1, jump)},
        {Finger(1, jump), Finger(2, jump + glm::vec2(10.0f, 0.0f))},
        {Finger(2, jump + glm::vec2(10.0f, 0.0f))},
        {},
    };
    const KEY_STATE jumpHeld[] = {KS_HIT, KS_DOWN, KS_DOWN, KS_RELEASE};
    for (std::size_t i = 0; i < fingers.size(); ++i) {
        step = both.Update(Play(fingers[i]));
        const bool anyOn = i + 1 < fingers.size();
        CHECK(anyOn ? Only(step, {TouchAction::Jump}) : Only(step, {}));
        bothState.Update(FrameOf(step));
        CHECK(bothState.GetKeyState(K_CTRL) == jumpHeld[i]);
    }
}

void testWhereItLanded() {
    const glm::vec2 open(512.0f, 300.0f);   // on no control
    const glm::vec2 jump = Centre(TouchControl::Jump);

    // Landing on nothing, in play: nothing, and no click.
    TouchControls touch;
    TouchStep step = touch.Update(Play({Finger(1, open)}));
    CHECK(Only(step, {}));
    CHECK_EQ(KeysDown(FrameOf(step)), 0);
    // Sliding onto a button presses nothing: a finger is what it landed on.
    step = touch.Update(Play({Finger(1, jump)}));
    CHECK(Only(step, {}));
    step = touch.Update(Play({Finger(1, Dpad({-100.0f, 0.0f}))}));
    CHECK(Only(step, {}));
    step = touch.Update(Play());
    // A new finger on the button does.
    step = touch.Update(Play({Finger(1, jump)}));
    CHECK(Only(step, {TouchAction::Jump}));

    // Landing on a button and sliding off keeps it held; the lift lets go,
    // wherever it is.
    TouchControls slide;
    const glm::vec2 sword = Centre(TouchControl::Sword);
    step = slide.Update(Play({Finger(7, sword)}));
    CHECK(Only(step, {TouchAction::Sword}));
    step = slide.Update(Play({Finger(7, open)}));
    CHECK(Only(step, {TouchAction::Sword}));
    step = slide.Update(Play({Finger(7, open, false)}));
    CHECK(Only(step, {}));
    // A finger the device stops reporting without an end lets go too.
    step = slide.Update(Play({Finger(8, sword)}));
    CHECK(Only(step, {TouchAction::Sword}));
    step = slide.Update(Play());
    CHECK(Only(step, {}));
}

// E13's pause, opened and tapped with the touch controls through its own
// pieces, as the layer runs them each tick: TouchControls, ApplyToFrame,
// PauseMenu::InputFrom, PauseMenu::Update.
void testPause() {
    const glm::vec2 screen = kWide;
    const glm::vec2 pauseButton = Centre(TouchControl::Pause, screen);
    TouchControls touch;
    PauseMenu pause;
    const auto tick = [&](std::vector<TouchContact> contacts) {
        TouchInput input = Play(std::move(contacts), screen);
        if (pause.Paused()) {
            // As the layer has it: the pause is clicked, and its own rows are the way on.
            input.scene = TouchScene::Menu;
            input.corner = TouchCorner::Hidden;
        }
        InputFrame frame;
        TouchControls::ApplyToFrame(touch.Update(input), frame);
        const PauseStep step = pause.Update(PauseMenu::InputFrom(frame, 1, true, screen));
        return std::make_pair(frame, step);
    };

    tick({});
    auto [frame, step] = tick({Finger(1, pauseButton)});
    CHECK(frame.keys[K_ESC]);
    CHECK(pause.Paused());
    CHECK(step.opened);
    CHECK(!step.tick);
    // Held on, under the pause: the button is gone, so the finger is dead -
    // no second Esc to close it, and no click.
    std::tie(frame, step) = tick({Finger(1, pauseButton)});
    CHECK(!frame.keys[K_ESC]);
    CHECK(!frame.keys[K_LMOUSE]);
    CHECK(pause.Paused());
    CHECK(!touch.Visible(TouchControl::Pause));
    CHECK(!touch.Visible(TouchControl::Dpad));
    std::tie(frame, step) = tick({});
    CHECK(pause.Paused());

    // A tap on Resume: the cursor there and the click, in one tick.
    const PauseMenu::Layout rows = PauseMenu::ComputeLayout(screen);
    const glm::vec2 resume = (rows.rowMin[PauseMenu::kResume] + rows.rowMax[PauseMenu::kResume]) * 0.5f;
    std::tie(frame, step) = tick({Finger(2, resume)});
    CHECK(frame.keys[K_LMOUSE]);
    CHECK(frame.cursor == resume);
    CHECK_EQ(KeysDown(frame), 1);
    CHECK(step.closed);
    CHECK(step.tick);
    CHECK(!step.sendCancel);
    CHECK(!pause.Paused());
    // Back in play with the finger still down: dead, never a key.
    std::tie(frame, step) = tick({Finger(2, resume)});
    CHECK_EQ(KeysDown(frame), 0);
    CHECK(touch.Visible(TouchControl::Dpad));
    tick({});

    // Paused again, and Main menu tapped: the original's cancel for one tick.
    tick({Finger(3, pauseButton)});
    CHECK(pause.Paused());
    tick({});
    const glm::vec2 mainMenu = (rows.rowMin[PauseMenu::kMainMenu] + rows.rowMax[PauseMenu::kMainMenu]) * 0.5f;
    std::tie(frame, step) = tick({Finger(4, mainMenu)});
    CHECK(step.closed);
    CHECK(step.sendCancel);
}

// A platform that makes a finger the mouse as well: on Android the first
// finger of a gesture holds the left button and moves the pointer, on the
// desktop the held mouse is contact 0. While a finger is down the touch
// controls own the left button.
void testPlatformMouse() {
    const auto emulated = [](glm::vec2 at) {
        InputFrame frame;
        frame.keys[K_LMOUSE] = true;
        frame.cursor = frame.cursorAbsolute = at;
        return frame;
    };
    // A finger on the jump button is a jump and not a click.
    TouchControls play;
    const glm::vec2 jump = Centre(TouchControl::Jump);
    InputFrame frame = emulated(jump);
    TouchControls::ApplyToFrame(play.Update(Play({Finger(0, jump)})), frame);
    CHECK(frame.keys[K_CTRL]);
    CHECK(!frame.keys[K_LMOUSE]);
    // The back button is the cancel and not a click under it.
    TouchControls back;
    const glm::vec2 backButton = Centre(TouchControl::Back);
    frame = emulated(backButton);
    TouchControls::ApplyToFrame(back.Update(Menu({Finger(0, backButton)}, TouchCorner::Back)), frame);
    CHECK(frame.keys[K_ESC]);
    CHECK(!frame.keys[K_LMOUSE]);
    // A menu tap is the click it was anyway.
    TouchControls menu;
    frame = emulated(glm::vec2(300.0f, 200.0f));
    TouchControls::ApplyToFrame(menu.Update(Menu({Finger(0, glm::vec2(300.0f, 200.0f))})), frame);
    CHECK(frame.keys[K_LMOUSE]);
    CHECK_EQ(KeysDown(frame), 1);
    // No finger down: the real mouse is left alone, press and cursor.
    TouchControls none;
    frame = emulated(glm::vec2(10.0f, 20.0f));
    TouchControls::ApplyToFrame(none.Update(Menu()), frame);
    CHECK(frame.keys[K_LMOUSE]);
    CHECK(frame.cursor == glm::vec2(10.0f, 20.0f));
    TouchControls::ApplyToFrame(none.Update(Play()), frame);
    CHECK(frame.keys[K_LMOUSE]);

    // The thumb on the disc is the gesture's first finger, so the platform's
    // mouse, all through a pause: the pause opened by a second finger, then
    // Resume tapped by a third, which is a fresh click although the platform's
    // button never let go.
    const glm::vec2 screen = kWide;
    const glm::vec2 thumb = Centre(TouchControl::Dpad, screen) + glm::vec2(100.0f, 0.0f);
    TouchControls touch;
    PauseMenu pause;
    const auto tick = [&](std::vector<TouchContact> contacts) {
        TouchInput input = Play(std::move(contacts), screen);
        if (pause.Paused()) {
            input.scene = TouchScene::Menu;
            input.corner = TouchCorner::Hidden;
        }
        InputFrame f = emulated(thumb);
        TouchControls::ApplyToFrame(touch.Update(input), f);
        const PauseStep step = pause.Update(PauseMenu::InputFrom(f, 1, true, screen));
        return std::make_pair(f, step);
    };
    auto [held, step] = tick({Finger(0, thumb)});
    CHECK(held.keys[K_RIGHT]);
    CHECK(!held.keys[K_LMOUSE]);
    std::tie(held, step) = tick({Finger(0, thumb), Finger(1, Centre(TouchControl::Pause, screen))});
    CHECK(step.opened);
    std::tie(held, step) = tick({Finger(0, thumb), Finger(1, Centre(TouchControl::Pause, screen))});
    CHECK(!held.keys[K_LMOUSE]);   // both fingers dead under the pause
    CHECK(!held.keys[K_RIGHT]);
    CHECK(pause.Paused());
    std::tie(held, step) = tick({Finger(0, thumb)});
    CHECK(pause.Paused());
    const PauseMenu::Layout rows = PauseMenu::ComputeLayout(screen);
    const glm::vec2 resume = (rows.rowMin[PauseMenu::kResume] + rows.rowMax[PauseMenu::kResume]) * 0.5f;
    std::tie(held, step) = tick({Finger(0, thumb), Finger(2, resume)});
    CHECK(held.keys[K_LMOUSE]);
    CHECK(held.cursor == resume);
    CHECK(step.closed);
    CHECK(!pause.Paused());
}

void testMenus() {
    // A tap: the cursor there and the left button, and nothing else.
    TouchControls touch;
    InputState state;
    const glm::vec2 at(300.0f, 200.0f);
    TouchStep step = touch.Update(Menu({Finger(1, at)}));
    CHECK(step.pointer);
    CHECK(step.pointerPos == at);
    for (int i = 0; i < kTouchActionCount; ++i) CHECK(!step.held[static_cast<std::size_t>(i)]);
    InputFrame frame = FrameOf(step);
    CHECK_EQ(KeysDown(frame), 1);
    CHECK(frame.keys[K_LMOUSE]);
    state.Update(frame);
    CHECK(state.GetKeyState(K_LMOUSE) == KS_HIT);   // getConfirmButtonStatus's KS_HIT
    CHECK(state.GetCursorPos() == at);
    // Where the jump button is in play: still the mouse, never K_CTRL.
    TouchControls hidden;
    step = hidden.Update(Menu({Finger(1, Centre(TouchControl::Jump))}));
    CHECK(step.pointer);
    CHECK(!FrameOf(step).keys[K_CTRL]);
    CHECK_EQ(KeysDown(FrameOf(step)), 1);

    // A drag moves the cursor with the button held (one click); a second
    // finger is ignored; the lift lets go.
    const glm::vec2 dragged(340.0f, 260.0f);
    step = touch.Update(Menu({Finger(1, dragged), Finger(2, glm::vec2(800.0f, 600.0f))}));
    CHECK(step.pointer);
    CHECK(step.pointerPos == dragged);
    state.Update(FrameOf(step));
    CHECK(state.GetKeyState(K_LMOUSE) == KS_DOWN);
    CHECK(state.GetCursorPos() == dragged);
    step = touch.Update(Menu({Finger(1, dragged, false), Finger(2, glm::vec2(800.0f, 600.0f))}));
    CHECK(!step.pointer);
    CHECK_EQ(KeysDown(FrameOf(step)), 0);
    step = touch.Update(Menu({Finger(2, glm::vec2(810.0f, 600.0f))}));   // still nobody's
    CHECK(!step.pointer);
    step = touch.Update(Menu({Finger(3, glm::vec2(100.0f, 100.0f))}));
    CHECK(step.pointer);

    // The back button (the arena select, game over): the original's cancel,
    // not a click. Anywhere else is still a click.
    TouchControls back;
    const glm::vec2 backButton = Centre(TouchControl::Back);
    step = back.Update(Menu({Finger(1, backButton)}, TouchCorner::Back));
    CHECK(Only(step, {TouchAction::Cancel}));
    step = back.Update(Menu({Finger(1, backButton), Finger(2, at)}, TouchCorner::Back));
    CHECK(step.Held(TouchAction::Cancel));
    CHECK(step.pointer);
    // In the main menu there is none: a tap there is a click.
    TouchControls mainMenu;
    step = mainMenu.Update(Menu({Finger(1, backButton)}, TouchCorner::Hidden));
    CHECK(step.pointer);
    CHECK(!step.Held(TouchAction::Cancel));

    // On the desktop the held mouse is the finger (SynthesiseMouseContact):
    // the frame the real mouse already made is unchanged by it - no second
    // click, no other cursor.
    InputFrame mouse;
    mouse.keys[K_LMOUSE] = true;
    mouse.cursor = mouse.cursorAbsolute = at;
    TouchControls desktop;
    InputFrame both = mouse;
    TouchControls::ApplyToFrame(desktop.Update(Menu({Finger(0, at)})), both);
    CHECK(both.keys == mouse.keys);
    CHECK(both.cursor == mouse.cursor);
    CHECK(both.cursorAbsolute == mouse.cursorAbsolute);
}

void testShownWhere() {
    TouchControls touch;
    touch.SetImageRoot(PENUMBRA_DATA_DIR);
    std::vector<HudCmd> out;

    // The main menu, and the pause: nothing at all.
    touch.Update(Menu());
    touch.AppendOverlay(out);
    CHECK(out.empty());
    for (int i = 0; i < kTouchControlCount; ++i) CHECK(!touch.Visible(static_cast<TouchControl>(i)));

    // The arena select, game over: the back button alone.
    touch.Update(Menu({}, TouchCorner::Back));
    out.clear();
    touch.AppendOverlay(out);
    CHECK_EQ(out.size(), std::size_t{1});
    CHECK(touch.Visible(TouchControl::Back));
    CHECK(!touch.Visible(TouchControl::Pause));
    CHECK(!touch.Visible(TouchControl::Jump));
    if (!out.empty()) CHECK(out[0].sprite.find("back.png") != std::string::npos);

    // In play: the disc, its two arrows (E25: no down), four buttons, the two
    // combo buttons, pause - and no knob, with no thumb on the disc, and no
    // down button away from the exit.
    const TouchManifest manifest = TouchControls::DefaultManifest();
    const std::uint8_t idle = static_cast<std::uint8_t>(std::lround(manifest.idleAlpha * 255.0f));
    const std::uint8_t pressed = static_cast<std::uint8_t>(std::lround(manifest.pressedAlpha * 255.0f));
    CHECK(pressed > idle);
    CHECK(!manifest.knobAtRest);
    CHECK(manifest.dpadDown.empty());
    touch.Update(Play());
    out.clear();
    touch.AppendOverlay(out);
    CHECK_EQ(out.size(), std::size_t{10});
    CHECK_EQ(Knobs(out), 0);
    CHECK(!touch.Visible(TouchControl::ExitDown));
    for (const HudCmd& cmd : out) CHECK_MSG(cmd.sprite.find("down.png") == std::string::npos, cmd.sprite);
    for (const HudCmd& cmd : out) {
        CHECK(cmd.kind == HudCmd::Kind::ShapedSprite);   // every image found
        CHECK_EQ(static_cast<int>(Alpha(cmd.color)), static_cast<int>(idle));
        CHECK(fs::path(cmd.sprite).is_absolute());
        // Icons only: nothing to translate (strings.json has no entry for it).
        CHECK(cmd.kind != HudCmd::Kind::Text);
    }
    CHECK(touch.Visible(TouchControl::Pause));
    CHECK(!touch.Visible(TouchControl::Back));

    // Held controls brighten: the sword, the right arrow and the knob.
    touch.Update(Play({Finger(1, Centre(TouchControl::Sword)), Finger(2, Dpad({100.0f, 0.0f}))}));
    out.clear();
    touch.AppendOverlay(out);
    int bright = 0;
    for (const HudCmd& cmd : out) {
        if (Alpha(cmd.color) != pressed) continue;
        ++bright;
        const bool expected = cmd.sprite.find("/sword.png") != std::string::npos ||
                              cmd.sprite.find("dpad_right.png") != std::string::npos ||
                              cmd.sprite.find("dpad_knob.png") != std::string::npos;
        CHECK_MSG(expected, cmd.sprite);
    }
    CHECK_EQ(bright, 3);
    // The knob follows the thumb, right of the centre.
    CHECK_EQ(Knobs(out), 1);
    for (const HudCmd& cmd : out) {
        if (cmd.sprite.find("dpad_knob.png") == std::string::npos) continue;
        CHECK((cmd.pos + cmd.size * 0.5f).x > Dpad({0.0f, 0.0f}).x + 10.0f);
    }
    // Left too, on the button held; none for a thumb pointing no direction
    // (the dead zone, straight up, and since E25 straight down), nor once it
    // lifts.
    const auto knobsFor = [&](glm::vec2 offset) {
        touch.Update(Play({Finger(3, Dpad(offset))}));
        std::vector<HudCmd> drawn;
        touch.AppendOverlay(drawn);
        return Knobs(drawn);
    };
    CHECK_EQ(knobsFor({0.0f, 100.0f}), 0);    // straight down: nothing held
    CHECK_EQ(knobsFor({-100.0f, 0.0f}), 1);
    CHECK_EQ(knobsFor({70.0f, -70.0f}), 1);   // up and right: right
    CHECK_EQ(knobsFor({70.0f, 70.0f}), 1);    // down and right: right
    CHECK_EQ(knobsFor({5.0f, 5.0f}), 0);      // the dead zone
    CHECK_EQ(knobsFor({0.0f, -100.0f}), 0);   // straight up: nothing held
    touch.Update(Play());
    out.clear();
    touch.AppendOverlay(out);
    CHECK_EQ(Knobs(out), 0);
    // A combo presses a side with no thumb on the disc: still no knob.
    TouchControls comboing;
    comboing.SetImageRoot(PENUMBRA_DATA_DIR);   // the knob's image found: only the rule keeps it off
    comboing.Update(Play({Finger(4, Centre(TouchControl::SwordCombo))}));
    bool sideHeld = false;
    for (int i = 0; i < 6; ++i) {
        out.clear();
        const TouchStep step = comboing.Update(Play());
        sideHeld = sideHeld || step.Held(TouchAction::Right) || step.Held(TouchAction::Left);
        comboing.AppendOverlay(out);
        CHECK_EQ(Knobs(out), 0);
    }
    CHECK(sideHeld);

    // The placeholder look keeps its knob at rest, at the disc's centre, as
    // it always had it (its manifest says "atRest": true).
    std::string warning;
    const TouchManifest placeholder = TouchControls::LoadManifest(
        fs::path(PENUMBRA_DATA_DIR) / "images" / "touch" / "placeholder" / TouchControls::kManifestFile, &warning);
    CHECK_MSG(warning.empty(), warning);
    CHECK(placeholder.knobAtRest);
    TouchControls old;
    old.SetManifest(placeholder);
    old.SetImageRoot(PENUMBRA_DATA_DIR);
    old.Update(Play());
    out.clear();
    old.AppendOverlay(out);
    CHECK_EQ(out.size(), std::size_t{12});
    CHECK_EQ(Knobs(out), 1);
    const glm::vec2 placeholderCentre =
        TouchControls::ComputeLayout(placeholder, kFourThree, TouchInsets{})[TouchControl::Dpad].Centre();
    for (const HudCmd& cmd : out) {
        if (cmd.sprite.find("dpad_knob.png") == std::string::npos) continue;
        CHECK_NEAR((cmd.pos + cmd.size * 0.5f).x, placeholderCentre.x);
        CHECK_NEAR((cmd.pos + cmd.size * 0.5f).y, placeholderCentre.y);
        CHECK_EQ(static_cast<int>(Alpha(cmd.color)), static_cast<int>(idle));
    }
    old.Update(Play({Finger(1, placeholderCentre + glm::vec2(5.0f, 5.0f))}));   // a thumb resting in the dead zone
    out.clear();
    old.AppendOverlay(out);
    CHECK_EQ(Knobs(out), 1);

    // The end screens: the controls (the wizard still walks) with back, not pause.
    touch.Update([] {
        TouchInput input = Play();
        input.corner = TouchCorner::Back;
        return input;
    }());
    CHECK(touch.Visible(TouchControl::Dpad));
    CHECK(touch.Visible(TouchControl::Back));
    CHECK(!touch.Visible(TouchControl::Pause));

    // Without its images a control is still there, as a plain square.
    TouchControls bare;
    bare.Update(Play());
    out.clear();
    bare.AppendOverlay(out);
    CHECK_EQ(out.size(), std::size_t{8});   // the disc, six buttons, pause; no arrows or knob
    for (const HudCmd& cmd : out) CHECK(cmd.kind == HudCmd::Kind::Rectangle);
}

// E25's down button: not part of the layout, drawn and touched only while the
// next_level door offers the way on (TouchInput::nextLevelOffered), above the
// left and right buttons and centred between them; a tap is K_DOWN alone, as
// the keyboard's - and the door's hold on it ends with the offer.
TouchInput AtTheExit(std::vector<TouchContact> contacts = {}, bool offered = true) {
    TouchInput input = Play(std::move(contacts));
    input.nextLevelOffered = offered;
    return input;
}

void testExitDown() {
    const TouchManifest manifest = TouchControls::DefaultManifest();
    const TouchLayout layout = Default();
    const TouchLayout::Box exit = layout[TouchControl::ExitDown];
    const TouchLayout::Box dpad = layout[TouchControl::Dpad];
    // Where the left and right buttons are drawn: 126 px squares at (-137,
    // -21.8) and (137, -21.8) from the disc's centre (images/touch/README.md).
    const float drawn = dpad.Size().x / 400.0f;
    const glm::vec2 leftButton = dpad.Centre() + glm::vec2(-137.0f, -21.8f) * drawn;
    const glm::vec2 rightButton = dpad.Centre() + glm::vec2(137.0f, -21.8f) * drawn;
    const float half = 63.0f * drawn;
    std::printf("  down button at (%.0f, %.0f)-(%.0f, %.0f); the arrows' tops at y %.0f, centred at x %.0f\n",
                exit.min.x, exit.min.y, exit.max.x, exit.max.y, leftButton.y - half, dpad.Centre().x);
    CHECK(manifest[TouchControl::ExitDown].enabled);
    CHECK(manifest[TouchControl::ExitDown].image == "images/touch/exit_down.png");
    CHECK_NEAR(exit.Centre().x, (leftButton.x + rightButton.x) * 0.5f);   // centred between them
    CHECK(exit.max.y < leftButton.y - half);                              // above them
    CHECK(exit.max.y > leftButton.y - half - 30.0f);                      // and near
    CHECK(exit.min.x > leftButton.x + half && exit.max.x < rightButton.x - half);   // in the gap's column
    CHECK_NEAR(exit.Size().x, 2.0f * half);                                // the arrows' size

    // Away from the exit: not there. A finger where it would be is the disc's
    // (pointing straight up: nothing), never down.
    TouchControls touch;
    touch.SetImageRoot(PENUMBRA_DATA_DIR);
    TouchStep step = touch.Update(Play({Finger(1, exit.Centre())}));
    CHECK(!touch.Visible(TouchControl::ExitDown));
    CHECK(Only(step, {}));
    CHECK(!step.Held(TouchAction::Down));
    touch.Update(Play());

    // At the exit: drawn over the rest, at the idle alpha, from its own image.
    step = touch.Update(AtTheExit());
    CHECK(touch.Visible(TouchControl::ExitDown));
    std::vector<HudCmd> out;
    touch.AppendOverlay(out);
    CHECK_EQ(out.size(), std::size_t{11});
    int downs = 0;
    for (const HudCmd& cmd : out) {
        if (cmd.sprite.find("exit_down.png") == std::string::npos) continue;
        ++downs;
        CHECK(cmd.pos == exit.min && cmd.size == exit.Size());
    }
    CHECK_EQ(downs, 1);

    // Its tap is K_DOWN alone: KS_HIT, held, released.
    InputState state;
    step = touch.Update(AtTheExit({Finger(2, exit.Centre())}));
    CHECK(Only(step, {TouchAction::Down}));
    CHECK(Tick(state, step, K_DOWN) == KS_HIT);
    CHECK(FrameOf(step).keys[K_DOWN] && !FrameOf(step).keys[K_LMOUSE]);
    step = touch.Update(AtTheExit({Finger(2, exit.Centre())}));
    CHECK(Tick(state, step, K_DOWN) == KS_DOWN);
    step = touch.Update(AtTheExit({Finger(2, exit.Centre(), false)}));
    CHECK(Only(step, {}));
    CHECK(Tick(state, step, K_DOWN) == KS_RELEASE);
    // Its corners too (its padding), and the arrows stay the disc's with it shown.
    CHECK(Only(touch.Update(AtTheExit({Finger(3, exit.min + glm::vec2(1.0f))})), {TouchAction::Down}));
    touch.Update(AtTheExit());
    CHECK(Only(touch.Update(AtTheExit({Finger(4, leftButton)})), {TouchAction::Left}));
    touch.Update(AtTheExit());
    CHECK(Only(touch.Update(AtTheExit({Finger(5, rightButton)})), {TouchAction::Right}));
    touch.Update(AtTheExit());
    // Walking onto the door with the thumb on right, down tapped with the
    // other: both, as the keyboard's right and down (getPlayerXYAxis's y > 0).
    step = touch.Update(AtTheExit({Finger(6, rightButton), Finger(7, exit.Centre())}));
    CHECK(Only(step, {TouchAction::Right, TouchAction::Down}));

    // The door's fade starts (the offer ends): the button goes, and the finger
    // on it is dead until it lifts - no more down.
    step = touch.Update(AtTheExit({Finger(6, rightButton), Finger(7, exit.Centre())}, false));
    CHECK(!touch.Visible(TouchControl::ExitDown));
    CHECK(Only(step, {TouchAction::Right}));
    step = touch.Update(AtTheExit({Finger(7, exit.Centre())}, true));   // offered again: still dead
    CHECK(Only(step, {}));
    touch.Update(AtTheExit({}, true));
    CHECK(Only(touch.Update(AtTheExit({Finger(8, exit.Centre())})), {TouchAction::Down}));   // a new tap

    // Not in a menu, nor in the pause, whatever the offer says.
    TouchControls menu;
    TouchInput input = Menu();
    input.nextLevelOffered = true;
    menu.Update(input);
    CHECK(!menu.Visible(TouchControl::ExitDown));

    // A manifest without it ("enabled": false) never shows it: the layout
    // before E25, the disc's down sector back.
    TouchControls before;
    before.SetManifest(DiscWithDown());
    before.Update(AtTheExit());
    CHECK(!before.Visible(TouchControl::ExitDown));
    CHECK(Only(before.Update(AtTheExit({Finger(9, DpadIn(DiscWithDown(), {0.0f, 100.0f}))})), {TouchAction::Down}));
}

// The corner button, screen by screen (TouchControls::CornerFor), for every
// scene the original has, the options screen with its own Back included.
void testCorner() {
    using Penumbra::Render::TouchScreen;
    const auto corner = [](const std::string& scene, bool level, bool finished = false, bool paused = false) {
        return TouchControls::CornerFor(TouchScreen{scene, level, finished, paused});
    };
    // The screens that are not levels, each with its reason.
    CHECK(corner("scenes/menu.esc", false) == TouchCorner::Hidden);          // cancel does nothing there
    CHECK(corner("scenes/videoModes.esc", false) == TouchCorner::Hidden);    // the original's own Back arrow
    CHECK(corner("scenes/arena_select.esc", false) == TouchCorner::Back);    // reads cancel only
    CHECK(corner("scenes/gameover.esc", false) == TouchCorner::Back);        // the same
    CHECK(corner("", false) == TouchCorner::Hidden);                         // before the first scene
    // A level, an arena, a checkpoint's reload: the pause; their end screens
    // (the campaign's, a Versus win): back.
    for (const char* scene : {"scenes/level1.esc", "scenes/checkpoint.esc", "scenes/pvp_lv3.esc"}) {
        CHECK_MSG(corner(scene, true) == TouchCorner::Pause, scene);
        CHECK_MSG(corner(scene, true, true) == TouchCorner::Back, scene);
    }
    // The pause's rows are the way on, wherever it is open.
    CHECK(corner("scenes/level1.esc", true, false, true) == TouchCorner::Hidden);
    CHECK(corner("scenes/level3.esc", true, true, true) == TouchCorner::Hidden);

    // Every scene the original ships, classified: none falls through unseen.
    const fs::path scenes = fs::path(PENUMBRA_ORIGINAL_DIR) / "scenes";
    std::error_code ec;
    if (fs::is_directory(scenes, ec)) {
        int seen = 0;
        for (const auto& entry : fs::directory_iterator(scenes, ec)) {
            if (entry.path().extension() != ".esc") continue;
            const std::string name = entry.path().filename().string();
            const std::string scene = "scenes/" + name;
            ++seen;
            // What doLoop runs them under: levelLoop or pvpLoop (main.as:116,
            // menu.as:333); the others have their own loops.
            const bool level = name.rfind("level", 0) == 0 || name.rfind("pvp_lv", 0) == 0;
            TouchCorner expected = TouchCorner::Pause;
            if (!level) {
                if (name == "menu.esc" || name == "videoModes.esc") expected = TouchCorner::Hidden;
                else if (name == "arena_select.esc" || name == "gameover.esc") expected = TouchCorner::Back;
                else CHECK_MSG(false, "a scene the corner rule was not written for: " + name);
            }
            CHECK_MSG(corner(scene, level) == expected, name);
            CHECK_MSG(corner(scene, level, false, true) == TouchCorner::Hidden, name);
        }
        CHECK_EQ(seen, 13);
    } else {
        std::printf("  (the original is not at %s: its scenes are not enumerated)\n", PENUMBRA_ORIGINAL_DIR);
    }

    // On the options screen nothing is drawn, and a finger on its Back arrow is the   // E31
    // mouse there: a click on it, no cancel. The arrow is at (500,40) in the original   // E31
    // (DrawSprite, videoModes.as:81), at (906,6) where the options art is (E27), and in   // E31
    // the top-left corner of what the window shows on the phone's larger layout (E31);   // E31
    // the finger here is synthetic and the test does not depend on where it lands.   // E31
    TouchControls touch;
    touch.SetImageRoot(PENUMBRA_DATA_DIR);
    const glm::vec2 arrow(540.0f, 70.0f);
    const TouchStep step = touch.Update(Menu({Finger(1, arrow)}, corner("scenes/videoModes.esc", false)));
    std::vector<HudCmd> out;
    touch.AppendOverlay(out);
    CHECK(out.empty());
    for (int i = 0; i < kTouchActionCount; ++i) CHECK(!step.Held(static_cast<TouchAction>(i)));
    CHECK(step.pointer);
    CHECK(step.pointerPos == arrow);
    CHECK(FrameOf(step).keys[K_LMOUSE]);
    CHECK(!FrameOf(step).keys[K_ESC]);
}

void testSceneChanges() {
    // A thumb holding right when a death loads game over: nothing held and no
    // click, then or when play comes back, until it lifts.
    TouchControls touch;
    const glm::vec2 right = Dpad({100.0f, 0.0f});
    TouchStep step = touch.Update(Play({Finger(1, right)}));
    CHECK(Only(step, {TouchAction::Right}));
    step = touch.Update(Menu({Finger(1, right)}, TouchCorner::Back));
    CHECK(Only(step, {}));
    CHECK_EQ(KeysDown(FrameOf(step)), 0);
    step = touch.Update(Play({Finger(1, right)}));
    CHECK(Only(step, {}));
    step = touch.Update(Play());
    step = touch.Update(Play({Finger(1, right)}));
    CHECK(Only(step, {TouchAction::Right}));

    // A finger that was the mouse in a menu does not steer when a level starts
    // under it.
    TouchControls menu;
    step = menu.Update(Menu({Finger(2, Dpad({-100.0f, 0.0f}))}));
    CHECK(step.pointer);
    step = menu.Update(Play({Finger(2, Dpad({-100.0f, 0.0f}))}));
    CHECK(Only(step, {}));
}

void testLatch() {
    const glm::vec2 jump = Centre(TouchControl::Jump);
    // A tap that came and went between two ticks still presses, for one tick.
    TouchControls touch;
    touch.LatchFrame({Finger(5, jump)});
    TouchStep step = touch.Update(Play());
    CHECK(Only(step, {TouchAction::Jump}));
    step = touch.Update(Play());
    CHECK(Only(step, {}));
    // Seen lifting by the tick: held for this one, where it lifted.
    touch.LatchFrame({Finger(6, Centre(TouchControl::Fire))});
    step = touch.Update(Play({Finger(6, Centre(TouchControl::Fire), false)}));
    CHECK(Only(step, {TouchAction::Fire}));
    step = touch.Update(Play());
    CHECK(Only(step, {}));
    // Still down at the tick: read as usual, and held on.
    touch.LatchFrame({Finger(7, jump)});
    step = touch.Update(Play({Finger(7, jump)}));
    CHECK(Only(step, {TouchAction::Jump}));
    step = touch.Update(Play({Finger(7, jump)}));
    CHECK(Only(step, {TouchAction::Jump}));
    // A finger a tick already holds is not latched: its lift is not stretched.
    touch.LatchFrame({Finger(7, jump)});
    step = touch.Update(Play());
    CHECK(Only(step, {}));
    // In a menu, a latched tap is a one-tick click.
    TouchControls menu;
    menu.LatchFrame({Finger(1, glm::vec2(200.0f, 300.0f)), Finger(2, glm::vec2(0.0f), false)});
    step = menu.Update(Menu());
    CHECK(step.pointer);
    CHECK(step.pointerPos == glm::vec2(200.0f, 300.0f));
    step = menu.Update(Menu());
    CHECK(!step.pointer);
}

bool Inside(const TouchLayout::Box& box, const glm::vec2& lo, const glm::vec2& hi) {
    constexpr float kSlack = 0.001f;
    return box.min.x >= lo.x - kSlack && box.min.y >= lo.y - kSlack && box.max.x <= hi.x + kSlack &&
           box.max.y <= hi.y + kSlack;
}

// Inside, but for the overhang past the edges the control hangs from: the
// manifest's, half the control's size at most (TouchControlSpec::overhang).
bool InsideHanging(const TouchLayout::Box& box, const TouchControlSpec& spec, const glm::vec2& lo,
                   const glm::vec2& hi) {
    const glm::vec2 hang = glm::min(spec.overhang, spec.size * 0.5f);
    glm::vec2 from = lo;
    glm::vec2 to = hi;
    if (spec.anchor == TouchAnchor::TopLeft || spec.anchor == TouchAnchor::BottomLeft) from.x -= hang.x;
    else to.x += hang.x;
    if (spec.anchor == TouchAnchor::TopLeft || spec.anchor == TouchAnchor::TopRight) from.y -= hang.y;
    else to.y += hang.y;
    return Inside(box, from, to);
}

bool Overlap(const TouchLayout::Box& a, const TouchLayout::Box& b) {
    return a.min.x < b.max.x && b.min.x < a.max.x && a.min.y < b.max.y && b.min.y < a.max.y;
}

// How far from its centre a finger still lands on a round control.
float Reach(const TouchLayout& layout, TouchControl control) {
    const glm::vec2 size = layout[control].Size();
    return 0.5f * std::min(size.x, size.y) + layout.hitPadding[static_cast<std::size_t>(control)];
}

// Where a finger still lands on a square control: its box and its padding.
TouchLayout::Box Padded(const TouchLayout& layout, TouchControl control) {
    const float pad = layout.hitPadding[static_cast<std::size_t>(control)];
    return {layout[control].min - glm::vec2(pad), layout[control].max + glm::vec2(pad)};
}

// Whether one finger could land on both, each touched by its shape as
// TouchControls::hit touches it: two round ones by their reaches, two square
// ones by their padded boxes, a round one and a square one by the circle's
// distance to the nearest point of the padded box.
bool ReachBoth(const TouchLayout& layout, const TouchManifest& manifest, TouchControl a, TouchControl b) {
    const bool roundA = manifest[a].shape == Penumbra::Render::TouchShape::Circle;
    const bool roundB = manifest[b].shape == Penumbra::Render::TouchShape::Circle;
    if (roundA && roundB) {
        return glm::length(layout[a].Centre() - layout[b].Centre()) <= Reach(layout, a) + Reach(layout, b);
    }
    if (!roundA && !roundB) return Overlap(Padded(layout, a), Padded(layout, b));
    const TouchControl round = roundA ? a : b;
    const TouchLayout::Box box = Padded(layout, roundA ? b : a);
    const glm::vec2 centre = layout[round].Centre();
    return glm::length(centre - glm::clamp(centre, box.min, box.max)) <= Reach(layout, round);
}

// Whether two controls are drawn over each other: as circles when both are
// round (the art's corners are transparent, so square boxes may touch), as
// their boxes otherwise.
bool DrawnOver(const TouchLayout& layout, const TouchManifest& manifest, TouchControl a, TouchControl b) {
    if (manifest[a].shape == Penumbra::Render::TouchShape::Circle &&
        manifest[b].shape == Penumbra::Render::TouchShape::Circle) {
        const float apart = glm::length(layout[a].Centre() - layout[b].Centre());
        return apart < 0.5f * layout[a].Size().x + 0.5f * layout[b].Size().x;
    }
    return Overlap(layout[a], layout[b]);
}

// Where an arrow's button is drawn: the opaque part of its image (the control's
// box is the image), as a rectangle of the logical screen.
TouchLayout::Box ArrowFace(const std::string& image, const TouchLayout::Box& dpad, unsigned minAlpha = 128u) {   // E28: the threshold
    const Penumbra::Render::DecodedImage decoded = Penumbra::Render::DecodeTexture(
        (fs::path(PENUMBRA_DATA_DIR) / image).generic_string(), Penumbra::Render::TextureVariant::Plain);
    CHECK_MSG(decoded.Valid(), image);
    if (!decoded.Valid()) return {};
    glm::ivec2 lo(decoded.width, decoded.height);
    glm::ivec2 hi(0);
    for (int y = 0; y < decoded.height; ++y) {
        for (int x = 0; x < decoded.width; ++x) {
            const std::size_t texel = static_cast<std::size_t>(y) * static_cast<std::size_t>(decoded.width) +
                                      static_cast<std::size_t>(x);
            if (decoded.rgba[texel * 4u + 3u] < minAlpha) continue;   // E28: was 128
            lo = glm::min(lo, glm::ivec2(x, y));
            hi = glm::max(hi, glm::ivec2(x + 1, y + 1));
        }
    }
    const glm::vec2 texel =
        dpad.Size() / glm::vec2(static_cast<float>(decoded.width), static_cast<float>(decoded.height));
    return {dpad.min + glm::vec2(lo) * texel, dpad.min + glm::vec2(hi) * texel};
}

// The shipped manifest, and the first (placeholder) look's, kept beside its art.
fs::path ShippedManifest() { return fs::path(PENUMBRA_DATA_DIR) / TouchControls::kManifestFile; }
fs::path PlaceholderManifest() {
    return fs::path(PENUMBRA_DATA_DIR) / "images" / "touch" / "placeholder" / TouchControls::kManifestFile;
}

// E33: what the shipped arrangement is, on one screen, as relations (its numbers are pinned once, as literals, by
// testButtonColumns and testTuningIdentity): two staggered columns of three, the right one (bottom to top: jump, spell
// combo, fire) near the safe area's right edge, not at it and not far in, and exactly the stagger higher than the left
// one (sword, sword combo, light) at every row; the columns the same width, apart by the gap, and not interleaved; each
// combo centred over its column and halfway between its two buttons, so a column's rows are evenly spaced; the gap, 16,
// between every pair of neighbours (the columns, and a button and a combo in a column); and the top of the cluster, the
// fire, well below the pause button.
void CheckColumns(const TouchLayout& layout, const glm::vec2& screen, const TouchInsets& safe, const std::string& where) {   // E33
    constexpr float kGap = 16.0f;       // between neighbours, E33 (E32 had 67 between the columns and 26 between the rows)
    constexpr float kStagger = 30.0f;   // how much higher the right column is than the left one, at every row
    const auto at = [](float a, float b) { return std::fabs(a - b) <= 0.001f; };   // E29
    const glm::vec2 hi = screen - glm::vec2(safe.right, safe.bottom);   // E29
    const TouchLayout::Box& jump = layout[TouchControl::Jump];   // E29
    const TouchLayout::Box& sword = layout[TouchControl::Sword];   // E29
    const TouchLayout::Box& fire = layout[TouchControl::Fire];   // E29
    const TouchLayout::Box& light = layout[TouchControl::Light];   // E29
    const TouchLayout::Box& swordCombo = layout[TouchControl::SwordCombo];   // E29
    const TouchLayout::Box& spellCombo = layout[TouchControl::SpellCombo];   // E29
    const TouchLayout::Box& pause = layout[TouchControl::Pause];   // E29
    // Each column on one centre line with its combo; the right one in from the edge by 24 to 60: not hugging it (E29 had
    // it 24 from the edge, in the corner) and not far in (E32 had it 117 from it).   // E33
    const float edge = hi.x - jump.max.x;   // E33
    CHECK_MSG(edge >= 24.0f && edge <= 60.0f && at(edge, hi.x - fire.max.x), where + ": the right edge " + std::to_string(edge));   // E33
    CHECK_MSG(at(light.max.x, sword.max.x) && at(light.min.x, sword.min.x), where + ": light over sword");   // E29
    CHECK_MSG(at(fire.max.x, jump.max.x) && at(fire.min.x, jump.min.x), where + ": fire over jump");   // E29
    CHECK_MSG(at(swordCombo.Centre().x, sword.Centre().x), where + ": the sword combo centred over the sword");   // E29
    CHECK_MSG(at(spellCombo.Centre().x, jump.Centre().x), where + ": the spell combo centred over the jump");   // E29
    // The stagger: the right column is higher than the left at each of the three rows, by the same amount (y grows
    // downward, so higher is a smaller y), the buttons and the combos alike.   // E33
    CHECK_MSG(at(sword.min.y - jump.min.y, kStagger) && at(sword.max.y - jump.max.y, kStagger), where + ": the jump " + std::to_string(sword.min.y - jump.min.y) + " above the sword");   // E33
    CHECK_MSG(at(swordCombo.min.y - spellCombo.min.y, kStagger) && at(swordCombo.max.y - spellCombo.max.y, kStagger), where + ": the spell combo " + std::to_string(swordCombo.min.y - spellCombo.min.y) + " above the sword combo");   // E33
    CHECK_MSG(at(light.min.y - fire.min.y, kStagger) && at(light.max.y - fire.max.y, kStagger), where + ": the fire " + std::to_string(light.min.y - fire.min.y) + " above the light");   // E33
    // Evenly spaced rows, each combo halfway between its two buttons.   // E33
    CHECK_MSG(at(sword.Centre().y - swordCombo.Centre().y, swordCombo.Centre().y - light.Centre().y) &&   // E32
                  at(jump.Centre().y - spellCombo.Centre().y, spellCombo.Centre().y - fire.Centre().y),   // E32
              where + ": the rows evenly spaced");   // E32
    CHECK_MSG(at(sword.min.y - swordCombo.max.y, swordCombo.min.y - light.max.y) &&   // E32
                  at(jump.min.y - spellCombo.max.y, spellCombo.min.y - fire.max.y),   // E32
              where + ": each combo halfway between its buttons");   // E32
    // The gap between neighbours: the columns, and in each column a combo and the button under it and the one over it.   // E33
    CHECK_MSG(at(jump.min.x - sword.max.x, kGap), where + ": the gap between the columns " + std::to_string(jump.min.x - sword.max.x));   // E33
    CHECK_MSG(at(sword.min.y - swordCombo.max.y, kGap) && at(swordCombo.min.y - light.max.y, kGap),   // E33
              where + ": the left column's gaps " + std::to_string(sword.min.y - swordCombo.max.y) + ", " + std::to_string(swordCombo.min.y - light.max.y));   // E33
    CHECK_MSG(at(jump.min.y - spellCombo.max.y, kGap) && at(spellCombo.min.y - fire.max.y, kGap),   // E33
              where + ": the right column's gaps " + std::to_string(jump.min.y - spellCombo.max.y) + ", " + std::to_string(spellCombo.min.y - fire.max.y));   // E33
    CHECK_MSG(jump.Centre().x > sword.Centre().x && jump.max.x > sword.max.x, where + ": the jump is right-most");   // E29
    // Bottom to top: left sword, its combo, light; right jump, the spell combo, fire.   // E29
    CHECK_MSG(sword.Centre().y > swordCombo.Centre().y && swordCombo.Centre().y > light.Centre().y, where + ": left order");   // E29
    CHECK_MSG(jump.Centre().y > spellCombo.Centre().y && spellCombo.Centre().y > fire.Centre().y, where + ": right order");   // E29
    // The columns do not interleave: everything of the left one is left of everything of the right one.   // E29
    CHECK_MSG(std::max({sword.max.x, swordCombo.max.x, light.max.x}) < std::min({jump.min.x, spellCombo.min.x, fire.min.x}),   // E29
              where + ": two columns");   // E29
    // The top of the cluster ends well below the pause button: the fire is the highest of the six (the right column is
    // the higher) and the one that is under the pause button, the light the top of the left column.   // E33
    CHECK_MSG(fire.min.y < light.min.y && fire.min.y < spellCombo.min.y, where + ": the fire is the top");   // E33
    CHECK_MSG(fire.min.y - pause.max.y >= 150.0f, where + ": the fire button's clearance of the pause " +   // E29
                                                        std::to_string(fire.min.y - pause.max.y));   // E29
    CHECK_MSG(light.min.y - pause.max.y >= 150.0f, where + ": the light's clearance of the pause " +   // E33
                                                        std::to_string(light.min.y - pause.max.y));   // E33
}   // E29

// A layout fits every screen: inside the safe area, where the thumbs are,
// clear of the HUD, nothing drawn over anything else and no finger reaching
// two controls. E29: `columns` is the shipped arrangement (two columns of three,
// the combos in the middle of them: CheckColumns); the placeholder look keeps   // E29
// E16's, the combos in a row above the four, which is the ordering checked   // E29
// here for it. E32: the shipped look's direction control is two buttons on a mostly empty 400-unit canvas, so   // E32
// the rules that ask where the thumbs are and what a finger reaches are read for it on its two drawn faces   // E32
// (ArrowFace). Under E32's grid, on a 4:3 screen a notch narrows to 848 units, the left column's edge was 304 from the   // E32
// right edge: its box ended where the canvas does and the right button's face was 10 units short of the sword   // E32
// button, and the disc's reach (200 + 30 from its centre) covered the sword button's padded box. E33's left column   // E33
// is 176 in (its box 296 from the edge), 138 units clear of that face on the same screen, and the disc's reach no   // E33
// longer covers it either (a finger on the padded box is 324 from the disc's centre, 230 is its reach); the faces' rules   // E33
// are kept as E32 restated them, since the box is still not where the thumb is.   // E33
void CheckLayoutFits(const TouchManifest& manifest, const std::string& name, bool columns = true) {   // E29
    // 4:3 (the menus, and 4:3 levels), 16:9, a 20:9 phone; bare, a notch and
    // gesture bar in landscape, a tablet's status bar.
    const glm::vec2 screens[] = {kFourThree, kWide, {1707.0f, 768.0f}};
    const TouchInsets insets[] = {{}, {88.0f, 0.0f, 88.0f, 24.0f}, {0.0f, 30.0f, 0.0f, 20.0f}};
    const TouchControl play[] = {TouchControl::Dpad,       TouchControl::Jump,       TouchControl::Sword,
                                 TouchControl::Fire,       TouchControl::Light,      TouchControl::SwordCombo,
                                 TouchControl::SpellCombo, TouchControl::Pause};
    // E32: the shipped look's two drawn buttons (the placeholder's disc is its whole box), read from the art once, as
    // fractions of the direction control's box (as testSizeCeiling reads them), and laid on each screen's box below.
    const TouchLayout::Box unitBox{{0.0f, 0.0f}, {1.0f, 1.0f}};   // E32
    std::vector<std::pair<const char*, TouchLayout::Box>> faceFractions;   // E32
    if (columns) {   // E32
        faceFractions.push_back({"left button", ArrowFace(manifest.dpadLeft, unitBox)});   // E32
        faceFractions.push_back({"right button", ArrowFace(manifest.dpadRight, unitBox)});   // E32
    }   // E32
    for (const glm::vec2& screen : screens) {
        for (const TouchInsets& safe : insets) {
            const std::string where = name + ", " + std::to_string(static_cast<int>(screen.x)) + "x768, inset " +
                                      std::to_string(static_cast<int>(safe.left)) + "/" +
                                      std::to_string(static_cast<int>(safe.top));
            const TouchLayout layout = TouchControls::ComputeLayout(manifest, screen, safe);
            const glm::vec2 lo(safe.left, safe.top);
            const glm::vec2 hi = screen - glm::vec2(safe.right, safe.bottom);
            for (int i = 0; i < kTouchControlCount; ++i) {
                const TouchControl control = static_cast<TouchControl>(i);
                CHECK_MSG(InsideHanging(layout[control], manifest[control], lo, hi),
                          where + ": " + TouchControls::ControlId(control));
                CHECK(layout[control].Size() == manifest[control].size);
            }
            // Where each is: the disc under the left thumb, the buttons under
            // the right, the pause at the top right, below the run's timer
            // (setupScene.as:337, (W-50, 0), 25 px) and clear of the status
            // frames at the top left (interface.as, 226x74 each).
            CHECK_MSG(layout[TouchControl::Dpad].max.x <= screen.x * 0.5f, where);   // the left half
            CHECK_MSG(layout[TouchControl::Dpad].min.y > screen.y * 0.5f, where);
            // E32: the shipped look's two drawn buttons on this screen's direction control (the placeholder's: none).
            std::vector<std::pair<const char*, TouchLayout::Box>> faces;   // E32
            const TouchLayout::Box& dpadBox = layout[TouchControl::Dpad];   // E32
            for (const auto& [which, fraction] : faceFractions) {   // E32
                faces.push_back({which, {dpadBox.min + fraction.min * dpadBox.Size(), dpadBox.min + fraction.max * dpadBox.Size()}});   // E32
            }   // E32
            // E32: under the right thumb is at or past the screen's middle and, in the shipped look, right of the
            // direction control's right button too. The middle was not a strict line under E32: on the notched 4:3 screen
            // the grid was as wide as the direction control's box allowed and the left column's box started exactly on it
            // (936 - 304 - 120 = 512); E33's starts at 936 - 176 - 120 = 640. The right button's face, which is what the
            // left thumb is under, ends well short of the middle (502 there), so it is required as well; the placeholder's
            // disc is its whole box and keeps the strict rule.
            const auto rightOfDirection = [&](const TouchLayout::Box& box) {   // E32
                return columns ? box.min.x >= screen.x * 0.5f && box.min.x > faces.back().second.max.x   // E32
                               : box.min.x > screen.x * 0.5f;   // E32
            };   // E32
            for (const TouchControl button : kButtonControls) {
                CHECK_MSG(rightOfDirection(layout[button]), where + ": " + TouchControls::ControlId(button));
                // E32: the shipped look is in the lower 61% where the placeholder's look is in the lower 60% (E29's top row,
                // 306 + 120 from the bottom, stood 41.4% down a screen with a 24 px bottom bar; E32's, 321 + 120, 39.45%,
                // and 39.97% with a 20 px bar). E33's highest button, the fire, is 316 + 120 from the bottom: 40.1% down
                // with the 24 px bar and 40.6% with 20 px, so this layout does not need the relaxation; it is left as E32
                // made it. Where the thumb reaches is not a line; what keeps the top row from the pause is the clearance
                // CheckColumns and testSizeCeiling pin.
                CHECK_MSG(layout[button].min.y > screen.y * (columns ? 0.39f : 0.4f), where);   // E32
            }
            CHECK_MSG(layout[TouchControl::Pause].min.x > screen.x * 0.5f, where);
            CHECK_MSG(layout[TouchControl::Pause].max.y < screen.y * 0.25f, where);
            // The combo buttons: under the right thumb too, and below the pause.   // E29
            for (const TouchControl combo : {TouchControl::SwordCombo, TouchControl::SpellCombo}) {
                CHECK_MSG(rightOfDirection(layout[combo]), where);   // E32
                CHECK_MSG(layout[combo].min.y > layout[TouchControl::Pause].max.y, where);
            }
            if (columns) {   // E29
                CheckColumns(layout, screen, safe, where);   // E29
            } else {   // E29: E16's row: above the four, each on its attack's side
                for (const TouchControl combo : {TouchControl::SwordCombo, TouchControl::SpellCombo}) {   // E29
                    CHECK_MSG(layout[combo].max.y < layout[TouchControl::Light].min.y, where);   // E29
                }   // E29
                CHECK_MSG(layout[TouchControl::SwordCombo].Centre().x < layout[TouchControl::SpellCombo].Centre().x, where);   // E29
            }   // E29
            CHECK_MSG(!Overlap(layout[TouchControl::Pause], {{screen.x - 50.0f, 0.0f}, {screen.x, 30.0f}}), where);
            CHECK_MSG(!Overlap(layout[TouchControl::Pause], {{0.0f, 0.0f}, {452.0f, 74.0f}}), where);
            CHECK_MSG(!Overlap(layout[TouchControl::Dpad], {{0.0f, 0.0f}, {452.0f, 74.0f}}), where);
            // E25's down button, where a layout has it: over the disc's empty
            // top (its own two buttons are at its sides), clear of everything
            // else, drawn and touched.
            if (manifest[TouchControl::ExitDown].enabled) {
                CHECK_MSG(layout[TouchControl::ExitDown].max.x < screen.x * 0.5f, where);
                for (const TouchControl other : play) {
                    if (other == TouchControl::Dpad) continue;
                    const std::string pair = where + ": exitDown and " + TouchControls::ControlId(other);
                    CHECK_MSG(!DrawnOver(layout, manifest, TouchControl::ExitDown, other), pair);
                    CHECK_MSG(!ReachBoth(layout, manifest, TouchControl::ExitDown, other), pair);
                }
            }
            // Nothing drawn over anything else, and no finger reaching two
            // of them (each by its shape, with its padding).
            for (std::size_t a = 0; a < std::size(play); ++a) {
                for (std::size_t b = a + 1; b < std::size(play); ++b) {
                    const std::string pair = where + ": " + TouchControls::ControlId(play[a]) + " and " +
                                             TouchControls::ControlId(play[b]);
                    CHECK_MSG(!DrawnOver(layout, manifest, play[a], play[b]), pair);
                    if (play[a] == TouchControl::Dpad && columns) {
                        // E32: a finger on a drawn button of the direction control reaches the other control's padded
                        // box or it does not (the box is where the thumb lands on that control).
                        for (const auto& [which, face] : faces) {
                            CHECK_MSG(!Overlap(face, Padded(layout, play[b])), pair + ", its " + which);
                        }
                        continue;
                    }
                    CHECK_MSG(!ReachBoth(layout, manifest, play[a], play[b]), pair);
                }
            }
        }
    }
}

void testLayout() {
    // The built-in layout, the one the game ships, and the placeholder look's.
    const TouchManifest manifest = TouchControls::DefaultManifest();
    CheckLayoutFits(manifest, "built in");
    std::string warning;
    CheckLayoutFits(TouchControls::LoadManifest(ShippedManifest(), &warning), "shipped");
    CHECK_MSG(warning.empty(), warning);
    CheckLayoutFits(TouchControls::LoadManifest(PlaceholderManifest(), &warning), "placeholder", false);   // E29: E16's row
    CHECK_MSG(warning.empty(), warning);

    // The anchor arithmetic, exactly (the built-in numbers, whole or half
    // pixels, so exact in floats): the bottom-left, bottom-right and top-right
    // corners of a notched widescreen's safe area.
    const TouchControlSpec& dpadSpec = manifest[TouchControl::Dpad];
    const TouchControlSpec& jumpSpec = manifest[TouchControl::Jump];
    const TouchControlSpec& lightSpec = manifest[TouchControl::Light];
    const TouchControlSpec& pauseSpec = manifest[TouchControl::Pause];
    const TouchLayout wide = TouchControls::ComputeLayout(manifest, kWide, {88.0f, 0.0f, 88.0f, 24.0f});
    CHECK(wide[TouchControl::Dpad].min ==
          glm::vec2(88.0f + dpadSpec.offset.x, 768.0f - 24.0f - dpadSpec.offset.y - dpadSpec.size.y));
    CHECK(wide[TouchControl::Jump].min == glm::vec2(1366.0f - 88.0f - jumpSpec.offset.x - jumpSpec.size.x,
                                                    768.0f - 24.0f - jumpSpec.offset.y - jumpSpec.size.y));
    CHECK(wide[TouchControl::Pause].min ==
          glm::vec2(1366.0f - 88.0f - pauseSpec.offset.x - pauseSpec.size.x, pauseSpec.offset.y));
    CHECK(wide[TouchControl::Pause].Size() == pauseSpec.size);

    // An offset past the screen is clamped into the safe area; scale grows
    // sizes, offsets and padding together.
    TouchManifest odd = manifest;
    odd[TouchControl::Jump].offset = glm::vec2(5000.0f, 5000.0f);
    odd[TouchControl::Light].anchor = TouchAnchor::TopLeft;
    const TouchInsets notch{60.0f, 10.0f, 60.0f, 10.0f};
    const TouchLayout clamped = TouchControls::ComputeLayout(odd, kFourThree, notch);
    CHECK(clamped[TouchControl::Jump].min == glm::vec2(60.0f, 10.0f));
    CHECK(clamped[TouchControl::Light].min == glm::vec2(60.0f + lightSpec.offset.x, 10.0f + lightSpec.offset.y));
    odd = manifest;
    odd.scale = 1.5f;
    const TouchLayout big = TouchControls::ComputeLayout(odd, kWide, {});
    CHECK(big[TouchControl::Jump].Size() == jumpSpec.size * 1.5f);
    CHECK(big[TouchControl::Jump].max ==
          glm::vec2(1366.0f - jumpSpec.offset.x * 1.5f, 768.0f - jumpSpec.offset.y * 1.5f));
    CHECK_NEAR(big.hitPadding[static_cast<std::size_t>(TouchControl::Jump)], jumpSpec.hitPadding * 1.5f);

    // A platform's safe area (window pixels) into the logical screen.
    View phone;   // a 2400x1080 phone in widescreen: the image is the window
    phone.logicalScreen = glm::vec2(1707.0f, 768.0f);
    phone.windowPixels = glm::uvec2(2400, 1080);
    phone.scale = 1080.0f / 768.0f;
    phone.viewportMin = glm::vec2(0.0f);
    phone.viewportMax = glm::vec2(2400.0f, 1080.0f);
    TouchInsets logical = TouchControls::WindowInsetsToLogical({120.0f, 0.0f, 120.0f, 45.0f}, phone);
    CHECK_NEAR(logical.left, 120.0f / phone.scale);
    CHECK_NEAR(logical.right, 120.0f / phone.scale);
    CHECK_NEAR(logical.bottom, 45.0f / phone.scale);
    CHECK_NEAR(logical.top, 0.0f);
    View boxed;   // the 4:3 menu pillarboxed in a 1366x768 window: the bars take 171 px each side
    boxed.logicalScreen = kFourThree;
    boxed.windowPixels = glm::uvec2(1366, 768);
    boxed.scale = 1.0f;
    boxed.viewportMin = glm::vec2(171.0f, 0.0f);
    boxed.viewportMax = glm::vec2(1195.0f, 768.0f);
    logical = TouchControls::WindowInsetsToLogical({100.0f, 0.0f, 200.0f, 0.0f}, boxed);
    CHECK_NEAR(logical.left, 0.0f);    // inside the bar already
    CHECK_NEAR(logical.right, 29.0f);  // 200 - 171
    CHECK(TouchControls::WindowInsetsToLogical({50.0f, 50.0f, 50.0f, 50.0f}, View{.scale = 0.0f}) == TouchInsets{});

    // A finger on a widescreen screen finds the button where it is drawn.
    TouchControls touch;
    TouchStep step = touch.Update(Play({Finger(1, Centre(TouchControl::Fire, kWide))}, kWide));
    CHECK(Only(step, {TouchAction::Fire}));
    TouchControls inset;
    TouchInput input = Play({Finger(1, wide[TouchControl::Jump].Centre())}, kWide);
    input.safeArea = {88.0f, 0.0f, 88.0f, 24.0f};
    CHECK(Only(inset.Update(input), {TouchAction::Jump}));
}

// The direction control's box hangs below the screen's bottom edge (the lower
// half of its disc is empty), which puts its two buttons 25 under the mean of the action buttons'   // E33
// bottom-row centres (E29 had them level with the sword button); its input is the same, and so are the rules that keep a hand-edited
// manifest on the screen.
void testDpadLowered() {
    const TouchManifest manifest = TouchControls::DefaultManifest();
    const TouchControlSpec& dpad = manifest[TouchControl::Dpad];
    CHECK(dpad.offset == glm::vec2(24.0f, -138.0f));
    CHECK(dpad.overhang == glm::vec2(0.0f, 150.0f));
    CHECK(dpad.size == glm::vec2(400.0f, 400.0f));
    CHECK(dpad.shape == Penumbra::Render::TouchShape::Circle);
    CHECK(manifest[TouchControl::ExitDown].offset == glm::vec2(161.0f, 160.0f));
    CHECK(manifest[TouchControl::ExitDown].size == glm::vec2(126.0f, 126.0f));   // the arrows' size
    // Nothing else hangs.
    for (int i = 1; i < kTouchControlCount; ++i) {
        CHECK_MSG(manifest.controls[static_cast<std::size_t>(i)].overhang == glm::vec2(0.0f),
                  TouchControls::ControlId(static_cast<TouchControl>(i)));
    }
    const auto closeTo = [](float a, float b, float eps) { return std::fabs(a - b) <= eps; };

    // On every screen and safe area: 138 below the safe area's bottom edge, the
    // buttons whole inside it and centred 84 above it (83.7), 25 under the mean of the centres of the bottom row of the action buttons (E33).
    const glm::vec2 screens[] = {kFourThree, kWide, {1707.0f, 768.0f}};
    const TouchInsets insets[] = {{}, {88.0f, 0.0f, 88.0f, 24.0f}, {0.0f, 30.0f, 0.0f, 20.0f}};
    for (const glm::vec2& screen : screens) {
        for (const TouchInsets& safe : insets) {
            const std::string where = std::to_string(static_cast<int>(screen.x)) + "x768, inset " +
                                      std::to_string(static_cast<int>(safe.left)) + "/" +
                                      std::to_string(static_cast<int>(safe.top)) + "/" +
                                      std::to_string(static_cast<int>(safe.bottom));
            const TouchLayout layout = TouchControls::ComputeLayout(manifest, screen, safe);
            const glm::vec2 lo(safe.left, safe.top);
            const glm::vec2 hi = screen - glm::vec2(safe.right, safe.bottom);
            const TouchLayout::Box box = layout[TouchControl::Dpad];
            CHECK_MSG(closeTo(box.max.y, hi.y + 138.0f, 0.01f), where);
            CHECK_MSG(closeTo(box.min.x, lo.x + 24.0f, 0.01f), where);
            const TouchLayout::Box left = ArrowFace(manifest.dpadLeft, box);
            const TouchLayout::Box right = ArrowFace(manifest.dpadRight, box);
            CHECK_MSG(Inside(left, lo, hi) && Inside(right, lo, hi), where + ": the buttons show whole");
            CHECK_MSG(closeTo(left.Centre().y, hi.y - 84.0f, 1.0f), where + ": 84 above the edge");
            CHECK_MSG(closeTo(right.Centre().y, left.Centre().y, 0.01f), where);
            // E33: the bottom row is two centres, the sword button's (34 + 60 above the edge) and the jump button's (64 + 60: the
            // right column is 30 higher); their mean is 109, and the direction buttons' centres are 25 under that.
            const float jumpUp = manifest[TouchControl::Jump].offset.y + 0.5f * manifest[TouchControl::Jump].size.y;   // E33
            const float swordUp = manifest[TouchControl::Sword].offset.y + 0.5f * manifest[TouchControl::Sword].size.y;   // E33
            const float bottomRow = 0.5f * (jumpUp + swordUp);   // E33
            CHECK_MSG(closeTo(hi.y - layout[TouchControl::Jump].Centre().y, jumpUp, 0.001f), where + ": the jump's centre");   // E33
            CHECK_MSG(closeTo(hi.y - layout[TouchControl::Sword].Centre().y, swordUp, 0.001f), where + ": the sword's centre");   // E33
            CHECK_MSG(closeTo(jumpUp - swordUp, 30.0f, 0.001f) && closeTo(bottomRow, 109.0f, 0.001f), where + ": the bottom row's 30 and 109");   // E33
            const float meanCentreY = 0.5f * (layout[TouchControl::Jump].Centre().y + layout[TouchControl::Sword].Centre().y);   // E33
            CHECK_MSG(closeTo(left.Centre().y - meanCentreY, bottomRow - 84.0f, 1.0f), where + ": 25 under the mean of the bottom row");   // E33
            // E25's down button above them: centred in the gap, a little above their tops (the button's
            // own 126 px squares are about 12 px below it, its faint shadow rim 17), touching neither.
            const TouchLayout::Box exit = layout[TouchControl::ExitDown];
            const float gap = left.min.y - exit.max.y;
            CHECK_MSG(closeTo(exit.Centre().x, 0.5f * (left.Centre().x + right.Centre().x), 1.0f), where);
            CHECK_MSG(gap > 14.0f && gap < 26.0f, where + ": the down button's gap " + std::to_string(gap));
            CHECK_MSG(Inside(exit, lo, hi), where);
            const TouchLayout::Box reach = Padded(layout, TouchControl::ExitDown);
            CHECK_MSG(!Overlap(reach, left) && !Overlap(reach, right), where);
        }
    }
    // On a 768-tall screen with no inset, the numbers the manifest's note gives.
    const TouchLayout plain = Default();
    CHECK(closeTo(plain[TouchControl::Dpad].max.y, 906.0f, 0.01f));
    CHECK(closeTo(plain[TouchControl::ExitDown].min.y, 482.0f, 0.01f));
    CHECK(closeTo(plain[TouchControl::ExitDown].max.y, 608.0f, 0.01f));

    // A zoomed level (E25): offsets and overhang scale together, so the same
    // fraction of the screen's height hangs and the buttons keep their height
    // above the window's edge.
    const glm::vec2 zoomed(1138.0f, 512.0f);
    const float unit = 512.0f / 768.0f;
    const TouchLayout small = TouchControls::ComputeLayout(manifest, zoomed, TouchInsets{}, unit);
    CHECK(closeTo(small[TouchControl::Dpad].max.y, zoomed.y + 138.0f * unit, 0.01f));
    CHECK(closeTo(ArrowFace(manifest.dpadLeft, small[TouchControl::Dpad]).Centre().y, zoomed.y - 84.0f * unit, unit));

    // A finger on a lowered button is the disc's, as before: left or right
    // alone, on the screen, down to its bottom row.
    {
        const TouchLayout::Box left = ArrowFace(manifest.dpadLeft, plain[TouchControl::Dpad]);
        const TouchLayout::Box right = ArrowFace(manifest.dpadRight, plain[TouchControl::Dpad]);
        int finger = 0;
        for (const auto& [face, action] :
             std::initializer_list<std::pair<TouchLayout::Box, TouchAction>>{{left, TouchAction::Left},
                                                                             {right, TouchAction::Right}}) {
            const glm::vec2 at = face.Centre();
            CHECK(at.y > 0.0f && at.y < kFourThree.y);
            TouchControls touch;
            CHECK(Only(touch.Update(Play({Finger(++finger, at)})), {action}));
            touch.Update(Play());
            const glm::vec2 low(at.x, kFourThree.y - 2.0f);
            CHECK(Only(touch.Update(Play({Finger(++finger, low)})), {action}));
        }
        TouchControls corner;
        CHECK(Only(corner.Update(Play({Finger(1, glm::vec2(2.0f, kFourThree.y - 2.0f))})), {TouchAction::Left}));
    }

    // The down button, shown, takes a finger on it and in its padding, and the
    // lowered buttons stay the disc's with it shown.
    {
        const TouchLayout::Box exit = plain[TouchControl::ExitDown];
        TouchControls touch;
        touch.Update(AtTheExit());
        CHECK(touch.Visible(TouchControl::ExitDown));
        CHECK(Only(touch.Update(AtTheExit({Finger(1, exit.Centre())})), {TouchAction::Down}));
        touch.Update(AtTheExit());
        const glm::vec2 padded(exit.Centre().x, exit.max.y + 5.0f);   // its padding is 6
        CHECK(Only(touch.Update(AtTheExit({Finger(2, padded)})), {TouchAction::Down}));
        touch.Update(AtTheExit());
        const TouchLayout::Box left = ArrowFace(manifest.dpadLeft, plain[TouchControl::Dpad]);
        CHECK(Only(touch.Update(AtTheExit({Finger(3, left.Centre())})), {TouchAction::Left}));
    }

    // The overhang is the allowance and nothing more. None: the box is held on
    // the screen, as every layout was before it.
    const auto laid = [](const TouchManifest& with) {
        return TouchControls::ComputeLayout(with, kFourThree, TouchInsets{});
    };
    TouchManifest hang = manifest;
    hang[TouchControl::Dpad].overhang = glm::vec2(0.0f);
    CHECK(closeTo(laid(hang)[TouchControl::Dpad].max.y, 768.0f, 0.01f));
    CHECK(closeTo(laid(hang)[TouchControl::Dpad].min.x, 24.0f, 0.01f));
    // 30: that far and no further.
    hang[TouchControl::Dpad].overhang = glm::vec2(0.0f, 30.0f);
    CHECK(closeTo(laid(hang)[TouchControl::Dpad].max.y, 768.0f + 30.0f, 0.01f));
    // Whatever is written, half of the control stays on the screen.
    hang[TouchControl::Dpad].overhang = glm::vec2(0.0f, 1000.0f);
    hang[TouchControl::Dpad].offset = glm::vec2(24.0f, -1000.0f);
    CHECK(closeTo(laid(hang)[TouchControl::Dpad].max.y, 768.0f + 200.0f, 0.01f));
    // Past the left edge it hangs from, by its x overhang; the far edges still hold it.
    hang = manifest;
    hang[TouchControl::Dpad].offset = glm::vec2(-50.0f, -138.0f);
    hang[TouchControl::Dpad].overhang = glm::vec2(40.0f, 150.0f);
    CHECK(closeTo(laid(hang)[TouchControl::Dpad].min.x, -40.0f, 0.01f));
    hang[TouchControl::Dpad].offset = glm::vec2(5000.0f, 5000.0f);
    CHECK(closeTo(laid(hang)[TouchControl::Dpad].min.x, 1024.0f - 400.0f, 0.01f));
    CHECK(closeTo(laid(hang)[TouchControl::Dpad].min.y, 0.0f, 0.01f));
    // The right and the top, for the controls that hang from them.
    hang = manifest;
    hang[TouchControl::Jump].offset = glm::vec2(-50.0f, 24.0f);
    hang[TouchControl::Jump].overhang = glm::vec2(50.0f, 0.0f);
    CHECK(closeTo(laid(hang)[TouchControl::Jump].max.x, 1024.0f + 50.0f, 0.01f));
    hang[TouchControl::Pause].offset = glm::vec2(20.0f, -20.0f);
    hang[TouchControl::Pause].overhang = glm::vec2(0.0f, 20.0f);
    CHECK(closeTo(laid(hang)[TouchControl::Pause].min.y, -20.0f, 0.01f));
    // A negative offset with no overhang is held at the edge, as before.
    hang = manifest;
    hang[TouchControl::Jump].offset = glm::vec2(-50.0f, -50.0f);
    CHECK(laid(hang)[TouchControl::Jump].max == glm::vec2(1024.0f, 768.0f));

    // In the file: an offset may be negative as far as the overhang says, in
    // whichever order the keys are written; past it, or with none, it is
    // reported and left as it was.
    std::string warning;
    TouchManifest parsed =
        TouchControls::ManifestFromJson(R"({"controls": {"jump": {"offset": [5, -3]}}})", &warning);
    CHECK(warning.find("jump.offset is below 0") != std::string::npos);
    CHECK(parsed[TouchControl::Jump].offset == manifest[TouchControl::Jump].offset);
    warning.clear();
    parsed = TouchControls::ManifestFromJson(
        R"({"controls": {"jump": {"offset": [5, -3], "overhang": [0, 10]}}})", &warning);
    CHECK_MSG(warning.empty(), warning);
    CHECK(parsed[TouchControl::Jump].offset == glm::vec2(5.0f, -3.0f));
    CHECK(parsed[TouchControl::Jump].overhang == glm::vec2(0.0f, 10.0f));
    warning.clear();
    parsed = TouchControls::ManifestFromJson(
        R"({"controls": {"jump": {"overhang": [0, 10], "offset": [5, -30]}}})", &warning);
    CHECK(warning.find("jump.offset is below -10") != std::string::npos);
    CHECK(parsed[TouchControl::Jump].offset == manifest[TouchControl::Jump].offset);
    warning.clear();
    parsed = TouchControls::ManifestFromJson(R"({"controls": {"jump": {"overhang": [-1, 0]}}})", &warning);
    CHECK(warning.find("jump.overhang is below 0") != std::string::npos);
    CHECK(parsed[TouchControl::Jump].overhang == glm::vec2(0.0f));
    warning.clear();
    TouchControls::ManifestFromJson(R"({"controls": {"jump": {"overhang": "far"}}})", &warning);
    CHECK(warning.find("jump.overhang is not [x, y]") != std::string::npos);
    // The shipped disc's own allowance covers a file that only moves it.
    warning.clear();
    parsed = TouchControls::ManifestFromJson(R"({"controls": {"dpad": {"offset": [24, -50]}}})", &warning);
    CHECK_MSG(warning.empty(), warning);
    CHECK(parsed[TouchControl::Dpad].offset == glm::vec2(24.0f, -50.0f));
    CHECK(parsed[TouchControl::Dpad].overhang == glm::vec2(0.0f, 150.0f));
    // The placeholder look's disc is wholly on the screen (it has no offset below the edge, so the
    // overhang it inherits from the defaults is not used).
    warning.clear();
    const TouchManifest placeholder = TouchControls::LoadManifest(PlaceholderManifest(), &warning);
    CHECK_MSG(warning.empty(), warning);
    CHECK(placeholder[TouchControl::Dpad].offset.y >= 0.0f);
    CHECK(Inside(TouchControls::ComputeLayout(placeholder, kFourThree, TouchInsets{})[TouchControl::Dpad],
                 glm::vec2(0.0f), kFourThree));
}

// Every image a manifest names is there, decodes, fits its box undistorted,
// has at least the pixels it is drawn at, and has no exact magenta (the HUD
// keys it out).
void CheckManifestArt(const TouchManifest& manifest) {
    const fs::path dataDir = PENUMBRA_DATA_DIR;
    std::vector<std::pair<std::string, glm::vec2>> images;
    for (int i = 0; i < kTouchControlCount; ++i) {
        const TouchControl control = static_cast<TouchControl>(i);
        CHECK_MSG(!manifest[control].image.empty(), TouchControls::ControlId(control));
        images.emplace_back(manifest[control].image, manifest[control].size);
    }
    const glm::vec2 dpadSize = manifest[TouchControl::Dpad].size;
    for (const std::string& arrow : {manifest.dpadLeft, manifest.dpadRight}) {
        CHECK(!arrow.empty());
        images.emplace_back(arrow, dpadSize);
    }
    // E25: a disc with a down sector draws its down arrow; one without has none.
    CHECK(manifest.downSector != manifest.dpadDown.empty());
    if (!manifest.dpadDown.empty()) images.emplace_back(manifest.dpadDown, dpadSize);
    CHECK(!manifest.knobImage.empty());
    CHECK(manifest.knobSize.x > 0.0f && manifest.knobSize.y > 0.0f);
    images.emplace_back(manifest.knobImage, manifest.knobSize);
    for (const auto& [image, size] : images) {
        const fs::path path = dataDir / image;
        CHECK_MSG(fs::is_regular_file(path), path.generic_string());
        const glm::ivec2 pixels = Penumbra::Render::ProbeImageSize(path.generic_string());
        CHECK_MSG(pixels.x >= static_cast<int>(size.x) && pixels.y >= static_cast<int>(size.y), image);
        if (pixels.x > 0 && pixels.y > 0) {
            const float aspect = static_cast<float>(pixels.x) / static_cast<float>(pixels.y);
            CHECK_MSG(std::fabs(aspect - size.x / size.y) < 0.02f * size.x / size.y, image);
        }
        const Penumbra::Render::DecodedImage decoded =
            Penumbra::Render::DecodeTexture(path.generic_string(), Penumbra::Render::TextureVariant::Plain);
        CHECK_MSG(decoded.Valid(), image);
        bool magenta = false;
        bool translucent = false;
        for (std::size_t p = 0; p + 3 < decoded.rgba.size(); p += 4) {
            magenta = magenta || (decoded.rgba[p] == 255 && decoded.rgba[p + 1] == 0 && decoded.rgba[p + 2] == 255);
            translucent = translucent || decoded.rgba[p + 3] < 255;
        }
        CHECK_MSG(!magenta, image);
        CHECK_MSG(translucent, image);   // drawn over the game, never a solid square
    }
}

// The direction control's arrows are drawn where their sectors are (they are
// whole buttons now, Magic Rampage's, each alone in the control's box): a
// finger on any opaque pixel of the left or right one holds that side alone,
// and one on the down one holds down - down alone at its middle; a square
// button's upper corners reach into the diagonals' sectors.
void CheckArrowsInSectors(const TouchManifest& manifest, const std::string& name) {
    const fs::path dataDir = PENUMBRA_DATA_DIR;
    const TouchLayout::Box dpad =
        TouchControls::ComputeLayout(manifest, kFourThree, TouchInsets{})[TouchControl::Dpad];
    const std::pair<std::string, TouchAction> arrows[] = {
        {manifest.dpadLeft, TouchAction::Left},
        {manifest.dpadRight, TouchAction::Right},
        {manifest.dpadDown, TouchAction::Down},
    };
    TouchControls touch;
    touch.SetManifest(manifest);
    int finger = 0;
    for (const auto& [image, action] : arrows) {
        if (image.empty()) continue;   // E25: the shipped disc has no down arrow
        const std::string where = name + ": " + image;
        const Penumbra::Render::DecodedImage decoded = Penumbra::Render::DecodeTexture(
            (dataDir / image).generic_string(), Penumbra::Render::TextureVariant::Plain);
        CHECK_MSG(decoded.Valid(), where);
        if (!decoded.Valid()) continue;
        const glm::vec2 texel =
            dpad.Size() / glm::vec2(static_cast<float>(decoded.width), static_cast<float>(decoded.height));
        int opaque = 0;
        int held = 0;
        int alone = 0;
        glm::vec2 sum(0.0f);
        for (int y = 0; y < decoded.height; ++y) {
            for (int x = 0; x < decoded.width; ++x) {
                const std::size_t texelIndex = static_cast<std::size_t>(y) * static_cast<std::size_t>(decoded.width) +
                                               static_cast<std::size_t>(x);
                if (decoded.rgba[texelIndex * 4u + 3u] != 255) continue;
                // A fresh finger each tick; the one before lifts.
                const glm::vec2 at =
                    dpad.min + (glm::vec2(static_cast<float>(x), static_cast<float>(y)) + 0.5f) * texel;
                const TouchStep step = touch.Update(Play({Finger(++finger, at)}));
                ++opaque;
                held += step.Held(action) ? 1 : 0;
                alone += Only(step, {action}) ? 1 : 0;
                sum += at;
            }
        }
        CHECK_MSG(opaque > 0, where);
        CHECK_MSG(held == opaque, where);
        if (action != TouchAction::Down) CHECK_MSG(alone == opaque, where);
        std::printf("  %s: %d opaque texels, %d hold it, %d hold it alone\n", where.c_str(), opaque, held, alone);
        if (opaque > 0) {
            CHECK_MSG(Only(touch.Update(Play({Finger(++finger, sum / static_cast<float>(opaque))})), {action}), where);
        }
    }
}

// Step 25 (E1's wide menus): the arena select and game over in a 20:9 phone's
// 2400x1080, the 4:3 screen centred with the world past its sides. Their Back
// button hangs from the window's corner, as a level's does - not from the 4:3
// box's - and a finger anywhere else, the sides included, is the mouse there.
void testWideMenuCorner() {
    View open;   // what CameraRig::ComputeView makes of a 1024x768 menu with its side margin
    open.logicalScreen = kFourThree;
    open.windowPixels = glm::uvec2(2400, 1080);
    open.scale = 1080.0f / 768.0f;
    open.viewportMin = glm::vec2(480.0f, 0.0f);   // (2400 - 1440) / 2
    open.viewportMax = glm::vec2(1920.0f, 1080.0f);
    open.openSides = 1024.0f;
    const float margin = 480.0f / open.scale;   // 341.33 logical
    const glm::vec2 areaMin = open.ShownLogicalMin();
    const glm::vec2 areaMax = open.ShownLogicalMax();
    CHECK_NEAR(areaMin.x, -margin);
    CHECK_NEAR(areaMax.x, 1024.0f + margin);

    // A level in the same window (E1: 768 x 20/9 wide, the image is the window).
    const glm::vec2 levelScreen(2400.0f * 768.0f / 1080.0f, 768.0f);
    const TouchManifest manifest = TouchControls::DefaultManifest();
    const TouchLayout level = TouchControls::ComputeLayout(manifest, levelScreen, TouchInsets{});
    const TouchLayout wide = TouchControls::ComputeLayout(manifest, areaMin, areaMax, TouchInsets{});
    // Every control the same distance from the window's edges as in the level.
    for (int i = 0; i < kTouchControlCount; ++i) {
        const TouchControl control = static_cast<TouchControl>(i);
        const glm::vec2 shifted = wide[control].min - areaMin;
        CHECK_MSG(std::fabs(shifted.x - level[control].min.x) < 1e-3f && std::fabs(shifted.y - level[control].min.y) < 1e-3f,
                  TouchControls::ControlId(control));
        CHECK_MSG(wide[control].Size() == level[control].Size(), TouchControls::ControlId(control));
    }
    // Back is past the 4:3 box's right edge, in the window's top-right corner.
    CHECK(wide[TouchControl::Back].min.x > 1024.0f);
    CHECK(wide[TouchControl::Back].max.x <= areaMax.x + 1e-3f);

    // Through Update, as the layer hands it over: the corner drawn there, and
    // a tap on it cancels.
    TouchControls touch;
    touch.SetImageRoot(PENUMBRA_DATA_DIR);
    TouchInput input = Menu({}, TouchCorner::Back);
    input.areaMin = areaMin;
    input.areaMax = areaMax;
    touch.Update(input);
    CHECK(touch.Visible(TouchControl::Back));
    CHECK(touch.Layout()[TouchControl::Back].min == wide[TouchControl::Back].min);
    const glm::vec2 back = wide[TouchControl::Back].Centre();
    input.contacts = {Finger(1, back)};
    TouchStep step = touch.Update(input);
    CHECK(step.Held(TouchAction::Cancel));
    CHECK(!step.pointer);
    CHECK(FrameOf(step).keys[K_ESC]);
    input.contacts = {Finger(1, back, false)};
    touch.Update(input);
    // A finger in the left side, past the 4:3 box: the mouse, at that point.
    input.contacts = {Finger(2, glm::vec2(-200.0f, 400.0f))};
    step = touch.Update(input);
    CHECK(step.pointer);
    CHECK(step.pointerPos == glm::vec2(-200.0f, 400.0f));
    CHECK(!step.Held(TouchAction::Cancel));
    input.contacts = {Finger(2, glm::vec2(-200.0f, 400.0f), false)};
    touch.Update(input);
    // No area given (a level, a barred menu): the logical screen, as before.
    TouchControls plain;
    plain.Update(Menu({}, TouchCorner::Back));
    CHECK(plain.Layout()[TouchControl::Back].min == Default()[TouchControl::Back].min);

    // A phone's cutout: measured from the window's edges, which the open sides
    // reach - no bar keeps it clear any more.
    TouchInsets logical = TouchControls::WindowInsetsToLogical({120.0f, 0.0f, 120.0f, 45.0f}, open);
    CHECK_NEAR(logical.left, 120.0f / open.scale);
    CHECK_NEAR(logical.right, 120.0f / open.scale);
    CHECK_NEAR(logical.bottom, 45.0f / open.scale);
    const TouchLayout notched = TouchControls::ComputeLayout(manifest, areaMin, areaMax, logical);
    CHECK(notched[TouchControl::Back].max.x <= areaMax.x - logical.right + 1e-3f);
    CHECK(std::fabs(notched[TouchControl::Back].max.x - (wide[TouchControl::Back].max.x - logical.right)) < 1e-3f);
    // Barred (4:3 or widescreen off): the bars keep the cutout clear, as before.
    View barred = open;
    barred.openSides = 0.0f;
    logical = TouchControls::WindowInsetsToLogical({120.0f, 0.0f, 120.0f, 45.0f}, barred);
    CHECK_NEAR(logical.left, 0.0f);
    CHECK_NEAR(logical.right, 0.0f);
}

void testManifest() {
    const fs::path dataDir = PENUMBRA_DATA_DIR;
    const fs::path file = ShippedManifest();
    CHECK_MSG(fs::exists(file), file.generic_string());
    std::string warning;
    const TouchManifest manifest = TouchControls::LoadManifest(file, &warning);
    CHECK_MSG(warning.empty(), warning);
    // The built-in layout is the shipped file's, field for field: a manifest
    // that goes missing changes nothing.
    CHECK(manifest == TouchControls::DefaultManifest());
    CHECK(!manifest.knobAtRest);

    const char* const ids[] = {"dpad",       "jump",       "sword",    "fire",  "light",
                               "swordCombo", "spellCombo", "exitDown", "pause", "back"};
    CHECK_EQ(std::size(ids), static_cast<std::size_t>(kTouchControlCount));
    for (int i = 0; i < kTouchControlCount; ++i) {
        CHECK(std::string(TouchControls::ControlId(static_cast<TouchControl>(i))) == ids[i]);
    }

    // The art the game ships (Magic Rampage's), and the placeholder look kept
    // one manifest away, with its own manifest beside it.
    CheckManifestArt(manifest);
    CheckArrowsInSectors(manifest, "shipped");
    // E25: the shipped disc's down arrow, still in the folder for a manifest
    // that asks for the disc's down sector back, is where that sector is.
    CheckManifestArt(DiscWithDown());
    CheckArrowsInSectors(DiscWithDown(), "shipped, with E16's down sector");
    CHECK_MSG(fs::exists(PlaceholderManifest()), PlaceholderManifest().generic_string());
    const TouchManifest placeholder = TouchControls::LoadManifest(PlaceholderManifest(), &warning);
    CHECK_MSG(warning.empty(), warning);
    CheckManifestArt(placeholder);
    CheckArrowsInSectors(placeholder, "placeholder");
    CHECK(placeholder[TouchControl::Jump].image.find("images/touch/placeholder/") == 0);
    // The placeholder look is older than E25: its disc has down, and no down button.
    CHECK(placeholder.downSector);
    CHECK(!placeholder[TouchControl::ExitDown].enabled);
    // E25's own fields, read and reported.
    warning.clear();
    CHECK(TouchControls::ManifestFromJson(R"({"controls": {"dpad": {"downSector": true}}})", &warning).downSector);
    CHECK(warning.empty());
    CHECK(!TouchControls::ManifestFromJson(R"({"controls": {"dpad": {"downSector": 1}}})", &warning).downSector);
    CHECK(warning.find("dpad.downSector") != std::string::npos);
    warning.clear();
    CHECK(!TouchControls::ManifestFromJson(R"({"controls": {"exitDown": {"enabled": false}}})", &warning)
               [TouchControl::ExitDown]
               .enabled);
    CHECK(warning.empty());

    // Loaded the way the HUD loads its images: through TextureCache, by the
    // absolute path, one quad each.
    entt::registry registry;
    Penumbra::Render::TextureCache textures(PENUMBRA_ORIGINAL_DIR);
    Penumbra::Render::FontAtlas fonts;
    Penumbra::Render::Localization loc;
    HudRenderer hud;
    hud.Attach(registry, textures, fonts, loc);
    TouchControls touch;
    touch.SetManifest(manifest);
    touch.SetImageRoot(dataDir);
    touch.Update(Play({}, kWide));
    std::vector<HudCmd> extra;
    touch.AppendOverlay(extra);
    View view;
    view.logicalScreen = kWide;
    view.windowPixels = glm::uvec2(1366, 768);
    view.scale = 1.0f;
    view.viewportMin = glm::vec2(0.0f);
    view.viewportMax = kWide;
    RenderSnapshot snapshot;
    std::vector<Supersonic::ScreenOverlay::Quad> quads;
    hud.Build(snapshot, view, quads, &extra);
    CHECK_EQ(quads.size(), extra.size());   // no bars in the widescreen view
    for (std::size_t i = 0; i < quads.size() && i < extra.size(); ++i) {
        CHECK_MSG(!quads[i].texture.empty(), extra[i].sprite);
        CHECK_NEAR(quads[i].min.x, extra[i].pos.x / 1366.0f);
        CHECK_NEAR(quads[i].max.y, (extra[i].pos.y + extra[i].size.y) / 768.0f);
    }

    // Read tolerantly: an empty file is the defaults; what is wrong is
    // reported and left at its default; notes ('_') are not reported.
    warning.clear();
    CHECK(TouchControls::ManifestFromJson("{}", &warning) == TouchControls::DefaultManifest());
    CHECK(warning.empty());
    warning.clear();
    CHECK(TouchControls::ManifestFromJson("{ \"controls\": ", &warning) == TouchControls::DefaultManifest());
    CHECK(!warning.empty());
    warning.clear();
    const TouchManifest partial = TouchControls::ManifestFromJson(
        R"({"_about": "x", "idleAlpha": 0.3, "scale": 9,
            "controls": {"_note": 1,
                         "jump": {"image": "art/a.png", "anchor": "topLeft", "offset": [5, 6], "size": [70, 80],
                                  "shape": "rect", "hitPadding": 3},
                         "sword": {"anchor": "middle", "offset": "far", "size": [0, 10]},
                         "dpad": {"deadZone": 0.4, "arrows": {"left": "l.png"}, "knob": {"size": [50, 50]}},
                         "kick": {}}})",
        &warning);
    CHECK_NEAR(partial.idleAlpha, 0.3f);
    CHECK_NEAR(partial.scale, 4.0f);   // clamped
    CHECK(partial[TouchControl::Jump].image == "art/a.png");
    CHECK(partial[TouchControl::Jump].anchor == TouchAnchor::TopLeft);
    CHECK(partial[TouchControl::Jump].offset == glm::vec2(5.0f, 6.0f));
    CHECK(partial[TouchControl::Jump].size == glm::vec2(70.0f, 80.0f));
    CHECK(partial[TouchControl::Jump].shape == Penumbra::Render::TouchShape::Rect);
    CHECK_NEAR(partial[TouchControl::Jump].hitPadding, 3.0f);
    CHECK(partial[TouchControl::Sword] == TouchControls::DefaultManifest()[TouchControl::Sword]);
    CHECK_NEAR(partial.deadZone, 0.4f);
    CHECK(partial.dpadLeft == "l.png");
    CHECK(partial.dpadRight == TouchControls::DefaultManifest().dpadRight);
    CHECK(partial.knobSize == glm::vec2(50.0f, 50.0f));
    CHECK(partial.knobAtRest == TouchControls::DefaultManifest().knobAtRest);
    CHECK(warning.find("sword.anchor") != std::string::npos);
    CHECK(warning.find("sword.offset") != std::string::npos);
    CHECK(warning.find("sword.size") != std::string::npos);
    CHECK(warning.find("kick") != std::string::npos);
    CHECK(warning.find("_note") == std::string::npos);
    CHECK(warning.find("_about") == std::string::npos);
    warning.clear();
    CHECK(TouchControls::ManifestFromJson(R"({"controls": {"dpad": {"knob": {"atRest": true}}}})", &warning)
              .knobAtRest);
    CHECK(warning.empty());
    CHECK(!TouchControls::ManifestFromJson(R"({"controls": {"dpad": {"knob": {"atRest": "yes"}}}})", &warning)
               .knobAtRest);
    CHECK(warning.find("knob.atRest") != std::string::npos);
    warning.clear();
    CHECK(TouchControls::LoadManifest(dataDir / "no_such_manifest.json", &warning) == TouchControls::DefaultManifest());
    CHECK(!warning.empty());

    // A rect-shaped control is touched as a rectangle, corners included; a
    // round one is not touched at its box's corner. (The built-in jump is
    // square, Magic Rampage's; each shape is set here.)
    TouchManifest square = TouchControls::DefaultManifest();
    square[TouchControl::Jump].shape = Penumbra::Render::TouchShape::Rect;
    square[TouchControl::Jump].hitPadding = 0.0f;
    TouchControls rect;
    rect.SetManifest(square);
    const TouchLayout::Box box = Default()[TouchControl::Jump];
    CHECK(Only(rect.Update(Play({Finger(1, box.min + glm::vec2(2.0f))})), {TouchAction::Jump}));
    TouchManifest disc = square;
    disc[TouchControl::Jump].shape = Penumbra::Render::TouchShape::Circle;
    TouchControls round;
    round.SetManifest(disc);
    CHECK(Only(round.Update(Play({Finger(1, box.min + glm::vec2(2.0f))})), {}));
}

void testSetting() {
    const Settings defaults = Settings::Defaults("en");
    CHECK(defaults.touchControls == "auto");
    CHECK(Settings::Defaults("pt").touchControls == "auto");

    // "auto" follows the build: off on this desktop build, on where
    // PENUMBRA_MOBILE is defined.
    CHECK(TouchControls::EnabledBySetting("on"));
    CHECK(TouchControls::EnabledBySetting("ON"));
    CHECK(!TouchControls::EnabledBySetting("off"));
    CHECK(TouchControls::EnabledBySetting("auto") == Penumbra::Render::kMobileBuild);
    CHECK(TouchControls::EnabledBySetting("") == Penumbra::Render::kMobileBuild);

    for (const char* mode : {"on", "off", "auto"}) {
        Settings changed = defaults;
        changed.touchControls = mode;
        const std::string json = changed.ToJson();
        CHECK(json.find(std::string("\"touchControls\": \"") + mode + "\"") != std::string::npos);
        std::string warning;
        const Settings back = Settings::FromJson(json, defaults, &warning);
        CHECK(back == changed);
        CHECK(warning.empty());
    }
    std::string warning;
    CHECK(Settings::FromJson(R"({"touchControls": "ON"})", defaults, &warning).touchControls == "on");
    CHECK(Settings::FromJson(R"({"touchControls": true})", defaults, &warning).touchControls == "on");
    CHECK(Settings::FromJson(R"({"touchControls": false})", defaults, &warning).touchControls == "off");
    CHECK(warning.empty());
    // An older file without it: the default. Nonsense: the default, and a warning.
    CHECK(Settings::FromJson(R"({"language": "en"})", defaults).touchControls == "auto");
    CHECK(Settings::FromJson(R"({"touchControls": "sometimes"})", defaults, &warning).touchControls == "auto");
    CHECK(!warning.empty());
}

// --- The combo buttons ----------------------------------------------------------------

TouchInput Facing(std::vector<TouchContact> contacts, TouchFacing facing, glm::vec2 screen = kFourThree) {
    TouchInput input = Play(std::move(contacts), screen);
    input.facing = facing;
    return input;
}

std::string TickName(const char* what, std::size_t tick) { return std::string(what) + ", tick " + std::to_string(tick); }

// Each macro's keys, tick by tick from the tap, for both facings: one key a
// tick, a release only between the two presses of the same side, each press
// a fresh KS_HIT; then the fingers' own keys again.
void testComboTimelines() {
    const glm::vec2 swordButton = Centre(TouchControl::SwordCombo);
    const glm::vec2 spellButton = Centre(TouchControl::SpellCombo);
    struct Side {
        TouchFacing facing;
        TouchAction action;
        KEY key;
    };
    for (const Side side : {Side{TouchFacing::Right, TouchAction::Right, K_RIGHT},
                            Side{TouchFacing::Left, TouchAction::Left, K_LEFT}}) {
        // The sword combo: -, side, -, side, sword (CMD side, side, SWORD).
        const std::vector<std::vector<TouchAction>> sword = {
            {}, {side.action}, {}, {side.action}, {TouchAction::Sword}, {}};
        const KEY_STATE sideStates[] = {KS_UP, KS_HIT, KS_RELEASE, KS_HIT, KS_RELEASE, KS_UP};
        const KEY_STATE swordStates[] = {KS_UP, KS_UP, KS_UP, KS_UP, KS_HIT, KS_RELEASE};
        TouchControls touch;
        InputState state;
        for (std::size_t t = 0; t < sword.size(); ++t) {
            // A tap: down on the first tick, up on the next.
            std::vector<TouchContact> fingers;
            if (t == 0) fingers.push_back(Finger(1, swordButton));
            const TouchStep step = touch.Update(Facing(fingers, side.facing));
            CHECK_MSG(Only(step, sword[t]), TickName("sword combo", t));
            CHECK(step.combo == (t < 5 ? TouchCombo::Sword : TouchCombo::None));
            state.Update(FrameOf(step));
            CHECK_MSG(state.GetKeyState(side.key) == sideStates[t], TickName("sword combo's side", t));
            CHECK_MSG(state.GetKeyState(K_S) == swordStates[t], TickName("sword combo's K_S", t));
        }
        CHECK(touch.RunningCombo() == TouchCombo::None);

        // The spell combo: -, down, side, fire (CMD DOWN, side, SPELL).
        const std::vector<std::vector<TouchAction>> spell = {
            {}, {TouchAction::Down}, {side.action}, {TouchAction::Fire}, {}};
        const KEY_STATE downStates[] = {KS_UP, KS_HIT, KS_RELEASE, KS_UP, KS_UP};
        const KEY_STATE spellSide[] = {KS_UP, KS_UP, KS_HIT, KS_RELEASE, KS_UP};
        const KEY_STATE fireStates[] = {KS_UP, KS_UP, KS_UP, KS_HIT, KS_RELEASE};
        TouchControls cast;
        InputState castState;
        for (std::size_t t = 0; t < spell.size(); ++t) {
            std::vector<TouchContact> fingers;
            if (t == 0) fingers.push_back(Finger(1, spellButton));
            const TouchStep step = cast.Update(Facing(fingers, side.facing));
            CHECK_MSG(Only(step, spell[t]), TickName("spell combo", t));
            CHECK(step.combo == (t < 4 ? TouchCombo::Spell : TouchCombo::None));
            castState.Update(FrameOf(step));
            CHECK_MSG(castState.GetKeyState(K_DOWN) == downStates[t], TickName("spell combo's K_DOWN", t));
            CHECK_MSG(castState.GetKeyState(side.key) == spellSide[t], TickName("spell combo's side", t));
            CHECK_MSG(castState.GetKeyState(K_D) == fireStates[t], TickName("spell combo's K_D", t));
        }
    }

    // No wizard to ask: where the disc last pointed; never pushed, right (a
    // currentDir never written reads RIGHT).
    TouchControls unknown;
    unknown.Update(Facing({Finger(1, swordButton)}, TouchFacing::Unknown));
    CHECK(Only(unknown.Update(Facing({}, TouchFacing::Unknown)), {TouchAction::Right}));
    for (int i = 0; i < 4; ++i) unknown.Update(Facing({}, TouchFacing::Unknown));
    unknown.Update(Facing({Finger(2, Dpad({-100.0f, 0.0f}))}, TouchFacing::Unknown));
    unknown.Update(Facing({}, TouchFacing::Unknown));
    unknown.Update(Facing({Finger(3, spellButton)}, TouchFacing::Unknown));
    unknown.Update(Facing({}, TouchFacing::Unknown));   // down
    CHECK(Only(unknown.Update(Facing({}, TouchFacing::Unknown)), {TouchAction::Left}));
    // His own facing wins over the disc's last push.
    TouchControls own;
    own.Update(Facing({Finger(1, Dpad({-100.0f, 0.0f}))}, TouchFacing::Right));
    own.Update(Facing({Finger(2, swordButton)}, TouchFacing::Right));
    CHECK(Only(own.Update(Facing({}, TouchFacing::Right)), {TouchAction::Right}));
}

// What the fingers do while a combo runs: the disc and the sword and fire
// buttons are held back (their presses would land in the combo buffer), jump
// and light are not; the disc steers again as soon as the combo is over, a
// sword finger held through it only once lifted. (No ObserveFrame here: the
// combo presses at once, as after a quiet spell.)
void testComboFingers() {
    const glm::vec2 right = Dpad({100.0f, 0.0f});
    const glm::vec2 sword = Centre(TouchControl::Sword);
    const glm::vec2 combo = Centre(TouchControl::SwordCombo);
    const std::vector<std::vector<TouchContact>> ticks = {
        {Finger(1, right)},                                                          // 0 walking
        {Finger(1, right), Finger(2, sword)},                                        // 1 a swing
        {Finger(1, right), Finger(2, sword)},                                        // 2
        {Finger(1, right), Finger(2, sword), Finger(3, combo)},                      // 3 the combo tapped
        {Finger(1, right), Finger(2, sword)},                                        // 4
        {Finger(1, right), Finger(2, sword), Finger(4, Centre(TouchControl::Jump))},   // 5 a jump in it
        {Finger(1, right), Finger(2, sword), Finger(5, Centre(TouchControl::Light))},  // 6 the light in it
        {Finger(1, right), Finger(2, sword)},                                        // 7 the combo's sword
        {Finger(1, right), Finger(2, sword)},                                        // 8 over
        {Finger(1, right)},                                                          // 9 the sword finger lifts
        {Finger(1, right), Finger(6, sword)},                                        // 10 a new swing
    };
    const std::vector<std::vector<TouchAction>> held = {
        {TouchAction::Right},
        {TouchAction::Right, TouchAction::Sword},
        {TouchAction::Right, TouchAction::Sword},
        {},
        {TouchAction::Right},
        {TouchAction::Jump},
        {TouchAction::Right, TouchAction::Light},
        {TouchAction::Sword},
        {TouchAction::Right},
        {TouchAction::Right},
        {TouchAction::Right, TouchAction::Sword},
    };
    TouchControls touch;
    InputState state;
    for (std::size_t t = 0; t < ticks.size(); ++t) {
        const TouchStep step = touch.Update(Facing(ticks[t], TouchFacing::Right));
        CHECK_MSG(Only(step, held[t]), TickName("fingers under a combo", t));
        state.Update(FrameOf(step));
        if (t == 4 || t == 6) CHECK_MSG(state.GetKeyState(K_RIGHT) == KS_HIT, TickName("the combo's side", t));
        if (t == 7) CHECK(state.GetKeyState(K_S) == KS_HIT);
        if (t == 8) CHECK(state.GetKeyState(K_RIGHT) == KS_HIT);   // the disc again
        if (t == 10) CHECK(state.GetKeyState(K_S) == KS_HIT);      // and the sword, from a new finger
    }
}

// A second tap on either combo button while one runs is ignored, not queued;
// a finger left on the button does nothing more; a tap after the end starts
// the next.
void testComboTaps() {
    const glm::vec2 sword = Centre(TouchControl::SwordCombo);
    const glm::vec2 spell = Centre(TouchControl::SpellCombo);
    const std::vector<std::vector<TouchContact>> ticks = {
        {Finger(1, sword)},
        {},
        {Finger(2, sword)},                    // again
        {Finger(3, spell)},                    // the other one
        {Finger(3, spell)},
        {Finger(3, spell)},                    // still on it after the end
        {},
        {Finger(4, spell)},                    // a new tap
        {},
        {},
        {},
    };
    const std::vector<std::vector<TouchAction>> held = {
        {}, {TouchAction::Right}, {}, {TouchAction::Right}, {TouchAction::Sword}, {}, {},
        {}, {TouchAction::Down}, {TouchAction::Right}, {TouchAction::Fire},
    };
    TouchControls touch;
    for (std::size_t t = 0; t < ticks.size(); ++t) {
        const TouchStep step = touch.Update(Facing(ticks[t], TouchFacing::Right));
        CHECK_MSG(Only(step, held[t]), TickName("taps", t));
    }
    CHECK(touch.RunningCombo() == TouchCombo::None);
}

// Leaving play, a load or CancelCombo stops a combo: its keys are simply not
// pressed any more, and none is left down.
void testComboCancel() {
    const glm::vec2 sword = Centre(TouchControl::SwordCombo, kWide);

    // The pause: opened by the corner button during a combo, closed by Resume.
    TouchControls touch;
    PauseMenu pause;
    InputState state;
    const auto tick = [&](std::vector<TouchContact> contacts) {
        TouchInput input = Facing(std::move(contacts), TouchFacing::Right, kWide);
        if (pause.Paused()) {
            input.scene = TouchScene::Menu;
            input.corner = TouchCorner::Hidden;
        }
        const TouchStep step = touch.Update(input);
        const InputFrame frame = FrameOf(step);
        const PauseStep paused = pause.Update(PauseMenu::InputFrom(frame, 1, true, kWide));
        if (paused.tick) state.Update(frame);   // the game reads only the ticks it runs
        return step;
    };
    tick({});
    tick({Finger(1, sword)});
    TouchStep step = tick({});
    CHECK(Only(step, {TouchAction::Right}));
    CHECK(state.GetKeyState(K_RIGHT) == KS_HIT);
    step = tick({Finger(2, Centre(TouchControl::Pause, kWide))});
    CHECK(pause.Paused());
    CHECK(step.Held(TouchAction::Cancel));
    step = tick({});
    CHECK(touch.RunningCombo() == TouchCombo::None);
    CHECK(Only(step, {}));
    const PauseMenu::Layout rows = PauseMenu::ComputeLayout(kWide);
    const glm::vec2 resume = (rows.rowMin[PauseMenu::kResume] + rows.rowMax[PauseMenu::kResume]) * 0.5f;
    tick({Finger(3, resume)});
    CHECK(!pause.Paused());
    for (int i = 0; i < 6; ++i) {
        step = tick({});
        CHECK(Only(step, {}));
    }
    for (const KEY key : {K_LEFT, K_RIGHT, K_DOWN, K_S, K_D}) CHECK(!state.KeyDown(key));

    // A load (a death's reload, the next level): the serial moves on.
    TouchControls load;
    TouchInput input = Facing({Finger(1, Centre(TouchControl::SwordCombo))}, TouchFacing::Right);
    input.sceneSerial = 7;
    load.Update(input);
    input.contacts.clear();
    CHECK(Only(load.Update(input), {TouchAction::Right}));
    input.sceneSerial = 8;
    step = load.Update(input);
    CHECK(Only(step, {}));
    CHECK(step.combo == TouchCombo::None);
    CHECK(load.RunningCombo() == TouchCombo::None);
    CHECK(Only(load.Update(input), {}));

    // A menu (a death into game over).
    TouchControls menu;
    menu.Update(Facing({Finger(1, Centre(TouchControl::SpellCombo))}, TouchFacing::Left));
    CHECK(Only(menu.Update(Facing({}, TouchFacing::Left)), {TouchAction::Down}));
    step = menu.Update(Menu({}, TouchCorner::Back));
    CHECK(Only(step, {}));
    CHECK(menu.RunningCombo() == TouchCombo::None);
    CHECK(Only(menu.Update(Facing({}, TouchFacing::Left)), {}));

    // CancelCombo: the layer's, when the touch controls are switched off.
    TouchControls off;
    off.Update(Facing({Finger(1, Centre(TouchControl::SwordCombo))}, TouchFacing::Right));
    CHECK(off.RunningCombo() == TouchCombo::Sword);
    off.CancelCombo();
    CHECK(off.RunningCombo() == TouchCombo::None);
    CHECK(Only(off.Update(Facing({}, TouchFacing::Right)), {}));
}

// "enabled": false takes a button out of the layout: not drawn, not touched,
// the rest as before.
void testComboManifest() {
    std::string warning;
    const TouchManifest noCombos = TouchControls::ManifestFromJson(
        R"({"controls": {"swordCombo": {"enabled": false}, "spellCombo": {"enabled": false}}})", &warning);
    CHECK(warning.empty());
    CHECK(!noCombos[TouchControl::SwordCombo].enabled);
    CHECK(!noCombos[TouchControl::SpellCombo].enabled);
    CHECK(noCombos[TouchControl::Jump].enabled);
    TouchControls touch;
    touch.SetManifest(noCombos);
    touch.SetImageRoot(PENUMBRA_DATA_DIR);
    TouchStep step = touch.Update(Play({Finger(1, Centre(TouchControl::SwordCombo))}));
    CHECK(Only(step, {}));
    CHECK(touch.RunningCombo() == TouchCombo::None);
    CHECK(!touch.Visible(TouchControl::SwordCombo));
    CHECK(!touch.Visible(TouchControl::SpellCombo));
    CHECK(touch.Visible(TouchControl::Jump));
    std::vector<HudCmd> out;
    touch.AppendOverlay(out);
    // The disc, its two arrows (E25: no down), the four buttons, pause (no knob at rest).
    CHECK_EQ(out.size(), std::size_t{8});
    for (const HudCmd& cmd : out) CHECK(cmd.sprite.find("combo_") == std::string::npos);
    step = touch.Update(Play({Finger(1, Centre(TouchControl::SwordCombo)), Finger(2, Centre(TouchControl::Jump))}));
    CHECK(Only(step, {TouchAction::Jump}));

    // One left in: it works alone.
    const TouchManifest spellOnly =
        TouchControls::ManifestFromJson(R"({"controls": {"swordCombo": {"enabled": false}}})", &warning);
    TouchControls one;
    one.SetManifest(spellOnly);
    one.Update(Facing({Finger(1, Centre(TouchControl::SpellCombo))}, TouchFacing::Right));
    CHECK(one.RunningCombo() == TouchCombo::Spell);
    CHECK(Only(one.Update(Facing({}, TouchFacing::Right)), {TouchAction::Down}));

    // Not true or false: a warning, and the button stays.
    warning.clear();
    const TouchManifest odd = TouchControls::ManifestFromJson(R"({"controls": {"spellCombo": {"enabled": 0}}})", &warning);
    CHECK(odd[TouchControl::SpellCombo].enabled);
    CHECK(warning.find("spellCombo.enabled") != std::string::npos);
}

// One game tick as the layer and the game run it - the touch controls, the
// frame, the observer, the Ethanon frame - then the wizard's combo buffer as
// controlCharacter reads it after the input (playerInput.as:358-381), in a
// bare Machine: the ported Combo, the one g_comboManager holds.
class ComboRig {
public:
    explicit ComboRig(Machine& machine) : m_machine(machine) {}

    TouchStep Tap(const TouchInput& input, const InputFrame& devices = InputFrame{}) {
        const TouchStep step = touch.Update(input);
        InputFrame frame = devices;
        TouchControls::ApplyToFrame(step, frame);
        Run(frame);
        return step;
    }
    // A frame made by hand, as the keyboard or a pad would.
    void Run(const InputFrame& frame) {
        touch.ObserveFrame(frame, 1);   // getPlayerJoystick(0) under the default g_controls
        m_machine.Frame(frame);
        combo.updateInput(0);
        if (combo.checkSequence(Script::CMD_LEFT, Script::CMD_LEFT, Script::CMD_SWORD)) {
            fired.push_back({tick, "sword left"});
        } else if (combo.checkSequence(Script::CMD_RIGHT, Script::CMD_RIGHT, Script::CMD_SWORD)) {
            fired.push_back({tick, "sword right"});
        }
        if (combo.checkSequence(Script::CMD_DOWN, Script::CMD_LEFT, Script::CMD_SPELL)) {
            fired.push_back({tick, "spell left"});
        } else if (combo.checkSequence(Script::CMD_DOWN, Script::CMD_RIGHT, Script::CMD_SPELL)) {
            fired.push_back({tick, "spell right"});
        }
        ++tick;
    }
    void Idle(int ticks) {
        for (int i = 0; i < ticks; ++i) Tap(Play());
    }
    // The one combo that fired since `from`, as "name@tick", or "".
    std::string FiredSince(int from) const {
        std::string out;
        for (const Fired& f : fired) {
            if (f.tick < from) continue;
            if (!out.empty()) out += ", ";
            out += f.name + "@" + std::to_string(f.tick - from);
        }
        return out;
    }

    struct Fired {
        int tick;
        std::string name;
    };
    TouchControls touch;
    Script::Combo combo;
    std::vector<Fired> fired;
    int tick = 0;

private:
    Machine& m_machine;
};

void testComboBuffer() {
    // The quiet the controls wait for is the buffer's own: the fewest ticks
    // whose GetTime difference (frame * 1000 / 60) is always more than
    // BUTTON_STRIDE (combo.as:44, :116).
    unsigned fewest = 0;
    for (unsigned n = 1; n < 60 && fewest == 0; ++n) {
        bool always = true;
        for (unsigned t = 0; t < 120; ++t) {
            if ((t + n) * 1000u / 60u - t * 1000u / 60u <= Script::BUTTON_STRIDE) always = false;
        }
        if (always) fewest = n;
    }
    CHECK_EQ(fewest, TouchControls::kComboQuietTicks);

    MachineConfig config;
    config.userRoot.clear();   // nothing is written
    Machine machine(config);
    Machine::Scope scope(machine);
    machine.Boot([] { LoadScene("", "", ""); });
    machine.Frame(InputFrame{});
    ComboRig rig(machine);
    // E37: below, a button is often tapped again soon after its last combo. Every wait is therefore as long as a button
    // rests (the buffer needs 14 quiet ticks, the button kComboCooldownTicks); what each combo then does is as it was.
    const int rest = static_cast<int>(TouchControls::kComboCooldownTicks);   // E37
    rig.Idle(rest);

    const glm::vec2 swordButton = Centre(TouchControl::SwordCombo);
    const glm::vec2 spellButton = Centre(TouchControl::SpellCombo);
    const auto tapAndRun = [&](glm::vec2 button, TouchFacing facing, int ticks) {
        const int from = rig.tick;
        rig.Tap(Facing({Finger(1, button)}, facing));
        for (int i = 1; i < ticks; ++i) rig.Tap(Facing({}, facing));
        return from;
    };

    // After a quiet spell each fires on its last press: the sword combo on its
    // fifth tick, the spell combo on its fourth, toward the way he faces.
    int from = tapAndRun(swordButton, TouchFacing::Right, 8);
    CHECK_MSG(rig.FiredSince(from) == "sword right@4", rig.FiredSince(from));
    rig.Idle(rest);
    from = tapAndRun(swordButton, TouchFacing::Left, 8);
    CHECK_MSG(rig.FiredSince(from) == "sword left@4", rig.FiredSince(from));
    rig.Idle(rest);
    from = tapAndRun(spellButton, TouchFacing::Right, 8);
    CHECK_MSG(rig.FiredSince(from) == "spell right@3", rig.FiredSince(from));
    rig.Idle(rest);
    from = tapAndRun(spellButton, TouchFacing::Left, 8);
    CHECK_MSG(rig.FiredSince(from) == "spell left@3", rig.FiredSince(from));

    // Back to back: the combo's own presses were presses, so the next one
    // waits out the buffer's 13 ticks: the sword combo's last press is on
    // tick 4, the spell combo's first on tick 4 + 14, its fire on 20.
    rig.Idle(rest);
    from = rig.tick;
    rig.Tap(Facing({Finger(1, swordButton)}, TouchFacing::Right));
    for (int i = 0; i < 4; ++i) rig.Tap(Facing({}, TouchFacing::Right));
    rig.Tap(Facing({Finger(2, spellButton)}, TouchFacing::Right));
    for (int i = 0; i < 20; ++i) rig.Tap(Facing({}, TouchFacing::Right));
    CHECK_MSG(rig.FiredSince(from) == "sword right@4, spell right@20", rig.FiredSince(from));

    // RIGHT AFTER A STEP: the disc pressed right, the combo tapped on the next
    // tick with the thumb still down. The buffer holds CMD_RIGHT: the first
    // press waits 14 ticks from the step, and the combo fires.
    rig.Idle(rest);
    const glm::vec2 right = Dpad({100.0f, 0.0f});
    const int step = rig.tick;
    rig.Tap(Facing({Finger(1, right)}, TouchFacing::Right));
    rig.Tap(Facing({Finger(1, right), Finger(2, swordButton)}, TouchFacing::Right));
    int firstPress = -1;
    for (int i = 0; i < 20; ++i) {
        const TouchStep held = rig.Tap(Facing({Finger(1, right)}, TouchFacing::Right));
        if (firstPress < 0 && held.Held(TouchAction::Right)) firstPress = rig.tick - 1 - step;
    }
    CHECK_EQ(firstPress, 14);
    CHECK_MSG(rig.FiredSince(step) == "sword right@17", rig.FiredSince(step));

    // Why it waits - the same presses by hand, pressed at once after the step,
    // fire nothing: the buffer reads RIGHT, RIGHT, RIGHT, SWORD.
    rig.Idle(rest);
    const auto keys = [](std::initializer_list<KEY> down) {
        InputFrame frame;
        for (const KEY key : down) frame.keys[static_cast<std::size_t>(key)] = true;
        return frame;
    };
    from = rig.tick;
    for (const InputFrame& frame : {keys({K_RIGHT}), keys({}), keys({K_RIGHT}), keys({}), keys({K_RIGHT}),
                                    keys({K_S}), keys({}), keys({})}) {
        rig.Run(frame);
    }
    CHECK_MSG(rig.FiredSince(from).empty(), rig.FiredSince(from));

    // Any device counts: the keyboard's Up and player 1's pad's sword button
    // (JK_04) a tick before the tap put it off 14 ticks from them too.
    rig.Idle(rest);
    from = rig.tick;
    rig.Tap(Play(), keys({K_UP}));
    rig.Tap(Facing({Finger(1, spellButton)}, TouchFacing::Left));
    for (int i = 0; i < 20; ++i) rig.Tap(Facing({}, TouchFacing::Left));
    CHECK_MSG(rig.FiredSince(from) == "spell left@16", rig.FiredSince(from));
    rig.Idle(rest);
    InputFrame pad;
    pad.pads[1].connected = true;
    pad.pads[1].buttons[JK_04] = true;
    InputFrame padIdle;
    padIdle.pads[1].connected = true;
    from = rig.tick;
    rig.Tap(Play(), pad);
    rig.Tap(Facing({Finger(1, swordButton)}, TouchFacing::Right), padIdle);
    for (int i = 0; i < 20; ++i) rig.Tap(Facing({}, TouchFacing::Right), padIdle);
    CHECK_MSG(rig.FiredSince(from) == "sword right@17", rig.FiredSince(from));
    // Player 2's pad does not (under the default g_controls it is pad 0).
    rig.Idle(rest);
    InputFrame other;
    other.pads[0].connected = true;
    other.pads[0].buttons[JK_04] = true;
    from = rig.tick;
    rig.Tap(Play(), other);
    rig.Tap(Facing({Finger(1, swordButton)}, TouchFacing::Right));
    for (int i = 0; i < 8; ++i) rig.Tap(Facing({}, TouchFacing::Right));
    CHECK_MSG(rig.FiredSince(from) == "sword right@5", rig.FiredSince(from));

    // Something that keeps pressing - the keyboard's Up, from the tap's own
    // tick on, every other tick: the combo gives up after its longest wait,
    // having pressed nothing, and nothing fires. (Pressing that starts only
    // after a combo's first press lands in the buffer as any device's would.)
    rig.Idle(rest);
    from = rig.tick;
    rig.Tap(Facing({Finger(1, swordButton)}, TouchFacing::Right), keys({K_UP}));
    int pressed = 0;
    for (int i = 0; i < 40; ++i) {
        const TouchStep held = rig.Tap(Facing({}, TouchFacing::Right), i % 2 == 1 ? keys({K_UP}) : keys({}));
        for (int a = 0; a < kTouchActionCount; ++a) pressed += held.held[static_cast<std::size_t>(a)] ? 1 : 0;
    }
    CHECK_EQ(pressed, 0);
    CHECK(rig.touch.RunningCombo() == TouchCombo::None);
    CHECK_MSG(rig.FiredSince(from).empty(), rig.FiredSince(from));
}

// --- E37: the combo buttons' cooldown -------------------------------------------------------------------------

// One game tick as the layer runs it for the touch controls: Update with this tick's fingers, then ObserveFrame with the frame
// the step made. The pause's and the editor's ticks run Update alone, and the tests that mean them call it by hand.
class CooldownRig {
public:
    TouchStep Tick(TouchInput input) {
        input.sceneSerial = serial;
        const TouchStep step = touch.Update(input);
        touch.ObserveFrame(FrameOf(step), 1);   // getPlayerJoystick(0) under the default g_controls
        ++tick;
        return step;
    }
    TouchStep Idle() { return Tick(Facing({}, TouchFacing::Right)); }
    TouchStep Tap(TouchControl button, int finger) {
        return Tick(Facing({Finger(finger, Centre(button))}, TouchFacing::Right));
    }
    // A tap on the sword combo button, then the four ticks of its macro that follow: the last of them presses the attack key.
    void SwordToItsAttackKey() {
        Tap(TouchControl::SwordCombo, 1);
        for (int i = 0; i < 4; ++i) Idle();
    }
    TouchControls touch;
    unsigned serial = 0;
    int tick = 0;
};

// The whole life of one button's cooldown: ready at the start; none while its macro runs; 60 ticks from the attack key, in
// which every tap is refused (not started, not queued) with the flash and, spaced out, the cue; then ready again.
void testComboCooldownGate() {
    constexpr unsigned kRest = TouchControls::kComboCooldownTicks;
    CHECK_EQ(kRest, 60u);   // 1000 ms at the game's 60 ticks a second

    CooldownRig rig;
    CHECK_EQ(rig.touch.CooldownTicksLeft(TouchCombo::Sword), 0u);
    CHECK_EQ(rig.touch.CooldownTicksLeft(TouchCombo::Spell), 0u);
    CHECK_NEAR(rig.touch.CooldownFraction(TouchCombo::Sword), 0.0f);
    CHECK_EQ(rig.touch.CooldownTicksLeft(TouchCombo::None), 0u);

    // Ready: the tap is taken, and nothing is refused.
    TouchStep step = rig.Tap(TouchControl::SwordCombo, 1);
    CHECK(!step.Denied(TouchCombo::Sword) && !step.Denied(TouchCombo::Spell) && !step.deniedSound);
    CHECK(rig.touch.RunningCombo() == TouchCombo::Sword);
    // The macro's ticks 1 to 3 rest nothing: the rest begins with the attack key, tick 4.
    for (std::size_t t = 1; t <= 3; ++t) {
        rig.Idle();
        CHECK_MSG(rig.touch.CooldownTicksLeft(TouchCombo::Sword) == 0u, TickName("before the attack key", t));
    }
    step = rig.Idle();
    CHECK(step.Held(TouchAction::Sword));
    CHECK(rig.touch.RunningCombo() == TouchCombo::None);
    // Set to 60 as the key was pressed, 59 once the game has run that tick.
    CHECK_EQ(rig.touch.CooldownTicksLeft(TouchCombo::Sword), kRest - 1u);
    CHECK_EQ(rig.touch.CooldownTicksLeft(TouchCombo::Spell), 0u);

    // The ticks 1 to 59 after the attack key: a new finger lands on the button every tick (the one before has lifted).
    float shade = rig.touch.CooldownFraction(TouchCombo::Sword);
    CHECK_NEAR(shade, 59.0f / 60.0f);
    int cues = 0;
    for (unsigned k = 1; k < kRest; ++k) {
        step = rig.Tap(TouchControl::SwordCombo, 100 + static_cast<int>(k));
        const std::string where = TickName("a tap after the attack key", k);
        CHECK_MSG(step.Denied(TouchCombo::Sword), where);
        CHECK_MSG(!step.Denied(TouchCombo::Spell), where);
        CHECK_MSG(rig.touch.RunningCombo() == TouchCombo::None && step.combo == TouchCombo::None, where);   // started nothing
        CHECK_MSG(Only(step, {}), where);
        // The cue: on the first refusal, then not again until kComboDeniedSoundGapTicks game ticks have run.
        CHECK_MSG(step.deniedSound == ((k - 1u) % TouchControls::kComboDeniedSoundGapTicks == 0u), where);
        if (step.deniedSound) ++cues;
        // The flash is counted from the refusal: it is on for the rest of its length.
        CHECK_MSG(rig.touch.DeniedFlashTicksLeft(TouchCombo::Sword) == TouchControls::kComboDeniedFlashTicks - 1u, where);
        // And the shade shrinks, a tick's worth a tick, to nothing.
        CHECK_MSG(rig.touch.CooldownTicksLeft(TouchCombo::Sword) == kRest - 1u - k, where);
        CHECK_MSG(rig.touch.CooldownFraction(TouchCombo::Sword) < shade, where);
        shade = rig.touch.CooldownFraction(TouchCombo::Sword);
    }
    CHECK_EQ(cues, 6);   // ticks 1, 11, 21, 31, 41, 51
    CHECK_EQ(rig.touch.CooldownTicksLeft(TouchCombo::Sword), 0u);
    CHECK_NEAR(rig.touch.CooldownFraction(TouchCombo::Sword), 0.0f);

    // The 60th tick: taken. Its macro's attack key is 4 ticks on, so two attacks are 64 ticks apart at the closest.
    step = rig.Tap(TouchControl::SwordCombo, 200);
    CHECK(!step.Denied(TouchCombo::Sword) && !step.deniedSound);
    CHECK(rig.touch.RunningCombo() == TouchCombo::Sword && step.combo == TouchCombo::Sword);
    for (int i = 0; i < 4; ++i) rig.Idle();
    CHECK_EQ(rig.touch.CooldownTicksLeft(TouchCombo::Sword), kRest - 1u);

    // The spell combo has its own, with its own attack key: the 4th tick of its macro.
    CooldownRig cast;
    step = cast.Tap(TouchControl::SpellCombo, 1);
    CHECK(cast.touch.RunningCombo() == TouchCombo::Spell);
    for (std::size_t t = 1; t <= 2; ++t) {
        cast.Idle();
        CHECK_MSG(cast.touch.CooldownTicksLeft(TouchCombo::Spell) == 0u, TickName("before the fire key", t));
    }
    step = cast.Idle();
    CHECK(step.Held(TouchAction::Fire));
    CHECK_EQ(cast.touch.CooldownTicksLeft(TouchCombo::Spell), kRest - 1u);
    CHECK_EQ(cast.touch.CooldownTicksLeft(TouchCombo::Sword), 0u);
    for (unsigned k = 1; k < kRest; ++k) {
        step = cast.Tap(TouchControl::SpellCombo, 100 + static_cast<int>(k));
        CHECK_MSG(step.Denied(TouchCombo::Spell) && !step.Denied(TouchCombo::Sword), TickName("a spell tap", k));
        CHECK_MSG(cast.touch.RunningCombo() == TouchCombo::None, TickName("a spell tap", k));
    }
    step = cast.Tap(TouchControl::SpellCombo, 200);
    CHECK(!step.Denied(TouchCombo::Spell) && cast.touch.RunningCombo() == TouchCombo::Spell);
}

// What a finger does: only its landing is a tap; a refusal is not queued; the buttons rest on their own.
void testComboCooldownFingers() {
    const glm::vec2 sword = Centre(TouchControl::SwordCombo);

    // A finger that lands while the button rests and stays down fires nothing when the rest ends, nor does it refuse again.
    CooldownRig rig;
    rig.SwordToItsAttackKey();
    TouchStep step = rig.Tick(Facing({Finger(2, sword)}, TouchFacing::Right));
    CHECK(step.Denied(TouchCombo::Sword));
    int refusals = 0;
    bool pressed = false;
    for (int t = 0; t < 100; ++t) {
        step = rig.Tick(Facing({Finger(2, sword)}, TouchFacing::Right));
        if (step.Denied(TouchCombo::Sword)) ++refusals;
        if (!Only(step, {})) pressed = true;
    }
    CHECK_EQ(refusals, 0);
    CHECK(!pressed);
    CHECK(rig.touch.RunningCombo() == TouchCombo::None);
    CHECK_EQ(rig.touch.CooldownTicksLeft(TouchCombo::Sword), 0u);   // it ended long ago
    // Lifted and put down again: a new tap, taken.
    rig.Idle();
    step = rig.Tap(TouchControl::SwordCombo, 3);
    CHECK(!step.Denied(TouchCombo::Sword) && rig.touch.RunningCombo() == TouchCombo::Sword);

    // Two fingers landing on a ready button in one tick are one press: the second is not a refusal. On a resting button
    // they are one refusal.
    CooldownRig pair;
    step = pair.Tick(Facing({Finger(1, sword), Finger(2, sword)}, TouchFacing::Right));
    CHECK(!step.Denied(TouchCombo::Sword) && !step.deniedSound);
    CHECK(pair.touch.RunningCombo() == TouchCombo::Sword);
    for (int i = 0; i < 4; ++i) pair.Idle();
    step = pair.Tick(Facing({Finger(3, sword), Finger(4, sword)}, TouchFacing::Right));
    CHECK(step.Denied(TouchCombo::Sword) && step.deniedSound);
    CHECK(pair.touch.RunningCombo() == TouchCombo::None);

    // Refused while the OTHER button's macro runs, and not queued: nothing of it starts when that one ends.
    CooldownRig busy;
    busy.Tap(TouchControl::SwordCombo, 1);
    step = busy.Tap(TouchControl::SpellCombo, 2);
    CHECK(step.Denied(TouchCombo::Spell) && !step.Denied(TouchCombo::Sword) && step.deniedSound);
    CHECK(busy.touch.RunningCombo() == TouchCombo::Sword);
    for (int i = 0; i < 3; ++i) busy.Idle();   // ticks 2 to 4: the sword macro's last
    CHECK(busy.touch.RunningCombo() == TouchCombo::None);
    CHECK_EQ(busy.touch.CooldownTicksLeft(TouchCombo::Spell), 0u);
    for (int i = 0; i < 20; ++i) {
        step = busy.Idle();
        CHECK_MSG(Only(step, {}) && step.combo == TouchCombo::None, TickName("after the refused spell tap", static_cast<std::size_t>(i)));
    }

    // The two rest apart: the spell combo is tapped while the sword button rests and runs; the sword button takes its tap
    // when its own rest is over, with the spell button still resting - and the spell button, then, is refused.
    CooldownRig both;
    both.SwordToItsAttackKey();
    step = both.Tap(TouchControl::SpellCombo, 2);
    CHECK(!step.Denied(TouchCombo::Spell) && both.touch.RunningCombo() == TouchCombo::Spell);
    for (int i = 0; i < 60 && both.touch.RunningCombo() != TouchCombo::None; ++i) both.Idle();
    CHECK(both.touch.RunningCombo() == TouchCombo::None);
    CHECK(both.touch.CooldownTicksLeft(TouchCombo::Sword) > 0u);
    CHECK(both.touch.CooldownTicksLeft(TouchCombo::Spell) > 0u);
    CHECK(both.touch.CooldownTicksLeft(TouchCombo::Sword) < both.touch.CooldownTicksLeft(TouchCombo::Spell));
    for (int i = 0; i < 100 && both.touch.CooldownTicksLeft(TouchCombo::Sword) > 0u; ++i) both.Idle();
    CHECK_EQ(both.touch.CooldownTicksLeft(TouchCombo::Sword), 0u);
    CHECK(both.touch.CooldownTicksLeft(TouchCombo::Spell) > 0u);
    step = both.Tap(TouchControl::SwordCombo, 3);
    CHECK(!step.Denied(TouchCombo::Sword) && both.touch.RunningCombo() == TouchCombo::Sword);
    step = both.Tap(TouchControl::SpellCombo, 4);
    CHECK(step.Denied(TouchCombo::Spell) && !step.Denied(TouchCombo::Sword));
}

// What starts no rest and what ends it: a macro stopped before its attack key; a new scene; and the ticks the game does not run.
void testComboCooldownReset() {
    const glm::vec2 sword = Centre(TouchControl::SwordCombo);

    // The layer's CancelCombo (the controls switched off) before the attack key: no rest.
    {
        CooldownRig rig;
        rig.Tap(TouchControl::SwordCombo, 1);
        rig.Idle();
        rig.touch.CancelCombo();
        CHECK_EQ(rig.touch.CooldownTicksLeft(TouchCombo::Sword), 0u);
        rig.Idle();
        const TouchStep step = rig.Tap(TouchControl::SwordCombo, 2);
        CHECK(!step.Denied(TouchCombo::Sword) && rig.touch.RunningCombo() == TouchCombo::Sword);
    }
    // The pause or a menu (Update with a scene that is not play) before it.
    {
        CooldownRig rig;
        rig.Tap(TouchControl::SwordCombo, 1);
        rig.Idle();
        rig.touch.Update(Menu({}, TouchCorner::Back));
        CHECK(rig.touch.RunningCombo() == TouchCombo::None);
        CHECK_EQ(rig.touch.CooldownTicksLeft(TouchCombo::Sword), 0u);
        const TouchStep step = rig.Tap(TouchControl::SwordCombo, 2);
        CHECK(!step.Denied(TouchCombo::Sword) && rig.touch.RunningCombo() == TouchCombo::Sword);
    }
    // The pause opening on the very tick the attack key is pressed: Update has made the key, but the game does not play that
    // tick (ObserveFrame is not called), so the key never reaches it; the pause's own Update follows. No rest, no refusal.
    {
        CooldownRig rig;
        rig.Tap(TouchControl::SwordCombo, 1);
        for (int i = 0; i < 3; ++i) rig.Idle();
        TouchInput last = Facing({}, TouchFacing::Right);
        last.sceneSerial = rig.serial;
        const TouchStep key = rig.touch.Update(last);   // the attack key's tick, as the pause takes it
        CHECK(key.Held(TouchAction::Sword));
        CHECK_EQ(rig.touch.CooldownTicksLeft(TouchCombo::Sword), 0u);
        rig.touch.Update(Menu({}, TouchCorner::Back));
        rig.touch.Update(Menu({}, TouchCorner::Back));
        CHECK_EQ(rig.touch.CooldownTicksLeft(TouchCombo::Sword), 0u);
        // Resumed: ticks run again and the button was never rested; the next tap starts a macro.
        rig.Idle();
        rig.Idle();
        CHECK_EQ(rig.touch.CooldownTicksLeft(TouchCombo::Sword), 0u);
        const TouchStep step = rig.Tap(TouchControl::SwordCombo, 2);
        CHECK(!step.Denied(TouchCombo::Sword) && rig.touch.RunningCombo() == TouchCombo::Sword);
    }
    // A load before it.
    {
        CooldownRig rig;
        rig.serial = 7;
        rig.Tap(TouchControl::SwordCombo, 1);
        rig.Idle();
        rig.serial = 8;
        rig.Idle();
        CHECK(rig.touch.RunningCombo() == TouchCombo::None);
        CHECK_EQ(rig.touch.CooldownTicksLeft(TouchCombo::Sword), 0u);
    }
    // The wait for the combo buffer that gives up (something else keeps pressing the keys it reads): nothing was pressed.
    {
        CooldownRig rig;
        const auto tickWithUp = [&](const TouchInput& input, bool up) {
            const TouchStep step = rig.touch.Update(input);
            InputFrame frame = FrameOf(step);
            if (up) frame.keys[K_UP] = true;
            rig.touch.ObserveFrame(frame, 1);
            return step;
        };
        tickWithUp(Facing({Finger(1, sword)}, TouchFacing::Right), true);
        int pressedKeys = 0;
        for (int i = 0; i < 40; ++i) {
            const TouchStep step = tickWithUp(Facing({}, TouchFacing::Right), i % 2 == 1);
            for (int a = 0; a < kTouchActionCount; ++a) pressedKeys += step.held[static_cast<std::size_t>(a)] ? 1 : 0;
        }
        CHECK_EQ(pressedKeys, 0);
        CHECK(rig.touch.RunningCombo() == TouchCombo::None);
        CHECK_EQ(rig.touch.CooldownTicksLeft(TouchCombo::Sword), 0u);
    }

    // A new scene (a load, a death's reload) clears both rests - and its first tap is taken, the clearing coming first.
    {
        CooldownRig rig;
        rig.serial = 3;
        rig.SwordToItsAttackKey();
        TouchStep step = rig.Tap(TouchControl::SpellCombo, 2);
        for (int i = 0; i < 60 && rig.touch.RunningCombo() != TouchCombo::None; ++i) rig.Idle();
        CHECK(rig.touch.CooldownTicksLeft(TouchCombo::Sword) > 0u && rig.touch.CooldownTicksLeft(TouchCombo::Spell) > 0u);
        // A refused tap: its flash is on, and goes with the load too.
        step = rig.Tap(TouchControl::SwordCombo, 3);
        CHECK(step.Denied(TouchCombo::Sword));
        CHECK(rig.touch.DeniedFlashTicksLeft(TouchCombo::Sword) > 0u);
        rig.serial = 4;
        step = rig.Tap(TouchControl::SwordCombo, 4);
        CHECK(!step.Denied(TouchCombo::Sword));
        CHECK(rig.touch.RunningCombo() == TouchCombo::Sword);
        CHECK_EQ(rig.touch.CooldownTicksLeft(TouchCombo::Spell), 0u);
        CHECK_EQ(rig.touch.DeniedFlashTicksLeft(TouchCombo::Sword), 0u);
    }

    // The ticks the game does not run never count: the pause's Update in a menu scene, the editor's in the edit scene, and
    // a play Update beyond the tick's one (the editor's paths call it again) - only ObserveFrame is the clock.
    {
        CooldownRig rig;
        rig.SwordToItsAttackKey();
        CHECK_EQ(rig.touch.CooldownTicksLeft(TouchCombo::Sword), 59u);
        for (int i = 0; i < 40; ++i) rig.touch.Update(Menu({}, TouchCorner::Hidden));
        for (int i = 0; i < 40; ++i) {
            TouchInput edit = Facing({}, TouchFacing::Right);
            edit.scene = TouchScene::Edit;
            edit.corner = TouchCorner::Hidden;
            rig.touch.Update(edit);
        }
        for (int i = 0; i < 40; ++i) rig.touch.Update(Facing({}, TouchFacing::Right));
        CHECK_EQ(rig.touch.CooldownTicksLeft(TouchCombo::Sword), 59u);
        // The game runs again: one tick, one count.
        rig.Idle();
        CHECK_EQ(rig.touch.CooldownTicksLeft(TouchCombo::Sword), 58u);
    }
}

// What the player sees: the commands the overlay appends for a resting, a recharging, a refused and a ready combo button,
// with the data folder's art and without; and that the editor's preview is the buttons at rest.
void testComboCooldownLook() {
    const TouchManifest manifest = TouchControls::DefaultManifest();
    const std::uint8_t idle = static_cast<std::uint8_t>(std::lround(manifest.idleAlpha * 255.0f));
    const std::uint8_t pressed = static_cast<std::uint8_t>(std::lround(manifest.pressedAlpha * 255.0f));
    const TouchLayout::Box box = Default()[TouchControl::SwordCombo];
    const glm::ivec2 art = Penumbra::Render::ProbeImageSize(
        (fs::path(PENUMBRA_DATA_DIR) / "images" / "touch" / "combo_sword.png").generic_string());
    CHECK(art.x > 0 && art.y > 0);

    const auto black = [](const HudCmd& cmd) { return (cmd.color & 0x00FFFFFFu) == 0u && Alpha(cmd.color) > 0; };
    const auto swordArt = [&](const HudCmd& cmd) {
        return cmd.sprite.find("combo_sword.png") != std::string::npos && !black(cmd);
    };
    const auto overlay = [](const TouchControls& touch) {
        std::vector<HudCmd> out;
        touch.AppendOverlay(out);
        return out;
    };
    const auto countBlack = [&](const std::vector<HudCmd>& out) {
        return static_cast<int>(std::count_if(out.begin(), out.end(), black));
    };
    const auto findOne = [](const std::vector<HudCmd>& out, const auto& wanted) {
        const HudCmd* found = nullptr;
        for (const HudCmd& cmd : out) {
            if (wanted(cmd)) found = &cmd;
        }
        return found;
    };

    CooldownRig rig;
    rig.touch.SetImageRoot(PENUMBRA_DATA_DIR);
    rig.Idle();
    // At rest: the ten commands of the rest look, the sword combo's art white at the idle alpha, no shade.
    std::vector<HudCmd> out = overlay(rig.touch);
    CHECK_EQ(out.size(), std::size_t{10});
    CHECK_EQ(countBlack(out), 0);
    const HudCmd* base = findOne(out, swordArt);
    CHECK(base != nullptr);
    if (base != nullptr) {
        CHECK_EQ(static_cast<int>(Alpha(base->color)), static_cast<int>(idle));
        CHECK_EQ(base->color & 0x00FFFFFFu, 0x00FFFFFFu);
        CHECK_NEAR(base->pos.x, box.min.x);
        CHECK_NEAR(base->pos.y, box.min.y);
    }

    // Recharging: the art grey at the idle alpha, and one more command, a black copy of the art cut to the top share of
    // it that has not recharged yet - the same box and image fraction - which gets smaller every tick.
    rig.SwordToItsAttackKey();
    float shadeHeight = box.Size().y;
    for (int t = 0; t < 59; ++t) {
        out = overlay(rig.touch);
        const std::string where = TickName("recharging", static_cast<std::size_t>(t));
        CHECK_MSG(out.size() == std::size_t{11}, where);
        CHECK_MSG(countBlack(out) == 1, where);
        const HudCmd* shade = findOne(out, black);
        const HudCmd* grey = findOne(out, swordArt);
        const float fraction = rig.touch.CooldownFraction(TouchCombo::Sword);
        CHECK_MSG(shade != nullptr && grey != nullptr, where);
        if (shade != nullptr && grey != nullptr) {
            // A tick after the attack key it is still lit (it ran then); from the next it is grey.
            if (t > 0) {
                CHECK_MSG(Alpha(grey->color) == idle && (grey->color & 0x00FFFFFFu) == 0x00969696u, where);
            }
            CHECK_MSG(shade->kind == HudCmd::Kind::ShapedSprite && shade->sprite == grey->sprite, where);
            CHECK_MSG(Alpha(shade->color) > idle, where);   // darker than the art it covers
            CHECK_MSG(std::fabs(shade->pos.x - box.min.x) < 0.01f && std::fabs(shade->pos.y - box.min.y) < 0.01f, where);
            CHECK_MSG(std::fabs(shade->size.x - box.Size().x) < 0.01f, where);
            CHECK_MSG(std::fabs(shade->size.y - box.Size().y * fraction) < 0.01f, where);
            CHECK_MSG(shade->spriteRectMin == glm::vec2(0.0f), where);
            CHECK_MSG(std::fabs(shade->spriteRectMax.x - static_cast<float>(art.x)) < 0.01f &&
                          std::fabs(shade->spriteRectMax.y - static_cast<float>(art.y) * fraction) < 0.01f,
                      where);
            CHECK_MSG(shade->size.y < shadeHeight, where);
            shadeHeight = shade->size.y;
        }
        rig.Idle();
    }
    // Ready (the 60th tick): no shade, the rest look's ten commands, and a glow - brighter than idle, back to idle in 12 ticks.
    CHECK_EQ(rig.touch.CooldownTicksLeft(TouchCombo::Sword), 0u);
    for (unsigned n = 0; n <= TouchControls::kComboReadyPulseTicks; ++n) {
        out = overlay(rig.touch);
        const std::string where = TickName("ready", n);
        CHECK_MSG(out.size() == std::size_t{10} && countBlack(out) == 0, where);
        base = findOne(out, swordArt);
        CHECK_MSG(base != nullptr, where);
        if (base != nullptr) {
            const bool glowing = n < TouchControls::kComboReadyPulseTicks;
            CHECK_MSG((Alpha(base->color) > idle) == glowing, where);
            CHECK_MSG(Alpha(base->color) <= pressed, where);
            CHECK_MSG((base->color & 0x00FFFFFFu) == 0x00FFFFFFu, where);
        }
        if (n == 0 && base != nullptr) CHECK_EQ(static_cast<int>(Alpha(base->color)), static_cast<int>(pressed));
        rig.Idle();
    }

    // Refused: the art red (full red, the others down), brighter, and shaken off its place - but the layout, which the
    // fingers are held to, does not move; the spell combo button is as it was. Then it fades back over 15 ticks.
    CooldownRig denied;
    denied.touch.SetImageRoot(PENUMBRA_DATA_DIR);
    denied.SwordToItsAttackKey();
    denied.Idle();
    const TouchLayout::Box spellBox = Default()[TouchControl::SpellCombo];
    denied.Tap(TouchControl::SwordCombo, 2);
    for (unsigned t = 0; t < TouchControls::kComboDeniedFlashTicks; ++t) {
        out = overlay(denied.touch);
        const std::string where = TickName("refused", t);
        base = findOne(out, swordArt);
        CHECK_MSG(base != nullptr, where);
        if (t < TouchControls::kComboDeniedFlashTicks - 1u && base != nullptr) {
            const std::uint32_t red = (base->color >> 16) & 0xFFu;
            const std::uint32_t green = (base->color >> 8) & 0xFFu;
            const std::uint32_t blue = base->color & 0xFFu;
            CHECK_MSG(red == 255u && green < 255u && green == blue, where);
            CHECK_MSG(Alpha(base->color) > idle, where);
            CHECK_MSG(std::fabs(base->pos.x - box.min.x) > 0.01f && std::fabs(base->pos.y - box.min.y) < 0.01f, where);
            CHECK_MSG(std::fabs(base->pos.x - box.min.x) <= 5.0f, where);   // by a few pixels, not off the button
        }
        const HudCmd* spell = findOne(out, [](const HudCmd& cmd) {
            return cmd.sprite.find("combo_spell.png") != std::string::npos;
        });
        CHECK_MSG(spell != nullptr && Alpha(spell->color) == idle && (spell->color & 0x00FFFFFFu) == 0x00FFFFFFu &&
                      std::fabs(spell->pos.x - spellBox.min.x) < 0.01f,
                  where);
        CHECK_MSG(denied.touch.Layout()[TouchControl::SwordCombo].min == box.min, where);
        denied.Idle();
    }
    // Over: the grey of a button that still rests, on its own place.
    out = overlay(denied.touch);
    base = findOne(out, swordArt);
    CHECK(base != nullptr);
    if (base != nullptr) {
        CHECK_EQ(base->color & 0x00FFFFFFu, 0x00969696u);
        CHECK_NEAR(base->pos.x, box.min.x);
    }

    // The editor's preview shows the buttons at rest, whatever they were doing: the same commands, bit for bit, as a
    // fresh set of controls in the same scene. In a menu (the pause) neither is there at all.
    CooldownRig resting;
    resting.touch.SetImageRoot(PENUMBRA_DATA_DIR);
    resting.SwordToItsAttackKey();
    resting.Tap(TouchControl::SpellCombo, 2);
    resting.Tap(TouchControl::SwordCombo, 3);   // refused, flashing
    CHECK(resting.touch.CooldownTicksLeft(TouchCombo::Sword) > 0u && resting.touch.DeniedFlashTicksLeft(TouchCombo::Sword) > 0u);
    TouchInput edit = Facing({}, TouchFacing::Right);
    edit.scene = TouchScene::Edit;
    edit.corner = TouchCorner::Hidden;
    resting.touch.Update(edit);
    TouchControls fresh;
    fresh.SetImageRoot(PENUMBRA_DATA_DIR);
    fresh.Update(edit);
    const std::vector<HudCmd> previewed = overlay(resting.touch);
    const std::vector<HudCmd> atRest = overlay(fresh);
    CHECK(!atRest.empty());
    CHECK_EQ(previewed.size(), atRest.size());
    for (std::size_t i = 0; i < previewed.size() && i < atRest.size(); ++i) {
        CHECK_MSG(previewed[i].kind == atRest[i].kind && previewed[i].sprite == atRest[i].sprite &&
                      previewed[i].pos == atRest[i].pos && previewed[i].size == atRest[i].size &&
                      previewed[i].color == atRest[i].color && previewed[i].spriteRectMax == atRest[i].spriteRectMax,
                  TickName("the editor's preview, command", i));
    }
    resting.touch.Update(Menu());
    CHECK(overlay(resting.touch).empty());

    // Without the art in the data folder: plain squares, and the shade a black rectangle of the same share of the box.
    CooldownRig bare;
    bare.SwordToItsAttackKey();
    for (int i = 0; i < 10; ++i) bare.Idle();
    out = overlay(bare.touch);
    CHECK_EQ(out.size(), std::size_t{9});   // the disc, six buttons, pause - and the shade
    const HudCmd* plainShade = findOne(out, black);
    CHECK(plainShade != nullptr);
    if (plainShade != nullptr) {
        CHECK(plainShade->kind == HudCmd::Kind::Rectangle && !plainShade->stretchToSides);
        CHECK(std::fabs(plainShade->size.y - box.Size().y * bare.touch.CooldownFraction(TouchCombo::Sword)) < 0.01f);
        CHECK(std::fabs(plainShade->size.x - box.Size().x) < 0.01f);
    }

    // Faint controls (opacity 0.2, an idle alpha of 0.09): the shade follows them. The pressed alpha has a floor of the
    // manifest's 0.9, so a shade taken from it alone would be about seven times more opaque than the button it covers.
    {
        CooldownRig faint;
        TouchTuning tuning;
        tuning.opacity = 0.2f;
        faint.touch.SetTuning(tuning);
        faint.touch.SetImageRoot(PENUMBRA_DATA_DIR);
        faint.SwordToItsAttackKey();
        faint.Idle();
        const std::vector<HudCmd> faintOut = overlay(faint.touch);
        const HudCmd* faintShade = findOne(faintOut, black);
        const HudCmd* faintArt = findOne(faintOut, swordArt);
        CHECK(faintShade != nullptr && faintArt != nullptr);
        if (faintShade != nullptr && faintArt != nullptr) {
            const int restAlpha = static_cast<int>(Alpha(faintArt->color));
            CHECK(restAlpha > 0 && restAlpha < 40);
            CHECK(static_cast<int>(Alpha(faintShade->color)) > restAlpha);
            CHECK(static_cast<int>(Alpha(faintShade->color)) <= static_cast<int>(std::lround(static_cast<float>(restAlpha) * 1.5f)) + 1);
        }
    }
}

// The cooldown is the touch buttons' alone. Combos typed on a keyboard (here: the frames their macros make, with
// TouchControls::Update never called - the controls off) fire as often as the combo buffer lets them, a button resting
// beside them changes nothing of it, and the typed combos do not wait for the button.
void testComboTypedUnlimited() {
    MachineConfig config;
    config.userRoot.clear();   // nothing is written
    Machine machine(config);
    Machine::Scope scope(machine);
    machine.Boot([] { LoadScene("", "", ""); });
    machine.Frame(InputFrame{});
    ComboRig rig(machine);
    rig.Idle(20);

    const auto hand = [](std::initializer_list<KEY> down) {
        InputFrame frame;
        for (const KEY key : down) frame.keys[static_cast<std::size_t>(key)] = true;
        return frame;
    };
    // CMD side, side, SWORD (playerInput.as:361-362) and CMD DOWN, side, SPELL (:380-381), as a keyboard types them.
    const auto typeSword = [&](KEY side) {
        for (const InputFrame& frame : {hand({}), hand({side}), hand({}), hand({side}), hand({K_S})}) rig.Run(frame);
    };
    const auto typeSpell = [&](KEY side) {
        for (const InputFrame& frame : {hand({}), hand({K_DOWN}), hand({side}), hand({K_D})}) rig.Run(frame);
    };
    const auto quiet = [&](int ticks) {
        for (int i = 0; i < ticks; ++i) rig.Run(hand({}));
    };

    // Twice in well under a second each, with the buffer's quiet between: all four go off, the touch cooldown's clock
    // never started (nothing here is the button's macro).
    int from = rig.tick;
    typeSword(K_LEFT);
    CHECK_MSG(rig.FiredSince(from) == "sword left@4", rig.FiredSince(from));
    quiet(20);
    from = rig.tick;
    typeSword(K_LEFT);
    CHECK_MSG(rig.FiredSince(from) == "sword left@4", rig.FiredSince(from));
    CHECK_EQ(rig.fired.size(), std::size_t{2});
    if (rig.fired.size() == 2) CHECK(rig.fired[1].tick - rig.fired[0].tick < static_cast<int>(TouchControls::kComboCooldownTicks));
    quiet(20);
    from = rig.tick;
    typeSpell(K_RIGHT);
    CHECK_MSG(rig.FiredSince(from) == "spell right@3", rig.FiredSince(from));
    quiet(20);
    from = rig.tick;
    typeSpell(K_RIGHT);
    CHECK_MSG(rig.FiredSince(from) == "spell right@3", rig.FiredSince(from));
    CHECK_EQ(rig.fired.size(), std::size_t{4});
    if (rig.fired.size() == 4) CHECK(rig.fired[3].tick - rig.fired[2].tick < static_cast<int>(TouchControls::kComboCooldownTicks));
    CHECK_EQ(rig.touch.CooldownTicksLeft(TouchCombo::Sword), 0u);
    CHECK_EQ(rig.touch.CooldownTicksLeft(TouchCombo::Spell), 0u);

    // The sword button's own combo, then, a typed one while it rests: it goes off at once, and the button goes on resting.
    rig.Idle(20);
    from = rig.tick;
    rig.Tap(Facing({Finger(1, Centre(TouchControl::SwordCombo))}, TouchFacing::Left));
    for (int i = 1; i < 8; ++i) rig.Tap(Facing({}, TouchFacing::Left));
    CHECK_MSG(rig.FiredSince(from) == "sword left@4", rig.FiredSince(from));
    CHECK(rig.touch.CooldownTicksLeft(TouchCombo::Sword) > 0u);
    quiet(20);
    from = rig.tick;
    typeSword(K_LEFT);
    CHECK_MSG(rig.FiredSince(from) == "sword left@4", rig.FiredSince(from));
    CHECK(rig.touch.CooldownTicksLeft(TouchCombo::Sword) > 0u);   // still resting: the typed combo did not wait for it
    CHECK(rig.touch.CooldownTicksLeft(TouchCombo::Sword) < TouchControls::kComboCooldownTicks - 20u);
    const TouchStep refused = rig.Tap(Facing({Finger(2, Centre(TouchControl::SwordCombo))}, TouchFacing::Left));
    CHECK(refused.Denied(TouchCombo::Sword) && Only(refused, {}));
}

TouchFacing FacingOf(const ETHEntity& wizard) {
    if (wizard == nullptr) return TouchFacing::Unknown;
    return wizard->GetUIntData("currentDir") == Script::LEFT ? TouchFacing::Left : TouchFacing::Right;
}

bool Standing(const ETHEntity& e) {
    return e != nullptr && e->IsAlive() && e->GetIntData("hp") > 0 && e->CheckCustomData("deathTime") == DT_NODATA &&
           e->GetUIntData("touchingGround") != 0;
}

// The combos through the real game: level 1, a fresh campaign, the combo
// buttons tapped as the layer feeds them (facing from the wizard's own
// currentDir). SHORTCUT: mana set to the maximum before each.
void testComboInGame() {
    if (!fs::exists(fs::path(PENUMBRA_ORIGINAL_DIR) / "scenes" / "level1.esc")) {
        std::printf("  (the original is not at %s: the combos in the real game are skipped)\n", PENUMBRA_ORIGINAL_DIR);
        return;
    }
    std::error_code ec;
    const fs::path userRoot = fs::temp_directory_path(ec) / ("penumbra-touch-" + std::to_string(std::random_device{}()));
    fs::remove_all(userRoot, ec);
    fs::create_directories(userRoot, ec);
    {
        MachineConfig config;
        config.userRoot = userRoot.generic_string();
        Machine machine(config);
        Machine::Scope scope(machine);
        Script::RegisterAll(machine);
        machine.Boot(Script::ScriptMain);
        machine.Frame(InputFrame{});

        TouchControls touch;
        std::set<int> seen;
        std::vector<ETHEntity> made;
        const auto spot = [&](int since) {
            for (const char* name : {"combo_sword.ent", "sword0.ent", "combo_fire_ball.ent", "fire_ball.ent"}) {
                ETHEntityArray found;
                GetEntityArray(name, found);
                for (const ETHEntity& e : found) {
                    if (e->GetID() >= since && seen.insert(e->GetID()).second) made.push_back(e);
                }
            }
        };
        const auto count = [&](const std::string& name) {
            return static_cast<int>(std::count_if(made.begin(), made.end(),
                                                  [&](const ETHEntity& e) { return e->GetEntityName() == name; }));
        };
        int since = 0;
        const auto tick = [&](std::vector<TouchContact> contacts) {
            TouchInput input = Play(std::move(contacts), machine.GetScreenSize());
            input.facing = FacingOf(SeekEntity("bruxo.ent"));
            input.sceneSerial = machine.Snapshot().sceneSerial;
            const TouchStep step = touch.Update(input);
            InputFrame frame;
            TouchControls::ApplyToFrame(step, frame);
            touch.ObserveFrame(frame, static_cast<int>(Script::getPlayerJoystick(0)));
            machine.Frame(frame);
            spot(since);
            return step;
        };

        const uint setup = Script::g_levelStartTime;
        Script::newGame("CAMPAIGN");
        for (int i = 0; i < 5 && Script::g_levelStartTime == setup; ++i) machine.Frame(InputFrame{});
        CHECK(GetSceneFileName() == "scenes/level1.esc");
        ETHEntity wizard = SeekEntity("bruxo.ent");
        for (int i = 0; i < 300 && !Standing(wizard); ++i) {
            tick({});
            wizard = SeekEntity("bruxo.ent");
        }
        if (!Standing(wizard)) {
            CHECK_MSG(false, "no wizard standing in level 1");
            return;
        }
        for (int i = 0; i < 20; ++i) tick({});
        const glm::vec2 screen = machine.GetScreenSize();

        // E16's touch wording in the real level: he stands at the first help
        // sign, whose message is on the HUD (addMessage's line at (10,70) and
        // its echo under him), and the HUD draws it through Localization -
        // in touch wording while the controls are on, as it was when off.
        {
            using Penumbra::Render::Language;
            const std::string sign = "Utilize as setas ou as direcionais do joystick para mover-se";
            Penumbra::Render::Localization loc;
            CHECK(loc.Load());
            int shown = 0;
            for (const HudCmd& cmd : machine.Snapshot().hud) {
                if (cmd.kind != HudCmd::Kind::Text || cmd.text != sign) continue;
                ++shown;
                loc.SetTouch(true);
                CHECK(loc.Translate(cmd.text, Language::Portuguese) ==
                      "Utilize as setas no canto inferior esquerdo para mover-se");
                CHECK(loc.Translate(cmd.text, Language::English) == "Use the arrows at the bottom left to move");
                loc.SetTouch(false);
                CHECK(loc.Translate(cmd.text, Language::Portuguese) == sign);
                CHECK(loc.Translate(cmd.text, Language::English) ==
                      "Use the arrow keys or the joystick's d-pad to move");
            }
            std::printf("  level 1's first help sign: %d text commands on the HUD, drawn in touch wording\n", shown);
            CHECK(shown >= 2);   // the line and its echo
        }

        // The sword combo, the way he stands.
        wizard->AddIntData("mp", wizard->GetIntData("maxMp"));
        int mp = wizard->GetIntData("mp");
        const uint facing = wizard->GetUIntData("currentDir");
        since = GetLastID();
        tick({Finger(1, Centre(TouchControl::SwordCombo, screen))});
        for (int i = 0; i < 8; ++i) tick({});
        std::printf("  sword combo facing %s: combo_sword.ent %d, sword0.ent %d, mp %d -> %d\n",
                    facing == Script::LEFT ? "left" : "right", count("combo_sword.ent"), count("sword0.ent"), mp,
                    wizard->GetIntData("mp"));
        CHECK_EQ(count("combo_sword.ent"), 1);
        CHECK_EQ(count("sword0.ent"), 0);
        CHECK(mp - wizard->GetIntData("mp") == 5 || mp - wizard->GetIntData("mp") == 4);
        for (const ETHEntity& e : made) {
            if (e->GetEntityName() == "combo_sword.ent") CHECK_EQ(e->GetUIntData("direction"), facing);
        }

        // The spell combo facing left, tapped the tick after a step left on
        // the disc: it waits out the buffer, then casts toward the left.
        for (int i = 0; i < 30; ++i) tick({});
        wizard->AddIntData("mp", wizard->GetIntData("maxMp"));
        mp = wizard->GetIntData("mp");
        made.clear();
        since = GetLastID();
        tick({Finger(2, Centre(TouchControl::Dpad, screen) + glm::vec2(-100.0f, 0.0f))});
        tick({});
        CHECK_EQ(wizard->GetUIntData("currentDir"), Script::LEFT);
        tick({Finger(3, Centre(TouchControl::SpellCombo, screen))});
        int ticks = 0;
        for (; ticks < 30 && count("combo_fire_ball.ent") == 0; ++ticks) tick({});
        tick({});
        std::printf("  spell combo facing left: combo_fire_ball.ent %d after %d ticks, fire_ball.ent %d, mp %d -> %d\n",
                    count("combo_fire_ball.ent"), ticks, count("fire_ball.ent"), mp, wizard->GetIntData("mp"));
        CHECK_EQ(count("combo_fire_ball.ent"), 1);
        CHECK_EQ(count("fire_ball.ent"), 0);
        CHECK(ticks >= 13);   // it waited for the buffer
        CHECK(mp - wizard->GetIntData("mp") >= 24 && mp - wizard->GetIntData("mp") <= 25);
        for (const ETHEntity& e : made) {
            if (e->GetEntityName() == "combo_fire_ball.ent") CHECK_EQ(e->GetUIntData("direction"), Script::LEFT);
        }
        CHECK_EQ(machine.ScriptAborts(), 0u);
    }
    fs::remove_all(userRoot, ec);
}

// E22 through the real game, fed as the layer feeds it on a phone: each tick
// InputMapper maps one Bluetooth gamepad (E22's order, the touch controls on;
// the cursor and the scripts' warps), then the fingers go through
// TouchControls. Without the pad, Versus shows its message, which the HUD
// draws in touch wording. With it, the menu's Versus opens and the arena
// select is tapped - the pad neither moves the menu's cursor nor confirms - and
// in the arena the touchscreen moves and jumps the wizard only, the pad the
// princess only. Then level 1: the pad's Start summons the co-op princess, and
// it walks her, not him.
void testPhoneVersusWithPad() {
    if (!fs::exists(fs::path(PENUMBRA_ORIGINAL_DIR) / "scenes" / "pvp_lv1.esc")) {
        std::printf("  (the original is not at %s: Versus with a pad in the real game is skipped)\n",
                    PENUMBRA_ORIGINAL_DIR);
        return;
    }
    namespace Pad = Supersonic::Pad;
    using Penumbra::Render::Language;
    std::error_code ec;
    const fs::path userRoot = fs::temp_directory_path(ec) / ("penumbra-touch-pad-" + std::to_string(std::random_device{}()));
    fs::remove_all(userRoot, ec);
    fs::create_directories(userRoot, ec);
    {
        MachineConfig config;
        config.userRoot = userRoot.generic_string();
        Machine machine(config);
        Machine::Scope scope(machine);
        Script::RegisterAll(machine);
        machine.Boot(Script::ScriptMain);

        Penumbra::Render::ControlSettings phone = Settings::Defaults("en").controls;
        phone.keyboardPlayer2 = false;   // a phone's default (Settings.hpp)
        Penumbra::Render::InputMapper mapper;
        mapper.SetControls(phone);
        mapper.SetTouchPlaysPlayer1(true);   // what the layer's SetTouchEnabled(true) does
        TouchControls touch;
        View view;
        view.logicalScreen = kFourThree;
        view.windowPixels = glm::uvec2(1024, 768);
        view.scale = 1.0f;
        view.viewportMin = glm::vec2(0.0f);
        view.viewportMax = kFourThree;

        bool padPlugged = false;
        glm::vec2 mouse(0.0f);   // window pixels, the logical screen's at scale 1
        Penumbra::Render::RawPad restingPad;
        restingPad.gamepad = true;
        restingPad.axes[Pad::LeftTrigger] = -1.0f;
        restingPad.axes[Pad::RightTrigger] = -1.0f;
        int fingers = 100;
        const auto tick = [&](std::vector<TouchContact> contacts, std::initializer_list<int> padButtons = {}) {
            Penumbra::Render::RawDevices raw;
            raw.mouseWindow = mouse;
            if (padPlugged) {
                Penumbra::Render::RawPad pad = restingPad;
                for (const int b : padButtons) pad.buttons[static_cast<std::size_t>(b)] = true;
                raw.pads.push_back(pad);
            }
            mapper.SetPlayer2Pad(static_cast<int>(Script::getPlayerJoystick(1)));
            mapper.SetMenuMode(Penumbra::Render::IsFixedLayoutScene(machine.GetSceneFileName()));
            InputFrame frame = mapper.BuildTick(raw, view);
            const std::string& loop = machine.LoopFunction();
            const bool level = loop == "levelLoop" || loop == "pvpLoop";
            TouchInput input;
            input.contacts = std::move(contacts);
            input.screen = machine.GetScreenSize();
            input.scene = level ? TouchScene::Play : TouchScene::Menu;
            input.corner = TouchControls::CornerFor(
                Penumbra::Render::TouchScreen{machine.GetSceneFileName(), level, Script::g_gameFinished, false});
            input.sceneSerial = machine.Snapshot().sceneSerial;
            const TouchStep step = touch.Update(input);
            TouchControls::ApplyToFrame(step, frame);
            if (step.pointer) mapper.WarpCursor(step.pointerPos, view);
            touch.ObserveFrame(frame, static_cast<int>(Script::getPlayerJoystick(0)));
            machine.Frame(frame);
            vector2 warp;
            if (machine.Input().TakeCursorRequest(warp)) mapper.WarpCursor(warp, view);
            mapper.EndFrame(raw);
            return frame;
        };
        const auto tap = [&](glm::vec2 at) {
            const int id = ++fingers;
            tick({Finger(id, at)});
            tick({Finger(id, at)});
            tick({});
        };
        const auto waitScene = [&](const std::string& scene, int limit) {
            int i = 0;
            for (; i < limit && machine.GetSceneFileName() != scene; ++i) tick({});
            return machine.GetSceneFileName() == scene ? i : -1;
        };
        const auto hudText = [&](const std::string& needle) {
            for (const HudCmd& cmd : machine.Snapshot().hud) {
                if (cmd.kind == HudCmd::Kind::Text && cmd.text.find(needle) != std::string::npos) return cmd.text;
            }
            return std::string();
        };
        const glm::vec2 kVersus(466.0f, 271.0f);     // menu.esc's Versus button (test_pn_scenarios)
        const glm::vec2 kArena1(276.0f, 244.0f);     // arena_select.esc's Obelisco

        CHECK(waitScene("scenes/menu.esc", 5) >= 0);
        for (int i = 0; i < 5; ++i) tick({});

        // No pad: Versus asks for one, in touch wording (strings.json "touch").
        tap(kVersus);
        CHECK(!Script::hasASecondController());
        const std::string message = hudText("ao menos um joystick");
        std::printf("  no pad: Versus says \"%s\"\n", message.c_str());
        CHECK(message == "\xC9 necess\xE1rio ao menos um joystick\n para jogar neste modo.");
        CHECK(machine.GetSceneFileName() == "scenes/menu.esc");
        Penumbra::Render::Localization loc;
        CHECK(loc.Load());
        loc.SetTouch(true);
        CHECK(loc.Translate(message, Language::Portuguese) ==
              "Conecte um gamepad para o jogador 2.\n\nO jogador 1 joga com os\n controles de toque.");
        CHECK(loc.Translate(message, Language::English) ==
              "Connect a gamepad for player 2.\n\nPlayer 1 plays with the\n touch controls.");
        loc.SetTouch(false);   // touch off: the original's, as ever
        CHECK(loc.Translate(message, Language::Portuguese) == Penumbra::Eth::Cp1252ToUtf8(message));   // E24: UTF-8 out
        CHECK(loc.Translate(message, Language::English) == "At least one joystick is needed\n to play this mode.");

        // A pad connects: player 2's, so Versus opens.
        padPlugged = true;
        for (int i = 0; i < 3; ++i) tick({});
        CHECK(Script::hasASecondController());
        CHECK(!hudText("Escolha uma arena").empty());
        tap(kVersus);
        const int select = waitScene("scenes/arena_select.esc", 5);
        std::printf("  a pad: Versus opens the arena select (%d ticks after the tap)\n", select);
        CHECK(select >= 0);
        for (int i = 0; i < 3; ++i) tick({});
        // menu.as:74: the joystick icon for joystick 0 (player 2's), none for 1.
        int icons = 0;
        for (const HudCmd& cmd : machine.Snapshot().hud) {
            if (cmd.kind != HudCmd::Kind::Sprite || cmd.sprite.find("joystick.png") == std::string::npos) continue;
            ++icons;
            CHECK_NEAR(cmd.pos.y, 0.0f);
        }
        CHECK_EQ(icons, 1);
        // The pad neither moves the menu's cursor (player 1's axis, menu.as:239)
        // nor confirms (player 1's, E14's A included), even on a thumbnail.
        // The cursor is put there with a mouse, not a finger: a tap would be
        // the click this checks the pad never makes.
        mouse = kArena1;
        for (int i = 0; i < 3; ++i) tick({});
        CHECK(!hudText("Obelisco").empty());
        const ETHEntity cursor = SeekEntity("cursor.ent");
        CHECK(cursor != nullptr);
        if (cursor != nullptr) {
            const vector2 before = cursor->GetPositionXY();
            for (int i = 0; i < 20; ++i) tick({}, {Pad::DpadRight, Pad::DpadDown});
            for (int i = 0; i < 3; ++i) {
                tick({}, {Pad::A, Pad::Start});
                tick({});
            }
            std::printf("  the pad on the arena select: cursor (%.0f,%.0f) -> (%.0f,%.0f); confirmed %s\n", before.x,
                        before.y, cursor->GetPositionXY().x, cursor->GetPositionXY().y,
                        cursor->CheckCustomData("newGame") == DT_NODATA ? "no" : "YES");
            CHECK_NEAR(cursor->GetPositionXY().x, before.x);
            CHECK_NEAR(cursor->GetPositionXY().y, before.y);
            CHECK(cursor->CheckCustomData("newGame") == DT_NODATA);
        }
        CHECK(machine.GetSceneFileName() == "scenes/arena_select.esc");
        tap(kArena1);
        const int arena = waitScene("scenes/pvp_lv1.esc", 200);
        std::printf("  Obelisco tapped: pvp_lv1.esc %d ticks later\n", arena);
        CHECK(arena >= 0);

        ETHEntity wizard;
        ETHEntity princess;
        for (int i = 0; i < 240; ++i) {
            wizard = SeekEntity("bruxo.ent");
            princess = SeekEntity("princess.ent");
            if (Standing(wizard) && Standing(princess)) break;
            tick({});
        }
        CHECK(Standing(wizard) && Standing(princess));
        if (Standing(wizard) && Standing(princess)) {
            const glm::vec2 screen = machine.GetScreenSize();
            for (int i = 0; i < 10; ++i) tick({});
            // Each walks away from the other, so neither is pushed.
            const float wizardAway = wizard->GetPosition().x < princess->GetPosition().x ? -1.0f : 1.0f;
            const int princessAway = wizardAway < 0.0f ? Pad::DpadRight : Pad::DpadLeft;
            vector2 w0 = wizard->GetPositionXY();
            vector2 q0 = princess->GetPositionXY();
            const int thumb = ++fingers;
            for (int i = 0; i < 20; ++i) tick({Finger(thumb, Centre(TouchControl::Dpad, screen) + glm::vec2(100.0f * wizardAway, 0.0f))});
            tick({});
            std::printf("  the disc: wizard x %.1f -> %.1f, princess x %.1f -> %.1f\n", w0.x, wizard->GetPosition().x,
                        q0.x, princess->GetPosition().x);
            CHECK((wizard->GetPosition().x - w0.x) * wizardAway > 30.0f);
            CHECK(std::fabs(princess->GetPosition().x - q0.x) < 1.0f);
            for (int i = 0; i < 10; ++i) tick({});
            w0 = wizard->GetPositionXY();
            q0 = princess->GetPositionXY();
            for (int i = 0; i < 20; ++i) tick({}, {princessAway});
            tick({});
            std::printf("  the pad's d-pad: princess x %.1f -> %.1f, wizard x %.1f -> %.1f\n", q0.x,
                        princess->GetPosition().x, w0.x, wizard->GetPosition().x);
            CHECK(std::fabs(princess->GetPosition().x - q0.x) > 30.0f);
            CHECK(std::fabs(wizard->GetPosition().x - w0.x) < 1.0f);
            // Jumps: the pad's A lifts her only; the jump button him only.
            for (int i = 0; i < 20; ++i) tick({});
            w0 = wizard->GetPositionXY();
            q0 = princess->GetPositionXY();
            float wizardTop = w0.y;
            float princessTop = q0.y;
            tick({}, {Pad::A});
            for (int i = 0; i < 10; ++i) {
                tick({});
                wizardTop = std::min(wizardTop, wizard->GetPosition().y);
                princessTop = std::min(princessTop, princess->GetPosition().y);
            }
            std::printf("  the pad's A: princess up %.1f px, wizard up %.1f px\n", q0.y - princessTop, w0.y - wizardTop);
            CHECK(q0.y - princessTop > 20.0f);
            CHECK(w0.y - wizardTop < 1.0f);
            for (int i = 0; i < 60; ++i) tick({});
            w0 = wizard->GetPositionXY();
            q0 = princess->GetPositionXY();
            wizardTop = w0.y;
            princessTop = q0.y;
            const int jump = ++fingers;
            tick({Finger(jump, Centre(TouchControl::Jump, screen))});
            for (int i = 0; i < 10; ++i) {
                tick({});
                wizardTop = std::min(wizardTop, wizard->GetPosition().y);
                princessTop = std::min(princessTop, princess->GetPosition().y);
            }
            std::printf("  the jump button: wizard up %.1f px, princess up %.1f px\n", w0.y - wizardTop, q0.y - princessTop);
            CHECK(w0.y - wizardTop > 20.0f);
            CHECK(q0.y - princessTop < 1.0f);
        }

        // Level 1 with the pad: its Start summons the co-op princess
        // (controlCharacters.as:674), and its d-pad walks her.
        const uint setup = Script::g_levelStartTime;
        Script::newGame("CAMPAIGN");
        for (int i = 0; i < 5 && Script::g_levelStartTime == setup; ++i) tick({});
        CHECK(machine.GetSceneFileName() == "scenes/level1.esc");
        wizard = SeekEntity("bruxo.ent");
        for (int i = 0; i < 300 && !Standing(wizard); ++i) {
            tick({});
            wizard = SeekEntity("bruxo.ent");
        }
        CHECK(Standing(wizard));
        if (Standing(wizard)) {
            for (int i = 0; i < 20; ++i) tick({});
            wizard->AddIntData("mp", wizard->GetIntData("maxMp"));   // SHORTCUT: mana for the summon
            const int lives = Script::g_lives;
            CHECK(Script::hasASecondController());
            tick({}, {Pad::Start});
            tick({});
            princess = SeekEntity("princess.ent");
            std::printf("  level 1, the pad's Start: princess %s, lives %d -> %d\n",
                        princess != nullptr ? "summoned" : "NOT summoned", lives, Script::g_lives);
            CHECK(princess != nullptr);
            CHECK_EQ(Script::g_lives, lives - 1);
            for (int i = 0; i < 60 && princess != nullptr && !Standing(princess); ++i) tick({});
            if (princess != nullptr && Standing(princess)) {
                const vector2 w0 = wizard->GetPositionXY();
                const vector2 q0 = princess->GetPositionXY();
                for (int i = 0; i < 30; ++i) tick({}, {Pad::DpadRight});
                tick({});
                std::printf("  her d-pad: princess x %.1f -> %.1f, wizard x %.1f -> %.1f\n", q0.x,
                            princess->GetPosition().x, w0.x, wizard->GetPosition().x);
                CHECK(princess->GetPosition().x - q0.x > 50.0f);
                CHECK(std::fabs(wizard->GetPosition().x - w0.x) < 1.0f);
            }
        }
        CHECK_EQ(machine.ScriptAborts(), 0u);
    }
    fs::remove_all(userRoot, ec);
}

// E25: the controls keep their size and place on the window when the screen
// is drawn at another scale - a zoomed level (768 / zoom tall), a phone's
// larger menu: every size, offset and padding times TouchInput::unit, except
// that a control hanging from the top keeps below the run's timer, whose
// 25 px the zoom enlarges.
void testTouchUnit() {
    const TouchManifest manifest = TouchControls::DefaultManifest();
    // A 2400x1080 phone: E1's level screen, and the same at 150%.
    const glm::vec2 plain(1707.0f, 768.0f);
    const glm::vec2 zoomed(1138.0f, 512.0f);
    const float unit = 512.0f / 768.0f;
    const float px0 = 1080.0f / 768.0f;
    const float px1 = 1080.0f / 512.0f;
    const TouchLayout before = TouchControls::ComputeLayout(manifest, plain, TouchInsets{});
    const TouchLayout after = TouchControls::ComputeLayout(manifest, zoomed, TouchInsets{}, unit);
    for (int i = 0; i < kTouchControlCount; ++i) {
        const TouchControl control = static_cast<TouchControl>(i);
        const std::string what = TouchControls::ControlId(control);
        const TouchControlSpec& spec = manifest[control];
        CHECK_MSG(std::fabs(after[control].Size().x * px1 - before[control].Size().x * px0) < 0.5f, what);
        CHECK_MSG(std::fabs(after[control].Size().y * px1 - before[control].Size().y * px0) < 0.5f, what);
        const auto index = static_cast<std::size_t>(i);
        CHECK_MSG(std::fabs(after.hitPadding[index] * px1 - before.hitPadding[index] * px0) < 0.5f, what);
        const bool left = spec.anchor == TouchAnchor::TopLeft || spec.anchor == TouchAnchor::BottomLeft;
        const bool top = spec.anchor == TouchAnchor::TopLeft || spec.anchor == TouchAnchor::TopRight;
        // The same distance from the window's side, give or take the widths' rounding (2400.4 and 2400.5 px).
        const float sideBefore = left ? before[control].min.x * px0 : (plain.x - before[control].max.x) * px0;
        const float sideAfter = left ? after[control].min.x * px1 : (zoomed.x - after[control].max.x) * px1;
        CHECK_MSG(std::fabs(sideAfter - sideBefore) < 1.0f, what);
        if (!top) {
            CHECK_MSG(std::fabs((zoomed.y - after[control].max.y) * px1 - (plain.y - before[control].max.y) * px0) < 0.5f,
                      what);
        } else {
            // Below the timer's row (25 px of the zoomed screen), as far below it
            // as the unzoomed one is, scaled.
            CHECK_NEAR(after[control].min.y, TouchControls::kTimerRowHeight +
                                                 (spec.offset.y - TouchControls::kTimerRowHeight) * unit);
            CHECK_MSG(after[control].min.y > TouchControls::kTimerRowHeight, what);
        }
    }
    // A unit of 1 is the layout as it always was.
    const TouchLayout same = TouchControls::ComputeLayout(manifest, plain, TouchInsets{}, 1.0f);
    for (int i = 0; i < kTouchControlCount; ++i) {
        const TouchControl control = static_cast<TouchControl>(i);
        CHECK(same[control].min == before[control].min && same[control].max == before[control].max);
    }
    // Through Update: a finger on a control where the zoomed layout has it,
    // and the knob drawn at the unit's size.
    TouchControls touch;
    touch.SetImageRoot(PENUMBRA_DATA_DIR);
    TouchInput input = Play({Finger(1, after[TouchControl::Jump].Centre())}, zoomed);
    input.unit = unit;
    CHECK(Only(touch.Update(input), {TouchAction::Jump}));
    input = Play({Finger(2, after[TouchControl::Dpad].Centre() + glm::vec2(-60.0f, 0.0f))}, zoomed);
    input.unit = unit;
    CHECK(Only(touch.Update(input), {TouchAction::Left}));
    std::vector<HudCmd> out;
    touch.AppendOverlay(out);
    CHECK_EQ(Knobs(out), 1);
    for (const HudCmd& cmd : out) {
        if (cmd.sprite.find("dpad_knob.png") != std::string::npos) CHECK_NEAR(cmd.size.x, manifest.knobSize.x * unit);
    }
}

// E25 through the real game, on a 2400x1080 phone as the layer sets it up:
// the campaign's levels at the automatic zoom (175%, a 976x439 screen) and
// the menu at 1024x768. The camera keeps the wizard inside the smaller
// screen; the next_level door raises Script::g_nextLevelOffered while he
// stands at it and not a step before (the layer lowers it before each frame,
// as done here); the down button, laid out in the zoomed screen, shows from
// then on, its tap sends the door its down, and level 2 loads, zoomed too.
void testExitDownInGame() {
    if (!fs::exists(fs::path(PENUMBRA_ORIGINAL_DIR) / "scenes" / "level1.esc")) {
        std::printf("  (the original is not at %s: the exit in the real game is skipped)\n", PENUMBRA_ORIGINAL_DIR);
        return;
    }
    std::error_code ec;
    const fs::path userRoot = fs::temp_directory_path(ec) / ("penumbra-exit-" + std::to_string(std::random_device{}()));
    fs::remove_all(userRoot, ec);
    fs::create_directories(userRoot, ec);
    {
        const glm::uvec2 phone(2400u, 1080u);
        const float zoom = Penumbra::Render::CampaignZoom(0, true, phone, true);
        CHECK_NEAR(zoom, 1.75f);
        const vector2 levelScreen = Penumbra::Render::ZoomedScreen(phone, true, zoom);
        MachineConfig config;
        config.userRoot = userRoot.generic_string();
        config.screenSizeForScene = [&](const std::string& scene) {
            return Penumbra::Render::IsCampaignScene(scene) ? levelScreen : vector2(1024.0f, 768.0f);
        };
        Machine machine(config);
        Machine::Scope scope(machine);
        Script::RegisterAll(machine);
        machine.Boot(Script::ScriptMain);
        machine.Frame(InputFrame{});

        TouchControls touch;
        bool offered = false;
        const auto tick = [&](std::vector<TouchContact> contacts) {
            TouchInput input = Play(std::move(contacts), machine.GetScreenSize());
            input.unit = machine.GetScreenSize().y / 768.0f;
            input.nextLevelOffered = offered;
            input.sceneSerial = machine.Snapshot().sceneSerial;
            const TouchStep step = touch.Update(input);
            InputFrame frame;
            TouchControls::ApplyToFrame(step, frame);
            Script::g_nextLevelOffered = false;
            machine.Frame(frame);
            offered = Script::g_nextLevelOffered;
            return step;
        };

        CHECK(machine.GetScreenSize() == vector2(1024.0f, 768.0f));
        const uint setup = Script::g_levelStartTime;
        Script::newGame("CAMPAIGN");
        for (int i = 0; i < 5 && Script::g_levelStartTime == setup; ++i) machine.Frame(InputFrame{});
        CHECK(GetSceneFileName() == "scenes/level1.esc");
        CHECK(machine.GetScreenSize() == vector2(976.0f, 439.0f));
        CHECK(GetScreenSize() == vector2(976.0f, 439.0f));   // what the scripts see
        ETHEntity wizard = SeekEntity("bruxo.ent");
        for (int i = 0; i < 300 && !Standing(wizard); ++i) {
            tick({});
            wizard = SeekEntity("bruxo.ent");
        }
        if (!Standing(wizard)) {
            CHECK_MSG(false, "no wizard standing in level 1");
            return;
        }
        // The camera's dead zone in the zoomed screen: he is inside it, walking right.
        const TouchLayout layout = TouchControls::ComputeLayout(TouchControls::DefaultManifest(), levelScreen,
                                                                TouchInsets{}, levelScreen.y / 768.0f);
        const glm::vec2 right = layout[TouchControl::Dpad].Centre() + glm::vec2(80.0f, 0.0f);
        bool inside = true;
        bool anyOffer = false;
        for (int i = 0; i < 90; ++i) {
            tick({Finger(1, right)});
            const vector2 onScreen = wizard->GetPositionXY() - GetCameraPos();
            inside = inside && onScreen.x > 0.0f && onScreen.x < levelScreen.x && onScreen.y > 0.0f &&
                     onScreen.y < levelScreen.y;
            anyOffer = anyOffer || offered;
        }
        tick({});
        std::printf("  E25 level 1 at 175%%: the screen %.0fx%.0f, the wizard at (%.0f, %.0f) on it after a walk\n",
                    GetScreenSize().x, GetScreenSize().y, (wizard->GetPositionXY() - GetCameraPos()).x,
                    (wizard->GetPositionXY() - GetCameraPos()).y);
        CHECK(inside);
        CHECK(!anyOffer);
        CHECK(!touch.Visible(TouchControl::ExitDown));

        // A step short of the door (176 px): no offer.
        wizard->SetPositionXY(vector2(1960.0f, -2010.0f));
        for (int i = 0; i < 40; ++i) {
            tick({});
            anyOffer = anyOffer || offered;
        }
        CHECK(!anyOffer);
        CHECK(!touch.Visible(TouchControl::ExitDown));

        // At the door (level1.esc: next_level at (2136, -1991)).
        wizard->SetPositionXY(vector2(2136.0f, -2010.0f));
        int ticks = 0;
        for (; ticks < 60 && !offered; ++ticks) tick({});
        ETHEntity door = SeekEntity("next_level");
        const float distance = door != nullptr ? glm::length(wizard->GetPositionXY() - door->GetPositionXY()) : -1.0f;
        std::printf("  E25 at the door: offered after %d ticks, %.0f px from it\n", ticks, distance);
        CHECK(offered);
        CHECK(distance >= 0.0f && distance < 80.0f);
        tick({});
        CHECK(touch.Visible(TouchControl::ExitDown));
        CHECK(touch.Layout()[TouchControl::ExitDown].min == layout[TouchControl::ExitDown].min);
        const TouchStep tapped = tick({Finger(2, layout[TouchControl::ExitDown].Centre())});
        CHECK(Only(tapped, {TouchAction::Down}));
        // The door's fade has started: no offer any more, the button gone.
        tick({Finger(2, layout[TouchControl::ExitDown].Centre())});
        CHECK(!offered);
        tick({});
        CHECK(!touch.Visible(TouchControl::ExitDown));
        int loaded = 0;
        for (; loaded < 600 && GetSceneFileName() != "scenes/level2.esc"; ++loaded) tick({});
        std::printf("  E25 the down button tapped: %s after %d ticks, the screen %.0fx%.0f\n", GetSceneFileName().c_str(),
                    loaded, GetScreenSize().x, GetScreenSize().y);
        CHECK(GetSceneFileName() == "scenes/level2.esc");
        CHECK(machine.GetScreenSize() == vector2(976.0f, 439.0f));
        tick({});
        CHECK(!offered);
        CHECK_EQ(machine.ScriptAborts(), 0u);
    }
    fs::remove_all(userRoot, ec);
}

bool Near(float a, float b, float eps = 0.01f) { return std::fabs(a - b) <= eps; }

// E26: the pause button hangs from the HUD frame's top-right corner, as the
// run's timer does (setupScene.as:337's (width - 50, 0), moved in by the
// frame), and stays below it; nothing else moves with the frame. The message
// rule's pause column (PhoneUi.hpp's kPauseColumn) is this manifest's.
void testPauseInHudFrame() {
    const TouchManifest manifest = TouchControls::DefaultManifest();
    const TouchControlSpec& pause = manifest[TouchControl::Pause];
    CHECK(pause.anchor == TouchAnchor::TopRight);
    CHECK_NEAR(pause.offset.x + pause.size.x, Penumbra::Render::kPauseColumn);
    for (const glm::vec2 screen : {glm::vec2(1707.0f, 768.0f), glm::vec2(1138.0f, 512.0f), glm::vec2(929.0f, 697.0f)}) {
        const float unit = screen.y / 768.0f;
        const TouchInsets frame{40.0f, 18.0f, 40.0f, 0.0f};
        const TouchLayout plain = TouchControls::ComputeLayout(manifest, screen, TouchInsets{}, unit);
        const TouchLayout framed = TouchControls::ComputeLayout(manifest, screen, TouchInsets{}, unit, frame);
        const std::string what = std::to_string(screen.x) + "x" + std::to_string(screen.y);
        // In step: moved by the frame's right and top inset, its size kept.
        CHECK_MSG(Near(framed[TouchControl::Pause].max.x, plain[TouchControl::Pause].max.x - frame.right), what);
        CHECK_MSG(Near(framed[TouchControl::Pause].min.y, plain[TouchControl::Pause].min.y + frame.top), what);
        CHECK_MSG(framed[TouchControl::Pause].Size() == plain[TouchControl::Pause].Size(), what);
        // Below the moved timer's row, and right of its left edge.
        const glm::vec2 timer(screen.x - 50.0f - frame.right, frame.top);
        CHECK_MSG(framed[TouchControl::Pause].min.y > timer.y + TouchControls::kTimerRowHeight - 0.01f, what);
        CHECK_MSG(framed[TouchControl::Pause].max.x > timer.x, what);
        // The message rule's column: the button's left edge, kPauseColumn at the unit in from the frame.
        CHECK_MSG(Near(framed[TouchControl::Pause].min.x,
                       screen.x - frame.right - Penumbra::Render::kPauseColumn * unit, 0.01f),
                  what);
        // The rest where the safe area puts them.
        for (int i = 0; i < kTouchControlCount; ++i) {
            const TouchControl control = static_cast<TouchControl>(i);
            if (control == TouchControl::Pause) continue;
            CHECK_MSG(framed[control].min == plain[control].min && framed[control].max == plain[control].max,
                      what + " " + TouchControls::ControlId(control));
        }
        // A safe area further in than the frame still wins.
        const TouchInsets notch{0.0f, 0.0f, 90.0f, 0.0f};
        const TouchLayout both = TouchControls::ComputeLayout(manifest, screen, notch, unit, frame);
        CHECK_MSG(Near(both[TouchControl::Pause].max.x, plain[TouchControl::Pause].max.x - 90.0f), what);
    }
    // Through Update: the frame comes with the input, and the zero frame is the layout as it was.
    TouchControls touch;
    TouchInput input = Play({}, glm::vec2(1138.0f, 512.0f));
    input.unit = 512.0f / 768.0f;
    touch.Update(input);
    const TouchLayout::Box before = touch.Layout()[TouchControl::Pause];
    input.hudFrame = TouchInsets{40.0f, 18.0f, 40.0f, 0.0f};
    touch.Update(input);
    CHECK(Near(touch.Layout()[TouchControl::Pause].max.x, before.max.x - 40.0f));
    CHECK(Near(touch.Layout()[TouchControl::Pause].min.y, before.min.y + 18.0f));
}

// A booted game at level 1 on a 2400x1080 phone, the campaign's scenes at the
// automatic zoom: the harness the in-game E25 and E26 checks share. `run`
// gets the machine once the wizard stands.
template <typename Run>
void InLevelOne(const char* what, Run run) {
    if (!fs::exists(fs::path(PENUMBRA_ORIGINAL_DIR) / "scenes" / "level1.esc")) {
        std::printf("  (the original is not at %s: %s is skipped)\n", PENUMBRA_ORIGINAL_DIR, what);
        return;
    }
    std::error_code ec;
    const fs::path userRoot = fs::temp_directory_path(ec) / ("penumbra-l1-" + std::to_string(std::random_device{}()));
    fs::remove_all(userRoot, ec);
    fs::create_directories(userRoot, ec);
    {
        const glm::uvec2 phone(2400u, 1080u);
        const vector2 levelScreen =
            Penumbra::Render::ZoomedScreen(phone, true, Penumbra::Render::CampaignZoom(0, true, phone, true));
        MachineConfig config;
        config.userRoot = userRoot.generic_string();
        config.screenSizeForScene = [&](const std::string& scene) {
            return Penumbra::Render::IsCampaignScene(scene) ? levelScreen : vector2(1024.0f, 768.0f);
        };
        Machine machine(config);
        Machine::Scope scope(machine);
        Script::RegisterAll(machine);
        machine.Boot(Script::ScriptMain);
        machine.Frame(InputFrame{});
        const uint setup = Script::g_levelStartTime;
        Script::newGame("CAMPAIGN");
        for (int i = 0; i < 5 && Script::g_levelStartTime == setup; ++i) machine.Frame(InputFrame{});
        ETHEntity wizard = SeekEntity("bruxo.ent");
        for (int i = 0; i < 300 && !Standing(wizard); ++i) {
            machine.Frame(InputFrame{});
            wizard = SeekEntity("bruxo.ent");
        }
        CHECK(GetSceneFileName() == "scenes/level1.esc");
        CHECK(machine.GetScreenSize() == vector2(976.0f, 439.0f));
        if (!Standing(wizard)) {
            CHECK_MSG(false, std::string("no wizard standing in level 1: ") + what);
        } else {
            run(machine, wizard, phone);
        }
        CHECK_EQ(machine.ScriptAborts(), 0u);
    }
    Script::g_touchHud = Script::TouchHud{};
    fs::remove_all(userRoot, ec);
}

// E25 with a second player: the princess summoned into a zoomed level 1 takes
// the scene back to E1's screen (CampaignUnzooms, applied after each frame as
// the layer applies it) for as long as the scene lasts, so the leash that
// kills her 3 s off screen (controlCharacters.as:342-360) is E1's. Summoned
// beside the wizard, then held 540 px below him - the camera keeps their
// midpoint in the screen's middle 40%, so a 512-tall screen's leash reaches
// 0.3 x 512 + 76 = 230 px past it and a 768-tall one's 306 - she lives
// through 5 s at E1's screen, and dies at the zoomed one without the rule.
void testCoopUnzoomsInGame() {
    const auto coop = [](const bool rule, bool& alive, vector2& screen, int& unzoomedAt) {
        InLevelOne("the co-op zoom", [&](Machine& machine, ETHEntity wizard, glm::uvec2 phone) {
            const vector2 plain = Penumbra::Render::ZoomedScreen(phone, true, 1.0f);
            // Summoned beside him (controlCharacters.as:700-705), on screen,
            // then held below him.
            const vector2 beside(48.0f, -6.0f);
            ETHEntity princess;
            AddEntity(Script::MAIN_CHARACTER_ENTITY1, vector3(wizard->GetPositionXY() + beside, 0.0f), princess);
            CHECK(princess != nullptr);
            unzoomedAt = -1;
            for (int tick = 0; tick < 330; ++tick) {
                princess = SeekEntity(Script::MAIN_CHARACTER_ENTITY1);
                if (princess == nullptr) break;
                princess->SetPositionXY(wizard->GetPositionXY() + (tick < 30 ? beside : vector2(0.0f, 540.0f)));
                princess->AddFloatData("forceX", 0.0f);
                princess->AddFloatData("forceY", 0.0f);
                machine.Frame(InputFrame{});
                if (rule && Penumbra::Render::CampaignUnzooms(GetSceneFileName(), machine.GetScreenSize(),
                                                              Script::g_gameFinished,
                                                              SeekEntity(Script::MAIN_CHARACTER_ENTITY1) != nullptr)) {
                    machine.SetScreenSize(plain);
                    if (unzoomedAt < 0) unzoomedAt = tick;
                }
            }
            princess = SeekEntity(Script::MAIN_CHARACTER_ENTITY1);
            alive = princess != nullptr && princess->GetIntData("hp") > 0;
            screen = machine.GetScreenSize();
        });
    };
    bool alive = false;
    vector2 screen;
    int unzoomedAt = -1;
    coop(true, alive, screen, unzoomedAt);
    std::printf("  E25 co-op: the princess 540 px below, E1's screen from tick %d: %s after 5 s, the screen %.0fx%.0f\n",
                unzoomedAt, alive ? "alive" : "dead", screen.x, screen.y);
    CHECK_EQ(unzoomedAt, 0);                                  // the first frame she is in
    CHECK(screen == vector2(1707.0f, 768.0f));                // and never zoomed again in the scene
    CHECK(alive);
    // Without the rule (E25 before this), the zoomed screen's leash.
    coop(false, alive, screen, unzoomedAt);
    std::printf("  E25 co-op without the rule: %s after 5 s at %.0fx%.0f\n", alive ? "alive" : "dead", screen.x,
                screen.y);
    CHECK(screen == vector2(976.0f, 439.0f));
    CHECK(!alive);
}

// E26 through the real game: the HUD's commands in level 1, drawn by the
// ported scripts. With Script::g_touchHud at rest (the desktop) they are the
// original's, field for field: the bars from (0, 0), each value in the bars'
// dark colour at the bar's end less 45 (27 for lv), sliding off the left edge
// as the bar empties; the lives at (448, 0); the timer at (width - 50, 0); the
// message lines from (10, 70). With a frame (the touch controls on) all of
// those, and nothing else, move by it - the timer by its top-right corner -
// and with `on` each value is the HUD's light text over the lives counter's
// dark shadow, never left of its bar's left end.
struct HudDrawn {
    std::vector<HudCmd> bars;       // interface/*.png, the values, the lives
    std::vector<HudCmd> values;     // "hp: ", "mp: ", "lv: " texts
    std::vector<HudCmd> timer;      // "m:ss"
    std::vector<HudCmd> messages;   // Arial 30
    std::vector<HudCmd> rest;
};

HudDrawn SortHud(const std::vector<HudCmd>& hud) {
    HudDrawn out;
    for (const HudCmd& cmd : hud) {
        const bool text = cmd.kind == HudCmd::Kind::Text;
        const bool value = text && (cmd.text.rfind("hp: ", 0) == 0 || cmd.text.rfind("mp: ", 0) == 0 ||
                                    cmd.text.rfind("lv: ", 0) == 0);
        const bool timer = text && cmd.font == "Arial Narrow" && cmd.fontSize == 25.0f &&
                           cmd.text.find(':') != std::string::npos && cmd.text.size() <= 6;
        if (value) out.values.push_back(cmd);
        if (value || (!text && cmd.sprite.rfind("interface/", 0) == 0) || (text && cmd.font == "Arial Black")) {
            out.bars.push_back(cmd);
        } else if (timer) {
            out.timer.push_back(cmd);
        } else if (text && cmd.font == "Arial" && cmd.fontSize == 30.0f) {
            out.messages.push_back(cmd);
        } else {
            out.rest.push_back(cmd);
        }
    }
    return out;
}

void testTouchHudInGame() {
    InLevelOne("the HUD's frame", [](Machine& machine, ETHEntity wizard, glm::uvec2) {
        const vector2 screen = machine.GetScreenSize();
        // A help sign's message is up in the first seconds of level 1; hp and
        // mp full, xp 0.
        // Two frames each: an entity callback's draws (the bars, from the
        // wizard's) reach the snapshot a frame after the loop's (the timer).
        const auto frameWith = [&](const Script::TouchHud& hud) {
            Script::g_touchHud = hud;
            machine.Frame(InputFrame{});
            machine.Frame(InputFrame{});
            return SortHud(machine.Snapshot().hud);
        };
        Script::g_messages.addMessage("Checkpoint...");
        const HudDrawn plain = frameWith(Script::TouchHud{});
        CHECK(!plain.messages.empty());
        CHECK(!plain.bars.empty() && plain.values.size() == 3u && plain.timer.size() == 2u);
        // The original's own: the values' colour and places.
        const int hp = wizard->GetIntData("hp");
        const int maxHp = wizard->GetIntData("maxHp");
        for (const HudCmd& cmd : plain.values) CHECK(cmd.color == 0xD0000000u && cmd.fontSize == 16.0f);
        for (const HudCmd& cmd : plain.values) {
            if (cmd.text.rfind("hp: ", 0) != 0) continue;
            CHECK_NEAR(cmd.pos.x, static_cast<float>(hp) / static_cast<float>(maxHp) * 200.0f - 45.0f);
            CHECK_NEAR(cmd.pos.y, 0.0f);
        }
        for (const HudCmd& cmd : plain.values) {
            if (cmd.text.rfind("lv: ", 0) == 0) CHECK_NEAR(cmd.pos.x, -27.0f);   // at 0 xp: off the left edge
        }
        CHECK_NEAR(plain.timer.back().pos.x, screen.x - 50.0f);
        CHECK_NEAR(plain.timer.back().pos.y, 0.0f);
        for (const HudCmd& cmd : plain.messages) CHECK(cmd.pos.x == 10.0f || cmd.pos.x == 13.0f);   // + its shadow

        // The frame alone: moved, nothing restyled.
        const Script::TouchHud framedOnly{false, 40.0f, 18.0f, 30.0f};
        const HudDrawn framed = frameWith(framedOnly);
        CHECK_EQ(framed.bars.size(), plain.bars.size());
        for (std::size_t i = 0; i < std::min(framed.bars.size(), plain.bars.size()); ++i) {
            const HudCmd& a = plain.bars[i];
            const HudCmd& b = framed.bars[i];
            CHECK_MSG(a.kind == b.kind && a.sprite == b.sprite && a.text == b.text && a.color == b.color &&
                          a.size == b.size,
                      b.sprite + b.text);
            CHECK_MSG(Near(b.pos.x, a.pos.x + 40.0f) && Near(b.pos.y, a.pos.y + 18.0f), b.sprite + b.text);
        }
        CHECK_EQ(framed.timer.size(), 2u);
        if (framed.timer.size() == 2u) {
            CHECK_NEAR(framed.timer.back().pos.x, screen.x - 50.0f - 30.0f);
            CHECK_NEAR(framed.timer.back().pos.y, 18.0f);
        }
        for (const HudCmd& cmd : framed.messages) CHECK(cmd.pos.x == 50.0f || cmd.pos.x == 53.0f);

        // The touch style: two texts a value, light over the dark shadow.
        const HudDrawn styled = frameWith(Script::TouchHud{true, 0.0f, 0.0f, 0.0f});
        CHECK_EQ(styled.values.size(), 6u);
        for (const HudCmd& cmd : styled.values) {
            CHECK(cmd.color == 0xF0000000u || cmd.color == ARGB(255, 203, 203, 228));
            CHECK(cmd.fontSize == 16.0f && cmd.font == "Arial Narrow");
        }
        // Low hp and no xp: each value inside its bar's left end, on touch only.
        wizard->AddIntData("hp", 10);
        const HudDrawn low = frameWith(Script::TouchHud{true, 40.0f, 18.0f, 30.0f});
        for (const HudCmd& cmd : low.values) {
            CHECK_MSG(cmd.pos.x >= 40.0f + 2.0f - 0.01f, cmd.text);
            if (cmd.color == ARGB(255, 203, 203, 228) && cmd.text.rfind("lv: ", 0) == 0) CHECK_NEAR(cmd.pos.x, 42.0f);
        }
        const HudDrawn lowPlain = frameWith(Script::TouchHud{});
        for (const HudCmd& cmd : lowPlain.values) {
            if (cmd.text.rfind("hp: ", 0) == 0) CHECK_NEAR(cmd.pos.x, 10.0f / static_cast<float>(maxHp) * 200.0f - 45.0f);
        }
        wizard->AddIntData("hp", hp);

        // The loading message (the exit door's fade, a death's) from the
        // frame's bottom-left corner, at (20, height - 70) without one.
        const auto loading = [&](const Script::TouchHud& hud) {
            Script::g_touchHud = hud;
            Script::loadingMessage();
            machine.Frame(InputFrame{});
            vector2 at(-1.0f);
            for (const HudCmd& cmd : machine.Snapshot().hud) {
                if (cmd.kind == HudCmd::Kind::Text && cmd.text.rfind("Carregando", 0) == 0 && cmd.color >> 24 == 255u) {
                    at = cmd.pos;
                }
            }
            return at;
        };
        CHECK(loading(Script::TouchHud{}) == vector2(20.0f, screen.y - 70.0f));
        CHECK(loading(Script::TouchHud{true, 40.0f, 18.0f, 30.0f, 12.0f}) == vector2(60.0f, screen.y - 82.0f));
    });
}

// ============================================================================================================
// ENHANCEMENT E28: the player's own layout laid over the manifest (TouchControls::WithTuning, MoveFor,
// GrabBox, SetTuning, TouchScene::Edit). Every number below was measured with the real ComputeLayout.
// ============================================================================================================

TouchMove Mv(float x, float y) { return TouchMove{x, y}; }

constexpr std::size_t Slot(TouchControl control) { return static_cast<std::size_t>(control); }

TouchTuning SizedAt(float size, float opacity = 1.0f) {
    TouchTuning tuning;
    tuning.size = size;
    tuning.opacity = opacity;
    return tuning;
}

TouchTuning WithMove(TouchControl control, float x, float y, float size = 1.0f) {
    TouchTuning tuning = SizedAt(size);
    tuning.move[Slot(control)] = TouchMove{x, y};
    return tuning;
}

// The built-in manifest's layout with `tuning` over it.
TouchLayout Tuned(const TouchTuning& tuning, glm::vec2 screen = kFourThree, TouchInsets safe = {}, float unit = 1.0f,
                  TouchInsets frame = {}) {
    return TouchControls::ComputeLayout(TouchControls::WithTuning(TouchControls::DefaultManifest(), tuning), screen,
                                        safe, unit, frame);
}

bool BoxIs(const TouchLayout::Box& box, glm::vec2 min, glm::vec2 max, float eps = 0.01f) {
    return Near(box.min.x, min.x, eps) && Near(box.min.y, min.y, eps) && Near(box.max.x, max.x, eps) &&
           Near(box.max.y, max.y, eps);
}

bool SameBox(const TouchLayout::Box& a, const TouchLayout::Box& b) { return a.min == b.min && a.max == b.max; }

// Every box and every padding, exactly.
bool SameLayout(const TouchLayout& a, const TouchLayout& b) {
    for (int i = 0; i < kTouchControlCount; ++i) {
        const auto index = static_cast<std::size_t>(i);
        if (!SameBox(a.boxes[index], b.boxes[index]) || a.hitPadding[index] != b.hitPadding[index]) return false;
    }
    return true;
}

TouchInput Edit(std::vector<TouchContact> contacts = {}) {
    TouchInput input;
    input.contacts = std::move(contacts);
    input.screen = kFourThree;
    input.scene = TouchScene::Edit;
    input.corner = TouchCorner::Hidden;
    return input;
}

constexpr TouchControl kMovable[] = {TouchControl::Dpad,       TouchControl::Jump,       TouchControl::Sword,
                                     TouchControl::Fire,       TouchControl::Light,      TouchControl::SwordCombo,
                                     TouchControl::SpellCombo, TouchControl::ExitDown,   TouchControl::Pause};

// The default layout at 4:3, exactly: every number is whole or a half, so a float holds it, and any change in
// the arithmetic ComputeLayout does (E28 split it into BoundsFor and PlaceControl) shows here as a difference. Then
// the identity: no tuning is the manifest, bit for bit, and its layout is the layout.
void testTuningIdentity() {
    // E34: the number of the arrangement this table is. A saved move is a delta from these places, and the settings file
    // keeps the moves only when it was written under this number (TouchTuning::kLayoutVersion): whoever changes a place
    // in the table, or the offset of the direction control, the down button or the pause button behind it, changes the
    // number with it, here and in TouchTuning.hpp, or a player's saved moves are laid on places they were not made for.
    CHECK_EQ(TouchTuning::kLayoutVersion, 4);   // 1 E29, 2 E32, 3 an E33 build with the left column higher (never released), 4 E33
    const TouchManifest built = TouchControls::DefaultManifest();
    struct Exact {
        TouchControl control;
        glm::vec2 min;
        glm::vec2 max;
    };
    const Exact table[] = {
        {TouchControl::Dpad, {24.0f, 506.0f}, {424.0f, 906.0f}},
        {TouchControl::Jump, {864.0f, 584.0f}, {984.0f, 704.0f}},   // E33: two staggered columns of three, the right one higher
        {TouchControl::Sword, {728.0f, 614.0f}, {848.0f, 734.0f}},   // E33
        {TouchControl::Fire, {864.0f, 332.0f}, {984.0f, 452.0f}},   // E33
        {TouchControl::Light, {728.0f, 362.0f}, {848.0f, 482.0f}},   // E33
        {TouchControl::SwordCombo, {738.0f, 498.0f}, {838.0f, 598.0f}},   // E33
        {TouchControl::SpellCombo, {874.0f, 468.0f}, {974.0f, 568.0f}},   // E33
        {TouchControl::ExitDown, {161.0f, 482.0f}, {287.0f, 608.0f}},
        {TouchControl::Pause, {908.0f, 44.0f}, {1004.0f, 108.5f}},
        {TouchControl::Back, {908.0f, 44.0f}, {1004.0f, 140.0f}},
    };
    const TouchLayout layout = TouchControls::ComputeLayout(built, kFourThree, TouchInsets{});
    for (const Exact& entry : table) {
        CHECK_MSG(layout[entry.control].min == entry.min && layout[entry.control].max == entry.max,
                  TouchControls::ControlId(entry.control));
    }

    std::string warning;
    const std::pair<const char*, TouchManifest> manifests[] = {
        {"built in", built},
        {"shipped", TouchControls::LoadManifest(ShippedManifest(), &warning)},
        {"placeholder", TouchControls::LoadManifest(PlaceholderManifest(), &warning)},
    };
    CHECK_MSG(warning.empty(), warning);
    TouchTuning offGrid;   // what Clamped() takes for the default: still the default
    offGrid.size = 1.04f;
    offGrid.opacity = 1.05f;
    offGrid.move[Slot(TouchControl::Jump)] = Mv(0.04f, -0.04f);
    offGrid.move[Slot(TouchControl::Back)] = Mv(5.0f, 5.0f);
    const glm::vec2 screens[] = {kFourThree, kWide, {1707.0f, 768.0f}};
    const TouchInsets insets[] = {{}, {88.0f, 0.0f, 88.0f, 24.0f}, {0.0f, 30.0f, 0.0f, 20.0f}};
    for (const auto& [name, manifest] : manifests) {
        CHECK_MSG(TouchControls::WithTuning(manifest, TouchTuning{}) == manifest, name);
        CHECK_MSG(TouchControls::WithTuning(manifest, offGrid) == manifest, name);
        const TouchManifest same = TouchControls::WithTuning(manifest, TouchTuning{});
        for (const glm::vec2& screen : screens) {
            for (const TouchInsets& safe : insets) {
                for (const float unit : {1.0f, 512.0f / 768.0f}) {
                    const TouchInsets frame = unit < 1.0f ? TouchInsets{40.0f, 18.0f, 40.0f, 0.0f} : TouchInsets{};
                    CHECK_MSG(SameLayout(TouchControls::ComputeLayout(same, screen, safe, unit, frame),
                                         TouchControls::ComputeLayout(manifest, screen, safe, unit, frame)),
                              name);
                }
            }
        }
    }

    // The same through the class: the base is what SetManifest was given, the manifest in use is the base with
    // the tuning over it, and either is the other while the tuning is the default.
    TouchControls touch;
    CHECK(touch.Manifest() == built && touch.BaseManifest() == built);   // before anyone set anything
    TouchManifest other = built;
    other.idleAlpha = 0.3f;
    touch.SetManifest(other);
    CHECK(touch.Manifest() == other && touch.BaseManifest() == other);
    touch.SetTuning(TouchTuning{});
    CHECK(touch.Manifest() == other);
    TouchTuning big = SizedAt(1.2f, 0.6f);
    touch.SetTuning(big);
    CHECK(touch.Tuning() == big);
    CHECK(touch.BaseManifest() == other && !(touch.Manifest() == other));
    CHECK(touch.Manifest() == TouchControls::WithTuning(other, big));
    touch.SetManifest(built);   // a new base keeps the player's tuning
    CHECK(touch.BaseManifest() == built && touch.Manifest() == TouchControls::WithTuning(built, big));
    touch.SetTuning(SizedAt(1.07f));   // held on the grid
    CHECK(touch.Tuning().size == 1.1f);
    touch.SetTuning(TouchTuning{});
    CHECK(touch.Manifest() == built);
}

// Size: one factor for every control but the Pause and Back, which keep the manifest's. Every size and
// distance and reach grows from the corner the control hangs from, as `scale` does.
void testTuningSize() {
    const TouchManifest built = TouchControls::DefaultManifest();
    const TouchLayout plain = Default();
    struct Row {
        TouchControl control;
        glm::vec2 min;
        glm::vec2 max;
    };
    // E32, E33: the six action buttons hang from the bottom right, so at a size a box's right and bottom edges are the
    // manifest's offset, scaled, in from the 4:3 screen's, and its size is the manifest's, scaled (the exact numbers
    // at size 1 are testTuningIdentity's table, the ones the grid is pinned to).
    const auto action = [&](TouchControl control, float size) {
        const TouchControlSpec& spec = built[control];
        const glm::vec2 max = kFourThree - spec.offset * size;
        return Row{control, max - spec.size * size, max};
    };
    const auto check = [&](float size, std::initializer_list<Row> rows) {
        const TouchLayout layout = Tuned(SizedAt(size));
        for (const Row& row : rows) {
            CHECK_MSG(BoxIs(layout[row.control], row.min, row.max),
                      std::string(TouchControls::ControlId(row.control)) + " at " + std::to_string(size));
        }
        for (const TouchControl control : {TouchControl::Jump, TouchControl::Sword, TouchControl::Fire, TouchControl::Light,
                                           TouchControl::SwordCombo, TouchControl::SpellCombo}) {   // E32
            const Row row = action(control, size);
            CHECK_MSG(BoxIs(layout[row.control], row.min, row.max),
                      std::string(TouchControls::ControlId(row.control)) + " at " + std::to_string(size));
        }
        // The Pause and Back are exactly what the manifest says, at every size.
        CHECK(SameBox(layout[TouchControl::Pause], plain[TouchControl::Pause]));
        CHECK(SameBox(layout[TouchControl::Back], plain[TouchControl::Back]));
    };
    check(1.2f, {{TouchControl::Dpad, {28.8f, 453.6f}, {508.8f, 933.6f}},
                 {TouchControl::ExitDown, {193.2f, 424.8f}, {344.4f, 576.0f}}});
    check(1.4f, {{TouchControl::Dpad, {33.6f, 401.2f}, {593.6f, 961.2f}},   // MR's ceiling
                 {TouchControl::ExitDown, {225.4f, 367.6f}, {401.8f, 544.0f}}});
    check(0.5f, {{TouchControl::Dpad, {12.0f, 637.0f}, {212.0f, 837.0f}},
                 {TouchControl::ExitDown, {80.5f, 625.0f}, {143.5f, 688.0f}}});
    check(0.4f, {});   // the floor
    // Wherever the controls hang from, they shrink toward that corner and stay on the screen.
    for (const float size : {0.4f, 0.7f, 1.0f, 1.3f, 1.4f}) {
        const TouchLayout layout = Tuned(SizedAt(size));
        for (const TouchControl control : kMovable) {
            const glm::vec2 expected = built[control].size * size;
            if (TouchControls::SizeExempt(control)) CHECK(layout[control].Size() == built[control].size);
            else CHECK_MSG(BoxIs(layout[control], layout[control].min, layout[control].min + expected, 0.001f),
                           TouchControls::ControlId(control));
        }
    }
    CHECK(TouchControls::SizeExempt(TouchControl::Pause) && TouchControls::SizeExempt(TouchControl::Back));
    CHECK(!TouchControls::SizeExempt(TouchControl::Jump) && !TouchControls::SizeExempt(TouchControl::ExitDown));

    // The reach and the knob grow with the control; `scale` is the manifest's own and never moves.
    TouchManifest half = TouchControls::WithTuning(built, SizedAt(0.5f));
    CHECK(half.scale == built.scale);
    CHECK(half.knobSize == glm::vec2(64.0f, 64.0f));
    CHECK_NEAR(half[TouchControl::Jump].hitPadding, 2.0f);
    CHECK_NEAR(half[TouchControl::Dpad].hitPadding, 15.0f);
    CHECK(half[TouchControl::Dpad].overhang == glm::vec2(0.0f, 75.0f));
    CHECK(half[TouchControl::Pause] == built[TouchControl::Pause]);
    CHECK(half[TouchControl::Back] == built[TouchControl::Back]);
    CHECK(half.idleAlpha == built.idleAlpha && half.pressedAlpha == built.pressedAlpha);
    CHECK_NEAR(Tuned(SizedAt(0.5f)).hitPadding[Slot(TouchControl::Jump)], 2.0f);
    // The placeholder look scales as well: it is a transform of whatever manifest was loaded.
    std::string warning;
    const TouchManifest placeholder = TouchControls::LoadManifest(PlaceholderManifest(), &warning);
    const TouchLayout before = TouchControls::ComputeLayout(placeholder, kFourThree, TouchInsets{});
    const TouchLayout after =
        TouchControls::ComputeLayout(TouchControls::WithTuning(placeholder, SizedAt(0.5f)), kFourThree, TouchInsets{});
    CHECK_NEAR(after[TouchControl::Jump].Size().x, before[TouchControl::Jump].Size().x * 0.5f);

    // The hit areas follow the drawn ones: at 0.5 the fire button is where it is drawn, not where it was. (E29, E32, E33: the
    // fire button, whose size-1 centre, (924, 392), is open ground at 0.5: nothing else is drawn or padded there.)   // E33
    TouchControls shrunk;
    shrunk.SetTuning(SizedAt(0.5f));
    const glm::vec2 oldCentre = Centre(TouchControl::Fire);   // (924, 392), inside the 120 px button of size 1   // E33
    const glm::vec2 newCentre = Tuned(SizedAt(0.5f))[TouchControl::Fire].Centre();   // (974, 580)   // E33
    CHECK(Only(shrunk.Update(Play({Finger(1, oldCentre)})), {}));
    CHECK(shrunk.RunningCombo() == TouchCombo::None);   // E29: nothing else was there either
    TouchControls shrunkAgain;
    shrunkAgain.SetTuning(SizedAt(0.5f));
    CHECK(Only(shrunkAgain.Update(Play({Finger(1, newCentre)})), {TouchAction::Fire}));   // E29
    TouchControls shrunkJump;   // E33: and the jump button's new centre, (974, 706), is the jump button's alone
    shrunkJump.SetTuning(SizedAt(0.5f));   // E29
    CHECK(Only(shrunkJump.Update(Play({Finger(1, Tuned(SizedAt(0.5f))[TouchControl::Jump].Centre())})), {TouchAction::Jump}));   // E29
    // And a control drawn larger is held where the old one was not.
    TouchControls grown;
    grown.SetTuning(SizedAt(1.4f));
    // E33: above the size-1 fire button and its padding (y 332 less 4), inside the 1.4 one (y 157.6 to 325.6).
    const glm::vec2 beyond(oldCentre.x, Default()[TouchControl::Fire].min.y - 20.0f);
    CHECK(Only(grown.Update(Play({Finger(1, beyond)})), {TouchAction::Fire}));   // E29
    TouchControls ordinary;
    CHECK(Only(ordinary.Update(Play({Finger(1, beyond)})), {}));
    CHECK(ordinary.RunningCombo() == TouchCombo::None);   // E29
}

// Opacity: a multiplier on the manifest's own alpha, and a held look that stays a cue at every opacity.
void testTuningOpacity() {
    const TouchManifest built = TouchControls::DefaultManifest();
    for (const float opacity : {0.2f, 0.6f, 1.0f, 1.4f, 1.8f}) {
        const TouchManifest manifest = TouchControls::WithTuning(built, SizedAt(1.0f, opacity));
        const float idle = std::clamp(0.45f * opacity, 0.0f, 1.0f);
        CHECK_MSG(manifest.idleAlpha == idle, std::to_string(opacity));
        CHECK_MSG(manifest.pressedAlpha == std::clamp(std::max(0.9f, idle + 0.15f), 0.0f, 1.0f), std::to_string(opacity));
        CHECK_MSG(manifest.controls == built.controls, std::to_string(opacity));   // sizes and places untouched
    }
    CHECK(TouchControls::WithTuning(built, SizedAt(1.0f, 1.0f)) == built);   // 1.0: today's, exactly: 0.45 and 0.9
    CHECK(TouchControls::WithTuning(built, SizedAt(1.0f, 0.2f)).pressedAlpha == 0.9f);   // 0.09 + 0.15 is below the manifest's
    // All nine steps: held is above idle by a margin the eye sees, and never past 1.
    for (int step = 1; step <= 9; ++step) {
        const float opacity = static_cast<float>(step) / 5.0f;
        const TouchManifest manifest = TouchControls::WithTuning(built, SizedAt(1.0f, opacity));
        CHECK_MSG(manifest.pressedAlpha >= manifest.idleAlpha + 0.14f, std::to_string(opacity));
        CHECK_MSG(manifest.pressedAlpha <= 1.0f && manifest.idleAlpha <= 1.0f && manifest.idleAlpha > 0.0f,
                  std::to_string(opacity));
    }
    // A hand-made manifest whose idle is already high: the cap, not a wrap.
    TouchManifest bright = built;
    bright.idleAlpha = 0.9f;
    CHECK(TouchControls::WithTuning(bright, SizedAt(1.0f, 1.8f)).idleAlpha == 1.0f);
    CHECK(TouchControls::WithTuning(bright, SizedAt(1.0f, 1.8f)).pressedAlpha == 1.0f);

    // As drawn: with no art found every control is a plain square at the control's alpha, never stretched to the
    // sides of a wide menu (the direction control's, in the editor, straddles x = 0).
    const auto bytes = [](float opacity, bool held) {
        TouchControls touch;
        touch.SetTuning(SizedAt(1.0f, opacity));
        touch.Update(Play(held ? std::vector<TouchContact>{Finger(1, Centre(TouchControl::Jump))}
                               : std::vector<TouchContact>{}));
        std::vector<HudCmd> out;
        touch.AppendOverlay(out);
        std::vector<int> alphas;
        for (const HudCmd& cmd : out) {
            CHECK(cmd.kind == HudCmd::Kind::Rectangle);
            CHECK(!cmd.stretchToSides);
            alphas.push_back(static_cast<int>(Alpha(cmd.color)));
        }
        return alphas;
    };
    for (const auto& [opacity, idle] : {std::pair{0.2f, 23}, std::pair{1.0f, 115}, std::pair{1.8f, 207}}) {
        const int pressed = static_cast<int>(
            std::lround(TouchControls::WithTuning(built, SizedAt(1.0f, opacity)).pressedAlpha * 255.0f));
        const std::vector<int> idleBytes = bytes(opacity, false);
        CHECK_EQ(idleBytes.size(), std::size_t{8});   // the disc, four buttons, two combos, the pause
        for (const int alpha : idleBytes) CHECK_MSG(alpha == idle, std::to_string(opacity));
        int brighter = 0;
        for (const int alpha : bytes(opacity, true)) brighter += alpha == pressed ? 1 : 0;
        CHECK_MSG(brighter == 1, std::to_string(opacity));   // the held jump
    }
    // With the art, a control's image is a sprite at the same alpha.
    TouchControls art;
    art.SetImageRoot(PENUMBRA_DATA_DIR);
    art.SetTuning(SizedAt(1.0f, 0.2f));
    art.Update(Play());
    std::vector<HudCmd> out;
    art.AppendOverlay(out);
    CHECK_EQ(out.size(), std::size_t{10});
    for (const HudCmd& cmd : out) CHECK_EQ(static_cast<int>(Alpha(cmd.color)), 23);
}

// Moves: manifest pixels, in the SCREEN's direction, from where the manifest puts the control, whatever it hangs
// from; not scaled by the size; scaled by the window like every offset; kept on the screen.
void testTuningMoves() {
    const TouchLayout plain = Default();
    CHECK(BoxIs(Tuned(WithMove(TouchControl::Jump, -40.0f, 12.0f))[TouchControl::Jump],   // E32: from where the grid has it
                plain[TouchControl::Jump].min + glm::vec2(-40.0f, 12.0f), plain[TouchControl::Jump].max + glm::vec2(-40.0f, 12.0f)));
    CHECK(BoxIs(Tuned(WithMove(TouchControl::Pause, -30.0f, 20.0f))[TouchControl::Pause], {878.0f, 64.0f}, {974.0f, 128.5f}));
    // Up past the floor: the pause stays below the run's timer row (25) and a margin (4).
    CHECK(BoxIs(Tuned(WithMove(TouchControl::Pause, 0.0f, -30.0f))[TouchControl::Pause], {908.0f, 29.0f}, {1004.0f, 93.5f}));
    CHECK(BoxIs(Tuned(WithMove(TouchControl::Pause, 0.0f, -15.0f))[TouchControl::Pause], {908.0f, 29.0f}, {1004.0f, 93.5f}));
    CHECK_NEAR(Tuned(WithMove(TouchControl::Pause, 0.0f, -14.0f))[TouchControl::Pause].min.y, 30.0f);
    // The direction control, and the down button that follows it (it sits over its gap); its own move adds.
    const TouchLayout dpadMoved = Tuned(WithMove(TouchControl::Dpad, 10.0f, -20.0f));
    CHECK(BoxIs(dpadMoved[TouchControl::Dpad], {34.0f, 486.0f}, {434.0f, 886.0f}));
    CHECK(BoxIs(dpadMoved[TouchControl::ExitDown], {171.0f, 462.0f}, {297.0f, 588.0f}));
    TouchTuning both = WithMove(TouchControl::Dpad, 10.0f, -20.0f);
    both.move[Slot(TouchControl::ExitDown)] = Mv(5.0f, 5.0f);
    CHECK(BoxIs(Tuned(both)[TouchControl::ExitDown], {176.0f, 467.0f}, {302.0f, 593.0f}));
    CHECK(BoxIs(Tuned(WithMove(TouchControl::ExitDown, 5.0f, 5.0f))[TouchControl::ExitDown], {166.0f, 487.0f}, {292.0f, 613.0f}));
    // Only what was moved moves.
    const TouchLayout jumpMoved = Tuned(WithMove(TouchControl::Jump, -40.0f, 12.0f));
    for (int i = 0; i < kTouchControlCount; ++i) {
        const TouchControl control = static_cast<TouchControl>(i);
        if (control == TouchControl::Jump) continue;
        CHECK_MSG(SameBox(jumpMoved[control], plain[control]), TouchControls::ControlId(control));
    }
    // Not scaled by the size: 40 px left and 12 down of where the half-size button is.
    const TouchLayout::Box halfJump = Tuned(SizedAt(0.5f))[TouchControl::Jump];   // E32
    CHECK(BoxIs(Tuned(WithMove(TouchControl::Jump, -40.0f, 12.0f, 0.5f))[TouchControl::Jump],
                halfJump.min + glm::vec2(-40.0f, 12.0f), halfJump.max + glm::vec2(-40.0f, 12.0f)));

    // Screen direction, every anchor, on every screen: a small move shifts the box by exactly that.
    const glm::vec2 screens[] = {kFourThree, kWide, {1707.0f, 768.0f}};
    const TouchInsets insets[] = {{}, {88.0f, 0.0f, 88.0f, 24.0f}, {0.0f, 30.0f, 0.0f, 20.0f}};
    for (const TouchControl control : kMovable) {
        for (const glm::vec2& screen : screens) {
            for (const TouchInsets& safe : insets) {
                const TouchLayout none = Tuned(TouchTuning{}, screen, safe);
                for (const TouchMove step : {Mv(3.0f, -2.0f), Mv(-3.0f, 2.0f)}) {
                    const TouchLayout moved = Tuned(WithMove(control, step.x, step.y), screen, safe);
                    const std::string where = std::string(TouchControls::ControlId(control)) + " on " +
                                              std::to_string(static_cast<int>(screen.x)) + " inset " +
                                              std::to_string(static_cast<int>(safe.left));
                    CHECK_MSG(Near(moved[control].min.x - none[control].min.x, step.x, 0.001f) &&
                                  Near(moved[control].min.y - none[control].min.y, step.y, 0.001f),
                              where);
                    CHECK_MSG(moved[control].Size() == none[control].Size(), where);
                }
            }
        }
    }
    // Scaled by the window like every offset (a zoomed level's unit), the pause's timer rule included.
    for (const float unit : {1.0f, 0.5f, 0.75f}) {
        for (const TouchControl control : kMovable) {
            const TouchLayout none = Tuned(TouchTuning{}, kFourThree, {}, unit);
            for (const TouchMove step : {Mv(8.0f, 6.0f), Mv(-8.0f, -6.0f)}) {   // small: no clamp, no floor
                const TouchLayout moved = Tuned(WithMove(control, step.x, step.y), kFourThree, {}, unit);
                CHECK_MSG(Near(moved[control].min.x - none[control].min.x, step.x * unit, 0.01f) &&
                              Near(moved[control].min.y - none[control].min.y, step.y * unit, 0.01f),
                          std::string(TouchControls::ControlId(control)) + " at " + std::to_string(unit));
            }
        }
    }

    // On the screen whatever the move says: the layout's own clamp (the dpad keeps its overhang).
    const TouchLayout far = Tuned(WithMove(TouchControl::Jump, 5000.0f, 5000.0f));
    CHECK(far[TouchControl::Jump].max == kFourThree);
    const TouchLayout gone = Tuned(WithMove(TouchControl::Pause, -5000.0f, -5000.0f));
    CHECK(gone[TouchControl::Pause].min.x == 0.0f && gone[TouchControl::Pause].min.y == TouchControls::kPauseTopClear);
    CHECK(Inside(Tuned(WithMove(TouchControl::Dpad, -5000.0f, 5000.0f))[TouchControl::Dpad], glm::vec2(0.0f, 0.0f),
                 glm::vec2(1024.0f, 768.0f + 150.0f)));
    // The pause's frame: moved up, it keeps the timer's row below the frame's top (E26) and E25's scaled timer row.
    const TouchInsets frame{40.0f, 18.0f, 40.0f, 0.0f};
    CHECK_NEAR(Tuned(WithMove(TouchControl::Pause, 0.0f, -100.0f), kFourThree, {}, 1.0f, frame)[TouchControl::Pause].min.y, 47.0f);
    const float unit = 512.0f / 768.0f;
    CHECK_NEAR(Tuned(WithMove(TouchControl::Pause, 0.0f, -100.0f), kFourThree, {}, unit)[TouchControl::Pause].min.y,
               TouchControls::kTimerRowHeight + 4.0f * unit);

    // What Clamped() holds: Back never moves, a typo cannot take a control off to infinity, a NaN is no move.
    TouchTuning wild;
    wild.move[Slot(TouchControl::Back)] = Mv(30.0f, 30.0f);
    wild.move[Slot(TouchControl::Fire)] = Mv(std::numeric_limits<float>::quiet_NaN(), 3.0f);
    wild.move[Slot(TouchControl::Light)] = Mv(1.0e9f, -1.0e9f);
    const TouchManifest sane = TouchControls::WithTuning(TouchControls::DefaultManifest(), wild);
    CHECK(sane[TouchControl::Back] == TouchControls::DefaultManifest()[TouchControl::Back]);   // not moved
    CHECK(wild.Clamped().move[Slot(TouchControl::Fire)] == Mv(0.0f, 3.0f));
    CHECK(wild.Clamped().move[Slot(TouchControl::Light)] == Mv(2048.0f, -2048.0f));
    CHECK(TouchControls::Movable(TouchControl::Pause) && !TouchControls::Movable(TouchControl::Back));
}

// A small generator with no platform in it (std::uniform_real_distribution differs between standard libraries,
// and a test that draws different points on each says different things).
struct Lcg {
    std::uint32_t state = 28u;
    float Next() {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>(state >> 8) / 16777216.0f;
    }
    float In(float lo, float hi) { return lo + (hi - lo) * Next(); }
};

TouchLayout LayoutOf(const TouchManifest& manifest, const TouchGeometry& geometry) {
    return TouchControls::ComputeLayout(manifest, geometry.AreaMin(), geometry.AreaMax(), geometry.safeArea,
                                        geometry.unit, geometry.hudFrame);
}

// What the layout keeps a control inside, derived here from the geometry alone (not by the layout's own code):
// the safe area, or for the pause the HUD's frame within it.
struct Room {
    glm::vec2 lo{0.0f};
    glm::vec2 hi{0.0f};
};

Room RoomOf(const TouchGeometry& geometry, bool framed) {
    const glm::vec2 min = geometry.AreaMin();
    const glm::vec2 max = geometry.AreaMax();
    const TouchInsets& safe = geometry.safeArea;
    Room room;
    room.lo = min + glm::vec2(std::max(0.0f, safe.left), std::max(0.0f, safe.top));
    room.hi = glm::max(room.lo, max - glm::vec2(std::max(0.0f, safe.right), std::max(0.0f, safe.bottom)));
    if (framed) {
        const TouchInsets& frame = geometry.hudFrame;
        const glm::vec2 lo = glm::max(room.lo, min + glm::vec2(frame.left, frame.top));
        room.hi = glm::max(lo, glm::min(room.hi, max - glm::vec2(frame.right, frame.bottom)));
        room.lo = lo;
    }
    return room;
}

// Inside its room but for the overhang the manifest allows, at the size and the window's scale it is drawn at.
bool KeptInside(const TouchManifest& tuned, TouchControl control, const TouchLayout::Box& box,
                const TouchGeometry& geometry) {
    TouchControlSpec spec = tuned[control];
    const float scale = tuned.scale * geometry.unit;
    spec.size *= scale;
    spec.overhang *= scale;
    const Room room = RoomOf(geometry, control == TouchControl::Pause);
    return InsideHanging(box, spec, room.lo, room.hi);
}

TouchGeometry GeometryOf(glm::vec2 screen, TouchInsets safe, float unit, TouchInsets frame) {
    TouchGeometry geometry;
    geometry.screen = screen;
    geometry.safeArea = safe;
    geometry.unit = unit;
    geometry.hudFrame = frame;
    return geometry;
}

// MoveFor is ComputeLayout run backwards: the move that puts a control where a finger drags it. Round trips
// through the real layout on every control, three screen shapes, a notch and a status bar, two window scales, the
// HUD's frame, E1's wide menus' area and every other size the editor offers; and the pins.
void testMoveFor() {
    const TouchManifest manifest = TouchControls::DefaultManifest();
    const TouchInsets insets[] = {{}, {88.0f, 0.0f, 88.0f, 24.0f}, {0.0f, 30.0f, 0.0f, 20.0f}};
    std::vector<TouchGeometry> geometries;
    for (const glm::vec2 screen : {kFourThree, kWide, glm::vec2(1707.0f, 768.0f)}) {
        for (const TouchInsets& safe : insets) {
            for (const float unit : {1.0f, 0.667f}) {
                for (const TouchInsets& frame : {TouchInsets{}, TouchInsets{40.0f, 18.0f, 40.0f, 0.0f}}) {
                    geometries.push_back(GeometryOf(screen, safe, unit, frame));
                }
            }
        }
    }
    for (const TouchInsets& safe : insets) {   // E1's wide menu: laid out across what is shown, past the screen's sides
        TouchGeometry area = GeometryOf(kFourThree, safe, 1.0f, {});
        area.areaMin = glm::vec2(-341.5f, 0.0f);
        area.areaMax = glm::vec2(1365.5f, 768.0f);
        geometries.push_back(area);
    }
    Lcg rng;
    int trials = 0;
    for (const TouchGeometry& geometry : geometries) {
        for (const float size : {0.4f, 1.0f, 1.2f, 1.4f}) {
            for (const TouchControl control : kMovable) {
                const std::string where = std::string(TouchControls::ControlId(control)) + " size " +
                                          std::to_string(size) + " on " + std::to_string(static_cast<int>(geometry.screen.x)) +
                                          (geometry.areaMax.x > geometry.areaMin.x ? " (area)" : "") + " unit " +
                                          std::to_string(geometry.unit) + (geometry.hudFrame.left > 0.0f ? " framed" : "");
                // One verdict per control, size and geometry over all its trials: the first thing that went wrong, with
                // the point it went wrong at (a thousand identical checks would say no more than that).
                std::string problem;
                const auto note = [&problem](bool ok, const std::string& what) {
                    if (!ok && problem.empty()) problem = what;
                };
                for (int trial = 0; trial < 8; ++trial, ++trials) {
                    // Every other control moved a little (the Dpad's takes the down button along), this one's move
                    // anything the layout allows.
                    TouchTuning others = SizedAt(size);
                    for (const TouchControl other : kMovable) {
                        if (other != control && rng.Next() < 0.4f) {
                            others.move[Slot(other)] = Mv(rng.In(-120.0f, 120.0f), rng.In(-120.0f, 120.0f));
                        }
                    }
                    TouchTuning mine = others;
                    mine.move[Slot(control)] = Mv(rng.In(-250.0f, 250.0f), rng.In(-250.0f, 250.0f));
                    const TouchLayout drawn = LayoutOf(TouchControls::WithTuning(manifest, mine), geometry);
                    const glm::vec2 target = drawn[control].min;   // a place the layout can put it, by construction

                    const TouchMove got = TouchControls::MoveFor(manifest, mine, control, target, geometry);
                    note(got == TouchControls::MoveFor(manifest, others, control, target, geometry), "its own move was not ignored");
                    TouchTuning applied = others;
                    applied.move[Slot(control)] = got;
                    const TouchLayout again = LayoutOf(TouchControls::WithTuning(manifest, applied), geometry);
                    note(Near(again[control].min.x, target.x, 0.15f) && Near(again[control].min.y, target.y, 0.15f),
                         "wanted (" + std::to_string(target.x) + "," + std::to_string(target.y) + ") got (" +
                             std::to_string(again[control].min.x) + "," + std::to_string(again[control].min.y) + ")");
                    // Idempotent: asking for where it is gives the move it has.
                    const TouchMove same = TouchControls::MoveFor(manifest, applied, control, again[control].min, geometry);
                    note(Near(same.x, got.x, 0.1f) && Near(same.y, got.y, 0.1f), "not idempotent");
                    // Nothing else moved by it, but the down button the direction control carries.
                    TouchTuning unmoved = others;
                    unmoved.move[Slot(control)] = Mv(0.0f, 0.0f);
                    const TouchLayout rest = LayoutOf(TouchControls::WithTuning(manifest, unmoved), geometry);
                    for (int i = 0; i < kTouchControlCount; ++i) {
                        const TouchControl other = static_cast<TouchControl>(i);
                        if (other == control || (control == TouchControl::Dpad && other == TouchControl::ExitDown)) continue;
                        note(SameBox(again[other], rest[other]), std::string("it moved ") + TouchControls::ControlId(other));
                    }
                    // Anywhere at all (off the screen too): the result is inside the room it must keep to.
                    const glm::vec2 anywhere(rng.In(-300.0f, 1800.0f), rng.In(-300.0f, 1100.0f));
                    TouchTuning dragged = others;
                    dragged.move[Slot(control)] = TouchControls::MoveFor(manifest, others, control, anywhere, geometry);
                    const TouchManifest tuned = TouchControls::WithTuning(manifest, dragged);
                    note(KeptInside(tuned, control, LayoutOf(tuned, geometry)[control], geometry),
                         "dragged to (" + std::to_string(anywhere.x) + "," + std::to_string(anywhere.y) + ") it left its room");
                }
                CHECK_MSG(problem.empty(), where + ": " + problem);
            }
        }
    }
    std::printf("  E28 MoveFor: %d round trips\n", trials);

    // The pins, 4:3, size 1.
    const TouchGeometry flat = GeometryOf(kFourThree, {}, 1.0f, {});
    const TouchTuning none;
    const auto moveTo = [&](TouchControl control, float x, float y) {
        return TouchControls::MoveFor(manifest, none, control, glm::vec2(x, y), flat);
    };
    const glm::vec2 jumpAt = Default()[TouchControl::Jump].min;   // E33: (864, 584)
    CHECK(moveTo(TouchControl::Jump, 700.0f, 600.0f) == Mv(700.0f - jumpAt.x, 600.0f - jumpAt.y));   // E33: (-164, 16)
    CHECK(moveTo(TouchControl::Jump, 2000.0f, 2000.0f) ==
          Mv(manifest[TouchControl::Jump].offset.x, manifest[TouchControl::Jump].offset.y));   // the bound (984, 704): the manifest's offset   // E33
    CHECK(moveTo(TouchControl::Dpad, -500.0f, 900.0f) == Mv(-24.0f, 12.0f));   // the bound (0, 518): the overhang
    CHECK(moveTo(TouchControl::Pause, 908.0f, 0.0f) == Mv(0.0f, -15.0f));   // below the timer's row: y 29
    CHECK(Tuned(WithMove(TouchControl::Pause, 0.0f, -15.0f))[TouchControl::Pause].min.y == 29.0f);
    CHECK(moveTo(TouchControl::Back, 100.0f, 100.0f).IsZero());   // Back never moves
    CHECK(moveTo(TouchControl::Jump, std::numeric_limits<float>::quiet_NaN(), 600.0f).x == 0.0f);   // no point, no move
    // The down button counts the direction control's move as its own start.
    const TouchTuning dpadAway = WithMove(TouchControl::Dpad, 10.0f, -20.0f);
    CHECK(TouchControls::MoveFor(manifest, dpadAway, TouchControl::ExitDown, glm::vec2(176.0f, 467.0f), flat) == Mv(5.0f, 5.0f));
    CHECK(TouchControls::MoveFor(manifest, dpadAway, TouchControl::Dpad, glm::vec2(34.0f, 486.0f), flat) == Mv(10.0f, -20.0f));
    // The HUD's frame (E26) holds the pause: in from the frame's right and below its top and the timer's row.
    const TouchInsets frame{40.0f, 18.0f, 40.0f, 0.0f};
    const TouchGeometry framed = GeometryOf(kFourThree, {}, 1.0f, frame);
    const TouchMove corner = TouchControls::MoveFor(manifest, none, TouchControl::Pause, glm::vec2(2000.0f, 0.0f), framed);
    CHECK(corner == Mv(20.0f, -15.0f));
    CHECK(BoxIs(Tuned(WithMove(TouchControl::Pause, corner.x, corner.y), kFourThree, {}, 1.0f, frame)[TouchControl::Pause],
                {888.0f, 47.0f}, {984.0f, 111.5f}));
    // A zoomed level (E25): the timer's row is the screen's 25 px, the margin scales.
    const float unit = 512.0f / 768.0f;
    const TouchGeometry zoomed = GeometryOf(glm::vec2(1138.0f, 512.0f), {}, unit, {});
    const TouchMove up = TouchControls::MoveFor(manifest, none, TouchControl::Pause, glm::vec2(1000.0f, -500.0f), zoomed);
    CHECK_NEAR(TouchControls::ComputeLayout(TouchControls::WithTuning(manifest, WithMove(TouchControl::Pause, up.x, up.y)),
                                            zoomed.screen, {}, unit)[TouchControl::Pause].min.y,
               TouchControls::kTimerRowHeight + 4.0f * unit);
}

// What a finger can grab in the editor: the strip the direction control's two buttons occupy, not its 400-unit
// disc (half of it off the screen, the rest empty).
void testGrabBox() {
    const TouchManifest manifest = TouchControls::DefaultManifest();
    const TouchLayout plain = Default();
    const TouchLayout::Box strip = TouchControls::GrabBox(TouchControl::Dpad, plain[TouchControl::Dpad]);
    CHECK(BoxIs(strip, {24.0f, 621.32f}, {424.0f, 747.32f}, 0.001f));
    for (int i = 0; i < kTouchControlCount; ++i) {
        const TouchControl control = static_cast<TouchControl>(i);
        if (control == TouchControl::Dpad) continue;
        CHECK_MSG(SameBox(TouchControls::GrabBox(control, plain[control]), plain[control]), TouchControls::ControlId(control));
    }
    // It is the buttons' whole drawn extent (every texel that is not transparent; the art is 406 px for the box's 400
    // units, so a texel is a unit within a unit and a half), and so it holds both opaque faces (alpha 128 and up),
    // which it exceeds by the art's soft rim, about 10 units a side.
    const auto unionOf = [&](unsigned minAlpha) {
        const TouchLayout::Box left = ArrowFace(manifest.dpadLeft, plain[TouchControl::Dpad], minAlpha);
        const TouchLayout::Box right = ArrowFace(manifest.dpadRight, plain[TouchControl::Dpad], minAlpha);
        return TouchLayout::Box{glm::min(left.min, right.min), glm::max(left.max, right.max)};
    };
    const TouchLayout::Box drawn = unionOf(1u);
    const TouchLayout::Box faces = unionOf(128u);
    std::printf("  E28 GrabBox: the buttons' drawn extent (%.2f,%.2f)-(%.2f,%.2f), their opaque faces (%.2f,%.2f)-(%.2f,%.2f), "
                "the strip (%.2f,%.2f)-(%.2f,%.2f)\n",
                drawn.min.x, drawn.min.y, drawn.max.x, drawn.max.y, faces.min.x, faces.min.y, faces.max.x, faces.max.y,
                strip.min.x, strip.min.y, strip.max.x, strip.max.y);
    CHECK(Near(strip.min.x, drawn.min.x, 1.5f) && Near(strip.min.y, drawn.min.y, 1.5f));
    CHECK(Near(strip.max.x, drawn.max.x, 1.5f) && Near(strip.max.y, drawn.max.y, 1.5f));
    CHECK(strip.min.x <= faces.min.x && strip.min.y <= faces.min.y && strip.max.x >= faces.max.x && strip.max.y >= faces.max.y);
    CHECK(faces.min.x - strip.min.x <= 10.5f && faces.min.y - strip.min.y <= 10.5f);   // the rim, no more
    CHECK(strip.max.x - faces.max.x <= 10.5f && strip.max.y - faces.max.y <= 10.5f);
    // At any size and place: the same fractions of the box.
    for (const float size : {0.4f, 1.2f, 1.4f}) {
        const TouchLayout::Box box = Tuned(SizedAt(size))[TouchControl::Dpad];
        const TouchLayout::Box grab = TouchControls::GrabBox(TouchControl::Dpad, box);
        CHECK_NEAR((grab.min.y - box.min.y) / box.Size().y, 0.2883f);
        CHECK_NEAR((grab.max.y - box.min.y) / box.Size().y, 0.6033f);
        CHECK(grab.min.x == box.min.x && grab.max.x == box.max.x);
        const TouchLayout::Box leftAt = ArrowFace(manifest.dpadLeft, box);
        CHECK_MSG(grab.min.y <= leftAt.min.y && grab.max.y >= leftAt.max.y, std::to_string(size));
    }
}

// The editor's scene: every control that moves is shown and none of them presses anything; no finger is the
// mouse; a finger there is dead in every scene after it; and the layout follows a tuning at once.
void testEditScene() {
    constexpr TouchControl kShown[] = {TouchControl::Dpad,       TouchControl::Jump,     TouchControl::Sword,
                                       TouchControl::Fire,       TouchControl::Light,    TouchControl::SwordCombo,
                                       TouchControl::SpellCombo, TouchControl::ExitDown, TouchControl::Pause};
    const glm::vec2 onDpad = Dpad({-100.0f, -21.7f});
    const glm::vec2 onJump = Centre(TouchControl::Jump);
    const glm::vec2 onPause = Centre(TouchControl::Pause);
    const glm::vec2 onNothing(500.0f, 300.0f);

    TouchControls touch;
    TouchStep step = touch.Update(Edit({Finger(1, onDpad), Finger(2, onJump), Finger(3, onPause)}));
    CHECK(Only(step, {}));   // the pause sends no Esc, the disc no direction, the jump no key
    CHECK(step.touching && !step.pointer && step.combo == TouchCombo::None);
    CHECK_EQ(KeysDown(FrameOf(step)), 0);
    for (const TouchControl control : kShown) CHECK_MSG(touch.Visible(control), TouchControls::ControlId(control));
    CHECK(!touch.Visible(TouchControl::Back));
    // Shown whatever the corner says, and the down button away from any door.
    touch.Update(Edit());
    CHECK(touch.Visible(TouchControl::Pause) && touch.Visible(TouchControl::ExitDown));
    TouchInput withCorner = Edit();
    withCorner.corner = TouchCorner::Back;
    touch.Update(withCorner);
    CHECK(touch.Visible(TouchControl::Pause) && !touch.Visible(TouchControl::Back));
    // A manifest without the combos shows none in the editor either (the layout has no such control).
    TouchManifest noCombos = TouchControls::DefaultManifest();
    noCombos[TouchControl::SwordCombo].enabled = false;
    TouchControls trimmed;
    trimmed.SetManifest(noCombos);
    trimmed.Update(Edit());
    CHECK(!trimmed.Visible(TouchControl::SwordCombo) && trimmed.Visible(TouchControl::SpellCombo));
    // The layout is the same as in play.
    touch.Update(Edit());
    CHECK(SameLayout(touch.Layout(), Default()));

    // What a finger of the editor does afterwards: nothing, until it lifts. In a menu (no click)...
    touch.Update(Edit({Finger(1, onDpad), Finger(2, onJump), Finger(3, onPause)}));
    step = touch.Update(Menu({Finger(1, onDpad), Finger(2, onJump), Finger(3, onPause)}));
    CHECK(Only(step, {}) && !step.pointer);
    CHECK_EQ(KeysDown(FrameOf(step)), 0);   // no click reaches the screen under the editor
    // ...while a new finger is the mouse as ever.
    step = touch.Update(Menu({Finger(1, onDpad), Finger(2, onJump), Finger(3, onPause), Finger(4, onNothing)}));
    CHECK(step.pointer && step.pointerPos == onNothing);
    // ...and in play.
    step = touch.Update(Play({Finger(1, onDpad), Finger(2, onJump), Finger(3, onPause)}));
    CHECK(Only(step, {}));
    touch.Update(Play());
    CHECK(Only(touch.Update(Play({Finger(5, onJump)})), {TouchAction::Jump}));   // lifted, they work again

    // A finger a level gave an owner (jump, the pause's Esc, the disc) lets go of it in the editor: no key from it,
    // and none after.
    TouchControls played;
    CHECK(Only(played.Update(Play({Finger(1, onJump), Finger(2, onPause), Finger(3, onDpad)})),
               {TouchAction::Jump, TouchAction::Cancel, TouchAction::Left}));
    CHECK(Only(played.Update(Edit({Finger(1, onJump), Finger(2, onPause), Finger(3, onDpad)})), {}));
    CHECK(Only(played.Update(Play({Finger(1, onJump), Finger(2, onPause), Finger(3, onDpad)})), {}));
    // A finger the menu made the mouse is not one in the editor, nor again after it.
    TouchControls menued;
    CHECK(menued.Update(Menu({Finger(1, onNothing)})).pointer);
    CHECK(!menued.Update(Edit({Finger(1, onNothing)})).pointer);
    CHECK(!menued.Update(Menu({Finger(1, onNothing)})).pointer);
    // A combo in progress is cancelled by the editor.
    TouchControls combo;
    combo.Update(Play({Finger(1, Centre(TouchControl::SwordCombo))}));
    CHECK(combo.RunningCombo() == TouchCombo::Sword);
    combo.Update(Edit());
    CHECK(combo.RunningCombo() == TouchCombo::None);

    // The fingers the last Update used, in every scene (the editor reads exactly these): the input's that are down
    // and those a frame without a tick saw.
    TouchControls fingers;
    fingers.Update(Menu({Finger(1, onNothing), Finger(2, onJump, false)}));
    CHECK_EQ(fingers.Down().size(), std::size_t{1});
    CHECK(fingers.Down().size() == 1 && fingers.Down()[0].id == 1 && fingers.Down()[0].position == onNothing);
    fingers.LatchFrame({Finger(9, onJump)});
    fingers.Update(Edit({Finger(1, onNothing)}));
    CHECK_EQ(fingers.Down().size(), std::size_t{2});
    int latched = 0;
    for (const TouchContact& contact : fingers.Down()) latched += contact.id == 9 && contact.position == onJump ? 1 : 0;
    CHECK_EQ(latched, 1);
    fingers.Update(Edit());
    CHECK(fingers.Down().empty());
    fingers.Update(Play({Finger(5, onJump)}));
    CHECK(fingers.Down().size() == 1 && fingers.Down()[0].id == 5);

    // SetTuning shows at once: the layout of the last Update, laid out again by the same geometry, area and all.
    TouchControls tuned;
    const TouchManifest built = TouchControls::DefaultManifest();
    TouchTuning first = WithMove(TouchControl::Jump, -40.0f, 12.0f, 1.2f);
    tuned.SetTuning(first);   // before any Update: stored, nothing to lay out
    CHECK(tuned.Tuning() == first);
    CHECK(tuned.Layout()[TouchControl::Jump].Size() == glm::vec2(0.0f));
    TouchInput wide = Edit();
    wide.areaMin = glm::vec2(-341.5f, 0.0f);
    wide.areaMax = glm::vec2(1365.5f, 768.0f);
    wide.safeArea = TouchInsets{88.0f, 0.0f, 88.0f, 24.0f};
    wide.hudFrame = TouchInsets{40.0f, 18.0f, 40.0f, 0.0f};
    wide.unit = 0.9f;
    tuned.Update(wide);
    const auto expected = [&](const TouchTuning& tuning, const TouchInput& input) {
        return TouchControls::ComputeLayout(TouchControls::WithTuning(built, tuning), input.areaMin, input.areaMax,
                                            input.safeArea, input.unit, input.hudFrame);
    };
    CHECK(SameLayout(tuned.Layout(), expected(first, wide)));
    const TouchLayout before = tuned.Layout();
    TouchTuning second = WithMove(TouchControl::Pause, -10.0f, 10.0f, 0.6f);
    second.move[Slot(TouchControl::Dpad)] = Mv(30.0f, -15.0f);
    tuned.SetTuning(second);
    CHECK(SameLayout(tuned.Layout(), expected(second, wide)));
    CHECK(!SameLayout(tuned.Layout(), before));
    // With no area, the screen.
    TouchInput screen = Edit();
    screen.screen = kWide;
    tuned.Update(screen);
    tuned.SetTuning(first);
    CHECK(SameLayout(tuned.Layout(), TouchControls::ComputeLayout(TouchControls::WithTuning(built, first), kWide, TouchInsets{})));
    // The direction control's resting knob (the placeholder look keeps one) goes with it, the same frame.
    std::string warning;
    TouchControls placeholder;
    placeholder.SetManifest(TouchControls::LoadManifest(PlaceholderManifest(), &warning));
    placeholder.SetImageRoot(PENUMBRA_DATA_DIR);
    placeholder.Update(Edit());
    placeholder.SetTuning(WithMove(TouchControl::Dpad, 40.0f, -30.0f));
    std::vector<HudCmd> out;
    placeholder.AppendOverlay(out);
    CHECK_EQ(Knobs(out), 1);
    for (const HudCmd& cmd : out) {
        if (cmd.sprite.find("dpad_knob.png") == std::string::npos) continue;
        const glm::vec2 centre = cmd.pos + cmd.size * 0.5f;
        CHECK(Near(centre.x, placeholder.Layout()[TouchControl::Dpad].Centre().x) &&
              Near(centre.y, placeholder.Layout()[TouchControl::Dpad].Centre().y));
    }

    // Twice in one tick with the same fingers changes nothing but the scene: the layer lays the controls out again
    // after the editor opens and closes.
    TouchControls twice;
    const std::vector<TouchContact> held = {Finger(1, onNothing), Finger(2, onJump)};
    const TouchStep once = twice.Update(Edit(held));
    const TouchLayout layoutOnce = twice.Layout();
    const std::vector<TouchContact> downOnce = twice.Down();
    const TouchStep again = twice.Update(Edit(held));
    CHECK(Only(once, {}) && Only(again, {}) && once.touching == again.touching && once.pointer == again.pointer);
    CHECK(SameLayout(layoutOnce, twice.Layout()) && downOnce.size() == twice.Down().size());
    for (const TouchControl control : kShown) CHECK(twice.Visible(control));
    // From a menu into the editor: the same fingers, the controls appear, nothing is pressed.
    TouchControls opened;
    CHECK(opened.Update(Menu(held)).pointer);
    CHECK(!opened.Visible(TouchControl::Jump));
    CHECK(Only(opened.Update(Edit(held)), {}));
    CHECK(opened.Visible(TouchControl::Jump) && opened.Down().size() == 2);
    // The editor closing: the controls go again, and the fingers stay dead.
    CHECK(Only(opened.Update(Menu(held)), {}) && !opened.Visible(TouchControl::Jump));
}

// The size at which the shipped layout still draws no control over another IN PLAY, where the controls keep their
// size on the window (E25's zoom takes the screen down to the window's scale and the unit with it), the pause hangs
// from E26's frame under the timer, and a phone has a notch or a bar. Magic Rampage's own ceiling is 1.4
// (TouchTuning::kMaxSize), where it pushes overlapping buttons apart; Penumbra does not, so above the measured
// size the shipped layout draws controls over each other and the player moves them apart (the editor is a live
// preview). Measured with the real layout, pinned here, and written in the DEVLOG: the editor offers every size to
// kMaxSize, every one keeps every control on the screen, and none up to the pinned one overlaps another.
struct InPlayCase {
    std::string name;
    glm::vec2 screen{0.0f};
    float unit = 1.0f;
    TouchInsets safe;
    TouchInsets frame;
};

// `bottomBarsInWindowPixels`: the bottom-only 48 and 100 px insets read as the platform reports a bar (window
// pixels: SafeAreaInsets), not in the 768-tall pixels CheckLayoutFits' own bars are written in (the other reading).
std::vector<InPlayCase> InPlayCases(bool bottomBarsInWindowPixels = false) {
    using namespace Penumbra::Render;
    struct Shape {
        const char* name;
        glm::uvec2 window;
    };
    // The window shapes of testPhoneHudFrame: a 20:9 phone (3.5% margin, 175% zoom), a 16:9 and a 4:3 tablet (1%, 125%,
    // held to 113% on the 4:3).
    const Shape shapes[] = {{"20:9 phone", {2400u, 1080u}}, {"16:9 tablet", {1920u, 1080u}}, {"4:3 tablet", {2048u, 1536u}}};
    // CheckLayoutFits' bars, in the pixels of E1's 768-tall screen (scaled to the window), and a bottom bar of 48 and 100.
    const TouchInsets bars[] = {{}, {88.0f, 0.0f, 88.0f, 24.0f}, {0.0f, 30.0f, 0.0f, 20.0f}, {0.0f, 0.0f, 0.0f, 48.0f},
                                {0.0f, 0.0f, 0.0f, 100.0f}};
    std::vector<InPlayCase> cases;
    for (const Shape& shape : shapes) {
        for (const TouchInsets& bar : bars) {
            const bool bottomOnly = bar.bottom > 0.0f && bar.left == 0.0f && bar.top == 0.0f && bar.right == 0.0f;   // the 48, the 100
            const float perE1 = bottomOnly && bottomBarsInWindowPixels ? 1.0f : static_cast<float>(shape.window.y) / 768.0f;
            const Supersonic::SafeAreaInsets safe{bar.left * perE1, bar.top * perE1, bar.right * perE1, bar.bottom * perE1};
            const float margin =
                FittedEdgeMargin(EdgeMarginPercent(kEdgeMarginAuto, true, shape.window), shape.window, true, safe);
            const float zoom = CampaignZoom(kZoomAutomatic, true, shape.window, true, safe, margin);
            const glm::vec2 screen = ZoomedScreen(shape.window, true, zoom);
            const HudFrame frame = ComputeHudFrame(shape.window, screen, safe, margin);
            const float perLogical = static_cast<float>(shape.window.y) / screen.y;   // the screen fills the window: no bars
            InPlayCase one;
            one.name = std::string(shape.name) + ", bars " + std::to_string(static_cast<int>(bar.left)) + "/" +
                       std::to_string(static_cast<int>(bar.top)) + "/" + std::to_string(static_cast<int>(bar.bottom));
            one.screen = screen;
            one.unit = screen.y / 768.0f;
            one.safe = TouchInsets{safe.left / perLogical, safe.top / perLogical, safe.right / perLogical, safe.bottom / perLogical};
            one.frame = TouchInsets{frame.left, frame.top, frame.right, frame.bottom};
            cases.push_back(one);
        }
    }
    return cases;
}

void testSizeCeiling() {
    const TouchManifest built = TouchControls::DefaultManifest();
    // The faces of the direction control's two buttons, as fractions of its box: the 400-unit disc is mostly empty and
    // half of it hangs off the screen, so it is the buttons that are drawn over.
    const TouchLayout::Box unitBox{{0.0f, 0.0f}, {1.0f, 1.0f}};
    const TouchLayout::Box leftFace = ArrowFace(built.dpadLeft, unitBox);
    const TouchLayout::Box rightFace = ArrowFace(built.dpadRight, unitBox);
    const auto face = [](const TouchLayout::Box& fraction, const TouchLayout::Box& box) {
        return TouchLayout::Box{box.min + fraction.min * box.Size(), box.min + fraction.max * box.Size()};
    };
    struct Part {
        std::string name;
        TouchLayout::Box box;
    };
    // The nine things that are drawn, the Pause in the overlap checks against both combo buttons and the rest.
    const auto partsOf = [&](const TouchLayout& layout) {
        std::vector<Part> parts;
        parts.push_back({"dpad left", face(leftFace, layout[TouchControl::Dpad])});
        parts.push_back({"dpad right", face(rightFace, layout[TouchControl::Dpad])});
        for (const TouchControl control : {TouchControl::Jump, TouchControl::Sword, TouchControl::Fire, TouchControl::Light,
                                           TouchControl::SwordCombo, TouchControl::SpellCombo, TouchControl::Pause,
                                           TouchControl::ExitDown}) {
            parts.push_back({TouchControls::ControlId(control), layout[control]});
        }
        return parts;
    };
    struct Verdict {
        std::vector<std::string> overlaps;
        bool onScreen = true;
    };
    const auto judge = [&](float size, const std::vector<InPlayCase>& cases) {
        Verdict verdict;
        const TouchManifest tuned = TouchControls::WithTuning(built, SizedAt(size));
        for (const InPlayCase& one : cases) {
            const TouchLayout layout = TouchControls::ComputeLayout(tuned, one.screen, one.safe, one.unit, one.frame);
            TouchGeometry geometry = GeometryOf(one.screen, one.safe, one.unit, one.frame);
            for (int i = 0; i < kTouchControlCount; ++i) {
                const TouchControl control = static_cast<TouchControl>(i);
                if (!KeptInside(tuned, control, layout[control], geometry)) verdict.onScreen = false;
            }
            if (!Inside(layout[TouchControl::Pause], glm::vec2(0.0f), one.screen)) verdict.onScreen = false;
            const std::vector<Part> parts = partsOf(layout);
            for (std::size_t a = 0; a < parts.size(); ++a) {
                for (std::size_t b = a + 1; b < parts.size(); ++b) {
                    if (a == 0 && b == 1) continue;   // the two buttons of one control
                    if (Overlap(parts[a].box, parts[b].box)) {
                        verdict.overlaps.push_back(one.name + ": " + parts[a].name + " and " + parts[b].name);
                    }
                }
            }
        }
        return verdict;
    };

    const std::vector<InPlayCase> inPlay = InPlayCases();
    CHECK_EQ(inPlay.size(), std::size_t{15});
    // The same bars as CheckLayoutFits has (none, a notch and a gesture bar, a tablet's status bar) in play: nine cases...
    std::vector<InPlayCase> nine;
    for (const InPlayCase& one : inPlay) {
        if (one.name.find("bars 0/0/48") == std::string::npos && one.name.find("bars 0/0/100") == std::string::npos) {
            nine.push_back(one);
        }
    }
    CHECK_EQ(nine.size(), std::size_t{9});
    // ...and CheckLayoutFits' own screens, unzoomed, with no frame: where the controls have always been checked.
    std::vector<InPlayCase> unzoomed;
    for (const glm::vec2 screen : {kFourThree, kWide, glm::vec2(1707.0f, 768.0f)}) {
        for (const TouchInsets& safe : {TouchInsets{}, TouchInsets{88.0f, 0.0f, 88.0f, 24.0f}, TouchInsets{0.0f, 30.0f, 0.0f, 20.0f}}) {
            InPlayCase one;
            one.name = std::to_string(static_cast<int>(screen.x)) + "x768 unzoomed, inset " + std::to_string(static_cast<int>(safe.left)) +
                       "/" + std::to_string(static_cast<int>(safe.top));
            one.screen = screen;
            one.safe = safe;
            unzoomed.push_back(one);
        }
    }
    // The largest size from which every smaller one is clean, over a set of cases; and, for the record, what overlaps
    // at the first size that does not (the DEVLOG's).
    const auto ceilingOf = [&](const char* what, const std::vector<InPlayCase>& set, bool listFirstFailure) {
        float ceiling = 0.0f;
        bool clean = true;
        bool listed = false;
        for (int step = 4; step <= 14; ++step) {
            const float size = static_cast<float>(step) / 10.0f;
            const Verdict verdict = judge(size, set);
            clean = clean && verdict.overlaps.empty();
            if (clean) ceiling = size;
            // Every size the editor offers keeps every control on the screen, overlap or not.
            CHECK_MSG(verdict.onScreen, std::string(what) + " at " + std::to_string(size));
            if (listFirstFailure && !clean && !listed) {
                listed = true;
                std::printf("  E28 %s, size %.1f is the first with an overlap:\n", what, size);
                for (const std::string& line : verdict.overlaps) std::printf("      %s\n", line.c_str());
            }
        }
        std::printf("  E28 %s: no overlap up to size %.1f (the editor offers up to %.1f)\n", what, ceiling, TouchTuning::kMaxSize);
        return ceiling;
    };
    const float unzoomedCeiling = ceilingOf("unzoomed screens", unzoomed, true);   // E29: every set says what stops it
    const float nineCeiling = ceilingOf("in play, the nine bars", nine, true);   // E29
    // The two bottom bars (48 and 100) read two ways: in the 768-tall pixels CheckLayoutFits' own bars are written in, and
    // as window pixels, the unit a platform reports them in (a bar of 100 of a 1080 px window is 9%, not 13%).
    const auto upTo = [](const std::vector<InPlayCase>& all, bool with100) {
        std::vector<InPlayCase> kept;
        for (const InPlayCase& one : all) {
            if (one.name.find("bars 0/0/100") == std::string::npos || with100) kept.push_back(one);
        }
        return kept;
    };
    const std::vector<InPlayCase> inPlayWindowPixels = InPlayCases(true);
    CHECK_EQ(inPlayWindowPixels.size(), std::size_t{15});
    const float with48 = ceilingOf("in play, the nine and a 48 px bottom bar (of E1's 768)", upTo(inPlay, false), true);   // E29
    const float with48InWindowPixels = ceilingOf("in play, the nine and a 48 px bottom bar (window pixels)", upTo(inPlayWindowPixels, false), true);   // E29
    const float ceiling = ceilingOf("in play, with a 48 and a 100 px bottom bar (of E1's 768)", inPlay, true);
    const float ceilingInWindowPixels = ceilingOf("in play, with a 48 and a 100 px bottom bar (window pixels)", inPlayWindowPixels, true);
    // E33: for the record, the same sets without the notched 4:3 shape (a 4:3 screen with 88-unit cut-outs on both sides, which no
    // device is known to have), the one that stops every set at 1.1: what would stop each next. Printed, not pinned.
    const auto withoutNotched = [](const std::vector<InPlayCase>& all) {
        std::vector<InPlayCase> kept;
        for (const InPlayCase& one : all) {
            if (one.name.find("inset 88/0") == std::string::npos && one.name.find("bars 88/0/24") == std::string::npos) kept.push_back(one);
        }
        return kept;
    };
    ceilingOf("unzoomed screens, without the notched 4:3 shape", withoutNotched(unzoomed), true);
    ceilingOf("in play, the nine bars, without the notched 4:3 shape", withoutNotched(nine), true);
    ceilingOf("in play, the nine and a 48 px bottom bar (of E1's 768), without the notched 4:3 shape", withoutNotched(upTo(inPlay, false)), true);
    ceilingOf("in play, the nine and a 48 px bottom bar (window pixels), without the notched 4:3 shape", withoutNotched(upTo(inPlayWindowPixels, false)), true);
    ceilingOf("in play, with a 48 and a 100 px bottom bar (of E1's 768), without the notched 4:3 shape", withoutNotched(inPlay), true);
    ceilingOf("in play, with a 48 and a 100 px bottom bar (window pixels), without the notched 4:3 shape", withoutNotched(inPlayWindowPixels), true);
    // The measured ceilings, pinned: what the editor's sizes can do to the shipped layout. The pause hangs under the
    // frame (E26) and, zoomed, under the timer (E25), where the fire button, the top of the right column (E29), grows up
    // to it; nothing is pushed apart as Magic Rampage pushes, so above the pinned size a player moves the controls apart
    // (the editor is a live preview). The largest step is TouchTuning::kMaxSize, MR's own. E33: the left column's far edge
    // is 296 from the screen's right edge (E32: 424, E29: 280) and the direction control's right button's face ends
    // 414.15 s from the left edge of the safe area (it grows from the bottom left corner), so on a 4:3 screen a notch
    // narrows to 848 units the two first meet above s = 848 / 710.15 = 1.194, which is 1.2 in steps of 0.1, and on a plain 4:3
    // screen above s = 1024 / 710.15 = 1.442, which the editor does not offer: every set stops at 1.1 (E32: 1.0, E29: 1.2;
    // E16's were 1.1, 1.1 and 1.0 on these sets and 1.2 unzoomed). What stops each at 1.2 is pinned below, and the pause
    // button's clearance of the fire button.
    constexpr float kUnzoomedCeiling = 1.1f;   // E33
    constexpr float kNineCeiling = 1.1f;   // E33
    constexpr float kWith48Ceiling = 1.1f;   // E33: both readings
    constexpr float kMeasuredCeiling = 1.1f;   // E33: both readings, with the 100 px bar too
    CHECK_MSG(unzoomedCeiling == kUnzoomedCeiling, std::to_string(unzoomedCeiling));
    CHECK_MSG(nineCeiling == kNineCeiling, std::to_string(nineCeiling));
    CHECK_MSG(with48 == kWith48Ceiling && with48InWindowPixels == kWith48Ceiling, std::to_string(with48) + " " + std::to_string(with48InWindowPixels));
    CHECK_MSG(ceiling == kMeasuredCeiling, std::to_string(ceiling));
    CHECK_MSG(ceilingInWindowPixels == kMeasuredCeiling, std::to_string(ceilingInWindowPixels));
    CHECK(ceiling <= with48 && with48 <= nineCeiling && nineCeiling <= TouchTuning::kMaxSize && unzoomedCeiling <= TouchTuning::kMaxSize);
    // E33: every ceiling is below the editor's top size, so the chain above says something. One step up, at 1.2, the notched 4:3
    // screens have exactly this pair overlapping in every set (the direction control's right button reaching the sword button: a
    // notch's two sides leave a 4:3 screen narrow), and the in-play set in E1's pixels adds the fire button against the pause button
    // under a 100 px bottom bar on the 20:9 phone: the fire button is the top of the right column, the higher one, and the pause
    // button hangs over that column. At 1.3 the fire button meets the pause button on the phone and the 16:9 tablet under the 100 px
    // bar and on the phone under the 48 px bar, and on the 4:3 tablet under the 100 px bar (E1's pixels), on the phone under the
    // 100 px bar in window pixels; at the top size on most other shapes with a bar, a notch or a tablet's status bar too (its 30 px
    // top inset pushes the pause button down), as the lists below say. No other pair overlaps up to the top size (E32: the down
    // button met the sword button and the sword combo).
    const auto overlapsAt = [&](float size, const std::vector<InPlayCase>& set) { return judge(size, set).overlaps; };   // E29
    using Lines = std::vector<std::string>;   // E29
    const std::string notchedPair = "4:3 tablet, bars 88/0/24: dpad right and sword";   // E33
    const std::string notchedUnzoomed = "1024x768 unzoomed, inset 88/0: dpad right and sword";   // E33
    const auto firePause = [](const char* shape) { return std::string(shape) + ": fire and pause"; };   // E33
    for (const std::vector<InPlayCase>* set : std::initializer_list<const std::vector<InPlayCase>*>{&unzoomed, &nine, &inPlay, &inPlayWindowPixels}) {   // E33
        CHECK((overlapsAt(1.1f, *set).empty()));   // E33
    }   // E33
    CHECK((overlapsAt(1.2f, unzoomed) == Lines{notchedUnzoomed}));   // E33
    CHECK((overlapsAt(1.2f, nine) == Lines{notchedPair}));   // E33
    CHECK((overlapsAt(1.2f, inPlay) == Lines{firePause("20:9 phone, bars 0/0/100"), notchedPair}));   // E33
    CHECK((overlapsAt(1.2f, inPlayWindowPixels) == Lines{notchedPair}));   // E33
    CHECK((overlapsAt(1.3f, unzoomed) == Lines{notchedUnzoomed}));   // E33
    CHECK((overlapsAt(1.3f, nine) == Lines{notchedPair}));   // E33
    CHECK((overlapsAt(1.3f, inPlay) ==   // E33
           Lines{firePause("20:9 phone, bars 0/0/48"), firePause("20:9 phone, bars 0/0/100"), firePause("16:9 tablet, bars 0/0/100"), notchedPair,   // E33
                 firePause("4:3 tablet, bars 0/0/100")}));   // E33
    CHECK((overlapsAt(1.3f, inPlayWindowPixels) == Lines{firePause("20:9 phone, bars 0/0/100"), notchedPair}));   // E33
    CHECK((overlapsAt(1.4f, unzoomed) ==   // E33
           Lines{notchedUnzoomed, firePause("1024x768 unzoomed, inset 0/30"), firePause("1366x768 unzoomed, inset 0/30"),   // E33
                 firePause("1707x768 unzoomed, inset 0/30")}));   // E33
    CHECK((overlapsAt(1.4f, nine) ==   // E33
           Lines{firePause("20:9 phone, bars 88/0/24"), firePause("20:9 phone, bars 0/30/20"), firePause("16:9 tablet, bars 0/30/20"), notchedPair,   // E33
                 firePause("4:3 tablet, bars 0/30/20")}));   // E33
    CHECK((overlapsAt(1.4f, inPlay) ==   // E33
           Lines{firePause("20:9 phone, bars 88/0/24"), firePause("20:9 phone, bars 0/30/20"), firePause("20:9 phone, bars 0/0/48"),   // E33
                 firePause("20:9 phone, bars 0/0/100"), firePause("16:9 tablet, bars 0/30/20"), firePause("16:9 tablet, bars 0/0/48"),   // E33
                 firePause("16:9 tablet, bars 0/0/100"), notchedPair, firePause("4:3 tablet, bars 0/30/20"), firePause("4:3 tablet, bars 0/0/48"),   // E33
                 firePause("4:3 tablet, bars 0/0/100")}));   // E33
    CHECK((overlapsAt(1.4f, inPlayWindowPixels) ==   // E33
           Lines{firePause("20:9 phone, bars 88/0/24"), firePause("20:9 phone, bars 0/30/20"), firePause("20:9 phone, bars 0/0/48"),   // E33
                 firePause("20:9 phone, bars 0/0/100"), firePause("16:9 tablet, bars 0/30/20"), firePause("16:9 tablet, bars 0/0/100"), notchedPair,   // E33
                 firePause("4:3 tablet, bars 0/30/20"), firePause("4:3 tablet, bars 0/0/100")}));   // E33
    // The fire button's top, the top of the cluster and the one under the pause button, and the light's, the top of the left column
    // (30 lower, and not under the pause: the pause is over the right column), against the pause button's bottom, in 768ths of the
    // screen: the least clearance over a set, at a size.   // E33
    const auto pauseClearance = [&](float size, const std::vector<InPlayCase>& set, TouchControl top = TouchControl::Fire) {   // E29
        const TouchManifest tuned = TouchControls::WithTuning(built, SizedAt(size));   // E29
        float least = std::numeric_limits<float>::max();   // E29
        for (const InPlayCase& one : set) {   // E29
            const TouchLayout layout = TouchControls::ComputeLayout(tuned, one.screen, one.safe, one.unit, one.frame);   // E29
            least = std::min(least, (layout[top].min.y - layout[TouchControl::Pause].max.y) / one.unit);   // E29
        }   // E29
        return least;   // E29
    };   // E29
    std::printf("  E33 the fire button's (the cluster's top) least clearance of the pause (of 768): size 1.0 %.1f, 1.1 %.1f, 1.2 %.1f, 1.3 %.1f, 1.4 %.1f (E1's 768 bars); "
                "%.1f, %.1f, %.1f, %.1f, %.1f (window pixels); the spell combo's at 1.4: %.1f, %.1f\n",   // E33
                pauseClearance(1.0f, inPlay), pauseClearance(1.1f, inPlay), pauseClearance(1.2f, inPlay), pauseClearance(1.3f, inPlay),   // E33
                pauseClearance(1.4f, inPlay), pauseClearance(1.0f, inPlayWindowPixels), pauseClearance(1.1f, inPlayWindowPixels),   // E33
                pauseClearance(1.2f, inPlayWindowPixels), pauseClearance(1.3f, inPlayWindowPixels), pauseClearance(1.4f, inPlayWindowPixels),   // E33
                pauseClearance(1.4f, inPlay, TouchControl::SpellCombo), pauseClearance(1.4f, inPlayWindowPixels, TouchControl::SpellCombo));   // E33
    std::printf("  E33 the light's (the left column's top, clear of the pause sideways): size 1.0 %.1f, 1.1 %.1f, 1.2 %.1f, 1.3 %.1f, 1.4 %.1f (E1's 768 bars); "
                "%.1f, %.1f, %.1f, %.1f, %.1f (window pixels)\n",   // E33
                pauseClearance(1.0f, inPlay, TouchControl::Light), pauseClearance(1.1f, inPlay, TouchControl::Light),   // E33
                pauseClearance(1.2f, inPlay, TouchControl::Light), pauseClearance(1.3f, inPlay, TouchControl::Light),   // E33
                pauseClearance(1.4f, inPlay, TouchControl::Light), pauseClearance(1.0f, inPlayWindowPixels, TouchControl::Light),   // E33
                pauseClearance(1.1f, inPlayWindowPixels, TouchControl::Light), pauseClearance(1.2f, inPlayWindowPixels, TouchControl::Light),   // E33
                pauseClearance(1.3f, inPlayWindowPixels, TouchControl::Light), pauseClearance(1.4f, inPlayWindowPixels, TouchControl::Light));   // E33
    // Default size: at least 70 clear on every case, the 100 px bar's included, for the fire button (measured 77.9 of E1's 768 and
    // 106.8 in window pixels) and for the light (107.9 and 136.8).   // E33
    for (const TouchControl top : {TouchControl::Fire, TouchControl::Light}) {   // E33
        CHECK_MSG(pauseClearance(1.0f, inPlay, top) >= 70.0f, std::to_string(pauseClearance(1.0f, inPlay, top)));   // E33
        CHECK_MSG(pauseClearance(1.0f, inPlayWindowPixels, top) >= 70.0f, std::to_string(pauseClearance(1.0f, inPlayWindowPixels, top)));   // E33
    }   // E33
    // Where the fire button's clearance is used up, on the tall bar: still clear at 1.1 and over at 1.2 in E1's pixels (E32: the same;
    // E29: 1.2 and 1.3), still clear at 1.2 and over at 1.3 in window pixels (E32: the same).   // E33
    CHECK_MSG(pauseClearance(1.1f, inPlay) >= 0.0f && pauseClearance(1.2f, inPlay) < 0.0f, std::to_string(pauseClearance(1.1f, inPlay)));   // E33
    CHECK_MSG(pauseClearance(1.2f, inPlayWindowPixels) >= 0.0f && pauseClearance(1.3f, inPlayWindowPixels) < 0.0f,   // E33
              std::to_string(pauseClearance(1.2f, inPlayWindowPixels)));   // E33
    // Every size up to the ceiling is clean on every case, and the default one on all the sets.
    for (int step = 4; step <= 10; ++step) {
        CHECK_MSG(judge(static_cast<float>(step) / 10.0f, inPlay).overlaps.empty(), std::to_string(step));
        CHECK_MSG(judge(static_cast<float>(step) / 10.0f, inPlayWindowPixels).overlaps.empty(), std::to_string(step));
    }
    CHECK(judge(1.0f, nine).overlaps.empty() && judge(1.0f, unzoomed).overlaps.empty());
}

// E29, E32, E33: the six action buttons' arrangement, on the three screens the layout is checked on: two staggered columns of
// three, the right one the higher; the sword at the bottom of the left one and the jump button at the bottom of the right one;
// above the sword the sword combo and the light; above the jump the spell combo and the fire. Every box, as literals (the   // E33
// offsets are the manifest's: the right column 40 from the right edge, the left 176 (16 between the columns), the combos 10   // E33
// further in than their buttons, the left column's rows at 34, 170 and 286 and the right column's, 30 higher, at 64, 200 and 316,   // E33
// 16 between neighbours, so a column's rows are 126 apart, centre to centre).   // E33
void testButtonColumns() {   // E29
    struct Expect {   // E29
        TouchControl control;   // E29
        glm::vec2 min;    // on the 4:3 screen, 1024 wide   // E29
        glm::vec2 max;   // E29
    };   // E29
    const Expect box4x3[] = {   // E29
        {TouchControl::Jump, {864.0f, 584.0f}, {984.0f, 704.0f}},         // right column, bottom: 30 higher than the sword   // E33
        {TouchControl::SpellCombo, {874.0f, 468.0f}, {974.0f, 568.0f}},   // above the jump   // E33
        {TouchControl::Fire, {864.0f, 332.0f}, {984.0f, 452.0f}},         // above the spell combo   // E33
        {TouchControl::Sword, {728.0f, 614.0f}, {848.0f, 734.0f}},        // left column, bottom   // E33
        {TouchControl::SwordCombo, {738.0f, 498.0f}, {838.0f, 598.0f}},   // above the sword   // E33
        {TouchControl::Light, {728.0f, 362.0f}, {848.0f, 482.0f}},        // above the sword combo   // E33
    };   // E29
    std::string warning;   // E29
    const std::pair<const char*, TouchManifest> manifests[] = {   // E29
        {"built in", TouchControls::DefaultManifest()},   // E29
        {"shipped", TouchControls::LoadManifest(ShippedManifest(), &warning)},   // E29
    };   // E29
    CHECK_MSG(warning.empty(), warning);   // E29
    for (const auto& [name, manifest] : manifests) {   // E29
        // The right-anchored boxes follow the screen's right edge: the same numbers shifted by its width less 1024.   // E29
        for (const glm::vec2 screen : {kFourThree, kWide, glm::vec2(1707.0f, 768.0f)}) {   // E29
            const TouchLayout layout = TouchControls::ComputeLayout(manifest, screen, TouchInsets{});   // E29
            const float shift = screen.x - kFourThree.x;   // E29
            for (const Expect& entry : box4x3) {   // E29
                CHECK_MSG(BoxIs(layout[entry.control], entry.min + glm::vec2(shift, 0.0f), entry.max + glm::vec2(shift, 0.0f), 0.001f),   // E29
                          std::string(name) + " " + TouchControls::ControlId(entry.control) + " on " + std::to_string(static_cast<int>(screen.x)));   // E29
            }   // E29
            // The sizes and the art are what they were: 120 and 100.   // E29
            for (const TouchControl control : {TouchControl::Jump, TouchControl::Sword, TouchControl::Fire, TouchControl::Light}) {   // E29
                CHECK(layout[control].Size() == glm::vec2(120.0f));   // E29
            }   // E29
            for (const TouchControl control : {TouchControl::SwordCombo, TouchControl::SpellCombo}) {   // E29
                CHECK(layout[control].Size() == glm::vec2(100.0f));   // E29
            }   // E29
            CHECK(layout.hitPadding[Slot(TouchControl::Jump)] == 4.0f && layout.hitPadding[Slot(TouchControl::SpellCombo)] == 6.0f);   // E29
        }   // E29
        // A notch and a gesture bar move the cluster with the safe area's right and bottom edges, not the screen's.   // E29
        const TouchInsets notch{88.0f, 0.0f, 88.0f, 24.0f};   // E29
        const TouchLayout inset = TouchControls::ComputeLayout(manifest, kWide, notch);   // E29
        // The safe area ends at (1278, 744): the jump button's box is 40 and 64 in from it, 120 square.   // E33
        CHECK(BoxIs(inset[TouchControl::Jump], {1118.0f, 560.0f}, {1238.0f, 680.0f}, 0.001f));   // E33
        CHECK(BoxIs(inset[TouchControl::Sword], {982.0f, 590.0f}, {1102.0f, 710.0f}, 0.001f));   // E33
        CHECK(BoxIs(inset[TouchControl::Fire], {1118.0f, 308.0f}, {1238.0f, 428.0f}, 0.001f));   // E33
        CHECK(BoxIs(inset[TouchControl::Light], {982.0f, 338.0f}, {1102.0f, 458.0f}, 0.001f));   // E33
        CHECK(BoxIs(inset[TouchControl::SwordCombo], {992.0f, 474.0f}, {1092.0f, 574.0f}, 0.001f));   // E33
        CHECK(BoxIs(inset[TouchControl::SpellCombo], {1128.0f, 444.0f}, {1228.0f, 544.0f}, 0.001f));   // E33
    }   // E29

    // What a finger means in the cluster: each button's centre is its own and nothing else's, on every screen, and   // E29
    // the two combos start theirs. And the middle of each gap between neighbours is no button at all.   // E29
    const TouchLayout layout = Default();   // E29
    const std::pair<TouchControl, TouchAction> single[] = {{TouchControl::Jump, TouchAction::Jump},   // E29
                                                           {TouchControl::Sword, TouchAction::Sword},   // E29
                                                           {TouchControl::Fire, TouchAction::Fire},   // E29
                                                           {TouchControl::Light, TouchAction::Light}};   // E29
    for (const auto& [control, action] : single) {   // E29
        TouchControls touch;   // E29
        CHECK_MSG(Only(touch.Update(Play({Finger(1, layout[control].Centre())})), {action}), TouchControls::ControlId(control));   // E29
    }   // E29
    {   // E29
        TouchControls sword;   // E29
        sword.Update(Play({Finger(1, layout[TouchControl::SwordCombo].Centre())}));   // E29
        CHECK(sword.RunningCombo() == TouchCombo::Sword);   // E29
        TouchControls spell;   // E29
        spell.Update(Play({Finger(1, layout[TouchControl::SpellCombo].Centre())}));   // E29
        CHECK(spell.RunningCombo() == TouchCombo::Spell);   // E29
    }   // E29
    // E33: the middles of the gaps, from the layout: between the columns (16 wide, 8 once each button's 4 of padding is taken off:   // E33
    // x 856 on the 4:3 screen) where a button of one column and a button of the other are at the same height (the columns are   // E33
    // staggered, so that is the stretch the two share), and between the combos (36 wide, 24 once their 6 of padding is taken off);   // E33
    // between the rows in each column (16 clear each side of a combo, 6 once its 6 and the button's 4 of padding are taken off);   // E33
    // and the dead strip right of the cluster.   // E33
    const TouchLayout::Box& jump = layout[TouchControl::Jump];   // E32
    const TouchLayout::Box& sword = layout[TouchControl::Sword];   // E32
    const TouchLayout::Box& fire = layout[TouchControl::Fire];   // E32
    const TouchLayout::Box& light = layout[TouchControl::Light];   // E32
    const TouchLayout::Box& swordCombo = layout[TouchControl::SwordCombo];   // E32
    const TouchLayout::Box& spellCombo = layout[TouchControl::SpellCombo];   // E32
    const auto mid = [](float a, float b) { return 0.5f * (a + b); };   // E32
    const float between = mid(sword.max.x, jump.min.x);   // E32
    const float combosBetween = mid(swordCombo.max.x, spellCombo.min.x);   // E33
    const glm::vec2 gaps[] = {   // E32
        {between, mid(sword.min.y, jump.max.y)},   // between the sword and the jump button, half way up the stretch they share   // E33
        {between, sword.min.y + 10.0f},   // the same, near the sword's top (the jump's top is 30 higher)   // E33
        {between, jump.max.y - 10.0f},   // the same, near the jump button's bottom (the sword's is 30 lower)   // E33
        {between, mid(light.min.y, fire.max.y)},   // between the light and the fire   // E33
        {combosBetween, mid(swordCombo.min.y, spellCombo.max.y)},   // between the sword combo and the spell combo   // E33
        {swordCombo.Centre().x, mid(sword.min.y, swordCombo.max.y)},   // between the sword and the sword combo   // E32
        {swordCombo.Centre().x, mid(swordCombo.min.y, light.max.y)},   // between the sword combo and the light   // E32
        {spellCombo.Centre().x, mid(jump.min.y, spellCombo.max.y)},   // between the jump and the spell combo   // E32
        {spellCombo.Centre().x, mid(spellCombo.min.y, fire.max.y)},   // between the spell combo and the fire   // E32
        {mid(jump.max.x, kFourThree.x), jump.Centre().y},   // right of the cluster: the strip down the screen's edge   // E32
    };   // E29
    for (const glm::vec2 point : gaps) {   // E29
        TouchControls touch;   // E29
        CHECK_MSG(Only(touch.Update(Play({Finger(1, point)})), {}), std::to_string(point.x) + "," + std::to_string(point.y));   // E29
        CHECK_MSG(touch.RunningCombo() == TouchCombo::None, std::to_string(point.x) + "," + std::to_string(point.y));   // E29
    }   // E29
    // The first ray out of each column is clear too: a thumb on the right column's edge side reaches no left button.   // E29
    CHECK(Only(TouchControls().Update(Play({Finger(1, glm::vec2(1012.0f, jump.Centre().y))})), {}));   // right of the jump, past its padding   // E32
    // E32, E33: the notched 4:3 screen (848 units wide, insets 88/0/88/24), where under E32 the direction control's right button and   // E33
    // the sword button were 10 apart and a finger just inside the sword's padding was within the disc's reach too (TouchControls::hit   // E33
    // takes the nearer centre); the sword's box now starts 138 from the button's face and the padded box is out of the disc's reach.   // E33
    // A finger just inside the sword's padding is the sword's alone, and one at the middle of the right button's face is Right alone.   // E33
    {   // E32
        const TouchInsets notch{88.0f, 0.0f, 88.0f, 24.0f};   // E32
        const TouchManifest manifest = TouchControls::DefaultManifest();   // E32
        const TouchLayout notched = TouchControls::ComputeLayout(manifest, kFourThree, notch);   // E32
        const glm::vec2 onSword(notched[TouchControl::Sword].min.x - 0.75f * notched.hitPadding[Slot(TouchControl::Sword)],   // E32
                                notched[TouchControl::Sword].Centre().y);   // E32
        const glm::vec2 onRight = ArrowFace(manifest.dpadRight, notched[TouchControl::Dpad]).Centre();   // E32
        TouchInput swordFinger = Play({Finger(1, onSword)}, kFourThree);   // E32
        swordFinger.safeArea = notch;   // E32
        CHECK(Only(TouchControls().Update(swordFinger), {TouchAction::Sword}));   // E32
        TouchInput rightFinger = Play({Finger(1, onRight)}, kFourThree);   // E32
        rightFinger.safeArea = notch;   // E32
        CHECK(Only(TouchControls().Update(rightFinger), {TouchAction::Right}));   // E32
    }   // E32
}   // E29

// The editor's words for each control are settings.json's keys and the manifest's ids.
void testTuningNames() {
    for (int i = 0; i < kTouchControlCount; ++i) {
        CHECK_MSG(std::string(TouchTuning::ControlKey(i)) == TouchControls::ControlId(static_cast<TouchControl>(i)),
                  std::to_string(i));
        CHECK_MSG(TouchTuning::IsMovableIndex(i) == TouchControls::Movable(static_cast<TouchControl>(i)), std::to_string(i));
    }
    CHECK(std::string(TouchTuning::ControlKey(-1)).empty() && std::string(TouchTuning::ControlKey(kTouchControlCount)).empty());
    CHECK(!TouchTuning::IsMovableIndex(-1) && !TouchTuning::IsMovableIndex(kTouchControlCount));
    CHECK(TouchTuning::IsMovableIndex(0) && TouchTuning::IsMovableIndex(8) && !TouchTuning::IsMovableIndex(9));
    CHECK_EQ(Penumbra::Render::kTuningControls, kTouchControlCount);
    CHECK_EQ(Penumbra::Render::kTuningBackIndex, static_cast<int>(TouchControl::Back));
    int movable = 0;
    for (const TouchControl control : kMovable) movable += TouchControls::Movable(control) ? 1 : 0;
    CHECK_EQ(movable, 9);
}

void runTests() {
    testKeys();
    testEachButton();
    testDirections();
    testManyFingers();
    testWhereItLanded();
    testPause();
    testMenus();
    testPlatformMouse();
    testShownWhere();
    testExitDown();   // E25
    testDpadLowered();   // E25
    testTouchUnit();  // E25
    testPauseInHudFrame();   // E26
    testCorner();
    testSceneChanges();
    testLatch();
    testLayout();
    testButtonColumns();   // E29
    testWideMenuCorner();
    testManifest();
    testSetting();
    testComboTimelines();
    testComboFingers();
    testComboTaps();
    testComboCancel();
    testComboManifest();
    testComboBuffer();
    testComboCooldownGate();   // E37
    testComboCooldownFingers();   // E37
    testComboCooldownReset();   // E37
    testComboCooldownLook();   // E37
    testComboTypedUnlimited();   // E37
    // E28: the player's own layout, all pure: before the ones that boot the real game.
    testTuningIdentity();   // E28
    testTuningSize();   // E28
    testTuningOpacity();   // E28
    testTuningMoves();   // E28
    testMoveFor();   // E28
    testGrabBox();   // E28
    testEditScene();   // E28
    testSizeCeiling();   // E28
    testTuningNames();   // E28
    // Last: they boot the real game, whose globals outlive it.
    testComboInGame();
    testExitDownInGame();   // E25
    testCoopUnzoomsInGame();   // E25
    testTouchHudInGame();   // E26
    testPhoneVersusWithPad();
}

} // namespace

TEST_MAIN("test_pn_render_touch", 300)
