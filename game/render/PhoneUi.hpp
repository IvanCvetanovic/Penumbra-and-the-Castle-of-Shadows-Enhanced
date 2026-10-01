#pragma once

// ENHANCEMENT E25: a phone-sized UI. On a phone the original's 1024x768 layout
// came out small: its levels show a wide stretch of the castle around a small
// wizard, its menu sits in the middle of a wide screen with the side panel's
// text at the size a 4:3 monitor had it, and E16's direction control carried a
// down arrow that only ever does anything at a level's exit. Wherever the touch
// controls are on (E16: a phone or a tablet by default, `--touch on` on a
// desktop), and nowhere else:
//
//   THE ZOOM. The campaign's levels (level1..3.esc and the checkpoint.esc a
//   death reloads) get a logical screen the zoom times smaller than E1's, the
//   window's shape kept; the renderer scales it up to the window. The scripts
//   see it as GetScreenSize(), so the camera's dead zone, the HUD, the
//   messages and the pause are all laid out in it, as they are in E1's wider
//   one. Not the menus, the options, game over or the Versus arenas (both
//   players must stay on screen). settings.json "zoom": "auto" (175% on a
//   phone-shaped screen, 125% on any other touchscreen) or a percentage from
//   100 to 200; a phone's options screen has the row. The zoom stops where a
//   message line would lose its room (MaxZoom, with E26's frame), and E1's
//   unzoomed screen comes back for the rest of the scene (CampaignUnzooms):
//   once the king is beaten, whose end screen lays its best times out for it
//   (down to y 566), and while player 2's princess is in the campaign, whom
//   controlCharacters.as:342-360 kills 3 s after she leaves the screen - a
//   zoomed screen would pull that leash in by a fifth at 125%, two-fifths at 175%.
//
//   THE MENU, LARGER. menu.esc and arena_select.esc keep their 1024x768 for
//   the scripts, but the view scales them up until the logo and the seven
//   buttons (kMenuFocusMin..Max, every language's art) fill the window's
//   height inside its safe area, set toward the left; the side panel
//   (menu.as:217) runs from its own left edge to the window's right, and its
//   title and body are set in what the window shows of it, as large as each
//   text allows (the script's showData with HudCmd::fit; a long body in two
//   columns, split at a blank line, where that sets it larger). Only where
//   that makes the menu at least kMenuMinGain larger than E1's view without
//   leaving the panel narrower - a phone, a 16:10 tablet; a 4:3 one is left as
//   it is.
//
//   THE DOWN BUTTON is the touch controls' (render/TouchControls.hpp): the
//   direction control is left and right only, and a down button shows above
//   them while the next_level door offers the way on.
//
// ENHANCEMENT E26: a safe frame for the HUD, for curved and cut-out screens.
// In play the original draws its HUD against the screen's edges: the hp, mp
// and lv bars from (0, 0) with their values riding the bars' ends in the bars'
// own dark colours, the lives at (448, 0), the run's timer at (width - 50, 0),
// the message lines from (10, 70) and "Carregando..." at (20, height - 70)
// (interface.as:43-95, setupScene.as:333 and :340, util.as:457). On a phone those edges are where the glass curves away or the
// corners are rounded off, so the values could not be read. Wherever the touch controls
// are on, those elements are drawn inside a frame instead: the display's safe
// area (Supersonic::SafeArea: a notch, a hole, rounded corners it reports) or
// an edge margin - settings.json "edgeMargin", "auto" or a percentage from 0
// to 8 of the screen's width at each side and of its height at the top, since
// no phone reports a curve - whichever keeps more; the touch pause button
// hangs from the same frame's corner, below the timer. Auto is
// kPhoneEdgeMarginPercent on a phone-shaped screen and
// kTabletEdgeMarginPercent on any other touchscreen. The values are drawn in
// the HUD's light text with the dark shadow of its lives counter, and never
// slide off the bars' left end. World text (the lore signs, the half-size
// message under the wizard, the damage numbers) is where its world is.
//
// ONE RULE holds the messages, the zoom and the frame together: a message
// line, from kMessageInset past the frame's left edge, keeps kMessageRoom +
// kMessageGap clear up to the pause button's column (kPauseColumn in from the
// frame's right edge, at the screen's unit) on every screen the touch
// controls draw a level on. E24's rooms (tests/data/l10n_rooms.json) hold
// every message a touch screen draws to kMessageRoom, so none runs under the
// translucent button. The zoom stops where the rule would break (MaxZoom),
// and a margin set by hand is held to what keeps it on E1's screen
// (FittedEdgeMargin).
//
// Pure: the layer and the suites call it with a window's size.

#include <string>

#include <glm/glm.hpp>

#include "platform/SafeArea.hpp"

