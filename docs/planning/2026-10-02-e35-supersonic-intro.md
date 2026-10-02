# E35: the Supersonic Engine's intro

Built 2026-10-02, on the layer that runs the Ethanon frame (`game/PenumbraLayer.*`). The player's view is in
[`../enhancements.md`](../enhancements.md) and [`../playing.md`](../playing.md); the code is `game/render/Splash.hpp` and `Splash.cpp` (pure) and the
layer's `StartSplash`, `StepSplash`, `EndSplash` and `DrawSplash`; the picture is `game/data/images/splash/supersonic-logo.png` (its README says how it was made).

## What and why

A normal start shows the logo of the Supersonic Engine, which this edition runs on, for two seconds, and then the menu. The original showed nothing: the
window opened on the menu's own fade from black. The intro is the edition's one place to say what it is built on, and it costs two seconds that a key, a
click or a tap shortens.

## The numbers

| | |
|---|---|
| Timeline | 120 engine ticks (2.0 s at 60 Hz): fade in 24, hold 72, fade out 24. Smoothstep ramps: smooth, monotone, the largest step 1.5/24 = 0.0625 of the alpha a tick |
| Skip | A press (any key, a mouse button, a finger, a gamepad button going down; never one already held) from tick 18 on fades the logo out from the alpha it has, over 24 ticks, so the intro ends at tick 42 at the earliest and at tick 119 at the latest. A press at ticks 0 to 17 is ignored, and so is one once the timeline's own fade-out has begun (tick 96 on) |
| Picture | A flat ground, `#14171C`, over the whole window, and the logo (2258 x 640) over it, centred in the display's safe area, 70% of the window's width, at most half its height, inside the safe area, at whole pixels. Quads in window fractions on the engine's `ScreenOverlay`, which is drawn over the finished image in display values |
| Asset | `engine/assets/branding/supersonic-logo.svg` at 2x, from an edited copy (the folder's README has the script): 2258 x 640 RGB, 91,721 bytes, the ground `#14171C` on the whole border, the lockup centred, no `#FF00FF`. MIT-0 |

The ground is the panel's own fill, not black: black would show the picture as a box. The lockup (the mark, SUPERSONIC, ENGINE and the rule: every pixel more than 30
levels off the ground) is centred in the picture to a pixel, with 160 and 161 px of ground to its left and right and 143 above and below, so centring the picture
centres what is seen. Measured on headless captures at tick 60 (Linux, lavapipe), where the lockup's centre is within 1.5 px of the safe area's, and every pixel
outside the picture's rectangle and on its border is exactly the ground:

| Window (safe area) | Picture on screen | Lockup, x | Lockup, y | Lockup's centre | Area's centre |
|---|---|---|---|---|---|
| 2992 x 1344 (199, 0, 0, 0) | 2094 x 594 at (549, 375) | 698 to 2492 | 508 to 835 | 1595.5 | 1595.5 |
| 1280 x 720 | 896 x 254 at (192, 233) | 256 to 1020 | 290 to 429 | 638.5 | 640 |
| 1024 x 768 | 717 x 203 at (154, 283) | 205 to 815 | 329 to 439 | 510.5 | 512 |
| 1080 x 2400 | 756 x 214 at (162, 1093) | 216 to 861 | 1141 to 1258 | 539 | 540 |

## How it runs

- While the intro runs, `OnFixedUpdate` does not step the Ethanon machine. There is no `Machine::Frame`, so no scene is loaded (`ScriptMain`'s `LoadScene` is
  only queued, and frame 1 serves it), no music is started (`menuPreLoop` starts it), `GetTime()` (the frame count) stays 0, no random number is drawn, and
  the layer's tick counter, which `--hold`, `--finger` and `--tour` count from, does not advance. The 121st tick is the game's first, and its frame is
  the one a layer without the intro runs at once.
- Each intro tick still builds the tick's input (`InputMapper::BuildTick`), so the mapper's latches and its cursor go on as under the pause, and watches the raw
  devices for a press (`SplashPressWatch`: the inputs held now that were not held at the previous look; the first look only records). A press in a frame that ran no
  tick is kept for the next tick.
