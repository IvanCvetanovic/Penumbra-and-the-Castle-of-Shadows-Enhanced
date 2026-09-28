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
| E10 | Enhanced settings on the original's options screen (videoModes.as): keyboard player 2, widescreen/4:3 levels, Português/English at once, music and effects volume in 10% steps (a new Stepper widget beside Switch), smooth motion (E8; 'Ativa/Desativa movimento suave'), saved to settings.json | the screen offered only the video-mode list, pixel shaders, window/fullscreen and the joystick layout, all forgotten at exit |
| E13 | A pause: in a level or an arena, Esc or player 1's Back freezes the game (the Machine does not tick: GetTime, fades, cooldowns and the run's clock stop, so best times exclude paused time) under 'Pausado'/'Paused' with Continuar/Resume and Menu principal/Main menu (arrows, stick/D-pad, Enter/A/Start, Esc/B/Back, mouse); Main menu feeds the original's own K_ESC for one tick; the music is ducked to 40%; it opens by itself when the window loses focus in play (settings.pauseOnFocusLoss, on; off under --fixed-step); not on the end screens, where Esc/Back still go to the menu | none: Esc in a level returned straight to the main menu and the run was lost (doLoop's escToGoToMenu) |
| E14 | Gamepad menus: in the menu, the arena select, the options screen and game over, a pad's A also confirms (JK_10) and B also cancels (JK_09); a button already held when a menu opens counts from its next press | only Start confirmed and only Back cancelled (getConfirmButtonStatus/getCancelButtonStatus, playerInput.as:267-305) |
| E15 | The five ambient horror.mp3 markers the level designer named "play_sound.ent" (level2 469, 493, 548; level3 219, 427) play once on screen like the correctly named ones (Script.hpp kPlaySoundEntFix) | setupScene.as:175 collected only "play_sound" by exact name: they never played |
| E16 | On-screen touch controls (render/TouchControls, game/data/touch_controls.json): a direction disc (left/right/down, the thumb slides) and jump/sword/fire/light as a pad's face buttons press player 1's own keys (K_LEFT/K_RIGHT/K_DOWN, K_CTRL, K_S, K_D, K_SPACE); a corner button sends K_ESC (E13's pause in play, back elsewhere); in the menus, the options, game over and the pause the buttons hide and a tap is a click there; placeholder art (tools/art/make_touch_art.py), the look and layout data-only; settings.touchControls auto/on/off (auto = on under PENUMBRA_MOBILE), --touch on the desktop (the held mouse is the finger) | keyboard and joysticks only |
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

### Open
- **Light, still not modelled** (Step 16): the live shadows (E9) darken the ambient too, where
  0.7.12's baked shadow removed only its own light (the menu barrel's shadow is near-black, the
  original's purple); baked light on static sprites' soft edges went through the sprite's own blend;
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
