// E13, the pause (render/PauseMenu), and what it needs around it: where and on
// what it opens (play only; Esc and player 1's Back, never Start), the freeze
// (the layer is told not to tick), the menu's navigation, Resume, Main menu's
// one tick of the original's cancel, the pause on focus loss, the overlay's
// HudCmds and HudRenderer's extra commands, the filter that keeps the pause's
// presses out of the game, the English, and settings.pauseOnFocusLoss.
// Pure: no window, no Machine, no original files.
// E28 adds the touch controls' editor (render/TouchEditor): its layout, the finger state machine (taps on release,   // E28
// dead fingers, locked drags, the size step's re-lock, restore, close, a dropped control that would be buried under a tile going back), the overlay, its art, and its   // E28
// words through HudRenderer in every language; HoldPressed. Those that use the art, the strings or the fonts need the   // E28
// data folder.   // E28
// E35 adds the Supersonic Engine's intro (render/Splash): its timeline and skip, what counts as a press, where   // E35
// the logo goes, its quads and file, and which starts play it - pure, the file needing the data folder.   // E35

#include <algorithm>
#include <cmath>   // E28
#include <cstdio>
#include <cstdlib>   // E35
#include <filesystem>   // E28
#include <fstream>   // E28
#include <string>
#include <string_view>   // E35
#include <system_error>   // E28
#include <vector>

#include <entt/entt.hpp>

#include "TestHarness.hpp"

#include "eth/Input.hpp"
#include "eth/Snapshot.hpp"
#include "eth/Text.hpp"
#include "render/ArabicShaping.hpp"
#include "render/FontAtlas.hpp"
#include "render/HudRenderer.hpp"
#include "render/Localization.hpp"
#include "render/PauseMenu.hpp"
#include "render/Settings.hpp"
#include "render/Splash.hpp"   // E35
#include "render/TextureCache.hpp"
#include "render/TextureDecode.hpp"   // E28
#include "render/TouchControls.hpp"   // E28
#include "render/TouchEditor.hpp"   // E28
#include "render/View.hpp"

namespace {

using namespace Penumbra::Eth;
using Penumbra::Render::HudRenderer;
using Penumbra::Render::Language;
using Penumbra::Render::Localization;
using Penumbra::Render::PauseInput;
using Penumbra::Render::PauseMenu;
using Penumbra::Render::PauseStep;
using Penumbra::Render::Settings;
using Penumbra::Render::View;

constexpr int kResume = PauseMenu::kResume;
constexpr int kMainMenu = PauseMenu::kMainMenu;

PauseInput Play() {
    PauseInput input;
    input.inPlayScene = true;
    return input;
}

// A press: held for one tick, then released. Returns the press's step.
PauseStep Tap(PauseMenu& pause, PauseInput input, bool PauseInput::*button) {
    input.*button = true;
    const PauseStep step = pause.Update(input);
    input.*button = false;
    pause.Update(input);
    return step;
}

// A menu that has seen a tick of play and is then paused by Esc.
PauseMenu Paused(const PauseInput& play = Play()) {
    PauseMenu pause;
    pause.Update(play);
    Tap(pause, play, &PauseInput::open);
    return pause;
}

std::uint8_t Alpha(uint argb) { return static_cast<std::uint8_t>(argb >> 24); }

void testOpensOnlyInPlay() {
    // Not in play (a menu, the options, game over): Esc is the scripts'.
    PauseMenu menu;
    PauseInput notPlay;
    menu.Update(notPlay);
    PauseStep step = Tap(menu, notPlay, &PauseInput::open);
    CHECK(!menu.Paused());
    CHECK(step.tick);
    CHECK(!step.opened);

    // In play the press opens it, and the tick of the press is not run: the
    // Esc that opened it never reaches escToGoToMenu.
    PauseMenu pause;
    PauseInput play = Play();
    pause.Update(play);
    play.open = true;
    step = pause.Update(play);
    CHECK(pause.Paused());
    CHECK(step.opened);
    CHECK(!step.tick);
    CHECK(!step.sendCancel);
    CHECK_EQ(pause.Selected(), kResume);

    // The freeze: held or released, nothing ticks while it is open.
    int ticked = 0;
    int opened = 0;
    for (int i = 0; i < 120; ++i) {
        play.open = i < 30;   // Esc held, then let go
        const PauseStep frozen = pause.Update(play);
        ticked += frozen.tick ? 1 : 0;
        opened += frozen.opened ? 1 : 0;
    }
    CHECK_EQ(ticked, 0);
    CHECK_EQ(opened, 0);
    CHECK(pause.Paused());

    // A held key is not a press: Esc held into play from a menu opens nothing,
    // and neither does a key held on the very first tick.
    PauseMenu held;
    PauseInput escHeld;
    escHeld.open = true;
    held.Update(escHeld);
    escHeld.inPlayScene = true;
    CHECK(held.Update(escHeld).tick);
    CHECK(!held.Paused());
    PauseMenu first;
    PauseInput firstTick = Play();
    firstTick.open = true;
    CHECK(first.Update(firstTick).tick);
    CHECK(!first.Paused());
}

// What opens it, from a mapped frame: K_ESC and player 1's Back, never Start
// (player 2's summon) and never player 2's pad.
void testInputFrom() {
    const glm::vec2 screen(1366.0f, 768.0f);
    const int player1Pad = 1;   // getPlayerJoystick(0) under the default g_controls
    InputFrame base;
    base.pads[0].connected = true;
    base.pads[1].connected = true;

    const auto from = [&](const InputFrame& frame) { return PauseMenu::InputFrom(frame, player1Pad, true, screen); };

    InputFrame esc = base;
    esc.keys[K_ESC] = true;
    CHECK(from(esc).open && from(esc).back);
    CHECK(!from(esc).confirm);

    InputFrame back1 = base;
    back1.pads[1].buttons[JK_09] = true;
    CHECK(from(back1).open && from(back1).back);
    InputFrame back2 = base;
    back2.pads[0].buttons[JK_09] = true;
    CHECK(!from(back2).open && !from(back2).back);

    InputFrame start1 = base;
    start1.pads[1].buttons[JK_10] = true;
    CHECK(!from(start1).open);
    CHECK(from(start1).confirm);   // in the menu Start confirms
    InputFrame start2 = base;
    start2.pads[0].buttons[JK_10] = true;
    CHECK(!from(start2).open && !from(start2).confirm);

    InputFrame enter = base;
    enter.keys[K_ENTER] = true;
    CHECK(from(enter).confirm && !from(enter).open);
    enter.keys[K_ALT] = true;   // Alt+Enter is the window's
    CHECK(!from(enter).confirm);

    InputFrame a = base;
    a.pads[1].buttons[JK_03] = true;
    CHECK(from(a).confirm && !from(a).open && !from(a).back);
    InputFrame b = base;
    b.pads[1].buttons[JK_02] = true;
    CHECK(from(b).back && !from(b).open && !from(b).confirm);

    InputFrame stick = base;
    stick.pads[1].xy = glm::vec2(0.0f, -1.0f);   // y down: up
    CHECK(from(stick).up && !from(stick).down);
    stick.pads[1].xy = glm::vec2(0.0f, 0.8f);
    CHECK(from(stick).down && !from(stick).up);
    stick.pads[1].xy = glm::vec2(0.9f, 0.3f);    // leaning sideways
    CHECK(!from(stick).down && !from(stick).up);
    stick.pads[1].xy = glm::vec2(0.0f);
    stick.pads[0].xy = glm::vec2(0.0f, 1.0f);    // player 2's stick
    CHECK(!from(stick).down);

    InputFrame keys = base;
    keys.keys[K_UP] = true;
    CHECK(from(keys).up);
    keys.keys[K_UP] = false;
    keys.keys[K_DOWN] = true;
    CHECK(from(keys).down);

    InputFrame mouse = base;
    mouse.cursor = glm::vec2(700.0f, 400.0f);
    mouse.keys[K_LMOUSE] = true;
    mouse.hasFocus = false;
    const PauseInput pointer = from(mouse);
    CHECK(pointer.click);
    CHECK(pointer.pointer == glm::vec2(700.0f, 400.0f));
    CHECK(!pointer.focused);
    CHECK(pointer.inPlayScene);
    CHECK(pointer.screen == screen);

    // A pad that is not connected says nothing; an index out of range is safe.
    InputFrame gone = back1;
    gone.pads[1].connected = false;
    CHECK(!from(gone).open);
    CHECK(!PauseMenu::InputFrom(esc, 9, true, screen).confirm);
    CHECK(PauseMenu::InputFrom(esc, -1, true, screen).open);

    // End to end: Start on player 1's pad in play never pauses; Back does.
    PauseMenu pause;
    pause.Update(from(base));
    pause.Update(from(start1));
    pause.Update(from(base));
    CHECK(!pause.Paused());
    pause.Update(from(back2));
    CHECK(!pause.Paused());
    pause.Update(from(back1));
    CHECK(pause.Paused());
}

void testNavigation() {
    PauseMenu pause = Paused();
    const PauseInput play = Play();
    CHECK_EQ(pause.Selected(), kResume);
    CHECK(!Tap(pause, play, &PauseInput::down).tick);
    CHECK_EQ(pause.Selected(), kMainMenu);
    Tap(pause, play, &PauseInput::down);           // wraps
    CHECK_EQ(pause.Selected(), kResume);
    Tap(pause, play, &PauseInput::up);             // wraps the other way
    CHECK_EQ(pause.Selected(), kMainMenu);
    Tap(pause, play, &PauseInput::up);
    CHECK_EQ(pause.Selected(), kResume);

    // Held, it moves once.
    PauseInput held = play;
    held.down = true;
    for (int i = 0; i < 20; ++i) pause.Update(held);
    CHECK_EQ(pause.Selected(), kMainMenu);
    pause.Update(play);
    CHECK(pause.Paused());

    // Every opening starts on Resume.
    Tap(pause, play, &PauseInput::back);
    CHECK(!pause.Paused());
    Tap(pause, play, &PauseInput::open);
    CHECK(pause.Paused());
    CHECK_EQ(pause.Selected(), kResume);
}

void testResume() {
    const PauseInput play = Play();

    // Confirm on Resume.
    PauseMenu pause = Paused();
    PauseStep step = Tap(pause, play, &PauseInput::confirm);
    CHECK(!pause.Paused());
    CHECK(step.tick);
    CHECK(step.closed);
    CHECK(!step.sendCancel);
    CHECK(pause.Update(play).tick);

    // Back (Esc, B, Back) resumes.
    pause = Paused();
    step = Tap(pause, play, &PauseInput::back);
    CHECK(!pause.Paused());
    CHECK(step.tick && step.closed && !step.sendCancel);

    // Esc opens it and Esc closes it - with a release between: the press that
    // opened it, still held, closes nothing.
    pause = PauseMenu();
    pause.Update(play);
    PauseInput escDown = play;
    escDown.open = true;
    escDown.back = true;
    pause.Update(escDown);
    CHECK(pause.Paused());
    for (int i = 0; i < 10; ++i) pause.Update(escDown);
    CHECK(pause.Paused());
    pause.Update(play);
    CHECK(pause.Paused());
    step = pause.Update(escDown);
    CHECK(!pause.Paused());
    CHECK(step.closed && step.tick);
    CHECK(!step.opened);
    // Still held on the next tick: it does not open again.
    CHECK(pause.Update(escDown).tick);
    CHECK(!pause.Paused());

    // Leaving play under it (nothing the layer does, but never stuck frozen).
    pause = Paused();
    PauseInput notPlay;
    step = pause.Update(notPlay);
    CHECK(!pause.Paused());
    CHECK(step.tick && step.closed);

    // The music: ducked while open, whole again after.
    pause = PauseMenu();
    CHECK_NEAR(pause.MusicScale(), 1.0f);
    pause = Paused();
    CHECK_NEAR(pause.MusicScale(), PauseMenu::kMusicDuck);
    CHECK_NEAR(PauseMenu::kMusicDuck, 0.4f);
    Tap(pause, play, &PauseInput::back);
    CHECK_NEAR(pause.MusicScale(), 1.0f);
}

void testMainMenu() {
    const PauseInput play = Play();
    PauseMenu pause = Paused();
    Tap(pause, play, &PauseInput::down);
    CHECK_EQ(pause.Selected(), kMainMenu);
    PauseInput confirm = play;
    confirm.confirm = true;
    const PauseStep step = pause.Update(confirm);
    CHECK(!pause.Paused());
    CHECK(step.tick);          // this tick runs, with the cancel in it
    CHECK(step.sendCancel);
    CHECK(step.closed);

    // Exactly one tick of cancel - and the cancel, were the layer to show it
    // back as Esc held, does not open the pause again (the scene is still a
    // level until the Frame that runs it loads the menu).
    int cancels = 0;
    PauseInput after = play;
    after.open = true;
    after.back = true;
    for (int i = 0; i < 60; ++i) {
        after.open = after.back = (i == 0);
        after.confirm = i < 5;
        const PauseStep next = pause.Update(after);
        cancels += next.sendCancel ? 1 : 0;
        CHECK(next.tick);
    }
    CHECK_EQ(cancels, 0);
    CHECK(!pause.Paused());
    // A fresh Esc in play opens it again.
    Tap(pause, play, &PauseInput::open);
    CHECK(pause.Paused());

    // The menu scene (not play) with the same key: nothing.
    PauseMenu toMenu = Paused();
    Tap(toMenu, play, &PauseInput::down);
    toMenu.Update(confirm);
    PauseInput menuScene;
    menuScene.open = true;
    CHECK(toMenu.Update(menuScene).tick);
    menuScene.open = false;
    toMenu.Update(menuScene);
    menuScene.open = true;
    CHECK(toMenu.Update(menuScene).tick);
    CHECK(!toMenu.Paused());
}

void testFocus() {
    PauseInput play = Play();

    // Losing the focus in play pauses.
    PauseMenu pause;
    pause.Update(play);
    play.focused = false;
    PauseStep step = pause.Update(play);
    CHECK(pause.Paused());
    CHECK(step.opened && !step.tick);
    // Still unfocused, and focus back: it stays open for the player to resume.
    pause.Update(play);
    play.focused = true;
    pause.Update(play);
    CHECK(pause.Paused());

    // Off: no.
    PauseMenu off;
    off.SetAutoPause(false);
    CHECK(!off.AutoPause());
    play.focused = true;
    off.Update(play);
    play.focused = false;
    CHECK(off.Update(play).tick);
    CHECK(!off.Paused());

    // Not in play: no.
    PauseMenu menu;
    PauseInput notPlay;
    menu.Update(notPlay);
    notPlay.focused = false;
    menu.Update(notPlay);
    CHECK(!menu.Paused());
    // ...nor on coming into play already unfocused (a headless run in a
    // background window), nor on a window unfocused from the first tick.
    notPlay.inPlayScene = true;
    CHECK(menu.Update(notPlay).tick);
    CHECK(!menu.Paused());
    PauseMenu headless;
    PauseInput never = Play();
    never.focused = false;
    for (int i = 0; i < 10; ++i) headless.Update(never);
    CHECK(!headless.Paused());

    // The click that brings the window back does not choose: not on its tick,
    // nor for a quarter second after; then clicks count.
    PauseMenu back;
    PauseInput in = Play();
    back.Update(in);
    in.focused = false;
    back.Update(in);
    CHECK(back.Paused());
    const PauseMenu::Layout layout = PauseMenu::ComputeLayout(in.screen);
    in.pointer = (layout.rowMin[kMainMenu] + layout.rowMax[kMainMenu]) * 0.5f;
    back.Update(in);
    in.focused = true;
    in.click = true;
    step = back.Update(in);
    CHECK(back.Paused());
    CHECK(!step.sendCancel);
    for (int i = 0; i < 8; ++i) {
        in.click = (i % 2) == 1;
        CHECK(!back.Update(in).sendCancel);
    }
    CHECK(back.Paused());
    in.click = false;
    for (int i = 0; i < 20; ++i) back.Update(in);
    in.click = true;
    step = back.Update(in);
    CHECK(!back.Paused());
    CHECK(step.sendCancel);
}

void testPointer() {
    const PauseInput play = Play();
    const PauseMenu::Layout layout = PauseMenu::ComputeLayout(play.screen);
    const glm::vec2 overResume = (layout.rowMin[kResume] + layout.rowMax[kResume]) * 0.5f;
    const glm::vec2 overMainMenu = (layout.rowMin[kMainMenu] + layout.rowMax[kMainMenu]) * 0.5f;

    PauseMenu pause = Paused();
    PauseInput in = play;
    in.pointer = overMainMenu;
    pause.Update(in);                               // moved over Main menu
    CHECK_EQ(pause.Selected(), kMainMenu);
    Tap(pause, in, &PauseInput::up);                // the keys win over a pointer at rest
    CHECK_EQ(pause.Selected(), kResume);
    for (int i = 0; i < 5; ++i) pause.Update(in);
    CHECK_EQ(pause.Selected(), kResume);
    CHECK(pause.Paused());

    // A click outside the rows does nothing.
    in.pointer = glm::vec2(5.0f, 5.0f);
    pause.Update(in);
    PauseStep step = Tap(pause, in, &PauseInput::click);
    CHECK(pause.Paused());
    CHECK(!step.closed);
    // On the row's edge is outside (strictly inside, as Switch tests).
    in.pointer = layout.rowMin[kResume];
    pause.Update(in);
    Tap(pause, in, &PauseInput::click);
    CHECK(pause.Paused());

    // A click on Resume resumes; on Main menu, the cancel.
    in.pointer = overResume;
    pause.Update(in);
    step = Tap(pause, in, &PauseInput::click);
    CHECK(!pause.Paused());
    CHECK(step.closed && !step.sendCancel);
    pause = Paused();
    in.pointer = overMainMenu;
    step = Tap(pause, in, &PauseInput::click);      // moved and clicked in one tick
    CHECK(!pause.Paused());
    CHECK(step.sendCancel);
}

void testOverlay() {
    // Nothing unless paused.
    PauseMenu running;
    running.Update(Play());
    std::vector<HudCmd> none;
    running.AppendOverlay(none);
    CHECK(none.empty());

    PauseInput wide = Play();
    wide.screen = glm::vec2(1366.0f, 768.0f);
    PauseMenu pause = Paused(wide);
    std::vector<HudCmd> cmds;
    cmds.push_back(HudCmd{});   // appended, not replaced
    pause.AppendOverlay(cmds);
    CHECK_EQ(cmds.size(), std::size_t{1 + 3 + 6});
    if (cmds.size() != 10) return;

    // The dim: the whole screen, every corner the same see-through black.
    const HudCmd& dim = cmds[1];
    CHECK(dim.kind == HudCmd::Kind::Rectangle);
    CHECK(dim.pos == glm::vec2(0.0f));
    CHECK(dim.size == wide.screen);
    CHECK(dim.color == dim.color1 && dim.color == dim.color2 && dim.color == dim.color3);
    CHECK((dim.color & 0x00FFFFFFu) == 0u);
    CHECK(Alpha(dim.color) > 64 && Alpha(dim.color) < 255);

    // The panel, centred, showData's gradient: no corner left at HudCmd's
    // opaque white default.
    const HudCmd& panel = cmds[2];
    CHECK(panel.kind == HudCmd::Kind::Rectangle);
    CHECK_NEAR(panel.pos.x + panel.size.x * 0.5f, 683.0f);
    CHECK_NEAR(panel.pos.y + panel.size.y * 0.5f, 384.0f);
    CHECK(panel.color == panel.color1 && panel.color2 == panel.color3);
    CHECK(Alpha(panel.color) > Alpha(panel.color2));
    CHECK((panel.color & 0x00FFFFFFu) == 0u && (panel.color2 & 0x00FFFFFFu) == 0u);
    const HudCmd& highlight = cmds[3];
    CHECK(highlight.kind == HudCmd::Kind::Rectangle);
    CHECK(highlight.color == highlight.color1 && highlight.color == highlight.color2 &&
          highlight.color == highlight.color3);
    CHECK(Alpha(highlight.color) < 128);
    const PauseMenu::Layout layout = PauseMenu::ComputeLayout(wide.screen);
    CHECK(highlight.pos == layout.rowMin[kResume]);
    for (std::size_t i = 1; i <= 3; ++i) CHECK(cmds[i].color3 != 0xFFFFFFFFu && cmds[i].color1 != 0xFFFFFFFFu);

    // shadowText pairs: a black copy at half the alpha a tenth of the size
    // down-right, then the text in the scripts' (203,203,228), Arial Narrow.
    const char* texts[] = {"Pausado", "Continuar", "Menu principal"};
    for (std::size_t t = 0; t < 3; ++t) {
        const HudCmd& shadow = cmds[4 + t * 2];
        const HudCmd& front = cmds[5 + t * 2];
        CHECK(shadow.kind == HudCmd::Kind::Text && front.kind == HudCmd::Kind::Text);
        CHECK(front.text == texts[t] && shadow.text == texts[t]);
        CHECK(front.font == "Arial Narrow" && shadow.font == "Arial Narrow");
        CHECK_NEAR(shadow.pos.x, front.pos.x + front.fontSize * 0.1f);
        CHECK_NEAR(shadow.pos.y, front.pos.y + front.fontSize * 0.1f);
        CHECK((front.color & 0x00FFFFFFu) == 0x00CBCBE4u);
        CHECK((shadow.color & 0x00FFFFFFu) == 0u);
        CHECK_EQ(static_cast<int>(Alpha(shadow.color)), Alpha(front.color) / 2);
        // Inside the panel.
        CHECK(front.pos.x > panel.pos.x && front.pos.y > panel.pos.y);
        CHECK(front.pos.y + front.fontSize < panel.pos.y + panel.size.y);
    }
    CHECK_NEAR(cmds[5].fontSize, 40.0f);
    CHECK_NEAR(cmds[7].fontSize, 30.0f);
    CHECK_EQ(static_cast<int>(Alpha(cmds[5].color)), 255);   // the title
    CHECK_EQ(static_cast<int>(Alpha(cmds[7].color)), 255);   // Continuar, selected
    CHECK(Alpha(cmds[9].color) < 255);                        // Menu principal, not

    // Moving the selection moves the highlight and the full alpha.
    Tap(pause, wide, &PauseInput::down);
    cmds.clear();
    pause.AppendOverlay(cmds);
    CHECK_EQ(cmds.size(), std::size_t{9});
    if (cmds.size() == 9) {
        CHECK(cmds[2].pos == layout.rowMin[kMainMenu]);
        CHECK(Alpha(cmds[6].color) < 255);
        CHECK_EQ(static_cast<int>(Alpha(cmds[8].color)), 255);
    }

    // 4:3: centred on 512.
    PauseMenu narrow = Paused();
    cmds.clear();
    narrow.AppendOverlay(cmds);
    CHECK(!cmds.empty());
    if (cmds.size() > 1) {
        CHECK(cmds[0].size == glm::vec2(1024.0f, 768.0f));
        CHECK_NEAR(cmds[1].pos.x + cmds[1].size.x * 0.5f, 512.0f);
    }
    // Rows inside the panel, one under the other, not overlapping.
    const PauseMenu::Layout four3 = PauseMenu::ComputeLayout(glm::vec2(1024.0f, 768.0f));
    CHECK(four3.rowMin[0].x > four3.panelMin.x && four3.rowMax[0].x < four3.panelMax.x);
    CHECK(four3.rowMax[0].y <= four3.rowMin[1].y);
    CHECK(four3.rowMax[1].y < four3.panelMax.y);
    CHECK(four3.title.y + PauseMenu::kTitleSize <= four3.rowMin[0].y);
    CHECK(four3.panelMin == glm::floor(four3.panelMin));
}

// The overlay through HudRenderer: after the snapshot's commands, before the
// bars, translated and laid out like the scripts' text.
void testThroughHudRenderer() {
    entt::registry registry;
    Penumbra::Render::TextureCache textures(PENUMBRA_ORIGINAL_DIR);
    Penumbra::Render::FontAtlas fonts;
    Localization loc;
    CHECK(loc.Load());
    loc.SetLanguage(Language::English);
    HudRenderer hud;
    hud.Attach(registry, textures, fonts, loc);

    // A 4:3 menu pillarboxed in a 1366x768 window: a scene rectangle, then the
    // overlay, then the two bars.
    View view;
    view.logicalScreen = glm::vec2(1024.0f, 768.0f);
    view.windowPixels = glm::uvec2(1366, 768);
    view.scale = 1.0f;
    view.viewportMin = glm::vec2(171.0f, 0.0f);
    view.viewportMax = glm::vec2(1195.0f, 768.0f);

    RenderSnapshot snapshot;
    HudCmd scene;
    scene.kind = HudCmd::Kind::Rectangle;
    scene.size = glm::vec2(10.0f);
    scene.color = scene.color1 = scene.color2 = scene.color3 = 0xFF102030u;
    snapshot.hud.push_back(scene);

    std::vector<Supersonic::ScreenOverlay::Quad> plain;
    hud.Build(snapshot, view, plain);
    std::vector<Supersonic::ScreenOverlay::Quad> nullExtra;
    hud.Build(snapshot, view, nullExtra, nullptr);
    CHECK_EQ(nullExtra.size(), plain.size());
    CHECK_EQ(plain.size(), std::size_t{1 + 2});

    PauseMenu pause = Paused();
    std::vector<HudCmd> extra;
    pause.AppendOverlay(extra);
    std::vector<Supersonic::ScreenOverlay::Quad> quads;
    hud.Build(snapshot, view, quads, &extra);
    CHECK(quads.size() > plain.size() + 3);
    if (quads.size() > plain.size() + 3) {
        // The scene's quad first, the dim next over the whole 4:3 box, the
        // bars last and black.
        CHECK(quads[0].color == plain[0].color);
        CHECK_NEAR(quads[1].min.x, 171.0f / 1366.0f);
        CHECK_NEAR(quads[1].max.x, 1195.0f / 1366.0f);
        CHECK_NEAR(quads[1].min.y, 0.0f);
        CHECK_NEAR(quads[1].max.y, 1.0f);
        CHECK_NEAR(quads[1].color.a, 150.0f / 255.0f);
        CHECK_NEAR(quads[1].color.r, 0.0f);
        const auto& lastBar = quads.back();
        const auto& firstBar = quads[quads.size() - 2];
        CHECK_NEAR(firstBar.max.x, 171.0f / 1366.0f);
        CHECK_NEAR(lastBar.min.x, 1195.0f / 1366.0f);
        CHECK_NEAR(lastBar.color.a, 1.0f);
        // No quad of the overlay is opaque white (a corner left at default).
        bool anyWhite = false;
        for (std::size_t i = 1; i + 2 < quads.size(); ++i) {
            if (quads[i].texture.empty() && quads[i].color == glm::vec4(1.0f)) anyWhite = true;
        }
        CHECK(!anyWhite);
    }

    // With the face on this machine: English glyphs, and the texts fit their rows.
    if (!fonts.FaceFile(PauseMenu::kFont).empty()) {
        std::size_t glyphs = 0;
        for (const auto& quad : quads) glyphs += quad.texture.rfind("penumbra:font:", 0) == 0 ? 1 : 0;
        // "Paused" + "Resume" + "Main menu": 6 + 6 + 8 glyphs with pixels, twice.
        CHECK_EQ(glyphs, std::size_t{(6 + 6 + 8) * 2});
        const PauseMenu::Layout layout = PauseMenu::ComputeLayout(view.logicalScreen);
        // E24: every language with its own file too, as HudRenderer draws it
        // (shaped and ordered), each overflow reported.
        Penumbra::Render::VisualText visual;
        const auto widthOf = [&](const char* text, const Language language, const float size, const glm::vec2 at) {
            return fonts
                .LayoutCodePoints(visual.Of(loc.Translate(text, language), Penumbra::Render::IsRightToLeft(language)),
                                  PauseMenu::kFont, size, at)
                .width;
        };
        for (const Penumbra::Render::LanguageInfo& info : Penumbra::Render::kLanguages) {
            const Language language = info.language;
            if (!loc.HasLanguageFile(language)) continue;
            const float title = widthOf(PauseMenu::kTitle, language, PauseMenu::kTitleSize, layout.title);
            CHECK_MSG(layout.title.x + title < layout.panelMax.x,
                      std::string(info.id) + " [" + Penumbra::Eth::Cp1252ToUtf8(PauseMenu::kTitle) + "] \"" +
                          loc.Translate(PauseMenu::kTitle, language) + "\": " +
                          std::to_string(layout.title.x + title) + " px, the panel ends at " +
                          std::to_string(layout.panelMax.x));
            for (int i = 0; i < PauseMenu::kItemCount; ++i) {
                const auto index = static_cast<std::size_t>(i);
                const float width = widthOf(PauseMenu::kItemText[index], language, PauseMenu::kItemSize, layout.text[index]);
                CHECK_MSG(layout.text[index].x + width < layout.rowMax[index].x,
                          std::string(info.id) + " [" + Penumbra::Eth::Cp1252ToUtf8(PauseMenu::kItemText[index]) + "] \"" +
                              loc.Translate(PauseMenu::kItemText[index], language) +
                              "\": " + std::to_string(layout.text[index].x + width) + " px, the row ends at " +
                              std::to_string(layout.rowMax[index].x));
            }
        }
    }
    hud.Detach();
}

bool SameQuads(const std::vector<Supersonic::ScreenOverlay::Quad>& a,
               const std::vector<Supersonic::ScreenOverlay::Quad>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].min != b[i].min || a[i].max != b[i].max || a[i].uvMin != b[i].uvMin || a[i].uvMax != b[i].uvMax ||
            a[i].color != b[i].color || a[i].texture != b[i].texture) {
            return false;
        }
    }
    return true;
}

