// potions.as, ported: a potion heals EVERY main character within 20 px of it,
// in bucket-list order, until one at full HP ends the search (potions.as:53-54).
// Original: extracted/app/potions.as (Penumbra, LGPL-3).

#include "script/Script.hpp"

namespace Penumbra::Script {

// potions.as:43. Only the potion's own bucket is searched.
//
// Kept as the original has it: a main character at full HP RETURNS, ending the
// search for everyone this frame (potions.as:53-54); and there is no break
// after a pickup, so the loop goes on over its own copy of the bucket with the
// potion already deleted - a handle stays readable after DeleteEntity, which
// potions.as:57 relies on for the position.
void doPotion(ETHEntity thisEntity) {
    ETHEntityArray entityArray;
    GetEntitiesFromBucket(thisEntity->GetCurrentBucket(), entityArray);
    for (uint t = 0; t < entityArray.size(); t++) {
        if (getDist(entityArray[t]->GetPositionXY(), thisEntity->GetPositionXY()) < 20.0f) {
            if (isAMainCharacter(entityArray[t])) {
                if (entityArray[t]->GetIntData("hp") >= entityArray[t]->GetIntData("maxHp"))
                    return;
                addToHp(entityArray[t], thisEntity->GetIntData("hp"));
                DeleteEntity(thisEntity);
                AddEntity("potion_pick.ent", thisEntity->GetPosition() + vector3(0.0f, 0.0f, 16.0f), 0.0f);
            }
        }
    }
}

// potions.as:63. No entity file is named potion.ent, so nothing binds this.
void ETHCallback_potion(ETHEntity thisEntity) {
    doPotion(thisEntity);
}

// potions.as:68
void ETHCallback_potion_small(ETHEntity thisEntity) {
    doPotion(thisEntity);
}

// potions.as:73
void ETHCallback_potion_large(ETHEntity thisEntity) {
    doPotion(thisEntity);
}

} // namespace Penumbra::Script
