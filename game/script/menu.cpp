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

namespace {                                                                                          // E30
// ENHANCEMENT E30 (not in the original): the menu song goes on through the scene loads between the     // E30
// menu's screens. The original's every load released every sample, and each of these screens' preLoops // E30
// started the song again from its first note (loopMenuSong, below); here the load that leaves one     // E30
// menu screen for another asks for the song to be kept (Audio.hpp's KeepOnNextLoad), and loopMenuSong // E30
// finds it playing. Asked only before the loads to a menu screen - the settings, the arena select and // E30
// the way back - and never before a start (newGame), so a level still begins in its own music.        // E30
// A song that is not playing (a menu entered from a level, the first screen of a run) is not kept:   // E30
// the call is then a no-op and loopMenuSong starts it as the original did.                           // E30
void keepMenuSong()                                                                                  // E30
{                                                                                                    // E30
    KeepSampleOnNextLoad("soundfx/menu.mp3");                                                        // E30
}                                                                                                    // E30
} // namespace                                                                                       // E30

// menu.as:43
void loopMenuSong()
{
    LoadMusic("soundfx/menu.mp3");
    // E30: a song kept through the load (keepMenuSong) is found playing and goes on. The original    // E30
    // started it from its first note on every one of these loads: its load released every sample,   // E30
    // so this always found it silent. A kept song gets its full volume explicitly: the original     // E30
    // had that from the release that forgot the sample, which a kept one has not had.               // E30
    if (SampleExists("soundfx/menu.mp3") && IsSamplePlaying("soundfx/menu.mp3"))                           // E30
    {                                                                                                      // E30
        SetSampleVolume("soundfx/menu.mp3", 1.0f);                                                         // E30
        return;                                                                                            // E30
    }                                                                                                      // E30
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
                        keepMenuSong();                               // E30
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
                    keepMenuSong();                                   // E30
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
        keepMenuSong();                                               // E30: from the settings or the arena select
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
