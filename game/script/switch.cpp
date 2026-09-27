// switch.as, ported line by line: a two-row option on the options screen, as
// text or as images. The original is extracted/app/switch.as, part of Penumbra
// (Andre Santee, 2010), free software under the GNU Lesser General Public
// License, version 3 or (at your option) any later version.

#include "script/Script.hpp"

namespace Penumbra::Script {

// switch.as:45. A text switch: m_image stays empty.
Switch::Switch(const string& b0, const string& b1)
{
    m_current = 0;
    m_button.resize(2);
    m_button[0] = b0;
    m_button[1] = b1;
}

// switch.as:53. An image switch: the labels are stored but never drawn
// (put() adds the label only when m_image is empty, switch.as:77-78).
Switch::Switch(const string& b0, const string& img0, const string& b1, const string& img1)
{
    m_current = 0;
    m_button.resize(2);
    m_button[0] = b0;
    m_button[1] = b1;

    m_image.resize(2);
    m_image[0] = img0;
    m_image[1] = img1;
}

// switch.as:65. Draws both rows from `pos` down; hovering a row and confirming
// selects it.
void Switch::put(const vector2& pos, const string& font, const float size, const float width)
{
    InputState& input = GetInputHandle();                             // switch.as:67
    const vector2 cursor = input.GetCursorPos();
    vector2 drawCursor(pos);

    for (uint t=0; t<2; t++)
    {
        // Every frame, for an image switch (switch.as:73-74).
        if (m_image.length() > 0)
            LoadSprite(m_image[t]);

        // 0x95 is cp1252's bullet: "[\x95] " marks the selected row (switch.as:76).
        string str = string("[") + ((m_current == t) ? "\x95" : " ") + "] ";
        if (m_image.length() == 0)
            str += m_button[t];

        // The hit area starts at drawCursor itself, although the image is drawn
        // 30 px to its right (switch.as:81, :98).
        const vector2 switchPos(drawCursor);
        const vector2 switchSize((m_image.length()==0) ? vector2(width, size) : GetSpriteSize(m_image[t]));

        uint8 alpha = 100;
        if (cursor.x > switchPos.x && cursor.y > switchPos.y
            && cursor.x < switchPos.x+switchSize.x && cursor.y < switchPos.y+switchSize.y)
        {
            alpha = 200;
            if (getConfirmButtonStatus(0) == KS_HIT)
            {
                m_current = t;
            }
        }

        // The conditional is an int in C++; its value is 255 or alpha either way.
        const uint8 currentAlpha = static_cast<uint8>((m_current == t) ? 255 : alpha);   // switch.as:94
        shadowText(drawCursor, str, font, size, currentAlpha, 203, 203, 228);
        if (m_image.length() > 0)
        {
            DrawSprite(m_image[t], drawCursor+vector2(30,0), ARGB(currentAlpha, 255, 255, 255));
        }
        drawCursor.y += switchSize.y;
    }
}

// switch.as:104
uint Switch::getCurrent() const
{
    return m_current;
}

// switch.as:109
void Switch::setCurrent(const uint newCurrent)
{
    m_current = newCurrent;
}

} // namespace Penumbra::Script
