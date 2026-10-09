// The platform side of the port: keys, pads and the keyboard second player
// mapped onto the Eth InputFrame the scripts read, the latch across frames that
// run no tick, the settings file, the audio device with no engine behind it,
// and the pointer on E1's wide menus (Step 25: the view's offset, hits, bars).
// Pure: no window, no GLFW calls, no audio device, no original files.

#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>   // E28
#include <string>
#include <utility>
#include <vector>

#include <GLFW/glfw3.h>

#include "TestHarness.hpp"

#include "eth/Snapshot.hpp"
#include "render/AudioOutEngine.hpp"
#include "render/CameraRig.hpp"
#include "render/InputMapper.hpp"
#include "render/Languages.hpp"
#include "render/PhoneUi.hpp"
#include "render/Settings.hpp"
#include "render/TouchControls.hpp"
#include "render/WideMenus.hpp"
#include "render/WindowMode.hpp"
#include "script/Script.hpp"   // E31: the phone options layout, pure

namespace {

using namespace Penumbra::Eth;
using Penumbra::Render::AudioOutEngine;
using Penumbra::Render::CameraRig;
using Penumbra::Render::ControlAction;
using Penumbra::Render::ControlSettings;
using Penumbra::Render::InputMapper;
using Penumbra::Render::KeyFromName;
using Penumbra::Render::KeyName;
using Penumbra::Render::RawDevices;
using Penumbra::Render::RawPad;
using Penumbra::Render::Settings;
using Penumbra::Render::View;
using Penumbra::Render::WindowAction;
namespace Pad = Supersonic::Pad;
namespace fs = std::filesystem;

ControlSettings Defaults() { return Settings::Defaults("en").controls; }

// The original's controls: no keyboard player 2, and pads in winmm's order
// (the first is joystick 0, player 2's under the default g_controls).
ControlSettings WithoutKeyboardPlayer2() {
    ControlSettings controls = Defaults();
    controls.keyboardPlayer2 = false;
    controls.firstPadIsPlayer1 = false;
    return controls;
}

RawPad Gamepad() {
    RawPad pad;
    pad.gamepad = true;
    pad.axes[Pad::LeftTrigger] = -1.0f;   // at rest
    pad.axes[Pad::RightTrigger] = -1.0f;
    return pad;
}

RawDevices With(std::initializer_list<int> keys) {
    RawDevices raw;
    for (const int key : keys) raw.keys[static_cast<std::size_t>(key)] = true;
    return raw;
}

InputFrame Map(const RawDevices& raw, const ControlSettings& controls, int player2Pad = 0) {
    return InputMapper::Map(raw, controls, glm::vec2(0.0f), player2Pad);
}

int CountKeys(const InputFrame& frame) {
    int count = 0;
    for (const bool down : frame.keys) count += down ? 1 : 0;
    return count;
}

int CountButtons(const InputFrame::Pad& pad) {
    int count = 0;
    for (const bool down : pad.buttons) count += down ? 1 : 0;
    return count;
}

void testKeyboardPlayer1() {
    const ControlSettings controls = Defaults();

    // K_CTRL is either Ctrl (VK_CONTROL), and nothing else goes down with it.
    InputFrame frame = Map(With({GLFW_KEY_LEFT_CONTROL}), controls);
    CHECK(frame.keys[K_CTRL]);
    CHECK_EQ(CountKeys(frame), 1);
    CHECK(Map(With({GLFW_KEY_RIGHT_CONTROL}), controls).keys[K_CTRL]);
    CHECK(Map(With({GLFW_KEY_RIGHT_ALT}), controls).keys[K_ALT]);
    CHECK(Map(With({GLFW_KEY_LEFT_SHIFT}), controls).keys[K_SHIFT]);

    // Both Enters are K_ENTER (VK_RETURN).
    CHECK(Map(With({GLFW_KEY_ENTER}), controls).keys[K_ENTER]);
    CHECK(Map(With({GLFW_KEY_KP_ENTER}), controls).keys[K_ENTER]);

    CHECK(Map(With({GLFW_KEY_S}), controls).keys[K_S]);
    CHECK(Map(With({GLFW_KEY_D}), controls).keys[K_D]);
    CHECK(Map(With({GLFW_KEY_SPACE}), controls).keys[K_SPACE]);
    CHECK(Map(With({GLFW_KEY_ESCAPE}), controls).keys[K_ESC]);
    CHECK(Map(With({GLFW_KEY_UP}), controls).keys[K_UP]);
    CHECK(Map(With({GLFW_KEY_LEFT}), controls).keys[K_LEFT]);
    CHECK(Map(With({GLFW_KEY_PAGE_UP}), controls).keys[K_PAGEUP]);
    CHECK(Map(With({GLFW_KEY_2}), controls).keys[K_2]);
    CHECK(Map(With({GLFW_KEY_F5}), controls).keys[K_F5]);
    CHECK(Map(With({GLFW_KEY_A}), controls).keys[K_A]);
    CHECK(Map(With({GLFW_KEY_EQUAL}), controls).keys[K_PLUS]);
    // The keypad digits were never K_0..K_9.
    CHECK_EQ(CountKeys(Map(With({GLFW_KEY_KP_2}), controls)), 0);

    RawDevices mouse;
    mouse.mouse[0] = true;
    CHECK(Map(mouse, controls).keys[K_LMOUSE]);
    mouse.mouse = {false, true, false};
    CHECK(Map(mouse, controls).keys[K_RMOUSE]);

    // A rebinding MOVES the control: the sword on A drives K_S, and S no
    // longer does (nor does A produce K_A).
    ControlSettings rebound = controls;
    rebound.player1[ControlAction::Sword] = {GLFW_KEY_A};
    CHECK(Map(With({GLFW_KEY_A}), rebound).keys[K_S]);
    CHECK(!Map(With({GLFW_KEY_A}), rebound).keys[K_A]);
    CHECK(!Map(With({GLFW_KEY_S}), rebound).keys[K_S]);

    // Player 1's keys never reach a pad.
    const InputFrame p1 = Map(With({GLFW_KEY_S, GLFW_KEY_LEFT}), controls);
    CHECK_EQ(CountButtons(p1.pads[0]), 0);
    CHECK(p1.pads[0].xy == glm::vec2(0.0f));
}

void testGamepads() {
    const ControlSettings controls = WithoutKeyboardPlayer2();

    // A is jump, JK_03 (playerInput.as:195), on joystick 0 - the first pad.
    RawDevices raw;
    raw.pads.push_back(Gamepad());
    raw.pads[0].buttons[Pad::A] = true;
    InputFrame frame = Map(raw, controls);
    CHECK(frame.pads[0].connected);
    CHECK(frame.pads[0].buttons[JK_03]);
    CHECK_EQ(CountButtons(frame.pads[0]), 1);
    CHECK(!frame.pads[1].connected);
    CHECK_EQ(CountKeys(frame), 0);

    // The rest by meaning.
    const auto buttonFor = [&controls](int glfwButton, J_KEY expected) {
        RawDevices one;
        one.pads.push_back(Gamepad());
        one.pads[0].buttons[static_cast<std::size_t>(glfwButton)] = true;
        const InputFrame mapped = InputMapper::Map(one, controls, glm::vec2(0.0f), 0);
        return mapped.pads[0].buttons[expected] && CountButtons(mapped.pads[0]) == 1;
    };
    CHECK(buttonFor(Pad::X, JK_04));       // sword
    CHECK(buttonFor(Pad::B, JK_02));       // fire
    CHECK(buttonFor(Pad::Y, JK_01));       // light
    CHECK(buttonFor(Pad::Start, JK_10));   // confirm / summon
    CHECK(buttonFor(Pad::Back, JK_09));    // cancel

    // A second pad is joystick 1: player 1's under the default g_controls.
    RawDevices two;
    two.pads.push_back(Gamepad());
    two.pads.push_back(Gamepad());
    two.pads[1].buttons[Pad::A] = true;
    frame = Map(two, controls);
    CHECK(frame.pads[0].connected && frame.pads[1].connected);
    CHECK(!frame.pads[0].buttons[JK_03]);
    CHECK(frame.pads[1].buttons[JK_03]);

    // firstPadIsPlayer1: the lone pad goes to the index player 1 reads.
    ControlSettings firstIsP1 = controls;
    firstIsP1.firstPadIsPlayer1 = true;
    frame = Map(raw, firstIsP1, 0);
    CHECK(!frame.pads[0].connected);
    CHECK(frame.pads[1].buttons[JK_03]);
    frame = Map(raw, firstIsP1, 1);    // g_controls = 1: player 1 reads 0
    CHECK(frame.pads[0].buttons[JK_03]);

    // An unmapped joystick is ignored unless asked for, then read by its own
    // button numbers as winmm did.
    RawDevices unmapped;
    RawPad rawPad;
    rawPad.gamepad = false;
    rawPad.rawButtons[2] = true;
    unmapped.pads.push_back(rawPad);
    CHECK(!Map(unmapped, controls).pads[0].connected);
    ControlSettings rawToo = controls;
    rawToo.rawJoysticks = true;
    CHECK(Map(unmapped, rawToo).pads[0].buttons[JK_03]);
}

// E12: under the shipped defaults (keyboard player 2 on, the first pad for
// player 1) one pad plays the wizard and drives the menu, and a second pad
// joins the keyboard's player 2.
void testLonePadIsPlayer1() {
    const ControlSettings controls = Defaults();
    RawDevices one;
    one.pads.push_back(Gamepad());
    one.pads[0].buttons[Pad::A] = true;
    // g_controls 0: player 1 reads joystick 1, player 2 joystick 0.
    InputFrame frame = Map(one, controls, 0);
    CHECK(frame.pads[1].connected);
    CHECK(frame.pads[1].buttons[JK_03]);            // the wizard's jump
    CHECK(frame.pads[0].connected);                 // the keyboard's player 2, idle
    CHECK(!frame.pads[0].buttons[JK_03]);
    RawDevices two = one;
    two.pads.push_back(Gamepad());
    two.pads[1].buttons[Pad::X] = true;
    frame = Map(two, controls, 0);
    CHECK(frame.pads[1].buttons[JK_03] && !frame.pads[1].buttons[JK_04]);
    CHECK(frame.pads[0].buttons[JK_04]);            // the second pad is the princess's
}

// E22: while the touch controls are on they are player 1, and real pads start
// at the index player 2 reads - never player 1's, whatever E12 says; off
// (E20's options row), E12's order is back from the next tick.
void testTouchIsPlayer1() {
    ControlSettings phone = Defaults();
    phone.keyboardPlayer2 = false;   // a phone's default (Settings.hpp)
    CHECK(phone.firstPadIsPlayer1);  // E12, as shipped
    ControlSettings original = phone;
    original.firstPadIsPlayer1 = false;
    using Order = std::array<int, kMaxJoysticks>;
    constexpr int kNo = InputMapper::kNoPad;

    // The rule. g_controls 0: player 2 reads joystick 0, player 1 joystick 1;
    // g_controls 1: the other way round (playerInput.as:43-53).
    CHECK((InputMapper::PadOrder(phone, 0, true) == Order{0, 2, 3, kNo}));
    CHECK((InputMapper::PadOrder(phone, 1, true) == Order{1, 2, 3, kNo}));
    CHECK((InputMapper::PadOrder(original, 0, true) == Order{0, 2, 3, kNo}));
    CHECK((InputMapper::PadOrder(original, 1, true) == Order{1, 2, 3, kNo}));
    // Off: E12's order, and the original's, exactly as before.
    CHECK((InputMapper::PadOrder(phone, 0, false) == Order{1, 0, 2, 3}));
    CHECK((InputMapper::PadOrder(phone, 1, false) == Order{0, 1, 2, 3}));
    CHECK((InputMapper::PadOrder(original, 0, false) == Order{0, 1, 2, 3}));
    CHECK((InputMapper::PadOrder(original, 1) == Order{0, 1, 2, 3}));

    // One pad, touch on: player 2's, and nothing on player 1's index - no
    // button, no direction, no key.
    RawDevices one;
    one.pads.push_back(Gamepad());
    one.pads[0].buttons[Pad::A] = true;
    one.pads[0].buttons[Pad::DpadRight] = true;
    InputFrame frame = InputMapper::Map(one, phone, glm::vec2(0.0f), 0, {}, true);
    CHECK(frame.pads[0].connected && frame.pads[0].buttons[JK_03]);
    CHECK_NEAR(frame.pads[0].xy.x, 1.0f);
    CHECK(!frame.pads[1].connected);
    CHECK_EQ(CountButtons(frame.pads[1]), 0);
    CHECK(frame.pads[1].xy == glm::vec2(0.0f));
    CHECK_EQ(CountKeys(frame), 0);
    frame = InputMapper::Map(one, phone, glm::vec2(0.0f), 1, {}, true);   // g_controls 1
    CHECK(frame.pads[1].connected && frame.pads[1].buttons[JK_03]);
    CHECK(!frame.pads[0].connected);
    // The same pad with touch off: E12 makes it player 1's, as before.
    frame = InputMapper::Map(one, phone, glm::vec2(0.0f), 0, {}, false);
    CHECK(frame.pads[1].buttons[JK_03] && !frame.pads[0].connected);

    // A second pad changes nothing for player 1: it goes where no player
    // reads. A fourth is not presented at all (three indices besides player 1's).
    RawDevices four = one;
    for (int i = 0; i < 3; ++i) four.pads.push_back(Gamepad());
    four.pads[1].buttons[Pad::X] = true;
    four.pads[3].buttons[Pad::Y] = true;
    frame = InputMapper::Map(four, phone, glm::vec2(0.0f), 0, {}, true);
    CHECK(frame.pads[0].buttons[JK_03] && !frame.pads[0].buttons[JK_04]);
    CHECK(!frame.pads[1].connected);
    CHECK(frame.pads[2].connected && frame.pads[2].buttons[JK_04]);
    CHECK(frame.pads[3].connected && CountButtons(frame.pads[3]) == 0);
    for (const InputFrame::Pad& pad : frame.pads) CHECK(!pad.buttons[JK_01]);   // the fourth's Y: nowhere

    // E14 in the menus: the pad's A also confirms on player 2's index only;
    // the menus read player 1's, which a pad never reaches under touch.
    frame = InputMapper::Map(one, phone, glm::vec2(0.0f), 0, Penumbra::Render::MenuButtons{true, 0, 0}, true);
    CHECK(frame.pads[0].buttons[JK_10]);
    CHECK(!frame.pads[1].buttons[JK_10]);

    // A desktop with --touch and keyboard player 2 on: the keys and the pad
    // share player 2's index, as they do without touch.
    RawDevices keysAndPad = one;
    keysAndPad.keys[GLFW_KEY_U] = true;
    frame = InputMapper::Map(keysAndPad, Defaults(), glm::vec2(0.0f), 0, {}, true);
    CHECK(frame.pads[0].buttons[JK_03] && frame.pads[0].buttons[JK_04]);
    CHECK(!frame.pads[1].connected);

    // The mapper, switched on, off (E20's row) and on again: the lone pad
    // follows at the next tick.
    View view;
    InputMapper mapper;
    mapper.SetControls(Defaults());   // keyboard player 2 on: index 0 is connected either way
    mapper.SetPlayer2Pad(0);
    CHECK(!mapper.TouchPlaysPlayer1());
    RawDevices idle;
    idle.pads.push_back(Gamepad());
    frame = mapper.BuildTick(one, view);
    CHECK(frame.pads[1].buttons[JK_03] && !frame.pads[0].buttons[JK_03]);
    mapper.EndFrame(one);
    mapper.SetTouchPlaysPlayer1(true);
    CHECK(mapper.TouchPlaysPlayer1());
    frame = mapper.BuildTick(one, view);
    CHECK(frame.pads[0].buttons[JK_03] && !frame.pads[1].connected);
    mapper.EndFrame(one);
    // A press made in a frame that ran no tick, then touch off before the
    // next tick: the latch was the pad's at player 2's index, which is now
    // the keyboard's alone - it is dropped, not handed to the keyboard.
    frame = mapper.BuildTick(idle, view);
    mapper.EndFrame(idle);
    mapper.EndFrame(one);
    mapper.SetTouchPlaysPlayer1(false);
    frame = mapper.BuildTick(idle, view);
    CHECK(frame.pads[0].connected && !frame.pads[0].buttons[JK_03]);
    CHECK(frame.pads[1].connected && !frame.pads[1].buttons[JK_03]);
    mapper.EndFrame(idle);
    frame = mapper.BuildTick(one, view);
    CHECK(frame.pads[1].buttons[JK_03] && !frame.pads[0].buttons[JK_03]);
    mapper.EndFrame(one);
}

void testSticks() {
    const ControlSettings controls = WithoutKeyboardPlayer2();
    const auto stick = [&controls](float x, float y) {
        RawDevices raw;
        raw.pads.push_back(Gamepad());
        raw.pads[0].axes[Pad::LeftX] = x;
        raw.pads[0].axes[Pad::LeftY] = y;
        return InputMapper::Map(raw, controls, glm::vec2(0.0f), 0).pads[0].xy;
    };
    // A resting stick's drift must not walk the character: getInputDirection
    // walks on any nonzero x (playerInput.as:166-174).
    CHECK_NEAR(stick(0.1f, -0.12f).x, 0.0f);
    CHECK_NEAR(stick(0.1f, -0.12f).y, 0.0f);
    CHECK_NEAR(stick(1.0f, 0.0f).x, 1.0f);
    CHECK_NEAR(stick(-1.0f, 0.0f).x, -1.0f);
    // GLFW's y is down-positive already, as winmm's was: no flip.
    CHECK_NEAR(stick(0.0f, 1.0f).y, 1.0f);
    CHECK_NEAR(stick(0.0f, -1.0f).y, -1.0f);
    CHECK(stick(0.6f, 0.0f).x > 0.4f && stick(0.6f, 0.0f).x < 0.6f);   // rescaled past the zone
    CHECK_NEAR(InputMapper::ApplyDeadzone(0.25f, 0.25f), 0.0f);

    RawDevices dpad;
    dpad.pads.push_back(Gamepad());
    dpad.pads[0].buttons[Pad::DpadLeft] = true;
    dpad.pads[0].buttons[Pad::DpadUp] = true;
    const InputFrame frame = Map(dpad, controls);
    CHECK_NEAR(frame.pads[0].xy.x, -1.0f);
    CHECK_NEAR(frame.pads[0].xy.y, -1.0f);
    CHECK_EQ(CountButtons(frame.pads[0]), 0);

    RawDevices trigger;
    trigger.pads.push_back(Gamepad());
    trigger.pads[0].axes[Pad::LeftTrigger] = 1.0f;
    CHECK(Map(trigger, controls).pads[0].buttons[JK_05]);
    CHECK_EQ(CountButtons(Map(trigger, controls).pads[0]), 1);
}

void testKeyboardPlayer2() {
    const ControlSettings controls = Defaults();
    CHECK(controls.keyboardPlayer2);

    // Connected with nothing held, on the index player 2 reads (joystick 0
    // under the default g_controls), so hasASecondController() holds steady.
    InputFrame frame = Map(RawDevices{}, controls, 0);
    CHECK(frame.pads[0].connected);
    CHECK(!frame.pads[1].connected);
    CHECK(frame.pads[0].xy == glm::vec2(0.0f));
    CHECK_EQ(CountButtons(frame.pads[0]), 0);
    frame = Map(RawDevices{}, controls, 1);
    CHECK(!frame.pads[0].connected);
    CHECK(frame.pads[1].connected);

    // J walks the princess left and no longer holds K_J (setupScene.as:393).
    frame = Map(With({GLFW_KEY_J}), controls, 0);
    CHECK_NEAR(frame.pads[0].xy.x, -1.0f);
    CHECK(!frame.keys[K_J]);
    CHECK_EQ(CountKeys(frame), 0);
    CHECK_NEAR(Map(With({GLFW_KEY_L}), controls, 0).pads[0].xy.x, 1.0f);
    CHECK_NEAR(Map(With({GLFW_KEY_K}), controls, 0).pads[0].xy.y, 1.0f);

    // I is up and jump, as Up is for player 1.
    frame = Map(With({GLFW_KEY_I}), controls, 0);
    CHECK_NEAR(frame.pads[0].xy.y, -1.0f);
    CHECK(frame.pads[0].buttons[JK_03]);
    CHECK(Map(With({GLFW_KEY_U}), controls, 0).pads[0].buttons[JK_04]);
    CHECK(Map(With({GLFW_KEY_O}), controls, 0).pads[0].buttons[JK_02]);
    CHECK(Map(With({GLFW_KEY_P}), controls, 0).pads[0].buttons[JK_01]);
    frame = Map(With({GLFW_KEY_BACKSPACE}), controls, 0);
    CHECK(frame.pads[0].buttons[JK_10]);
    CHECK(!frame.keys[K_BACK]);

    // On the other index when g_controls says so.
    CHECK(Map(With({GLFW_KEY_U}), controls, 1).pads[1].buttons[JK_04]);
    CHECK(!Map(With({GLFW_KEY_U}), controls, 1).pads[0].buttons[JK_04]);

    // Off: player 2 needs a real pad again, and J is K_J again.
    const ControlSettings off = WithoutKeyboardPlayer2();
    frame = Map(With({GLFW_KEY_J}), off, 0);
    CHECK(!frame.pads[0].connected);
    CHECK(frame.keys[K_J]);

    // A real pad on player 2's index is merged with the keys, never displaced
    // onto player 1's index. (In the original pad order a lone pad lands there;
    // under E12 it is the second pad that does - testLonePadIsPlayer1.)
    ControlSettings originalOrder = controls;
    originalOrder.firstPadIsPlayer1 = false;
    RawDevices merged = With({GLFW_KEY_L});
    merged.pads.push_back(Gamepad());
    merged.pads[0].buttons[Pad::X] = true;
    frame = Map(merged, originalOrder, 0);
    CHECK(frame.pads[0].connected);
    CHECK(frame.pads[0].buttons[JK_04]);
    CHECK_NEAR(frame.pads[0].xy.x, 1.0f);
    CHECK(!frame.pads[1].connected);
}

void testCursorAndText() {
    // A 4:3 box pillarboxed in a 1366x768 window: 171 px bars, scale 1.
    View view;
    view.logicalScreen = {1024.0f, 768.0f};
    view.windowPixels = {1366, 768};
    view.scale = 1.0f;
    view.viewportMin = {171.0f, 0.0f};
    view.viewportMax = {1195.0f, 768.0f};
    glm::vec2 logical = InputMapper::WindowToLogical({271.0f, 50.0f}, view);
    CHECK_NEAR(logical.x, 100.0f);
    CHECK_NEAR(logical.y, 50.0f);
    view.scale = 2.0f;
    logical = InputMapper::WindowToLogical({371.0f, 100.0f}, view);
    CHECK_NEAR(logical.x, 100.0f);
    CHECK_NEAR(logical.y, 50.0f);
    view.scale = 1.0f;

    InputMapper mapper;
    mapper.SetControls(Defaults());
    RawDevices raw;
    raw.mouseWindow = {271.0f, 50.0f};
    InputFrame frame = mapper.BuildTick(raw, view);
    CHECK_NEAR(frame.cursor.x, 100.0f);
    CHECK(frame.cursorAbsolute == frame.cursor);
    mapper.EndFrame(raw);
    // The script moves the cursor (menu.as:239); a still mouse leaves it there.
    mapper.WarpCursor({300.0f, 200.0f}, view);
    frame = mapper.BuildTick(raw, view);
    CHECK_NEAR(frame.cursor.x, 300.0f);
    CHECK_NEAR(frame.cursor.y, 200.0f);
    mapper.EndFrame(raw);
    // A moving mouse takes it back.
    raw.mouseWindow = {181.0f, 10.0f};
    frame = mapper.BuildTick(raw, view);
    CHECK_NEAR(frame.cursor.x, 10.0f);
    mapper.EndFrame(raw);
    // A warp cannot leave the logical screen.
    mapper.WarpCursor({-50.0f, 5000.0f}, view);
    CHECK_NEAR(mapper.Cursor().x, 0.0f);
    CHECK_NEAR(mapper.Cursor().y, 768.0f);

    CHECK(InputMapper::ToCp1252({'a', 'B', '1'}) == "aB1");
    CHECK(InputMapper::ToCp1252({0xE3, 0xE7}) == "\xE3\xE7");   // ã ç
    CHECK(InputMapper::ToCp1252({0x20AC, 0x2014}) == "\x80\x97");
    CHECK(InputMapper::ToCp1252({'\t'}) == "\t");   // InputState::Update expands it
    CHECK(InputMapper::ToCp1252({0x08, 0x1B, 0x0D, 0x85}).empty());
    CHECK(InputMapper::ToCp1252({0x4E2D}) == "?");
}

// Step 23: the system pointer shows over the bars, where the scripts' cursor
// sprite is drawn under a bar. Pixels at the edge belong to one box or the
// other, never both: viewportMin is the screen's, viewportMax the bar's.
void testPointerOverBars() {
    // Pillarboxed: the 1024x768 menu in a 1366x768 window, 171 px bars.
    View pillarbox;
    pillarbox.logicalScreen = {1024.0f, 768.0f};
    pillarbox.windowPixels = {1366, 768};
    pillarbox.scale = 1.0f;
    pillarbox.viewportMin = {171.0f, 0.0f};
    pillarbox.viewportMax = {1195.0f, 768.0f};
    CHECK_MSG(!InputMapper::PointerOverBars({683.0f, 384.0f}, pillarbox), "the middle of the menu");
    CHECK_MSG(InputMapper::PointerOverBars({10.0f, 384.0f}, pillarbox), "the left bar");
    CHECK_MSG(InputMapper::PointerOverBars({1300.0f, 384.0f}, pillarbox), "the right bar");
    CHECK_MSG(InputMapper::PointerOverBars({0.0f, 0.0f}, pillarbox), "the window's first pixel is a bar's");
    CHECK_MSG(InputMapper::PointerOverBars({170.0f, 384.0f}, pillarbox), "the left bar's last column");
    CHECK_MSG(!InputMapper::PointerOverBars({171.0f, 384.0f}, pillarbox), "viewportMin is the screen's");
    CHECK_MSG(!InputMapper::PointerOverBars({1194.5f, 384.0f}, pillarbox), "the screen's last column");
    CHECK_MSG(InputMapper::PointerOverBars({1195.0f, 384.0f}, pillarbox), "viewportMax is the bar's");
    CHECK_MSG(InputMapper::PointerOverBars({1365.0f, 767.0f}, pillarbox), "the window's last pixel");
    // Off the window: not over a bar, whatever the last position said.
    CHECK(!InputMapper::PointerOverBars({-1.0f, 384.0f}, pillarbox));
    CHECK(!InputMapper::PointerOverBars({1366.0f, 384.0f}, pillarbox));
    CHECK(!InputMapper::PointerOverBars({683.0f, 768.0f}, pillarbox));

    // Letterboxed: 4:3 in a 1024x1000 window, bars of 116 px above and below.
    View letterbox;
    letterbox.logicalScreen = {1024.0f, 768.0f};
    letterbox.windowPixels = {1024, 1000};
    letterbox.scale = 1.0f;
    letterbox.viewportMin = {0.0f, 116.0f};
    letterbox.viewportMax = {1024.0f, 884.0f};
    CHECK_MSG(InputMapper::PointerOverBars({512.0f, 50.0f}, letterbox), "the top bar");
    CHECK_MSG(InputMapper::PointerOverBars({512.0f, 950.0f}, letterbox), "the bottom bar");
    CHECK_MSG(InputMapper::PointerOverBars({512.0f, 115.0f}, letterbox), "the top bar's last row");
    CHECK_MSG(!InputMapper::PointerOverBars({512.0f, 116.0f}, letterbox), "the screen's first row");
    CHECK_MSG(!InputMapper::PointerOverBars({512.0f, 883.0f}, letterbox), "the screen's last row");
    CHECK_MSG(InputMapper::PointerOverBars({512.0f, 884.0f}, letterbox), "the bottom bar's first row");
    CHECK_MSG(!InputMapper::PointerOverBars({0.0f, 500.0f}, letterbox), "no bar at the sides");

    // The widescreen view fills the window: no bar anywhere on it.
    View filled;
    filled.logicalScreen = {1366.0f, 768.0f};
    filled.windowPixels = {1366, 768};
    filled.scale = 1.0f;
    filled.viewportMin = {0.0f, 0.0f};
    filled.viewportMax = {1366.0f, 768.0f};
    for (const glm::vec2 point : {glm::vec2(0.0f, 0.0f), glm::vec2(683.0f, 384.0f), glm::vec2(1365.0f, 767.0f),
                                  glm::vec2(0.0f, 767.0f)}) {
        CHECK(!InputMapper::PointerOverBars(point, filled));
    }
    // A logical screen a hair wider than the window (1365.3334 x 1.40625 =
    // 1920.0001, CameraRig::ComputeView) leaves no bar at its right edge.
    filled.windowPixels = {1920, 1080};
    filled.viewportMax = {1920.0001f, 1080.0f};
    CHECK(!InputMapper::PointerOverBars({1919.0f, 540.0f}, filled));

    // In the editor the image starts at imageOrigin: the bars move with it,
    // and the editor's own panels beside the image are not bars.
    View inEditor = pillarbox;
    inEditor.imageOrigin = {300.0f, 40.0f};
    CHECK(InputMapper::PointerOverBars({310.0f, 424.0f}, inEditor));
    CHECK(!InputMapper::PointerOverBars({983.0f, 424.0f}, inEditor));
    CHECK_MSG(!InputMapper::PointerOverBars({100.0f, 424.0f}, inEditor), "a panel left of the image");
}

void testLatch() {
    View view;
    InputMapper mapper;
    mapper.SetControls(Defaults());
    const RawDevices idle;
    const RawDevices space = With({GLFW_KEY_SPACE});

    // A frame with a tick, then a frame with none in which Space was tapped:
    // the next tick sees it (HIT), the one after does not.
    mapper.BuildTick(idle, view);
    mapper.EndFrame(idle);
    mapper.EndFrame(space);
    CHECK(mapper.BuildTick(idle, view).keys[K_SPACE]);
    CHECK(!mapper.BuildTick(idle, view).keys[K_SPACE]);
    mapper.EndFrame(idle);

    // A key held through a tickless frame is not stretched past its release.
    CHECK(mapper.BuildTick(space, view).keys[K_SPACE]);
    mapper.EndFrame(space);
    mapper.EndFrame(space);
    CHECK(!mapper.BuildTick(idle, view).keys[K_SPACE]);
    mapper.EndFrame(idle);

    // Pad buttons latch the same way.
    RawDevices pad;
    pad.pads.push_back(Gamepad());
    RawDevices padA = pad;
    padA.pads[0].buttons[Pad::A] = true;
    ControlSettings noKeyboard = WithoutKeyboardPlayer2();
    mapper.SetControls(noKeyboard);
    mapper.BuildTick(pad, view);
    mapper.EndFrame(pad);
    mapper.EndFrame(padA);
    CHECK(mapper.BuildTick(pad, view).pads[0].buttons[JK_03]);
    mapper.EndFrame(pad);

    // Typed text reaches exactly one tick: the first of its frame, or the
    // first after a tickless frame.
    RawDevices typedA;
    typedA.typed = {'a'};
    CHECK(mapper.BuildTick(typedA, view).typed == "a");
    CHECK(mapper.BuildTick(typedA, view).typed.empty());
    mapper.EndFrame(typedA);
    RawDevices typedB;
    typedB.typed = {'b'};
    mapper.EndFrame(typedB);
    RawDevices typedC;
    typedC.typed = {'c'};
    CHECK(mapper.BuildTick(typedC, view).typed == "bc");
    mapper.EndFrame(typedC);
}

// E14: in the menus a pad's A also presses JK_10 (the scripts' confirm) and
// B also JK_09 (their cancel); outside them nothing changes.
void testMenuMode() {
    using Penumbra::Render::MenuButtons;
    const ControlSettings controls = WithoutKeyboardPlayer2();
    const MenuButtons on{true, 0, 0};
    const auto press = [&controls](int glfwButton, const MenuButtons& menu) {
        RawDevices one;
        one.pads.push_back(Gamepad());
        one.pads[0].buttons[static_cast<std::size_t>(glfwButton)] = true;
        return InputMapper::Map(one, controls, glm::vec2(0.0f), 0, menu).pads[0];
    };

    // Off (the default): unchanged.
    InputFrame::Pad pad = press(Pad::A, MenuButtons{});
    CHECK(pad.buttons[JK_03] && !pad.buttons[JK_10]);
    CHECK_EQ(CountButtons(pad), 1);
    pad = press(Pad::B, MenuButtons{});
    CHECK(pad.buttons[JK_02] && !pad.buttons[JK_09]);
    CHECK_EQ(CountButtons(pad), 1);

    // On: A is jump AND confirm, B fire AND cancel.
    pad = press(Pad::A, on);
    CHECK(pad.buttons[JK_03] && pad.buttons[JK_10]);
    CHECK_EQ(CountButtons(pad), 2);
    pad = press(Pad::B, on);
    CHECK(pad.buttons[JK_02] && pad.buttons[JK_09]);
    CHECK_EQ(CountButtons(pad), 2);
    // The rest as ever.
    CHECK(press(Pad::Start, on).buttons[JK_10] && CountButtons(press(Pad::Start, on)) == 1);
    CHECK(press(Pad::Back, on).buttons[JK_09] && CountButtons(press(Pad::Back, on)) == 1);
    CHECK(press(Pad::X, on).buttons[JK_04] && CountButtons(press(Pad::X, on)) == 1);
    CHECK(press(Pad::Y, on).buttons[JK_01] && CountButtons(press(Pad::Y, on)) == 1);

    // Every real pad: the second one too.
    RawDevices two;
    two.pads.push_back(Gamepad());
    two.pads.push_back(Gamepad());
    two.pads[1].buttons[Pad::A] = true;
    InputFrame frame = InputMapper::Map(two, controls, glm::vec2(0.0f), 0, on);
    CHECK(frame.pads[1].buttons[JK_03] && frame.pads[1].buttons[JK_10]);
    CHECK(!frame.pads[0].buttons[JK_10]);

    // A bit set: that pad's A was held from before, and is not aliased.
    frame = InputMapper::Map(two, controls, glm::vec2(0.0f), 0, MenuButtons{true, 0b10u, 0});
    CHECK(frame.pads[1].buttons[JK_03] && !frame.pads[1].buttons[JK_10]);

    // Not an unmapped joystick (its numbering is the device's)...
    ControlSettings rawToo = controls;
    rawToo.rawJoysticks = true;
    RawDevices unmapped;
    RawPad rawPad;
    rawPad.gamepad = false;
    rawPad.buttons[Pad::A] = true;   // what GLFW's layout would call A: not read for such a pad
    rawPad.rawButtons[2] = true;
    unmapped.pads.push_back(rawPad);
    pad = InputMapper::Map(unmapped, rawToo, glm::vec2(0.0f), 0, on).pads[0];
    CHECK(pad.buttons[JK_03] && !pad.buttons[JK_10]);
    CHECK_EQ(CountButtons(pad), 1);

    // ...nor the keyboard's player 2: his jump (I) stays JK_03 alone.
    const ControlSettings withKeyboard = Defaults();
    frame = InputMapper::Map(With({GLFW_KEY_I}), withKeyboard, glm::vec2(0.0f), 0, on);
    CHECK(frame.pads[0].buttons[JK_03] && !frame.pads[0].buttons[JK_10]);
    frame = InputMapper::Map(With({GLFW_KEY_O}), withKeyboard, glm::vec2(0.0f), 0, on);
    CHECK(frame.pads[0].buttons[JK_02] && !frame.pads[0].buttons[JK_09]);

    // The mapper: SetMenuMode, and an A held since before menu mode began
    // (Main menu picked with A in the pause) is not a confirm until pressed anew.
    View view;
    InputMapper mapper;
    mapper.SetControls(controls);
    RawDevices idle;
    idle.pads.push_back(Gamepad());
    RawDevices padA = idle;
    padA.pads[0].buttons[Pad::A] = true;
    RawDevices padB = idle;
    padB.pads[0].buttons[Pad::B] = true;
    CHECK(!mapper.MenuMode());
    CHECK(!mapper.BuildTick(padA, view).pads[0].buttons[JK_10]);   // a level: jump only
    mapper.EndFrame(padA);
    mapper.SetMenuMode(true);
    CHECK(mapper.MenuMode());
    frame = mapper.BuildTick(padA, view);                             // still the same press
    CHECK(frame.pads[0].buttons[JK_03] && !frame.pads[0].buttons[JK_10]);
    mapper.EndFrame(padA);
    CHECK(!mapper.BuildTick(padA, view).pads[0].buttons[JK_10]);
    mapper.EndFrame(padA);
    CHECK(!mapper.BuildTick(idle, view).pads[0].buttons[JK_10]);     // released
    mapper.EndFrame(idle);
    frame = mapper.BuildTick(padA, view);                             // pressed anew
    CHECK(frame.pads[0].buttons[JK_03] && frame.pads[0].buttons[JK_10]);
    mapper.EndFrame(padA);
    CHECK(mapper.BuildTick(padB, view).pads[0].buttons[JK_09]);       // B was never held: at once
    mapper.EndFrame(padB);
    // A press in a frame that ran no tick is latched with its alias.
    mapper.BuildTick(idle, view);
    mapper.EndFrame(idle);
    mapper.EndFrame(padA);
    CHECK(mapper.BuildTick(idle, view).pads[0].buttons[JK_10]);
    mapper.EndFrame(idle);
    // Off again: A is jump only.
    mapper.SetMenuMode(false);
    frame = mapper.BuildTick(padA, view);
    CHECK(frame.pads[0].buttons[JK_03] && !frame.pads[0].buttons[JK_10]);
    mapper.EndFrame(padA);
    // On again with B held (a death into gameover.esc holding fire): not a cancel.
    mapper.SetMenuMode(true);
    frame = mapper.BuildTick(padB, view);
    CHECK(frame.pads[0].buttons[JK_02] && !frame.pads[0].buttons[JK_09]);
    mapper.EndFrame(padB);
}

void testKeyNames() {
    CHECK(KeyName(GLFW_KEY_LEFT_CONTROL) == "LeftCtrl");
    CHECK(KeyName(GLFW_KEY_A) == "A");
    CHECK(KeyName(GLFW_KEY_7) == "7");
    CHECK(KeyName(GLFW_KEY_F12) == "F12");
    CHECK(KeyName(GLFW_KEY_KP_3) == "Keypad3");
    CHECK_EQ(KeyFromName("leftctrl"), GLFW_KEY_LEFT_CONTROL);
    CHECK_EQ(KeyFromName("f12"), GLFW_KEY_F12);
    CHECK_EQ(KeyFromName("Keypad7"), GLFW_KEY_KP_7);
    CHECK_EQ(KeyFromName("KeypadEnter"), GLFW_KEY_KP_ENTER);
    CHECK_EQ(KeyFromName("j"), GLFW_KEY_J);
    CHECK_EQ(KeyFromName("341"), GLFW_KEY_LEFT_CONTROL);
    CHECK_EQ(KeyFromName("161"), -1);   // GLFW_KEY_WORLD_1: the engine never polls it
    CHECK_EQ(KeyFromName("nonsense"), -1);
    CHECK_EQ(KeyFromName("12"), -1);
    CHECK(KeyName(5).empty());
    // Every code GLFW reports survives a round trip through its name.
    int roundTrips = 0;
    bool allRoundTrip = true;
    for (int code = 0; code <= Supersonic::Key::Last; ++code) {
        const std::string name = KeyName(code);
        if (name.empty()) continue;   // not a code GLFW reports
        ++roundTrips;
        if (KeyFromName(name) != code) allRoundTrip = false;
    }
    CHECK(allRoundTrip);
    CHECK(roundTrips > 100);
}

// The system's language on every platform: a POSIX or BCP 47 locale name, and
// the hook a platform without one in the environment (Android, iOS) calls.
void testSystemLocale() {
    for (const char* yes : {"pt", "PT", "pt_BR", "pt_PT.UTF-8", "pt-BR", "pt@euro", "pt.ISO-8859-1"}) {
        CHECK_MSG(Settings::LocaleIsPortuguese(yes), yes);
    }
    for (const char* no : {"", "p", "C", "POSIX", "C.UTF-8", "en_US.UTF-8", "es_ES", "ptx", "pta_XX", "de-pt"}) {
        CHECK_MSG(!Settings::LocaleIsPortuguese(no), no);
    }
    // E24: a locale's language, when the game speaks it; English otherwise.
    const std::pair<const char*, const char*> locales[] = {
        {"pt_BR", "pt"}, {"pt-PT", "pt"}, {"PT", "pt"}, {"en_GB", "en"}, {"en-US", "en"}, {"en", "en"},
        {"de_DE.UTF-8", "de"}, {"de-AT", "de"}, {"es_ES", "es"}, {"es-419", "es"}, {"fr_CA", "fr"},
        {"fr.UTF-8", "fr"}, {"it_IT@euro", "it"}, {"ru_RU", "ru"}, {"RU", "ru"}, {"tr_TR", "tr"},
        {"uk_UA", "uk"}, {"uk-UA", "uk"}, {"ja_JP.UTF-8", "ja"}, {"ja", "ja"}, {"ar_EG", "ar"},
        {"ar-SA", "ar"}, {"zh_CN", "en"}, {"ko-KR", "en"}, {"C", "en"}, {"POSIX", "en"},
        {"C.UTF-8", "en"}, {"", "en"}, {"ptx", "en"}, {"deu", "en"}, {"u", "en"},
    };
    for (const auto& [locale, id] : locales) {
        CHECK_MSG(Settings::LanguageOfLocale(locale) == id,
                  std::string(locale) + " -> " + Settings::LanguageOfLocale(locale) + ", not " + id);
    }
    // Windows' primary language ids (winnt.h), which Settings.cpp checks
    // against LANG_* where it is built for Windows.
    const std::pair<unsigned, const char*> langIds[] = {
        {0x09, "en"}, {0x07, "de"}, {0x0A, "es"}, {0x0C, "fr"}, {0x10, "it"}, {0x16, "pt"}, {0x19, "ru"},
        {0x1F, "tr"}, {0x22, "uk"}, {0x11, "ja"}, {0x01, "ar"}, {0x04, "en"}, {0x12, "en"}, {0x00, "en"},
    };
    for (const auto& [langId, id] : langIds) {
        CHECK_MSG(Settings::LanguageOfPrimaryLangId(langId) == id, std::to_string(langId) + " -> " + id);
    }
    Settings::SetSystemLocale("pt-BR");
    CHECK(Settings::SystemLanguageIsPortuguese());
    CHECK(Settings::SystemLanguage() == "pt");
    CHECK(Settings::Defaults(Settings::SystemLanguage()).language == "pt");
    Settings::SetSystemLocale("uk_UA");
    CHECK(Settings::SystemLanguage() == "uk");
    CHECK(!Settings::SystemLanguageIsPortuguese());
    CHECK(Settings::Defaults(Settings::SystemLanguage()).language == "uk");
    Settings::SetSystemLocale("zh-Hans-CN");
    CHECK(Settings::Defaults(Settings::SystemLanguage()).language == "en");
    Settings::SetSystemLocale("en-GB");
    CHECK(!Settings::SystemLanguageIsPortuguese());

    // E27: automatic follows the system. What main() does: Defaults() in the
    // system's language, then the file read over it; a file that says "auto",
    // says nothing about the language, or says something unknown is in the
    // system's language, one that names a language is in that one, and none of
    // it depends on the machine the test runs on.
    const std::pair<const char*, const char*> systems[] = {
        {"en-US", "en"}, {"de_DE.UTF-8", "de"}, {"ja-JP", "ja"}, {"ar", "ar"},   {"pt-BR", "pt"},
        {"uk_UA", "uk"}, {"xx_XX", "en"},       {"zh-Hans-CN", "en"}, {"C", "en"},
    };
    for (const auto& [locale, id] : systems) {
        Settings::SetSystemLocale(locale);
        const Settings defaults = Settings::Defaults(Settings::SystemLanguage());
        CHECK_MSG(defaults.languageAuto && defaults.language == id, locale);
        for (const char* json : {R"({"language": "auto"})", R"({"language": "AUTO"})", R"({"language": "Auto"})", "{}",
                                 R"({"volume": {"music": 0.5}})"}) {
            const Settings read = Settings::FromJson(json, defaults);
            CHECK_MSG(read.languageAuto && read.language == id, std::string(locale) + " " + json);
        }
        // A language written by name is that language, and no longer automatic.
        const Settings german = Settings::FromJson(R"({"language": "de"})", defaults);
        CHECK_MSG(!german.languageAuto && german.language == "de", locale);
        // Unknown: a warning, and automatic.
        std::string warning;
        const Settings unknown = Settings::FromJson(R"({"language": "klingon"})", defaults, &warning);
        CHECK_MSG(unknown.languageAuto && unknown.language == id && !warning.empty(), locale);
        // What is saved says "auto", not the id it resolved to, so the next start resolves it again.
        CHECK_MSG(defaults.ToJson().find("\"language\": \"auto\"") != std::string::npos, locale);
        CHECK_MSG(Settings::FromJson(defaults.ToJson(), defaults) == defaults, locale);
    }
    // Defaults that are not automatic are no stand-in for the system: "auto" in
    // the file asks it.
    {
        Settings::SetSystemLocale("ja-JP");
        Settings explicitPortuguese = Settings::Defaults("pt");
        explicitPortuguese.languageAuto = false;
        const Settings read = Settings::FromJson(R"({"language": "auto"})", explicitPortuguese);
        CHECK(read.languageAuto && read.language == "ja");
    }
    Settings::SetSystemLocale("");   // back to the system's own answer, whatever it is
}

void testSettings() {
    const Settings pt = Settings::Defaults("pt");
    const Settings en = Settings::Defaults("en");
    CHECK(pt.language == "pt");
    CHECK(en.language == "en");
    CHECK(Settings::Defaults().language == "en");
    CHECK(Settings::Defaults("xx").language == "en");   // not a language the game speaks
    CHECK(Settings::Defaults("JA").language == "ja");
    CHECK(en.controls.keyboardPlayer2);
    CHECK(en.controls.firstPadIsPlayer1);    // E12
    CHECK_EQ(en.controls.joystickLayout, 0);
    CHECK_EQ(en.controls.Player2Pad(), 0);   // g_controls 0: player 2 reads joystick 0
    CHECK(en.pixelShaders);
    CHECK(en.pauseOnFocusLoss);              // E13
    CHECK(en.difficulty == "normal" && !en.HardDifficulty());   // E36: the original's game unless the player picks Hard
    CHECK(pt.difficulty == "normal" && Settings::Defaults("ja").difficulty == "normal");   // E36: whatever the language
    CHECK_EQ(en.fullscreenWidth, 0);         // Step 23: the desktop's mode
    CHECK_EQ(en.fullscreenHeight, 0);
    // E23: a first launch is fullscreen, automatic in all three.
    CHECK(en.fullscreen);
    CHECK_EQ(en.fullscreenRefresh, 0);
    CHECK_EQ(en.windowWidth, 0);
    CHECK_EQ(en.windowHeight, 0);
    CHECK(en.touchTuning == Penumbra::Render::TouchTuning{});   // E28: the touch controls as the manifest has them
    CHECK_EQ(Settings::kVersion, 2);
    CHECK(en.controls.player1[ControlAction::Jump] ==
          (std::vector<int>{GLFW_KEY_LEFT_CONTROL, GLFW_KEY_RIGHT_CONTROL}));
    CHECK(en.controls.player2[ControlAction::Left] == std::vector<int>{GLFW_KEY_J});

    const fs::path dir = fs::temp_directory_path() /
                         ("penumbra-settings-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code ec;
    fs::remove_all(dir, ec);

    // Nothing there yet: the defaults, and nothing to complain about.
    std::string warning;
    CHECK(Settings::Load(dir, en, &warning) == en);
    CHECK(warning.empty());

    Settings changed = en;
    changed.language = "pt";
    changed.languageAuto = false;            // E27: an id in the file is a choice; "auto" would read as en
    changed.windowWidth = 1920;
    changed.windowHeight = 1080;
    changed.fullscreen = true;
    changed.fullscreenWidth = 1280;          // Step 23
    changed.fullscreenHeight = 720;
    changed.fullscreenRefresh = 144;         // E23
    changed.widescreen = false;
    changed.musicVolume = 0.35f;
    changed.effectsVolume = 0.8f;
    changed.pixelShaders = false;
    changed.pauseOnFocusLoss = false;        // E13
    changed.difficulty = "hard";             // E36
    changed.controls.joystickLayout = 1;
    changed.controls.keyboardPlayer2 = false;
    changed.controls.firstPadIsPlayer1 = true;
    changed.controls.rawJoysticks = true;
    changed.controls.stickDeadzone = 0.3f;
    changed.controls.player2[ControlAction::Sword] = {GLFW_KEY_N, GLFW_KEY_KP_DECIMAL};
    changed.controls.player1[ControlAction::Cancel] = {};
    std::string error;
    CHECK(changed.Save(dir, &error));
    CHECK(error.empty());
    CHECK(fs::exists(Settings::FilePath(dir)));
    CHECK(!fs::exists(dir / "settings.json.tmp"));
    warning.clear();
    const Settings loaded = Settings::Load(dir, en, &warning);
    CHECK(loaded == changed);
    CHECK(warning.empty());
    // Saving over an existing file works too.
    changed.musicVolume = 0.5f;
    CHECK(changed.Save(dir, &error));
    CHECK(Settings::Load(dir, en).musicVolume == 0.5f);

    // A broken file: the defaults, and a warning.
    {
        std::ofstream broken(Settings::FilePath(dir), std::ios::binary | std::ios::trunc);
        broken << "{ \"language\": \"pt\", ";
    }
    warning.clear();
    CHECK(Settings::Load(dir, en, &warning) == en);
    CHECK(!warning.empty());

    // A partial file: what it says, clamped, and the defaults for the rest.
    warning.clear();
    const Settings partial = Settings::FromJson(
        R"({"language": "PT", "volume": {"music": 2.5}, "window": {"width": 10, "height": 10},
            "controls": {"player1": {"sword": ["A", "bogus"]}}})",
        en, &warning);
    CHECK(partial.language == "pt");
    CHECK(!partial.languageAuto);   // E27: named, so no longer automatic
    CHECK(partial.musicVolume == 1.0f);
    CHECK(partial.effectsVolume == en.effectsVolume);
    CHECK_EQ(partial.windowWidth, 640);
    CHECK_EQ(partial.windowHeight, 480);
    CHECK(partial.controls.player1[ControlAction::Sword] == std::vector<int>{GLFW_KEY_A});
    CHECK(partial.controls.player1[ControlAction::Fire] == en.controls.player1[ControlAction::Fire]);
    CHECK(partial.controls.player2 == en.controls.player2);
    CHECK(partial.difficulty == "normal");   // E36: an older file without the key is the default
    CHECK(!warning.empty());   // "bogus"
    CHECK(Settings::FromJson("[1, 2]", en) == en);

