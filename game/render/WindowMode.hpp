#pragma once

// What the scripts' SetWindowProperties becomes on the engine's window
// (enhancement E2, Step 23), as pure decisions a suite can reach.
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

#include <glm/glm.hpp>

namespace Penumbra::Render {

struct WindowAction {
    enum class Kind {
        EnterFullscreen,        // at `size`, the saved mode; 0 x 0 is the desktop's
        LeaveFullscreen,        // back to the windowed size and position
        SwitchFullscreenMode,   // already fullscreen: the display to `size`
        ResizeWindow,           // windowed: the window to `size`
    };
    Kind kind = Kind::ResizeWindow;
    glm::uvec2 size{0u};
};

// `windowed` and `requested` are the scripts' SetWindowProperties;
// `fullscreenNow` is what the layer last asked of the window (the Machine's
// own flag is already overwritten by the request, and WindowControl's answer
// is a frame late); `savedMode` is the settings' fullscreen mode.
WindowAction DecideWindowAction(bool windowed, glm::uvec2 requested, bool fullscreenNow, glm::uvec2 savedMode);

// What a mode picked while fullscreen is saved as: 0 x 0 when it is the
// desktop's own size, so the setting follows the desktop if its resolution
// changes later instead of pinning the old one; otherwise the size.
glm::uvec2 FullscreenModeToSave(glm::uvec2 picked, glm::uvec2 desktop);

} // namespace Penumbra::Render
