// E13, the pause (render/PauseMenu), and what it needs around it: where and on
// what it opens (play only; Esc and player 1's Back, never Start), the freeze
// (the layer is told not to tick), the menu's navigation, Resume, Main menu's
// one tick of the original's cancel, the pause on focus loss, the overlay's
// HudCmds and HudRenderer's extra commands, the filter that keeps the pause's
// presses out of the game, the English, and settings.pauseOnFocusLoss.
// Pure: no window, no Machine, no original files.

#include <string>
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
#include "render/TextureCache.hpp"
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
    testFilter();
    testEnglish();
    testSetting();
}

} // namespace

TEST_MAIN("test_pn_render_pause", 150)