    // Step 23: the fullscreen mode. A size, or 0 x 0 for the desktop's; an
    // older file without it is the desktop's, and says nothing.
    const auto fullscreenMode = [&en](const std::string& window, std::string* warn) {
        const Settings read = Settings::FromJson("{\"window\": {" + window + "}}", en, warn);
        return glm::ivec2(read.fullscreenWidth, read.fullscreenHeight);
    };
    warning.clear();
    const glm::ivec2 picked = fullscreenMode(R"("fullscreenWidth": 800, "fullscreenHeight": 600)", &warning);
    const glm::ivec2 desktop = fullscreenMode(R"("fullscreenWidth": 0, "fullscreenHeight": 0)", &warning);
    const glm::ivec2 older = fullscreenMode(R"("width": 1280, "height": 720)", &warning);
    CHECK(picked == glm::ivec2(800, 600));
    CHECK(desktop == glm::ivec2(0, 0));
    CHECK(older == glm::ivec2(0, 0));
    CHECK(warning.empty());
    // Broken: the default (the desktop's), never a clamped guess at a mode,
    // and a warning for each.
    for (const char* broken : {
             R"("fullscreenWidth": 1280)",                                  // half a size
             R"("fullscreenWidth": 1280, "fullscreenHeight": 0)",           // half a size again
             R"("fullscreenWidth": 99999, "fullscreenHeight": 720)",        // no monitor has it
             R"("fullscreenWidth": 320, "fullscreenHeight": 240)",          // below the window's range
             R"("fullscreenWidth": -1280, "fullscreenHeight": 720)",
             R"("fullscreenWidth": 1280.5, "fullscreenHeight": 720)",
             R"("fullscreenWidth": "1280", "fullscreenHeight": 720)",
         }) {
        warning.clear();
        const glm::ivec2 mode = fullscreenMode(broken, &warning);
        CHECK_MSG(mode == glm::ivec2(0, 0), broken);
        CHECK_MSG(!warning.empty(), broken);
    }
    // A broken mode leaves the rest of the window as the file says.
    const Settings brokenMode = Settings::FromJson(
        R"({"window": {"width": 1280, "height": 720, "fullscreen": true, "fullscreenWidth": 7}})", en);
    CHECK_EQ(brokenMode.windowWidth, 1280);
    CHECK(brokenMode.fullscreen);
    CHECK_EQ(brokenMode.fullscreenWidth, 0);

