// characterAI.as, ported: the three enemy brains the enemy callbacks
// (controlCharacters.as) call once a frame. They only see a main character in
// their own bucket or the two on each side, level with them
// (findMainCharNeighbours(thisEntity, true)), and act through custom data -
// "action", "currentDir", forceX - that move() and animateCharacter consume.
//
// Ported from extracted/app/characterAI.as. Its header, kept (the PORTUGUES
// block before it says the same in Portuguese):
/*
This file is part of Penumbra.

Penumbra is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

Penumbra is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with Ethanon Engine. If not, see
<http://www.gnu.org/licenses/>.
*/

#include "script/Script.hpp"

namespace Penumbra::Script {

// characterAI.as:43. Keeps its distance: flees while the player is within
// attackRadius, otherwise stands facing them and casts effectEntity every
// coolDown ms while they are level with it (isOnSight, 76 px).
void rangedCharacterAI(ETHEntity thisEntity, const string& effectEntity, const float speedMultiplier)
{
    // The first frame counts as a cast, so the first spell waits a full coolDown.
    if (thisEntity->CheckCustomData("lastRangedAttack") == DT_NODATA)
    {
        thisEntity->AddUIntData("lastRangedAttack", GetTime());
    }

    const float speed = (thisEntity->GetFloatData("speed"));
    [[maybe_unused]] const uint action = thisEntity->GetUIntData("action");            // characterAI.as:51, never read
    [[maybe_unused]] const float viewRadius = thisEntity->GetFloatData("viewRadius");  // characterAI.as:52, never read
    const float attackRadius = thisEntity->GetFloatData("attackRadius");
    ETHEntity mainChar = findMainCharNeighbours(thisEntity, true);
    if (mainChar != nullptr)
    {
        if (mainChar->GetIntData("hp") <= 0)
            mainChar = nullptr;
    }

    vector2 pos = thisEntity->GetPositionXY();

    float dist = 0;

    if (mainChar != nullptr)
    {
        dist = getDist(thisEntity->GetPositionXY(), mainChar->GetPositionXY());
    }

    if (mainChar != nullptr)
    {
        const vector2 mainCharPos = mainChar->GetPositionXY();

        bool running = false;
        // if our character is too close, run like hell
        if (dist < attackRadius)
        {
            if (pos.x < mainCharPos.x)
            {
                thisEntity->AddUIntData("currentDir", LEFT);
                setForceX(thisEntity,-speed);
            }
            else
            {
                thisEntity->AddUIntData("currentDir", RIGHT);
                setForceX(thisEntity, speed);
            }
            running = true;
        }
        else
        {
            if (pos.x > mainCharPos.x)
            {
                thisEntity->AddUIntData("currentDir", LEFT);
            }
            else
            {
                thisEntity->AddUIntData("currentDir", RIGHT);
            }
            setForceX(thisEntity, 0);
        }
        if (!running)
        {
            if (isOnSight(thisEntity, mainChar, 1.0f))
            {
                // uint subtraction wraps like the original's (characterAI.as:106)
                if (GetTime()-thisEntity->GetUIntData("lastRangedAttack") > thisEntity->GetUIntData("coolDown"))
                {
                    castSpell(thisEntity, effectEntity, 0, vector3(0,0,0), thisEntity->GetIntData("damage"), speedMultiplier);
                    thisEntity->AddUIntData("lastRangedAttack", GetTime());
                    PlaySample("soundfx/cast_fire_spell.ogg");
                }
            }
        }
    }
    // With no living player near, nothing is written: forceX and currentDir
    // keep their last values (characterAI.as:70-114).
}

// characterAI.as:117. STANDING until a player comes within viewRadius, then
// CHASING until they are 2*viewRadius away or gone; while chasing it walks
// towards them and swings when within attackRadius.
void meleeCharacterAI(ETHEntity thisEntity, const string& swordEffect)
{
    const float speed = (thisEntity->GetFloatData("speed"));
    const uint action = thisEntity->GetUIntData("action");
    const float viewRadius = thisEntity->GetFloatData("viewRadius");
    const float attackRadius = thisEntity->GetFloatData("attackRadius");
    ETHEntity mainChar = findMainCharNeighbours(thisEntity, true);
    if (mainChar != nullptr)
    {
        if (mainChar->GetIntData("hp") <= 0)
            mainChar = nullptr;
    }

    vector2 pos = thisEntity->GetPositionXY();

    float dist = 0;

    if (mainChar != nullptr)
    {
        dist = getDist(thisEntity->GetPositionXY(), mainChar->GetPositionXY());
    }

    // `action` was read above, so a switch to STANDING here takes effect next
    // frame: this frame still runs the CHASING branch (characterAI.as:139-163).
    if (dist >= viewRadius*2 || mainChar == nullptr)
    {
        thisEntity->AddUIntData("action", STANDING);
    }

    if (action == STANDING)
    {
        setForceX(thisEntity, 0);
        if (mainChar != nullptr)
        {
            if (dist < viewRadius)
            {
                thisEntity->AddUIntData("action", CHASING);
                if (thisEntity->CheckCustomData("chaseSfx") != DT_NODATA)
                {
                    const string sample = thisEntity->GetStringData("chaseSfx");
                    if (!IsSamplePlaying(sample))
                    {
                        PlaySample(sample);
                    }
                }
            }
        }
    }
    else if (action == CHASING)
    {
        if (mainChar != nullptr)
        {
            const vector2 mainCharPos = mainChar->GetPositionXY();

            // chase the main character
            // It stops within 10 px of the player on x, and while any
            // knock-back is left (characterAI.as:170-171).
            if (dist > attackRadius && isOnSight(thisEntity, mainChar, 2.0f)
                && abs(pos.x-mainCharPos.x) > 10 && getLength(getKnockBackVector(thisEntity)) == 0)
            {
                if (pos.x < mainCharPos.x)
                {
                    thisEntity->AddUIntData("currentDir", RIGHT);
                    setForceX(thisEntity, speed);
                }
                else
                {
                    thisEntity->AddUIntData("currentDir", LEFT);
                    setForceX(thisEntity,-speed);
                }
            }
            else
            {
                setForceX(thisEntity, 0);
            }
            if (swordAttack(thisEntity, swordEffect, "enemy_sword_beam.ent", (dist <= attackRadius)))
            {
                if (thisEntity->GetUIntData("jumpBackAfterAttack") != 0)
                {
                    const uint dir = thisEntity->GetUIntData("currentDir");
                    const float jumpBack = (dir == LEFT) ? 12.0f : -12.0f;
                    knockBack(thisEntity, vector2(jumpBack,-12));   // characterAI.as:194, away from where it faces
                }
            }
        }
    }
}

// characterAI.as:201. Ranged beyond `distance` from the player, melee within.
// With no player near it does nothing at all, not even the melee brain's
// return to STANDING.
void mixedCharacterAI(ETHEntity thisEntity, const string& meleeFx, const string& rangedFx, const float distance,
                      const float speedMultiplier)
{
    ETHEntity mainChar = findMainCharNeighbours(thisEntity, true);
    if (mainChar != nullptr)
    {
        if (getDist(mainChar->GetPositionXY(), thisEntity->GetPositionXY()) > distance)
        {
            rangedCharacterAI(thisEntity, rangedFx, speedMultiplier);
        }
        else
        {
            meleeCharacterAI(thisEntity, meleeFx);
        }
    }
}

} // namespace Penumbra::Script
