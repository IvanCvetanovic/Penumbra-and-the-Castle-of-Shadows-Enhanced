// interface.as, ported line by line: a player's HUD (hp, mp and xp bars, lives
// or PvP points) and the mana regeneration. The original is
// extracted/app/interface.as, part of Penumbra (Andre Santee, 2010), free
// software under the GNU Lesser General Public License, version 3 or (at your
// option) any later version.

#include "script/Script.hpp"

namespace Penumbra::Script {

// ENHANCEMENT E26 (Script.hpp): the HUD's frame on a touch screen.
TouchHud g_touchHud;

// E26: where the HUD's edges are: the screen's, or the frame's on a touch screen.
vector2 hudTopLeft()
{
    return vector2(g_touchHud.left, g_touchHud.top);
}

vector2 hudTopRight()
{
    return vector2(-g_touchHud.right, g_touchHud.top);
}

vector2 hudBottomLeft()
{
    return vector2(g_touchHud.left, -g_touchHud.bottom);
}

// E26: the HUD panel as a stone plaque. frame.png is the original's border
// for a screen's corner: stone along its bottom and right only, the top and
// left being the screen's edges. Inside a phone's safe area those two edges
// are bare, so the touch HUD adds them (drawPlaqueStone) and puts the panel
// at the safe area's corner instead of in the edge margin: the stone is the
// margin. Art measurements (px of frame.png): the stone strips are 16 thick,
// the bottom one is rows 47-62 with its shadow to row 66, the right one is
// columns 200-215 with its shadow to column 221.
namespace {
const float kStone = 16.0f;
const float kStoneTop = 47.0f;          // the bottom strip's first row
const float kStoneEnd = kStoneTop+16.0f;
const float kShadowEnd = 67.0f;         // the first row below the bottom strip's shadow
const float kPlaqueGap = 6.0f;          // the message lines' clearance below the plaque
}

vector2 hudBarsTopLeft()
{
    if (!g_touchHud.plaque) return hudTopLeft();
    return vector2(g_touchHud.panelLeft+kStone, g_touchHud.panelTop+kStone);
}

// interface.as's message lines start at (10, 70) below the frame's top; on a
// plaque no higher than its bottom edge (the stone and the shadow under it).
vector2 hudMessagesTopLeft()
{
    vector2 at = vector2(10,70)+hudTopLeft();
    if (g_touchHud.plaque)
        at.y = max(at.y, g_touchHud.panelTop+kStone+kShadowEnd+kPlaqueGap);
    return at;
}

// E26: the plaque's missing top and left stone for the panel at `idOffset`,
// each piece cut from frame.png's own strips so the stone matches. They touch
// but never overlap each other or frame.png's stone (the frame is drawn
// translucent, so an overlap would show twice), and most cuts fall on the
// strips' mortar joints (bottom strip: x 48, 112, 176, 192); the top strip's
// seam at x 200 does not, and reads as one more crack in the stone. The soft
// shadow beside the top-right corner and under the bottom-left one restarts
// frame.png's own ramp where the pieces meet (a few px, faint).
// The left piece is a whole strip beside the first player's bars; beside the
// next player's it is what the frame's pitch leaves past the previous panel's
// right strip, so the stone runs on from panel to panel along the top.
namespace {
void drawPlaqueStone(const vector2& idOffset, const uint playerId, const float framePitch, const float rail)
{
    const string frame = "interface/frame.png";
    const uint color = 0xA0FFFFFF;   // as the frame itself is drawn
    const float left = playerId == 0 ? kStone : framePitch-(rail+kStone);
    if (left <= 0.0f) return;
    // The top strip along the bars, its right corner (and that corner's share of the right shadow).
    DrawSpritePart(frame, idOffset+vector2(0,-kStone), vector2(0,kStoneTop), vector2(rail,kStoneEnd), color);
    DrawSpritePart(frame, idOffset+vector2(rail,-kStone), vector2(176,kStoneTop), vector2(192,kStoneEnd), color);
    DrawSpritePart(frame, idOffset+vector2(rail+kStone,-kStone), vector2(rail+kStone,0), vector2(rail+kStone+6,kStone), color);
    // The left strip: its top corner, its side (the right strip's own stone), its bottom corner and that corner's shadow.
    DrawSpritePart(frame, idOffset+vector2(-left,-kStone), vector2(48-left,kStoneTop), vector2(48,kStoneEnd), color);
    DrawSpritePart(frame, idOffset+vector2(-left,0), vector2(rail,0), vector2(rail+left,kStoneTop), color);
    DrawSpritePart(frame, idOffset+vector2(-left,kStoneTop), vector2(112-left,kStoneTop), vector2(112,kShadowEnd), color);
}
}

// E26: a bar's value on a touch screen. The original draws it in the bars'
// own dark colour (0xD0000000) riding the bar's end, where on a phone's red
// and blue it could not be read, and slides it off the screen's left edge as
// the bar empties (at 0 xp "lv: 1" is at x -27). Here: the HUD's light text
// (setupScene.as's 203,203,228) over the dark shadow its lives counter has
// (interface.as:81-82, 0xF0000000 1.5 px down and right), at the bar's end
// but never left of the bar's own left end.
void hudValue(const vector2& pos, const float barLeft, const string& text)
{
    const vector2 at(max(pos.x, barLeft+2.0f), pos.y);
    DrawText(at+vector2(1.5f,1.5f), text, "Arial Narrow", 16.0f, 0xF0000000);
    DrawText(at, text, "Arial Narrow", 16.0f, ARGB(255,203,203,228));
}

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
    // E26: from the frame's top-left corner on a touch screen ((0, 0) otherwise).
    const vector2 idOffset = vector2(frameSize.x*static_cast<float>(playerId), 0)+hudBarsTopLeft();   // E26: hudBarsTopLeft (hudTopLeft off the plaque)

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
    if (g_touchHud.on)                                                          // E26
        hudValue(idOffset+vector2(hpLength-textReturn, 0), idOffset.x, "hp: " + Str(hp));   // E26
    else                                                                        // E26
        DrawText(idOffset+vector2(hpLength-textReturn, 0), "hp: " + Str(hp), "Arial Narrow", 16.0f, 0xD0000000);

