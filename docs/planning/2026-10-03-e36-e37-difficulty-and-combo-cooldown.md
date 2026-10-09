# E36 and E37: Normal and Hard, and a rest for the touch combo buttons

Built 2026-10-03, for release 1.0.5. The player's view is in [`../enhancements.md`](../enhancements.md), [`../playing.md`](../playing.md#difficulty-and-best-times) and
[`../controls.md`](../controls.md). E36 is in the scripts (`game/script/`), the settings and the layer; E37 is in `game/render/TouchControls.*` and the layer's `ApplyTouch`.

## What and why

**E36.** The campaign had one difficulty, the original's. It now has two: Normal is that game, unchanged, and Hard doubles every enemy's hp. The five best times are kept for each, so a
Hard time is not measured against a Normal one. Hard is one factor, `HARD_HP_FACTOR`, applied in one place, `spawn()`. The choice is asked for where a game starts: confirming New Game
opens a prompt with the two difficulties, and the Settings screens have no row for it.

An earlier build of this release put the choice on the Settings screens (a desktop row beside the language chooser and a fifth phone row with 78 and 75 px cells); it moved to New Game
before that build was released. Releases 1.0.5 to 1.0.7 shipped the first design (the Settings row) and this one was committed on 2026-10-09 and went out in the rebuilt files of release 1.0.8 (build 10); the
Settings screens are as they were in 1.0.4.

**E37.** A combo button makes a whole combo in one tap: the sword combo (left, left, S: the stronger sword and the beam, 5 mana) and the spell combo (down, side, D: the blast, a fireball of
225 damage, 25 mana). The heavy attacks came too cheap from a button that fires them in one tap, so each combo button now rests after it. A combo typed on a keyboard or a pad is as it was.

## E36: the numbers

