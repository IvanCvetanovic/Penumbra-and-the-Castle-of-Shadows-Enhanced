# E31: the options screen on a phone

Built 2026-10-01, on the options screen of E10, E20, E23, E24 and E27 and the touch controls' editor of E28. This page is what was built, the numbers it
rests on, where the build differs from the geometry first worked out for it, and what is still open. The player's view is in
[`../enhancements.md`](../enhancements.md); the code is `game/script/optionsPhone.cpp`, with `game/render/PhoneUi.*` and a few lines of
`game/PenumbraLayer.cpp`.

## What it is

Where the phone layout is up (`Script::g_mobileLayout`) and the options art is loaded, `screenModesLoop` hands the frame to `phoneOptionsLoop()` and
returns. The desktop's layout, and E20's phone layout without the art (which the suites that have no data folder still run), are untouched.

```
 [<Back]  Video Options                          (globe) [<] English   [>]      <- header: pinned to the corners of what the window shows
 +--------------------------------------------------------------------------+
 | [v] Enable pixel shaders        | [v] Disable smooth motion              |  rows 88 px tall, cells 474 px wide
 | [v] Widescreen                  | [v] Play on without focus              |
 |     Takes effect from the next  |                                        |
 | [v] Enable touch controls       | [>] Adjust controls                    |
 | [v] Keyboard for player 2       | [v] [keyboard | pad: Player 1/2 image] |
 | Refresh rate                    | Zoom                                   |
 | [<]  Automatic (highest)   [>]  | [<]  Automatic                    [>]  |
 | Music volume      [-] 100% [+]  | Effects volume        [-] 100% [+]     |
 +--------------------------------------------------------------------------+
```

- One layout for every window shape, with no paging and no scrolling. The body is a stone panel inside x 12 to 1012 of the 1024x768 screen; the header
  follows the corners of what the window shows. The height is what limits it, not the width, so a 20:9 phone gets the same body as a 4:3 tablet, and its
  extra width carries the header and the room's scenery.
- Rows are 88 logical px (down to 68 where a bottom inset leaves less room). The text is 30 px (labels 26, the widescreen hint 22, the title 40), check boxes
  44 px, arrow, minus and plus buttons drawn at 56 px and hit over 86 px. On a 2400x1080 phone at density 2.625 a row is about 47 dp; the old rows were 25 px, about 13 dp.
- A two-way `Switch` is one cell: a ticked box and the wording of the state it is in, and a tap anywhere in the cell flips it. The six pairs and E28's
  Adjust button therefore take four rows of two cells instead of twelve rows. Refresh rate and Zoom, and Music and Effects, sit side by side. All 14
  controls stay, in all eleven languages, with no new string.
- The language chooser (globe, arrows, the name in its own script) is in the header's top-right corner, where a player who cannot read the screen looks;
  it has no label. The Back arrow is in the top-left corner, inside the safe area or E26's edge margin, whichever is more, in every language (the screen
  is not mirrored in Arabic).
- No hover: a cell lights only while it is pressed, and idle text is at full alpha (the old layout drew the row not selected at 100 of 255).

## Why

On a phone the screen was E27's panel over the original's single column: 22 px check boxes and 24 px arrows in 25 px rows, about 13 dp each on a
2400x1080 screen, the unselected half of every pair dim, and the Back arrow at the top right of the 4:3 box. A 1080 px tall phone is only 360 to 411 dp
tall, so the original's controls in two-row switches (23 rows) could never be made touch-sized in one column.

## Alternatives not taken

- A single column of 85 px rows with pages or scrolling: 14 controls are 1190 px, over the 768 px screen. Pages need a state and a test per page; scrolling
  needs a clip primitive, which `HudCmd` has not (rows would draw over the header).
- Two-row radio pairs, enlarged: six pairs alone are 12 rows of 85 px. One collapsed row keeps both wordings and a bigger target.
- A segmented two-halves control: the longest wording is 372 px at size 30 (Italian), so two halves need over 744 px per switch.
- Hiding keyboard-for-player-2 and the joystick layout on a phone: the rows are capped at 88 px with them in, and with the touch controls off and a
  Bluetooth pad they still do something.
- Three columns on a 20:9 window: a second geometry, with its own rooms, for rows the 88 px cap does not need.
- A mirrored layout and Back arrow for Arabic: nothing else in the game is mirrored (labels anchor at their left, E28's editor has its Back arrow top-left
  in all eleven languages). To be revisited with native readers.
- Scaling the desktop layout: twice the size makes the panel 1360 px wide, past the 1024 px box, and 1500 tall.

## The geometry

