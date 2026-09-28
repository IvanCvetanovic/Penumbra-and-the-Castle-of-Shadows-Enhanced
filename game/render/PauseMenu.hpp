#pragma once

// ENHANCEMENT E13: a pause. 0.7.12 had none: Esc in a level went straight back
// to the main menu (doLoop's escToGoToMenu, setupScene.as:392 and menu.as:393)
// and the run was lost.
//
// THE FREEZE. While paused the layer does not run Machine::Frame, so nothing in
// the game moves: GetTime() stops, and with it every cooldown, fade, particle
// and the run's clock - the best times exclude paused time (the original had
// no pause to exclude). The world is drawn as the last tick left it.
//
// WHERE AND ON WHAT IT OPENS. Only in play, which the layer decides (the
// scene's loop is levelLoop or pvpLoop): the menus, the options and game over
// keep Esc as the original had it. Only on the original's own cancel for
// player 1 - K_ESC, or Back (JK_09) on player 1's joystick, getPlayerJoystick(0)
// (getCancelButtonStatus, playerInput.as:289) - and never on Start, which is
// player 2's summon (controlCharacters.as:679). And when the window loses focus
// in play (settings.pauseOnFocusLoss): on the LOSS, focused -> unfocused, never
// on a window that simply is unfocused, so a headless run in a background
// window does not pause itself.
//
// THE MENU. 'Pausado' over 'Continuar' and 'Menu principal', drawn the way the
// scripts draw their panels: Eth::HudCmds - a dim over the whole screen, a
// panel in showData's black gradient (menu.as:217), shadowText pairs in Arial
// Narrow in the scripts' (203,203,228) (util.as:450) - that the layer hands to
// HudRenderer after the snapshot's, so they are translated (strings.json) and
// laid out (FontAtlas) exactly as the game's own text. Up/Down (the keys, the
// stick or the D-pad) move and wrap, Enter/A/Start confirm, Esc/B/Back
// resume; the mouse selects what it moves over and clicks it - but not with
// the click that brings the window back to the front (a quarter second of
// grace after the focus returns), which must never land on Main menu.
//
// MAIN MENU is the original's own way out: the pause closes and the game is fed
// its cancel - K_ESC held for exactly one tick - so doLoop's escToGoToMenu sees
// the KS_HIT it always saw and does what it always did. That cancel never
// opens the pause again (it is latched as already held).
//
// AFTER A PAUSE, what was pressed in it stays in it. FilterForGame holds every
// key, button and stick the game did not see held before the pause up until it
// is released: otherwise the Esc that resumed would be escToGoToMenu's KS_HIT,
// the A that confirmed a jump, the stick that moved down a "next level" at a
// portal, and the Enter, click or Start that picked Main menu a confirm on the
// menu's first frame (getConfirmButtonStatus reads K_RETURN, K_LMOUSE, JK_10).
//
// Pure: fed held states once a tick, it answers what the layer should do. No
// window, no Machine, no fonts.

#include <array>
#include <vector>

#include <glm/glm.hpp>

#include "eth/Input.hpp"
#include "eth/Snapshot.hpp"

namespace Penumbra::Render {

// One tick of what the pause reads, as HELD states: the menu finds the presses
// itself, so a key held over many ticks acts once.
struct PauseInput {
    bool inPlayScene = false;       // the layer's call: the Machine's loop is levelLoop or pvpLoop
    bool focused = true;            // the window has the focus
    bool open = false;              // the original's cancel for player 1: K_ESC, JK_09 on his pad
    bool up = false;
    bool down = false;
    bool confirm = false;           // Enter (not Alt+Enter), A, Start
    bool back = false;              // Esc, B, Back
    glm::vec2 pointer{0.0f};        // the cursor, logical screen pixels (InputFrame::cursor)
    bool click = false;             // the left mouse button
    glm::vec2 screen{1024.0f, 768.0f};   // the logical screen the menu is laid out on
};

// What the layer does this tick.
struct PauseStep {
    bool tick = true;               // run Machine::Frame
    bool sendCancel = false;        // hold K_ESC down in this tick's frame, after FilterForGame
    bool opened = false;            // the pause opened: duck the music
    bool closed = false;            // it closed: restore the music
};

class PauseMenu {
public:
    enum Item : int { kResume = 0, kMainMenu = 1, kItemCount = 2 };

