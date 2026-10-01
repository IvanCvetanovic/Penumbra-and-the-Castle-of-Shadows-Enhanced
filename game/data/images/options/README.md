# The options screen's and the language button's art (E27), and the touch controls' editor's tiles (E28)

The PNGs in this folder are the pieces the options screen is built from (stone-frame panels,
check boxes, arrow, minus and plus buttons, icons) and the main menu's language button, and, since E28,
the seven tiles of the touch controls' editor (`edit_*.png`: shrink, enlarge, dim, brighten, the closed and
the open padlock, the circular arrow), which `game/render/TouchEditor.cpp` draws by their path - they are
not in the `kOptionsArt` list of `game/script/Script.hpp`. Most are
**Magic Rampage 7.8.7 (Asantee Games)** user-interface art, taken from its Android package
`com.asanteegames.magicrampage.apk` (the base APK inside the APKPure `.xapk`); two icons and the button's
globe are drawn here. Asantee also made the original Penumbra.

`tools/art/make_options_art.py` makes them from that package and only reads it:

```
python tools/art/make_options_art.py --xapk "<path>/Magic+Rampage_7.8.7_APKPure.xapk" [--sheet sheet.png]
```

It uses the touch buttons' helpers (`tools/art/make_mr_touch_art.py`: the package reader, the
premultiplied resampling, the soft shadow and the styled glyph), so these look like the touch
controls beside them. Like those, it converts Magic Rampage's palette PNGs to straight-alpha RGBA,
clears the colour under fully transparent pixels, resamples premultiplied, and checks that no pixel
is exact magenta `#FF00FF`, which the HUD keys out. A second run writes the same bytes.

`game/script/optionsArt.cpp` draws them: `drawPanel` (nine slices of `panel.png`), `drawOptionsIcon`
(the others, by name). The scripts name them by their file name without the folder or `.png`.

## Which file is which

Every source path is under the base APK's `assets/`.

| Ours | Size | Magic Rampage 7.8.7, `assets/...` | How |
|---|---|---|---|
| `panel.png` | 92x92 | `sprites/smaller-purple-frame-bg.png` | The stone frame with its dark-purple inside, cut to its body (see below). Drawn in nine slices: the corners are 26 px (`kPanelSlice` in `Script.hpp`), the edges and the middle stretch. |
| `check_on.png` | 64x64 | `sprites/check-icon.png` | The green check in its stone frame, cut to the body and scaled to 64. |
| `check_off.png` | 64x64 | `sprites/smaller-purple-frame-bg.png` | **Composed.** The panel's frame stretched, in nine slices, to the check box's body size and then scaled to 64: the same stone, an empty purple inside. |
| `arrow_left.png`, `arrow_right.png` | 64x64 | `sprites/inventory-back-button.png`, `sprites/inventory-next-button.png` | The stone frame with a white triangle, cut to the body and scaled to 64. |
| `minus.png`, `plus.png` | 64x64 | `sprites/smaller-frame-bg.png` | **Composed.** The dark stone frame with a white minus or plus drawn on it, with the pad icons' soft shadow. |
| `speaker.png`, `music.png` | 64x64 | `sprites/audio-toggle-on.png`, `sprites/music-toggle-on.png` | As shipped. |
| `pad.png` | 96x47 | `sprites/fighter-controller.png` | The cream gamepad, cut to its pixels (122x60) and scaled to 96 wide. The one image that is not square. |
| `globe.png` | 64x64 | none | **Drawn.** Magic Rampage has no globe: a white disc with a meridian ellipse, the central meridian, the equator and two latitudes cut out of it, supersampled, with the soft shadow of its icons. |
| `monitor.png` | 64x64 | none | **Drawn.** A rounded screen in a bezel on a short stand, white, the same way. |
| `globe_button.png` | 96x96 | `sprites/dpad-frame.png` | **Composed.** The touch buttons' blank pad frame with the globe styled as their icons are (fill, black outline, shadow). The main menu's language button. |
| `edit_shrink.png` | 96x96 | `sprites/smaller-frame-bg.png`, `sprites/resize-symbol-shrink.png` | **Composed.** The dark stone frame (cut to its body, scaled to 96) with Magic Rampage's own shrink symbol, scaled to 48 and centred. The touch controls' editor: every control smaller (E28). |
| `edit_enlarge.png` | 96x96 | `sprites/smaller-frame-bg.png`, `sprites/resize-symbol-enlarge.png` | **Composed.** The same frame with its enlarge symbol: every control larger (E28). |
| `edit_dim.png` | 96x96 | `sprites/smaller-frame-bg.png`, `sprites/adjust-symbol-transparent.png` | **Composed.** The same frame with its see-through square: every control fainter (E28). |
| `edit_brighten.png` | 96x96 | `sprites/smaller-frame-bg.png`, `sprites/adjust-symbol-opaque.png` | **Composed.** The same frame with its solid square: every control stronger (E28). |
| `edit_locked.png` | 96x96 | `sprites/smaller-frame-bg.png`, `sprites/lock-locked.png` | **Composed.** The dark frame with the closed padlock, scaled to 48: the controls cannot be dragged (E28). |
| `edit_unlocked.png` | 96x96 | `sprites/smaller-frame-red-bg.png`, `sprites/lock-unlocked.png` | **Composed.** The RED stone frame (Magic Rampage's own: its body is smaller than the dark one's, and is scaled to the same 96) with the open padlock: the controls can be dragged. Red means "editing", not an error (E28). |
| `edit_restore.png` | 96x96 | `sprites/smaller-frame-bg.png`, `sprites/icon-restart-64.png` | **Composed.** The dark frame with the circular arrow, scaled to 56 (Magic Rampage draws it at 1.2): put the controls back where they were (E28). |

## The frames' shadow

Magic Rampage's stone frames come with a wide soft shadow past the frame, cut off by the image's
edge, which shows as a halo box when the image is cropped. The script takes it off (pixels that are
dark and not mostly opaque) and crops to the stone, so a panel's rectangle is the visible frame. At
their native size every one of these frames has a stone border 12 px thick. `panel.png`'s slice is
26 px because the top edge between the two corner pieces is then the same along its whole length
(the script checks it) and each corner piece holds a whole rounded corner and the inside's shade.