Logical px of the 1024x768 screen; every number is a `k` constant of `optionsPhone.cpp`, and `phoneOptionsLayout(area, touchOn, refreshRow)` is pure, so
the suites pin it. `area` is what the window shows (below); `F` is the frame's top, `B` its bottom, `fl`/`fr` its left and right insets, `sMin`/`sMax` the
shown x range.

```
Hc       = clamp(floor((564 - B - F) / 6), 68, 88)                 564 = 768 less the fixed parts (204)
Back art = (floor(sMin.x + fl + 8), F + 8, 123, 92)                the art at its own size; the title at (backX + 139, backY + 26), size 40
Back hit = (floor(sMin.x) - 1, -1, backX + 143 - floor(sMin.x) + 1, backY + 101)
language = centre cy = backY + 46, right edge floor(sMax.x - fr - 8): [>] 86 x 84, value box 168, [<] 86 x 84, globe 56 at 8 px left of [<]
panel    = left max(12, ceil(sMin.x + fl)), right min(1012, floor(sMax.x - fr)), W clamp(right - left, 800, 1000); x = left (12 on every phone),
           y = F + 102; cells (W - 52) / 2 wide (474), 20 apart, 16 px inside the panel; rows Hc tall, 6 apart; a 10 px gap before the choosers
rows     = R1 pixel shaders | smooth motion   R2 widescreen | pause on focus loss   R3 touch | Adjust controls (while touch is on)
           R4 keyboard p2 | joystick image     R5 refresh rate | zoom (30 + Hc tall: a label strip, then the buttons)   R6 music | effects
```

The 2400x1080 phone (area -341.33 to 1365.33, frame 60/27/60/0, `Hc` 88): Back art (-274, 35), its hit box (-343, -1, 212, 136), title (-135, 61), globe
(893, 53), language buttons (957, 39) and (1211, 39) with the value box (1043, 39, 168, 84); panel (12, 129, 1000, 624); rows at y 145, 239, 333 and 427,
the choosers' cards at y 525 (118 tall) and the volumes' at 649; columns at x 28 and 522. Other shapes move only in y with `F` and `Hc`, and the header with the area:

| Shape | F | Hc | Back art | Panel |
|---|---|---|---|---|
| 1024x768 (frame 10/8/10/0) | 8 | 88 | (18, 16) | (12, 110, 1000, 624) |
| 1920x1080 (frame 14/8/14/0) | 8 | 88 | (-149, 16) | (12, 110, 1000, 624) |
| 1280x800 (area -102.4 to 1126.4) | 8 | 88 | (-83, 16) | (12, 110, 1000, 624) |
| 2532x1170, safe area 132,0,132,63 px (frame 87/27/87/41) | 27 | 82 | (-225, 35) | (12, 129, 1000, 588) |
| 1024x768 with 88/0/88/24 px cut-outs | 8 | 88 | (96, 16) | (88, 110, 848, 624), cells 398 |

Inside a cell (offsets from its `(x, y, w, h)`, `cy` its vertical centre): a toggle's check box (`check_on`, always the ticked one, as it marks the wording shown) is
44 px at (x + 12, cy - 22) and its text at x + 66, size 30, room 396; the widescreen cell centres a block of 30 + 2 + 22 px, the wording above its hint
(alpha 170). The Adjust cell has an `arrow_right` icon and "Adjust controls" in the same places. The joystick cell shows the current one of the two
300x72 pictures (scaled down to fit `h - 10`) after its check box. A chooser's label is at (x + 14, y + 2), size 26; its buttons are hit over 86 x `Hc` at 4 px
from the card's edges, with the value centred between them; a stepper's label is at (x + 14), its `[-]`, a 68 px value box and `[+]` at the right. Cards are
ARGB(34, 203, 203, 228) with a 2 px line at alpha 60, and alpha 92 while pressed; the panel is E27's (alpha 232, 13 px corner).

Vertical budget: the panel's bottom is `F + 198 + 6 Hc` and must clear the bottom inset by 6, so 2400x1080 leaves 15 px under the panel, a notched iPhone shape
(bottom inset 41) leaves 10 above its inset with `Hc` 82, and a 150 px bottom bar on a 1280x720 window (about 160 logical px) forces `Hc` 68, with the panel's bottom 6 px under the bar. 88 is the cap because six
rows already fill the 768 px at `F` 27.

## Behaviour

- A hit is the cursor strictly inside a rectangle with player 1's confirm at `KS_HIT`. A finger on no control is the mouse (`TouchScene::Menu`): the cursor
  and the left button in the same tick, and the cursor stays where the finger lifted, so nothing may light on hover.
- A toggle, the Adjust cell and the joystick cell answer over the whole cell. A chooser or a stepper answers only over its two buttons; its label, value box
  and card do nothing. A button at its end is drawn at alpha 70 and does nothing; nothing wraps. The cards are 6 px apart in a column and 20 px between
  columns, and the hit boxes are the cards' own, so a press in a gap does nothing, which is better than the wrong toggle flipping.
