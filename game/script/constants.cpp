// constants.as, ported. The constants themselves (PI .. MAX_PVP_POINTS,
// MAIN_CHARACTER_ENTITY0/1) are inline in Script.hpp so every file sees them
// as main.as's #includes made them; this file holds the two functions.
//
// Ported from extracted/app/constants.as. Its header, kept (the PORTUGUES block
// before it says the same in Portuguese):
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

// constants.as:73
string actionToString(const uint action)
{
    switch (action)
    {
    case STANDING:
        return "STANDING";
    case CHASING:
        return "CHASING";
    case COOLDOWN:
        return "COOLDOWN";
    }
    return "";
}

// constants.as:90. By entity NAME, which for an AddEntity'd entity is its .ent
// file name WITH the extension (Entity::GetEntityName), hence "bruxo.ent".
bool isAMainCharacter(ETHEntity thisEntity)
{
    if (thisEntity->GetEntityName() == MAIN_CHARACTER_ENTITY0)
        return true;
    if (thisEntity->GetEntityName() == MAIN_CHARACTER_ENTITY1)
        return true;
    return false;
}

} // namespace Penumbra::Script
