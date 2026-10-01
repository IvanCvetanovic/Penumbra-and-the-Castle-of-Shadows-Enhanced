#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include "core/EngineLayer.hpp"
#include "core/WindowControl.hpp"

#include "eth/EthTypes.hpp"
#include "platform/SafeArea.hpp"
#include "render/AudioOutEngine.hpp"
#include "render/CameraRig.hpp"
#include "render/FontAtlas.hpp"
#include "render/HudRenderer.hpp"
#include "render/InputMapper.hpp"
#include "render/Interpolation.hpp"
#include "render/LightRenderer.hpp"
#include "render/Localization.hpp"
#include "render/ParticleRenderer.hpp"
#include "render/PauseMenu.hpp"
#include "render/PhoneUi.hpp"
#include "render/Settings.hpp"
#include "render/ShadowRenderer.hpp"
#include "render/SpriteRenderer.hpp"
#include "render/TextureCache.hpp"
#include "render/TouchControls.hpp"
#include "render/TouchEditor.hpp"   // E28
#include "render/View.hpp"
#include "render/WindowMode.hpp"

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
//    the menus, laid out for 1024x768, keep that size, centred, and in a wide
//    window the world goes on past their sides (render/WideMenus.hpp).
//  - Settings (render/Settings.hpp) persist in the user directory; the
//    original's own options switches are seeded from them.
//  - The window follows the scripts (E2): SetWindowProperties's windowed flag
//    (Alt+Enter, the options screen's switch) and its mode list go to the
//    engine's WindowControl - a mode picked while fullscreen switches the
//    display to it, as 0.7.12 did (render/WindowMode.hpp) - and HideCursor
//    hides the system pointer, as 0.7.12 did while cursor.ent was drawn in
//    its place, except over the bars, where cursor.ent cannot be seen.
//  - The display mode is automatic unless the player picks one (E23):
//    fullscreen at the desktop's size and the monitor's highest rate there, a
//    window fitted to the monitor; the options screen's mode list and its
//    refresh-rate row pick either by hand.
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

    // E28: a synthetic finger for headless captures (--finger): in LOGICAL pixels, down from tick `fromTick`   // E28
    // to `toTick` inclusive (counted from the first tick this layer runs, as --hold), moving in a straight   // E28
    // line from `from` to `to` over that span.                                                                // E28
    struct DevFinger {                                                                                        // E28
        int id = 0;                                                                                           // E28
        glm::vec2 from{0.0f};                                                                                 // E28
        glm::vec2 to{0.0f};                                                                                   // E28
        unsigned fromTick = 0;                                                                                // E28
        unsigned toTick = 0;                                                                                  // E28
    };                                                                                                        // E28

    struct Options {
        // The original's files and the port's own data (strings.json,
        // images/en), as main.cpp found them (eth/Paths.hpp): beside the
        // executable in a packaged game, else where the build points.
        std::filesystem::path originalDir = PENUMBRA_ORIGINAL_DIR;
        std::filesystem::path dataDir = PENUMBRA_DATA_DIR;
        std::filesystem::path userDir;      // where saves and settings go; empty = none
        std::string startScene;             // "" = the menu, as the original boots
        // --tour a,b,c@N: after the menu boots, start each scene in turn for N
        // ticks (as --start starts one), for one headless launch that captures
        // many screens - every launch of a fresh exe is a Smart App Control
        // verdict, and every refusal a notification on the desktop.
        std::vector<std::string> tour;
        unsigned tourTicks = 0;
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
        // E13's pause on focus loss for this run (off under --fixed-step: a
        // capture's window often never has the focus); never saved.
        std::optional<bool> pauseOnFocusLossOverride;
        // E16's touch controls for this run (--touch: on, the mouse as the
        // finger); never saved.
        std::optional<bool> touchOverride;
        // E25's zoom for this run (--zoom auto|<percent>: 0 is automatic);
        // never saved, and a pick on a phone's options screen replaces it.
        std::optional<int> zoomOverride;
        // E20's phone layout of the options screen and the menu's footer for
        // this run (--mobile-layout on|off), for captures of a phone's screens
        // on a desktop. Dev-only, never saved.
        std::optional<bool> mobileLayoutOverride;
        // E26's edge margin for this run (--edge-margin auto|<percent>:
        // negative is automatic); never saved.
        std::optional<float> edgeMarginOverride;
        // --safe-area l,t,r,b: the display's safe-area insets (window px) in
        // place of what the platform reports (none on a desktop), for
        // captures of a notched phone's screens on a desktop. Dev-only.
        std::optional<Supersonic::SafeAreaInsets> safeAreaOverride;
        // --cursor x,y: the scripts' cursor pinned at a logical-screen point,
        // for headless captures of the mouse-driven menu (the live OS pointer
        // otherwise decides which panel a capture shows). A finger (E16)
        // moves it only while it is down: after the lift the pin is back, so
        // on a device run with this flag a tap's picker or cursor light
        // jumps back to the pinned point (Step 26).
        std::optional<glm::vec2> devCursor;
        // --pointer x,y: the scripts' cursor at a WINDOW pixel, mapped every
        // tick as the real mouse is (the view's offset and scale), for captures
        // that check a click lands where it is seen at any window size. Like
        // --cursor, it overrides the scripts' own warps. Dev-only.
        std::optional<glm::vec2> devPointer;
        // --spawn x,y: the wizard put at a scene point (Ethanon pixels) the
        // first tick he exists, for captures of places far from a level's
        // start (Step 24's pit edges). Dev-only, never saved.
        std::optional<glm::vec2> devSpawn;
        // --princess: player 2's princess put beside the wizard the first
        // tick he exists in a campaign level, as a pad's Start would summon
        // her (without its cost), for captures of co-op (E25's unzoom, E26's
        // two HUD blocks). Dev-only.
        bool devPrincess = false;
        // --hp <n>: the wizard's hp set once, the first tick he exists, for
        // captures of a low bar (E26's values). Dev-only.
        std::optional<int> devHp;
        // E23 for this run (--refresh auto|<Hz>): the fullscreen refresh rate
        // over the settings' (0 = automatic); never saved, and a pick on the
        // options screen's row replaces it, as a pick replaces --lang.
        std::optional<uint32_t> refreshOverride;
        // --modes: the display modes the options screen lists and its
        // refresh-rate row offers, and the desktop's among them, instead of
        // the monitor's - for captures of a list the capture machine does not
        // have (a virtual X server lists one mode). Dev-only, never saved; a
        // pick still goes to the real monitor, which refuses a mode it lacks.
        std::vector<Supersonic::DisplayMode> devModes;
        Supersonic::DisplayMode devDesktop;
        // E28's touch tuning for this run (--touch-tuning): over the settings' until the editor's first        // E28
        // commit replaces it. Never saved.                                                                    // E28
        std::optional<Render::TouchTuning> touchTuningOverride;                                                // E28
        // --touch-editor [locked|unlocked]: opens the editor the first tick the options scene is up (the     // E28
        // value is whether it starts unlocked). Dev-only.                                                    // E28
        std::optional<bool> devTouchEditor;                                                                    // E28
        // --finger: synthetic fingers merged into the touch contacts. Dev-only.                              // E28
        std::vector<DevFinger> devFingers;                                                                     // E28
        // Any of the three E28 flags above: SaveSettings never writes, so a synthetic drag cannot reach the   // E28
        // player's real settings.json.                                                                       // E28
        bool noSave = false;                                                                                   // E28
    };

    explicit PenumbraLayer(Options options);
    ~PenumbraLayer() override;

    const char* Name() const override { return "Penumbra"; }
    void OnAttach(entt::registry& registry) override;
    void OnDetach(entt::registry& registry) override;
    void OnFixedUpdate(entt::registry& registry, float fixedDelta) override;
    void OnUpdate(entt::registry& registry, float deltaTime) override;

    Eth::Machine* Machine() { return m_machine.get(); }

    // E28: what a test can read of the touch controls and their editor (the layer-driven check at the end of   // E28
    // test_pn_render_hud): whether the editor is open, the controls as they are laid out, the overlay this     // E28
    // layer last built for the HUD pass, and the settings as they stand.                                       // E28
    bool EditorOpen() const { return m_editor.IsOpen(); }                                                      // E28
    const Render::TouchEditor& Editor() const { return m_editor; }                                             // E28
    const Render::TouchControls& Touch() const { return m_touch; }                                             // E28
    const std::vector<Eth::HudCmd>& Overlay() const { return m_overlay; }                                      // E28
    const Render::Settings& CurrentSettings() const { return m_settings; }                                     // E28