// E24: in a right-to-left language the title and the items end at the panel's
// and the rows' right, inset as they start from the left (HudCmd::rtlRight);
// in every other language nothing moves.
void testRightToLeft() {
    const PauseMenu::Layout layout = PauseMenu::ComputeLayout(glm::vec2(1024.0f, 768.0f));
    CHECK_NEAR(layout.panelMax.x - layout.titleRight, layout.title.x - layout.panelMin.x);
    for (std::size_t i = 0; i < static_cast<std::size_t>(PauseMenu::kItemCount); ++i) {
        CHECK_NEAR(layout.rowMax[i].x - layout.textRight[i], layout.text[i].x - layout.rowMin[i].x);
        CHECK(layout.textRight[i] > layout.text[i].x);
    }

    PauseMenu pause = Paused();
    std::vector<HudCmd> cmds;
    pause.AppendOverlay(cmds);
    CHECK_EQ(cmds.size(), std::size_t{3 + 6});
    if (cmds.size() != 9) return;
    const float rights[3] = {layout.titleRight, layout.textRight[0], layout.textRight[1]};
    for (std::size_t t = 0; t < 3; ++t) {
        const HudCmd& shadow = cmds[3 + t * 2];
        const HudCmd& front = cmds[4 + t * 2];
        CHECK_NEAR(front.rtlRight, rights[t]);
        CHECK_NEAR(shadow.rtlRight, rights[t] + front.fontSize * 0.1f);
    }
    for (std::size_t i = 0; i < 3; ++i) CHECK(cmds[i].rtlRight == 0.0f);   // the rectangles

    entt::registry registry;
    Penumbra::Render::TextureCache textures(PENUMBRA_ORIGINAL_DIR);
    Penumbra::Render::FontAtlas fonts;
    fonts.SetSystemFontsEnabled(false);
    Localization loc;
    CHECK(loc.Load());
    HudRenderer hud;
    hud.Attach(registry, textures, fonts, loc);
    // 4:3 at one window pixel a logical one: a quad's x times 1024 is its x.
    View view;
    view.logicalScreen = glm::vec2(1024.0f, 768.0f);
    view.windowPixels = glm::uvec2(1024, 768);
    view.scale = 1.0f;
    view.viewportMin = glm::vec2(0.0f);
    view.viewportMax = glm::vec2(1024.0f, 768.0f);
    RenderSnapshot empty;
    const auto quadsOf = [&](const std::vector<HudCmd>& extra) {
        std::vector<Supersonic::ScreenOverlay::Quad> quads;
        hud.Build(empty, view, quads, &extra);
        return quads;
    };
    std::vector<HudCmd> unmarked = cmds;
    for (HudCmd& cmd : unmarked) cmd.rtlRight = 0.0f;

    // Left to right: the overlay's every quad where it was without the marks.
    for (const Penumbra::Render::LanguageInfo& info : Penumbra::Render::kLanguages) {
        if (info.rightToLeft) continue;
        loc.SetLanguage(info.language);
        const auto marked = quadsOf(cmds);
        CHECK(marked.size() > 3);
        CHECK_MSG(SameQuads(marked, quadsOf(unmarked)), info.id);
    }

    if (!loc.HasLanguageFile(Language::Arabic) || fonts.FaceFile(PauseMenu::kFont).empty()) return;
    loc.SetLanguage(Language::Arabic);
    const float lefts[3] = {layout.title.x, layout.text[0].x, layout.text[1].x};
    for (std::size_t t = 0; t < 3; ++t) {
        const std::vector<HudCmd> front = {cmds[4 + t * 2]};
        const auto quads = quadsOf(front);
        CHECK(!quads.empty());
        float left = 1e9f;
        float right = -1e9f;
        for (const auto& quad : quads) {
            left = std::min(left, quad.min.x * 1024.0f);
            right = std::max(right, quad.max.x * 1024.0f);
        }
        // Ends at the inset right edge, give or take the last letter's side
        // bearing; starts well right of where the left-to-right text starts.
        CHECK_MSG(right <= rights[t] + 3.0f && right >= rights[t] - 8.0f,
                  std::to_string(t) + ": ink to " + std::to_string(right) + ", edge " + std::to_string(rights[t]));
        CHECK(left > lefts[t] + 40.0f);
        std::printf("  E24 pause ar: text %zu ink x %.0f-%.0f, edge %.0f\n", t, left, right, rights[t]);
        // Unmarked, as before: from the left inset.
        const std::vector<HudCmd> plain = {unmarked[4 + t * 2]};
        const auto before = quadsOf(plain);
        if (!before.empty()) CHECK(before.front().min.x * 1024.0f < lefts[t] + 6.0f);
    }
    hud.Detach();
}

// What the game sees after a pause: nothing pressed in it until it is released.
void testFilter() {
    const PauseInput play = Play();
    PauseMenu pause;
    pause.Update(play);

    // Before any pause the filter passes everything.
    InputFrame walking;
    walking.keys[K_RIGHT] = true;
    walking.pads[1].connected = true;
    walking.pads[1].xy = glm::vec2(1.0f, 0.0f);   // player 1's stick, walking right too
    InputFrame seen = walking;
    pause.FilterForGame(seen);
    CHECK(seen.keys[K_RIGHT]);
    CHECK(seen.pads[1].xy == glm::vec2(1.0f, 0.0f));

    // Esc pauses (no Frame, so no filter call); Esc resumes.
    Tap(pause, play, &PauseInput::open);
    CHECK(pause.Paused());
    PauseInput resume = play;
    resume.back = true;
    CHECK(pause.Update(resume).closed);

    // The resuming tick: Esc, A and the stick pushed down were pressed in the
    // pause; Right and the rightward lean were held through it.
    InputFrame frame = walking;
    frame.keys[K_ESC] = true;
    frame.pads[1].buttons[JK_03] = true;
    frame.pads[0].connected = true;
    frame.pads[0].xy = glm::vec2(0.0f, 1.0f);
    pause.FilterForGame(frame);
    CHECK(!frame.keys[K_ESC]);                       // not escToGoToMenu's KS_HIT
    CHECK(!frame.pads[1].buttons[JK_03]);            // not a jump
    CHECK(frame.pads[0].xy == glm::vec2(0.0f));      // not a "next level"
    CHECK(frame.keys[K_RIGHT]);                      // still walking
    CHECK(frame.pads[1].xy == glm::vec2(1.0f, 0.0f));

    // Held on: still held up. A key pressed now, after the pause, goes through.
    frame = walking;
    frame.keys[K_ESC] = true;
    frame.pads[1].buttons[JK_03] = true;
    frame.pads[0].connected = true;
    frame.pads[0].xy = glm::vec2(0.0f, 1.0f);
    frame.keys[K_S] = true;
    pause.FilterForGame(frame);
    CHECK(!frame.keys[K_ESC] && !frame.pads[1].buttons[JK_03]);
    CHECK(frame.pads[0].xy == glm::vec2(0.0f));
    CHECK(frame.keys[K_S]);

    // Released, then pressed again: the new press is the game's.
    InputFrame released = walking;
    released.pads[0].connected = true;
    pause.FilterForGame(released);
    frame = walking;
    frame.pads[1].buttons[JK_03] = true;
    frame.pads[0].connected = true;
    frame.pads[0].xy = glm::vec2(0.0f, 1.0f);
    pause.FilterForGame(frame);
    CHECK(frame.pads[1].buttons[JK_03]);
    CHECK(frame.pads[0].xy == glm::vec2(0.0f, 1.0f));

    // Main menu picked with Enter: the tick with the cancel filters Enter out
    // (the layer then adds K_ESC), and so does the menu's first tick while
    // Enter is still down - getConfirmButtonStatus must not see a KS_HIT.
    PauseMenu leave;
    leave.Update(play);
    InputFrame idle;
    leave.FilterForGame(idle);
    Tap(leave, play, &PauseInput::open);
    Tap(leave, play, &PauseInput::down);
    PauseInput confirm = play;
    confirm.confirm = true;
    const PauseStep step = leave.Update(confirm);
    CHECK(step.sendCancel);
    InputFrame enter;
    enter.keys[K_ENTER] = true;
    enter.keys[K_LMOUSE] = true;
    leave.FilterForGame(enter);
    CHECK(!enter.keys[K_ENTER] && !enter.keys[K_LMOUSE]);
    if (step.sendCancel) enter.keys[K_ESC] = true;   // the layer's cancel, after the filter
    CHECK(enter.keys[K_ESC]);
    PauseInput menuScene;
    menuScene.confirm = true;
    CHECK(leave.Update(menuScene).tick);
    InputFrame menuFirst;
    menuFirst.keys[K_ENTER] = true;
    menuFirst.keys[K_LMOUSE] = true;
    leave.FilterForGame(menuFirst);
    CHECK(!menuFirst.keys[K_ENTER] && !menuFirst.keys[K_LMOUSE]);
    CHECK(!menuFirst.keys[K_ESC]);                   // the cancel was one tick
    InputFrame up;
    leave.FilterForGame(up);
    InputFrame again;
    again.keys[K_ENTER] = true;
    leave.FilterForGame(again);
    CHECK(again.keys[K_ENTER]);                      // a new press on the menu

    // The touch editor closes by the same keys and freezes the game as the pause does: HoldPressed arms the filter   // E28
    // without a pause, so the Esc or click that closed it never reaches the options screen under it.   // E28
    PauseMenu plain;   // E28
    InputFrame nothing;   // E28
    plain.FilterForGame(nothing);   // E28
    InputFrame escDown;   // E28
    escDown.keys[K_ESC] = true;   // E28
    escDown.keys[K_LMOUSE] = true;   // E28
    plain.FilterForGame(escDown);   // E28
    CHECK(escDown.keys[K_ESC] && escDown.keys[K_LMOUSE]);        // with no HoldPressed they pass   // E28
    PauseMenu editor;   // E28
    editor.FilterForGame(nothing);   // E28
    editor.HoldPressed();   // E28
    InputFrame closing;   // E28
    closing.keys[K_ESC] = true;   // E28
    closing.keys[K_LMOUSE] = true;   // E28
    editor.FilterForGame(closing);   // E28
    CHECK(!closing.keys[K_ESC] && !closing.keys[K_LMOUSE]);      // masked: the game never saw them go down   // E28
    InputFrame stillDown = closing;   // E28
    stillDown.keys[K_ESC] = true;   // E28
    stillDown.keys[K_LMOUSE] = true;   // E28
    editor.FilterForGame(stillDown);   // E28
    CHECK(!stillDown.keys[K_ESC] && !stillDown.keys[K_LMOUSE]);  // and for as long as they are held   // E28
    InputFrame letGo;   // E28
    editor.FilterForGame(letGo);   // E28
    InputFrame fresh;   // E28
    fresh.keys[K_ESC] = true;   // E28
    editor.FilterForGame(fresh);   // E28
    CHECK(fresh.keys[K_ESC]);                                    // a press after the release is the game's   // E28
    // A key the game saw go down before is not masked: HoldPressed holds up only what it had not seen.   // E28
    PauseMenu strolling;   // E28
    InputFrame right;   // E28
    right.keys[K_RIGHT] = true;   // E28
    strolling.FilterForGame(right);   // E28
    strolling.HoldPressed();   // E28
    InputFrame stillRight;   // E28
    stillRight.keys[K_RIGHT] = true;   // E28
    strolling.FilterForGame(stillRight);   // E28
    CHECK(stillRight.keys[K_RIGHT]);   // E28
}

