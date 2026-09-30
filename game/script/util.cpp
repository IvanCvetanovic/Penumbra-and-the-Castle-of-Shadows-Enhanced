// util.as, ported: the shared physics, geometry and HUD helpers. Characters
// move by "forceX/forceY" and "knockBackX/knockBackY" custom data that move()
// integrates once per frame; every neighbour and collision query is built on
// buckets (docs/spec/30-ethanon-runtime.md), so these helpers are the game's
// physics engine. RIGHT/LEFT/DOWN/UP (util.as:238-241) are in Script.hpp.
//
// Ported from extracted/app/util.as. Its header, kept (the PORTUGUES block
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

// util.as:43. The first entity named entityName, searching the entity's own
// bucket, then +1, -1, +2, -2 on x (then the four diagonals unless
// horizontalOnly): with two matches, this order and each bucket list's order
// decide which one is returned.
ETHEntity findAmongNeighbourEntities(ETHEntity thisEntity, const string& entityName, const bool horizontalOnly)
{
    ETHEntityArray entityArray;
    const vector2 bucket = thisEntity->GetCurrentBucket();
    GetEntitiesFromBucket(bucket, entityArray);
    GetEntitiesFromBucket(bucket+vector2(1,0), entityArray);
    GetEntitiesFromBucket(bucket+vector2(-1,0), entityArray);
    GetEntitiesFromBucket(bucket+vector2(2,0), entityArray);
    GetEntitiesFromBucket(bucket+vector2(-2,0), entityArray);
    if (!horizontalOnly)
    {
        GetEntitiesFromBucket(bucket+vector2(1,1), entityArray);
        GetEntitiesFromBucket(bucket+vector2(-1,1), entityArray);
        GetEntitiesFromBucket(bucket+vector2(1,-1), entityArray);
        GetEntitiesFromBucket(bucket+vector2(-1,-1), entityArray);
    }
    uint size = entityArray.size();
    for (uint t=0; t<size; t++)
    {
        if (entityArray[t]->GetEntityName() == entityName)
        {
            return entityArray[t];
        }
    }
    return nullptr;
}

// util.as:70. The princess is looked for only when a second controller exists
// (util.as:75); with both found, the nearer one wins and a tie goes to the
// princess (handle1, util.as:84-87).
ETHEntity findMainCharNeighbours(ETHEntity thisEntity, const bool horizontalOnly)
{
    [[maybe_unused]] ETHEntity handle;   // util.as:72, declared and never used
    ETHEntity handle0 = findAmongNeighbourEntities(thisEntity, MAIN_CHARACTER_ENTITY0, horizontalOnly);

    if (!hasASecondController())
        return handle0;

    ETHEntity handle1 = findAmongNeighbourEntities(thisEntity, MAIN_CHARACTER_ENTITY1, horizontalOnly);

    if (handle0 != nullptr && handle1 != nullptr)
    {
        const float dist0 = getSquaredDist(thisEntity->GetPositionXY(), handle0->GetPositionXY());
        const float dist1 = getSquaredDist(thisEntity->GetPositionXY(), handle1->GetPositionXY());
        if (dist0<dist1)
            return handle0;
        else
            return handle1;
    }
    else if (handle0 != nullptr)
    {
        return handle0;
    }
    return handle1;
}

// util.as:96. The camera rect grown by SIZE_TOLERANCE (76) px on each side,
// inclusive at the edges.
bool isInScreen(ETHEntity thisEntity)
{
    const vector2 pos = thisEntity->GetPositionXY();
    const vector2 screenMin = GetCameraPos();
    const vector2 screenMax = GetCameraPos()+GetScreenSize();
    if (pos.x-SIZE_TOLERANCE > screenMax.x)
        return false;
    if (pos.x+SIZE_TOLERANCE < screenMin.x)
        return false;
    if (pos.y-SIZE_TOLERANCE > screenMax.y)
        return false;
    if (pos.y+SIZE_TOLERANCE < screenMin.y)
        return false;
    return true;
}