    // The music's share while paused, on top of the player's own volume.
    static constexpr float kMusicDuck = 0.4f;
    // How far the stick must lean to move the selection (after the dead zone).
    static constexpr float kStickPress = 0.5f;

    // The texts, in the original's language (cp1252; strings.json has the
    // English, looked up at the draw boundary like every other text).
    static constexpr const char* kTitle = "Pausado";
    static constexpr const char* kItemText[kItemCount] = {"Continuar", "Menu principal"};
    static constexpr const char* kFont = "Arial Narrow";
    static constexpr float kTitleSize = 40.0f;   // showData's title
    static constexpr float kItemSize = 30.0f;

    // Where the overlay's parts are, logical screen pixels (+y down).
    struct Layout {
        glm::vec2 panelMin{0.0f};
        glm::vec2 panelMax{0.0f};
        glm::vec2 title{0.0f};
        std::array<glm::vec2, kItemCount> rowMin{};   // the rows the pointer hits and the highlight fills
        std::array<glm::vec2, kItemCount> rowMax{};
        std::array<glm::vec2, kItemCount> text{};
    };
    static Layout ComputeLayout(const glm::vec2& screen);

    // The pause's reading of one mapped frame: K_ESC and player 1's pad
    // (`player1Pad` = getPlayerJoystick(0)) - its Back opens and resumes, B
    // resumes, A and Start confirm, its stick or D-pad moves - with Enter,
    // the arrow keys and the mouse. Player 2's pad is not read: under the
    // keyboard second player it carries I/J/K/L and Backspace.
    static PauseInput InputFrom(const Eth::InputFrame& frame, int player1Pad, bool inPlayScene,
                                const glm::vec2& screen);

    // settings.pauseOnFocusLoss (and off for a --fixed-step run).
    void SetAutoPause(bool on) { m_autoPause = on; }
    bool AutoPause() const { return m_autoPause; }

    // Once a tick, before Machine::Frame would run.
    PauseStep Update(const PauseInput& input);

    bool Paused() const { return m_paused; }
    int Selected() const { return m_selected; }
    float MusicScale() const { return m_paused ? kMusicDuck : 1.0f; }

    // The overlay, appended to `out` - nothing unless paused - laid out on the
    // last Update's screen. For HudRenderer::Draw's `extra`.
    void AppendOverlay(std::vector<Eth::HudCmd>& out) const;

    // On every tick that runs Machine::Frame, on the frame it is given, before
    // a cancel is added: after a pause, holds up what was pressed in it until
    // released (see the header comment), and remembers what the game saw.
    void FilterForGame(Eth::InputFrame& frame);

private:
    struct Held {
        std::array<bool, Eth::K_COUNT> keys{};
        std::array<std::array<bool, 32>, Eth::kMaxJoysticks> buttons{};
        std::array<bool, Eth::kMaxJoysticks> stick{};   // leaning at all
    };

    // The item under the pointer, or -1.
    int itemAt(const glm::vec2& pointer) const;
    void close(PauseStep& step);

    bool m_autoPause = true;
    bool m_paused = false;
    int m_selected = kResume;
    glm::vec2 m_screen{1024.0f, 768.0f};

    bool m_havePrevious = false;
    PauseInput m_previous;
    int m_focusGrace = 0;           // ticks until a click counts again after the focus returned

    // FilterForGame: what the game last saw, what it may not see yet.
    bool m_filterPending = false;
    Held m_seen;
    Held m_masked;
};

} // namespace Penumbra::Render