void testEnglish() {
    Localization loc;
    CHECK(loc.Load());
    const char* english[] = {"Paused", "Resume", "Main menu"};
    const char* portuguese[] = {PauseMenu::kTitle, PauseMenu::kItemText[0], PauseMenu::kItemText[1]};
    for (std::size_t i = 0; i < 3; ++i) {
        CHECK_MSG(loc.HasTranslation(portuguese[i]), portuguese[i]);
        CHECK(loc.Translate(portuguese[i], Language::English) == english[i]);
        CHECK(loc.Translate(portuguese[i], Language::Portuguese) == Penumbra::Eth::Cp1252ToUtf8(portuguese[i]));
    }
}

void testSetting() {
    const Settings defaults = Settings::Defaults("en");
    CHECK(defaults.pauseOnFocusLoss);
    CHECK(Settings::Defaults("pt").pauseOnFocusLoss);

    Settings off = defaults;
    off.pauseOnFocusLoss = false;
    const std::string json = off.ToJson();
    CHECK(json.find("\"pauseOnFocusLoss\": false") != std::string::npos);
    std::string warning;
    const Settings back = Settings::FromJson(json, defaults, &warning);
    CHECK(!back.pauseOnFocusLoss);
    CHECK(back == off);
    CHECK(warning.empty());
    CHECK(Settings::FromJson(defaults.ToJson(), off) == defaults);

    // An older file without it: the default. A wrong type: the default, and a warning.
    CHECK(Settings::FromJson("{\"language\": \"en\"}", defaults).pauseOnFocusLoss);
    warning.clear();
    CHECK(Settings::FromJson("{\"pauseOnFocusLoss\": 3}", defaults, &warning).pauseOnFocusLoss);
    CHECK(!warning.empty());
}

// E36: settings.difficulty - "normal" (the default) or "hard", written in lower case, read in any case, and anything else
// (a number, a boolean, another word, an empty string) is normal with a warning; the file's version stays 2.
void testDifficultySetting() {   // E36
    const Settings defaults = Settings::Defaults("en");
    CHECK(defaults.difficulty == "normal" && !defaults.HardDifficulty());

    // Written as the word, and read back.
    Settings hard = defaults;
    hard.difficulty = "hard";
    CHECK(hard.HardDifficulty());
    const std::string json = hard.ToJson();
    CHECK(json.find("\"difficulty\": \"hard\"") != std::string::npos);
    CHECK(defaults.ToJson().find("\"difficulty\": \"normal\"") != std::string::npos);
    CHECK(json.find("\"version\": 2") != std::string::npos);   // not bumped: nothing reads the number
    std::string warning;
    const Settings back = Settings::FromJson(json, defaults, &warning);
    CHECK(back.HardDifficulty() && back.difficulty == "hard");
    CHECK(back == hard);
    CHECK(warning.empty());
    CHECK(Settings::FromJson(defaults.ToJson(), hard) == defaults);   // a file that says normal wins over defaults that say hard

    // Any case of the word.
    for (const char* spelling : {"hard", "Hard", "HARD", "hArD"}) {
        warning.clear();
        const Settings read = Settings::FromJson(std::string("{\"difficulty\": \"") + spelling + "\"}", defaults, &warning);
        CHECK_MSG(read.HardDifficulty() && read.difficulty == "hard", spelling);   // stored in its one form
        CHECK_MSG(warning.empty(), spelling);
    }
    for (const char* spelling : {"normal", "Normal", "NORMAL"}) {
        warning.clear();
        const Settings read = Settings::FromJson(std::string("{\"difficulty\": \"") + spelling + "\"}", hard, &warning);
        CHECK_MSG(!read.HardDifficulty() && read.difficulty == "normal", spelling);
        CHECK_MSG(warning.empty(), spelling);
    }

    // A file without the key keeps what the defaults say - normal in every real run - and says nothing.
    warning.clear();
    CHECK(Settings::FromJson("{\"language\": \"en\"}", defaults, &warning).difficulty == "normal");
    CHECK(Settings::FromJson("{\"language\": \"en\"}", hard, &warning).difficulty == "hard");
    CHECK(warning.empty());

    // Anything else is normal, whatever the defaults were, and worth a warning.
    for (const char* wrong : {"3", "true", "1", "null", "[]", "{}", "\"extreme\"", "\"\"", "\"hardcore\"", "\"hard \""}) {
        warning.clear();
        const Settings read = Settings::FromJson(std::string("{\"difficulty\": ") + wrong + "}", hard, &warning);
        CHECK_MSG(!read.HardDifficulty() && read.difficulty == "normal", wrong);
        CHECK_MSG(!warning.empty(), wrong);
    }
    // A broken one leaves the other fields as they were.
    warning.clear();
    const Settings mixed = Settings::FromJson("{\"difficulty\": 7, \"pauseOnFocusLoss\": false}", defaults, &warning);
    CHECK(mixed.difficulty == "normal" && !mixed.pauseOnFocusLoss);
    CHECK(!warning.empty());
}

// E28 BEGIN pure editor tests (render/TouchEditor): no fonts, no original files, no art   // E28
using Penumbra::Render::TouchAnchor;   // E28
using Penumbra::Render::TouchContact;   // E28
using Penumbra::Render::TouchControl;   // E28
using Penumbra::Render::TouchControls;   // E28
using Penumbra::Render::TouchEditInput;   // E28
using Penumbra::Render::TouchEditor;   // E28
using Penumbra::Render::TouchEditStep;   // E28
using Penumbra::Render::TouchEditWidget;   // E28
using Penumbra::Render::TouchGeometry;   // E28
using Penumbra::Render::TouchInput;   // E28
using Penumbra::Render::TouchInsets;   // E28
using Penumbra::Render::TouchLayout;   // E28
using Penumbra::Render::TouchManifest;   // E28
using Penumbra::Render::TouchMove;   // E28
using Penumbra::Render::TouchScene;   // E28
using Penumbra::Render::TouchStep;   // E28
using Penumbra::Render::TouchTuning;   // E28
using Penumbra::Render::kTouchEditWidgetCount;   // E28

TouchContact Finger(const int id, const glm::vec2& at) {   // E28
    TouchContact contact;   // E28
    contact.id = id;   // E28
    contact.position = at;   // E28
    contact.down = true;   // E28
    return contact;   // E28
}   // E28

bool AtPoint(const glm::vec2& value, const float x, const float y, const float eps = 0.01f) {   // E28
    return std::fabs(value.x - x) <= eps && std::fabs(value.y - y) <= eps;   // E28
}   // E28

bool BoxIs(const TouchLayout::Box& box, const float x0, const float y0, const float x1, const float y1) {   // E28
    return AtPoint(box.min, x0, y0) && AtPoint(box.max, x1, y1);   // E28
}   // E28

bool BoxesOverlap(const TouchLayout::Box& a, const TouchLayout::Box& b) {   // E28
    return a.min.x < b.max.x && a.max.x > b.min.x && a.min.y < b.max.y && a.max.y > b.min.y;   // E28
}   // E28

constexpr std::size_t Index(const TouchControl control) { return static_cast<std::size_t>(control); }   // E28

// The layer's tick, in its order: the controls see the fingers (the Edit scene), then the editor does, then the controls   // E28
// take the tuning it hands back. The editor is given the test's own finger list, which is what TouchControls::Down()   // E28
// returns (the same fingers, latched ones merged): the latch itself is tested apart (testEditorLatchedTap).   // E28
struct EditRig {   // E28
    TouchControls controls;   // E28
    TouchEditor editor;   // E28
    TouchInput base;   // E28
    TouchStep touch;       // what the controls made of the last tick's fingers   // E28
    int commits = 0;       // ticks that said commit   // E28
    int changes = 0;       // ticks that said changed   // E28

    explicit EditRig(const TouchManifest& manifest = TouchControls::DefaultManifest()) {   // E28
        controls.SetManifest(manifest);   // E28
        base.scene = TouchScene::Edit;   // E28
    }   // E28

    // The window of a 20:9 phone: the shown area runs 341.5 past the 4:3 box each side.   // E28
    void Wide() {   // E28
        base.areaMin = glm::vec2(-341.5f, 0.0f);   // E28
        base.areaMax = glm::vec2(1365.5f, 768.0f);   // E28
    }   // E28

    TouchGeometry Geometry() const { return TouchGeometry::From(base); }   // E28
    TouchLayout::Box Widget(const TouchEditWidget widget) const {   // E28
        return TouchEditor::ComputeLayout(Geometry()).widget[static_cast<std::size_t>(widget)];   // E28
    }   // E28
    glm::vec2 Where(const TouchEditWidget widget) const { return Widget(widget).Centre(); }   // E28
    glm::vec2 Where(const TouchControl control) const { return controls.Layout()[control].Centre(); }   // E28

    // The layer's OpenTouchEditor: the controls are laid out first (the prime), then the editor opens.   // E28
    void Open(const TouchTuning& tuning = TouchTuning{}, const std::vector<TouchContact>& downNow = {},   // E28
              const bool unlocked = false) {   // E28
        controls.SetTuning(tuning);   // E28
        TouchInput in = base;   // E28
        in.contacts = downNow;   // E28
        controls.Update(in);   // E28
        editor.Open(tuning, downNow, unlocked);   // E28
    }   // E28

    TouchEditStep Tick(const std::vector<TouchContact>& fingers = {}, const bool close = false) {   // E28
        TouchInput in = base;   // E28
        in.contacts = fingers;   // E28
        touch = controls.Update(in);   // E28
        TouchEditInput edit;   // E28
        edit.down = &fingers;   // E28
        edit.geometry = TouchGeometry::From(in);   // E28
        edit.close = close;   // E28
        const TouchEditStep step = editor.Update(edit, controls);   // E28
        if (step.changed) controls.SetTuning(editor.Tuning());   // E28
        commits += step.commit ? 1 : 0;   // E28
        changes += step.changed ? 1 : 0;   // E28
        return step;   // E28
    }   // E28

    // A finger lands on `from`, then moves to `to` in `ticks` equal steps (it stays down). Returns how many of the moving   // E28
    // ticks said changed.   // E28
    int Drag(const int id, const glm::vec2& from, const glm::vec2& to, const int ticks) {   // E28
        Tick({Finger(id, from)});   // E28
        int changed = 0;   // E28
        for (int i = 1; i <= ticks; ++i) {   // E28
            const glm::vec2 at = from + (to - from) * (static_cast<float>(i) / static_cast<float>(ticks));   // E28
            changed += Tick({Finger(id, at)}).changed ? 1 : 0;   // E28
        }   // E28
        return changed;   // E28
    }   // E28

    // A tap: down for one tick (shorter than the layer's tick would still count), up the next. The lift tick's step.   // E28
    TouchEditStep Tap(const TouchEditWidget widget, const int id = 1) {   // E28
        Tick({Finger(id, Where(widget))});   // E28
        return Tick();   // E28
    }   // E28

    std::vector<HudCmd> Overlay() const {   // E28
        std::vector<HudCmd> out;   // E28
        editor.AppendOverlay(controls, out);   // E28
        return out;   // E28
    }   // E28
    std::size_t ControlCommands() const {   // E28
        std::vector<HudCmd> out;   // E28
        controls.AppendOverlay(out);   // E28
        return out.size();   // E28
    }   // E28
};   // E28

// The overlay's tail is fixed: 7 widgets, then 14 Texts.   // E28
constexpr std::size_t kEditTexts = 14;   // E28
constexpr std::size_t kEditWidgets = 7;   // E28

const HudCmd& WidgetCmd(const std::vector<HudCmd>& overlay, const TouchEditWidget widget) {   // E28
    return overlay[overlay.size() - kEditTexts - kEditWidgets + static_cast<std::size_t>(widget)];   // E28
}   // E28
const HudCmd& TextCmd(const std::vector<HudCmd>& overlay, const std::size_t index) {   // E28
    return overlay[overlay.size() - kEditTexts + index];   // E28
}   // E28

TouchTuning Tuned() {   // E28
    TouchTuning tuning;   // E28
    tuning.size = 0.8f;   // E28
    tuning.opacity = 0.6f;   // E28
    tuning.move[Index(TouchControl::Jump)] = TouchMove{-40.0f, 12.0f};   // E28
    tuning.move[Index(TouchControl::Pause)] = TouchMove{-30.0f, 20.0f};   // E28
    return tuning;   // E28
}   // E28

// Where everything is: the tiles, the back arrow, and the title kept clear of the Pause.   // E28
void testEditorLayout() {   // E28
    // 4:3, no notch.   // E28
    const TouchEditor::Layout four3 = TouchEditor::ComputeLayout(TouchGeometry{});   // E28
    const auto at = [](const TouchEditor::Layout& layout, const TouchEditWidget widget) {   // E28
        return layout.widget[static_cast<std::size_t>(widget)];   // E28
    };   // E28
    CHECK(BoxIs(at(four3, TouchEditWidget::Back), 41.0f, 41.0f, 105.0f, 105.0f));   // E28
    CHECK(BoxIs(at(four3, TouchEditWidget::SizeLess), 352.0f, 120.0f, 448.0f, 216.0f));   // E28
    CHECK(BoxIs(at(four3, TouchEditWidget::SizeMore), 576.0f, 120.0f, 672.0f, 216.0f));   // E28
    CHECK(BoxIs(at(four3, TouchEditWidget::OpacityLess), 352.0f, 228.0f, 448.0f, 324.0f));   // E28
    CHECK(BoxIs(at(four3, TouchEditWidget::OpacityMore), 576.0f, 228.0f, 672.0f, 324.0f));   // E28
    CHECK(BoxIs(at(four3, TouchEditWidget::Lock), 408.0f, 344.0f, 504.0f, 440.0f));   // E28
    CHECK(BoxIs(at(four3, TouchEditWidget::Restore), 520.0f, 344.0f, 616.0f, 440.0f));   // E28
    CHECK(AtPoint(four3.title, 125.0f, 53.0f));   // E28
    CHECK_NEAR(four3.titleRight, 884.0f);   // E28
    CHECK_NEAR(four3.centreX, 512.0f);   // E28
    CHECK_NEAR(four3.rowY[0], 168.0f);   // E28
    CHECK_NEAR(four3.rowY[1], 276.0f);   // E28
    CHECK_NEAR(four3.rowY[2], 392.0f);   // E28

    // 20:9: the shown area runs 341.5 past the box each side. The centred widgets stay where they were; the back arrow   // E28
    // and the title follow the area's corner.   // E28
    TouchGeometry wide;   // E28
    wide.areaMin = glm::vec2(-341.5f, 0.0f);   // E28
    wide.areaMax = glm::vec2(1365.5f, 768.0f);   // E28
    const TouchEditor::Layout phone = TouchEditor::ComputeLayout(wide);   // E28
    for (int w = 1; w < kTouchEditWidgetCount; ++w) {   // E28
        const auto widget = static_cast<TouchEditWidget>(w);   // E28
        CHECK(BoxIs(at(phone, widget), at(four3, widget).min.x, at(four3, widget).min.y, at(four3, widget).max.x,   // E28
                    at(four3, widget).max.y));   // E28
    }   // E28
    CHECK_NEAR(at(phone, TouchEditWidget::Back).min.x, -300.5f);   // E28
    CHECK_NEAR(at(phone, TouchEditWidget::Back).min.y, 41.0f);   // E28
    CHECK_NEAR(phone.title.x, -300.5f + 64.0f + 20.0f);   // E28
    CHECK_NEAR(phone.titleRight, 1365.5f - 140.0f);   // E28

    // A notch on the left moves the arrow and the title by its width, and nothing else.   // E28
    TouchGeometry notch = wide;   // E28
    notch.safeArea = TouchInsets{88.0f, 0.0f, 88.0f, 24.0f};   // E28
    const TouchEditor::Layout notched = TouchEditor::ComputeLayout(notch);   // E28
    CHECK_NEAR(at(notched, TouchEditWidget::Back).min.x, at(phone, TouchEditWidget::Back).min.x + 88.0f);   // E28
    CHECK_NEAR(notched.title.x, phone.title.x + 88.0f);   // E28
    CHECK_NEAR(notched.titleRight, 1365.5f - 88.0f - 140.0f);   // E28
    CHECK(BoxIs(at(notched, TouchEditWidget::Lock), 408.0f, 344.0f, 504.0f, 440.0f));   // E28
    // The top inset moves the rows down with it.   // E28
    TouchGeometry top = wide;   // E28
    top.safeArea = TouchInsets{0.0f, 30.0f, 0.0f, 20.0f};   // E28
    const TouchEditor::Layout lowered = TouchEditor::ComputeLayout(top);   // E28
    CHECK_NEAR(at(lowered, TouchEditWidget::Back).min.y, 71.0f);   // E28
    CHECK_NEAR(lowered.title.y, 83.0f);   // E28
    CHECK_NEAR(at(lowered, TouchEditWidget::SizeLess).min.y, 150.0f);   // E28
    CHECK_NEAR(lowered.rowY[2], 422.0f);   // E28

    // The title ends clear of the Pause, which hangs from the HUD frame's corner when that lies further in   // E28
    // than the notch (E26): the bigger of the two.   // E28
    TouchGeometry framed = wide;   // E28
    framed.hudFrame = TouchInsets{59.8f, 26.9f, 59.8f, 0.0f};   // E28
    CHECK_NEAR(TouchEditor::ComputeLayout(framed).titleRight, 1365.5f - 59.8f - 140.0f);   // E28
    framed.safeArea = TouchInsets{88.0f, 0.0f, 88.0f, 0.0f};   // E28
    CHECK_NEAR(TouchEditor::ComputeLayout(framed).titleRight, 1365.5f - 88.0f - 140.0f);   // E28

    // Every widget inside the safe area and none over another, on the shapes a screen comes in.   // E28
    const glm::vec2 areas[3][2] = {{glm::vec2(0.0f), glm::vec2(1024.0f, 768.0f)},   // E28
                                   {glm::vec2(-171.0f, 0.0f), glm::vec2(1195.0f, 768.0f)},   // E28
                                   {glm::vec2(-341.5f, 0.0f), glm::vec2(1365.5f, 768.0f)}};   // E28
    const TouchInsets insets[3] = {TouchInsets{}, TouchInsets{88.0f, 0.0f, 88.0f, 24.0f},   // E28
                                   TouchInsets{0.0f, 30.0f, 0.0f, 20.0f}};   // E28
    for (const auto& area : areas) {   // E28
        for (const TouchInsets& inset : insets) {   // E28
            TouchGeometry g;   // E28
            g.areaMin = area[0];   // E28
            g.areaMax = area[1];   // E28
            g.safeArea = inset;   // E28
            const TouchEditor::Layout layout = TouchEditor::ComputeLayout(g);   // E28
            const glm::vec2 lo = area[0] + glm::vec2(inset.left, inset.top);   // E28
            const glm::vec2 hi = area[1] - glm::vec2(inset.right, inset.bottom);   // E28
            for (int a = 0; a < kTouchEditWidgetCount; ++a) {   // E28
                const TouchLayout::Box& box = layout.widget[static_cast<std::size_t>(a)];   // E28
                CHECK(box.min.x >= lo.x && box.min.y >= lo.y && box.max.x <= hi.x && box.max.y <= hi.y);   // E28
                for (int b = a + 1; b < kTouchEditWidgetCount; ++b) {   // E28
                    CHECK(!BoxesOverlap(box, layout.widget[static_cast<std::size_t>(b)]));   // E28
                }   // E28
            }   // E28
            CHECK_NEAR(layout.centreX, 512.0f);   // E28
        }   // E28
    }   // E28
}   // E28