namespace Penumbra::Render {

// --- The zoom --------------------------------------------------------------------------------

// settings.json "zoom": 0 is automatic, otherwise a percentage in [100, 200].
inline constexpr int kZoomAutomatic = 0;
inline constexpr int kZoomMinPercent = 100;
inline constexpr int kZoomMaxPercent = 200;
// The options screen's choices after "Autom\xE1tica".
inline constexpr int kZoomSteps[] = {100, 125, 150, 175, 200};
// A screen at least this long for its height (landscape) is a phone's: 18:9,
// 19.5:9, 20:9 and 21:9 are; 16:9 and every tablet (4:3, 3:2, 16:10) are not.
inline constexpr float kPhoneAspect = 1.9f;
inline constexpr int kPhoneZoomPercent = 175;
inline constexpr int kTabletZoomPercent = 125;
// The original's height, which E1's view keeps and the zoom divides.
inline constexpr float kUnzoomedHeight = 768.0f;

// --- The message rule (E24's rooms, E25's zoom, E26's frame) ---------------------------------

// MessageManager's lines start 10 px in (setupScene.as:333, (10, 70)).
inline constexpr float kMessageInset = 10.0f;
// The widest a message line may be: tests/data/l10n_rooms.json's cap for
// every message and help sign a touch screen draws. E24 gave them 888 px, to
// the pause button on the original's 1024; E25's zoom brought the button in
// to 795 px from a message's start on a 4:3 tablet at 113%, and this is that
// less the gap.
inline constexpr float kMessageRoom = 785.0f;
// What a message keeps clear past its room.
inline constexpr float kMessageGap = 10.0f;
// E16's pause button at the screen's top right, in the manifest's px (a
// screen 768 tall): 20 in from the edge and 96 wide (TouchControls'
// DefaultManifest; the touch suite checks the two agree).
inline constexpr float kPauseColumn = 116.0f;

// --- E26: the HUD's safe frame ---------------------------------------------------------------

// settings.json "edgeMargin": automatic (negative), or a percentage from 0 to 8.
inline constexpr float kEdgeMarginAuto = -1.0f;
inline constexpr float kEdgeMarginMaxPercent = 8.0f;
// Automatic: a curved phone's glass bends away over about 3-4% of its short
// side (the top, on its side) and its rounded corners take as much of its long
// one; a tablet's corners are rounded less. 3.5% leaves a 20:9 phone its 175%
// zoom step (the message rule).
inline constexpr float kPhoneEdgeMarginPercent = 3.5f;
inline constexpr float kTabletEdgeMarginPercent = 1.0f;

// How far in from each edge of a level's logical screen its HUD is drawn,
// logical px (all 0: at the edges, as the original).
struct HudFrame {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    bool IsZero() const { return left == 0.0f && top == 0.0f && right == 0.0f && bottom == 0.0f; }
    bool operator==(const HudFrame& other) const = default;
};

// A settings.json value, held: negative is automatic (kEdgeMarginAuto),
// otherwise 0..kEdgeMarginMaxPercent.
float ClampEdgeMarginSetting(float percent);
// The margin asked for, in percent: 0 without the touch controls, whatever
// the setting says; with them, the setting, automatic being
// kPhoneEdgeMarginPercent on a phone-shaped window and kTabletEdgeMarginPercent
// on any other.
float EdgeMarginPercent(float setting, bool touch, glm::uvec2 window);
// The frame a level's screen (logical px) gets in a window (image px), the
// screen fitted to it as CameraRig does: at each side the safe area's inset
// or `marginPercent` of the window's width, whichever is more, at the top its
// inset or that percent of the height, at the bottom its inset; less what the
// bars already keep clear, in the screen's logical px.
HudFrame ComputeHudFrame(glm::uvec2 window, glm::vec2 screen, const Supersonic::SafeAreaInsets& safe,
                         float marginPercent);
// A message line's clearance on a screen with a frame: from where it starts
// (kMessageInset past the frame's left) to the pause button's column
// (kPauseColumn at the screen's unit, screen.y / 768, in from the frame's
// right). Every line is counted as if it met the button, which a zoom past
// about 185% lifts above the first row.
float MessageClearance(glm::vec2 screen, const HudFrame& frame);
// The margin a window takes: `percent`, no more than leaves a message line
// kMessageRoom + kMessageGap on E1's unzoomed screen (a 4:3 tablet takes up
// to 5%). The safe area's insets are the display's and are never reduced.
float FittedEdgeMargin(float percent, glm::uvec2 window, bool widescreen, const Supersonic::SafeAreaInsets& safe);

// --- The zoom's rules ------------------------------------------------------------------------

// Whether a window of this size is phone-shaped (its long side over its short).
bool IsPhoneShaped(glm::uvec2 window);
// A settings.json value, clamped: 0 (automatic) or 100..200.
int ClampZoomSetting(int percent);
// The zoom asked for, in percent: 100 without the touch controls; with them,
// the setting, automatic being kPhoneZoomPercent on a phone-shaped window and
// kTabletZoomPercent on any other.
int ZoomPercent(int setting, bool touch, glm::uvec2 window);
// The largest zoom (a factor, at least 1) at which a message line keeps
// kMessageRoom + kMessageGap (MessageClearance) with the frame the window,
// its safe area and the margin give the zoomed screen. And the same in whole
// percent, rounded down: the options screen offers no step above it.
float MaxZoom(glm::uvec2 window, bool widescreen, const Supersonic::SafeAreaInsets& safe = {},
              float marginPercent = 0.0f);
int MaxZoomPercent(glm::uvec2 window, bool widescreen, const Supersonic::SafeAreaInsets& safe = {},
                   float marginPercent = 0.0f);
// The zoom a campaign level is drawn at: ZoomPercent / 100, no more than MaxZoom.
float CampaignZoom(int setting, bool touch, glm::uvec2 window, bool widescreen,
                   const Supersonic::SafeAreaInsets& safe = {}, float marginPercent = 0.0f);
// level1.esc, level2.esc, level3.esc and checkpoint.esc (scenes/...).
bool IsCampaignScene(const std::string& sceneFile);
// The logical screen at a zoom: E1's (768 tall, as wide as the window's shape
// and never narrower than 1024; 1024x768 with widescreen off) divided by it,
// whole pixels. A zoom of 1 is E1's screen exactly.
glm::vec2 ZoomedScreen(glm::uvec2 window, bool widescreen, float zoom);
// After a frame of a campaign scene: whether its zoomed screen (shorter than
// 768) gives way to E1's for the rest of the scene - once the campaign is
// finished (Script::g_gameFinished: the end screen), or while player 2's
// princess (princess.ent) exists. A scene loaded next is zoomed again.
bool CampaignUnzooms(const std::string& sceneFile, glm::vec2 screen, bool gameFinished, bool princess);

// --- The menu, larger ------------------------------------------------------------------------

// What of menu.esc a phone's menu must show whole, logical px: from the
// demons' left and the logo's glow to the panel's left edge (menu.as:217,
// 1024 x 0.618) and below the Quit button's foot (583.5). Every language's
// buttons end left of the panel (the Arabic ones reach x 630), and
// arena_select.esc's thumbnails lie inside it.
inline constexpr glm::vec2 kMenuFocusMin{30.0f, 20.0f};
inline constexpr glm::vec2 kMenuFocusMax{632.8f, 600.0f};
// menu.as:217: the panel's left edge, 1024 - 1024 x (1 - 0.618).
inline constexpr float kMenuPanelLeft = 632.8f;
// The frame is used only when it draws the menu at least this much larger
// than E1's view does.
inline constexpr float kMenuMinGain = 1.1f;
// The panel's text: at most this many times as large as E1's view draws it.
inline constexpr float kMenuMaxTextGain = 1.6f;

// menu.esc and arena_select.esc (which shares its panel).
bool IsPhoneMenuScene(const std::string& sceneFile);

// Where a phone's menu puts the 1024x768 screen in the image.
struct MenuFrame {
    bool active = false;
    float scale = 1.0f;          // image px per logical px
    glm::vec2 viewportMin{0.0f}; // the screen's (0, 0) in image px, whole; may be off the image
    float baseScale = 1.0f;      // E1's view's scale for this window: the screen fitted to it
};

// The frame for a window (image px), the focus box inside the safe area
// (image px: a notch at the left, a bar or the home indicator at the bottom,
// a camera at the top); inactive where it would not gain kMenuMinGain, or
// would leave the panel narrower than E1's view does.
MenuFrame ComputeMenuFrame(glm::uvec2 window, const Supersonic::SafeAreaInsets& safe = {});

// The side panel's text box on a phone's menu, logical px, and how far its
// text may scale (Eth::TextFit): showData sets its title 10 px in and 20 down
// from where the window's safe area shows the panel, and the box ends 10 px
// short of what it shows of it at the right and the bottom (the safe area's
// right and bottom insets, image px, kept out), and of a corner button's
// column (its left edge, logical px; 0 when none shows).
struct MenuPanel {
    glm::vec2 min{0.0f};
    glm::vec2 max{0.0f};
    // The part of the 1024x768 screen the window's safe area shows
    // (loadingMessage's corner, the gamepad icons').
    glm::vec2 shownMin{0.0f};
    glm::vec2 shownMax{0.0f};
    float minScale = 1.0f;       // the text as large as E1's view draws it
    float maxScale = 1.0f;
};
MenuPanel ComputeMenuPanel(const MenuFrame& frame, glm::uvec2 window, const Supersonic::SafeAreaInsets& safe = {},
                           float cornerLeft = 0.0f);

} // namespace Penumbra::Render
