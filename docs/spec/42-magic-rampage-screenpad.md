# How Magic Rampage lets the player adjust its on-screen pad

Scope: read-only knowledge of one feature of **Magic Rampage 7.8.7** (Asantee Games, the maker of the original Penumbra): the screen that
changes the size, the transparency and the place of its on-screen buttons. It was decoded on 2026-10-01 from the Android package (the base APK,
`com.asanteegames.magicrampage.apk`, inside the APKPure `.xapk`) as the reference for the enhanced edition's own adjustment screen (E28,
[`../planning/2026-10-01-e28-adjustable-touch-controls.md`](../planning/2026-10-01-e28-adjustable-touch-controls.md)), which copies what is
confirmed here and says where it differs. Nothing in this file was observed running: Magic Rampage was only decoded, never launched.

Confidence tags: **confirmed** (read directly from bytecode, a layer file or a string table), **likely** (follows from confirmed facts but was not
observed), **guess** (no direct evidence). Every path below is inside the package.

## 0. How it was decoded

Magic Rampage's gameplay is compiled AngelScript 2.34.0 (the version string is in `libmachine.so`; the compiled modules are
`assets/game_64.bin` and `assets/game_32.bin`). There is no script source. The saved-bytecode format of 2.34.0 (`as_restore.cpp` in the public
AngelScript source, at its "Releasing 2.34.0" commit) was read by a small Python port, which parsed the whole of `game_64.bin` (5,203,909 of
5,203,909 bytes; 71 enums, 1,275 classes, 19,305 functions, 8,349 string constants). The module carries debug information, so function names,
parameter names, local variable names and source file names survive, and a second tool disassembled any function with its call targets, string
constants, float immediates and property names resolved. Both tools, and the dumps, live on the development machine only (`reference/` and `out/`
are not in the repository).

Where the evidence is:

- The layout of the popup: `assets/layers/ControlsOptions.layer` (plain data), plus the constructor of the class that builds it.
- The behaviour: the compiled functions of these source files, whose names the debug information keeps:
  `src/layer-elements/ControlsOptionsLayer.angelscript` (class `ControlsOptionsLayer`),
  `src/gameplay/character-system/controllers/CharacterScreenPadController.angelscript` (classes `character::CharacterScreenPadController`,
  `character::ScreenPadButton`, `character::ScreenPadButtonAlphaController`), `SEF/src/UserData.angelscript` and `SEF/src/UIButtonSwitch.angelscript`.
- The numbers: float immediates in the bytecode (`0x3dcccccd` is 0.1f, `0x3ecccccd` is 0.4f, `0x3fb33333` is 1.4f, `0x3e4ccccd` is 0.2f,
  `0x3fe66666` is 1.8f), compared and added at known places.
- The words: `assets/lang/<id>/ui.strings`.
- Not available: the engine's own C++ (rendering, text conversion of a float), so what depends only on it is marked as such.

The name "screen pad" is Magic Rampage's own: its string table says "the name we give to the buttons on screen (derived from gamepad)".

## 1. Entry and the popup

| Question | Answer | Evidence | Conf. |
|---|---|---|---|
| How is it reached? | Main menu, Options, the **Controls** button, which opens the popup `ControlsOptionsLayer`. The button exists only when `isUsingTouchScreen()` (Android or iOS and not console mode, or the shared-data flag `SD_FORCE_TOUCH_CONTROLS == "true"`). | `OptionsLayer::OptionsLayer` (the `controls` button of `assets/layers/Options.element`, gated by that test); `OptionsLayer::callTouchControlsSettingsLayer` builds the popup and logs the event `options_menu_touch_controls_open`; `isUsingTouchScreen` in `Version.angelscript` | confirmed |
| In a game, from the pause? | No. `OptionsLayer` is constructed only by the start screen and by the close handlers of its own child popups, and `assets/layers/GamePaused.layer` has only resume, restart and quit. | every caller of `OptionsLayer(const string&)` | confirmed |
| Back | `ControlsOptionsLayer::onPopupCloseBegin` re-opens the options list with `controls` pre-selected. | that function | confirmed |
| Style | `options = popup`, `darkenBg = 0.6` (the screen behind is darkened to 60%). `detectEscapeTouch()` returns false: a tap outside does not close it. | `assets/layers/ControlsOptions.layer`, that class | confirmed |
| Live preview | The constructor builds a real `CharacterScreenPadController(layer, addPause = true, addArcaneDummy = true, capacitive = true)` and inserts its six buttons as **background elements** of the popup, so the actual in-game pad is drawn at its real size, transparency and place behind the widgets. No input drives the pad: it is a preview. | the constructor, `buildButtons(true, true)`, `insertBackgroundElement` | confirmed |