// Open: locked, the tuning in force, the entry's finger dead, a held close key not a close.   // E28
void testEditorOpen() {   // E28
    TouchEditor idle;   // E28
    CHECK(!idle.IsOpen());   // E28
    CHECK(idle.Locked());   // E28
    CHECK(idle.Update(TouchEditInput{}, TouchControls{}).changed == false);   // inert: nothing open, nothing to answer   // E28

    const TouchTuning tuning = Tuned();   // E28
    EditRig rig;   // E28
    rig.Open(tuning);   // E28
    CHECK(rig.editor.IsOpen());   // E28
    CHECK(rig.editor.Locked());   // E28
    CHECK(rig.editor.Tuning() == tuning);   // E28
    EditRig unlocked;   // E28
    unlocked.Open(tuning, {}, true);   // E28
    CHECK(unlocked.editor.IsOpen() && !unlocked.editor.Locked());   // E28

    // The finger that tapped the entry is still down, here on the lock tile: lifting it activates nothing...   // E28
    EditRig dead;   // E28
    const glm::vec2 lock = dead.Where(TouchEditWidget::Lock);   // E28
    dead.Open(TouchTuning{}, {Finger(7, lock)});   // E28
    dead.Tick({Finger(7, lock)});   // E28
    dead.Tick({Finger(7, lock)});   // E28
    const TouchEditStep lifted = dead.Tick();   // E28
    CHECK(dead.editor.Locked());   // E28
    CHECK(!lifted.changed && !lifted.commit && !lifted.closed);   // E28
    // ...and the next finger works.   // E28
    dead.Tap(TouchEditWidget::Lock, 8);   // E28
    CHECK(!dead.editor.Locked());   // E28
    // A finger down at Open does not grab a control either (unlocked, on the jump button).   // E28
    EditRig grabless;   // E28
    const glm::vec2 jump = grabless.Where(TouchControl::Jump);   // E28
    grabless.Open(TouchTuning{}, {Finger(3, jump)}, true);   // E28
    grabless.Drag(3, jump, jump + glm::vec2(-40.0f, 12.0f), 3);   // E28
    grabless.Tick();   // E28
    CHECK(grabless.editor.Tuning().move[Index(TouchControl::Jump)].IsZero());   // E28
    CHECK_EQ(grabless.changes, 0);   // E28
    CHECK_EQ(grabless.commits, 0);   // E28

    // Esc held when it opens is not a close; only a press after a release is.   // E28
    EditRig held;   // E28
    held.Open();   // E28
    CHECK(!held.Tick({}, true).closed);   // E28
    CHECK(!held.Tick({}, true).closed);   // E28
    CHECK(held.editor.IsOpen());   // E28
    held.Tick({}, false);   // E28
    const TouchEditStep edge = held.Tick({}, true);   // E28
    CHECK(edge.closed);   // E28
    CHECK(!held.editor.IsOpen());   // E28

    // A finger list that is missing, ids that are not fingers and fingers that are up: nothing.   // E28
    EditRig odd;   // E28
    odd.Open();   // E28
    TouchEditInput nobody;   // E28
    nobody.geometry = odd.Geometry();   // E28
    const TouchEditStep none = odd.editor.Update(nobody, odd.controls);   // E28
    CHECK(!none.changed && !none.commit && !none.closed);   // E28
    TouchContact up = Finger(5, odd.Where(TouchEditWidget::Lock));   // E28
    up.down = false;   // E28
    const std::vector<TouchContact> noise = {up, Finger(-1, odd.Where(TouchEditWidget::Lock))};   // E28
    odd.Tick(noise);   // E28
    odd.Tick();   // E28
    CHECK(odd.editor.Locked());   // E28
}   // E28

// Drag: a control follows the finger by the offset it was taken at, one finger per control.   // E28
void testEditorDrag() {   // E28
    EditRig rig;   // E28
    rig.Open(TouchTuning{}, {}, true);   // E28
    CHECK(!rig.editor.Locked());   // E28
    CHECK(rig.controls.Visible(TouchControl::Jump));   // E28
    const glm::vec2 jump = rig.Where(TouchControl::Jump);   // E28
    CHECK(AtPoint(jump, 924.0f, 644.0f));   // 4:3, size 1: the box (864,584)-(984,704)   // E28, E33: the staggered columns
    // E33: the editor shows the shipped arrangement: from the bottom, left the sword, its combo and the light, right the jump (30 higher than   // E33
    // the sword), the spell combo and the fire, the combos centred over their columns and 30 apart in height as the columns are.   // E33
    CHECK(AtPoint(rig.Where(TouchControl::Sword), 788.0f, 674.0f));   // E33
    CHECK(AtPoint(rig.Where(TouchControl::SwordCombo), 788.0f, 548.0f));   // E33
    CHECK(AtPoint(rig.Where(TouchControl::Light), 788.0f, 422.0f));   // E33
    CHECK(AtPoint(rig.Where(TouchControl::SpellCombo), 924.0f, 518.0f));   // E33
    CHECK(AtPoint(rig.Where(TouchControl::Fire), 924.0f, 392.0f));   // E33
    const glm::vec2 target = jump + glm::vec2(-40.0f, 12.0f);   // E28

    CHECK(!rig.Tick({Finger(1, jump)}).changed);   // landing moves nothing   // E28
    int changedTicks = 0;   // E28
    for (int i = 1; i <= 3; ++i) {   // E28
        const glm::vec2 at = jump + (target - jump) * (static_cast<float>(i) / 3.0f);   // E28
        const TouchEditStep step = rig.Tick({Finger(1, at)});   // E28
        changedTicks += step.changed ? 1 : 0;   // E28
        CHECK(!step.commit);   // never per drag tick   // E28
    }   // E28
    CHECK_EQ(changedTicks, 3);   // E28
    CHECK_EQ(rig.commits, 0);   // E28
    CHECK_NEAR(rig.editor.Tuning().move[Index(TouchControl::Jump)].x, -40.0f);   // E28
    CHECK_NEAR(rig.editor.Tuning().move[Index(TouchControl::Jump)].y, 12.0f);   // E28
    // The controls took it at once: the box is where the finger left it.   // E28
    CHECK(AtPoint(rig.Where(TouchControl::Jump), 884.0f, 656.0f, 0.2f));   // E28, E33
    // A finger standing still changes nothing.   // E28
    CHECK(!rig.Tick({Finger(1, target)}).changed);   // E28
    const TouchEditStep lift = rig.Tick();   // E28
    CHECK(lift.commit && !lift.changed);   // E28
    CHECK_EQ(rig.commits, 1);   // E28
    // Nothing else moved.   // E28
    for (int i = 0; i < Penumbra::Render::kTuningControls; ++i) {   // E28
        if (i != static_cast<int>(TouchControl::Jump)) CHECK(rig.editor.Tuning().move[static_cast<std::size_t>(i)].IsZero());   // E28
    }   // E28

    // The control kept its offset from the finger: grabbed away from its centre, it still does not jump to it.   // E28
    EditRig offset;   // E28
    offset.Open(TouchTuning{}, {}, true);   // E28
    const TouchLayout::Box jumpBox = offset.controls.Layout()[TouchControl::Jump];   // E28
    const glm::vec2 corner = jumpBox.min + glm::vec2(10.0f, 10.0f);   // E28
    offset.Drag(1, corner, corner + glm::vec2(-30.0f, -20.0f), 2);   // E28
    CHECK(AtPoint(offset.controls.Layout()[TouchControl::Jump].min, jumpBox.min.x - 30.0f, jumpBox.min.y - 20.0f, 0.2f));   // E28
    offset.Tick();   // E28

    // Two fingers move two controls; a third on a held control is ignored; one on nothing does nothing.   // E28
    EditRig two;   // E28
    two.Open(TouchTuning{}, {}, true);   // E28
    const glm::vec2 j = two.Where(TouchControl::Jump);   // E28
    const glm::vec2 s = two.Where(TouchControl::Sword);   // E28
    two.Tick({Finger(1, j), Finger(2, s)});   // E28
    for (int i = 1; i <= 3; ++i) {   // E28
        const float f = static_cast<float>(i);   // E28
        two.Tick({Finger(1, j + glm::vec2(-10.0f, 4.0f) * f), Finger(2, s + glm::vec2(6.0f, -8.0f) * f)});   // E28
    }   // E28
    CHECK_NEAR(two.editor.Tuning().move[Index(TouchControl::Jump)].x, -30.0f);   // E28
    CHECK_NEAR(two.editor.Tuning().move[Index(TouchControl::Jump)].y, 12.0f);   // E28
    CHECK_NEAR(two.editor.Tuning().move[Index(TouchControl::Sword)].x, 18.0f);   // E28
    CHECK_NEAR(two.editor.Tuning().move[Index(TouchControl::Sword)].y, -24.0f);   // E28
    const glm::vec2 heldJump = j + glm::vec2(-30.0f, 12.0f);   // E28
    const TouchTuning before = two.editor.Tuning();   // E28
    two.Tick({Finger(1, heldJump), Finger(2, s + glm::vec2(18.0f, -24.0f)), Finger(3, heldJump)});   // E28
    two.Tick({Finger(1, heldJump), Finger(2, s + glm::vec2(18.0f, -24.0f)), Finger(3, heldJump + glm::vec2(25.0f, 25.0f))});   // E28
    CHECK(two.editor.Tuning() == before);   // E28
    two.Tick({Finger(1, heldJump), Finger(2, s + glm::vec2(18.0f, -24.0f)), Finger(4, glm::vec2(200.0f, 400.0f))});   // E28
    two.Tick({Finger(1, heldJump), Finger(2, s + glm::vec2(18.0f, -24.0f)), Finger(4, glm::vec2(260.0f, 380.0f))});   // E28
    CHECK(two.editor.Tuning() == before);   // E28
    // Each lift is a gesture's end: the first one saves what was moved (the other finger's place too), the second has   // E28
    // nothing left to save.   // E28
    const int commitsBefore = two.commits;   // E28
    CHECK(two.Tick({Finger(2, s + glm::vec2(18.0f, -24.0f))}).commit);   // E28
    CHECK(!two.Tick().commit);   // E28
    CHECK_EQ(two.commits, commitsBefore + 1);   // E28

    // A widget wins over the control behind it: a tile over the jump button is a tile, and a finger on it moves nothing.   // E28
    TouchManifest behind = TouchControls::DefaultManifest();   // E28
    behind[TouchControl::Jump].anchor = TouchAnchor::TopLeft;   // E28
    behind[TouchControl::Jump].offset = glm::vec2(400.0f, 340.0f);   // E28
    behind[TouchControl::Jump].overhang = glm::vec2(0.0f);   // E28
    EditRig tile(behind);   // E28
    tile.Open(TouchTuning{}, {}, true);   // E28
    const glm::vec2 lockCentre = tile.Where(TouchEditWidget::Lock);   // E28
    CHECK(BoxesOverlap(tile.controls.Layout()[TouchControl::Jump], tile.Widget(TouchEditWidget::Lock)));   // E28
    tile.Drag(1, lockCentre, lockCentre + glm::vec2(100.0f, 0.0f), 3);   // slides off the tile: a cancelled tap   // E28
    tile.Tick();   // E28
    CHECK(tile.editor.Tuning().move[Index(TouchControl::Jump)].IsZero());   // E28
    CHECK(!tile.editor.Locked());   // E28
    // The part of the button that is not under the tile still takes it.   // E28
    const glm::vec2 below(410.0f, 455.0f);   // E28
    CHECK(tile.controls.Layout()[TouchControl::Jump].max.y > below.y);   // E28
    CHECK(tile.Drag(2, below, below + glm::vec2(0.0f, 30.0f), 2) > 0);   // E28
    tile.Tick();   // E28
    CHECK(!tile.editor.Tuning().move[Index(TouchControl::Jump)].IsZero());   // E28

    // A control let go with its whole grab area under a tile could never be taken again, so it goes back to   // E28
    // where it was when the finger took it - and nothing is saved, the tuning being what it was.   // E28
    TouchManifest tiny = TouchControls::DefaultManifest();   // E28
    tiny[TouchControl::Jump].anchor = TouchAnchor::TopLeft;   // E28
    tiny[TouchControl::Jump].offset = glm::vec2(700.0f, 500.0f);   // E28
    tiny[TouchControl::Jump].overhang = glm::vec2(0.0f);   // E28
    tiny[TouchControl::Jump].size = glm::vec2(48.0f);   // E28
    EditRig buried(tiny);   // E28
    buried.Open(TouchTuning{}, {}, true);   // E28
    const glm::vec2 tinyCentre = buried.Where(TouchControl::Jump);   // E28
    CHECK(AtPoint(tinyCentre, 724.0f, 524.0f));   // E28
    const TouchLayout::Box more = buried.Widget(TouchEditWidget::SizeMore);   // E28
    buried.Drag(1, tinyCentre, more.Centre(), 6);   // E28
    CHECK(BoxesOverlap(buried.controls.Layout()[TouchControl::Jump], more));   // it followed the finger onto the tile   // E28
    const TouchEditStep drop = buried.Tick();   // E28
    CHECK(drop.changed);   // E28
    CHECK(!drop.commit);   // E28
    CHECK(buried.editor.Tuning().move[Index(TouchControl::Jump)].IsZero());   // E28
    CHECK(AtPoint(buried.controls.Layout()[TouchControl::Jump].Centre(), 724.0f, 524.0f));   // E28
    // Partly under the tile it stays where it was dropped, and is saved.   // E28
    buried.Drag(2, tinyCentre, glm::vec2(680.0f, 168.0f), 6);   // E28
    const TouchEditStep kept = buried.Tick();   // E28
    CHECK(!kept.changed);   // E28
    CHECK(kept.commit);   // E28
    CHECK(!buried.editor.Tuning().move[Index(TouchControl::Jump)].IsZero());   // E28

    // A grab that merely lands does not rewrite a stored move the layout had clamped (a file from a larger screen): the
    // jump button is held where it is drawn, and nothing is saved by touching it.
    TouchTuning farAway;   // E28
    farAway.move[Index(TouchControl::Jump)] = TouchMove{900.0f, 900.0f};   // E28
    EditRig clamped;   // E28
    clamped.Open(farAway, {}, true);   // E28
    const glm::vec2 stuck = clamped.Where(TouchControl::Jump);   // E28
    CHECK(stuck.x < 1024.0f && stuck.y < 768.0f);   // E28
    clamped.Tick({Finger(1, stuck)});   // E28
    clamped.Tick({Finger(1, stuck)});   // E28
    clamped.Tick();   // E28
    CHECK(clamped.editor.Tuning() == farAway);   // E28
    CHECK_EQ(clamped.changes, 0);   // E28
    CHECK_EQ(clamped.commits, 0);   // E28
}   // E28

// Locked: a drag does nothing, and nothing the editor shows is a key.   // E28
void testEditorLocked() {   // E28
    EditRig rig;   // E28
    rig.Open();   // E28
    CHECK(rig.editor.Locked());   // E28
    CHECK(rig.controls.Visible(TouchControl::Jump));   // E28
    CHECK(rig.controls.Visible(TouchControl::Pause));   // E28
    const glm::vec2 jump = rig.Where(TouchControl::Jump);   // E28
    const TouchTuning before = rig.editor.Tuning();   // E28
    CHECK_EQ(rig.Drag(1, jump, jump + glm::vec2(-40.0f, 12.0f), 3), 0);   // E28
    const TouchEditStep lift = rig.Tick();   // E28
    CHECK(rig.editor.Tuning() == before);   // E28
    CHECK(!lift.changed && !lift.commit);   // E28
    CHECK_EQ(rig.changes, 0);   // E28
    CHECK_EQ(rig.commits, 0);   // E28

    // The Pause control is a thing to drag here, not a key: the finger on it presses nothing.   // E28
    rig.Tick({Finger(2, rig.Where(TouchControl::Pause))});   // E28
    for (const bool held : rig.touch.held) CHECK(!held);   // E28
    CHECK(!rig.touch.pointer);   // E28
    CHECK(rig.touch.touching);   // E28
    rig.Tick();   // E28

    // Unlocked by a tap, the same drag moves it; locking again, with a second finger, lets go of it.   // E28
    rig.Tap(TouchEditWidget::Lock);   // E28
    CHECK(!rig.editor.Locked());   // E28
    rig.Tick({Finger(1, jump)});   // E28
    CHECK(rig.Tick({Finger(1, jump + glm::vec2(-10.0f, 3.0f))}).changed);   // E28
    rig.Tick({Finger(1, jump + glm::vec2(-10.0f, 3.0f)), Finger(2, rig.Where(TouchEditWidget::Lock))});   // E28
    const TouchEditStep locked = rig.Tick({Finger(1, jump + glm::vec2(-10.0f, 3.0f))});   // E28
    CHECK(rig.editor.Locked());   // E28
    CHECK(locked.commit && !locked.changed);   // locking saves what was moved   // E28
    const TouchTuning kept = rig.editor.Tuning();   // E28
    CHECK(!rig.Tick({Finger(1, jump + glm::vec2(-40.0f, 12.0f))}).changed);   // E28
    CHECK(rig.editor.Tuning() == kept);   // E28
    CHECK(!rig.Tick().commit);   // and the lift has nothing left to save   // E28
}   // E28

