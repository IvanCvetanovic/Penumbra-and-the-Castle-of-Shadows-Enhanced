// Script.hpp first: an engine header that reaches <windows.h> would turn the
// script API's DrawText into DrawTextA and its min/max into macros.
#include "script/Script.hpp"

#include "PenumbraLayer.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "core/Application.hpp"
#include "core/Input.hpp"
#include "core/Light2D.hpp"
#include "core/Log.hpp"
#include "core/SimulationClock.hpp"
#include "core/WindowControl.hpp"
#include "eth/Machine.hpp"
#include "eth/Paths.hpp"
#include "platform/SafeArea.hpp"
#include "render/DrawOrder.hpp"
#include "render/WindowMode.hpp"

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

// ETHShaderManager's m_fakeEyeHeight (ETHShaderManager.cpp:55).
constexpr float kFakeEyeHeight = 768.0f;

// Fullscreen at the player's mode, or at the desktop's when there is none or
// this monitor does not offer it (SetFullscreenMode logs which). The setting
// is kept either way, for the monitor that does.
void RequestFullscreen(Supersonic::WindowControl& window, glm::uvec2 mode) {
    if (mode.x > 0 && mode.y > 0 && window.SetFullscreenMode(mode.x, mode.y)) return;
    window.SetFullscreen(true);
}

} // namespace

PenumbraLayer::PenumbraLayer(Options options)
    : m_options(std::move(options)),
      m_settings(m_options.settings),
      m_textures(m_options.originalDir.generic_string()) {}

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

    m_localization.Load((m_options.dataDir / Eth::kDataMarker).generic_string());
    ApplyLanguage();

    // Fullscreen from the first frame is the monitor's size, not the one main()
    // asked for: the first scene's widescreen width follows what is shown.
    if (Supersonic::WindowControl* window = WindowControlOf(registry); window != nullptr && window->IsFullscreen()) {
        const glm::uvec2 size = window->WindowSize();
        if (size.x > 0 && size.y > 0) m_options.windowPixels = size;
        // The engine went fullscreen at the desktop's mode before any layer
        // could name one. A saved mode (Step 23) switches to it at the top of
        // the first frame, so the first scene is as wide as that mode is.
        const glm::uvec2 mode = SavedFullscreenMode();
        if (m_options.startFullscreen && mode.x > 0 && mode.y > 0 && window->SetFullscreenMode(mode.x, mode.y)) {
            m_options.windowPixels = mode;
        }
    }

    Eth::MachineConfig config;
    config.gameRoot = m_options.originalDir.generic_string();
    config.userRoot = m_options.userDir.string();
    config.screenSize = LogicalScreenFor("scenes/menu.esc");
    config.screenSizeForScene = [this](const std::string& scene) { return LogicalScreenFor(scene); };
    m_machine = std::make_unique<Eth::Machine>(config);

    m_audio.Attach(registry);
    // Music is decoded whole; keeping it across LoadScene spares a hitch on
    // every death and level change (the scripts reload the same few files).
    m_audio.SetKeepDecodedClips(true);
    m_machine->Samples().SetOutput(&m_audio);
    ApplyVolumes();
    m_pause.SetAutoPause(PauseOnFocusLoss());   // E13
    // E16: the touch controls - on a phone by default, on the desktop with
    // --touch (or touchControls "on"), where the mouse is the finger.
    SetTouchEnabled(
        m_options.touchOverride.value_or(Render::TouchControls::EnabledBySetting(m_settings.touchControls)));
    // E20: a phone's options screen (Script.hpp). From the build, not from the
    // controls: a desktop run with --touch still has a window to switch.
    Script::g_mobileLayout = Render::kMobileBuild;

    // The original's option switches, remembered across launches (E6).
    Script::g_controls.setCurrent(static_cast<Eth::uint>(m_settings.controls.joystickLayout));
    Script::g_enablePS.setCurrent(m_settings.pixelShaders ? 0u : 1u);
    // Row 1 is "Tela-cheia". From what the window opens as, not the settings: a
    // --windowed or --fullscreen run would otherwise be switched back by the
    // menu's first frame (menu.as:103-106).
    Script::g_windowed.setCurrent(m_options.startFullscreen ? 1u : 0u);
    // E10: the enhanced settings' own rows on the options screen. Language and
    // view from what this run shows (a --lang or --widescreen flag included),
    // as g_windowed is, so the screen's first frame changes nothing.
    Script::g_language.setCurrent(Portuguese() ? 0u : 1u);
    Script::g_widescreen.setCurrent(Widescreen() ? 0u : 1u);
    Script::g_keyboardP2.setCurrent(m_settings.controls.keyboardPlayer2 ? 0u : 1u);
    Script::g_musicVolume.setCurrent(Script::g_musicVolume.stepFor(m_settings.musicVolume));
    Script::g_effectsVolume.setCurrent(Script::g_effectsVolume.stepFor(m_settings.effectsVolume));
    Script::g_smoothMotion.setCurrent(SmoothMotion() ? 0u : 1u);   // E8's row: as this run draws (--smooth, --fixed-step)
    Script::g_pauseOnFocusLoss.setCurrent(PauseOnFocusLoss() ? 0u : 1u);   // E13's, likewise
    Script::g_touchControls.setCurrent(m_touchEnabled ? 0u : 1u);   // E20's: as this run has them (--touch)
    m_input.SetControls(m_settings.controls);
    m_interp.SetEnabled(SmoothMotion());

    // Textures first: a key handed out before the registry is attached is
    // never uploaded, and the pools keep the first key they are given.
    m_textures.Attach(registry);
    m_rig.Attach(registry);
    // The stand-ins for the Windows faces come with the data found (FontAtlas.hpp).
    m_fonts.SetBundledFontsDirectory((m_options.dataDir / "fonts").generic_string());
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

