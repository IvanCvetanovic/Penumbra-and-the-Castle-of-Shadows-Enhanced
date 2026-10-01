// ENHANCEMENT E28: the touch controls' editor (render/TouchEditor.hpp; docs/planning/2026-10-01-e28-adjustable-touch-controls.md says what it is for).
//
// Pure, like PauseMenu: it is told this tick's fingers and what the controls' layout is, and it answers what changed
// and what to draw. Every timer is a tick counter (the pulse, the outlines' breathing): no wall clock, no fonts, no audio.

#include "render/TouchEditor.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <system_error>
#include <utility>

namespace Penumbra::Render {

namespace {

using Eth::HudCmd;

// Eth::ARGB, at compile time.
constexpr Eth::uint Argb(Eth::uint a, Eth::uint r, Eth::uint g, Eth::uint b) {
    return (a << 24) | (r << 16) | (g << 8) | b;
}

// Opaque on purpose: at 245 the options screen's panel still ghosted through on a wide window (a capture at
// 1280x720 showed "Video Options" behind the title), and the controls may be drawn as faint as a fifth.
constexpr Eth::uint kBackdropTop = Argb(255, 30, 26, 44);
constexpr Eth::uint kBackdropBottom = Argb(255, 14, 12, 22);

// The title's origin: from the back arrow's right edge and the safe area's top.
constexpr float kTitleGap = 20.0f;
constexpr float kTitleDrop = 53.0f;

// The readouts. The pure class cannot measure text, so the number is placed as if "1.0" at Arial Narrow 40 were
// 46 px wide and 40 tall (origin (cx - 23, row - 20)): an assumption, and a pulse grows it about
// that centre. The signs sit a fixed 15 px either side of the number's box.
constexpr float kNumberHalfWidth = 23.0f;
constexpr float kNumberHalfHeight = 20.0f;
constexpr float kMinusLeft = 54.0f;    // the "-"'s origin, left of the centre
constexpr float kPlusRight = 38.0f;    // the "+"'s origin, right of the centre
constexpr float kPulseGrew = 1.2f;
constexpr float kPulseShrank = 0.8f;
constexpr Eth::uint8 kSignAlpha = 90;  // Magic Rampage's faint 0x44

// The tiles' alphas: a tile at its limit and a restore with nothing to restore are dimmed, never gone.
constexpr Eth::uint8 kTileAlpha = 255;
constexpr Eth::uint8 kLimitAlpha = 90;
constexpr Eth::uint8 kRestoreIdleAlpha = 70;
constexpr float kPressedInset = 3.0f;   // a held tile is drawn this much smaller on every side

// The outlines round every movable control while unlocked: they breathe between 40 and 180, a grabbed one is thicker
// and solid.
constexpr float kOutlineWidth = 2.0f;
constexpr float kGrabbedOutlineWidth = 3.0f;
constexpr double kBreathBase = 110.0;
constexpr double kBreathSwing = 70.0;
constexpr double kPi = 3.14159265358979323846;

// A missing image is drawn as a square this grey (TouchControls' Plain).
constexpr Eth::uint8 kPlainGrey = 90;

// images/options/<name>.png, by TouchEditor::m_images' index.
constexpr const char* kArtNames[] = {"arrow_left", "edit_shrink", "edit_enlarge", "edit_dim",
                                     "edit_brighten", "edit_locked", "edit_unlocked", "edit_restore"};
constexpr std::size_t kArtLockedIndex = 5;
constexpr std::size_t kArtUnlockedIndex = 6;

std::size_t ArtIndexOf(const TouchEditWidget widget, const bool locked) {
    switch (widget) {
        case TouchEditWidget::Back: return 0;
        case TouchEditWidget::SizeLess: return 1;
        case TouchEditWidget::SizeMore: return 2;
        case TouchEditWidget::OpacityLess: return 3;
        case TouchEditWidget::OpacityMore: return 4;
        case TouchEditWidget::Lock: return locked ? kArtLockedIndex : kArtUnlockedIndex;
        case TouchEditWidget::Restore: return 7;
        case TouchEditWidget::Count: break;
    }
    return 0;
}

bool StrictlyInside(const glm::vec2& point, const TouchLayout::Box& box) {
    return point.x > box.min.x && point.x < box.max.x && point.y > box.min.y && point.y < box.max.y;
}

bool Within(const glm::vec2& point, const TouchLayout::Box& box) {
    return point.x >= box.min.x && point.x <= box.max.x && point.y >= box.min.y && point.y <= box.max.y;
}

// `inner` lies wholly inside `outer`, edges included.
bool WhollyInside(const TouchLayout::Box& inner, const TouchLayout::Box& outer) {
    return inner.min.x >= outer.min.x && inner.min.y >= outer.min.y && inner.max.x <= outer.max.x &&
           inner.max.y <= outer.max.y;
}

bool IsDown(const std::vector<TouchContact>& down, const int id) {
    return std::any_of(down.begin(), down.end(),
                       [id](const TouchContact& contact) { return contact.down && contact.id == id; });
}

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

// A solid rectangle that stays exactly where it is drawn: the open sides' stretch (HudCmd::stretchToSides) would turn a
// thin line that meets x = 0 into a band across the whole shown width.
HudCmd Solid(const glm::vec2& pos, const glm::vec2& size, Eth::uint8 alpha, Eth::uint8 grey) {
    HudCmd cmd = Rectangle(pos, size, Eth::ARGB(alpha, grey, grey, grey), Eth::ARGB(alpha, grey, grey, grey));
    cmd.stretchToSides = false;
    return cmd;
}

HudCmd Sprite(const std::string& path, const glm::vec2& min, const glm::vec2& size, Eth::uint8 alpha) {
    HudCmd cmd;
    // Stretched to the box, as the controls' art is (the art has at least the box's logical pixels).
    cmd.kind = HudCmd::Kind::ShapedSprite;
    cmd.sprite = path;
    cmd.pos = min;
    cmd.size = size;
    cmd.color = Eth::ARGB(alpha, 255, 255, 255);
    return cmd;
}

// util.as:450 shadowText, as PauseMenu draws it: a black copy at half the alpha, a tenth of the size down and right,
// under the text in the scripts' (203,203,228). `rtlRight` is where a right-to-left language ends it (0: none, as the
// digits and signs have - they are not translated, and a nonzero rtlRight on their shadow would put it at the box's edge).
void ShadowText(std::vector<HudCmd>& out, const glm::vec2& pos, const float rtlRight, const std::string& text,
                const float size, const Eth::uint8 alpha) {
    HudCmd shadow;
    shadow.kind = HudCmd::Kind::Text;
    shadow.pos = pos + glm::vec2(size * 0.1f, size * 0.1f);
    shadow.rtlRight = rtlRight > 0.0f ? rtlRight + size * 0.1f : 0.0f;
    shadow.text = text;
    shadow.font = TouchEditor::kFont;
    shadow.fontSize = size;
    shadow.color = Eth::ARGB(static_cast<Eth::uint8>(alpha / 2), 0, 0, 0);
    HudCmd front = shadow;
    front.pos = pos;
    front.rtlRight = rtlRight;
    front.color = Eth::ARGB(alpha, 203, 203, 228);
    out.push_back(std::move(shadow));
    out.push_back(std::move(front));
}

// One decimal, always, from the integer tenths so that no locale can change the point: 1.0, 0.4, 1.2.
std::string Readout(const float value) {
    const long tenths = std::lround(value * 10.0f);
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
}

// 110 + 70 sin(2 pi t / 48), t the Updates since Open. (The argument is reduced first: a long session never feeds sin a
// large number.)
Eth::uint8 BreathAlpha(const unsigned tick) {
    const double turn = static_cast<double>(tick % TouchEditor::kBreathTicks) / static_cast<double>(TouchEditor::kBreathTicks);
    return static_cast<Eth::uint8>(std::lround(kBreathBase + kBreathSwing * std::sin(2.0 * kPi * turn)));
}

// Four thin rectangles just inside `box`: its top and bottom rows, then its left and right columns between them.
void AppendOutline(std::vector<HudCmd>& out, const TouchLayout::Box& box, const float width, const Eth::uint8 alpha) {
    const glm::vec2 size = box.Size();
    const float t = std::max(0.0f, std::min(width, std::min(size.x, size.y) * 0.5f));
    const float side = std::max(0.0f, size.y - 2.0f * t);
    out.push_back(Solid(box.min, glm::vec2(size.x, t), alpha, 255));
    out.push_back(Solid(glm::vec2(box.min.x, box.max.y - t), glm::vec2(size.x, t), alpha, 255));
    out.push_back(Solid(glm::vec2(box.min.x, box.min.y + t), glm::vec2(t, side), alpha, 255));
    out.push_back(Solid(glm::vec2(box.max.x - t, box.min.y + t), glm::vec2(t, side), alpha, 255));
}

} // namespace

TouchEditor::Layout TouchEditor::ComputeLayout(const TouchGeometry& geometry) {
    Layout layout;
    const glm::vec2 areaMin = geometry.AreaMin();
    const glm::vec2 areaMax = geometry.AreaMax();
    const TouchInsets& safe = geometry.safeArea;
    // The shown area is symmetric about the logical screen's middle, so the centred widgets sit at the same x on every
    // window; the corner-anchored back arrow and title follow the area's corner (and its notch).
    const glm::vec2 lo = areaMin + glm::vec2(std::max(0.0f, safe.left), std::max(0.0f, safe.top));
    layout.centreX = (areaMin.x + areaMax.x) * 0.5f;
    for (std::size_t row = 0; row < kRowY.size(); ++row) layout.rowY[row] = lo.y + kRowY[row];

    const float half = kTile * 0.5f;
    const auto tile = [half](const float centreX, const float centreY) {
        return TouchLayout::Box{glm::vec2(centreX - half, centreY - half), glm::vec2(centreX + half, centreY + half)};
    };
    const auto widget = [&layout](TouchEditWidget which) -> TouchLayout::Box& {
        return layout.widget[static_cast<std::size_t>(which)];
    };
    TouchLayout::Box& back = widget(TouchEditWidget::Back);
    back.min = lo + glm::vec2(kBackInset);
    back.max = back.min + glm::vec2(kBackSize);
    widget(TouchEditWidget::SizeLess) = tile(layout.centreX - kPairOffset, layout.rowY[0]);
    widget(TouchEditWidget::SizeMore) = tile(layout.centreX + kPairOffset, layout.rowY[0]);
    widget(TouchEditWidget::OpacityLess) = tile(layout.centreX - kPairOffset, layout.rowY[1]);
    widget(TouchEditWidget::OpacityMore) = tile(layout.centreX + kPairOffset, layout.rowY[1]);
    widget(TouchEditWidget::Lock) = tile(layout.centreX - kLockOffset, layout.rowY[2]);
    widget(TouchEditWidget::Restore) = tile(layout.centreX + kLockOffset, layout.rowY[2]);

    layout.title = glm::vec2(back.max.x + kTitleGap, lo.y + kTitleDrop);
    // A right-to-left title ends clear of the Pause button, which hangs from the HUD frame's corner when that
    // lies further in than the notch.
    const float right = std::max(std::max(0.0f, safe.right), std::max(0.0f, geometry.hudFrame.right));
    layout.titleRight = areaMax.x - right - kTitleRightInset;
    return layout;
}

void TouchEditor::SetImageRoot(const std::filesystem::path& dataDir) {
    m_imageRoot = dataDir;
    for (std::string& image : m_images) image.clear();
    if (m_imageRoot.empty()) return;
    for (std::size_t i = 0; i < kArtCount; ++i) {
        const std::filesystem::path path = m_imageRoot / "images" / "options" / (std::string(kArtNames[i]) + ".png");
        std::error_code ec;
        // Absolute (when the data folder is): TextureCache takes an absolute path as it is.
        if (std::filesystem::is_regular_file(path, ec)) m_images[i] = path.generic_string();
    }
}

void TouchEditor::Open(const TouchTuning& tuning, const std::vector<TouchContact>& downNow, const bool startUnlocked) {
    m_open = true;
    m_locked = !startUnlocked;
    m_tuning = tuning.Clamped();
    m_saved = m_tuning;
    m_fingers.clear();
    // The finger that tapped the entry is still down: dead until it lifts, so it can neither press a tile nor grab a
    // control.
    for (const TouchContact& contact : downNow) {
        if (!contact.down || contact.id < 0) continue;
        Finger dead;
        dead.use = Use::Dead;
        dead.position = contact.position;
        m_fingers[contact.id] = dead;
    }
    m_pulse = {};
    m_ticks = 0;
    m_prevClose = false;
    m_haveInput = false;
    m_geometry = TouchGeometry{};
}

bool TouchEditor::IsOpen() const { return m_open; }

const TouchTuning& TouchEditor::Tuning() const { return m_tuning; }

bool TouchEditor::Locked() const { return m_locked; }

bool TouchEditor::grabbed(const TouchControl control) const {
    return std::any_of(m_fingers.begin(), m_fingers.end(), [control](const auto& entry) {
        return entry.second.use == Use::Control && entry.second.control == control;
    });
}

void TouchEditor::dropGrabs() {
    for (auto& entry : m_fingers) {
        if (entry.second.use == Use::Control) entry.second.use = Use::None;
    }
}

void TouchEditor::commit(TouchEditStep& step) {
    step.commit = true;
    m_saved = m_tuning;
}

void TouchEditor::commitIfDirty(TouchEditStep& step) {
    if (m_tuning != m_saved) commit(step);
}

void TouchEditor::stepSize(const int direction, TouchEditStep& step) {
    const float next = TouchTuning::StepSize(m_tuning.size, direction);
    if (next == m_tuning.size) return;   // at its limit: the tap does nothing
    m_tuning.size = next;
    step.changed = true;
    m_locked = true;                     // Magic Rampage's refreshScreenPad: a size step re-locks
    dropGrabs();
    m_pulse[0] = Pulse{kPulseTicks, direction};
    commit(step);
}

void TouchEditor::stepOpacity(const int direction, TouchEditStep& step) {
    const float next = TouchTuning::StepOpacity(m_tuning.opacity, direction);
    if (next == m_tuning.opacity) return;
    m_tuning.opacity = next;             // the lock is left as it is
    step.changed = true;
    m_pulse[1] = Pulse{kPulseTicks, direction};
    commit(step);
}

TouchEditStep TouchEditor::Update(const TouchEditInput& input, const TouchControls& controls) {
    TouchEditStep step;
    if (!m_open) return step;
    m_geometry = input.geometry;
    const Layout layout = ComputeLayout(input.geometry);
    const std::vector<TouchContact> noFingers;
    const std::vector<TouchContact>& down = input.down != nullptr ? *input.down : noFingers;

    // The close key's edge. The first Update after Open takes it as already seen: a key held through the open does not
    // close it.
    const bool closeEdge = input.close && m_haveInput && !m_prevClose;
    m_prevClose = input.close;
    m_haveInput = true;
    for (Pulse& pulse : m_pulse) {
        if (pulse.ticksLeft > 0) --pulse.ticksLeft;
    }

    // 1. Lifts. A tile acts when its finger lifts inside it (a completed tap, as Magic Rampage's isPressed); a drag
    // ends. Several taps in one tick act in finger order.
    std::vector<TouchEditWidget> taps;
    for (auto it = m_fingers.begin(); it != m_fingers.end();) {
        if (IsDown(down, it->first)) {
            ++it;
            continue;
        }
        const Finger& finger = it->second;
        if (finger.use == Use::Widget) {
            if (StrictlyInside(finger.position, layout.widget[static_cast<std::size_t>(finger.widget)])) {
                taps.push_back(finger.widget);
            }
        } else if (finger.use == Use::Control) {
            const std::size_t index = static_cast<std::size_t>(finger.control);
            // A control let go with its whole grab area under a tile or the back arrow could never be
            // grabbed again (a widget wins every hit test), so it goes back to where it was when it was taken.
            const TouchLayout::Box grab = TouchControls::GrabBox(finger.control, controls.Layout()[finger.control]);
            const bool buried = std::any_of(layout.widget.begin(), layout.widget.end(),
                                            [&grab](const TouchLayout::Box& box) { return WhollyInside(grab, box); });
            if (buried && m_tuning.move[index] != finger.startMove) {
                m_tuning.move[index] = finger.startMove;
                step.changed = true;
            }
            // Once per gesture end, and only if the tuning is not what was last saved (a revert may have made it so).
            commitIfDirty(step);
        }
        it = m_fingers.erase(it);
    }

    // 2. New fingers, and where the known ones are now.
    for (const TouchContact& contact : down) {
        if (!contact.down || contact.id < 0) continue;
        const auto known = m_fingers.find(contact.id);
        if (known != m_fingers.end()) {
            known->second.moved = contact.position != known->second.position;
            known->second.position = contact.position;
            continue;
        }
        Finger finger;
        finger.position = contact.position;
        // The widgets win over the controls behind them.
        for (int w = 0; w < kTouchEditWidgetCount; ++w) {
            if (StrictlyInside(contact.position, layout.widget[static_cast<std::size_t>(w)])) {
                finger.use = Use::Widget;
                finger.widget = static_cast<TouchEditWidget>(w);
                break;
            }
        }
        if (finger.use == Use::None && !m_locked) {
            // The visible movable control whose grab area holds it, the nearer centre if two, one a finger already
            // holds skipped. While locked a finger on a control does nothing at all.
            float bestDistance = std::numeric_limits<float>::max();
            for (int i = 0; i < kTouchControlCount; ++i) {
                const TouchControl control = static_cast<TouchControl>(i);
                if (!TouchControls::Movable(control) || !controls.Visible(control) || grabbed(control)) continue;
                const TouchLayout::Box area = TouchControls::GrabBox(control, controls.Layout()[control]);
                if (!Within(contact.position, area)) continue;
                const glm::vec2 offset = contact.position - area.Centre();
                const float distance = glm::dot(offset, offset);
                if (distance >= bestDistance) continue;
                bestDistance = distance;
                finger.use = Use::Control;
                finger.control = control;
                finger.grabOffset = contact.position - controls.Layout()[control].min;
                finger.startMove = m_tuning.move[static_cast<std::size_t>(i)];
            }
        }
        m_fingers.emplace(contact.id, finger);
    }

    // 3. The drag: the control keeps its offset from the finger, wherever the layout lets it go. Only a finger that
    // moved: a grab that merely lands must not rewrite a stored move the layout had clamped.
    for (auto& entry : m_fingers) {
        const Finger& finger = entry.second;
        if (finger.use != Use::Control || !finger.moved) continue;
        const TouchMove wanted = TouchControls::MoveFor(controls.BaseManifest(), m_tuning, finger.control,
                                                        finger.position - finger.grabOffset, input.geometry);
        TouchMove& current = m_tuning.move[static_cast<std::size_t>(finger.control)];
        if (wanted == current) continue;
        current = wanted;
        step.changed = true;
    }

    // 4. The taps.
    bool closing = false;
    for (const TouchEditWidget tap : taps) {
        switch (tap) {
            case TouchEditWidget::Back: closing = true; break;
            case TouchEditWidget::SizeLess: stepSize(-1, step); break;
            case TouchEditWidget::SizeMore: stepSize(+1, step); break;
            case TouchEditWidget::OpacityLess: stepOpacity(-1, step); break;
            case TouchEditWidget::OpacityMore: stepOpacity(+1, step); break;
            case TouchEditWidget::Lock:
                m_locked = !m_locked;
                if (m_locked) {
                    dropGrabs();
                    commitIfDirty(step);
                }
                break;
            case TouchEditWidget::Restore:
                // Always locks; puts the moves back only when there are any (size and opacity stay).
                m_locked = true;
                dropGrabs();
                if (m_tuning.Moved()) {
                    for (TouchMove& move : m_tuning.move) move = TouchMove{};
                    step.changed = true;
                    commit(step);
                }
                break;
            case TouchEditWidget::Count: break;
        }
        if (closing) break;
    }

    // 5. Closing ends every gesture and keeps whatever is unsaved.
    if (closing || closeEdge) {
        commitIfDirty(step);
        step.closed = true;
        m_open = false;
        m_fingers.clear();
        return step;
    }
    ++m_ticks;
    return step;
}

void TouchEditor::AppendOverlay(const TouchControls& controls, std::vector<Eth::HudCmd>& out) const {
    if (!m_open) return;
    // Before the first Update the geometry is the default (4:3, no notch): at most one drawn frame, since the layer's
    // next tick feeds Update.
    const Layout layout = ComputeLayout(m_geometry);
    const glm::vec2 areaMin = m_geometry.AreaMin();
    const glm::vec2 areaMax = m_geometry.AreaMax();

    // 1. The backdrop over everything shown, the wide sides of a wide menu included.
    out.push_back(Rectangle(areaMin, areaMax - areaMin, kBackdropTop, kBackdropBottom));

    // 2. The real controls, at the player's size, opacity and place.
    controls.AppendOverlay(out);

    // 3. Where a finger can take a control, while unlocked.
    if (!m_locked) {
        const Eth::uint8 breath = BreathAlpha(m_ticks);
        for (int i = 0; i < kTouchControlCount; ++i) {
            const TouchControl control = static_cast<TouchControl>(i);
            if (!TouchControls::Movable(control) || !controls.Visible(control)) continue;
            const TouchLayout::Box area = TouchControls::GrabBox(control, controls.Layout()[control]);
            if (grabbed(control)) AppendOutline(out, area, kGrabbedOutlineWidth, 255);
            else AppendOutline(out, area, kOutlineWidth, breath);
        }
    }

    // 4. The seven widgets, over the controls.
    const bool sizeLowest = TouchTuning::StepSize(m_tuning.size, -1) == m_tuning.size;
    const bool sizeHighest = TouchTuning::StepSize(m_tuning.size, +1) == m_tuning.size;
    const bool opacityLowest = TouchTuning::StepOpacity(m_tuning.opacity, -1) == m_tuning.opacity;
    const bool opacityHighest = TouchTuning::StepOpacity(m_tuning.opacity, +1) == m_tuning.opacity;
    for (int w = 0; w < kTouchEditWidgetCount; ++w) {
        const TouchEditWidget widget = static_cast<TouchEditWidget>(w);
        const TouchLayout::Box& box = layout.widget[static_cast<std::size_t>(w)];
        Eth::uint8 alpha = kTileAlpha;
        switch (widget) {
            case TouchEditWidget::SizeLess: if (sizeLowest) alpha = kLimitAlpha; break;
            case TouchEditWidget::SizeMore: if (sizeHighest) alpha = kLimitAlpha; break;
            case TouchEditWidget::OpacityLess: if (opacityLowest) alpha = kLimitAlpha; break;
            case TouchEditWidget::OpacityMore: if (opacityHighest) alpha = kLimitAlpha; break;
            case TouchEditWidget::Restore: if (!m_tuning.Moved()) alpha = kRestoreIdleAlpha; break;
            case TouchEditWidget::Back:
            case TouchEditWidget::Lock:
            case TouchEditWidget::Count: break;
        }
        // Held: a finger that landed on it is down and still inside.
        const bool held = std::any_of(m_fingers.begin(), m_fingers.end(), [&](const auto& entry) {
            return entry.second.use == Use::Widget && entry.second.widget == widget &&
                   StrictlyInside(entry.second.position, box);
        });
        const glm::vec2 inset = held ? glm::vec2(kPressedInset) : glm::vec2(0.0f);
        const glm::vec2 min = box.min + inset;
        const glm::vec2 size = box.Size() - inset * 2.0f;
        const std::string& image = m_images[ArtIndexOf(widget, m_locked)];
        if (!image.empty()) out.push_back(Sprite(image, min, size, alpha));
        else out.push_back(Solid(min, size, alpha, kPlainGrey));
    }

    // 5. The words: the title, then each readout's "-", number and "+".
    ShadowText(out, layout.title, layout.titleRight, kTitle, kTitleSize, 255);
    const float values[2] = {m_tuning.size, m_tuning.opacity};
    for (std::size_t row = 0; row < 2; ++row) {
        const float y = layout.rowY[row] - kNumberHalfHeight;
        float scale = 1.0f;
        if (m_pulse[row].ticksLeft > 0) scale = m_pulse[row].direction > 0 ? kPulseGrew : kPulseShrank;
        ShadowText(out, glm::vec2(layout.centreX - kMinusLeft, y), 0.0f, "-", kReadoutSize, kSignAlpha);
        ShadowText(out, glm::vec2(layout.centreX - kNumberHalfWidth * scale, layout.rowY[row] - kNumberHalfHeight * scale),
                   0.0f, Readout(values[row]), kReadoutSize * scale, 255);
        ShadowText(out, glm::vec2(layout.centreX + kPlusRight, y), 0.0f, "+", kReadoutSize, kSignAlpha);
    }
}

} // namespace Penumbra::Render
