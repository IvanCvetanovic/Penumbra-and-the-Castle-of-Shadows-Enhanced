#include "render/WindowMode.hpp"

#include <algorithm>

namespace Penumbra::Render {

using Supersonic::DisplayMode;
using Supersonic::WindowControl;

WindowAction DecideWindowAction(bool windowed, glm::uvec2 requested, bool fullscreenNow, glm::uvec2 savedMode) {
    const bool fullscreen = !windowed;
    if (fullscreen != fullscreenNow) {
        // The flag flipped. Its size is the logical screen's (menu.as:111),
        // never a mode: the way in is at the mode the player last picked.
        if (!fullscreen) return {WindowAction::Kind::LeaveFullscreen, glm::uvec2(0u)};
        const bool haveMode = savedMode.x > 0 && savedMode.y > 0;
        return {WindowAction::Kind::EnterFullscreen, haveMode ? savedMode : glm::uvec2(0u)};
    }
    // A line of the mode list, with the flag as it was. 0 x 0 is E23's
    // automatic line, passed on as it is.
    return {fullscreen ? WindowAction::Kind::SwitchFullscreenMode : WindowAction::Kind::ResizeWindow, requested};
}

FullscreenChoice ChooseFullscreen(const std::vector<DisplayMode>& modes, const DisplayMode& desktop,
                                  glm::uvec2 savedSize, uint32_t savedRate) {
    FullscreenChoice choice;
    const glm::uvec2 desktopSize(desktop.width, desktop.height);

    // The size: the saved one where this monitor has it at any rate.
    choice.sizeAutomatic = savedSize.x == 0 || savedSize.y == 0;
    choice.size = desktopSize;
    if (!choice.sizeAutomatic) {
        const DisplayMode offered = WindowControl::ChooseFullscreenMode(
            modes, desktop, savedSize.x, savedSize.y, WindowControl::kHighestRefreshRate);
        if (offered.width != 0) {
            choice.size = savedSize;
        } else {
            choice.sizeFellBack = true;
        }
    }
    if (choice.size.x == 0 || choice.size.y == 0) {
        choice.size = glm::uvec2(0u);
        return choice;
    }

    // The rate: the saved one where the size has it, else the highest.
    choice.rateAutomatic = savedRate == 0;
    choice.rate = WindowControl::kHighestRefreshRate;
    if (!choice.rateAutomatic) {
        if (WindowControl::ChooseFullscreenMode(modes, desktop, choice.size.x, choice.size.y, savedRate).width != 0) {
            choice.rate = savedRate;
        } else {
            choice.rateFellBack = true;
        }
    }
    choice.mode = WindowControl::ChooseFullscreenMode(modes, desktop, choice.size.x, choice.size.y, choice.rate);
    return choice;
}

RateChoices ChooseRates(const std::vector<DisplayMode>& modes, const DisplayMode& desktop,
                        const FullscreenChoice& choice, uint32_t savedRate) {
    RateChoices out;
    out.rates.push_back(0);
    const std::vector<uint32_t> offered = WindowControl::RefreshRatesAt(modes, desktop, choice.size.x, choice.size.y);
    out.rates.insert(out.rates.end(), offered.begin(), offered.end());
    if (!offered.empty()) out.automaticRate = offered.back();
    if (savedRate != 0) {
        const auto at = std::find(out.rates.begin() + 1, out.rates.end(), savedRate);
        if (at != out.rates.end()) out.current = static_cast<uint32_t>(at - out.rates.begin());
    }
    return out;
}

} // namespace Penumbra::Render