- A press is the cursor inside with the confirm at `KS_HIT` or `KS_DOWN`: the card is at alpha 92 and a button is drawn at 64 px, its art's own size. A lifted
  finger is neither, so nothing stays lit.
- Order in the frame, as the desktop's, so the layer's reads after it see the new state: the panel; every cell, updated then drawn; the title and the language
  chooser; `UsePixelShaders`, `showToggleFullscreenMessage`, `waitForInputToMenu`; the Back arrow last, `if (hit) goToMenu()`. Back leaves through `goToMenu()`,
  not `LoadScene`, so the menu's song (E30) goes on, as it does for Esc.
- The cells only read and write the globals the old rows did (`g_enablePS`, `g_touchControls`, `g_keyboardP2`, `g_controls`, `g_widescreen`,
  `g_smoothMotion`, `g_pauseOnFocusLoss`, `g_refreshRate`, `g_zoom`, `g_language`, the volumes, `g_adjustTouchControls`), so the layer's code after the frame is
  unchanged. Four const accessors were added for the drawing: `Switch::getLabel`, `Switch::getImage`, `Stepper::getLabel`, `Chooser::getLabel`.
- E28: the Adjust cell raises the same flag under the same condition. The editor's Back arrow (64 px at 41, 41 from the area's corner) and this screen's share
  the corner; E28's four leak guards make that harmless, and a test pins it.

## The published area

The layer sets `Script::g_optionsArea` every tick, right after `SetSidesShown`, from the pure `Render::ComputeFixedLayoutArea(window, sideMargin, safeArea,
marginPercent)`: E1's view of a 1024x768 fixed-layout scene (`scale = min(w/1024, h/768)`, the box on a whole pixel) widened by `Machine::SideMargin()` as far as
the window goes, and the frame inside it (`ComputeHudFrame`: the safe inset or the edge margin, whichever is more). The margin is the touch controls' always, not the
live switch, so the corners stay where they are when this screen turns the controls off. The sides are the scene's, not the window's: flipping the widescreen switch
applies from the next scene, so the header must not move under a screen that has not. Where none is published (the suites) the loop uses the whole screen with no frame.
It publishes every tick, whatever the scene, because the first frame of a scene runs before the layer could know the scene had changed.

## Text fit and rooms

The wordings were measured with the game's `FontAtlas` in all eleven languages, at several raster scales. Rooms: toggles, Adjust and the hint 396 px; chooser labels 446;
stepper labels 206; the language value box 148 (the other two chooser values 274); the title 429. Tightest: Italian "Pausa quando il gioco perde il focus" 372 of 396,
the language boxes' "Automatica" in German 130 of 148, the stepper labels 171 of 206 (Spanish), "Automatica (maxima)" 232 of 274. Nothing is over.