// util.as:112
bool isInScreen(const vector2& pos)
{
    const vector2 screenMin = GetCameraPos();
    const vector2 screenMax = GetCameraPos()+GetScreenSize();
    if (pos.x-SIZE_TOLERANCE > screenMax.x)
        return false;
    if (pos.x+SIZE_TOLERANCE < screenMin.x)
        return false;
    if (pos.y-SIZE_TOLERANCE > screenMax.y)
        return false;
    if (pos.y+SIZE_TOLERANCE < screenMin.y)
        return false;
    return true;
}

// util.as:127. GetCollisionBox is relative to the entity; this is the box in
// the world (z included, though checkBoxHit ignores it).
collisionBox getAbsoluteCollisionBox(ETHEntity thisEntity)
{
    collisionBox r = thisEntity->GetCollisionBox();
    r.pos += thisEntity->GetPosition();
    return r;
}

// util.as:134
float getLength(const vector2& v)
{
    return sqrt(v.x*v.x + v.y*v.y);
}

// util.as:139
float getDist(const vector2& v0, const vector2& v1)
{
    return getLength(v1-v0);
}

// util.as:144
float getSquaredDist(const vector2& v0, const vector2& v1)
{
    const vector2 v(v1-v0);
    return v.x*v.x + v.y*v.y;
}

// util.as:150. Only a vertical band: "on sight" is |dy| <= 76*mult, whatever
// the horizontal distance or what stands between.
bool isOnSight(ETHEntity thisEntity, ETHEntity other, const float mult)
{
    vector2 pos = thisEntity->GetPositionXY();
    vector2 otherPos = other->GetPositionXY();
    if (otherPos.y > pos.y+SIZE_TOLERANCE*mult)
        return false;
    if (otherPos.y < pos.y-SIZE_TOLERANCE*mult)
        return false;
    return true;
}

// util.as:161. Knock-backs accumulate; only their upward (negative) y part is
// kept (util.as:166). A missing key reads 0, so the first knock needs no setup.
void knockBack(ETHEntity thisEntity, const vector2& v)
{
    const float x = thisEntity->GetFloatData("knockBackX");
    const float y = thisEntity->GetFloatData("knockBackY");
    thisEntity->AddFloatData("knockBackX", x+v.x);
    thisEntity->AddFloatData("knockBackY", y+min(0.0f, v.y));
}

// util.as:169
vector2 getKnockBackVector(ETHEntity thisEntity)
{
    return vector2(thisEntity->GetFloatData("knockBackX"),thisEntity->GetFloatData("knockBackY"));
}

// util.as:174
void setKnockBackVector(ETHEntity thisEntity, const vector2& v)
{
    thisEntity->AddFloatData("knockBackX", v.x);
    thisEntity->AddFloatData("knockBackY", v.y);
}

// util.as:180
void applyForce(ETHEntity thisEntity, const vector2& v)
{
    const float x = thisEntity->GetFloatData("forceX");
    const float y = thisEntity->GetFloatData("forceY");
    thisEntity->AddFloatData("forceX", x+v.x);
    thisEntity->AddFloatData("forceY", y+v.y);
}

