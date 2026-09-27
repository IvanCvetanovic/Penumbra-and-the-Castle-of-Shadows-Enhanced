// gameover.as, ported (extracted/app/gameover.as; Penumbra by Andre Santee, LGPL-3.0-or-later).
// scenes/gameover.esc's preLoop and loop, loaded when the main character's
// death fade ends with g_lives < 0 (controlCharacters.as:449).

#include "script/Script.hpp"

namespace Penumbra::Script {

uint g_gameOverStartTime = 0;   // gameover.as:43

// gameover.as:45
void gameOverPreLoop()
{
    LoadSoundEffect("soundfx/thunder.mp3");
    LoadSoundEffect("soundfx/laugh_king.mp3");
    LoadSoundEffect("soundfx/gameover.mp3");

    LoadSprite("entities/gameover.png");

    PlaySample("soundfx/laugh_king.mp3");
    PlaySample("soundfx/gameover.mp3");
    SetBackgroundColor(0xFF000000);
    SetAmbientLight(vector3(0,0,0));
    // gameover.as:57 `GetScreenSize()/2`: glm's vector/scalar operator needs
    // the scalar in the vector's own type.
    const vector2 screenMiddle = GetScreenSize()/2.0f;
    AddEntity("bruxo_dead.ent", vector3(screenMiddle.x, screenMiddle.y, 0), 0.0f);
    g_gameOverStartTime = GetTime();
}

// gameover.as:62
void gameOverLoop()
{
    const vector2 spriteSize = GetSpriteSize("entities/gameover.png");
    const vector2 screenMiddle = GetScreenSize()/2.0f;
    DrawSprite("entities/gameover.png", vector2(screenMiddle.x-spriteSize.x/2, screenMiddle.y-200.0f), 0xFFFFFFFF);

    fadeIn(g_gameOverStartTime);
    waitForInputToMenu();
}

} // namespace Penumbra::Script
