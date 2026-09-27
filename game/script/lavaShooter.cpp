// lavaShooter.as, ported: the lava shooters (shooter.ent, static, so their
// callback runs only while their bucket is drawn) and the lava balls they
// throw (fire_shoot.ent, dynamic): a ball rises, falls back under a gravity of
// its own, and bursts on the first collidable thing it touches in its bucket
// column (docs/spec/12-logic-ai-world.md section 8.2).
//
// Ported from extracted/app/lavaShooter.as. Its header, kept (the PORTUGUES
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

// lavaShooter.as:43
void ETHCallback_shooter(ETHEntity thisEntity)
{
    // First run: coolDown (2500 in shooter.ent and in each of the 18 placements
    // in level1, level3 and pvp_lv2) becomes this shooter's own period,
    // 1000 + rand(2500) ms, drawn once (rand is inclusive, int rand(const int)
    // in 0.7.12, so the uint converts to int as AngelScript's call did).
    if (thisEntity->CheckCustomData("lastShoot") == DT_NODATA)
    {
        thisEntity->AddUIntData("lastShoot", GetTime());
        thisEntity->AddUIntData("coolDown", static_cast<uint>(1000+rand(static_cast<int>(thisEntity->GetUIntData("coolDown")))));
    }
    // uint subtraction wraps like the original's (lavaShooter.as:50)
    if (GetTime()-thisEntity->GetUIntData("lastShoot") >= thisEntity->GetUIntData("coolDown"))
    {
        vector3 pos = thisEntity->GetPosition();
        ETHEntity handle;
        AddEntity("fire_shoot.ent", pos+vector3(0,0,15), handle);   // lavaShooter.as:54
        handle->AddFloatData("force", 500+randF(thisEntity->GetFloatData("force")));
        handle->AddFloatData("ownerX", thisEntity->GetPositionXY().x);
        handle->AddFloatData("ownerY", thisEntity->GetPositionXY().y);
        handle->AddIntData("damage", 18);
        thisEntity->AddUIntData("lastShoot", GetTime());
        PlaySample("soundfx/cast_fire_spell.ogg");
    }
}

// lavaShooter.as:64
void ETHCallback_fire_shoot(ETHEntity thisEntity)
{
    // The upward force loses 0.6*GRAVITY per second (nothing on a scene
    // load's frame), and the ball rises 0.6*force/fps px a frame.
    float force = thisEntity->GetFloatData("force");
    thisEntity->AddFloatData("force", force-(UnitsPerSecond(GRAVITY))*0.6f);
    force = thisEntity->GetFloatData("force");

    const float fps = GetFPSRate() == 0 ? 60 : GetFPSRate();   // lavaShooter.as:70
    thisEntity->AddToPositionXY(vector2(0,(-force/fps)*0.6f));

    // Only its own bucket and the ones above and below: the ball moves on y.
    const vector2 bucket = thisEntity->GetCurrentBucket();
    ETHEntityArray entities;
    GetEntitiesFromBucket(bucket, entities);
    GetEntitiesFromBucket(bucket+vector2(0,1), entities);
    GetEntitiesFromBucket(bucket+vector2(0,-1), entities);

    const vector2 ownerPos = vector2(thisEntity->GetFloatData("ownerX"), thisEntity->GetFloatData("ownerY"));
    collisionBox thisBox = getAbsoluteCollisionBox(thisEntity);

    const uint size = entities.size();
    for (uint t=0; t<size; t++)
    {
        if (!entities[t]->Collidable())
            continue;

        if (entities[t]->GetID() == thisEntity->GetID())
            continue;

        // Other lava balls.
        if (entities[t]->GetEntityName() == thisEntity->GetEntityName())
            continue;

        if (entities[t]->GetEntityName() == "instant_death.ent" ||
            entities[t]->GetEntityName() == "instant_death2.ent" ||
            entities[t]->GetEntityName() == "instant_death3.ent")
            continue;

        if (entities[t]->GetEntityName() == "npc_wall.ent")
            continue;

        collisionBox box = getAbsoluteCollisionBox(entities[t]);

        if (checkBoxHit(box, thisBox))
        {
            // Damage straight to hp: no fire resistance and no damage number,
            // unlike doDamage (lavaShooter.as:106-115).
            if (entities[t]->CheckCustomData("hp") != DT_NODATA)
            {
                addToHp(entities[t],-thisEntity->GetIntData("damage"));

                // Pushed away from the SHOOTER, not from the ball (lavaShooter.as:110).
                const vector2 backVector = normalize(entities[t]->GetPositionXY()-ownerPos)*15.0f;
                float pushBackBias = 1;
                if (entities[t]->CheckCustomData("pushBackBias") != DT_NODATA)
                    pushBackBias = entities[t]->GetFloatData("pushBackBias");
                knockBack(entities[t], backVector*pushBackBias);
            }
            AddEntity("explosion.ent", thisEntity->GetPosition(), 0.0f);
            g_camera.startEarthquake(20);
            DeleteEntity(thisEntity);
            return;
        }
    }

    // Fallen back below the shooter: gone, with a splash at the shooter.
    if (thisEntity->GetPositionXY().y > ownerPos.y)
    {
        DeleteEntity(thisEntity);
        // z is the ball's world y, as in the original (lavaShooter.as:126); the
        // handle still reads a deleted entity's position (Entity.hpp).
        AddEntity("lava_drops.ent", vector3(ownerPos.x, ownerPos.y-thisBox.size.y, thisEntity->GetPositionXY().y), 0.0f);
    }
}

} // namespace Penumbra::Script