    // E23: the windowed size. 0 x 0 is automatic; so is half a size, which a
    // file can only hold by a typo, and a warning says so; any other value is
    // clamped to the window's range, as it always was.
    const auto windowedSize = [&en](const std::string& window, std::string* warn) {
        const Settings read = Settings::FromJson("{\"window\": {" + window + "}}", en, warn);
        return glm::ivec2(read.windowWidth, read.windowHeight);
    };
    warning.clear();
    CHECK(windowedSize(R"("width": 0, "height": 0)", &warning) == glm::ivec2(0, 0));
    CHECK(windowedSize(R"("width": 1280, "height": 720)", &warning) == glm::ivec2(1280, 720));
    CHECK(windowedSize(R"("width": 99999, "height": 100)", &warning) == glm::ivec2(15360, 480));   // clamped
    CHECK(windowedSize(R"("fullscreen": false)", &warning) == glm::ivec2(0, 0));   // no size: the default
    CHECK(warning.empty());
    for (const char* half : {R"("width": 1280)", R"("height": 720)", R"("width": 0, "height": 720)",
                             R"("width": 1280, "height": 0)"}) {
        warning.clear();
        CHECK_MSG(windowedSize(half, &warning) == glm::ivec2(0, 0), half);
        CHECK_MSG(!warning.empty(), half);
    }
    // Over explicit defaults, one extent alone is read against the other.
    Settings sized = en;
    sized.windowWidth = 1366;
    sized.windowHeight = 768;
    CHECK(Settings::FromJson(R"({"window": {"width": 1280}})", sized).windowWidth == 1280);
    CHECK(Settings::FromJson(R"({"window": {"width": 1280}})", sized).windowHeight == 768);

    // E23: the refresh rate. 0 (automatic) or a whole number of Hz; an older
    // file without it is automatic and says nothing; anything else is
    // automatic with a warning.
    const auto refresh = [&en](const std::string& window, std::string* warn) {
        return Settings::FromJson("{\"window\": {" + window + "}}", en, warn).fullscreenRefresh;
    };
    warning.clear();
    CHECK_EQ(refresh(R"("fullscreenRefresh": 165)", &warning), 165);
    CHECK_EQ(refresh(R"("fullscreenRefresh": 0)", &warning), 0);
    CHECK_EQ(refresh(R"("fullscreenWidth": 0, "fullscreenHeight": 0)", &warning), 0);   // a Step 23 file
    CHECK(warning.empty());
    for (const char* broken : {R"("fullscreenRefresh": -60)", R"("fullscreenRefresh": 59.94)",
                               R"("fullscreenRefresh": 5000)", R"("fullscreenRefresh": "144")",
                               R"("fullscreenRefresh": true)"}) {
        warning.clear();
        CHECK_MSG(refresh(broken, &warning) == 0, broken);
        CHECK_MSG(!warning.empty(), broken);
    }
    // A file written before E23 (version 1) keeps what it says, and the new
    // field is automatic: the development laptop's, as it was on 2026-09-29.
    warning.clear();
    const Settings version1 = Settings::FromJson(
        R"({"version": 1, "language": "en", "window": { "width": 1920, "height": 1200, "fullscreen": true,
            "fullscreenWidth": 0, "fullscreenHeight": 0 }})", en, &warning);
    CHECK(warning.empty());
    CHECK(version1.fullscreen);
    CHECK_EQ(version1.windowWidth, 1920);
    CHECK_EQ(version1.windowHeight, 1200);
    CHECK_EQ(version1.fullscreenWidth, 0);
    CHECK_EQ(version1.fullscreenRefresh, 0);
    const Settings version1Windowed = Settings::FromJson(
        R"({"version": 1, "window": { "width": 1366, "height": 768, "fullscreen": false }})", en);
    CHECK(!version1Windowed.fullscreen);
    CHECK_EQ(version1Windowed.windowWidth, 1366);
    // The file written back says it all, E23's field included.
    CHECK(changed.ToJson().find("\"fullscreenRefresh\": 144") != std::string::npos);
    CHECK(en.ToJson().find("\"version\": 2") != std::string::npos);
    CHECK(en.ToJson().find("\"width\": 0, \"height\": 0, \"fullscreen\": true") != std::string::npos);
    // E27: automatic is saved as "auto" and read in the language the defaults
    // carry (main() hands it the system's), whichever that is.
    CHECK(Settings::FromJson(en.ToJson(), en) == en);
    CHECK(en.languageAuto && pt.languageAuto);
    CHECK(en.ToJson().find("\"language\": \"auto\"") != std::string::npos);
    CHECK(en.ToJson().find("\"language\": \"en\"") == std::string::npos);
    CHECK(Settings::FromJson(en.ToJson(), pt).languageAuto);
    CHECK(Settings::FromJson(en.ToJson(), pt).language == "pt");

    // E24: every language the game speaks is saved and read back; a file from
    // before E24 ("pt", "en") reads as it did; anything else keeps the default.
    // E27: a language that is named is written as its id and is a choice.
    for (const Penumbra::Render::LanguageInfo& info : Penumbra::Render::kLanguages) {
        Settings speaking = en;
        speaking.language = info.id;
        speaking.languageAuto = false;
        CHECK_MSG(speaking.ToJson().find(std::string("\"language\": \"") + info.id + "\"") != std::string::npos, info.id);
        warning.clear();
        const Settings back = Settings::FromJson(speaking.ToJson(), Settings::Defaults("pt"), &warning);
        CHECK_MSG(back == speaking, info.id);
        CHECK_MSG(warning.empty(), info.id);
        CHECK_MSG(Settings::Defaults(info.id).language == info.id, info.id);
    }
    CHECK(Settings::FromJson(R"({"language": "pt"})", en).language == "pt");
    CHECK(Settings::FromJson(R"({"language": "en"})", pt).language == "en");
    CHECK(Settings::FromJson(R"({"language": "Uk"})", en).language == "uk");
    for (const char* unknown : {"zh", "xx", "", "english", "pt-BR"}) {
        warning.clear();
        const std::string json = std::string("{\"language\": \"") + unknown + "\"}";
        const Settings read = Settings::FromJson(json, pt, &warning);
        CHECK_MSG(read.language == "pt" && read.languageAuto, unknown);   // E27: unknown is automatic
        CHECK_MSG(!warning.empty(), unknown);
    }
    CHECK(!Settings::FromJson(R"({"language": "pt"})", en).languageAuto);   // E27: a file from before it stays a choice
    CHECK(!Settings::FromJson(R"({"language": "en"})", pt).languageAuto);

    // Nowhere to save.
    CHECK(!en.Save(fs::path(), &error));
    CHECK(!error.empty());

    fs::remove_all(dir, ec);
}

// ENHANCEMENT E28: settings.json's "touchTuning" (render/TouchTuning). The default is written and read back
// unchanged; a block written by the game reads back the same; an older file has none and says nothing; every
// wrong field is a warning and keeps its default while the rest of the block is read; numbers are held to
// their ranges and snapped to their grids (size and moves a tenth, opacity a fifth); and the steps the
// editor's tiles take land exactly on the limits and stop there. E34: the block carries the layout (the
// arrangement of the default) it was written under, and a block written under another keeps its size and its
// opacity but not its moves, which are deltas from places that are no longer the default's.
Penumbra::Render::TouchMove Mv(const float x, const float y) { return Penumbra::Render::TouchMove{x, y}; }

