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
| E1 | Widescreen: logical view 768 px tall, width by aspect; the menus keep their 1024x768 layout, centred, and in a window wider than 4:3 their scene goes on past the sides instead of bars (Step 25: the scenes' own tiles continued, edge rectangles to the window's edge, up to 4:1); off = the original's 4:3 with bars everywhere | 1024x768 only |
| E2 | Any window size / fullscreen, rendered at native resolution; fullscreen at the desktop's mode, or at a mode picked from the list while fullscreen (switched as 0.7.12 did, saved as window.fullscreenWidth/Height, Step 23) | 1024x768 or listed video modes |
| E3 | Modern gamepads (XInput via GLFW) mapped by meaning, for both players | winmm button numbers |
| E4 | Keyboard second player (presented to the scripts as joystick 1) | P2 needs a joystick |
| E5 | English text alongside Portuguese, switchable | Portuguese only |
| E6 | Settings persisted (language, window, volumes, controls) | options reset every launch |
| E7 | Original bugs fixed where they are plainly bugs (listed per step) | — |
| E12 | One gamepad plays the wizard and drives the menu; a second plays the princess (firstPadIsPlayer1) | the first pad was player 2's |
| E11 | Standing on two floor tiles at once counts as standing (kFloorSeamFix) | the last tile decided; one airborne frame at each seam |
| E8 | Smooth motion: the world (sprites, lights and halos, shadows, particles, camera) drawn between the last two ticks by SimulationClock::alpha, one tick behind; whole-pixel ends stay on whole pixels; never across a scene load, a frame gap or a jump over 64 px; settings.smoothMotion (on) and --smooth on\|off, off under --fixed-step | one tick per 60 Hz vsync |
| E9 | Shadows drawn live, their visible end at the light's reach | static shadows baked into lightmaps at 8x the caster's height |
| E10 | Enhanced settings on the original's options screen (videoModes.as): keyboard player 2, widescreen/4:3 levels, Português/English at once, music and effects volume in 10% steps (a new Stepper widget beside Switch), smooth motion (E8; 'Ativa/Desativa movimento suave'), saved to settings.json | the screen offered only the video-mode list, pixel shaders, window/fullscreen and the joystick layout, all forgotten at exit |
| E13 | A pause: in a level or an arena, Esc or player 1's Back freezes the game (the Machine does not tick: GetTime, fades, cooldowns and the run's clock stop, so best times exclude paused time) under 'Pausado'/'Paused' with Continuar/Resume and Menu principal/Main menu (arrows, stick/D-pad, Enter/A/Start, Esc/B/Back, mouse); Main menu feeds the original's own K_ESC for one tick; the music is ducked to 40%; it opens by itself when the window loses focus in play (settings.pauseOnFocusLoss, on; off under --fixed-step); not on the end screens, where Esc/Back still go to the menu | none: Esc in a level returned straight to the main menu and the run was lost (doLoop's escToGoToMenu) |
| E14 | Gamepad menus: in the menu, the arena select, the options screen and game over, a pad's A also confirms (JK_10) and B also cancels (JK_09); a button already held when a menu opens counts from its next press | only Start confirmed and only Back cancelled (getConfirmButtonStatus/getCancelButtonStatus, playerInput.as:267-305) |
| E15 | The five ambient horror.mp3 markers the level designer named "play_sound.ent" (level2 469, 493, 548; level3 219, 427) play once on screen like the correctly named ones (Script.hpp kPlaySoundEntFix) | setupScene.as:175 collected only "play_sound" by exact name: they never played |
| E16 | On-screen touch controls (render/TouchControls, game/data/touch_controls.json): a direction disc (left/right/down, the thumb slides) and jump/sword/fire/light as a pad's face buttons press player 1's own keys (K_LEFT/K_RIGHT/K_DOWN, K_CTRL, K_S, K_D, K_SPACE); a corner button sends K_ESC (E13's pause in play, back elsewhere); in the menus, the options, game over and the pause the buttons hide and a tap is a click there; Magic Rampage's buttons (tools/art/make_mr_touch_art.py, game/data/images/touch/README.md; the first placeholder art, tools/art/make_touch_art.py, kept in images/touch/placeholder/ with its manifest), the look and layout data-only; settings.touchControls auto/on/off (auto = on under PENUMBRA_MOBILE), --touch on the desktop (the held mouse is the finger); two combo buttons play the sword and spell combos as macros of the same keys, toward the way the wizard faces, after the combo buffer has emptied (Step 17) | keyboard and joysticks only |
| E17 | Stand-in fonts (game/data/fonts: Liberation Sans Bold 2.1.5, SIL OFL; DejaVu Sans Bold 2.37, Bitstream Vera licence): off Windows, and on a Windows machine missing a face, FontAtlas draws with them at the metrics of the Windows face they stand for (Arial Narrow = Liberation Sans Bold at 82% width, within 1/2048 em of Arial Narrow Bold on every cp1252 character the game draws). On Windows the system faces still come first | the Windows system faces (D3DXCreateFontA), nothing else |
| E18 | MP3 without Media Foundation (eth/SoundDecode, dr_mp3 vendored at a pinned commit): where the engine cannot decode an MP3 (every platform but Windows) the port decodes it itself; on Windows the engine's Media Foundation path still decodes, dr_mp3 only if it refuses a file | Audiere on Windows |
| E19 | The first language off Windows: Portuguese when LC_ALL, LC_MESSAGES or LANG starts with pt (Settings::SetSystemLocale is the hook Android and iOS feed the device locale into) | Portuguese only |
| E20 | The options screen on a phone (Script::g_mobileLayout, raised by the layer on PENUMBRA_MOBILE builds; a runtime flag, so test_pn_scenarios 21 runs it on the desktop): no video-mode list, no windowed/fullscreen switch and no "Pressione Alt+Enter" line (menu footer and options screen); in the switch's place E16's touch controls on/off ("Ativa/Desativa controles de toque", saved as settings.touchControls "on"/"off", applied at once) | the video modes, the window switch and the Alt+Enter line, on every machine |

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

### Step 9 — a pause, gamepad menus, the smooth-motion row, six more scenarios, one test executable (2026-09-28)
- **E13** render/PauseMenu, pure, fed held input each tick. The layer skips Machine::Frame while it
  is open, feeds K_ESC for one tick on Main menu, ducks the music to 40%, shows the pointer and
  draws the last tick without blending. It opens only under levelLoop/pvpLoop with g_gameFinished
  false, only on K_ESC or JK_09 on getPlayerJoystick(0), and on a focus loss. After a pause,
  FilterForGame keeps what was pressed in it out of the game until released. Clicks count again 15
  ticks after the focus returns. The overlay goes to HudRenderer::Draw's new `extra` commands,
  translated and laid out like the scripts' text. Eth::Machine gained LoopFunction() (read-only).
- **E14** InputMapper::SetMenuMode in IsFixedLayoutScene's scenes (set each tick from the scene
  file, before BuildTick). The stick cursor stays at the original's 5 px per frame.