bool PenumbraLayer::SmoothMotion() const {
    return m_options.smoothMotionOverride.value_or(m_settings.smoothMotion);
}

bool PenumbraLayer::PauseOnFocusLoss() const {
    return m_options.pauseOnFocusLossOverride.value_or(m_settings.pauseOnFocusLoss);
}

// E13: the loops doLoop runs under (setupScene.as:226-240) - but not their end
// screens (g_gameFinished), where Esc and Back already lead to the menu
// (doLoop's waitForInputToMenu) and a pause would only stand in the way.
bool PenumbraLayer::InPlayScene() const {
    const std::string& loop = m_machine->LoopFunction();
    return (loop == "levelLoop" || loop == "pvpLoop") && !Script::g_gameFinished;
}

std::vector<Render::TouchContact> PenumbraLayer::TouchContacts() const {
    std::vector<Render::TouchContact> contacts;
    const int count = Supersonic::Input::ContactCount();
    for (int i = 0; i < count; ++i) {
        const Supersonic::Contact contact = Supersonic::Input::GetContact(i);
        if (contact.id < 0) continue;
        // The contacts are in Input::MousePosition()'s coordinates, so the
        // pointer's own mapping takes them to the logical screen.
        const glm::vec2 logical = Render::InputMapper::WindowToLogical(contact.position, m_view);
        contacts.push_back(Render::TouchContact{contact.id, logical, contact.phase != Supersonic::ContactPhase::Ended});
    }
    return contacts;
}