private:
    // The screen the scripts see for a scene (GetScreenSize): the menus'
    // 1024x768, or the widescreen view.
    Eth::vector2 LogicalScreenFor(const std::string& sceneFile) const;
    void ApplyDevHolds(Eth::InputFrame& frame) const;
    void StartDevScene(const std::string& scene);
    // The engine's window, or null (a host that publishes none).
    static Supersonic::WindowControl* WindowControlOf(entt::registry& registry);
    // What the scripts asked of the window this tick, handed to the engine.
    void ApplyWindowRequest(entt::registry& registry);
    // The settings' fullscreen display mode; 0 x 0 is automatic (the desktop's size).
    glm::uvec2 SavedFullscreenMode() const;
    // E23: the settings' windowed size; 0 x 0 is automatic (fitted to the monitor).
    glm::uvec2 SavedWindowedSize() const;
    // E23: the fullscreen refresh rate this run uses (--refresh, else the
    // settings'); 0 is automatic.
    uint32_t FullscreenRefresh() const;
    // E23: that rate as a phone's display is asked for it.
    uint32_t PreferredRefreshRate() const;
    // E23: the monitor's modes and its desktop mode, or --modes'.
    std::vector<Supersonic::DisplayMode> DisplayModesOf(const Supersonic::WindowControl& window) const;
    Supersonic::DisplayMode DesktopModeOf(const Supersonic::WindowControl& window) const;
    // E23: where fullscreen goes on this monitor (render/WindowMode.hpp).
    Render::FullscreenChoice FullscreenChoiceFor(const Supersonic::WindowControl& window) const;
    // E23: fullscreen at that choice - Alt+Enter, the options' switch, a pick,
    // a launch - logged with `why`; the desktop's mode when nothing else is left.
    void RequestFullscreen(Supersonic::WindowControl& window, const char* why);
    // GetVideoMode's list, from the monitor the window is on, and E23's marks.
    void RefreshVideoModes(entt::registry& registry);
    // E23: the refresh-rate row's options, from the fullscreen choice (the
    // desktop) or the phone's two.
    void RefreshRateChoices(entt::registry& registry);
    // The HUD's and the sprites' language, from m_settings (or --lang).
    void ApplyLanguage();
    // What this run shows: the settings unless a flag overrides them.
    bool Widescreen() const;
    Render::Language CurrentLanguage() const;
    bool SmoothMotion() const;
    bool PauseOnFocusLoss() const;
    // E13: a level or an arena being played, where the pause may open.
    bool InPlayScene() const;
    // The master volumes: the player's, the music ducked while paused (E13).
    void ApplyVolumes();
    // Writes m_settings to the user directory and re-applies what it drives.
    void SaveSettings();
    // E16: the fingers on the screen (the engine's contacts: real touches on
    // a phone, the held left mouse button on a desktop) in logical pixels.
    std::vector<Render::TouchContact> TouchContacts() const;
    // E16: this tick's touches pressed into the frame, before the pause and
    // the game read it.
    void ApplyTouch(Eth::InputFrame& frame);
    // E28: this tick's TouchInput - ApplyTouch's, extracted so that opening and closing the editor can lay the   // E28
    // controls out again for the scene they are about to be in, with the same fingers and the same geometry.    // E28
    Render::TouchInput BuildTouchInput();                                                                       // E28
    // E28: the tuning in force: --touch-tuning's until the editor first commits, else the settings'.            // E28
    Render::TouchTuning TuningNow() const { return m_options.touchTuningOverride.value_or(m_settings.touchTuning); }   // E28
    // E28: open the touch controls' editor (the options screen's button, --touch-editor): the Machine stands    // E28
    // still until it closes, as under the pause.                                                                // E28
    void OpenTouchEditor(bool startUnlocked, bool closeKeyHeld);                                                                 // E28
    // E16/E20: the touch controls on or off, their layout read the first time
    // they come on (at attach, or from the options screen's row), and with
    // them E22's pad order (InputMapper::PadOrder).
    void SetTouchEnabled(bool enabled);
    // E25 (render/PhoneUi.hpp): the zoom setting this run uses (--zoom, else
    // the settings'), and the zoom a campaign level loads at now.
    int ZoomSetting() const;
    float CampaignZoom() const;
    // E25: the frame of a phone's larger menu for a scene drawn in an image
    // this large - inactive unless the touch controls are on, E1's widescreen
    // too, and the scene is the menu or the arena select.
    Render::MenuFrame MenuFrameFor(const std::string& sceneFile, glm::uvec2 image) const;
    // E25: the zoom row's options (Script::g_zoom), from the setting.
    void RefreshZoomChoices();
    // The display's safe-area insets now, window px (--safe-area, else the platform's).
    Supersonic::SafeAreaInsets SafeInsets() const;
    // E26 (render/PhoneUi.hpp): the edge margin this run uses, percent
    // (--edge-margin, else the settings'; 0 without the touch controls), as
    // much of it as the window leaves the messages their room with.
    float EdgeMargin() const;
    // E26: the HUD's frame for this tick: in a level's or an arena's loop
    // with the touch controls on, from the screen the scripts have now;
    // zero anywhere else.
    // The frame of this tick's HUD: the edge margin and the safe area's insets, or,
    // asked without the margin, the safe area's alone (where E26's plaque stands).
    Render::HudFrame CurrentHudFrame(bool withMargin = true) const;

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
    // E13: the pause.
    Render::PauseMenu m_pause;
    // E16: the touch controls, when this run has them.
    Render::TouchControls m_touch;
    bool m_touchEnabled = false;
    bool m_touchManifestLoaded = false;
    // E28: the editor of those controls' size, opacity and places; whether --touch-editor has opened it yet;   // E28
    // and whether SaveSettings has said once that a dev run writes nothing.                                   // E28
    Render::TouchEditor m_editor;                                                                              // E28
    bool m_devEditorOpened = false;                                                                            // E28
    bool m_noSaveLogged = false;                                                                               // E28
    unsigned m_ticksThisFrame = 0;   // the latch of a tap no tick saw
    // The layer's own HUD commands for the HUD pass, in drawing order: the
    // touch controls (E16), then the pause (E13).
    std::vector<Eth::HudCmd> m_overlay;
    std::unique_ptr<Eth::Machine> m_machine;
    Render::View m_view;
    bool m_pillarbox = true;
    unsigned m_ticks = 0;
    bool m_devStarted = false;
    bool m_devSpawned = false;
    std::size_t m_tourIndex = 0;
    unsigned m_tourSince = 0;
    // The last windowed/fullscreen state asked of the engine. Not the Machine's
    // (SetWindowProperties overwrites it before the layer sees the request) and
    // not WindowControl::IsFullscreen (a frame late).
    bool m_windowFullscreen = false;
    const Eth::Scene* m_modesScene = nullptr;   // the scene the mode list was read for
    // E23: the refresh-rate row's rates (0 = automatic), as last given to
    // Script::g_refreshRate, and the index it was given.
    std::vector<uint32_t> m_rateChoices;
    uint32_t m_rateChoiceSeeded = 0;
    // E25: the zoom row's percentages (0 = automatic) and the index given to
    // Script::g_zoom; and whether the next_level door offered player 1 the
    // way on in the last frame, and in which scene (a load drops it).
    std::vector<int> m_zoomChoices;
    uint32_t m_zoomChoiceSeeded = 0;
    // E27: the language row's choice as it was last seeded or applied. The row
    // is 0 = automatic (the system's language), then Render::kLanguages in
    // order: index i + 1 is kLanguages[i]. A different index is a pick; what
    // the run shows (--lang) is only ever the seed, so a flag never reads as one.
    uint32_t m_languageChoiceSeeded = 0;
    bool m_nextLevelOffered = false;
    unsigned m_nextLevelSerial = 0;
    // E26: this tick's HUD frame (CurrentHudFrame), which the scripts and the
    // pause button get, and the last one logged.
    Render::HudFrame m_hudFrame;
    // E26: the same without the margin, the corner of the player panel's stone plaque.
    Render::HudFrame m_panelFrame;
    Render::HudFrame m_hudFrameLogged;
    // E25: the safe insets RefreshZoomChoices last counted with; insets can
    // arrive after attach (SafeArea.hpp), and the zoom's limit follows them.
    Supersonic::SafeAreaInsets m_zoomChoicesSafe;
    bool m_devPrincessDone = false;   // --princess
    bool m_devHpDone = false;         // --hp
};

} // namespace Penumbra
