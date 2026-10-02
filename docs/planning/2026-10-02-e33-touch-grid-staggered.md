# E33 and E34: staggered columns of action buttons, and a saved layout that follows the default

Built 2026-10-02, on E32's grid of action buttons and E28's editor. E33 is the new default arrangement of the six action buttons; E34 is the rule that
keeps a player's saved moves from being laid on an arrangement they were not made for, which this change would otherwise have done. Both are on this
page (one page, since the reset exists because of the arrangement). The player's view is in [`../enhancements.md`](../enhancements.md) and
[`../controls.md`](../controls.md#touch-controls-e16); the numbers are in `game/data/touch_controls.json` and `TouchControls::DefaultManifest()`
(`game/render/TouchControls.cpp`), which must be equal (`testManifest`), and the layout number is `TouchTuning::kLayoutVersion`
(`game/render/TouchTuning.hpp`). An intermediate build of this change had the left column the higher and layout number 3; it was never in a release, and
this page describes the final arrangement, layout number 4.

## What changed, and why

E32's grid put the cluster far in from the right edge with a wide gap between neighbours: the left column's far edge was 424 units from the screen's edge
(25% of a 20:9 phone's 1707-unit width, 31% of a 16:9 screen's 1366) and the cluster was 307 wide. The columns were level. E33 keeps the two columns of three and
the order of the controls and changes where they are:

| Control | E32 [x, y] | E33 [x, y] |
|---|---|---|
| jump | 117, 49 | 40, 64 |
| sword | 304, 49 | 176, 34 |
| fire | 117, 321 | 40, 316 |
| light | 304, 321 | 176, 286 |
| sword combo | 314, 195 | 186, 170 |
| spell combo | 127, 195 | 50, 200 |

Offsets are in units, inward from the bottom-right safe corner (anchor `bottomRight`); the sizes (120 and 100) and the hit paddings (4 and 6) are E29's, and
the direction control, the down button and the pause button do not move. The arrangement:

```
 left column R 176..296 (30 lower)        right column R 40..160 (the higher)
 light        y 286..406                  fire         y 316..436
 sword combo  y 170..270  (R 186..286)    spell combo  y 200..300  (R 50..150)
 sword        y  34..154                  jump         y  64..184
```

- 16 between every pair of neighbours: the columns (160 to 176), and in each column a combo and the button under it and over it (E32: 67 and 26). A combo (100)
  is centred over its column (120), so its x offset is its column's plus 10, and a column's rows are 126 apart centre to centre.
- The right column is 40 in from the edge, the left 176 (E32: 117 and 304); the cluster is 256 wide (E32: 307) and its far edge 296 in (E32: 424; E29: 280): 17% of a 20:9
  phone's width and 22% of a 16:9 screen's.
- The columns are staggered, the right column, the one nearest the edge, 30 higher than the left at every row (jump 64 against sword 34, combos 200 against 170, fire 316 against
  light 286). That is the direction of E29's stagger (the right column 30 higher, which E29 chose as the arc of a thumb pivoting at the corner), with the columns now 16 apart and 40 from the
  edge; E32 had none. This stagger is not derived from a model or a measurement: its amount and its direction are a layout decision, and untried on a real phone (open, below).
- The cluster's top is 436 up on the right (the fire, the highest of the six) and 406 on the left (the light; E32: 441). The mean of the bottom row's offsets is 49, as in E32 (64 and 34),
  so the direction control's two buttons, whose centres are 83.7 above the edge, stay 25 under the middle of the bottom row: 10.3 under the sword button's centre (94) and 40.3 under
  the jump button's (124).

## What the suites reported first

The first run after changing only `touch_controls.json`, `DefaultManifest()` and `kLayoutVersion`, before any test was touched: `test_pn_render_touch` 7,617 checks and 145 failures,
`test_pn_render_pause` 4,088 and 7, `test_pn_render_hud` 13,112 and 0, `test_pn_render_input` 10,307 and 2. Every failure is a literal that restated the direction of the stagger or the
numbers behind it, or a pin; none is one of the geometric rules:

- `CheckColumns`: the stagger relation (54: three checks, one per row, on each of the 18 screens and safe areas) and "the light is the top" (18).
- `testDpadLowered`: the bottom row's 30 and 109 (9: the sign of the 30).
- `testTuningIdentity`: the pin of `kLayoutVersion` (1) and the table (6).
- `testButtonColumns`' boxes on 4:3, 16:9 and 20:9 (36) and on the notched 16:9 (12).
- `testSizeCeiling`: the lists of overlapping pairs at 1.2, 1.3 and 1.4 (7) and the two places where the fire button's clearance of the pause is used up (2). The five ceilings, still 1.1, and the
  clearance at 1.0 (at least 70) held.
- `test_pn_render_pause`: the editor's control centres (7).
- `test_pn_render_input`: the one literal layout number kept, `3.0` (1), and the list of layout numbers that lose their moves, which held the new number, 4 (1).

Not one of `DrawnOver`, `ReachBoth`, the half-screen rule, the 39% rule, the down button's rules, the inside-the-safe-area rule or the pause clearance failed, so no number of the
arrangement was changed to satisfy a rule.

## The rules, and what moved

- **Kept as they were**: `DrawnOver` (strict, unchanged), the inside-the-safe-area rule, the pause clearance (150, measured on both the fire, which is the cluster's top and under the
  pause button, and the light), `kMaxSize` 1.4.
- **E32's proxy-rule restatements, kept** (the rules read on the direction control's two drawn buttons instead of its 400-unit box, the half-screen rule as "at or past the middle and
  right of the direction control's right button", `ReachBoth` between the direction control and the rest as "neither drawn button overlaps the padded box", the 39% line for the
  shipped look). They are no longer needed by this arrangement: with the original rules put back in a scratch copy of the suite (the disc's own reach through `ReachBoth`, the strict half-screen
  line, 40%) `test_pn_render_touch` still passes (7,491 checks, 0 failures). On the notched 4:3 screen the left column's box now starts at 936 - 176 - 120 = 640 (E32: 512, exactly on the middle),
  138 units from the right button's face (E32: 10) and 324 from the disc's centre where its reach is 230; the top of the cluster stands 40.1% down a screen with a 24 px bottom bar (308 of 768) and 40.6% with 20 px. The comments say so.