// SizeLess / SizeMore / OpacityLess / OpacityMore.   // E28
void testEditorSteps() {   // E28
    const auto size = [](const EditRig& rig) { return rig.editor.Tuning().size; };   // E28
    const auto opacity = [](const EditRig& rig) { return rig.editor.Tuning().opacity; };   // E28

    EditRig rig;   // E28
    rig.Open();   // E28
    TouchEditStep step = rig.Tap(TouchEditWidget::SizeMore);   // E28
    CHECK(size(rig) == 1.1f);   // E28
    CHECK(step.changed && step.commit);   // E28
    step = rig.Tap(TouchEditWidget::SizeMore);   // E28
    CHECK(size(rig) == 1.2f);   // E28
    CHECK(step.changed && step.commit);   // E28
    step = rig.Tap(TouchEditWidget::SizeMore);   // E28
    CHECK(size(rig) == 1.3f);   // E28
    step = rig.Tap(TouchEditWidget::SizeMore);   // E28
    CHECK(size(rig) == 1.4f);   // Magic Rampage's own ceiling   // E28
    CHECK(step.changed && step.commit);   // E28
    CHECK(size(rig) == TouchTuning::kMaxSize);   // E28
    step = rig.Tap(TouchEditWidget::SizeMore);   // at the top: the tap does nothing   // E28
    CHECK(size(rig) == 1.4f);   // E28
    CHECK(!step.changed && !step.commit);   // E28
    CHECK_EQ(rig.commits, 4);   // E28
    for (int i = 0; i < 10; ++i) rig.Tap(TouchEditWidget::SizeLess);   // E28
    CHECK(size(rig) == 0.4f);   // E28
    step = rig.Tap(TouchEditWidget::SizeLess);   // E28
    CHECK(size(rig) == 0.4f);   // E28
    CHECK(!step.changed && !step.commit);   // E28

    EditRig down;   // E28
    down.Open();   // E28
    for (int i = 0; i < 6; ++i) CHECK(down.Tap(TouchEditWidget::SizeLess).changed);   // E28
    CHECK(size(down) == 0.4f);   // exactly, six tenths down from 1.0   // E28
    CHECK(!down.Tap(TouchEditWidget::SizeLess).changed);   // E28
    CHECK(size(down) == 0.4f);   // E28

    // Opacity: a fifth a tap, 0.2 to 1.8.   // E28
    EditRig fade;   // E28
    fade.Open();   // E28
    const float up[] = {1.2f, 1.4f, 1.6f, 1.8f};   // E28
    for (const float want : up) {   // E28
        step = fade.Tap(TouchEditWidget::OpacityMore);   // E28
        CHECK(opacity(fade) == want);   // E28
        CHECK(step.changed && step.commit);   // E28
    }   // E28
    step = fade.Tap(TouchEditWidget::OpacityMore);   // E28
    CHECK(opacity(fade) == 1.8f);   // E28
    CHECK(!step.changed && !step.commit);   // E28
    EditRig dim;   // E28
    dim.Open();   // E28
    const float low[] = {0.8f, 0.6f, 0.4f, 0.2f};   // E28
    for (const float want : low) {   // E28
        step = dim.Tap(TouchEditWidget::OpacityLess);   // E28
        CHECK(opacity(dim) == want);   // E28
        CHECK(step.changed && step.commit);   // E28
    }   // E28
    CHECK(!dim.Tap(TouchEditWidget::OpacityLess).changed);   // E28
    CHECK(opacity(dim) == 0.2f);   // E28
    CHECK(size(dim) == 1.0f);   // each only its own   // E28

    // A size step locks again, an opacity step does not.   // E28
    EditRig lock;   // E28
    lock.Open({}, {}, true);   // E28
    lock.Tap(TouchEditWidget::OpacityMore);   // E28
    CHECK(!lock.editor.Locked());   // E28
    lock.Tap(TouchEditWidget::SizeLess);   // E28
    CHECK(lock.editor.Locked());   // E28
    lock.Tap(TouchEditWidget::Lock);   // E28
    CHECK(!lock.editor.Locked());   // E28
    lock.Tap(TouchEditWidget::SizeLess, 4);   // E28
    CHECK(lock.editor.Locked());   // E28
    // A step at a limit does not lock (nothing was stepped).   // E28
    EditRig limit;   // E28
    TouchTuning smallest;   // E28
    smallest.size = 0.4f;   // E28
    limit.Open(smallest, {}, true);   // E28
    limit.Tap(TouchEditWidget::SizeLess);   // E28
    CHECK(!limit.editor.Locked());   // E28

    // A step lets go of a held control; the finger that held it does nothing more.   // E28
    EditRig held;   // E28
    held.Open({}, {}, true);   // E28
    const glm::vec2 jump = held.Where(TouchControl::Jump);   // E28
    held.Tick({Finger(1, jump)});   // E28
    CHECK(held.Tick({Finger(1, jump + glm::vec2(-10.0f, 3.0f))}).changed);   // E28
    held.Tick({Finger(1, jump + glm::vec2(-10.0f, 3.0f)), Finger(2, held.Where(TouchEditWidget::SizeLess))});   // E28
    held.Tick({Finger(1, jump + glm::vec2(-10.0f, 3.0f))});   // E28
    CHECK(held.editor.Tuning().size == 0.9f);   // E28
    CHECK(held.editor.Locked());   // E28
    const TouchMove placed = held.editor.Tuning().move[Index(TouchControl::Jump)];   // E28
    CHECK(!held.Tick({Finger(1, jump + glm::vec2(-50.0f, 20.0f))}).changed);   // E28
    CHECK(held.editor.Tuning().move[Index(TouchControl::Jump)] == placed);   // E28

    // A tap shorter than a tick (down in one, gone the next) is a tap; one that slides off before it lifts is not.   // E28
    EditRig quick;   // E28
    quick.Open();   // E28
    quick.Tick({Finger(1, quick.Where(TouchEditWidget::SizeMore))});   // E28
    CHECK(quick.Tick().changed);   // E28
    CHECK(size(quick) == 1.1f);   // E28
    EditRig slide;   // E28
    slide.Open();   // E28
    const glm::vec2 tile = slide.Where(TouchEditWidget::SizeMore);   // E28
    slide.Tick({Finger(1, tile)});   // E28
    slide.Tick({Finger(1, tile + glm::vec2(0.0f, 200.0f))});   // E28
    CHECK(!slide.Tick().changed);   // E28
    CHECK(size(slide) == 1.0f);   // E28
    // Sliding onto a tile from outside presses nothing either.   // E28
    slide.Tick({Finger(2, tile + glm::vec2(0.0f, 200.0f))});   // E28
    slide.Tick({Finger(2, tile)});   // E28
    CHECK(!slide.Tick().changed);   // E28
    // The tile's edge is outside, as a Switch's.   // E28
    slide.Tick({Finger(3, glm::vec2(slide.Widget(TouchEditWidget::SizeMore).min.x, tile.y))});   // E28
    CHECK(!slide.Tick().changed);   // E28
    CHECK(size(slide) == 1.0f);   // E28
}   // E28

// Lock and Restore.   // E28
void testEditorRestore() {   // E28
    const TouchTuning tuning = Tuned();   // E28
    EditRig rig;   // E28
    rig.Open(tuning, {}, true);   // E28
    CHECK(Alpha(WidgetCmd(rig.Overlay(), TouchEditWidget::Restore).color) == 255);   // something to restore   // E28
    const TouchEditStep step = rig.Tap(TouchEditWidget::Restore);   // E28
    CHECK(step.changed && step.commit);   // E28
    CHECK(rig.editor.Locked());   // E28
    CHECK(!rig.editor.Tuning().Moved());   // E28
    CHECK(rig.editor.Tuning().size == 0.8f);       // size and opacity stay   // E28
    CHECK(rig.editor.Tuning().opacity == 0.6f);   // E28
    CHECK(Alpha(WidgetCmd(rig.Overlay(), TouchEditWidget::Restore).color) == 70);   // E28
    CHECK_EQ(rig.commits, 1);   // E28

    // With nothing moved it only locks.   // E28
    EditRig bare;   // E28
    bare.Open({}, {}, true);   // E28
    const TouchEditStep none = bare.Tap(TouchEditWidget::Restore);   // E28
    CHECK(bare.editor.Locked());   // E28
    CHECK(!none.changed && !none.commit);   // E28
    EditRig tuned;   // E28
    TouchTuning sized;   // E28
    sized.size = 1.2f;   // E28
    tuned.Open(sized, {}, true);   // E28
    const TouchEditStep noMoves = tuned.Tap(TouchEditWidget::Restore);   // E28
    CHECK(tuned.editor.Locked() && !noMoves.changed && !noMoves.commit);   // E28
    CHECK(tuned.editor.Tuning().size == 1.2f);   // E28

    // The lock tile toggles; locking saves only what is unsaved.   // E28
    EditRig toggle;   // E28
    toggle.Open();   // E28
    CHECK(!toggle.Tap(TouchEditWidget::Lock).commit);   // E28
    CHECK(!toggle.editor.Locked());   // E28
    const TouchEditStep again = toggle.Tap(TouchEditWidget::Lock);   // E28
    CHECK(toggle.editor.Locked());   // E28
    CHECK(!again.changed && !again.commit);   // E28
}   // E28

// Close: the arrow, the key's edge, the unsaved drag.   // E28
void testEditorClose() {   // E28
    EditRig arrow;   // E28
    arrow.Open();   // E28
    const TouchEditStep closed = arrow.Tap(TouchEditWidget::Back);   // E28
    CHECK(closed.closed);   // E28
    CHECK(!closed.changed && !closed.commit);   // E28
    CHECK(!arrow.editor.IsOpen());   // E28
    // Inert after: nothing is answered, nothing is drawn, and the tuning is kept.   // E28
    CHECK(arrow.Overlay().empty());   // E28
    const TouchEditStep inert = arrow.Tap(TouchEditWidget::SizeMore);   // E28
    CHECK(!inert.changed && !inert.commit && !inert.closed);   // E28
    CHECK(arrow.editor.Tuning().size == 1.0f);   // E28

    // A finger that slides off the arrow does not close it.   // E28
    EditRig slide;   // E28
    slide.Open();   // E28
    const glm::vec2 back = slide.Where(TouchEditWidget::Back);   // E28
    slide.Tick({Finger(1, back)});   // E28
    slide.Tick({Finger(1, back + glm::vec2(300.0f, 0.0f))});   // E28
    CHECK(!slide.Tick().closed);   // E28
    CHECK(slide.editor.IsOpen());   // E28

    // The key: one tick down is a close; held and pressed again after a release is another.   // E28
    EditRig key;   // E28
    key.Open();   // E28
    key.Tick();   // E28
    CHECK(key.Tick({}, true).closed);   // E28
    CHECK(!key.editor.IsOpen());   // E28
    EditRig again;   // E28
    again.Open();   // E28
    again.Tick({}, true);   // held at the open   // E28
    again.Tick({}, true);   // E28
    CHECK(again.editor.IsOpen());   // E28
    again.Tick();   // E28
    CHECK(again.Tick({}, true).closed);   // E28

    // Closing in the middle of a drag keeps what was moved, in the same step.   // E28
    EditRig drag;   // E28
    drag.Open({}, {}, true);   // E28
    const glm::vec2 jump = drag.Where(TouchControl::Jump);   // E28
    drag.Tick();   // E28
    drag.Tick({Finger(1, jump)});   // E28
    CHECK(drag.Tick({Finger(1, jump + glm::vec2(-20.0f, 6.0f))}).changed);   // E28
    CHECK_EQ(drag.commits, 0);   // E28
    const TouchEditStep mid = drag.Tick({Finger(1, jump + glm::vec2(-20.0f, 6.0f))}, true);   // E28
    CHECK(mid.closed && mid.commit);   // E28
    CHECK(!drag.editor.IsOpen());   // E28
    CHECK_NEAR(drag.editor.Tuning().move[Index(TouchControl::Jump)].x, -20.0f);   // E28
    CHECK_NEAR(drag.editor.Tuning().move[Index(TouchControl::Jump)].y, 6.0f);   // E28
    // The finger lifting afterwards finds the editor shut.   // E28
    const TouchEditStep late = drag.Tick();   // E28
    CHECK(!late.commit && !late.closed);   // E28

    // Re-opened, it starts clean: locked, the tuning given, no finger from before.   // E28
    drag.Open(TouchTuning{});   // E28
    CHECK(drag.editor.IsOpen() && drag.editor.Locked());   // E28
    CHECK(!drag.editor.Tuning().Moved());   // E28
}   // E28

