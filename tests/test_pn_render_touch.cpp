// E16, the on-screen touch controls (render/TouchControls): which key each
// control presses and that the Eth runtime sees it as the keyboard's (KS_HIT,
// then KS_DOWN, then KS_RELEASE); the direction control's sectors and a thumb
// sliding between them; several fingers at once; a finger's control being the
// one it landed on; the pause opened and tapped through E13's own pieces; a
// tap in a menu as a click at that point; what is shown where; the layout on
// 4:3 and widescreen screens with and without a safe area; the manifest and
// its art; the touchControls setting.
// Pure: no window, no Machine, no original files.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <iterator>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <entt/entt.hpp>

#include "TestHarness.hpp"

#include "eth/Input.hpp"
#include "eth/Snapshot.hpp"
#include "render/FontAtlas.hpp"
#include "render/HudRenderer.hpp"
#include "render/Localization.hpp"
#include "render/PauseMenu.hpp"
#include "render/Settings.hpp"
#include "render/TextureCache.hpp"
#include "render/TextureDecode.hpp"
#include "render/TouchControls.hpp"
#include "render/View.hpp"

namespace {

namespace fs = std::filesystem;
using namespace Penumbra::Eth;
using Penumbra::Render::HudRenderer;
using Penumbra::Render::PauseMenu;
using Penumbra::Render::PauseStep;
using Penumbra::Render::Settings;
using Penumbra::Render::TouchAction;
using Penumbra::Render::TouchAnchor;
using Penumbra::Render::TouchContact;
using Penumbra::Render::TouchControl;
using Penumbra::Render::TouchControls;
using Penumbra::Render::TouchCorner;
using Penumbra::Render::TouchInput;
using Penumbra::Render::TouchInsets;
using Penumbra::Render::TouchLayout;
using Penumbra::Render::TouchManifest;
using Penumbra::Render::TouchScene;
using Penumbra::Render::TouchStep;
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

// Exactly these are held (and no pointer).
bool Only(const TouchStep& step, std::initializer_list<TouchAction> actions) {
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

void testDirections() {
    const TouchManifest manifest = TouchControls::DefaultManifest();
    const float radius = 0.5f * manifest[TouchControl::Dpad].size.x;
    const float dead = manifest.deadZone * radius;
    const auto steer = [](glm::vec2 offset) {
        TouchControls touch;
        return touch.Update(Play({Finger(1, Dpad(offset))}));
    };
    CHECK(Only(steer({0.0f, 0.0f}), {}));
    CHECK(Only(steer({dead - 2.0f, 0.0f}), {}));                 // the dead zone
    CHECK(Only(steer({-dead - 2.0f, 0.0f}), {TouchAction::Left}));
    CHECK(Only(steer({-100.0f, 0.0f}), {TouchAction::Left}));
    CHECK(Only(steer({100.0f, 0.0f}), {TouchAction::Right}));
    // Straight down is down alone: a thumb held down at the next_level door
    // (main.as:208) does not walk off it.
    CHECK(Only(steer({0.0f, 100.0f}), {TouchAction::Down}));
    CHECK(Only(steer({30.0f, 100.0f}), {TouchAction::Down}));    // 16.7 degrees off: still straight
    CHECK(Only(steer({-80.0f, 80.0f}), {TouchAction::Left, TouchAction::Down}));
    CHECK(Only(steer({80.0f, 80.0f}), {TouchAction::Right, TouchAction::Down}));
    CHECK(Only(steer({-100.0f, 30.0f}), {TouchAction::Left}));   // 16.7 degrees below: still level
    CHECK(Only(steer({-100.0f, 45.0f}), {TouchAction::Left, TouchAction::Down}));
    // No up: straight up is nothing, up-left is left.
    CHECK(Only(steer({0.0f, -100.0f}), {}));
    CHECK(Only(steer({-80.0f, -80.0f}), {TouchAction::Left}));
    CHECK(Only(steer({80.0f, -80.0f}), {TouchAction::Right}));
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
    // command a frame, Left before Down): down first, then down-left.
    TouchControls spell;
    InputState spellState;
    spellState.Update(FrameOf(spell.Update(Play({Finger(5, Dpad({0.0f, 100.0f}))}))));
    CHECK(spellState.GetKeyState(K_DOWN) == KS_HIT);
    CHECK(spellState.GetKeyState(K_LEFT) == KS_UP);
    spellState.Update(FrameOf(spell.Update(Play({Finger(5, Dpad({-80.0f, 80.0f}))}))));
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

    // In play: the disc, its three arrows, the knob, four buttons, pause.
    const TouchManifest manifest = TouchControls::DefaultManifest();
    const std::uint8_t idle = static_cast<std::uint8_t>(std::lround(manifest.idleAlpha * 255.0f));
    const std::uint8_t pressed = static_cast<std::uint8_t>(std::lround(manifest.pressedAlpha * 255.0f));
    CHECK(pressed > idle);
    touch.Update(Play());
    out.clear();
    touch.AppendOverlay(out);
    CHECK_EQ(out.size(), std::size_t{10});
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
        const bool expected = cmd.sprite.find("sword.png") != std::string::npos ||
                              cmd.sprite.find("dpad_right.png") != std::string::npos ||
                              cmd.sprite.find("dpad_knob.png") != std::string::npos;
        CHECK_MSG(expected, cmd.sprite);
    }
    CHECK_EQ(bright, 3);
    // The knob follows the thumb, right of the centre.
    for (const HudCmd& cmd : out) {
        if (cmd.sprite.find("dpad_knob.png") == std::string::npos) continue;
        CHECK((cmd.pos + cmd.size * 0.5f).x > Dpad({0.0f, 0.0f}).x + 10.0f);
    }

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
    CHECK_EQ(out.size(), std::size_t{6});   // the disc, four buttons, pause; no arrows or knob
    for (const HudCmd& cmd : out) CHECK(cmd.kind == HudCmd::Kind::Rectangle);
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

bool Overlap(const TouchLayout::Box& a, const TouchLayout::Box& b) {
    return a.min.x < b.max.x && b.min.x < a.max.x && a.min.y < b.max.y && b.min.y < a.max.y;
}

// How far from its centre a finger still lands on a round control.
float Reach(const TouchLayout& layout, TouchControl control) {
    return 0.5f * layout[control].Size().x + layout.hitPadding[static_cast<std::size_t>(control)];
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

void testLayout() {
    const TouchManifest manifest = TouchControls::DefaultManifest();
    // 4:3 (the menus, and 4:3 levels), 16:9, a 20:9 phone; bare, a notch and
    // gesture bar in landscape, a tablet's status bar.
    const glm::vec2 screens[] = {kFourThree, kWide, {1707.0f, 768.0f}};
    const TouchInsets insets[] = {{}, {88.0f, 0.0f, 88.0f, 24.0f}, {0.0f, 30.0f, 0.0f, 20.0f}};
    const TouchControl play[] = {TouchControl::Dpad, TouchControl::Jump, TouchControl::Sword, TouchControl::Fire,
                                 TouchControl::Light, TouchControl::Pause};
    for (const glm::vec2& screen : screens) {
        for (const TouchInsets& safe : insets) {
            const std::string where = std::to_string(static_cast<int>(screen.x)) + "x768, inset " +
                                      std::to_string(static_cast<int>(safe.left)) + "/" +
                                      std::to_string(static_cast<int>(safe.top));
            const TouchLayout layout = TouchControls::ComputeLayout(manifest, screen, safe);
            const glm::vec2 lo(safe.left, safe.top);
            const glm::vec2 hi = screen - glm::vec2(safe.right, safe.bottom);
            for (int i = 0; i < kTouchControlCount; ++i) {
                const TouchControl control = static_cast<TouchControl>(i);
                CHECK_MSG(Inside(layout[control], lo, hi), where + ": " + TouchControls::ControlId(control));
                CHECK(layout[control].Size() == manifest[control].size);
            }
            // Where each is: the disc under the left thumb, the buttons under
            // the right, the pause at the top right, below the run's timer
            // (setupScene.as:337, (W-50, 0), 25 px) and clear of the status
            // frames at the top left (interface.as, 226x74 each).
            CHECK(layout[TouchControl::Dpad].max.x < screen.x * 0.5f);
            CHECK(layout[TouchControl::Dpad].min.y > screen.y * 0.5f);
            for (const TouchControl button : kButtonControls) {
                CHECK(layout[button].min.x > screen.x * 0.5f);
                CHECK(layout[button].min.y > screen.y * 0.4f);
            }
            CHECK(layout[TouchControl::Pause].min.x > screen.x * 0.5f);
            CHECK(layout[TouchControl::Pause].max.y < screen.y * 0.25f);
            CHECK(!Overlap(layout[TouchControl::Pause], {{screen.x - 50.0f, 0.0f}, {screen.x, 30.0f}}));
            CHECK(!Overlap(layout[TouchControl::Pause], {{0.0f, 0.0f}, {452.0f, 74.0f}}));
            CHECK(!Overlap(layout[TouchControl::Dpad], {{0.0f, 0.0f}, {452.0f, 74.0f}}));
            // Nothing drawn over anything else, and no finger reaching two
            // of them (the circles with their padding).
            for (std::size_t a = 0; a < std::size(play); ++a) {
                for (std::size_t b = a + 1; b < std::size(play); ++b) {
                    CHECK_MSG(!DrawnOver(layout, manifest, play[a], play[b]),
                              where + ": " + TouchControls::ControlId(play[a]) + " over " +
                                  TouchControls::ControlId(play[b]));
                    const float apart = glm::length(layout[play[a]].Centre() - layout[play[b]].Centre());
                    CHECK(apart > Reach(layout, play[a]) + Reach(layout, play[b]));
                }
            }
        }
    }

    // The anchor arithmetic, exactly.
    const TouchLayout wide = TouchControls::ComputeLayout(manifest, kWide, {88.0f, 0.0f, 88.0f, 24.0f});
    CHECK(wide[TouchControl::Dpad].min == glm::vec2(88.0f + 40.0f, 768.0f - 24.0f - 40.0f - 260.0f));
    CHECK(wide[TouchControl::Jump].min ==
          glm::vec2(1366.0f - 88.0f - 142.0f - 120.0f, 768.0f - 24.0f - 24.0f - 120.0f));
    CHECK(wide[TouchControl::Pause].min == glm::vec2(1366.0f - 88.0f - 20.0f - 84.0f, 44.0f));

    // An offset past the screen is clamped into the safe area; scale grows
    // sizes, offsets and padding together.
    TouchManifest odd = manifest;
    odd[TouchControl::Jump].offset = glm::vec2(5000.0f, 5000.0f);
    odd[TouchControl::Light].anchor = TouchAnchor::TopLeft;
    const TouchInsets notch{60.0f, 10.0f, 60.0f, 10.0f};
    const TouchLayout clamped = TouchControls::ComputeLayout(odd, kFourThree, notch);
    CHECK(clamped[TouchControl::Jump].min == glm::vec2(60.0f, 10.0f));
    CHECK(clamped[TouchControl::Light].min == glm::vec2(60.0f + 142.0f, 10.0f + 260.0f));
    odd = manifest;
    odd.scale = 1.5f;
    const TouchLayout big = TouchControls::ComputeLayout(odd, kWide, {});
    CHECK(big[TouchControl::Jump].Size() == glm::vec2(180.0f));
    CHECK(big[TouchControl::Jump].max == glm::vec2(1366.0f - 142.0f * 1.5f, 768.0f - 24.0f * 1.5f));
    CHECK_NEAR(big.hitPadding[static_cast<std::size_t>(TouchControl::Jump)], 24.0f);

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

void testManifest() {
    const fs::path dataDir = PENUMBRA_DATA_DIR;
    const fs::path file = dataDir / TouchControls::kManifestFile;
    CHECK_MSG(fs::exists(file), file.generic_string());
    std::string warning;
    const TouchManifest manifest = TouchControls::LoadManifest(file, &warning);
    CHECK_MSG(warning.empty(), warning);

    // Every image it names is there, decodes, fits its box undistorted, has
    // at least the pixels it is drawn at, and has no exact magenta (the HUD
    // keys it out).
    std::vector<std::pair<std::string, glm::vec2>> images;
    for (int i = 0; i < kTouchControlCount; ++i) {
        const TouchControl control = static_cast<TouchControl>(i);
        CHECK_MSG(!manifest[control].image.empty(), TouchControls::ControlId(control));
        images.emplace_back(manifest[control].image, manifest[control].size);
    }
    const glm::vec2 dpadSize = manifest[TouchControl::Dpad].size;
    for (const std::string& arrow : {manifest.dpadLeft, manifest.dpadRight, manifest.dpadDown}) {
        CHECK(!arrow.empty());
        images.emplace_back(arrow, dpadSize);
    }
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
    CHECK(warning.find("sword.anchor") != std::string::npos);
    CHECK(warning.find("sword.offset") != std::string::npos);
    CHECK(warning.find("sword.size") != std::string::npos);
    CHECK(warning.find("kick") != std::string::npos);
    CHECK(warning.find("_note") == std::string::npos);
    CHECK(warning.find("_about") == std::string::npos);
    warning.clear();
    CHECK(TouchControls::LoadManifest(dataDir / "no_such_manifest.json", &warning) == TouchControls::DefaultManifest());
    CHECK(!warning.empty());

    // A rect-shaped control is touched as a rectangle, corners included.
    TouchManifest square = TouchControls::DefaultManifest();
    square[TouchControl::Jump].shape = Penumbra::Render::TouchShape::Rect;
    square[TouchControl::Jump].hitPadding = 0.0f;
    TouchControls rect;
    rect.SetManifest(square);
    const TouchLayout::Box box = Default()[TouchControl::Jump];
    CHECK(Only(rect.Update(Play({Finger(1, box.min + glm::vec2(2.0f))})), {TouchAction::Jump}));
    TouchControls round;
    CHECK(Only(round.Update(Play({Finger(1, box.min + glm::vec2(2.0f))})), {}));
}

void testSetting() {
    const Settings defaults = Settings::Defaults(false);
    CHECK(defaults.touchControls == "auto");
    CHECK(Settings::Defaults(true).touchControls == "auto");

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
    testSceneChanges();
    testLatch();
    testLayout();
    testManifest();
    testSetting();
}

} // namespace

TEST_MAIN("test_pn_render_touch", 300)
