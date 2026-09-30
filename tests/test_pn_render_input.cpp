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
#include "render/Settings.hpp"
#include "render/WideMenus.hpp"
#include "render/WindowMode.hpp"

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
    CHECK_EQ(en.fullscreenWidth, 0);         // Step 23: the desktop's mode
    CHECK_EQ(en.fullscreenHeight, 0);
    // E23: a first launch is fullscreen, automatic in all three.
    CHECK(en.fullscreen);
    CHECK_EQ(en.fullscreenRefresh, 0);
    CHECK_EQ(en.windowWidth, 0);
    CHECK_EQ(en.windowHeight, 0);
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
    CHECK(partial.musicVolume == 1.0f);
    CHECK(partial.effectsVolume == en.effectsVolume);
    CHECK_EQ(partial.windowWidth, 640);
    CHECK_EQ(partial.windowHeight, 480);
    CHECK(partial.controls.player1[ControlAction::Sword] == std::vector<int>{GLFW_KEY_A});
    CHECK(partial.controls.player1[ControlAction::Fire] == en.controls.player1[ControlAction::Fire]);
    CHECK(partial.controls.player2 == en.controls.player2);
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
    CHECK(Settings::FromJson(en.ToJson(), Settings::Defaults("pt")) == en);

    // E24: every language the game speaks is saved and read back; a file from
    // before E24 ("pt", "en") reads as it did; anything else keeps the default.
    for (const Penumbra::Render::LanguageInfo& info : Penumbra::Render::kLanguages) {
        Settings speaking = en;
        speaking.language = info.id;
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
        CHECK_MSG(Settings::FromJson(json, pt, &warning).language == "pt", unknown);
        CHECK_MSG(!warning.empty(), unknown);
    }

    // Nowhere to save.
    CHECK(!en.Save(fs::path(), &error));
    CHECK(!error.empty());

    fs::remove_all(dir, ec);
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
void testWideMenuHits() {
    struct Button {
        const char* name;
        glm::vec2 min;
        glm::vec2 max;
    };
    const Button buttons[] = {
        {"novo_jogo", {231.5f, 200.5f}, {600.5f, 225.5f}},       {"versus", {281.5f, 258.5f}, {650.5f, 283.5f}},
        {"como_jogar", {196.5f, 314.5f}, {565.5f, 339.5f}},      {"melhores_tempos", {182.5f, 376.5f}, {551.5f, 401.5f}},
        {"opcoes_de_video", {229.5f, 433.5f}, {598.5f, 458.5f}}, {"creditos", {332.5f, 494.5f}, {701.5f, 519.5f}},
        {"sair", {449.5f, 558.5f}, {818.5f, 583.5f}},
    };
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
    testLatch();
    testMenuMode();
    testKeyNames();
    testSettings();
    testWindowActions();
    testAutomaticDisplayMode();
    testSystemLocale();
    testAudioWithoutEngine();
}

} // namespace

TEST_MAIN("test_pn_render_input", 100)
