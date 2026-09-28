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
- The pause's pointer now selects a row only when it moves at least 2 logical pixels between ticks (a
  window nudged under a resting mouse or a rounding wobble no longer takes the selection from the keys).
- Pause transitions (open, close, selection, with the inputs that caused them) are logged to
  %APPDATA%\Penumbra\penumbra.log, for Ivan's own testing.
Gates: test_pn_all once - 16 suites, 3719 checks, 0 failures.

### Open
- **Not modelled in the light**: the light pass's alpha test (a texel at alpha <= 1/255 took no
  light, highlight included); the highlights 0.7.12 baked into static sprites' lightmaps with the
  first frame's eye (every light is live here, E9, so they follow the camera); per-pixel depth of
  vertical sprites, which leaned back in the z-buffer in 0.7.12 (docs/spec/21).
- **pauseOnFocusLoss** has no row on the options screen (settings.json only); the column at x 255
  is full down to the Alt+Enter line.
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
