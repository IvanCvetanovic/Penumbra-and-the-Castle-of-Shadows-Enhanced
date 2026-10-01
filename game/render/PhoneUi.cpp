#include "render/PhoneUi.hpp"

#include <algorithm>
#include <cmath>

namespace Penumbra::Render {

namespace {

// The original's screen, which the menus keep and E1's levels start from.
constexpr glm::vec2 kFourThree{1024.0f, 768.0f};

float Aspect(glm::uvec2 window) {
    return window.y > 0 ? static_cast<float>(window.x) / static_cast<float>(window.y) : kFourThree.x / kFourThree.y;
}

// What a message line needs: its room and the gap past it.
constexpr float kMessageNeeds = kMessageRoom + kMessageGap;

float ClearanceAt(glm::uvec2 window, bool widescreen, float zoom, const Supersonic::SafeAreaInsets& safe,
                  float marginPercent) {
    const glm::vec2 screen = ZoomedScreen(window, widescreen, zoom);
    return MessageClearance(screen, ComputeHudFrame(window, screen, safe, marginPercent));
}

} // namespace

// --- E26 -------------------------------------------------------------------------------------

float ClampEdgeMarginSetting(float percent) {
    if (!std::isfinite(percent)) return kEdgeMarginAuto;
    if (percent < 0.0f) return kEdgeMarginAuto;
    return std::min(percent, kEdgeMarginMaxPercent);
}

float EdgeMarginPercent(float setting, bool touch, glm::uvec2 window) {
    if (!touch) return 0.0f;
    const float clamped = ClampEdgeMarginSetting(setting);
    if (clamped >= 0.0f) return clamped;
    return IsPhoneShaped(window) ? kPhoneEdgeMarginPercent : kTabletEdgeMarginPercent;
}

HudFrame ComputeHudFrame(glm::uvec2 window, glm::vec2 screen, const Supersonic::SafeAreaInsets& safe,
                         float marginPercent) {
    HudFrame frame;
    if (window.x == 0 || window.y == 0 || !(screen.x > 0.0f) || !(screen.y > 0.0f)) return frame;
    const glm::vec2 image(window);
    // The screen in the image as CameraRig::ComputeView places a level's: the
    // largest copy that fits, centred on a whole pixel; the rest is bars.
    const float scale = std::min(image.x / screen.x, image.y / screen.y);
    const glm::vec2 shownMin = glm::round((image - screen * scale) * 0.5f);
    const glm::vec2 shownMax = shownMin + screen * scale;
    const float margin = std::clamp(marginPercent, 0.0f, kEdgeMarginMaxPercent) / 100.0f;
    const float side = margin * image.x;
    const float left = std::max(std::max(0.0f, safe.left), side);
    const float right = std::max(std::max(0.0f, safe.right), side);
    const float top = std::max(std::max(0.0f, safe.top), margin * image.y);
    const float bottom = std::max(0.0f, safe.bottom);
    frame.left = std::max(0.0f, left - shownMin.x) / scale;
    frame.right = std::max(0.0f, right - (image.x - shownMax.x)) / scale;
    frame.top = std::max(0.0f, top - shownMin.y) / scale;
    frame.bottom = std::max(0.0f, bottom - (image.y - shownMax.y)) / scale;
    return frame;
}

float MessageClearance(glm::vec2 screen, const HudFrame& frame) {
    const float unit = screen.y / kUnzoomedHeight;
    const float pauseLeft = screen.x - frame.right - kPauseColumn * unit;
    return pauseLeft - (frame.left + kMessageInset);
}

float FittedEdgeMargin(float percent, glm::uvec2 window, bool widescreen, const Supersonic::SafeAreaInsets& safe) {
    percent = std::clamp(percent, 0.0f, kEdgeMarginMaxPercent);
    if (percent <= 0.0f || ClearanceAt(window, widescreen, 1.0f, safe, percent) >= kMessageNeeds) return percent;
    // The clearance only shrinks as the margin grows: the largest that keeps
    // it, to a hundredth of a percent (none, if the safe area alone does not).
    float fits = 0.0f;
    float breaks = percent;
    for (int i = 0; i < 20 && breaks - fits > 0.01f; ++i) {
        const float mid = 0.5f * (fits + breaks);
        (ClearanceAt(window, widescreen, 1.0f, safe, mid) >= kMessageNeeds ? fits : breaks) = mid;
    }
    return std::floor(fits * 100.0f) / 100.0f;
}

// --- The zoom --------------------------------------------------------------------------------

bool IsPhoneShaped(glm::uvec2 window) {
    if (window.x == 0 || window.y == 0) return false;
    const float longSide = static_cast<float>(std::max(window.x, window.y));
    const float shortSide = static_cast<float>(std::min(window.x, window.y));
    return longSide >= kPhoneAspect * shortSide;
}

int ClampZoomSetting(int percent) {
    if (percent == kZoomAutomatic) return kZoomAutomatic;
    return std::clamp(percent, kZoomMinPercent, kZoomMaxPercent);
}

int ZoomPercent(int setting, bool touch, glm::uvec2 window) {
    if (!touch) return kZoomMinPercent;
    const int clamped = ClampZoomSetting(setting);
    if (clamped != kZoomAutomatic) return clamped;
    return IsPhoneShaped(window) ? kPhoneZoomPercent : kTabletZoomPercent;
}

float MaxZoom(glm::uvec2 window, bool widescreen, const Supersonic::SafeAreaInsets& safe, float marginPercent) {
    if (window.x == 0 || window.y == 0) return 1.0f;
    // Everything in the clearance but the start's 10 px shrinks with the
    // screen, and the frame's insets with it (they are the window's pixels):
    // clearance(z) + 10 = (clearance(1) + 10) / z.
    const float atOne = ClearanceAt(window, widescreen, 1.0f, safe, marginPercent);
    float zoom = std::max(1.0f, (atOne + kMessageInset) / (kMessageNeeds + kMessageInset));
    // Whole pixels round the screen: back off until the rule holds.
    for (int i = 0; i < 100 && zoom > 1.0f && ClearanceAt(window, widescreen, zoom, safe, marginPercent) < kMessageNeeds;
         ++i) {
        zoom = std::max(1.0f, zoom - 0.001f);
    }
    return zoom;
}

int MaxZoomPercent(glm::uvec2 window, bool widescreen, const Supersonic::SafeAreaInsets& safe, float marginPercent) {
    // A hair under, so a limit of exactly 150% offers the 150% step.
    return static_cast<int>(std::floor(MaxZoom(window, widescreen, safe, marginPercent) * 100.0f + 1e-3f));
}

float CampaignZoom(int setting, bool touch, glm::uvec2 window, bool widescreen, const Supersonic::SafeAreaInsets& safe,
                   float marginPercent) {
    const float asked = static_cast<float>(ZoomPercent(setting, touch, window)) / 100.0f;
    if (!(asked > 1.0f)) return 1.0f;
    return std::clamp(asked, 1.0f, MaxZoom(window, widescreen, safe, marginPercent));
}

bool IsCampaignScene(const std::string& sceneFile) {
    return sceneFile == "scenes/level1.esc" || sceneFile == "scenes/level2.esc" || sceneFile == "scenes/level3.esc" ||
           sceneFile == "scenes/checkpoint.esc";
}

glm::vec2 ZoomedScreen(glm::uvec2 window, bool widescreen, float zoom) {
    zoom = std::max(1.0f, zoom);
    const float height = std::round(kUnzoomedHeight / zoom);
    if (!widescreen) return {std::round(kFourThree.x / zoom), height};
    // E1's rule on the smaller height: the window's shape, and no narrower
    // than the original's 4:3 at this zoom.
    return {std::max(std::round(kFourThree.x / zoom), std::round(height * Aspect(window))), height};
}

bool CampaignUnzooms(const std::string& sceneFile, glm::vec2 screen, bool gameFinished, bool princess) {
    // Only a zoomed screen is shorter than 768; an unzoomed one is left alone.
    return IsCampaignScene(sceneFile) && screen.y < kUnzoomedHeight && (gameFinished || princess);
}

// --- The menu, larger ------------------------------------------------------------------------

bool IsPhoneMenuScene(const std::string& sceneFile) {
    return sceneFile == "scenes/menu.esc" || sceneFile == "scenes/arena_select.esc";
}

MenuFrame ComputeMenuFrame(glm::uvec2 window, const Supersonic::SafeAreaInsets& safe) {
    MenuFrame frame;
    if (window.x == 0 || window.y == 0) return frame;
    const glm::vec2 image(window);
    const float safeLeft = std::max(0.0f, safe.left);
    const float safeTop = std::max(0.0f, safe.top);
    const float safeBottom = std::max(0.0f, safe.bottom);
    // E1's view: the 1024x768 screen fitted to the window, centred on a whole
    // pixel (CameraRig::ComputeView), its panel from x 632.8 to the window's edge.
    frame.baseScale = std::min(image.x / kFourThree.x, image.y / kFourThree.y);
    if (!(image.x * kFourThree.y > image.y * kFourThree.x)) return frame;   // no wider than 4:3: nothing to gain
    const float baseLeft = std::round((image.x - kFourThree.x * frame.baseScale) * 0.5f);
    const float basePanel = image.x - (baseLeft + kMenuPanelLeft * frame.baseScale);

    // As large as the focus box fits the safe area's height, and no larger
    // than leaves the panel E1's width.
    const glm::vec2 focus = kMenuFocusMax - kMenuFocusMin;
    const float safeHeight = std::max(0.0f, image.y - safeTop - safeBottom);
    const float fill = safeHeight / focus.y;
    const float panelLimit = (image.x - safeLeft - basePanel) / focus.x;
    const float scale = std::min(fill, panelLimit);
    if (!(scale >= frame.baseScale * kMenuMinGain)) return frame;

    frame.active = true;
    frame.scale = scale;
    // The focus box from the window's left (past a notch), centred in the
    // safe area's height, but never so far down or up that the screen's top
    // or bottom edge comes into the image (nothing of the scene is collected
    // past them) - unless the safe area needs it: the logo and the Quit
    // button stay inside it first.
    const float left = safeLeft - kMenuFocusMin.x * scale;
    const float centred = safeTop + (safeHeight - focus.y * scale) * 0.5f - kMenuFocusMin.y * scale;
    float top = std::clamp(centred, std::min(0.0f, image.y - kFourThree.y * scale), 0.0f);
    // (scale is never past the fill, so the box fits - but at exactly the fill
    // float rounding can put the high bound a hair under the low, and
    // std::clamp with hi < lo is undefined: the high is held at the low.)
    const float lowTop = safeTop - kMenuFocusMin.y * scale;
    const float highTop = std::max(lowTop, image.y - safeBottom - kMenuFocusMax.y * scale);
    top = std::clamp(top, lowTop, highTop);
    frame.viewportMin = glm::round(glm::vec2(left, top));
    return frame;
}

MenuPanel ComputeMenuPanel(const MenuFrame& frame, glm::uvec2 window, const Supersonic::SafeAreaInsets& safe,
                           float cornerLeft) {
    MenuPanel panel;
    if (!frame.active || !(frame.scale > 0.0f)) return panel;
    const glm::vec2 image(window);
    // What the window's safe area shows of the screen, logical px: from its
    // top-left to its bottom-right corner, within the 1024x768 screen
    // vertically (across, E1's world goes on past its sides).
    const float safeTop = std::max(0.0f, safe.top);
    const float safeBottom = std::max(0.0f, safe.bottom);
    // The safe area's left too: the loading message sits in this corner, and a
    // side cutout would cover it.
    panel.shownMin = glm::vec2((std::max(0.0f, safe.left) - frame.viewportMin.x) / frame.scale,
                               std::max(0.0f, (safeTop - frame.viewportMin.y) / frame.scale));
    panel.shownMax = glm::vec2((image.x - frame.viewportMin.x) / frame.scale,
                               std::min(kFourThree.y, (image.y - safeBottom - frame.viewportMin.y) / frame.scale));

    // showData's insets: 10 px in from the panel's left, 20 down from its top;
    // the same 10 kept at the right, where the window ends it.
    float right = panel.shownMax.x - std::max(0.0f, safe.right) / frame.scale - 10.0f;
    if (cornerLeft > 0.0f) right = std::min(right, cornerLeft - 10.0f);
    panel.min = glm::vec2(kMenuPanelLeft + 10.0f, panel.shownMin.y + 20.0f);
    panel.max = glm::vec2(std::max(panel.min.x, right), std::max(panel.min.y, panel.shownMax.y - 10.0f));
    // In E1's view a logical px is baseScale image px: its text's size is the least.
    panel.minScale = frame.baseScale / frame.scale;
    panel.maxScale = panel.minScale * kMenuMaxTextGain;
    return panel;
}

} // namespace Penumbra::Render
