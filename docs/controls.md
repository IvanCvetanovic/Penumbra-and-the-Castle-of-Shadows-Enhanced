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

**Combos.** Each gap between presses must be at most about 210 ms (`combo.as`;
[`spec/11-logic-player-combat.md`](spec/11-logic-player-combat.md) §10):

- **Sword combo (5 mana):** ← ← S or → → S. A stronger sword and a beam; the screen shakes.
- **Blast (25 mana):** ↓ ← D or ↓ → D. A big fireball with 225 damage.

**The original's hidden keys.** When the menu's fade ends on New Game:

- Hold 2 or 3 to start at level 2 or level 3.
- Hold Page Up to start both characters at level 15 (`main.as`).

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
| Walk / down | The three buttons at the bottom left (left, right, down): one control, the thumb slides between them | Left, Right, Down |
| Jump | Bottom button of the four at the bottom right | Ctrl |
| Sword | Left button | S |
| Fireball | Right button | D |
| Light spell | Top button | Space |
| Sword combo | The left button of the two above the four | the way he faces twice, then S |
| Spell combo | The right button of the two above the four | Down, the way he faces, then D |
| Pause (in a level) / back (arena select, game over, the end screens) | The button at the top right | Esc |

- Several fingers work at once: hold a direction and tap the buttons.
- Two players on one phone (E22): player 1 plays on the touchscreen and player 2 on a Bluetooth
  gamepad. Versus opens once a pad is connected (until then its entry says "Connect a gamepad for
  player 2"), and in the campaign the pad's Start summons the princess. The pad never moves the
  wizard, and it does not drive the menus: player 1 taps them.
- A combo button presses its combo's keys one per tick, toward the way the wizard faces. If a
  direction, the sword or the fireball was pressed in the last fifth of a second, it first waits
  until the game's combo memory has emptied (the original forgets a combo 210 ms after its last
  key), so the combo always registers. While it runs, the disc, the sword and the fireball buttons
  wait; jump, light and pause do not. A second tap is ignored until it is done; the pause or a new
  scene stops it.
- In the menus, the options, game over and the pause, the buttons are hidden and a tap clicks
  where it lands. The options screen has no back button at the top right: the original's own Back
  arrow is on it, and a tap on it goes back. The main menu and the pause have none either.
- The focus brackets show on the direction button the thumb holds, and nowhere while no direction
  is held (`"atRest"` in the manifest's `knob`; the placeholder look keeps its knob at the centre).
- While the touch controls are on, the game's control hints speak of the buttons, in both
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
  change; `"enabled": false` there removes a button (the combo buttons, say). The first,
  placeholder look is kept in `game/data/images/touch/placeholder/` with its own manifest: copy that
  over `game/data/touch_controls.json` to go back to it.
