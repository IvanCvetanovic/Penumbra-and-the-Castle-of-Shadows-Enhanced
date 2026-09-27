# Penumbra on Supersonic — the port's step record

Started 2026-09-27. Ivan asked for Penumbra e o Castelo das Sombras to be remade on his own engine,
in this repository, with the engine improved where the game needs it.

## Rulings (Ivan, 2026-09-27)

- **R1 Enhanced from the start.** The original is the base; visuals, controls, resolution and balance
  may be modernised freely. The scripts remain the gameplay spec.
- **R2 Portuguese + English**, selectable in the game.
- **R3 The original may be run** (from a scratch copy) for reference captures.
- **R4 Assets are read in place** from `extracted/app` (committed; the repository is private). No
  converted copies are committed.
- **R5 Commits as Ivan, no AI trailers**, here and in the engine; checkpoints at Claude's discretion.
- **R6 A second Claude session (Magic Portals) also edits Supersonic.** Engine changes are additive,
  made from this repo's `engine/`, rebased before push, announced to that session. Agreed with it on
  2026-09-27: keep existing enum values and their order (append new ones).

## Architecture (decided 2026-09-27, see CLAUDE.md)

- `game/eth` — the Ethanon 0.7.12 runtime the scripts ran on, emulated game-side: files, buckets,
  custom data, callbacks, the frame order, particles, input, samples. One Ethanon frame per 60 Hz
  engine tick. Produces a `RenderSnapshot` per frame.
- `game/script` — the 25 `.as` files ported to C++ close to line by line against `eth/Eth.hpp`.
- `game/render` + `PenumbraLayer` — the snapshot drawn with the engine: sprite quads, Light2D with
  normal maps, projected shadows, particles as pooled quads, HUD via ScreenOverlay, TrueType text.
- Alternatives rejected: embedding AngelScript and running the original scripts (exact, but no room
  for the enhancements Ivan asked for and no C++ to test); a data-only rewrite in the Magic Portals
  style (the scripts ARE the spec here, so re-deriving them would only add error).

## Enhancements (planned; each switchable where it changes gameplay)

| # | Enhancement | Original |
|---|---|---|
| E1 | Widescreen: logical view 768 px tall, width by aspect; menus pillarboxed at 1024x768 | 1024x768 only |
| E2 | Any window size / fullscreen, rendered at native resolution | 1024x768 or listed video modes |
| E3 | Modern gamepads (XInput via GLFW) mapped by meaning, for both players | winmm button numbers |
| E4 | Keyboard second player (presented to the scripts as joystick 1) | P2 needs a joystick |
| E5 | English text alongside Portuguese, switchable | Portuguese only |
| E6 | Settings persisted (language, window, volumes, controls) | options reset every launch |
| E7 | Original bugs fixed where they are plainly bugs (listed per step) | — |
| E12 | One gamepad plays the wizard and drives the menu; a second plays the princess (firstPadIsPlayer1) | the first pad was player 2's |
| E11 | Standing on two floor tiles at once counts as standing (kFloorSeamFix) | the last tile decided; one airborne frame at each seam |
| E8 | Smooth motion: the world (sprites, lights and halos, shadows, particles, camera) drawn between the last two ticks by SimulationClock::alpha, one tick behind; whole-pixel ends stay on whole pixels; never across a scene load, a frame gap or a jump over 64 px; settings.smoothMotion (on) and --smooth on\|off, off under --fixed-step | one tick per 60 Hz vsync |
| E9 | Shadows drawn live, their visible end at the light's reach | static shadows baked into lightmaps at 8x the caster's height |
| E10 | Enhanced settings on the original's options screen (videoModes.as): keyboard player 2, widescreen/4:3 levels, Português/English at once, music and effects volume in 10% steps (a new Stepper widget beside Switch), saved to settings.json | the screen offered only the video-mode list, pixel shaders, window/fullscreen and the joystick layout, all forgotten at exit |

## Steps

### Step 0 — scaffold (done 2026-09-27)
Engine submodule at 4bfcf67; CMake (engine subproject, game/eth, game/script, PenumbraGame, tests);
tools/build.bat; CLAUDE.md; docs/spec (eight mapping reports + synthesis); reference/ (gitignored
Ethanon 0.7.12 source, GS2D r485, disassembly, scripts); the Eth contract headers.

