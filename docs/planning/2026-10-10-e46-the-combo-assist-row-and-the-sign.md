# E46: the combo assist's options row, and the level-1 sign's commas

Built 2026-10-10, in the game (`game/script/videoModes.cpp`, `Script.hpp`, `PenumbraLayer.cpp`, `game/data/strings*.json`), after [E44](2026-10-09-e44-combo-assist.md) left both open.

## What and why

- **A row for the assist.** E44 made the assist a setting, `controls.comboAssist` in `settings.json`, and said so in the guides; nothing in the game could turn it off. The desktop's Settings screen now has a `Switch` for it, "Combo assist" /
  "No combo assist" (`g_comboAssist`, like E13's "Pause on focus loss"), at (600, 564) to the right of the language chooser, in the place the E36 first design's difficulty row occupied (nothing else is in x 590-909 there in the desktop's two
  layouts). It is not drawn on a phone's layouts (E20's list, E31's large screen): a phone has no keyboard for the assist to help, and its cells are laid out elsewhere, so no phone text or pin moved.
- **It reaches the live mapper.** The input mapper holds its own copy of the controls, so a switch that only wrote the settings would change the file and nothing the player does until a restart. The layer's tick keeps `settings.controls`
  in step with the switch as it does for the second player's keys and calls `InputMapper::SetControls` at once; seeded from the setting when the layer attaches.
- **The sign.** Level 1's sign said "Try out a few key combinations, like [forward + forward + sword]" (and, on a phone, "button combinations"). A combo is three separate taps one after another and a plus sign between keys reads as keys pressed
  together. The translated values of that sign (the ten languages other than Portuguese, and the touch wording's Portuguese) say "[forward, forward, sword]" now: commas, the ideographic comma in Japanese, the Arabic comma in Arabic. Only translated
  values changed, 21 lines in 10 files. The plain sign's Portuguese is the original's own text, which the game looks the translations up by and which has no table of its own, so a Portuguese keyboard player still reads the plus signs.
  Whether the plus signs were what the Windows tester misread is not known.
- Eleven languages for the two labels; the Japanese uses katakana the font already has, so the bundled face is as it was.

## Measured and tested

- `test_pn_render_hud`: both labels in every language fit their room (309 px with the check box in front; the rooms file says the phone has none) and every glyph is in the bundled faces (the Arabic shaped), 15,187 checks. A layer-level test
  (`TestComboAssistInLayer`) drives the engine's raw keys through a real layer: with the assist on LEFT and S pressed on one tick reach the game one tick apart (left, then S); after the switch is flipped, the next pair reach it together; flipped back,
  apart again; settings that say off seed the switch with its second row.
- Scenario 14 clicks the new row like the others (both states drawn, the switch flips and flips back). Scenarios 21 and 26 (the phone's options screens, exact texts) did not change.
- Not run: a real click on a real desktop screen (the capture below is on a software driver), and a native speaker's reading of any of the labels.
