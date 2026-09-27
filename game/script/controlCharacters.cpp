// controlCharacters.as, ported: character collision, animation, death, and the
// per-frame callbacks of the two players and every enemy, plus the king's and
// player 1's summoning. A faithful port - the original's quirks are kept on
// purpose (docs/spec/11-logic-player-combat.md, 12-logic-ai-world.md).
//
// The original's header (controlCharacters.as:1-41; it gives the same notice in
// Portuguese first):
//
// This file is part of Penumbra.
//
// Penumbra is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// Penumbra is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with Ethanon Engine. If not, see
// <http://www.gnu.org/licenses/>.

#include "script/Script.hpp"

namespace Penumbra::Script {

// controlCharacters.as:43
void doCharacterCollision(ETHEntity thisEntity, const bool useNpcInvisibleWalls)
{
    const vector2 force = getCurrentForce(thisEntity);
    const uint dir = findDirection(force);

    // assign the current direction to the entity
    thisEntity->AddUIntData("walking", (dir == LEFT || dir == RIGHT) ? 1 : 0);   // controlCharacters.as:49 (never read)

    // controlCharacters.as:51-53: the current bucket, then the neighbours the
    // force points into, in findDestinationBuckets' order - the resolution
    // below depends on this order.
    ETHEntityArray collidableEntities;
    GetEntitiesFromBucket(thisEntity->GetCurrentBucket(), collidableEntities);
    collidableEntities += findDestinationBuckets(thisEntity, force, true);

    collisionBox thisBox = thisEntity->GetCollisionBox();
    thisBox.pos += thisEntity->GetPosition();

    const int size = static_cast<int>(collidableEntities.size());   // controlCharacters.as:58
    bool thinnerBoxHit = false;
    // E11: whether ANY box collided this frame passed the thinner-box test.
    bool anyThinnerBoxHit = false;
    // controlCharacters.as:60: read nowhere - even the TESTING overlay's line that
    // named it (:215) is commented out inside that block's /* */.
    uint collisionCount = 0;
    for (int t = 0; t < size; t++)
    {
        if (!collidableEntities[t]->Collidable())
            continue;

        if (!useNpcInvisibleWalls)
        {
            if (collidableEntities[t]->GetEntityName() == "npc_wall.ent")
            {
                continue;
            }
        }

        if (thisEntity->GetID() == collidableEntities[t]->GetID())
            continue;

        // if the target entity's owner is itself (it's probably a sword effect
        if (collidableEntities[t]->CheckCustomData("ownerID") != DT_NODATA)   // controlCharacters.as:78
            continue;

        collisionBox box = collidableEntities[t]->GetCollisionBox();
        box.pos += collidableEntities[t]->GetPosition();

        if (checkBoxHit(thisBox, box))   // controlCharacters.as:84
        {
            collisionCount++;

            // controlCharacters.as:88-101: no `continue` here, so a kill box is
            // also resolved as a solid below; in PvP each kill box touched in a
            // frame costs a point.
            if (collidableEntities[t]->GetEntityName() == "instant_death.ent"
                || collidableEntities[t]->GetEntityName() == "instant_death2.ent"
                || collidableEntities[t]->GetEntityName() == "instant_death3.ent")
            {
                thisEntity->AddIntData("hp", 0);
                if (isAMainCharacter(thisEntity))
                {
                    if (thisEntity->CheckCustomData("pvpMode") != DT_NODATA)
                    {
                        g_pvpPoints[thisEntity->GetUIntData("playerId")]--;
                        thisEntity->AddUIntData("dontFrag", 1);   // controlCharacters.as:98 (never read)
                    }
                }
            }

            if (collidableEntities[t]->CheckCustomData("dontCollide") != DT_NODATA)   // controlCharacters.as:103
                continue;


            if (collidableEntities[t]->CheckCustomData("hp") != DT_NODATA)   // controlCharacters.as:107
            {
                // if both are characters, knock'em back
                if (collidableEntities[t]->CheckCustomData("hp") != DT_NODATA
                    && thisEntity->CheckCustomData("hp") != DT_NODATA)
                {
                    vector2 thisPos = thisEntity->GetPositionXY();
                    vector2 pos = collidableEntities[t]->GetPositionXY();
                    // controlCharacters.as:115-117: float literals because glm's
                    // vector-by-scalar operators do not deduce from an int, where
                    // AngelScript converted the 2 and the 7 to float. Eth's
                    // normalize gives NaN for two characters at one position,
                    // as the original's did.
                    vector2 half = (thisPos + pos) / 2.0f;
                    knockBack(thisEntity, normalize(thisPos - pos) * 7.0f);
                    knockBack(collidableEntities[t], normalize(pos - thisPos) * 7.0f);
                    AddEntity("jumpfx.ent", vector3(half.x, half.y, 0), 0.0f);   // controlCharacters.as:118
                    continue;
                }
            }

            // check collision against a thinner box to prevent "wall grabing"
            collisionBox thinnerBox = thisBox;   // controlCharacters.as:124
            thinnerBox.size.x *= 0.6f;
            // controlCharacters.as:126: overwritten by every solid hit, so the
            // touchingGround test after the loop sees only the last one.
            thinnerBoxHit = checkBoxHit(thinnerBox, box);
            anyThinnerBoxHit = anyThinnerBoxHit || thinnerBoxHit;   // E11

            const vector2 currentPos = thisEntity->GetPositionXY();
            uint collDir = findBoxDirection(thisBox, box);   // controlCharacters.as:129
            // controlCharacters.as:130-180: each snap places the ORIGIN from the
            // box sizes, ignoring the box's own offset, and thisBox is refreshed
            // only after a horizontal snap.
            if (collDir == RIGHT)
            {
                setForceX(thisEntity, 0);
                thisEntity->SetPositionXY(
                    vector2(((box.pos.x - box.size.x/2) - thisBox.size.x/2)-1, currentPos.y));

                // refresh box position
                thisBox = thisEntity->GetCollisionBox();
                thisBox.pos += thisEntity->GetPosition();
            }
            else if (collDir == LEFT)   // controlCharacters.as:140
            {
                setForceX(thisEntity, 0);
                thisEntity->SetPositionXY(
                    vector2(((box.pos.x + box.size.x/2) + thisBox.size.x/2)+1, currentPos.y));

                // refresh box position
                thisBox = thisEntity->GetCollisionBox();
                thisBox.pos += thisEntity->GetPosition();
            }
            else if (collDir == DOWN)   // controlCharacters.as:150
            {
                // if it's falling...
                // (`force` is the one read before the loop, not the zeroed one)
                if (force.y > 0)
                {
                    if (thinnerBoxHit)
                    {
                        if (force.y > 700.0f)   // controlCharacters.as:157: the hard landing
                        {
                            PlaySample("soundfx/fall.ogg");
                            const vector3 pos(thisBox.pos.x, thisBox.pos.y+thisBox.size.y/2, 0.0f);
                            // controlCharacters.as:161: the int 0 picks the angle (float)
                            // overload here as in AngelScript, which has no int -> string
                            // conversion; in C++ the string overload would need a
                            // user-defined conversion, so it loses.
                            AddEntity("fall.ent", pos, 0);
                            g_camera.startEarthquake(10.0f);
                        }

                        setForceY(thisEntity, 0);   // controlCharacters.as:165
                        thisEntity->SetPositionXY(
                            vector2(currentPos.x, (box.pos.y - box.size.y/2) - thisBox.size.y/2));
                        thisEntity->AddUIntData("touchingGround", 1);
                    }
                }
            }
            else if (collDir == UP)   // controlCharacters.as:172
            {
                if (thinnerBoxHit)
                {
                    setForceY(thisEntity, 0);
                    thisEntity->SetPositionXY(
                        vector2(currentPos.x, (box.pos.y + box.size.y/2) + thisBox.size.y/2));
                }
            }
        } // if collided...

    } // for

    // if he's up, he's not touching the ground
    if (force.y < 0)   // controlCharacters.as:186
    {
        thisEntity->AddUIntData("touchingGround", 0);
    }
    else
    {
        // controlCharacters.as:192 tested the thinner box of the LAST box
        // collided only, so where two floor tiles meet under the wizard the
        // second tile's miss made him airborne for a frame and restarted his
        // walk cycle (docs/planning, E11). kFloorSeamFix asks whether any box
        // held him; off, it is the original's line.
        if (!(kFloorSeamFix ? anyThinnerBoxHit : thinnerBoxHit))
        {
            thisEntity->AddUIntData("touchingGround", 0);
        }
    }

    // if it touches the ground, zero the jump count
    if (thisEntity->GetUIntData("touchingGround") == 1)   // controlCharacters.as:199
        thisEntity->AddIntData("jumps", 0);

    // controlCharacters.as:202-219: a `#if TESTING` debug overlay (FPS and
    // forces drawn as text), dropped - TESTING is never defined in the shipped game.
}

// controlCharacters.as:222
void animateCharacter(ETHEntity thisEntity, std::shared_ptr<frameTimer> timer)
{
    if (timer != nullptr)
    {
        const uint stride = thisEntity->GetUIntData("stride");
        const uint dir = thisEntity->GetUIntData("currentDir");
        const bool touchingGround = (thisEntity->GetUIntData("touchingGround") != 0);
        const vector2 force = getCurrentForce(thisEntity);

        if (touchingGround)   // controlCharacters.as:231
        {
            if (force.x < 0)
            {
                timer->Set(4, 7, stride);
            }
            else if (force.x > 0)
            {
                timer->Set(8, 11, stride);
            }
            else
            {
                // controlCharacters.as:243-246: no final else. currentDir only ever
                // holds LEFT or RIGHT (characterAI.as:80-180, playerInput.as:341/346,
                // setupScene.as:54), or reads 0 = RIGHT when missing, so one of the
                // two always applies.
                if (dir == LEFT)
                    timer->Set(4, 4, stride);
                else if (dir == RIGHT)
                    timer->Set(8, 8, stride);
            }
        }
        else   // controlCharacters.as:249
        {
            if (dir == LEFT)
            {
                if (force.y > 0)
                    timer->Set(5, 5, stride);
                else
                    timer->Set(4, 4, stride);
            }
            else if (dir == RIGHT)
            {
                if (force.y > 0)
                    timer->Set(9, 9, stride);
                else
                    timer->Set(8, 8, stride);
            }
        }
        thisEntity->SetFrame(timer->Get());   // controlCharacters.as:266
    }
}

// controlCharacters.as:270
void ETHCallback_bruxo(ETHEntity thisEntity)
{
    // unconditional, so the camera keeps following player 0 while he is dead
    g_camera.setMainCharPos(thisEntity->GetPositionXY(), 0);

    std::shared_ptr<frameTimer> timer;

    // if it has no timer yet, instantiate a timer for this one
    if (!g_frameTimers.get("id" + Str(thisEntity->GetID()), timer))   // controlCharacters.as:277
    {
        auto newTimer = std::make_shared<frameTimer>();
        newTimer->Set(thisEntity->GetFrame(), thisEntity->GetFrame(), 0);
        g_frameTimers.set("id" + Str(thisEntity->GetID()), newTimer);
        timer = newTimer;
    }

    if (g_gameFinished)   // controlCharacters.as:285: invulnerable on the end screen
    {
        thisEntity->AddIntData("hp", 100);
    }
    if (!isMainCharDead(thisEntity, "fade_out_beam.ent"))   // controlCharacters.as:289
    {
        applyForce(thisEntity, vector2(0, UnitsPerSecond(GRAVITY)));
        controlCharacter(thisEntity, 0);
        move(thisEntity);
        doCharacterCollision(thisEntity, false);
        animateCharacter(thisEntity, timer);
        drawPlayerStatus(thisEntity);
        doMpRecovery(thisEntity, 350, 1);

        if (thisEntity->CheckCustomData("pvpMode") == DT_NODATA)   // controlCharacters.as:299
            player1Summoner(thisEntity);
    }
}

// controlCharacters.as:304
void ETHCallback_princess(ETHEntity thisEntity)
{
    std::shared_ptr<frameTimer> timer;

    // if it has no timer yet, instantiate a timer for this one
    if (!g_frameTimers.get("id" + Str(thisEntity->GetID()), timer))   // controlCharacters.as:309
    {
        auto newTimer = std::make_shared<frameTimer>();
        newTimer->Set(thisEntity->GetFrame(), thisEntity->GetFrame(), 0);
        g_frameTimers.set("id" + Str(thisEntity->GetID()), newTimer);
        timer = newTimer;
    }

    if (g_gameFinished)   // controlCharacters.as:317
    {
        thisEntity->AddIntData("hp", 100);
    }

    const bool pvpMode = (thisEntity->CheckCustomData("pvpMode") != DT_NODATA);   // controlCharacters.as:322

    // In the campaign the princess is a summoned creature: she dies like an
    // enemy (isDead), not like a player, and costs no reload.
    bool dead = false;
    if (pvpMode)
        dead = isMainCharDead(thisEntity, "fade_out_beam.ent");
    else
        dead = isDead(thisEntity, "fade_out_beam.ent");

    if (!dead)   // controlCharacters.as:330
    {
        g_camera.setMainCharPos(thisEntity->GetPositionXY(), 1);

        applyForce(thisEntity, vector2(0, UnitsPerSecond(GRAVITY)));
        controlCharacter(thisEntity, 1);
        move(thisEntity);
        doCharacterCollision(thisEntity, false);
        animateCharacter(thisEntity, timer);
        drawPlayerStatus(thisEntity);
        doMpRecovery(thisEntity, 350, 1);

        // controlCharacters.as:342-360: the campaign leash. lastTimeAlive is
        // missing on her first frame, and a gap of 4 s or more since the last
        // run of this callback suspends the leash for that frame.
        if (!pvpMode && thisEntity->CheckCustomData("lastTimeAlive") != DT_NODATA)
        {
            if (GetTime()-thisEntity->GetUIntData("lastTimeAlive") < 4000)
            {
                if (!isInScreen(thisEntity))
                {
                    // "The summoned creature cannot leave the screen's field of view"
                    g_messages.addMessage("A criatura invocada n\xE3o pode sair do campo de vis\xE3o da tela");
                    // vanishTime reads 0 if she has never been on screen (controlCharacters.as:349)
                    if (GetTime()-thisEntity->GetUIntData("vanishTime") > 3000)
                    {
                        thisEntity->AddIntData("hp", 0);
                        g_camera.startEarthquake(20.0f);
                    }
                }
                else
                {
                    thisEntity->AddUIntData("vanishTime", GetTime());   // controlCharacters.as:357
                }
            }
        }
    }
    else   // controlCharacters.as:362
    {
        g_camera.setMainCharPos(vector2(0, 0), 1);
    }
    // controlCharacters.as:366: also on the frame isDead deleted her; the
    // handle still reaches the dead entity, as in 0.7.12.
    thisEntity->AddUIntData("lastTimeAlive", GetTime());
}

// controlCharacters.as:369
bool isDead(ETHEntity thisEntity, const string& beamEffect)
{
    if (thisEntity->GetIntData("hp") <= 0)
    {
        const uint now = GetTime();
        if (thisEntity->CheckCustomData("deathTime") == DT_NODATA)   // controlCharacters.as:374: the first dead frame
        {
            if (beamEffect != "")
                AddEntity(beamEffect, thisEntity->GetPosition()+vector3(0, 0, 4), 0.0f);   // controlCharacters.as:377
            if (thisEntity->HasParticleSystem(0))
                thisEntity->KillParticleSystem(0);
            thisEntity->AddUIntData("deathTime", now);
            thisEntity->SetColor(vector3(0, 0, 0));
            thisEntity->SetCollision(false);

            // controlCharacters.as:384: both players gain, whatever killed it
            // and whether or not player 1 exists.
            if (thisEntity->CheckCustomData("pvpMode") == DT_NODATA && !isAMainCharacter(thisEntity))
            {
                // add death to exp
                addToExp(0, thisEntity->GetIntData("expGiven"));
                addToExp(1, thisEntity->GetIntData("expGiven"));
            }
        }
        const uint deathTime = thisEntity->GetUIntData("deathTime");
        const uint deadTime = now-deathTime;   // controlCharacters.as:392 (uint wrap, as the original)

        if (deadTime >= DEAD_FADE_OUT_TIME)
        {
            DeleteEntity(thisEntity);
            return true;
        }
        thisEntity->SetAlpha(1.0f-(static_cast<float>(deadTime)/static_cast<float>(DEAD_FADE_OUT_TIME)));   // controlCharacters.as:399

        return true;
    }
    return false;
}

// controlCharacters.as:406
bool isMainCharDead(ETHEntity thisEntity, const string& beamEffect)
{
    const bool pvpMode = (thisEntity->CheckCustomData("pvpMode") != DT_NODATA);
    [[maybe_unused]] const uint playerId = thisEntity->GetUIntData("playerId");   // controlCharacters.as:409 (never read)

    // controlCharacters.as:411: a finished PvP match freezes both players
    if (pvpMode && g_gameFinished)
        return true;

    if (thisEntity->GetIntData("hp") <= 0)   // controlCharacters.as:414
    {
        const uint now = GetTime();
        if (thisEntity->CheckCustomData("deathTime") == DT_NODATA)
        {
            if (beamEffect != "")
                AddEntity(beamEffect, thisEntity->GetPosition(), 0.0f);   // controlCharacters.as:420 (no +4 z, unlike isDead)
            if (thisEntity->HasParticleSystem(0))
                thisEntity->KillParticleSystem(0);
            thisEntity->AddUIntData("deathTime", now);
            thisEntity->SetColor(vector3(0, 0, 0));
            thisEntity->SetCollision(false);
        }
        const uint deathTime = thisEntity->GetUIntData("deathTime");
        const uint deadTime = now-deathTime;

        // controlCharacters.as:430-451: LoadScene only queues the load, which
        // runs in the next frame after its loop and before any callback, so
        // this branch runs once per death (docs/spec/90-synthesis.md). PvP
        // decrements g_lives too. The rest of the function still runs this frame.
        if (deadTime >= DEAD_FADE_OUT_TIME)
        {
            g_lives--;
            if (g_lives >= 0 || pvpMode)
            {
                if (thisEntity->CheckCustomData("hasCheckpoint") == DT_NODATA)
                {
                    if (!pvpMode)
                        LoadScene(GetSceneFileName(), "setupScene", "levelLoop");
                    else
                        LoadScene(GetSceneFileName(), "setupScene", "pvpLoop");
                }
                else
                {
                    LoadScene("scenes/checkpoint.esc", "setupScene", "levelLoop");   // controlCharacters.as:444
                }
            }
            else
            {
                LoadScene("scenes/gameover.esc", "gameOverPreLoop", "gameOverLoop");   // controlCharacters.as:449
            }
        }

        const float alpha = (static_cast<float>(deadTime)/static_cast<float>(DEAD_FADE_OUT_TIME));   // controlCharacters.as:453
        if (IsSamplePlaying("soundfx/fase.mp3"))
            SetSampleVolume("soundfx/fase.mp3", 1.0f-alpha);

        thisEntity->SetAlpha(1.0f-alpha);
        // controlCharacters.as:458: on the reload frame deadTime is 3000-3016 ms, so
        // alpha*255 is 255-256.4; from deadTime 3012 it reaches 256 and the uint8
        // conversion wraps to 0, as AngelScript's did - that last frame's black
        // rectangle can be nearly or fully transparent.
        const uint rectColor = ARGB(ToUint8(alpha*255.0f), 0, 0, 0);
        DrawRectangle(vector2(0, 0), GetScreenSize(), rectColor, rectColor, rectColor, rectColor);

        if (alpha > 0.7f && g_lives >= 0)   // controlCharacters.as:461
        {
            loadingMessage();
        }

        return true;
    }
    return false;
}

// controlCharacters.as:471
void ETHCallback_warrior(ETHEntity thisEntity)
{
    g_numNpcs++;
    std::shared_ptr<frameTimer> timer;

    // if it has no timer yet, instantiate a timer for this one
    if (!g_frameTimers.get("id" + Str(thisEntity->GetID()), timer))   // controlCharacters.as:477
    {
        auto newTimer = std::make_shared<frameTimer>();
        newTimer->Set(thisEntity->GetFrame(), thisEntity->GetFrame(), 0);
        g_frameTimers.set("id" + Str(thisEntity->GetID()), newTimer);
        timer = newTimer;
    }

    if (!isDead(thisEntity, "fade_out_beam.ent"))   // controlCharacters.as:485
    {
        applyForce(thisEntity, vector2(0, UnitsPerSecond(GRAVITY)));
        meleeCharacterAI(thisEntity, "enemy_sword.ent");
        move(thisEntity);
        doCharacterCollision(thisEntity, true);
        animateCharacter(thisEntity, timer);
    }
}

// controlCharacters.as:495
void ETHCallback_minion(ETHEntity thisEntity)
{
    g_numNpcs++;
    std::shared_ptr<frameTimer> timer;

    // if it has no timer yet, instantiate a timer for this one
    if (!g_frameTimers.get("id" + Str(thisEntity->GetID()), timer))   // controlCharacters.as:501
    {
        auto newTimer = std::make_shared<frameTimer>();
        newTimer->Set(thisEntity->GetFrame(), thisEntity->GetFrame(), 0);
        g_frameTimers.set("id" + Str(thisEntity->GetID()), newTimer);
        timer = newTimer;
    }

    if (!isDead(thisEntity, "fade_out_beam.ent"))   // controlCharacters.as:509
    {
        applyForce(thisEntity, vector2(0, UnitsPerSecond(GRAVITY)));
        meleeCharacterAI(thisEntity, "enemy_sword.ent");
        move(thisEntity);
        doCharacterCollision(thisEntity, true);
        animateCharacter(thisEntity, timer);
    }
}

// controlCharacters.as:519
void ETHCallback_knight(ETHEntity thisEntity)
{
    g_numNpcs++;
    std::shared_ptr<frameTimer> timer;

    // if it has no timer yet, instantiate a timer for this one
    if (!g_frameTimers.get("id" + Str(thisEntity->GetID()), timer))   // controlCharacters.as:525
    {
        auto newTimer = std::make_shared<frameTimer>();
        newTimer->Set(thisEntity->GetFrame(), thisEntity->GetFrame(), 0);
        g_frameTimers.set("id" + Str(thisEntity->GetID()), newTimer);
        timer = newTimer;
    }

    if (!isDead(thisEntity, "fade_out_beam.ent"))   // controlCharacters.as:533
    {
        applyForce(thisEntity, vector2(0, UnitsPerSecond(GRAVITY)));
        meleeCharacterAI(thisEntity, "enemy_sword.ent");
        move(thisEntity);
        doCharacterCollision(thisEntity, true);
        animateCharacter(thisEntity, timer);
    }
}

// controlCharacters.as:543
void ETHCallback_impy(ETHEntity thisEntity)
{
    g_numNpcs++;
    std::shared_ptr<frameTimer> timer;

    // if it has no timer yet, instantiate a timer for this one
    if (!g_frameTimers.get("id" + Str(thisEntity->GetID()), timer))   // controlCharacters.as:549
    {
        auto newTimer = std::make_shared<frameTimer>();
        newTimer->Set(thisEntity->GetFrame(), thisEntity->GetFrame(), 0);
        g_frameTimers.set("id" + Str(thisEntity->GetID()), newTimer);
        timer = newTimer;
    }

    if (!isDead(thisEntity, "fade_out_beam.ent"))   // controlCharacters.as:557
    {
        applyForce(thisEntity, vector2(0, UnitsPerSecond(GRAVITY)));
        rangedCharacterAI(thisEntity, "fire_ball.ent", 1);
        move(thisEntity);
        doCharacterCollision(thisEntity, true);
        animateCharacter(thisEntity, timer);
    }
}

// controlCharacters.as:567
void ETHCallback_master_knight(ETHEntity thisEntity)
{
    g_numNpcs++;
    std::shared_ptr<frameTimer> timer;

    // if it has no timer yet, instantiate a timer for this one
    if (!g_frameTimers.get("id" + Str(thisEntity->GetID()), timer))   // controlCharacters.as:573
    {
        auto newTimer = std::make_shared<frameTimer>();
        newTimer->Set(thisEntity->GetFrame(), thisEntity->GetFrame(), 0);
        g_frameTimers.set("id" + Str(thisEntity->GetID()), newTimer);
        timer = newTimer;
    }

    if (!isDead(thisEntity, "fade_out_beam_large.ent"))   // controlCharacters.as:581
    {
        applyForce(thisEntity, vector2(0, UnitsPerSecond(GRAVITY)));
        meleeCharacterAI(thisEntity, "dark_sword.ent");
        move(thisEntity);
        doCharacterCollision(thisEntity, true);
        animateCharacter(thisEntity, timer);
    }
}

// controlCharacters.as:591
void ETHCallback_paladin(ETHEntity thisEntity)
{
    g_numNpcs++;
    std::shared_ptr<frameTimer> timer;

    // if it has no timer yet, instantiate a timer for this one
    if (!g_frameTimers.get("id" + Str(thisEntity->GetID()), timer))   // controlCharacters.as:597
    {
        auto newTimer = std::make_shared<frameTimer>();
        newTimer->Set(thisEntity->GetFrame(), thisEntity->GetFrame(), 0);
        g_frameTimers.set("id" + Str(thisEntity->GetID()), newTimer);
        timer = newTimer;
    }

    if (!isDead(thisEntity, "fade_out_beam.ent"))   // controlCharacters.as:605
    {
        applyForce(thisEntity, vector2(0, UnitsPerSecond(GRAVITY)));
        meleeCharacterAI(thisEntity, "paladin_sword.ent");
        move(thisEntity);
        doCharacterCollision(thisEntity, true);
        animateCharacter(thisEntity, timer);
    }
}

// controlCharacters.as:615
void summoner(ETHEntity thisEntity, const string& creature,
              const string& summonOriginEntity, const uint interval)
{
    if (thisEntity->CheckCustomData("lastSummon") == DT_NODATA)
        thisEntity->AddUIntData("lastSummon", GetTime());

    if (GetTime()-thisEntity->GetUIntData("lastSummon") >= interval)   // controlCharacters.as:621
    {
        // only the one bucket beside the summoner, on the side it faces
        ETHEntityArray entities;
        if (thisEntity->GetUIntData("currentDir") == LEFT)
            GetEntitiesFromBucket(thisEntity->GetCurrentBucket()+vector2(-1, 0), entities);
        else
            GetEntitiesFromBucket(thisEntity->GetCurrentBucket()+vector2(1, 0), entities);

        const uint size = entities.size();
        for (uint t = 0; t < size; t++)   // controlCharacters.as:630
        {
            if (entities[t]->GetEntityName() == summonOriginEntity)
            {
                AddEntity("summon.ent", entities[t]->GetPosition(), 0.0f);
                ETHEntity handle;
                // controlCharacters.as:636: randF(6) is the only random call in
                // the characters' code, an x jitter of [0, 6] px.
                AddEntity(creature+".ent", entities[t]->GetPosition()+vector3(0, 0, -10)+vector3(randF(6), 0, 0), handle);
                spawn(handle, creature);   // controlCharacters.as:637
                break;
            }
        }
        thisEntity->AddUIntData("lastSummon", GetTime());   // controlCharacters.as:641
    }
}

// controlCharacters.as:645
void ETHCallback_king(ETHEntity thisEntity)
{
    g_numNpcs++;
    std::shared_ptr<frameTimer> timer;

    // if it has no timer yet, instantiate a timer for this one
    if (!g_frameTimers.get("id" + Str(thisEntity->GetID()), timer))   // controlCharacters.as:651
    {
        auto newTimer = std::make_shared<frameTimer>();
        newTimer->Set(thisEntity->GetFrame(), thisEntity->GetFrame(), 0);
        g_frameTimers.set("id" + Str(thisEntity->GetID()), newTimer);
        timer = newTimer;
    }

    if (!isDead(thisEntity, "fade_out_beam.ent"))   // controlCharacters.as:659
    {
        applyForce(thisEntity, vector2(0, UnitsPerSecond(GRAVITY)));
        mixedCharacterAI(thisEntity, "paladin_sword.ent", "fire_ball.ent", 100, 1.5f);
        move(thisEntity);
        doCharacterCollision(thisEntity, true);
        animateCharacter(thisEntity, timer);
        summoner(thisEntity, "warrior", "summon", 5000);   // controlCharacters.as:666
    }
    else
    {
        // controlCharacters.as:670: set on every frame of the king's fade
        g_gameFinished = true;
    }
}

// controlCharacters.as:674
void player1Summoner(ETHEntity player0)
{
    if (!hasASecondController())
        return;

    // controlCharacters.as:679: player 1's confirm is only his pad's JK_10, START
    // (playerInput.as:281-283)
    if (getConfirmButtonStatus(1) == KS_HIT)
    {
        // controlCharacters.as:681: `<=`, so exactly 50 is refused although the
        // message asks for 50.
        if (player0->GetIntData("mp") <= 50)
        {
            // "50 mana is needed to summon the creature"
            g_messages.addMessage("\xC9 necess\xE1rio de 50 mana para invocar a criatura");
            return;
        }

        ETHEntityArray visible;
        GetVisibleEntities(visible);   // controlCharacters.as:688
        {
            uint size = visible.size();
            for (uint t = 0; t < size; t++)
            {
                if (visible[t]->GetEntityName() == MAIN_CHARACTER_ENTITY1)
                {
                    // "It is not possible to summon 2 creatures at the same time"
                    g_messages.addMessage("N\xE3o \xE9 poss\xEDvel invocar 2 criaturas ao mesmo tempo");
                    return;
                }
            }
        }
        // controlCharacters.as:700-705: the summon box is player 0's box moved
        // one box-width plus 1 px to the RIGHT (and 6 px up), whichever way he
        // faces; only his bucket and the one to its right are searched.
        const vector2 currentBucket = player0->GetCurrentBucket();
        ETHEntityArray entitiesAround;
        GetEntitiesFromBucket(currentBucket, entitiesAround);
        GetEntitiesFromBucket(currentBucket+vector2(1, 0), entitiesAround);
        collisionBox playerBox = getAbsoluteCollisionBox(player0);
        playerBox.pos += vector3(playerBox.size.x+1, -6, 0);
        uint size = entitiesAround.size();
        bool maySummon = true;
        for (uint t = 0; t < size; t++)   // controlCharacters.as:708
        {
            if (!entitiesAround[t]->Collidable())
                continue;
            collisionBox box = getAbsoluteCollisionBox(entitiesAround[t]);
            if (checkBoxHit(box, playerBox))
            {
                maySummon = false;
                break;
            }
        }
        if (maySummon)   // controlCharacters.as:719
        {
            addToMp(player0, -50);
            // "Magic creature summoned"
            g_messages.addMessage("Criatura m\xE1gica invocada");
            AddEntity("summon.ent", playerBox.pos, 0.0f);
            AddEntity("princess.ent", playerBox.pos, 0.0f);
            g_lives--;   // controlCharacters.as:725: a summon costs a life (the HUD can show -1)
        }
        else
        {
            // "Impossible to summon the creature from here"
            g_messages.addMessage("Imposs\xEDvel invocar criatura daqui");
        }
    }
}

} // namespace Penumbra::Script