void PenumbraLayer::ApplyTouch(Eth::InputFrame& frame) {
    Render::TouchInput input;
    input.contacts = TouchContacts();
    input.screen = m_machine->GetScreenSize();
    // The window's safe area (a notch, rounded corners, a bar that shows), in
    // window pixels from the platform - zero on the desktop - brought into the
    // logical screen the controls are laid out in.
    input.safeArea = Render::TouchInsets{};
    const Supersonic::SafeAreaInsets safe = Supersonic::SafeArea::Get();
    if (!safe.IsZero()) {
        input.safeArea = Render::TouchControls::WindowInsetsToLogical(
            Render::TouchInsets{safe.left, safe.top, safe.right, safe.bottom}, m_view);
    }

    // The controls are a level's or an arena's (the loops doLoop runs under,
    // their end screens included: the wizard still walks there). Everything
    // else - the menus, the options, game over - and the pause are clicked.
    const std::string& loop = m_machine->LoopFunction();
    const bool level = loop == "levelLoop" || loop == "pvpLoop";
    const bool paused = m_pause.Paused();
    input.scene = level && !paused ? Render::TouchScene::Play : Render::TouchScene::Menu;
    // Pause, back or nothing, screen by screen: TouchControls::CornerFor.
    input.corner = Render::TouchControls::CornerFor(
        Render::TouchScreen{m_machine->GetSceneFileName(), level, Script::g_gameFinished, paused});
    // E16's combo buttons press toward the way the wizard faces: his own
    // currentDir (controlCharacter writes it from every device, a level's
    // start leaves it unwritten, which reads as RIGHT), from the last tick.
    if (level) {
        const Eth::ETHEntity wizard = Eth::SeekEntity(Script::MAIN_CHARACTER_ENTITY0);
        if (wizard != nullptr) {
            input.facing = wizard->GetUIntData("currentDir") == Script::LEFT ? Render::TouchFacing::Left
                                                                            : Render::TouchFacing::Right;
        }
    }
    input.sceneSerial = m_machine->Snapshot().sceneSerial;

    const Render::TouchStep step = m_touch.Update(input);
    Render::TouchControls::ApplyToFrame(step, frame);
    // The cursor stays where the finger lifted, as the mouse's would: the
    // mapper keeps it until the scripts or the real mouse move it.
    if (step.pointer) m_input.WarpCursor(step.pointerPos, m_view);
}

void PenumbraLayer::ApplyVolumes() {
    m_machine->Samples().SetMasterVolumes(m_settings.musicVolume * m_pause.MusicScale(), m_settings.effectsVolume);
}

void PenumbraLayer::SetTouchEnabled(bool enabled) {
    if (enabled && !m_touchManifestLoaded) {
        std::string warning;
        const std::filesystem::path manifest = m_options.dataDir / Render::TouchControls::kManifestFile;
        m_touch.SetManifest(Render::TouchControls::LoadManifest(manifest, &warning));
        m_touch.SetImageRoot(m_options.dataDir);
        m_touchManifestLoaded = true;
        if (!warning.empty()) SUPERSONIC_LOG_WARN("Penumbra") << "touch controls: " << warning << std::endl;
    }
    // Said when they come on, and when they go off - not for a run that never
    // had them, as before E20.
    if (enabled || m_touchEnabled) {
        SUPERSONIC_LOG_INFO("Penumbra") << "touch controls " << (enabled ? "on" : "off") << std::endl;
    }
    // A combo does not wait for them to come back on.
    if (!enabled) m_touch.CancelCombo();
    m_touchEnabled = enabled;
}

void PenumbraLayer::SaveSettings() {
    ApplyLanguage();
    m_interp.SetEnabled(SmoothMotion());
    m_pause.SetAutoPause(PauseOnFocusLoss());
    std::string error;
    if (!m_options.userDir.empty() && !m_settings.Save(m_options.userDir, &error)) {
        SUPERSONIC_LOG_WARN("Penumbra") << "settings not saved: " << error << std::endl;
    }
}

