// cameraManager.as, ported line by line: the camera's dead-zone follow of the
// player (or of both players' midpoint), the vertical earthquake shake, and the
// pixel snapping. The original is extracted/app/cameraManager.as, part of
// Penumbra (Andre Santee, 2010), free software under the GNU Lesser General
// Public License, version 3 or (at your option) any later version.
//
// The constructor (cameraManager.as:45-58) only assigned constants; they are
// the members' initializers in Script.hpp.

#include "script/Script.hpp"

namespace Penumbra::Script {

// cameraManager.as:60. cameraSpeed is never read anywhere.
void CameraManager::setCameraSpeed(const float speed)
{
    m_cameraSpeed = speed;
}

// cameraManager.as:65
void CameraManager::setMainCharPos(const vector2& v, const uint player)
{
    if (player == 0)
        m_mainCharPos0 = v;
    else if (player == 1)
        m_mainCharPos1 = v;
}

// cameraManager.as:73
vector2 CameraManager::getMainCharPos(const uint player) const
{
    if (player == 0)
        return m_mainCharPos0;
    else if (player == 1)
        return m_mainCharPos1;
    return vector2(0,0);
}

// cameraManager.as:82. One step every trembleInterval ms: down by earthquake,
// then back up by the same amount and halve it, until it drops below 1.
void CameraManager::doEarthquake()
{
    if (m_earthquake != 0 && (GetTime()-m_lastTremble) >= m_trembleInterval)
    {
        if (m_eqForth)
        {
            AddToCameraPos(vector2(0,m_earthquake));
            m_backValue = m_earthquake;
            m_eqForth = !m_eqForth;
        }
        else
        {
            AddToCameraPos(vector2(0,-m_backValue));
            m_earthquake *= 0.5f;
            m_eqForth = !m_eqForth;
        }
        m_lastTremble = GetTime();
    }
    if (abs(m_earthquake) < 1.0f)                                     // cameraManager.as:100
    {
        m_earthquake = 0.0f;
    }
}

// cameraManager.as:106
void CameraManager::startEarthquake(const float force)
{
    m_earthquake = abs(force);
}

// cameraManager.as:111. An instant dead zone (no smoothing, no level bounds).
// pvpMode, speedBias and camMax are never used (cameraManager.as:111,114,126).
void CameraManager::adjustCameraPos([[maybe_unused]] const bool pvpMode)
{
    doEarthquake();
    [[maybe_unused]] float speedBias = 0;

    vector2 mainCharPos = m_mainCharPos0;

    // Player 1's position is (0,0) while there is no second player, so the
    // camera follows the midpoint only when there is one (cameraManager.as:118).
    if (m_mainCharPos1 != vector2(0,0))
    {
        mainCharPos = (m_mainCharPos0+m_mainCharPos1)/2.0f;
    }

    vector2 currentCamPos = GetCameraPos();
    const vector2 screenSize = GetScreenSize();
    // Both computed once, from the camera before any of the four adjustments.
    const vector2 screenMainCharPos = mainCharPos-currentCamPos;      // cameraManager.as:125
    [[maybe_unused]] const vector2 camMax = screenSize+currentCamPos;

    if (screenMainCharPos.x > (1-m_screenLimit.x)*screenSize.x)       // cameraManager.as:128
        currentCamPos = vector2(mainCharPos.x-((1-m_screenLimit.x)*screenSize.x), currentCamPos.y);
    if (screenMainCharPos.x < (m_screenLimit.x)*screenSize.x)
        currentCamPos = vector2(mainCharPos.x-((m_screenLimit.x)*screenSize.x), currentCamPos.y);

    if (screenMainCharPos.y > (1-m_screenLimit.y)*screenSize.y)       // cameraManager.as:133
        currentCamPos = vector2(currentCamPos.x, mainCharPos.y-((1-m_screenLimit.y)*screenSize.y));
    if (screenMainCharPos.y < (m_screenLimit.y)*screenSize.y)
        currentCamPos = vector2(currentCamPos.x, mainCharPos.y-((m_screenLimit.y)*screenSize.y));

    SetCameraPos(currentCamPos);                                      // cameraManager.as:138
    SetPositionRoundUp(true);
    roundUpCameraPos();
}

// cameraManager.as:143. Whole-pixel camera, so sprites map 1:1 to texels.
void CameraManager::roundUpCameraPos()
{
    SetCameraPos(vector2(floor(GetCameraPos().x), floor(GetCameraPos().y)));
}

} // namespace Penumbra::Script
