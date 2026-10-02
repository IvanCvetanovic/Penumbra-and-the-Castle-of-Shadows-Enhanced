# E28: adjustable touch controls

Built 2026-10-01, on the touch controls of E16, E25 and E26. The reference is the screen Magic Rampage 7.8.7 has for its own on-screen pad
([`../spec/42-magic-rampage-screenpad.md`](../spec/42-magic-rampage-screenpad.md)); this page is what was built, where it differs from it, and what is
still open. The player's view is in [`../enhancements.md`](../enhancements.md) and [`../controls.md`](../controls.md).

## What it is

On a touch-enabled options screen an **Adjust controls** button, under the touch controls' switch (a cell beside it on a phone's large layout, E31), opens a full-screen editor that shows the real
controls at their real size, opacity and place on a dark backdrop:

- **Size tiles** (shrink, enlarge): one global size for every control but Pause and Back, 0.4 to 1.4, a tenth a tap, default 1.0. Pause and Back keep
  their size because the pause column feeds the message room, the zoom's limit and the phone menu's panel.
- **Opacity tiles** (dim, brighten): a multiplier of the manifest's own idle alpha (0.45), 0.2 to 1.8, a fifth a tap, default 1.0.
- **Padlock**: the editor opens locked. Unlock it and drag any control; a size step or a restore locks it again. The lock is never saved.
- **Circular arrow** (Restore): puts every control back where the manifest has it, and keeps size and opacity. Nothing is asked first.
- **Back arrow** at the top left: leaves. Esc leaves too.

The result is `touchTuning` in `settings.json` and applies from the next tick wherever the touch controls are drawn and touched: levels and
arenas, the menus' corner button, and the pause preview. The default tuning is the manifest itself, bit for bit: `WithTuning` returns the base
manifest when nothing is set, and the level captures at the default tuning are byte-identical to those taken before the change (the Numbers part of
the DEVLOG entry has the checksums). Without the touch controls nothing changes.

### The entry

A button at (255, 222), 25 px high, x 255 to 530, in the band the options screen leaves free: the touch switch's two rows end at y 220, E27's first
rule is at y 248, and the refresh rate's hint starts at x 540. It is drawn only while the touch controls are on (`g_touchControls.getCurrent() == 0`),
as an `arrow_right` icon and "Adjust controls" in Arial Narrow 25 (without the options art, `[>] ` and the label), alpha 200, 255 under the cursor.
A confirm with the cursor strictly inside raises `Script::g_adjustTouchControls`; the layer reads and lowers it at the end of that tick and opens
the editor. The finger that tapped is dead until it lifts: it cannot press a tile or grab a control.

