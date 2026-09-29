#pragma once

// The engine camera the snapshot is seen through, and the scene's rendering
// settings, owned by one object so the two cannot disagree about what a
// logical pixel is.
//
// ETHANON: the camera is the world position of the screen's top-left, the
// screen is GetScreenSize() logical pixels, drawn 1:1 into the backbuffer.
// ENGINE: an orthographic camera at the logical screen's centre, orthoHeight
// the world height the window shows, looking down -z at the plane every
// renderer draws in, ranks ordered by z (render/View.hpp).
//
// The window's size in game mode: EditorLayer::buildGameView publishes the
// rectangle the game is drawn into as Supersonic::ViewportInfo in
// registry.ctx() every frame, and resizes the offscreen target to it at the
// start of the next frame (EditorLayer::ApplyPendingResize) - so the rectangle
// a layer reads in OnUpdate (which runs before BuildUI) is exactly the size of
// the image this frame renders into. Before the first BuildUI there is none;
// the logical screen stands in for that one frame.

#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include "eth/Snapshot.hpp"
#include "render/View.hpp"

namespace Supersonic {
class ScreenOverlay;
}

namespace Penumbra::Render {

class CameraRig {
public:
    // The camera stands at z = kCameraZ and sees world z in
    // [kCameraZ - kFarPlane, kCameraZ - kNearPlane] = [-1000, 999]: every rank
    // RankZ can make (kLayerBase .. kLayerBase + 1000) and the background just
    // behind rank 0.
    static constexpr float kCameraZ = 1000.0f;
    static constexpr float kNearPlane = 1.0f;
    static constexpr float kFarPlane = 2000.0f;

    // A bar thinner than this is a rounding sliver, not a bar.
    static constexpr float kMinBarPixels = 1.0f;

    // A black bar, as a ScreenOverlay::Quad rectangle: fractions of the image,
    // (0, 0) top-left, +y down.
    struct Bar {
        glm::vec2 min{0.0f};
        glm::vec2 max{0.0f};
    };

    // Creates the camera entity (primary, and every other camera stops being
    // primary) and puts the scene's RenderSettings in registry.ctx(): display-
    // encoded, a flat background colour, no bloom. Must run before the first
    // frame that draws a sprite - TextureCache's uploads are found only by a
    // DisplayEncoded scene (TextureCache.cpp).
    void Attach(entt::registry& registry);
    void Detach(entt::registry& registry);

    // Once a frame, before the renderers: places the camera for the snapshot,
    // writes the snapshot's background colour into RenderSettings, and returns
    // the frame's View (with imageOrigin from ViewportInfo). `pillarbox` keeps
    // the logical screen's aspect inside the window, centred, with black bars
    // (Bars) over the rest - or, where the snapshot collected the world past
    // the screen's sides (RenderSnapshot::sideMargin, E1's wide menus), that
    // world in place of the side bars; otherwise the logical screen is scaled to the
    // window's height and centred across it (the widescreen view, where the two
    // aspects match). Attaches itself if Attach was not called.
    View Update(entt::registry& registry, const Eth::RenderSnapshot& snapshot, bool pillarbox);

    // The View for a snapshot shown in a window of `windowPixels`, with no
    // registry (for suites, and for the layer's first tick, which runs before
    // any Update). viewportMin is rounded to a whole pixel; imageOrigin is 0.
    static View ComputeView(const Eth::RenderSnapshot& snapshot, glm::uvec2 windowPixels, bool pillarbox);

    // The image the game renders into this frame (ViewportInfo), or `fallback`
    // rounded when nothing has published one yet.
    static glm::uvec2 WindowPixels(const entt::registry& registry, const glm::vec2& fallback);

    // The parts of the image outside the logical screen, in ScreenOverlay
    // fractions: left and right full height, then top and bottom between them.
    // Empty when the logical screen covers the image. THE one rule for the
    // bars: HudRenderer::Build draws exactly these, last, in black. Under E1's
    // open sides (View::openSides, a menu in a wide window) the scene shows
    // past the screen's left and right edges, and only what lies beyond that
    // (View::ShownMin/ShownMax) is barred.
    static std::vector<Bar> Bars(const View& view);

    // Adds Bars(view) to the overlay as opaque black quads. NOT for a layer
    // that runs HudRenderer, which already ends its pass with the same bars
    // (so a HUD primitive reaching past the logical screen is cut off as the
    // original's backbuffer cut it off); calling both draws them twice. For a
    // frame drawn without the HUD renderer.
    static void AddBars(Supersonic::ScreenOverlay& overlay, const View& view);

    // SetBackgroundColor's ARGB as the display-encoded clear colour.
    static glm::vec3 BackgroundColour(Eth::uint argb);

    entt::entity Camera() const { return m_camera; }

private:
    entt::entity m_camera{entt::null};
};

} // namespace Penumbra::Render