- **`CheckColumns`, as relations**: the right column's edge 24 to 60 from the safe area's (not hugging it, not far in), the columns' edges equal; the right column exactly 30 higher at each of the three
  rows (a literal, `kStagger`); 16 between neighbours, exactly (`kGap`, columns and rows, not `>=`); rows evenly spaced and each combo halfway between its buttons and centred over its column; the order
  from the bottom and the columns not interleaved; the fire the top and 150 or more clear of the pause, the light too. The absolute numbers are pinned in the same places as E32: the table of `testTuningIdentity`
  (and, beside it, `kLayoutVersion` = 4), the boxes of `testButtonColumns` (4:3, 16:9, 20:9 and a notched 16:9) and the editor's centres in `test_pn_render_pause`.
- **`testDpadLowered`**: the bottom row is two centres now (the sword's 94 and the jump's 124 above the edge); the direction control's buttons are 25 under their mean (109).
- **`testButtonColumns`**: the gap points are rebuilt for the stagger (the strip between the columns is only the stretch the two columns share, from the sword's top to the jump's bottom, 8 units free of padding; between the combos 24; between rows
  6 once a combo's 6 and a button's 4 of padding are taken off), and the notched 4:3 case is kept for what it checks (a finger just inside the sword's padding is the sword's alone, one on the right button's face is Right alone) though the two are no longer within reach of each other.

## The ceilings and clearances, measured

Printed by `testSizeCeiling` and pinned there. The largest size from which every smaller one is clean, on every set: **1.1** (E32: 1.0; E29: 1.2). One step up, 1.2, the notched pair overlaps in every set:
the direction control's right button and the sword button on the notched 4:3 screen (88/0/88/24; the face's right edge is at 88 + 414.15 s and the sword's box starts at 936 - 296 s, so they meet above s = 848 / 710.15
= 1.194, and on a plain 4:3 screen above 1024 / 710.15 = 1.442, which the editor does not offer); and the in-play set in E1's 768-tall pixels adds the fire button against the pause button on a 20:9 phone with a 100 px bottom bar (the fire is the top of the right column, and the pause
button hangs over that column). At 1.3 the fire button against the pause button joins on a 20:9 phone with a 48 px bar, and on the 16:9 and 4:3 tablets with a 100 px bar, in E1's pixels, and in window pixels on the phone with the 100 px bar; at 1.4 it
overlaps on most other shapes with a bar, a notch or a tablet's status bar (a 30 px top inset pushes the pause button down), unzoomed screens included. No other pair overlaps up to 1.4 (E32 also had the down button against the sword button and the sword combo).