// What the tail of the overlay holds, at 4:3 at the default tuning.   // E28
void testEditorOverlay() {   // E28
    EditRig rig;   // E28
    CHECK(rig.Overlay().empty());   // E28
    rig.Open();   // E28
    // Before the editor's first Update it draws on the default geometry, the 4:3 screen: the backdrop is there.   // E28
    const std::vector<HudCmd> first = rig.Overlay();   // E28
    CHECK(!first.empty());   // E28
    if (!first.empty()) CHECK(first[0].size == glm::vec2(1024.0f, 768.0f));   // E28
    rig.Tick();   // E28

    const std::size_t controlCommands = rig.ControlCommands();   // E28
    const std::vector<HudCmd> locked = rig.Overlay();   // E28
    CHECK_EQ(locked.size(), 1 + controlCommands + kEditWidgets + kEditTexts);   // no outlines while locked   // E28
    if (locked.size() < 1 + kEditWidgets + kEditTexts) return;   // E28

    // The backdrop first: a gradient over the whole shown area, near opaque (the options text must not show through).   // E28
    CHECK(locked[0].kind == HudCmd::Kind::Rectangle);   // E28
    CHECK(locked[0].pos == glm::vec2(0.0f));   // E28
    CHECK(locked[0].size == glm::vec2(1024.0f, 768.0f));   // E28
    CHECK(locked[0].color == locked[0].color1 && locked[0].color2 == locked[0].color3);   // E28
    CHECK(locked[0].color != locked[0].color2);   // E28
    CHECK(Alpha(locked[0].color) > 230 && Alpha(locked[0].color2) > 230);   // E28
    CHECK(locked[0].stretchToSides);   // it spans the whole area, as a fade does   // E28
    // The controls' own commands next, as they would draw alone.   // E28
    std::vector<HudCmd> alone;   // E28
    rig.controls.AppendOverlay(alone);   // E28
    for (std::size_t i = 0; i < alone.size() && 1 + i < locked.size(); ++i) {   // E28
        const HudCmd& a = alone[i];   // E28
        const HudCmd& b = locked[1 + i];   // E28
        CHECK(a.kind == b.kind && a.pos == b.pos && a.size == b.size && a.color == b.color && a.sprite == b.sprite);   // E28
    }   // E28

    // The words: the title, then each readout's "-", number and "+".   // E28
    const TouchEditor::Layout layout = TouchEditor::ComputeLayout(TouchGeometry{});   // E28
    for (std::size_t i = 0; i < kEditTexts; ++i) CHECK(TextCmd(locked, i).kind == HudCmd::Kind::Text);   // E28
    const HudCmd& titleShadow = TextCmd(locked, 0);   // E28
    const HudCmd& title = TextCmd(locked, 1);   // E28
    CHECK(title.text == TouchEditor::kTitle && titleShadow.text == TouchEditor::kTitle);   // E28
    CHECK(title.font == "Arial Narrow");   // E28
    CHECK_NEAR(title.fontSize, 40.0f);   // E28
    CHECK(title.pos == layout.title);   // E28
    CHECK_NEAR(title.rtlRight, layout.titleRight);   // E28
    CHECK_NEAR(titleShadow.rtlRight, layout.titleRight + 4.0f);   // E28
    CHECK_NEAR(titleShadow.pos.x, title.pos.x + 4.0f);   // E28
    CHECK_NEAR(titleShadow.pos.y, title.pos.y + 4.0f);   // E28
    CHECK((title.color & 0x00FFFFFFu) == 0x00CBCBE4u);   // E28
    CHECK((titleShadow.color & 0x00FFFFFFu) == 0u);   // E28
    CHECK_EQ(static_cast<int>(Alpha(titleShadow.color)), Alpha(title.color) / 2);   // E28
    const char* const row[2][3] = {{"-", "1.0", "+"}, {"-", "1.0", "+"}};   // E28
    for (std::size_t r = 0; r < 2; ++r) {   // E28
        for (std::size_t c = 0; c < 3; ++c) {   // E28
            const HudCmd& shadow = TextCmd(locked, 2 + r * 6 + c * 2);   // E28
            const HudCmd& front = TextCmd(locked, 3 + r * 6 + c * 2);   // E28
            CHECK(front.text == row[r][c] && shadow.text == row[r][c]);   // E28
            // Digits and signs are not words: no right edge, shadow included (0 means none).   // E28
            CHECK(front.rtlRight == 0.0f && shadow.rtlRight == 0.0f);   // E28
            CHECK(front.font == "Arial Narrow");   // E28
            CHECK_NEAR(front.fontSize, 40.0f);   // E28
            CHECK((front.color & 0x00FFFFFFu) == 0x00CBCBE4u);   // E28
            CHECK_NEAR(shadow.pos.x, front.pos.x + 4.0f);   // E28
            // The number in full, the signs faint.   // E28
            if (c == 1) CHECK_EQ(static_cast<int>(Alpha(front.color)), 255);   // E28
            else CHECK_EQ(static_cast<int>(Alpha(front.color)), 90);   // E28
            const float rowY = layout.rowY[r];   // E28
            CHECK_NEAR(front.pos.y, rowY - 20.0f);   // E28
            const float expectX = c == 0 ? 512.0f - 54.0f : (c == 1 ? 512.0f - 23.0f : 512.0f + 38.0f);   // E28
            CHECK_NEAR(front.pos.x, expectX);   // E28
        }   // E28
    }   // E28

    // Without art the widgets are plain squares that stay where they are drawn, in order, in the tiles' boxes.   // E28
    for (int w = 0; w < kTouchEditWidgetCount; ++w) {   // E28
        const auto widget = static_cast<TouchEditWidget>(w);   // E28
        const HudCmd& cmd = WidgetCmd(locked, widget);   // E28
        CHECK(cmd.kind == HudCmd::Kind::Rectangle);   // E28
        CHECK(!cmd.stretchToSides);   // E28
        CHECK(cmd.pos == rig.Widget(widget).min && cmd.size == rig.Widget(widget).Size());   // E28
    }   // E28
    // Restore is dim with nothing moved, the others whole (the size and opacity ones at their limits are tested apart).   // E28
    CHECK_EQ(static_cast<int>(Alpha(WidgetCmd(locked, TouchEditWidget::Restore).color)), 70);   // E28
    CHECK_EQ(static_cast<int>(Alpha(WidgetCmd(locked, TouchEditWidget::Back).color)), 255);   // E28
    CHECK_EQ(static_cast<int>(Alpha(WidgetCmd(locked, TouchEditWidget::Lock).color)), 255);   // E28

    // A held tile is drawn smaller, by 3 on every side, for as long as the finger is on it.   // E28
    rig.Tick({Finger(1, rig.Where(TouchEditWidget::Lock))});   // E28
    const std::vector<HudCmd> pressed = rig.Overlay();   // E28
    const TouchLayout::Box lockBox = rig.Widget(TouchEditWidget::Lock);   // E28
    CHECK(WidgetCmd(pressed, TouchEditWidget::Lock).pos == lockBox.min + glm::vec2(3.0f));   // E28
    CHECK(WidgetCmd(pressed, TouchEditWidget::Lock).size == lockBox.Size() - glm::vec2(6.0f));   // E28
    CHECK(WidgetCmd(pressed, TouchEditWidget::Back).size == rig.Widget(TouchEditWidget::Back).Size());   // E28
    rig.Tick({Finger(1, lockBox.Centre() + glm::vec2(0.0f, 400.0f))});   // slid off: not held any more   // E28
    CHECK(WidgetCmd(rig.Overlay(), TouchEditWidget::Lock).size == lockBox.Size());   // E28
    rig.Tick();   // E28
    CHECK(rig.editor.Locked());   // the tap that slid off did not toggle it   // E28

    // The numbers read as the tuning says, one decimal, whatever the value.   // E28
    for (int tenths = 4; tenths <= 14; ++tenths) {   // E28
        TouchTuning tuning;   // E28
        tuning.size = static_cast<float>(tenths) / 10.0f;   // E28
        tuning.opacity = static_cast<float>(tenths % 9 + 1) / 5.0f;   // E28
        EditRig readout;   // E28
        readout.Open(tuning);   // E28
        readout.Tick();   // E28
        const std::vector<HudCmd> out = readout.Overlay();   // E28
        CHECK(TextCmd(out, 5).text == std::to_string(tenths / 10) + "." + std::to_string(tenths % 10));   // E28
        const int fifths = tenths % 9 + 1;   // E28
        CHECK(TextCmd(out, 11).text == std::to_string(fifths * 2 / 10) + "." + std::to_string(fifths * 2 % 10));   // E28
    }   // E28

    // Unlocked: every visible movable control gets four outline rectangles, in TouchControl order, before the widgets.   // E28
    EditRig open;   // E28
    open.Open({}, {}, true);   // E28
    open.Tick();   // E28
    const std::vector<HudCmd> unlocked = open.Overlay();   // E28
    const std::size_t outlineCount = unlocked.size() - 1 - open.ControlCommands() - kEditWidgets - kEditTexts;   // E28
    CHECK_EQ(outlineCount, std::size_t{36});   // nine controls: the eight and the Pause (Back is hidden here)   // E28
    for (std::size_t i = 0; i < unlocked.size() && i < outlineCount; ++i) {   // E28
        const HudCmd& cmd = unlocked[1 + open.ControlCommands() + i];   // E28
        CHECK(cmd.kind == HudCmd::Kind::Rectangle);   // E28
        CHECK(!cmd.stretchToSides);   // a thin line must not be stretched across a wide menu   // E28
    }   // E28
    CHECK(!open.controls.Visible(TouchControl::Back));   // E28
    for (int i = 0; i < static_cast<int>(TouchControl::Count); ++i) {   // E28
        const auto control = static_cast<TouchControl>(i);   // E28
        if (control != TouchControl::Back) CHECK_MSG(open.controls.Visible(control), TouchControls::ControlId(control));   // E28
    }   // E28

    // The breathing: 110 + 70 sin(2 pi t / 48), t the Updates since Open; a drag makes the grabbed one solid.   // E28
    EditRig breath;   // E28
    breath.Open({}, {}, true);   // E28
    const std::size_t at = 1 + breath.ControlCommands();   // E28
    for (unsigned t = 0; t <= 100; ++t) {   // E28
        if (t > 0) breath.Tick();   // E28
        const unsigned expected = static_cast<unsigned>(std::lround(110.0 + 70.0 * std::sin(2.0 * 3.14159265358979323846 * (t % 48) / 48.0)));   // E28
        const std::vector<HudCmd> out = breath.Overlay();   // E28
        CHECK(out.size() > at);   // E28
        if (out.size() > at) CHECK_EQ(static_cast<unsigned>(Alpha(out[at].color)), expected);   // E28
        if (t == 0) CHECK_EQ(expected, 110u);   // E28
    }   // E28
    const glm::vec2 jump = breath.Where(TouchControl::Jump);   // E28
    breath.Tick({Finger(1, jump)});   // E28
    const std::vector<HudCmd> grabbed = breath.Overlay();   // E28
    int solid = 0;   // E28
    for (std::size_t i = at; i < at + 36; ++i) solid += Alpha(grabbed[i].color) == 255 ? 1 : 0;   // E28
    CHECK_EQ(solid, 4);   // only the held control's four, and thicker   // E28
    // The controls are the player's: their alpha is the user's, the editor's widgets are never dimmed with it.   // E28
    TouchTuning faint;   // E28
    faint.opacity = 0.2f;   // E28
    EditRig dim;   // E28
    dim.Open(faint);   // E28
    dim.Tick();   // E28
    const std::vector<HudCmd> dimmed = dim.Overlay();   // E28
    CHECK_EQ(static_cast<int>(Alpha(dimmed[1].color)), 23);   // 0.45 x 0.2 = 0.09   // E28
    CHECK_EQ(static_cast<int>(Alpha(WidgetCmd(dimmed, TouchEditWidget::Back).color)), 255);   // E28
    CHECK_EQ(static_cast<int>(Alpha(WidgetCmd(dimmed, TouchEditWidget::OpacityLess).color)), 90);   // at its limit   // E28
    CHECK_EQ(static_cast<int>(Alpha(WidgetCmd(dimmed, TouchEditWidget::OpacityMore).color)), 255);   // E28
    CHECK_EQ(static_cast<int>(Alpha(WidgetCmd(dimmed, TouchEditWidget::SizeLess).color)), 255);   // E28
    TouchTuning big;   // E28
    big.size = TouchTuning::kMaxSize;   // E28
    big.opacity = 1.8f;   // E28
    EditRig top;   // E28
    top.Open(big);   // E28
    top.Tick();   // E28
    const std::vector<HudCmd> highest = top.Overlay();   // E28
    CHECK_EQ(static_cast<int>(Alpha(WidgetCmd(highest, TouchEditWidget::SizeMore).color)), 90);   // E28
    CHECK_EQ(static_cast<int>(Alpha(WidgetCmd(highest, TouchEditWidget::OpacityMore).color)), 90);   // E28
    CHECK_EQ(static_cast<int>(Alpha(WidgetCmd(highest, TouchEditWidget::SizeLess).color)), 255);   // E28
    TouchTuning small;   // E28
    small.size = 0.4f;   // E28
    EditRig bottom;   // E28
    bottom.Open(small);   // E28
    bottom.Tick();   // E28
    CHECK_EQ(static_cast<int>(Alpha(WidgetCmd(bottom.Overlay(), TouchEditWidget::SizeLess).color)), 90);   // E28

    // The wide area: the backdrop covers all of it, not the 4:3 box.   // E28
    EditRig phone;   // E28
    phone.Wide();   // E28
    phone.Open();   // E28
    phone.Tick();   // E28
    const std::vector<HudCmd> wide = phone.Overlay();   // E28
    CHECK(!wide.empty());   // E28
    if (!wide.empty()) {   // E28
        CHECK(wide[0].pos == glm::vec2(-341.5f, 0.0f));   // E28
        CHECK(wide[0].size == glm::vec2(1707.0f, 768.0f));   // E28
    }   // E28
}   // E28

// The number pulses: 1.2 times as large after a step up, 0.8 after a step down, for six ticks.   // E28
void testEditorPulse() {   // E28
    EditRig rig;   // E28
    rig.Open();   // E28
    rig.Tick();   // E28
    CHECK_NEAR(TextCmd(rig.Overlay(), 5).fontSize, 40.0f);   // E28
    rig.Tap(TouchEditWidget::SizeMore);   // E28
    // The step's own tick and the five after it.   // E28
    for (int t = 0; t < 6; ++t) {   // E28
        const std::vector<HudCmd> out = rig.Overlay();   // E28
        CHECK_NEAR(TextCmd(out, 5).fontSize, 48.0f);    // the size number, grown   // E28
        CHECK_NEAR(TextCmd(out, 11).fontSize, 40.0f);   // the opacity number, not   // E28
        // Kept centred: the origin moves up and left by half of what it grew.   // E28
        CHECK_NEAR(TextCmd(out, 5).pos.x, 512.0f - 23.0f * 1.2f);   // E28
        CHECK_NEAR(TextCmd(out, 5).pos.y, 168.0f - 20.0f * 1.2f);   // E28
        CHECK(TextCmd(out, 5).text == "1.1");   // E28
        if (t < 5) rig.Tick();   // E28
    }   // E28
    rig.Tick();   // E28
    CHECK_NEAR(TextCmd(rig.Overlay(), 5).fontSize, 40.0f);   // E28
    CHECK_NEAR(TextCmd(rig.Overlay(), 5).pos.x, 512.0f - 23.0f);   // E28
    // Down: shrunk, the other readout's own.   // E28
    rig.Tap(TouchEditWidget::OpacityLess);   // E28
    CHECK_NEAR(TextCmd(rig.Overlay(), 11).fontSize, 32.0f);   // E28
    CHECK_NEAR(TextCmd(rig.Overlay(), 11).pos.x, 512.0f - 23.0f * 0.8f);   // E28
    CHECK_NEAR(TextCmd(rig.Overlay(), 5).fontSize, 40.0f);   // E28
    CHECK(TextCmd(rig.Overlay(), 11).text == "0.8");   // E28
    // A tap at a limit changes nothing, so there is nothing to pulse.   // E28
    EditRig limit;   // E28
    TouchTuning top;   // E28
    top.size = TouchTuning::kMaxSize;   // E28
    limit.Open(top);   // E28
    limit.Tap(TouchEditWidget::SizeMore);   // E28
    CHECK_NEAR(TextCmd(limit.Overlay(), 5).fontSize, 40.0f);   // E28
}   // E28

// A tap that lasted less than a tick reaches the editor through TouchControls::Down(), latched; the editor keeps no   // E28
// latch of its own.   // E28
void testEditorLatchedTap() {   // E28
    EditRig rig;   // E28
    rig.Open();   // E28
    rig.controls.LatchFrame({Finger(1, rig.Where(TouchEditWidget::Lock))});   // down and up in a frame no tick saw   // E28
    TouchInput in = rig.base;   // E28
    rig.controls.Update(in);   // E28
    CHECK(!rig.controls.Down().empty());   // E28
    TouchEditInput edit;   // E28
    edit.down = &rig.controls.Down();   // E28
    edit.geometry = TouchGeometry::From(in);   // E28
    rig.editor.Update(edit, rig.controls);   // E28
    CHECK(rig.editor.Locked());   // E28
    rig.controls.Update(in);   // the next tick: gone   // E28
    CHECK(rig.controls.Down().empty());   // E28
    edit.down = &rig.controls.Down();   // E28
    rig.editor.Update(edit, rig.controls);   // E28
    CHECK(!rig.editor.Locked());   // E28
}   // E28

// A data folder that holds the editor's images: they are drawn as sprites by name; one missing is a plain square.   // E28
void testEditorArtRoot() {   // E28
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "penumbra_e28_pause_suite_art";   // E28
    std::error_code ec;   // E28
    std::filesystem::remove_all(root, ec);   // E28
    std::filesystem::create_directories(root / "images" / "options", ec);   // E28
    const char* const names[] = {"arrow_left", "edit_shrink", "edit_enlarge", "edit_dim",   // E28
                                 "edit_brighten", "edit_locked", "edit_unlocked", "edit_restore"};   // E28
    // Only their being there matters: the editor looks each up once and the renderer decodes it.   // E28
    for (const char* name : names) std::ofstream(root / "images" / "options" / (std::string(name) + ".png")) << "x";   // E28
    const auto endsWith = [](const std::string& text, const std::string& tail) {   // E28
        return text.size() >= tail.size() && text.compare(text.size() - tail.size(), tail.size(), tail) == 0;   // E28
    };   // E28

    EditRig rig;   // E28
    rig.editor.SetImageRoot(root);   // E28
    rig.Open();   // E28
    rig.Tick();   // E28
    std::vector<HudCmd> out = rig.Overlay();   // E28
    const char* const order[kEditWidgets] = {"arrow_left", "edit_shrink", "edit_enlarge", "edit_dim",   // E28
                                             "edit_brighten", "edit_locked", "edit_restore"};   // E28
    if (out.size() >= kEditWidgets + kEditTexts) {   // E28
        for (int w = 0; w < kTouchEditWidgetCount; ++w) {   // E28
            const HudCmd& cmd = WidgetCmd(out, static_cast<TouchEditWidget>(w));   // E28
            CHECK(cmd.kind == HudCmd::Kind::ShapedSprite);   // E28
            CHECK_MSG(endsWith(cmd.sprite, std::string("/images/options/") + order[w] + ".png"), cmd.sprite);   // E28
            CHECK(cmd.pos == rig.Widget(static_cast<TouchEditWidget>(w)).min);   // E28
            CHECK(cmd.size == rig.Widget(static_cast<TouchEditWidget>(w)).Size());   // E28
            CHECK((cmd.color & 0x00FFFFFFu) == 0x00FFFFFFu);   // untinted: the art is the look   // E28
        }   // E28
    }   // E28
    // Unlocked, the lock tile is the red frame's.   // E28
    rig.Tap(TouchEditWidget::Lock);   // E28
    out = rig.Overlay();   // E28
    if (out.size() >= kEditWidgets + kEditTexts) {   // E28
        CHECK(endsWith(WidgetCmd(out, TouchEditWidget::Lock).sprite, "/edit_unlocked.png"));   // E28
    }   // E28
    // One image missing: that tile is a plain square, the rest keep their art.   // E28
    std::filesystem::remove(root / "images" / "options" / "edit_dim.png", ec);   // E28
    rig.editor.SetImageRoot(root);   // E28
    out = rig.Overlay();   // E28
    if (out.size() >= kEditWidgets + kEditTexts) {   // E28
        CHECK(WidgetCmd(out, TouchEditWidget::OpacityLess).kind == HudCmd::Kind::Rectangle);   // E28
        CHECK(WidgetCmd(out, TouchEditWidget::OpacityMore).kind == HudCmd::Kind::ShapedSprite);   // E28
    }   // E28
    // No data folder at all: seven squares.   // E28
    rig.editor.SetImageRoot(std::filesystem::path());   // E28
    out = rig.Overlay();   // E28
    if (out.size() >= kEditWidgets + kEditTexts) {   // E28
        for (int w = 0; w < kTouchEditWidgetCount; ++w) {   // E28
            CHECK(WidgetCmd(out, static_cast<TouchEditWidget>(w)).kind == HudCmd::Kind::Rectangle);   // E28
        }   // E28
    }   // E28
    std::filesystem::remove_all(root, ec);   // E28
}   // E28
// E28 END pure editor tests   // E28

// The art the editor names is there and looks as it should (tools/art/make_options_art.py): 96 px tiles and the 64 px   // E28
// back arrow, round corners, no magenta, the unlocked lock on a red frame and every other one not.   // E28
void testEditorArt() {   // E28
    const std::filesystem::path dir = std::filesystem::path(PENUMBRA_DATA_DIR) / "images" / "options";   // E28
    struct Image {   // E28
        const char* name;   // E28
        int size;   // E28
        bool red;   // E28
    };   // E28
    const Image images[] = {{"arrow_left", 64, false},   {"edit_shrink", 96, false},   {"edit_enlarge", 96, false},   // E28
                            {"edit_dim", 96, false},     {"edit_brighten", 96, false}, {"edit_locked", 96, false},   // E28
                            {"edit_unlocked", 96, true}, {"edit_restore", 96, false}};   // E28
    for (const Image& image : images) {   // E28
        const std::filesystem::path path = dir / (std::string(image.name) + ".png");   // E28
        CHECK_MSG(std::filesystem::is_regular_file(path), path.generic_string());   // E28
        const glm::ivec2 pixels = Penumbra::Render::ProbeImageSize(path.generic_string());   // E28
        CHECK_MSG(pixels.x == image.size && pixels.y == image.size, image.name);   // E28
        const Penumbra::Render::DecodedImage decoded =   // E28
            Penumbra::Render::DecodeTexture(path.generic_string(), Penumbra::Render::TextureVariant::Plain);   // E28
        CHECK_MSG(decoded.Valid(), image.name);   // E28
        if (!decoded.Valid()) continue;   // E28
        const auto alphaAt = [&decoded](const int x, const int y) {   // E28
            return decoded.rgba[(static_cast<std::size_t>(y) * static_cast<std::size_t>(decoded.width) +   // E28
                                 static_cast<std::size_t>(x)) * 4 + 3];   // E28
        };   // E28
        const int last = decoded.width - 1;   // E28
        CHECK_MSG(alphaAt(0, 0) < 255 && alphaAt(last, 0) < 255 && alphaAt(0, last) < 255 && alphaAt(last, last) < 255,   // E28
                  image.name);   // rounded: the corners show what is under   // E28
        double red = 0.0;   // E28
        double green = 0.0;   // E28
        std::size_t solid = 0;   // E28
        bool magenta = false;   // E28
        for (std::size_t p = 0; p + 3 < decoded.rgba.size(); p += 4) {   // E28
            magenta = magenta || (decoded.rgba[p] == 255 && decoded.rgba[p + 1] == 0 && decoded.rgba[p + 2] == 255);   // E28
            if (decoded.rgba[p + 3] != 255) continue;   // E28
            red += decoded.rgba[p];   // E28
            green += decoded.rgba[p + 1];   // E28
            ++solid;   // E28
        }   // E28
        CHECK_MSG(!magenta, image.name);   // E28
        CHECK_MSG(solid > 0, image.name);   // E28
        if (solid == 0) continue;   // E28
        const double meanRed = red / static_cast<double>(solid);   // E28
        const double meanGreen = green / static_cast<double>(solid);   // E28
        // The frame's body: red when it is the unlocked one's (editing), a neutral dark stone otherwise.   // E28
        if (image.red) CHECK_MSG(meanRed > meanGreen + 40.0, image.name);   // E28
        else CHECK_MSG(meanRed <= meanGreen + 40.0, image.name);   // E28
    }   // E28
}   // E28

// The glyph quads of a command list, as the box they cover in logical px (an empty box when there are none).   // E28
struct InkBox {   // E28
    bool any = false;   // E28
    glm::vec2 min{0.0f};   // E28
    glm::vec2 max{0.0f};   // E28
};   // E28

