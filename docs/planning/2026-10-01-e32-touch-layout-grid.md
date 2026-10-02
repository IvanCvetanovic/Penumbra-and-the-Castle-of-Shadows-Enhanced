# E32: the touch controls' action buttons as a grid

> **2026-10-02: superseded by E33** ([`2026-10-02-e33-touch-grid-staggered.md`](2026-10-02-e33-touch-grid-staggered.md)). The arrangement below is E32's. E33 replaced it with two staggered
> columns, nearer the edge and closer together, which changes every number in "What it is", the size ceilings and the pause clearances, and the editor tiles' overlap with the Light button (the
> open item below: it remains only where a notch narrows a 5:4 screen). E34 made a saved tuning's moves follow the default's arrangement, which this page's "nothing is migrated" did not. The page is kept as it was written.

Built 2026-10-02, on E29's two columns of action buttons and E28's editor. This page is what was wrong with E29's numbers, what was measured to replace them, the
numbers, the test rules that had to move and why, and what was checked. The player's view is in [`../enhancements.md`](../enhancements.md) and
[`../controls.md`](../controls.md#touch-controls-e16); the numbers are in `game/data/touch_controls.json` and `TouchControls::DefaultManifest()`
(`game/render/TouchControls.cpp`), which must be equal (`testManifest`).

## What was wrong

E29 packed the six action buttons against the bottom-right corner: the right column's edge 24 units from the safe area's, the columns 16 apart, a button and the
next one 16 apart, and the right column 30 higher than the left so that the two bottom buttons followed the arc of a thumb pivoting at the corner; the stagger put the
jump button 30 units above the sword button. Those numbers came from a model of a thumb pivoting at the corner, not from a layout placed by hand. A layout placed by
hand in the editor on a 2992x1344 phone was measured, and its structure (below) was adopted.

## What was measured

A layout placed by hand in the editor (E28) on a 2992x1344 phone was measured from its screenshot: the logical screen is 768 tall, so 1.75 px a unit. The display has a
cut-out on the left of 199 px (113.7 units), found by pixel match, and none measurable at the top or the
bottom. **The right inset is assumed 0**: it is not observable in the screenshot (the pause button, which had not been moved, only bounds it, at 104.7 px or 59.8 units
at most). With no inset on the right or bottom a control's distance from the safe area's corner is then its distance from the screenshot's edge; if the display has a
right cut-out of r units, the shipped default, anchored at the safe area's corner, sits r units further in than the placement it was derived from. The editor's outlines were
located exactly (grey, slightly blue, 3 px wide) and the shipped default of the time (E29's) re-rendered with the Linux headless build at the same size,
`--safe-area 199,0,0,0` and the screenshot's opacity (1.6; size 1.0) reproduced the screenshot to the pixel apart from the six buttons. The default idle opacity,
`idleAlpha`, was not revisited. The distances of each button's box from the right and bottom edges, in units:

| Button | R | B |
|---|---|---|
| jump | 112.6 | 53.7 |
| sword | 314.3 | 44.6 |
| fire | 121.7 | 318.9 |
| light | 310.9 | 323.4 |
| sword combo (R of its 100 box) | 322.9 | 176.0 |
| spell combo (R of its 100 box) | 127.4 | 193.1 |

Read as deliberate: the cluster sits much further in from the right edge (about 117, not 24), the columns are spread (right edges about 117 and 313, so about 76
between two columns of 120, not 16), and the rows are a little further apart. Read as imprecise dragging: the columns' x (within 1.7 and 4.5 units of a mean), the
rows' y (top 321 within 2.3, bottom 49 within 4.5, the combos 176 and 193), and E29's stagger, which the picture does not support (the right column is 9 and 17 units
higher than the left at the bottom and middle rows and 4.5 lower at the top, not 30 higher throughout).

## What it is

Offsets in units, inward from the bottom-right safe corner (anchor `bottomRight`); sizes (120 and 100) and hit paddings (4 and 6) are E29's.

| Control | E29 [x, y] | E32 [x, y] |
|---|---|---|
| jump | 24, 54 | 117, 49 |
| sword | 160, 24 | 304, 49 |
| fire | 24, 306 | 117, 321 |
| light | 160, 276 | 304, 321 |
| sword combo | 170, 160 | 314, 195 |
| spell combo | 34, 190 | 127, 195 |

The direction control (offset [24, -138]), the down button (exitDown [161, 160]) and the pause button do not move. The grid:

```
 right column R 117..237        left column R 304..424
 fire        y 321..441         light       y 321..441        top row: 321 above the bottom edge
 spell combo y 195..295         sword combo y 195..295        middle: halfway, 26 clear of each button, 136 between row centres
 jump        y  49..169         sword       y  49..169        bottom row: 49 above the bottom edge
```

- The columns are 67 apart (E29: 16), the rows level (E29: the right column 30 higher), the three rows 136 apart centre to centre, each combo centred over its
  column and halfway between its two buttons (E29: 16 between neighbours).
- The cluster is 307 wide (E29: 256) and reaches 441 up from the edge (E29: 426).
- The direction control's two buttons are drawn with their centres 83.7 above the edge (E25), so they now sit 25 under the bottom row's centres (109); they were level
  with the sword button's.
- A tuning saved under E29 keeps its moves, which are deltas from the default's places and are not scaled, so they are laid on the new places until Restore, which
  clears the moves only (the size and the opacity stay). Nothing is migrated.

### The one number that is not the clean grid

A clean grid would have the left column at 313 (76 between the columns) and the sword combo at 323. On a 4:3 screen that a notch narrows to 848 units (the shape
`CheckLayoutFits` and `testSizeCeiling` use: 88 px cut-outs on both sides, 24 at the bottom) the direction control's box ends 24 + 400 = 424 from the safe area's
left edge and the left column's box starts 313 + 120 = 433 from its right edge: the boxes would overlap by 9 units (and the drawn right button's face would reach 3 units
into the sword button's hit padding). The columns were therefore brought together by exactly 9: the sword, the light at 304 and the sword combo at 314, so that the
boxes touch (`DrawnOver` is strict). The direction control did not move. The sword is then 10.3 units nearer the right edge than the picture has it, the light 6.9,
the sword combo 8.9; the right column is untouched.

## Residuals against the picture

Rendered with the Linux headless build at 2992x1344 and `--safe-area 199,0,0,0` (the editor, `--start videoModes --touch on --mobile-layout on --touch-editor
unlocked`, no tuning) and measured as above; the table is the new layout less the measured one (R: negative is nearer the right edge; B: positive is higher):

| Button | dR | dB |
|---|---|---|
| jump | +4.4 | -4.7 |
| sword | -10.3 | +4.4 |
| fire | -4.7 | +2.1 |
| light | -6.9 | -2.4 |
| sword combo | -8.9 | +19.0 |
| spell combo | -0.4 | +1.9 |

Every residual but two is within 9 units. The sword's x (10.3) is the 9-unit shave above. The sword combo's y (19.0) is the choice of the even grid: the picture has
that combo 11 units above the sword button and 47 under the light, the other combo 19 and 26 from its neighbours; the grid puts both 26 from theirs. The structure
(two columns, the right one far in from the edge, a wide gap, rows level) is the picture's.

## The rules of the test suites, and what moved

The first run of the suites after only the manifest change (before any test was touched): `test_pn_render_touch` 7,307 checks and 276 failures, `test_pn_render_pause`
4,088 and 7, `test_pn_render_hud` 13,112 and 0. By cause:

- **Literals that restated E29's numbers** (231 of the 276, and the 7 of the pause suite): `CheckColumns`' margin 24, gap 16 and rise 30, and "the left column is the lower
  one"; `testTuningIdentity`'s table; `testTuningSize`'s rows at 1.2, 1.4, 0.5 and 0.4; the finger points of `testTuningSize` and `testTuningMoves`; `testMoveFor`'s pins;
  `testDpadLowered`'s "level with the sword" and "the jump is higher"; `testButtonColumns`' boxes and gaps; the editor's control centres in `test_pn_render_pause`. They
  were rewritten: the relations a grid obeys (`CheckColumns`: equal column edges, level rows, evenly spaced rows, each combo halfway between its buttons, the columns apart
  by at least a row gap, not interleaved, the right column at least 100 from the edge, the top row 150 clear of the pause), the six boxes' size-scaled places derived from the
  manifest (`testTuningSize`), the moved boxes derived from the layout (`testTuningMoves`, `testMoveFor`) and the gap points derived from the layout's own gaps
  (`testButtonColumns`), while the absolute numbers are pinned in two places only: the exact table of `testTuningIdentity` and the boxes of `testButtonColumns` (4:3, 16:9, 20:9
  and a notched 16:9) and the editor's centres in `test_pn_render_pause`.
- **Real geometric rules, kept**: inside the safe area, nothing drawn over anything (`DrawnOver`, unchanged), the pause clearance. The shave above is what `DrawnOver` required.
- **Proxy rules, restated on the direction control's two drawn buttons** (`ArrowFace`, the art's own fractions, which `GrabBox` and `testSizeCeiling` use; `CheckLayoutFits`
  reads them once and lays them on each screen's box). Its box is a 400-unit canvas that is mostly empty, and its reach is a disc of 200 + 30 from its centre; on the notched
  4:3 screen that disc covers the sword button's padded box whatever the grid (the left column would have to be less than 270 from the right edge to stay off it: at 269 the
  padded box starts 231 from the disc's centre, at 270 exactly 230, which `ReachBoth`'s `<=` still counts as reached), and `TouchControls::hit` sends a finger both reach to
  the nearest centre. So for the shipped look (the placeholder's disc keeps the old rules) the half-screen rule on the buttons (`min.x > 0.5w`) is now "at or past the half
  (`>=`: at 304 on the notched 4:3 screen the sword's box starts exactly on it, 936 - 304 - 120 = 512) and right of the direction control's right button", and `ReachBoth`
  between the direction control and any other control is "neither drawn button overlaps its padded box". Nothing was deleted. The face is 502.15 from the screen's left on that
  shape, the sword's box starts at 512 (9.85 clear) and its padded box at 508 (5.85 clear). The half-screen line is kept, not replaced by the face alone: the face's right edge is
  far short of it (about 414 on a bare 4:3 screen, where the half is 512, and the half is 683 on a 16:9 screen and 853.5 on a 20:9 one), so the face rule alone would have
  let the buttons come to the left of the middle of those screens.
- **A rule relaxed**: the buttons stay below 40% of the screen's height (`min.y > 0.4h`) became 39% for the shipped look (the placeholder's keeps 40%). The top row rose by 15: on a
  4:3 screen with a 24 px bottom bar it reaches 39.45% (E29: 41.4%; the tightest case, 303 of 768), and with a 20 px bar 39.97%. The line was a proxy for the thumb's reach; what
  keeps the top row from the pause button is the clearance that is pinned.
- **The ceilings** (`testSizeCeiling`), measured, not bent: the size above which the shipped layout draws a control over another in play is **1.0** on every set, where E29's was 1.2.
  The direction control's right button meets the sword button on the notched 4:3 screen from 1.1 (the faces 10 apart at 1.0; on a plain 4:3 screen from 1.3, which is derived and
  not pinned: the face's right edge is 414.15 s from the left edge and the sword's box starts at 1024 - 424 s, so they first meet above s = 1.222, which is 1.3 in steps of 0.1),
  and that is the only overlap one step above the ceiling on all four sets. The fire button's clearance of the pause button, in 768ths of the screen, least over the cases:

  | Size | 1.0 | 1.1 | 1.2 | 1.3 | 1.4 |
  |---|---|---|---|---|---|
  | E1's 768 bars | 72.9 | 28.8 | -15.3 | -59.4 | -103.5 |
  | window pixels | 101.8 | 57.7 | 13.6 | -30.5 | -74.6 |

  (E29: 87.9 and 116.8 at 1.0.) The pause alone would allow 1.1 in E1's pixels and 1.2 in window pixels; the editor's top size, `kMaxSize`, stays 1.4. The pins: the six
  ceilings (the unzoomed screens, the nine bars, and the 48 px and the 100 px bars, each in both readings), the pairs that overlap at 1.1 (every set), at 1.2 (the two in-play sets: the fire button against the pause under the 100 px bar in E1's pixels, and the down button against
  the sword button on the notched 4:3 screen) and at 1.4 (the unzoomed and nine-case sets), and the clearance at 1.0 (at least 70), 1.1 and 1.2.

## Checked

- `tools/build_linux.sh --test`: 17 suites, 39,290 checks, 0 failures (before: 17 suites, 39,140; `test_pn_render_touch` 7,457 of them, from 7,307, and `test_pn_render_pause` unchanged at 4,088).
  `tools\check.bat` on `TouchControls.cpp`, `test_pn_render_touch.cpp` and `test_pn_render_pause.cpp`: no warnings at /W4. Windows suites were not run (Smart App Control).
- With the left column and the light put back at 313 and the sword combo at 323 (in `touch_controls.json` and in `DefaultManifest()` both, so that `testManifest` stays equal),
  `test_pn_render_touch` fails 37 checks, so the rules do not pass vacuously: the half-screen rule on the sword and the light (4: the box starts at 503, short of the half, 512, though
  the drawn face, which ends at 502.15, would still be clear), `DrawnOver` between the direction control and the sword (2), the direction control's two drawn buttons against the sword
  button's padded box (2), `testTuningIdentity`'s table (3), `testButtonColumns` boxes (18 on its three screens, 6 on the notched 16:9) and the two overlap lists at 1.2 (2).
  The pause suite was not run in that state.
- Renders of the editor and of play at 2992x1344 with `--safe-area 199,0,0,0`, at 2400x1080 with `--safe-area 132,0,132,63` (a notch), at 1024x768 with 88/0/88/24, and at 1920x1080,
  2560x1600, 1280x800, 1280x720 and a bare 1024x768 were read: no control is drawn over another in play on any of them, nothing is clipped by the safe area, and the six boxes measured in the editor renders
  are the manifest's within a unit. The shipped screenshots (`docs/images/*.jpg`, `site/images/*`) were not retaken; they show E29's layout until the release's pass.
- The Android emulator (Android 13, x86_64, a 1344x2992 screen, landscape, a fresh install of a debug APK built from this tree, `--safe-area 199,0,0,0` in the args file): the
  unpacked `touch_controls.json` holds the new numbers, and the editor capture's box edges equal the Linux render's, edge for edge. Measured in that capture the left column's centres
  are within 0.5 px of each other in x, and so are the right column's; the paired buttons of each row have the same y (0 px apart); the columns' centres are 187 units apart and the
  rows 136. A real-touch pass with `adb shell input` only: the menu, Settings, Adjust controls (opens locked), the padlock (unlocks), Restore (the layout is already the default:
  nothing moves), Back twice, New Game, level 1 with the six buttons and the direction control where the editor had them; the jump button held lights and the wizard rises, the sword
  button held lights and a slash is drawn; Pause opens the pause menu. The emulator reports no cut-out, so the 199 px inset was given by flag; a real cut-out, Android 15's
  edge-to-edge window and any right inset were not exercised.

## Open

- **Not tried on a real phone**: the layout has been seen in the Linux headless build and in the Android emulator only.
- **The right inset is assumed 0** (not observable in the screenshot, above): with a right cut-out of r units the cluster sits r units further in than the placement it was derived from.
- **Design note, not acted on**: the right margin of 117 was measured on a phone with a 113.7-unit cut-out on its left, so on a screen with no cut-outs the direction control sits 24
  from the left edge and the cluster 117 from the right edge: the layout is asymmetric there. One option, not done: for the touch controls, mirror the larger of the two side insets to both sides.
- **The editor's tiles over the Light button on 4:3 screens** (new with E32: E29's left column started 160 + 120 from the edge, clear of every tile). The tiles are centred on the shown area
  (rows 168, 276 and 392 below the safe area's top, 96 across; the Lock and Restore pair is centred on 512 for a 1024-wide area, Restore centred at 568 and spanning 520 to 616) and the Light
  button's box now starts 304 + 120 from the safe area's right edge. A widget wins a hit test over the control behind it and Restore is destructive (it clears the moves and locks the editor:
  `TouchEditor.cpp`), so a finger on the covered part of the Light button lands on the tile. The overlaps, in units of the screen, width by height, the opacity-up tile being the right one
  of the second row:

  | Shape (insets left/top/right/bottom) | Tile over the Light button | Basis |
  |---|---|---|
  | 4:3, 1024x768, none | Restore 16 x 96 | captured |
  | 4:3, 0/0/0/24 | opacity-up 72 x 21; Restore 16 x 79 | captured |
  | 4:3, 88/0/88/24 (the notched shape) | opacity-up 56 x 21; Restore 96 x 79 | captured |
  | 4:3, 0/0/0/20 | opacity-up 72 x 17; Restore 16 x 83 | modelled |
  | 4:3, 0/30/0/20 | opacity-up 72 x 47; Restore 16 x 53; Restore over the sword combo button 6 x 17 | modelled |
  | 5:4, 960 wide, 88/0/88/24 | opacity-up over the fire button 5 x 21 and over the Light 24 x 21; Lock over the Light 24 x 79; Restore over the Light 80 x 79 | modelled |
  | 3:2, 1152 wide, 0/30/0/20 | opacity-up 8 x 47 | modelled |
  | 16:10, 1229 wide, 88/0/88/24 | opacity-up 57.5 x 21; Restore 1.5 x 79 | modelled |
  | 16:9 and 20:9, every inset | none | captured at 2992x1344, 2400x1080, 1920x1080 and 1280x720, modelled for the rest |

  On the plain 4:3 screen the art does not overlap: along a pixel row through the tile's centre the tile's art (its soft shadow included) fades out by about x 611 and the Light's face starts
  at about 613; but a tap at (610, 392) fires Restore, and one on the Light's face (640, 392) does not. On the notched 4:3 screen about 70% of the Light's face is under the Restore tile: a tap at
  its centre fires Restore, a drag from there does nothing, and a drag from its uncovered top strip still moves it. "Captured" is a render of the editor with the Linux headless build
  (`--touch-editor unlocked`, with the safe area given); "modelled" is the tile and box arithmetic from `TouchEditor.hpp`'s constants and the manifest, which reproduces the captured
  rows. A fix, not done: shift the Lock and Restore pair left on narrow areas, by how far the Light's box reaches into it, computed from the manifest. No test covers it.
  *2026-10-02, E33: with the Light's box starting 296 from the edge instead of 424 and its top 406 above the bottom edge (E32: 321 + 120 = 441), the overlap is gone on every 4:3 screen, the one with a notch's two 88-unit cut-outs included; it remains only as the Restore tile over an 8 x 96 strip of the Light's box on the modelled 5:4 screen with a notch. The table above is E32's.*
- The sword combo's y is 19 units from the picture's (above).
- The size ceiling of 1.0 is the notched 4:3 screen's, a 4:3 screen with a phone's 88-unit cut-outs on both sides, which no device is known to have. Leaving that one shape out, the
  sets' ceilings would read 1.2 (the unzoomed screens, the nine bars, the 48 px bar, and the window-pixel reading with the 100 px bar) and 1.1 (the 100 px bar in E1's pixels).