The fire button's least clearance of the pause button, in 768ths of the screen, over the in-play cases (the fire is the top of the cluster and under the pause button; the light is not under it, so its row is vertical only):

| Size | 1.0 | 1.1 | 1.2 | 1.3 | 1.4 |
|---|---|---|---|---|---|
| fire, E1's 768 bars | 77.9 | 34.3 | -9.3 | -52.9 | -96.5 |
| fire, window pixels | 106.8 | 63.2 | 19.6 | -24.0 | -67.6 |
| light, E1's 768 bars (vertical only: it is not under the pause) | 107.9 | 67.3 | 26.7 | -13.9 | -54.5 |
| light, window pixels | 136.8 | 96.2 | 55.6 | 15.0 | -25.6 |

(The spell combo's at 1.4: 93.9 and 122.8. E32: the fire's 72.9 and 101.8 at 1.0.) Pinned: at least 70 at 1.0 for both buttons in both readings; the fire's clearance used up between 1.1 and 1.2 (E1's pixels) and between 1.2 and 1.3 (window pixels).

Without the notched 4:3 shape (a 4:3 screen with 88-unit cut-outs on both sides, which no device is known to have), the ceilings would read: unzoomed 1.3, the nine bars 1.3, the 48 px bar 1.2 (E1's pixels) and 1.3 (window pixels),
the 100 px bar 1.1 (E1's) and 1.2 (window pixels); printed by the suite, not pinned.

## The editor's tiles over the Light button

E32 recorded that the Restore and brighten tiles of the editor lay over the Light button on 4:3 screens: the tiles are centred on the shown area, and a widget wins a hit test over the control behind it. With the Light's
box now starting 296 from the safe area's right edge (E32: 424) and its top 406 above the bottom edge (E32: 441) the overlap is gone on every 4:3 screen and remains only where a notch narrows a 5:4 one. The tile arithmetic from `TouchEditor.hpp`'s constants (rows 168, 276 and 392 below the safe area's top, tiles 96 across, Lock
and Restore at the area's centre -56 and +56, the size and opacity tiles at -112 and +112) reproduces every row of E32's table for E32's boxes, and gives, for E33's, the tile over a control, width by height in units:

| Shape (insets left/top/right/bottom) | Tile over a control | Basis |
|---|---|---|
| 4:3, 1024x768, none | none | captured |
| 4:3, 0/0/0/24 | none | captured |
| 4:3, 88/0/88/24 (the notched shape) | none | captured |
| 4:3, 0/0/0/20; 0/30/0/20 | none | modelled |
| 5:4, 960 wide, 88/0/88/24 | Restore over the Light 8 x 96 | modelled |
| 3:2, 1152 wide, 0/30/0/20; 16:10, 1229 wide, 88/0/88/24 | none | modelled |
| 16:9 and 20:9, every inset | none | modelled (the 2400x1080 render looked at) |

The Light's top is 406 above the safe area's bottom edge, at y = 362 - b of the 768-tall screen when that edge is b above the screen's, so the brighten-opacity tile (its bottom at y = 324 + t, with t the safe area's top inset) is clear of it vertically whenever b + t is at most 38, whatever the width; the Restore tile
(to the area's centre + 104) clears the Light from a screen width of 800 + 2 r (a safe-area width of 800) with equal side insets r, and the brighten-opacity tile, where b + t is more, from 912 + 2 r. "Captured" is a render of the editor with the Linux headless build at 1024x768 (`--start videoModes --touch on
--mobile-layout on --touch-editor unlocked`, with `--safe-area` where given): the six outlines sit on the manifest's boxes edge for edge (left and top edges exact, right and bottom one pixel inside the max edge) on the plain, the 24 px bar and the notched capture; on the notched one the Light's outline starts at x 640, y 338, 14 units below the brighten tile (576..672 by 228..324, its art ending at y 324) and 24 units right of the Restore tile (520..616). The remaining overlap is the empty margin of the Light's box, not its face (the face
starts about 10 units in), but a finger on it is the tile's. No test covers it. A fix, not done: shift the Lock and Restore pair and the right tiles left on narrow areas by how far the Light's box reaches into them.

## E34: the saved layout follows the default

A move saved in the editor is a delta from the default's place for that control (`TouchTuning::move`), applied by `TouchControls::WithTuning`. E29, E32 and this change each moved the default, and a tuning saved under the one before was
laid on the next: the moves stacked on the new places (a very wide, staggered cluster) until Restore. E29's and E32's notes state that nothing is migrated. E34 drops them instead, once per change of the arrangement.

- **The format**: `settings.json`'s `touchTuning` object has an integer `layout` first: `{ "layout": 4, "size": 1.1, "opacity": 0.6, "move": { ... } }`. `TouchTuning::kLayoutVersion` names the arrangement of the shipped default:
  **1 is E29's, 2 is E32's, 3 is an intermediate build of E33 (the left column the higher; never in a release) and 4 is E33's**. Whoever changes any default offset (the six action buttons, the direction control, the down button, the pause button) bumps it by one; the comment on the constant says so, and
  `testTuningIdentity` pins the value (`CHECK_EQ(kLayoutVersion, 4)`) directly above the table of the default's places, so changing the table means touching the line that says to bump it. Nothing enforces the bump:
  a table edit that leaves the version alone still passes the version check.
- **Reading** (`TouchTuning::ReadFrom`): `layout` is looked up before anything else, whatever the order of the keys, and compared as the double it is to the constant (so `4.0` is 4, `4.5`, `1e40` and `-1e300` are not, and nothing is narrowed to an int). Equal: the block is read as before.
  Missing or different (3 included): the size and the opacity are read, the `move` object is not read at all (no warning, since a missing or older number is an expected migration, and no complaint about its contents), and the moves stay the caller's default, none. A `layout` that
  is not a number (`"4"`, `true`, `null`, an array or object) warns (`touchTuning.layout is not a number`) and counts as different. The struct does not hold the number (it would have reached `Clamped`, `operator==`, `IsDefault`, `WithTuning`'s identity shortcut and `ParseFlag`):
  it is written by `ToJson` and read by `ReadFrom` only.
- **Writing**: `ToJson` always writes the current number, so the next save of settings after loading an older file writes `{ "layout": 4, "size": 1.1, "opacity": 0.6, "move": {} }`; the old file on disk is not rewritten until then. A button moved afterwards is saved with the current number and read back. The
  developer flags (`--touch-tuning`, `--touch-editor`, `--finger`) write nothing, as before; Restore still clears the moves and keeps the size and the opacity.
- **Nothing is shown**: a player whose moves were dropped is not told. The cluster is in the default places.

Tests (`test_pn_render_input`, `testSettingsTouchTuning`, 10,224 to 10,316 checks): the existing literals of the written block carry the field (built from the constant); `read` puts the current layout into a block's object text so that the tests of the move
parser still exercise it, `readRaw` is the block as written; a block with the current layout keeps size, opacity and both moves (also written 4.0, built from the constant, and with the layout after the moves); one with no layout (the format of E28 to E32), with 0, 1, 2, 3, 5, 99, -3, 4.5, 3.9999999, 1e40 and -1e300, with 3
written as an integer and as 3.0 (the intermediate build's number), and with the layout after the moves and another number, keeps size and opacity and loses the moves, without a warning; a non-number layout does the same with the warning; what is dropped is not read (`{"layout": 3, "move": 5}` and a block with a bad move and no layout draw no warning) while a wrong size still warns; a whole settings text in the E28 to E32
format reads equal to the same settings with no moves, saves as the current text, and through a real file (`Load`, `Save`, `Load`) is a fixed point, after which a newly moved button survives a save and a load.

## Mutation checks

Each mutation applied to `touch_controls.json` and `DefaultManifest()` together (only the offsets named), the four suites built and run, then reverted and proved reverted with `cmp` against copies of the post-change files:

| Mutation | touch | pause | hud | input |
|---|---|---|---|---|
| the stagger swapped (left column higher: only the y offsets exchanged) | 144 failures | 7 | 0 | 0 |
| the right column at 117 (jump, spell combo, fire; the left column untouched) | 306 | 5 | 0 | 0 |
| the whole cluster 77 further in (right column at 117, the gaps kept: only the edge changes) | 86 | 7 | 0 | 0 |
| the gap between the columns 67 (the left column 51 further out) | 49 | 3 | 0 | 0 |
| the gaps between rows 26 again (E32's, the stagger kept) | 95 | 4 | 0 | 0 |
| the layout drop turned off in `ReadFrom` | 0 | 0 | 0 | 30 |

Which relation caught each: the stagger swap `CheckColumns`' three stagger checks (18 each) and the "fire is the top" check (18); the cluster 77 in the right-edge relation (18); the columns' gap the gap relation (18); the rows' gaps the two column-gap relations (18 each) and the 39% line on the top row (12); the right column alone at 117 the right-edge, column-gap and columns-not-interleaved relations (18 each) and `DrawnOver` and `ReachBoth` (90 each, the two columns then overlapping). Of
the 30 failures of the last mutation 21 are the dropped-moves checks (every call of `drops`: no layout, the number 3 three ways, the other numbers and the values that are not numbers), 2 the "what is dropped is not read" ones (2 more follow from the warning those leave behind) and 5 the migration through the whole text and the file. The suites that do not read the layout stayed at 0.

## Checked

- `tools/build_linux.sh --test`: 17 suites, 39,542 checks, 0 failures (before E33: 39,290; `test_pn_render_touch` 7,457 to 7,617, `test_pn_render_input` 10,224 to 10,316, the others unchanged). `tools\check.bat` on `TouchControls.cpp`, `TouchTuning.cpp`, `Settings.cpp`,
  `HeaderCheck.cpp` and the three suites that changed: no warnings at /W4. The Windows suites were not run (Smart App Control).
- Renders with the Linux headless build: the editor at 1024x768 plain, with a 24 px bar and notched; level 1 at 1280x720 and the editor at 2400x1080 with size 1.2, opacity 0.6, the padlock open and three controls moved (the shipped screenshots, `docs/images/touch-controls.jpg` and
  `touch-editor.jpg`, are these two, retaken: nothing is clipped, the right column is the higher one). The README's and `controls.md`'s captions and alt texts describe them as before.
- The final arrangement, layout number 4, was run in the Android emulator (Android 13, x86_64, a 1344x2992 screen, a debug APK built from the source of this change, `--safe-area 199,0,0,0`): its six boxes measured at the manifest's places within 0.3 units and identical to the Linux render; a settings file from before the reset (no layout number, six old moves, opacity 1.6) and a file with layout 3 both showed the default arrangement with the opacity kept; the same moves under layout 4 were applied, and `--touch-tuning light=-30:-20` moved only the light; a drag in the real editor was saved as layout 4 with that move, and Restore emptied the moves; level 1 showed two columns with the right one higher. An earlier build of this change, with the left column the higher and layout number 3, was run in the emulator the same way before. Not exercised: a real phone, arm64, a release build, an update over a release-signed install, a layout-3 file with a non-default size, the Apple builds and a real right-hand cut-out.

## Open

- **Not tried on a real phone**: the arrangement has run in the Linux headless build and in the Android emulator (see Checked), not on a phone. The direction of the stagger and the 40 from the edge are untested by hand.
- **The right inset is assumed 0** where the arrangement meets a real display (E32's note): with a right cut-out of r units the cluster sits r units further in.
- **E32's proxy rules** (the drawn buttons for the direction control, `>=` for the middle, 39%) are not needed by this arrangement and can be put back to the originals, which pass.
- **The editor's tiles over the Light button** remain on a notched 5:4 screen (above); no test covers it.
- **The ceiling of 1.1** is the notched 4:3 screen's, which no device is known to have, and, in E1's pixels with a 100 px bottom bar, the fire button's against the pause button.
- **A dropped move is silent**, and the bump of `kLayoutVersion` is by convention.