`tests/data/l10n_rooms.json` gained a `phone` member on the 28 entries drawn by `videoModes.cpp` or `switch.cpp` (22 rooms, 6 `null` for texts this layout does not draw:
the Language label, the window switch's two wordings, and three texts of the video-mode list and the fullscreen hint). `TestRooms` requires it on any such entry, measures it like the desktop room and reports
`room <lang> phone "<key>" line N: ... px > ... px`. When a body is narrower than 1000 px (only a 4:3 window with side cut-outs) every body text is set through
`TextFit` (`minScale` 0.7, never wrapped), so the Italian pause wording at 848 px shrinks to about 0.86.

## Where the build differs from the geometry first worked out

The first in-engine render matched the mockups and no constant was retuned. Seven things differ in the code:

1. The Back hit box reaches one px past the corner on its left and top edges: a cursor clamped to the shown area sits exactly on the edge and a hit is strictly inside.
2. Narrow bodies draw text through `DrawText` with the fit directly, not `shadowText`'s `TextFit` overload, which gives its shadow a right-to-left edge even for 0 and so
   would set an Arabic shadow at the box's right with its text at the left.
3. A pressed button is drawn at 64 px instead of being lit with a square: a rectangle meeting x 0 or 1024 is stretched out to the shown edge by `HudRenderer`, and the language
   chooser's `[<]` sits across x 1024 on a 20:9 phone (seen in-engine, then fixed). No script rectangle crosses those edges.
4. The Adjust slot is always laid out and gated on the live touch switch, so a tap that turns the controls on or off shows or hides it in the same frame.
5. The notch case's Back arrow is at x -225, not -224: the shown edge is -319.015.
6. `Chooser::getOption` stays; the language name is drawn with it.
7. The tags are `// E31`.

## Tests

`test_pn_all` on Linux: 17 suites, 39,140 checks, 0 failures (28,556 at E28). Per suite: render_input 10,224, render_hud 13,112, scenarios 1,788 (scenario 26: 640),
render_touch 7,307, render_pause 4,088, audio 304; the others are unchanged.

- `test_pn_render_input`: `ComputeFixedLayoutArea` in five shapes and a zero window, and against `CameraRig::ComputeView`'s own shown rectangle in 20 window and
  side combinations; `phoneOptionsLayout` rect by rect in eight shapes (2400x1080, 1024x768, 1920x1080, 1280x800, 2532x1170 with a notch, a 150 px bottom bar, a 4:3 window with
  88 px cut-outs, no area), with the invariants (inside 12 to 1012, no overlapping hit boxes, every cell inside the panel), touch off and no refresh row.
- `test_pn_render_hud`: the rooms (above), plus three more checks: the measurement is run on a room one px too narrow and one line short and must say "over", and on the real room and
  one exactly as wide as the text and must not; the room numbers in the file equal the layout's own, so a layout change that narrows a cell shows as a stale room; the title fits the room the
  narrowest header leaves (273 px; the widest title is 258, Spanish). `TestLanguages` holds every language name and "Automatica" to 148 px.
- The layer test: the editor of E28 and this screen's Back arrow share the corner. A tap on the editor's, of one tick or three, closes the editor and leaves the screen up for 40 ticks; a finger put
  down on it and held across the Esc that closes it presses nothing, nor does its lift; and then a fresh tap on the same spot leaves for the menu. With E28's guards 3 and 4 both removed the
  held-finger leg fails; guard 3 alone does not leak, because guard 4 covers the held finger too.
- Scenario 26, with the art on and the area set by hand in four shapes: the exact 18 front texts of a frame (17 at alpha 255, the hint at 170), every toggle flipping once on a tap at its centre and
  at its corners and not on its edges or in the gaps, the Adjust cell (dead and absent with touch off, back in the frame the touch cell is tapped), steppers and choosers stopping at their
  ends, a card at alpha 92 while pressed and 34 after, Back by tap, by the screen's corner and by Esc with the menu's song never stopped, the iOS slot, the narrow body's fit, and no rectangle
  meeting x 0 or 1024. Mutations tried in a scratch copy, each caught: the Back box not past the corner, a card lit on hover, the Adjust cell live with touch off, a hit not strictly inside, Back by a
  bare `LoadScene`, a toggle flipping every frame the confirm is held.

## Captures

Headless, Linux/lavapipe, `--touch on --mobile-layout on --fixed-step`, each in a throwaway user directory: 2400x1080 in all eleven languages, 1920x1080, 1280x720, 1280x800, 1920x1200,
3120x1440, 2532x1170 with `--safe-area 132,0,132,63`, 1024x768, 1024x768 with 88/0/88/24 px cut-outs (the 848 px body), touch off, widescreen off and a 150 px bottom bar; presses by `--finger`, `--pointer` and
`--cursor` on every kind of cell, button, gap and the Back arrow; the editor opened from Adjust and closed again. The desktop's options screen (`--mobile-layout off`) in five captures (1280x720, 1024x768
in Portuguese, 1920x1080 in German with touch off, a hover and a click) is byte-identical to the build before E31 (SHA-256 of the 1280x720 one begins 599060e7).

## Open

- No real phone has been tried. The dp figures are model values: a row is 47.1 dp at 2400x1080 and density 2.625, 44.2 at 2.8, 41.2 at 3.0; a button's 86 px hit box 46.1, 43.2 and 40.3; an
  iPhone 2532x1170 at 3x (`Hc` 82) 41.6 and 43.7. A row at the bottom-inset minimum (68) is 36 dp.
- The collapsed switch shows only the wording of its current state; the other wording is not shown. A dim second line (22 px, room 396) is the cheap remedy if that reads unclearly.
- Where a narrow body shrinks a text, the line's top stays where it was, so it sits a little high.
- A boot straight into the options screen (`--start videoModes`) lays its first frame out for the 4:3 box, the window not being known yet. Coming from the menu has no such frame.
- The art was not regenerated. At 3120x1440 a 56 px button is about 105 screen px from 64 px art; the captures look crisp enough. If it does not on a device, `ICON` in
  `tools/art/make_options_art.py` can be raised to 96.
- The room widths were measured with the stand-in faces, whose advances are within 1/2048 em of Arial Narrow; the tightest slack is 18 px. The iOS layout without the refresh row is checked in the
  pure layout and in scenario 26 but not in an engine capture. The Arabic, Japanese, Turkish and Ukrainian wordings have not been read by native speakers (unchanged), and a screen reader is not part of the game.
- The art-less E20 layout stays as code only the suites without a data folder run; it can go when scenarios 21 and 24 are moved onto the new gate.