### Step 1 — the Eth runtime (done 2026-09-27, 9c64f8a)
game/eth implemented from reference/eth-0.7.12: TinyXML legacy readers/writer (all 13 scenes rewrite
byte-exact), ENML (enml.h's missing-key rule), buckets, typed custom data, soft delete, callbacks by
name, the 0.7.12 frame order, the HUD queue, drawHash order, ETHParticleManager, input state
machines, the sample bank, the snapshot. Gates: test_pn_runtime 277, test_pn_formats 589,
test_pn_particles 81 checks, 0 failures.

### Step 2 — the scripts (done 2026-09-27, 9c64f8a)
All 25 .as files ported to C++ (game/script), each adversarially reviewed against its .as: one
discrepancy found (interface.as's float divisions must abort on 0, as AngelScript's asBC_DIVf did).
Gate: test_pn_boot 46 checks - the real game boots headless, menu -> level1, the wizard spawns,
walks 150 px/s, jumps 111 px for 50 ticks, his sword lives 20 ticks, 0 script aborts.
- **E7 (first bug fix):** data.enml has no lv20, and the original read the thresholds directly, so a
  player skipped level 20 and, past lv30, addToExp never returned. expForLevel (Script.hpp) falls back
  to the last level that has a threshold; drawPlayerStatus uses it too.

### Step 3 — drawing it (done 2026-09-27)
game/render: TextureCache (colour keys, DDS, renormalised normal maps, additive/modulate variants),
DrawOrder (one painter's order from 0.7.12's depths), CameraRig (ortho camera, DisplayEncoded,
pillarbox), SpriteRenderer, Lighting + LightRenderer (every light live through Light2D, halos,
torch flicker), ShadowRenderer (the projected strips of dynaShadowVS), ParticleRenderer, FontAtlas
(system TrueType faces, bold, GDI cell height), HudRenderer, Localization + game/data/strings.json
(English), InputMapper (gamepads by meaning, keyboard player 2), AudioOutEngine, Settings. Engine
abb9e8a: Ogg Vorbis (e955663 pins it). PenumbraLayer wires them; main.cpp takes --start, --hold,
--lang, --widescreen.
Gates: build zero warnings; test_pn_render_textures 144, _particles 137, _hud 258, _input 166,
test_pn_audio 177 (all 35 sounds decode), test_pn_render_lights 132 checks with 3 failures (shadow
edge cases on a bare registry; open). First headless level1 frame compared with
reference/captures/05_level1_after14s.png: walls, torches, crystals, HUD and English text in place.
- **Smart App Control.** Freshly linked executables with no version resource were refused at random
  (Penumbra.exe twice in a row, two suites). With game/windows' icon, VERSIONINFO and manifest on
  every executable (penumbra_windows_resources), the next launches of all of them were accepted.

### Step 4 — the whole game, played headless (done 2026-09-27)
tests/test_pn_scenarios.cpp drives the real ported game through ten scenarios on the Eth Machine -
the menu (a cursor on New Game and Enter), melee (warrior chase, hits, sword kills, XP to both
players, 180-tick fade), the impy (fireball /5 on a fire-resistant target), a potion, lava shooters
and instant death, a checkpoint (saved to the USER dir, reloaded on death, lives once), deaths to game
over (12 deaths from 13 lives), level1 -> level2 through next_level, level 3's falling bridge and the
king (event01, chefao.mp3, the summon; his death writes the record to hs.enml), and PvP (500 hp,
points, 3 to win) - with only teleports, marker moves and direct hp writes as shortcuts. 458 checks,
0 failures, 0 script aborts, and no port bug found.

### Step 5 — English art, fullscreen, shadows (done 2026-09-27)
- **E5 art.** The words baked into images have English variants in game/data/images/en, generated by
  tools/art/make_english_art.py from the originals and swapped at run time by
  Localization::ImageVariant (SpriteRenderer albedo and normal map, Lighting, HudRenderer): the menu
  labels (menu_buttons + its normal and gloss maps; Courier New fitted to the original, the glow and
  emboss re-derived by the measured relation), the logo subtitle ('and the Castle of Shadows'),
  'Voltar' -> 'Back' (arrow_button) and 'Jogador 1/2' -> 'Player 1/2' (input_options1/2). The logo
  subtitle and 'Back' use Matura MT Script Capitals in place of the original's hand-cut uncial.
  **Ruling (R4 exception):** these generated variants carry some original pixels (the Penumbra letters,
  the pad art, the arrow body) and are committed; they are new art the English game needs, in a
  private repository that already holds every original asset.
- **E2 window (engine a516b7c, WindowControl).** Alt+Enter and the options screen's switch really go
  fullscreen - borderless, at the monitor's current mode - and back; settings.fullscreen persists it;
  --fullscreen/--windowed for one run. The options screen lists each 32-bit size once (0.7.12 repeated
  one line per refresh rate) and picking one resizes the window rather than switching the display
  mode. The OS pointer is hidden while the scripts hide it (main.as:133), as 0.7.12 did while it drew
  cursor.ent.
- **E9 shadows drawn live to the light's reach.** 0.7.12 baked static shadows into lightmaps at 8x the
  caster's height, removing only that light's contribution; drawn live over everything, 8x would darken
  far past the light. The strip's visible end (shadow.dds's v = 5/32) now ends at the light's reach.
  Modelled error against the bake along six menu barrels 0.017-0.059, the old apex cap 0.075-0.123.
  Level 1 under the first torch: mean |original - port| 3.0 -> 2.0 levels.