    DrawShapedSprite("interface/rail.png", idOffset+vector2(0,16), vector2(rail,height), 0xFFFFFFFF);   // interface.as:67
    DrawShapedSprite("interface/mp.png", idOffset+vector2(0,16), vector2(mpLength,height), 0xFFFFFFFF);
    if (g_touchHud.on)                                                          // E26
        hudValue(idOffset+vector2(mpLength-textReturn, height), idOffset.x, "mp: " + Str(mp));   // E26
    else                                                                        // E26
        DrawText(idOffset+vector2(mpLength-textReturn, height), "mp: " + Str(mp), "Arial Narrow", 16.0f, 0xD0000000);

    DrawShapedSprite("interface/rail.png", idOffset+vector2(0,32), vector2(rail,height), 0xFFFFFFFF);   // interface.as:71
    DrawShapedSprite("interface/xp.png", idOffset+vector2(0,32), vector2(xpLength,height), 0xFFFFFFFF);
    if (g_touchHud.on)                                                          // E26
        hudValue(idOffset+vector2(xpLength-(textReturn*0.6f), height*2), idOffset.x, "lv: " + Str(g_charLevel[playerId]));   // E26
    else                                                                        // E26
        DrawText(idOffset+vector2(xpLength-(textReturn*0.6f), height*2), "lv: " + Str(g_charLevel[playerId]), "Arial Narrow", 16.0f, 0xD0000000);

    const uint textColor = ARGB(200,203,203,228);
    if (thisEntity->CheckCustomData("pvpMode") == DT_NODATA)          // interface.as:76
    {
        // Campaign: only player 0 shows the lives, with a drop shadow.
        if (playerId == 0)
        {
            DrawSprite("interface/skull_interface.png", vector2(rail*2+48.0f,0)+hudBarsTopLeft(), 0xFFFFFFFF);   // E26: hudBarsTopLeft
            DrawText(vector2(rail*2+48.0f+21.5f,1.5f)+hudBarsTopLeft(), "" + Str(g_lives), "Arial Black", 17, 0xF0000000);   // E26
            DrawText(vector2(rail*2+48.0f+20,0)+hudBarsTopLeft(), "" + Str(g_lives), "Arial Black", 17, textColor);   // E26
        }
    }
    else
    {
        // PvP: the "shadow" is drawn at the very spot of the text, with no
        // offset, unlike the lives above (interface.as:90-91).
        const vector2 skullPos = vector2(idOffset.x, idOffset.y+frameSize.y)+vector2(16,16);   // E26: idOffset.y, 0 but in a frame
        DrawSprite("interface/skull_interface.png", skullPos, 0xFFFFFFFF);
        const string text = ""+Str(g_pvpPoints[playerId]);
        DrawText(skullPos+vector2(30,0), text, "Arial Black", 17, 0xF0000000);
        DrawText(skullPos+vector2(30,0), text, "Arial Black", 17, textColor);
    }

    DrawSprite("interface/frame.png", idOffset+vector2(0,0), 0xA0FFFFFF);   // interface.as:94
    if (g_touchHud.plaque)                                                    // E26
        drawPlaqueStone(idOffset, playerId, frameSize.x, rail);               // E26
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
