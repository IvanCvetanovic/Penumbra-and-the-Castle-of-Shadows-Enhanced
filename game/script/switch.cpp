// switch.as, ported line by line: a two-row option on the options screen, as
// text or as images. The original is extracted/app/switch.as, part of Penumbra
// (Andre Santee, 2010), free software under the GNU Lesser General Public
// License, version 3 or (at your option) any later version.
// Enhancement E10 adds the Stepper after the port; each of its lines is marked.   // E10

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

// ENHANCEMENT E10 (Script.hpp): the Stepper. 0.7.12's scripts cannot measure   // E10
// text, so its row is fixed columns, as Switch's width is: the label, then an   // E10
// arrow box, the value, the other arrow box. The boxes are wider than their     // E10
// "[<]" (about 22 px at 25): a pad moves the picker 5 px a frame.               // E10
namespace {                                                           // E10
constexpr float kStepperArrowWidth = 40.0f;                           // E10
constexpr float kStepperArrowInset = 9.0f;                            // E10: centres "[<]" in its box
constexpr float kStepperValueWidth = 60.0f;                           // E10: room for "100%"
constexpr float kStepperValueInset = 4.0f;                            // E10
} // namespace                                                        // E10

// E10 (Script.hpp)
Stepper::Stepper(const string& label, const uint steps, const uint current)   // E10
{                                                                     // E10
    m_label = label;                                                  // E10
    m_steps = max(steps, 1u);                                         // E10: getFraction divides by it
    m_current = min(current, m_steps);                                // E10
}                                                                     // E10

// E10. Draws the row at `pos`; hovering an arrow that can still move and      // E10
// confirming steps the value, with Switch's hit test (strictly inside), its    // E10
// confirm (a fresh KS_HIT of player 0's) and its alphas: 100 idle, 200 hovered, // E10
// 255 for what is current (here the label and the value). An arrow at its end  // E10
// is drawn faint and does not light up.                                        // E10
void Stepper::put(const vector2& pos, const string& font, const float size, const float labelWidth)   // E10
{                                                                     // E10
    InputState& input = GetInputHandle();                             // E10
    const vector2 cursor = input.GetCursorPos();                      // E10

    shadowText(pos, m_label, font, size, 255, 203, 203, 228);         // E10

    const vector2 lessPos(pos.x+labelWidth, pos.y);                   // E10
    const vector2 valuePos(lessPos.x+kStepperArrowWidth, pos.y);      // E10
    const vector2 morePos(valuePos.x+kStepperValueWidth, pos.y);      // E10
    for (uint t=0; t<2; t++)                                          // E10: 0 = "[<]", 1 = "[>]"
    {                                                                 // E10
        const vector2 arrowPos = (t == 0) ? lessPos : morePos;        // E10
        const bool canMove = (t == 0) ? (m_current > 0) : (m_current < m_steps);   // E10
        uint8 alpha = static_cast<uint8>(canMove ? 100 : 40);         // E10
        if (canMove && cursor.x > arrowPos.x && cursor.y > arrowPos.y     // E10
            && cursor.x < arrowPos.x+kStepperArrowWidth && cursor.y < arrowPos.y+size)   // E10
        {                                                             // E10
            alpha = 200;                                              // E10
            if (getConfirmButtonStatus(0) == KS_HIT)                  // E10
            {                                                         // E10
                m_current = (t == 0) ? m_current-1 : m_current+1;     // E10
            }                                                         // E10
        }                                                             // E10
        shadowText(arrowPos+vector2(kStepperArrowInset, 0), (t == 0) ? "[<]" : "[>]", font, size, alpha, 203, 203, 228);   // E10
    }                                                                 // E10

    // After the arrows, so a step taken this frame shows this frame.         // E10
    const string value = Str(m_current*100/m_steps) + "%";            // E10
    shadowText(valuePos+vector2(kStepperValueInset, 0), value, font, size, 255, 203, 203, 228);   // E10
}                                                                     // E10

// E10 (Script.hpp)
uint Stepper::getCurrent() const                                      // E10
{                                                                     // E10
    return m_current;                                                 // E10
}                                                                     // E10

// E10 (Script.hpp)
void Stepper::setCurrent(const uint newCurrent)                       // E10
{                                                                     // E10
    m_current = min(newCurrent, m_steps);                             // E10
}                                                                     // E10

// E10 (Script.hpp)
uint Stepper::getSteps() const                                        // E10
{                                                                     // E10
    return m_steps;                                                   // E10
}                                                                     // E10

// E10 (Script.hpp)
float Stepper::getFraction() const                                    // E10
{                                                                     // E10
    return static_cast<float>(m_current)/static_cast<float>(m_steps); // E10
}                                                                     // E10

// E10. Nearest, halves away from zero, so a hand-edited 0.75 reads as 8 and   // E10
// the layer, comparing steps rather than floats, leaves that file alone.       // E10
uint Stepper::stepFor(const float fraction) const                     // E10
{                                                                     // E10
    if (!(fraction > 0.0f))                                           // E10: also NaN
        return 0;                                                     // E10
    if (fraction >= 1.0f)                                             // E10
        return m_steps;                                               // E10
    return min(static_cast<uint>(fraction*static_cast<float>(m_steps)+0.5f), m_steps);   // E10
}                                                                     // E10

} // namespace Penumbra::Script