void testSettingsTouchTuning() {
    using Penumbra::Render::TouchControl;
    using Penumbra::Render::TouchMove;
    using Penumbra::Render::TouchTuning;
    const Settings en = Settings::Defaults("en");
    CHECK(en.touchTuning == TouchTuning{});
    CHECK(en.touchTuning.IsDefault());
    CHECK(!en.touchTuning.Moved());
    CHECK_EQ(Settings::kVersion, 2);   // nothing reads it: E24-E27 added fields without bumping it, and so did E28
    const std::string layoutField = "\"layout\": " + std::to_string(TouchTuning::kLayoutVersion);   // E34: the value is pinned in test_pn_render_touch
    CHECK(en.ToJson().find("\"touchTuning\": { " + layoutField + R"(, "size": 1, "opacity": 1, "move": {} },)") != std::string::npos);   // E34

    // What the game writes: one line, the controls in their order, only the moved ones, the shortest numbers.
    Settings tuned = en;
    tuned.touchTuning.size = 1.1f;
    tuned.touchTuning.opacity = 0.6f;
    tuned.touchTuning.move[static_cast<std::size_t>(TouchControl::Pause)] = {-30.0f, 20.0f};   // set first: written by order
    tuned.touchTuning.move[static_cast<std::size_t>(TouchControl::Jump)] = {-40.0f, 12.0f};
    const std::string json = tuned.ToJson();
    CHECK(json.find("\"touchTuning\": { " + layoutField +
                    R"(, "size": 1.1, "opacity": 0.6, "move": { "jump": [-40, 12], "pause": [-30, 20] } },)") != std::string::npos);   // E34
    CHECK(tuned.touchTuning.Moved());
    CHECK(!tuned.touchTuning.IsDefault());
    std::string warning;
    CHECK(Settings::FromJson(json, en, &warning) == tuned);   // Load(Save(s)) == s
    CHECK_MSG(warning.empty(), warning);
    // The same through the file.
    const fs::path dir = fs::temp_directory_path() /
                         ("penumbra-tuning-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code ec;
    fs::remove_all(dir, ec);
    std::string error;
    CHECK(tuned.Save(dir, &error));
    warning.clear();
    CHECK(Settings::Load(dir, en, &warning) == tuned);
    CHECK_MSG(warning.empty(), warning);
    fs::remove_all(dir, ec);
    // Held, not asked: nothing the file could not read is written (no "nan", no "-0").
    Settings odd = en;
    odd.touchTuning.size = std::numeric_limits<float>::quiet_NaN();
    odd.touchTuning.move[static_cast<std::size_t>(TouchControl::Fire)] = {-0.04f, std::numeric_limits<float>::infinity()};
    odd.touchTuning.move[static_cast<std::size_t>(TouchControl::Back)] = {5.0f, 5.0f};
    const std::string oddJson = odd.ToJson();
    const std::string oddBlock = odd.touchTuning.ToJson();
    CHECK(oddBlock.find("\"size\": 1,") != std::string::npos);
    CHECK(oddBlock.find("\"fire\": [0, 2048]") != std::string::npos);   // held, and with a zero, not a "-0"
    CHECK(oddBlock.find("nan") == std::string::npos && oddBlock.find("inf") == std::string::npos);
    CHECK(oddBlock.find("\"back\"") == std::string::npos);
    CHECK(Settings::FromJson(oddJson, en).touchTuning == odd.touchTuning.Clamped());

    // An older file has no block: the defaults, and nothing to say.
    warning.clear();
    CHECK(Settings::FromJson(R"({"language": "en", "edgeMargin": 3.5})", en, &warning).touchTuning == TouchTuning{});
    CHECK(warning.empty());

    const auto readRaw = [&en](const std::string& block, std::string* warn) {   // E34: the block exactly as written
        return Settings::FromJson("{\"touchTuning\": " + block + "}", en, warn).touchTuning;
    };
    // E34: a block as a game of this layout writes it, for the tests of what is read: the current "layout" put into an
    // object's text (anything that is not an object is left as it is, being what is wrong with it).
    const auto read = [&](const std::string& block, std::string* warn) {
        if (block.size() < 2 || block.front() != '{') return readRaw(block, warn);
        return readRaw("{ " + layoutField + (block == "{}" ? std::string(" }") : ", " + block.substr(1)), warn);
    };
    constexpr std::size_t kJump = static_cast<std::size_t>(TouchControl::Jump);
    constexpr std::size_t kBack = static_cast<std::size_t>(TouchControl::Back);
    // Ranges and grids: size 0.4..1.4 by tenths, opacity 0.2..1.8 by fifths, a move to 2048 by tenths.
    warning.clear();
    CHECK(read(R"({"size": 9})", &warning).size == TouchTuning::kMaxSize);   // 1.4: MR's own ceiling
    CHECK(read(R"({"size": -1})", &warning).size == TouchTuning::kMinSize);
    CHECK(read(R"({"size": 1.07})", &warning).size == 1.1f);
    CHECK(read(R"({"size": 1.04})", &warning).size == 1.0f);
    CHECK(read(R"({"opacity": 0.01})", &warning).opacity == TouchTuning::kMinOpacity);
    CHECK(read(R"({"opacity": 9})", &warning).opacity == TouchTuning::kMaxOpacity);
    CHECK(read(R"({"opacity": 1.07})", &warning).opacity == 1.0f);   // fifths: 1.07 is nearer 1.0 than 1.2
    CHECK(read(R"({"opacity": 1.11})", &warning).opacity == 1.2f);
    CHECK(read(R"({"move": {"jump": [1000000000, -1e9]}})", &warning).move[kJump] == Mv(2048.0f, -2048.0f));
    CHECK(read(R"({"move": {"jump": [1e40, -1e300]}})", &warning).move[kJump] == Mv(2048.0f, -2048.0f));
    CHECK(read(R"({"move": {"jump": [-40.04, 12.06]}})", &warning).move[kJump] == Mv(-40.0f, 12.1f));
    const TouchMove snapped = read(R"({"move": {"jump": [0.04, -0.04]}})", &warning).move[kJump];
    CHECK(snapped.IsZero() && !std::signbit(snapped.x) && !std::signbit(snapped.y));   // zero, not "-0"
    CHECK_MSG(warning.empty(), warning);   // clamping a number is not a complaint
    CHECK(read(R"({"move": {}})", &warning) == TouchTuning{});
    CHECK(read(R"({"move": {"_note": "kept by hand", "jump": [1, 2]}})", &warning).move[kJump] == Mv(1.0f, 2.0f));
    CHECK(warning.empty());

    // Every wrong field: a warning that names it, its default kept, the rest of the block read.
    const auto complains = [&read](const std::string& block, const char* what) {
        std::string warn;
        const TouchTuning result = read(block, &warn);
        CHECK_MSG(warn.find(what) != std::string::npos, block + " -> " + warn);
        return result;
    };
    CHECK(complains(R"({"size": "x"})", "touchTuning.size").size == 1.0f);
    CHECK(complains(R"({"size": true})", "touchTuning.size").size == 1.0f);
    CHECK(complains(R"({"opacity": [1]})", "touchTuning.opacity").opacity == 1.0f);
    CHECK(complains(R"({"move": 5})", "touchTuning.move") == TouchTuning{});
    CHECK(complains(R"({"move": [1, 2]})", "touchTuning.move") == TouchTuning{});
    CHECK(complains(R"({"move": {"jump": [1]}})", "touchTuning.move.jump").move[kJump].IsZero());
    CHECK(complains(R"({"move": {"jump": [1, 2, 3]}})", "touchTuning.move.jump").move[kJump].IsZero());
    CHECK(complains(R"({"move": {"jump": [1, "a"]}})", "touchTuning.move.jump").move[kJump].IsZero());
    CHECK(complains(R"({"move": {"jump": "x"}})", "touchTuning.move.jump").move[kJump].IsZero());
    CHECK(complains(R"({"move": {"jump": {"x": 1, "y": 2}}})", "touchTuning.move.jump").move[kJump].IsZero());
    CHECK(complains(R"({"move": {"fly": [1, 2]}})", "touchTuning.move.fly") == TouchTuning{});   // an unknown control
    CHECK(complains(R"({"move": {"back": [1, 2]}})", "touchTuning.move.back").move[kBack].IsZero());   // Back never moves
    for (const char* wrongType : {"[1, 2]", "7", "\"big\"", "null", "true"}) {   // the block itself
        CHECK(complains(wrongType, "touchTuning is not an object") == TouchTuning{});
    }
    // A bad field costs only itself.
    std::string mixed;
    const TouchTuning partial =
        read(R"({"size": "x", "opacity": 0.6, "move": {"jump": [3, 4], "back": [1, 1], "pause": [-5]}})", &mixed);
    CHECK(partial.size == 1.0f && partial.opacity == 0.6f);
    CHECK(partial.move[kJump] == Mv(3.0f, 4.0f));
    CHECK(partial.move[kBack].IsZero() && partial.move[static_cast<std::size_t>(TouchControl::Pause)].IsZero());
    CHECK(mixed.find("touchTuning.size") != std::string::npos && mixed.find("touchTuning.move.back") != std::string::npos &&
          mixed.find("touchTuning.move.pause") != std::string::npos);
    // And a hand-edited file loses nothing else.
    const Settings rest = Settings::FromJson(R"({"language": "pt", "touchTuning": 5, "zoom": 150})", en, &mixed);
    CHECK(rest.language == "pt" && rest.zoom == 150);

    // E34: the layout the moves were saved against. The same block, "layout" aside: the size and the opacity are read
    // whatever it says; the moves only when it is the current one.
    const std::string body = R"("size": 1.2, "opacity": 0.6, "move": { "jump": [-40, 12], "pause": [-30, 20] })";
    const auto keeps = [&](const std::string& text, const char* what) {   // size, opacity and both moves, no warning
        std::string warn;
        const TouchTuning got = readRaw(text, &warn);
        CHECK_MSG(got.size == 1.2f && got.opacity == 0.6f, what);
        CHECK_MSG(got.move[kJump] == Mv(-40.0f, 12.0f) &&
                      got.move[static_cast<std::size_t>(TouchControl::Pause)] == Mv(-30.0f, 20.0f), what);
        CHECK_MSG(warn.empty(), std::string(what) + ": " + warn);
    };
    const auto drops = [&](const std::string& text, const char* what, const char* complaint) {   // size and opacity, no move
        std::string warn;
        const TouchTuning got = readRaw(text, &warn);
        CHECK_MSG(got.size == 1.2f && got.opacity == 0.6f, what);
        CHECK_MSG(!got.Moved(), what);
        if (complaint == nullptr) CHECK_MSG(warn.empty(), std::string(what) + ": " + warn);   // an expected migration is not a fault
        else CHECK_MSG(warn.find(complaint) != std::string::npos, std::string(what) + ": " + warn);
    };
    keeps("{ " + layoutField + ", " + body + " }", "the current layout");
    keeps("{ \"layout\": " + std::to_string(TouchTuning::kLayoutVersion) + ".0, " + body + " }", "the current layout, written as a double");
    keeps("{ " + body + ", " + layoutField + " }", "the layout after the moves");   // looked up, not read in order
    drops("{ " + body + " }", "no layout: a file from E28 to E32", nullptr);
    // 3 is the number an intermediate build of E33 (the left column the higher; never released) wrote beside its moves, which
    // were made against places that are not this layout's: they go, as every older number's do.
    drops("{ \"layout\": 3, " + body + " }", "layout 3: the intermediate E33 build", nullptr);
    drops("{ \"layout\": 3.0, " + body + " }", "layout 3, written as a double", nullptr);
    drops("{ " + body + ", \"layout\": 3 }", "the layout after the moves, another one", nullptr);
    for (const char* other : {"0", "1", "2", "3", "5", "99", "-3", "4.5", "3.9999999", "1e40", "-1e300"}) {
        drops("{ \"layout\": " + std::string(other) + ", " + body + " }", other, nullptr);   // older, newer, never a layout
    }
    // The current number as text and as the one element of an array: not numbers, so not the current layout either.
    const std::string currentLayout = std::to_string(TouchTuning::kLayoutVersion);
    for (const std::string& notNumber : {"\"" + currentLayout + "\"", std::string("true"), std::string("null"),
                                         "[" + currentLayout + "]", std::string("{}"), std::string("\"x\"")}) {
        drops("{ \"layout\": " + notNumber + ", " + body + " }", notNumber.c_str(), "touchTuning.layout");
    }
    // What is dropped is not read: no complaint about a move the file could not have meant for this layout.
    warning.clear();
    CHECK(!readRaw(R"({ "layout": 3, "move": 5 })", &warning).Moved() && warning.empty());
    CHECK(!readRaw(R"({ "size": 1.2, "move": { "jump": [1], "fly": [1, 2] } })", &warning).Moved() && warning.empty());
    // ...but the size and the opacity are checked as ever, whatever the layout.
    CHECK(complains(R"({ "layout": 3, "size": "x" })", "touchTuning.size").size == 1.0f);
    // An empty block and one with a layout alone are the defaults.
    CHECK(readRaw("{ " + layoutField + " }", &warning) == TouchTuning{} && warning.empty());
    CHECK(readRaw("{}", &warning) == TouchTuning{} && warning.empty());

    // Through the whole settings text and the file: what the game wrote under E28 to E32 is the current text without
    // its "layout"; read, it keeps the size and the opacity and loses the moves, the next save writes the current layout
    // (and no moves), and that file reads back as written.
    std::string oldText = json;
    const std::size_t layoutAt = oldText.find(layoutField + ", ");
    CHECK(layoutAt != std::string::npos);
    if (layoutAt != std::string::npos) oldText.erase(layoutAt, layoutField.size() + 2);
    CHECK(oldText.find("\"layout\"") == std::string::npos);
    CHECK(oldText.find("\"touchTuning\": { \"size\": 1.1, \"opacity\": 0.6, \"move\": { \"jump\": [-40, 12]") != std::string::npos);
    warning.clear();
    const Settings migrated = Settings::FromJson(oldText, en, &warning);
    CHECK_MSG(warning.empty(), warning);
    CHECK(migrated.touchTuning.size == 1.1f && migrated.touchTuning.opacity == 0.6f && !migrated.touchTuning.Moved());
    Settings withoutMoves = tuned;   // everything else in the file is read as ever
    withoutMoves.touchTuning.move = {};
    CHECK(migrated == withoutMoves);
    const std::string migratedBlock = "\"touchTuning\": { " + layoutField + R"(, "size": 1.1, "opacity": 0.6, "move": {} },)";
    CHECK(migrated.ToJson().find(migratedBlock) != std::string::npos);
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    {
        std::ofstream file(Settings::FilePath(dir), std::ios::binary | std::ios::trunc);
        file << oldText;
    }
    warning.clear();
    const Settings loaded = Settings::Load(dir, en, &warning);
    CHECK_MSG(warning.empty(), warning);
    CHECK(loaded == withoutMoves);
    CHECK(loaded.Save(dir, &error));
    std::string saved;
    {
        std::ifstream file(Settings::FilePath(dir), std::ios::binary);
        saved.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }
    CHECK(saved.find(migratedBlock) != std::string::npos);
    CHECK(Settings::Load(dir, en, &warning) == loaded);
    // A player who moves a control after that keeps it: written with the current layout, read back.
    Settings moved = loaded;
    moved.touchTuning.move[kJump] = {-40.0f, 12.0f};
    CHECK(moved.Save(dir, &error));
    CHECK(Settings::Load(dir, en, &warning) == moved);
    fs::remove_all(dir, ec);

    // Clamped: what every use of a tuning goes through.
    TouchTuning wild;
    wild.size = 7.0f;
    wild.opacity = -3.0f;
    wild.move[kJump] = {std::numeric_limits<float>::quiet_NaN(), 1.0e9f};
    wild.move[kBack] = {4.0f, 4.0f};
    const TouchTuning held = wild.Clamped();
    CHECK(held.size == TouchTuning::kMaxSize && held.opacity == TouchTuning::kMinOpacity);
    CHECK(held.move[kJump] == Mv(0.0f, 2048.0f));
    CHECK(held.move[kBack].IsZero());
    CHECK(held.Clamped() == held);   // idempotent
    CHECK(TouchTuning{}.Clamped() == TouchTuning{});
    CHECK(TouchTuning{}.IsDefault() && !TouchTuning{}.Moved());
    CHECK(TouchTuning::SnapSize(std::numeric_limits<float>::quiet_NaN()) == 1.0f);   // a NaN is the default, not the limit
    CHECK(TouchTuning::SnapOpacity(std::numeric_limits<float>::quiet_NaN()) == 1.0f);
    CHECK(TouchTuning::SnapSize(std::numeric_limits<float>::infinity()) == TouchTuning::kMaxSize);
    CHECK(TouchTuning::SnapSize(-std::numeric_limits<float>::infinity()) == TouchTuning::kMinSize);

    // The tiles' steps. Whole steps of the grid, so a chain ends EXACTLY on the limit (no ==-with-slack) and a
    // step at the limit changes nothing.
    CHECK(TouchTuning::StepSize(1.0f, -1) == 0.9f);
    float size = 1.0f;
    for (int i = 0; i < 6; ++i) size = TouchTuning::StepSize(size, -1);
    CHECK(size == 0.4f && size == TouchTuning::kMinSize);
    CHECK(TouchTuning::StepSize(size, -1) == size);   // a seventh: unchanged
    size = 1.0f;
    for (const float expected : {1.1f, 1.2f, 1.3f, 1.4f}) {   // MR's range: four steps up
        size = TouchTuning::StepSize(size, +1);
        CHECK(size == expected);
    }
    CHECK(size == TouchTuning::kMaxSize);
    CHECK(TouchTuning::StepSize(size, +1) == size);
    float opacity = 1.0f;
    for (const float expected : {1.2f, 1.4f, 1.6f, 1.8f}) {
        opacity = TouchTuning::StepOpacity(opacity, +1);
        CHECK(opacity == expected);
    }
    CHECK(opacity == TouchTuning::kMaxOpacity && TouchTuning::StepOpacity(opacity, +1) == opacity);
    opacity = 1.0f;
    for (const float expected : {0.8f, 0.6f, 0.4f, 0.2f}) {
        opacity = TouchTuning::StepOpacity(opacity, -1);
        CHECK(opacity == expected);
    }
    CHECK(opacity == TouchTuning::kMinOpacity && TouchTuning::StepOpacity(opacity, -1) == opacity);
    // Eleven sizes and nine opacities in all, each reached from the one before by one tap.
    int sizes = 1;
    for (float s = TouchTuning::kMinSize; TouchTuning::StepSize(s, +1) != s; s = TouchTuning::StepSize(s, +1)) ++sizes;
    int opacities = 1;
    for (float o = TouchTuning::kMinOpacity; TouchTuning::StepOpacity(o, +1) != o; o = TouchTuning::StepOpacity(o, +1)) ++opacities;
    CHECK_EQ(sizes, 11);
    CHECK_EQ(opacities, 9);
    // Off the grid, a step starts from where the value lands: a hand-edited 2.0 is 1.4, and one step down is 1.3.
    CHECK(TouchTuning::StepSize(2.0f, -1) == 1.3f);
    CHECK(TouchTuning::StepSize(std::numeric_limits<float>::quiet_NaN(), +1) == 1.1f);
    CHECK(TouchTuning::StepSize(1.0f, 0) == 1.0f);

    // --touch-tuning's text.
    TouchTuning flag;
    std::string flagError;
    CHECK(TouchTuning::ParseFlag("size=1.2,opacity=0.6,jump=-40:30,dpad=12:0", flag, &flagError));
    CHECK_MSG(flagError.empty(), flagError);
    CHECK(flag.size == 1.2f && flag.opacity == 0.6f);
    CHECK(flag.move[kJump] == Mv(-40.0f, 30.0f));
    CHECK(flag.move[static_cast<std::size_t>(TouchControl::Dpad)] == Mv(12.0f, 0.0f));
    CHECK(flag.move[static_cast<std::size_t>(TouchControl::Pause)].IsZero());
    CHECK(TouchTuning::ParseFlag(" size = 1.1 , jump = 1.5 : -2 ", flag, &flagError));   // spaces are fine
    CHECK(flag.size == 1.1f && flag.opacity == 1.0f && flag.move[kJump] == Mv(1.5f, -2.0f));   // replacing, not merging
    CHECK(TouchTuning::ParseFlag("size=9,opacity=-5,pause=5000:-5000", flag, nullptr));   // clamps, like the file
    CHECK(flag.size == TouchTuning::kMaxSize && flag.opacity == TouchTuning::kMinOpacity);
    CHECK(flag.move[static_cast<std::size_t>(TouchControl::Pause)] == Mv(2048.0f, -2048.0f));
    CHECK(TouchTuning::ParseFlag("size=1.2,size=0.8", flag, nullptr) && flag.size == 0.8f);   // the last wins
    for (const char* bad : {"nope=1", "back=1:1", "jump=5", "jump=a:b", "jump=1:2:3", "size=abc", "size=", "size", "=1",
                            "size=1.2,,opacity=1", "size=1.2,", "", "  ", "size=nan", "opacity=inf", "jump=1:"}) {
        TouchTuning kept;
        kept.size = 1.04f;   // an off-grid caller's value: left Clamped
        flagError.clear();
        CHECK_MSG(!TouchTuning::ParseFlag(bad, kept, &flagError), bad);
        CHECK_MSG(!flagError.empty(), bad);
        CHECK_MSG(kept.size == 1.0f, bad);
        CHECK(!TouchTuning::ParseFlag(bad, kept, nullptr));   // no error text asked for: still no crash
    }
    CHECK(TouchTuning::ParseFlag("nope=1", flag, &flagError) == false && flagError.find("nope") != std::string::npos);
}

// Step 23: what SetWindowProperties becomes. A flip of the windowed flag
// (Alt+Enter, the options' switch) carries the logical screen's size, never a
// mode; a line of the mode list carries its size with the flag as it was.
void testWindowActions() {
    using Kind = WindowAction::Kind;
    using Penumbra::Render::DecideWindowAction;
    const glm::uvec2 logical(1024u, 768u);
    const glm::uvec2 none(0u);
    const glm::uvec2 saved(1280u, 720u);

    // Into fullscreen: at the saved mode, or the desktop's when there is none.
    WindowAction action = DecideWindowAction(false, logical, false, saved);
    CHECK(action.kind == Kind::EnterFullscreen);
    CHECK_MSG(action.size == saved, "the saved mode, not the logical screen's size");
    action = DecideWindowAction(false, logical, false, none);
    CHECK(action.kind == Kind::EnterFullscreen);
    CHECK_MSG(action.size == none, "the desktop's mode");
    // Half a saved mode is none (Settings refuses it anyway).
    action = DecideWindowAction(false, logical, false, glm::uvec2(1280u, 0u));
    CHECK(action.size == none);

    // Out of it: the windowed size to come back to is the window's, not the request's.
    action = DecideWindowAction(true, logical, true, saved);
    CHECK(action.kind == Kind::LeaveFullscreen);

    // A line picked in fullscreen switches the display (0.7.12's device reset);
    // picked in a window it sizes the window.
    action = DecideWindowAction(false, glm::uvec2(800u, 600u), true, saved);
    CHECK(action.kind == Kind::SwitchFullscreenMode);
    CHECK(action.size == glm::uvec2(800u, 600u));
    action = DecideWindowAction(true, glm::uvec2(800u, 600u), false, saved);
    CHECK(action.kind == Kind::ResizeWindow);
    CHECK(action.size == glm::uvec2(800u, 600u));

    // E23: the list's automatic line sends 0 x 0, passed on in either.
    action = DecideWindowAction(false, none, true, saved);
    CHECK(action.kind == Kind::SwitchFullscreenMode);
    CHECK(action.size == none);
    action = DecideWindowAction(true, none, false, saved);
    CHECK(action.kind == Kind::ResizeWindow);
    CHECK(action.size == none);
}

// E23: where fullscreen goes, and what the refresh-rate row offers.
void testAutomaticDisplayMode() {
    using Penumbra::Render::ChooseFullscreen;
    using Penumbra::Render::ChooseRates;
    using Penumbra::Render::FullscreenChoice;
    using Penumbra::Render::RateChoices;
    using Supersonic::DisplayMode;
    using Supersonic::WindowControl;
    constexpr uint32_t kHighest = WindowControl::kHighestRefreshRate;
    // A 1920x1200 panel at 60 Hz on the desktop that also runs 165 at its
    // size (listed; the desktop's own 60 is not), 1280x800 at 60 and 120,
    // 800x600 at 60.
    const std::vector<DisplayMode> modes = {{800, 600, 60}, {1280, 800, 60}, {1280, 800, 120}, {1920, 1200, 165}};
    const DisplayMode desktop{1920, 1200, 60};

    // Automatic: the desktop's size at the highest rate there.
    FullscreenChoice choice = ChooseFullscreen(modes, desktop, glm::uvec2(0u), 0);
    CHECK(choice.size == glm::uvec2(1920u, 1200u));
    CHECK_EQ(choice.rate, kHighest);
    CHECK(choice.mode == (DisplayMode{1920, 1200, 165}));
    CHECK(choice.sizeAutomatic && choice.rateAutomatic);
    CHECK(!choice.sizeFellBack && !choice.rateFellBack);
    // A desktop already at its highest: that mode, which switches nothing.
    choice = ChooseFullscreen({{1920, 1200, 60}}, desktop, glm::uvec2(0u), 0);
    CHECK(choice.mode == desktop);
    // The development laptop's panel as Windows reports it (one source mode, 1920x1200 at 60).
    choice = ChooseFullscreen({{1920, 1200, 60}}, desktop, glm::uvec2(0u), 0);
    CHECK(choice.mode == desktop);

    // A picked size, automatic rate: that size's highest.
    choice = ChooseFullscreen(modes, desktop, glm::uvec2(1280u, 800u), 0);
    CHECK(choice.size == glm::uvec2(1280u, 800u));
    CHECK(choice.mode == (DisplayMode{1280, 800, 120}));
    CHECK(!choice.sizeAutomatic);
    // A picked rate where the size has it; the highest where it does not.
    choice = ChooseFullscreen(modes, desktop, glm::uvec2(1280u, 800u), 60);
    CHECK_EQ(choice.rate, 60u);
    CHECK(choice.mode == (DisplayMode{1280, 800, 60}));
    CHECK(!choice.rateFellBack && !choice.rateAutomatic);
    choice = ChooseFullscreen(modes, desktop, glm::uvec2(800u, 600u), 165);
    CHECK_EQ(choice.rate, kHighest);
    CHECK(choice.mode == (DisplayMode{800, 600, 60}));
    CHECK(choice.rateFellBack);
    // The desktop's own rate at its size, unlisted: offered.
    choice = ChooseFullscreen(modes, desktop, glm::uvec2(0u), 60);
    CHECK(choice.mode == desktop);
    CHECK(!choice.rateFellBack);
    // A saved size this monitor lacks (a file from another monitor): the
    // desktop's, at the highest, and the choice says so.
    choice = ChooseFullscreen(modes, desktop, glm::uvec2(2560u, 1440u), 0);
    CHECK(choice.size == glm::uvec2(1920u, 1200u));
    CHECK(choice.sizeFellBack);
    CHECK(choice.mode == (DisplayMode{1920, 1200, 165}));
    // No monitor at all: nothing to ask for but plain fullscreen.
    choice = ChooseFullscreen({}, DisplayMode{}, glm::uvec2(0u), 0);
    CHECK(choice.size == glm::uvec2(0u));
    // A virtual X server: one mode, its rate unknown.
    const std::vector<DisplayMode> xvfb = {{3000, 1600, 0}};
    choice = ChooseFullscreen(xvfb, DisplayMode{3000, 1600, 0}, glm::uvec2(0u), 0);
    CHECK(choice.size == glm::uvec2(3000u, 1600u));
    CHECK_EQ(choice.mode.refreshRate, 0u);

    // The row: automatic first, then the size's rates, lowest first.
    choice = ChooseFullscreen(modes, desktop, glm::uvec2(0u), 0);
    RateChoices rates = ChooseRates(modes, desktop, choice, 0);
    CHECK(rates.rates == (std::vector<uint32_t>{0, 60, 165}));
    CHECK_EQ(rates.current, 0u);
    CHECK_EQ(rates.automaticRate, 165u);
    choice = ChooseFullscreen(modes, desktop, glm::uvec2(0u), 165);
    rates = ChooseRates(modes, desktop, choice, 165);
    CHECK_EQ(rates.current, 2u);
    // A saved rate the size lacks shows as automatic, which is what runs.
    choice = ChooseFullscreen(modes, desktop, glm::uvec2(1280u, 800u), 165);
    rates = ChooseRates(modes, desktop, choice, 165);
    CHECK(rates.rates == (std::vector<uint32_t>{0, 60, 120}));
    CHECK_EQ(rates.current, 0u);
    CHECK_EQ(rates.automaticRate, 120u);
    // No rate known: the automatic line alone, with no number to show.
    choice = ChooseFullscreen(xvfb, DisplayMode{3000, 1600, 0}, glm::uvec2(0u), 0);
    rates = ChooseRates(xvfb, DisplayMode{3000, 1600, 0}, choice, 0);
    CHECK(rates.rates == (std::vector<uint32_t>{0}));
    CHECK_EQ(rates.automaticRate, 0u);

    // The automatic window's share, and what it comes to on that laptop's panel over
    // a 48-pixel taskbar (the engine's FitWindowedSize).
    CHECK(Penumbra::Render::kAutoWindowFraction == 0.85f);
    CHECK(WindowControl::FitWindowedSize(WindowControl::ScreenRect{0, 0, 1920, 1152}, desktop,
                                         Penumbra::Render::kAutoWindowFraction) == glm::uvec2(1566u, 979u));
}

void testAudioWithoutEngine() {
    // A suite on a bare registry: no engine, so nothing plays and nothing is
    // decoded, but Load still answers whether the file is there.
    const fs::path file = fs::temp_directory_path() /
                          ("penumbra-audio-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".ogg");
    { std::ofstream(file, std::ios::binary) << "OggS"; }
    AudioOutEngine out;
    entt::registry registry;
    out.Attach(registry);
    CHECK(!out.Available());
    CHECK(out.Load(file.string(), false));
    CHECK(!out.Load(file.string() + ".missing", true));
    CHECK_EQ(out.Play(file.string(), true, 1.0f, 0.0f), VoiceId{0});
    CHECK(!out.IsPlaying(0));
    CHECK(!out.IsPlaying(12345));
    out.Set(12345, 0.5f, 0.5f);
    out.Stop(12345);
    out.UnloadAll();
    CHECK_EQ(out.LoadedCount(), std::size_t{0});
    out.Detach();
    std::error_code ec;
    fs::remove(file, ec);
}

// Step 25 (E1's wide menus): a menu in a wide window. What the view of a
// 1024x768 menu snapshot is, with the side margin the Machine collects
// (RenderSnapshot::sideMargin) or without it (4:3, or widescreen off).
View MenuView(glm::uvec2 window, float sideMargin) {
    RenderSnapshot snapshot;
    snapshot.screenSize = vector2(1024.0f, 768.0f);
    snapshot.sideMargin = sideMargin;
    return CameraRig::ComputeView(snapshot, window, true);
}

bool Near(float a, float b, float eps = 0.01f) { return std::fabs(a - b) <= eps; }

// The sizes Step 25 measures: 16:10, 16:9, 20:9, 21:9.
const glm::uvec2 kWideWindows[] = {{1920u, 1200u}, {1920u, 1080u}, {2400u, 1080u}, {2560u, 1080u}};

void testWideMenuView() {
    using Penumbra::Render::kFixedLayoutScreen;
    using Penumbra::Render::kWideMenuMargin;
    using Penumbra::Render::WiderThan;
    for (const glm::uvec2 window : kWideWindows) {
        const std::string size = std::to_string(window.x) + "x" + std::to_string(window.y);
        const View barred = MenuView(window, 0.0f);
        const View open = MenuView(window, kWideMenuMargin);
        // The screen stays exactly where the pillarbox put it: the same scale
        // (the window's height over 768) and the same whole-pixel offset.
        CHECK_MSG(open.scale == barred.scale, size);
        CHECK_MSG(open.viewportMin == barred.viewportMin, size);
        CHECK_MSG(open.viewportMax == barred.viewportMax, size);
        CHECK_MSG(Near(open.scale, static_cast<float>(window.y) / 768.0f, 1e-5f), size);
        const float offset = std::round((static_cast<float>(window.x) - 1024.0f * open.scale) * 0.5f);
        CHECK_MSG(open.viewportMin == glm::vec2(offset, 0.0f), size);
        // Only the bars give way: the whole image is shown, nothing barred.
        CHECK_MSG(barred.openSides == 0.0f && open.openSides == kWideMenuMargin, size);
        CHECK_MSG(open.ShownMin() == glm::vec2(0.0f), size);
        CHECK_MSG(open.ShownMax() == glm::vec2(window), size);
        CHECK_MSG(CameraRig::Bars(open).empty(), size);
        CHECK_MSG(CameraRig::Bars(barred).size() == 2u, size);
        CHECK_MSG(barred.ShownMin() == barred.viewportMin && barred.ShownMax() == barred.viewportMax, size);
        // In logical pixels the shown part runs from -margin to 1024 + margin,
        // the margin being what an E1 level of this window adds each side.
        const float margin = offset / open.scale;
        CHECK_MSG(Near(open.ShownLogicalMin().x, -margin), size);
        CHECK_MSG(Near(open.ShownLogicalMax().x, 1024.0f + margin), size);
        CHECK_MSG(open.ShownLogicalMin().y == 0.0f && open.ShownLogicalMax().y == 768.0f, size);
        const float levelWidth = 768.0f * static_cast<float>(window.x) / static_cast<float>(window.y);
        CHECK_MSG(Near(margin, (levelWidth - 1024.0f) * 0.5f, 0.5f), size);
        CHECK_MSG(barred.ShownLogicalMin() == glm::vec2(0.0f) && barred.ShownLogicalMax() == barred.logicalScreen, size);
        // Logical -> window -> logical, through the pointer's mapping and the
        // HUD's, anywhere on the image (the sides included).
        for (const glm::vec2 logical : {glm::vec2(0.0f), glm::vec2(512.0f, 384.0f), glm::vec2(1024.0f, 768.0f),
                                        glm::vec2(-margin, 100.0f), glm::vec2(1024.0f + margin - 1.0f, 700.0f)}) {
            const glm::vec2 pixel = open.viewportMin + logical * open.scale;
            const glm::vec2 back = InputMapper::WindowToLogical(pixel, open);
            CHECK_MSG(Near(back.x, logical.x) && Near(back.y, logical.y), size);
            const glm::vec2 fraction = open.HudToFraction(logical) * glm::vec2(window);
            CHECK_MSG(Near(fraction.x, pixel.x, 0.05f) && Near(fraction.y, pixel.y, 0.05f), size);
        }
        CHECK_MSG(WiderThan(window, kFixedLayoutScreen), size);
    }

    // Exactly 4:3 is not wider: no margin is collected, and a view that had
    // one would show nothing more (1024x768 is drawn as it always was).
    CHECK(!WiderThan(glm::uvec2(1024u, 768u), kFixedLayoutScreen));
    CHECK(!WiderThan(glm::uvec2(800u, 600u), kFixedLayoutScreen));
    CHECK(WiderThan(glm::uvec2(1025u, 768u), kFixedLayoutScreen));
    CHECK(!WiderThan(glm::uvec2(1280u, 1024u), kFixedLayoutScreen));
    CHECK(!WiderThan(glm::uvec2(0u, 0u), kFixedLayoutScreen));
    const View fourThree = MenuView(glm::uvec2(1024u, 768u), kWideMenuMargin);
    CHECK(fourThree.ShownMin() == glm::vec2(0.0f) && fourThree.ShownMax() == glm::vec2(1024.0f, 768.0f));
    CHECK(CameraRig::Bars(fourThree).empty());
    // Narrower than 4:3 (5:4): still letterboxed, top and bottom.
    const View tall = MenuView(glm::uvec2(1280u, 1024u), kWideMenuMargin);
    CHECK_EQ(CameraRig::Bars(tall).size(), std::size_t{2});
    CHECK(InputMapper::PointerOverBars({640.0f, 10.0f}, tall));
    CHECK(!InputMapper::PointerOverBars({640.0f, 512.0f}, tall));
    // Past 4:1 the margin runs out: bars again beyond it (4000x768: the screen
    // at 1488..2512, shown from 464 to 3536).
    const View ultra = MenuView(glm::uvec2(4000u, 768u), kWideMenuMargin);
    CHECK(ultra.ShownMin() == glm::vec2(464.0f, 0.0f));
    CHECK(ultra.ShownMax() == glm::vec2(3536.0f, 768.0f));
    CHECK_EQ(CameraRig::Bars(ultra).size(), std::size_t{2});
    CHECK(InputMapper::PointerOverBars({100.0f, 384.0f}, ultra));
    CHECK(!InputMapper::PointerOverBars({500.0f, 384.0f}, ultra));
    CHECK(InputMapper::PointerOverBars({3536.0f, 384.0f}, ultra));
    CHECK(!InputMapper::PointerOverBars({3535.0f, 384.0f}, ultra));

    // The scenes it applies to.
    using Penumbra::Render::IsFixedLayoutScene;
    for (const char* scene : {"scenes/menu.esc", "scenes/arena_select.esc", "scenes/videoModes.esc", "scenes/gameover.esc"}) {
        CHECK_MSG(IsFixedLayoutScene(scene), scene);
    }
    for (const char* scene : {"scenes/level1.esc", "scenes/pvp_lv2.esc", "scenes/checkpoint.esc", "", "empty"}) {
        CHECK_MSG(!IsFixedLayoutScene(scene), scene);
    }
}

// A click where a menu item is drawn hits it at every size: the window pixel
// the pillarbox formula puts each of menu.esc's buttons at (its collision box,
// menu.esc: position + <Collision> offset, +-size/2) comes back through the
// mapper inside that box, and the sides hit nothing.
struct MenuButton {
    const char* name;
    glm::vec2 min;
    glm::vec2 max;
};
// menu.esc's seven buttons: each collision box (position + <Collision> offset,
// +-size/2), in the 1024x768 screen.
const MenuButton kMenuButtons[] = {
    {"novo_jogo", {231.5f, 200.5f}, {600.5f, 225.5f}},       {"versus", {281.5f, 258.5f}, {650.5f, 283.5f}},
    {"como_jogar", {196.5f, 314.5f}, {565.5f, 339.5f}},      {"melhores_tempos", {182.5f, 376.5f}, {551.5f, 401.5f}},
    {"opcoes_de_video", {229.5f, 433.5f}, {598.5f, 458.5f}}, {"creditos", {332.5f, 494.5f}, {701.5f, 519.5f}},
    {"sair", {449.5f, 558.5f}, {818.5f, 583.5f}},
};

void testWideMenuHits() {
    using Button = MenuButton;
    const auto& buttons = kMenuButtons;
    std::vector<glm::uvec2> windows(std::begin(kWideWindows), std::end(kWideWindows));
    windows.push_back(glm::uvec2(1024u, 768u));
    for (const glm::uvec2 window : windows) {
        const View view = MenuView(window, Penumbra::Render::kWideMenuMargin);
        // Independently of CameraRig: the 4:3 screen scaled to the height and
        // centred on a whole pixel.
        const float scale = static_cast<float>(window.y) / 768.0f;
        const float left = std::round((static_cast<float>(window.x) - 1024.0f * scale) * 0.5f);
        for (const Button& button : buttons) {
            const std::string what = std::string(button.name) + " at " + std::to_string(window.x) + "x" +
                                     std::to_string(window.y);
            // Its centre and two pixels inside each corner.
            const glm::vec2 inset(2.0f / scale);
            for (const glm::vec2 logical : {(button.min + button.max) * 0.5f, button.min + inset, button.max - inset}) {
                InputMapper mapper;
                mapper.SetControls(Defaults());
                RawDevices raw;
                raw.mouseWindow = glm::vec2(left, 0.0f) + logical * scale;
                raw.mouse[0] = true;
                const InputFrame frame = mapper.BuildTick(raw, view);
                const bool inside = frame.cursor.x > button.min.x && frame.cursor.x < button.max.x &&
                                    frame.cursor.y > button.min.y && frame.cursor.y < button.max.y;
                CHECK_MSG(inside, what);
                CHECK_MSG(frame.keys[K_LMOUSE], what);
                CHECK_MSG(!InputMapper::PointerOverBars(raw.mouseWindow, view), what);
            }
        }
        // The sides: left of the screen's 0 and right of its 1024 - on the
        // image, not over a bar, and on no button.
        if (left >= 2.0f) {
            for (const glm::vec2 pixel : {glm::vec2(1.0f, 210.0f), glm::vec2(static_cast<float>(window.x) - 2.0f, 570.0f)}) {
                InputMapper mapper;
                RawDevices raw;
                raw.mouseWindow = pixel;
                const InputFrame frame = mapper.BuildTick(raw, view);
                CHECK(frame.cursor.x < 0.0f || frame.cursor.x > 1024.0f);
                for (const Button& button : buttons) {
                    CHECK(!(frame.cursor.x > button.min.x && frame.cursor.x < button.max.x &&
                            frame.cursor.y > button.min.y && frame.cursor.y < button.max.y));
                }
                CHECK(!InputMapper::PointerOverBars(pixel, view));
            }
        }
    }
}

// The menus warp the cursor every tick (menu.as:239, videoModes.as:61): a
// still mouse in an open side stays where it is, not pulled to the 4:3 box.
void testWideMenuWarp() {
    const View open = MenuView(glm::uvec2(1920u, 1080u), Penumbra::Render::kWideMenuMargin);
    const float margin = open.viewportMin.x / open.scale;   // 240 px = 170.67 logical
    InputMapper mapper;
    mapper.SetControls(Defaults());
    RawDevices raw;
    raw.mouseWindow = {20.0f, 540.0f};
    InputFrame frame = mapper.BuildTick(raw, open);
    const float expected = (20.0f - 240.0f) / open.scale;
    CHECK(Near(frame.cursor.x, expected));
    mapper.EndFrame(raw);
    mapper.WarpCursor(frame.cursorAbsolute + glm::vec2(0.0f), open);   // the script's SetCursorPos(abs + 0)
    CHECK(Near(mapper.Cursor().x, expected));
    frame = mapper.BuildTick(raw, open);
    CHECK(Near(frame.cursor.x, expected));
    mapper.EndFrame(raw);
    // A pad pushing past what is shown stops at its edge.
    mapper.WarpCursor({-5000.0f, 384.0f}, open);
    CHECK(Near(mapper.Cursor().x, -margin));
    mapper.WarpCursor({5000.0f, 900.0f}, open);
    CHECK(Near(mapper.Cursor().x, 1024.0f + margin));
    CHECK_NEAR(mapper.Cursor().y, 768.0f);
    // Barred (4:3, widescreen off): the 4:3 box, exactly as before.
    const View barred = MenuView(glm::uvec2(1920u, 1080u), 0.0f);
    mapper.WarpCursor({-5000.0f, -5.0f}, barred);
    CHECK(mapper.Cursor() == glm::vec2(0.0f));
    mapper.WarpCursor({5000.0f, 5000.0f}, barred);
    CHECK(mapper.Cursor() == glm::vec2(1024.0f, 768.0f));

    // The system pointer over what remains barred, and nowhere else.
    CHECK(!InputMapper::PointerOverBars({20.0f, 540.0f}, open));
    CHECK(InputMapper::PointerOverBars({20.0f, 540.0f}, barred));
    CHECK(!InputMapper::PointerOverBars({1919.0f, 1079.0f}, open));
    CHECK(!InputMapper::PointerOverBars({1920.0f, 540.0f}, open));   // off the image
    View inEditor = open;
    inEditor.imageOrigin = {300.0f, 40.0f};
    CHECK(!InputMapper::PointerOverBars({310.0f, 580.0f}, inEditor));
    CHECK(!InputMapper::PointerOverBars({100.0f, 580.0f}, inEditor));
}

// === E25: a phone-sized UI (render/PhoneUi.hpp) =================================================

// settings.json "zoom" read, written and read back, and the zoom's rules: none
// without the touch controls, whatever it says; automatic 175% on a
// phone-shaped screen and 125% on any other; and never past where a message
// line would lose its room before the pause button (MaxZoom; E26's frame is
// testPhoneHudFrame's). The screen a campaign level gets at it.
void testPhoneZoom() {
    using namespace Penumbra::Render;
    const Settings en = Settings::Defaults("en");
    CHECK_EQ(en.zoom, 0);
    CHECK(en.ToJson().find("\"zoom\": \"auto\"") != std::string::npos);
    std::string warning;
    const auto zoomOf = [&](const std::string& json) {
        warning.clear();
        return Settings::FromJson(json, en, &warning).zoom;
    };
    CHECK_EQ(zoomOf(R"({"zoom": "auto"})"), 0);
    CHECK(warning.empty());
    CHECK_EQ(zoomOf(R"({"zoom": "AUTO"})"), 0);
    CHECK_EQ(zoomOf(R"({"zoom": 150})"), 150);
    CHECK(warning.empty());
    CHECK_EQ(zoomOf(R"({"zoom": 137.6})"), 138);
    CHECK_EQ(zoomOf(R"({"zoom": 50})"), 100);
    CHECK_EQ(zoomOf(R"({"zoom": 400})"), 200);
    CHECK_EQ(zoomOf(R"({"zoom": "big"})"), 0);
    CHECK(warning.find("zoom") != std::string::npos);
    CHECK_EQ(zoomOf(R"({"zoom": true})"), 0);
    CHECK(warning.find("zoom") != std::string::npos);
    CHECK_EQ(zoomOf(R"({"language": "en"})"), 0);   // a file from before E25: automatic
    for (const int zoom : {0, 100, 125, 150, 175, 200, 130}) {
        Settings changed = en;
        changed.zoom = zoom;
        const Settings back = Settings::FromJson(changed.ToJson(), en, &warning);
        CHECK_EQ(back.zoom, zoom);
        CHECK(back == changed);
    }
    CHECK_EQ(ClampZoomSetting(0), 0);
    CHECK_EQ(ClampZoomSetting(90), 100);
    CHECK_EQ(ClampZoomSetting(250), 200);

    // Phone-shaped: 18:9 and longer, either way up; 16:9 and every tablet not.
    for (const glm::uvec2 phone : {glm::uvec2(2400u, 1080u), glm::uvec2(2340u, 1080u), glm::uvec2(2160u, 1080u),
                                   glm::uvec2(2520u, 1080u), glm::uvec2(1080u, 2400u)}) {
        CHECK_MSG(IsPhoneShaped(phone), std::to_string(phone.x) + "x" + std::to_string(phone.y));
    }
    for (const glm::uvec2 other : {glm::uvec2(1920u, 1080u), glm::uvec2(2048u, 1536u), glm::uvec2(2560u, 1600u),
                                   glm::uvec2(2360u, 1640u), glm::uvec2(0u, 0u)}) {
        CHECK_MSG(!IsPhoneShaped(other), std::to_string(other.x) + "x" + std::to_string(other.y));
    }
    // What is asked.
    CHECK_EQ(ZoomPercent(0, false, {2400u, 1080u}), 100);     // no touch controls: no zoom
    CHECK_EQ(ZoomPercent(175, false, {2400u, 1080u}), 100);   // whatever the setting says
    CHECK_EQ(ZoomPercent(0, true, {2400u, 1080u}), 175);
    CHECK_EQ(ZoomPercent(0, true, {1920u, 1080u}), 125);
    CHECK_EQ(ZoomPercent(0, true, {2048u, 1536u}), 125);
    CHECK_EQ(ZoomPercent(175, true, {2048u, 1536u}), 175);
    CHECK_EQ(ZoomPercent(100, true, {2400u, 1080u}), 100);

    // What a campaign level gets, and its screen: the window's shape, whole
    // pixels, never narrower than 1024; a zoom of 1 is E1's screen exactly.
    struct Case {
        glm::uvec2 window;
        bool widescreen;
        bool touch;
        int setting;
        float zoom;
        glm::vec2 screen;
    };
    const Case cases[] = {
        {{2400u, 1080u}, true, true, 0, 1.75f, {976.0f, 439.0f}},          // a 20:9 phone, automatic
        {{2400u, 1080u}, true, true, 125, 1.25f, {1364.0f, 614.0f}},
        {{2400u, 1080u}, true, true, 175, 1.75f, {976.0f, 439.0f}},
        // Held where a message keeps 795 px to the pause button: 197% (no frame; E26's is
        // testPhoneHudFrame's), (1707 - 116) / 805.
        {{2400u, 1080u}, true, true, 200, 1591.0f / 805.0f, {864.0f, 389.0f}},
        {{2400u, 1080u}, true, false, 150, 1.0f, {1707.0f, 768.0f}},       // no touch controls: E1's
        {{2400u, 1080u}, false, true, 0, 908.0f / 805.0f, {908.0f, 681.0f}},   // 4:3 levels: 113% at most
        {{2520u, 1080u}, true, true, 200, 2.0f, {896.0f, 384.0f}},         // 21:9: 200%
        {{1920u, 1080u}, true, true, 0, 1.25f, {1092.0f, 614.0f}},         // 16:9, automatic
        {{2560u, 1600u}, true, true, 0, 1.25f, {982.0f, 614.0f}},          // a 16:10 tablet, automatic
        {{2048u, 1536u}, true, true, 0, 908.0f / 805.0f, {908.0f, 681.0f}},   // a 4:3 tablet: 125% held at 113%
    };
    for (const Case& c : cases) {
        const std::string what = std::to_string(c.window.x) + "x" + std::to_string(c.window.y) + " zoom " +
                                 std::to_string(c.setting) + (c.widescreen ? "" : " 4:3") + (c.touch ? "" : " no touch");
        const float zoom = CampaignZoom(c.setting, c.touch, c.window, c.widescreen);
        const glm::vec2 screen = ZoomedScreen(c.window, c.widescreen, zoom);
        std::printf("  E25 %s: %.3f, screen %.0fx%.0f\n", what.c_str(), zoom, screen.x, screen.y);
        CHECK_MSG(Near(zoom, c.zoom, 0.002f), what + ": " + std::to_string(zoom));
        CHECK_MSG(screen == c.screen, what);
        // The message rule, whole pixels and all.
        CHECK_MSG(MessageClearance(screen, HudFrame{}) >= kMessageRoom + kMessageGap, what);
        if (c.widescreen) {
            const float aspect = static_cast<float>(c.window.x) / static_cast<float>(c.window.y);
            CHECK_MSG(std::fabs(screen.x - screen.y * aspect) <= 1.0f, what);   // no bars
        }
    }
    // The options screen's steps: none past what the screen can show.
    CHECK_EQ(MaxZoomPercent({2400u, 1080u}, true), 197);
    CHECK_EQ(MaxZoomPercent({2560u, 1080u}, true), 211);
    CHECK_EQ(MaxZoomPercent({1920u, 1080u}, true), 155);
    CHECK_EQ(MaxZoomPercent({2048u, 1536u}, true), 112);
    CHECK_EQ(MaxZoomPercent({2400u, 1080u}, false), 112);
    // A zoom of 1 is E1's own screen, in any window.
    for (const glm::uvec2 window : kWideWindows) {
        const float aspect = static_cast<float>(window.x) / static_cast<float>(window.y);
        CHECK(ZoomedScreen(window, true, 1.0f) == glm::vec2(std::max(1024.0f, std::round(768.0f * aspect)), 768.0f));
        CHECK(ZoomedScreen(window, false, 1.0f) == glm::vec2(1024.0f, 768.0f));
    }
    // The scenes it applies to: the campaign's, the checkpoint a death reloads
    // included; never the menus, the options, game over or the arenas.
    for (const char* scene : {"scenes/level1.esc", "scenes/level2.esc", "scenes/level3.esc", "scenes/checkpoint.esc"}) {
        CHECK_MSG(IsCampaignScene(scene), scene);
    }
    for (const char* scene : {"scenes/menu.esc", "scenes/arena_select.esc", "scenes/videoModes.esc",
                              "scenes/gameover.esc", "scenes/pvp_lv1.esc", "scenes/pvp_lv6.esc", ""}) {
        CHECK_MSG(!IsCampaignScene(scene), scene);
    }
    // A zoomed campaign scene goes back to E1's screen once the campaign is
    // finished or while the princess is there; an unzoomed one, an arena (her
    // Versus) or the menus never do.
    const glm::vec2 zoomed(1138.0f, 512.0f);
    const glm::vec2 plain(1707.0f, 768.0f);
    CHECK(CampaignUnzooms("scenes/level1.esc", zoomed, false, true));
    CHECK(CampaignUnzooms("scenes/checkpoint.esc", zoomed, false, true));
    CHECK(CampaignUnzooms("scenes/level3.esc", zoomed, true, false));
    CHECK(!CampaignUnzooms("scenes/level1.esc", zoomed, false, false));
    CHECK(!CampaignUnzooms("scenes/level1.esc", plain, true, true));
    CHECK(!CampaignUnzooms("scenes/pvp_lv1.esc", zoomed, false, true));
    CHECK(!CampaignUnzooms("scenes/menu.esc", zoomed, true, true));
}

// ENHANCEMENT E26: the HUD's safe frame. settings.json "edgeMargin" read,
// written and read back; the margin asked for (none without the touch
// controls; automatic 3.5% on a phone-shaped screen, 1% on another); the
// frame a level's screen gets from it and the display's safe area, whichever
// keeps more, less the bars; and the message rule on every screen the touch
// controls draw a level on - the phone and tablet shapes, their safe areas,
// the automatic margin, every zoom the Zoom row offers there and E1's
// unzoomed screen (co-op, the arenas, 100%): a message line keeps its room
// and the gap up to the pause button, and the row offers the steps it did
// before the frame (175% on 20:9, 150% on 18:9; a 4:3 tablet still zoomed).
void testPhoneHudFrame() {
    using namespace Penumbra::Render;
    using Supersonic::SafeAreaInsets;
    const Settings en = Settings::Defaults("en");
    CHECK(en.edgeMargin < 0.0f);
    CHECK(en.ToJson().find("\"edgeMargin\": \"auto\"") != std::string::npos);
    std::string warning;
    const auto marginOf = [&](const std::string& json) {
        warning.clear();
        return Settings::FromJson(json, en, &warning).edgeMargin;
    };
    CHECK(marginOf(R"({"edgeMargin": "auto"})") < 0.0f);
    CHECK(warning.empty());
    CHECK(marginOf(R"({"edgeMargin": "Auto"})") < 0.0f);
    CHECK(Near(marginOf(R"({"edgeMargin": 3.5})"), 3.5f));
    CHECK(warning.empty());
    CHECK(Near(marginOf(R"({"edgeMargin": 0})"), 0.0f));
    CHECK(Near(marginOf(R"({"edgeMargin": 12})"), 8.0f));
    CHECK(Near(marginOf(R"({"edgeMargin": -2})"), 0.0f));
    CHECK(marginOf(R"({"edgeMargin": "wide"})") < 0.0f);
    CHECK(warning.find("edgeMargin") != std::string::npos);
    CHECK(marginOf(R"({"language": "en"})") < 0.0f);   // a file from before E26: automatic
    for (const float margin : {-1.0f, 0.0f, 2.5f, 3.5f, 8.0f}) {
        Settings changed = en;
        changed.edgeMargin = margin;
        const Settings back = Settings::FromJson(changed.ToJson(), en, &warning);
        CHECK(back == changed);
    }
    CHECK(ClampEdgeMarginSetting(-5.0f) == kEdgeMarginAuto);
    CHECK(ClampEdgeMarginSetting(20.0f) == kEdgeMarginMaxPercent);

    // What is asked.
    CHECK_EQ(EdgeMarginPercent(kEdgeMarginAuto, false, {2400u, 1080u}), 0.0f);   // no touch controls: none
    CHECK_EQ(EdgeMarginPercent(5.0f, false, {2400u, 1080u}), 0.0f);              // whatever the setting says
    CHECK_EQ(EdgeMarginPercent(kEdgeMarginAuto, true, {2400u, 1080u}), kPhoneEdgeMarginPercent);
    CHECK_EQ(EdgeMarginPercent(kEdgeMarginAuto, true, {2048u, 1536u}), kTabletEdgeMarginPercent);
    CHECK_EQ(EdgeMarginPercent(kEdgeMarginAuto, true, {1920u, 1080u}), kTabletEdgeMarginPercent);
    CHECK_EQ(EdgeMarginPercent(0.0f, true, {2400u, 1080u}), 0.0f);
    CHECK_EQ(EdgeMarginPercent(6.0f, true, {2400u, 1080u}), 6.0f);
    CHECK_EQ(EdgeMarginPercent(30.0f, true, {2400u, 1080u}), kEdgeMarginMaxPercent);

    // The frame, on a 20:9 phone: 3.5% of 2400 px at the sides and of 1080 at
    // the top, in the screen's pixels - E1's (1.40625 image px each) and 150%'s
    // (2.109) - and nothing at the bottom, where no HUD is.
    const glm::uvec2 phone(2400u, 1080u);
    HudFrame frame = ComputeHudFrame(phone, {1707.0f, 768.0f}, SafeAreaInsets{}, 3.5f);
    CHECK(Near(frame.left, 84.0f / 1.40625f, 0.05f) && Near(frame.right, 84.0f / 1.40625f, 0.6f));
    CHECK(Near(frame.top, 37.8f / 1.40625f, 0.05f) && frame.bottom == 0.0f);
    frame = ComputeHudFrame(phone, {1138.0f, 512.0f}, SafeAreaInsets{}, 3.5f);
    const float zoomedScale = 1080.0f / 512.0f;
    CHECK(Near(frame.left, 84.0f / zoomedScale, 0.05f) && Near(frame.top, 37.8f / zoomedScale, 0.05f));
    // A notch wider than the margin decides its side; a bar at the bottom only the bottom.
    frame = ComputeHudFrame({2532u, 1170u}, {1662.0f, 768.0f}, SafeAreaInsets{132.0f, 0.0f, 0.0f, 63.0f}, 3.5f);
    const float iphoneScale = 1170.0f / 768.0f;
    CHECK(Near(frame.left, 132.0f / iphoneScale, 0.05f));
    CHECK(Near(frame.right, 0.035f * 2532.0f / iphoneScale, 0.6f));
    CHECK(Near(frame.bottom, 63.0f / iphoneScale, 0.05f));
    // 4:3 levels on a phone: the bars keep the sides clear; the top is still kept.
    frame = ComputeHudFrame(phone, {1024.0f, 768.0f}, SafeAreaInsets{}, 3.5f);
    CHECK(frame.left == 0.0f && frame.right == 0.0f && Near(frame.top, 37.8f / 1.40625f, 0.05f));
    // No margin and no safe area: the original's edges.
    CHECK(ComputeHudFrame(phone, {1707.0f, 768.0f}, SafeAreaInsets{}, 0.0f).IsZero());
    CHECK(ComputeHudFrame({1024u, 768u}, {1024.0f, 768.0f}, SafeAreaInsets{}, 0.0f).IsZero());

    // A margin set by hand is held to what leaves a message its room on E1's
    // screen: a 4:3 tablet takes 5%, a phone all 8.
    const float tabletMax = FittedEdgeMargin(8.0f, {2048u, 1536u}, true, SafeAreaInsets{});
    std::printf("  E26 a 4:3 tablet's margin set to 8%%: %.2f%%\n", tabletMax);
    CHECK(tabletMax > 4.9f && tabletMax < 5.1f);
    CHECK_EQ(FittedEdgeMargin(1.0f, {2048u, 1536u}, true, SafeAreaInsets{}), 1.0f);
    CHECK_EQ(FittedEdgeMargin(8.0f, phone, true, SafeAreaInsets{}), 8.0f);
    CHECK_EQ(FittedEdgeMargin(-1.0f, phone, true, SafeAreaInsets{}), 0.0f);

    // The rule, everywhere a level is drawn with the touch controls.
    struct Shape {
        const char* name;
        glm::uvec2 window;
        SafeAreaInsets safe;
    };
    const Shape shapes[] = {
        {"18:9", {2160u, 1080u}, {}},
        {"20:9", {2400u, 1080u}, {}},
        {"19.5:9, a notch", {2532u, 1170u}, {132.0f, 0.0f, 0.0f, 63.0f}},
        {"19.5:9, a notch either side", {2532u, 1170u}, {132.0f, 0.0f, 132.0f, 63.0f}},
        {"21:9", {2520u, 1080u}, {}},
        {"16:9", {1920u, 1080u}, {}},
        {"16:10", {2560u, 1600u}, {}},
        {"3:2", {2160u, 1440u}, {}},
        {"10.9-inch", {2360u, 1640u}, {}},
        {"4:3", {2048u, 1536u}, {}},
    };
    const float needs = kMessageRoom + kMessageGap;
    for (const Shape& shape : shapes) {
        for (const bool widescreen : {true, false}) {
            const std::string what = std::string(shape.name) + (widescreen ? "" : ", 4:3 levels");
            const float margin =
                FittedEdgeMargin(EdgeMarginPercent(kEdgeMarginAuto, true, shape.window), shape.window, widescreen,
                                 shape.safe);
            const int limit = MaxZoomPercent(shape.window, widescreen, shape.safe, margin);
            const float automatic = CampaignZoom(kZoomAutomatic, true, shape.window, widescreen, shape.safe, margin);
            float least = 1e9f;
            std::vector<float> zooms = {1.0f, automatic, MaxZoom(shape.window, widescreen, shape.safe, margin)};
            for (const int step : kZoomSteps) {
                if (step <= limit) zooms.push_back(static_cast<float>(step) / 100.0f);
            }
            for (const float zoom : zooms) {
                const glm::vec2 screen = ZoomedScreen(shape.window, widescreen, zoom);
                const HudFrame at = ComputeHudFrame(shape.window, screen, shape.safe, margin);
                const float clear = MessageClearance(screen, at);
                least = std::min(least, clear);
                CHECK_MSG(clear >= needs, what + " at " + std::to_string(zoom) + ": " + std::to_string(clear));
                CHECK_MSG(at.left >= 0.0f && at.top >= 0.0f && at.right >= 0.0f, what);
            }
            std::printf("  E26 %s: margin %.2f%%, zoom up to %d%%, automatic %.0f%%, a message's room at least %.0f px\n",
                        what.c_str(), margin, limit, automatic * 100.0f, least - kMessageGap);
            CHECK_MSG(automatic > 1.0f, what);   // every touch screen still zooms in
        }
    }
    // The steps the Zoom row offers, as before the frame.
    const auto limitOf = [](glm::uvec2 window, SafeAreaInsets safe = {}) {
        const float margin =
            FittedEdgeMargin(EdgeMarginPercent(kEdgeMarginAuto, true, window), window, true, safe);
        return MaxZoomPercent(window, true, safe, margin);
    };
    CHECK(limitOf({2400u, 1080u}) >= 175);
    CHECK(limitOf({2340u, 1080u}) >= 175);
    CHECK(limitOf({2160u, 1080u}) >= 150);
    CHECK(limitOf({2532u, 1170u}, {132.0f, 0.0f, 132.0f, 63.0f}) >= 150);
    CHECK(limitOf({1920u, 1080u}) >= 150);
    CHECK(limitOf({2560u, 1600u}) >= 125);
    CHECK(limitOf({2160u, 1440u}) >= 125);
}

// ENHANCEMENT E31: what a window shows of a fixed-layout scene (the options screen), the frame inside it, and the phone   // E31
// options layout's geometry on it (game/script/optionsPhone.cpp). Pure: no Machine, no layer.                           // E31
Penumbra::Script::OptionsArea OptionsAreaOf(const Penumbra::Render::FixedLayoutArea& area) {                            // E31
    return Penumbra::Script::OptionsArea{vector2(area.shownMin.x, area.shownMin.y), vector2(area.shownMax.x, area.shownMax.y),   // E31
                                         area.frame.left, area.frame.top, area.frame.right, area.frame.bottom};         // E31
}                                                                                                                       // E31

void testFixedLayoutArea() {                                                                                            // E31
    using Penumbra::Render::ComputeFixedLayoutArea;                                                                     // E31
    using Penumbra::Render::FixedLayoutArea;                                                                            // E31
    const Supersonic::SafeAreaInsets none{};                                                                            // E31
    const float open = Penumbra::Render::kWideMenuMargin;                                                               // E31

    // A 4:3 window with no sides to show: the screen itself, the frame the margin (1% of the width, of the height) leaves.   // E31
    FixedLayoutArea area = ComputeFixedLayoutArea({1024u, 768u}, 0.0f, none, 1.0f);                                     // E31
    CHECK(area.shownMin == glm::vec2(0.0f, 0.0f) && area.shownMax == glm::vec2(1024.0f, 768.0f));                       // E31
    CHECK(Near(area.frame.left, 10.24f) && Near(area.frame.right, 10.24f) && Near(area.frame.top, 7.68f) && area.frame.bottom == 0.0f);   // E31
    // A 20:9 phone with the sides shown: E1's view, 1.40625 image px per logical px, 341.33 logical px past each side.    // E31
    area = ComputeFixedLayoutArea({2400u, 1080u}, open, none, 3.5f);                                                    // E31
    CHECK(Near(area.shownMin.x, -341.3333f, 0.01f) && Near(area.shownMax.x, 1365.3333f, 0.01f));                       // E31
    CHECK(area.shownMin.y == 0.0f && area.shownMax.y == 768.0f);                                                        // E31
    CHECK(Near(area.frame.left, 59.73f, 0.1f) && Near(area.frame.right, 59.73f, 0.1f) && Near(area.frame.top, 26.87f, 0.1f) && area.frame.bottom == 0.0f);   // E31
    // The same phone with the sides not shown (widescreen off at the scene's load): the 4:3 box between bars, and the     // E31
    // margin falls in the bars.                                                                                          // E31
    area = ComputeFixedLayoutArea({2400u, 1080u}, 0.0f, none, 3.5f);                                                    // E31
    CHECK(area.shownMin.x == 0.0f && area.shownMax.x == 1024.0f && area.frame.left == 0.0f && area.frame.right == 0.0f);   // E31
    // A 16:9 tablet at 1%.                                                                                               // E31
    area = ComputeFixedLayoutArea({1920u, 1080u}, open, none, 1.0f);                                                    // E31
    CHECK(Near(area.shownMin.x, -170.6667f, 0.01f) && Near(area.shownMax.x, 1194.6667f, 0.01f));                        // E31
    CHECK(Near(area.frame.left, 13.65f, 0.1f) && Near(area.frame.top, 7.68f, 0.1f));                                    // E31
    // A notch at both sides and a home indicator: the safe area decides, in logical px (1.5234 image px each).            // E31
    area = ComputeFixedLayoutArea({2532u, 1170u}, open, Supersonic::SafeAreaInsets{132.0f, 0.0f, 132.0f, 63.0f}, 3.5f);  // E31
    CHECK(Near(area.shownMin.x, -319.0f, 0.5f) && Near(area.shownMax.x, 1343.0f, 0.5f));                                // E31
    CHECK(Near(area.frame.left, 86.6f, 0.1f) && Near(area.frame.right, 86.6f, 0.1f) && Near(area.frame.bottom, 41.4f, 0.1f));   // E31
    // A zero window (before the first frame): the 4:3 screen and no frame.                                               // E31
    area = ComputeFixedLayoutArea({0u, 0u}, open, Supersonic::SafeAreaInsets{50.0f, 50.0f, 50.0f, 50.0f}, 3.5f);        // E31
    CHECK(area.shownMin == glm::vec2(0.0f, 0.0f) && area.shownMax == glm::vec2(1024.0f, 768.0f) && area.frame.IsZero());   // E31

    // The rectangle is what CameraRig::ComputeView and View::ShownLogicalMin / Max say the window shows.                // E31
    const glm::uvec2 windows[] = {{2400u, 1080u}, {1920u, 1080u}, {2532u, 1170u}, {1280u, 800u}, {1024u, 768u},         // E31
                                  {2560u, 1600u}, {3120u, 1440u}, {800u, 600u}, {1000u, 1000u}, {2340u, 1080u}};          // E31
    for (const glm::uvec2 window : windows) {                                                                           // E31
        for (const float sides : {0.0f, open}) {                                                                        // E31
            RenderSnapshot snapshot;                                                                                    // E31
            snapshot.screenSize = vector2(1024.0f, 768.0f);                                                             // E31
            snapshot.sideMargin = sides;                                                                                // E31
            const View view = CameraRig::ComputeView(snapshot, window, true, nullptr);                                  // E31
            area = ComputeFixedLayoutArea(window, sides, none, 0.0f);                                                   // E31
            const std::string what = std::to_string(window.x) + "x" + std::to_string(window.y) + " sides " + std::to_string(sides);   // E31
            CHECK_MSG(Near(area.shownMin.x, view.ShownLogicalMin().x, 1e-3f) && Near(area.shownMax.x, view.ShownLogicalMax().x, 1e-3f), what);   // E31
            CHECK_MSG(area.shownMin.y == view.ShownLogicalMin().y && area.shownMax.y == view.ShownLogicalMax().y, what);   // E31
        }                                                                                                               // E31
    }                                                                                                                   // E31
}                                                                                                                       // E31

using Penumbra::Script::PhoneCell;   // E31
using Penumbra::Script::PhoneRect;   // E31

bool SameRect(const PhoneRect& r, float x, float y, float w, float h) {   // E31
    return r.x == x && r.y == y && r.w == w && r.h == h;   // E31
}   // E31

std::string RectText(const PhoneRect& r) {   // E31
    return "(" + std::to_string(r.x) + ", " + std::to_string(r.y) + ", " + std::to_string(r.w) + ", " + std::to_string(r.h) + ")";   // E31
}   // E31

// Strictly inside each other's area: boxes that only touch do not overlap (the scripts' hit test is strict).   // E31
bool Overlap(const PhoneRect& a, const PhoneRect& b) {   // E31
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;   // E31
}   // E31

// Every box a tap acts on, named: a toggle's whole cell, a chooser's or stepper's two buttons, the language buttons, Back.   // E31
std::vector<std::pair<std::string, PhoneRect>> HitBoxes(const Penumbra::Script::PhoneOptionsLayout& l) {   // E31
    using namespace Penumbra::Script;   // E31
    std::vector<std::pair<std::string, PhoneRect>> boxes;   // E31
    for (const PhoneCell c : {PC_PIXEL_SHADERS, PC_SMOOTH_MOTION, PC_WIDESCREEN, PC_PAUSE_FOCUS, PC_TOUCH, PC_ADJUST, PC_KEYBOARD_P2, PC_JOYSTICK}) {   // E31
        if (l.present[c]) boxes.emplace_back("cell " + std::to_string(c), l.cell[c]);   // E31
    }   // E31
    for (const PhoneCell c : {PC_REFRESH, PC_ZOOM, PC_MUSIC, PC_EFFECTS}) {   // E31
        if (!l.present[c]) continue;   // E31
        boxes.emplace_back("less " + std::to_string(c), l.less[c]);   // E31
        boxes.emplace_back("more " + std::to_string(c), l.more[c]);   // E31
    }   // E31
    boxes.emplace_back("language less", l.langLess);   // E31
    boxes.emplace_back("language more", l.langMore);   // E31
    boxes.emplace_back("back", l.backHit);   // E31
    return boxes;   // E31
}   // E31

void testPhoneOptionsLayout() {   // E31
    using namespace Penumbra::Script;   // E31
    using Penumbra::Render::ComputeFixedLayoutArea;   // E31
    const float open = Penumbra::Render::kWideMenuMargin;   // E31

    // The 20:9 phone, 2400x1080: the body is 12..1012 whatever the window; the header goes to the   // E31
    // shown area's corners, inside the frame.                                                                          // E31
    OptionsArea phone = OptionsAreaOf(ComputeFixedLayoutArea({2400u, 1080u}, open, Supersonic::SafeAreaInsets{}, 3.5f));   // E31
    PhoneOptionsLayout l = phoneOptionsLayout(phone, true, true);   // E31
    CHECK_EQ(l.hc, 88.0f);   // E31
    CHECK_MSG(SameRect(l.panel, 12, 129, 1000, 624), "panel " + RectText(l.panel));   // E31
    CHECK_MSG(SameRect(l.back, -274, 35, 123, 92), "back " + RectText(l.back));   // E31
    CHECK_MSG(SameRect(l.backHit, -343, -1, 212, 136), "back hit " + RectText(l.backHit));   // one px past the corner on the left and top edges   // E31
    CHECK(l.title.x == -135.0f && l.title.y == 61.0f);   // E31
    CHECK_MSG(SameRect(l.globe, 893, 53, 56, 56), "globe " + RectText(l.globe));   // E31
    CHECK(SameRect(l.langLess, 957, 39, 86, 84) && SameRect(l.langValue, 1043, 39, 168, 84) && SameRect(l.langMore, 1211, 39, 86, 84));   // E31
    CHECK(SameRect(l.cell[PC_PIXEL_SHADERS], 28, 145, 474, 88) && SameRect(l.cell[PC_SMOOTH_MOTION], 522, 145, 474, 88));   // E31
    CHECK(SameRect(l.cell[PC_WIDESCREEN], 28, 239, 474, 88) && SameRect(l.cell[PC_PAUSE_FOCUS], 522, 239, 474, 88));   // E31
    CHECK(SameRect(l.cell[PC_TOUCH], 28, 333, 474, 88) && SameRect(l.cell[PC_ADJUST], 522, 333, 474, 88));   // E31
    CHECK(SameRect(l.cell[PC_KEYBOARD_P2], 28, 427, 474, 88) && SameRect(l.cell[PC_JOYSTICK], 522, 427, 474, 88));   // E31
    CHECK(SameRect(l.cell[PC_REFRESH], 28, 525, 474, 118) && SameRect(l.cell[PC_ZOOM], 522, 525, 474, 118));   // E31
    CHECK(SameRect(l.less[PC_REFRESH], 32, 555, 86, 88) && SameRect(l.value[PC_REFRESH], 118, 555, 294, 88) &&   // E31
          SameRect(l.more[PC_REFRESH], 412, 555, 86, 88));   // E31
    CHECK(SameRect(l.less[PC_ZOOM], 526, 555, 86, 88) && SameRect(l.value[PC_ZOOM], 612, 555, 294, 88) &&   // E31
          SameRect(l.more[PC_ZOOM], 906, 555, 86, 88));   // E31
    CHECK(SameRect(l.cell[PC_MUSIC], 28, 649, 474, 88) && SameRect(l.cell[PC_EFFECTS], 522, 649, 474, 88));   // E31
    CHECK(SameRect(l.less[PC_MUSIC], 258, 649, 86, 88) && SameRect(l.value[PC_MUSIC], 344, 649, 68, 88) &&   // E31
          SameRect(l.more[PC_MUSIC], 412, 649, 86, 88));   // E31
    CHECK(SameRect(l.less[PC_EFFECTS], 752, 649, 86, 88) && SameRect(l.value[PC_EFFECTS], 838, 649, 68, 88) &&   // E31
          SameRect(l.more[PC_EFFECTS], 906, 649, 86, 88));   // E31
    for (int c = 0; c < PC_COUNT; ++c) CHECK_MSG(l.present[c], "cell " + std::to_string(c));   // E31

    // The 4:3 screen (area 0..1024, frame 10 / 8 / 10 / 0): the rows at y 126, 220, 314, 408, the choosers at 506, the volumes at 630.   // E31
    l = phoneOptionsLayout(OptionsAreaOf(ComputeFixedLayoutArea({1024u, 768u}, 0.0f, Supersonic::SafeAreaInsets{}, 1.0f)), true, true);   // E31
    CHECK_EQ(l.hc, 88.0f);   // E31
    CHECK(SameRect(l.back, 18, 16, 123, 92) && SameRect(l.panel, 12, 110, 1000, 624));   // E31
    CHECK(SameRect(l.cell[PC_PIXEL_SHADERS], 28, 126, 474, 88) && SameRect(l.cell[PC_WIDESCREEN], 28, 220, 474, 88));   // E31
    CHECK(SameRect(l.cell[PC_ADJUST], 522, 314, 474, 88) && SameRect(l.cell[PC_JOYSTICK], 522, 408, 474, 88));   // E31
    CHECK(SameRect(l.cell[PC_REFRESH], 28, 506, 474, 118) && SameRect(l.cell[PC_EFFECTS], 522, 630, 474, 88));   // E31
    CHECK(SameRect(l.langLess, 666, 20, 86, 84));   // E31
    // The old single column's spots mean something else here: the touch row's second line (255, 207) is a pixel shaders cell,   // E31
    // the old Back arrow's corner (906, 6) the language's [>] and the old language row (255, 564) the refresh cell.     // E31
    CHECK(l.cell[PC_PIXEL_SHADERS].x < 255.0f && 255.0f < l.cell[PC_PIXEL_SHADERS].x + l.cell[PC_PIXEL_SHADERS].w);   // E31
    CHECK(l.cell[PC_PIXEL_SHADERS].y < 207.0f && 207.0f < l.cell[PC_PIXEL_SHADERS].y + l.cell[PC_PIXEL_SHADERS].h);   // E31
    CHECK(l.langMore.x < 930.0f && 930.0f < l.langMore.x + l.langMore.w && l.langMore.y < 50.0f && 50.0f < l.langMore.y + l.langMore.h);   // E31

    // 16:9 and 16:10 windows: the body does not move; the header follows the area's corners.                              // E31
    l = phoneOptionsLayout(OptionsAreaOf(ComputeFixedLayoutArea({1920u, 1080u}, open, Supersonic::SafeAreaInsets{}, 1.0f)), true, true);   // E31
    CHECK(SameRect(l.back, -149, 16, 123, 92) && SameRect(l.panel, 12, 110, 1000, 624) && SameRect(l.langLess, 832, 20, 86, 84));   // E31
    l = phoneOptionsLayout(OptionsAreaOf(ComputeFixedLayoutArea({1280u, 800u}, open, Supersonic::SafeAreaInsets{}, 1.0f)), true, true);   // E31
    CHECK(SameRect(l.back, -83, 16, 123, 92) && SameRect(l.panel, 12, 110, 1000, 624));   // E31

    // A notch and a home indicator: the frame is the safe area's (87 / 27 / 87 / 41), the rows 82 tall.                  // E31
    l = phoneOptionsLayout(OptionsAreaOf(ComputeFixedLayoutArea({2532u, 1170u}, open, Supersonic::SafeAreaInsets{132.0f, 0.0f, 132.0f, 63.0f}, 3.5f)), true, true);   // E31
    CHECK_EQ(l.hc, 82.0f);   // E31
    // The shown edge is -319.015 (486 px of bar / 1.5234375), so the arrow is at floor(-319.015 + 87 + 8) = -225   // E31
    // (a shown edge rounded to -319.0 would give -224).                                                           // E31
    CHECK_MSG(SameRect(l.back, -225, 35, 123, 92) && SameRect(l.panel, 12, 129, 1000, 588), "notch " + RectText(l.back) + " " + RectText(l.panel));   // E31
    CHECK(SameRect(l.cell[PC_PIXEL_SHADERS], 28, 145, 474, 82) && SameRect(l.cell[PC_PAUSE_FOCUS], 522, 233, 474, 82));   // E31
    CHECK(SameRect(l.cell[PC_REFRESH], 28, 501, 474, 112) && SameRect(l.cell[PC_MUSIC], 28, 619, 474, 82));   // E31
    CHECK(l.panel.y + l.panel.h <= 768.0f - 41.0f);   // the panel's bottom is above the home indicator   // E31

    // A bottom bar (a 150 px bar in a 1280x720 window is 160 logical px): the rows give way to 68, the panel's bottom runs   // E31
    // 6 px under the bar's top (documented, not fixed: the rows cannot be shorter).                                       // E31
    l = phoneOptionsLayout(OptionsAreaOf(ComputeFixedLayoutArea({1280u, 720u}, open, Supersonic::SafeAreaInsets{0.0f, 0.0f, 0.0f, 150.0f}, 1.0f)), true, true);   // E31
    CHECK_EQ(l.hc, 68.0f);   // E31
    CHECK_MSG(SameRect(l.panel, 12, 110, 1000, 504), "bottom bar " + RectText(l.panel));   // E31

    // The narrow case: a 4:3 window whose frame reaches 88 px in (a cut-out at each side): the body is what is left of   // E31
    // it, 848 wide, and the cells are 398 wide.                                                                          // E31
    l = phoneOptionsLayout(OptionsAreaOf(ComputeFixedLayoutArea({1024u, 768u}, 0.0f, Supersonic::SafeAreaInsets{88.0f, 0.0f, 88.0f, 24.0f}, 1.0f)), true, true);   // E31
    CHECK_MSG(SameRect(l.panel, 88, 110, 848, 624) && SameRect(l.back, 96, 16, 123, 92), "narrow " + RectText(l.panel));   // E31
    CHECK(SameRect(l.cell[PC_PIXEL_SHADERS], 104, 126, 398, 88) && SameRect(l.cell[PC_ADJUST], 522, 314, 398, 88));   // E31
    CHECK(SameRect(l.less[PC_REFRESH], 108, 536, 86, 88) && SameRect(l.value[PC_REFRESH], 194, 536, 218, 88));   // E31
    CHECK(SameRect(l.less[PC_EFFECTS], 676, 630, 86, 88) && SameRect(l.more[PC_EFFECTS], 830, 630, 86, 88));   // E31

    // No area published (the suites): the whole screen, no frame.                                                        // E31
    l = phoneOptionsLayout(OptionsArea{}, true, true);   // E31
    CHECK_MSG(SameRect(l.back, 8, 8, 123, 92) && SameRect(l.panel, 12, 102, 1000, 624), "no area " + RectText(l.panel));   // E31

    // Touch off drops the Adjust cell; no refresh row puts Zoom in the left column.                                       // E31
    l = phoneOptionsLayout(phone, false, true);   // E31
    CHECK(!l.present[PC_ADJUST] && l.present[PC_TOUCH] && l.present[PC_REFRESH] && l.present[PC_ZOOM]);   // E31
    l = phoneOptionsLayout(phone, true, false);   // E31
    CHECK(!l.present[PC_REFRESH] && l.present[PC_ZOOM] && l.present[PC_ADJUST]);   // E31
    CHECK(SameRect(l.cell[PC_ZOOM], 28, 525, 474, 118) && SameRect(l.less[PC_ZOOM], 32, 555, 86, 88));   // E31

    // Every shape: the body is inside 12..1012 (a rectangle that crossed x 0 or 1024 would be stretched out to the shown edge),    // E31
    // every cell inside the panel, every hit box inside the shown area and the frame (Back's reaches the corner on purpose), and   // E31
    // no two hit boxes overlap, so a press does one thing.                                                                 // E31
    struct Shape { glm::uvec2 window; float sides; Supersonic::SafeAreaInsets safe; float margin; };   // E31
    const Shape shapes[] = {{{2400u, 1080u}, open, {}, 3.5f}, {{1920u, 1080u}, open, {}, 1.0f}, {{1280u, 800u}, open, {}, 1.0f},   // E31
                            {{1024u, 768u}, 0.0f, {}, 1.0f}, {{2400u, 1080u}, 0.0f, {}, 3.5f}, {{2532u, 1170u}, open, {132.0f, 0.0f, 132.0f, 63.0f}, 3.5f},   // E31
                            {{1024u, 768u}, 0.0f, {88.0f, 0.0f, 88.0f, 24.0f}, 1.0f}, {{1280u, 720u}, open, {0.0f, 0.0f, 0.0f, 150.0f}, 1.0f},   // E31
                            {{2340u, 1080u}, open, {}, 3.5f}, {{3120u, 1440u}, open, {}, 8.0f}};   // E31
    for (const Shape& shape : shapes) {   // E31
        const Penumbra::Render::FixedLayoutArea fixed = ComputeFixedLayoutArea(shape.window, shape.sides, shape.safe, shape.margin);   // E31
        const OptionsArea area = OptionsAreaOf(fixed);   // E31
        for (const bool touch : {true, false}) {   // E31
            for (const bool refresh : {true, false}) {   // E31
                const std::string what = std::to_string(shape.window.x) + "x" + std::to_string(shape.window.y) + (touch ? " touch" : " no touch") + (refresh ? "" : " no refresh row");   // E31
                const PhoneOptionsLayout lay = phoneOptionsLayout(area, touch, refresh);   // E31
                CHECK_MSG(lay.panel.x >= 12.0f && lay.panel.x + lay.panel.w <= 1012.0f, what + " panel " + RectText(lay.panel));   // E31
                CHECK_MSG(lay.panel.y + lay.panel.h <= 768.0f, what + " panel bottom");   // E31
                const float left = fixed.shownMin.x + fixed.frame.left;   // E31
                const float right = fixed.shownMax.x - fixed.frame.right;   // E31
                const auto boxes = HitBoxes(lay);   // E31
                for (std::size_t i = 0; i < boxes.size(); ++i) {   // E31
                    const PhoneRect& r = boxes[i].second;   // E31
                    const bool isBack = boxes[i].first == "back";   // E31
                    CHECK_MSG(r.x >= (isBack ? std::floor(fixed.shownMin.x) - 1.0f : left - 1.0f) && r.x + r.w <= right + 1.0f &&   // E31
                                  r.y >= (isBack ? -1.0f : 0.0f) && r.y + r.h <= 768.0f,   // E31
                              what + " " + boxes[i].first + " " + RectText(r));   // E31
                    if (!isBack) {   // E31
                        CHECK_MSG(r.x >= lay.panel.x || boxes[i].first.rfind("language", 0) == 0, what + " " + boxes[i].first + " left of the panel");   // E31
                    }   // E31
                    for (std::size_t j = i + 1; j < boxes.size(); ++j) {   // E31
                        CHECK_MSG(!Overlap(r, boxes[j].second), what + ": " + boxes[i].first + " overlaps " + boxes[j].first);   // E31
                    }   // E31
                }   // E31
                for (int c = 0; c < PC_COUNT; ++c) {   // E31
                    if (!lay.present[c]) continue;   // E31
                    const PhoneRect& cell = lay.cell[c];   // E31
                    CHECK_MSG(cell.x >= lay.panel.x && cell.x + cell.w <= lay.panel.x + lay.panel.w &&   // E31
                                  cell.y >= lay.panel.y && cell.y + cell.h <= lay.panel.y + lay.panel.h,   // E31
                              what + " cell " + std::to_string(c) + " " + RectText(cell));   // E31
                }   // E31
            }   // E31
        }   // E31
    }   // E31
}   // E31

// E25's larger menu: where each window puts the 1024x768 menu - the logo and
// the seven buttons filling the height, from the left, the panel no narrower
// than in E1's view - and a 4:3 or narrower window left as it was.
View PhoneMenuView(glm::uvec2 window, const Penumbra::Render::MenuFrame& frame) {
    RenderSnapshot snapshot;
    snapshot.screenSize = vector2(1024.0f, 768.0f);
    snapshot.sideMargin = Penumbra::Render::kWideMenuMargin;
    return CameraRig::ComputeView(snapshot, window, true, &frame);
}

const glm::uvec2 kPhoneMenuWindows[] = {{2400u, 1080u}, {2340u, 1080u}, {2520u, 1080u}, {1920u, 1080u},
                                        {2560u, 1600u}, {1920u, 1200u}, {2360u, 1640u}};

void testPhoneMenuFrame() {
    using namespace Penumbra::Render;
    for (const glm::uvec2 window : kPhoneMenuWindows) {
        const std::string what = std::to_string(window.x) + "x" + std::to_string(window.y);
        const glm::vec2 image(window);
        const MenuFrame frame = ComputeMenuFrame(window);
        const float base = std::min(image.x / 1024.0f, image.y / 768.0f);
        CHECK_MSG(frame.active, what);
        CHECK_MSG(Near(frame.baseScale, base, 1e-5f), what);
        CHECK_MSG(frame.scale >= base * kMenuMinGain - 1e-5f, what);
        // The focus box whole in the window, at its left; the screen's top and
        // bottom edges never inside it (nothing is collected past them).
        const glm::vec2 focusMin = frame.viewportMin + kMenuFocusMin * frame.scale;
        const glm::vec2 focusMax = frame.viewportMin + kMenuFocusMax * frame.scale;
        CHECK_MSG(focusMin.x >= -0.5f && focusMin.x <= 0.5f && focusMin.y >= -0.5f, what);
        CHECK_MSG(focusMax.y <= image.y + 0.5f && focusMax.x < image.x, what);
        CHECK_MSG(frame.viewportMin.y <= 0.0f && frame.viewportMin.y + 768.0f * frame.scale >= image.y - 0.5f, what);
        // The panel, from x 632.8 to the window's edge, no narrower than E1's.
        const float baseLeft = std::round((image.x - 1024.0f * base) * 0.5f);
        const float basePanel = image.x - (baseLeft + kMenuPanelLeft * base);
        const float panel = image.x - (frame.viewportMin.x + kMenuPanelLeft * frame.scale);
        std::printf("  E25 menu %s: x%.3f (E1's x%.3f, %.0f%% larger), panel %.0f px (E1's %.0f)\n", what.c_str(),
                    frame.scale, base, (frame.scale / base - 1.0f) * 100.0f, panel, basePanel);
        CHECK_MSG(panel >= basePanel - 1.0f, what);

        // The view it makes: that scale and place, the whole window shown (no
        // bars), what is shown of the screen from the crop.
        const View view = PhoneMenuView(window, frame);
        CHECK_MSG(view.scale == frame.scale && view.viewportMin == frame.viewportMin, what);
        CHECK_MSG(CameraRig::Bars(view).empty(), what);
        CHECK_MSG(view.ShownMin() == glm::vec2(0.0f) && view.ShownMax() == image, what);
        CHECK_MSG(Near(view.ShownLogicalMin().x, -frame.viewportMin.x / frame.scale), what);
        CHECK_MSG(Near(view.ShownLogicalMin().y, std::max(0.0f, -frame.viewportMin.y / frame.scale)), what);
        CHECK_MSG(Near(view.ShownLogicalMax().x, (image.x - frame.viewportMin.x) / frame.scale), what);
        CHECK_MSG(Near(view.ShownLogicalMax().y, std::min(768.0f, (image.y - frame.viewportMin.y) / frame.scale)), what);
        CHECK_MSG(view.CroppedBottom(), what);

        // The panel's text box: 10 px in from the panel's left, 20 down from
        // what is shown of its top, 10 short of the window's right and bottom.
        const MenuPanel box = ComputeMenuPanel(frame, window);
        CHECK_MSG(Near(box.min.x, kMenuPanelLeft + 10.0f), what);
        CHECK_MSG(Near(box.min.y, view.ShownLogicalMin().y + 20.0f), what);
        CHECK_MSG(Near(box.max.x, view.ShownLogicalMax().x - 10.0f), what);
        CHECK_MSG(Near(box.max.y, view.ShownLogicalMax().y - 10.0f), what);
        CHECK_MSG(Near(box.shownMin.x, view.ShownLogicalMin().x) && Near(box.shownMin.y, view.ShownLogicalMin().y), what);
        CHECK_MSG(Near(box.shownMax.x, view.ShownLogicalMax().x) && Near(box.shownMax.y, view.ShownLogicalMax().y), what);
        CHECK_MSG(Near(box.minScale * frame.scale, base, 1e-4f), what);   // E1's size on the window, at least
        CHECK_MSG(Near(box.maxScale, box.minScale * kMenuMaxTextGain, 1e-4f), what);
        // A notch at the right and a corner button keep the text out of them.
        const Supersonic::SafeAreaInsets rightNotch{0.0f, 0.0f, 90.0f, 0.0f};
        const MenuPanel inset = ComputeMenuPanel(frame, window, rightNotch, box.max.x - 40.0f);
        CHECK_MSG(Near(inset.max.x, box.max.x - 50.0f), what);
        const MenuPanel notch = ComputeMenuPanel(frame, window, rightNotch);
        CHECK_MSG(Near(notch.max.x, box.max.x - 90.0f / frame.scale), what);
    }
    // Without a safe area the frame is E25's exactly (one clamp, the screen's
    // edges kept out of the image), whichever bounds the scale - the fill (a
    // phone) or the panel (a tablet).
    for (const glm::uvec2 window : kPhoneMenuWindows) {
        const glm::vec2 image(window);
        const MenuFrame frame = ComputeMenuFrame(window, Supersonic::SafeAreaInsets{});
        const float s = frame.scale;
        const float centred = (image.y - (kMenuFocusMax.y - kMenuFocusMin.y) * s) * 0.5f - kMenuFocusMin.y * s;
        const glm::vec2 before =
            glm::round(glm::vec2(-kMenuFocusMin.x * s, std::clamp(centred, std::min(0.0f, image.y - 768.0f * s), 0.0f)));
        CHECK_MSG(frame.viewportMin == before,
                  std::to_string(window.x) + "x" + std::to_string(window.y) + ": " +
                      std::to_string(frame.viewportMin.y) + " against " + std::to_string(before.y));
    }
    // A notch at the left: the focus box starts past it.
    const MenuFrame notched = ComputeMenuFrame({2400u, 1080u}, Supersonic::SafeAreaInsets{100.0f, 0.0f, 0.0f, 0.0f});
    CHECK(notched.active);
    CHECK(Near(notched.viewportMin.x + kMenuFocusMin.x * notched.scale, 100.0f, 0.5f));
    // A notched phone with its home indicator (an iPhone on its side): the
    // logo and the Quit button's foot inside the safe area, the notch to the
    // left of the focus box, the panel's box and its loading corner above the
    // indicator; a camera at the top, likewise.
    for (const Supersonic::SafeAreaInsets safe : {Supersonic::SafeAreaInsets{132.0f, 0.0f, 0.0f, 63.0f},
                                                   Supersonic::SafeAreaInsets{0.0f, 80.0f, 0.0f, 0.0f},
                                                   Supersonic::SafeAreaInsets{132.0f, 40.0f, 132.0f, 63.0f}}) {
        const glm::uvec2 window(2532u, 1170u);
        const std::string what = "2532x1170 safe " + std::to_string(safe.left) + "," + std::to_string(safe.top) +
                                 "," + std::to_string(safe.right) + "," + std::to_string(safe.bottom);
        const MenuFrame frame = ComputeMenuFrame(window, safe);
        CHECK_MSG(frame.active, what);
        const glm::vec2 focusMin = frame.viewportMin + kMenuFocusMin * frame.scale;
        const glm::vec2 focusMax = frame.viewportMin + kMenuFocusMax * frame.scale;
        const float quitFoot = frame.viewportMin.y + 583.5f * frame.scale;
        std::printf("  E25 menu %s: x%.3f, the focus box y %.0f-%.0f, the Quit button's foot at %.0f\n", what.c_str(),
                    frame.scale, focusMin.y, focusMax.y, quitFoot);
        CHECK_MSG(focusMin.x >= safe.left - 0.5f, what);
        CHECK_MSG(focusMin.y >= safe.top - 0.5f, what);
        CHECK_MSG(focusMax.y <= 1170.0f - safe.bottom + 0.5f, what);
        CHECK_MSG(quitFoot <= 1170.0f - safe.bottom, what);
        const MenuPanel box = ComputeMenuPanel(frame, window, safe);
        CHECK_MSG(frame.viewportMin.y + box.min.y * frame.scale >= safe.top - 0.5f, what);
        CHECK_MSG(frame.viewportMin.y + box.max.y * frame.scale <= 1170.0f - safe.bottom - 0.5f, what);
        CHECK_MSG(frame.viewportMin.y + box.shownMax.y * frame.scale <= 1170.0f - safe.bottom + 0.5f, what);
        CHECK_MSG(frame.viewportMin.y + box.shownMin.y * frame.scale >= safe.top - 0.5f, what);
        CHECK_MSG(frame.viewportMin.x + box.max.x * frame.scale <= 2532.0f - safe.right - 0.5f, what);
    }

    // No wider than 4:3, or a 4:3 tablet whose panel would narrow: E1's view.
    for (const glm::uvec2 window : {glm::uvec2(1024u, 768u), glm::uvec2(2048u, 1536u), glm::uvec2(1280u, 1024u),
                                    glm::uvec2(2732u, 2048u), glm::uvec2(0u, 0u)}) {
        const MenuFrame frame = ComputeMenuFrame(window);
        CHECK_MSG(!frame.active, std::to_string(window.x) + "x" + std::to_string(window.y));
        if (window.x == 0) continue;
        const View view = PhoneMenuView(window, frame);
        const View before = MenuView(window, Penumbra::Render::kWideMenuMargin);
        CHECK(view.scale == before.scale && view.viewportMin == before.viewportMin &&
              view.viewportMax == before.viewportMax);
    }
    // E1's views are never cropped: their shown area is what it always was.
    for (const glm::uvec2 window : kWideWindows) {
        const View open = MenuView(window, Penumbra::Render::kWideMenuMargin);
        CHECK(!open.CroppedTop() && !open.CroppedBottom());
        CHECK(open.ShownLogicalMin().y == 0.0f && open.ShownLogicalMax().y == 768.0f);
    }
    CHECK(IsPhoneMenuScene("scenes/menu.esc") && IsPhoneMenuScene("scenes/arena_select.esc"));
    CHECK(!IsPhoneMenuScene("scenes/videoModes.esc") && !IsPhoneMenuScene("scenes/gameover.esc") &&
          !IsPhoneMenuScene("scenes/level1.esc"));
}

// A tap or a click where a menu button is drawn in E25's larger menu hits it:
// every button whole on the window, and the window pixel of its middle and of
// two points just inside its drawn part (left of the panel, which covers the
// rest, as it does in the original) coming back through the mouse's mapping
// and through a finger's (the layer's TouchContacts, then E16's pointer)
// inside its collision box.
void testPhoneMenuHits() {
    using namespace Penumbra::Render;
    for (const glm::uvec2 window : kPhoneMenuWindows) {
        const MenuFrame frame = ComputeMenuFrame(window);
        const View view = PhoneMenuView(window, frame);
        const glm::vec2 image(window);
        for (const MenuButton& button : kMenuButtons) {
            const std::string what = std::string(button.name) + " at " + std::to_string(window.x) + "x" +
                                     std::to_string(window.y);
            const glm::vec2 drawnMax(std::min(button.max.x, kMenuPanelLeft - 1.0f), button.max.y);
            const glm::vec2 topLeft = view.viewportMin + button.min * view.scale;
            const glm::vec2 bottomRight = view.viewportMin + button.max * view.scale;
            CHECK_MSG(topLeft.x >= 0.0f && topLeft.y >= 0.0f && bottomRight.y <= image.y, what);
            const glm::vec2 inset(2.0f / view.scale);
            for (const glm::vec2 logical : {(button.min + drawnMax) * 0.5f, button.min + inset, drawnMax - inset}) {
                const glm::vec2 pixel = view.viewportMin + logical * view.scale;
                const auto inside = [&](const glm::vec2& p) {
                    return p.x > button.min.x && p.x < button.max.x && p.y > button.min.y && p.y < button.max.y;
                };
                // The mouse.
                InputMapper mapper;
                mapper.SetControls(Defaults());
                RawDevices raw;
                raw.mouseWindow = pixel;
                raw.mouse[0] = true;
                const InputFrame clicked = mapper.BuildTick(raw, view);
                CHECK_MSG(inside(clicked.cursor) && clicked.keys[K_LMOUSE], what);
                CHECK_MSG(!InputMapper::PointerOverBars(pixel, view), what);
                // A finger: the menu's pointer, where the layer maps a contact.
                TouchControls touch;
                TouchInput input;
                input.contacts = {TouchContact{0, InputMapper::WindowToLogical(pixel, view), true}};
                input.screen = view.logicalScreen;
                input.areaMin = view.ShownLogicalMin();
                input.areaMax = view.ShownLogicalMax();
                input.scene = TouchScene::Menu;
                input.corner = TouchCorner::Hidden;
                input.unit = frame.baseScale / frame.scale;
                InputFrame tapped;
                TouchControls::ApplyToFrame(touch.Update(input), tapped);
                CHECK_MSG(inside(tapped.cursor) && tapped.keys[K_LMOUSE], what);
            }
        }
    }
}

// E44: the combo keys that land on one tick are handed over one tick apart (InputMapper::serialiseComboPresses).
void testComboAssist() {
    View view;
    ControlSettings assisted = Defaults();
    CHECK_MSG(assisted.comboAssist, "on by default");
    ControlSettings original = assisted;
    original.comboAssist = false;

    // Ticks of a mapper, one frame each: the keys down at each, as the game saw them.
    const auto run = [&view](const ControlSettings& controls, const std::vector<RawDevices>& ticks) {
        InputMapper mapper;
        mapper.SetControls(controls);
        mapper.SetPlayer2Pad(0);
        std::vector<InputFrame> frames;
        for (const RawDevices& raw : ticks) {
            frames.push_back(mapper.BuildTick(raw, view));
            mapper.EndFrame(raw);
        }
        return frames;
    };
    const RawDevices none;
    const RawDevices leftS = With({GLFW_KEY_LEFT, GLFW_KEY_S});

    // Off, the original's: both keys on the same tick, and the recorder keeps the first.
    {
        const auto frames = run(original, {leftS, leftS});
        CHECK(frames[0].keys[K_LEFT] && frames[0].keys[K_S]);
    }
    // On: left now, S on the next tick (and still held after).
    {
        const auto frames = run(assisted, {leftS, leftS, leftS});
        CHECK_MSG(frames[0].keys[K_LEFT] && !frames[0].keys[K_S], "the first in combo order is seen now");
        CHECK_MSG(frames[1].keys[K_LEFT] && frames[1].keys[K_S], "and the other one tick after it");
        CHECK(frames[2].keys[K_LEFT] && frames[2].keys[K_S]);
    }
    // Down with a side key: down first, whichever side, however they arrive.
    for (const int side : {GLFW_KEY_LEFT, GLFW_KEY_RIGHT}) {
        const int sideKey = side == GLFW_KEY_LEFT ? K_LEFT : K_RIGHT;
        const auto frames = run(assisted, {With({side, GLFW_KEY_DOWN}), With({side, GLFW_KEY_DOWN})});
        CHECK_MSG(frames[0].keys[K_DOWN] && !frames[0].keys[sideKey], "down comes before a side");
        CHECK(frames[1].keys[K_DOWN] && frames[1].keys[sideKey]);
    }
    // All three of the blast's keys at once: down, a side, then D, a tick each.
    {
        const RawDevices all = With({GLFW_KEY_RIGHT, GLFW_KEY_DOWN, GLFW_KEY_D});
        const auto frames = run(assisted, {all, all, all, all});
        CHECK(frames[0].keys[K_DOWN] && !frames[0].keys[K_RIGHT] && !frames[0].keys[K_D]);
        CHECK(frames[1].keys[K_DOWN] && frames[1].keys[K_RIGHT] && !frames[1].keys[K_D]);
        CHECK(frames[2].keys[K_DOWN] && frames[2].keys[K_RIGHT] && frames[2].keys[K_D]);
    }
    // A key that comes up before its turn is still seen, for one tick, and no longer.
    {
        const auto frames = run(assisted, {leftS, With({GLFW_KEY_LEFT}), With({GLFW_KEY_LEFT}), none});
        CHECK(frames[0].keys[K_LEFT] && !frames[0].keys[K_S]);
        CHECK_MSG(frames[1].keys[K_S], "a tap that lost its tick to another key is seen on the next");
        CHECK(!frames[2].keys[K_S]);
        CHECK(!frames[3].keys[K_LEFT]);
    }
    // Nothing waits when nothing collides: a lone key, a held key with another pressed later, and the jump.
    {
        const auto lone = run(assisted, {With({GLFW_KEY_S})});
        CHECK_MSG(lone[0].keys[K_S], "a lone press is not delayed");
        const auto later = run(assisted, {With({GLFW_KEY_LEFT}), leftS});
        CHECK_MSG(later[1].keys[K_LEFT] && later[1].keys[K_S], "a key pressed while another is held is not delayed");
        const auto jump = run(assisted, {With({GLFW_KEY_LEFT, GLFW_KEY_UP})});
        CHECK_MSG(jump[0].keys[K_LEFT] && jump[0].keys[K_UP], "the jump never waits");
    }
    // Losing the window's focus drops what was waiting.
    {
        RawDevices away = leftS;
        away.focused = false;
        const auto frames = run(assisted, {leftS, away, With({GLFW_KEY_LEFT})});
        CHECK(frames[0].keys[K_LEFT] && !frames[0].keys[K_S]);
        CHECK_MSG(!frames[1].hasFocus, "the game reads every key as up while the window is unfocused");
        CHECK_MSG(frames[2].keys[K_LEFT] && !frames[2].keys[K_S], "a press held back before the focus went is not seen after it");
    }
    // A frame that ran no tick latches its press, and the latch is serialised too.
    {
        InputMapper mapper;
        mapper.SetControls(assisted);
        mapper.SetPlayer2Pad(0);
        InputFrame frame = mapper.BuildTick(none, view);
        mapper.EndFrame(none);    // the frame that ran that tick
        mapper.EndFrame(leftS);   // a frame with no tick of its own: both presses are latched for the next one
        frame = mapper.BuildTick(none, view);
        CHECK(frame.keys[K_LEFT] && !frame.keys[K_S]);
        mapper.EndFrame(none);
        frame = mapper.BuildTick(none, view);
        CHECK_MSG(frame.keys[K_S], "and the second is seen on the tick after");
    }
    // The setting is saved as controls.comboAssist and reads back; an old file without it keeps the default.
    {
        Settings settings = Settings::Defaults("en");
        settings.controls.comboAssist = false;
        std::string warning;
        const Settings loaded = Settings::FromJson(settings.ToJson(), Settings::Defaults("en"), &warning);
        CHECK(!loaded.controls.comboAssist && warning.empty());
        const Settings old = Settings::FromJson(R"({"controls": {"keyboardPlayer2": false}})", Settings::Defaults("en"), &warning);
        CHECK_MSG(old.controls.comboAssist, "a file written before E44 keeps the default");
    }
}

void runTests() {
    testKeyboardPlayer1();
    testGamepads();
    testLonePadIsPlayer1();
    testTouchIsPlayer1();
    testSticks();
    testKeyboardPlayer2();
    testCursorAndText();
    testPointerOverBars();
    testWideMenuView();
    testWideMenuHits();
    testWideMenuWarp();
    testPhoneZoom();        // E25
    testPhoneMenuFrame();   // E25
    testPhoneMenuHits();    // E25
    testPhoneHudFrame();    // E26
    testFixedLayoutArea();  // E31
    testPhoneOptionsLayout();   // E31
    testLatch();
    testComboAssist();   // E44
    testMenuMode();
    testKeyNames();
    testSettings();
    testSettingsTouchTuning();   // E28
    testWindowActions();
    testAutomaticDisplayMode();
    testSystemLocale();
    testAudioWithoutEngine();
}

} // namespace

TEST_MAIN("test_pn_render_input", 100)
