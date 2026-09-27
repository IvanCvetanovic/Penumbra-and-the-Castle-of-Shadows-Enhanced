// videoModes.as, ported line by line: the options screen (video mode list,
// pixel shader, window and second-player input switches). The original is
// extracted/app/videoModes.as, part of Penumbra (Andre Santee, 2010), free
// software under the GNU Lesser General Public License, version 3 or (at your
// option) any later version.

#include "script/Script.hpp"

namespace Penumbra::Script {

// Script globals: never saved, so every launch starts at row 0 of each
// (docs/spec/90-synthesis.md, persistence). The labels of g_controls are never
// drawn: Switch::put shows only the brackets for an image switch.
Switch g_enablePS("Ativa pixel shaders", "Desativa pixel shaders");   // videoModes.as:43
Switch g_windowed("Janela", "Tela-cheia");                            // videoModes.as:44
Switch g_controls("2\xBA joystick para jogador 2", "interface/input_options1.png", "1\xBA joystick para jogador 1", "interface/input_options2.png");   // videoModes.as:45

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

    showToggleFullscreenMessage();                                    // videoModes.as:131
    waitForInputToMenu();

    if(putBackButton(vector2(500, 40)))
        goToMenu();
}

} // namespace Penumbra::Script
