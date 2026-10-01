// menu.as, ported line by line: the main menu and the arena select (both driven
// by the cursor entity's callback), the Alt+Enter window toggle, and the ways
// back to the menu. The original is extracted/app/menu.as, part of Penumbra
// (Andre Santee, 2010), free software under the GNU Lesser General Public
// License, version 3 or (at your option) any later version.
//
// The panel texts como_jogar, config, creditos, novo_jogo and versus
// (menu.as:123-215) are defined in Script.hpp, as the bytes AngelScript made of
// the heredocs; they are not repeated here.

#include "script/Script.hpp"

namespace Penumbra::Script {

// ENHANCEMENT E27 (Script.hpp, optionsArt.cpp): the main menu's language button. Every button of    // E27
// the menu is a picture of words in the current language, so a player who cannot read it has no    // E27
// way to find the options, and the language row on them: a globe, which needs no words, opens a    // E27
// list of the languages each in its own script. The list is modal; a row sets the options screen's  // E27
// language chooser (index 0 automatic, then Languages.hpp's order), which the layer applies as it    // E27
// does a pick there. Only when the added art is loaded.                                            // E27
namespace {                                                                                          // E27
bool g_languageListOpen = false;                                                                     // E27: reset by the menu's preloop
constexpr float kGlobeSize = 76.0f;                                                                  // E27
constexpr float kGlobeMargin = 24.0f;                                                                // E27: from the visible corner (above the Alt+Enter line, y 753)
constexpr float kListRow = 32.0f;                                                                    // E27
constexpr float kListWidth = 360.0f;                                                                 // E27
constexpr float kListInset = 14.0f;                                                                  // E27: the rows' room inside the panel's stone

// The part of the menu's screen the window shows: a phone's larger menu shows less to the right    // E27
// (and starts at its safe area); elsewhere it is the logical screen.                                // E27
void shownArea(vector2& lo, vector2& hi)                                                             // E27
{                                                                                                    // E27
    if (g_phonePanel.on)                                                                             // E27
    {                                                                                                // E27
        lo = g_phonePanel.shownMin;                                                                  // E27
        hi = g_phonePanel.shownMax;                                                                  // E27
        return;                                                                                      // E27
    }                                                                                                // E27
    lo = vector2(0.0f, 0.0f);                                                                        // E27
    hi = GetScreenSize();                                                                            // E27
}                                                                                                    // E27

bool inside(const vector2& p, const vector2& lo, const vector2& size)                                // E27
{                                                                                                    // E27
    return p.x > lo.x && p.y > lo.y && p.x < lo.x+size.x && p.y < lo.y+size.y;                       // E27
}                                                                                                    // E27

// The open list, over a dim menu. Returns whether it is open, in which case the rest of the cursor   // E27
// callback is skipped: the buttons below do not answer while a language is being chosen.            // E27
bool languageList()                                                                                  // E27
{                                                                                                    // E27
    if (!g_languageListOpen)                                                                         // E27
        return false;                                                                                // E27
    InputState& input = GetInputHandle();                                                            // E27
    const vector2 cursor = input.GetCursorPos();                                                     // E27
    vector2 lo, hi;                                                                                  // E27
    shownArea(lo, hi);                                                                               // E27
    const uint rows = g_language.getCount();                                                         // E27
    const float height = 78.0f+kListRow*static_cast<float>(rows)+14.0f;                              // E27
    const vector2 size(kListWidth, height);                                                          // E27
    const vector2 pos((lo.x+hi.x-size.x)*0.5f, max(lo.y+10.0f, (lo.y+hi.y-size.y)*0.5f));           // E27

    const uint dim = ARGB(165, 0, 0, 0);                                                             // E27
    DrawRectangle(vector2(0.0f, 0.0f), GetScreenSize(), dim, dim, dim, dim);                         // E27
    drawPanel(pos, size, 246, 13.0f);                                                                // E27
    drawOptionsIcon("globe", vector2(pos.x+(size.x-46.0f)*0.5f, pos.y+16.0f), 46.0f, 255);           // E27

    // The language the game is in: the row the chooser stands on (0 automatic).                      // E27
    const uint current = g_language.getCurrent();                                                    // E27
    int hover = -1;                                                                                  // E27
    for (uint i = 0; i < rows; i++)                                                                  // E27
    {                                                                                                // E27
        const vector2 rowPos(pos.x+kListInset, pos.y+70.0f+kListRow*static_cast<float>(i));          // E27
        const vector2 rowSize(size.x-2.0f*kListInset, kListRow);                                     // E27
        const bool over = inside(cursor, rowPos, rowSize);                                           // E27
        if (over)                                                                                    // E27
            hover = static_cast<int>(i);                                                             // E27
        if (over)                                                                                    // E27
        {                                                                                            // E27
            const uint glow = ARGB(70, 203, 203, 228);                                               // E27
            DrawRectangle(rowPos, rowSize, glow, glow, glow, glow);                                  // E27
        }                                                                                            // E27
        const uint8 alpha = static_cast<uint8>((over || i == current) ? 255 : 190);                  // E27
        drawOptionsIcon((i == current) ? "check_on" : "check_off", rowPos+vector2(6.0f, 5.0f), 22.0f, alpha);   // E27
        shadowText(rowPos+vector2(44.0f, 3.0f), g_language.getOption(i), "Arial Narrow", 25.0f, alpha, 203,203,228);   // E27
    }                                                                                                // E27

    // The menu's own cursor is a light in the world, under everything drawn here: a small mark.      // E27
    const uint shade = ARGB(210, 0, 0, 0);                                                           // E27
    const uint mark = ARGB(240, 255, 255, 255);                                                      // E27
    DrawRectangle(cursor-vector2(7.0f, 7.0f), vector2(14.0f, 14.0f), shade, shade, shade, shade);    // E27
    DrawRectangle(cursor-vector2(5.0f, 5.0f), vector2(10.0f, 10.0f), mark, mark, mark, mark);        // E27

    // A pick sets the chooser and closes the list; a click outside the panel, or cancel, closes it.   // E27
    if (getConfirmButtonStatus(0) == KS_HIT)                                                         // E27
    {                                                                                                // E27
        if (hover >= 0)                                                                              // E27
        {                                                                                            // E27
            g_language.setCurrent(static_cast<uint>(hover));                                         // E27
            g_languageListOpen = false;                                                              // E27
        }                                                                                            // E27
        else if (!inside(cursor, pos, size))                                                         // E27
            g_languageListOpen = false;                                                              // E27
    }                                                                                                // E27
    else if (getCancelButtonStatus(0) == KS_HIT)                                                     // E27
        g_languageListOpen = false;                                                                  // E27
    return true;                                                                                     // E27
}                                                                                                    // E27

// The globe, bottom left of what the window shows: the panels' text (the credits, the best times)    // E27
// runs down the right side, and the buttons end well above (y 600); drawn last, over the picture.    // E27
// A confirm on it opens the list from the next frame.
void languageButton()                                                                                // E27
{                                                                                                    // E27
    if (!optionsArtReady() || GetSceneFileName() != "scenes/menu.esc" || g_languageListOpen)         // E27
        return;                                                                                      // E27
    vector2 lo, hi;                                                                                  // E27
    shownArea(lo, hi);                                                                               // E27
    const vector2 pos(lo.x+kGlobeMargin, hi.y-kGlobeMargin-kGlobeSize);                              // E27
    const vector2 cursor = GetInputHandle().GetCursorPos();                                          // E27
    const bool over = inside(cursor, pos, vector2(kGlobeSize, kGlobeSize));                          // E27
    drawOptionsIcon("globe_button", pos, kGlobeSize, over ? 255 : 225);                              // E27
    if (over && getConfirmButtonStatus(0) == KS_HIT)                                                 // E27
        g_languageListOpen = true;                                                                   // E27
}                                                                                                    // E27
} // namespace                                                                                       // E27

// menu.as:43
void loopMenuSong()
{
    LoadMusic("soundfx/menu.mp3");
    PlaySample("soundfx/menu.mp3");
    LoopSample("soundfx/menu.mp3", true);
}

// menu.as:50. The preLoop of menu.esc AND arena_select.esc.
void menuPreLoop()
{
    loopMenuSong();

    LoadSoundEffect("soundfx/help.mp3");
    SetSampleVolume("soundfx/help.mp3", 0.3f);

    LoadSoundEffect("soundfx/newgame.mp3");
    LoadSoundEffect("soundfx/fail.ogg");

    LoadSprite("interface/joystick.png");
    loadOptionsArt();                                                 // E27: the language button's and list's art
    g_languageListOpen = false;                                       // E27: a scene load closes the list

    SetBorderBucketsDrawing(false);

    // A full-screen black rectangle, so the scene's first frame is black.
    drawRect(0xFF000000);                                             // menu.as:64

    ETHEntity cursor = SeekEntity("cursor.ent");                      // menu.as:66
    if (cursor != nullptr)
    {
        cursor->AddStringData("lastButton", "none");
        cursor->AddUIntData("menuStartTime", GetTime());
    }
}

// menu.as:74. Re-enumerates the joysticks EVERY call and marks each detected one
// with an icon at the top right.
void detectJoysticks()
{
    const vector2 screenSize = GetScreenSize();
    const vector2 joySpriteSize = GetSpriteSize("interface/joystick.png");
    InputState& input = GetInputHandle();                             // menu.as:78
    input.DetectJoysticks();
    // E25: on a phone's larger menu the screen's right edge is in the middle  // E25
    // of the panel's text: the icons go to the top edge of what the window    // E25
    // shows of the panel, in a row from the corner the text's lines leave     // E25
    // free (the right, or the left for a right-to-left language), at half the // E25
    // size E1's view draws them - above the body's first line - and showData  // E25
    // keeps its text out of their box.                                        // E25
    g_joystickIconsMin = g_joystickIconsMax = vector2(0,0);                     // E25
    if (g_phonePanel.on)                                                         // E25
    {                                                                            // E25
        const vector2 iconSize = joySpriteSize*(g_phonePanel.minScale*0.5f);     // E25
        for (uint t = 0; t < 2; t++)                                             // E25
        {                                                                        // E25
            if (input.GetJoystickStatus(t) != JS_DETECTED)                       // E25
                continue;                                                        // E25
            const float slot = iconSize.x*static_cast<float>(t);                 // E25
            const vector2 iconPos(g_phonePanel.rightToLeft ? g_phonePanel.min.x+slot   // E25
                                                           : g_phonePanel.max.x-iconSize.x-slot,   // E25
                                  g_phonePanel.shownMin.y);                      // E25
            DrawShapedSprite("interface/joystick.png", iconPos, iconSize, ARGB(150,255,255,255));   // E25
            // The row's box: the icons share their top.                        // E25
            if (g_joystickIconsMax.x <= g_joystickIconsMin.x)                    // E25
                g_joystickIconsMin = g_joystickIconsMax = iconPos;               // E25
            g_joystickIconsMin.x = min(g_joystickIconsMin.x, iconPos.x);         // E25
            g_joystickIconsMax = vector2(max(g_joystickIconsMax.x, iconPos.x+iconSize.x), iconPos.y+iconSize.y);   // E25
        }                                                                        // E25
        return;                                                                  // E25
    }                                                                            // E25
    // E26: in a level (held J, setupScene.as:393-394) from the HUD frame's corner.  // E26
    if (input.GetJoystickStatus(0) == JS_DETECTED)
    {
        DrawSprite("interface/joystick.png", vector2(screenSize.x-joySpriteSize.x, 0)+hudTopRight(),   // E26: hudTopRight
                   ARGB(150,255,255,255));
    }
    if (input.GetJoystickStatus(1) == JS_DETECTED)
    {
        DrawSprite("interface/joystick.png", vector2(screenSize.x-joySpriteSize.x, joySpriteSize.y)+hudTopRight(),   // E26
                   ARGB(150,255,255,255));
    }
}

// menu.as:92. In menu.esc goToMenu() does nothing (the scene is already the
// menu); this only matters in arena_select.esc.
void menuLoop()
{
    waitForInputToMenu();
}

// menu.as:97
void showToggleFullscreenMessage()
{
    const vector2 screenSize = GetScreenSize();
    InputState& input = GetInputHandle();                             // menu.as:100
    // E20: not on a phone, which has neither the keys nor a window to switch;   // E20
    // the switching below stays (g_windowed's row is not drawn there).          // E20
    if (!g_mobileLayout)                                                         // E20
        shadowText(vector2(0,screenSize.y-15), "Pressione Alt+Enter para trocar entre fullscreen e modo janela", "Arial Narrow", 15.0f, 255,203,203,228);

    // g_windowed's row 0 is "Janela" (windowed): when the switch disagrees with
    // the window, the window follows the switch (menu.as:103-106).
    bool toggle = false;
    if ((Windowed() && g_windowed.getCurrent() == 1)
        || (!Windowed() && g_windowed.getCurrent() == 0))
        toggle = true;

    // && binds tighter than ||, as in AngelScript (menu.as:108).
    if ((input.GetKeyState(K_ALT) == KS_DOWN && input.GetKeyState(K_RETURN) == KS_HIT) || toggle)
    {
        const bool windowed = !Windowed();
        SetWindowProperties(APPLICATION_TITLE, static_cast<uint>(screenSize.x), static_cast<uint>(screenSize.y), windowed, true, PF32BIT);   // menu.as:111
        if (windowed)
        {
            g_windowed.setCurrent(0);
        }
        else
        {
            g_windowed.setCurrent(1);
        }
    }
}

// menu.as:217. The right-hand panel: a vertical gradient over the right
// 1-0.618 of the screen, a title and a body.
void showData(const string& title, const string& content)
{
    const vector2 screenSize = GetScreenSize();
    const vector2 rectSize(screenSize.x*(1-0.618f), screenSize.y);
    const uint rectColor0 = ARGB(190,0,0,0);
    const uint rectColor1 = ARGB(55,0,0,0);
    const vector2 rectPos(screenSize.x-rectSize.x, 0);

    const float textSize = screenSize.y <= 700 ? 20.0f : 25.0f;      // menu.as:225
    DrawRectangle(rectPos, rectSize,
                  rectColor0, rectColor0, rectColor1, rectColor1);
    // E25: on a phone the menu is drawn larger, from the left, and the panel   // E25
    // runs on to the window's right edge: the title and the body are set in    // E25
    // the part of it the window shows (g_phonePanel), 10 px in and 20 down as  // E25
    // above, the body 50 below the title, both scaled by one factor to fill    // E25
    // it (HudCmd::fit) - never smaller than they are drawn without E25.        // E25
    // A long body may be set in two columns, split at one of its blank lines, // E25
    // where that sets the panel larger (How to Play, the credits). No line     // E25
    // crosses the gamepad icons, with 6 px about them (the body's first line   // E25
    // is below them at any size; only a long title meets them).                // E25
    if (g_phonePanel.on)                                                         // E25
    {                                                                            // E25
        TextFit fit;                                                             // E25
        fit.min = g_phonePanel.min;                                              // E25
        fit.max = g_phonePanel.max;                                              // E25
        fit.group = 1;                                                           // E25
        fit.minScale = g_phonePanel.minScale;                                    // E25
        fit.maxScale = g_phonePanel.maxScale;                                    // E25
        if (g_joystickIconsMax.x > g_joystickIconsMin.x)                         // E25
        {                                                                        // E25
            fit.avoidMin = g_joystickIconsMin-vector2(6,6);                      // E25
            fit.avoidMax = g_joystickIconsMax+vector2(6,6);                      // E25
        }                                                                        // E25
        shadowText(fit.min, title, "Arial Narrow", 40.0f, 255,203,203,228, fit.max.x, fit);   // E25
        TextFit body = fit;                                                      // E25
        body.columns = 2;                                                        // E25
        shadowText(fit.min+vector2(0,50), content, "Arial Narrow", textSize, 255,203,203,228, fit.max.x, body);   // E25
        return;                                                                  // E25
    }                                                                            // E25
    // E24: a right-to-left language sets the title and the body against the
    // panel's right edge, 10 px in as they are from its left, so every panel's
    // lines start at one edge; the panel runs to the screen's right edge
    // (screenSize.x, whole, where rectPos.x+rectSize.x is 1024 give or take a float).
    const float rtlRight = screenSize.x-10;                           // E24
    shadowText(rectPos+vector2(10,20), title, "Arial Narrow", 40.0f, 255,203,203,228, rtlRight);   // E24: rtlRight
    shadowText(rectPos+vector2(10,70), content, "Arial Narrow", textSize, 255,203,203,228, rtlRight);   // E24: rtlRight
}

// menu.as:232. The whole menu and arena-select logic, run every frame (the
// cursor is dynamic).
void ETHCallback_cursor(ETHEntity thisEntity)
{
    detectJoysticks();

    [[maybe_unused]] const vector2 screenSize = GetScreenSize();    // menu.as:236, never read
    InputState& input = GetInputHandle();                             // menu.as:237
    // Read BEFORE the move below, so the entity lags the OS cursor by a frame
    // (menu.as:238-240).
    const vector2 cursorPos = input.GetCursorPos();
    input.SetCursorPos(input.GetCursorAbsolutePos()+getPlayerXYAxis(0)*5.0f);
    thisEntity->SetPositionXY(cursorPos);

    // E27: the language list, when open, is all the menu answers to.
    if (languageList())                                               // E27
    {                                                                 // E27
        showToggleFullscreenMessage();                                // E27: Alt+Enter and its line, as under the buttons
        fadeIn(thisEntity->GetUIntData("menuStartTime"));             // E27: as the callback's last line
        return;                                                       // E27
    }                                                                 // E27

    ETHEntity handle;                                                 // menu.as:242

    if (thisEntity->CheckCustomData("newGame") == DT_NODATA)          // menu.as:244
    {
        if (CollideDynamic(thisEntity, handle))
        {
            const string entityName = handle->GetEntityName();
            const bool confirmed = getConfirmButtonStatus(0) == KS_HIT;
            if (entityName == "creditos")
            {
                // E21: the enhanced edition's credit after the original team's.
                showData("Cr\xE9" "ditos", creditos + creditosEnhanced);   // menu.as:252: creditos
            } else
            if (entityName == "melhores_tempos")
            {
                showData("Melhores tempos", getRecordTimeList());
            } else
            if (entityName == "como_jogar")
            {
                showData("Como Jogar", como_jogar);
            } else
            if (entityName == "versus")                                // menu.as:262
            {
                if (hasASecondController())
                {
                    showData("Jogador versus Jogador", versus);
                    if (confirmed)
                    {
                        goToPvp();
                    }
                }
                else
                {
                    if (confirmed)
                    {
                        PlaySample("soundfx/fail.ogg");
                    }
                    string message = "\xC9 necess\xE1rio ao menos um joystick\n para jogar neste modo.";   // menu.as:278
                    if (input.GetJoystickStatus(0) == JS_DETECTED)
                    {
                        message += endl + endl + "J\xE1 h\xE1 um joystick plugado." + endl
                        + "Mude as op\xE7\xF5" "es de entrada no menu" + endl + "de configura\xE7\xF5" "es para poder" + endl
                        + "utilizar o teclado e o joystick" + endl + "por 2 jogadores.";   // menu.as:281-283
                    }
                    showData("Jogador versus Jogador", message);
                }
            } else
            if (entityName == "novo_jogo")                             // menu.as:288
            {
                showData("Novo jogo", novo_jogo);
                if (confirmed)
                {
                    // Starts the 3 s fade-out; newGame() runs when it ends
                    // (the else branch below).
                    thisEntity->AddUIntData("newGame", GetTime());
                    thisEntity->AddStringData("scene", "CAMPAIGN");
                    PlaySample("soundfx/newgame.mp3");
                }
            } else
            if (entityName == "sair")                                  // menu.as:298
            {
                showData("Sair do jogo", "");
                // Exit() only requests the exit; the callback runs on.
                if (confirmed)
                    Exit();
            } else
            if (entityName == "opcoes_de_video")                       // menu.as:304
            {
                showData("Configura\xE7\xF5" "es", config);
                if (confirmed)
                {
                    LoadScene("scenes/videoModes.esc", "screenModesPreLoop", "screenModesLoop");
                }
            } else if (entityName == "thumbnail")                      // menu.as:311
            {
                const string number = handle->GetStringData("name");
                const string title = handle->GetStringData("title");
                string extra = "";
                bool allow = true;

                // An arena with a "score" unlocks only for a best campaign
                // time strictly below it (menu.as:318-326).
                if (handle->CheckCustomData("score") != DT_NODATA)
                {
                    if (handle->GetUIntData("score") <= getGetBestTime())
                    {
                        allow = false;
                        extra = "\n\n\xC9 necess\xE1rio terminar o jogo\nem menos de " +
                            getTimeString(handle->GetUIntData("score")) + " para\nliberar esta arena.";   // menu.as:323-324
                    }
                }

                showData(title, g_gameData.get("global", "arena"+number)+extra);   // menu.as:328

                if (confirmed && allow)
                {
                    thisEntity->AddUIntData("newGame", GetTime());
                    thisEntity->AddStringData("scene", "pvp_lv" + number + ".esc");
                    PlaySample("soundfx/newgame.mp3");
                } else if (confirmed && !allow)
                {
                    PlaySample("soundfx/fail.ogg");
                }
            }

            // lastButton is never reset when the cursor leaves every button, so
            // coming back to the same one is silent (menu.as:341-345).
            if (entityName != thisEntity->GetStringData("lastButton"))
            {
                PlaySample("soundfx/help.mp3");
            }
            thisEntity->AddStringData("lastButton", entityName);
        }
        showToggleFullscreenMessage();                                // menu.as:347
    }
    else
    {
        // fadeOut leaves bias unwritten on the frame it returns true, so the
        // volume snaps back to 1 then (menu.as:351-356).
        float bias = 0;
        if (fadeOut(thisEntity->GetUIntData("newGame"), bias))
        {
            newGame(thisEntity->GetStringData("scene"));
        }
        SetSampleVolume("soundfx/menu.mp3", 1.0f-bias);
        loadingMessage();
    }
    if (thisEntity->CheckCustomData("newGame") == DT_NODATA)         // E27: not while a start's fade runs
        languageButton();                                             // E27: over the picture, under the fade
    fadeIn(thisEntity->GetUIntData("menuStartTime"));                 // menu.as:359
}

// menu.as:362. Darkens a locked arena every frame.
void ETHCallback_thumbnail(ETHEntity thisEntity)
{
    if (thisEntity->CheckCustomData("score") != DT_NODATA)
    {
        if (thisEntity->GetUIntData("score") <= getGetBestTime())
        {
            thisEntity->SetColor(vector3(0.05f, 0.05f, 0.05f));
            //thisEntity.SetAlpha(0.8f);
        }
    }
}

// menu.as:374
bool waitForInputToMenu()
{
    if (getCancelButtonStatus(0) == KS_HIT)
    {
        return goToMenu();
    }
    return false;
}

// menu.as:383
bool goToMenu()
{
    if (GetSceneFileName() != "scenes/menu.esc")
    {
        LoadScene("scenes/menu.esc", "menuPreLoop", "menuLoop", vector2(1024,256));
        return true;
    }
    return false;
}

// menu.as:393. Unlike goToMenu, it reloads the menu even from the menu.
bool escToGoToMenu()
{
    InputState& input = GetInputHandle();                             // menu.as:395
    if (input.GetKeyState(K_ESC) == KS_HIT)
    {
        LoadScene("scenes/menu.esc", "menuPreLoop", "menuLoop", vector2(1024,256));
        return true;
    }
    return false;
}

} // namespace Penumbra::Script