void PenumbraLayer::RefreshVideoModes(entt::registry& registry) {
    Supersonic::WindowControl* window = WindowControlOf(registry);
    if (window == nullptr) return;
    // E2: each size once. 0.7.12 listed every refresh rate of a size again, as
    // identical "WxHx32" lines (videoModes.as:99-101); the port picks the rate
    // itself - the desktop's where the monitor offers the size at it, else the
    // highest (WindowControl::ChooseFullscreenMode) - so a rate is nothing to pick.
    std::vector<Eth::videoMode> modes;
    const auto listed = [&modes](uint32_t width, uint32_t height) {
        return std::any_of(modes.begin(), modes.end(),
                           [&](const Eth::videoMode& m) { return m.width == width && m.height == height; });
    };
    for (const Supersonic::DisplayMode& mode : window->DisplayModes()) {
        if (!listed(mode.width, mode.height)) modes.push_back(Eth::videoMode{mode.width, mode.height, Eth::PF32BIT});
    }
    // The desktop's own size is the line that switches nothing, and the way
    // back from another mode; a platform can leave it out of its list. In the
    // list's own order: by area, then width (WindowControl::SelectDisplayModes).
    const Supersonic::DisplayMode desktop = window->DesktopMode();
    if (desktop.width > 0 && desktop.height > 0 && !listed(desktop.width, desktop.height)) {
        const Eth::videoMode native{desktop.width, desktop.height, Eth::PF32BIT};
        const auto before = [](const Eth::videoMode& a, const Eth::videoMode& b) {
            const uint64_t areaA = uint64_t{a.width} * a.height;
            const uint64_t areaB = uint64_t{b.width} * b.height;
            return areaA != areaB ? areaA < areaB : a.width < b.width;
        };
        modes.insert(std::upper_bound(modes.begin(), modes.end(), native, before), native);
    }
    m_machine->SetVideoModes(std::move(modes));
}

glm::uvec2 PenumbraLayer::SavedFullscreenMode() const {
    return glm::uvec2(static_cast<unsigned>(std::max(m_settings.fullscreenWidth, 0)),
                      static_cast<unsigned>(std::max(m_settings.fullscreenHeight, 0)));
}