// util.as:188. One frame of motion: the force is px/s divided by the frame
// RATE (not scaled by the frame time), the knock-back is px/frame and decays
// by 0.8 a frame, and each, once it reaches 40% of the entity's smaller side,
// is cut to 90% of that.
void move(ETHEntity thisEntity)
{
    // if it's down and touching the ground, remove force Y to avoid unnecessary movement
    if (thisEntity->GetUIntData("touchingGround") == 1 && thisEntity->GetFloatData("forceY") > 0)
        thisEntity->AddFloatData("forceY", 0);

    vector2 v(thisEntity->GetFloatData("forceX"), thisEntity->GetFloatData("forceY"));
    const float fps = GetFPSRate() == 0 ? 60 : GetFPSRate();   // util.as:195
    v = (v/fps);

    // do not let it be greater than the entity's size to avoid passing through objects
    const float size = min(thisEntity->GetSize().x, thisEntity->GetSize().y)*0.4f;
    if (getLength(v) >= size)
    {
        v = normalize(v)*size*0.9f;
    }

    vector2 knockVector(thisEntity->GetFloatData("knockBackX"), thisEntity->GetFloatData("knockBackY"));
    if (getLength(knockVector) >= size)
    {
        knockVector = normalize(knockVector)*size*0.9f;
    }

    // The decayed knock-back is stored BEFORE it is applied (util.as:211-218),
    // so this frame already moves by 0.8 of it.
    knockVector *= 0.8f;
    if (abs(knockVector.x) < 0.1f)
        knockVector.x = 0.0f;
    if (abs(knockVector.y) < 0.1f)
        knockVector.y = 0.0f;
    setKnockBackVector(thisEntity, knockVector);

    thisEntity->AddToPositionXY(v+knockVector);
}

// util.as:221
vector2 getCurrentForce(ETHEntity thisEntity)
{
    const float x = thisEntity->GetFloatData("forceX");
    const float y = thisEntity->GetFloatData("forceY");
    return vector2(x,y);
}

// util.as:228
void setForceX(ETHEntity thisEntity, const float f)
{
    thisEntity->AddFloatData("forceX", f);
}

// util.as:233
void setForceY(ETHEntity thisEntity, const float f)
{
    thisEntity->AddFloatData("forceY", f);
}

// util.as:242. GetAngle measures from +Y towards +X (its atan2 arguments are
// swapped, docs/spec/30-ethanon-runtime.md), which is why +X is 45..135 here.
uint findDirection(const vector2& v)
{
    const float angle = GetAngle(v);
    const float angleInDregree = radianToDegree(angle);

    uint dir;
    if (angleInDregree >= 45 && angleInDregree < 135)
    {
        dir = RIGHT;
    }
    else if (angleInDregree >= 135 && angleInDregree < 225)
    {
        dir = UP;
    }
    else if (angleInDregree >= 225 && angleInDregree < 315)
    {
        dir = LEFT;
    }
    else
    {
        dir = DOWN;
    }
    return dir;
}

// util.as:267. The direction from box a towards box b. Only b's half size
// decides the alignment tests (util.as:274-279).
uint findBoxDirection(const collisionBox& a, const collisionBox& b)
{
    vector2 halfSizeA = vector2(a.size.x/2, a.size.y/2);
    vector2 halfSizeB = vector2(b.size.x/2, b.size.y/2);
    vector2 posA = vector2(a.pos.x, a.pos.y);
    vector2 posB = vector2(b.pos.x, b.pos.y);

    const bool verticalyAligned =
        (posA.x > posB.x-halfSizeB.x)
    &&  (posA.x < posB.x+halfSizeB.x);
    const bool horizontalyAligned =
        (posA.y > posB.y-halfSizeB.y)
    &&  (posA.y < posB.y+halfSizeB.y);

    if (horizontalyAligned)
    {
        if (posA.x < posB.x)
            return RIGHT;
        else
            return LEFT;
    }
    if (verticalyAligned)
    {
        if (posA.y < posB.y)
            return DOWN;
        else
            return UP;
    }

    // um pouco de "engenharia alternativa" [a bit of "alternative engineering"]
    // util.as:296-300: b below a (a's bottom at most 5 px into b's top) is DOWN.
    if (posA.y+halfSizeA.y-5 <= posB.y-halfSizeB.y)
    {
        return DOWN;
    }

    return findDirection(b.pos - a.pos);   // util.as:302, the vector3 overload
}

// util.as:305
string directionToString(const uint dir)
{
    switch (dir)
    {
    case LEFT:
        return "LEFT";
    case RIGHT:
        return "RIGHT";
    case UP:
        return "UP";
    case DOWN:
        return "DOWN";
    }
    return "";
}

