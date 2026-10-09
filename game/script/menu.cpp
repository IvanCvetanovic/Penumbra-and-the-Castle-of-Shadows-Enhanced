// menu.as, ported line by line: the main menu and the arena select (both driven
// by the cursor entity's callback), the Alt+Enter window toggle, and the ways
// back to the menu. The original is extracted/app/menu.as, part of Penumbra
// (Andre Santee, 2010), free software under the GNU Lesser General Public
// License, version 3 or (at your option) any later version.
//
// The panel texts como_jogar, config, creditos, novo_jogo and versus
// (menu.as:123-215) are defined in Script.hpp, as the bytes AngelScript made of
// the heredocs; they are not repeated here.

#include "script/Script.hpp"

namespace Penumbra::Script {

namespace {                                                                                          // E30
// ENHANCEMENT E30 (not in the original): the menu song goes on through the scene loads between the     // E30
// menu's screens. The original's every load released every sample, and each of these screens' preLoops // E30
// started the song again from its first note (loopMenuSong, below); here the load that leaves one     // E30
// menu screen for another asks for the song to be kept (Audio.hpp's KeepOnNextLoad), and loopMenuSong // E30
// finds it playing. Asked only before the loads to a menu screen - the settings, the arena select and // E30
// the way back - and never before a start (newGame), so a level still begins in its own music.        // E30
// A song that is not playing (a menu entered from a level, the first screen of a run) is not kept:   // E30
// the call is then a no-op and loopMenuSong starts it as the original did.                           // E30
void keepMenuSong()                                                                                  // E30
{                                                                                                    // E30
    KeepSampleOnNextLoad("soundfx/menu.mp3");                                                        // E30
}                                                                                                    // E30
} // namespace                                                                                       // E30

// menu.as:43
void loopMenuSong()
{
    LoadMusic("soundfx/menu.mp3");
    // E30: a song kept through the load (keepMenuSong) is found playing and goes on. The original    // E30
    // started it from its first note on every one of these loads: its load released every sample,   // E30
    // so this always found it silent. A kept song gets its full volume explicitly: the original     // E30
    // had that from the release that forgot the sample, which a kept one has not had.               // E30
    if (SampleExists("soundfx/menu.mp3") && IsSamplePlaying("soundfx/menu.mp3"))                           // E30
    {                                                                                                      // E30
        SetSampleVolume("soundfx/menu.mp3", 1.0f);                                                         // E30
        return;                                                                                            // E30
    }                                                                                                      // E30
    PlaySample("soundfx/menu.mp3");
    LoopSample("soundfx/menu.mp3", true);
}

// menu.as:50. The preLoop of menu.esc AND arena_select.esc.
void menuPreLoop()
{
    loopMenuSong();

    LoadSoundEffect("soundfx/help.mp3");
    SetSampleVolume("soundfx/help.mp3", 0.3f);

    LoadSoundEffect("soundfx/newgame.mp3");
    LoadSoundEffect("soundfx/fail.ogg");

    LoadSprite("interface/joystick.png");

    SetBorderBucketsDrawing(false);

    // A full-screen black rectangle, so the scene's first frame is black.
    drawRect(0xFF000000);                                             // menu.as:64

    ETHEntity cursor = SeekEntity("cursor.ent");                      // menu.as:66
    if (cursor != nullptr)
    {
        cursor->AddStringData("lastButton", "none");
        cursor->AddUIntData("menuStartTime", GetTime());
    }
}

// menu.as:74. Re-enumerates the joysticks EVERY call and marks each detected one
// with an icon at the top right.
void detectJoysticks()
{
    const vector2 screenSize = GetScreenSize();
    const vector2 joySpriteSize = GetSpriteSize("interface/joystick.png");
    InputState& input = GetInputHandle();                             // menu.as:78
    input.DetectJoysticks();
    // E25: on a phone's larger menu the screen's right edge is in the middle  // E25
    // of the panel's text: the icons go to the top edge of what the window    // E25
    // shows of the panel, in a row from the corner the text's lines leave     // E25
    // free (the right, or the left for a right-to-left language), at half the // E25
    // size E1's view draws them - above the body's first line - and showData  // E25
    // keeps its text out of their box.                                        // E25
    g_joystickIconsMin = g_joystickIconsMax = vector2(0,0);                     // E25
    if (g_phonePanel.on)                                                         // E25
    {                                                                            // E25
        const vector2 iconSize = joySpriteSize*(g_phonePanel.minScale*0.5f);     // E25
        for (uint t = 0; t < 2; t++)                                             // E25
        {                                                                        // E25
            if (input.GetJoystickStatus(t) != JS_DETECTED)                       // E25
                continue;                                                        // E25
            const float slot = iconSize.x*static_cast<float>(t);                 // E25
            const vector2 iconPos(g_phonePanel.rightToLeft ? g_phonePanel.min.x+slot   // E25
                                                           : g_phonePanel.max.x-iconSize.x-slot,   // E25
                                  g_phonePanel.shownMin.y);                      // E25
            DrawShapedSprite("interface/joystick.png", iconPos, iconSize, ARGB(150,255,255,255));   // E25
            // The row's box: the icons share their top.                        // E25
            if (g_joystickIconsMax.x <= g_joystickIconsMin.x)                    // E25
                g_joystickIconsMin = g_joystickIconsMax = iconPos;               // E25
            g_joystickIconsMin.x = min(g_joystickIconsMin.x, iconPos.x);         // E25
            g_joystickIconsMax = vector2(max(g_joystickIconsMax.x, iconPos.x+iconSize.x), iconPos.y+iconSize.y);   // E25
        }                                                                        // E25
        return;                                                                  // E25
    }                                                                            // E25
    // E26: in a level (held J, setupScene.as:393-394) from the HUD frame's corner.  // E26
    if (input.GetJoystickStatus(0) == JS_DETECTED)
    {
        DrawSprite("interface/joystick.png", vector2(screenSize.x-joySpriteSize.x, 0)+hudTopRight(),   // E26: hudTopRight
                   ARGB(150,255,255,255));
    }
    if (input.GetJoystickStatus(1) == JS_DETECTED)
    {
        DrawSprite("interface/joystick.png", vector2(screenSize.x-joySpriteSize.x, joySpriteSize.y)+hudTopRight(),   // E26
                   ARGB(150,255,255,255));
    }
}

// menu.as:92. In menu.esc goToMenu() does nothing (the scene is already the
// menu); this only matters in arena_select.esc.
void menuLoop()
{
    waitForInputToMenu();
}

// menu.as:97
void showToggleFullscreenMessage()
{
    const vector2 screenSize = GetScreenSize();
    InputState& input = GetInputHandle();                             // menu.as:100
    // E20: not on a phone, which has neither the keys nor a window to switch;   // E20
    // the switching below stays (g_windowed's row is not drawn there).          // E20
    if (!g_mobileLayout)                                                         // E20
        shadowText(vector2(0,screenSize.y-15), "Pressione Alt+Enter para trocar entre fullscreen e modo janela", "Arial Narrow", 15.0f, 255,203,203,228);

    // g_windowed's row 0 is "Janela" (windowed): when the switch disagrees with
    // the window, the window follows the switch (menu.as:103-106).
    bool toggle = false;
    if ((Windowed() && g_windowed.getCurrent() == 1)
        || (!Windowed() && g_windowed.getCurrent() == 0))
        toggle = true;

    // && binds tighter than ||, as in AngelScript (menu.as:108).
    if ((input.GetKeyState(K_ALT) == KS_DOWN && input.GetKeyState(K_RETURN) == KS_HIT) || toggle)
    {
        const bool windowed = !Windowed();
        SetWindowProperties(APPLICATION_TITLE, static_cast<uint>(screenSize.x), static_cast<uint>(screenSize.y), windowed, true, PF32BIT);   // menu.as:111
        if (windowed)
        {
            g_windowed.setCurrent(0);
        }
        else
        {
            g_windowed.setCurrent(1);
        }
    }
}

// menu.as:217. The right-hand panel: a vertical gradient over the right
// 1-0.618 of the screen, a title and a body.
void showData(const string& title, const string& content)
{
    const vector2 screenSize = GetScreenSize();
    const vector2 rectSize(screenSize.x*(1-0.618f), screenSize.y);
    const uint rectColor0 = ARGB(190,0,0,0);
    const uint rectColor1 = ARGB(55,0,0,0);
    const vector2 rectPos(screenSize.x-rectSize.x, 0);

    const float textSize = screenSize.y <= 700 ? 20.0f : 25.0f;      // menu.as:225
    DrawRectangle(rectPos, rectSize,
                  rectColor0, rectColor0, rectColor1, rectColor1);
    // E25: on a phone the menu is drawn larger, from the left, and the panel   // E25
    // runs on to the window's right edge: the title and the body are set in    // E25
    // the part of it the window shows (g_phonePanel), 10 px in and 20 down as  // E25
    // above, the body 50 below the title, both scaled by one factor to fill    // E25
    // it (HudCmd::fit) - never smaller than they are drawn without E25.        // E25
    // A long body may be set in two columns, split at one of its blank lines, // E25
    // where that sets the panel larger (How to Play, the credits). No line     // E25
    // crosses the gamepad icons, with 6 px about them (the body's first line   // E25
    // is below them at any size; only a long title meets them).                // E25
    if (g_phonePanel.on)                                                         // E25
    {                                                                            // E25
        TextFit fit;                                                             // E25
        fit.min = g_phonePanel.min;                                              // E25
        fit.max = g_phonePanel.max;                                              // E25
        fit.group = 1;                                                           // E25
        fit.minScale = g_phonePanel.minScale;                                    // E25
        fit.maxScale = g_phonePanel.maxScale;                                    // E25
        if (g_joystickIconsMax.x > g_joystickIconsMin.x)                         // E25
        {                                                                        // E25
            fit.avoidMin = g_joystickIconsMin-vector2(6,6);                      // E25
            fit.avoidMax = g_joystickIconsMax+vector2(6,6);                      // E25
        }                                                                        // E25
        shadowText(fit.min, title, "Arial Narrow", 40.0f, 255,203,203,228, fit.max.x, fit);   // E25
        TextFit body = fit;                                                      // E25
        body.columns = 2;                                                        // E25
        shadowText(fit.min+vector2(0,50), content, "Arial Narrow", textSize, 255,203,203,228, fit.max.x, body);   // E25
        return;                                                                  // E25
    }                                                                            // E25
    // E24: a right-to-left language sets the title and the body against the
    // panel's right edge, 10 px in as they are from its left, so every panel's
    // lines start at one edge; the panel runs to the screen's right edge
    // (screenSize.x, whole, where rectPos.x+rectSize.x is 1024 give or take a float).
    const float rtlRight = screenSize.x-10;                           // E24
    shadowText(rectPos+vector2(10,20), title, "Arial Narrow", 40.0f, 255,203,203,228, rtlRight);   // E24: rtlRight
    shadowText(rectPos+vector2(10,70), content, "Arial Narrow", textSize, 255,203,203,228, rtlRight);   // E24: rtlRight
}

// ENHANCEMENT E36: the best times panel's body. Both difficulties' lists, Normal's first, each under its name  // E36
// (menu.as:240 drew the one list). A heading and its list are one paragraph, one blank line between the two,  // E36
// so that a phone's panel, where it sets the body in two columns (at a blank line), breaks it between the     // E36
// lists and nowhere else; 2 + 10 lines and the blank one, 13 of the 27 the panel's body has room for.        // E36
// getRecordTimeList's lines each end in a break.                                                              // E36
string recordsPanelText()                                       // E36
{
    return difficultyName(DIFFICULTY_NORMAL) + "\n" + getRecordTimeList(DIFFICULTY_NORMAL)   // E36
        + "\n" + difficultyName(DIFFICULTY_HARD) + "\n" + getRecordTimeList(DIFFICULTY_HARD);   // E36
}

// ENHANCEMENT E36: New Game asks for the difficulty before it starts. Confirming "Novo jogo" no longer starts the     // E36
// campaign: it opens a prompt over the menu - the cursor entity's custom data "pickDifficulty" (when it opened),        // E36
// "pickRow" (the row lit: 0 Normal, 1 Hard) and "pickX" / "pickY" (where the pointer was last seen) - and choosing a   // E36
// row starts the campaign at once, as the confirm did (the 3 s fade-out, then newGame, which latches g_difficulty).    // E36
// Esc, a pad's Back or a click outside the panel (once it is 350 ms old) closes it with nothing started. While it is     // E36
// open the system pointer is shown (cursor.ent, which is drawn under the panel, cannot be seen). g_difficulty is only    // E36
// a holder, the state the layer seeds from the settings and saves when it changes; it is written by a chosen row alone   // E36
// (never by hover or a cancel), so what the prompt opens on is the last choice, Normal the first time.                   // E36
namespace {                                                                                                          // E36
// The panel's parts in the menu's logical px at scale 1: 560 x 330 centred, a 24 px inset to the title (Arial Narrow    // E36
// 40, one line) and to the two rows (512 x 100, 10 px apart), each with its name (34) and its description (22) in a     // E36
// text box 480 wide that starts 22 px in, after the lit row's 6 px bar. The rows' texts stay 10 px short of the         // E36
// row's right edge and the title has the whole inner width, 512 px.                                                    // E36
constexpr float kPromptWidth = 560.0f;                                                                               // E36
constexpr float kPromptHeight = 330.0f;                                                                              // E36
constexpr float kPromptInset = 24.0f;                                                                                // E36
constexpr float kPromptTitleY = 20.0f;                                                                               // E36
constexpr float kPromptRowTop = 90.0f;                                                                               // E36: 30 px under the title's line
constexpr float kPromptRowHeight = 100.0f;                                                                           // E36
constexpr float kPromptRowStep = 110.0f;                                                                             // E36: the second row 10 px under the first
constexpr float kPromptBar = 6.0f;                                                                                   // E36
constexpr float kPromptTextX = 22.0f;                                                                                // E36
constexpr float kPromptTextWidth = 480.0f;                                                                           // E36
constexpr float kPromptLabelY = 14.0f;                                                                               // E36: the name's top, in the row
constexpr float kPromptDescY = 60.0f;                                                                                // E36: the description's top: 18 px to the row's foot
constexpr float kPromptTitleSize = 40.0f;                                                                            // E36: showData's title size
constexpr float kPromptLabelSize = 34.0f;                                                                            // E36
constexpr float kPromptDescSize = 22.0f;                                                                             // E36
// On a phone the panel keeps this share of the shown rectangle free at each side (8 percent).                       // E36
constexpr float kPromptPhoneMargin = 0.08f;                                                                          // E36
// ...and, slid sideways when it would not, this far inside the logical screen's left and right edges (x 0 and 1024): // E36
// HudRenderer::addRectangle stretches a rectangle with an edge within half a pixel of either one out to the window's // E36
// side, as the backdrop and a fade need, which on the panel would be a dark band; 2 px clears it for every rectangle // E36
// the panel is made of.                                                                                             // E36
constexpr float kPromptScreenMargin = 2.0f;                                                                          // E36

// A row's second line: what the difficulty does.                                                                    // E36
string promptDescription(const uint row)                                                                            // E36
{                                                                                                                    // E36
    return row == DIFFICULTY_HARD ? string("Os inimigos t\xEAm o dobro de vida.") : string("Os inimigos como no jogo original.");   // E36
}                                                                                                                    // E36

// Strictly inside, as putBackButton and Switch::put hit their boxes.                                                // E36
bool promptInside(const vector2& p, const vector2& lo, const vector2& hi)                                            // E36
{                                                                                                                    // E36
    return p.x > lo.x && p.y > lo.y && p.x < hi.x && p.y < hi.y;                                                     // E36
}                                                                                                                    // E36
} // namespace                                                                                                       // E36

// E36. Where the prompt's parts are: one function for the drawing and the hit tests, so that they cannot disagree. // E36
// A window: scale 1, centred on the menu's logical screen (GetScreenSize: 1024 x 768, the widescreen sides are art    // E36
// only). A phone's larger menu (g_phonePanel.on) shows only shownMin..shownMax of that screen, scaled up from the     // E36
// left; its panel text box ends 10 px short of the window's right edge less the safe area's inset (PhoneUi.cpp,        // E36
// ComputeMenuPanel), so max.x + 10 is the shown rectangle's safe right edge. The panel is scaled, panel and texts   // E36
// together, by one factor: the largest at which it leaves 8 percent of the rectangle free at every side, and no larger // E36
// than g_phonePanel.maxScale (1.6 times the size E1's view draws text,                                              // E36
// in this scene's units: a logical px is frame.scale image px there, minScale = baseScale / frame.scale of them), and // E36
// centred in the rectangle. A wide window shows more than the screen across (shownMax.x past 1024, where only the   // E36
// art's sides are): a panel that would then come within kPromptScreenMargin of the screen's right edge is slid left // E36
// by the overshoot (and one past the left edge right by the shortfall), so that it stays centred in what the window // E36
// shows wherever it can. It always fits between the margins: frame.scale is at least 1.1 times baseScale (PhoneUi.cpp, // E36
// kMenuMinGain), so maxScale is at most 1.6 / 1.1 = 1.46 and the panel at most 815 px of the 1020 between them.     // E36
// A rectangle too small for even minScale keeps the fit: a panel that overflows the window is worse than a small one. // E36
DifficultyPromptBox difficultyPromptBox()                                                                            // E36
{                                                                                                                    // E36
    DifficultyPromptBox box;                                                                                         // E36
    vector2 areaMin(0.0f, 0.0f);                                                                                     // E36
    vector2 areaMax = GetScreenSize();                                                                               // E36
    float s = 1.0f;                                                                                                  // E36
    if (g_phonePanel.on)                                                                                             // E36
    {                                                                                                                // E36
        areaMin = g_phonePanel.shownMin;                                                                             // E36
        areaMax = vector2(min(g_phonePanel.shownMax.x, g_phonePanel.max.x+10.0f), g_phonePanel.shownMax.y);          // E36
        const float keep = 1.0f-2.0f*kPromptPhoneMargin;                                                             // E36
        s = min(min((areaMax.x-areaMin.x)*keep/kPromptWidth, (areaMax.y-areaMin.y)*keep/kPromptHeight),             // E36
                g_phonePanel.maxScale);                                                                              // E36
        if (!(s > 0.0f))                                                                                             // E36
            s = g_phonePanel.minScale > 0.0f ? g_phonePanel.minScale : 1.0f;                                         // E36: a degenerate rectangle
    }                                                                                                                // E36
    box.scale = s;                                                                                                   // E36
    const vector2 size = vector2(kPromptWidth, kPromptHeight)*s;                                                     // E36
    box.panelMin = (areaMin+areaMax)*0.5f-size*0.5f;                                                                 // E36
    if (g_phonePanel.on)                                                                                             // E36
    {                                                                                                                // E36
        // Slid along x, not shrunk, and before anything is placed from panelMin: the title, rows and texts move too. // E36
        const float overshoot = box.panelMin.x+size.x-(GetScreenSize().x-kPromptScreenMargin);                       // E36
        if (overshoot > 0.0f)                                                                                        // E36
            box.panelMin.x -= overshoot;                                                                             // E36
        if (box.panelMin.x < kPromptScreenMargin)                                                                    // E36
            box.panelMin.x = kPromptScreenMargin;                                                                    // E36
    }                                                                                                                // E36
    box.panelMax = box.panelMin+size;                                                                                // E36
    box.titlePos = box.panelMin+vector2(kPromptInset, kPromptTitleY)*s;                                              // E36
    box.titleRight = box.panelMin.x+(kPromptWidth-kPromptInset)*s;                                                   // E36
    for (uint t = 0; t < 2; t++)                                                                                     // E36
    {                                                                                                                // E36
        box.rowMin[t] = box.panelMin+vector2(kPromptInset, kPromptRowTop+kPromptRowStep*static_cast<float>(t))*s;    // E36
        box.rowMax[t] = box.rowMin[t]+vector2(kPromptWidth-2.0f*kPromptInset, kPromptRowHeight)*s;                   // E36
        box.labelPos[t] = box.rowMin[t]+vector2(kPromptTextX, kPromptLabelY)*s;                                      // E36
        box.descPos[t] = box.rowMin[t]+vector2(kPromptTextX, kPromptDescY)*s;                                        // E36
        box.textRight[t] = box.rowMin[t].x+(kPromptTextX+kPromptTextWidth)*s;                                        // E36
    }                                                                                                                // E36
    return box;                                                                                                      // E36
}                                                                                                                    // E36

namespace {                                                                                                          // E36
// The prompt over a dimmed menu, last in the callback so that it is on top (the HUD draws in call order). The     // E36
// backdrop is the whole logical screen, which HudRenderer carries on to a widescreen window's sides as it does a      // E36
// fade's; the panel is the menu's dark violet-black, the lit row a bar at its left edge with full-alpha text, the     // E36
// other row at the 100 the options screen gives its idle rows. A right-to-left language sets every text against the   // E36
// right edge of its box (the rtlRight overload), the bar stays at the left.                                         // E36
void drawDifficultyPrompt(const DifficultyPromptBox& box, const uint lit)                                           // E36
{                                                                                                                    // E36
    const float s = box.scale;                                                                                       // E36
    drawRect(ARGB(150,0,0,0));                                                                                       // E36
    const vector2 size = box.panelMax-box.panelMin;                                                                  // E36
    const uint top = ARGB(244,26,18,44);                                                                             // E36
    const uint foot = ARGB(244,8,5,16);                                                                              // E36
    DrawRectangle(box.panelMin, size, top, top, foot, foot);                                                         // E36
    const uint edge = ARGB(210,203,203,228);                                                                         // E36
    const float line = 2.0f*s;                                                                                       // E36
    DrawRectangle(box.panelMin, vector2(size.x, line), edge, edge, edge, edge);                                      // E36
    DrawRectangle(box.panelMin+vector2(0.0f, size.y-line), vector2(size.x, line), edge, edge, edge, edge);           // E36
    DrawRectangle(box.panelMin+vector2(0.0f, line), vector2(line, size.y-2.0f*line), edge, edge, edge, edge);        // E36
    DrawRectangle(box.panelMin+vector2(size.x-line, line), vector2(line, size.y-2.0f*line), edge, edge, edge, edge);   // E36
    for (uint t = 0; t < 2; t++)                                                                                     // E36
    {                                                                                                                // E36
        const bool on = t == lit;                                                                                    // E36
        const vector2 rowSize = box.rowMax[t]-box.rowMin[t];                                                         // E36
        const uint fill = on ? ARGB(64,203,203,228) : ARGB(14,203,203,228);                                          // E36
        DrawRectangle(box.rowMin[t], rowSize, fill, fill, fill, fill);                                               // E36
        if (on)                                                                                                      // E36
        {                                                                                                            // E36
            const uint bar = ARGB(255,203,203,228);                                                                  // E36
            DrawRectangle(box.rowMin[t], vector2(kPromptBar*s, rowSize.y), bar, bar, bar, bar);                      // E36
        }                                                                                                            // E36
        shadowText(box.labelPos[t], difficultyName(t), "Arial Narrow", kPromptLabelSize*s,                           // E36
                   static_cast<uint8>(on ? 255 : 100), 203,203,228, box.textRight[t]);                               // E36
        shadowText(box.descPos[t], promptDescription(t), "Arial Narrow", kPromptDescSize*s,                          // E36
                   static_cast<uint8>(on ? 200 : 75), 203,203,228, box.textRight[t]);                                // E36
    }                                                                                                                // E36
    shadowText(box.titlePos, "Escolha a dificuldade", "Arial Narrow", kPromptTitleSize*s, 255,203,203,228, box.titleRight);   // E36
}                                                                                                                    // E36

void closeDifficultyPrompt(ETHEntity cursor)                                                                         // E36
{                                                                                                                    // E36
    cursor->EraseData("pickDifficulty");                                                                             // E36
    cursor->EraseData("pickRow");                                                                                    // E36
    cursor->EraseData("pickX");                                                                                      // E36
    cursor->EraseData("pickY");                                                                                      // E36
}                                                                                                                    // E36

// One frame of the open prompt. `interactive` is false on the frame that opened it: that frame's confirm is the one   // E36
// that opened it and must not also choose, so it only draws. The pointer is read as the other screens read it; a      // E36
// "click" is the left or right button's fresh press (a finger's tap is the left button held for the tick the cursor    // E36
// jumps to it, TouchControls' menu mode), so a tap and a click are one thing. getConfirmButtonStatus is not used to   // E36
// tell them apart because the first held key wins there: Enter held would hide a click. A click counts only once the  // E36
// prompt is kPromptSettleTime old (a double-click on New Game opens it with the first click and puts the second on the  // E36
// button, or on a row that a phone's panel has under it); before that it neither closes nor chooses, and is not taken   // E36
// for the confirm either. Esc, a pad's Back and Enter or a pad's confirm are never delayed. The subtraction is          // E36
// unsigned, as the other scripts' timers are.                                                                          // E36
void difficultyPrompt(ETHEntity cursor, const vector2& pointer, const bool interactive)                              // E36
{                                                                                                                    // E36
    InputState& input = GetInputHandle();                                                                            // E36
    const DifficultyPromptBox box = difficultyPromptBox();                                                           // E36
    uint row = min(cursor->GetUIntData("pickRow"), 1u);                                                              // E36
    if (interactive)                                                                                                 // E36
    {                                                                                                                // E36
        const bool pointerClick = input.GetKeyState(K_LMOUSE) == KS_HIT || input.GetKeyState(K_RMOUSE) == KS_HIT;    // E36
        const bool click = pointerClick && GetTime()-cursor->GetUIntData("pickDifficulty") >= kPromptSettleTime;     // E36
        // The highlight follows the pointer only when it moved, so a pointer at rest does not fight the keys.       // E36
        const bool moved = pointer.x != cursor->GetFloatData("pickX") || pointer.y != cursor->GetFloatData("pickY");   // E36
        cursor->AddFloatData("pickX", pointer.x);                                                                    // E36
        cursor->AddFloatData("pickY", pointer.y);                                                                    // E36
        int over = -1;                                                                                               // E36
        for (uint t = 0; t < 2; t++)                                                                                 // E36
        {                                                                                                            // E36
            if (promptInside(pointer, box.rowMin[t], box.rowMax[t]))                                                 // E36
                over = static_cast<int>(t);                                                                          // E36
        }                                                                                                            // E36

        // Cancel, or a click outside the panel (a phone's main menu has no Back button). A click inside the panel    // E36
        // on no row does nothing. Only a click closes by position: Enter with the pointer anywhere never does.      // E36
        if (getCancelButtonStatus(0) == KS_HIT                                                                       // E36
            || (click && over < 0 && !promptInside(pointer, box.panelMin, box.panelMax)))                            // E36
        {                                                                                                            // E36
            closeDifficultyPrompt(cursor);                                                                           // E36
            return;                                                                                                  // E36
        }                                                                                                            // E36

        const uint before = row;                                                                                     // E36
        if (getUpButtonStatus(0) == KS_HIT && row > 0)                                                               // E36
            row--;                                                                                                   // E36
        if (getDownButtonStatus(0) == KS_HIT && row < 1)                                                             // E36
            row++;                                                                                                   // E36
        if (moved && over >= 0)                                                                                      // E36
            row = static_cast<uint>(over);                                                                           // E36

        // A click chooses the row it is on; any other confirm (Enter, a pad's) the lit one, wherever the pointer is. // E36
        // A click that is still settling is the confirm too (the mouse buttons are in getConfirmButtonStatus): it    // E36
        // must not choose the lit row by that road.                                                                   // E36
        int chosen = -1;                                                                                             // E36
        if (click)                                                                                                   // E36
            chosen = over;                                                                                           // E36
        else if (!pointerClick && getConfirmButtonStatus(0) == KS_HIT)                                               // E36
            chosen = static_cast<int>(row);                                                                          // E36
        if (chosen >= 0)                                                                                             // E36
        {                                                                                                            // E36
            // The old start (menu.as:293-295): the 3 s fade-out, and newGame() runs when it ends. Nothing writes    // E36
            // g_difficulty again before then.                                                                       // E36
            g_difficulty.setCurrent(static_cast<uint>(chosen));                                                      // E36
            cursor->AddUIntData("newGame", GetTime());                                                               // E36
            cursor->AddStringData("scene", "CAMPAIGN");                                                              // E36
            PlaySample("soundfx/newgame.mp3");                                                                       // E36
            closeDifficultyPrompt(cursor);                                                                           // E36
            return;                                                                                                  // E36
        }                                                                                                            // E36

        if (row != before)                                                                                           // E36
            PlaySample("soundfx/help.mp3");                                                                          // E36: the menu's hover sound, once a change
        cursor->AddUIntData("pickRow", row);                                                                         // E36
    }                                                                                                                // E36
    drawDifficultyPrompt(box, row);                                                                                  // E36
}                                                                                                                    // E36
} // namespace                                                                                                       // E36

// menu.as:232. The whole menu and arena-select logic, run every frame (the
// cursor is dynamic).
void ETHCallback_cursor(ETHEntity thisEntity)
{
    detectJoysticks();

    [[maybe_unused]] const vector2 screenSize = GetScreenSize();    // menu.as:236, never read
    InputState& input = GetInputHandle();                             // menu.as:237
    // Read BEFORE the move below, so the entity lags the OS cursor by a frame
    // (menu.as:238-240).
    const vector2 cursorPos = input.GetCursorPos();
    // E36: while New Game's difficulty prompt is open the direction keys and the stick move its highlight and not   // E36
    // the OS cursor (the prompt reads the pointer as it is); the entity still follows the pointer.                  // E36
    const bool picking = thisEntity->CheckCustomData("pickDifficulty") != DT_NODATA;   // E36
    if (!picking)                                                     // E36
        input.SetCursorPos(input.GetCursorAbsolutePos()+getPlayerXYAxis(0)*5.0f);
    thisEntity->SetPositionXY(cursorPos);

    ETHEntity handle;                                                 // menu.as:242

    if (thisEntity->CheckCustomData("newGame") == DT_NODATA)          // menu.as:244
    {
        // E36: nothing behind the prompt answers the pointer: no panels, no hover sounds, no buttons.            // E36
        if (!picking && CollideDynamic(thisEntity, handle))             // E36: !picking &&
        {
            const string entityName = handle->GetEntityName();
            const bool confirmed = getConfirmButtonStatus(0) == KS_HIT;
            if (entityName == "creditos")
            {
                // E21: the enhanced edition's credit after the original team's.
                showData("Cr\xE9" "ditos", creditos + creditosEnhanced);   // menu.as:252: creditos
            } else
            if (entityName == "melhores_tempos")
            {
                showData("Melhores tempos", recordsPanelText());   // E36: both difficulties' lists (menu.as:240: the one)
            } else
            if (entityName == "como_jogar")
            {
                showData("Como Jogar", como_jogar);
            } else
            if (entityName == "versus")                                // menu.as:262
            {
                if (hasASecondController())
                {
                    showData("Jogador versus Jogador", versus);
                    if (confirmed)
                    {
                        keepMenuSong();                               // E30
                        goToPvp();
                    }
                }
                else
                {
                    if (confirmed)
                    {
                        PlaySample("soundfx/fail.ogg");
                    }
                    string message = "\xC9 necess\xE1rio ao menos um joystick\n para jogar neste modo.";   // menu.as:278
                    if (input.GetJoystickStatus(0) == JS_DETECTED)
                    {
                        message += endl + endl + "J\xE1 h\xE1 um joystick plugado." + endl
                        + "Mude as op\xE7\xF5" "es de entrada no menu" + endl + "de configura\xE7\xF5" "es para poder" + endl
                        + "utilizar o teclado e o joystick" + endl + "por 2 jogadores.";   // menu.as:281-283
                    }
                    showData("Jogador versus Jogador", message);
                }
            } else
            if (entityName == "novo_jogo")                             // menu.as:288
            {
                showData("Novo jogo", novo_jogo);
                if (confirmed)
                {
                    // E36: opens the difficulty prompt (below the panels, in this callback) instead of starting; a   // E36
                    // row chosen there starts the 3 s fade-out (menu.as:293-295), and newGame() runs when it ends   // E36
                    // (the else branch below). The lit row is the last choice; the pointer is noted so that a     // E36
                    // pointer at rest does not move it.                                                           // E36
                    thisEntity->AddUIntData("pickDifficulty", GetTime());   // E36
                    thisEntity->AddUIntData("pickRow", g_difficulty.getCurrent() == DIFFICULTY_HARD ? 1u : 0u);   // E36
                    thisEntity->AddFloatData("pickX", cursorPos.x);   // E36
                    thisEntity->AddFloatData("pickY", cursorPos.y);   // E36
                }
            } else
            if (entityName == "sair")                                  // menu.as:298
            {
                showData("Sair do jogo", "");
                // Exit() only requests the exit; the callback runs on.
                if (confirmed)
                    Exit();
            } else
            if (entityName == "opcoes_de_video")                       // menu.as:304
            {
                showData("Configura\xE7\xF5" "es", config);
                if (confirmed)
                {
                    keepMenuSong();                                   // E30
                    LoadScene("scenes/videoModes.esc", "screenModesPreLoop", "screenModesLoop");
                }
            } else if (entityName == "thumbnail")                      // menu.as:311
            {
                const string number = handle->GetStringData("name");
                const string title = handle->GetStringData("title");
                string extra = "";
                bool allow = true;

                // An arena with a "score" unlocks only for a best campaign
                // time strictly below it (menu.as:318-326). E36: the better of
                // Normal's and Hard's best times, getGetBestTime().
                if (handle->CheckCustomData("score") != DT_NODATA)
                {
                    if (handle->GetUIntData("score") <= getGetBestTime())
                    {
                        allow = false;
                        extra = "\n\n\xC9 necess\xE1rio terminar o jogo\nem menos de " +
                            getTimeString(handle->GetUIntData("score")) + " para\nliberar esta arena.";   // menu.as:323-324
                    }
                }

                showData(title, g_gameData.get("global", "arena"+number)+extra);   // menu.as:328

                if (confirmed && allow)
                {
                    thisEntity->AddUIntData("newGame", GetTime());
                    thisEntity->AddStringData("scene", "pvp_lv" + number + ".esc");
                    PlaySample("soundfx/newgame.mp3");
                } else if (confirmed && !allow)
                {
                    PlaySample("soundfx/fail.ogg");
                }
            }

            // lastButton is never reset when the cursor leaves every button, so
            // coming back to the same one is silent (menu.as:341-345).
            if (entityName != thisEntity->GetStringData("lastButton"))
            {
                PlaySample("soundfx/help.mp3");
            }
            thisEntity->AddStringData("lastButton", entityName);
        }
        showToggleFullscreenMessage();                                // menu.as:347
    }
    else
    {
        // fadeOut leaves bias unwritten on the frame it returns true, so the
        // volume snaps back to 1 then (menu.as:351-356).
        float bias = 0;
        if (fadeOut(thisEntity->GetUIntData("newGame"), bias))
        {
            newGame(thisEntity->GetStringData("scene"));
        }
        SetSampleVolume("soundfx/menu.mp3", 1.0f-bias);
        loadingMessage();
    }
    fadeIn(thisEntity->GetUIntData("menuStartTime"));                 // menu.as:359
    // E36: last of all, so that it is drawn over the panels, the Alt+Enter line AND the fade-in's black rectangle (it was     // E36
    // drawn before that rectangle, so a prompt opened in the menu's first three seconds was veiled by it while it already   // E36
    // took input). The frame that opened it only draws it (`picking` was false at the frame's start): its confirm must not  // E36
    // also choose. Only ever open while newGame is unset: choosing erases it in the same step that sets newGame.            // E36
    if (thisEntity->CheckCustomData("pickDifficulty") != DT_NODATA)   // E36
        difficultyPrompt(thisEntity, cursorPos, picking);             // E36
    // E36: the pointer the player sees is cursor.ent, drawn under every HUD command, so the prompt's panel covers it;   // E36
    // while the prompt is open the layer shows the system pointer instead (it reads this flag), and hides it again      // E36
    // as soon as the prompt is closed or a row is chosen. At the end, so that the frame that closes it already hides.   // E36
    HideCursor(thisEntity->CheckCustomData("pickDifficulty") == DT_NODATA);   // E36
}

// menu.as:362. Darkens a locked arena every frame.
void ETHCallback_thumbnail(ETHEntity thisEntity)
{
    if (thisEntity->CheckCustomData("score") != DT_NODATA)
    {
        if (thisEntity->GetUIntData("score") <= getGetBestTime())
        {
            thisEntity->SetColor(vector3(0.05f, 0.05f, 0.05f));
            //thisEntity.SetAlpha(0.8f);
        }
    }
}

// menu.as:374
bool waitForInputToMenu()
{
    if (getCancelButtonStatus(0) == KS_HIT)
    {
        return goToMenu();
    }
    return false;
}

// menu.as:383
bool goToMenu()
{
    if (GetSceneFileName() != "scenes/menu.esc")
    {
        keepMenuSong();                                               // E30: from the settings or the arena select
        LoadScene("scenes/menu.esc", "menuPreLoop", "menuLoop", vector2(1024,256));
        return true;
    }
    return false;
}

// menu.as:393. Unlike goToMenu, it reloads the menu even from the menu.
bool escToGoToMenu()
{
    InputState& input = GetInputHandle();                             // menu.as:395
    if (input.GetKeyState(K_ESC) == KS_HIT)
    {
        LoadScene("scenes/menu.esc", "menuPreLoop", "menuLoop", vector2(1024,256));
        return true;
    }
    return false;
}

} // namespace Penumbra::Script