### The rows, in order

Positions are fractions of the screen, from the layer file at its 1280x720 reference (1 px = 1/1280 of the width), and the constructor, which
does the left and right split itself. Whether the pixel constants are multiplied by a density factor on a high-density device was not decoded
(guess: they scale with the screen like the rest of the interface).

| # | Element | Position, size | Content | Conf. |
|---|---|---|---|---|
| 1 | `back` | (0.053, 0.053), top-left origin, scale 0.85 | `assets/sprites/inventory-back-button.png`: a dark rounded frame with a white left-pointing triangle; slides in from the left in 400 ms | confirmed |
| 2 | `title` | (0.5, 0.03), top-centre origin, a 400x100 frame | `frame-title-bg.png` (a dark pill, nine-sliced) holding the string `screenPadAdjustments`, font `Averia48.fnt`; slides in from the top in 600 ms | confirmed |
| 3 | `shrink` | (0.5, 0.27), moved left by 102 px | `smaller-frame-bg.png` frame, 128 px at scale 0.8 (102 px square), holding `resize-symbol-shrink.png`: a small white rounded square with an arrow pointing in from the upper right | confirmed |
| 4 | `sizeText` | centred at (0.5, 0.27), between 3 and 5 | text `- 1.0 +` in `Averia48`: the signs in `0x44ffffff` (about 27% white), the number in full white; `.0` is added only when the value is exactly 1.0. Pulses to 1.2x (grew) or 0.8x (shrank) for 100 ms, only if no animation is running. | confirmed |
| 5 | `enlarge` | (0.5, 0.27), moved right by 102 px | the same frame holding `resize-symbol-enlarge.png`: a large white rounded square, the arrow pointing out | confirmed |
| 6 | `dim` | (0.5, 0.40), moved left by 102 px | the frame holding `adjust-symbol-transparent.png`: a grey, see-through-looking rounded square with a white outline | confirmed |
| 7 | `transparencyText` | centred at (0.5, 0.40) | the same `- 1.0 +` format and pulse | confirmed |
| 8 | `brighten` | (0.5, 0.40), moved right by 102 px | the frame holding `adjust-symbol-opaque.png`: a near-white solid rounded square | confirmed |
| 9 | `lockSwitch` | (0.5, 0.57), moved **left** by 0.72 x 102 px (about 73 px), a 140x140 frame, centre origin | `smaller-frame-bg.png` with `lock-locked.png` while locked (the starting state), `lock-unlocked.png` and the red frame `smaller-frame-red-bg.png` while unlocked | confirmed |
| 10 | `refresh` | (0.5, 0.57), moved **right** by 0.72 x 102 px, a 140x140 frame | the frame holding `icon-restart-64.png` (a white circular arrow, scale 1.2). 27% alpha (`0x44ffffff`) when nothing is moved, full white when any button has a custom offset. | confirmed |

The screen has **no** text for size, transparency, lock, move or restore: rows 3 to 10 are icon-only, and the only words are the title and the
numbers. There is **no** move button: moving is "unlock, then drag a pad button". The 128 px `arrow-minus.png` and `arrow-plus.png` are in
`assets/sprites/` but this screen does not use them.