// util.as:321
uint findDirection(const vector3& v)
{
    return findDirection(vector2(v.x,v.y));
}

// util.as:326. The buckets the entity is moving into (and, with
// forceBottomBuckets, the three below it), appended in this order.
ETHEntityArray findDestinationBuckets(ETHEntity thisEntity, const vector2& dir, const bool forceBottomBuckets)
{
    ETHEntityArray entityArray;
    vector2 bucket = thisEntity->GetCurrentBucket();

    if (dir.x < 0)
        GetEntitiesFromBucket(bucket+vector2(-1,0), entityArray);
    if (dir.x > 0)
        GetEntitiesFromBucket(bucket+vector2(1,0), entityArray);

    if (dir.y > 0 || forceBottomBuckets)
        GetEntitiesFromBucket(bucket+vector2(0,1), entityArray);
    if (dir.y < 0)
        GetEntitiesFromBucket(bucket+vector2(0,-1), entityArray);

    if (forceBottomBuckets)
    {
        GetEntitiesFromBucket(bucket+vector2(-1,1), entityArray);
        GetEntitiesFromBucket(bucket+vector2(1,1), entityArray);
    }

    return entityArray;
}

// util.as:350. 2-D and inclusive: boxes that only touch at an edge hit.
bool checkBoxHit(collisionBox box0, collisionBox box1)
{
    const vector3 halfSize0 = box0.size/2.0f;
    const vector3 halfSize1 = box1.size/2.0f;

    const vector3 v3Min0 = box0.pos-halfSize0;
    const vector3 v3Max0 = box0.pos+halfSize0;

    const vector3 v3Min1 = box1.pos-halfSize1;
    const vector3 v3Max1 = box1.pos+halfSize1;

    if (v3Min0.x > v3Max1.x)
        return false;
    if (v3Min0.y > v3Max1.y)
        return false;

    if (v3Max0.x < v3Min1.x)
        return false;
    if (v3Max0.y < v3Min1.y)
        return false;

    return true;
}

// util.as:374
void drawRect(const uint rectColor)
{
    DrawRectangle(vector2(0,0), GetScreenSize(), rectColor, rectColor, rectColor, rectColor);
}

// util.as:379. Black over the screen, fading from opaque to clear over
// LIVE_FADE_IN_TIME ms; true once that has passed.
bool fadeIn(const uint startTime)
{
    const uint elapsed = GetTime()-startTime;
    if (elapsed < LIVE_FADE_IN_TIME)
    {
        const float alpha = 1.0f-(static_cast<float>(elapsed)/static_cast<float>(LIVE_FADE_IN_TIME));
        const uint rectColor = ARGB(ToUint8(alpha*255.0f),0,0,0);
        DrawRectangle(vector2(0,0), GetScreenSize(), rectColor, rectColor, rectColor, rectColor);
        return false;
    }
    return true;
}

// util.as:392. The reverse of fadeIn. bias is written only while fading: the
// call that returns true leaves it as the caller set it.
bool fadeOut(const uint startTime, float& bias)
{
    const uint elapsed = GetTime()-startTime;
    if (elapsed < LIVE_FADE_IN_TIME)
    {
        const float alpha = (static_cast<float>(elapsed)/static_cast<float>(LIVE_FADE_IN_TIME));
        bias = alpha;
        const uint rectColor = ARGB(ToUint8(alpha*255.0f),0,0,0);
        DrawRectangle(vector2(0,0), GetScreenSize(), rectColor, rectColor, rectColor, rectColor);
        return false;
    }
    return true;
}

