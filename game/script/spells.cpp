// spells.as, ported: casting a spell entity, the light spell that follows its
// caster, and the fire balls. Original: extracted/app/spells.as (Penumbra, LGPL-3).

#include "script/Script.hpp"

namespace Penumbra::Script {

// spells.as:43. Owners without an "mp" key (NPCs) cast for free; there is no
// cooldown here, only mana.
bool castSpell(ETHEntity owner, const string& spellName, const int mana, const vector3& spellOffset,
               const int damage, const float speedMultiplier) {
    // if there's not enough mana...
    if (owner->CheckCustomData("mp") != DT_NODATA) {
        if (owner->GetIntData("mp") < mana)
            return false;
        else
            addToMp(owner, -mana);
    }

    ETHEntity handle;
    AddEntity(spellName, owner->GetPosition() + spellOffset, handle);
    handle->AddFloatData("spellOffsetX", spellOffset.x);
    handle->AddFloatData("spellOffsetY", spellOffset.y);
    handle->AddFloatData("speed", owner->GetFloatData("speed") * speedMultiplier);
    handle->AddUIntData("direction", owner->GetUIntData("currentDir"));
    handle->AddFloatData("ownerX", owner->GetPositionXY().x);
    handle->AddFloatData("ownerY", owner->GetPositionXY().y);
    handle->AddIntData("ownerID", owner->GetID());
    handle->AddIntData("damage", damage);

    if (owner->CheckCustomData("playerId") != DT_NODATA) {
        handle->AddUIntData("playerId", owner->GetUIntData("playerId"));
    }

    if (isAMainCharacter(owner)) {
        handle->AddUIntData("mainCharacter", 1);
        if (owner->CheckCustomData("pvpMode") != DT_NODATA) {
            handle->AddUIntData("pvpMode", 1);
        }
    }

    return true;
}

// spells.as:83. Runs every frame the light lives: it re-arms the caster's
// "a light is alive" flag (controlCharacter clears it at the end of each of
// its calls, playerInput.as:439) and keeps the light at the caster + offset.
void ETHCallback_light_spell(ETHEntity thisEntity) {
    g_castingLight[thisEntity->GetUIntData("playerId")] = true;
    vector2 bucket = thisEntity->GetCurrentBucket();
    ETHEntityArray entitiesAround;
    // spells.as:88-93: its own bucket, the three below and the two beside, not
    // the row above - the light floats 40 px ABOVE its caster (y is down), so
    // the caster is in the light's row or the one below it.
    GetEntitiesFromBucket(bucket, entitiesAround);
    GetEntitiesFromBucket(bucket + vector2(0.0f, 1.0f), entitiesAround);
    GetEntitiesFromBucket(bucket + vector2(1.0f, 1.0f), entitiesAround);
    GetEntitiesFromBucket(bucket + vector2(-1.0f, 1.0f), entitiesAround);
    GetEntitiesFromBucket(bucket + vector2(1.0f, 0.0f), entitiesAround);
    GetEntitiesFromBucket(bucket + vector2(-1.0f, 0.0f), entitiesAround);

    vector2 offset(0.0f, 0.0f);   // AngelScript's vector2 default-constructs to zero
    offset.x = thisEntity->GetFloatData("spellOffsetX");
    offset.y = thisEntity->GetFloatData("spellOffsetY");

    const uint size = entitiesAround.size();
    for (uint t = 0; t < size; t++) {
        if (isAMainCharacter(entitiesAround[t])) {
            if (thisEntity->GetIntData("ownerID") == entitiesAround[t]->GetID()) {
                thisEntity->SetPositionXY(entitiesAround[t]->GetPositionXY() + offset);
                break;
            }
        }
    }
}

// spells.as:113
void ETHCallback_fire_ball(ETHEntity thisEntity) {
    manageFireBall(thisEntity, "explosion.ent");
}

// spells.as:118
void ETHCallback_combo_fire_ball(ETHEntity thisEntity) {
    manageFireBall(thisEntity, "big_explosion.ent");
}

// spells.as:123. The fire ball flies straight at 1.5x its stored speed and is
// deleted on the first frame doDamage reports any overlap - with
// collideEverything, that is walls and other attacks too (npc_wall excepted).
// Whether it explodes depends on the LAST entity it overlapped that frame
// (doDamage's `r`, doDamage.as:137/151): a wall listed after a fire-resistant
// NPC still makes it explode.
void manageFireBall(ETHEntity thisEntity, const string& explosion) {
    vector2 dir(1.0f, 0.0f);
    if (thisEntity->GetUIntData("direction") == LEFT) {
        dir.x = -1.0f;
    }
    thisEntity->AddToPositionXY(dir * UnitsPerSecond(thisEntity->GetFloatData("speed")) * 1.5f);
    ETHEntity target = doDamage(thisEntity, 15.0f, false, true, ATTACK_MODE_FIRE, 1.0f);
    if (target != nullptr) {
        bool explode = true;
        if (target->CheckCustomData("fireResistant") != DT_NODATA)
            if (target->GetUIntData("fireResistant") != 0)
                explode = false;

        if (explode) {
            AddEntity(explosion, thisEntity->GetPosition(), 0.0f);
            g_camera.startEarthquake(20.0f);
        }
        DeleteEntity(thisEntity);
        return;
    }
    // spells.as:147: another attack's doDamage overlapped this one and marked
    // it; it vanishes without an explosion.
    if (thisEntity->CheckCustomData("hitBy") != DT_NODATA) {
        DeleteEntity(thisEntity);
        return;
    }
}

} // namespace Penumbra::Script