## 2. Size

| Question | Answer | Evidence | Conf. |
|---|---|---|---|
| One scale or one per button? | **One global scale** for all pad buttons. There is no selected-button concept: `m_lastGrabbedButton` is written when a button is grabbed and never read. | `ControlsOptionsLayer::update`; a scan of the class's fields | confirmed |
| Key, default | `screenPadScale`, a float stored as text; 1.0 | `getCurrentScale()` = `getFloatUserData("screenPadScale", 1.0)` | confirmed |
| Step | 0.1 per **tap**, not per frame | `SUBIf` / `ADDIf` with `0x3dcccccd` | confirmed |
| Minimum, maximum | **0.4** (shrink only while the value is above 0.4f) and **1.4** (enlarge only while below 1.4f): eleven values, and a float32 simulation, with or without a six-digit text round trip, lands exactly on 0.4 and 1.4 and stops | `CMPIf` with `0x3ecccccd` and `0x3fb33333` | confirmed |
| Applied to | `m_scale = screenPadScale * getPlatformScreenPadScale()`, the platform factor being 0.75 when `SD_BIG_TOUCHSCREEN_ENABLED == "true"` and 1.0 otherwise. `setScale(m_scale)` on left, right, attack and jump; **pause is `m_scale x 0.9`**; the arcane-rune dummy is `m_scale` times its own scale. The sprites are 128 px, so 0.4 to 1.4 is about 51 to 179 px. | `buildButtons`, `getPlatformScreenPadScale` | confirmed |
| Layout after a resize | The default layout is **recomputed** from the scaled size, so the margins stay constant. Every pad button sits on the bottom edge, the sprite box flush with the screen's bottom (the visible face is about 100 px inside a soft shadow), 44 px from the left and right screen edges, 32 px between neighbours. Left to right: left, right, a gap, pause (bottom centre, 0.9x, origin bottom-centre), a gap, attack, jump (in the bottom-right corner). Every button's pivot is its centre, so a size change grows it about its own centre while the default centre is recomputed. The row is also held to a width of **1498 px** (`live::SCREEN_PAD_MAXIMUM_WIDTH`), centred, on very wide screens. Custom offsets (section 4) are added on top and are **not** scaled. | `buildButtons`, `UIButton::getMinMaxPoints`, the constants 44.0, 32.0 and 1498.0 | confirmed |
| After a size step | `refreshScreenPad` clears the grabbed button, **re-locks** the lock switch, updates the text and **rebuilds all buttons**. Overlaps made by growing are pushed apart by `collapseButtons` (section 4). | `refreshScreenPad`, `update` | confirmed |
| Hit areas | Each button's touch area starts as its sprite inset 4 px, then grows in 16 px steps on each side (4 or 8 cycles), then in 2 px steps, a side stopping once it would overlap another button (`expandClickableArea`): touch targets are fat and fill the free space. While unlocked, each area is drawn as a faint white rounded frame (`rounded-edges-frame.png` at `0x22ffffff`). | `ScreenPadButton::resetClickableArea`, `draw`, `expandClickableArea` | confirmed |

## 3. Transparency

