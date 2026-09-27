// The platform side of the port: keys, pads and the keyboard second player
// mapped onto the Eth InputFrame the scripts read, the latch across frames that
// run no tick, the settings file, and the audio device with no engine behind it.
// Pure: no window, no GLFW calls, no audio device, no original files.

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <GLFW/glfw3.h>

#include "TestHarness.hpp"

#include "render/AudioOutEngine.hpp"
#include "render/InputMapper.hpp"
#include "render/Settings.hpp"

namespace {

using namespace Penumbra::Eth;
using Penumbra::Render::AudioOutEngine;
using Penumbra::Render::ControlAction;
using Penumbra::Render::ControlSettings;
using Penumbra::Render::InputMapper;
using Penumbra::Render::KeyFromName;
using Penumbra::Render::KeyName;
using Penumbra::Render::RawDevices;
using Penumbra::Render::RawPad;
using Penumbra::Render::Settings;
using Penumbra::Render::View;
namespace Pad = Supersonic::Pad;
namespace fs = std::filesystem;

ControlSettings Defaults() { return Settings::Defaults(false).controls; }

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

void testSettings() {
    const Settings pt = Settings::Defaults(true);
    const Settings en = Settings::Defaults(false);
    CHECK(pt.language == "pt");
    CHECK(en.language == "en");
    CHECK(en.controls.keyboardPlayer2);
    CHECK(en.controls.firstPadIsPlayer1);    // E12
    CHECK_EQ(en.controls.joystickLayout, 0);
    CHECK_EQ(en.controls.Player2Pad(), 0);   // g_controls 0: player 2 reads joystick 0
    CHECK(en.pixelShaders);
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
    changed.widescreen = false;
    changed.musicVolume = 0.35f;
    changed.effectsVolume = 0.8f;
    changed.pixelShaders = false;
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
        R"({"language": "PT", "volume": {"music": 2.5}, "window": {"width": 10},
            "controls": {"player1": {"sword": ["A", "bogus"]}}})",
        en, &warning);
    CHECK(partial.language == "pt");
    CHECK(partial.musicVolume == 1.0f);
    CHECK(partial.effectsVolume == en.effectsVolume);
    CHECK_EQ(partial.windowWidth, 640);
    CHECK_EQ(partial.windowHeight, en.windowHeight);
    CHECK(partial.controls.player1[ControlAction::Sword] == std::vector<int>{GLFW_KEY_A});
    CHECK(partial.controls.player1[ControlAction::Fire] == en.controls.player1[ControlAction::Fire]);
    CHECK(partial.controls.player2 == en.controls.player2);
    CHECK(!warning.empty());   // "bogus"
    CHECK(Settings::FromJson("[1, 2]", en) == en);

    // Nowhere to save.
    CHECK(!en.Save(fs::path(), &error));
    CHECK(!error.empty());

    fs::remove_all(dir, ec);
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

void runTests() {
    testKeyboardPlayer1();
    testGamepads();
    testLonePadIsPlayer1();
    testSticks();
    testKeyboardPlayer2();
    testCursorAndText();
    testLatch();
    testKeyNames();
    testSettings();
    testAudioWithoutEngine();
}

} // namespace

TEST_MAIN("test_pn_render_input", 100)