- `OnUpdate` keeps its prelude (the window's size, the view) and then, instead of the world and the HUD, adds the intro's quads to the overlay. The system pointer is
  hidden for the intro.
- When the clock is done, the next tick drops it and, if anything is down or was just pressed, arms `PauseMenu::HoldPressed`, as the touch editor does on closing: the game's
  first tick holds up whatever is down, which it has never seen, until it is released. The press that skipped the intro, a click, a finger or a button, is therefore not a
  confirm on the menu's first frame, and the next press is the menu's. A finger is the menu's pointer through the same key. With the devices at rest nothing is armed, so a
  `--hold` key that begins at the game's tick 0 is not held back.
- A logo that cannot be read (a data folder without `images/splash/`) means no intro, with a line in the log; it is never a blank screen for two seconds.
- Two log lines: `Supersonic intro: started` and `Supersonic intro: finished at tick N (skipped|ran out)`, N being the intro's ticks.

## Who plays it

`Penumbra::Render::SplashWanted` reads the command line without the program's name. The intro plays unless a flag outside a list of the player's own is given.
That list is `--original --data --window --fullscreen --windowed --lang --widescreen --smooth --touch --zoom --edge-margin --refresh --safe-area --splash`.
The platform glue passes `--original` and `--data` (Android, Mac and iPhone/iPad; the Windows and Linux downloads pass nothing), so a normal start of each plays it.
Any other `--` flag, the engine's (`--frames`, `--screenshot`, `--screenshot-every`, `--fixed-step`, `--record`, `--replay`, `--scene`, `--import-assets`) and the
developer's (`--start`, `--tour`, `--hold`, `--cursor`, `--pointer`, `--spawn`, `--princess`, `--hp`, `--modes`, `--mobile-layout`, `--touch-tuning`, `--touch-editor`,
`--finger`), and any added later, turns it off, so no capture, suite or replay changes without being edited. `--splash on` plays it whatever else is given (a capture of the
intro: `--splash on --fixed-step --frames 30 --screenshot <path>`), `--splash off` removes it, and the last of them decides. `PenumbraLayer::Options::splash` is false unless
`main.cpp` sets it, so a layer built by a suite runs from its first tick as before. There is no settings row.

## Checked

- `test_pn_render_pause` (pure): the timeline, every skip tick from 0 to 119, the press watch for every key, button, finger and pad, the layout in 14 window shapes with 6
  insets, the quads, the logo's file (size, border, no magenta, opaque, the lockup's place and its centring, nothing else in the picture), the flags (every one above, alone and among a normal start's, `--splash` over them).
- `test_pn_render_hud` (the real layer on a bare registry): the machine stands still for 120 ticks and runs its first frame on the 121st with the sprites, texts, time and
  random numbers of a layer without the intro; the overlay holds the ground and the logo at the timeline's alpha and nothing else; a press at tick 17, even held on, is ignored,
  one at 30 ends it at 54; an Enter, a mouse button and a finger held across the end are not seen by the menu until released and the next press is; a press that goes down and up
  inside a frame that runs no tick (a display faster than 60 Hz) is the press of the tick it is handed to, at ticks 17, 18, 50, 95 and 96 the same as one a tick sees, and a tap
  between the intro's last tick and the game's first, in one such frame or in two, never reaches the menu; `Machine::FrameSeconds()` is 0 on the game's first tick (the one after
  the scene load) and a tick's length on the second, with the intro and without; no intro without the option or without the logo.
- Mutations (a copy of the source in a temporary folder, restored and compared byte for byte), each failing the suite that holds it: a skip threshold of 0, `--splash off` ignored, the press let
  through to the menu, the machine stepped during the intro, developer flags allowed, a skip that cuts instead of fading, the hold armed with nothing down, the tick counter
  advancing during the intro, a frame that runs no tick not watching for a press (which `pressAt` in `test_pn_render_hud` catches), `EndSplash` without the press kept from such a
  frame (which the tap over two frames with no tick catches), and the first tick after a scene load given a tick's length (which the `FrameSeconds` checks catch).
- Headless captures on Linux with the old and the new build: the menu and level 1 at several sizes, byte for byte the same (SHA-256); and `--splash on --frames 125` (the intro
  and then the menu's fifth frame) byte for byte the same as the old `--frames 5`.

## Open

- The menu's scene loads, and its textures decode, on the first tick after the intro, not behind it, so a slow machine may pause there. The Ethanon machine is not stepped during
  the intro by design, and nothing else could do the load.
- Alt+Enter during the intro counts as a press and ends it; fullscreen is the menu's (the scripts switch it), so it is one more press after.
- The logo is a 2x image, uploaded by the engine with a mip chain. At 2992 x 1344 it is shown at 93% of its size and at 1280 x 720 at 40% (a 2.5 times minification), where a
  capture read at full size shows no visible aliasing; the four shapes of the table above were each read at full size at tick 60.
- A start with no flags now opens on the intro, so a script that drives the menu by input in the first two seconds meets it: on a device, `tools/build_android.sh --run "<flags>"`
  leaves the flags in `penumbra_args.txt` for the next launch (`game/android/AndroidMain.cpp`; the iPhone/iPad glue reads the same file name from the app's Documents folder), and
  `--run "--splash off"` removes the intro for that launch. A launch with any developer flag (`--start`, `--screenshot`...) has none already. `--run ""` starts a normal one, with it.
