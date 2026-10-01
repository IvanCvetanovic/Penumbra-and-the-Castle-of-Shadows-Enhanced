// ENHANCEMENT E31 (not in the original): the options screen on a phone, larger. Written for this enhanced
// edition and kept with the ported scripts it replaces the frame of (videoModes.cpp's screenModesLoop hands over
// to phoneOptionsLoop where g_mobileLayout is up and the options art is loaded), under the same licence as
// them (LICENSE.md: LGPL-3.0-or-later for game/script/).
//
// What was wrong: a phone's options screen was the original's single column, 25 px rows with 22 px check boxes
// and 24 px arrows, about 13 dp on a 2400x1080 screen, the row that is not selected at a third of full alpha,
// and the Back arrow at the top right of the 4:3 box. This is one layout for every window shape, with no paging
// and no scrolling: a stone panel of two columns of cells, 88 logical px tall (down to 68 where a bottom inset
// leaves less room), a header whose Back arrow is at the top-left of what the window shows and whose language
// chooser (its globe, arrows and name) is at the top-right, 30 px text, and arrow, minus and plus buttons hit over
// 86 px. A two-way Switch is ONE cell: a ticked check box and the wording of its current state, a tap anywhere in
// it flips it. All 14 controls stay, in every language, with no string the desktop layout does not have.
// A cell lights only while it is pressed (the cursor inside it and a confirm held), never on hover: a finger
// leaves the cursor where it lifted, and a hover tint would leave the last cell it touched lit for ever.
//
// The geometry is phoneOptionsLayout(), pure, so the suites pin it; the loop below only reads the globals the
// desktop's rows read and writes the same ones (the layer reads them after the frame), through the same
// getters, setters and goToMenu() as screenModesLoop.

#include <algorithm>
#include <cmath>

#include "script/Script.hpp"