| | |
|---|---|
| Enemy hp, Normal / Hard | warrior 75 / 150, minion 45 / 90, knight 150 / 300, impy 75 / 150, paladin 400 / 800, master knight 1700 / 3400, king 3500 / 7000 (`data.enml`'s `hp`, times `HARD_HP_FACTOR` = 2). The warriors the king summons are `warrior` and so 150 |
| Experience a kill gives | the base hp in both (`expGiven` is read again from `data.enml` and is not doubled) |
| Not touched | the players (bruxo, princess and vert_bruxo never pass through `spawn()`), the potions' hp (what they heal), Versus (an arena's warrior is 75) |
| Setting | `difficulty` in `settings.json`, `"normal"` (default) or `"hard"`, written after `pauseOnFocusLoss`. It is the last difficulty chosen at New Game's prompt: not an option on any screen, only the remembered answer and the prompt's starting highlight. It is written when a row is chosen (the layer saves when the choice differs from the one it holds, so choosing the remembered row writes nothing). Read in any case; anything else is Normal with the warning `difficulty is not "normal" or "hard"; using normal`; a missing key keeps the default. The file's `version` stays 2 (nothing reads it) |
| Flag | `--difficulty normal\|hard`: this run's starting highlight for the prompt and the difficulty a `--start` campaign level plays at, over the setting; never saved, and a different row chosen at the prompt replaces it, as a pick replaces `--lang`. Any other value prints an error and exits. It removes the intro like every flag outside a player's own (E35) |
| The prompt | Opened by the confirm on New Game on the main menu (Enter, a click, a tap, a gamepad's Start or A), which starts nothing. A panel centred over the menu, dimmed by a black rectangle of alpha 150 over the whole logical screen, titled "Choose difficulty" (Portuguese "Escolha a dificuldade"), with two rows, Normal ("Enemies as in the original game.") and Hard ("Enemies have twice the health."), each a name and a one-line description. The lit row is the last difficulty chosen, Normal the first time |
| Prompt input | Up and Down (the arrow keys, a gamepad's D-pad or stick) move the lit row, stopping at the ends; a pointer that moves onto a row lights it, and one at rest leaves the keys alone. Enter or the gamepad's confirm starts at the lit row, wherever the pointer is. A click or a tap on a row chooses that row and starts at once (a fresh left or right press: a tap is the left button held for the tick the cursor jumps to it). Esc, a gamepad's Back, or a click or tap outside the panel closes it and starts nothing; only a click closes by position, and a click inside the panel on no row does nothing. Pointer clicks (a left or right press, a tap) are ignored for the first 350 ms after the prompt opens (`kPromptSettleTime`): the second click of a double-click on New Game lands on its box (the cursor entity's collision box is 31 px square, so New Game answers for pointer y 185 to 241), which is outside the desktop panel and overlaps the first row on a phone; Esc and Enter are not delayed. While it is open the menu behind it does not react (the hover test is skipped: no panels, sounds or buttons), the direction keys move the highlight, not the pointer, and the system pointer is shown (`HideCursor(false)`), because the pointer the menu draws is a world particle under every HUD command. The frame that opens it only draws it, so its confirm cannot also choose. A change of row plays `soundfx/help.mp3`, the menu's hover sound, once; a start plays `soundfx/newgame.mp3` and the 3-second fade-out, as the old confirm did |
| Prompt geometry | 560 x 330 logical px, centred on the 1024x768 screen (the widescreen sides are art only), so its top is at y 219. Inside it a 24 px inset: the title (Arial Narrow 40, one 512 px line, 20 px down) and two rows of 512 x 100, 10 px apart, at 90 and 200 from the panel's top (centres (512, 359) and (512, 469) on the desktop), each with a name (34) and a description (22) in a text box of 480 px that starts 22 px in, after the lit row's 6 px bar. The lit row has the brighter fill, its name at alpha 255 and its description at 200; the other row's texts are at 100 and 75. One function, `difficultyPromptBox()`, gives every part, for the drawing, the hit tests and the suites, so that they cannot disagree. A right-to-left language sets each text against the right edge of its box |
| Prompt on a phone | A phone's larger menu (`g_phonePanel`) shows only part of the screen. The panel is centred in the rectangle it shows (its right edge is the menu panel's text box's plus 10, which is the safe area's), then slid left where that would put its right edge past x 1022 or right where its left edge would be under x 2 (the HUD carries a rectangle that meets the logical screen's edge out to the window's side, so a panel touching x 1024 would grow a dark band across a 21:9 phone; 2520x1080 and 2640x1080 slide by 23 and 55 logical px, 2400x1080 and the 2992x1344 phone not at all), and panel, rows and texts are scaled together by one factor: the largest that leaves 8 percent of the rectangle free at every side, at most `g_phonePanel.maxScale`, which is 1.6 times the size E1's view draws the menu's text. A rectangle too small for even `minScale` keeps the fit: a panel that overflows the window is worse than a small one. A phone's main menu has no Back button, so a tap outside the panel is the way out |
| Settings screens | As in 1.0.4: no Difficulty row on the desktop screen and none in E31's phone layout, whose cells are 88 px tall again (held to 68 to 88). `PC_DIFFICULTY` and the row's `Switch` labels are gone; the `Switch` that stays, `g_difficulty`, is drawn nowhere |
| Best Times panel | Normal's five under "Normal", a blank line, Hard's five under "Hard": 13 lines (14 with the break that ends them) of the 27 the panel's body holds. One pattern, so that a phone's two-column split can only fall between the lists |
| New Game panel | the original's story alone (`menu.as:186`), as before: hovering New Game shows it and nothing else |
| End screen | "Best times (Normal):" or "Best times (Hard):" and the played difficulty's list only: both lists would run off the 768 px screen |
| Texts | three new strings (the prompt's title and the two descriptions), the rows' names, Normal and Hard (the best-times headings and the end screen's heading use the same two), and two new patterns (the end screen's heading and the Best Times body) in all eleven languages (the Portuguese is the port's own, English in `strings.json`, the other nine in `strings/<id>.json`), each with a room in `tests/data/l10n_rooms.json`: the title one line of 512 px at size 40, the names 480 px at 34 and the descriptions 480 px at 22, which hold at every scale because the prompt is scaled as a whole. `strings.json` has 103 strings and 19 patterns (11 translatable). The Japanese face was regenerated by `tools/l10n/make_fonts.py`: four new kanji against 1.0.4 (933 to 937 glyphs, 161,968 to 163,040 bytes), two of them, 倍 and 択, for the prompt's texts |

`hs.enml` once a record has been written (the shipped file holds only the first block, with five times of 3599000):

```
hs
{
    hs0 = 3599000;
    hs1 = 3599000;
    hs2 = 3599000;
    hs3 = 3599000;
    hs4 = 3599000;
}
hsHard
{
    hs0 = 3599000;
    ...
}
```

The original's entity `hs` (ascending milliseconds, 3599000 ms being 59:59) stays Normal's list, so a file written by any earlier version is read as Normal's, untouched.
Hard's list is a second entity of the same file, `hsHard`, with the same keys `hs0` to `hs4`, ascending in milliseconds. The first record written creates both; a Normal time leaves Hard's block
byte for byte as it was, and the reverse (the file is read whole, both lists are held, and both are written back; the writer sorts the entities, `hs` before `hsHard`).

## E36: how it runs

- `Script::g_difficulty` (a `Switch` in `videoModes.cpp`, row 0 Normal, labels "Normal" and "Difícil", drawn nowhere) is a plain holder of the last choice. The layer sets it when it attaches, from the
  setting or from `--difficulty` over it, and every tick keeps the settings in step with a change (the override is dropped and the setting saved at once, as the other rows do). The only thing that
  writes it afterwards is a row chosen at the prompt, never a hover or a cancel, so a change seen by the layer is always a choice.
- The prompt is in `menu.cpp`. The cursor entity carries its state as custom data: `pickDifficulty` (the tick it opened; its presence means it is open), `pickRow` (the lit row), and `pickX` and `pickY`
  (where the pointer was last seen, so that a pointer at rest does not fight the keys). The confirm on `novo_jogo` (`menu.as:288`) sets them where it used to start the game; `difficultyPrompt` runs
  last in the cursor callback, so that it draws over the panels. A chosen row sets `g_difficulty` and does what the confirm did (`newGame` data, `"CAMPAIGN"`, the sample; `menu.as:293-295`) and
  erases the four data. `difficultyPromptBox`, `drawDifficultyPrompt` and `difficultyPrompt` are the three parts.
- `newGame("CAMPAIGN")` (`main.as:99`) latches `g_runDifficulty` from the holder when the menu's fade ends, 3 seconds after the row was chosen; nothing writes the holder in between.
  `resetData()` puts it back to Normal, which is what keeps Versus Normal (the arena select and an arena's `newGame` both pass through it). A death and a checkpoint reload a scene without `resetData`,
  and a `next_level` door loads the next scene the same way, so the run keeps its difficulty. The layer's developer start (`--start level1`) latches it the same way, for a campaign level, from what
  the prompt would open on.
- `spawn()` (`setupScene.cpp`) gives each enemy its stats: the spawn markers (`doLoop`), the king (`event01`) and the warriors he summons (`controlCharacters.as:637`) all pass through it, so
  the doubling is in that one place. A checkpoint file holds an enemy at the hp it had (150 for a Hard warrior) and spawns nothing, so a reload from a checkpoint is not doubled again.
  The `TestHpOwners` check makes the premise a test: only bruxo, princess, vert_bruxo and the two potions define an `hp`, and every scene placement that carries one is a potion (54 of them).
- `scores.cpp`: `addNewRecordTime(elapsed, difficulty)`, `getRecordTimeList(difficulty)` and `getGetBestTime(difficulty)` are new; the original's three signatures remain, for Normal, and
  `getGetBestTime()` is the better of the two lists' first times. `readRecord` starts each key at `DEFAULT_RECORD_TIME` (3599000). The first port of this file let a missing key repeat the previous
  one (its comment says the original's script read it as 0): either would be wrong for Hard's list in a file written before E36, where the whole entity is missing, and 0 would unlock every
  locked arena.
- The arenas: `menu.as:318-326` keeps an arena locked while its `score` is at most the best time, so it opens for a time strictly below it. With `getGetBestTime()` the better of the two lists, a
  finish in either difficulty opens it, at the same thresholds (Templo Sagrado 720000 ms, Neblina 900000 ms). With both lists at 59:59, as shipped, both stay locked.
- `extracted/app/hs.enml` is never written: `GetAbsolutePath("hs.enml")` redirects a write to the user's data directory, as before.

## E37: the numbers

| | |
|---|---|
| Rest | `kComboCooldownTicks` = 60 game ticks (1 s at 60 Hz) per button, set to 60 on the tick the macro presses its attack key (S or D) and counted down by `ObserveFrame`, once for each tick the game runs. A tap on the k-th tick after the attack key is refused for k = 1 to 59 and takes at k = 60 |
| Macros | the sword combo is five ticks (none, side, none, side, S), the spell combo four (none, down, side, D); the attack key is the last. A tap taken at k = 60 therefore puts two attacks of one button at least **64** (sword) or **63** (spell) ticks apart, more if the macro waits for the combo memory (up to 30 ticks) |
| The buttons | each has its own rest (`TouchComboSlot`); one macro runs at a time, as before |
| Refused | a finger landing on a resting button, or on either combo button while a macro runs (`TouchStep::comboDenied`, by button). Nothing starts and nothing is queued; a second finger landing on the button that a first started the same tick is one press, not a refusal. Only a finger's landing is a tap, so one held through the end of the rest fires nothing |
| Flash | `kComboDeniedFlashTicks` = 15: the art tinted from red (255, 60, 60) back to white, brightened to the pressed alpha, and shaken left and right every two ticks by up to 5 manifest px (scaled like the button), less as it fades |
| Shade | while resting the art is grey (150) and black covers the top `ticks left / 60` of the box at 0.75 of the pressed alpha (so it follows the player's opacity): the button's own art again, black and cut to the same top share of the image, so the rounded corners stay clear of it; a plain rectangle where the art's size is not known (`ProbeImageSize` reads the header). It is gone on the tick the button takes a tap |
| Glow | `kComboReadyPulseTicks` = 12: the alpha runs from the idle value to the pressed one over the 12 ticks after the rest ends |
| Cue | `soundfx/fail.ogg` (the sample the original's menu plays for a locked arena; 1.29 s), at most once in `kComboDeniedSoundGapTicks` = 10 game ticks (a mashed button gives the cues at ticks 1, 11, 21, 31, 41, 51 of the rest) and not restarted while it still sounds. `PlayComboDenied` loads it on first use, since a scene load releases the samples; the log line is `E37 combo refused a tap (<sword, spell or both buttons>): the cue plays`, followed by the tick |
| Cleared | by a scene load (`TouchInput::sceneSerial`: the next level, a death's reload, leaving to a menu), never by the pause or the editor, which would be a way round the rest. A macro cancelled before its attack key (the pause, a menu, the controls switched off, a scene load, the wait for the combo memory given up after 30 ticks) starts none |
| Frozen | by the pause and the touch editor, which run `Update` alone: the clock is `ObserveFrame`'s, called once per game tick after `Update`, so an extra `Update` cannot count a tick twice |
| Not limited | combos typed on a keyboard or a pad, and combos made of the ordinary touch buttons (walk plus sword or fire): `playerInput` and the combo buffer are untouched, and nothing in `TouchControls` stands between those keys and them |

The look and the sound are in play only: the editor's preview draws the buttons at rest and plays nothing. There are no new words.

## Files

E36: `game/render/Settings.{hpp,cpp}`, `game/PenumbraLayer.{hpp,cpp}`, `game/main.cpp`, `game/script/{Script.hpp,main.cpp,setupScene.cpp,scores.cpp,menu.cpp,videoModes.cpp}`
(`optionsPhone.cpp` is as it was in 1.0.4), `game/data/strings.json` and `game/data/strings/*.json`, `game/data/fonts/NotoSansJP-Bold.ttf` with its README, `tests/data/l10n_rooms.json`.
E37: `game/render/TouchControls.{hpp,cpp}`, `game/PenumbraLayer.{hpp,cpp}`.
Tests: `test_pn_boot` (`TestDifficultyStats`, `TestHardRun`, `TestRecords`), `test_pn_scenarios` (scenario 27, Hard played through the prompt; scenario 28, the prompt: it opens and starts nothing, the
remembered highlight, Up, Down and Enter, a click and a tap on each row, Esc, a click outside the panel, a click inside it on no row, Alt+Enter, a phone's geometry, the pointer not drifting under the keys,
the fade, and `g_runDifficulty` latched when it ends; the other scenarios that start the campaign from the menu go through the prompt; scenario 8's end screen), `test_pn_formats` (`TestHpOwners`),
`test_pn_render_pause` (the setting, the flag), `test_pn_render_input` (the setting's default and round trip; the phone layout pinned at E31's 88 px cells), `test_pn_render_hud` (the layer's seeding of
the difficulty, the texts and their rooms), `test_pn_render_touch` (`testComboCooldownGate`, `...Fingers`, `...Reset`, `...Look`, `testComboTypedUnlimited`).

## Checked

- Linux: `test_pn_all` runs 17 suites and 47,580 checks, 0 failures. On Windows, with the real Arial Narrow installed, the first full run of the previous build passed 15 of the 17 suites (the two others: one Versus check of a new test, fixed afterwards, and the 18 failures below); this build's Windows executables were built with zero warnings and are covered by GitHub's Windows CI job, and Windows Smart App Control refused two earlier launches on the development machine, so no Windows launch is claimed. E25's panels check in `test_pn_render_hud` has 18 known failures there, older than this release and described under E28 in `DEVLOG.md`; they
  pass on Linux with the stand-in fonts.
- Headless captures on Linux (lavapipe) at 1280x720 and 1024x768: a combo button idle, pressed, dark right after it fired, recharging, flashing red on a refused tap at ticks 90
  and 150 and not on the accepted tap at tick 135, and glowing when ready; the Best Times panel.

## Open

- **No real phone.** Nothing of 1.0.5 has run on one, so the length of the rest, the flash and the cue are untried under a thumb, and so is a tap's exact landing on a row of the difficulty prompt.
- **The Windows `Penumbra.exe` was not launched on the machine it was built on**: Windows Smart App Control refused two earlier launches on the development machine, as it can for any unsigned build. The Windows build is covered
  by the suites that ran there, by GitHub's Windows CI job and by the Linux captures. The Mac, iPhone and iPad builds are made by CI only.
- **Every New Game asks.** Starting takes two presses (Enter and Enter, or two taps), even for a player who always plays the same difficulty; the prompt opens on the last choice.
- **A tap on the other combo button while a macro runs is refused too**, with the flash and the cue, not ignored: the tap lands on a button that cannot act on it, and the button says so. The macro is a few ticks long, so this is seen only for a tap within those ticks.
- **A Hard kill gives the Normal experience** (the base hp), so a Hard run levels up more slowly per hit. It is left as `data.enml` states it.
- **A Hard finish opens an arena at the Normal thresholds.** The better of the two best times is compared with each arena's score.
- **An older build that writes a record drops Hard's list**: it clears the file and writes `hs` alone. The newer build reads that file as Normal's with Hard's at 59:59.
- **A phone's main menu has no Back button**, so a tap outside the prompt's panel is the only way out of it besides choosing a row; the menu's buttons are not tested while the prompt is open, including on the frame that closes it.
- **A combo button rests after its attack key even when the script refuses the move for lack of mana** (the sword combo needs 5 mana, the spell combo 25): the cooldown belongs to the touch layer, which does not
  know the mana, and a retry within the second is refused with the flash and the cue.
