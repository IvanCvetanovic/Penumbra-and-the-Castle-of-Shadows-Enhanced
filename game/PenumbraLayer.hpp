#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include "core/EngineLayer.hpp"

#include "eth/EthTypes.hpp"
#include "render/AudioOutEngine.hpp"
#include "render/CameraRig.hpp"
#include "render/FontAtlas.hpp"
#include "render/HudRenderer.hpp"
#include "render/InputMapper.hpp"
#include "render/Interpolation.hpp"
#include "render/LightRenderer.hpp"
#include "render/Localization.hpp"
#include "render/ParticleRenderer.hpp"
#include "render/Settings.hpp"
#include "render/ShadowRenderer.hpp"
#include "render/SpriteRenderer.hpp"
#include "render/TextureCache.hpp"
#include "render/View.hpp"

namespace Supersonic {
class WindowControl;
}

namespace Penumbra {

namespace Eth {
class Machine;
class Scene;
struct InputFrame;
}

// Penumbra as the engine plays it.
//
//  - ONE ETHANON FRAME PER TICK. OnFixedUpdate runs eth/Machine::Frame once,
//    on the 60 Hz tick the original's scripts were written against (they count
//    frames in places and milliseconds in others, and only agree at 60).
//  - The picture is the Machine's RenderSnapshot, which it takes at 0.7.12's
//    render point. OnUpdate draws it every frame, including frames between
//    ticks, with pooled engine entities (render/): sprites, shadows, Light2D
//    lights and halos, particles, then the HUD through the ScreenOverlay.
//  - The world is seen through a logical screen 768 pixels tall (the
//    original's height) and as wide as the window's aspect (enhancement E1);
//    the menus, laid out for 1024x768, keep that size and are pillarboxed.
//  - Settings (render/Settings.hpp) persist in the user directory; the
//    original's own options switches are seeded from them.
//  - The window follows the scripts (E2): SetWindowProperties's windowed flag
//    (Alt+Enter, the options screen's switch) and its mode list go to the
//    engine's WindowControl, and HideCursor hides the system pointer, as
//    0.7.12 did while cursor.ent was drawn in its place.
class PenumbraLayer final : public Supersonic::EngineLayer {
public:
    static constexpr float kTick = 1.0f / 60.0f;

    // A key held over a span of ticks, for headless captures (--hold): the
    // Ethanon KEY and the first and last tick it is down (inclusive, counted
    // from the first tick this layer runs).
    struct DevHold {
        Eth::KEY key = Eth::K_UP;
        unsigned from = 0;
        unsigned to = 0;
    };

    struct Options {
        // The original's files and the port's own data (strings.json,
        // images/en), as main.cpp found them (eth/Paths.hpp): beside the
        // executable in a packaged game, else where the build points.
        std::filesystem::path originalDir = PENUMBRA_ORIGINAL_DIR;
        std::filesystem::path dataDir = PENUMBRA_DATA_DIR;
        std::filesystem::path userDir;      // where saves and settings go; empty = none
        std::string startScene;             // "" = the menu, as the original boots
        glm::uvec2 windowPixels{1366, 768}; // what the window opens at; then kept current (for the logical width)
        std::vector<DevHold> holds;
        Render::Settings settings;          // as loaded from the user directory (main also sizes the window)
        bool startFullscreen = false;       // what the window opens as (flags over the settings)
        // This run's overrides (--lang, --widescreen): they change what is
        // shown and never reach settings.json, which keeps what the player chose.
        std::optional<std::string> languageOverride;
        std::optional<bool> widescreenOverride;
        // E8 for this run (--smooth on|off; off under --fixed-step unless
        // --smooth on): never saved, like the two above.
        std::optional<bool> smoothMotionOverride;
        // --cursor x,y: the scripts' cursor pinned at a logical-screen point,
        // for headless captures of the mouse-driven menu (the live OS pointer
        // otherwise decides which panel a capture shows).
        std::optional<glm::vec2> devCursor;
    };

    explicit PenumbraLayer(Options options);
    ~PenumbraLayer() override;

    const char* Name() const override { return "Penumbra"; }
    void OnAttach(entt::registry& registry) override;
    void OnDetach(entt::registry& registry) override;
    void OnFixedUpdate(entt::registry& registry, float fixedDelta) override;
    void OnUpdate(entt::registry& registry, float deltaTime) override;

    Eth::Machine* Machine() { return m_machine.get(); }

private:
    // The screen the scripts see for a scene (GetScreenSize): the menus'
    // 1024x768, or the widescreen view.
    Eth::vector2 LogicalScreenFor(const std::string& sceneFile) const;
    void ApplyDevHolds(Eth::InputFrame& frame) const;
    void StartDevScene();
    // The engine's window, or null (a host that publishes none).
    static Supersonic::WindowControl* WindowControlOf(entt::registry& registry);
    // What the scripts asked of the window this tick, handed to the engine.
    void ApplyWindowRequest(entt::registry& registry);
    // GetVideoMode's list, from the monitor the window is on.
    void RefreshVideoModes(entt::registry& registry);
    // The HUD's and the sprites' language, from m_settings (or --lang).
    void ApplyLanguage();
    // What this run shows: the settings unless a flag overrides them.
    bool Widescreen() const;
    bool Portuguese() const;
    bool SmoothMotion() const;
    // Writes m_settings to the user directory and re-applies what it drives.
    void SaveSettings();

    Options m_options;
    Render::Settings m_settings;
    Render::TextureCache m_textures;
    Render::FontAtlas m_fonts;
    Render::Localization m_localization;
    Render::AudioOutEngine m_audio;
    Render::InputMapper m_input;
    Render::CameraRig m_rig;
    Render::SpriteRenderer m_sprites;
    Render::ShadowRenderer m_shadows;
    Render::LightRenderer m_lights;
    Render::ParticleRenderer m_particles;
    Render::HudRenderer m_hud;
    // E8: the world drawn between the last two ticks (render/Interpolation.hpp).
    Render::SnapshotInterpolator m_interp;
    std::unique_ptr<Eth::Machine> m_machine;
    Render::View m_view;
    bool m_pillarbox = true;
    unsigned m_ticks = 0;
    bool m_devStarted = false;
    // The last windowed/fullscreen state asked of the engine. Not the Machine's
    // (SetWindowProperties overwrites it before the layer sees the request) and
    // not WindowControl::IsFullscreen (a frame late).
    bool m_windowFullscreen = false;
    const Eth::Scene* m_modesScene = nullptr;   // the scene the mode list was read for
};

} // namespace Penumbra
