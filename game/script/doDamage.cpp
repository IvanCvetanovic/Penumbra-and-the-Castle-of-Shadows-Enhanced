// doDamage.as, ported: the combat routine the sword and fire ball entities call
// every frame they live (swords.as:109-136, spells.as:131). The lava shooter's
// shots do NOT use it; they have their own collision loop (lavaShooter.as:64-128).
// Original: extracted/app/doDamage.as (Penumbra, LGPL-3).
//
// Its quirks are the original's and are kept on purpose (docs/spec/90-synthesis.md,
// 11-logic-player-combat.md sections 12.4 and 23): `isResistant` and `damage` carry over
// from one target to the next within a call; a target is damaged once per
// attack entity (the "id<N>" mark) but shakes the camera every frame it
// overlaps; and a PvP kill whose attacker is gone (doDamage.as:176, :193)
// throws, because isAMainCharacter(null) is a null handle access. That aborts
// the whole running CALLBACK, not just this function: manageFireBall then skips
// its explosion and DeleteEntity for that frame (spells.as:132-145).

#include "script/Script.hpp"

namespace Penumbra::Script {

// knockFromOwner: if true, the "knock back" will push the target from the attacker's position
// instead of pushing from the hit entity effect
// doDamage.as:45. Returns the last entity the attack overlapped, or null.
ETHEntity doDamage(ETHEntity thisEntity, const float knockForce, const bool knockFromOwner,
                   const bool collideEverything, const uint attackMode, const float damageMultiplier) {
    ETHEntity r = nullptr;
    const uint dir = thisEntity->GetUIntData("direction");
    vector2 bucket = thisEntity->GetCurrentBucket();
    const bool mainChar = (thisEntity->CheckCustomData("mainCharacter") != DT_NODATA);

    // doDamage.as:53-75: the attack's bucket, the one it faces, and for the
    // players' attacks the ones below and above too - in this order, which is
    // the order targets are hit in (and which one `r` ends up being).
    ETHEntityArray entities;
    GetEntitiesFromBucket(bucket, entities);

    if (dir == LEFT) {
        GetEntitiesFromBucket(bucket + vector2(-1.0f, 0.0f), entities);

        if (mainChar) {
            GetEntitiesFromBucket(bucket + vector2(0.0f, 1.0f), entities);
            GetEntitiesFromBucket(bucket + vector2(0.0f, -1.0f), entities);
        }
    } else {
        GetEntitiesFromBucket(bucket + vector2(1.0f, 0.0f), entities);

        if (mainChar) {
            GetEntitiesFromBucket(bucket + vector2(0.0f, 1.0f), entities);
            GetEntitiesFromBucket(bucket + vector2(0.0f, -1.0f), entities);
        }
    }

    bool isResistant = false;   // doDamage.as:77: set once per CALL, never reset per target
    bool pvpMode = (thisEntity->CheckCustomData("pvpMode") != DT_NODATA);


    int damage = 0;
    if (mainChar) {
        // doDamage.as:84-86: a main-character attack without a playerId would
        // index g_charLevel[2], out of bounds, and abort; every one has one.
        uint ownerPlayerId = MAX_PLAYERS;
        if (thisEntity->CheckCustomData("playerId") != DT_NODATA)
            ownerPlayerId = thisEntity->GetUIntData("playerId");
        // doDamage.as:87: D + int(D*level/4) in float, truncated; then times
        // the TRUNCATED multiplier.
        damage = ((thisEntity->GetIntData("damage"))
                  + static_cast<int>(static_cast<float>(thisEntity->GetIntData("damage"))
                                     * (static_cast<float>(g_charLevel[ownerPlayerId]) / 4)))
                 * static_cast<int>(damageMultiplier);
    } else {
        damage = thisEntity->GetIntData("damage") * static_cast<int>(damageMultiplier);
    }

    const int ownerID = thisEntity->GetIntData("ownerID");

    collisionBox thisBox = getAbsoluteCollisionBox(thisEntity);
    const uint size = entities.size();
    for (uint t = 0; t < size; t++) {
        if (!entities[t]->Collidable())
            continue;

        if (entities[t]->GetID() == thisEntity->GetID())
            continue;

        if (thisEntity->GetIntData("ownerID") == entities[t]->GetID())
            continue;

        // prevent two blows from the same character to hit each other
        if (entities[t]->CheckCustomData("ownerID") != DT_NODATA)
            if (thisEntity->GetIntData("ownerID") == entities[t]->GetIntData("ownerID"))
                continue;

        if (entities[t]->CheckCustomData("hp") == DT_NODATA && !collideEverything)
            continue;

        if (collideEverything) {
            if (entities[t]->GetEntityName() == "npc_wall.ent")
                continue;
        }

        // don't let friendly fire happen
        if (!pvpMode)
            if (isAMainCharacter(entities[t]) && thisEntity->CheckCustomData("mainCharacter") != DT_NODATA)
                continue;

        // don't let it hit friend's attacks
        if (!pvpMode)
            if (entities[t]->CheckCustomData("mainCharacter") != DT_NODATA && thisEntity->CheckCustomData("mainCharacter") != DT_NODATA)
                continue;

        collisionBox box = getAbsoluteCollisionBox(entities[t]);

        if (checkBoxHit(box, thisBox)) {
            r = entities[t];
            // doDamage.as:138: how a fire ball learns another attack touched
            // it (spells.as:147).
            entities[t]->AddStringData("hitBy", thisEntity->GetEntityName());

            // doDamage.as:140-141: every frame of overlap, not only the first hit.
            if (isAMainCharacter(entities[t]))
                g_camera.startEarthquake(min(10.0f, static_cast<float>(damage)));

            if (entities[t]->CheckCustomData("hp") == DT_NODATA)
                continue;

            // adds a custom data whose name equals its id so we can easily
            // check what entities it has hit
            const string dataName = "id" + Str(entities[t]->GetID());
            if (thisEntity->CheckCustomData(dataName) == DT_NODATA) {
                r = entities[t];

                // check it the target is resistant to this kind of blow
                if (entities[t]->CheckCustomData("fireResistant") != DT_NODATA) {
                    if (attackMode == ATTACK_MODE_FIRE && entities[t]->GetUIntData("fireResistant") != 0) {
                        isResistant = true;
                    }
                }

                // give damage and attach attacker'ss ID to the target
                thisEntity->AddIntData(dataName, 0);
                // doDamage.as:164: integer division written back into `damage`,
                // so each later target in this call is divided again.
                damage = (isResistant) ? damage / 5 : damage;
                addToHp(entities[t], -damage);
                g_messages.addMessage(-damage, entities[t]->GetPositionXY());

                // if we're on pvp mode and the character died, assign a PVP point to the opponent
                if (entities[t]->CheckCustomData("pvpMode") != DT_NODATA) {
                    if (entities[t]->GetIntData("hp") <= 0) {
                        if (isAMainCharacter(entities[t])) {
                            ETHEntity attacker = SeekEntity(ownerID);
                            // doDamage.as:176-178: tested BEFORE the null check,
                            // so a vanished attacker throws here and aborts the
                            // running callback, as AngelScript did (the hp loss
                            // and the damage number above have happened). Kept.
                            if (isAMainCharacter(attacker)) {
                                if (attacker != nullptr) {
                                    const uint playerId = attacker->GetUIntData("playerId");
                                    g_pvpPoints[playerId]++;
                                }
                            } else {
                                // killed by an NPC's sword or fire ball (hazards such
                                // as lava shots never call doDamage): the victim
                                // loses a point
                                const uint playerId = entities[t]->GetUIntData("playerId");
                                g_pvpPoints[playerId]--;
                            }
                        } else {
                            // doDamage.as:192-193: no null check at all here; a
                            // vanished attacker throws in isAMainCharacter.
                            ETHEntity attacker = SeekEntity(ownerID);
                            if (isAMainCharacter(attacker)) {
                                const uint playerId = attacker->GetUIntData("playerId");
                                addToExp(playerId, entities[t]->GetIntData("expGiven"));
                            }

                        }
                    }
                }

                // knock back
                vector2 ownerPos(0.0f, 0.0f);   // AngelScript's vector2 default-constructs to zero
                if (knockFromOwner)
                    ownerPos = vector2(thisEntity->GetFloatData("ownerX"), thisEntity->GetFloatData("ownerY"));
                else
                    ownerPos = thisEntity->GetPositionXY();

                const vector2 backVector = normalize(entities[t]->GetPositionXY() - ownerPos) * knockForce;
                float pushBackBias = (isResistant) ? 0.2f : 1.0f;
                if (entities[t]->CheckCustomData("pushBackBias") != DT_NODATA)
                    pushBackBias = entities[t]->GetFloatData("pushBackBias");

                knockBack(entities[t], backVector * pushBackBias);

                // doDamage.as:217-218: the effect goes at the attack's ORIGIN
                // plus half its box width, not at the box centre.
                collisionBox hitBox = getAbsoluteCollisionBox(thisEntity);
                hitBox.pos = thisEntity->GetPosition() + vector3((dir == RIGHT) ? hitBox.size.x / 2 : -hitBox.size.x / 2, 0.0f, 0.0f);

                if (!isResistant) {
                    if (thisEntity->CheckCustomData("hit") != DT_NODATA) {
                        ETHEntity particle;
                        AddEntity(thisEntity->GetStringData("hit"), hitBox.pos, particle);
                        if (backVector.x > 0) {
                            particle->MirrorParticleSystemX(0, true);
                        }
                    }
                } else {
                    AddEntity("hit_fail.ent", hitBox.pos, 0.0f);
                }
                entities[t]->AddUIntData("action", CHASING);   // doDamage.as:236: a hit NPC gives chase
                //return r;
            }
        }
    }
    return r;
}

} // namespace Penumbra::Script
