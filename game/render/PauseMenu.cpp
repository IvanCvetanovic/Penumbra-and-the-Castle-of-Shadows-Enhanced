#include "render/PauseMenu.hpp"

#include <glm/glm.hpp>

#include <cstddef>
#include <utility>

namespace Penumbra::Render {

namespace {

using Eth::HudCmd;

// The panel: small, and centred on whatever the logical screen is (1024 wide
// in 4:3, wider in the widescreen view; always 768 tall). Arial Narrow 30 in a
// 44-pixel row leaves 7 above and below; the widest item, "Menu principal",
// is about 170 pixels at that size.
constexpr glm::vec2 kPanelSize{340.0f, 210.0f};
constexpr glm::vec2 kTitleOffset{20.0f, 16.0f};
constexpr glm::vec2 kFirstRowOffset{12.0f, 84.0f};
constexpr float kRowStep = 54.0f;
// How far (logical pixels, one tick to the next) the pointer must move to select.
constexpr float kPointerMovePixels = 2.0f;
constexpr glm::vec2 kRowSize{316.0f, 44.0f};
constexpr glm::vec2 kRowTextOffset{14.0f, 7.0f};

// Alphas: the scripts' Switch shows the current row at 255 and the others at
// 100 (switch.as:94); a little more here, since these rows are the only way on.
constexpr Eth::uint8 kSelectedAlpha = 255;
constexpr Eth::uint8 kOtherAlpha = 110;

// Eth::ARGB, at compile time.
constexpr Eth::uint Argb(Eth::uint a, Eth::uint r, Eth::uint g, Eth::uint b) {
    return (a << 24) | (r << 16) | (g << 8) | b;
}
constexpr Eth::uint kDim = Argb(150, 0, 0, 0);
// showData's panel (menu.as:217-228): black, 190 at the top to 55 at the bottom.
constexpr Eth::uint kPanelTop = Argb(190, 0, 0, 0);
constexpr Eth::uint kPanelBottom = Argb(55, 0, 0, 0);
constexpr Eth::uint kHighlight = Argb(40, 203, 203, 228);

// A click that brings the window back to the front is not a choice: a player
// who clicked into the game to come back must not land on Main menu. Clicks
// count again this many ticks after the focus returns.
constexpr int kFocusGraceTicks = 15;

HudCmd Rectangle(const glm::vec2& pos, const glm::vec2& size, Eth::uint top, Eth::uint bottom) {
    HudCmd cmd;
    cmd.kind = HudCmd::Kind::Rectangle;
    cmd.pos = pos;
    cmd.size = size;
    // Every corner set: HudCmd's corners default to opaque white.
    cmd.color = top;
    cmd.color1 = top;
    cmd.color2 = bottom;
    cmd.color3 = bottom;
    return cmd;
}

// util.as:450 shadowText: a black copy at half the alpha, offset by a tenth of
// the size, under the text in the scripts' (203,203,228).
void ShadowText(std::vector<HudCmd>& out, const glm::vec2& pos, const char* text, float size, Eth::uint8 alpha) {
    HudCmd shadow;
    shadow.kind = HudCmd::Kind::Text;
    shadow.pos = pos + glm::vec2(size * 0.1f, size * 0.1f);
    shadow.text = text;
    shadow.font = PauseMenu::kFont;
    shadow.fontSize = size;
    shadow.color = Eth::ARGB(static_cast<Eth::uint8>(alpha / 2), 0, 0, 0);
    HudCmd front = shadow;
    front.pos = pos;
    front.color = Eth::ARGB(alpha, 203, 203, 228);
    out.push_back(std::move(shadow));
    out.push_back(std::move(front));
}


} // namespace

PauseMenu::Layout PauseMenu::ComputeLayout(const glm::vec2& screen) {
    Layout layout;
    // Whole pixels, so the text lands on them as the scripts' does.
    layout.panelMin = glm::floor((screen - kPanelSize) * 0.5f);
    layout.panelMax = layout.panelMin + kPanelSize;
    layout.title = layout.panelMin + kTitleOffset;
    for (int i = 0; i < kItemCount; ++i) {
        const auto index = static_cast<std::size_t>(i);
        layout.rowMin[index] = layout.panelMin + kFirstRowOffset + glm::vec2(0.0f, kRowStep * static_cast<float>(i));
        layout.rowMax[index] = layout.rowMin[index] + kRowSize;
        layout.text[index] = layout.rowMin[index] + kRowTextOffset;
    }
    return layout;
}

PauseInput PauseMenu::InputFrom(const Eth::InputFrame& frame, int player1Pad, bool inPlayScene,
                                const glm::vec2& screen) {
    using namespace Eth;
    PauseInput input;
    input.inPlayScene = inPlayScene;
    input.focused = frame.hasFocus;
    input.screen = screen;
    input.pointer = frame.cursor;
    input.click = frame.keys[K_LMOUSE];
    input.open = frame.keys[K_ESC];
    input.back = frame.keys[K_ESC];
    // Alt+Enter is the window's (menu.as:108), as getConfirmButtonStatus has it.
    input.confirm = frame.keys[K_ENTER] && !frame.keys[K_ALT];
    input.up = frame.keys[K_UP];
    input.down = frame.keys[K_DOWN];
    if (player1Pad >= 0 && player1Pad < kMaxJoysticks) {
        const InputFrame::Pad& pad = frame.pads[static_cast<std::size_t>(player1Pad)];
        if (pad.connected) {
            input.open = input.open || pad.buttons[JK_09];
            input.back = input.back || pad.buttons[JK_09] || pad.buttons[JK_02];
            input.confirm = input.confirm || pad.buttons[JK_03] || pad.buttons[JK_10];
            input.up = input.up || pad.xy.y <= -kStickPress;
            input.down = input.down || pad.xy.y >= kStickPress;
        }
    }
    return input;
}

int PauseMenu::itemAt(const glm::vec2& pointer) const {
    const Layout layout = ComputeLayout(m_screen);
    for (int i = 0; i < kItemCount; ++i) {
        const auto index = static_cast<std::size_t>(i);
        // Strictly inside, as the scripts' Switch tests its rows (switch.as:86).
        if (pointer.x > layout.rowMin[index].x && pointer.y > layout.rowMin[index].y &&
            pointer.x < layout.rowMax[index].x && pointer.y < layout.rowMax[index].y) {
            return i;
        }
    }
    return -1;
}

void PauseMenu::close(PauseStep& step) {
    m_paused = false;
    step.tick = true;
    step.closed = true;
    m_filterPending = true;
}

PauseStep PauseMenu::Update(const PauseInput& input) {
    PauseStep step;
    m_screen = input.screen;

    // Presses are this tick against the last; the first tick has none.
    const PauseInput& was = m_havePrevious ? m_previous : input;
    const bool openPressed = input.open && !was.open;
    const bool upPressed = input.up && !was.up;
    const bool downPressed = input.down && !was.down;
    const bool confirmPressed = input.confirm && !was.confirm;
    const bool backPressed = input.back && !was.back;
    // Moved by the player's hand, not by a rounding wobble: a pointer that
    // shifts by a pixel or less (a window nudged under a resting mouse, a
    // DPI round trip) must not take the selection from the keys.
    const bool pointerMoved = glm::length(input.pointer - was.pointer) >= kPointerMovePixels;
    const bool focusLost = was.focused && !input.focused;
    if (input.focused && !was.focused) m_focusGrace = kFocusGraceTicks;
    const bool clickPressed = input.click && !was.click && input.focused && m_focusGrace == 0;
    if (m_focusGrace > 0) --m_focusGrace;

    if (!m_paused) {
        if (input.inPlayScene && (openPressed || (m_autoPause && focusLost))) {
            // The tick that opens it is not run either: the Esc that opened it
            // must never reach escToGoToMenu.
            m_paused = true;
            m_selected = kResume;
            step.tick = false;
            step.opened = true;
        }
    } else if (!input.inPlayScene || backPressed) {
        // Out of play under it (nothing the layer does today), or Esc/B/Back.
        close(step);
    } else {
        step.tick = false;
        if (upPressed) m_selected = (m_selected + kItemCount - 1) % kItemCount;
        if (downPressed) m_selected = (m_selected + 1) % kItemCount;
        // The pointer selects only when it moves, so a mouse left lying over a
        // row does not undo the keys.
        const int hovered = itemAt(input.pointer);
        if (pointerMoved && hovered >= 0) m_selected = hovered;
        int chosen = -1;
        if (clickPressed && hovered >= 0) {
            m_selected = hovered;
            chosen = hovered;
        } else if (confirmPressed) {
            chosen = m_selected;
        }
        if (chosen == kResume) {
            close(step);
        } else if (chosen == kMainMenu) {
            close(step);
            step.sendCancel = true;
        }
    }

    m_previous = input;
    m_havePrevious = true;
    // The cancel the game is fed this tick is not a press of the key that
    // opens the pause: taken as already held, it needs a fresh press.
    if (step.sendCancel) m_previous.open = true;
    return step;
}

void PauseMenu::AppendOverlay(std::vector<Eth::HudCmd>& out) const {
    if (!m_paused) return;
    const Layout layout = ComputeLayout(m_screen);
    out.push_back(Rectangle(glm::vec2(0.0f), m_screen, kDim, kDim));
    out.push_back(Rectangle(layout.panelMin, layout.panelMax - layout.panelMin, kPanelTop, kPanelBottom));
    const auto selected = static_cast<std::size_t>(m_selected);
    out.push_back(Rectangle(layout.rowMin[selected], layout.rowMax[selected] - layout.rowMin[selected], kHighlight,
                            kHighlight));
    ShadowText(out, layout.title, kTitle, kTitleSize, kSelectedAlpha);
    for (int i = 0; i < kItemCount; ++i) {
        const auto index = static_cast<std::size_t>(i);
        ShadowText(out, layout.text[index], kItemText[index], kItemSize, i == m_selected ? kSelectedAlpha : kOtherAlpha);
    }
}

void PauseMenu::FilterForGame(Eth::InputFrame& frame) {
    const glm::vec2 centred(0.0f);
    if (m_filterPending) {
        // The first tick after a pause: whatever is down now and was not down
        // when the game last looked was pressed in the pause.
        m_filterPending = false;
        for (std::size_t k = 0; k < frame.keys.size(); ++k) {
            if (frame.keys[k] && !m_seen.keys[k]) m_masked.keys[k] = true;
        }
        for (std::size_t p = 0; p < frame.pads.size(); ++p) {
            for (std::size_t b = 0; b < frame.pads[p].buttons.size(); ++b) {
                if (frame.pads[p].buttons[b] && !m_seen.buttons[p][b]) m_masked.buttons[p][b] = true;
            }
            if (frame.pads[p].xy != centred && !m_seen.stick[p]) m_masked.stick[p] = true;
        }
    }

    // Held up until released, across a scene change too: the Enter that picked
    // Main menu is still down on the menu's first frame.
    for (std::size_t k = 0; k < frame.keys.size(); ++k) {
        if (!m_masked.keys[k]) continue;
        if (frame.keys[k]) frame.keys[k] = false;
        else m_masked.keys[k] = false;
    }
    for (std::size_t p = 0; p < frame.pads.size(); ++p) {
        for (std::size_t b = 0; b < frame.pads[p].buttons.size(); ++b) {
            if (!m_masked.buttons[p][b]) continue;
            if (frame.pads[p].buttons[b]) frame.pads[p].buttons[b] = false;
            else m_masked.buttons[p][b] = false;
        }
        if (m_masked.stick[p]) {
            if (frame.pads[p].xy != centred) frame.pads[p].xy = centred;
            else m_masked.stick[p] = false;
        }
    }

    for (std::size_t k = 0; k < frame.keys.size(); ++k) m_seen.keys[k] = frame.keys[k];
    for (std::size_t p = 0; p < frame.pads.size(); ++p) {
        m_seen.buttons[p] = frame.pads[p].buttons;
        m_seen.stick[p] = frame.pads[p].xy != centred;
    }
}

} // namespace Penumbra::Render
