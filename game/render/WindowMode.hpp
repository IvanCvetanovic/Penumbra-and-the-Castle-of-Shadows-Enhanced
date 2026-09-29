#pragma once

// What the scripts' SetWindowProperties becomes on the engine's window
// (enhancement E2, Step 23), and the automatic display mode (E23, Step 27), as
// pure decisions a suite can reach.
//
// The scripts ask in 0.7.12's terms, a windowed flag and a size, from two
// places: Alt+Enter and the options screen's switch flip the flag and send the
// logical screen's size (menu.as:108-119), which is not the window's and is
// ignored; a line of the mode list sends its size with the flag as it was
// (videoModes.as:110). In 0.7.12 a line picked while fullscreen switched the
// display to that mode - a device reset at the new back buffer size - and one
// picked in a window sized the window. The port does the same through
// WindowControl::SetFullscreenMode and SetWindowedSize, and remembers the
// fullscreen mode (Settings::fullscreenWidth/Height) for the next way in.
//
// E23: the list's first line, "Autom\xE1tico (melhor)", sends 0 x 0, which
// 0.7.12 never did (it listed only the display's modes): automatic, in
// whichever of the two the window is. Automatic fullscreen is the desktop's
// size - the monitor's native size as the system runs it - at the highest rate
// the monitor offers there; an automatic window is fitted to the monitor
// (WindowControl::FitWindowToMonitor at kAutoWindowFraction).

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "core/WindowControl.hpp"

namespace Penumbra::Render {

// E23: an automatic window's share of its monitor's work area, each way. The
// largest window of the monitor's own shape with room around it for the title
// bar and the frame, and for the taskbar to stay in view: on a 1920x1200
// panel over a 48-pixel taskbar, 1566x979 (the work area's height binds),
// 7.5% of it clear above and below.
inline constexpr float kAutoWindowFraction = 0.85f;

struct WindowAction {
    enum class Kind {
        EnterFullscreen,        // at the saved mode (the layer's ChooseFullscreen)
        LeaveFullscreen,        // back to the windowed size and position
        SwitchFullscreenMode,   // already fullscreen: the display to `size`; 0 x 0 = automatic (E23)
        ResizeWindow,           // windowed: the window to `size`; 0 x 0 = automatic (E23)
    };
    Kind kind = Kind::ResizeWindow;
    glm::uvec2 size{0u};
};

// `windowed` and `requested` are the scripts' SetWindowProperties;
// `fullscreenNow` is what the layer last asked of the window (the Machine's
// own flag is already overwritten by the request, and WindowControl's answer
// is a frame late); `savedMode` is the settings' fullscreen mode.
WindowAction DecideWindowAction(bool windowed, glm::uvec2 requested, bool fullscreenNow, glm::uvec2 savedMode);

// E23: the fullscreen mode the settings come to on this monitor.
struct FullscreenChoice {
    // The size to ask SetFullscreenMode for: the saved one, or the desktop's
    // when it is automatic or this monitor does not offer it. 0 x 0 when no
    // monitor reports a mode, and only SetFullscreen(true) is left.
    glm::uvec2 size{0u};
    // SetFullscreenMode's rate: the saved rate where the monitor offers the
    // size at it, else WindowControl::kHighestRefreshRate.
    uint32_t rate = Supersonic::WindowControl::kHighestRefreshRate;
    // What that comes to (WindowControl::ChooseFullscreenMode): the rate the
    // display will run at, for the log and the refresh-rate row.
    Supersonic::DisplayMode mode;
    bool sizeAutomatic = true;    // the saved size is 0 x 0
    bool sizeFellBack = false;    // a saved size this monitor does not offer: the desktop's instead
    bool rateAutomatic = true;    // the saved rate is 0
    bool rateFellBack = false;    // a saved rate not offered at the size: the highest instead
};

FullscreenChoice ChooseFullscreen(const std::vector<Supersonic::DisplayMode>& modes,
                                  const Supersonic::DisplayMode& desktop, glm::uvec2 savedSize,
                                  uint32_t savedRate);

// E23: the refresh-rate row's choices for a fullscreen choice: 0 (automatic)
// first, then every rate the monitor offers at the choice's size, lowest
// first (WindowControl::RefreshRatesAt). `current` is the saved rate's index,
// 0 when it is automatic or not offered there (the display then runs at the
// highest, which is what "automatic" says); `automaticRate` is the rate the
// automatic line comes to, 0 when the platform does not know its rates.
struct RateChoices {
    std::vector<uint32_t> rates;
    uint32_t current = 0;
    uint32_t automaticRate = 0;
};

RateChoices ChooseRates(const std::vector<Supersonic::DisplayMode>& modes, const Supersonic::DisplayMode& desktop,
                        const FullscreenChoice& choice, uint32_t savedRate);

} // namespace Penumbra::Render
