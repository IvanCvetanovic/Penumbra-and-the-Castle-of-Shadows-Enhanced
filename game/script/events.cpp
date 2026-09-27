// events.as, ported (extracted/app/events.as; Penumbra by Andre Santee, LGPL-3.0-or-later).
// The king's arena trigger (level3's event01) and the falling bridge stones.

#include "script/Script.hpp"

namespace Penumbra::Script {

// events.as:43. level3's static, collidable event01 box: any main character in
// its bucket starts the boss fight. There is no break after the first one, so
// both players in the bucket on one frame run the body twice (two kings, two
// summon effects, chefao.mp3 restarted) - docs/spec/10 section 18 item 14.
void ETHCallback_event01(ETHEntity thisEntity)
{
    ETHEntityArray entities;
    GetEntitiesFromBucket(thisEntity->GetCurrentBucket(), entities);
    const uint size = entities.size();
    for (uint t=0; t<size; t++)
    {
        if (isAMainCharacter(entities[t]))
        {
            StopSample("soundfx/fase.mp3");
            PlaySample("soundfx/laugh_king.mp3");
            PlaySample("soundfx/chefao.mp3");
            LoopSample("soundfx/chefao.mp3", true);
            AddEntity("summon.ent", thisEntity->GetPosition(), 0.0f);
            ETHEntity handle;
            AddEntity("king.ent", thisEntity->GetPosition()+vector3(0,0,-10), handle);
            spawn(handle, "king");
            // events.as:60-61: the handle keeps the deleted trigger readable, so
            // a second pass of the loop still reads its position.
            DeleteEntity(thisEntity);
            AddEntity("invisible_wall.ent", vector3(10112, 1408, 0), 0.0f);
        }
    }
}

// SIZE_ADDITION (events.as:66) is in Script.hpp.

// events.as:67. A stone of the bridge: a main character standing on it starts
// it; 600 ms later it loses its collision and falls under its own gravity
// until it leaves the screen.
void ETHCallback_falling_bridge(ETHEntity thisEntity)
{
    if (thisEntity->CheckCustomData("falling") == DT_NODATA)
    {
        ETHEntityArray entities;
        GetEntitiesFromBucket(thisEntity->GetCurrentBucket(), entities);
        GetEntitiesFromBucket(thisEntity->GetCurrentBucket()+vector2(0,-1), entities);
        // events.as:74-77: the box is centred (checkBoxHit, util.as:350) and y
        // grows downwards, so +SIZE_ADDITION height with the centre raised by
        // SIZE_ADDITION/2 moves only the TOP edge, up by SIZE_ADDITION; with
        // the width cut to 90%, a character standing on the stone touches it
        // and one brushing its side does not.
        collisionBox stoneBox = getAbsoluteCollisionBox(thisEntity);
        stoneBox.size.y += SIZE_ADDITION;
        stoneBox.size.x *= 0.9f;
        stoneBox.pos.y -= SIZE_ADDITION/2;
        const uint size = entities.size();
        for (uint t=0; t<size; t++)
        {
            if (isAMainCharacter(entities[t]))
            {
                collisionBox charBox = getAbsoluteCollisionBox(entities[t]);
                if (checkBoxHit(charBox, stoneBox))
                {
                    thisEntity->AddFloatData("gravity", 0.0f);
                    thisEntity->AddUIntData("falling", GetTime());
                }
            }
        }
    }
    else if (GetTime()-thisEntity->GetUIntData("falling") > 600)
    {
        thisEntity->SetCollision(false);
        const float gravity = thisEntity->GetFloatData("gravity");
        // events.as:96: the trigger set gravity to 0, so this is the first
        // falling frame: one dust burst.
        if (gravity == 0.0f)
            AddEntity("bridge_fall.ent", thisEntity->GetPosition()+vector3(0,0,-2), 0.0f);
        thisEntity->AddFloatData("gravity", gravity+UnitsPerSecond(GRAVITY));
        const float fps = GetFPSRate() == 0 ? 60 : GetFPSRate();
        thisEntity->AddToPositionXY(vector2(0,thisEntity->GetFloatData("gravity")/fps));
        if (!isInScreen(thisEntity))
        {
            DeleteEntity(thisEntity);
        }
    }
}

} // namespace Penumbra::Script
