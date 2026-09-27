// swords.as, ported: spawning a sword swing, and the sword entities' callbacks.
// Original: extracted/app/swords.as (Penumbra, LGPL-3).
//
// A swing is two temporary entities: the sword (a particle burst with a
// collision box, which runs doDamage every frame until its particles end) and
// a purely visual beam. The sword stays where it was spawned; its lifetime,
// set by its particles, is the hit window.

#include "script/Script.hpp"

namespace Penumbra::Script {

// swords.as:43. isInRange is the melee AI's `dist <= attackRadius`
// (characterAI.as:188; always true for the players); an out-of-range call only
// restarts the lastTimeDidntSee clock.
bool swordAttack(ETHEntity thisEntity, const string& swordName, const string& beamName, const bool isInRange) {
    if (!isInRange) {
        thisEntity->AddUIntData("lastTimeDidntSee", GetTime());
        return false;
    }

    // swords.as:51-64: the cooldown runs from the last swing, or - for an NPC
    // with waitBeforeAttack - from lastTimeDidntSee, which is written both on
    // every out-of-range call (:47) and on every swing (:104): the later of
    // the two. uint subtractions, wrapping as the original's did.
    if (thisEntity->GetUIntData("waitBeforeAttack") == 0) {
        if ((GetTime() - thisEntity->GetUIntData("lastSwordAttack")) < thisEntity->GetUIntData("coolDown")) {
            return false;
        }
    } else {
        if ((GetTime() - thisEntity->GetUIntData("lastTimeDidntSee")) < thisEntity->GetUIntData("coolDown")) {
            return false;
        }
    }

    vector3 pos = thisEntity->GetPosition();
    const vector2 force(0.0f, 0.0f);// = getCurrentForce(thisEntity);
    vector3 swordPos = pos + vector3(force.x, force.y, 0.0f);
    uint dir = thisEntity->GetUIntData("currentDir");
    if (dir == LEFT) {
        swordPos += vector3(-28.0f, 0.0f, 10.0f);
    } else {
        swordPos += vector3(28.0f, 0.0f, 10.0f);
    }

    ETHEntity sword;
    ETHEntity beam;
    AddEntity(swordName, swordPos + vector3(0.0f, 0.0f, 8.0f), sword);
    AddEntity(beamName, pos + vector3(0.0f, 0.0f, 8.0f), beam);
    // swords.as:83-87: only the particles are mirrored; the sword's collision
    // box offset (-4, +4 in sword0.ent) is not, so a left swing reaches 64.5 px
    // from its owner and a right one 56.5 px (spec 11 section 11.3).
    if (dir == LEFT) {
        sword->MirrorParticleSystemX(0, true);
        beam->MirrorParticleSystemX(0, true);
    }
    sword->AddFloatData("ownerX", thisEntity->GetPositionXY().x);
    sword->AddFloatData("ownerY", thisEntity->GetPositionXY().y);
    sword->AddIntData("ownerID", thisEntity->GetID());
    sword->AddUIntData("direction", dir);
    sword->AddIntData("damage", thisEntity->GetIntData("damage"));

    if (isAMainCharacter(thisEntity)) {
        sword->AddUIntData("mainCharacter", 1);
        if (thisEntity->CheckCustomData("pvpMode") != DT_NODATA) {
            sword->AddUIntData("pvpMode", 1);
        }
        sword->AddUIntData("playerId", thisEntity->GetUIntData("playerId"));
    }

    thisEntity->AddUIntData("lastTimeDidntSee", GetTime());
    thisEntity->AddUIntData("lastSwordAttack", GetTime());
    return true;
}

// swords.as:109 (warrior, minion, knight)
void ETHCallback_enemy_sword(ETHEntity thisEntity) {
    doDamage(thisEntity, 4.0f, true, false, ATTACK_MODE_PHYSICAL, 1.0f);
}

// swords.as:114 (master_knight)
void ETHCallback_dark_sword(ETHEntity thisEntity) {
    doDamage(thisEntity, 12.0f, true, false, ATTACK_MODE_PHYSICAL, 1.0f);
}

//main character swords
// swords.as:120
void ETHCallback_sword0(ETHEntity thisEntity) {
    doDamage(thisEntity, 4.0f, true, false, ATTACK_MODE_PHYSICAL, 1.0f);
}
// swords.as:124
void ETHCallback_sword1(ETHEntity thisEntity) {
    doDamage(thisEntity, 4.0f, true, false, ATTACK_MODE_PHYSICAL, 1.0f);
}
// swords.as:128
void ETHCallback_combo_sword(ETHEntity thisEntity) {
    doDamage(thisEntity, 25.0f, true, false, ATTACK_MODE_PHYSICAL, 5.0f);
}

// swords.as:133 (paladin, king)
void ETHCallback_paladin_sword(ETHEntity thisEntity) {
    doDamage(thisEntity, 15.0f, true, false, ATTACK_MODE_PHYSICAL, 1.0f);
}

} // namespace Penumbra::Script