On the phone's large options layout (E31, [`2026-10-01-e31-phone-options-screen.md`](2026-10-01-e31-phone-options-screen.md)) the button is a cell of the third row's right
column, (522, 314) with 474 x 88 px on a 1024x768 window (frame 10/8/10/0), with the same label and condition; the (255, 222) box above is the art-less
layout's. The editor's Back arrow (64 px, 41 in from the shown area's top-left corner) and the options screen's own Back arrow (123 x 92 px, at the
frame's corner plus 8) share that corner; the four leak guards below keep a tap on the one from reaching the other.

### The editor's screen

The options scene is laid out as the menus are (1024x768 logical, unit 1), and in a window wider than 4:3 the shown rectangle `areaMin..areaMax` is the
screen. The centred widgets sit at the same x on every screen; the back arrow and the title follow the area's top-left corner and its safe area.

- Backdrop: an opaque vertical gradient, ARGB (30,26,44) to (14,12,22), over the whole shown area. It is opaque because a translucent one let the
  options text show through under controls drawn as faint as a fifth.
- Controls: `TouchControls::AppendOverlay`, at the player's opacity, every control but Back (Back, in arena select and on the end screens, never moves:
  its left edge feeds the phone menu's panel). The exit-down button is shown even though it is only offered at the exit door.
- Widgets, 96 px tiles, from the area's top: the size row's centre 168 below it, the opacity row's 276, the lock row's 392; the pair at the centre x minus
  and plus 112, the lock and restore at minus and plus 56. The back arrow is 64 px at (41, 41) from the corner; the title (Arial Narrow 40) starts
  20 px right of it and ends at the area's right edge less the larger of the safe inset and the HUD frame's inset, less 140 (the Pause's column). Each row
  reads `- 1.0 +` with one decimal always; the number pulses to 1.2x (grew) or 0.8x (shrank) for six ticks.
- Unlocked: the lock tile is red, and each movable control has a thin white outline (2 px, alpha breathing 110 plus or minus 70 over 48 ticks, 3 px solid
  while held) around the area a finger can take: its box, or for the direction control the strip its two buttons occupy.
- A tile at its limit is drawn at alpha 90 and ignores taps; Restore is at alpha 70 while nothing is moved; a held tile is drawn inset 3 px.

The tile art is Magic Rampage's own symbols on its stone frames, composed by `tools/art/make_options_art.py` into
`game/data/images/options/edit_*.png` (96x96; `LICENSE.md` and that folder's README say which file is which). They are drawn by path, not through
`kOptionsArt` in `game/script/Script.hpp`: a missing one is drawn as a plain square.

### The gestures

- A tile acts on **release** with the finger still strictly inside it (a completed tap); one that slides off cancels. Widgets win over controls.
- Grab (unlocked only): a finger that lands on no widget takes the visible movable control whose grab area holds it (the nearer centre if two; one
  another finger holds is skipped). The control keeps its offset from the finger, wherever the layout lets it go; two fingers may move two controls.
  While locked a finger on a control does nothing.
- A control let go with its whole grab area under a tile could never be taken again, so it goes back to where it was when the finger took it.
- A change is applied at once, so a drag shows in the same frame; it is stored in the settings and saved once per gesture end (a lift, a tile, close),
  never per drag tick, and only if the tuning differs from what was last saved.

## Data model

`TouchTuning` (`game/render/TouchTuning.hpp`, standalone: `<array>` and `<string>` only, because `Settings.hpp` must stay free of `windows.h`'s
`DrawText` macro): `size`, `opacity`, and `move[10]`, one `TouchMove {x, y}` per `TouchControl` in its order (dpad, jump, sword, fire, light,
swordCombo, spellCombo, exitDown, pause, back).

- **Grids.** Held as whole steps and turned back with one division, so a chain of steps ends exactly on 0.4 and 1.4 (or 0.2 and 1.8) and stops, with no
  drift. `Clamped()`: size 0.4..1.4 by 0.1, opacity 0.2..1.8 by 0.2, each move component to plus or minus 2048 by 0.1, NaN to the default, `move[back]`
  forced to zero.
- **Units.** A move is in the **manifest's pixels**, in the **screen's direction** (+x right, +y down) whatever the control's anchor, from where the
  manifest puts the control. `WithTuning` turns it into the anchor-relative offset (`left ? +dx : -dx`, `top ? +dy : -dy`). Moves are scaled by
  `manifest.scale` and the unit like every offset, so they survive a window change or a rotation; they are not scaled by the size.
- **`TouchControls::WithTuning(base, tuning)`** returns the base itself for the default tuning. Otherwise: size multiplies each non-exempt control's
  size, offset, overhang and hit padding, and the knob; moves are added to the offset; the exit-down button also follows the direction control's move
  (it sits over its gap); a moved top-anchored Pause is held at least `kPauseTopClear` (the timer's row, 25, plus 4) below the HUD frame's top; idle
  alpha is the manifest's times the opacity, and the held alpha `max(manifest's held, idle + 0.15)`, both clamped to 1. The result is one derived
  manifest, so the layout, the hit tests, the knob and the art read one thing.
- **One placement.** `ComputeLayout` was split into `BoundsFor`, `PlaceControl` and `LayOut`, with every expression kept in its order (the existing
  layout suites pin it); `MoveFor` runs the same `PlaceControl` backwards to turn a wanted box corner into a move, so no clamp is written twice: the
  safe area, the overhang, the HUD frame for the Pause and E25's timer rule all apply to a drag exactly as to a layout.
- **`settings.json`:**
  `"touchTuning": { "size": 1.1, "opacity": 0.6, "move": { "jump": [-40, 12], "pause": [-30, 20] } }`, written after `edgeMargin`, always, on one line,
  with only the non-zero moves. `Settings::kVersion` stays 2 (nothing reads it). Reading is as tolerant as the other fields: an absent block is the
  default; a wrong field (not an object, a non-number, a move that is not two numbers, an unknown control, `back`) keeps its default with a warning;
  numbers are clamped as above. An older build that re-saves drops the block.

### Dev flags (none saved)

`--touch-tuning "size=1.2,opacity=0.6,jump=-40:30,dpad=12:0"` replaces the settings' tuning for the run (comma-separated `key=value`; `size` and `opacity`
numbers, a control id with `dx:dy`; the last of a repeated key wins; a bad item prints the usage and exits 1). `--touch-editor [locked|unlocked]` opens
the editor on the first tick the options scene is up and turns the touch controls on unless `--touch` says otherwise. `--finger
<id>:<x>,<y>@<from>-<to>[/<x2>,<y2>]` is a synthetic finger in logical pixels, down from tick `from` to `to` and moving in a line to (x2, y2), merged
into the touch contacts. Any of the three sets `Options::noSave`: the run's `SaveSettings` writes nothing, because a capture runs against the real
user directory and a synthetic drag must not rewrite a player's `settings.json`.

## The layer's glue (`PenumbraLayer`)

`ApplyTouch`'s first half became `BuildTouchInput()`, so that opening and closing the editor can lay the controls out again for the scene they are about
to be in. With the editor open, the touch scene is `TouchScene::Edit` (every control but Back shown, no finger presses anything, no pointer), the corner is
hidden, and the HUD frame is that of the shown rectangle's own shape (the layer's own frame is zero outside a level, and a wide menu's logical width is 1024).
`TouchControls::Down()` hands the editor the fingers the last `Update` used, a latched tap included. A change calls `SetTuning`, which recomputes the layout
from the stored geometry at once; a commit stores the tuning in the settings and saves; a close re-lays the controls out for the menu.

While the editor is open the Machine does not tick (as under the pause), the blend alpha is held, the cursor shows, and the editor's overlay replaces the
controls' and the pause's. A window resize still works; the options screen's own keys, Alt+Enter among them, are the script's and stand still with it.

**The four leak guards.** The Esc or the click that closes the editor, or a finger down at that moment, must not reach the options screen, whose
`waitForInputToMenu` would drop the player into the main menu. Every one of these exists, and removing any one lets it through:

1. The Machine's frame is skipped while the editor is open (read after `ApplyTouch`, so the closing tick does run it).
2. In the Edit scene `step.touching` is true and `pointer` false, so `ApplyToFrame` forces `K_LMOUSE` false (the desktop's mouse as the finger too).
3. Every new finger in the Edit scene has no owner, and one that had an owner loses it: it presses nothing now and clicks nothing in the scene that follows.
4. `PauseMenu::HoldPressed()` arms `FilterForGame` on close, so whatever is down that the game had not seen stays masked until it is released.

Also: `HudCmd::stretchToSides` (default true, the scripts' behaviour) is false for the editor's outlines and for `TouchControls`' plain squares, because
`HudRenderer` stretches any rectangle that meets the logical screen's edge out to what is shown whenever the view shows past the sides, which turned a thin
outline on a notched phone into a band. And `Script::g_adjustTouchControls`, like every script global, outlives a Machine, so the layer lowers it at attach.

## Where it differs from Magic Rampage, and why

| Question | Magic Rampage 7.8.7 | E28 |
|---|---|---|
| Entry | Options, Controls, only with touch; not from the pause | A button under the touch switch on the options screen; not in the pause. Copied |
| Size | 0.4 to 1.4, a tenth a tap, one global scale | The same range and step; the Pause is not scaled with it (MR scales it by 0.9) |
| Transparency | A multiplier, 0.2 to 1.8, a fifth a tap, on a base alpha of 0.25 that also falls as the pad grows | The same range and step on Penumbra's base of 0.45; no extra dimming of a bigger pad |
| Per-button size, a selected button, a move button, a restore confirmation | None | None: unlock and drag; Restore asks nothing; copied |
| Lock | Opens locked, red frame and wiggling buttons when unlocked, a size step or restore re-locks | The same, with a breathing outline for the wiggle (a draw command has no rotation) |
| Overlaps | Pads are pushed apart (128 px/s) and kept out of two forbidden HUD zones | **Nothing is pushed apart.** See the size ceiling below |
| Screen limits | None: a button can be dragged until its centre is off the screen | Controls stay inside the safe area (the layout's own clamp; the direction control keeps its existing overhang); a lost button is a bug |
| Stored as | `screenPadScale`, `screenPadTransparency` and one `x,y` per button, in absolute screen pixels | `touchTuning` in `settings.json`, moves in manifest pixels relative to each control's anchor, so they follow a rotation or another window size |
| A move while a second finger is down | The deltas of every finger add up | One finger per control; held at the limit rather than wound up past it |
| Sounds | Click, grab at speed 0.8, drop at speed 1.2, lock-tile sounds | None: the editor is pure (no audio path), and neither the options rows nor the pause have sounds |
| Words | Only the title and the button's label | Two strings in eleven languages, in this edition's own wording (`Ajustar controles`, `Ajustar controles de toque`) |
| Pressed look | An opaque white flash (likely) | The held alpha is the larger of the manifest's and idle plus 0.15, so a held cue is visible at every opacity |
| Leaving | The back arrow | The back arrow and Esc |

### The size ceiling

Size goes to 1.4 because that is Magic Rampage's own ceiling, but Magic Rampage pushes overlapping buttons apart and Penumbra does not, so above a
measured size the shipped layout draws controls over each other and the player moves them apart (the editor is a live preview of exactly that).
`testSizeCeiling` measures it in play: with E26's frame, E25's zoom at its automatic value, a notch, a gesture bar and a bottom bar, and the Pause
included in the overlap checks. The largest size with no drawn overlap on any case is **1.1** on the nine standard cases (a 20:9 phone, a 16:9 and a 4:3
tablet, each bare, with a notch's 88 px sides and 24 px bottom, and with a tablet's 30 px top and 20 px bottom), **1.0** when a 100 px bottom bar is added
(at 1.1 the spell-combo button meets the Pause on all three shapes), and **1.2** on the unzoomed screens the layout was always checked on. Above the
pinned ceiling the spell-combo button grows into the Pause button on every screen. Every size the editor offers keeps every control, and the Pause,
inside the safe area; only overlap is left to the player. A cap that depends on the screen's shape was not built: a size saved on a phone would have to
be re-clamped when the window changes shape.

These figures are E28's, for the arrangement E16 gave the buttons. E29 put the action buttons in two columns of three, and the same measurement then gives **1.2** in
every in-play case, the spell combo button no longer reaching the Pause. Above 1.2 the direction control's right button meets the sword button on a 4:3 screen
with an 88 px notch, and the fire button meets the Pause under a 100 px bottom bar; the editor's top size, `kMaxSize`, stays 1.4.
E32 spread the columns (67 apart, in from the edge) and levelled the rows, and the measurement then gives **1.0** in every in-play case: from 1.1 the sword button
meets the direction control's right button on that notched 4:3 screen, and from 1.2 the fire button meets the Pause under the 100 px bar, counted in the
768-tall pixels the layout is written in (E1's; from 1.3 counted in a window's pixels) ([`2026-10-01-e32-touch-layout-grid.md`](2026-10-01-e32-touch-layout-grid.md)).
E33 brought the columns to 16 apart and 40 from the edge and staggered them (the right one 30 higher), and the measurement then gives **1.1** in every in-play case: from 1.2 the sword button meets the direction control's right button on that notched 4:3 screen, and from 1.2 the fire button meets the Pause under the 100 px bar in E1's pixels (from 1.3 in a window's pixels) ([`2026-10-02-e33-touch-grid-staggered.md`](2026-10-02-e33-touch-grid-staggered.md)).

## Tests

`test_pn_all` on Linux: 17 suites, 28,556 checks, 0 failures, 0 warnings (22,109 before). New code is in the existing suites (a new `tests/test_pn_*.cpp`
would be an 18th suite, and 17 is documented).

- `test_pn_render_touch` (6,936): the default tuning equals the manifest bit for bit (the built-in, the shipped and the placeholder look, on three
  screens and three insets); every control's box at sizes 0.4, 0.5, 1.2 and 1.4; opacity alphas; moves by anchor and their floors; `MoveFor` round trips
  (11,232 of them, over every control, three screen shapes, three insets, two window scales, with and without the HUD frame, the wide menu's area, four
  sizes (0.4, 1.0, 1.2, 1.4) and eight targets each: the layout puts the box where it was asked, nothing else moves, the result is always inside the
  safe area, and asking again gives the same move); `GrabBox` against the opaque pixels of the direction control's art; the Edit scene (nothing presses anything, fingers stay
  dead into the next scene, `SetTuning` shows at once, `Update` twice in a tick changes nothing); the size ceilings above; the control names.
- `test_pn_render_input` (2,438): the settings' round trip and its JSON text, every tolerance, the steps' limits, `--touch-tuning`'s grammar.
- `test_pn_render_pause` (4,083): the editor's layout on 4:3, a 20:9 area and a notch; open, drag, locked, steps, restore and close; the overlay's order
  and counts; the pulse and the breathing; the art; the words through `HudRenderer` in every language (the title fits its room and clears the Pause and
  the tiles; an outline stays thin on a wide menu); `HoldPressed`.
- `test_pn_render_hud` (11,440): the two strings have English and a line in all nine language files, and each a room in `tests/data/l10n_rooms.json`;
  the real `PenumbraLayer` driven on a bare registry (the entry opens the editor, the Machine stands still, a synthetic drag is kept, Esc closes it
  without the options screen reading it, a finger down at that moment clicks nothing).
- `test_pn_scenarios` (1,085): scenario 24, the button's drawing and its click.

## Captures

Headless, Linux/lavapipe, `--touch on --mobile-layout on --lang en --fixed-step`, each in a throwaway user directory; none is committed.

| Name | Window, frames | Extra flags | Shows |
|---|---|---|---|
| c1 | 1280x720, 1024x768, and 1280x720 in Portuguese; 8 | `--start videoModes` | the entry button under the touch switch |
| c2 | 1024x768 and 2400x1080; 12 | `--start videoModes --touch-editor` | the locked editor at the default tuning |
| c3 | 1024x768 and 2400x1080; 12 | `--touch-editor unlocked --touch-tuning "size=1.2,opacity=0.6,jump=-40:30,pause=-30:20,dpad=12:0"` | the unlocked editor, tuned (the picture in `controls.md`) |
| c4 | 2400x1080; 12 | `--touch-editor unlocked --safe-area 132,0,132,48 --edge-margin 3.5` | a notch: every control inside the insets, the Pause at the HUD frame's corner |
| c5 | 1024x768; 24 | `--touch-editor unlocked --finger 1:812,684@6-12/772,696` | a real drag through the layer |
| c6 | 1280x720; 12 and 30 | `--finger 1:400,235@8-10` | the entry tap (it also lands on a tile, which proves the entry finger is dead) and the freeze: the two captures are identical |
| c7 | 2400x1080; 120 | `--start level1`, with and without `--touch-tuning` | the tuning in play |
| c10 | 1024x768; 12 | `--touch-editor --lang de`, `ja`, `ar`, `ru` | the title on one line, clear of the Pause |
| c11 | 2400x1080; 12 and 120 | `--touch-tuning "size=1.4"`, `"size=0.4,opacity=0.2"` | the limits, in the editor and in play |

The default captures (`--start level1` at 2400x1080 and 1024x768, 120 frames) are byte-identical to the ones taken at the previous commit.
Under E29's arrangement c3's `jump=-40:30` would put the jump button over the sword button (E32's grid keeps them 67 apart, and the moved jump button clears the sword by 27 at size 1.0); an example for the two columns is `--touch-tuning "size=1.2,opacity=0.6,light=-60:-20,pause=-30:20,dpad=12:0"`.
A capture of a finger at a moved jump button's new centre and at its old one (`--start level1 --zoom 100 --touch-tuning "jump=-40:30" --finger 1:772,714@40-44`, and `1:812,684` for the old centre) came out identical at frame 60, so it shows nothing: that the hit areas follow the drawn ones is pinned by `test_pn_render_touch` (a button at size 0.5 is pressed where it is drawn and not where it was, and one at size 1.4 is pressed beyond its old edge).

## Open

- The translations of the two strings are drafts in the vocabulary the existing touch strings use; those in Arabic, Japanese, Turkish and Ukrainian
  have not been read by a native speaker. Japanese uses only glyphs already in the bundled Noto subset.
- Android's system Back closed the editor on the emulator; a real phone has not been tried. The on-screen arrow is the way out regardless.
- A pad's Back or B cannot close the editor: with the touch controls on, no pad is player 1 (E22), so only the arrow and Esc close it.
- Above the size ceiling the player moves overlapping controls apart by hand; nothing pushes them (see above).
- The headless checks use `--finger`'s synthetic fingers and the layer-driven suite check. Real touch input was tried once, on the Android emulator (tap, swipe, Back, the saved
  file, a relaunch); a real touchscreen has not been.
- Not built, each a small addition: sounds; per-control size; a restore confirmation; a flash of the lock tile on a drag while locked; an entry in the
  pause menu; Magic Rampage's push-apart and forbidden zones; a framed title.