// SetWindowProperties as the scripts use it: the windowed flag flipped
// (Alt+Enter or the options screen's switch, menu.as:108-119), or a line of the
// mode list with the flag as it was (videoModes.as:110). render/WindowMode.hpp
// decides which.
void PenumbraLayer::ApplyWindowRequest(entt::registry& registry) {
    Eth::WindowRequest& request = m_machine->Window();
    if (!request.changed) return;
    request.changed = false;
    Supersonic::WindowControl* window = WindowControlOf(registry);

    using Kind = Render::WindowAction::Kind;
    const Render::WindowAction action = Render::DecideWindowAction(
        request.windowed, glm::uvec2(request.width, request.height), m_windowFullscreen, SavedFullscreenMode());

    if (action.kind == Kind::EnterFullscreen || action.kind == Kind::LeaveFullscreen) {
        // The window keeps its windowed size to come back to.
        const bool fullscreen = action.kind == Kind::EnterFullscreen;
        m_windowFullscreen = fullscreen;
        if (window != nullptr) {
            if (fullscreen) {
                RequestFullscreen(*window, action.size);
            } else {
                window->SetFullscreen(false);
            }
        }
        if (fullscreen != m_settings.fullscreen) {
            m_settings.fullscreen = fullscreen;
            SaveSettings();
        }
        return;
    }
    if (window == nullptr) return;

    if (action.kind == Kind::SwitchFullscreenMode) {
        // Step 23: 0.7.12 switched the display to the mode picked, and so does
        // the port now (it used to set only the size to come back at, so a
        // pick in fullscreen showed nothing). The windowed size is left alone:
        // the list sizes a window only when picked in one.
        if (!window->SetFullscreenMode(action.size.x, action.size.y)) return;
        const Supersonic::DisplayMode desktop = window->DesktopMode();
        const glm::uvec2 saved = Render::FullscreenModeToSave(action.size, glm::uvec2(desktop.width, desktop.height));
        if (saved != SavedFullscreenMode()) {
            m_settings.fullscreenWidth = static_cast<int>(saved.x);
            m_settings.fullscreenHeight = static_cast<int>(saved.y);
            SaveSettings();
        }
        return;
    }

    // Picked in a window: the window takes that size and renders at it.
    if (!window->SetWindowedSize(action.size.x, action.size.y)) return;
    const int width = static_cast<int>(action.size.x);
    const int height = static_cast<int>(action.size.y);
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
void PenumbraLayer::StartDevScene(const std::string& scene) {
    const bool pvp = scene.rfind("pvp_", 0) == 0;
    Script::resetData();
    Eth::UsePixelShaders(true);
    // The screens that are not levels start the way the scripts start them.
    if (scene == "arena_select.esc") {
        Script::goToPvp();   // setupScene.as:242: Versus
    } else if (scene == "gameover.esc") {
        Eth::LoadScene("scenes/gameover.esc", "gameOverPreLoop", "gameOverLoop");   // controlCharacters.as:449
    } else if (scene == "videoModes.esc") {
        Eth::LoadScene("scenes/videoModes.esc", "screenModesPreLoop", "screenModesLoop");   // menu.as
    } else {
        Eth::LoadScene("scenes/" + scene, "setupScene", pvp ? "pvpLoop" : "levelLoop");
    }
    SUPERSONIC_LOG_INFO("Penumbra") << "dev start: scenes/" << scene << std::endl;
}

void PenumbraLayer::OnFixedUpdate(entt::registry& registry, float fixedDelta) {
    (void)fixedDelta;
    Eth::Machine::Scope scope(*m_machine);

    // Which pad player 2 reads follows the live g_controls switch.
    m_input.SetPlayer2Pad(static_cast<int>(Script::getPlayerJoystick(1)));
    // E14: in the screens laid out as menus a pad's A and B also confirm and
    // cancel (the scripts read only Start and Back there).
    m_input.SetMenuMode(IsFixedLayoutScene(m_machine->GetSceneFileName()));
    Eth::InputFrame frame = m_input.BuildTick(m_view);
    ApplyDevHolds(frame);
    if (m_options.devCursor) frame.cursor = frame.cursorAbsolute = *m_options.devCursor;
    ++m_ticksThisFrame;
    // E16: the fingers press player 1's keys, or click in a menu, before
    // anything reads the frame - the pause included, which a finger opens
    // and whose rows a finger taps.
    if (m_touchEnabled) ApplyTouch(frame);

    // E13: the pause reads the tick first. While it is open the Machine does
    // not run, so GetTime(), and every fade, cooldown and the run's clock with
    // it, stands still.
    const Render::PauseInput pauseInput = Render::PauseMenu::InputFrom(
        frame, static_cast<int>(Script::getPlayerJoystick(0)), InPlayScene(), m_machine->GetScreenSize());
    const int selectedBefore = m_pause.Selected();
    const Render::PauseStep pause = m_pause.Update(pauseInput);
    if (pause.opened || pause.closed) ApplyVolumes();
    // What moved the pause, in the log: a pause is driven by keys, pads, the
    // pointer and the window's focus at once, and only a live run shows which.
    if (pause.opened || pause.closed || (m_pause.Paused() && m_pause.Selected() != selectedBefore)) {
        SUPERSONIC_LOG_INFO("Penumbra")
            << "pause " << (pause.opened ? "opened" : pause.closed ? "closed" : "selection") << " -> item "
            << m_pause.Selected() << (pause.sendCancel ? " (main menu)" : "") << " | tick " << m_ticks
            << " focused " << pauseInput.focused << " open " << pauseInput.open << " back " << pauseInput.back
            << " up " << pauseInput.up << " down " << pauseInput.down << " confirm " << pauseInput.confirm
            << " click " << pauseInput.click << " pointer " << pauseInput.pointer.x << "," << pauseInput.pointer.y
            << std::endl;
    }
    if (pause.tick) {
        // What was pressed in the pause stays out of the game until released.
        m_pause.FilterForGame(frame);
        // Main menu: the original's own cancel, for exactly this tick, which
        // doLoop's escToGoToMenu turns into the menu (menu.as:393).
        if (pause.sendCancel) frame.keys[Eth::K_ESC] = true;
        // E16: what the game is about to read, for the combo buttons, which
        // must know when its combo buffer is empty - touch on or off, since a
        // combo tapped later counts from presses made before.
        m_touch.ObserveFrame(frame, static_cast<int>(Script::getPlayerJoystick(0)));
        // E8: where the outgoing tick drew everything, before Frame rebuilds the
        // snapshot in place - the pose the frames until the next tick blend from.
        m_interp.BeginTick(m_machine->Snapshot());
        m_machine->Frame(frame);   // steps the key and button state machines itself
    }
    ++m_ticks;   // engine ticks, paused or not: --hold spans stay where they were put

    if (!m_options.startScene.empty() && !m_devStarted) {
        m_devStarted = true;
        StartDevScene(m_options.startScene);
    }
    if (!m_options.tour.empty() && m_tourIndex < m_options.tour.size() && m_options.tourTicks > 0) {
        if (m_tourSince == 0) StartDevScene(m_options.tour[m_tourIndex]);
        if (++m_tourSince >= m_options.tourTicks) {
            m_tourSince = 0;
            ++m_tourIndex;
        }
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
    const bool keyboardPlayer2 = Script::g_keyboardP2.getCurrent() == 0;   // E10
    if (layout != m_settings.controls.joystickLayout || pixelShaders != m_settings.pixelShaders ||
        keyboardPlayer2 != m_settings.controls.keyboardPlayer2) {
        m_settings.controls.joystickLayout = layout;
        m_settings.pixelShaders = pixelShaders;
        m_settings.controls.keyboardPlayer2 = keyboardPlayer2;   // E10
        m_input.SetControls(m_settings.controls);
        SaveSettings();
    }

    // E10: the enhanced rows. A pick on the screen replaces this run's --lang,
    // --widescreen or --smooth flag (or --fixed-step's pause-off), which would
    // otherwise go on overriding it.
    // Volumes are compared as steps, so a hand-edited 0.75 is left as it is
    // until the player moves it. The language, the volumes and smooth motion
    // apply at once (SaveSettings); the view at the next scene load
    // (LogicalScreenFor).
    const bool portuguese = Script::g_language.getCurrent() == 0;
    const bool widescreen = Script::g_widescreen.getCurrent() == 0;
    const bool smoothMotion = Script::g_smoothMotion.getCurrent() == 0;
    const bool pauseOnFocusLoss = Script::g_pauseOnFocusLoss.getCurrent() == 0;
    const bool musicMoved = Script::g_musicVolume.getCurrent() != Script::g_musicVolume.stepFor(m_settings.musicVolume);
    const bool effectsMoved =
        Script::g_effectsVolume.getCurrent() != Script::g_effectsVolume.stepFor(m_settings.effectsVolume);
    if (portuguese != Portuguese() || widescreen != Widescreen() || smoothMotion != SmoothMotion() ||
        pauseOnFocusLoss != PauseOnFocusLoss() || musicMoved || effectsMoved) {
        if (portuguese != Portuguese()) {
            m_options.languageOverride.reset();
            m_settings.language = portuguese ? "pt" : "en";
        }
        if (widescreen != Widescreen()) {
            m_options.widescreenOverride.reset();
            m_settings.widescreen = widescreen;
        }
        if (smoothMotion != SmoothMotion()) {
            m_options.smoothMotionOverride.reset();
            m_settings.smoothMotion = smoothMotion;
        }
        if (pauseOnFocusLoss != PauseOnFocusLoss()) {
            m_options.pauseOnFocusLossOverride.reset();
            m_settings.pauseOnFocusLoss = pauseOnFocusLoss;
        }
        if (musicMoved) m_settings.musicVolume = Script::g_musicVolume.getFraction();
        if (effectsMoved) m_settings.effectsVolume = Script::g_effectsVolume.getFraction();
        ApplyVolumes();
        SaveSettings();   // ApplyLanguage() first
    }

    // E20: the touch controls' row (drawn on a phone's options screen only).
    // A pick replaces --touch, is saved as "on" or "off" - no longer "auto" -
    // and applies from the next tick.
    const bool touchControls = Script::g_touchControls.getCurrent() == 0;
    if (touchControls != m_touchEnabled) {
        m_options.touchOverride.reset();
        m_settings.touchControls = touchControls ? "on" : "off";
        SetTouchEnabled(touchControls);
        SaveSettings();
    }

    if (m_machine->QuitRequested()) Supersonic::Application::RequestQuit();
}

void PenumbraLayer::OnUpdate(entt::registry& registry, float deltaTime) {
    (void)deltaTime;
    const Eth::RenderSnapshot& snapshot = m_machine->Snapshot();
    const bool paused = m_pause.Paused();   // E13

    Supersonic::WindowControl* window = WindowControlOf(registry);
    if (window != nullptr) {
        // The next scene is as wide as the window is by then (E1), after a
        // fullscreen switch or a picked mode. Zero while minimised: keep the last.
        const glm::uvec2 size = window->WindowSize();
        if (size.x > 0 && size.y > 0) m_options.windowPixels = size;
    }

    // E8: the world between the last two ticks, alpha of the way (one tick
    // behind, render/Interpolation.hpp); the snapshot itself when smoothing
    // is off or the two ticks may not be blended. The order is the tick's
    // (the blend is index for index the same lists), the strips' shapes are
    // the tick's (a mesh rebuild waits on the GPU: once a tick, not a frame),
    // and the HUD and the pointer are the tick's own.
    // While paused (E13) no tick moves the blend on: the last tick as it is,
    // or a live alpha would rock the world between the last two.
    const auto* clock = registry.ctx().find<Supersonic::SimulationClock>();
    const float alpha = paused || clock == nullptr ? 1.0f : clock->alpha;
    const Eth::RenderSnapshot& world = m_interp.Frame(snapshot, alpha);
    m_view = m_rig.Update(registry, world, m_pillarbox);
    if (window != nullptr) {
        // 0.7.12 hid the system pointer while the scripts asked (main.as:133)
        // and drew cursor.ent in its place. Every frame is cheap: it does
        // nothing when the request already stands, and the focus and editor
        // vetoes stay Input's.
        // E13: the pointer shows over the pause's menu, which it can click.
        // Step 23: and over the bars of a pillarboxed or letterboxed frame,
        // where cursor.ent is drawn under a bar (the scripts' cursor follows
        // the mouse there, unclamped) and the player had no pointer at all.
        // Measured against this frame's view, after the rig has placed it.
        const bool overBars = Render::InputMapper::PointerOverBars(Supersonic::Input::MousePosition(), m_view);
        window->SetCursorVisible(!snapshot.cursorHidden || paused || overBars);
    }
    // Where the gloss maps' highlights are seen from: 0.7.12's fake eye
    // (ETHShaderManager::SetFakeEyePosition), each light mirrored across the
    // line 3/4 of the way down the screen, at height 768 (m_fakeEyeHeight's
    // default; the scripts never change it). It moves with the camera the
    // frame is drawn with - the blend's, under E8.
    registry.ctx().insert_or_assign(
        Supersonic::Light2DEye{-(m_view.camera.y + 0.75f * m_view.logicalScreen.y), kFakeEyeHeight});
    const Render::DrawOrder order = Render::ComputeDrawOrder(snapshot);
    m_sprites.Draw(registry, world, m_view, order);
    m_shadows.Draw(registry, world, m_view, order, &snapshot);
    m_lights.Draw(registry, world, m_view, order, &m_shadows.BakedStrips());
    m_particles.Draw(registry, world, m_view, order);
    // E13: the pause's overlay over the scripts' HUD, under the bars. Once a
    // frame: it begins the font atlas's frame.
    // E16: the touch controls under it, over the scripts' HUD. One Draw a
    // frame, both in it.
    m_overlay.clear();
    if (m_touchEnabled) m_touch.AppendOverlay(m_overlay);
    m_pause.AppendOverlay(m_overlay);
    // E16: the scripts' control hints in touch wording while the touch
    // controls are on (strings.json "touch"), from this frame on.
    m_localization.SetTouch(m_touchEnabled);
    m_hud.Draw(registry, snapshot, m_view, m_overlay.empty() ? nullptr : &m_overlay);
    m_input.EndFrame();                       // once a frame, tick or not
    // E16: a finger that came down in a frame no tick saw is handed to the
    // next tick, as EndFrame hands on a key.
    if (m_touchEnabled && m_ticksThisFrame == 0) m_touch.LatchFrame(TouchContacts());
    m_ticksThisFrame = 0;
}

} // namespace Penumbra
