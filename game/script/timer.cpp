// timer.as, ported (extracted/app/timer.as; Penumbra by Andre Santee, LGPL-3.0-or-later).
// The run clock of the campaign: g_timer starts in resetData (main.as:67) and
// its elapsed time is the record addNewRecordTime stores (setupScene.as:367).

#include "script/Script.hpp"

namespace Penumbra::Script {

// timer.as:43. Minutes are neither wrapped nor padded ("12:00", "1:05"); only
// the seconds get a leading zero.
string getTimeString(const uint time)
{
    const uint secs = (time/1000)%60;
    string seconds;
    if (secs < 10)
        seconds = "0"+Str(secs);
    else
        seconds = ""+Str(secs);
    return "" + Str((time/1000)/60) + ":" + seconds;
}

// timer.as:54-59: the constructor only set startTime = 0, which is the member
// initializer in Script.hpp.

// timer.as:61
void Timer::start()
{
    m_startTime = GetTime();
}

// timer.as:66. uint subtraction on purpose: GetTime() is a wrapping uint32
// counter, and the difference stays right across the wrap.
uint Timer::getElapsedTime()
{
    return GetTime()-m_startTime;
}

// timer.as:71
void Timer::showTimer(const vector2& pos, const float size, const uint8 a,
                      const uint8 r, const uint8 g, const uint8 b)
{
    const uint elapsed = getElapsedTime();
    string time = getTimeString(elapsed);
    shadowText(pos, time, "Arial Narrow", size, a, r, g, b);
}

} // namespace Penumbra::Script
