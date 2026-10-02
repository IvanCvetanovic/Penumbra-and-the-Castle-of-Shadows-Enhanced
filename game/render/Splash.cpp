#include "render/Splash.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace Penumbra::Render {

namespace {

// Smoothstep: 0 at 0, 1 at 1, zero slope at both, monotone between.
float Ease(float x) {
    x = std::clamp(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

} // namespace

// ---- The timeline -------------------------------------------------------------------------------------------

float SplashAlpha(const unsigned tick) {
    if (tick >= kSplashTotalTicks) return 0.0f;
    if (tick < kSplashFadeInTicks) return Ease(static_cast<float>(tick) / static_cast<float>(kSplashFadeInTicks));
    if (tick <= kSplashFadeOutStart) return 1.0f;
    return 1.0f - Ease(static_cast<float>(tick - kSplashFadeOutStart) / static_cast<float>(kSplashFadeOutTicks));
}

bool SplashDone(const unsigned tick) { return tick >= kSplashTotalTicks; }

void SplashClock::Step(const bool pressed) {
    if (Done()) return;
    // The tick that is running is numbered m_ticks; Ticks() counts it once this returns. A press during the
    // fade-out of the timeline itself is no skip: the intro is ending already and a skip would only lengthen it.
    if (pressed && !m_skipTick && m_ticks >= kSplashSkipFromTick && m_ticks < kSplashFadeOutStart) {
        m_skipTick = m_ticks;
    }
    ++m_ticks;
}

bool SplashClock::Done() const {
    if (m_skipTick) return m_ticks - *m_skipTick >= kSplashFadeOutTicks;
    return SplashDone(m_ticks);
}

float SplashClock::Alpha() const {
    if (!m_skipTick) return SplashAlpha(m_ticks);
    // From what was on the screen when the press came (the alpha the previous tick drew), down to nothing.
    const float start = SplashAlpha(*m_skipTick);
    return start * (1.0f - Ease(static_cast<float>(m_ticks - *m_skipTick) / static_cast<float>(kSplashFadeOutTicks)));
}

// ---- What counts as a press ---------------------------------------------------------------------------------

SplashHeld SplashHeldOf(const RawDevices& raw, const std::vector<int>& contactIds) {
    SplashHeld held;
    for (std::size_t k = 0; k < raw.keys.size(); ++k) {
        if (raw.keys[k]) held.set(kSplashKeyBase + k);
    }
    for (std::size_t b = 0; b < raw.mouse.size(); ++b) {
        if (raw.mouse[b]) held.set(kSplashMouseBase + b);
    }
    for (const int id : contactIds) {
        if (id >= 0) held.set(kSplashTouchBase + static_cast<std::size_t>(id % 8));
    }
    for (std::size_t p = 0; p < raw.pads.size() && p < kSplashPads; ++p) {
        const RawPad& pad = raw.pads[p];
        const std::size_t base = kSplashPadBase + p * kSplashPadBits;
        for (std::size_t b = 0; b < pad.buttons.size(); ++b) {
            if (pad.buttons[b]) held.set(base + b);
        }
        for (std::size_t b = 0; b < pad.rawButtons.size(); ++b) {
            if (pad.rawButtons[b]) held.set(base + pad.buttons.size() + b);
        }
    }
    return held;
}

bool SplashPressWatch::Observe(const SplashHeld& held) {
    const bool pressed = m_have && (held & ~m_held).any();
    m_held = held;
    m_have = true;
    return pressed;
}

// ---- The picture --------------------------------------------------------------------------------------------

SplashLayout ComputeSplashLayout(const glm::uvec2& window, const Supersonic::SafeAreaInsets& safe,
                                 const glm::vec2& image) {
    SplashLayout layout;
    if (window.x == 0 || window.y == 0 || !(image.x > 0.0f) || !(image.y > 0.0f)) return layout;

    const glm::vec2 whole(window);
    glm::vec2 areaMin(std::max(safe.left, 0.0f), std::max(safe.top, 0.0f));
    glm::vec2 areaMax(whole.x - std::max(safe.right, 0.0f), whole.y - std::max(safe.bottom, 0.0f));
    // Insets that leave nothing are not believed: the window is the area.
    if (!(areaMax.x - areaMin.x >= 1.0f) || !(areaMax.y - areaMin.y >= 1.0f)) {
        areaMin = glm::vec2(0.0f);
        areaMax = whole;
    }
    const glm::vec2 area = areaMax - areaMin;

    const float aspect = image.x / image.y;
    // 70% of the window's width, at most half its height, and inside the safe area.
    float width = std::min({whole.x * kSplashWidthFraction, whole.y * kSplashHeightFraction * aspect, area.x,
                            area.y * aspect});
    width = std::max(1.0f, std::round(width));
    const float height = std::max(1.0f, std::round(width / aspect));

    layout.size = glm::vec2(width, height);
    const glm::vec2 centred = areaMin + (area - layout.size) * 0.5f;
    layout.pos = glm::vec2(std::round(centred.x), std::round(centred.y));
    return layout;
}

std::vector<Supersonic::ScreenOverlay::Quad> BuildSplashQuads(const glm::uvec2& window, const SplashLayout& layout,
                                                              const float alpha, const std::string& logoKey) {
    using Quad = Supersonic::ScreenOverlay::Quad;
    std::vector<Quad> quads;
    if (window.x == 0 || window.y == 0) return quads;

    Quad ground;   // the whole image, untextured
    ground.color = glm::vec4(static_cast<float>(kSplashGround[0]), static_cast<float>(kSplashGround[1]),
                             static_cast<float>(kSplashGround[2]), 255.0f) /
                   255.0f;
    quads.push_back(std::move(ground));

    const float opacity = std::clamp(alpha, 0.0f, 1.0f);
    if (logoKey.empty() || !(opacity > 0.0f) || !(layout.size.x > 0.0f) || !(layout.size.y > 0.0f)) return quads;

    const glm::vec2 whole(window);
    Quad logo;
    logo.min = layout.pos / whole;
    logo.max = (layout.pos + layout.size) / whole;
    logo.color = glm::vec4(1.0f, 1.0f, 1.0f, opacity);
    logo.texture = logoKey;
    quads.push_back(std::move(logo));
    return quads;
}

// ---- Who plays it ----------------------------------------------------------------------------------------------

bool SplashWanted(const std::vector<std::string>& args) {
    std::optional<bool> forced;
    bool developerFlag = false;
    for (std::size_t i = 0; i < args.size(); ++i) {
        std::string_view arg = args[i];
        if (arg.substr(0, 2) != "--") continue;   // a value, or a short flag (-h)
        if (const std::size_t equals = arg.find('='); equals != std::string_view::npos) arg = arg.substr(0, equals);
        if (arg == "--splash") {
            if (i + 1 < args.size() && (args[i + 1] == "on" || args[i + 1] == "off")) forced = args[++i] == "on";
            continue;
        }
        if (std::find(std::begin(kSplashPlayerFlags), std::end(kSplashPlayerFlags), arg) ==
            std::end(kSplashPlayerFlags)) {
            developerFlag = true;
        }
    }
    return forced.value_or(!developerFlag);
}

} // namespace Penumbra::Render
