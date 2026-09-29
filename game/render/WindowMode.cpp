#include "render/WindowMode.hpp"

namespace Penumbra::Render {

WindowAction DecideWindowAction(bool windowed, glm::uvec2 requested, bool fullscreenNow, glm::uvec2 savedMode) {
    const bool fullscreen = !windowed;
    if (fullscreen != fullscreenNow) {
        // The flag flipped. Its size is the logical screen's (menu.as:111),
        // never a mode: the way in is at the mode the player last picked.
        if (!fullscreen) return {WindowAction::Kind::LeaveFullscreen, glm::uvec2(0u)};
        const bool haveMode = savedMode.x > 0 && savedMode.y > 0;
        return {WindowAction::Kind::EnterFullscreen, haveMode ? savedMode : glm::uvec2(0u)};
    }
    // A line of the mode list, with the flag as it was.
    return {fullscreen ? WindowAction::Kind::SwitchFullscreenMode : WindowAction::Kind::ResizeWindow, requested};
}

glm::uvec2 FullscreenModeToSave(glm::uvec2 picked, glm::uvec2 desktop) {
    return picked == desktop ? glm::uvec2(0u) : picked;
}

} // namespace Penumbra::Render
