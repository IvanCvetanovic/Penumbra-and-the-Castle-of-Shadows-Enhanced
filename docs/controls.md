# Controls

The keyboard bindings of both players are in `settings.json` and can be changed there
([Settings and saves](playing.md#settings-and-saves)).

## Player 1, keyboard

These are the original's keys:

| Action | Key |
|---|---|
| Walk | ← → |
| Jump | ↑ or Ctrl |
| Sword | S |
| Fireball (10 mana) | D |
| Light spell (50 mana) | Space |
| Confirm (menus) | Enter; the mouse drives the main menu |
| Pause (in a level or an arena; Main menu inside) | Esc |
| Fullscreen / window | Alt+Enter |

Esc in the menus, the options and game over goes back, as in the original. In the pause, the
arrows choose, Enter confirms and Esc resumes; the mouse works too.

**Combos.** A combo is three separate taps, one after another: each key pressed afresh (a held key does not count, and the same key
twice needs a release between the presses), each within about 210 ms of the one before (`combo.as`; in practice 13 ticks of the game's 60 a
second, 217 ms)
([`spec/11-logic-player-combat.md`](https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/blob/main/docs/spec/11-logic-player-combat.md) §10):

- **Sword combo (5 mana):** ← ← S or → → S. A stronger sword and a beam; the screen shakes.
- **Blast (25 mana):** ↓ ← D or ↓ → D. A big fireball with 225 damage.

The original's combo memory is strict, and these are its rules, which the game keeps:

- It records **one key per tick** (a sixtieth of a second), the first of: left, right, up, down, S, D. Keys that go down in the same tick
  keep only that one. Pressing down and a side key together loses the down, so the blast cannot be made by pressing the diagonal.
  (Typing a combo on a keyboard, a hand rolls the keys, and about half of two keys 8 ms apart land on one tick: see **Combo assist**.)
- It matches only the **first three keys** since it last emptied, and it empties after about a quarter of a second with no new key. Any key
  recorded just before a combo spoils that try: the jump (Up), a sword swing (S), a fireball (D), another arrow. Pressing on without a pause
  never recovers; stop for a quarter of a second and begin again. Mashing does not work.
- Without the mana (5 for the sword combo, 25 for the blast) a short message is shown and an ordinary sword swing or fireball happens
  instead. A combo that is mistyped gives no message at all.

**Combo assist (E44).** On by default. Of the keyboard's five combo keys (down, left, right, S, D), the ones that go down together on one
tick are handed to the game one tick apart, in the order a combo needs (down, then a side, then S or D), so a roll of the fingers records
every key instead of the first. Only player 1's keyboard, only presses that land on the same tick, and no other rule changes; a plain press
alone is never delayed, and a press that has to wait waits a sixtieth of a second per key at most. The row **Combo assist** of the desktop's
Settings screen (E46, beside the language) turns it off ("No combo assist") and on, at once; `"comboAssist": false` under `controls` in
`settings.json` does the same. Off, the original's rule is back. (A phone has no row for it: it has no keyboard for the assist to help.)

Typed on a keyboard or a pad, a combo has no rest: it can be repeated as often as the keys can be pressed
and the mana lasts, as in the original. The combo buttons of the touch controls are the only ones that
rest after each use ([E37](#touch-controls-e16)).

**The original's hidden keys.** When the menu's fade ends on New Game (the fade that starts once a
difficulty is chosen, [below](#choosing-the-difficulty-e36)):

- Hold 2 or 3 to start at level 2 or level 3.
- Hold Page Up to start both characters at level 15 (`main.as`).

## Choosing the difficulty (E36)

Confirming New Game on the main menu (Enter, a click, a tap, a gamepad's Start or A) no longer starts the
game: it opens a prompt, "Choose difficulty", over the dimmed menu, with two rows, Normal ("Enemies as in the
original game.") and Hard ("Enemies have twice the health."). The lit row is the one chosen last, Normal the
first time. What the difficulties are is in [playing.md](playing.md#difficulty-and-best-times).

- **Keyboard:** Up and Down light the other row (they stop at the ends), Enter starts the game at the lit row,
  and Esc closes the prompt and starts nothing.
- **Gamepad:** the D-pad or the stick lights the other row, Start or A starts the game at the lit row, and
  Back or B closes the prompt. (A pad drives the menus only while the touch controls are off: with them on, the first pad
  plays player 2, [E22](#touch-controls-e16).)
- **Mouse:** a click on a row chooses it and starts at once, and a pointer that moves onto a row lights it. A
  click outside the panel closes the prompt; a click inside the panel on no row does nothing.
- **Touch:** a tap on a row chooses it and starts at once. A phone's main menu has no Back button, so a tap
  outside the panel is the way out; a tap inside it on no row does nothing.
- For the first third of a second after the prompt opens, clicks and taps are ignored (so a double-click on New
  Game neither chooses a row nor closes the prompt); Esc and Enter are not delayed. While the prompt is open the
  menu behind it does not answer the pointer (no hover panels), the system pointer is shown over it, and the
  direction keys and the stick move the highlight, not the pointer. Alt+Enter still switches between a window
  and fullscreen. A chosen row starts the old 3-second fade-out and then the campaign.

## Player 2, keyboard (E4)

This is on by default. Player 2 is presented to the game as the joystick that player 2 reads.

| Action | Key |
|---|---|
| Walk | J L |
| Up / down | I K |
| Jump | I |
| Sword | U |
| Fireball | O |
| Light spell | P |
| Start (summon the creature in the campaign) | Backspace |

- In the campaign, player 2 joins by pressing Start. The summon costs player 1 50 mana and a life.
- Only player 1 can take checkpoints and finish a level.

## Gamepads (E3)

Any pad that GLFW recognises as a gamepad (an Xbox layout, for example) is mapped by what each
button means onto the button numbers the original read:

| Action | Pad | Original's button |
|---|---|---|
| Walk / up / down | Left stick or D-pad | axes |
| Jump | A | 3 |
| Sword | X | 4 |
| Fireball | B | 2 |
| Light spell | Y | 1 |
| Confirm / summon | Start | 10 |
| Pause (player 1, in a level or an arena) / back | Back | 9 |
| Confirm / back in the menus (E14) | A / B | 10 / 9 |

- Pads are read every frame. The original's "hold J to detect joysticks" is no longer needed.
- By default the first pad plays the wizard (player 1) and drives the menu, and a second pad plays
  the princess (E12). In 2010 the first pad was player 2's; `firstPadIsPlayer1: false` in
  `settings.json` restores that. The options screen's input switch (the original's) still swaps
  which joystick each player reads.
- While the touch controls are on (a phone, or `--touch`), they are player 1 and the first pad
  plays player 2 (E22): the princess in the campaign (Start summons her), player 2 in Versus. A
  second pad is not used then; to play on two pads, turn the touch controls off in the options.

## Touch controls (E16)

On a phone or a tablet the game draws its own buttons. `touchControls` in `settings.json` is
`auto` (on in the Android and iOS builds), `on` or `off`. On a desktop, `--touch` shows them, and
the mouse, held down, is the finger.

| Action | Touch | The key it presses |
|---|---|---|
| Walk | The two buttons at the bottom left (left, right), a little under the bottom row of action buttons (25 units under the middle of the jump and sword buttons): one control, the thumb slides between them | Left, Right |
| Next level (E25) | The down button that shows above the two, only while the wizard stands at a level's exit | Down |
| Jump | The right-most button, at the bottom right (the bottom of the right column, the higher of the two columns) | Ctrl |
| Sword | The bottom button of the left column, 30 units lower than the jump button | S |
| Fireball | The top of the right column, above the spell combo button (the highest of the six) | D |
| Light spell | The top of the left column, above the sword combo button | Space |
| Sword combo | The middle of the left column, between the sword and the light; rests for a second after each use (E37) | the way he faces twice, then S |
| Spell combo | The middle of the right column, between the jump and the fireball; rests for a second after each use (E37) | Down, the way he faces, then D |
| Pause (in a level) / back (arena select, game over, the end screens) | The button at the top right | Esc |

The six action buttons at the bottom right are two staggered columns of three under the right thumb (E29, E32, E33):

| | Left column | Right column |
|---|---|---|
| Top | Light spell | Fireball |
| Middle | Sword combo | Spell combo |
| Bottom | Sword | Jump |

The six action buttons are at the bottom right, in two columns of three under the right thumb. The right column stands a little higher
than the left one. From the bottom up, the left column holds the sword, the sword combo and the light spell, and the right column holds
jump, the spell combo and the fireball. The walking buttons are at the bottom left, a little under the bottom row of action buttons.
The exact positions are in [the layout's record](https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/blob/main/docs/planning/2026-10-02-e33-touch-grid-staggered.md).

- Several fingers work at once: hold a direction and tap the buttons.
- The down button (E25) appears only where down does something on its own: at a level's exit
  door, where it takes the wizard to the next level. Everywhere else the direction control is
  left and right only; the spell combo's down is its combo button's.
- The camera is zoomed in on the campaign's levels while the touch controls are on (E25): 175% on
  a phone (held at what the screen has room for: about 162% on an 18:9 one), 125% on a tablet (a 4:3
  one has room for about 110%), or what the options screen's Zoom row says. The controls keep their size. Once the princess is summoned the level goes back to the
  unzoomed view, so she can follow the wizard as far as she could in the original.
- The HUD keeps clear of a phone's curved edges, rounded corners and camera (E26): the timer and
  the messages are drawn a little way in, the pause button with them, and the hp, mp and level
  values are light on a dark shadow. `edgeMargin` in `settings.json` sets how far in (`"auto"`,
  or 0 to 8 percent). The hp, mp and level panel is a stone plaque, stone on all four sides,
  standing in the corner of the display's safe area instead of floating in that margin.
- Two players on one phone (E22): player 1 plays on the touchscreen and player 2 on a Bluetooth
  gamepad. Versus opens once a pad is connected (until then its entry says "Connect a gamepad for
  player 2"), and in the campaign the pad's Start summons the princess. The pad never moves the
  wizard, and it does not drive the menus: player 1 taps them.
- A combo button presses its combo's keys one per tick, toward the way the wizard faces. If a
  direction, the sword or the fireball was pressed in the last fifth of a second, it first waits
  until the game's combo memory has emptied (the original forgets a combo 210 ms after its last
  key), so the combo always registers. While it runs, the disc, the sword and the fireball buttons
  wait; jump, light and pause do not. A tap on either combo button while a combo runs is refused
  (the next bullet); the pause or a new scene stops the combo.
- Each combo button rests for one second (60 game ticks) after it is used (E37), counted from the tick
  its keys press the attack key, S or D. The sword combo and the spell combo rest separately, so the
  rest of one does not hold the other back. While a button rests it is grey, with a dark shade over
  the part not yet recharged, from the top, shrinking until the button takes a tap again, and then it
  glows for a moment. A tap on a resting button, or on either combo button while a combo runs (a few
  ticks), starts nothing and is not queued: that button flashes red and shakes for a quarter of a
  second, and the original's "not yet" sound (`soundfx/fail.ogg`, the one its menu plays for a locked
  arena) plays, at most once in a sixth of a second and not restarted while it is still sounding. A
  finger held on a resting button does not fire it when the rest ends; lift it and tap again. A combo
  that is stopped before its attack key (the pause, a menu, the controls switched off, a scene load,
  giving up the wait for the combo memory) starts no rest, every new scene (the next level, a death's
  reload, leaving to a menu) starts with both buttons ready, and the pause and the touch editor stop
  the count. The editor's preview shows the buttons at rest. Combos typed on a keyboard or a pad, and
  combos made from the ordinary buttons (walking plus sword or fireball), have no rest.
- In the menus, the options, game over and the pause, the buttons are hidden and a tap clicks
  where it lands. The options screen has no touch back button at the top right: the original's own Back
  arrow is on it (at the top left on a phone's large layout, E31), and a tap on it goes back. The main menu and the pause have none either: a tap on New Game opens E36's difficulty prompt, a tap on one of its rows starts the campaign at that difficulty, and a tap outside its panel closes it ([above](#choosing-the-difficulty-e36)).
- The focus brackets show on the direction button the thumb holds, and nowhere while no direction
  is held (`"atRest"` in the manifest's `knob`; the placeholder look keeps its knob at the centre).
- While the touch controls are on, the game's control hints speak of the buttons, in all eleven
  languages: level 1's help signs ("Use the arrows at the bottom left to move", "Sword strike: the
  sword button", ...), the lore sign about key combinations, and the How to Play panel, whose
  keyboard lines become the buttons. (A player on a pad turns the touch controls off in the
  options and reads the original.) These texts are the `touch` section of
  `game/data/strings.json`; with the touch controls off, every text is the original's.
- The buttons are Magic Rampage's (Asantee Games), taken from
  its Android package by `tools/art/make_mr_touch_art.py`. The ones it has no equivalent for (down,
  fire, light, the two combos) are made in its style from its parts: its blank button with one of
  its glyphs or rune gems (`game/data/images/touch/README.md` says which is which). The images and
  layout are in `game/data/touch_controls.json` and `game/data/images/touch/`, so new art is a data
  change; `"enabled": false` there removes a button (the combo buttons, say), and `"overhang"` lets a
  control's box lie past the screen edge it hangs from (the direction control's empty lower half
  does, which is what puts its two buttons 25 units under the middle of the action buttons' bottom row). The first,
  placeholder look is kept in `game/data/images/touch/placeholder/` with its own manifest: copy that
  over `game/data/touch_controls.json` to go back to it.

### Adjusting the buttons (E28)

The buttons' size, opacity and places are the player's to change. With the touch controls on, the options screen has an **Adjust controls** button
next to the touch controls' switch (on a phone, a cell of the large layout of E31, beside the switch's cell; under it in the art-less layout). It opens a full-screen editor that shows the real buttons on a dark background, as they are in a level: the two staggered columns of three described above (E29, E32, E33), at the size, opacity and places set.

![The options screen on a 20:9 phone: two columns of settings, the language chooser at the top right, and the Adjust controls button beside the touch controls' switch](images/options-phone.jpg)

*A phone's options screen (E31): Adjust controls is the cell to the right of Enable touch controls.*

![The editor on a 20:9 phone, with the size at 1.2, the opacity at 0.6, the padlock open and some buttons moved](images/touch-editor.jpg)

*A tuned example: size 1.2, opacity 0.6, the padlock open (red) and a few buttons moved. The thin frames show where a finger takes each button.*

| Control | What it does |
|---|---|
| The two tiles of the top row (shrink, enlarge) | Make every button smaller or larger, 0.4 to 1.4 times, a tenth a tap; the number between them shows the size. The pause and back buttons keep their size. |
| The two tiles of the second row (dim, brighten) | Make every button fainter or stronger, 0.2 to 1.8 times the normal look, a fifth a tap. |
| The padlock | The editor opens locked. Unlock it (its tile turns red) and a finger can drag any button, which keeps its place under the finger; a size step or a restore locks it again. |
| The circular arrow | Restore: puts every button back in the default arrangement. The size and the opacity stay. It is dim while nothing is moved, and needs no confirmation. |
| The arrow at the top left, or Esc | Leaves the editor. |

- A tile acts when the finger lifts inside it. A tile at its limit is dim and does nothing. A change shows at once, on the buttons behind the tiles.
- The padlock is never saved; the size, the opacity and the places are: `touchTuning` in `settings.json`
  ([playing.md](playing.md#settings-and-saves)). They apply from the next tick, in levels, arenas and the menus' corner button.
- A button's place is kept as its distance from where the default arrangement puts it (and that as a distance from the corner it hangs from, not as a
  screen position, so it follows a rotation or another window size). A distance is only meaningful against the arrangement it was made from, so
  `touchTuning` also holds a `layout` number (E34), the number of the default arrangement it was saved under, 4 for the one described above (1 is E29's, 2 is E32's, and 3 an intermediate build of E33's that was never in a release). When
  a game's default arrangement has another number, or the saved settings have none (every file written before E34), the saved places are dropped
  and the buttons are in the default places, while the saved size and opacity are kept; the next save writes the current number. Restore does the same by
  hand: it returns to the default arrangement, and a changed default no longer carries a player's moves over.
- `touch_controls.json` stays the base look and layout, and the tuning is laid over it: a button's size is the manifest's `scale` times the tuning's
  size times its own `size`, and the placeholder look is adjusted the same way. With nothing changed the buttons are exactly the manifest's.
- Every button stays inside the screen's safe area, and the pause button below the run's timer. The direction control keeps its overhang below the
  screen's edge, and the down button (E25) moves with it. The back button, in the arena select and on the end screens, never moves.
- Nothing stops a button from covering the HUD or another button, and the circular arrow puts the places back, not the size. With the shipped layout,
  no button is drawn over another in play: the layout is clear up to 1.1, on every screen shape and with a notch, a gesture bar or a tablet's bars
  (E33: E32's level grid held to 1.0, E29's two columns to 1.2, and E16's arrangement to 1.1, or 1.0 with a tall bottom bar). From 1.2 the direction
  control's right button meets the sword button on a 4:3 screen with a notch (138 units apart at 1.0; on a plain 4:3 screen they would meet from 1.5,
  past the editor's top size), and the fire button, the highest of the six, meets the pause button on a 20:9 phone under a 100-pixel bottom bar (counted in
  the 768-tall pixels the layout is written in; counted in a 1080-pixel window's pixels, from 1.3); at 1.3 it does so on a 20:9 phone with a 48-pixel bar as well,
  and on 16:9 and 4:3 tablets with a 100-pixel one (in a window's pixels, on the phone with the 100-pixel bar), and at 1.4 on most other shapes with a bar, a
  notch or a tablet's status bar; the buttons are then moved apart by hand, the editor being a live preview of it.
- The editor is opened by hand from the options screen. For captures, `--touch-editor`, `--touch-tuning` and `--finger`
  ([playing.md](playing.md#command-line)) open it, set a tuning and put a finger on the screen; a run with any of them saves nothing.
