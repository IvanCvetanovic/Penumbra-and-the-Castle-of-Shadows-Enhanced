// Script.hpp first: an engine header that reaches <windows.h> would turn the
// script API's DrawText into DrawTextA and its min/max into macros.
#include "script/Script.hpp"

#include "PenumbraLayer.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <utility>

#include "core/Application.hpp"
#include "core/Input.hpp"
#include "core/Light2D.hpp"
#include "core/Log.hpp"
#include "core/ScreenOverlay.hpp"   // E35
#include "core/SimulationClock.hpp"
#include "core/WindowControl.hpp"
#include "eth/Machine.hpp"
#include "eth/Paths.hpp"
#include "platform/SafeArea.hpp"
#include "render/DrawOrder.hpp"
#include "render/Splash.hpp"   // E35
#include "render/WideMenus.hpp"
#include "render/WindowMode.hpp"

namespace Penumbra {

namespace {
// E23: on a phone only Android sets the display's refresh rate (its activity's
// display mode and ANativeWindow_setFrameRate); the iOS backend drops the
// request, so there the rate row is left out rather than offer nothing.
#if defined(__ANDROID__)
constexpr bool kPhoneRefreshRate = true;
#else
constexpr bool kPhoneRefreshRate = false;
#endif
} // namespace

namespace {

// The scenes laid out for the original's 1024x768: their buttons, panels and
// thumbnails sit at fixed pixels (menu.as, videoModes.as, gameover.as), so
// they keep that screen, centred; in a wide window the world goes on past its
// sides instead of bars (E1, render/WideMenus.hpp).
using Render::IsFixedLayoutScene;

// ETHShaderManager's m_fakeEyeHeight (ETHShaderManager.cpp:55).
constexpr float kFakeEyeHeight = 768.0f;

// E23: what a fullscreen choice comes to, in one line of the log - for the
// player's log file and the live checks, which cannot see the display.
// `size` and `rate` are what was asked for: the settings', or --refresh's.
std::string DescribeFullscreenChoice(const Render::FullscreenChoice& choice, const std::vector<uint32_t>& rates,
                                     glm::uvec2 size, uint32_t rate) {
    std::ostringstream out;
    out << choice.size.x << "x" << choice.size.y << " (";
    if (choice.sizeFellBack) {
        out << size.x << "x" << size.y << " is not offered: the desktop's";
    } else {
        out << (choice.sizeAutomatic ? "automatic: the desktop's" : "picked");
    }
    out << ") @ " << choice.mode.refreshRate << " Hz (";
    if (choice.rateFellBack) {
        out << rate << " Hz is not offered at this size: the highest";
    } else {
        out << (choice.rateAutomatic ? "automatic: the highest" : "picked");
    }
    out << "); rates at this size:";
    if (rates.empty()) out << " none known";
    for (std::size_t i = 0; i < rates.size(); ++i) out << (i == 0 ? " " : ", ") << rates[i];
    return out.str();
}

} // namespace

PenumbraLayer::PenumbraLayer(Options options)
    : m_options(std::move(options)),
      m_settings(m_options.settings),
      m_textures(m_options.originalDir.generic_string()) {}

PenumbraLayer::~PenumbraLayer() = default;

Eth::vector2 PenumbraLayer::LogicalScreenFor(const std::string& sceneFile) const {
    constexpr float kHeight = 768.0f;
    // E25: a campaign level, zoomed while the touch controls are on - E1's
    // screen (or the 4:3 one) that many times smaller, fixed per scene as E1's is.
    if (Render::IsCampaignScene(sceneFile)) {
        const float zoom = CampaignZoom();
        if (zoom > 1.0f) return Render::ZoomedScreen(m_options.windowPixels, Widescreen(), zoom);
    }
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
    // asked for, and an automatic window is the one the engine fitted to the
    // monitor (E23): the first scene's widescreen width follows what is shown.
    if (Supersonic::WindowControl* window = WindowControlOf(registry); window != nullptr) {
        const glm::uvec2 size = window->WindowSize();
        if (size.x > 0 && size.y > 0) m_options.windowPixels = size;
        if constexpr (Render::kMobileBuild) {
            // E23: a phone's display at the rate the settings ask for - the
            // highest by default - from the first frame. Android only: the
            // iOS backend sets no rate (and the row is left out there).
            if constexpr (kPhoneRefreshRate) window->SetPreferredRefreshRate(PreferredRefreshRate());
        } else if (window->IsFullscreen() && m_options.startFullscreen) {
            // The engine went fullscreen at the desktop's mode before any
            // layer could name one. The saved mode (Step 23), or E23's
            // automatic one - the desktop's size at the highest rate there -
            // switches to it at the top of the first frame (nothing at all
            // when the desktop runs that already), so the first scene is as
            // wide as that mode is.
            const Render::FullscreenChoice choice = FullscreenChoiceFor(*window);
            if (m_options.splash) {
                // E38: not before the first frame. The switch blanks the display and the engine's thread is held in
                // the driver's call until it is done - seconds, on some monitors and TVs - so asked for here it ran
                // at the top of the first frame, before a single frame had been drawn, and a player saw a black
                // screen that did not answer. After the first frame the window is on screen and read (FinishLaunch).
                // Only with the intro: a start with a development flag (no intro) asks at once, as before.
                m_launchFullscreenPending = true;
            } else {
                RequestFullscreen(*window, "launch");
            }
            if (choice.size.x > 0 && choice.size.y > 0) m_options.windowPixels = choice.size;
        }
    }

    Eth::MachineConfig config;
    config.gameRoot = m_options.originalDir.generic_string();
    config.userRoot = m_options.userDir.string();
    config.screenSize = LogicalScreenFor("scenes/menu.esc");
    config.screenSizeForScene = [this](const std::string& scene) { return LogicalScreenFor(scene); };
    // E1 for the menus: as the levels' width, decided at each load.
    config.widenScene = [this](const std::string& scene, const Eth::SceneFile& file) {
        return Widescreen() ? Render::WidenScene(scene, file) : Eth::SceneWidening{};
    };
    m_machine = std::make_unique<Eth::Machine>(config);

    m_audio.Attach(registry);
    // Music is decoded whole; keeping it across LoadScene spares a hitch on
    // every death and level change (the scripts reload the same few files).
    m_audio.SetKeepDecodedClips(true);
    m_machine->Samples().SetOutput(&m_audio);
    ApplyVolumes();
    m_pause.SetAutoPause(PauseOnFocusLoss());   // E13
    // E28: a Script global outlives a Machine, so the editor's button starts unraised; and the editor finds its art.
    Script::g_adjustTouchControls = false;   // E28
    m_editor.SetImageRoot(m_options.dataDir);   // E28
    // E16: the touch controls - on a phone by default, on the desktop with
    // --touch (or touchControls "on"), where the mouse is the finger.
    SetTouchEnabled(
        m_options.touchOverride.value_or(Render::TouchControls::EnabledBySetting(m_settings.touchControls)));
    // E20: a phone's options screen (Script.hpp). From the build, not from the
    // controls: a desktop run with --touch still has a window to switch.
    Script::g_mobileLayout = m_options.mobileLayoutOverride.value_or(Render::kMobileBuild);
    Script::g_refreshRateRow = !Render::kMobileBuild || kPhoneRefreshRate;   // E23: not on iOS

    // The original's option switches, remembered across launches (E6).
    Script::g_controls.setCurrent(static_cast<Eth::uint>(m_settings.controls.joystickLayout));
    Script::g_enablePS.setCurrent(m_settings.pixelShaders ? 0u : 1u);
    // Row 1 is "Tela-cheia". From what the window opens as, not the settings: a
    // --windowed or --fullscreen run would otherwise be switched back by the
    // menu's first frame (menu.as:103-106).
    Script::g_windowed.setCurrent(m_options.startFullscreen ? 1u : 0u);
    // E10: the enhanced settings' own rows on the options screen. Language and
    // view from what this run shows (a --lang or --widescreen flag included),
    // as g_windowed is, so the screen's first frame changes nothing. E24: the
    // languages in the picker's order, each drawn as its own name. E27: and
    // before them automatic, the script's own word as the refresh rate's row
    // has it (strings.json translates it): index 0, then kLanguages[i] at i + 1.
    // A --lang run shows its language, so it seeds that language's own entry.
    // E27: the folder the options screen's and the menu's added art is read from.
    Script::g_artDir = m_options.dataDir.generic_string();
    Eth::array<Eth::string> languageNames;
    languageNames.insertLast(Eth::string("Autom\xE1tica"));
    for (const Render::LanguageInfo& info : Render::kLanguages) {
        languageNames.insertLast(Render::Localization::LanguageNameKey(info.language));
    }
    const bool languageShownAuto = m_settings.languageAuto && !m_options.languageOverride.has_value();
    Script::g_language.setOptions(languageNames,
                                  languageShownAuto
                                      ? 0u
                                      : static_cast<Eth::uint>(Render::LanguageIndex(CurrentLanguage()) + 1));
    m_languageChoiceSeeded = Script::g_language.getCurrent();
    Script::g_widescreen.setCurrent(Widescreen() ? 0u : 1u);
    Script::g_keyboardP2.setCurrent(m_settings.controls.keyboardPlayer2 ? 0u : 1u);
    Script::g_comboAssist.setCurrent(m_settings.controls.comboAssist ? 0u : 1u);   // E46
    Script::g_musicVolume.setCurrent(Script::g_musicVolume.stepFor(m_settings.musicVolume));
    Script::g_effectsVolume.setCurrent(Script::g_effectsVolume.stepFor(m_settings.effectsVolume));
    Script::g_smoothMotion.setCurrent(SmoothMotion() ? 0u : 1u);   // E8's row: as this run draws (--smooth, --fixed-step)
    Script::g_pauseOnFocusLoss.setCurrent(PauseOnFocusLoss() ? 0u : 1u);   // E13's, likewise
    Script::g_difficulty.setCurrent(HardDifficulty() ? Script::DIFFICULTY_HARD : Script::DIFFICULTY_NORMAL);   // E36's: the New Game prompt opens on what this run has (the setting, or --difficulty); row 0 is Normal
    Script::g_touchControls.setCurrent(m_touchEnabled ? 0u : 1u);   // E20's: as this run has them (--touch)
    RefreshZoomChoices();   // E25's: as this run zooms (--zoom)
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
    if (m_options.splash) StartSplash();   // E35

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
    const glm::uvec2 image = Render::CameraRig::WindowPixels(registry, glm::vec2(m_options.windowPixels));
    // E25: the menu's first taps map through the frame it is drawn in.
    const Render::MenuFrame menuFrame = MenuFrameFor("scenes/menu.esc", image);
    m_view = Render::CameraRig::ComputeView(seed, image, m_pillarbox, &menuFrame);
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

int PenumbraLayer::ZoomSetting() const {
    return Render::ClampZoomSetting(m_options.zoomOverride.value_or(m_settings.zoom));
}

float PenumbraLayer::CampaignZoom() const {
    // E26: as far as the frame leaves the messages their room.
    return Render::CampaignZoom(ZoomSetting(), m_touchEnabled, m_options.windowPixels, Widescreen(), SafeInsets(),
                                EdgeMargin());
}

Supersonic::SafeAreaInsets PenumbraLayer::SafeInsets() const {
    return m_options.safeAreaOverride.value_or(Supersonic::SafeArea::Get());
}

float PenumbraLayer::EdgeMargin() const {
    const float asked = Render::EdgeMarginPercent(m_options.edgeMarginOverride.value_or(m_settings.edgeMargin),
                                                  m_touchEnabled, m_options.windowPixels);
    return Render::FittedEdgeMargin(asked, m_options.windowPixels, Widescreen(), SafeInsets());
}

Render::HudFrame PenumbraLayer::CurrentHudFrame(const bool withMargin) const {
    // The loops the HUD is drawn under, their end screens included (the
    // bars stay where they were); the menus have none.
    const std::string& loop = m_machine->LoopFunction();
    if (!m_touchEnabled || (loop != "levelLoop" && loop != "pvpLoop")) return Render::HudFrame{};
    return Render::ComputeHudFrame(m_view.windowPixels, m_machine->GetScreenSize(), SafeInsets(),
                                   withMargin ? EdgeMargin() : 0.0f);
}

Render::MenuFrame PenumbraLayer::MenuFrameFor(const std::string& sceneFile, const glm::uvec2 image) const {
    // The larger menu shows the world past the screen's sides, which only
    // E1's wide menus collect.
    if (!m_touchEnabled || !Widescreen() || !Render::IsPhoneMenuScene(sceneFile)) return Render::MenuFrame{};
    return Render::ComputeMenuFrame(image, SafeInsets());
}

void PenumbraLayer::RefreshZoomChoices() {
    // E25: automatic, then the steps this screen can show (a step past
    // MaxZoom would draw as the one below it); the setting, a hand-edited
    // percentage or one past the limit here, is shown as it is, in its place.
    const int setting = ZoomSetting();
    m_zoomChoicesSafe = SafeInsets();
    const int limit = Render::MaxZoomPercent(m_options.windowPixels, Widescreen(), m_zoomChoicesSafe, EdgeMargin());
    m_zoomChoices.assign(1, Render::kZoomAutomatic);
    for (const int step : Render::kZoomSteps) {
        if (step == Render::kZoomMinPercent || step <= limit) m_zoomChoices.push_back(step);
    }
    if (setting != Render::kZoomAutomatic &&
        std::find(m_zoomChoices.begin(), m_zoomChoices.end(), setting) == m_zoomChoices.end()) {
        m_zoomChoices.insert(std::upper_bound(m_zoomChoices.begin() + 1, m_zoomChoices.end(), setting), setting);
    }
    Eth::array<Eth::string> labels;
    uint32_t current = 0;
    for (std::size_t i = 0; i < m_zoomChoices.size(); ++i) {
        // The script's Portuguese: strings.json has "Autom\xE1tica", and a
        // percentage is the pattern "{int}%", the same in every language.
        labels.insertLast(m_zoomChoices[i] == Render::kZoomAutomatic ? std::string("Autom\xE1tica")
                                                                     : std::to_string(m_zoomChoices[i]) + "%");
        if (m_zoomChoices[i] == setting) current = static_cast<uint32_t>(i);
    }
    Script::g_zoom.setOptions(labels, current);
    m_zoomChoiceSeeded = Script::g_zoom.getCurrent();
}

Render::Language PenumbraLayer::CurrentLanguage() const {
    // main() and Settings let through only the ids Languages.hpp lists.
    Render::Language language = Render::Language::English;
    Render::LanguageFromId(m_options.languageOverride.value_or(m_settings.language), language);
    return language;
}

void PenumbraLayer::ApplyLanguage() { m_localization.SetLanguage(CurrentLanguage()); }

bool PenumbraLayer::SmoothMotion() const {
    return m_options.smoothMotionOverride.value_or(m_settings.smoothMotion);
}

bool PenumbraLayer::PauseOnFocusLoss() const {
    return m_options.pauseOnFocusLossOverride.value_or(m_settings.pauseOnFocusLoss);
}

bool PenumbraLayer::HardDifficulty() const {   // E36
    return m_options.hardDifficultyOverride.value_or(m_settings.HardDifficulty());   // E36
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
    // E28: --finger's synthetic fingers, in logical pixels already (nothing to map). A pure function of the tick count,   // E28
    // so the several calls a tick makes, and OnUpdate's latch, agree.                                                    // E28
    for (const DevFinger& finger : m_options.devFingers) {                                                              // E28
        if (m_ticks < finger.fromTick || m_ticks > finger.toTick) continue;                                              // E28
        const float along = static_cast<float>(m_ticks - finger.fromTick) /                                              // E28
                            static_cast<float>(std::max(1u, finger.toTick - finger.fromTick));                          // E28
        contacts.push_back(Render::TouchContact{finger.id, finger.from + (finger.to - finger.from) * along, true});    // E28
    }                                                                                                                   // E28
    return contacts;
}

Render::TouchInput PenumbraLayer::BuildTouchInput() {   // E28: ApplyTouch's first half, as it was but for the editor's scene at its end
    Render::TouchInput input;
    input.contacts = TouchContacts();
    input.screen = m_machine->GetScreenSize();
    // E25: the controls keep their size on the window - laid out for a screen
    // 768 tall, which a zoomed level's is not, and at E1's scale, which a
    // phone's larger menu is not.
    const Render::MenuFrame menuFrame = MenuFrameFor(m_machine->GetSceneFileName(), m_view.windowPixels);
    input.unit = menuFrame.active ? menuFrame.baseScale / menuFrame.scale
                                  : input.screen.y / Render::kUnzoomedHeight;
    // E25: the down button, while the door offered the way on in this scene.
    input.nextLevelOffered = m_nextLevelOffered && m_nextLevelSerial == m_machine->Snapshot().sceneSerial;
    // E1's wide menus: laid out across what is shown, into the window's corners.
    if (m_view.openSides > 0.0f) {
        input.areaMin = m_view.ShownLogicalMin();
        input.areaMax = m_view.ShownLogicalMax();
    }
    // The window's safe area (a notch, rounded corners, a bar that shows), in
    // window pixels from the platform - zero on the desktop - brought into the
    // logical screen the controls are laid out in.
    input.safeArea = Render::TouchInsets{};
    const Supersonic::SafeAreaInsets safe = SafeInsets();
    if (!safe.IsZero()) {
        input.safeArea = Render::TouchControls::WindowInsetsToLogical(
            Render::TouchInsets{safe.left, safe.top, safe.right, safe.bottom}, m_view);
    }
    // E26: the pause button hangs from the HUD frame's corner, under the timer.
    input.hudFrame = Render::TouchInsets{m_hudFrame.left, m_hudFrame.top, m_hudFrame.right, m_hudFrame.bottom};

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

    // E28: the editor shows every control but Back and presses nothing. The options scene it stands over is laid   // E28
    // out as the menus are (unit 1, the shown rectangle across a wide window), and its HUD frame is the frame of   // E28
    // that rectangle's own shape: the layer's m_hudFrame is zero outside a level, and a wide menu's GetScreenSize()   // E28
    // is 1024 wide, which would measure the margin from the bars.                                                  // E28
    if (m_editor.IsOpen()) {                                                                                       // E28
        input.scene = Render::TouchScene::Edit;                                                                    // E28
        input.corner = Render::TouchCorner::Hidden;                                                                // E28
        const Render::TouchGeometry geometry = Render::TouchGeometry::From(input);                                 // E28
        const Render::HudFrame frame = Render::ComputeHudFrame(m_view.windowPixels, geometry.AreaMax() - geometry.AreaMin(),   // E28
                                                               SafeInsets(), EdgeMargin());                        // E28
        input.hudFrame = Render::TouchInsets{frame.left, frame.top, frame.right, frame.bottom};                    // E28
    }                                                                                                              // E28
    return input;                                                                                                  // E28
}   // E28

void PenumbraLayer::ApplyTouch(Eth::InputFrame& frame) {   // E28: BuildTouchInput is the first half of what was here
    const Render::TouchInput input = BuildTouchInput();   // E28
    const Render::TouchStep step = m_touch.Update(input);
    Render::TouchControls::ApplyToFrame(step, frame);
    // The cursor stays where the finger lifted, as the mouse's would: the
    // mapper keeps it until the scripts or the real mouse move it.
    if (step.pointer) m_input.WarpCursor(step.pointerPos, m_view);
    // E37: a tap on a combo button that is recharging (or while a combo runs): TouchControls spaces the cue.
    if (step.deniedSound) PlayComboDenied(step);   // E37

    // E28: the editor reads the fingers this Update used (TouchControls::Down) and says what changed. A change is   // E28
    // applied at once, so a drag shows this frame; a gesture's end is stored and saved, never every drag tick.      // E28
    if (m_editor.IsOpen()) {                                                                                       // E28
        Render::TouchEditInput edit;                                                                               // E28
        edit.down = &m_touch.Down();                                                                               // E28
        edit.geometry = Render::TouchGeometry::From(input);                                                        // E28
        edit.close = Render::PauseMenu::InputFrom(frame, static_cast<int>(Script::getPlayerJoystick(0)), false,    // E28
                                                  input.screen).back;                                              // E28
        const Render::TouchEditStep result = m_editor.Update(edit, m_touch);                                       // E28
        if (result.changed) m_touch.SetTuning(m_editor.Tuning());                                                  // E28
        if (result.commit) {                                                                                       // E28
            m_settings.touchTuning = m_editor.Tuning();                                                            // E28
            m_options.touchTuningOverride.reset();                                                                 // E28
            SaveSettings();                                                                                        // E28
        }                                                                                                          // E28
        if (result.closed) {                                                                                       // E28
            // The Esc or the click that closed it must not reach the options screen's waitForInputToMenu         // E28
            // (videoModes.cpp), which would drop the player into the main menu.                                  // E28
            m_pause.HoldPressed();                                                                                 // E28
            // The scene is the menu's again: the controls hide now, not one frame over the options screen.       // E28
            m_touch.Update(BuildTouchInput());                                                                     // E28
        }                                                                                                          // E28
    }                                                                                                              // E28
}   // E28

// E37: the cue of a combo button that refused a tap: soundfx/fail.ogg, the original's own "not yet" - its menu plays it for an
// arena that is locked. The menu's preLoop loads it and every LoadScene releases the samples, so a level loads it here
// (PlaySample never loads; the decoded clip outlives the load, so only the first call decodes). It runs 1.29 s and
// PlaySample restarts a sample that is playing, so one that still sounds is left to finish: restarted every few ticks, a
// mashed button would only ever replay the first tenth of a second of it.
void PenumbraLayer::PlayComboDenied(const Render::TouchStep& step) {   // E37
    constexpr const char* kSample = "soundfx/fail.ogg";   // E37
    Eth::SampleBank& samples = m_machine->Samples();   // E37
    if (!samples.LoadSoundEffect(kSample) || samples.IsSamplePlaying(kSample)) return;   // E37
    samples.PlaySample(kSample);   // E37
    const bool sword = step.Denied(Render::TouchCombo::Sword);   // E37
    const bool spell = step.Denied(Render::TouchCombo::Spell);   // E37
    SUPERSONIC_LOG_INFO("Penumbra") << "E37 combo refused a tap (" << (sword && spell ? "both buttons" : sword ? "sword" : "spell")   // E37
                                    << "): the cue plays | tick " << m_ticks << std::endl;   // E37
}   // E37

// E28: opens the touch controls' editor; the Machine stands still from the next tick on, as under the pause.      // E28
void PenumbraLayer::OpenTouchEditor(const bool startUnlocked, const bool closeHeld) {                              // E28
    // The finger that tapped the button is down still: the editor holds it dead until it lifts.                 // E28
    m_editor.Open(TuningNow(), m_touch.Down(), startUnlocked);                                                    // E28
    // The scene is the editor's now: lay the controls out for it, so the first frame drawn already has them.     // E28
    const Render::TouchInput input = BuildTouchInput();                                                           // E28
    m_touch.Update(input);                                                                                        // E28
    // One inert Update (every finger is dead, a held close key is taken as already seen) gives the editor the    // E28
    // real area and notch before the first draw; Open() alone leaves it the 4:3 default for that frame.          // E28
    Render::TouchEditInput edit;                                                                                  // E28
    edit.down = &m_touch.Down();                                                                                  // E28
    edit.geometry = Render::TouchGeometry::From(input);                                                           // E28
    edit.close = closeHeld;                                                                                       // E28
    m_editor.Update(edit, m_touch);                                                                               // E28
    SUPERSONIC_LOG_INFO("Penumbra") << "touch controls editor opened (" << (startUnlocked ? "unlocked" : "locked")   // E28
                                    << ") | tick " << m_ticks << std::endl;                                       // E28
}

void PenumbraLayer::ApplyVolumes() {
    m_machine->Samples().SetMasterVolumes(m_settings.musicVolume * m_pause.MusicScale(), m_settings.effectsVolume);
}

void PenumbraLayer::SetTouchEnabled(bool enabled) {
    if (enabled && !m_touchManifestLoaded) {
        std::string warning;
        const std::filesystem::path manifest = m_options.dataDir / Render::TouchControls::kManifestFile;
        m_touch.SetManifest(Render::TouchControls::LoadManifest(manifest, &warning));
        m_touch.SetTuning(TuningNow());   // E28: the player's own size, opacity and places over the file's
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
    // E22: on, they are player 1 and the first pad is player 2's; off, the
    // pads go back to E12's order from the next tick.
    m_input.SetTouchPlaysPlayer1(enabled);
}

void PenumbraLayer::SaveSettings() {
    ApplyLanguage();
    m_interp.SetEnabled(SmoothMotion());
    m_pause.SetAutoPause(PauseOnFocusLoss());
    // E28: a dev run (--touch-tuning, --touch-editor, --finger) runs against the player's real user directory, and a   // E28
    // synthetic drag must never be able to rewrite their settings.json.                                              // E28
    if (m_options.noSave) {                                                                                           // E28
        if (!m_noSaveLogged) {                                                                                        // E28
            m_noSaveLogged = true;                                                                                    // E28
            SUPERSONIC_LOG_INFO("Penumbra") << "dev run: settings not saved" << std::endl;                            // E28
        }                                                                                                             // E28
        return;                                                                                                       // E28
    }                                                                                                                 // E28
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
    for (const Supersonic::DisplayMode& mode : DisplayModesOf(*window)) {
        if (!listed(mode.width, mode.height)) modes.push_back(Eth::videoMode{mode.width, mode.height, Eth::PF32BIT});
    }
    // The desktop's own size is the line that switches nothing, and the way
    // back from another mode; a platform can leave it out of its list. In the
    // list's own order: by area, then width (WindowControl::SelectDisplayModes).
    const Supersonic::DisplayMode desktop = DesktopModeOf(*window);
    // E23: the monitor's native size, as the list marks it.
    Script::g_nativeVideoMode = Eth::videoMode{desktop.width, desktop.height, Eth::PF32BIT};
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
    RefreshRateChoices(registry);   // E23: the rates of the size fullscreen would take on this monitor
}

glm::uvec2 PenumbraLayer::SavedFullscreenMode() const {
    return glm::uvec2(static_cast<unsigned>(std::max(m_settings.fullscreenWidth, 0)),
                      static_cast<unsigned>(std::max(m_settings.fullscreenHeight, 0)));
}

glm::uvec2 PenumbraLayer::SavedWindowedSize() const {
    if (m_settings.windowWidth <= 0 || m_settings.windowHeight <= 0) return glm::uvec2(0u);
    return glm::uvec2(static_cast<unsigned>(m_settings.windowWidth), static_cast<unsigned>(m_settings.windowHeight));
}

uint32_t PenumbraLayer::FullscreenRefresh() const {
    return m_options.refreshOverride.value_or(static_cast<uint32_t>(std::max(m_settings.fullscreenRefresh, 0)));
}

uint32_t PenumbraLayer::PreferredRefreshRate() const {
    const uint32_t rate = FullscreenRefresh();
    return rate == 0 ? Supersonic::WindowControl::kHighestRefreshRate : rate;
}

std::vector<Supersonic::DisplayMode> PenumbraLayer::DisplayModesOf(const Supersonic::WindowControl& window) const {
    return m_options.devModes.empty() ? window.DisplayModes() : m_options.devModes;
}

Supersonic::DisplayMode PenumbraLayer::DesktopModeOf(const Supersonic::WindowControl& window) const {
    return m_options.devModes.empty() ? window.DesktopMode() : m_options.devDesktop;
}

Render::FullscreenChoice PenumbraLayer::FullscreenChoiceFor(const Supersonic::WindowControl& window) const {
    return Render::ChooseFullscreen(DisplayModesOf(window), DesktopModeOf(window), SavedFullscreenMode(),
                                    FullscreenRefresh());
}

// Fullscreen at the player's mode, or E23's automatic one; the desktop's size
// where this monitor does not offer the saved one, and the highest rate where
// it does not offer the saved rate there (the settings kept either way, for
// the monitor that does). SetFullscreen(true), the desktop's mode as it is,
// when no monitor reports a mode or the engine refuses the request.
void PenumbraLayer::RequestFullscreen(Supersonic::WindowControl& window, const char* why) {
    const Render::FullscreenChoice choice = FullscreenChoiceFor(window);
    const std::vector<uint32_t> rates = Supersonic::WindowControl::RefreshRatesAt(
        DisplayModesOf(window), DesktopModeOf(window), choice.size.x, choice.size.y);
    SUPERSONIC_LOG_INFO("Penumbra") << "E23 fullscreen (" << why << "): "
                                    << DescribeFullscreenChoice(choice, rates, SavedFullscreenMode(), FullscreenRefresh())
                                    << std::endl;
    if (choice.size.x > 0 && choice.size.y > 0 && window.SetFullscreenMode(choice.size.x, choice.size.y, choice.rate)) {
        return;
    }
    window.SetFullscreen(true);
}

void PenumbraLayer::RefreshRateChoices(entt::registry& registry) {
    // E23: the row's options, labelled in the script's Portuguese (strings.json
    // has the English patterns). The layer keeps the rates; the script only
    // moves the index.
    Eth::array<Eth::string> labels;
    uint32_t current = 0;
    const std::vector<uint32_t> previous = m_rateChoices;
    if constexpr (Render::kMobileBuild) {
        // A phone: the display's highest, or 60 Hz to save the battery (and a
        // hand-edited rate, shown as it is).
        m_rateChoices = {0u, 60u};
        const uint32_t saved = FullscreenRefresh();
        if (saved != 0 && saved != 60) m_rateChoices.push_back(saved);
        for (std::size_t i = 0; i < m_rateChoices.size(); ++i) {
            if (m_rateChoices[i] == saved) current = static_cast<uint32_t>(i);
        }
        labels.insertLast("Autom\xE1tica (m\xE1xima)");
        for (std::size_t i = 1; i < m_rateChoices.size(); ++i) labels.insertLast(std::to_string(m_rateChoices[i]) + " Hz");
    } else {
        Supersonic::WindowControl* window = WindowControlOf(registry);
        const std::vector<Supersonic::DisplayMode> modes = window != nullptr ? DisplayModesOf(*window)
                                                                             : m_options.devModes;
        const Supersonic::DisplayMode desktop = window != nullptr ? DesktopModeOf(*window) : m_options.devDesktop;
        const Render::FullscreenChoice choice =
            Render::ChooseFullscreen(modes, desktop, SavedFullscreenMode(), FullscreenRefresh());
        const Render::RateChoices choices = Render::ChooseRates(modes, desktop, choice, FullscreenRefresh());
        m_rateChoices = choices.rates;
        current = choices.current;
        labels.insertLast(choices.automaticRate > 0
                              ? "Autom\xE1tica (" + std::to_string(choices.automaticRate) + " Hz)"
                              : std::string("Autom\xE1tica"));
        for (std::size_t i = 1; i < m_rateChoices.size(); ++i) labels.insertLast(std::to_string(m_rateChoices[i]) + " Hz");
    }
    // Logged when the offer changes (a new monitor, another fullscreen size),
    // not at every scene: what the row offers is otherwise invisible to a
    // live check that cannot open the options screen.
    if (m_rateChoices != previous) {
        std::ostringstream offered;
        for (std::size_t i = 0; i < m_rateChoices.size(); ++i) {
            offered << (i == 0 ? "" : ", ") << (m_rateChoices[i] == 0 ? std::string("automatic")
                                                                      : std::to_string(m_rateChoices[i]) + " Hz");
        }
        SUPERSONIC_LOG_INFO("Penumbra") << "E23 refresh-rate row: " << offered.str() << "; current "
                                        << (current < m_rateChoices.size() && m_rateChoices[current] != 0
                                                ? std::to_string(m_rateChoices[current]) + " Hz"
                                                : std::string("automatic"))
                                        << std::endl;
    }
    Script::g_refreshRate.setOptions(labels, current);
    m_rateChoiceSeeded = Script::g_refreshRate.getCurrent();
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
                RequestFullscreen(*window, "entered");
            } else {
                // E23: an automatic window is fitted to the monitor again on
                // the way out, wherever that monitor's work area is now (the
                // engine applies the size before it leaves fullscreen).
                if (SavedWindowedSize() == glm::uvec2(0u)) window->FitWindowToMonitor(Render::kAutoWindowFraction);
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
        // E23: 0 x 0 is the automatic line. A size is saved as the size picked,
        // the native one included (Step 23 saved that as 0 x 0; the automatic
        // line is now how to follow the desktop), at the saved rate where it
        // has it and the highest where not - the rate kept for a size that has
        // it. Nothing is saved for a size this monitor does not offer.
        const Render::FullscreenChoice choice = Render::ChooseFullscreen(
            DisplayModesOf(*window), DesktopModeOf(*window), action.size, FullscreenRefresh());
        if (choice.sizeFellBack || choice.size.x == 0 || choice.size.y == 0) return;
        if (action.size != SavedFullscreenMode()) {
            m_settings.fullscreenWidth = static_cast<int>(action.size.x);
            m_settings.fullscreenHeight = static_cast<int>(action.size.y);
            SaveSettings();
        }
        RequestFullscreen(*window, action.size.x == 0 ? "automatic picked" : "size picked");
        RefreshRateChoices(registry);
        return;
    }

    // Picked in a window: the window takes that size and renders at it. E23:
    // centred on its monitor (Step 23 kept its top-left, which could leave a
    // larger window hanging off the screen); the automatic line fits it to the
    // monitor.
    if (action.size == glm::uvec2(0u)) {
        if (!window->FitWindowToMonitor(Render::kAutoWindowFraction)) return;
    } else if (!window->SetWindowedSize(action.size.x, action.size.y, true)) {
        return;
    }
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
        // E36: a campaign level started from here is what New Game makes of it (main.as:99-122), so it is played
        // at the difficulty New Game's prompt would open on (the setting, or --difficulty); resetData put the run
        // back to Normal, and an arena is never Hard.
        if (!pvp) {   // E36
            Script::g_runDifficulty = Script::g_difficulty.getCurrent() == Script::DIFFICULTY_HARD   // E36
                                          ? Script::DIFFICULTY_HARD : Script::DIFFICULTY_NORMAL;   // E36
        }   // E36
        Eth::LoadScene("scenes/" + scene, "setupScene", pvp ? "pvpLoop" : "levelLoop");
    }
    SUPERSONIC_LOG_INFO("Penumbra") << "dev start: scenes/" << scene << std::endl;
}

// ---- E35: the Supersonic Engine's intro (render/Splash.hpp) ---------------------------------------------------------
//
// While m_splash holds a clock the Machine is not stepped: no Frame, so no scene load, no music, no GetTime() (it is
// the frame count), no random number is drawn and nothing waits in a pending load - the first menu frame is the one
// it would have been at once. m_ticks stands still too, so --hold, --finger and --tour count from that frame.

void PenumbraLayer::StartSplash() {
    const std::filesystem::path logo = m_options.dataDir / Render::kSplashLogoFile;
    std::error_code ec;
    const std::string path = logo.generic_string();
    if (std::filesystem::is_regular_file(logo, ec)) {
        m_splashImage = glm::vec2(m_textures.Size(path));
        // Plain: no colour key, nothing of the logo is a transparent magenta.
        m_splashKey = m_textures.Key(path, Render::TextureVariant::Plain);
    }
    if (m_splashKey.empty() || !(m_splashImage.x > 0.0f) || !(m_splashImage.y > 0.0f)) {
        m_splashKey.clear();
        SUPERSONIC_LOG_WARN("Penumbra") << "Supersonic intro: the logo cannot be read (" << path
                                        << "); starting without it" << std::endl;
        return;
    }
    m_splash.emplace();
    SUPERSONIC_LOG_INFO("Penumbra") << "Supersonic intro: started" << std::endl;
}

void PenumbraLayer::WatchSplash(const Render::RawDevices& raw) {
    std::vector<int> fingers;
    const int count = Supersonic::Input::ContactCount();
    for (int i = 0; i < count; ++i) {
        const Supersonic::Contact contact = Supersonic::Input::GetContact(i);
        if (contact.id >= 0 && contact.phase != Supersonic::ContactPhase::Ended) fingers.push_back(contact.id);
    }
    if (m_splashWatch.Observe(Render::SplashHeldOf(raw, fingers))) m_splashPressSeen = true;
}

void PenumbraLayer::StepSplash() {
    // The tick as every tick begins it, so the mapper's latches and its cursor go on as they do under the pause.
    const Render::RawDevices raw = Render::InputMapper::PollDevices();
    (void)m_input.BuildTick(raw, m_view);
    ++m_ticksThisFrame;
    WatchSplash(raw);
    const bool pressed = m_splashPressSeen;
    m_splashPressSeen = false;
    m_splash->Step(pressed);
    if (m_splash->Done()) {
        SUPERSONIC_LOG_INFO("Penumbra") << "Supersonic intro: finished at tick " << m_splash->Ticks() << " ("
                                        << (m_splash->Skipped() ? "skipped" : "ran out") << ")" << std::endl;
    }
}

void PenumbraLayer::EndSplash() {
    m_splash.reset();
    // Whatever is down now was pressed during the intro, never seen by the game: the first tick of the game holds
    // it up until it is released, as after a pause, so the press that skipped the intro (a key, a click, a finger,
    // a button) is not also a confirm on the menu's first frame. Only when something real is down or was just
    // pressed: the hold looks at the frame as the first tick makes it, development flags' keys included
    // (--hold from tick 0), and has nothing to hold back from a start with the devices at rest.
    if (m_splashWatch.Held().any() || m_splashPressSeen) m_pause.HoldPressed();
    m_splashPressSeen = false;
}

void PenumbraLayer::DrawSplash(entt::registry& registry) {
    const Render::SplashLayout layout =
        Render::ComputeSplashLayout(m_view.windowPixels, SafeInsets(), m_splashImage);
    m_splashQuads = Render::BuildSplashQuads(m_view.windowPixels, layout, m_splash->Alpha(), m_splashKey);
    auto* const* slot = registry.ctx().find<Supersonic::ScreenOverlay*>();
    if (slot == nullptr || *slot == nullptr) return;   // a bare registry (a suite) has no overlay
    for (const Supersonic::ScreenOverlay::Quad& quad : m_splashQuads) (*slot)->Add(quad);
}

void PenumbraLayer::OnFixedUpdate(entt::registry& registry, float fixedDelta) {
    (void)fixedDelta;
    // E35: the intro ends on the tick after its last, and that tick is the game's first.
    if (m_splash && m_splash->Done()) EndSplash();
    if (m_splash) {
        StepSplash();
        return;
    }
    Eth::Machine::Scope scope(*m_machine);

    // Which pad player 2 reads follows the live g_controls switch.
    m_input.SetPlayer2Pad(static_cast<int>(Script::getPlayerJoystick(1)));
    // E14: in the screens laid out as menus a pad's A and B also confirm and
    // cancel (the scripts read only Start and Back there).
    m_input.SetMenuMode(IsFixedLayoutScene(m_machine->GetSceneFileName()));
    Eth::InputFrame frame = m_input.BuildTick(m_view);
    ApplyDevHolds(frame);
    if (m_options.devCursor) frame.cursor = frame.cursorAbsolute = *m_options.devCursor;
    // --pointer: a window pixel, through the real mouse's mapping, every tick.
    if (m_options.devPointer) {
        frame.cursor = frame.cursorAbsolute = Render::InputMapper::WindowToLogical(*m_options.devPointer, m_view);
    }
    ++m_ticksThisFrame;
    // E26: the HUD's frame for this tick, from the screen the last frame left
    // (a load or E25's return to E1's screen changes it), before the pause
    // button is laid out in it and the scripts draw in it.
    if (!(SafeInsets() == m_zoomChoicesSafe)) RefreshZoomChoices();   // E25: insets that came after attach
    m_hudFrame = CurrentHudFrame();
    m_panelFrame = CurrentHudFrame(false);
    if (!(m_hudFrame == m_hudFrameLogged)) {
        SUPERSONIC_LOG_INFO("Penumbra") << "E26 HUD frame: left " << m_hudFrame.left << " top " << m_hudFrame.top
                                        << " right " << m_hudFrame.right << " (edge margin " << EdgeMargin()
                                        << "%, screen " << m_machine->GetScreenSize().x << "x"
                                        << m_machine->GetScreenSize().y << "; plaque corner " << m_panelFrame.left
                                        << ", " << m_panelFrame.top << ") | tick " << m_ticks << std::endl;
        m_hudFrameLogged = m_hudFrame;
    }
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
    // E28: the touch controls' editor stands the Machine still as the pause does. Read AFTER ApplyTouch above, so the   // E28
    // tick that closes it runs the game and FilterForGame sees the key that closed it.                                // E28
    const bool editing = m_editor.IsOpen();   // E28
    if (pause.tick && !editing) {   // E28: && !editing
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
        // E1: a menu's world past its sides is collected while the window is
        // wider than the 1024x768 it is laid out on (none for a level).
        m_machine->SetSidesShown(Render::WiderThan(m_options.windowPixels, Render::kFixedLayoutScreen));
        // E31: what the window shows of the options scene and the frame inside it, for the phone's options layout   // E31
        // (optionsPhone.cpp). Every tick, whatever the scene: the first frame of a scene runs before the layer knows   // E31
        // it changed. The sides are the scene's (SideMargin), not the window's: flipping the widescreen switch here   // E31
        // applies from the next scene, so the header must not move under a screen that has not. The margin is the   // E31
        // touch controls' always (not the live switch), so the corners stay when this screen turns them off.   // E31
        {   // E31
            const float margin = Render::EdgeMarginPercent(   // E31
                m_options.edgeMarginOverride.value_or(m_settings.edgeMargin), true, m_view.windowPixels);   // E31
            const Render::FixedLayoutArea area = Render::ComputeFixedLayoutArea(   // E31
                m_view.windowPixels, m_machine->SideMargin(), SafeInsets(), margin);   // E31
            Script::g_optionsArea = Script::OptionsArea{area.shownMin, area.shownMax, area.frame.left, area.frame.top,   // E31
                                                        area.frame.right, area.frame.bottom};   // E31
        }   // E31
        // E23: the mode list marks the choice of whichever the window is in.
        const glm::uvec2 chosen = m_windowFullscreen ? SavedFullscreenMode() : SavedWindowedSize();
        Script::g_chosenVideoMode = Eth::videoMode{chosen.x, chosen.y, Eth::PF32BIT};
        // E25: where a phone's larger menu sets its panel's text (showData),
        // from the frame the menu is drawn in; off anywhere else.
        Script::g_phonePanel = Script::PhonePanel{};
        const Render::MenuFrame menuFrame = MenuFrameFor(m_machine->GetSceneFileName(), m_view.windowPixels);
        if (menuFrame.active) {
            // The arena select's back button keeps the panel's text out of its column.
            float cornerLeft = 0.0f;
            if (m_touchEnabled && m_touch.Visible(Render::TouchControl::Back)) {
                cornerLeft = m_touch.Layout()[Render::TouchControl::Back].min.x;
            }
            const Render::MenuPanel panel =
                Render::ComputeMenuPanel(menuFrame, m_view.windowPixels, SafeInsets(), cornerLeft);
            Script::g_phonePanel = Script::PhonePanel{true,          panel.min,      panel.max,
                                                      panel.minScale, panel.maxScale, panel.shownMin,
                                                      panel.shownMax, m_localization.RightToLeft()};
        }
        // E26: the HUD's values and frame (zero outside a level or an arena).
        // The player panel's plaque stands at the safe area's corner, not in the
        // margin: wherever the touch controls are.
        Script::g_touchHud = Script::TouchHud{m_touchEnabled,   m_hudFrame.left,  m_hudFrame.top,
                                              m_hudFrame.right, m_hudFrame.bottom, m_touchEnabled,
                                              m_panelFrame.left, m_panelFrame.top};
        // E25: the door raises it again in this frame if it still offers the way on.
        Script::g_nextLevelOffered = false;
        m_machine->Frame(frame);   // steps the key and button state machines itself
        if (Script::g_nextLevelOffered != m_nextLevelOffered) {
            const bool offered = Script::g_nextLevelOffered;
            SUPERSONIC_LOG_INFO("Penumbra") << "E25 next level " << (offered ? "offered" : "no longer offered")
                                            << ": the down button " << (offered ? "shows" : "hides") << " | tick "
                                            << m_ticks << std::endl;
        }
        m_nextLevelOffered = Script::g_nextLevelOffered;
        m_nextLevelSerial = m_machine->Snapshot().sceneSerial;
        // E25: a zoomed campaign scene goes back to E1's screen, from here to
        // the next load, once the king is beaten - the end screen lays its
        // time and best times out for the unzoomed screen (setupScene.as:
        // 365-373, down to y 566) - or once player 2's princess is there, in
        // a scene loaded with her or summoned into it: the leash that kills
        // her 3 s off screen (controlCharacters.as:342-360) is the screen,
        // which the zoom would pull in (by a fifth at 125%, two-fifths at 175%).
        // SeekEntity walks the whole scene: only where the answer can matter,
        // a zoomed campaign scene.
        const bool zoomedCampaign = Render::IsCampaignScene(m_machine->GetSceneFileName()) &&
                                    m_machine->GetScreenSize().y < Render::kUnzoomedHeight;
        const bool princess = zoomedCampaign && Eth::SeekEntity(Script::MAIN_CHARACTER_ENTITY1) != nullptr;
        if (Render::CampaignUnzooms(m_machine->GetSceneFileName(), m_machine->GetScreenSize(),
                                    Script::g_gameFinished, princess)) {
            const Eth::vector2 unzoomed = Render::ZoomedScreen(m_options.windowPixels, Widescreen(), 1.0f);
            m_machine->SetScreenSize(unzoomed);
            SUPERSONIC_LOG_INFO("Penumbra") << "E25 " << (Script::g_gameFinished ? "end screen" : "player 2's princess")
                                            << ": the zoom ends, the screen is " << unzoomed.x << "x" << unzoomed.y
                                            << " | tick " << m_ticks << std::endl;
        }
        // --princess and --hp: once, the first tick the wizard exists in a
        // campaign level (the princess) or any level or arena (hp).
        if ((m_options.devPrincess && !m_devPrincessDone) || (m_options.devHp && !m_devHpDone)) {
            if (Eth::ETHEntity wizard = Eth::SeekEntity("bruxo.ent"); wizard != nullptr && InPlayScene()) {
                if (m_options.devPrincess && !m_devPrincessDone &&
                    Render::IsCampaignScene(m_machine->GetSceneFileName())) {
                    // Where controlCharacters.as:700-705 summons her: one box to his right, 6 px up.
                    const Eth::vector2 at = wizard->GetPositionXY() + Eth::vector2(48.0f, -6.0f);
                    Eth::AddEntity(Script::MAIN_CHARACTER_ENTITY1, Eth::vector3(at, 0.0f), 0.0f);
                    m_devPrincessDone = true;
                    SUPERSONIC_LOG_INFO("Penumbra") << "dev princess at " << at.x << "," << at.y << " | tick "
                                                    << m_ticks << std::endl;
                }
                if (m_options.devHp && !m_devHpDone) {
                    wizard->AddIntData("hp", *m_options.devHp);
                    m_devHpDone = true;
                    SUPERSONIC_LOG_INFO("Penumbra") << "dev hp " << *m_options.devHp << " | tick " << m_ticks
                                                    << std::endl;
                }
            }
        }
        // --spawn: once, the first tick the wizard exists in a level or arena;
        // the camera follows him on the next tick as it follows any move.
        if (m_options.devSpawn && !m_devSpawned && InPlayScene()) {
            if (Eth::ETHEntity wizard = Eth::SeekEntity("bruxo.ent"); wizard != nullptr) {
                wizard->SetPositionXY(Eth::vector2(m_options.devSpawn->x, m_options.devSpawn->y));
                m_devSpawned = true;
                SUPERSONIC_LOG_INFO("Penumbra") << "dev spawn: the wizard at " << m_options.devSpawn->x << ","
                                                << m_options.devSpawn->y << " | tick " << m_ticks << std::endl;
            }
        }
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
    const bool comboAssist = Script::g_comboAssist.getCurrent() == 0;   // E46: at once, like the second player's keys
    if (layout != m_settings.controls.joystickLayout || pixelShaders != m_settings.pixelShaders ||
        keyboardPlayer2 != m_settings.controls.keyboardPlayer2 || comboAssist != m_settings.controls.comboAssist) {   // E46
        m_settings.controls.joystickLayout = layout;
        m_settings.pixelShaders = pixelShaders;
        m_settings.controls.keyboardPlayer2 = keyboardPlayer2;   // E10
        m_settings.controls.comboAssist = comboAssist;   // E46
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
    // E27: the language row is 0 = automatic, i + 1 = kLanguages[i]; a pick is
    // a row other than the one last seeded or applied (m_languageChoiceSeeded),
    // which a --lang run seeds with its own language, so only the player moves it.
    // Automatic is the system's language (English when the game does not speak
    // it), applied at once like any pick, and written as "auto".
    const Eth::uint languageRow = Script::g_language.getCurrent();
    const bool languagePicked = languageRow != m_languageChoiceSeeded && languageRow <= Render::kLanguageCount;
    const bool widescreen = Script::g_widescreen.getCurrent() == 0;
    const bool smoothMotion = Script::g_smoothMotion.getCurrent() == 0;
    const bool pauseOnFocusLoss = Script::g_pauseOnFocusLoss.getCurrent() == 0;
    const bool hardDifficulty = Script::g_difficulty.getCurrent() == Script::DIFFICULTY_HARD;   // E36
    const bool musicMoved = Script::g_musicVolume.getCurrent() != Script::g_musicVolume.stepFor(m_settings.musicVolume);
    const bool effectsMoved =
        Script::g_effectsVolume.getCurrent() != Script::g_effectsVolume.stepFor(m_settings.effectsVolume);
    if (languagePicked || widescreen != Widescreen() || smoothMotion != SmoothMotion() ||
        pauseOnFocusLoss != PauseOnFocusLoss() || hardDifficulty != HardDifficulty() || musicMoved || effectsMoved) {   // E36: hardDifficulty
        if (languagePicked) {
            m_options.languageOverride.reset();
            m_languageChoiceSeeded = languageRow;
            if (languageRow == 0) {
                m_settings.languageAuto = true;
                m_settings.language = Render::Settings::SystemLanguage();
            } else {
                m_settings.languageAuto = false;
                m_settings.language = Render::LanguageId(Render::kLanguages[languageRow - 1].language);
            }
            SUPERSONIC_LOG_INFO("Penumbra") << "E27 language picked: "
                                            << (m_settings.languageAuto ? "automatic (" + m_settings.language + ")"
                                                                        : m_settings.language)
                                            << std::endl;
        }
        const bool viewChanged = widescreen != Widescreen();
        if (viewChanged) {
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
        // E36: g_difficulty changes only when New Game's prompt confirms a pick (never on hover or cancel), so a change
        // here is a choice: it replaces --difficulty and is saved at once as the difficulty the prompt opens on next
        // time. The run that pick starts latched it in newGame (Script::g_runDifficulty); it does not read it again.
        if (hardDifficulty != HardDifficulty()) {   // E36
            m_options.hardDifficultyOverride.reset();   // E36
            m_settings.difficulty = hardDifficulty ? "hard" : "normal";   // E36
        }   // E36
        if (musicMoved) m_settings.musicVolume = Script::g_musicVolume.getFraction();
        if (effectsMoved) m_settings.effectsVolume = Script::g_effectsVolume.getFraction();
        ApplyVolumes();
        SaveSettings();   // ApplyLanguage() first
        if (viewChanged) RefreshZoomChoices();   // E25: 4:3 levels have less room to zoom
    }

    // E20: the touch controls' row (drawn on a phone's options screen only).
    // A pick replaces --touch, is saved as "on" or "off" - no longer "auto" -
    // and applies from the next tick.
    const bool touchControls = Script::g_touchControls.getCurrent() == 0;
    if (touchControls != m_touchEnabled) {
        m_options.touchOverride.reset();
        m_settings.touchControls = touchControls ? "on" : "off";
        SetTouchEnabled(touchControls);
        RefreshZoomChoices();   // E25/E26: the zoom's limit follows the frame, which follows touch
        SaveSettings();
    }

    // E28: the close key (Esc; under touch no pad is player 1) as the frame of this tick has it: one held as the editor opens is not a close.   // E28
    const bool closeKeyHeld = Render::PauseMenu::InputFrom(frame, static_cast<int>(Script::getPlayerJoystick(0)), false,   // E28
                                                           m_machine->GetScreenSize()).back;                          // E28
    // E28: the options screen's "Adjust controls" (raised by the frame that ran this tick), or --touch-editor on the   // E28
    // first tick the options scene is up. Only with the touch controls on, which is what the button is drawn under.    // E28
    if (Script::g_adjustTouchControls) {                                                                              // E28
        Script::g_adjustTouchControls = false;                                                                        // E28
        if (m_touchEnabled && !m_editor.IsOpen()) OpenTouchEditor(false, closeKeyHeld);                               // E28
    }                                                                                                                 // E28
    if (m_options.devTouchEditor.has_value() && !m_devEditorOpened && m_touchEnabled && !m_editor.IsOpen() &&         // E28
        m_machine->GetSceneFileName() == "scenes/videoModes.esc") {                                                   // E28
        m_devEditorOpened = true;                                                                                     // E28
        OpenTouchEditor(*m_options.devTouchEditor, closeKeyHeld);                                                     // E28
    }                                                                                                                 // E28

    // E25: the zoom's row (a phone's options screen). A pick replaces --zoom,
    // is saved, and applies from the next level loaded, as E1's view does.
    const uint32_t zoomIndex = Script::g_zoom.getCurrent();
    if (zoomIndex != m_zoomChoiceSeeded && zoomIndex < m_zoomChoices.size()) {
        m_options.zoomOverride.reset();
        m_settings.zoom = m_zoomChoices[zoomIndex];
        SaveSettings();
        SUPERSONIC_LOG_INFO("Penumbra") << "E25 zoom picked: "
                                        << (m_settings.zoom == Render::kZoomAutomatic
                                                ? std::string("automatic")
                                                : std::to_string(m_settings.zoom) + "%")
                                        << " (from the next level)" << std::endl;
        RefreshZoomChoices();
    }

    // E23: the refresh rate's row. A pick replaces --refresh and is saved; in
    // fullscreen the display switches to it at once, at the size it is at; on
    // a phone the display's preferred mode follows; in a window it waits for
    // the next fullscreen (the row's hint says so).
    const uint32_t rateIndex = Script::g_refreshRate.getCurrent();
    if (rateIndex != m_rateChoiceSeeded && rateIndex < m_rateChoices.size()) {
        m_options.refreshOverride.reset();
        m_settings.fullscreenRefresh = static_cast<int>(m_rateChoices[rateIndex]);
        SaveSettings();
        SUPERSONIC_LOG_INFO("Penumbra") << "E23 refresh rate picked: "
                                        << (m_settings.fullscreenRefresh == 0
                                                ? std::string("automatic")
                                                : std::to_string(m_settings.fullscreenRefresh) + " Hz")
                                        << (Render::kMobileBuild ? " (the display's)"
                                            : m_windowFullscreen ? " (now)"
                                                                 : " (the next time fullscreen)")
                                        << std::endl;
        if (Supersonic::WindowControl* window = WindowControlOf(registry)) {
            if constexpr (Render::kMobileBuild) {
                if constexpr (kPhoneRefreshRate) window->SetPreferredRefreshRate(PreferredRefreshRate());
            } else if (m_windowFullscreen) {
                RequestFullscreen(*window, "refresh rate picked");
            }
        }
        RefreshRateChoices(registry);
    }

    if (m_machine->QuitRequested()) Supersonic::Application::RequestQuit();
}

// E38: what waits for a first drawn frame, in two steps. The engine draws between two OnUpdates and applies a window
// request at the top of the next frame, before the update:
//  - the second OnUpdate: the first DrawFrame has returned, so the window has been drawn into; the display switch the
//    launch asked for is requested now (the engine applies it at the top of the third frame);
//  - the third: that switch, which blocks in the driver and is the stall this exists for, has returned, so the start
//    is finished and its marker goes. Taken away earlier, a hang in the switch would leave no mark of an unfinished
//    start.
void PenumbraLayer::FinishLaunch(entt::registry& registry) {
    if (m_updates == 2) {
        if (m_launchFullscreenPending) {
            m_launchFullscreenPending = false;
            Supersonic::WindowControl* window = WindowControlOf(registry);
            // Still fullscreen: a window the player left in the first moments is not put back.
            if (window != nullptr && window->IsFullscreen()) {
                RequestFullscreen(*window, "launch, after the first frame");
            }
        }
        return;
    }
    if (!m_options.startMarker.empty()) {
        std::error_code ec;
        std::filesystem::remove(m_options.startMarker, ec);
        if (ec) {
            SUPERSONIC_LOG_WARN("Penumbra") << "the start marker " << m_options.startMarker.string()
                                            << " could not be removed (" << ec.message()
                                            << "): the next start opens in a window" << std::endl;
        }
    }
}

void PenumbraLayer::OnUpdate(entt::registry& registry, float deltaTime) {
    (void)deltaTime;
    if (m_updates < 3) {   // E38
        ++m_updates;
        if (m_updates >= 2) FinishLaunch(registry);
    }
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
    // E28: the editor stops the Machine as the pause does: no tick moves the blend on, and a live alpha would rock the   // E28
    // options scene under it between its last two.                                                                    // E28
    const bool frozen = paused || m_editor.IsOpen();   // E28
    const float alpha = frozen || clock == nullptr ? 1.0f : clock->alpha;   // E28: was `paused`
    const Eth::RenderSnapshot& world = m_interp.Frame(snapshot, alpha);
    // E25: a phone's menu, larger and to the left; the pointer and the touch
    // controls map through the view this returns, as ever.
    // The image the rig measures, with the same stand-in before one is published.
    const Render::MenuFrame menuFrame =
        MenuFrameFor(snapshot.sceneFile, Render::CameraRig::WindowPixels(registry, snapshot.screenSize));
    m_view = m_rig.Update(registry, world, m_pillarbox, &menuFrame);
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
        // E35: not during the intro, whose frame the bars of the machine's (empty) view must not decide.
        window->SetCursorVisible(!m_splash && (!snapshot.cursorHidden || paused || m_editor.IsOpen() || overBars));   // E28: the scripts' cursor is frozen under the editor
    }
    if (m_splash) {
        // E35: the intro's frame instead of the machine's: its quads in window pixels, so nothing of the machine's
        // view (its bars, its logical screen) is in it. A press in a frame no tick ran is kept for the next tick;
        // the mapper's latches go on as in any frame.
        DrawSplash(registry);
        if (m_ticksThisFrame == 0) {
            const Render::RawDevices raw = Render::InputMapper::PollDevices();
            WatchSplash(raw);
            m_input.EndFrame(raw);
        } else {
            m_input.EndFrame();
        }
        m_ticksThisFrame = 0;
        return;
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
    if (m_editor.IsOpen()) {   // E28: the editor draws the controls itself, under its own widgets
        m_editor.AppendOverlay(m_touch, m_overlay);   // E28
    } else {   // E28
        if (m_touchEnabled) m_touch.AppendOverlay(m_overlay);   // E28
        m_pause.AppendOverlay(m_overlay);   // E28
    }   // E28
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
