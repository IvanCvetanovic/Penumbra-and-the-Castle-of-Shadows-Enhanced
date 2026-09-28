// videoModes.as, ported line by line: the options screen (video mode list,
// pixel shader, window and second-player input switches). The original is
// extracted/app/videoModes.as, part of Penumbra (Andre Santee, 2010), free
// software under the GNU Lesser General Public License, version 3 or (at your
// option) any later version.
// Enhancement E10 adds the enhanced settings' rows; each of its lines is marked.   // E10

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
// The languages are named in their own tongue in both, so a player who cannot  // E10
// read the current one still finds theirs.                                     // E10
Switch g_keyboardP2("Teclado para o jogador 2", "Jogador 2 s\xF3 no joystick");   // E10
Switch g_widescreen("Tela larga (widescreen)", "Tela 4:3 (original)");         // E10
Switch g_language("Portugu\xEAs", "English");                                  // E10
Stepper g_musicVolume("Volume da m\xFAsica", 10, 10);                           // E10: 100%, the settings' default
Stepper g_effectsVolume("Volume dos efeitos", 10, 10);                          // E10
// E8's switch, worded as the original's own on/off row (g_enablePS).           // E10
Switch g_smoothMotion("Ativa movimento suave", "Desativa movimento suave");     // E10
// E13's automatic pause (settings.pauseOnFocusLoss), beside E8's.             // E13
Switch g_pauseOnFocusLoss("Pausa ao perder o foco", "Continua sem o foco");      // E13

// videoModes.as:47
string videoModeToString(const videoMode& vm)
{
    return ""+Str(vm.width)+"x"+Str(vm.height)+"x"+Str(vm.format == PF32BIT ? 32 : 16);
}

// videoModes.as:52
void screenModesPreLoop()
{
    loopMenuSong();
    LoadSprite("interface/arrow_button.png");
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
    for (uint t=0; t<GetVideoModeCount(); t++)
    {
        // Only 32-bit modes of at least 800x600 are offered; every refresh rate
        // of one size is listed again (videoModes.as:99-101).
        const videoMode mode = GetVideoMode(t);
        if (mode.format != PF32BIT || mode.width < 800 || mode.height < 600)
            continue;

        uint8 alpha = 100;
        if (mousePos.x > cursor.x && mousePos.x < cursor.x+textWidth
            && mousePos.y > cursor.y && mousePos.y < cursor.y+fontSize)
        {
            alpha = 255;
            if (getConfirmButtonStatus(0) == KS_HIT)
            {
                SetWindowProperties(APPLICATION_TITLE, mode.width, mode.height, Windowed(), true, PF32BIT);   // videoModes.as:110
            }
        }

        shadowText(cursor, videoModeToString(mode), "Arial Narrow", fontSize, alpha, 203,203,228);

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

    g_windowed.put(vector2(255, origin.y+70), "Arial Narrow", fontSize, 256);
    g_controls.put(vector2(255, origin.y+160), "Arial Narrow", fontSize, 256);

    // ENHANCEMENT E10: the enhanced settings, in the same column below          // E10
    // g_controls, whose two 72 px images end at y 404: switches 20 px apart as  // E10
    // g_enablePS and g_windowed are, then the steppers, one 25 px row each,     // E10
    // then E8's switch 10 px below them. Below 744 only the Alt+Enter line      // E10
    // (y 753); x 520 on is free at these y.                                     // E10
    g_keyboardP2.put(vector2(255, origin.y+324), "Arial Narrow", fontSize, 256);   // E10: y 424-474
    g_widescreen.put(vector2(255, origin.y+394), "Arial Narrow", fontSize, 256);   // E10: y 494-544
    // The menus stay 4:3 (pillarboxed), so the view shows only in a level.     // E10
    shadowText(vector2(520, origin.y+411), "Vale a partir da pr\xF3xima fase", "Arial Narrow", 15.0f, 150, 203,203,228);   // E10
    g_language.put(vector2(255, origin.y+464), "Arial Narrow", fontSize, 256);     // E10: y 564-614
    g_musicVolume.put(vector2(255, origin.y+534), "Arial Narrow", fontSize, 180);  // E10: y 634-659
    g_effectsVolume.put(vector2(255, origin.y+559), "Arial Narrow", fontSize, 180);   // E10: y 659-684
    g_smoothMotion.put(vector2(255, origin.y+594), "Arial Narrow", fontSize, 256);    // E10: y 694-744
    g_pauseOnFocusLoss.put(vector2(540, origin.y+594), "Arial Narrow", fontSize, 256);   // E13: x 540-796, y 694-744

    showToggleFullscreenMessage();                                    // videoModes.as:131
    waitForInputToMenu();

    if(putBackButton(vector2(500, 40)))
        goToMenu();
}

} // namespace Penumbra::Script
