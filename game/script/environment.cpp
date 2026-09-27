// environment.as, ported (extracted/app/environment.as; Penumbra by Andre Santee, LGPL-3.0-or-later).
// The sky: storm lightning (clouds.ent), fog and dawn. setupScene turns each
// "environment" marker into one of these entities, carrying the marker's
// bgColor and bgImage (setupScene.as:192-216).

#include "script/Script.hpp"

namespace Penumbra::Script {

// environment.as:43. A lightning flash is 1600 ms over 15 steps: the sky goes
// grey-to-white (peak*255) while the ambient light goes towards black
// ((1-peak)*saved), so the world flashes into silhouettes.
void ETHCallback_clouds(ETHEntity thisEntity)
{
    const uint lightningTime = 1600;
    const array<float> peaks = {
        0.7f,
        0.8f,
        0.9f,
        1.0f,
        1.0f,
        0.8f,
        0.6f,
        0.4f,
        0.8f,
        1.0f,
        0.9f,
        0.8f,
        0.6f,
        0.3f,
        0.1f
    };

    positionEnvironmentElements(thisEntity);

    // environment.as:66-79: first call only (or never, for an entity restored
    // from checkpoint.esc with these keys already set). lastLightning = now
    // and nextLightning = 10 s make the first frames fall in the flash branch
    // below: a fresh clouds entity starts with a flash and no thunder (a
    // restored one resumes from its saved lastLightning instead).
    if (thisEntity->CheckCustomData("ambientR") == DT_NODATA)
    {
        thisEntity->AddFloatData("ambientR", GetAmbientLight().x);
        thisEntity->AddFloatData("ambientG", GetAmbientLight().y);
        thisEntity->AddFloatData("ambientB", GetAmbientLight().z);
    }
    if (thisEntity->CheckCustomData("lastLightning") == DT_NODATA)
    {
        thisEntity->AddUIntData("lastLightning", GetTime());
    }
    if (thisEntity->CheckCustomData("nextLightning") == DT_NODATA)
    {
        thisEntity->AddUIntData("nextLightning", 10000);
    }

    const vector3 ambient(
        thisEntity->GetFloatData("ambientR"),
        thisEntity->GetFloatData("ambientG"),
        thisEntity->GetFloatData("ambientB"));

    const uint elapsed = GetTime()-thisEntity->GetUIntData("lastLightning");
    if (elapsed >= thisEntity->GetUIntData("nextLightning"))
    {
        PlaySample("soundfx/thunder.mp3");
        thisEntity->AddUIntData("lastLightning", GetTime());
        // environment.as:91: rand(6000) is [0, 6000] inclusive, then int -> uint.
        thisEntity->AddUIntData("nextLightning", static_cast<uint>(6000+rand(6000)));
    }
    else if (elapsed < lightningTime)
    {
        // environment.as:95: elapsed < 1600, so the float is in [0, 15) and
        // the float -> uint conversion is exact truncation.
        uint index = static_cast<uint>((static_cast<float>(elapsed)/static_cast<float>(lightningTime))*static_cast<float>(peaks.length()));
        if (index >= peaks.length())
            index = peaks.length()-1;
        const float peak = peaks[index];
        uint8 color = ToUint8(peak*255.0f);
        SetBackgroundColor(ARGB(255, color, color, color));
        vector3 peakColor(1-peak, 1-peak, 1-peak);
        peakColor.x *= ambient.x;
        peakColor.y *= ambient.y;
        peakColor.z *= ambient.z;
        SetAmbientLight(peakColor);
    }
    else
    {
        // environment.as:109: gameover.esc's clouds.ent has no bgColor, so
        // there the sky keeps the last flash's grey.
        if (thisEntity->CheckCustomData("bgColor") != DT_NODATA)
            SetBackgroundColor(thisEntity->GetUIntData("bgColor"));
        SetAmbientLight(ambient);
    }
}

// environment.as:115
void ETHCallback_fog(ETHEntity thisEntity)
{
    positionEnvironmentElements(thisEntity);
    if (thisEntity->CheckCustomData("bgColor") != DT_NODATA)
        SetBackgroundColor(thisEntity->GetUIntData("bgColor"));
}

// environment.as:122. Keeps the entity (and so its particle emitters) at a
// fixed place on screen, and sets the background image every frame.
void positionEnvironmentElements(ETHEntity thisEntity)
{
    const vector2 cam = GetCameraPos();
    thisEntity->SetPosition(vector3(cam.x+64, cam.y+350, 0));

    if (thisEntity->CheckCustomData("bgImage") != DT_NODATA)
    {
        SetBackgroundImage("entities/" + thisEntity->GetStringData("bgImage"));
        if (thisEntity->GetStringData("bgImage") == "planets.png")
        {
            // environment.as:132-136: an additive 329x329 planet in the sky at
            // 60% of the screen width, not a full-screen backdrop.
            SetBackgroundAlphaAdd();
            vector2 screenSize(GetScreenSize());
            const vector2 bgMin(screenSize.x*0.6f, 100);
            const vector2 bgMax(bgMin+vector2(329,329));
            PositionBackgroundImage(bgMin, bgMax);
        }
        else
        {
            PositionBackgroundImage(vector2(0,0), GetScreenSize());
        }
    }
}

// environment.as:145. pvp_lv1's dawn: unlike clouds and fog it is not moved
// with the camera.
void ETHCallback_dawn(ETHEntity thisEntity)
{
    if (thisEntity->CheckCustomData("bgColor") != DT_NODATA)
        SetBackgroundColor(thisEntity->GetUIntData("bgColor"));

    if (thisEntity->CheckCustomData("bgImage") != DT_NODATA)
    {
        SetBackgroundImage("entities/" + thisEntity->GetStringData("bgImage"));
        PositionBackgroundImage(vector2(0,0), GetScreenSize());
    }
}

} // namespace Penumbra::Script