| Question | Answer | Evidence | Conf. |
|---|---|---|---|
| Key, default | `screenPadTransparency`, 1.0. A **multiplier**, not an alpha. | `getCurrentTransparency()` | confirmed |
| Step, range | 0.2 per tap, from **0.2** (dim only while the value is above 0.2f) to **1.8** (brighten only while below 1.8f): nine values | `SUBIf` / `ADDIf` with `0x3e4ccccd`; `CMPIf` with `0x3e4ccccd` and `0x3fe66666` | confirmed |
| What it does | Left, right, attack, jump: `alpha = min(computeButtonAlpha() x t, 1.0)`. Pause: `min(t x 0.6, 1.0)`. Arcane dummy: `min(t x 0.25, 1.0)`. | `setTransparencyFromUserData` | confirmed |
| Base alpha | `computeButtonAlpha() = min(0.25 / m_scale^2, 0.25)`: 0.25 at any scale up to 1, falling as the pad grows (0.174 at 1.2, 0.128 at 1.4). At scale 1.0 the idle alpha of the four main buttons runs from 0.05 (t = 0.2) through 0.25 (the default) to 0.45 (t = 1.8). The constants are `character::SCREEN_PAD_BUTTON_ALPHA = 0.25` and `SCREEN_PAD_PAUSE_BUTTON_ALPHA = 0.6`. | `computeButtonAlpha`, the global initialisers | confirmed |
| Pressed look | Each button has `setPressColor(Color(0xffffffff))` (opaque white) and the standard `UIButton` press effect, so a pressed button should flash to full opacity whatever its idle alpha. | `ScreenPadButton::DefaultConstructor`, `UIButton::applyPressingEffects` | likely |
| `ScreenPadButtonAlphaController` | **Not the user setting.** A fade attached to every pad button: `dimButtons()` lowers an internal 0..1 factor by `unitsPerSecond(1.6)` a frame, `brightenButtons()` raises it, `isVisible()` is factor above 0.005. `PlayerCharacter::update` (and the tavern and duel interfaces) dims the pad while the character is not under the player's controller (cutscenes, dialogs) and brightens it after; the factor multiplies the final alpha. There is no auto-hide when idle. | `ScreenPadButtonAlphaController::*`, `PlayerCharacter::update` | confirmed |
| Feedback | The pad behind the popup changes at once (`setTransparencyFromUserData()` after every tap); the number pulses 0.8x (dim) or 1.2x (brighten). | `update` | confirmed |

## 4. Move, lock and restore

The state machine is `ControlsOptionsLayer::update()`, run every frame. There is no hold-to-repeat anywhere.

1. `locked = lockSwitch.isEnabled()`. The screen **always opens locked** (`m_lastLockState = true`); the lock is never saved. confirmed.
2. When the lock changes:
   - to **locked**: the switch's release sound becomes `soundfx/metallic_button.mp3`; the wiggle stops on `left right attack jump pause useArcaneRune`
     and on the lock icon; the frame goes back to `smaller-frame-bg.png`; `setDrawClickableArea(false)`.
   - to **unlocked**: the release sound becomes `soundfx/item_pick_deny.mp3`; every pad button and the lock icon get a
     `UIWiggleEffect(stride 200, angleOffset 2.0, randomize true)` (a sinusoidal rotation of plus or minus 2 angle units, presumably degrees, period
     200 ms, random start phase: the iOS "jiggle"); the frame becomes the red `smaller-frame-red-bg.png`; `setDrawClickableArea(true)` (the faint
     hit-area frames appear). confirmed.
3. Which tap plays which sound (derived by static analysis): `UIButton::update` plays the release sound before `onButtonPressed` flips the switch,
   and the layer assigns the new release sound one frame later, so the tap that unlocks plays `metallic_button.mp3` and the tap that re-locks plays
   `item_pick_deny.mp3`: the "deny" sample is the lock-engaged cue, not an error. The press sound at touch-down is `click.mp3` in both. likely (the
   order is confirmed, the intent is inferred).
4. **Drag, only while unlocked.**
   - Grab: on the frame a touch starts (`getAnyHitPos() != NO_TOUCH`), `getButtonDrawableAreaOnPoint(pos)` (the **drawn** sprite area, not the
     expanded hit area) is compared with the six pad buttons; a match becomes `m_grabbedButton` and plays `pleasant_interface_38.mp3` at volume 1.0,
     **speed 0.8**. confirmed.
   - Move: while grabbed, `offset += getAllTouchMoves()`, the sum of `GetTouchMove` over every touch in the down state, in screen pixels per frame.
     The button moves by the finger's deltas; it does not snap its centre to the finger. Each change is written to disk at once. confirmed.
   - Release: when every touch is up (`getAnyTouch() == KS_NONE`) or any touch releases, the grab ends and the sample plays at **speed 1.2**.
     confirmed.
   - Locked: a pending grab is dropped, with the 1.2 sample. confirmed.
