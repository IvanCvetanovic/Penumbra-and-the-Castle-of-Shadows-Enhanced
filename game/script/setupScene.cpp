// setupScene.as, ported (extracted/app/setupScene.as; Penumbra by Andre Santee, LGPL-3.0-or-later).
// The preLoop of every level, arena and checkpoint load (setupScene) and their
// loop (levelLoop/pvpLoop -> doLoop): spawning from markers as they come on
// screen, messages, camera, the run timer and both end screens.

#include "script/Script.hpp"

namespace Penumbra::Script {

// setupScene.as:43. The "play" music markers: all three are fase.mp3
// (docs/spec/10 section 11.5). Static, so it runs once its bucket is drawn.
void ETHCallback_play(ETHEntity thisEntity)
{
    const string name = thisEntity->GetStringData("name");
    LoadMusic("soundfx/" + name);
    PlaySample("soundfx/" + name);
    LoopSample("soundfx/" + name, true);
    DeleteEntity(thisEntity);
}

// setupScene.as:52. Gives an enemy just added from a spawn marker its AI state
// and its data.enml stats. The typed getters leave the temporary unwritten on
// a missing key, so it would keep the previous stat; no shipped section is
// missing one (docs/spec/10 section 6.4).
void spawn(ETHEntity handle, const string& name)
{
    handle->AddUIntData("currentDir", LEFT);
    handle->AddFloatData("forceX", 0);
    handle->AddFloatData("forceY", 0);
    handle->AddFloatData("knockBackX", 0);
    handle->AddFloatData("knockBackY", 0);
    handle->AddUIntData("touchingGround", 0);
    handle->AddUIntData("action", STANDING);
    handle->AddUIntData("lastSwordAttack", 0);
    handle->AddUIntData("lastTimeDidntSee", GetTime());
    handle->AddUIntData("fireResistant", 0);

    int iTemp = 0;
    uint uiTemp = 0;
    float fTemp = 0;

    g_gameData.getInt(name, "hp", iTemp);
    handle->AddIntData("hp", iTemp);

    g_gameData.getInt(name, "damage", iTemp);
    handle->AddIntData("damage", iTemp);

    g_gameData.getFloat(name, "speed", fTemp);
    handle->AddFloatData("speed", fTemp);

    g_gameData.getFloat(name, "viewRadius", fTemp);
    handle->AddFloatData("viewRadius", fTemp);

    g_gameData.getFloat(name, "attackRadius", fTemp);
    handle->AddFloatData("attackRadius", fTemp);

    g_gameData.getFloat(name, "pushBackBias", fTemp);
    handle->AddFloatData("pushBackBias", fTemp);

    g_gameData.getUint(name, "stride", uiTemp);
    handle->AddUIntData("stride", uiTemp);

    g_gameData.getUint(name, "coolDown", uiTemp);
    handle->AddUIntData("coolDown", uiTemp);

    g_gameData.getUint(name, "waitBeforeAttack", uiTemp);
    handle->AddUIntData("waitBeforeAttack", uiTemp);

    g_gameData.getUint(name, "jumpBackAfterAttack", uiTemp);
    handle->AddUIntData("jumpBackAfterAttack", uiTemp);

    g_gameData.getUint(name, "fireResistant", uiTemp);
    handle->AddUIntData("fireResistant", uiTemp);

    // setupScene.as:102-103: the experience an enemy gives is its hp.
    g_gameData.getInt(name, "hp", iTemp);
    handle->AddIntData("expGiven", iTemp);

    string sTemp;
    if ((sTemp = g_gameData.get(name, "chaseSfx")) != "")
    {
        const string fileName = "soundfx/" + sTemp;
        handle->AddStringData("chaseSfx", fileName);
        LoadSoundEffect(fileName);
    }
}

// setupScene.as:114. The preLoop: every LoadScene has just released all
// sprites and samples (E:ETHEngine.cpp:826-827), so they are loaded again here
// on each level, arena, respawn and checkpoint load.
void setupScene()
{
    // disable pixel shaders if the game is set up for that
    if (g_enablePS.getCurrent() == 1)
        UsePixelShaders(false);

    LoadSprite("interface/hp.png");
    LoadSprite("interface/mp.png");
    LoadSprite("interface/xp.png");
    LoadSprite("interface/rail.png");
    LoadSprite("interface/skull_interface.png");
    LoadSprite("interface/frame.png");
    LoadSprite("interface/blend.png");
    LoadSprite("interface/joystick.png");
    LoadSprite("entities/gameover.png");
    LoadSoundEffect("soundfx/creature_show_up.mp3");
    LoadSoundEffect("soundfx/light_spell.mp3");
    LoadSoundEffect("soundfx/cast_fire_spell.ogg");
    LoadSoundEffect("soundfx/cast_fire_spell.ogg");
    LoadSoundEffect("soundfx/cast_fire_spell.ogg");
    LoadSoundEffect("soundfx/respawn.mp3");
    LoadSoundEffect("soundfx/paladin_appear.mp3");
    LoadSoundEffect("soundfx/thunder.mp3");
    LoadSoundEffect("soundfx/horror.mp3");
    LoadSoundEffect("soundfx/fall.ogg");
    LoadSoundEffect("soundfx/laugh_king.mp3");
    LoadSoundEffect("soundfx/pvp_win.ogg");
    LoadSoundEffect("soundfx/death_king.ogg");
    LoadSoundEffect("soundfx/checkpoint.mp3");
    LoadSoundEffect("soundfx/creature_show_up.mp3");
    LoadSoundEffect("soundfx/creature_dying.mp3");
    LoadSoundEffect("soundfx/explosion.ogg");

    LoadSoundEffect("soundfx/help.mp3");
    LoadSoundEffect("soundfx/hit01.ogg");
    LoadSoundEffect("soundfx/potion_pick.ogg");
    LoadSoundEffect("soundfx/minion.ogg");
    LoadSoundEffect("soundfx/sword_combo.ogg");
    LoadSoundEffect("soundfx/sword01.mp3");
    LoadSoundEffect("soundfx/vanish.ogg");
    LoadSoundEffect("soundfx/jump01.ogg");
    LoadSoundEffect("soundfx/jump02.ogg");

    LoadSoundEffect("soundfx/blast_attack.ogg");
    SetSampleVolume("soundfx/blast_attack.ogg", 0.5f);

    LoadMusic("soundfx/chefao.mp3");
    LoadMusic("soundfx/fase.mp3");

    SetBackgroundColor(0xFF000000);

    g_frameTimers.deleteAll();

    SetBorderBucketsDrawing(false);

    //AddEntity("silent_sword.ent",vector3(-4000,-4000,0), 0);

    // setupScene.as:171-175: exact names, so the "play_sound.ent"-named
    // markers (level2, level3) and level3's "spawn2" are never collected.
    g_spawn.clear();
    GetEntityArray("spawn", g_spawn);

    g_sounds.clear();
    GetEntityArray("play_sound", g_sounds);

    g_levelStartTime = GetTime();

    PlaySample("soundfx/respawn.mp3");

    // remove the flashlights left by the level designer
    {
        ETHEntityArray flashLights;
        GetEntityArray("flashlight.ent", flashLights);
        const uint size = flashLights.size();
        for (uint t=0; t<size; t++)
        {
            DeleteEntity(flashLights[t]);
        }
    }

    // put environment effects
    {
        ETHEntityArray effects;
        GetEntityArray("environment", effects);
        const uint size = effects.size();
        for (uint t=0; t<size; t++)
        {
            ETHEntity handle;
            // setupScene.as:200: this default is never added - a marker
            // without a "name" (pvp_lv5's) gets no effect at all.
            string entity = "clouds.ent";
            if (effects[t]->CheckCustomData("name") != DT_NODATA)
            {
                entity = effects[t]->GetStringData("name");
                AddEntity(entity, effects[t]->GetPosition(), handle);
            }
            if (handle != nullptr)
            {
                handle->AddUIntData("bgColor", effects[t]->GetUIntData("bgColor"));
                if (effects[t]->CheckCustomData("bgImage") != DT_NODATA)
                {
                    handle->AddStringData("bgImage", effects[t]->GetStringData("bgImage"));
                }
            }
            DeleteEntity(effects[t]);
        }
    }

    // draws a black rect so that the first frame of the scene won't be visible
    // and the fade in works fine
    drawRect(0xFF000000);

    g_camera.setMainCharPos(vector2(0,0), 0);
    g_camera.setMainCharPos(vector2(0,0), 1);
}

// setupScene.as:226
void levelLoop()
{
    doLoop(false);
}

// setupScene.as:231. The boss track is the arenas' music. On the end screen
// doLoop stops it every frame and this restarts it the next (docs/spec/10
// section 18 item 8).
void pvpLoop()
{
    if (!IsSamplePlaying("soundfx/chefao.mp3"))
    {
        PlaySample("soundfx/chefao.mp3");
        LoopSample("soundfx/chefao.mp3", true);
    }

    doLoop(true);
}

// setupScene.as:242. The menu's "versus" button. No bucket size, so the arena
// select scene gets the default 256, unlike menu.esc's 1024x256.
void goToPvp()
{
    resetData();
    LoadScene("scenes/arena_select.esc", "menuPreLoop", "menuLoop");
}

// setupScene.as:248
void doLoop(const bool pvp)
{
    if (pvp)
        SetBorderBucketsDrawing(true);

    InputState& input = GetInputHandle();

    fadeIn(g_levelStartTime);

    // setupScene.as:257-312: markers spawn their entity once they come within
    // the screen (+/-SIZE_TOLERANCE). This runs BEFORE adjustCameraPos, so on
    // the first frame after a load (camera reset to 0,0) the test is against
    // the rect at the origin.
    const uint size = g_spawn.size();
    for (uint t=0; t<size; t++)
    {
        if (!g_spawn[t]->IsAlive())
            continue;

        // setupScene.as:264-267: the `#if TESTING` force for "bruxo" is dropped.
        bool forceSpawn = false;

        if (!isInScreen(g_spawn[t]) && !forceSpawn)
            continue;

        ETHEntity handle;
        const string name = g_spawn[t]->GetStringData("name");

        // setupScene.as:276: the KEY's presence counts, whatever its value;
        // every player marker has complete=0 and is complete.
        bool isComplete = false;
        if (g_spawn[t]->CheckCustomData("complete") != DT_NODATA)
            isComplete = true;

        AddEntity(name + ".ent", g_spawn[t]->GetPosition()-vector3(0,0,10), handle);
        DeleteEntity(g_spawn[t]);

        // setupScene.as:282: unchecked, as in the original - if the .ent could
        // not be added, the null handle aborts the rest of this loop call.
        handle->AddUIntData("waitBeforeAttack", 0);
        handle->AddUIntData("lastTimeAlive", 0);

        // add pvp specific data
        if (pvp)
        {
            handle->AddUIntData("pvpMode", 1);
            if (isComplete)
            {
                handle->AddIntData("hp", handle->GetIntData("hp")*5);
                handle->AddIntData("maxHp", handle->GetIntData("maxHp")*5);
            }
        }

        if (isAMainCharacter(handle))
        {
            g_camera.setMainCharPos(handle->GetPositionXY(), handle->GetUIntData("playerId"));
        }

        // plays it's show up sound effect if there's any
        if (handle->CheckCustomData("showUpSfx") != DT_NODATA)
        {
            PlaySample("soundfx/" + handle->GetStringData("showUpSfx"));
            g_camera.startEarthquake(8);
        }

        if (isComplete)
            continue;

        spawn(handle, name);
    }

    // play sound effects if it aproaches them
    const uint sounds = g_sounds.size();
    for (uint t=0; t<sounds; t++)
    {
        if (!g_sounds[t]->IsAlive())
            continue;

        if (!isInScreen(g_sounds[t]))
            continue;

        PlaySample("soundfx/" + g_sounds[t]->GetStringData("name"));
        DeleteEntity(g_sounds[t]);
    }

    // setupScene.as:328-330: the `#if TESTING` NPC counter display is dropped;
    // the enemy callbacks still count into g_numNpcs.
    g_numNpcs = 0;

    g_messages.showMessages(vector2(10,70), "Arial", 30, 203, 203, 228, g_camera);
    g_camera.adjustCameraPos(pvp);
    const float textSize = 25.0f;

    // handles the game ending
    if (!g_gameFinished)
    {
        g_timer.showTimer(vector2(GetScreenSize().x-50, 0), textSize, 255, 203, 203, 228);

        if (pvp)
        {
            bool gameWon = false;
            for (uint t=0; t<MAX_PLAYERS; t++)
            {
                if (g_pvpPoints[t] >= MAX_PVP_POINTS)
                    gameWon = true;
            }
            if (abs(g_pvpPoints[1]-g_pvpPoints[0]) >= MAX_PVP_POINTS)
            {
                gameWon = true;
            }
            if (gameWon)
            {
                g_gameFinished = true;
                PlaySample("soundfx/pvp_win.ogg");
            }
        }
    }
    else
    {
        if (!pvp)
        {
            // setupScene.as:365-371: the first finished frame records the run
            // time (from newGame's resetData, fades and loads included).
            if (g_newRecordTime == 0)
            {
                g_newRecordTime = g_timer.getElapsedTime();
                StopSample("soundfx/chefao.mp3");
                PlaySample("soundfx/death_king.ogg");
                addNewRecordTime(g_newRecordTime);
            }
            shadowText(vector2(100,100-15), "Seu tempo total foi:", "Arial Narrow", 30, 255, 203, 203, 228);
            shadowText(vector2(100,100), getTimeString(g_newRecordTime), "Arial Narrow", 256, 255, 203, 203, 228);
            shadowText(vector2(110,356), "Melhores tempos:\n" + getRecordTimeList()
                + "\n\n", "Arial Narrow", 30, 255, 203, 203, 228);
        }
        else
        {
            StopSample("soundfx/chefao.mp3");
            // setupScene.as:380: a tie (3-3 reached between two checks) reads
            // "Jogador 2".
            const uint winnerId = g_pvpPoints[0] > g_pvpPoints[1] ? 1 : 2;
            [[maybe_unused]] const int diff = abs(g_pvpPoints[0]-g_pvpPoints[1]);   // setupScene.as:381: never read

            const vector2 spriteSize = GetSpriteSize("entities/gameover.png");
            // setupScene.as:384 `GetScreenSize()/2`: glm's vector/scalar
            // operator needs the scalar in the vector's own type.
            const vector2 screenMiddle = GetScreenSize()/2.0f;
            DrawSprite("entities/gameover.png", vector2(screenMiddle.x-spriteSize.x/2, screenMiddle.y-200.0f), 0xFFFFFFFF);

            shadowText(vector2(200,350-15), endl + endl + "Jogador " + Str(winnerId)
                        + " \xE9 o vencedor!" + endl, "Arial Narrow", 50, 255, 203, 203, 228);
        }
        waitForInputToMenu();
    }
    escToGoToMenu();
    if (input.KeyDown(K_J))
        detectJoysticks();
}

} // namespace Penumbra::Script
