#pragma once

#include <filesystem>
#include <memory>
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
#include "render/LightRenderer.hpp"
#include "render/Localization.hpp"
#include "render/ParticleRenderer.hpp"
#include "render/Settings.hpp"
#include "render/ShadowRenderer.hpp"
#include "render/SpriteRenderer.hpp"
#include "render/TextureCache.hpp"
#include "render/View.hpp"

namespace Penumbra {

namespace Eth {
class Machine;
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
        std::filesystem::path userDir;      // where saves and settings go; empty = none
        std::string startScene;             // "" = the menu, as the original boots
        glm::uvec2 windowPixels{1366, 768}; // what the window opens at (for the logical width)
        std::vector<DevHold> holds;
        Render::Settings settings;          // loaded by main (it also sizes the window)
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
    std::unique_ptr<Eth::Machine> m_machine;
    Render::View m_view;
    bool m_pillarbox = true;
    unsigned m_ticks = 0;
    bool m_devStarted = false;
};

} // namespace Penumbra