5. **Constraints** (`collapseButtons(speed, onlyThisOne)` every frame; `speed` is 256 while a button is grabbed, otherwise 128 px/s):
   - For each pad button `a` that is `collapsable` (left, right, attack, jump, the arcane dummy; **not** pause) and each other button `b` (skipping
     `b.ghost`): if `a.isOverlapping(b, 16 px)` then `a.offset += normalize(a.mid - b.mid) x unitsPerSecond(speed)` and the hit area is
     recomputed. While a button is grabbed only that one is pushed. So pad buttons cannot be left overlapping: they are pushed apart, including after
     a resize. confirmed.
   - Two **forbidden zones**, invisible gameplay HUD areas created on first use as `fobidden0` and `fobidden1` (sic) with a red frame `0x44ff0000`:
     **400 x 170 px at the top-left corner** and **300 x 486 px hanging from the top centre**. Pad buttons are pushed out of them like any
     overlap. The pause button is `ghost` (ignored by the others and ignoring them) but is still pushed out of the zones. The zones exist **only**
     here (`collapseButtons` is called nowhere else). confirmed (their existence); likely (that they are visible, as ordinary drawables inserted
     as background elements).
   - **No screen-bounds clamp** was found. The functions read for it: `ControlsOptionsLayer::update`, `ScreenPadButton::setAbsCustomOffset`,
     `updateOffsetNormPos`, `CharacterScreenPadController::collapseButtons`, and `UIDrawable::setNormPos`, `addToNormPos`, `addToNormPosX`,
     `addToNormPosY`, `setNormPosX`, `setNormPosY`, `addAbsValueToNormPos`, `updateCurrentPos` and `getAbsolutePos` (all plain stores or
     additions). A button can therefore be dragged until its centre is off the screen, the finger staying on it because the grab is relative.
     confirmed by absence in those functions; the engine's rendering side was not available.
6. **Restore** (the `refresh` button, no confirmation): `isPressed()` re-locks (`lockSwitch.setEnabled(true)`), `resetCustomPositionChange()` sets
   every pad button's offset to (0, 0) and saves it, and `expandClickableArea(false)`. It looks enabled only when `hasAnyCustomPositionChange()`
   (any offset not zero): then a white icon and the release sound `soundfx/projectile_heavy-blade_throw.mp3`; otherwise 27% alpha and no sound
   (it still re-locks if tapped). It does **not** reset the size or the transparency. There is **no** "restore the default positions?" dialog: the
   string `restoreDefaultPositionsQ` exists in `ui.strings` but no layer or function refers to it. confirmed.

### Quirks to know before copying

- `isPressed()` is **a completed tap** (the touch is released inside the button; `m_pressed` is set on the RELEASE state for non-capacitive
  buttons), so one tap is one step and holding does nothing. confirmed.
- Offsets are **absolute screen pixels**, not normalised: they do not follow a resolution or window change. A copy should store them relative to
  something that does (guess: Magic Rampage did not care because phones rarely change resolution).
- The layer re-reads `screenPadScale` and `screenPadTransparency` from disk every frame.
- `getAllTouchMoves` sums every moving touch, so a second finger adds to a drag.
- The pause button's transparency ignores the base alpha and the scale (`min(t x 0.6, 1)`), so at the default it is 0.6.
- A resize re-locks the switch but does not re-centre custom offsets.

## 5. Persistence

All through `SEF/src/UserData.angelscript`: `setUserData(key, value)` writes the value as text to `<GetExternalStorageDirectory()><key>.userValue`
(one small file per key); `getFloatUserData` and `getVector2UserData` parse it back, with defaults when the file is missing. confirmed.