// util.as:406. data.enml's global.lvN is the experience level N needs. enml's
// getInt leaves nextExp UNWRITTEN when the key is missing (lv20, and lv31
// onwards), so here it keeps what it held: the previous level's threshold
// inside the loop, but 0 on a call's first read - so at level 20 the next gain
// passes lv20 at once, and from level 31 the loop never ends (every threshold
// reads 0). Kept as ported; what the original's uninitialised &out temporary
// copied back is an open question (docs/spec/30-ethanon-runtime.md). In that
// runaway loop g_charLevel's ++ (util.as:414) eventually passes INT_MAX:
// AngelScript wraps, C++ leaves it undefined (MSVC wraps) - moot once the hang
// gets its ruling.
void addToExp(const uint player, const int exp)
{
    g_exp[player] += exp;
    // E7: expForLevel in place of the original's two direct lookups
    // (util.as:409, :415), which left nextExp stale on a missing key.
    int nextExp = expForLevel(g_charLevel[player]);
    while (g_exp[player] >= nextExp)
    {
        const int diff = g_exp[player]-nextExp;
        g_charLevel[player]++;
        nextExp = expForLevel(g_charLevel[player]);
        g_exp[player] = diff;
    }
}

// E7 (Script.hpp). Searches down from the level asked for; a level at or
// below 0, or a data.enml with no lv keys at all, needs one point, so a
// broken file cannot turn this into a loop either.
int expForLevel(const int level)
{
    for (int l = level; l >= 1; --l)
    {
        int value = 0;
        if (g_gameData.getInt("global", "lv" + Str(l), value) && value > 0)
            return value;
    }
    return 1;
}

// util.as:420. Clamped at 0, and at maxHp only if the entity has that key.
void addToHp(ETHEntity thisEntity, const int value)
{
    int hp = thisEntity->GetIntData("hp");

    if (thisEntity->CheckCustomData("maxHp") == DT_NODATA)
    {
        hp = max(0, value+hp);
    }
    else
    {
        hp = max(0, min(value+hp, thisEntity->GetIntData("maxHp")));
    }
    thisEntity->AddIntData("hp", hp);
}

// util.as:435
void addToMp(ETHEntity thisEntity, const int value)
{
    int mp = thisEntity->GetIntData("mp");

    if (thisEntity->CheckCustomData("maxMp") == DT_NODATA)
    {
        mp = max(0, value+mp);
    }
    else
    {
        mp = max(0, min(value+mp, thisEntity->GetIntData("maxMp")));
    }
    thisEntity->AddIntData("mp", mp);
}

// util.as:450. A black copy at half the alpha, offset by a tenth of the size,
// under the coloured text.
void shadowText(const vector2& pos, const string& text, const string& font, const float size,
                const uint8 a, const uint8 r, const uint8 g, const uint8 b)
{
    DrawText(pos+vector2(size*0.1f, size*0.1f), text, font, size, ARGB(static_cast<uint8>(a/2),0,0,0));
    DrawText(pos, text, font, size, ARGB(a,r,g,b));
}

// ENHANCEMENT E24 (not in the original): the same in a box whose right edge is
// rtlRight, where a right-to-left language ends the text (HudCmd::rtlRight);
// the shadow's edge is offset with it. Left to right it draws as the above.
void shadowText(const vector2& pos, const string& text, const string& font, const float size,
                const uint8 a, const uint8 r, const uint8 g, const uint8 b, const float rtlRight)
{
    DrawText(pos+vector2(size*0.1f, size*0.1f), text, font, size, ARGB(static_cast<uint8>(a/2),0,0,0), rtlRight+size*0.1f);
    DrawText(pos, text, font, size, ARGB(a,r,g,b), rtlRight);
}

// util.as:457. "Carregando..." [Loading...], with the original's trailing \n.
void loadingMessage()
{
    shadowText(vector2(20,GetScreenSize().y-70), "Carregando...\n", "Arial Narrow", 60, 255, 203, 203, 228);
}

// util.as:462. The first entity named `name` among the visible buckets' entities.
ETHEntity findEntityInScreen(const string& name)
{
    ETHEntityArray entities;
    GetVisibleEntities(entities);
    for (uint t=0; t<entities.size(); t++)
    {
        if (entities[t]->GetEntityName() == name)
        {
            return entities[t];
        }
    }
    return nullptr;
}

} // namespace Penumbra::Script