- **Dev flags** no longer leak: --lang and --widescreen are run-only (never saved), --window implies a
  windowed run, --cursor x,y pins the scripts' cursor for menu captures.
Gates: 13 suites, 2593 checks, 0 failures (test_pn_render_lights 145, test_pn_render_english 113,
test_pn_scenarios 458); menu in English and Portuguese, level 1, fullscreen 1920x1200 and both
Alt+Enter directions captured, all exit 0 with validation active.

### Step 6 — standing sprites, highlights, smooth motion, the options screen, a package (done 2026-09-27)
- **Vertical sprites and gloss highlights (engine b999491: f30df7c + b999491, pushed after the Magic
  Portals session's go-ahead; at b999491 test_light2d 151, test_resourcesync 37, test_audio 90 pass,
  test_materials refused by Smart App Control).**
  Sprite2DLight::vertical/verticalBaseY stands a 2D sprite up in the light - the flat frame turned a
  quarter turn about x through the base line, which through the port's y flip is exactly
  verticalSprite_ppl's P and vPixelLight's swizzled normal; specularStrength/specularPower plus
  MaterialComponent::glossTexturePath (a fifth material binding, white when unnamed) add
  mainSpecular's Blinn highlight per light before the clamp, seen from a per-light eye
  (Light2DEye). Both opt-in; AllPasses and every non-opted sprite are bit-identical (engine suites,
  run in the engine agent's clone at f30df7c: test_light2d 151, test_materials 371,
  test_resourcesync 37, and four more, 0 failures; not rerun at b999491, nor in this build).
  Wiring: render/Lighting stands every lit ET_VERTICAL sprite on verticalBaseY = -position.y at
  height z + ZAxisDirection.y * z (replacing the middle-row height); a lit <Gloss> sprite with pixel
  shaders on gets the map (entities/ + file name, Plain, language-swapped like the normal map) and
  strength specularBrightness / lightIntensity (0.7.12's highlight left lightIntensity out; the
  engine's light colour includes it); PenumbraLayer publishes Light2DEye each frame
  (mirrorY = -(camera.y + 0.75 * 768), height 768: ETHShaderManager::SetFakeEyePosition).
  Menu (1366x768, cursor parked) against reference/captures/00_menu.png, mean |ref - port| per
  region before -> after: left pedestal 24.4 -> 12.9, right pedestal 18.1 -> 11.1, left torso
  10.2 -> 5.8, right torso 10.5 -> 9.0, lower-left barrel 27.9 -> 3.9 (its lid no longer lit by
  lights behind it). Every pixel that changed lies on a lit vertical or glossy sprite; the button
  labels only brightened (1199-1718 pixels each, up to 137 levels), and the menu's 38 glossy floor
  tiles (white_ground.ent) show a streak of glints below each light, toward the viewer. Level 1
  has no vertical entity and its only gloss (potions) is off screen at the start: the 1024x768
  capture is byte-identical to the one before.
- **E8 smooth motion** (render/Interpolation, ShadowRenderer's `shapes`, snapshot identity:
  ParticleDraw system/particleId/lifeStartMs, RenderSnapshot::sceneSerial). Gate: with the engine
  at b999491, E8 (off under --fixed-step), E10 and the path resolver in, level 1's fixed-step
  capture is byte-identical to the previous build's.
- **E10 options screen** (a Stepper widget beside Switch, five new globals seeded, read back, saved
  and applied by PenumbraLayer; a pick on the screen replaces that run's --lang/--widescreen).
- **Packaging.** game/eth/Paths resolves the original and the data at run time (flag > beside the
  exe > the build's path); tools/package.bat assembles out/package/Penumbra (exe, engine SPIR-V,
  game/data, the original's data minus exe/dll/as/cg, app-local CRT); README.md. The package
  launched from %TEMP% finds both roots beside itself and draws the menu. main.cpp now exits 1 on
  an unparseable flag and prints the game's and the engine's options for --help.
Gates: build zero warnings; 15 suites, 13 run, 2345 checks, 0 failures (test_pn_paths 71,
test_pn_render_interp 189, test_pn_render_lights 171, test_pn_render_english 124,
test_pn_render_hud 274, test_pn_scenarios 532 with scenario 14 (E10) passing). test_pn_boot and
test_pn_formats were refused by Smart App Control on both launches of this build (not run).
- **No console, a log file, the licences.** Penumbra.exe is a Windows-subsystem program (a game
  started from Explorer opens no console); the log is mirrored to %APPDATA%\Penumbra\penumbra.log;
  headless runs still receive stdout/stderr through the shell's redirection (the level 1 capture is
  byte-identical). LICENSE.md says what is under which terms - game/script and the Ethanon-derived
  parts of game/eth are LGPL-3.0-or-later as works derived from the original's LGPL scripts and
  engine - and licenses/ holds the LGPL-3.0 and GPL-3.0 texts, which the package carries.
  test_pn_boot and test_pn_formats were refused again after a relink (third time); not run on this
  build - test_pn_scenarios (532 checks) covers boot's ground, and no parser changed.

### Step 7 — the floor seam (done 2026-09-27)
- **E11:** doCharacterCollision (controlCharacters.as:126, :192) kept only the LAST collided box's
  thinner-box test, so where two floor tiles meet the wizard counted as airborne for one frame and his
  walk cycle restarted (seen in level1 at x 3322 and 3334.5, the floor01 176/177 seam at 3328).
  Script.hpp kFloorSeamFix counts every box collided that frame; false restores the original line.
  test_pn_scenarios: the walk over the seam now reports no airborne frame (532 checks, 0 failures,
  before the assertion was added); the added assertion's build was refused by Smart App Control.
- Kept as the original (harmless): a checkpoint is part of its own save, so each respawn at it takes
  it again; g_lives also counts Versus deaths (resetData restores them; Versus shows points).
- Performance on this laptop (Radeon 780M, 1920x1080, level 3): 60 fps at vsync (FIFO), 2.7 ms of
  CPU per frame at the median; loading a level costs one 0.7 s frame (the original: ~14 s).

### Step 8 — played live, and a lone gamepad (done 2026-09-27)
- **Live input.** Penumbra.exe driven in real time with real mouse and keyboard events (a rig like
  reference/analysis/orig_rig.ps1, no --fixed-step, no --hold): the mouse hovers New Game (the story
  panel opens) and clicks it, level 1 loads, the wizard walks right about 225 px in 1.5 s and left,
  jumps, swings, casts the fireball and the light spell, and Esc returns to the menu. (Injected arrow
  keys must carry KEYEVENTF_EXTENDEDKEY: without it their scan code is the numpad's, which GLFW
  reports as KP_6 - a rig detail; a physical keyboard sends the extended code.)
- **E8 against the camera** is pinned by test_pn_render_interp's real-game section: a walking
  wizard's drawn screen x never leaves the span between his two tick positions.
- **E12:** with the shipped defaults a lone gamepad went to joystick 0 - player 2's - so a solo player
  with a pad drove the princess (who must be summoned) and the menu ignored it. The first real pad now
  goes to player 1's index and the second to player 2's (settings.controls.firstPadIsPlayer1, default
  true; false is the original). test_pn_render_input 172 checks, 0 failures.
- LICENSE.md now states only what Ivan said (the permission is his to describe; his own code has no
  licence chosen yet).

### Open
- **Engine push**: b999491 is only in this checkout. Build and run the touched engine suites
  (test_light2d, test_materials, test_resourcesync) at b999491, `git -C engine pull --rebase`, tell
  magic-portals-remake-93, push, then commit the pin separately.
- **test_pn_boot and test_pn_formats** were not run on this build (Smart App Control).
- **Not modelled in the light**: the light pass's alpha test (a texel at alpha <= 1/255 took no
  light, highlight included); the highlights 0.7.12 baked into static sprites' lightmaps with the
  first frame's eye (every light is live here, E9, so they follow the camera); per-pixel depth of
  vertical sprites, which leaned back in the z-buffer in 0.7.12 (docs/spec/21).
- **smoothMotion** has no row on the options screen yet (room at x 255, y 694-740).
