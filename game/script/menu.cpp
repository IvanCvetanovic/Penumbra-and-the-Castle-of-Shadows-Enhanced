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
    if (input.GetJoystickStatus(0) == JS_DETECTED)
    {
        DrawSprite("interface/joystick.png", vector2(screenSize.x-joySpriteSize.x, 0),
                   ARGB(150,255,255,255));
    }
    if (input.GetJoystickStatus(1) == JS_DETECTED)
    {
        DrawSprite("interface/joystick.png", vector2(screenSize.x-joySpriteSize.x, joySpriteSize.y),
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
    shadowText(rectPos+vector2(10,20), title, "Arial Narrow", 40.0f, 255,203,203,228);
    shadowText(rectPos+vector2(10,70), content, "Arial Narrow", textSize, 255,203,203,228);
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
                showData("Cr\xE9" "ditos", creditos);                  // menu.as:252
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