namespace Penumbra::Script {

namespace {

// --- The geometry's constants, logical px of the 1024 x 768 screen ----------------------------------------
constexpr float kScreenW = 1024.0f;
constexpr float kScreenH = 768.0f;
constexpr float kBackW = 123.0f;           // interface/arrow_button.png at its own size, never scaled
constexpr float kBackH = 92.0f;
constexpr float kHeaderMargin = 8.0f;      // the frame's edge to the Back arrow, left and top
constexpr float kTitleGap = 16.0f;         // the arrow to the title
constexpr float kTitleSize = 40.0f;
constexpr float kPanelXMin = 12.0f;        // the panel never leaves 12..1012: a rectangle that crosses x 0 or 1024 is stretched out to the shown edge
constexpr float kPanelXMax = 1012.0f;
constexpr float kPanelWMax = 1000.0f;
constexpr float kPanelWMin = 800.0f;
constexpr float kPanelGap = 2.0f;          // the header's bottom to the panel's top
constexpr float kPad = 16.0f;              // the panel's edge to the cells (the stone takes 13)
constexpr float kColGap = 20.0f;
constexpr float kRowGap = 6.0f;
constexpr float kGroupGap = 10.0f;         // between the fourth row and the choosers
constexpr float kLabelStrip = 30.0f;       // a chooser cell's label strip above its buttons
constexpr float kTextSize = 30.0f;
constexpr float kLabelSize = 26.0f;
constexpr float kHintSize = 22.0f;
constexpr float kBox = 44.0f;              // the check box, and the Adjust cell's arrow
constexpr float kBoxX = 12.0f;
constexpr float kTextX = 66.0f;            // a toggle cell's text
constexpr float kTextRight = 12.0f;        // clear space at its right
constexpr float kBtnIcon = 56.0f;          // an arrow, minus or plus button's art
constexpr float kBtnIconPressed = 64.0f;   // while it is pressed: the art's own size
constexpr float kBtnHitW = 86.0f;          // and its hit box's width
constexpr float kBtnInset = 4.0f;
constexpr float kStepperValueW = 68.0f;
constexpr float kLabelX = 14.0f;
constexpr float kLangValueW = 168.0f;
constexpr float kGlobe = 56.0f;
constexpr float kLangRowH = 84.0f;
constexpr float kHcMin = 68.0f;
constexpr float kHcMax = 88.0f;
constexpr float kBottom = 6.0f;            // the panel's bottom to the frame's
// The height the header, the panel's padding, the gaps and the chooser strip take; what the rows share is the rest.
constexpr float kFixedHeight = kHeaderMargin+kBackH+kPanelGap+kPad+(3.0f*kRowGap+kGroupGap+kLabelStrip+kRowGap)+kPad+kBottom;

// --- The look -----------------------------------------------------------------------------------------------
constexpr uint8 kCardAlpha = 34;           // a cell's card: ARGB(.., 203,203,228) over the stone
constexpr uint8 kCardTopAlpha = 60;        // its 2 px top line
constexpr uint8 kCardPressedAlpha = 92;    // the card while it is pressed
constexpr uint8 kPanelAlpha = 232;         // E27's
constexpr float kPanelCorner = 13.0f;      // E27's: the stone's border
constexpr uint8 kEndStopAlpha = 70;        // a button that cannot move
constexpr uint8 kBackIdleAlpha = 230;
constexpr uint8 kHintAlpha = 170;
constexpr float kHintGap = 2.0f;
constexpr float kFitMinScale = 0.7f;       // a narrow body's text shrinks to this, no further
constexpr uint kFitGroupBase = 4096;       // above showData's group 1
constexpr float kImageMargin = 10.0f;      // the joystick image's room above and below, in a cell

const char* const kFont = "Arial Narrow";

// What one frame reads: the cursor and player 0's confirm (a tap is KS_HIT at the contact point, KS_DOWN while held),
// read once, and whether the body is narrower than its nominal 1000 px (then every text is set through TextFit).
struct Frame
{
    vector2 cursor{0.0f};
    KEY_STATE confirm = KS_UP;
    bool narrow = false;
    uint fitGroup = kFitGroupBase;
};

// The hit test of every control in the scripts: the cursor strictly inside.
bool inside(const vector2& p, const PhoneRect& r)
{
    return p.x > r.x && p.y > r.y && p.x < r.x+r.w && p.y < r.y+r.h;
}

bool hit(const Frame& f, const PhoneRect& r)
{
    return f.confirm == KS_HIT && inside(f.cursor, r);
}

// Lit while pressed: the cursor inside and the confirm down this frame or held. KS_RELEASE and KS_UP are not, so a
// finger that lifted leaves nothing lit.
bool pressed(const Frame& f, const PhoneRect& r)
{
    return (f.confirm == KS_HIT || f.confirm == KS_DOWN) && inside(f.cursor, r);
}

vector2 at(const PhoneRect& r)
{
    return vector2(r.x, r.y);
}

// Text in the screen's colour with its shadow, left-anchored in every language (the options screen is not mirrored).
// In a narrow body, scaled down into its room instead of running out of it (never wrapped, never below kFitMinScale).
// Not shadowText's TextFit overload: that one gives its shadow a right-to-left edge even for rtlRight 0, which would
// put an Arabic shadow at the box's right while its text stays at the left.
void putText(Frame& f, const vector2& pos, const string& text, const float size, const uint8 alpha, const float room)
{
    if (!f.narrow)
    {
        shadowText(pos, text, kFont, size, alpha, 203,203,228);
        return;
    }
    TextFit fit;
    fit.min = pos;
    fit.max = pos+vector2(room, size);
    fit.group = ++f.fitGroup;
    fit.minScale = kFitMinScale;
    fit.maxScale = 1.0f;
    DrawText(pos+vector2(size*0.1f, size*0.1f), text, kFont, size, ARGB(static_cast<uint8>(alpha/2), 0,0,0), 0.0f, fit);
    DrawText(pos, text, kFont, size, ARGB(alpha, 203,203,228), 0.0f, fit);
}

void fillRect(const PhoneRect& r, const uint8 alpha)
{
    const uint c = ARGB(alpha, 203,203,228);
    DrawRectangle(at(r), vector2(r.w, r.h), c, c, c, c);
}

// The card of a cell: a flat tint with a 2 px line at its top, brighter while the cell is pressed.
void drawCard(const PhoneRect& r, const bool lit)
{
    fillRect(r, lit ? kCardPressedAlpha : kCardAlpha);
    fillRect(PhoneRect{r.x, r.y, r.w, 2.0f}, kCardTopAlpha);
}

// A button's art centred in its hit box. Its art is at full alpha when idle, so a press is shown by drawing it at its
// own pixel size, a seventh larger; an end stop is faint and never lights. Not a lit square behind it, as a cell has:
// a rectangle that meets x 0 or 1024 is stretched out to the shown edge (HudRenderer::addRectangle), and the language
// chooser's [<] sits across x 1024 on a 20:9 phone.
void drawButton(const Frame& f, const PhoneRect& box, const char* icon, const bool enabled)
{
    const float size = enabled && pressed(f, box) ? kBtnIconPressed : kBtnIcon;
    drawOptionsIcon(icon, vector2(box.x+(box.w-size)*0.5f, box.y+(box.h-size)*0.5f), size,
                    enabled ? static_cast<uint8>(255) : kEndStopAlpha);
}

// The step a pair of buttons asks for this frame: -1, 0 or +1. A button that cannot move does nothing.
int stepOf(const Frame& f, const PhoneRect& less, const PhoneRect& more, const bool canLess, const bool canMore)
{
    if (canLess && hit(f, less))
        return -1;
    if (canMore && hit(f, more))
        return 1;
    return 0;
}

// --- The cells ----------------------------------------------------------------------------------------------

// A two-way Switch as one cell: the ticked box marks the wording shown (as the original's selected row is marked),
// and the tap flips it; the wording is drawn after the flip, as Switch::put draws after its update. `hint` is the
// widescreen cell's second line.
void putToggle(Frame& f, const PhoneRect& r, Switch& sw, const bool hint)
{
    const bool lit = pressed(f, r);
    if (hit(f, r))
        sw.setCurrent(sw.getCurrent() == 0 ? 1u : 0u);
    drawCard(r, lit);
    const float cy = r.y+r.h*0.5f;
    drawOptionsIcon("check_on", vector2(r.x+kBoxX, cy-kBox*0.5f), kBox);
    const string label = sw.getLabel(sw.getCurrent());
    const float room = r.w-kTextX-kTextRight;
    if (hint)
    {
        // The two lines as one block, centred in the cell.
        const float block = kTextSize+kHintGap+kHintSize;
        const float top = r.y+(r.h-block)*0.5f;
        putText(f, vector2(r.x+kTextX, top), label, kTextSize, 255, room);
        putText(f, vector2(r.x+kTextX, top+kTextSize+kHintGap), "Vale a partir da pr\xF3xima fase", kHintSize, kHintAlpha, room);
    }
    else
        putText(f, vector2(r.x+kTextX, cy-kTextSize*0.5f), label, kTextSize, 255, room);
}

// E28's button: a cell of its own that raises g_adjustTouchControls, which the layer reads after the frame.
void putAdjust(Frame& f, const PhoneRect& r)
{
    const bool lit = pressed(f, r);
    if (hit(f, r))
        g_adjustTouchControls = true;
    drawCard(r, lit);
    const float cy = r.y+r.h*0.5f;
    drawOptionsIcon("arrow_right", vector2(r.x+kBoxX, cy-kBox*0.5f), kBox);
    putText(f, vector2(r.x+kTextX, cy-kTextSize*0.5f), "Ajustar controles", kTextSize, 255, r.w-kTextX-kTextRight);
}

// g_controls, the image switch: the ticked box and the picture of the current state (the keyboard and a pad, or two
// pads), which the draw boundary localises. Both sprites are loaded every frame, as Switch::put does.
void putJoystick(Frame& f, const PhoneRect& r)
{
    const bool lit = pressed(f, r);
    if (hit(f, r))
        g_controls.setCurrent(g_controls.getCurrent() == 0 ? 1u : 0u);
    drawCard(r, lit);
    LoadSprite(g_controls.getImage(0));
    LoadSprite(g_controls.getImage(1));
    const float cy = r.y+r.h*0.5f;
    drawOptionsIcon("check_on", vector2(r.x+kBoxX, cy-kBox*0.5f), kBox);
    const string image = g_controls.getImage(g_controls.getCurrent());
    const vector2 own = GetSpriteSize(image);
    if (own.x > 0.0f && own.y > 0.0f)
    {
        const float k = std::min(1.0f, (r.h-kImageMargin)/own.y);
        DrawShapedSprite(image, vector2(r.x+kTextX, cy-own.y*k*0.5f), vector2(own.x*k, own.y*k), ARGB(255, 255,255,255));
    }
}

// Refresh rate and Zoom: the label on its strip, then [<] value [>] under it; only the two buttons answer.
void putChooser(Frame& f, const PhoneOptionsLayout& l, const PhoneCell cell, Chooser& chooser)
{
    const PhoneRect& r = l.cell[cell];
    drawCard(r, false);
    const uint count = chooser.getCount();
    const uint before = chooser.getCurrent();
    const int step = stepOf(f, l.less[cell], l.more[cell], before > 0, before+1 < count);
    if (step < 0)
        chooser.setCurrent(before-1);
    else if (step > 0)
        chooser.setCurrent(before+1);
    const uint now = chooser.getCurrent();
    putText(f, vector2(r.x+kLabelX, r.y+2.0f), chooser.getLabel(), kLabelSize, 255, r.w-2.0f*kLabelX);
    drawButton(f, l.less[cell], "arrow_left", now > 0);
    drawButton(f, l.more[cell], "arrow_right", now+1 < count);
    const PhoneRect& v = l.value[cell];
    putText(f, vector2(v.x+12.0f, v.y+v.h*0.5f-kTextSize*0.5f), chooser.getOption(now), kTextSize, 255, v.w-12.0f-8.0f);
}

// Music and Effects: the label, then [-] value [+] on one line.
void putStepper(Frame& f, const PhoneOptionsLayout& l, const PhoneCell cell, Stepper& stepper)
{
    const PhoneRect& r = l.cell[cell];
    drawCard(r, false);
    const uint before = stepper.getCurrent();
    const int step = stepOf(f, l.less[cell], l.more[cell], before > 0, before < stepper.getSteps());
    if (step < 0)
        stepper.setCurrent(before-1);
    else if (step > 0)
        stepper.setCurrent(before+1);
    const uint now = stepper.getCurrent();
    const float cy = r.y+r.h*0.5f;
    putText(f, vector2(r.x+kLabelX, cy-kLabelSize*0.5f), stepper.getLabel(), kLabelSize, 255,
            l.less[cell].x-10.0f-(r.x+kLabelX));
    drawButton(f, l.less[cell], "minus", now > 0);
    drawButton(f, l.more[cell], "plus", now < stepper.getSteps());
    const PhoneRect& v = l.value[cell];
    putText(f, vector2(v.x+4.0f, cy-kTextSize*0.5f), Str(now*100/stepper.getSteps())+"%", kTextSize, 255, v.w-4.0f);
}

// The header's language chooser: the globe, [<], the name in its own tongue, [>]. No label: the globe is its sign,
// the cue a player who cannot read the screen looks for. Chooser's index contract (0 = Automatic, i + 1 = the
// layer's i-th language) is the layer's; this only moves the index.
void putLanguage(Frame& f, const PhoneOptionsLayout& l)
{
    const uint count = g_language.getCount();
    const uint before = g_language.getCurrent();
    const int step = stepOf(f, l.langLess, l.langMore, before > 0, before+1 < count);
    if (step < 0)
        g_language.setCurrent(before-1);
    else if (step > 0)
        g_language.setCurrent(before+1);
    const uint now = g_language.getCurrent();
    drawOptionsIcon("globe", at(l.globe), kGlobe);
    drawButton(f, l.langLess, "arrow_left", now > 0);
    drawButton(f, l.langMore, "arrow_right", now+1 < count);
    putText(f, vector2(l.langValue.x+12.0f, l.langValue.y+l.langValue.h*0.5f-kTextSize*0.5f), g_language.getOption(now),
            kTextSize, 255, l.langValue.w-12.0f-8.0f);
}

} // namespace

// --- The geometry -------------------------------------------------------------------------------------------

PhoneOptionsLayout phoneOptionsLayout(const OptionsArea& given, const bool touchOn, const bool refreshRow)
{
    OptionsArea area = given;
    if (!(area.shownMax.x > area.shownMin.x))   // none published: the whole screen, no frame
    {
        area = OptionsArea{};
        area.shownMax = vector2(kScreenW, kScreenH);
    }
    // The frame in whole px, as the cells are laid out on a grid.
    const float fl = std::round(area.left);
    const float ft = std::round(area.top);
    const float fr = std::round(area.right);

    PhoneOptionsLayout l;
    // The six rows' height: what the frame's top and bottom leave of the screen, less the fixed parts, shared by six.
    l.hc = std::clamp(std::floor((kScreenH-kFixedHeight-area.bottom-ft)/6.0f), kHcMin, kHcMax);
    const float hc = l.hc;

    // The header: the Back arrow at the top-left corner of what the window shows, inside the frame; its hit box
    // reaches the corner itself (the frame is for what is drawn, not for what a thumb hits) and is one px past it on
    // the two edges, because a cursor clamped to the shown area sits exactly on the edge and a hit is strictly inside.
    const float backX = std::floor(area.shownMin.x+fl+kHeaderMargin);
    const float backY = ft+kHeaderMargin;
    l.back = PhoneRect{backX, backY, kBackW, kBackH};
    const float shownLeft = std::floor(area.shownMin.x);
    l.backHit = PhoneRect{shownLeft-1.0f, -1.0f, backX+kBackW+20.0f-shownLeft+1.0f, backY+kBackH+kHeaderMargin+1.0f};
    l.title = vector2(backX+kBackW+kTitleGap, backY+std::floor((kBackH-kTitleSize)*0.5f));
    // The language chooser from the top-right corner in: [>], the name's box, [<], the globe.
    const float cy = backY+std::floor(kBackH*0.5f);
    const float rightEdge = std::floor(area.shownMax.x-fr-kHeaderMargin);
    l.langMore = PhoneRect{rightEdge-kBtnHitW, cy-kLangRowH*0.5f, kBtnHitW, kLangRowH};
    l.langValue = PhoneRect{l.langMore.x-kLangValueW, l.langMore.y, kLangValueW, kLangRowH};
    l.langLess = PhoneRect{l.langValue.x-kBtnHitW, l.langMore.y, kBtnHitW, kLangRowH};
    l.globe = PhoneRect{l.langLess.x-8.0f-kGlobe, cy-kGlobe*0.5f, kGlobe, kGlobe};

    // The panel: 12..1012 wherever the window shows that much inside the frame, else what is left of it (down to 800).
    const float left = std::max(kPanelXMin, std::ceil(area.shownMin.x+fl));
    const float right = std::min(kPanelXMax, std::floor(area.shownMax.x-fr));
    const float pw = std::max(kPanelWMin, std::min(kPanelWMax, right-left));
    float px = left;
    if (right-left < kPanelWMin)
        px = kScreenW*0.5f-std::floor(pw*0.5f);
    if (right-left > kPanelWMax)
        px = kScreenW*0.5f-kPanelWMax*0.5f;
    const float panelY = ft+kHeaderMargin+kBackH+kPanelGap;
    const float wc = std::floor((pw-2.0f*kPad-kColGap)*0.5f);
    const float col[2] = {px+kPad, px+kPad+wc+kColGap};
    float rowY[4];
    float y = panelY+kPad;
    for (float& r : rowY)
    {
        r = y;
        y += hc+kRowGap;
    }
    const float r5 = y-kRowGap+kGroupGap;
    const float r6 = r5+kLabelStrip+hc+kRowGap;
    l.panel = PhoneRect{px, panelY, pw, r6+hc+kPad-panelY};

    const auto place = [&l, wc, hc](const PhoneCell c, const float x, const float yy)
    {
        l.cell[c] = PhoneRect{x, yy, wc, hc};
        l.present[c] = true;
    };
    place(PC_PIXEL_SHADERS, col[0], rowY[0]);
    place(PC_SMOOTH_MOTION, col[1], rowY[0]);
    place(PC_WIDESCREEN, col[0], rowY[1]);
    place(PC_PAUSE_FOCUS, col[1], rowY[1]);
    place(PC_TOUCH, col[0], rowY[2]);
    if (touchOn)
        place(PC_ADJUST, col[1], rowY[2]);
    place(PC_KEYBOARD_P2, col[0], rowY[3]);
    place(PC_JOYSTICK, col[1], rowY[3]);

    // Refresh | Zoom, each a card of its label strip and a row of two buttons; Zoom takes the left place where
    // nothing sets a refresh rate (iOS).
    const auto chooser = [&l, wc, hc, r5](const PhoneCell c, const float x)
    {
        l.cell[c] = PhoneRect{x, r5, wc, kLabelStrip+hc};
        l.present[c] = true;
        const float yy = r5+kLabelStrip;
        l.less[c] = PhoneRect{x+kBtnInset, yy, kBtnHitW, hc};
        l.more[c] = PhoneRect{x+wc-kBtnInset-kBtnHitW, yy, kBtnHitW, hc};
        l.value[c] = PhoneRect{x+kBtnInset+kBtnHitW, yy, wc-2.0f*(kBtnInset+kBtnHitW), hc};
    };
    if (refreshRow)
    {
        chooser(PC_REFRESH, col[0]);
        chooser(PC_ZOOM, col[1]);
    }
    else
        chooser(PC_ZOOM, col[0]);

    // Music | Effects: the label, then [-] value [+] at the cell's right.
    const auto stepper = [&l, wc, hc, r6](const PhoneCell c, const float x)
    {
        l.cell[c] = PhoneRect{x, r6, wc, hc};
        l.present[c] = true;
        l.more[c] = PhoneRect{x+wc-kBtnInset-kBtnHitW, r6, kBtnHitW, hc};
        l.value[c] = PhoneRect{l.more[c].x-kStepperValueW, r6, kStepperValueW, hc};
        l.less[c] = PhoneRect{l.value[c].x-kBtnHitW, r6, kBtnHitW, hc};
    };
    stepper(PC_MUSIC, col[0]);
    stepper(PC_EFFECTS, col[1]);
    return l;
}

bool phoneOptionsOn()
{
    return g_mobileLayout && optionsArtReady();
}

// --- The frame ----------------------------------------------------------------------------------------------

void phoneOptionsLoop()
{
    // What the window shows of the screen, from the layer; the suites publish none and get the whole screen.
    OptionsArea area = g_optionsArea;
    if (!(area.shownMax.x > area.shownMin.x))
    {
        area = OptionsArea{};
        area.shownMax = GetScreenSize();
    }
    // The Adjust cell's slot is always laid out: whether it shows and answers is the touch switch's live state, read
    // when its turn comes (after the touch cell's, so a tap that turns the controls off or on shows at once).
    const PhoneOptionsLayout l = phoneOptionsLayout(area, true, g_refreshRateRow);

    Frame f;
    f.cursor = GetInputHandle().GetCursorPos();
    f.confirm = getConfirmButtonStatus(0);
    f.narrow = l.panel.w < kPanelWMax;

    drawPanel(at(l.panel), vector2(l.panel.w, l.panel.h), kPanelAlpha, kPanelCorner);

    putToggle(f, l.cell[PC_PIXEL_SHADERS], g_enablePS, false);
    putToggle(f, l.cell[PC_SMOOTH_MOTION], g_smoothMotion, false);
    putToggle(f, l.cell[PC_WIDESCREEN], g_widescreen, true);
    putToggle(f, l.cell[PC_PAUSE_FOCUS], g_pauseOnFocusLoss, false);
    putToggle(f, l.cell[PC_TOUCH], g_touchControls, false);
    if (g_touchControls.getCurrent() == 0)
        putAdjust(f, l.cell[PC_ADJUST]);
    putToggle(f, l.cell[PC_KEYBOARD_P2], g_keyboardP2, false);
    putJoystick(f, l.cell[PC_JOYSTICK]);
    if (g_refreshRateRow)
        putChooser(f, l, PC_REFRESH, g_refreshRate);
    putChooser(f, l, PC_ZOOM, g_zoom);
    putStepper(f, l, PC_MUSIC, g_musicVolume);
    putStepper(f, l, PC_EFFECTS, g_effectsVolume);

    // The header: the title at the Back arrow's side, the language chooser at the corner opposite.
    shadowText(l.title, "Op\xE7\xF5" "es de v\xED" "deo", kFont, kTitleSize, 255, 203,203,228);   // videoModes.as:89
    putLanguage(f, l);

    UsePixelShaders(g_enablePS.getCurrent() == 0);                    // videoModes.as:126
    showToggleFullscreenMessage();                                    // videoModes.as:131
    waitForInputToMenu();

    // The Back arrow, last so nothing is drawn over it: goToMenu(), as the desktop's, so the menu song is kept.
    const bool pressedBack = pressed(f, l.backHit);
    DrawSprite("interface/arrow_button.png", at(l.back), ARGB(pressedBack ? static_cast<uint8>(255) : kBackIdleAlpha, 203,203,228));
    if (hit(f, l.backHit))
        goToMenu();
}

} // namespace Penumbra::Script