| Key (file stem) | Value | Written by | Default |
|---|---|---|---|
| `screenPadScale` | decimal float text, e.g. `0.8` | size taps | 1.0 |
| `screenPadTransparency` | decimal float text | dim and brighten taps | 1.0 |
| `dpad-custom-offset-<name>`, for `name` in `left`, `right`, `attack`, `jump`, `pause`, `useArcaneRune` | `x,y` as doubles, absolute pixels | every drag frame, every collapse push, restore (which writes `0,0`) | `0,0` |

The same three keys are read in play: `CharacterMultiInputController` builds the same `CharacterScreenPadController`, and `ArcaneRuneButton`,
`ArcaneRuneDummyButton` and `SideSipButton` also read `screenPadScale` and `screenPadTransparency`. The lock, the grabbed button and the forbidden
zones are never saved. confirmed.

## 6. Words

`assets/lang/<id>/ui.strings` holds ten languages (de, en, es, fr, it, ja, pt, ru, tr, uk; no Arabic), 1,227 keys each. The only keys the 7.8.7
code and layers use for this feature are **`controls`** (the Options button) and **`screenPadAdjustments`** (the popup's title); the numbers, the
signs and the `.0` are not translated. The package has eleven more keys on the subject (`buttonsSize`, `buttonSize`, `buttonSizeShort`, `lockButtonPos`, `lockButtonShort`, `moveButtons`,
`moveButtonsShort`, `restoreDefaultPositionsQ`, `screenpadSize`, `screepadOptions` (sic), `adjustControls`) in every language. Ten of them have no reference
in `game_64.bin`, `game_32.bin`, `classes*.dex`, `libmachine.so` or any `.layer` or `.element` (searched as ASCII and as UTF-16); the eleventh,
`moveButtonsShort`, is referenced once, by the keyboard tutorial's "Move", which is unrelated. The ten look like the leftovers of an earlier or planned design with text labels, and are not Magic Rampage's behaviour.
No key mentions transparency, opacity, dim or
brighten: those two rows are icon-only. The texts are Asantee's; the enhanced edition's own are in its `strings.json`.

## 7. Assets this screen uses

| Asset (under `assets/`) | Use |
|---|---|
| `sprites/inventory-back-button.png`, `frame-title-bg.png`, `smaller-frame-bg.png`, `smaller-frame-red-bg.png` | the back button, the title pill, the widget frames, the unlocked lock's frame |
| `sprites/resize-symbol-shrink.png`, `resize-symbol-enlarge.png`, `adjust-symbol-transparent.png`, `adjust-symbol-opaque.png`, `lock-locked.png`, `lock-unlocked.png`, `icon-restart-64.png` | the seven glyphs |
| `sprites/rounded-edges-frame.png` | the hit-area frames (white `0x22ffffff`) and the forbidden zones (red `0x44ff0000`) |
| `soundfx/click.mp3` | the hit sound of every widget |
| `soundfx/metallic_button.mp3`, `item_pick_deny.mp3` | the lock switch's release sounds (section 4) |
| `soundfx/pleasant_interface_38.mp3` | grab (speed 0.8) and drop (speed 1.2) |
| `soundfx/projectile_heavy-blade_throw.mp3` | restore, when there is something to restore |
| `soundfx/button.mp3` | the back button's release |
| `Averia48.fnt` | the title and the two numbers |

## 8. What was not found, and what is open

- Not in the shipped UI: a per-button size, a per-button transparency, a selected-button concept, a move-mode toggle, a restore confirmation,
  auto-hide when idle, a screen-edge clamp, a saved lock state. (confirmed by decoding)
- The exact scale of the pixel constants (44, 32, 102, 140, 400x170, 300x486, 1498) on a device with a density factor. guess.
- The exact text the engine writes for a float (the "1.4" or "0.6" look) is inferred from the special case that appends `.0` to 1.0; the engine's
  C++ is not in the package. likely.
- The pressed look (a flash to full opacity) is inferred from `setPressColor(white)`, never observed. likely.
