// Script.hpp first: an engine header that reaches <windows.h> would turn the
// script API's DrawText into DrawTextA and its min/max into macros.
#include "script/Script.hpp"

#include "PenumbraLayer.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "core/Application.hpp"
#include "core/Log.hpp"
#include "core/SimulationClock.hpp"
#include "core/WindowControl.hpp"
#include "eth/Machine.hpp"
#include "render/DrawOrder.hpp"

namespace Penumbra {

namespace {

// The scenes laid out for the original's 1024x768: their buttons, panels and
// thumbnails sit at fixed pixels (menu.as, videoModes.as, gameover.as), so
// they keep that screen and are pillarboxed rather than widened.
bool IsFixedLayoutScene(const std::string& sceneFile) {
    for (const char* name : {"menu.esc", "arena_select.esc", "videoModes.esc", "gameover.esc"}) {
        if (sceneFile.find(name) != std::string::npos) return true;
    }
    return false;
}

} // namespace

PenumbraLayer::PenumbraLayer(Options options)
    : m_options(std::move(options)), m_settings(m_options.settings), m_textures(PENUMBRA_ORIGINAL_DIR) {}

PenumbraLayer::~PenumbraLayer() = default;

Eth::vector2 PenumbraLayer::LogicalScreenFor(const std::string& sceneFile) const {
    constexpr float kHeight = 768.0f;
    if (!Widescreen() || IsFixedLayoutScene(sceneFile)) return {1024.0f, kHeight};
    // E1: as wide as the window's shape, never narrower than the original.
    // Fixed per scene: the scripts read GetScreenSize() every frame, but a
    // scene's spawn triggers and camera dead zone should not move under a
    // player who resizes mid-level.
    const glm::uvec2 window = m_options.windowPixels;
    const float aspect = window.y > 0 ? static_cast<float>(window.x) / static_cast<float>(window.y) : 4.0f / 3.0f;
    return {std::max(1024.0f, std::round(kHeight * aspect)), kHeight};
}

void PenumbraLayer::OnAttach(entt::registry& registry) {
    // The original's frame ran once per 60 Hz vsync; its scripts count on it.
    if (auto* clock = registry.ctx().find<Supersonic::SimulationClock>()) clock->fixedDelta = kTick;

    m_localization.Load();
    ApplyLanguage();

    // Fullscreen from the first frame is the monitor's size, not the one main()
    // asked for: the first scene's widescreen width follows what is shown.
    if (Supersonic::WindowControl* window = WindowControlOf(registry); window != nullptr && window->IsFullscreen()) {
        const glm::uvec2 size = window->WindowSize();
        if (size.x > 0 && size.y > 0) m_options.windowPixels = size;
    }

    Eth::MachineConfig config;
    config.userRoot = m_options.userDir.string();
    config.screenSize = LogicalScreenFor("scenes/menu.esc");
    config.screenSizeForScene = [this](const std::string& scene) { return LogicalScreenFor(scene); };
    m_machine = std::make_unique<Eth::Machine>(config);

    m_audio.Attach(registry);
    // Music is decoded whole; keeping it across LoadScene spares a hitch on
    // every death and level change (the scripts reload the same few files).
    m_audio.SetKeepDecodedClips(true);
    m_machine->Samples().SetOutput(&m_audio);
    m_machine->Samples().SetMasterVolumes(m_settings.musicVolume, m_settings.effectsVolume);

    // The original's option switches, remembered across launches (E6).
    Script::g_controls.setCurrent(static_cast<Eth::uint>(m_settings.controls.joystickLayout));
    Script::g_enablePS.setCurrent(m_settings.pixelShaders ? 0u : 1u);
    // Row 1 is "Tela-cheia". From what the window opens as, not the settings: a
    // --windowed or --fullscreen run would otherwise be switched back by the
    // menu's first frame (menu.as:103-106).
    Script::g_windowed.setCurrent(m_options.startFullscreen ? 1u : 0u);
    m_input.SetControls(m_settings.controls);

    // Textures first: a key handed out before the registry is attached is
    // never uploaded, and the pools keep the first key they are given.
    m_textures.Attach(registry);
    m_rig.Attach(registry);
    m_fonts.Attach(registry);
    m_sprites.Attach(registry, m_textures);
    m_sprites.SetLocalization(&m_localization);   // E5: the menu's worded images and their normal maps
    m_shadows.Attach(registry, m_textures);
    m_lights.Attach(registry, m_textures);
    m_particles.Attach(registry, m_textures);
    m_hud.Attach(registry, m_textures, m_fonts, m_localization);

    {
        Eth::Machine::Scope scope(*m_machine);
        Script::RegisterAll(*m_machine);
        m_machine->Boot(Script::ScriptMain);
    }
    // ScriptMain asked for a 1024x768 window (main.as:144); the settings and the
    // flags decided the window already. Its Windowed() must say what the window
    // is, as 0.7.12's did, for the menu's switch to agree with it.
    Eth::WindowRequest& windowRequest = m_machine->Window();
    windowRequest.windowed = !m_options.startFullscreen;
    windowRequest.changed = false;
    m_windowFullscreen = m_options.startFullscreen;
    RefreshVideoModes(registry);

    // The first tick runs before any OnUpdate has measured the window.
    Eth::RenderSnapshot seed;
    seed.screenSize = config.screenSize;
    m_view = Render::CameraRig::ComputeView(
        seed, Render::CameraRig::WindowPixels(registry, glm::vec2(m_options.windowPixels)), m_pillarbox);
}

void PenumbraLayer::OnDetach(entt::registry& registry) {
    m_hud.Detach();
    m_particles.Detach(registry);
    m_lights.Detach(registry);
    m_shadows.Detach(registry);
    m_sprites.Detach(registry);
    m_rig.Detach(registry);
    m_fonts.Detach();
    // Before the output goes: tearing the scene down stops particle sounds
    // through the bank, which must not reach a detached engine.
    if (m_machine) m_machine->Samples().SetOutput(nullptr);
    m_audio.Detach();
    m_machine.reset();
}

Supersonic::WindowControl* PenumbraLayer::WindowControlOf(entt::registry& registry) {
    auto* const* window = registry.ctx().find<Supersonic::WindowControl*>();
    return window != nullptr ? *window : nullptr;
}

bool PenumbraLayer::Widescreen() const {
    return m_options.widescreenOverride.value_or(m_settings.widescreen);
}

bool PenumbraLayer::Portuguese() const {
    return m_options.languageOverride.value_or(m_settings.language) == "pt";
}

void PenumbraLayer::ApplyLanguage() {
    m_localization.SetLanguage(Portuguese() ? Render::Language::Portuguese : Render::Language::English);
}

void PenumbraLayer::SaveSettings() {
    ApplyLanguage();
    std::string error;
    if (!m_options.userDir.empty() && !m_settings.Save(m_options.userDir, &error)) {
        SUPERSONIC_LOG_WARN("Penumbra") << "settings not saved: " << error << std::endl;
    }
}

void PenumbraLayer::RefreshVideoModes(entt::registry& registry) {
    Supersonic::WindowControl* window = WindowControlOf(registry);
    if (window == nullptr) return;
    // E2: each size once. 0.7.12 listed every refresh rate of a size again, as
    // identical "WxHx32" lines (videoModes.as:99-101); the port opens a window
    // or covers the monitor at its current rate, so a rate is nothing to pick.
    std::vector<Eth::videoMode> modes;
    for (const Supersonic::DisplayMode& mode : window->DisplayModes()) {
        const bool listed = std::any_of(modes.begin(), modes.end(), [&](const Eth::videoMode& m) {
            return m.width == mode.width && m.height == mode.height;
        });
        if (!listed) modes.push_back(Eth::videoMode{mode.width, mode.height, Eth::PF32BIT});
    }
    m_machine->SetVideoModes(std::move(modes));
}

// SetWindowProperties as the scripts use it: the windowed flag flipped
// (Alt+Enter or the options screen's switch, menu.as:108-119), or a line of the
// mode list with the flag as it was (videoModes.as:110).
void PenumbraLayer::ApplyWindowRequest(entt::registry& registry) {
    Eth::WindowRequest& request = m_machine->Window();
    if (!request.changed) return;
    request.changed = false;
    Supersonic::WindowControl* window = WindowControlOf(registry);

    const bool fullscreen = !request.windowed;
    if (fullscreen != m_windowFullscreen) {
        // Only the flag: the size that comes with it is the logical screen's
        // (menu.as:111), which is not the window's here. The window keeps its
        // windowed size to come back to.
        m_windowFullscreen = fullscreen;
        if (window != nullptr) window->SetFullscreen(fullscreen);
        if (fullscreen != m_settings.fullscreen) {
            m_settings.fullscreen = fullscreen;
            SaveSettings();
        }
        return;
    }

    // A mode picked from the list. 0.7.12 switched the display to it; the port
    // sizes the window to it (the size to return to, while fullscreen) and
    // renders at that size.
    if (window == nullptr || !window->SetWindowedSize(request.width, request.height)) return;
    const int width = static_cast<int>(request.width);
    const int height = static_cast<int>(request.height);
    if (width != m_settings.windowWidth || height != m_settings.windowHeight) {
        m_settings.windowWidth = width;
        m_settings.windowHeight = height;
        SaveSettings();
    }
}

void PenumbraLayer::ApplyDevHolds(Eth::InputFrame& frame) const {
    for (const DevHold& hold : m_options.holds) {
        if (m_ticks >= hold.from && m_ticks <= hold.to) frame.keys[static_cast<std::size_t>(hold.key)] = true;
    }
}

// --start: what the menu's New Game does once its fade is over (main.as:99-122),
// without the menu - for captures and suites that want a level at once.
void PenumbraLayer::StartDevScene() {
    const std::string& scene = m_options.startScene;
    const bool pvp = scene.rfind("pvp_", 0) == 0;
    Script::resetData();
    Eth::UsePixelShaders(true);
    Eth::LoadScene("scenes/" + scene, "setupScene", pvp ? "pvpLoop" : "levelLoop");
    SUPERSONIC_LOG_INFO("Penumbra") << "dev start: scenes/" << scene << std::endl;
}

void PenumbraLayer::OnFixedUpdate(entt::registry& registry, float fixedDelta) {
    (void)fixedDelta;
    Eth::Machine::Scope scope(*m_machine);

    // Which pad player 2 reads follows the live g_controls switch.
    m_input.SetPlayer2Pad(static_cast<int>(Script::getPlayerJoystick(1)));
    Eth::InputFrame frame = m_input.BuildTick(m_view);
    ApplyDevHolds(frame);
    if (m_options.devCursor) frame.cursor = frame.cursorAbsolute = *m_options.devCursor;
    m_machine->Frame(frame);   // steps the key and button state machines itself
    ++m_ticks;

    if (!m_options.startScene.empty() && !m_devStarted) {
        m_devStarted = true;
        StartDevScene();
    }

    Eth::vector2 warp;
    if (m_machine->Input().TakeCursorRequest(warp)) m_input.WarpCursor(warp, m_view);

    ApplyWindowRequest(registry);

    // The monitor's modes are read when a scene opens, not every tick: the
    // options screen is a scene, and the window may have moved screens since.
    if (m_machine->CurrentScene() != m_modesScene) {
        m_modesScene = m_machine->CurrentScene();
        RefreshVideoModes(registry);
    }

    // The options screen's switches are the original's own globals; keep the
    // settings file in step with them.
    const int layout = static_cast<int>(Script::g_controls.getCurrent());
    const bool pixelShaders = Script::g_enablePS.getCurrent() == 0;
    if (layout != m_settings.controls.joystickLayout || pixelShaders != m_settings.pixelShaders) {
        m_settings.controls.joystickLayout = layout;
        m_settings.pixelShaders = pixelShaders;
        m_input.SetControls(m_settings.controls);
        SaveSettings();
    }

    if (m_machine->QuitRequested()) Supersonic::Application::RequestQuit();
}

void PenumbraLayer::OnUpdate(entt::registry& registry, float deltaTime) {
    (void)deltaTime;
    const Eth::RenderSnapshot& snapshot = m_machine->Snapshot();

    if (Supersonic::WindowControl* window = WindowControlOf(registry)) {
        // 0.7.12 hid the system pointer while the scripts asked (main.as:133)
        // and drew cursor.ent in its place. Every frame is cheap: it does
        // nothing when the request already stands, and the focus and editor
        // vetoes stay Input's.
        window->SetCursorVisible(!snapshot.cursorHidden);
        // The next scene is as wide as the window is by then (E1), after a
        // fullscreen switch or a picked mode. Zero while minimised: keep the last.
        const glm::uvec2 size = window->WindowSize();
        if (size.x > 0 && size.y > 0) m_options.windowPixels = size;
    }

    m_view = m_rig.Update(registry, snapshot, m_pillarbox);
    const Render::DrawOrder order = Render::ComputeDrawOrder(snapshot);
    m_sprites.Draw(registry, snapshot, m_view, order);
    m_shadows.Draw(registry, snapshot, m_view, order);
    m_lights.Draw(registry, snapshot, m_view, order);
    m_particles.Draw(registry, snapshot, m_view, order);
    m_hud.Draw(registry, snapshot, m_view);   // once a frame: it begins the font atlas's frame
    m_input.EndFrame();                       // once a frame, tick or not
}

} // namespace Penumbra
