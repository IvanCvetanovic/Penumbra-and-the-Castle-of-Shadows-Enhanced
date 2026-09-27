// interface.as, ported line by line: a player's HUD (hp, mp and xp bars, lives
// or PvP points) and the mana regeneration. The original is
// extracted/app/interface.as, part of Penumbra (Andre Santee, 2010), free
// software under the GNU Lesser General Public License, version 3 or (at your
// option) any later version.

#include "script/Script.hpp"

namespace Penumbra::Script {

// interface.as:43. Player n's HUD sits one frame width to the right of player
// n-1's, in screen pixels (not scaled with the resolution).
void drawPlayerStatus(ETHEntity thisEntity)
{
    const float rail = 200.0f;
    const float height = 16.0f;
    const int maxHp = thisEntity->GetIntData("maxHp");
    const int maxMp = thisEntity->GetIntData("maxMp");
    const uint playerId = thisEntity->GetUIntData("playerId");
    const vector2 frameSize = GetSpriteSize("interface/frame.png");
    const vector2 idOffset(frameSize.x*static_cast<float>(playerId), 0);

    // interface.as:54 read global.lv<level> directly; data.enml has no lv20,
    // which left nextExp 0 and aborted this callback at the division below.
    // E7: expForLevel (Script.hpp) falls back to the last level that has one.
    // A 0 maxHp or maxMp still aborts at its division (interface.as:59-60),
    // as in the original, taking the rest of the caller's callback with it.
    const int nextExp = expForLevel(g_charLevel[playerId]);
    const int maxXp = nextExp;

    const int hp = thisEntity->GetIntData("hp");
    const int mp = thisEntity->GetIntData("mp");
    const float hpLength = DivF(static_cast<float>(hp), static_cast<float>(maxHp))*rail;   // interface.as:59
    const float mpLength = DivF(static_cast<float>(mp), static_cast<float>(maxMp))*rail;   // interface.as:60
    const float xpLength = DivF(static_cast<float>(g_exp[playerId]), static_cast<float>(maxXp))*rail;   // interface.as:61
    // Each bar's value rides at the bar's right end, this far back from it.
    const float textReturn = 45;
    DrawShapedSprite("interface/rail.png", idOffset+vector2(0,0), vector2(rail,height), 0xFFFFFFFF);   // interface.as:63
    DrawShapedSprite("interface/hp.png", idOffset+vector2(0,0), vector2(hpLength,height), 0xFFFFFFFF);
    DrawText(idOffset+vector2(hpLength-textReturn, 0), "hp: " + Str(hp), "Arial Narrow", 16.0f, 0xD0000000);

    DrawShapedSprite("interface/rail.png", idOffset+vector2(0,16), vector2(rail,height), 0xFFFFFFFF);   // interface.as:67
    DrawShapedSprite("interface/mp.png", idOffset+vector2(0,16), vector2(mpLength,height), 0xFFFFFFFF);
    DrawText(idOffset+vector2(mpLength-textReturn, height), "mp: " + Str(mp), "Arial Narrow", 16.0f, 0xD0000000);

    DrawShapedSprite("interface/rail.png", idOffset+vector2(0,32), vector2(rail,height), 0xFFFFFFFF);   // interface.as:71
    DrawShapedSprite("interface/xp.png", idOffset+vector2(0,32), vector2(xpLength,height), 0xFFFFFFFF);
    DrawText(idOffset+vector2(xpLength-(textReturn*0.6f), height*2), "lv: " + Str(g_charLevel[playerId]), "Arial Narrow", 16.0f, 0xD0000000);

    const uint textColor = ARGB(200,203,203,228);
    if (thisEntity->CheckCustomData("pvpMode") == DT_NODATA)          // interface.as:76
    {
        // Campaign: only player 0 shows the lives, with a drop shadow.
        if (playerId == 0)
        {
            DrawSprite("interface/skull_interface.png", vector2(rail*2+48.0f,0), 0xFFFFFFFF);
            DrawText(vector2(rail*2+48.0f+21.5f,1.5f), "" + Str(g_lives), "Arial Black", 17, 0xF0000000);
            DrawText(vector2(rail*2+48.0f+20,0), "" + Str(g_lives), "Arial Black", 17, textColor);
        }
    }
    else
    {
        // PvP: the "shadow" is drawn at the very spot of the text, with no
        // offset, unlike the lives above (interface.as:90-91).
        const vector2 skullPos = vector2(idOffset.x, frameSize.y)+vector2(16,16);
        DrawSprite("interface/skull_interface.png", skullPos, 0xFFFFFFFF);
        const string text = ""+Str(g_pvpPoints[playerId]);
        DrawText(skullPos+vector2(30,0), text, "Arial Black", 17, 0xF0000000);
        DrawText(skullPos+vector2(30,0), text, "Arial Black", 17, textColor);
    }

    DrawSprite("interface/frame.png", idOffset+vector2(0,0), 0xA0FFFFFF);   // interface.as:94
    DrawSprite("interface/blend.png", idOffset+vector2(0,0), textColor);
}

// interface.as:98. +incr mana every time more than `stride` ms have passed
// since the last increment.
void doMpRecovery(ETHEntity thisEntity, const uint stride, const int incr)
{
    if (thisEntity->CheckCustomData("lastMpIncr") == DT_NODATA)
    {
        thisEntity->AddUIntData("lastMpIncr", GetTime());
    }
    if (GetTime()-thisEntity->GetUIntData("lastMpIncr") > stride)     // interface.as:104 (uint32 wrap, as the original)
    {
        addToMp(thisEntity, incr);
        thisEntity->AddUIntData("lastMpIncr", GetTime());
    }
}

} // namespace Penumbra::Script
