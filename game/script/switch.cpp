// switch.as, ported line by line: a two-row option on the options screen, as
// text or as images. The original is extracted/app/switch.as, part of Penumbra
// (Andre Santee, 2010), free software under the GNU Lesser General Public
// License, version 3 or (at your option) any later version.
// Enhancement E10 adds the Stepper after the port; each of its lines is marked.   // E10
// Enhancement E23 adds the Chooser after it; each of its lines is marked.          // E23

#include "script/Script.hpp"

namespace Penumbra::Script {

// ENHANCEMENT E27 (Script.hpp, optionsArt.cpp): when the port's added art is loaded, a row's   // E27
// "[x]" is a stone-frame check box and a stepper's or chooser's "[<]" a button of the same     // E27
// stone, in the places the text occupied (the boxes and the hit areas are unchanged). Without   // E27
// the art (the suites, a build without the images) the text is drawn as it always was.          // E27
namespace {                                                           // E27
constexpr float kMarkIcon = 22.0f;                                    // E27: a switch row's check box
constexpr float kMarkWidth = 26.0f;                                   // E27: where its label starts: the box and a gap (the brackets took about 24)
constexpr float kButtonIcon = 24.0f;                                  // E27: an arrow, minus or plus button, in its 40 x 25 box
constexpr float kButtonBoxWidth = 40.0f;                              // E27: kStepperArrowWidth, which is declared below
} // namespace                                                        // E27

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
        // E27: with the art, the mark is a check box (drawn below) and the label starts after it.
        const bool art = optionsArtReady();                           // E27
        string str = art ? string() : string("[") + ((m_current == t) ? "\x95" : " ") + "] ";   // E27
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
        if (art)                                                      // E27
        {                                                             // E27
            drawOptionsIcon((m_current == t) ? "check_on" : "check_off", drawCursor+vector2(0.0f, (size-kMarkIcon)*0.5f),
                            kMarkIcon, currentAlpha);                 // E27
            if (!str.empty())                                       // E27
                shadowText(drawCursor+vector2(kMarkWidth, 0.0f), str, font, size, currentAlpha, 203, 203, 228);   // E27
        }                                                             // E27
        else                                                          // E27
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

// E31 (Script.hpp): what the phone's layout draws of a two-row switch.          // E31
string Switch::getLabel(const uint i) const                           // E31
{                                                                     // E31
    return i < m_button.length() ? m_button[i] : string();            // E31
}                                                                     // E31

// E31 (Script.hpp)
string Switch::getImage(const uint i) const                           // E31
{                                                                     // E31
    return i < m_image.length() ? m_image[i] : string();              // E31
}                                                                     // E31

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
        if (optionsArtReady())                                        // E27: a minus and a plus button
            drawOptionsIcon((t == 0) ? "minus" : "plus", arrowPos+vector2((kButtonBoxWidth-kButtonIcon)*0.5f, (size-kButtonIcon)*0.5f),
                            kButtonIcon, alpha);                      // E27
        else                                                          // E27
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

// E31 (Script.hpp)
string Stepper::getLabel() const                                      // E31
{                                                                     // E31
    return m_label;                                                   // E31
}                                                                     // E31

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

// E23 (Script.hpp): the Chooser, in the Stepper's boxes and alphas.           // E23
Chooser::Chooser(const string& label)                                 // E23
{                                                                     // E23
    m_label = label;                                                  // E23
}                                                                     // E23

// E23 (Script.hpp)
void Chooser::setOptions(const array<string>& options, const uint current)   // E23
{                                                                     // E23
    m_options = options;                                              // E23
    setCurrent(current);                                              // E23
}                                                                     // E23

// E23. The label at `pos`, the row one line below it; hovering an arrow that   // E23
// can still move and confirming moves one option, with the Stepper's hit test  // E23
// (strictly inside the arrow's box), confirm and alphas (100 idle, 200         // E23
// hovered, 40 at an end, 255 for the label and the value).                     // E23
void Chooser::put(const vector2& pos, const string& font, const float size, const float valueWidth)   // E23
{                                                                     // E23
    InputState& input = GetInputHandle();                             // E23
    const vector2 cursor = input.GetCursorPos();                      // E23

    shadowText(pos, m_label, font, size, 255, 203, 203, 228);         // E23

    const uint count = m_options.length();                            // E23
    const vector2 lessPos(pos.x, pos.y+size);                         // E23
    const vector2 valuePos(lessPos.x+kStepperArrowWidth, lessPos.y);  // E23
    const vector2 morePos(valuePos.x+valueWidth, lessPos.y);          // E23
    for (uint t=0; t<2; t++)                                          // E23: 0 = "[<]", 1 = "[>]"
    {                                                                 // E23
        const vector2 arrowPos = (t == 0) ? lessPos : morePos;        // E23
        const bool canMove = (t == 0) ? (m_current > 0) : (m_current+1 < count);   // E23
        uint8 alpha = static_cast<uint8>(canMove ? 100 : 40);         // E23
        if (canMove && cursor.x > arrowPos.x && cursor.y > arrowPos.y     // E23
            && cursor.x < arrowPos.x+kStepperArrowWidth && cursor.y < arrowPos.y+size)   // E23
        {                                                             // E23
            alpha = 200;                                              // E23
            if (getConfirmButtonStatus(0) == KS_HIT)                  // E23
            {                                                         // E23
                m_current = (t == 0) ? m_current-1 : m_current+1;     // E23
            }                                                         // E23
        }                                                             // E23
        if (optionsArtReady())                                        // E27: a left and a right arrow button
            drawOptionsIcon((t == 0) ? "arrow_left" : "arrow_right", arrowPos+vector2((kButtonBoxWidth-kButtonIcon)*0.5f, (size-kButtonIcon)*0.5f),
                            kButtonIcon, alpha);                      // E27
        else                                                          // E27
        shadowText(arrowPos+vector2(kStepperArrowInset, 0), (t == 0) ? "[<]" : "[>]", font, size, alpha, 203, 203, 228);   // E23
    }                                                                 // E23

    // After the arrows, so a move made this frame shows this frame.          // E23
    if (m_current < count)                                            // E23
        shadowText(valuePos+vector2(kStepperValueInset, 0), m_options[m_current], font, size, 255, 203, 203, 228);   // E23
}                                                                     // E23

// E23 (Script.hpp)
uint Chooser::getCurrent() const                                      // E23
{                                                                     // E23
    return m_current;                                                 // E23
}                                                                     // E23

// E23 (Script.hpp)
void Chooser::setCurrent(const uint newCurrent)                       // E23
{                                                                     // E23
    const uint count = m_options.length();                            // E23
    m_current = count == 0 ? 0 : min(newCurrent, count-1);            // E23
}                                                                     // E23

// E23 (Script.hpp)
uint Chooser::getCount() const                                        // E23
{                                                                     // E23
    return m_options.length();                                        // E23
}                                                                     // E23

// E27 (Script.hpp)
string Chooser::getOption(const uint i) const                         // E27
{                                                                     // E27
    return i < m_options.length() ? m_options[i] : string();          // E27
}                                                                     // E27

// E31 (Script.hpp)
string Chooser::getLabel() const                                      // E31
{                                                                     // E31
    return m_label;                                                   // E31
}                                                                     // E31

} // namespace Penumbra::Script
