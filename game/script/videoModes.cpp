// videoModes.as, ported line by line: the options screen (video mode list,
// pixel shader, window and second-player input switches). The original is
// extracted/app/videoModes.as, part of Penumbra (Andre Santee, 2010), free
// software under the GNU Lesser General Public License, version 3 or (at your
// option) any later version.
// Enhancement E10 adds the enhanced settings' rows; each of its lines is marked.   // E10
// Enhancement E23 adds the automatic display mode and the refresh rate; marked.  // E23
// Enhancement E24 makes the language row a chooser of eleven; marked.            // E24

#include "script/Script.hpp"

namespace Penumbra::Script {

// Script globals: never saved, so every launch starts at row 0 of each
// (docs/spec/90-synthesis.md, persistence). The labels of g_controls are never
// drawn: Switch::put shows only the brackets for an image switch.
Switch g_enablePS("Ativa pixel shaders", "Desativa pixel shaders");   // videoModes.as:43
Switch g_windowed("Janela", "Tela-cheia");                            // videoModes.as:44
Switch g_controls("2\xBA joystick para jogador 2", "interface/input_options1.png", "1\xBA joystick para jogador 1", "interface/input_options2.png");   // videoModes.as:45

// ENHANCEMENT E10 (Script.hpp): the enhanced settings' rows, labelled in the   // E10
// script's Portuguese like the rows above (strings.json has their English).    // E10
// Row 0 of each Switch is the settings' default; the layer seeds all six.      // E10
Switch g_keyboardP2("Teclado para o jogador 2", "Jogador 2 s\xF3 no joystick");   // E10
Switch g_widescreen("Tela larga (widescreen)", "Tela 4:3 (original)");         // E10
// E24: the language, a chooser whose options the layer gives it: every         // E24
// language named in its own tongue, whatever the current one, so a player who  // E24
// cannot read the current one still finds theirs.                              // E24
Chooser g_language("Idioma");                                                   // E24
Stepper g_musicVolume("Volume da m\xFAsica", 10, 10);                           // E10: 100%, the settings' default
Stepper g_effectsVolume("Volume dos efeitos", 10, 10);                          // E10
// E8's switch, worded as the original's own on/off row (g_enablePS).           // E10
Switch g_smoothMotion("Ativa movimento suave", "Desativa movimento suave");     // E10
// E13's automatic pause (settings.pauseOnFocusLoss), beside E8's.             // E13
Switch g_pauseOnFocusLoss("Pausa ao perder o foco", "Continua sem o foco");      // E13
// E20 (Script.hpp): the phone's layout, and E16's touch controls on or off,    // E20
// worded as the original's own on/off row (g_enablePS).                         // E20
bool g_mobileLayout = false;                                                     // E20
Switch g_touchControls("Ativa controles de toque", "Desativa controles de toque");   // E20
// E23 (Script.hpp): what the mode list marks, and the refresh rate's row, all   // E23
// the layer's to set. Left as they are here (no layer: the suites), the list   // E23
// marks its automatic line and nothing as native, and the row has no options.  // E23
videoMode g_chosenVideoMode{0, 0, PF32BIT};                                      // E23
videoMode g_nativeVideoMode{0, 0, PF32BIT};                                      // E23
Chooser g_refreshRate("Taxa de atualiza\xE7\xE3o");                               // E23
bool g_refreshRateRow = true;                                                    // E23: the layer lowers it where no rate can be set
// E25 (Script.hpp): the zoom's row on a phone, and the menu panel's text box.   // E25
Chooser g_zoom("Zoom");                                                          // E25
PhonePanel g_phonePanel;                                                         // E25
vector2 g_joystickIconsMin{0.0f};                                                // E25
vector2 g_joystickIconsMax{0.0f};                                                // E25

namespace {                                                                      // E23
// The refresh rate's row: at x 540, beside the window switch (x 255, y 170-220), // E23
// in the room between the back arrow (to y 132) and the input images (from y    // E23
// 260); the value box holds "Autom\xE1tica (m\xE1xima)" and "Automatic (165 Hz)"   // E23
// with room to spare (test_pn_render_hud measures them).                         // E23
constexpr float kRefreshRateX = 540.0f;                                          // E23
constexpr float kRefreshRateValueWidth = 200.0f;                                 // E23
// E24: the language's row keeps E10's switch's 256 px: "[<]" 40, the name 176,  // E24
// "[>]" 40 (test_pn_render_hud measures every name in the box).                  // E24
constexpr float kLanguageValueWidth = 176.0f;                                    // E24
// E25: the zoom's row on a phone, in the refresh rate's column and value box,  // E25
// beside g_keyboardP2's switch (x 255-511, y 424-474): below the input images  // E25
// (to y 404) and above E10's hint "Vale a partir da pr\xF3xima fase" (y 511),  // E25
// which says when it applies too.                                               // E25
constexpr float kZoomX = kRefreshRateX;                                          // E25
constexpr float kZoomValueWidth = kRefreshRateValueWidth;                        // E25
} // namespace                                                                   // E23

// ENHANCEMENT E27 (Script.hpp, optionsArt.cpp): the options screen on a stone-frame panel, in     // E27
// groups with icons. Every row keeps its place and its box (the rooms in tests/data/l10n_rooms.json  // E27
// still hold); the frame, the rules between the groups and the icons are drawn under and beside     // E27
// them, and only when the added art is loaded: without it the screen is the original's.             // E27
namespace {                                                                                          // E27
constexpr float kPanelX = 242.0f;                                                                    // E27: a little left of the rows (x 255)
constexpr float kPanelY = 92.0f;                                                                     // E27: above the first row (y 100)
constexpr float kPanelWidth = 680.0f;                                                                // E27: to x 922, past the refresh row's last box (x 820) and the longest right-hand label (Italian, x 879)
constexpr float kPanelBottom = 750.0f;                                                               // E27: above the Alt+Enter line (y 753)
constexpr float kPanelAlpha = 232.0f;                                                                // E27
constexpr float kPanelCorner = 13.0f;                                                                // E27: the stone's border, half of the art's 26
constexpr float kRowMark = 20.0f;                                                                    // E27: a mode list row's check box
constexpr float kRowMarkWidth = 24.0f;                                                               // E27: where its text starts: the brackets' own width, the list's column is tight (200 px)
// The gaps between the groups of rows (y 237-260, 474-494, 544-564, 614-634, 684-694).               // E27
constexpr float kRuleY[] = {248.0f, 484.0f, 554.0f, 624.0f, 689.0f};                                 // E27

// A rule across the panel, fading out at both ends.                                                // E27
void drawRule(const float y)                                                                        // E27
{                                                                                                    // E27
    const float x0 = kPanelX+22.0f;                                                                  // E27
    const float half = (kPanelWidth-44.0f)*0.5f;                                                     // E27
    const uint clear = ARGB(0, 203, 203, 228);                                                       // E27
    const uint line = ARGB(110, 203, 203, 228);                                                      // E27
    DrawRectangle(vector2(x0, y), vector2(half, 2.0f), clear, line, clear, line);                    // E27
    DrawRectangle(vector2(x0+half, y), vector2(half, 2.0f), line, clear, line, clear);               // E27
}                                                                                                    // E27

// The frame under the rows: the mode list's panel (its height from its lines), the rows' panel,      // E27
// the rules and each group's icon, drawn first so the rows come over them.                          // E27
void drawOptionsFrame(const uint listLines)                                                         // E27
{                                                                                                    // E27
    if (!optionsArtReady())                                                                          // E27
        return;                                                                                      // E27
    const uint8 alpha = static_cast<uint8>(kPanelAlpha);                                             // E27
    if (listLines > 0)                                                                               // E27
        drawPanel(vector2(22.0f, kPanelY), vector2(216.0f, min(16.0f+25.0f*static_cast<float>(listLines), kPanelBottom-kPanelY)),
                  alpha, kPanelCorner);                                                              // E27
    drawPanel(vector2(kPanelX, kPanelY), vector2(kPanelWidth, kPanelBottom-kPanelY), alpha, kPanelCorner);   // E27
    for (const float y : kRuleY)                                                                     // E27
        drawRule(y);                                                                                 // E27
    // One icon to a group, at the right where the rows leave room: the display's, the players'       // E27
    // input, the language's (beside its chooser, the cue a player who cannot read the screen looks     // E27
    // for) and the audio's (a note and a speaker on their rows).                                      // E27
    drawOptionsIcon("monitor", vector2(864.0f, 102.0f), 40.0f, 230);                                  // E27
    drawOptionsIcon("pad", vector2(850.0f, 318.0f), 56.0f, 230);                                      // E27
    drawOptionsIcon("globe", vector2(530.0f, 564.0f), 46.0f, 255);                                    // E27
    drawOptionsIcon("music", vector2(600.0f, 634.0f), 24.0f, 230);                                    // E27
    drawOptionsIcon("speaker", vector2(600.0f, 659.0f), 24.0f, 230);                                  // E27
}                                                                                                    // E27

// A line of the mode list: the original's "[x] label", or a check box and the label.                 // E27
void putModeRow(const vector2& pos, const bool current, const string& label, const uint8 alpha, const float fontSize)   // E27
{                                                                                                    // E27
    if (optionsArtReady())                                                                           // E27
    {                                                                                                // E27
        drawOptionsIcon(current ? "check_on" : "check_off", pos+vector2(0.0f, (fontSize-kRowMark)*0.5f), kRowMark, alpha);   // E27
        shadowText(pos+vector2(kRowMarkWidth, 0.0f), label, "Arial Narrow", fontSize, alpha, 203,203,228);   // E27
        return;                                                                                      // E27
    }                                                                                                // E27
    const string mark = string("[") + (current ? "\x95" : " ") + "] ";                               // E27
    shadowText(pos, mark+label, "Arial Narrow", fontSize, alpha, 203,203,228);                       // E27
}                                                                                                    // E27
} // namespace                                                                                       // E27

// videoModes.as:47
string videoModeToString(const videoMode& vm)
{
    return ""+Str(vm.width)+"x"+Str(vm.height)+"x"+Str(vm.format == PF32BIT ? 32 : 16);
}

// E23 (Script.hpp): a line of the mode list without videoModeToString's bit
// depth, and the monitor's native size named as such.
string videoModeLabel(const videoMode& vm)                            // E23
{                                                                     // E23
    string label = Str(vm.width)+"x"+Str(vm.height);                  // E23
    if (vm.width == g_nativeVideoMode.width && vm.height == g_nativeVideoMode.height)   // E23
        label += " (nativa)";                                         // E23
    return label;                                                     // E23
}                                                                     // E23

// videoModes.as:52
void screenModesPreLoop()
{
    loopMenuSong();
    LoadSprite("interface/arrow_button.png");
    loadOptionsArt();                                                 // E27
}

// videoModes.as:58. The picker reads the cursor after requesting the move (the
// menu cursor reads it before, menu.as:238-240), but SetCursorPos only records
// the request and the next input update applies it (docs/spec/30 section 4.1;
// gs2d-r485 Input/Win/gs2dWinInput.cpp:434-446), so both lag one frame.
void ETHCallback_picker(ETHEntity thisEntity)
{
    InputState& input = GetInputHandle();                             // videoModes.as:60
    input.SetCursorPos(input.GetCursorAbsolutePos()+getPlayerXYAxis(0)*5.0f);
    thisEntity->SetPositionXY(input.GetCursorPos());
}

// videoModes.as:65. Draws the back arrow at `cursor`; true on a confirm while
// the mouse is strictly inside it.
bool putBackButton(const vector2& cursor)
{
    bool r = false;
    InputState& input = GetInputHandle();                             // videoModes.as:68
    const vector2 mouseCursor = input.GetCursorPos();
    const vector2 spriteSize = GetSpriteSize("interface/arrow_button.png");
    uint8 alpha = 100;
    if (mouseCursor.x > cursor.x && mouseCursor.y > cursor.y
        && mouseCursor.x < cursor.x+spriteSize.x && mouseCursor.y < cursor.y+spriteSize.y)
    {
        alpha = 255;
        if (getConfirmButtonStatus(0) == KS_HIT)
        {
            r = true;
        }
    }
    DrawSprite("interface/arrow_button.png", cursor, ARGB(alpha, 203,203,228));
    return r;
}

// videoModes.as:85
void screenModesLoop()
{
    const vector2 titlePos(30,30);
    const vector2 origin(30,100);
    shadowText(titlePos, "Op\xE7\xF5" "es de v\xED" "deo", "Arial Narrow", 40.0f, 255,203,203,228);   // videoModes.as:89

    InputState& input = GetInputHandle();                             // videoModes.as:91
    const vector2 mousePos(input.GetCursorPos());
    const vector2 cursorOrigin = origin;
    vector2 cursor = cursorOrigin;
    const float fontSize = 25;
    const float textWidth = 200.0f;
    // E20: a phone's screen has one mode, the one it is in.                     // E20
    const uint videoModeCount = g_mobileLayout ? 0u : GetVideoModeCount();       // E20

    // E27: the frame under the rows, first so they draw over it; the mode list's panel is as   // E27
    // tall as the lines the loop below makes (the automatic one and each 32-bit mode of at      // E27
    // least 800x600).                                                                           // E27
    uint listLines = g_mobileLayout ? 0u : 1u;                                   // E27
    for (uint t=0; t<videoModeCount; t++)                                        // E27
    {                                                                            // E27
        const videoMode mode = GetVideoMode(t);                                  // E27
        if (mode.format == PF32BIT && mode.width >= 800 && mode.height >= 600)   // E27
            listLines++;                                                         // E27
    }                                                                            // E27
    drawOptionsFrame(listLines);                                                 // E27

    // E23: the list's first line, the automatic mode, hit and drawn as the      // E23
    // lines below it are; 0 x 0 asks the layer for "automatic" in whichever of   // E23
    // window and fullscreen the game is in. The current choice is marked on     // E23
    // every line as a Switch row marks its own (switch.as:76) and drawn at 255.  // E23
    if (!g_mobileLayout)                                                         // E23
    {                                                                            // E23
        const bool current = g_chosenVideoMode.width == 0 && g_chosenVideoMode.height == 0;   // E23
        uint8 alpha = static_cast<uint8>(current ? 255 : 100);                   // E23
        if (mousePos.x > cursor.x && mousePos.x < cursor.x+textWidth             // E23
            && mousePos.y > cursor.y && mousePos.y < cursor.y+fontSize)          // E23
        {                                                                        // E23
            alpha = 255;                                                         // E23
            if (getConfirmButtonStatus(0) == KS_HIT)                             // E23
            {                                                                    // E23
                SetWindowProperties(APPLICATION_TITLE, 0, 0, Windowed(), true, PF32BIT);   // E23
            }                                                                    // E23
        }                                                                        // E23
        putModeRow(cursor, current, "Autom\xE1tico (melhor)", alpha, fontSize);   // E23, E27: putModeRow draws "[x] label" or a check box and the label
        cursor.y += fontSize;                                                    // E23
    }                                                                            // E23

    for (uint t=0; t<videoModeCount; t++)                                        // E20: GetVideoModeCount() (videoModes.as:97)
    {
        // Only 32-bit modes of at least 800x600 are offered; every refresh rate
        // of one size is listed again (videoModes.as:99-101).
        const videoMode mode = GetVideoMode(t);
        if (mode.format != PF32BIT || mode.width < 800 || mode.height < 600)
            continue;

        // E23: the current choice drawn at 255, as a Switch's current row is.   // E23
        const bool current = mode.width == g_chosenVideoMode.width && mode.height == g_chosenVideoMode.height;   // E23
        uint8 alpha = static_cast<uint8>(current ? 255 : 100);                  // E23: 100 (videoModes.as:104)
        if (mousePos.x > cursor.x && mousePos.x < cursor.x+textWidth
            && mousePos.y > cursor.y && mousePos.y < cursor.y+fontSize)
        {
            alpha = 255;
            if (getConfirmButtonStatus(0) == KS_HIT)
            {
                SetWindowProperties(APPLICATION_TITLE, mode.width, mode.height, Windowed(), true, PF32BIT);   // videoModes.as:110
            }
        }

        // E23: "[x] WxH" rather than videoModeToString's "WxHx32".            // E23
        putModeRow(cursor, current, videoModeLabel(mode), alpha, fontSize);   // E23: videoModeToString(mode); E27: putModeRow

        // Past the bottom of the screen the list continues in a new column
        // (videoModes.as:116-121).
        cursor.y += fontSize;
        if (cursor.y > GetScreenSize().y)
        {
            cursor.x += textWidth;
            cursor.y = cursorOrigin.y;
        }
    }

    // adjust pixel shader states
    g_enablePS.put(vector2(255, origin.y), "Arial Narrow", fontSize, 256);   // videoModes.as:125
    UsePixelShaders(g_enablePS.getCurrent() == 0);

    // E20: on a phone, E16's touch controls in the window switch's place:      // E20
    // there is no window to switch, and the rows are the same two 25 px lines.  // E20
    if (g_mobileLayout)                                                          // E20
        g_touchControls.put(vector2(255, origin.y+70), "Arial Narrow", fontSize, 256);   // E20: y 170-220
    else                                                                         // E20
        g_windowed.put(vector2(255, origin.y+70), "Arial Narrow", fontSize, 256);
    g_controls.put(vector2(255, origin.y+160), "Arial Narrow", fontSize, 256);

    // E23: the refresh rate, beside the window switch on the desktop and the    // E23
    // touch controls' row on a phone: the label at y 170, "[<] value [>]" at    // E23
    // 195-220. In a window the rate is the compositor's, and a pick waits for   // E23
    // the next fullscreen; the hint says so, as E10's widescreen hint does.     // E23
    if (g_refreshRateRow)                                                        // E23
        g_refreshRate.put(vector2(kRefreshRateX, origin.y+70), "Arial Narrow", fontSize, kRefreshRateValueWidth);   // E23: y 170-220
    if (g_refreshRateRow && !g_mobileLayout && Windowed())                       // E23
        shadowText(vector2(kRefreshRateX, origin.y+122), "Vale para a tela cheia", "Arial Narrow", 15.0f, 150, 203,203,228);   // E23: y 222-237

    // ENHANCEMENT E10: the enhanced settings, in the same column below          // E10
    // g_controls, whose two 72 px images end at y 404: switches 20 px apart as  // E10
    // g_enablePS and g_windowed are, then the steppers, one 25 px row each,     // E10
    // then E8's switch 10 px below them. Below 744 only the Alt+Enter line      // E10
    // (y 753); x 520 on is free at these y.                                     // E10
    g_keyboardP2.put(vector2(255, origin.y+324), "Arial Narrow", fontSize, 256);   // E10: y 424-474
    if (g_mobileLayout)                                                          // E25
        g_zoom.put(vector2(kZoomX, origin.y+324), "Arial Narrow", fontSize, kZoomValueWidth);   // E25: y 424-474
    g_widescreen.put(vector2(255, origin.y+394), "Arial Narrow", fontSize, 256);   // E10: y 494-544
    // The view applies from the next scene: the next level, and the menus      // E10
    // (render/WideMenus.hpp), whose world goes on past their sides.            // E10
    shadowText(vector2(520, origin.y+411), "Vale a partir da pr\xF3xima fase", "Arial Narrow", 15.0f, 150, 203,203,228);   // E10
    g_language.put(vector2(255, origin.y+464), "Arial Narrow", fontSize, kLanguageValueWidth);   // E24: y 564-614, E10's switch's place
    g_musicVolume.put(vector2(255, origin.y+534), "Arial Narrow", fontSize, 180);  // E10: y 634-659
    g_effectsVolume.put(vector2(255, origin.y+559), "Arial Narrow", fontSize, 180);   // E10: y 659-684
    g_smoothMotion.put(vector2(255, origin.y+594), "Arial Narrow", fontSize, 256);    // E10: y 694-744
    g_pauseOnFocusLoss.put(vector2(540, origin.y+594), "Arial Narrow", fontSize, 256);   // E13: x 540-796, y 694-744

    showToggleFullscreenMessage();                                    // videoModes.as:131
    waitForInputToMenu();

    // E27: on the panel's right, out of its way (the original's place, x 500, is over it).
    if(putBackButton(optionsArtReady() ? vector2(906, 6) : vector2(500, 40)))
        goToMenu();
}

} // namespace Penumbra::Script
