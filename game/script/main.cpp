// main.as, ported (extracted/app/main.as; Penumbra by Andre Santee, LGPL-3.0-or-later).
// The module's globals, the boot (main(), here ScriptMain()), newGame, and the
// four trigger callbacks that live in main.as: help, story, checkpoint and
// next_level. main.as:74-97 #included the other 24 files; Script.hpp is that
// module's symbol table.

#include "script/Script.hpp"

namespace Penumbra::Script {

// main.as:43-60. Script globals: they survive every LoadScene (docs/spec/10
// section 1).
dictionary<frameTimer> g_frameTimers;
enmlFile g_gameData;
ETHEntityArray g_spawn;
ETHEntityArray g_sounds;
MessageManager g_messages;
CameraManager g_camera;
Timer g_timer;

int g_lives = 0;
uint g_numNpcs = 0;
uint g_levelStartTime = 0;
bool g_gameFinished = false;
array<int> g_exp(2, 0);
array<int> g_charLevel(2, 8);           // 8 until the first resetData sets 1; never observed
uint g_newRecordTime = 0;
array<bool> g_castingLight(2, false);
array<int> g_pvpPoints(2, 0);
array<Combo> g_comboManager(2);

// main.as:62. Called by newGame (main.as:101) and goToPvp (setupScene.as:244).
void resetData()
{
    g_exp[0] = g_exp[1] = 0;
    g_charLevel[0] = g_charLevel[1] = 1;
    g_gameData.getInt("global", "lives", g_lives);
    g_timer.start();
    g_gameFinished = false;
    g_newRecordTime = 0;
    g_castingLight[0] = g_castingLight[1] = false;
    g_pvpPoints[0] = g_pvpPoints[1] = 0;
}

// main.as:99. `sceneName` is "CAMPAIGN" or an arena's .esc file name (menu.as).
void newGame(const string& sceneName)
{
    resetData();

    if (IsPixelShaderSupported())
        UsePixelShaders(true);

    if (sceneName == "CAMPAIGN")
    {
        int level = 1;
        // main.as:109-115: keys HELD when the menu's fade ends pick the level,
        // and PAGEUP starts both characters at level 15 (the original's cheats).
        InputState& input = GetInputHandle();
        if (input.GetKeyState(K_2) == KS_DOWN)
            level = 2;
        if (input.GetKeyState(K_3) == KS_DOWN)
            level = 3;
        if (input.GetKeyState(K_PAGEUP) == KS_DOWN)
            g_charLevel[0] = g_charLevel[1] = 15;
        LoadScene("scenes/level" + Str(level) + ".esc", "setupScene", "levelLoop");
    }
    else
    {
        LoadScene("scenes/" + sceneName, "setupScene", "pvpLoop");
    }
}

// main.as:124, main(). Runs before the first frame (Machine::Boot); the
// LoadScene is only a request, served by frame 1. The menu's 1024x256 buckets
// are the one non-default bucket size in the game.
void ScriptMain()
{
    LoadScene("scenes/menu.esc", "menuPreLoop", "menuLoop", vector2(1024,256));
    //LoadScene("scenes/checkpoint.esc", "setupScene", "levelLoop");

    // main.as:130-132 and :140-142: the `#if TESTING` overrides are dropped
    // (TESTING is never defined in the shipped game, docs/spec/30).
    bool hideCursor = true;
    HideCursor(hideCursor);

    const string str = GetStringFromFile(GetAbsolutePath("data.enml"));
    g_gameData.parseString(str);
    g_gameData.getInt("global", "lives", g_lives);

    bool windowed = true;

    SetWindowProperties(APPLICATION_TITLE, 1024, 768, windowed, true, PF32BIT);
}

// main.as:147. A static entity, so it runs only while its bucket is drawn; the
// proximity test only sees entities in the trigger's own bucket.
void ETHCallback_help(ETHEntity thisEntity)
{
    ETHEntityArray entityArray;
    GetEntitiesFromBucket(thisEntity->GetCurrentBucket(), entityArray);
    for (uint t=0; t<entityArray.size(); t++)
    {
        if (getDist(entityArray[t]->GetPositionXY(), thisEntity->GetPositionXY()) < 30.0f)
        {
            if (isAMainCharacter(entityArray[t]))
            {
                g_messages.addMessage(thisEntity->GetStringData("message"));
            }
        }
    }
}

// main.as:163. World-anchored lore text from data.enml's global section.
void ETHCallback_story(ETHEntity thisEntity)
{
    shadowText(thisEntity->GetPositionXY()-GetCameraPos(),
        g_gameData.get("global", thisEntity->GetStringData("name")),
        "Arial Narrow", 16.0f, 100,203,203,228);
}

// main.as:170
void ETHCallback_checkpoint(ETHEntity thisEntity)
{
    ETHEntityArray entityArray;
    GetEntitiesFromBucket(thisEntity->GetCurrentBucket(),entityArray);
    for (uint t=0; t<entityArray.size(); t++)
    {
        if (getDist(entityArray[t]->GetPositionXY(), thisEntity->GetPositionXY()) < 30.0f)
        {
            if (isAMainCharacter(entityArray[t]))
            {
                if (entityArray[t]->GetUIntData("playerId") == 0)
                {
                    entityArray[t]->AddUIntData("hasCheckpoint", 1);
                    // main.as:183: written into the save, never read by any script.
                    entityArray[t]->AddIntData("lives", g_lives);
                    // main.as:184: the save is taken BEFORE this checkpoint is
                    // deleted, so it contains the checkpoint itself and a
                    // respawn there triggers it again (docs/spec/10 section 12).
                    SaveScene("scenes/checkpoint.esc");
                    g_messages.addMessage("Checkpoint...");
                    DeleteEntity(thisEntity);
                    // main.as:187: reads the position of the entity it has just
                    // deleted; the handle keeps the dead object readable.
                    AddEntity("checkpoint_effect.ent", thisEntity->GetPosition()+vector3(0,0,10), 0.0f);
                }
            }
        }
    }
}

// main.as:194
void ETHCallback_next_level(ETHEntity thisEntity)
{
    if (thisEntity->CheckCustomData("fadeOut") == DT_NODATA)
    {
        ETHEntityArray entityArray;
        GetEntitiesFromBucket(thisEntity->GetCurrentBucket(),entityArray);
        for (uint t=0; t<entityArray.size(); t++)
        {
            if (getDist(entityArray[t]->GetPositionXY(), thisEntity->GetPositionXY()) < 80.0f)
            {
                if (isAMainCharacter(entityArray[t]))
                {
                    if (entityArray[t]->GetUIntData("playerId") == 0)
                    {
                        // main.as:208: player 1 holding down (key or stick).
                        if (getPlayerXYAxis(0).y > 0)
                        {
                            thisEntity->AddUIntData("fadeOut", GetTime());
                        }
                    }
                }
            }
        }
    }
    else
    {
        // main.as:219-226: fadeOut leaves `bias` unwritten on the frame it
        // returns true (util.as:392-403), so here the music volume is back at
        // 1.0 for the frame the load is requested. (AngelScript 2.20 copied
        // back its &out temporary instead; the load releases every sample on
        // the next frame either way.)
        float bias = 0;
        if (fadeOut(thisEntity->GetUIntData("fadeOut"), bias))
        {
            if (IsPixelShaderSupported())
                UsePixelShaders(true);
            LoadScene("scenes/" + thisEntity->GetStringData("name"), "setupScene", "levelLoop");
        }
        SetSampleVolume("soundfx/fase.mp3", 1.0f-bias);
        loadingMessage();
    }
}

} // namespace Penumbra::Script