- **E10 follow-up: smooth motion on the options screen.** g_smoothMotion ('Ativa movimento suave' /
  'Desativa movimento suave', worded as the original's own g_enablePS row) at x 255, y 694-744,
  under the steppers and above the Alt+Enter line (y 753); seeded from what the run draws, read
  back and saved like g_widescreen (a pick replaces --smooth), applied at once through SaveSettings.
  Captured in English at 1024x768: no overlap, 'Disable smooth motion' ends near x 477 (the
  Portuguese 'Desativa movimento suave' was not captured; it is about as long as the existing
  'Jogador 2 só no joystick' row, which fits). Under --fixed-step
  the 'Desativa' row is the selected one (the run's override), as the widescreen row shows --widescreen.
- **Coverage (test_pn_scenarios).** Six new scenarios 15-20: the K_2 cheat and play_sound; level2
  -> level3 through next_level 428; the paladin and the master knight (every data.enml stat
  measured); the summon's price and refusals; arenas 2-6 with lv6 played to 3-0; a lone gamepad
  from boot in a second runtime. No port bug found; 0 script aborts. ORIGINAL bugs pinned, not
  fixed: five horror.mp3 markers are named `play_sound.ent`, so setupScene.as:175's
  GetEntityArray("play_sound") never collects them and they never play (level2 469, 493, 548;
  level3 219, 427); a princess alive but off screen does not block a second summon, which costs
  another 50 mana and life (controlCharacters.as:688-697); exactly 50 mana is refused although the
  message says 50 is needed (controlCharacters.as:681); a pad-only player had no way from a level
  back to the menu (escToGoToMenu reads only K_ESC) - E13's pause now gives one (player 1's Back,
  then Main menu), not yet covered by a scenario (they drive the scripts, not the layer).
- **test_pn_all** (tests/all): every tests/test_pn_*.cpp compiled into one executable from a
  wrapper per suite (WrapSuite.cmake, rewritten when the suite changes); each suite runs in a child
  process of that same file (the ported script's globals outlive a Machine, so the three suites
  that boot the real game cannot share a process); not registered with ctest. **Smart App Control
  accepted it on its first launch**, and all 16 child starts of it: 16 suites, 3716 checks,
  0 failures in 3.3 s - test_pn_audio 177, _boot 46, _formats 589, _particles 81, _paths 71,
  _render_english 124, _render_hud 277, _render_input 203 (+testMenuMode), _render_interp 189,
  _render_lights 171, _render_particles 137, _render_pause 312 (new), _render_textures 144,
  _runtime 277, _scenarios 916 (914 + the smooth-motion row's two presence checks, added without a
  tick so that no later scenario's timing moves), _smoke 2. test_pn_boot and test_pn_formats ran
  for the first time since Step 5; their own exes had been refused on every build since.
- **Captures** (Penumbra.exe; the full build's link was refused, exit 126; one relink with a real
  change - --help now lists --tour and the screens --start can open - was accepted): level 1 paused
  at 1366x768 in English (dim, panel, 'Paused', 'Resume' highlighted, no menu.esc load after the
  Esc); the options screen with the new row; one batched navigation run (--hold RIGHT@40-260,
  ESC@120, DOWN@150, UP@170, ENTER@190, ESC@230, DOWN@250, ENTER@270, a PNG every 10 frames): while
  paused, every pixel outside the panel is identical (the timer, the HUD and the world frozen),
  only the highlighted row moves; the walk goes on after Resume with RIGHT held through the pause;
  Main menu reaches the menu within 10 frames. Validation clean in all three.
Gates: build zero warnings (full build, then Penumbra alone); test_pn_all 16/16.

- **E15** (the decision above) and the final run of this round: build zero warnings;
  test_pn_all once - 16 suites, 3719 checks, 0 failures (test_pn_scenarios 919 with E15's
  expectations). Smart App Control refused test_pn_all's first link of the round and, through an
  orchestrator mistake, the same binary a second time; a relink with a real change (the runner's
  summary now lists every suite with its time) was accepted.

### Step 10 — the pause, live (2026-09-28)
- Driven with real input events (a desktop rig): Esc in level 1 opened the pause and froze the game (two
  frames 1.5 s apart byte-identical), the selection moved, and Main menu reached the main menu. Two
  anomalies (Main menu preselected on opening; an Enter that did not resume) traced to the test, not the
  game: the rig moves the window under a resting mouse on its first screenshot, and the Magic Portals
  session's game windows were opening on the same desktop, taking the focus (auto-pause) and some of the
  rig's input. Live desktop tests stopped while another session drives windows here.
- The pause's pointer now selects a row only when it moves at least 2 logical pixels between ticks: a
  wobble of a pixel (DPI or rounding) no longer takes the selection from the keys. It does not cover
  the rig's anomaly: a window moved far under a resting mouse jumps the pointer past the threshold in
  one tick (a player cannot do that; dragging a window carries the cursor with it). Side effect: a
  pointer creeping slower than 2 px a tick (about 120 logical px/s) does not highlight a row; its click
  still chooses it. No check pins the threshold yet. If it matters, measure from an anchor set when the
  pause opens and on each key move, and pin wobble, slow creep and key-then-rest in test_pn_render_pause.
- Pause transitions (open, close, selection, with the inputs that caused them) are logged to
  %APPDATA%\Penumbra\penumbra.log, for Ivan's own testing.
Gates: test_pn_all once - 16 suites, 3719 checks, 0 failures.

### Step 11 - session 3 opens: the pause's options row (2026-09-28)
Ivan's ruling for session 3: polish the open list, and make the game play on Android, iOS, Windows,
Linux and macOS, with on-screen controls for mobile (button art from Magic Rampage, which is not on
this laptop: placeholder art until he gives its path; macOS/iOS verified on a GitHub Actions macOS
runner). The Magic Portals session does no platform work: the engine's platform backends are ours,
additive, the desktop path unchanged.
- **pauseOnFocusLoss** now has its row: "Pausa ao perder o foco" / "Continua sem o foco" (Pause on
  focus loss / Play on without focus), a Switch at x 540-796, y 694-744, beside E8's (the column at
  x 255 is full to the Alt+Enter line). Seeded from what the run does (--fixed-step turns it off), a
  pick replaces that override and is saved as E8's is.
Gates: build zero warnings; test_pn_all once - 16 suites, 3731 checks, 0 failures (scenario 14 clicks
the new row both ways; render_hud checks its English).

### Step 12 - touch controls, E16 (2026-09-28)
- **What player 1 has** (playerInput.as, combo.as, main.as): walk left/right (held: getPlayerXYAxis),
  down (held at the next_level door, main.as:208; first in the spell combo), jump (K_UP or K_CTRL,
  KS_HIT; an air jump costs 4 MP), sword K_S, fire K_D, light K_SPACE (each on KS_HIT, none repeats
  while held), the combos Left-Left-Sword and Down-Left/Right-Fire (one command a frame, 210 ms
  apart), cancel K_ESC. Up only jumps and feeds an unused CMD_UP, so the direction control has none.
  Menus are the cursor plus getConfirmButtonStatus (Enter, either mouse button, JK_10); the arena
  select and game over leave only on cancel, which the corner button gives them.
- **render/TouchControls** is pure, like PauseMenu: the layer hands it each tick's contacts (the
  engine's, through InputMapper::WindowToLogical) before the pause reads the frame, and its HudCmds
  go into the same extras as the pause's, under them. A contact belongs to what it first landed on
  until it lifts; one whose control is hidden under it is dead. A tap between two ticks is latched
  for the next, as InputMapper latches keys. While a finger is down the left mouse button is the
  touch controls' alone: the engine's Android backend makes a gesture's first finger the mouse too
  (as the desktop's held mouse is contact 0), and that finger on a button must not click under it.
- **Not done here (engine, platform):** safe-area insets (TouchControls::WindowInsetsToLogical is
  ready for them), telling a real touch from the synthesised mouse contact, the Android back key.

### Step 13 - Linux (WSL Ubuntu 24.04), and what it takes off Windows (2026-09-28)
- **Builds with GCC 13.3 and Clang 18.1.3**, the game at -Wall -Wextra -Wpedantic and the suites at
  -Wall -Wextra with zero warnings in the port's code (the engine's own third-party warnings remain).
  tools/build_linux.sh configures on the Linux filesystem (~/pn-build-linux), never under /mnt/c or
  build/, and runs test_pn_all once with --test.
- **One path resolver** (eth/Paths ResolveUnder): the path as the loaders joined it when it exists
  (always, on Windows), else a case-folded index of the root with backslashes read as '/'. Scenes,
  entities, image sizes, GetStringFromFile, the enml files, samples, textures, strings.json, the
  English images and the fonts go through it. The original's 446 files have no case or separator
  mismatch, so it is a guard for case-sensitive storage (Linux, Android), not a fix.
- **E17-E19** (above): stand-in fonts, dr_mp3, the POSIX locale. Liberation Sans Narrow was not used:
  it is GPLv2 with exceptions, and Liberation 2.x dropped it.
- **tests/all/RunAll.cpp** starts each suite with posix_spawn off Windows (same --suite/--list
  protocol, a timeout, a crash reported by its signal); iOS cannot spawn and reports it.
Gates (Linux): test_pn_all, 17 suites (with E16's), 4917 checks, 0 failures, on both compilers.
Headless captures under xvfb + lavapipe with validation on and no validation errors; level 1 at
frame 300 against the Windows capture of 27 Sep: 99.3% of pixels within 8/255 (text edges, flame
particles, one lit area). Silent without a sound device; ALSA's null device plays the MP3s.

### Step 14 - Android (2026-09-28)
- **The engine had no Android** (its README: no android_main, no ANativeWindow surface, no touch
  source, no audio). Added, all behind `SUPERSONIC_WINDOW_GLFW` (platform/WindowBackend.hpp: 1 on
  the desktop, 0 on Android) or `if (ANDROID)` in CMake, the desktop code unchanged:
  platform/android (android_main over the NDK's native_app_glue, the looper and lifecycle, logcat,
  touch as up to 8 contacts with the first finger also the mouse, keys with Back as Escape,
  gamepads, a latch so a press shorter than a frame is still seen), a platform-neutral
  platform/Gamepads, the surface from ANativeWindow and its release/restore when Android takes the
  window away, an identity-transform swapchain in the window's orientation, AAudio over the same
  AudioMixer as ALSA, user data in the app's private storage, VMA's Vulkan functions fetched at run
  time (API 26's libvulkan.so lacks the 1.1 ones). The dead AndroidNativeApp stubs are gone.
- **The game** is a SHARED library on Android (`PenumbraMain` is main.cpp's body; the desktop main
  calls it), with PENUMBRA_MOBILE public on PenumbraGame (E16's controls on, keyboard player 2 off by
  default); InputMapper reads pads from Supersonic::Gamepads on Android; AndroidMain.cpp unpacks
  the files, feeds the device's language to E19's hook, and reads one-shot flags from
  penumbra_args.txt. game/android/AndroidManifest.xml: NativeActivity, no code, landscape,
  minSdk 26, targetSdk 35. tools/build_android.sh + tools/android_package.py: NDK CMake, aapt2,
  zipalign, apksigner - no Gradle.
- **Measured** on an emulator of our own (Penumbra_API33_x86_64, port 5560, SwiftShader Vulkan 1.2,
  headless; never the Magic Portals session's AVD): the menu, a tap as a click, New Game to level 1,
  the touch controls moving the wizard, Back opening the pause, Home and back (surface rebuilt,
  pause open, 5 of 5), the pause's Main menu, Quit, a pad's Select and A. The emulator draws about
  5 frames a second (SwiftShader), so its game clock runs slow. AAudio opens, pauses and closes
  with the app (dumpsys), not heard (-no-audio). arm64-v8a is built, not run.
- **Not done:** immersive mode (the navigation bar needs a call on the UI thread: a Java helper or
  GameActivity), the soft keyboard for a high-score name, safe-area insets, two-finger touch on a
  device, a real phone's GPU.
Gates: engine suites, 57 of 57, on the desktop path under Linux (WSL; the engine's CI is dispatch
only); the editor compiles on Windows with zero warnings (not launched); Penumbra on Windows:
build zero warnings, test_pn_all once - 17 suites, 5006 checks, 0 failures; Android: both ABIs
build, the APK packages.

### Step 15 - Android, rounds 2 and 3 (2026-09-28)
- **Music on the phone**: every MP3 decodes through E18 (menu.mp3 14 s, fase.mp3 120 s, chefao.mp3,
  the effects), one log line per decode. **Language**: a fresh install follows the device (English
  on en-US; Portuguese after persist.sys.locale pt-BR). **Keyboard player 2** is off on a phone.
- **Immersive**: the engine's SupersonicActivity (a small NativeActivity subclass, javac + d8 in
  tools/build_android.sh, no Gradle) hides both system bars, again after Home and on focus.
- **The notch (orchestrator's ruling, option b)**: the window stays off the display cutout, so the
  whole game - its HUD in the top corners included - is drawn clear of it; measured with the tall
  cutout overlay: 1184x720 beside a 96 px black strip, the HUD right of it. The engine's
  SafeArea::Get() (window-pixel insets, zero on the desktop) still feeds E16's controls whatever
  insets remain, and is the API iOS implements.
- **No name entry to port**: the high scores (hs.enml) hold times only; GetLastCharInput's one caller,
  stringInput, is never called (docs/spec/90).
- **E20**, the options screen on a phone (above): no video-mode list, no window switch, no Alt+Enter
  line (menu footer too); a touch-controls switch in their place, saved as touchControls on/off.
Not measured: Android 15+ (edge-to-edge enforced; the emulator is API 33), a real phone.

### Step 16 - rendering fidelity: baked highlights, the light pass's alpha test, standing sprites' rows (2026-09-28)
- **Static highlights** (Lighting.cpp kBakedHighlightEye, on): static glossy sprites see static
  lights' highlights from 0.7.12's lightmap-bake eye, (L.x, top + 1.5 screenH, z + 768), fixed to the
  sprite (ETHShaderManager.cpp:76-90, ETHScene.cpp:561-589); the live eye slid them with the camera
  (up to 28 levels for a 200 px camera move, pvp_lv5, through the engine's CPU copy of the light
  loop). Menu vs the original's capture: 5.28 -> 4.96 overall, 7.66 -> 4.21 on the pixels it changes.
  The devils' "bronze" was this sheen plus fog, halo-ratio and torch-flicker variation from frame to
  frame: at frame 360 the left devil is within 0.2 levels of the reference.
- **The light pass's alpha test** (Lighting.cpp kLightPassAlphaTest, on): each light's pass is
  alpha-tested as 0.7.12's was (ALPHAREF 1 GREATER, the pass alpha per shader variant, lightIntensity
  kept apart; GameSpace.dll SetAlphaMode, BeginLightPass @0x43a1b0). Faint glows no longer tint:
  menu letter edges 9.3 -> 2.6, level2 18.7 -> 15.8, level3 10.3 -> 6.0 on the pixels changed by more
  than 8 levels.
- **Standing sprites' rows** (DrawOrder.cpp kPerRowVerticalDepth, on): a standing sprite is cut into
  bands of rows where a sprite or particle overlapping it lies between its base and top, as 0.7.12's
  per-row depth (defaultStaticAmbientVS.cg:132, pixelLightVS.cg:129) sorted them: the cursor's
  sparkles go behind the arena thumbnails' upper rows; the low fog behind the menu barrels 4.9 -> 0.7.
- **Engine** (opt-in; without the game's flags the menu, level 2 and level 3 are pixel-identical to
  the old engine's): Light2DComponent::baked, Sprite2DLight::bakedEye/bakedEyeY/lightAlphaTest,
  Light2DAlphaTest in the light buffer's former padding word; shader.frag, frag.spv regenerated with
  the SDK's glslc (which reproduces the previous frag.spv byte for byte from the previous source).
- docs/spec/21: the bake-eye formula, which held only in the bake's moved frame.
Gates: Linux test_pn_all 17 suites, 5008 checks, 0 failures (render_lights 193, render_textures
184); engine suites on Linux (test_light2d 187, test_materials 395); Windows build zero warnings;
Windows test_pn_all refused by Smart App Control this build (exit 126, one notification): not run.

### Step 17 - macOS and iOS (2026-09-28, branch apple-port)
Built and run only on GitHub's macOS runners (macos-15 arm64, Xcode 26.3, iOS 26.2 simulators):
this laptop cannot compile against Apple's SDKs. `.github/workflows/apple.yml`, on pushes to the
`apple-port` branch and by hand, never on main; MoltenVK 1.4.2 from the KhronosGroup release's
static xcframework, linked into the program (no loader, no SDK). The engine's side is on its
`apple-port` branch, all behind `__APPLE__` / `SUPERSONIC_PLATFORM_IOS` / `if (APPLE)`: CoreAudio
(macOS and iOS), a UIKit backend (src/platform/ios), GLFW told to use the linked Vulkan and to
leave the working directory alone, MoltenVK linked directly, portability extensions only where
offered. Its README's Platforms section lists it all.
- **The game.** macos/MacMain.mm is main() on a Mac: NSLocale's first language into E19's hook
  (a Finder-started app has no LANG), and inside Penumbra.app --original/--data at
  Contents/Resources and the engine run from ~/Library/Caches/<bundle id>/engine with the
  bundle's shaders copied in (a bundle is never written). ios/IOSMain.mm is SupersonicMain on
  iOS: the same with the bundle's root, the device's language, and one-shot flags from
  Documents/penumbra_args.txt. PENUMBRA_MOBILE on iOS (E16's controls), not on macOS. InputMapper
  reads pads from Supersonic::Gamepads wherever the window is not GLFW's. tools/apple/make_app.sh
  assembles Penumbra.app for either (original/ without the Windows binaries and scripts, data/,
  engine/assets/shaders, the original's skull icon via tools/apple/ico_to_png.py, ad-hoc signed).
- **The notch (orchestrator's ruling):** the game is drawn clear of it - the Metal view is the
  safe area's width (black beside the notch or Dynamic Island) and runs to the bottom edge;
  SafeArea::Get (the engine's, Step 15) reports what remains inside it (measured: 2250x1206
  of 2622x1206 pixels, a 60-pixel home-indicator band). E20's phone options follow
  PENUMBRA_MOBILE, so iOS has them too (not seen: no frame in the simulator).
- **extracted/ is CRLF on every checkout now** (.gitattributes): the repository stores it LF,
  and test_pn_formats' round trips against the shipped bytes failed 14 checks on the Mac's LF
  checkout. The readme.txt files keep the *.txt rule, as on Windows.
- **Measured, macOS:** every target builds (the game's code with no warnings; the engine's
  MeshRegistry.cpp and two EnTT instantiations warn under clang, as on Linux); test_pn_all once:
  17 suites, 4914 checks, 0 failures (Linux 4917: test_pn_paths' three case-sensitive checks
  skip on APFS's case-insensitive default, as on Windows; Windows' 5006 include its fonts and
  Media Foundation). Penumbra.app, started from an unrelated folder, rendered level 1 on the
  runner's Apple Paravirtual GPU (--window 1024x768 came out 1024x653, presumably fitted to
  the runner's display, whose size was not measured), English text, lights and torches as on Windows; CoreAudio pulled 384000 frames in 750
  callbacks from the runner's virtual sound device (not heard). With the Mac's language list
  set to Portuguese (AppleLanguages pt-BR, LANG still English, no settings.json) the same
  capture's text is Portuguese: NSLocale reaches E19's hook.
- **Measured, iOS:** the simulator build (arm64) and a device build (iphoneos arm64, minos 16.3)
  compile, link and assemble. In the simulator the app installs and launches (the skull icon on
  the home screen), the scene connects, CoreAudio's RemoteIO pulls, Vulkan instance, device and
  swapchain are made, the engine initialises and the game loads level 1 with its touch controls
  on - and the first draw fails: the simulator's GPU (Metal family Apple 2) cannot draw with a
  non-zero base instance, which the engine's instanced batches use. Getting that far took two
  engine fixes found there (array views sampled-only; Tier 1 argument buffers off). No frame, so
  no screenshot of the game; no device run (it needs a signing identity).
- **Not done:** a frame in the simulator (a renderer change: base instance), any iOS device,
  signing and notarising, the Mac's own fullscreen/Retina check by a person, sound heard on
  either.

### Step 18 - the touch controls' combo buttons (E16 addendum, 2026-09-28)
The two combos (playerInput.as:358-395) are hard with a thumb on a disc, so two buttons play them.
- **The macro** is player 1's keys, one a tick, into the same frame: sword combo `-, side, -, side,
  K_S` (CMD side, side, SWORD, fires on its 5th tick), spell combo `-, K_DOWN, side, K_D` (CMD
  DOWN, side, SPELL, fires on its 4th). Tick 0 lets go of what the disc held, so the first press is
  a fresh KS_HIT; the release between the two sides is the only one the buffer needs (it records
  the first HIT of a frame, a different key's while the last is releasing).
- **The side** is the wizard's own currentDir, read by the layer from bruxo.ent each tick: he turns
  from the keyboard and pads too, and a level starts with it unwritten (RIGHT). Without a wizard,
  the side the disc was last pushed; else right.
- **The buffer must be empty.** checkSequence matches only the first three commands since it was
  last emptied, and combo.as:116 empties it only on a frame without a command more than 210 ms
  after the last: 13 ticks at GetTime = frame * 1000 / 60. TouchControls::ObserveFrame watches the
  frames the game runs (touch on or off) for a new press of anything the buffer reads for player 1
  - his six keys, his pad's stick past 0.8, JK_04, JK_02 - and the first press waits for 13 quiet
  ticks: no wait after a quiet spell, 14 ticks from a step just before, 217 ms between two combos. A
  macro that cannot get its quiet in 30 ticks (something keeps pressing) gives up.
- **While it runs** the disc's directions and the sword and fire buttons are held back (their HITs
  would land in the buffer); jump, light and pause are not. A sword or fire finger held through it
  stays held back until it lifts; the disc steers again at once. A second tap on either combo button
  is ignored; the pause, a menu, a load (the snapshot's sceneSerial) or switching the controls off
  (E20's row) cancels it, and its keys are simply no longer pressed.
- **Layout**: 100 px buttons in a row above the four (bottomRight offsets (216, 400) and (88, 400)),
  the sword combo on the sword's side. Placeholder glyphs in make_touch_art.py: chevrons and a sword;
  a down-then-forward arrow and a fire ball. A manifest control with `"enabled": false` is not there.
- **Tests** (test_pn_render_touch): each timeline tick by tick for both facings, through
  InputState; the fingers held back under a combo; taps; cancelling; the enabled flag; the layout at
  1024/1366/1707 with and without a safe area; the ported Combo fed by the macros in a bare Machine
  (fires on the expected tick, facing either way; waits exactly to tick 14 after a step; the same
  presses without the wait fire nothing; the keyboard's and player 1's pad's presses count, player
  2's pad's do not; back to back; the give-up); and level 1 of the real game: combo_sword.ent (no
  sword0.ent) and, right after a step left, combo_fire_ball.ent with direction LEFT.
Gates: Linux (WSL, GCC, -Wall -Wextra -Wpedantic) no warnings in the touched files; test_pn_all 17
suites, 5596 checks, 0 failures (render_touch 1636; in level 1 the sword combo cost 100 -> 96 mana
with a tick of regeneration, the spell combo fired 16 ticks after the step, 100 -> 75). Windows:
check.bat clean on every touched file; not built or run this step.

### Step 19 - Magic Rampage's buttons; the pause and the touch controls tested on the devices (2026-09-28)
- **The art** (Ivan gave Magic Rampage 7.8.7's XAPK from APKPure): MR's own pad is six square buttons
  built in its compiled script (CharacterScreenPadController in assets/game_32.bin: left, right,
  jump, attack, pause, the arcane-rune special), every button dpad-frame.png with an icon baked in,
  128 px only, no pressed-state image (the press is a tint). Used as shipped: jump, sword (MR's
  melee dagger), pause (its pill), back (inventory-back-button), left, right, and the knob (SEF's
  focus brackets). Built from MR parts in MR's style: down (dpad-right's face mirrored onto the
  frame), fire and light (the frame plus MR's element glyphs, outlined and shadowed like its
  icons), the sword combo (MR's arcane-rune special: "forward three times and attack") and the
  spell combo (a fire rune). tools/art/make_mr_touch_art.py makes them from the XAPK (read only) or
  an extraction; game/data/images/touch/README.md holds the provenance. The placeholders and their
  layout moved to images/touch/placeholder/ (one manifest copy away). The layout: three square
  direction buttons where the disc's sectors are, the diamond, the two combos above it, the pill.
  test_pn_render_touch checks every manifest's art and fit, and that every opaque texel of each
  arrow presses its direction.
- **Windows, the pause** (one launch of the packaged exe, input only from the game's own --hold and
  --cursor): Esc opens it, the game frozen (paused frames 0 px apart), Down and Up move the
  selection, a pointer at rest over a row does not undo the keys, Enter on Resume runs the game
  again, Esc reopens and Esc closes without leaving the level, a click selects and chooses, Main
  menu reaches the menu. Not testable without OS input or a pad: Alt-Tab, a real pad, the music at
  40%.
- **Android, the touch controls** (my emulator, adb input only, default settings): a tap on New
  Game; hold right, hold left, a slide from left to right; jump, sword, fire, light; two fingers
  (hold right and tap sword, through the emulator's multi-touch events); both combo buttons (the
  sword beam, the combo fireball, their mana); the pause pill, Resume, Main menu; Back opening and
  closing the pause; Home and back (the pause open, the frame rebuilt); a pad's A and B in the
  menus and Select in a level; the options' touch switch. A skeptical review re-read every capture
  and log: all confirmed but "Start never pauses" (no event shows Start reached the game).
- **For Ivan's eye:** the combo buttons lie over level 1's first sign at the start; at rest the
  knob's brackets frame the gap between left and right; the options screen shows the original's
  Back beside MR's corner back; a thumb between two direction buttons presses both (the sectors);
  the level tutorial still speaks of arrow keys on a phone.
Gates: Windows build zero warnings, test_pn_all once, 17 suites, 7539 checks, 0 failures; Linux
7450, 0 failures; Android both ABIs build.

### Step 20 - the touch controls: the knob at rest, the options' back, hints in touch wording (E16 addendum, 2026-09-28)
Three of Step 19's "for Ivan's eye" points, which he approved as fixes.
- **The knob at rest.** Magic Rampage's focus brackets were drawn at the direction control's centre
  with no thumb on it, framing the empty gap between left and right like a missing button. They are
  now drawn only while the thumb points a direction (past the dead zone and not within 22.5 degrees
  of straight up), on the button it holds as before; by where the thumb is, not by what is held,
  since a combo presses a side with no thumb there. A manifest field, `knob.atRest`
  (TouchManifest::knobAtRest): the struct's default is the old `true`; the shipped manifest and
  DefaultManifest() say `false`; the placeholder manifest, which names every field, says `true` and
  keeps its knob at its disc's centre. A check now holds DefaultManifest() equal to the shipped
  file.
- **The corner button, screen by screen.** It was Back on the options screen too, beside the
  original's own Back arrow (putBackButton, videoModes.as:65), which a finger already clicks. The
  rule is now TouchControls::CornerFor, one reason per screen: the pause - hidden (its rows lead
  on); a level or an arena - pause; their end screens - back (waitForInputToMenu); the arena select
  and game over - back (they read cancel only and draw no button); the options - hidden (a Back of
  their own); the main menu and before any scene - hidden (cancel does nothing there).
- **Control hints in touch wording.** Every text the game draws that names a key, found by
  grepping the scripts, data.enml and the scenes for setas, tecla, pressione, joystick,
  direcionais, CTRL, espaço, 'S', 'D', Enter, Esc, START: level 1's seven help signs (HUD line and
  its echo under the wizard), data.enml's comboTip lore sign, and menu.as's como_jogar panel. Each
  has a touch wording in both languages in strings.json's new `touch` section, keyed by the
  original's Portuguese, naming the buttons by what they show ("Utilize as setas no canto inferior
  esquerdo para mover-se" / "Use the arrows at the bottom left to move", "Golpe de espada: botão
  da espada" / "Sword strike: the sword button"). Localization::SetTouch, set by the layer from its
  switch every frame before the HUD draws, so the options row takes effect at once; the lookup is
  whole strings, in either language, with its own memo (the English memo must not hand a touch
  wording back once touch is off); off, not a byte changes. The scripts are untouched. Left in
  the original's words on purpose: "Pressione Alt+Enter..." (E20 does not draw it on a phone; a
  desktop with --touch still has the keyboard), the Versus screen's joystick messages and the
  settings blurb (what the mode needs, not a key to press), the options' row labels.
- **How to Play, decided:** the controls block's keyboard names give way to the buttons line for
  line ("Pulo: botão de pulo", "Ataque/espada: botão da espada"...), "Detectar joysticks: segure J"
  becomes the combo buttons and the pause button (no J on a phone; the menu finds a pad by itself),
  and the 2-player part (the second pad's START) stays word for word. Not both lists: the panel's
  column holds three more lines, not seven. The joystick numbers go too: a line with a button name
  and "(joystick N)" is wider than the 381 px column, and a player on a pad turns the touch controls
  off in the options and reads the original. 26 lines, 720 px of the 768.
- **Tests.** test_pn_render_touch: no knob at rest or with the thumb in the dead zone or straight
  up, nor under a combo's side; one while left, right, down or a diagonal is held; the placeholder's
  knob at rest at its centre, idle alpha, as before; DefaultManifest() == the shipped file; atRest
  read and a wrong type reported; CornerFor for every scene the original ships (13, none
  unclassified), a checkpoint reload, the end screens, the pause; on the options screen nothing drawn
  and a finger on the original's Back a click, no Esc; level 1 of the real game shows the first help
  sign on the HUD, and it translates to its touch wording (on) and to today's English (off).
  test_pn_render_hud: every text of the original's (and the enhanced rows' labels) that names a
  control has a touch wording in both languages unless kept for a reason, and nothing else has one;
  every `touch` key matches one of them (9); off gives exactly what a Localization never told about
  touch gives, on gives the wording, and a language switch with touch on gives it too; the three
  spellings of como_jogar (CRLF, LF, Script.hpp's lone CR) find one wording; the wordings fit where
  they are drawn, measured with the bundled stand-in fonts (help line under 1024 px at Arial 30,
  the panel 381 px by 768, the lore sign's two lines); HudRenderer draws the wording's glyphs with
  touch on and the original's with it off.
Gates: Windows check.bat clean at /W4 on every touched file (TouchControls, Localization,
PenumbraLayer, HeaderCheck, both suites); not built or run. Linux (WSL, GCC, -Wall -Wextra
-Wpedantic) no warnings in the touched files; test_pn_all 17 suites, 7879 checks: render_hud 631
and render_touch 3584, 0 failures each (in level 1 the first help sign's text is on the HUD 4
times - its line and the echo, each shadowed). The tree also held the shadows work in progress at
the time, whose render_lights failed 2 ShadowRenderer slot-count checks; that is not this step's.
Not tried on a device: the emulator pass is a later step's.

### Step 21 - the baked shadows take only their own light (2026-09-28)
- **0.7.12** (ETHRenderEntity::GenerateLightmap, E:ETHRenderEntity.cpp:465-531; E:ETHScene.cpp:929-947;
  docs/spec/21 §2.6, §3.2): a static caster's shadow from a static light existed only in the
  lightmaps. Per static light, the receiver's light pass alone went into a scratch target, every
  static castShadow entity's shadow was drawn over THAT target (BeginShadowPass: AM_PIXEL, black
  shadow.dds; maxOpacity: alpha byte(255 x opacity); drawToTarget: length x 8), none for an
  ET_VERTICAL receiver, and the target was added into the lightmap; the live pass of that pair, and
  its real-time shadow, never ran. So the shadow took away its own light and left the ambient and
  every other light alone. The port drew those shadows black over the finished frame (real-time
  alpha, length cut to the light's reach), ambient and all: the menu barrels' shadows near-black
  where the original's are purple.
- **Now** (render/Lighting.cpp kBakedShadowsOwnLight, on): ShadowRenderer hands each baked pair's
  strip - the bake's, x 8 uncut at alpha byte(255 x opacity) - to its light (BakedStrips ->
  LightRenderer -> the light's Light2DShadowsComponent), shadow.dds's alpha is the engine's mask,
  and every lit static sprite that does not stand up (Sprite2DLight::lightShadows) multiplies each
  light's add, highlight included, by what that light's strips leave of it at the fragment: the
  bake's scratch target, per fragment, the lights still live (the torches flicker). Real-time pairs (a dynamic
  light or a dynamic caster) are overlays as before. Off, the old capped overlay returns.
- **Engine** (opt-in): Light2DShadowsComponent and Light2DShadowMask; scene binding 13 (a count,
  per-light ranges in GatherLights2D's order, a 32 x 32 mask, up to 256 strips of 64 bytes);
  Sprite2DLight::lightShadows (flag bit 7); shadeSprite2D multiplies each light's clamped add by
  lightShadowKeep2D after the alpha test; CPU twins Light2D::ShadowMaskAt, ShadowStripUv,
  ShadowKeep; ARCHITECTURE.md. frag.spv regenerated with the SDK's glslc (which first reproduced
  the committed frag.spv byte for byte from the committed source). With this engine and the switch
  off, the menu and levels 1-3 are pixel-identical to the previous build's (lavapipe).
- **Measured** (lavapipe, 1024x768, Portuguese, frame 300, cursor (875,383); mean absolute
  difference from the original's captures; out/shots/shadows): menu 4.96 -> 4.14 over the screen;
  on the 8.0% of pixels the change moves by more than 8 levels, 12.14 -> 3.98 (49913 closer by more
  than 4 levels, 870 further). The top-right barrel's shadow (920-990 x 270-390): original
  (61, 21, 64), before (44, 25, 54), after (59, 22, 62); the shadow right of the middle barrels
  (660-740 x 300-360): (37, 4, 36), (34, 11, 27), (39, 7, 37); unshadowed floor unchanged. The
  devils' long shadows from their fires now darken the floor behind them as the original's do.
  Level 1 (05_level1_after14s): the shadow of the block under the first torch, 6567 pixels changed,
  8.55 -> 0.91 (whole screen 0.78 -> 0.67). Levels 2 and 3, pvp_lv3, pvp_lv6 and arena_select have
  no baked pair in view at frame 300: unchanged (the arenas against the switch-off build, the
  menu's and levels' stand-in for the old one). pvp_lv2 changes (122030 pixels: its torches'
  tile_shadow cones and the crystals' single_tile_shadow strips, x 9.5, now removing only their
  light, fully); no capture of the original to compare.
- **Not modelled**: the bake used every static caster in the scene, the snapshot holds the visible
  buckets' only (Machine.cpp, Scene::VisibleBuckets: no border ring in the menu and levels, one in
  PvP), so a caster in a bucket off screen casts nothing - its shadow points away from its light,
  so into view mostly when that light is off screen too, where the port has no live light at all
  (Open, below); a receiver is looked up where it is
  drawn (the menu floor is at z 0, where that is where it is lit); the scratch target's 8-bit steps.
  Around the menu's left fire the port is 15-20 levels greener than the original in shadow and out
  of it (its halo or its light: another difference), which the black overlay used to hide inside
  the upper-left barrel's shadow (0-20 x 300-370: 8.9 -> 13.0). The fragment stage now reads 7
  storage buffers (was 6): not run on a phone or an Apple GPU this step.
Gates: check.bat clean at /W4 on every touched C++ file (game, engine, the three suites); Linux
(WSL, GCC) test_pn_all 17 suites, 7935 checks, 0 failures (render_lights 249; the tree held the
touch work in progress too); engine alone on Linux, 57 of 57 suites (test_light2d 281, test_materials
418). Windows not built or run this step.
- **Caught on the emulator before the push**: binding 13 made eight storage buffers per scene set
  while VulkanRenderer's pool still budgeted a literal seven. Windows and Linux drivers allocated
  the set anyway; SwiftShader refused it and the APK died at startup (allocateDescriptorSets:
  ErrorOutOfPoolMemory) - for every engine game, not only this one. The pool is now sized from
  VulkanPipeline::kStorageBuffersPerSceneSet (8, listed binding by binding, pinned by
  test_materials), and createDescriptorSetLayout throws if the layout it builds ever disagrees.
  After the fix the APK starts, and the menu's shadows are purple on the emulator too.
Gates after the fix: Windows build zero warnings, test_pn_all once - 17 suites, 8024 checks, 0
failures; Linux 7935, 0 failures; engine 57 of 57 (ctest, on Linux); Android both ABIs, the menu
and level 1 on the emulator (out/shots/android/fixes/final_*.png).

### Step 22 - the pause with real input on Windows (2026-09-29)
With the Magic Portals session idle and the desktop free, the packaged game (the Smart App
Control-accepted binary; PauseMenu has not changed since it was built) was driven with real OS
input for half a minute (keybd_event, mouse_event, and a small window of the test's own taking the
foreground), from penumbra.log's pause lines and client-area captures (out/shots/windows/live/):
- a real Esc opens the pause (tick 501) and a real Esc closes it (641);
- losing the foreground to another window pauses a running level on its own ("pause opened ...
  focused 0", tick 708);
- the click that brings the window back, on the Main menu row, only moves the highlight: the
  15-tick grace keeps it from choosing (tick 800, still paused; 5_refocus_click_still_paused.png);
- a real click on Resume closes it (tick 887).
The first run's arrow keys never arrived: the test sent them with scan code 0, and GLFW reads keys
by scan code (a test-side mistake; real arrows were checked live in session 2 and by --hold in
Step 19). A freshly linked build/game/Penumbra.exe was refused by Smart App Control first (one
notification); the accepted packaged binary was used instead, never the refused one again. Still
unmeasured: a real gamepad, and the music's 40% while paused, by ear.

### Step 23 - fullscreen modes, and the pointer over the bars (2026-09-29)
Ivan, playtesting on Windows: "I cannot change the resolution after going into fullscreen. Nothing
changes when I press on different resolutions at all", and "there are black bars on the sides of the
screen and I cannot reach in them with my cursor".
- **A mode picked in fullscreen.** 0.7.12: a line of the options screen's list calls
  SetWindowProperties(title, w, h, Windowed(), ...) (videoModes.as:110), which reset the D3D9
  device at that back buffer size (E:ETHEngine.cpp:144-166, G:Video/Direct3D9/gs2dD3D9.cpp:886-960):
  in fullscreen, a display mode change. The port turned the pick into WindowControl::SetWindowedSize,
  which while fullscreen is only the size to come back at, and the engine's fullscreen covers the
  monitor at its current mode, never switching: nothing visible happened.
  - **Engine** (opt-in; SetFullscreen(true) and every existing path unchanged):
    `WindowControl::SetFullscreenMode(w, h)`, latched as a fullscreen request carrying its size
    (Requests::fullscreenMode, zeroes for SetFullscreen; the last request wins either way).
    `ChooseFullscreenMode` (pure) picks the mode: the desktop's own for the desktop's size (no
    switch, and the way back from another mode), else the size at the desktop's rate where the
    monitor offers it there, else at its highest. A size neither listed nor the desktop's is refused
    (false, nothing latched, a warning). NativeWindowControl applies it with glfwSetWindowMonitor,
    entering fullscreen (remembering the windowed rectangle, as SetFullscreen does) or changing mode
    in place; GLFW puts the desktop's mode back on leaving and on focus loss (auto-iconify). While
    switched, DesktopMode() answers the desktop's mode, which glfwGetVideoMode no longer does.
    Logged as "Fullscreen on <monitor> at WxH @ R Hz, switched from the desktop's WxH @ R Hz."
    Android and iOS take the request and drop it (their ApplyPending, unchanged). test_gameruntime:
    the latch, the orderings, the refusal, the desktop's size accepted unlisted, the rate choice.
    ARCHITECTURE.md's "no exclusive mode at another resolution" is now the opt-in call.
  - **Game** (render/WindowMode, pure, test_pn_render_input): a line picked while fullscreen
    switches the display to it and is saved as `window.fullscreenWidth/fullscreenHeight` in
    settings.json (0 x 0, the default, is the desktop's; picking the desktop's size saves 0 x 0 so a
    later desktop resolution is followed); picked in a window it sizes the window as before and the
    fullscreen mode is left alone. Alt+Enter, the options' switch, a launch with fullscreen saved
    and `--fullscreen` all enter at the saved mode, or the desktop's; a monitor that does not offer
    the saved mode gets the desktop's and the setting is kept. A broken value (half a size, out of
    640..15360 x 480..8640, a fraction, a string) is the desktop's, with a warning - never clamped
    into a mode no monitor has. The list always holds the desktop's own size (inserted in the list's
    order when the platform leaves it out). 0.7.12's Alt+Enter went fullscreen at the logical
    screen's 1024x768 (menu.as:111); the port's goes at the saved mode or the desktop's (E2).
- **The pointer over the bars.** The menus are laid out for 1024x768 and pillarboxed (E1); the
  scripts hide the system pointer (HideCursor, main.as:133) and draw cursor.ent, which follows the
  mouse into a bar and is drawn under it - so over a bar the player had no pointer at all. Now the
  system pointer shows while the mouse is on the image but outside the logical screen's box
  (InputMapper::PointerOverBars, half-open: viewportMin is the screen's pixel, viewportMax the
  bar's; the letterbox's top and bottom bars too; nothing when the view fills the window) and hides
  again over the image, as the scripts asked. The scripts' request stays the source of truth
  (SetCursorVisible(!cursorHidden || paused || overBars)); the logical cursor they read is
  unchanged. Not visible in a capture (the system pointer is not in the image): by hand.
Gates: check.bat clean at /W4 on every touched C++ file (the game's, the engine's WindowControl,
NativeWindowControl and test_gameruntime); Linux (WSL, GCC) test_pn_all 17 suites, 7998 checks, 0
failures (render_input 286); the engine alone on Linux, 57 of 57 suites (test_gameruntime 172), no
new warning. The Android and iOS NativeWindowControl files were syntax-checked with GCC
(-Wall -Wextra -Wpedantic, the native-surface window backend by define), not built with the NDK
or Xcode: they see only new private declarations of plain types in the shared header, and drop a
mode request as they drop any fullscreen request. Windows not built or run by this step's author; a mode switch needs
a real monitor, so the switch itself is measured only by a live run.
- **Measured live** (Ivan's laptop, 1920x1200 panel; one run of the new Penumbra.exe, the game's
  own --hold input, settings.json backed up and restored): fullscreen at the desktop's 1920x1200;
  the list's first line picked -> "at 800x600 @ 60 Hz, switched from the desktop's 1920x1200"
  (swapchain 800x600); Alt+Enter -> "Windowed at 1920x1170" (the saved window, a 1914x1153 client
  area, not rescaled); Alt+Enter -> fullscreen at the saved 800x600 again; exit -> the desktop's
  mode back. The final capture is 800x600 with the options screen filling it (a 4:3 mode, no
  bars); settings.json held fullscreenWidth/Height 800x600. After the review, DesktopMode()
  answers with the remembered desktop mode only while the window still covers the monitor it
  switched (a monitor unplugged while switched would otherwise leave a stale pointer). The pointer
  over the bars is covered by its pure tests and is Ivan's to see by hand.
Gates: Windows build zero warnings, test_pn_all once - 17 suites, 8087 checks, 0 failures; Linux
7998; engine ctest 57 of 57 (Linux); Android both ABIs build.

### Step 24 - the thin lines at the pits' edges: textures clamped, as 0.7.12 sampled them (2026-09-29)
Ivan, after playing the whole game on Windows (a 1920x1200 panel, fullscreen, the widescreen view):
"these weird lines at places that had holes leading to the bottom. There were some thin lines at the
edges, I can't explain why they happened at all."
- **Where** (Linux, lavapipe, 1920x1200 widescreen, fixed-step; the wizard put near the pits with
  the new `--spawn x,y`): a 1-2 pixel dark line hanging two or three pixels above the top of every
  ground.png and cliff_left.png quad. Over a pit's edge it reaches out past the rock into the empty
  air, where it shows most: pvp_lv1 at x 400-455 and 650-700, y 799-800 (frame 240, beside the
  first pit); level 3 at the checkpoint (`--spawn 4480,2530`, frame 200) along row 864, 10 levels
  darker than the grey sky it crosses (77 -> 67 at x 908, over the pit). In level 1 (`--spawn
  650,-800`), thin red lines in the black beside the lit walls, at the transparent edges of
  half_wall01/02 (opaque on their other side); and the HUD panels' top rows. With E8's smooth motion
  (`--fixed-step 0.0083333 --smooth on`, the wizard walking) the line changes from frame to frame
  with the camera's sub-pixel phase. At 1366x768 and 1024x768 (1:1) nothing more than 8 levels
  apart outside a torch's flame.
- **Cause.** The engine samples every texture REPEATING (VulkanImage::CreateSampler's default),
  bilinearly. 0.7.12 sampled CLAMPED: D3D9Video's start calls SetClamp(true)
  (G:Video/Direct3D9/gs2dD3D9.cpp:1677; :623-643 put D3DTADDRESS_CLAMP on U and V of every stage)
  and nothing in Ethanon turns it off. It also drew 1:1 with the half-texel alignment
  (gs2dD3D9Sprite.cpp:581), so its samples landed on texel centres. The port magnifies (1.5625
  image pixels a texel at 1200 tall, 1.40625 at 1080), so a pixel whose centre lies within about
  0.8 pixels of a sprite's edge samples up to half a texel outside the image, and repeating, that
  half is the OPPOSITE edge. ground.png is transparent in its top two rows and opaque along its
  bottom row, so its top edge drew half of the bottom row: a dark line just above the ground.
  cliff_left.png's bottom row is wider than its rock's top, so its line stretched out over the pit.
  **Proven by the switch alone**, same frames: pvp_lv1 25343 pixels change (1269 by more than 8
  levels; rows 799-800, 1266 each), the level 3 checkpoint 1550 (666; row 864), level 1 10983
  (1694), and every line segment in the crops is gone; on the level 3 walk under E8, every frame of
  the camera's move (f297-f330, one in three captured: 151-362 pixels a frame more than 8 levels
  off, on row 864). No capture of the original shows a pit, so the verdict "not in the original"
  rests on GS2D's clamp at 1:1, above.
- **Ruled out**: kPerRowVerticalDepth (no ET_VERTICAL sprite in any level or arena, so nothing is
  cut into bands there), kFloorSeamFix (collision, not drawing), E8 (the lines are in fixed-step
  frames, where it is off), and kLightPassAlphaTest, kBakedShadowsOwnLight, E9's live shadows,
  particles and fog: the clamp switch alone, with all of them as they were, removes every segment.
- **Fix** (render/TextureCache.cpp kClampToEdge, on): every image TextureCache uploads is sampled
  clamped to its edges, as 0.7.12 sampled it; off, they repeat as before. **Engine** (opt-in,
  additive): TextureRegistry::UploadRGBA takes a sampler address mode, repeat by default, so every
  other caller and game is unchanged; ReplaceRGBA still rebuilds a texture with the defaults, filter
  and wrap. No engine suite uploads through a device, so none covers the parameter.
- **Tests** (test_pn_render_textures, EdgesAreClampedAsTheOriginalSampledThem): a CPU model of a
  Vulkan linear sample at level 0 over the real ground.png and cliff_left.png. Repeating, the
  pixel on ground.png's top edge reads 127.5 of alpha (half the opaque bottom row), and cliff_left's
  line lands on columns with no rock near the top; with the cache's mode (TexturesClampToEdge()),
  neither; at 1:1 the two modes read the same.
- **At 1:1** (level 1 at 1024x768, level 3 at 1366x768, fixed-step, switch off vs on): not
  byte-identical, 992 and 936 pixels, 6 and 0 of them more than 8 levels (the torch's flame). The
  rest lie on the same edges (level 3's row 553 is its ground top), 7 levels at most: most likely
  lavapipe's level of detail at exactly 1:1 comes out a hair above 0 and mixes in the next mip
  level, whose edge texels wrapped as well. Not measured on a GPU.
- **Not fixed**: a cell of a sprite sheet still samples the cell beside it across an inner
  boundary when magnified. A scan of the 21 multi-cell sheets (texel pairs either side of an inner
  cell boundary more than 64 levels apart, premultiplied): thumbnails.png 411 of 1536; the
  STONE03A4x wall sheets 10-46 of 128-512; master_knight 45/2112, LOS-Nac-Normal 34/960, bruxo
  16/960, princess 10/960, paladin 4/960, ground2 3/3584, half_ground 1/768; cliff_left, crates,
  Tubo_Bitmap, impy, king, knight, warrior and menu_buttons none. None of it seen in the captures.
  Clamping per cell needs the fragment shader to hold the coordinate inside the cell's outer texel
  centres, an engine change of its own. A half-texel inset of the frame's UVs would avoid the engine,
  but it blurs every sprite at 1:1.
- **Dev flag** `--spawn x,y`: the wizard put at a scene point the first tick he exists in a level or
  an arena (PenumbraLayer; in --help).
Gates: check.bat clean at /W4 (engine TextureRegistry.cpp; game TextureCache.cpp, PenumbraLayer.cpp,
main.cpp; test_pn_render_textures.cpp); Linux (WSL, GCC) test_pn_all 17 suites, 9605 checks,
0 failures (render_textures 191; the tree held the MENUS agent's work in progress too). Windows not
built or run by this step.

### Step 25 - the menus fill a wide window (E1, 2026-09-29)
Ivan, having played the whole game through on Windows (a 1920x1200 panel): "the starting screen
still has black bars at the left and right, would there be a way to make it work for wide screen on
windows? What about very wide screens on android? Apart from that, inside of the game shows the
correct resolution."
- **What the four fixed-layout screens hold past their 1024x768** (extracted/app/scenes; their
  buttons, panels and thumbnails sit at fixed pixels, menu.as, videoModes.as, gameover.as):
  - menu.esc and arena_select.esc: a floor of lit white_ground.ent tiles (256x256, a normal and a
    gloss map) laid from x 0 to 2048 (y 0-1280 and 0-1024): right of the screen the original's own
    art; left of x 0, nothing.
  - videoModes.esc: a room of four 256-wide columns (wall11/wall10 of STONE03A4x7, face_no_light
    under arch01, floor03) from x 0 to 1024, half_wall02/01 framing its two ends, the crystal's
    light at x 772; nothing on either side.
  - gameover.esc: clouds.ent, a particle system emitted at x -236 (StartPoint -300 from the camera
    + 64) drifting right about 1300 px, over black with no ambient light: it already spans more
    than the screen.
- **Chosen: (a), the world continued, the UI as the original laid it.** The scripts keep their
  1024x768 screen (GetScreenSize) and everything they draw and hit-test stays where they put it.
  In a window wider than 4:3 the pillarbox already scaled that screen by the window's height, so
  the screen stays exactly where it was (the same scale, the same whole-pixel offset: 240 px at
  1920x1080, 160 at 1920x1200, 480 at 2400x1080, 560 at 2560x1080); only the side bars give way
  to the world past the screen's edges. Where the files stop, the backdrop continues each scene's
  own tile rows at their own 256-px step (menu 22 tiles, arena 16, options 40, game over none),
  far enough for a 4:1 window (1024 logical pixels each side); past 4:1 the bars come back.
  Considered: (b) a darkened, blurred or mirrored fill needs the finished frame as a texture, which
  the engine's ScreenOverlay does not offer (an engine change) for a look the scenes' own tiles give
  exactly; a capture of (a) without the backdrop (out/shots/widemenus/approach/) shows why the
  backdrop is needed: floor on the right, black on the left, and the options room in a black band.
  Cropping the top and bottom was ruled out (the logo, Back and the footer sit at the edges).
- **How** (Eth layer, then render/):
  - `MachineConfig::widenScene` (render/WideMenus::WidenScene) answers, at each load, a side
    margin and a backdrop. `Machine::SetSidesShown`, set every tick by the layer while the window
    is wider than 4:3, makes the render walk the wider rectangle. ONLY WHAT IS DRAWN CHANGES: what
    the margin brings in grows no depth range, runs no static callback, advances no particle
    system (advancing draws from the scripts' generator) and does not count toward the reseed
    after a frame with particles in view; the backdrop lives outside the buckets
    (`Scene::AddBackdrop`, ids from 1 000 000 000), so no query, collision or callback of the
    scripts sees it. Exactly 4:3, or widescreen off, is the old code path.
  - `RenderSnapshot::sideMargin` -> `View::openSides`: `ShownMin/ShownMax` (what is not barred),
    `ShownLogicalMin/Max`. CameraRig::Bars, InputMapper::PointerOverBars (Step 23's system pointer
    over the bars follows whatever bars remain) and TouchControls::WindowInsetsToLogical measure
    from the shown area; InputMapper::WarpCursor clamps to it (the menus warp the cursor every
    tick, menu.as:239, videoModes.as:61, and a clamp to the 4:3 box pulled a still mouse in the
    side to the box's edge). WindowToLogical and HudToFraction are untouched.
  - HUD: a rectangle that meets the screen's left or right edge from inside goes on to the edge of
    what is shown in that edge's colours - the fades and drawRect (util.as:379-403, the whole
    screen), showData's panel (menu.as:217, against the right edge). Nothing else is stretched;
    text and sprites stay at their 1024x768 places.
  - E16: the touch controls are laid out across the shown area (TouchInput::areaMin/areaMax), so
    the arena select's and game over's Back button hangs from the window's corner as a level's does.
    E13's pause is only in play scenes (no change); E20's phone options screen is videoModes.esc
    and widens with it.
  - `--pointer x,y`: the scripts' cursor at a window pixel, mapped every tick as the real mouse is
    (like --cursor, over the scripts' own warps), for captures that click where an item is seen.
- **The switch**: E1's own row, "Tela larga (widescreen)": on (the default) = widescreen levels and
  wide menus; off = the original's 4:3 with bars everywhere. Decided per scene load, as the levels'
  width is, so the options screen's toggle shows on the way back to the menu.
- **Measured** (Linux, lavapipe, headless; out/shots/widemenus/):
  - 4:3 identity: HEAD's binary against HEAD plus only this step's files, every fixed-layout screen
    at 1024x768 in both languages, with the default flags and with --widescreen off, plus the
    menu at 1920x1080 and the options at 2560x1080 with widescreen off: byte-identical PNGs, 18
    of 18 (identity/), and again for the final binary (12 of them). HEAD's binary is
    run-to-run identical. The wide sizes with widescreen off are the old 4:3 with bars, byte for
    byte.
  - Every screen at 1920x1200, 1920x1080, 2400x1080 and 2560x1080 in both languages (new/): the
    menu, the arena select, the options and game over fill the window; the backdrop meets the
    file's tiles with no seam (column means at the menu's x 0 within the texture's own variation).
  - Clicks at window pixels (--pointer + --hold LMOUSE, in English, clicks/): at every size the
    pointer on Credits shows the credits panel, a click on Settings opens the options screen, a
    click on the options' Back arrow returns to the menu, and a pointer in the side draws
    cursor.ent and its light there (measured at the pointer's own column: 64, 96, 192, 224 px).
    The English How to Play and Credits panels fit their 4:3 box at 21:9; no text runs into the
    sides.
  - Android (my emulator, Penumbra_API33_x86_64, cold-booted headless with `-skin 2400x1080` on
    the command line - the AVD untouched - SwiftShader; android/): the main menu fills the 20:9
    screen (adb screencap), and an adb tap at Settings' window pixel (1062, 627) opened the
    options screen - E20's phone layout - filling it too. The first boot froze under the Linux
    captures' load (qemu stopped using CPU) and was killed and booted again; the emulator's own
    Messages app raised a not-responding dialog over the game (closed through adb). Not
    measured: Windows (this step built and ran nothing there), a real phone, and where the
    options screen's picker sits after the finger lifts (seen at the tap's x, higher up).
- **Tests**: test_pn_render_input (the view at 16:10, 16:9, 20:9 and 21:9 keeps the pillarbox's
  scale and offset; logical to window and back; every menu.esc button hit at its window pixel at
  each size and at 1024x768, the sides hit nothing; the warp in a side; PointerOverBars with no bars
  left, beyond 4:1 and in a narrow window's letterbox), test_pn_render_hud (the edge rectangles;
  every scene's backdrop from the original's files; the real main menu at 16:9: New Game's button
  and the panel's title at their 4:3 places plus the 240-px margin, the panel to the window's edge,
  the same sprites in the same order with the sides barred), test_pn_render_touch (Back in the
  window's corner at 20:9, a tap on it cancels, a finger in the side is the mouse, a cutout inset
  from the window's edge), test_pn_runtime (one scene run plain, widened and barred, widened and
  shown: the same rand() draws, static callbacks and depth range; the shown run's sprites a
  superset in the same order).
- **Open, for Ivan**: the options screen's hint "Vale a partir da próxima fase" ("Takes effect from
  the next level") under the widescreen row is now also true of the menus from the next screen;
  left as it is, since changing it changes the 4:3 options screen. showData's panel reaching the
  window's edge (a choice: the alternative is a dark box ending in mid-floor). The half walls that
  framed the options room now stand inside a longer room.
Gates: check.bat clean at /W4 on every touched C++ file (eth/Machine, eth/Scene, render/WideMenus,
CameraRig, HudRenderer, InputMapper, TouchControls, HeaderCheck, PenumbraLayer, main.cpp,
script/videoModes and the four suites); Linux (WSL, GCC, no warning in game code) test_pn_all 17
suites, 9605 checks, 0 failures (render_input 821, render_hud 1629, render_touch 3622, runtime
306; the tree also held Step 24's work in progress). No engine change. Windows not built or run
by this step.

### Open
- **iOS: builds, untested** (Ivan, 2026-09-28: "leave it alone, we only need it to build"). No frame
  in the simulator (base-instance drawing); no signed device run. Not to be worked on unless asked.
- **Light, still not modelled** (Step 16; the baked shadows are Step 21's, which lists what they
  still miss): baked light on static sprites' soft edges went through the sprite's own blend;
  a translucent texel's depth write blocked fog drawn behind it; the bake used every static light
  in the scene, the port's live lights come from visible buckets (unverified at screen edges).
- **Touch (E16), for Ivan:** a combo button (Left-Left-Sword and Down-Forward-Fire are hard with a
  thumb); Versus on a phone needs pads; the layout and the placeholder art until the Magic Rampage
  buttons (not on this laptop) are given.
- **Decided (orchestrator, 2026-09-28):** the misnamed horror markers play (E15, switchable). A solo
  player counts as having a second controller while keyboard player 2 is on (E4): Versus opens and the
  AI may look for a princess that is not there - intended, harmless. The original's off-screen second
  summon and its refusal at exactly 50 mana are kept as the original (harmless quirks, not bugs that
  break play). test_pn_all's child starts are one file's, so one Smart App Control verdict per build:
  accepted as the way to run the suites.
- **E13/E14 rulings to confirm:** no pause on the end screens (g_gameFinished); the post-pause input
  filter; E14's fresh-press rule; the 15-tick click grace after refocus; the pause reads player 1's
  keys and pad only; auto-pause off under --fixed-step.
- **E13/E14 to confirm by hand (Ivan):** Esc in a level, Resume/Main menu with keys and mouse, Alt-Tab
  in a level (the pause, the music at 40%), the click back in, a real pad's A/B in the menus, Start never
  pausing. No scenario drives the pause through
  the layer (a pad-driven way out of a level for scenario 20).
- **--tour under a pause**: the tour counts engine ticks (its block is outside the pause's `if
  (pause.tick)`), so a tour with a held Esc would move on to its next scenes while the pause is
  open. Dev-only; left as is (not measured).
- **test_pn_all's child processes** (17 starts of one file per run) against rule 5's "once per
  build": run once this build as instructed, every start accepted. A full build now compiles every
  suite twice.