InkBox InkOf(const std::vector<Supersonic::ScreenOverlay::Quad>& quads, const View& view) {   // E28
    InkBox ink;   // E28
    for (const auto& quad : quads) {   // E28
        if (quad.texture.rfind("penumbra:font:", 0) != 0) continue;   // E28
        const glm::vec2 lo = (quad.min * glm::vec2(view.windowPixels) - view.viewportMin) / view.scale;   // E28
        const glm::vec2 hi = (quad.max * glm::vec2(view.windowPixels) - view.viewportMin) / view.scale;   // E28
        ink.min = ink.any ? glm::min(ink.min, lo) : lo;   // E28
        ink.max = ink.any ? glm::max(ink.max, hi) : hi;   // E28
        ink.any = true;   // E28
    }   // E28
    return ink;   // E28
}   // E28

bool InkOverlaps(const InkBox& ink, const TouchLayout::Box& box) {   // E28
    return ink.any && ink.min.x < box.max.x && ink.max.x > box.min.x && ink.min.y < box.max.y && ink.max.y > box.min.y;   // E28
}   // E28

// The overlay through HudRenderer: a thin outline stays thin on a wide menu, and in every language the title   // E28
// and the numbers fit their rooms and keep clear of the tiles, the back arrow and the Pause button.   // E28
void testEditorThroughHudRenderer() {   // E28
    entt::registry registry;   // E28
    Penumbra::Render::TextureCache textures(PENUMBRA_ORIGINAL_DIR);   // E28
    Penumbra::Render::FontAtlas fonts;   // E28
    Localization loc;   // E28
    CHECK(loc.Load());   // E28
    HudRenderer hud;   // E28
    hud.Attach(registry, textures, fonts, loc);   // E28
    CHECK(loc.HasTranslation(TouchEditor::kTitle));   // E28

    // E1's open sides: a 20:9 window shows 342 logical px past the 4:3 box each side.   // E28
    View wideView;   // E28
    wideView.logicalScreen = glm::vec2(1024.0f, 768.0f);   // E28
    wideView.windowPixels = glm::uvec2(1708, 768);   // E28
    wideView.scale = 1.0f;   // E28
    wideView.viewportMin = glm::vec2(342.0f, 0.0f);   // E28
    wideView.viewportMax = glm::vec2(1366.0f, 768.0f);   // E28
    wideView.openSides = 342.0f;   // E28
    View plainView;   // E28
    plainView.logicalScreen = glm::vec2(1024.0f, 768.0f);   // E28
    plainView.windowPixels = glm::uvec2(1024, 768);   // E28
    plainView.scale = 1.0f;   // E28
    plainView.viewportMin = glm::vec2(0.0f);   // E28
    plainView.viewportMax = glm::vec2(1024.0f, 768.0f);   // E28
    const RenderSnapshot empty;   // E28
    const auto quadsOf = [&](const std::vector<HudCmd>& extra, const View& view) {   // E28
        std::vector<Supersonic::ScreenOverlay::Quad> quads;   // E28
        hud.Build(empty, view, quads, &extra);   // E28
        return quads;   // E28
    };   // E28

    // The d-pad's grab strip straddles x = 0 on a notched phone: its outline's top row meets the screen's left   // E28
    // edge. HudRenderer carries a rectangle that meets it out to the edge of what is shown (a fade's way) - and must not   // E28
    // for one that says not to.   // E28
    {   // E28
        HudCmd line;   // E28
        line.kind = HudCmd::Kind::Rectangle;   // E28
        line.pos = glm::vec2(-230.0f, 621.0f);   // E28
        line.size = glm::vec2(400.0f, 2.0f);   // all four corners opaque white, HudCmd's default   // E28
        const auto whiteReach = [&](const HudCmd& cmd, float& low, float& high) {   // E28
            int count = 0;   // E28
            low = 1e9f;   // E28
            high = -1e9f;   // E28
            for (const auto& quad : quadsOf({cmd}, wideView)) {   // E28
                if (!quad.texture.empty() || quad.color != glm::vec4(1.0f)) continue;   // E28
                ++count;   // E28
                low = std::min(low, quad.min.x * 1708.0f - 342.0f);   // E28
                high = std::max(high, quad.max.x * 1708.0f - 342.0f);   // E28
            }   // E28
            return count;   // E28
        };   // E28
        float low = 0.0f;   // E28
        float high = 0.0f;   // E28
        HudCmd stays = line;   // E28
        stays.stretchToSides = false;   // E28
        CHECK_EQ(whiteReach(stays, low, high), 1);   // E28
        CHECK_NEAR(low, -230.0f);   // E28
        CHECK_NEAR(high, 170.0f);   // E28
        CHECK(whiteReach(line, low, high) > 1);   // the default is the stretch: the check has teeth   // E28
        CHECK(low < -300.0f);   // E28
    }   // E28

    if (fonts.FaceFile(TouchEditor::kFont).empty()) {   // E28
        hud.Detach();   // E28
        return;   // E28
    }   // E28

    // The phone: the shown area 20:9, a 3.5% HUD frame (E26), at the edge of the window the Pause hangs from it.   // E28
    EditRig phone;   // E28
    phone.Wide();   // E28
    phone.base.hudFrame = TouchInsets{59.8f, 26.9f, 59.8f, 0.0f};   // E28
    EditRig four3;   // E28
    struct Case {   // E28
        const char* name;   // E28
        EditRig* rig;   // E28
        const View* view;   // E28
    };   // E28
    const Case cases[] = {{"4:3", &four3, &plainView}, {"20:9", &phone, &wideView}};   // E28
    Penumbra::Render::VisualText visual;   // E28
    for (const Case& scene : cases) {   // E28
        EditRig& rig = *scene.rig;   // E28
        rig.Open({}, {}, true);   // E28
        rig.Tick();   // E28
        const std::vector<HudCmd> overlay = rig.Overlay();   // E28
        const TouchEditor::Layout layout = TouchEditor::ComputeLayout(rig.Geometry());   // E28
        const TouchLayout::Box pause = rig.controls.Layout()[TouchControl::Pause];   // E28
        if (overlay.size() < kEditTexts) continue;   // E28
        if (scene.view == &wideView) {   // E28
            // The title's edge is left of the Pause, which the HUD frame holds in from the notch's edge; with   // E28
            // the safe area alone it would run under the button.   // E28
            CHECK(layout.titleRight < pause.min.x);   // E28
            TouchGeometry bare = rig.Geometry();   // E28
            bare.hudFrame = TouchInsets{};   // E28
            CHECK(TouchEditor::ComputeLayout(bare).titleRight > pause.min.x);   // E28
        }   // E28
        for (const Penumbra::Render::LanguageInfo& info : Penumbra::Render::kLanguages) {   // E28
            const Language language = info.language;   // E28
            if (!loc.HasLanguageFile(language)) continue;   // E28
            loc.SetLanguage(language);   // E28
            const std::string where = std::string(scene.name) + " " + info.id;   // E28
            // The title: one line, in its room (750 px: 140 from the right edge and the Pause's column).   // E28
            const Penumbra::Render::TextLayout words = fonts.LayoutCodePoints(   // E28
                visual.Of(loc.Translate(TouchEditor::kTitle, language), info.rightToLeft), TouchEditor::kFont,   // E28
                TouchEditor::kTitleSize, layout.title);   // E28
            CHECK_MSG(words.lines == 1, where + " title lines: " + std::to_string(words.lines));   // E28
            CHECK_MSG(words.width <= 750.0f, where + " title is " + std::to_string(words.width) + " px wide");   // E28

            // Every text, shadow and front, as drawn: its ink clear of every tile, the arrow and the Pause.   // E28
            for (std::size_t t = 0; t < kEditTexts; ++t) {   // E28
                const HudCmd& cmd = TextCmd(overlay, t);   // E28
                const InkBox ink = InkOf(quadsOf({cmd}, *scene.view), *scene.view);   // E28
                CHECK_MSG(ink.any, where + " text " + std::to_string(t));   // E28
                for (int w = 0; w < kTouchEditWidgetCount; ++w) {   // E28
                    CHECK_MSG(!InkOverlaps(ink, layout.widget[static_cast<std::size_t>(w)]),   // E28
                              where + " text " + std::to_string(t) + " over widget " + std::to_string(w));   // E28
                }   // E28
                if (t < 2) CHECK_MSG(!InkOverlaps(ink, pause), where + " the title is over the Pause button");   // E28
                // The title keeps to its room; right to left it ends at titleRight (its shadow a tenth past).   // E28
                if (t < 2 && ink.any) {   // E28
                    if (info.rightToLeft) {   // E28
                        const float edge = layout.titleRight + (t == 0 ? 4.0f : 0.0f);   // E28
                        CHECK_MSG(ink.max.x <= edge + 3.0f && ink.max.x >= edge - 8.0f,   // E28
                                  where + " title ink ends at " + std::to_string(ink.max.x) + ", edge " + std::to_string(edge));   // E28
                        CHECK_MSG(ink.min.x >= layout.title.x - 8.0f, where + " title runs left of its start");   // E28
                    } else {   // E28
                        CHECK_MSG(ink.min.x >= layout.title.x - 2.0f && ink.max.x <= layout.title.x + 750.0f + 6.0f,   // E28
                                  where + " title ink " + std::to_string(ink.min.x) + " - " + std::to_string(ink.max.x));   // E28
                    }   // E28
                }   // E28
            }   // E28
        }   // E28
    }   // E28
    hud.Detach();   // E28
}   // E28

