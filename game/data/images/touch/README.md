# The touch controls' buttons (E16)

The PNGs in this folder are the on-screen buttons of **Magic Rampage 7.8.7 (Asantee Games)**, taken
from its Android package `com.asanteegames.magicrampage.apk` (the base APK inside the APKPure
`.xapk`). Asantee also made the
original Penumbra.

`tools/art/make_mr_touch_art.py` makes them from that package and only reads it:

```
python tools/art/make_mr_touch_art.py --xapk "<path>/Magic+Rampage_7.8.7_APKPure.xapk"
```

The script does four things to every file:

- converts Magic Rampage's palette PNGs (transparency in a tRNS chunk) to straight-alpha RGBA;
- clears the colour under fully transparent pixels;
- resamples premultiplied;
- checks that no pixel is exact magenta `#FF00FF`, which the HUD keys out.

`game/data/touch_controls.json` lays the buttons out. `render/TouchControls` draws them at their
idle alpha and brightens a button while it is held; there is no pressed image.

## Which file is which

Every source path is under the base APK's `assets/`.

| Ours | Magic Rampage 7.8.7, `assets/...` | How |
|---|---|---|
| `jump.png` | `sprites/jump-pad-button.png` | Its jump button, as shipped. |
| `sword.png` | `sprites/attack-pad-button.png` | Its melee attack button (a dagger), as shipped. |
| `pause.png` | `sprites/pause-pad-button.png` | Cropped to its pill, `(0, 42, 128, 128)`, giving 128x86 (the pill sits in the lower part of a 128 square). |
| `back.png` | `sprites/inventory-back-button.png` | Its back button (on about forty of its screens), as shipped. |
| `dpad_left.png` | `sprites/dpad-left.png` | As shipped, placed alone on a transparent 406x406 canvas (the direction control's 330 logical px) where its sector is. |
| `dpad_right.png` | `sprites/dpad-right.png` | The same. |
| `dpad_down.png` | `sprites/dpad-right.png`, `sprites/dpad-frame.png` | **Composed.** Magic Rampage has no down button. `dpad-right`'s inner face (pixels 30..97) is mirrored across the main diagonal onto the blank frame, so the right arrow becomes a down arrow and keeps its shadow to the lower right. The face's shading carries over as a ratio to the frame, and the arrow's outline and fill carry over as they are. Placed as the other two. |
| `dpad.png` | none | Transparent: Magic Rampage has no disc under its buttons. The control's base image is empty. |
| `dpad_knob.png` | `SEF/media/button-highlight.png` | As shipped: the four corner brackets its UI frames a focused button with. Here they follow the thumb onto the direction button it holds. |
| `fire.png` | `sprites/dpad-frame.png`, `sprites/elements/element-fire.png` | **Composed.** The white fire-element glyph redrawn in the style of its pad icons (see below), centred on the blank frame. |
| `light.png` | `sprites/dpad-frame.png`, `sprites/elements/element-light.png` | **Composed.** The light-element glyph, the same way. |
| `combo_sword.png` | `sprites/dpad-frame.png`, `sprites/button_rune_arcane_off.png` | **Composed.** The Arcane Rune gem, scaled to 68 px with the icons' shadow, on the blank frame. |
| `combo_spell.png` | `sprites/dpad-frame.png`, `entities/rune_fire.png` | **Composed.** Its fire rune gem, scaled to 64 px, the same way. It pairs with the one above. |

**The icon style.** Magic Rampage's pad icons, as measured on `dpad-left.png`, are drawn like this:

- a fill of about 215 grey;
- a black outline about 3.5 px wide;
- a soft shadow to the lower right.

The composed glyphs are drawn the same way: fill 215, outline 3.5 px, a shadow offset (3, 3) with
a 3.5 px blur at 0.6.

**The direction control stays one round control.** The thumb slides between left, right and down
without lifting. Its three buttons are drawn where its sectors are:

- centres (-113, -18), (113, -18) and (0, 111) logical px from its centre;
- each 104 logical px.

The script feeds every opaque pixel through the sectors that `TouchControls.cpp` reads, and
`test_pn_render_touch` checks the same:

- all of the left and right buttons hold that side alone;
- all of the down button holds down, and 96.9% of it holds down alone;
- its upper corners reach into the down-left and down-right sectors.

**The combo buttons.** Magic Rampage's nearest control is the Arcane Rune button, a one-tap special.
Its tip is "Forward three times and Attack" (`lang/en/tips.strings`, tip26), and its glyph is three
chevrons and a sword. That matches the sword combo (side, side, sword).

- The rune on the blank frame is this script's pairing. In Magic Rampage it is inferred only from
  the two images sitting together in its compiled script's constant pool.
- The fire rune for the spell combo (down, side, fire) is chosen to pair with it.

**How Magic Rampage builds its pad.** This is read from its compiled script, `assets/game_32.bin`
(`CharacterScreenPadController`):

- six square buttons: left, right, jump, attack, pause and the Arcane Rune;
- each is `sprites/dpad-frame.png` (a 128 px light-grey face, a white bevelled rim and a soft drop
  shadow) with a pre-baked icon;
- a press tints the button;
- the art exists only at 128 px, with no `hd/` or `fullhd/` variant.

## The placeholder look

`placeholder/` holds the first look, drawn by `tools/art/make_touch_art.py`: a disc and round
buttons, shapes only, the port's own. Its manifest is `placeholder/touch_controls.json`, with the
layout that look was made for. To go back to it, copy that file over
`game/data/touch_controls.json`. `test_pn_render_touch` checks both manifests' art and layout.
