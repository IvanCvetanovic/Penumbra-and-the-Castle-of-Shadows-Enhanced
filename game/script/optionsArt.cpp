// ENHANCEMENT E27 (not in the original): the helpers that draw the art the port added for the
// options screen (game/data/images/options/). Written for this   // E29
// enhanced edition and kept with the ported scripts it is called from, under the same licence as
// them (LICENSE.md: LGPL-3.0-or-later for game/script/).

#include <algorithm>

#include "script/Script.hpp"

namespace Penumbra::Script {

// The data folder, set by the layer (and the suites): "" is none, and every helper then draws nothing.
string g_artDir;   // E27

string optionsArtPath(const string& name)
{
    if (g_artDir.empty())
        return string();
    // Absolute and with forward slashes: ReadPath and the texture cache take an absolute path
    // as it is, which is how this art (not the original's, so not under the game root) is found.
    string dir = g_artDir;
    std::replace(dir.begin(), dir.end(), '\\', '/');
    while (!dir.empty() && dir.back() == '/')
        dir.pop_back();
    return dir+"/images/options/"+name;
}

void loadOptionsArt()
{
    if (g_artDir.empty())
        return;
    for (const char* name : kOptionsArt)
        LoadSprite(optionsArtPath(string(name)+".png"));
}

bool optionsArtReady()
{
    if (g_artDir.empty())
        return false;
    const vector2 own = GetSpriteSize(optionsArtPath("check_on.png"));
    return own.x > 0.0f && own.y > 0.0f;
}

void drawOptionsIcon(const string& name, const vector2& pos, const float size, const uint8 alpha)
{
    if (g_artDir.empty() || !(size > 0.0f) || alpha == 0)
        return;
    const string path = optionsArtPath(name+".png");
    // GetSpriteSize never loads: a screen that did not call loadOptionsArt draws no icons, not
    // a wrong one.
    const vector2 own = GetSpriteSize(path);
    if (!(own.x > 0.0f && own.y > 0.0f))
        return;
    const float height = size*own.y/own.x;
    DrawShapedSprite(path, pos+vector2(0.0f, (size-height)*0.5f), vector2(size, height), ARGB(alpha, 255, 255, 255));
}

void drawPanel(const vector2& pos, const vector2& size, const uint8 alpha, const float slice)
{
    if (g_artDir.empty() || !(size.x > 0.0f && size.y > 0.0f) || alpha == 0)
        return;
    const string path = optionsArtPath("panel.png");
    const vector2 own = GetSpriteSize(path);
    if (!(own.x > 0.0f && own.y > 0.0f))
        return;

    // The art's corners are kPanelSlice px (less in an image too small for two); they are drawn
    // `slice` px, no more than half of a side, so a panel under two slices wide or high has
    // smaller corners and no middle rather than overlapping ones.
    const float sx = std::min(kPanelSlice, own.x*0.5f);
    const float sy = std::min(kPanelSlice, own.y*0.5f);
    const float corner = std::max(slice, 0.0f);
    const float dx = std::min(corner, size.x*0.5f);
    const float dy = std::min(corner, size.y*0.5f);
    const float srcX[4] = {0.0f, sx, own.x-sx, own.x};
    const float srcY[4] = {0.0f, sy, own.y-sy, own.y};
    const float dstX[4] = {pos.x, pos.x+dx, pos.x+size.x-dx, pos.x+size.x};
    const float dstY[4] = {pos.y, pos.y+dy, pos.y+size.y-dy, pos.y+size.y};
    const uint color = ARGB(alpha, 255, 255, 255);
    for (int j=0; j<3; j++)
    {
        for (int i=0; i<3; i++)
        {
            // Each piece takes its edges from the same four numbers as its neighbours, so they meet
            // exactly. The middle of a panel with no room for one has no width or height: skipped.
            const float w = dstX[i+1]-dstX[i];
            const float h = dstY[j+1]-dstY[j];
            if (!(w > 0.0f && h > 0.0f))
                continue;
            DrawShapedSpritePart(path, vector2(dstX[i], dstY[j]), vector2(w, h),
                                 vector2(srcX[i], srcY[j]), vector2(srcX[i+1], srcY[j+1]), color);
        }
    }
}

} // namespace Penumbra::Script
