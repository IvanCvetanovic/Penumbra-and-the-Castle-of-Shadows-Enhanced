// eth_util.as, ported: the helper file Ethanon 0.7.12 shipped with its Machine
// and the game included unchanged (docs/spec/30-ethanon-runtime.md). Of it the
// game uses only frameTimer (animateCharacter); the string helpers and
// stringInput are never called (docs/spec/90-synthesis.md, Gaps).
//
// Ported from extracted/app/eth_util.as. Its header, kept (the PORTUGUES block
// after it says the same in Portuguese, except that it names version 2 of the
// licence where the English names version 3):
/*-----------------------------------------------------------------------

 Ethanon Engine (C) Copyright 2009-2010 Andre Santee
 http://www.asantee.net/gamespace/ethanon/

  ENGLISH
  -------

    This file is part of Ethanon Engine.

    Ethanon Engine is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    Ethanon Engine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with Ethanon Engine. If not, see
    <http://www.gnu.org/licenses/>.

-----------------------------------------------------------------------*/

#include "script/Script.hpp"

namespace Penumbra::Script {

// Creates a string from a vector3
// eth_util.as:49. AngelScript's string + float is Str(float) (%g, 6 digits).
string Vector3ToString(const vector3& v3)
{
    return "(" + Str(v3.x) + ", " + Str(v3.y) + ", " + Str(v3.z) + ")";
}

// Creates a string from a vector2
// eth_util.as:55
string Vector2ToString(const vector2& v2)
{
    return "(" + Str(v2.x) + ", " + Str(v2.y) + ")";
}

// Converts a pixel format assignment to a stringInput
// eth_util.as:61
string FormatToString(const PIXEL_FORMAT format)
{
    if (format == PF32BIT)
        return "32";
    if (format == PF16BIT)
        return "16";
    return "unknown";
}

/*
 * stringInput class:
 * Places an input area on screen where the user can type texts
 */
// eth_util.as:74-122. The constructor (eth_util.as:76-81) is the member
// initializers in Script.hpp.

// eth_util.as:82
void stringInput::PlaceInput(const string& sText, const vector2& pos, const string& sFont,
                             const float fSize, const uint dwColor)
{
    const uint time = GetTime();
    // uint subtraction: wraps like the original's (eth_util.as:86)
    if ((time-m_lastBlink) > m_blinkTime)
    {
        m_showingCarret = m_showingCarret==0 ? 1 : 0;
        m_lastBlink = GetTime();
    }

    InputState& input = GetInputHandle();   // eth_util.as:92

    string lastInput = input.GetLastCharInput();
    if (lastInput != "")
    {
        m_ss += lastInput;
    }

    // K_LEFT erases too, as in the original (eth_util.as:100)
    if (input.GetKeyState(K_BACKSPACE) == KS_HIT || input.GetKeyState(K_LEFT) == KS_HIT)
    {
        const uint nLen = static_cast<uint>(m_ss.length());
        if (nLen > 0)
            m_ss.resize(nLen-1);
    }

    string outputString = sText + ": " + m_ss;
    if (m_showingCarret==1)
        outputString += "|";
    DrawText(pos, outputString, sFont, fSize, dwColor);   // eth_util.as:110
}

// eth_util.as:113
string stringInput::GetString()
{
    return m_ss;
}

/*
 * frameTimer class:
 * This object helps handling keyframe animation
 */
// eth_util.as:128-167. The constructor (eth_util.as:130-134) is the member
// initializers in Script.hpp.

// eth_util.as:136
uint frameTimer::Get()
{
    return m_currentFrame;
}

// eth_util.as:141
uint frameTimer::Set(const uint nFirst, const uint nLast, const uint nStride)
{
    // A new frame range restarts the animation at its first frame at once.
    if (nFirst != m_currentFirst || nLast != m_currentLast)
    {
        m_currentFrame = nFirst;
        m_currentFirst = nFirst;
        m_currentLast  = nLast;
        m_lastTime = GetTime();
        return m_currentFrame;
    }

    // Strictly greater (eth_util.as:152): a frame is held until MORE than
    // nStride ms have passed. uint subtraction wraps like the original's.
    if (GetTime()-m_lastTime > nStride)
    {
        m_currentFrame++;
        if (m_currentFrame > nLast)
            m_currentFrame = nFirst;
        m_lastTime = GetTime();
    }

    return m_currentFrame;
}

} // namespace Penumbra::Script