// ENHANCEMENT E35: THE ENGINE'S INTRO (render/Splash). Pure: the timeline, the skip, what counts as a press, where
// the logo goes, the quads, the logo's file, and which starts play it. This suite holds them because it already holds
// what the intro's skip is built from - the ticks a screen stands still for, and the input held back after them
// (testFilter).
// The layer's side of it (the machine standing still, the first menu frame, the press kept out of the menu) is
// test_pn_render_hud's TestSplashInLayer.
namespace splash {

using Penumbra::Render::SplashClock;

// The logo's size in pixels (images/splash/README.md): the testSplashLogoFile checks the file against it, and the
// layout takes its proportions from it.
constexpr int kLogoWidth = 2258;
constexpr int kLogoHeight = 640;

// A clock run to its end with a press at `pressAt` (never when it is negative), the alpha after each tick.
struct Run {
    SplashClock clock;
    std::vector<float> alpha;   // after tick 1, 2, ...
};

Run RunClock(const int pressAt) {
    Run run;
    for (int tick = 0; tick < 200 && !run.clock.Done(); ++tick) {
        run.clock.Step(tick == pressAt);
        run.alpha.push_back(run.clock.Alpha());
    }
    return run;
}

void testSplashTimeline() {   // E35
    using namespace Penumbra::Render;
    CHECK_EQ(kSplashFadeInTicks, 24u);
    CHECK_EQ(kSplashHoldTicks, 72u);
    CHECK_EQ(kSplashFadeOutTicks, 24u);
    CHECK_EQ(kSplashTotalTicks, 120u);
    CHECK_EQ(kSplashFadeOutStart, 96u);
    CHECK_EQ(kSplashSkipFromTick, 18u);

    CHECK(SplashAlpha(0) == 0.0f);
    CHECK(SplashAlpha(24) == 1.0f);
    CHECK(SplashAlpha(60) == 1.0f);
    CHECK(SplashAlpha(96) == 1.0f);
    CHECK(SplashAlpha(120) == 0.0f);
    CHECK(SplashAlpha(100000) == 0.0f);
    CHECK(!SplashDone(119));
    CHECK(SplashDone(120));

    // In 0..1 everywhere; strictly rising over the fade-in, level over the hold, strictly falling over the
    // fade-out; the biggest step is smoothstep's own (1.5 / 24 = 0.0625), so it is a ramp and not a cut.
    float biggest = 0.0f;
    for (unsigned t = 0; t <= 130; ++t) {
        const float a = SplashAlpha(t);
        CHECK_MSG(a >= 0.0f && a <= 1.0f, "alpha " + std::to_string(t));
        if (t > 0) biggest = std::max(biggest, std::fabs(a - SplashAlpha(t - 1)));
        if (t < 24) CHECK_MSG(SplashAlpha(t + 1) > a, "rises at " + std::to_string(t));
        if (t >= 24 && t < 96) CHECK_MSG(a == 1.0f, "holds at " + std::to_string(t));
        if (t >= 96 && t < 120) CHECK_MSG(SplashAlpha(t + 1) < a, "falls at " + std::to_string(t));
        // The fade-out is the fade-in backwards.
        if (t <= 24) CHECK_NEAR(SplashAlpha(96 + t), SplashAlpha(24 - t));
    }
    CHECK_MSG(biggest < 0.0626f, "the biggest step is " + std::to_string(biggest));
    CHECK(biggest > 0.05f);

    // The clock with no press follows that timeline tick by tick and ends on the 120th.
    SplashClock clock;
    CHECK(!clock.Done() && clock.Ticks() == 0 && clock.Alpha() == 0.0f && !clock.Skipped());
    for (unsigned t = 1; t <= 120; ++t) {
        CHECK(!clock.Done());
        clock.Step(false);
        CHECK_EQ(clock.Ticks(), t);
        CHECK(clock.Alpha() == SplashAlpha(t));
    }
    CHECK(clock.Done() && !clock.Skipped() && clock.Alpha() == 0.0f);
    clock.Step(true);   // a finished clock stays as it is
    CHECK(clock.Ticks() == 120u && !clock.Skipped());
}

void testSplashSkip() {   // E35
    using namespace Penumbra::Render;

    // Before tick 18 a press is ignored: the intro runs its whole 120 ticks, whatever the press.
    for (int t = 0; t < 18; ++t) {
        const Run run = RunClock(t);
        CHECK_MSG(!run.clock.Skipped(), "a press at tick " + std::to_string(t) + " skipped it");
        CHECK_MSG(run.clock.Ticks() == 120u, "a press at tick " + std::to_string(t) + " ended it at " +
                                                 std::to_string(run.clock.Ticks()));
    }

    // From tick 18 to the tick before the fade-out begins by itself it ends the intro, in 24 ticks at most, by
    // a fade that never rises (no pop for the presses made while the alpha is still below 1).
    for (int t = 18; t < 96; ++t) {
        const Run run = RunClock(t);
        const std::string where = "a press at tick " + std::to_string(t);
        CHECK_MSG(run.clock.Skipped() && run.clock.SkipTick() == static_cast<unsigned>(t), where);
        CHECK_MSG(run.clock.Done() && run.clock.Ticks() == static_cast<unsigned>(t) + 24u,
                  where + " ended at " + std::to_string(run.clock.Ticks()));
        CHECK_MSG(run.clock.Ticks() < kSplashTotalTicks, where + " is not sooner than the timeline's end");
        float before = SplashAlpha(static_cast<unsigned>(t));   // what the previous tick drew
        for (std::size_t i = static_cast<std::size_t>(t); i < run.alpha.size(); ++i) {
            CHECK_MSG(run.alpha[i] <= before && run.alpha[i] >= 0.0f,
                      where + ": the fade rises at " + std::to_string(i + 1));
            before = run.alpha[i];
        }
        CHECK_MSG(run.alpha.back() == 0.0f, where + " ends on nothing");
        // Not a hard cut: the first tick after the press is within one ramp step of the one before it.
        CHECK_MSG(std::fabs(run.alpha[static_cast<std::size_t>(t)] - SplashAlpha(static_cast<unsigned>(t))) < 0.0626f,
                  where + " is a cut");
    }

    // From the first tick of the timeline's own fade-out, a press changes nothing: it ends at tick 120.
    for (int t = 96; t < 120; ++t) {
        const Run run = RunClock(t);
        CHECK_MSG(!run.clock.Skipped() && run.clock.Ticks() == 120u, "a press at tick " + std::to_string(t));
    }

    // A second press changes nothing about the first.
    SplashClock clock;
    for (int t = 0; t < 40; ++t) clock.Step(t == 30);
    CHECK(clock.SkipTick() == 30u);
    clock.Step(true);
    clock.Step(true);
    CHECK(clock.SkipTick() == 30u);
    // And a press held across ticks is one press: the clock is told by SplashPressWatch, below.
}

void testSplashPress() {   // E35
    using namespace Penumbra::Render;
    const std::vector<int> none;

    // A key still down from before the intro is no press; a release is none; a fresh press is.
    RawDevices raw;
    raw.keys[Supersonic::Key::Enter] = true;
    SplashPressWatch watch;
    CHECK(!watch.Observe(SplashHeldOf(raw, none)));   // the first look only records
    CHECK(!watch.Observe(SplashHeldOf(raw, none)));   // held on
    raw.keys[Supersonic::Key::Enter] = false;
    CHECK(!watch.Observe(SplashHeldOf(raw, none)));   // released
    raw.keys[Supersonic::Key::Enter] = true;
    CHECK(watch.Observe(SplashHeldOf(raw, none)));    // pressed again
    CHECK(!watch.Observe(SplashHeldOf(raw, none)));   // and held
    // A second key while the first is held is a press: it is what went down, not "anything is down".
    raw.keys[Supersonic::Key::Space] = true;
    CHECK(watch.Observe(SplashHeldOf(raw, none)));
    // Letting one go while the other stays is no press.
    raw.keys[Supersonic::Key::Enter] = false;
    CHECK(!watch.Observe(SplashHeldOf(raw, none)));

    // Every kind of input is one, each by itself.
    const auto pressedBy = [&](const auto& set) {
        RawDevices device;
        std::vector<int> fingers;
        SplashPressWatch w;
        w.Observe(SplashHeldOf(device, fingers));   // nothing held at the start
        set(device, fingers);
        return w.Observe(SplashHeldOf(device, fingers));
    };
    for (int code = 0; code <= Supersonic::Key::Last; ++code) {
        CHECK_MSG(pressedBy([code](RawDevices& d, std::vector<int>&) {
                      d.keys[static_cast<std::size_t>(code)] = true;
                  }),
                  "key " + std::to_string(code));
    }
    for (std::size_t b = 0; b < 3; ++b) {
        CHECK_MSG(pressedBy([b](RawDevices& d, std::vector<int>&) { d.mouse[b] = true; }),
                  "mouse " + std::to_string(b));
    }
    for (int id = 0; id < 8; ++id) {
        CHECK_MSG(pressedBy([id](RawDevices&, std::vector<int>& f) { f.push_back(id); }),
                  "finger " + std::to_string(id));
    }
    for (std::size_t pad = 0; pad < kSplashPads; ++pad) {
        for (int b = 0; b < Supersonic::Pad::ButtonCount; ++b) {
            CHECK_MSG(pressedBy([pad, b](RawDevices& d, std::vector<int>&) {
                          d.pads.resize(pad + 1);
                          d.pads[pad].buttons[static_cast<std::size_t>(b)] = true;
                      }),
                      "pad " + std::to_string(pad) + " button " + std::to_string(b));
        }
        for (std::size_t b = 0; b < 32; ++b) {
            CHECK_MSG(pressedBy([pad, b](RawDevices& d, std::vector<int>&) {
                          d.pads.resize(pad + 1);
                          d.pads[pad].gamepad = false;
                          d.pads[pad].rawButtons[b] = true;
                      }),
                      "pad " + std::to_string(pad) + " raw button " + std::to_string(b));
        }
    }
    // A stick, a trigger, the mouse moving and a pad past the fourth are not presses: they drift, or are not read.
    CHECK(!pressedBy([](RawDevices& d, std::vector<int>&) {
        d.pads.resize(1);
        d.pads[0].axes[Supersonic::Pad::LeftX] = 1.0f;
        d.pads[0].axes[Supersonic::Pad::RightTrigger] = 1.0f;
        d.mouseWindow = glm::vec2(300.0f, 200.0f);
    }));
    CHECK(!pressedBy([](RawDevices& d, std::vector<int>&) {
        d.pads.resize(kSplashPads + 1);
        d.pads[kSplashPads].buttons[0] = true;
    }));
    // No two inputs share a bit: each one held adds one.
    RawDevices every;
    std::vector<int> fingers;
    for (std::size_t k = 0; k < every.keys.size(); ++k) every.keys[k] = true;
    for (std::size_t b = 0; b < 3; ++b) every.mouse[b] = true;
    for (int id = 0; id < 8; ++id) fingers.push_back(id);
    every.pads.resize(kSplashPads);
    for (RawPad& pad : every.pads) {
        pad.buttons.fill(true);
        pad.rawButtons.fill(true);
    }
    CHECK_EQ(SplashHeldOf(every, fingers).count(), kSplashHeldBits);
}

void testSplashLayout() {   // E35
    using namespace Penumbra::Render;
    const glm::vec2 logo(static_cast<float>(kLogoWidth), static_cast<float>(kLogoHeight));
    const float aspect = logo.x / logo.y;   // 2258 / 640
    const Supersonic::SafeAreaInsets none;

    // The shapes the captures are taken at. (Pinned for a logo of 2258 x 640: another picture moves them.)
    SplashLayout wide =
        ComputeSplashLayout(glm::uvec2(2992, 1344), Supersonic::SafeAreaInsets{199.0f, 0.0f, 0.0f, 0.0f}, logo);
    CHECK(wide.size == glm::vec2(2094.0f, 594.0f));   // 70% of 2992 wide
    CHECK(wide.pos == glm::vec2(549.0f, 375.0f));     // centred in x from 199 to 2992, in y from 0 to 1344
    SplashLayout hd = ComputeSplashLayout(glm::uvec2(1280, 720), none, logo);
    CHECK(hd.size == glm::vec2(896.0f, 254.0f) && hd.pos == glm::vec2(192.0f, 233.0f));
    SplashLayout old = ComputeSplashLayout(glm::uvec2(1024, 768), none, logo);
    CHECK(old.size == glm::vec2(717.0f, 203.0f) && old.pos == glm::vec2(154.0f, 283.0f));
    SplashLayout portrait = ComputeSplashLayout(glm::uvec2(1080, 2400), none, logo);
    CHECK(portrait.size == glm::vec2(756.0f, 214.0f) && portrait.pos == glm::vec2(162.0f, 1093.0f));
    // A very wide window: the height cap (half the window's) is what limits it.
    SplashLayout ultra = ComputeSplashLayout(glm::uvec2(5120, 1080), none, logo);
    CHECK(ultra.size == glm::vec2(1905.0f, 540.0f) && ultra.pos == glm::vec2(1608.0f, 270.0f));

    // In every shape: whole pixels, the logo's proportions, within 70% of the width and half the height, inside the
    // safe area, and centred in it to a pixel.
    const glm::uvec2 windows[] = {{640, 480},   {800, 600},   {1024, 768},  {1280, 720},  {1366, 768},
                                  {1920, 1080}, {2400, 1080}, {2560, 1080}, {2992, 1344}, {3440, 1440},
                                  {5120, 1080}, {1080, 2400}, {720, 1280},  {600, 1024}};
    const Supersonic::SafeAreaInsets insets[] = {none, {0.0f, 88.0f, 0.0f, 0.0f}, {199.0f, 0.0f, 0.0f, 0.0f},
                                                 {88.0f, 0.0f, 88.0f, 0.0f}, {0.0f, 66.0f, 0.0f, 66.0f},
                                                 {120.0f, 40.0f, 60.0f, 90.0f}};
    for (const glm::uvec2 window : windows) {
        for (const Supersonic::SafeAreaInsets& safe : insets) {
            const SplashLayout layout = ComputeSplashLayout(window, safe, logo);
            const std::string where = std::to_string(window.x) + "x" + std::to_string(window.y) + " insets " +
                                      std::to_string(safe.left) + "," + std::to_string(safe.top) + "," +
                                      std::to_string(safe.right) + "," + std::to_string(safe.bottom);
            CHECK_MSG(layout.size.x == std::round(layout.size.x) && layout.size.y == std::round(layout.size.y) &&
                          layout.pos.x == std::round(layout.pos.x) && layout.pos.y == std::round(layout.pos.y),
                      where + ": not whole pixels");
            CHECK_MSG(std::fabs(layout.size.x / layout.size.y - aspect) < 2.0f / layout.size.y,
                      where + ": proportions");
            CHECK_MSG(layout.size.x <= 0.70f * static_cast<float>(window.x) + 0.5f &&
                          layout.size.y <= 0.50f * static_cast<float>(window.y) + 0.5f,
                      where + ": larger than 70% x 50%");
            const glm::vec2 areaMin(safe.left, safe.top);
            const glm::vec2 areaMax(static_cast<float>(window.x) - safe.right,
                                    static_cast<float>(window.y) - safe.bottom);
            CHECK_MSG(layout.pos.x >= areaMin.x && layout.pos.y >= areaMin.y &&
                          layout.pos.x + layout.size.x <= areaMax.x && layout.pos.y + layout.size.y <= areaMax.y,
                      where + ": outside the safe area");
            const glm::vec2 centre = layout.pos + layout.size * 0.5f;
            const glm::vec2 middle = (areaMin + areaMax) * 0.5f;
            CHECK_MSG(std::fabs(centre.x - middle.x) <= 1.0f && std::fabs(centre.y - middle.y) <= 1.0f,
                      where + ": not centred");
            // The width's own rule where neither the height cap nor the safe area binds.
            if (0.70f * static_cast<float>(window.x) / aspect <= 0.50f * static_cast<float>(window.y) &&
                0.70f * static_cast<float>(window.x) <= areaMax.x - areaMin.x) {
                CHECK_MSG(std::fabs(layout.size.x - 0.70f * static_cast<float>(window.x)) <= 0.5f,
                          where + ": not 70% wide");
            }
        }
    }

    // A safe area narrower than 70% of the window holds it; insets that leave nothing are not believed; nothing
    // to draw gives nothing.
    const SplashLayout narrow =
        ComputeSplashLayout(glm::uvec2(1000, 1000), Supersonic::SafeAreaInsets{300.0f, 0.0f, 300.0f, 0.0f}, logo);
    CHECK(narrow.size.x == 400.0f && narrow.pos.x == 300.0f);
    const SplashLayout nothingLeft =
        ComputeSplashLayout(glm::uvec2(1000, 600), Supersonic::SafeAreaInsets{600.0f, 0.0f, 600.0f, 0.0f}, logo);
    CHECK(nothingLeft.size == ComputeSplashLayout(glm::uvec2(1000, 600), none, logo).size);
    CHECK(ComputeSplashLayout(glm::uvec2(0, 600), none, logo).size == glm::vec2(0.0f));
    CHECK(ComputeSplashLayout(glm::uvec2(600, 0), none, logo).size == glm::vec2(0.0f));
    CHECK(ComputeSplashLayout(glm::uvec2(600, 600), none, glm::vec2(0.0f)).size == glm::vec2(0.0f));
}

void testSplashQuads() {   // E35
    using namespace Penumbra::Render;
    const glm::uvec2 window(1280, 720);
    const glm::vec2 logo(static_cast<float>(kLogoWidth), static_cast<float>(kLogoHeight));
    const SplashLayout layout = ComputeSplashLayout(window, Supersonic::SafeAreaInsets{}, logo);

    // The ground first, over the whole image, untextured, in the logo's panel colour as display bytes.
    const std::vector<Supersonic::ScreenOverlay::Quad> quads = BuildSplashQuads(window, layout, 0.5f, "penumbra:logo");
    CHECK_EQ(quads.size(), 2u);
    CHECK(quads[0].min == glm::vec2(0.0f) && quads[0].max == glm::vec2(1.0f) && quads[0].texture.empty());
    CHECK(quads[0].color == glm::vec4(20.0f / 255.0f, 23.0f / 255.0f, 28.0f / 255.0f, 1.0f));
    CHECK(kSplashGround[0] == 0x14 && kSplashGround[1] == 0x17 && kSplashGround[2] == 0x1C);
    // Then the logo at its place, full texture, white at the intro's alpha.
    CHECK(quads[1].texture == "penumbra:logo");
    CHECK(quads[1].min == layout.pos / glm::vec2(window) &&
          quads[1].max == (layout.pos + layout.size) / glm::vec2(window));
    CHECK(quads[1].uvMin == glm::vec2(0.0f) && quads[1].uvMax == glm::vec2(1.0f));
    CHECK(quads[1].color == glm::vec4(1.0f, 1.0f, 1.0f, 0.5f));

    // Nothing of the logo at alpha 0, without a texture, or in an empty window; alpha is held to 0..1.
    CHECK_EQ(BuildSplashQuads(window, layout, 0.0f, "penumbra:logo").size(), 1u);
    CHECK_EQ(BuildSplashQuads(window, layout, -1.0f, "penumbra:logo").size(), 1u);
    CHECK_EQ(BuildSplashQuads(window, layout, 1.0f, "").size(), 1u);
    CHECK_EQ(BuildSplashQuads(glm::uvec2(0, 720), layout, 1.0f, "penumbra:logo").size(), 0u);
    CHECK_EQ(BuildSplashQuads(window, SplashLayout{}, 1.0f, "penumbra:logo").size(), 1u);
    CHECK(BuildSplashQuads(window, layout, 7.0f, "penumbra:logo")[1].color.a == 1.0f);
}

void testSplashLogoFile() {   // E35
    using namespace Penumbra::Render;
    const std::filesystem::path path = std::filesystem::path(PENUMBRA_DATA_DIR) / kSplashLogoFile;
    CHECK_MSG(std::filesystem::is_regular_file(path), path.generic_string());
    std::error_code ec;
    const auto bytes = std::filesystem::file_size(path, ec);
    CHECK_MSG(!ec && bytes > 0 && bytes < 150u * 1024u, "the logo is " + std::to_string(bytes) + " bytes");
    const glm::ivec2 pixels = ProbeImageSize(path.generic_string());
    CHECK_MSG(pixels.x == kLogoWidth && pixels.y == kLogoHeight,
              "the logo is " + std::to_string(pixels.x) + "x" + std::to_string(pixels.y));
    // As the layer loads it: Plain, which keys nothing. Opaque (no alpha: the canvas is the ground all round, which is
    // what makes the flat ground meet it), the ground on the whole border, and not one pixel of exact magenta, which
    // every other HUD image has keyed out.
    const DecodedImage decoded = DecodeTexture(path.generic_string(), TextureVariant::Plain);
    CHECK(decoded.Valid());
    if (!decoded.Valid()) return;
    const int width = decoded.width;
    const int height = decoded.height;
    const auto texel = [&decoded, width](const int x, const int y) {
        const std::size_t texelIndex = static_cast<std::size_t>(y * width + x);   // int: the picture is small
        return &decoded.rgba[texelIndex * 4];
    };
    const int right = width - 1;
    const int bottom = height - 1;
    for (const glm::ivec2 corner :
         {glm::ivec2(0, 0), glm::ivec2(right, 0), glm::ivec2(0, bottom), glm::ivec2(right, bottom)}) {
        const std::uint8_t* p = texel(corner.x, corner.y);
        CHECK_MSG(p[0] == kSplashGround[0] && p[1] == kSplashGround[1] && p[2] == kSplashGround[2] && p[3] == 255,
                  "corner " + std::to_string(corner.x) + "," + std::to_string(corner.y));
    }
    // One pass: magenta, translucency, the border, and the extents of what is off the ground. The lockup is what is
    // more than 30 levels off it (the mark, the wordmark and the rule's strong part); the faint rest is the rule's
    // tail and the mark's thinnest arc.
    struct Box {
        int minX = 1 << 30, minY = 1 << 30, maxX = -1, maxY = -1;
        void Add(const int x, const int y) {
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }
    };
    Box lockup;
    Box offGround;
    std::size_t magenta = 0;
    std::size_t translucent = 0;
    std::size_t borderOff = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::uint8_t* p = texel(x, y);
            if (p[0] == 255 && p[1] == 0 && p[2] == 255) ++magenta;
            if (p[3] != 255) ++translucent;
            const int deviation = std::max({std::abs(p[0] - kSplashGround[0]), std::abs(p[1] - kSplashGround[1]),
                                            std::abs(p[2] - kSplashGround[2])});
            if (deviation > 0) offGround.Add(x, y);
            if (deviation > 30) lockup.Add(x, y);
            if (deviation > 0 && (x == 0 || y == 0 || x == right || y == bottom)) ++borderOff;
        }
    }
    CHECK_EQ(magenta, 0u);
    CHECK_EQ(translucent, 0u);
    CHECK_EQ(borderOff, 0u);

    // The lockup's place, pinned for this picture: x 160 to 2096, y 143 to 496. It is centred in it, to a pixel
    // (the width is even, so the two sides may differ by one), so that centring the picture centres what is seen.
    CHECK_MSG(lockup.minX == 160 && lockup.maxX == 2096 && lockup.minY == 143 && lockup.maxY == 496,
              "the lockup is x " + std::to_string(lockup.minX) + " to " + std::to_string(lockup.maxX) + ", y " +
                  std::to_string(lockup.minY) + " to " + std::to_string(lockup.maxY));
    CHECK_MSG(std::abs(lockup.minX - (right - lockup.maxX)) <= 1, "the lockup is not centred across");
    CHECK_MSG(std::abs(lockup.minY - (bottom - lockup.maxY)) <= 1, "the lockup is not centred down");
    // Nothing else is in the picture: not a band of any strength across it (the engine's SVG has four faint ones
    // that would end in square edges at the top and bottom), and not a tail of the rule that runs to the right edge
    // and is cut there: above and below the lockup the ground is exact, and the rule has faded out well short of
    // the edge.
    CHECK_MSG(offGround.minY >= lockup.minY && offGround.maxY <= lockup.maxY,
              "something off the ground above or below the lockup: y " + std::to_string(offGround.minY) + " to " +
                  std::to_string(offGround.maxY));
    CHECK_MSG(offGround.minX >= lockup.minX - 8 && offGround.maxX <= lockup.maxX + 100 && offGround.maxX < right - 40,
              "something off the ground beside the lockup: x " + std::to_string(offGround.minX) + " to " +
                  std::to_string(offGround.maxX));
}

void testSplashWanted() {   // E35
    using namespace Penumbra::Render;
    using Args = std::vector<std::string>;

    // A normal start: nothing, or the flags the platform glue and a player's own choices give.
    CHECK(SplashWanted({}));
    CHECK(SplashWanted(Args{"--original", "/game/original", "--data", "/game/data"}));
    CHECK(SplashWanted(Args{"--original", "x", "--data", "y", "--window", "1280x720", "--lang", "pt", "--safe-area",
                            "199,0,0,0"}));
    for (const std::string_view flag : kSplashPlayerFlags) {
        // Each flag a player has, alone and with a value (a value is no flag).
        CHECK_MSG(SplashWanted(Args{std::string(flag)}), std::string(flag));
        if (flag != "--splash") CHECK_MSG(SplashWanted(Args{std::string(flag), "on"}), std::string(flag));
    }
    CHECK(SplashWanted(Args{"--fullscreen"}));
    CHECK(SplashWanted(Args{"--windowed", "--widescreen", "off", "--smooth", "on", "--touch", "--zoom", "125",
                            "--edge-margin", "4", "--refresh", "144"}));

    // Every developer's flag turns it off, alone and among a normal start's.
    const Args developer = {"--start", "--tour", "--hold", "--cursor", "--pointer", "--spawn", "--princess", "--hp",
                            "--mobile-layout", "--modes", "--touch-tuning", "--touch-editor", "--finger",
                            "--difficulty",   // E36: a capture's flag, so it removes the intro like the others
                            // The engine's.
                            "--frames", "--screenshot", "--screenshot-every", "--fixed-step", "--scene", "--record",
                            "--replay", "--import-assets",
                            // One nobody has written yet: unknown flags fail safe.
                            "--frobnicate"};
    for (const std::string& flag : developer) {
        CHECK_MSG(!SplashWanted(Args{flag}), flag + " alone");
        CHECK_MSG(!SplashWanted(Args{"--original", "x", "--data", "y", flag, "level1", "--window", "1280x720"}),
                  flag + " among others");
        CHECK_MSG(!SplashWanted(Args{flag + "=1"}), flag + "=1");
    }
    CHECK(!SplashWanted(Args{"--start", "level1", "--frames", "300", "--screenshot", "/tmp/x.png", "--fixed-step"}));

    // --splash decides over everything, the last one winning.
    CHECK(!SplashWanted(Args{"--splash", "off"}));
    CHECK(!SplashWanted(Args{"--original", "x", "--splash", "off", "--data", "y"}));
    CHECK(SplashWanted(Args{"--splash", "on"}));
    for (const std::string& flag : developer) {
        CHECK_MSG(SplashWanted(Args{flag, "--splash", "on"}), flag + " with --splash on");
        CHECK_MSG(SplashWanted(Args{"--splash", "on", flag}), "--splash on before " + flag);
    }
    CHECK(SplashWanted(Args{"--start", "level1", "--frames", "125", "--fixed-step", "--splash", "on"}));
    CHECK(!SplashWanted(Args{"--splash", "on", "--splash", "off"}));
    CHECK(SplashWanted(Args{"--splash", "off", "--splash", "on"}));
    // A --splash with no value (or a wrong one) is not a decision: main.cpp refuses it. Here it is a flag of the
    // player's.
    CHECK(SplashWanted(Args{"--splash"}));
    CHECK(!SplashWanted(Args{"--splash", "--start", "level1"}));
}

} // namespace splash

void runTests() {
    testOpensOnlyInPlay();
    testInputFrom();
    testNavigation();
    testResume();
    testMainMenu();
    testFocus();
    testPointer();
    testOverlay();
    testThroughHudRenderer();
    testRightToLeft();   // E24
    testFilter();
    testEnglish();
    testSetting();
    testDifficultySetting();   // E36
    testEditorLayout();   // E28
    testEditorOpen();   // E28
    testEditorDrag();   // E28
    testEditorLocked();   // E28
    testEditorSteps();   // E28
    testEditorRestore();   // E28
    testEditorClose();   // E28
    testEditorOverlay();   // E28
    testEditorPulse();   // E28
    testEditorLatchedTap();   // E28
    testEditorArtRoot();   // E28
    testEditorArt();   // E28
    testEditorThroughHudRenderer();   // E28
    splash::testSplashTimeline();   // E35
    splash::testSplashSkip();   // E35
    splash::testSplashPress();   // E35
    splash::testSplashLayout();   // E35
    splash::testSplashQuads();   // E35
    splash::testSplashLogoFile();   // E35
    splash::testSplashWanted();   // E35
}

} // namespace

TEST_MAIN("test_pn_render_pause", 150)
