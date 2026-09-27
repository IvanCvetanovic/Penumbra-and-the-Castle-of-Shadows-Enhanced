#include "render/CameraRig.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "core/Components.hpp"
#include "core/RenderSettings.hpp"
#include "core/ScreenOverlay.hpp"
#include "core/ViewportInfo.hpp"

namespace Penumbra::Render {

namespace {

// The original's screen, for a snapshot that somehow carries no size.
constexpr glm::vec2 kOriginalScreen{1024.0f, 768.0f};

glm::vec2 LogicalScreen(const Eth::RenderSnapshot& snapshot) {
    const glm::vec2 size = snapshot.screenSize;
    return (size.x > 0.0f && size.y > 0.0f && std::isfinite(size.x) && std::isfinite(size.y)) ? size
                                                                                                : kOriginalScreen;
}

// What a D3D9 game on an 8-bit target drew in (MPR's SceneRendering, the same
// reasons): texture bytes are display values and every multiply and blend
// happens on them; no sky pass, a flat clear colour; no bloom, which the
// original never drew.
void ApplySceneRendering(Supersonic::RenderSettings& rendering) {
    rendering.encoding = Supersonic::RenderSettings::SceneEncoding::DisplayEncoded;
    rendering.background = Supersonic::RenderSettings::Background::Color;
    rendering.bloomIntensity = 0.0f;
}

} // namespace

void CameraRig::Attach(entt::registry& registry) {
    using namespace Supersonic;

    // Assigned rather than emplaced: a --scene load may already have put a
    // RenderSettings of its own in the context. Before anything is drawn -
    // TextureCache's keys are found only under a DisplayEncoded scene.
    RenderSettings rendering;
    ApplySceneRendering(rendering);
    const glm::vec3 black = BackgroundColour(0xFF000000u);
    rendering.backgroundColor[0] = black.r;
    rendering.backgroundColor[1] = black.g;
    rendering.backgroundColor[2] = black.b;
    registry.ctx().insert_or_assign<RenderSettings>(std::move(rendering));

    if (m_camera != entt::null && registry.valid(m_camera)) return;

    // The game owns its view: a camera some scene brought along would win
    // FindPrimaryCamera by iteration order otherwise.
    for (const entt::entity other : registry.view<CameraComponent>()) {
        registry.get<CameraComponent>(other).isPrimary = false;
    }

    m_camera = registry.create();
    registry.emplace<TagComponent>(m_camera, "Penumbra Camera");
    auto& camera = registry.emplace<CameraComponent>(m_camera);
    camera.projection = CameraComponent::Projection::Orthographic;
    camera.nearPlane = kNearPlane;
    camera.farPlane = kFarPlane;
    camera.isPrimary = true;
    // It defaults to true, and CameraSystem would fly the view on WASD, Space
    // and Shift - keys the game itself uses.
    camera.flyControlsEnabled = false;
    // Looking down -z, +y up the screen (yaw -90 is the -z front).
    camera.yaw = -90.0f;
    camera.pitch = 0.0f;
    camera.updateCameraVectors();
    camera.orthoHeight = kOriginalScreen.y;
    camera.aspect = kOriginalScreen.x / kOriginalScreen.y;
    camera.position = glm::vec3(kOriginalScreen.x * 0.5f, -kOriginalScreen.y * 0.5f, kCameraZ);
    registry.emplace<TransformComponent>(m_camera).position = camera.position;
}

void CameraRig::Detach(entt::registry& registry) {
    if (m_camera != entt::null && registry.valid(m_camera)) registry.destroy(m_camera);
    m_camera = entt::null;
}

View CameraRig::Update(entt::registry& registry, const Eth::RenderSnapshot& snapshot, bool pillarbox) {
    using namespace Supersonic;
    if (m_camera == entt::null || !registry.valid(m_camera) || !registry.all_of<CameraComponent>(m_camera)) {
        m_camera = entt::null;
        Attach(registry);
    }

    View view = ComputeView(snapshot, WindowPixels(registry, LogicalScreen(snapshot)), pillarbox);
    if (const auto* viewport = registry.ctx().find<ViewportInfo>(); viewport != nullptr) {
        view.imageOrigin = viewport->rect.min;
    }

    // The camera stands at the world point the IMAGE's centre shows: logical
    // pixel (imageCentre - viewportMin) / scale from the logical screen's
    // top-left. With the viewport centred that is (cam.x + W/2, cam.y + H/2);
    // ComputeView rounds viewportMin to a whole pixel, and reading the centre
    // back from it keeps the world exactly where the HUD and the pointer
    // mapping put the logical screen. The world height the whole image shows
    // is its pixel height over the scale, so a pillarboxed or letterboxed frame
    // shows the logical screen exactly inside its viewport. The aspect is the
    // image's; EditorLayer::buildGameView writes the same value into the
    // primary camera after this, every frame.
    auto& camera = registry.get<CameraComponent>(m_camera);
    const glm::vec2 imageCentre = glm::vec2(view.windowPixels) * 0.5f;
    const glm::vec2 centre = view.camera + (imageCentre - view.viewportMin) / view.scale;
    const glm::vec3 position = ToWorld(centre, kCameraZ);
    const float aspect = static_cast<float>(view.windowPixels.x) / static_cast<float>(view.windowPixels.y);
    const float orthoHeight = static_cast<float>(view.windowPixels.y) / view.scale;
    if (camera.position != position) camera.position = position;
    if (camera.aspect != aspect) camera.aspect = aspect;
    if (camera.orthoHeight != orthoHeight) camera.orthoHeight = orthoHeight;
    if (camera.projection != CameraComponent::Projection::Orthographic) {
        camera.projection = CameraComponent::Projection::Orthographic;
    }
    camera.flyControlsEnabled = false;
    if (auto* transform = registry.try_get<TransformComponent>(m_camera); transform && transform->position != position) {
        transform->position = position;
    }

    // SetBackgroundColor is the clear colour and persists across scenes
    // (docs/spec/30 §3.6); the snapshot carries the value of the frame.
    RenderSettings* rendering = registry.ctx().find<RenderSettings>();
    if (rendering == nullptr) rendering = &registry.ctx().emplace<RenderSettings>();
    ApplySceneRendering(*rendering);
    const glm::vec3 clear = BackgroundColour(snapshot.backgroundColor);
    rendering->backgroundColor[0] = clear.r;
    rendering->backgroundColor[1] = clear.g;
    rendering->backgroundColor[2] = clear.b;
    return view;
}

View CameraRig::ComputeView(const Eth::RenderSnapshot& snapshot, glm::uvec2 windowPixels, bool pillarbox) {
    View view;
    view.camera = snapshot.camera;
    view.logicalScreen = LogicalScreen(snapshot);
    view.windowPixels = glm::max(windowPixels, glm::uvec2(1u));
    const glm::vec2 window(view.windowPixels);

    // Pillarboxed: the largest copy of the logical screen that fits, centred
    // (a window narrower than the screen's aspect letterboxes instead).
    // Otherwise the logical screen fills the window's height, which is the
    // scale View documents, and is centred across its width - the whole
    // window whenever the logical screen was sized to the window's aspect.
    view.scale = pillarbox ? std::min(window.x / view.logicalScreen.x, window.y / view.logicalScreen.y)
                           : window.y / view.logicalScreen.y;
    const glm::vec2 shown = view.logicalScreen * view.scale;
    // A whole pixel: FontAtlas lands every glyph edge on a whole image pixel
    // relative to this corner, which only stays sharp if the corner is one
    // (an odd bar total, e.g. 1365 - 1024, would otherwise put it at .5).
    // Rounded, not floored: a logical screen sized to the window's aspect
    // comes out a hair wider than the window (1365.3334 x 1.40625 =
    // 1920.0001) and must stay at 0, not -1. Update centres the camera on
    // this rounded box, not on the exact middle.
    view.viewportMin = glm::round((window - shown) * 0.5f);
    view.viewportMax = view.viewportMin + shown;
    return view;
}

glm::uvec2 CameraRig::WindowPixels(const entt::registry& registry, const glm::vec2& fallback) {
    glm::vec2 size = fallback;
    if (const auto* viewport = registry.ctx().find<Supersonic::ViewportInfo>(); viewport != nullptr) {
        const glm::vec2 published = viewport->Size();
        if (published.x >= 1.0f && published.y >= 1.0f) size = published;
    }
    return glm::uvec2(static_cast<unsigned>(std::max(1.0f, std::round(size.x))),
                      static_cast<unsigned>(std::max(1.0f, std::round(size.y))));
}

std::vector<CameraRig::Bar> CameraRig::Bars(const View& view) {
    std::vector<Bar> bars;
    const glm::vec2 window(glm::max(view.windowPixels, glm::uvec2(1u)));
    const glm::vec2 inner0 = glm::clamp(view.viewportMin, glm::vec2(0.0f), window);
    const glm::vec2 inner1 = glm::clamp(view.viewportMax, glm::vec2(0.0f), window);
    const auto add = [&](glm::vec2 min, glm::vec2 max) {
        if (max.x - min.x < kMinBarPixels || max.y - min.y < kMinBarPixels) return;
        bars.push_back(Bar{min / window, max / window});
    };
    // Left and right full height, top and bottom between them.
    add({0.0f, 0.0f}, {inner0.x, window.y});
    add({inner1.x, 0.0f}, {window.x, window.y});
    add({inner0.x, 0.0f}, {inner1.x, inner0.y});
    add({inner0.x, inner1.y}, {inner1.x, window.y});
    return bars;
}

void CameraRig::AddBars(Supersonic::ScreenOverlay& overlay, const View& view) {
    for (const Bar& bar : Bars(view)) {
        Supersonic::ScreenOverlay::Quad quad;
        quad.min = bar.min;
        quad.max = bar.max;
        quad.color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        overlay.Add(quad);
    }
}

glm::vec3 CameraRig::BackgroundColour(Eth::uint argb) {
    return glm::vec3(static_cast<float>((argb >> 16) & 0xFFu), static_cast<float>((argb >> 8) & 0xFFu),
                     static_cast<float>(argb & 0xFFu)) /
           255.0f;
}

} // namespace Penumbra::Render
