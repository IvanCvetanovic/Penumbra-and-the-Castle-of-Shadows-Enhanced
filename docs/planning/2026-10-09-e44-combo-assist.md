# E44: combo assist (keys pressed together are handed to the combo recorder one tick apart)

Built 2026-10-09, in the game (`game/render/InputMapper.{hpp,cpp}`, `Settings.{hpp,cpp}`, the text of the how-to-play files and the guides).

## What and why

A Windows tester said that if keys are pressed "too fast or at once" they do not register, "when trying to do the combo". Whether that was the game or the player was unknown, so a
review of the code (the keyboard path from GLFW to the scripts; the original's `combo.as` against the port; the keyboard's hardware and the OS; every finding re-read by a second reader) looked
at it. Verdict: **not a port defect.** `combo.cpp`, `playerInput.cpp`, `InputMapper` and the Eth layer's key states are byte-identical from 1.0.4 to 1.0.7 and a line-for-line copy of the
original, and E37's rest applies to the touch combo buttons only. What the tester met are the original's rules, which the game never explains:

- A combo is three separate fresh taps (a held key does not count; the same key twice needs a release): the sword combo is left, left, S (or right, right, S), 5 mana; the blast is down,
  a side, D, 25 mana.
- The recorder (`Combo::updateInput`, `combo.as:63`) keeps **one command per tick** - the first KS_HIT in the order left, right, up, down, S, D - and a HIT lasts one tick. Keys that go down
  in the same sixtieth of a second keep only that one, and down with a side key always loses the down. A hand rolling a combo lands two keys 8 ms apart on one tick about half the time
  (never from 18 ms: a model of the port at 60, 144 and 500 frames a second and of the original agree).
- Each gap between presses may be at most 13 ticks (217 ms: `GetTime()` is whole milliseconds of tick*1000/60 and the test is `> 210`).
- The buffer matches only its **first three entries** since it last emptied (after 14 quiet ticks, 233 ms). Any key recorded just before the combo - the jump (Up), a sword swing, a
  fireball, another arrow - spoils that try, and pressing on without a pause never recovers: mashing does not work.
- Without 5 or 25 mana the combo shows a short message and a plain sword swing or fireball happens; a mistyped combo gives no message.
- Shipped texts said "Left Left S, and Down Left D" and the level-1 sign "[forward + forward + sword]", which reads as keys together.

Keyboard rollover, Windows Filter Keys and a very low frame rate can add to it and cannot be told from the code.

## What changed

- **Combo assist** (`ControlSettings::comboAssist`, default on; `"comboAssist": false` under `controls` in `settings.json` gives the original's rule back; an options row since E46): of the keyboard's five combo keys (down, left, right, S, D), the ones that
  are NEW on one tick (down, and not seen down at the last tick) or were held back by an earlier tick are handed to the game one tick apart, in the order a combo needs: down, then a side,
  then S or D. A key that comes up before its turn is still seen, for one tick. The jump (Up) is not one of them and never waits; a lone press is never delayed; a key pressed while another is held
  is not delayed; the wait is a sixtieth of a second per key at most; the window losing the focus drops what was waiting. It is in `InputMapper::BuildTick`, after the latch of a frame that ran
  no tick, so the scripts are untouched (rule 9) and only player 1's keyboard is touched. Touch macros, pads and the second player's keys are as they were.
- The texts: the three `how-to-play.txt` (English and Portuguese), `docs/controls.md` (the rules above, and the assist), the README's combo line and the install guides now say that a combo is
  three separate taps, one after another, each within a fifth of a second of the last, and what to do when it fails.

## Measured and tested

- `test_pn_render_input` has 26 new checks (the assisted sequences tick by tick: left with S, down with each side, all three of the blast's keys, a key that comes up before its turn, a lone press,
  a held key with another pressed later, the jump, the focus, a latched press, and the setting's JSON round trip and its default in a file written before E44) and every other check of the suite
  is unchanged.
- **End to end, added after the release (scenario 29 of `test_pn_scenarios`, which now links the whole game for it):** the keyboard's keys per tick go through a real `InputMapper` into the game's
  recorder. Assist off: the second RIGHT with S pressed together, DOWN with RIGHT together, and all three of the spell's keys together make no combo (a plain fireball); with it on each makes the combo
  (5 and 25 mana); three clean taps make it both ways. The tester's "keys pressed too fast or at once do not register" is therefore reproduced by the original's rule and fixed by the assist, in the game's
  own machine. **Not run: a real keyboard** (rollover, Filter Keys, key-repeat timing, a frame rate that is not 60 Hz): the keys here are ticks, not hardware.

## Open

- Done in [E46](2026-10-10-e46-the-combo-assist-row-and-the-sign.md): the desktop's options screen has a row for it, and the level-1 sign separates its three keys with commas in ten languages.
- The buffer's anchoring (a stray key spoils the try) and the 217 ms gap are the original's and unchanged. A looser recorder (match the last three keys, not the first) would make mashing work,
  and would change what a player can do by accident; not done.
