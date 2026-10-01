#pragma once

// ENHANCEMENT E28: the touch controls' editor (docs/planning/2026-10-01-e28-adjustable-touch-controls.md). Pure, like PauseMenu: fed this tick's fingers and the
// controls' layout, it answers what changed and what to draw. No window, no Machine, no fonts, no audio.

#include <array>
#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "eth/Snapshot.hpp"
#include "render/TouchControls.hpp"
#include "render/TouchTuning.hpp"

namespace Penumbra::Render {

enum class TouchEditWidget : int { Back = 0, SizeLess, SizeMore, OpacityLess, OpacityMore, Lock, Restore, Count };
inline constexpr int kTouchEditWidgetCount = static_cast<int>(TouchEditWidget::Count);

struct TouchEditInput {
    const std::vector<TouchContact>* down = nullptr;   // TouchControls::Down() of THIS tick (null: no finger)
    TouchGeometry geometry;                            // the same the layer gave TouchControls::Update this tick
    bool close = false;                                // Esc, HELD (the editor finds the edge); under touch no pad is player 1, so a pad's Back or B does not close it
};

struct TouchEditStep {
    bool changed = false;   // the tuning differs from last tick's: the layer calls m_touch.SetTuning(editor.Tuning())
    bool commit = false;    // a gesture ended with something unsaved: the layer stores it and saves settings.json (never per drag tick)
    bool closed = false;    // the editor closed (back arrow or Esc): the layer resumes the Machine and arms the closing-key filter
};

class TouchEditor {
public:
    // The words (cp1252 = ASCII here; strings.json translates) and the look; where each piece sits is ComputeLayout's.
    static constexpr const char* kTitle = "Ajustar controles de toque";
    static constexpr const char* kFont = "Arial Narrow";
    static constexpr float kTitleSize = 40.0f;
    static constexpr float kReadoutSize = 40.0f;
    static constexpr float kTile = 96.0f;
    static constexpr float kBackSize = 64.0f;
    static constexpr float kBackInset = 41.0f;
    static constexpr float kPairOffset = 112.0f;       // tile centres at cx -+ this (size and opacity rows)
    static constexpr float kLockOffset = 56.0f;        // lock at cx - this, restore at cx + this
    static constexpr std::array<float, 3> kRowY = {168.0f, 276.0f, 392.0f};   // from lo.y: size, opacity, lock rows
    static constexpr float kTitleRightInset = 140.0f;  // the title's right edge from the area's (the pause preview's column)
    static constexpr unsigned kPulseTicks = 6;
    static constexpr unsigned kBreathTicks = 48;

    struct Layout {
        std::array<TouchLayout::Box, kTouchEditWidgetCount> widget{};   // by TouchEditWidget
        glm::vec2 title{0.0f};                         // text origin
        float titleRight = 0.0f;                       // HudCmd::rtlRight
        float centreX = 0.0f;
        std::array<float, 3> rowY{};                   // absolute y of the three row centres
    };
    static Layout ComputeLayout(const TouchGeometry& geometry);

    // images/options/{arrow_left,edit_shrink,edit_enlarge,edit_dim,edit_brighten,edit_locked,edit_unlocked,edit_restore}.png under
    // `dataDir`, looked up once; a missing one is drawn as a plain square, as TouchControls draws a control without art.
    void SetImageRoot(const std::filesystem::path& dataDir);

    // `tuning` is what is in force. `downNow`: the fingers on the screen this instant (the one that tapped the entry): dead until they lift.
    void Open(const TouchTuning& tuning, const std::vector<TouchContact>& downNow, bool startUnlocked = false);
    bool IsOpen() const;
    TouchEditStep Update(const TouchEditInput& input, const TouchControls& controls);
    const TouchTuning& Tuning() const;
    bool Locked() const;

    // Nothing unless open. Order (tests pin it): 1 backdrop Rectangle (areaMin..areaMax); 2 controls.AppendOverlay; 3 outlines (unlocked
    // only: 4 Rectangles per visible movable control, in TouchControl order); 4 widget Sprites in TouchEditWidget order (7); 5 Texts:
    // title shadow+front, then for the size row and the opacity row `-`, number, `+` each shadow+front (14 Texts in all).
    void AppendOverlay(const TouchControls& controls, std::vector<Eth::HudCmd>& out) const;

private:
    // What a finger is for, settled when it lands and kept until it lifts: Dead (down at Open: it is known, so it is
    // never "new"), Widget (a tile or the back arrow: it acts when it lifts inside), Control (dragging one control),
    // None (landed on nothing, or on a control while locked, or its control was let go: it does nothing).
    enum class Use { Dead, Widget, Control, None };

    struct Finger {
        Use use = Use::None;
        glm::vec2 position{0.0f};                         // where it was last seen down
        bool moved = false;                               // it is somewhere else than the tick before
        TouchEditWidget widget = TouchEditWidget::Back;   // Use::Widget
        TouchControl control = TouchControl::Count;       // Use::Control
        glm::vec2 grabOffset{0.0f};                       // finger - the control's box min at the grab
        TouchMove startMove{};                            // the control's move at the grab (a drop buried under a tile goes back to it)
    };

    // A readout's number grows (direction +1) or shrinks (-1) for kPulseTicks after a step.
    struct Pulse {
        unsigned ticksLeft = 0;
        int direction = 0;
    };

    // The eight images of the editor, `m_images` in this order: the back arrow, the four size and opacity tiles, the lock
    // closed and open, the restore arrow.
    static constexpr std::size_t kArtCount = 8;

    bool grabbed(TouchControl control) const;                 // some finger holds it
    void dropGrabs();                                         // every Control finger becomes None
    void commitIfDirty(TouchEditStep& step);                  // commit when the tuning is not what was last saved
    void commit(TouchEditStep& step);                         // commit now (a step), remembering what was saved
    void stepSize(int direction, TouchEditStep& step);
    void stepOpacity(int direction, TouchEditStep& step);

    std::filesystem::path m_imageRoot;
    std::array<std::string, kArtCount> m_images{};            // "" = not there: a plain square
    bool m_open = false;
    bool m_locked = true;
    TouchTuning m_tuning;                                     // what is in force, the drag's moves included
    TouchTuning m_saved;                                      // what the last commit (or Open) handed the layer
    TouchGeometry m_geometry;                                 // the last Update's, which AppendOverlay lays out by
    std::map<int, Finger> m_fingers;                          // every finger down, by id
    std::array<Pulse, 2> m_pulse{};                           // the size readout, the opacity readout
    unsigned m_ticks = 0;                                     // Updates since Open: the outlines' breathing
    bool m_prevClose = false;                                 // the close edge's memory
    bool m_haveInput = false;                                 // an Update has run since Open
};

} // namespace Penumbra::Render
