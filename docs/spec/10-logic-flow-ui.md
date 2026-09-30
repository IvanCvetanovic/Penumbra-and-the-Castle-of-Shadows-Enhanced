# Penumbra e o Castelo das Sombras: game flow, scenes and UI porting spec

Scope: `main.as`, `setupScene.as`, `menu.as`, `videoModes.as`, `gameover.as`, `scores.as`, `timer.as`, `interface.as`, `messageManager.as`, `cameraManager.as`, `events.as`, `environment.as`. I also read the helpers they depend on: `util.as`, `playerInput.as`, `switch.as`, `constants.as`, `eth_util.as`, and `controlCharacters.as:270-469` and `:645-732` (death, lives, respawn, king). All paths are relative to `<Desktop>\Penumbra-and-the-Castle-of-Shadows-Enhanced\extracted\app`. Lines 1-41 of every file are the LGPL header. I checked that line 41 is `*/` in all 16 files.

**How each engine claim is sourced:**
- **[2010-bin]**: from the string and signature table of `machine.exe` or `GameSpace.dll`, which is the shipped 2010 runtime.
- **[2013-src]**: from `<Desktop>\Magic-Portals-Remake\reference\ethanon` (Dec 2013). It may differ from 2010.
- **[data]**: inferred from the scenes and scripts.
- **[script]**: read directly from the `.as` files.

---

## 0. Engine execution model the scripts assume (working hypothesis)

**2010 script API signatures [2010-bin]**, the ones that matter here:
- `void DrawText(const vector2 &in, const string &in text, const string &in font, const float size, const uint color)`. In 2010 the font is a system TrueType face name and the size is explicit. `GameSpace.dll` imports `D3DXCreateFontA`. The 2013 API is different (bitmap fonts, no size).
- `int AddEntity(const string &in, const vector3 &in, const float angle)` and `int AddEntity(const string &in, const vector3 &in, ETHEntity@ &out)`.
- `bool CollideDynamic(const ETHEntity &in, ETHEntity@ &out)` is **native** in 2010. In 2013 it became script code in `documentation/Sample-Projects/Sample-project/Collide.angelscript:3-84`.
- `void LoadScene(const string&, const string& onLoaded, const string& onUpdate[, const vector2& bucketSize])`.
- `bool SaveScene(const string&)`.
- `void PositionBackgroundImage(const vector2 &in min, const vector2 &in max)`, `SetBackgroundAlphaAdd()`, `bool SetBackgroundImage(const string&)`.
- Joysticks go through WinMM `joyGetPosEx`. Fonts go through `D3DXCreateFontA`. Audio goes through Audiere (`GS_AUDIO_SAMPLE::Play/Stop/SetLoop/SetVolume`).

**Frame order (hypothesis: the 2013 `ETHEngine::Update`/`ETHScene::Update` order, `ETHEngine.cpp:141-190`, `ETHScene.cpp:459-497`):**
1. A `LoadScene` requested in the previous frame is executed now. The request is deferred, and the last request in a frame wins. Loading builds the scene, sets the camera to (0,0) [2013-src `ETHScriptWrapper.Scene.cpp:607`], clears the top-layer draw list, then runs the onLoaded ("preLoop") function. Draws queued by the preLoop therefore appear in the first frame.
2. Callbacks of all **dynamic** (`static="0"`) entities that have an `ETHCallback_<name>` run, visible or not.
3. The scene loop function runs (`levelLoop`, `menuLoop`, and so on).
4. Visible buckets are computed from the camera the loop just set, plus a border ring if `SetBorderBucketsDrawing(true)`. Callbacks of **static** (`static="1"`) entities in those buckets run.
5. The scene renders, then the "top layer": every `DrawText`/`DrawSprite`/`DrawShapedSprite`/`DrawRectangle` queued this frame, in call order, in screen space, over the scene.

**Evidence for this model:**
- `machine.exe` contains `ETHRenderEntity::RunCallbackScript`, so callbacks are tied to render entities.
- Level1's music trigger `play` is static and sits at (3981,-598), far from the spawn at (410,-42). The music therefore starts only when the camera reaches it.
- Every trigger entity is static: `play`, `help`, `story`, `checkpoint.ent`, `next_level`, `event01`.
- The actors are dynamic: `bruxo.ent`, `princess.ent`, `clouds.ent`, `falling_bridge.ent`, `cursor.ent`, `thumbnail`, `picker`.
- Checkpoint restore only works if the dynamic bruxo runs while off-screen: the camera starts at (0,0) and the bruxo is far away.

**Consequences for draw order:**
- The HUD (drawn from the bruxo callback, step 2) is drawn **under** `doLoop`'s `fadeIn` rectangle, so it fades in with the scene.
- Messages and the timer (step 3, after `fadeIn`) are **not** faded.
- Story text and the `next_level` fade-out (step 4) are drawn over everything else.
- The callback name is `"ETHCallback_" + EntityName with the extension removed` [2013-src `ETHASUtil.cpp:63-68`]. Scene instances can be renamed (`spawn`, `help`, `story`, `play`, `next_level`, `environment`, `play_sound`, `event01`, `summon`, the menu buttons), and the renamed name is what matters.
- `GetEntityArray(name)` compares the full EntityName exactly [2013-src `ETHBucketManager.cpp:488-503`]. That is why the scripts pass `"spawn"` but `"flashlight.ent"`.

**Engine primitives:**
- `GetTime()`: milliseconds since application start, uint, monotonic across scene loads. Saved custom data (for example in `checkpoint.esc`) holds absolute `GetTime()` values, so **the port's clock must not reset per scene**.
- `UnitsPerSecond(v) = v * lastFrameElapsedMs / 1000` [2013-src `ETHScriptWrapper.System.cpp:95-98`].
- `GetFPSRate()`: measured fps, which can be 0 at start.
- `rand(n)`: in 2013 it is `MTRand::randInt(n)`, which is **inclusive** of n [2013-src `Randomizer.cpp:31-34`]. The 2010 behaviour is unknown.
- Key states: `KS_HIT` on the first frame a key is down, then `KS_DOWN`, then `KS_RELEASE` for one frame, then `KS_UP`. `KeyDown()` is true for HIT or DOWN [2013-src `KeyStateManager.cpp:33-57`].
- Joysticks (WinMM, [2013-src `WinInput.cpp:615-700`], same DLL imports in 2010):
  - `GetJoystickXY` is normalized to -1..1, with |v| < 0.01 set to 0.
  - `JK_LEFT/RIGHT/UP/DOWN` are derived from X/Y crossing ±0.8.
  - `JK_01..JK_32` are raw WinMM button bits 1..32.
- `SetCursorPos` is deferred to the next input update and truncated to int [2013-src `WinInput.cpp:443-455`].
- Audio: one Audiere stream per sample name, so a sample cannot overlap itself.
  - `PlaySample` on a playing sample **restarts** it and applies the stored per-sample volume [2013-src `AudiereSample::Play`, `AudiereAudio.cpp:231-246`].
  - `LoadMusic` streams; `LoadSoundEffect` loads into memory.
  - Loading an already-loaded name is a no-op, so volume and loop persist.
  - Whether `LoadScene` releases audio: 2013 releases all resources only when the new file name differs from the current one [2013-src `ETHScriptWrapper.Scene.cpp:572-578`]. 2010 is unknown (open question).
- `DrawRectangle(pos,size,c0,c1,c2,c3)` corner colours are c0 top-left, c1 top-right, c2 bottom-left, c3 bottom-right [2010 `data/defaultVS.cg` comments]. The fill is bilinearly interpolated.
- `DeleteEntity(e)`: `IsAlive()` becomes false immediately. The handle stays usable in the same frame (`main.as:186-187` reads the position after deleting).
- The camera position is the world coordinate of the screen's top-left corner. World and screen use +x right and +y down.
- World to screen for an entity is `(x,y) + ZAxisDirection*z` [2013-src]:
  - `menu.esc` and `arena_select.esc` use ZAxisDirection (0,-1).
  - Levels, arenas, `videoModes.esc` and `gameover.esc` use (0,0).
- Default bucket size is 256x256 [2013-src `ETHTypes.h:58`]. Bucket = `floor(pos/bucketSize)`.
- `TESTING` is defined only when the player is launched with `-testing` [2010-bin]. **All `#if TESTING` code is off in release; ignore it.**

---

## 1. Script globals (persist across all `LoadScene`s) - `main.as:43-72`

| Global | Type / init | Meaning |
|---|---|---|
| `g_frameTimers` | dictionary | Per-entity animation `frameTimer` keyed `"id"+GetID()`. Cleared by `setupScene` (`setupScene.as:165`). |
| `g_gameData` | enmlFile | `data.enml`, parsed once in `main()` (`main.as:135-136`). |
| `g_spawn`, `g_sounds` | ETHEntityArray | `spawn` markers and `play_sound` markers of the current scene, filled in `setupScene`. |
| `g_messages` | MessageManager | Section 9. |
| `g_camera` | CameraManager | Section 8. |
| `g_timer` | Timer | Run timer (`timer.as:54-80`). |
| `g_lives` | int = 0 | Lives. Set from `data.enml` `global.lives` = **13**. |
| `g_numNpcs` | uint | Debug NPC count. Incremented by enemy callbacks, zeroed in `doLoop`. Display is TESTING-only. |
| `g_levelStartTime` | uint | `GetTime()` at `setupScene`, used as the fade-in origin. |
| `g_gameFinished` | bool | Set when the king dies (`controlCharacters.as:670`) or on PvP victory (`setupScene.as:356`). |
| `g_exp[2]` | int (0,0) | Experience per player. |
| `g_charLevel[2]` | int (8,8) initially, 1 after `resetData` | Character level per player. The initial 8 is never observed. |
| `g_newRecordTime` | uint 0 | Final campaign time. 0 means not yet recorded. |
| `g_castingLight[2]` | bool | Light-spell-active flag (other dimension). |
| `g_pvpPoints[2]` | int | PvP score. Can go negative. |
| `g_comboManager[2]` | Combo | Other dimension. |
| `g_enablePS`, `g_windowed`, `g_controls` | Switch (`videoModes.as:43-45`) | Session-only settings, never saved. All default to 0. |
| `g_gameOverStartTime` | uint (`gameover.as:43`) | Fade-in origin for the game-over screen. |

**`resetData()` (`main.as:62-72`)**, in this order:
1. `g_exp = {0,0}`.
2. `g_charLevel = {1,1}`.
3. `g_lives = data.enml global.lives` (13).
4. `g_timer.start()`, which sets startTime = `GetTime()`.
5. `g_gameFinished = false`.
6. `g_newRecordTime = 0`.
7. `g_castingLight = {false,false}`.
8. `g_pvpPoints = {0,0}`.

Called from `newGame` (`main.as:101`) and `goToPvp` (`setupScene.as:244`).

**Constants (`constants.as`):**
- `APPLICATION_TITLE = "Penumbra e o Castelo das Sombras - Ethanon Engine"`.
- `GRAVITY = 1200`.
- `DEAD_FADE_OUT_TIME = 3000`, `LIVE_FADE_IN_TIME = 3000`.
- `SIZE_TOLERANCE = 76`.
- `MAX_PLAYERS = 2`, `MAX_PVP_POINTS = 3`.
- `MAIN_CHARACTER_ENTITY0 = "bruxo.ent"` (player 0), `MAIN_CHARACTER_ENTITY1 = "princess.ent"` (player 1).
- `isAMainCharacter(e)` is true when the name is one of those two.
- `STANDING=0`, `RIGHT=0 LEFT=1 DOWN=2 UP=3` (`util.as:238-241`).

UI colour everywhere: RGB **(203,203,228)**.

---

## 2. Scene state machine: every `LoadScene` call

| # | Source | Target scene | onLoaded | onUpdate | Bucket | Trigger |
|---|---|---|---|---|---|---|
| 1 | `main.as:126` | `scenes/menu.esc` | `menuPreLoop` | `menuLoop` | **(1024,256)** | Boot |
| 2 | `main.as:116` | `scenes/level{1,2,3}.esc` | `setupScene` | `levelLoop` | 256 | `newGame("CAMPAIGN")`. Level is 1, or 2 if K_2 is held, or 3 if K_3 is held (K_3 wins). |
| 3 | `main.as:120` | `scenes/` + `pvp_lvN.esc` | `setupScene` | `pvpLoop` | 256 | `newGame(arena)` |
| 4 | `main.as:224` | `scenes/` + next_level.`name` (`level2.esc`, `level3.esc`) | `setupScene` | `levelLoop` | 256 | 3 s after P1 pressed down at a `next_level` |
| 5 | `setupScene.as:245` | `scenes/arena_select.esc` | `menuPreLoop` | `menuLoop` | **256** (no vector passed) | `goToPvp()` from the menu "versus" button |
| 6 | `menu.as:309` | `scenes/videoModes.esc` | `screenModesPreLoop` | `screenModesLoop` | 256 | Confirm on "opcoes_de_video" |
| 7 | `menu.as:387` (`goToMenu`) | `scenes/menu.esc` | `menuPreLoop` | `menuLoop` | (1024,256) | Cancel button, or the video-modes back arrow. **No-op if already in `scenes/menu.esc`.** |
| 8 | `menu.as:398` (`escToGoToMenu`) | `scenes/menu.esc` | `menuPreLoop` | `menuLoop` | (1024,256) | K_ESC hit during any level or arena (every `doLoop` frame) |
| 9 | `controlCharacters.as:438` | `GetSceneFileName()` (same file) | `setupScene` | `levelLoop` | 256 | Campaign main character dead for 3 s, lives ≥ 0 after decrement, no checkpoint |
| 10 | `controlCharacters.as:440` | same file | `setupScene` | `pvpLoop` | 256 | PvP: either player dead for 3 s (always reloads) |
| 11 | `controlCharacters.as:444` | `scenes/checkpoint.esc` | `setupScene` | **`levelLoop`** | 256 | Dead, lives ≥ 0 (or PvP), and the player entity has `hasCheckpoint` |
| 12 | `controlCharacters.as:449` | `scenes/gameover.esc` | `gameOverPreLoop` | `gameOverLoop` | 256 | Dead, `g_lives < 0` after decrement, not PvP |

- The game never loads `level1→2→3` by itself: the chain is data-driven through `next_level.name`. `level1.esc` has `next_level{name="level2.esc"}` at (2136,-1991,-80). `level2.esc` has `next_level{name="level3.esc"}` at (320,-1864,-52). `level3.esc` ends with the king (Section 11).
- `main.as:127` has a commented-out direct `checkpoint.esc` load.
- Script globals survive every load. Entities, and therefore their custom data, do not, except through `checkpoint.esc`.

---

## 3. Boot - `main()` (`main.as:124-145`)

1. `LoadScene("scenes/menu.esc","menuPreLoop","menuLoop",vector2(1024,256))`. This is deferred, so it executes next frame.
2. `HideCursor(true)`. The OS cursor is hidden for the whole session, and the `cursor.ent`/`picker` entity is the visible pointer.
3. Read `GetStringFromFile(GetAbsolutePath("data.enml"))`, then `g_gameData.parseString`. The path is next to the exe.
4. `g_gameData.getInt("global","lives",g_lives)` gives 13.
5. `SetWindowProperties(APPLICATION_TITLE, 1024, 768, windowed=true, true, PF32BIT)`. The 5th bool is vsync in 2013 [2013-src]; unverified for 2010.

**ENML semantics** [2013-src, per `Magic-Portals-Remake/docs/ethanon-formats.md` §6.1]:
- `key = value;` with leading whitespace skipped and trailing whitespace kept. Values may span lines.
- The files use CRLF, so multi-line values contain `\r\n`; the port should normalize to `\n`.
- Typed getters **leave the out-variable unchanged** when the key is missing. This matters for Section 12 bugs #10-#11.
- `get()` returns `""` when missing.
- Text is Latin-1 (cp1252 rendering).

---

## 4. Menu scene (`scenes/menu.esc`, `menu.as`)

### 4.1 Scene content (`menu.esc`)

Scene properties: ambient (0.2,0,0.2), lightIntensity 2, ZAxisDirection (0,-1).

The buttons are dynamic, collidable entities with renamed names. They all use `menu_buttons.png` (512x512, cut 1x8, so each frame is 512x64). The frame is the button label, and the button has no callback of its own.

| Entity name | Frame | Label | Pos (x,y,z) | Hit box (world, cbox pos (-18,4), size 369x25) |
|---|---|---|---|---|
| `novo_jogo` | 0 | Novo jogo | (434,209,10) | x 231.5-600.5, y 200.5-225.5 |
| `versus` | 6 | Versus | (484,267,10) | x 281.5-650.5, y 258.5-283.5 |
| `como_jogar` | 5 | Como jogar | (399,323,10) | x 196.5-565.5, y 314.5-339.5 |
| `melhores_tempos` | 2 | Melhores tempos | (385,385,10) | x 182.5-551.5, y 376.5-401.5 |
| `opcoes_de_video` | 4 | Configurações | (432,442,10) | x 229.5-598.5, y 433.5-458.5 |
| `creditos` | 1 | Créditos | (535,503,10) | x 332.5-701.5, y 494.5-519.5 |
| `sair` | 3 | Sair | (652,567,10) | x 449.5-818.5, y 558.5-583.5 |

- Frame 7 is blank.
- Sprites are type 0 (centred), drawn at y - z because ZAxisDirection is (0,-1).
- The rest of `menu.esc` is decoration:
  - 36 static `white_ground.ent` tiles, 256² each, over (128..1920, 128..1152).
  - `gamelogo.ent` (type 2, 567x145) at (340,470,294), drawn at screen y 176.
  - `devil.ent` at (112,292) and (539,301).
  - `ground_fire.ent` torches (with light) at (110,326), (590,594), (537,331).
  - 14 `barrel.ent`.
  - `green_light_menu.ent` light at (237,44,16): range 284.5, colour (0.3,1,0.7).
  - `blue_light.ent` at (868,577,20) and (897,176,20): range 232, colour (0.3,0.7,1).
  - `fog_menu.ent` particles at (329,718,314).
  - **`cursor.ent`** at (785,335,10): dynamic, collidable, cbox (0,0) size 31x31x148, point light range 181 colour (1,0.7,1), plus a `flash.bmp` spark particle system.

### 4.2 `menuPreLoop()` (`menu.as:50-72`)

Also used for `arena_select.esc`. In order:
1. `loopMenuSong()`: `LoadMusic("soundfx/menu.mp3")`, `PlaySample`, `LoopSample(true)`.
2. `LoadSoundEffect("soundfx/help.mp3")`, then `SetSampleVolume(help,0.3)`.
3. `LoadSoundEffect("soundfx/newgame.mp3")`, `LoadSoundEffect("soundfx/fail.ogg")`.
4. `LoadSprite("interface/joystick.png")`.
5. `SetBorderBucketsDrawing(false)`.
6. `drawRect(0xFF000000)`: a full-screen black rectangle for the first frame.
7. `cursor = SeekEntity("cursor.ent")`. If found: `AddStringData("lastButton","none")` and `AddUIntData("menuStartTime",GetTime())`.

### 4.3 `menuLoop()` (`menu.as:92-95`)

Calls `waitForInputToMenu()`. If P1's cancel button is `KS_HIT`, this calls `goToMenu()`, which does nothing while in the menu. In practice it only affects `arena_select.esc`: ESC or cancel returns to the menu.

### 4.4 `ETHCallback_cursor(e)` (`menu.as:232-360`)

This is the whole menu and arena-select logic. It runs every frame because the cursor is dynamic.

1. `detectJoysticks()` (`menu.as:74-90`), every frame:
   - `input.DetectJoysticks()`.
   - If joystick 0 is detected: `DrawSprite("interface/joystick.png",(W-180,0),ARGB(150,255,255,255))`. The sprite is 180x130, so at 1024 wide this is (844,0).
   - If joystick 1 is detected: the same sprite at (W-180,130).
2. `cursorPos = input.GetCursorPos()`, the window-client mouse position read **before** moving.
3. `input.SetCursorPos(GetCursorAbsolutePos() + getPlayerXYAxis(0)*5)`. The keyboard arrows or P1's stick move the OS mouse 5 px **per frame**, so speed depends on frame rate.
4. `e.SetPositionXY(cursorPos)`. The entity lags the OS mouse by one frame.
5. **If `CheckCustomData("newGame")==DT_NODATA`** (normal state):
   - If `CollideDynamic(e, handle)` finds a non-static collidable entity whose absolute 3D box overlaps the cursor's (strict `<=` separation test on x, y and z, surrounding 3x3 buckets [2013-src script version]):
     - `name = handle.GetEntityName()` and `confirmed = (getConfirmButtonStatus(0)==KS_HIT)`.
     - Branch by name:
       - `creditos`: `showData("Créditos", creditos)`.
       - `melhores_tempos`: `showData("Melhores tempos", getRecordTimeList())`. This reads `hs.enml` from disk every frame.
       - `como_jogar`: `showData("Como Jogar", como_jogar)`.
       - `versus`:
         - If `hasASecondController()`: `showData("Jogador versus Jogador", versus)`. On confirm: `goToPvp()`, which is `resetData()` then load `arena_select.esc` (row 5).
         - Otherwise: on confirm `PlaySample("soundfx/fail.ogg")`. The message is `"É necessário ao menos um joystick\n para jogar neste modo."`. If joystick 0 is detected, append `"\n\nJá há um joystick plugado.\nMude as opções de entrada no menu\nde configurações para poder\nutilizar o teclado e o joystick\npor 2 jogadores."`. Then `showData("Jogador versus Jogador", message)`.
       - `novo_jogo`: `showData("Novo jogo", novo_jogo)`. On confirm: `AddUIntData("newGame",GetTime())`, `AddStringData("scene","CAMPAIGN")`, `PlaySample("soundfx/newgame.mp3")`.
       - `sair`: `showData("Sair do jogo","")`. On confirm: `Exit()`.
       - `opcoes_de_video`: `showData("Configurações", config)`. On confirm: load `videoModes.esc` (row 6).
       - `thumbnail` (arena select):
         - `number = handle.name`, `title = handle.title`, `extra = ""`, `allow = true`.
         - If the thumbnail has `score` and `score <= getGetBestTime()`: `allow = false` and `extra = "\n\nÉ necessário terminar o jogo\nem menos de " + getTimeString(score) + " para\nliberar esta arena."`.
         - `showData(title, data.enml global["arena"+number] + extra)`.
         - Confirm and allowed: `newGame=GetTime()`, `scene="pvp_lv"+number+".esc"`, play `newgame.mp3`.
         - Confirm and locked: play `fail.ogg`.
     - Hover sound: if `name != e.lastButton`, `PlaySample("soundfx/help.mp3")` (volume 0.3). Then `lastButton = name`. `lastButton` is **not** reset when the cursor leaves all buttons, so leaving a button and returning to it is silent.
   - `showToggleFullscreenMessage()` (Section 5.3). This is drawn only in the normal state.
6. **Else** (a new game is pending):
   - `bias=0`. If `fadeOut(e.newGame, bias)` has finished (3000 ms elapsed), call `newGame(e.scene)`.
   - `SetSampleVolume("soundfx/menu.mp3", 1-bias)`. On the frame the fade completes, bias stays 0, so the volume snaps back to 1.0.
   - `loadingMessage()`.
7. `fadeIn(e.menuStartTime)`. This runs every frame, and the 3 s black fade-in is drawn over everything the callback drew.

**Scene switch on confirm.** Confirming a new game or an arena does not switch immediately:
- There is a 3 s fade-out to black while "Carregando..." is shown and the menu music fades.
- The cursor and hover logic are disabled during the fade.
- Then `newGame()` runs.

**`newGame(sceneName)` (`main.as:99-122`):**
1. `resetData()`.
2. If `IsPixelShaderSupported()`, `UsePixelShaders(true)`.
3. For `"CAMPAIGN"`:
   - `level=1`; if K_2 is `KS_DOWN` (held, not just hit), level 2; if K_3 is `KS_DOWN`, level 3.
   - If K_PAGEUP is `KS_DOWN`, `g_charLevel = {15,15}` (a cheat).
   - These keys are sampled on the frame the 3 s fade ends.
4. Load the level (row 2) or the arena (row 3).

**`showData(title, content)` (`menu.as:217-230`).** At 1024x768:
- `rectSize = (W*(1-0.618), H) = (391.168, 768)` and `rectPos = (W-rectSize.x, 0) = (632.832, 0)`.
- `DrawRectangle(rectPos, rectSize, c0, c0, c1, c1)` with `c0=ARGB(190,0,0,0)` and `c1=ARGB(55,0,0,0)`. This is a vertical gradient on the right 38.2 % of the screen, dark at the top and lighter at the bottom.
- `shadowText(rectPos+(10,20) = (642.8,20), title, "Arial Narrow", 40, 255, 203,203,228)`.
- `shadowText(rectPos+(10,70) = (642.8,70), content, "Arial Narrow", textSize, 255, …)`, where `textSize = (H <= 700) ? 20 : 25`, so 25 at 768.

**Menu texts** (`menu.as:123-215`; heredocs, leading and trailing newline trimmed by AngelScript [AS manual], line endings CRLF):
- `como_jogar`: "Controles\n Movimento: < e >\n Pulo: ^ ou CTRL (joystick 3)\n Ataque/espada: S (joystick 4)\n Ataque/fogo: D (joystick 2)\n Acionar Luz: ESPAÇO (joystick 1)\n\n Detectar joysticks: segure J\n\n2 jogadores:\n Se houver um joystick plugado,\n o personagem principal ganha \n a habilidade de invocar a\n criatura que pode ser controlada\n pelo 2º jogador.\n\n Para invocar a criatura basta pres-\n sionar START no 2º controle.\n\n A mágica de invocação custa 50 mana\n e uma vida, mas possibilitará que 2\n jogadores lutem lado-a-lado.\n\n Somente o jogador 1 pode pegar\n checkpoints e passar de fase."
- `config`: "Ajuste as opções de vídeo como\nresolução de tela e uso de shaders.\n\nConfigure também o uso do joystick\npelo segundo jogador."
- `creditos`: "André Santee\n -Programação\n -Scripting e mecânica do jogo\n -Efeitos especiais\n -Game design\n\nArthur Santee\n -Modelagem 3D\n -Game design\n\nGabriel Duarte\n -Trilha sonora\n  gabrielduarte.wordpress.com\n\nApproaching Thunderstorm\n -www.freesoundtrackmusic.com\n\nAgradecimentos especiais:\n-James Hastings-Trew por ter nos\n cedido algumas de suas texturas\n  planetpixelemporium.com\n-José Rodolfo Ortale\n-Rafael \"Pet\" Alencar\n-Taina Monclaire"
- `novo_jogo`: "Penumbra não é um bom lugar\npara se viver. Quando acaba a\nneblina, chega a tempestade.\n\nEras atrás, um mago muito\npoderoso tomou o controle da\nsombria terra chamada Penumbra,\nnomeando-se o rei deste mundo.\n\nPor diversos séculos, a Ordem dos\nBruxos de Penumbra têm buscado\nderrubar o tirano.\n\nApós incontáveis anos treinando\nas artes da mágica e da luta você\nfoi escolhido pela Ordem para \nir ao Castelo das Sombras\nde Penumbra e assassinar o rei,\nacabando com um reinado que dura\nanos."
- `versus`: "Escolha uma arena e dispute uma\npartida contra outro jogador.\n\n-Quem derrotar o outro ganha 1 ponto\n-Vence quem fizer 3 pontos primeiro\n-A partida acaba se a diferença no\nplacar exceder 3 pontos"

### 4.5 Arena select (`scenes/arena_select.esc`)

Scene properties: ambient (0.2,0,0.2), ZAxisDirection (0,-1), default bucket size.

Content: 32 `white_ground` tiles, `green_light_menu.ent` at (185,292,18), `ground_fire.ent` at (460,309), `cursor.ent` at (302,436,10), and six **`thumbnail`** entities. Thumbnails are dynamic, type 2, use `thumbnails.png` (256², cut 4x4, 64x64 frames), have emissive (0.8,0.8,0.8), cbox pos (0,32) size 64x64x68, and z=4.

| Arena `name` | Frame | `title` | Pos | `score` (unlock threshold) | Hit box x / y |
|---|---|---|---|---|---|
| 4 | 3 | Vale | (110,232) | none | 78-142 / 232-296 |
| 2 | 1 | Inferno | (191,213) | none | 159-223 / 213-277 |
| 1 | 0 | Obelisco | (276,212) | none | 244-308 / 212-276 |
| 3 | 2 | Cova | (361,220) | none | 329-393 / 220-284 |
| 6 | 5 | Neblina | (436,208) | 900000 (15:00) | 404-468 / 208-272 |
| 5 | 4 | Templo Sagrado | (510,219) | 720000 (12:00) | 478-542 / 219-283 |

- With the 31x31 cursor box, neighbouring hit zones overlap. For example, arenas 1 and 3 both hit for cursor x in 313.5-323.5. Which one wins depends on bucket and insertion order.
- Arena descriptions come from `data.enml` `global.arena1..arena6` (lines 90-120).

**`ETHCallback_thumbnail(e)` (`menu.as:362-372`):** if `score` exists and `score <= getGetBestTime()`, call `e.SetColor((0.05,0.05,0.05))` every frame, which darkens it. Unlocking needs a best campaign time strictly below the threshold. With the shipped `hs.enml`, where every entry is 3599000, arenas 5 and 6 are locked.

### 4.6 Video modes (`scenes/videoModes.esc`, `videoModes.as`)

Scene content: an arch and wall dungeon backdrop, plus a **`picker`** entity at (213,165,10). The picker is dynamic, collidable, has a light (range 181, colour (1,0.7,1)) and particles.

**`screenModesPreLoop`** (`:52-56`): `loopMenuSong()` and `LoadSprite("interface/arrow_button.png")` (123x92).

**`ETHCallback_picker`** (`:58-63`): `SetCursorPos(abs + axis(0)*5)`, then `SetPositionXY(GetCursorPos())`. Unlike the menu cursor, this reads after setting.

**`screenModesLoop`** (`:85-136`), per frame:
1. `shadowText((30,30), "Opções de vídeo", "Arial Narrow", 40, 255, UI colour)`.
2. Mode list:
   - Start at `cursor=(30,100)`, font size 25, column width 200.
   - For each `t < GetVideoModeCount()`: `mode=GetVideoMode(t)`. Skip modes that are not `PF32BIT`, have width < 800, or have height < 600.
   - Hover is `mouse.x ∈ (cursor.x, cursor.x+200)` and `mouse.y ∈ (cursor.y, cursor.y+25)`, both strict. Hovered alpha is 255, otherwise 100.
   - Hover plus confirm hit: `SetWindowProperties(title, mode.width, mode.height, Windowed(), true, PF32BIT)`.
   - Draw `"<w>x<h>x32"` with `shadowText(cursor, …, "Arial Narrow", 25, alpha, UI colour)`.
   - Then `cursor.y += 25`. If `cursor.y > H`, move to the next column: `x += 200`, `y = 100`.
   - The list can contain duplicates (one entry per refresh rate).
3. `g_enablePS.put((255,100), "Arial Narrow", 25, 256)`. Options: row 0 "Ativa pixel shaders" at y=100, row 1 "Desativa pixel shaders" at y=125. Then `UsePixelShaders(g_enablePS.getCurrent()==0)` every frame.
4. `g_windowed.put((255,170), …)`. Row 0 "Janela" at y=170, row 1 "Tela-cheia" at y=195.
5. `g_controls.put((255,260), …)`, an image switch:
   - Row 0: `interface/input_options1.png` (300x72; keyboard for Jogador 1, joystick for Jogador 2) at y=260.
   - Row 1: `input_options2.png` (joystick for both players) at y=332.
   - Each image is drawn at `drawCursor+(30,0)` with `ARGB(alpha,255,255,255)`.
   - The hit area is the 300x72 image size starting at `drawCursor` itself, not offset by 30.
   - The text labels `"2º joystick para jogador 2"` and `"1º joystick para jogador 1"` are **not** drawn for image switches. Only the brackets are.
6. `showToggleFullscreenMessage()`.
7. `waitForInputToMenu()`.
8. If `putBackButton((500,40))`, call `goToMenu()`:
   - Draws `arrow_button.png` at (500,40) with `ARGB(alpha,203,203,228)`.
   - Alpha is 255 when the mouse is strictly inside the 123x92 rect, otherwise 100.
   - Returns true on confirm hit while hovered.

**`Switch.put(pos,font,size,width)`** (`switch.as:65-102`), for rows t=0,1:
- `str = "[" + (current==t ? "" : " ") + "] "`. The selected row shows `"[] "` and the unselected row `"[ ] "`, followed by the label for text switches.
- Row size is `(width,size)` for text switches, or the sprite size for image switches.
- Hover (strict inequalities) raises alpha to 200 (otherwise 100), and a confirm hit sets `current = t`.
- Drawn alpha is 255 if selected, otherwise the hover alpha.
- `shadowText(drawCursor, str, font, size, alpha, 203,203,228)`.
- The image, if any, is drawn at `drawCursor+(30,0)`.
- `drawCursor.y += rowSize.y`.
- For image switches, `LoadSprite(image[t])` is called every frame.

---

## 5. Shared UI helpers (`util.as`, `menu.as`)

### 5.1 Text and rectangle helpers

- **`shadowText(pos,text,font,size,a,r,g,b)`** (`util.as:450-455`): `DrawText(pos + (0.1*size, 0.1*size), text, font, size, ARGB(a/2,0,0,0))`, then `DrawText(pos, text, font, size, ARGB(a,r,g,b))`. `a/2` is integer division.
- **`drawRect(c)`** (`:374-377`): a full-screen `DrawRectangle` in a single colour.
- **`fadeIn(start)`** (`:379-390`): `e = GetTime()-start`. If `e < 3000`, draw a full-screen rectangle `ARGB(uint8((1-e/3000)*255),0,0,0)` and return false; otherwise return true.
- **`fadeOut(start, out bias)`** (`:392-404`): if `e < 3000`, `bias = e/3000`, draw `ARGB(uint8(bias*255),0,0,0)` and return false. Otherwise return true **without writing bias**, so callers keep `bias=0`.
- **`loadingMessage()`** (`:457-460`): `shadowText((20, H-70), "Carregando...\n", "Arial Narrow", 60, 255, UI colour)`. At 768 high this is (20,698).

### 5.2 `isInScreen`

**`isInScreen(e)`** (`:96-110`): true when the entity position lies inside `[cam-76, cam+screen+76]` on both axes. Used for spawning, `play_sound`, and removing falling bridges.

### 5.3 Fullscreen toggle message

**`showToggleFullscreenMessage()`** (`menu.as:97-121`):
1. `shadowText((0, H-15), "Pressione Alt+Enter para trocar entre fullscreen e modo janela", "Arial Narrow", 15, 255, UI colour)`.
2. `toggle = (Windowed() && g_windowed==1) || (!Windowed() && g_windowed==0)`. Index 0 means windowed.
3. If `(K_ALT is KS_DOWN and K_RETURN is KS_HIT) || toggle`:
   - `windowed = !Windowed()`.
   - `SetWindowProperties(title, W, H, windowed, true, PF32BIT)`.
   - `g_windowed = windowed ? 0 : 1`.

`getConfirmButtonStatus` ignores RETURN while ALT is held, so Alt+Enter never confirms a button.

---

## 6. Level and PvP scenes (`setupScene.as`)

### 6.1 `setupScene()`: onLoaded for every level, arena and checkpoint (`:114-224`)

In order:
1. If `g_enablePS==1`, `UsePixelShaders(false)`.
2. `LoadSprite` for `interface/{hp,mp,xp,rail,skull_interface,frame,blend,joystick}.png` and `entities/gameover.png`.
3. `LoadSoundEffect`:
   - `creature_show_up.mp3`, `light_spell.mp3`, `cast_fire_spell.ogg` (x3), `respawn.mp3`, `paladin_appear.mp3`, `thunder.mp3`, `horror.mp3`, `fall.ogg`, `laugh_king.mp3`, `pvp_win.ogg`, `death_king.ogg`, `checkpoint.mp3`, `creature_show_up.mp3` (again), `creature_dying.mp3`, `explosion.ogg`, `help.mp3`, `hit01.ogg`, `potion_pick.ogg`, `minion.ogg`, `sword_combo.ogg`, `sword01.mp3`, `vanish.ogg`, `jump01.ogg`, `jump02.ogg`, `blast_attack.ogg`.
   - Then `SetSampleVolume(blast_attack.ogg, 0.5)`.
   - All are in `soundfx/`.
4. `LoadMusic("soundfx/chefao.mp3")`, `LoadMusic("soundfx/fase.mp3")`. These load only; nothing plays yet.
5. `SetBackgroundColor(0xFF000000)`.
6. `g_frameTimers.deleteAll()`.
7. `SetBorderBucketsDrawing(false)`.
8. `g_spawn = GetEntityArray("spawn")` and `g_sounds = GetEntityArray("play_sound")`. Exact names only: **the `play_sound.ent`-named instances (3 in level2, 2 in level3) are never collected and do nothing.**
9. `g_levelStartTime = GetTime()`.
10. `PlaySample("soundfx/respawn.mp3")`. This plays on every level, arena, respawn and checkpoint load.
11. Delete every entity named `"flashlight.ent"`. These are designer placeholders: level1 has 3, level3 has 15, pvp_lv3 has 1, pvp_lv4 has 2.
12. For every `"environment"` marker:
    - If it has a `name`: `AddEntity(name, marker.GetPosition(), @h)`, then `h.bgColor = marker.bgColor` (uint) and, if present, `h.bgImage = marker.bgImage`.
    - Always `DeleteEntity(marker)`.
    - The default string `"clouds.ent"` is dead code. A marker without `name`, as in `pvp_lv5`, gets no effect and its bgColor is ignored.
13. `drawRect(0xFF000000)`, so the first frame is black.
14. `g_camera.setMainCharPos((0,0),0)` and `g_camera.setMainCharPos((0,0),1)`.

**Environment markers in the data** [data]. bgColor values: 4278848010 = 0xFF0A0A0A, 4283256141 = 0xFF4D4D4D, 4278190080 = 0xFF000000.

| Scene | Marker pos | name | bgColor | bgImage |
|---|---|---|---|---|
| level1 | none | | | |
| level2 | (142,246) | clouds.ent | 0xFF0A0A0A | none |
| level3 | (116,97) | clouds.ent | 0xFF4D4D4D | planets.png |
| pvp_lv1 | (159,168) | dawn.ent | 0xFF4D4D4D | dawn.png |
| pvp_lv2 | none | | | |
| pvp_lv3 | (174,278) | clouds.ent | 0xFF0A0A0A | none |
| pvp_lv4 | (130,122) | clouds.ent | 0xFF4D4D4D | planets.png |
| pvp_lv5 | (148,129) | (none: ignored) | 0xFF000000 | none |
| pvp_lv6 | (181,148) | fog.ent | 0xFF4D4D4D | planets.png |

### 6.2 `levelLoop()` and `pvpLoop()` (`:226-240`)

- `levelLoop` calls `doLoop(false)`.
- `pvpLoop`: if `!IsSamplePlaying("soundfx/chefao.mp3")`, `PlaySample` and `LoopSample(true)`. Then `doLoop(true)`. The boss track is the PvP music.

### 6.3 `doLoop(pvp)` (`:248-395`): exact order

1. If `pvp`, `SetBorderBucketsDrawing(true)`. This runs every frame and adds a ring of off-screen buckets to the static-callback and render set.
2. `fadeIn(g_levelStartTime)`: a 3 s fade from black, drawn over the HUD (see Section 0).
3. **Spawning.** For each `g_spawn[t]`:
   - Skip it if `!IsAlive()` or `!isInScreen(marker)` (±76 px).
   - `name = marker.name` and `isComplete = marker has "complete"`. Only the presence of the key counts; all player spawns carry `int complete=0`, which still counts as complete.
   - `AddEntity(name+".ent", marker.GetPosition()-(0,0,10), @h)`, then `DeleteEntity(marker)`.
   - `h.waitBeforeAttack = 0u` and `h.lastTimeAlive = 0u`.
   - If `pvp`: `h.pvpMode = 1u`. If complete: `h.hp *= 5` and `h.maxHp *= 5`, so players have 500/500.
   - If `isAMainCharacter(h)`: `g_camera.setMainCharPos(h.GetPositionXY(), h.playerId)`.
   - If `h` has `showUpSfx`:
     - `PlaySample("soundfx/"+showUpSfx)` and `g_camera.startEarthquake(8)`.
     - `master_knight.ent` and `vert_master_knight.ent` use `creature_show_up.mp3`; `paladin.ent` uses `paladin_appear.mp3`.
   - If complete, stop here. Otherwise call `spawn(h, name)` (Section 6.4).
   - Spawn-marker census: 271 in total. Players are 9x `bruxo` and 6x `princess`, all `complete`. Enemies are warrior 71, minion 69, knight 60, impy 23, paladin 19, master_knight 14.
   - `level3`'s `spawn2` bruxo marker at (9814,2180) is named differently, so it is never collected and is inert.
4. **Ambient one-shots.** For each `g_sounds[t]` that is alive and `isInScreen`: `PlaySample("soundfx/"+name)` then `DeleteEntity`. The only active ones are two `horror.mp3` markers in level2, at (-391,-74) and (-1149,-77).
5. `g_numNpcs = 0`.
6. `g_messages.showMessages((10,70), "Arial", 30, 203,203,228, g_camera)` (Section 9).
7. `g_camera.adjustCameraPos(pvp)` (Section 8).
8. If `!g_gameFinished`:
   - `g_timer.showTimer((W-50, 0), 25, 255, UI colour)` draws `getTimeString(GetTime()-startTime)` with `shadowText`, "Arial Narrow" 25. At 1024 wide this is (974,0).
   - If `pvp`: the game is won if `g_pvpPoints[i] >= 3` for either player, or if `abs(p1-p0) >= 3` (negative scores count toward the difference). When won: `g_gameFinished = true` and `PlaySample("soundfx/pvp_win.ogg")`.
9. Else, if the game is finished:
   - **Campaign:**
     - If `g_newRecordTime==0` (first finished frame): `g_newRecordTime = g_timer.getElapsedTime()`, `StopSample("soundfx/chefao.mp3")`, `PlaySample("soundfx/death_king.ogg")`, `addNewRecordTime(g_newRecordTime)`.
     - Every frame, draw:
       - `shadowText((100,85), "Seu tempo total foi:", "Arial Narrow", 30, 255, UI)`.
       - `shadowText((100,100), getTimeString(g_newRecordTime), "Arial Narrow", **256**, 255, UI)`: a huge clock.
       - `shadowText((110,356), "Melhores tempos:\n" + getRecordTimeList() + "\n\n", "Arial Narrow", 30, 255, UI)`.
   - **PvP:**
     - `StopSample(chefao.mp3)` every frame. `pvpLoop` also restarts it every frame (Section 12, quirk 8).
     - `winnerId = p0 > p1 ? 1 : 2`, so a tie gives "2".
     - `DrawSprite("entities/gameover.png" (570x114), (W/2-285, H/2-200) = (227,184), 0xFFFFFFFF)`. The art is black letters on transparency.
     - `shadowText((200,335), "\n\nJogador " + winnerId + " é o vencedor!\n", "Arial Narrow", 50, 255, UI)`.
     - The computed `diff` is unused.
   - `waitForInputToMenu()`: P1's cancel (ESC or joystick `JK_09`) returns to the menu.
10. `escToGoToMenu()`: K_ESC `KS_HIT` returns to the menu at any time, with no confirmation and no pause menu.
11. If K_J is down (held), `detectJoysticks()`: joystick icons at the top right while J is held.

### 6.4 `spawn(h, name)`: enemy initialisation (`:52-112`)

Custom data it adds:
- `currentDir = LEFT(1)`.
- `forceX`, `forceY`, `knockBackX`, `knockBackY` = 0 (float).
- `touchingGround = 0`, `action = STANDING(0)`, `lastSwordAttack = 0`.
- `lastTimeDidntSee = GetTime()`, `fireResistant = 0`.

Then from `data.enml` section `[name]`:
- `hp` (int), `damage` (int).
- `speed`, `viewRadius`, `attackRadius`, `pushBackBias` (float).
- `stride`, `coolDown`, `waitBeforeAttack`, `jumpBackAfterAttack`, `fireResistant` (uint).
- `expGiven` (int) = the `hp` value.
- If the section has `chaseSfx`: `chaseSfx = "soundfx/"+value` and `LoadSoundEffect(that)`. Only minion has it (`minion.ogg`).

A missing key leaves the temporary variable holding the previous key's value; no shipped section is missing keys.

`spawn` is also called by `event01` for `"king"` (hp 3500, damage 10, speed 100, stride 120, coolDown 1000, viewRadius 280, attackRadius 35, jumpBackAfterAttack 1, pushBackBias 0.15).

---

## 7. HUD: `drawPlayerStatus(e)` (`interface.as:43-96`)

Called from `ETHCallback_bruxo` (`controlCharacters.as:296`) and `ETHCallback_princess` (`:339`) every frame while that player is alive. There is no split screen; both HUDs sit side by side on one screen.

**Inputs:**
- `rail=200`, `height=16`, `frameSize = GetSpriteSize("interface/frame.png") = (226,74)`.
- `idOffset = (226*playerId, 0)`.
- `maxXp = data.enml global["lv"+g_charLevel[pid]]`, which is 0 if the key is missing.
- Bar lengths: `hpLen = hp/maxHp*200`, `mpLen = mp/maxMp*200`, `xpLen = g_exp[pid]/maxXp*200` (float math).
- `textColor = ARGB(200,203,203,228)`.

| # | Call | Position (P1, idOffset 0; P2 adds x+226) | Size / params |
|---|---|---|---|
| 1 | DrawShapedSprite `interface/rail.png` | (0,0) | 200x16, 0xFFFFFFFF |
| 2 | DrawShapedSprite `interface/hp.png` | (0,0) | hpLen x 16 |
| 3 | DrawText `"hp: "+hp` | (hpLen-45, 0) | "Arial Narrow" 16, 0xD0000000 |
| 4 | rail | (0,16) | 200x16 |
| 5 | `interface/mp.png` | (0,16) | mpLen x 16 |
| 6 | DrawText `"mp: "+mp` | (mpLen-45, 16) | "Arial Narrow" 16, 0xD0000000 |
| 7 | rail | (0,32) | 200x16 |
| 8 | `interface/xp.png` | (0,32) | xpLen x 16 |
| 9 | DrawText `"lv: "+g_charLevel[pid]` | (xpLen-27, 32) | "Arial Narrow" 16, 0xD0000000 |
| 10a | Campaign, pid 0 only: DrawSprite `skull_interface.png` (18x20) | (448,0) | 0xFFFFFFFF |
| 10b | DrawText `""+g_lives` (shadow) | (469.5,1.5) | "Arial Black" 17, 0xF0000000 |
| 10c | DrawText `""+g_lives` | (468,0) | "Arial Black" 17, textColor |
| 10' | PvP (entity has `pvpMode`): DrawSprite skull | (idOffset.x+16, 74+16) = (16,90) for P1, (242,90) for P2 | |
| 10'' | DrawText `""+g_pvpPoints[pid]` twice at the **same** spot | skull+(30,0) = (46,90) / (272,90) | "Arial Black" 17: first 0xF0000000, then textColor |
| 11 | DrawSprite `interface/frame.png` (226x74 stone frame) | (226*pid, 0) | 0xA0FFFFFF |
| 12 | DrawSprite `interface/blend.png` (200x48 gradient) | (226*pid, 0) | textColor |

- The HUD is fixed to the screen's top-left in pixels and is not scaled with the resolution.
- In the campaign, the summoned princess (pid 1) gets the second HUD at x=226 without a lives counter.
- Bar sprites are 16x16 textures stretched to the given size.

**`doMpRecovery(e, 350, 1)`** (`interface.as:98-108`), called right after `drawPlayerStatus`:
- If `lastMpIncr` is missing, set it to `GetTime()`.
- If `GetTime()-lastMpIncr > 350`: `addToMp(e,+1)`, clamped to `maxMp`, and `lastMpIncr = GetTime()`.
- Net rate is about 2.85 mp/s, quantised to frames.

---

## 8. Camera: `CameraManager` (`cameraManager.as:43-158`)

**State:**
- `mainCharPos0`, `mainCharPos1` (initially (0,0)).
- `screenLimit = (0.4, 0.3)`.
- `earthquake = 0`, `lastTremble = 0`, `trembleInterval = 70` ms, `eqForth = true`, `backValue = 0`.
- `cameraSpeed = 150`, `accelerationTime = 20000` and `inBoundsLastTime` are **unused**.

**Feeders:**
- `ETHCallback_bruxo` sets pos0 every frame, **even while dead** (`controlCharacters.as:272`).
- The princess sets pos1 while alive and resets it to (0,0) when dead (`:332`, `:364`).
- `doLoop` sets pos0 and pos1 on spawn.
- `setupScene` zeroes both.

**`adjustCameraPos(pvpMode)`**: `pvpMode` is unused. In order:
1. `doEarthquake()`.
2. `target = pos0`. If `pos1 != (0,0)`, `target = (pos0+pos1)/2`. This is the two-player midpoint, used both in PvP and when the princess is summoned.
3. `cam = GetCameraPos()` and `s = target - cam`, both computed once.
4. Horizontal:
   - If `s.x > 0.6*W`: `cam.x = target.x - 0.6*W`.
   - If `s.x < 0.4*W`: `cam.x = target.x - 0.4*W`.
5. Vertical:
   - If `s.y > 0.7*H`: `cam.y = target.y - 0.7*H`.
   - If `s.y < 0.3*H`: `cam.y = target.y - 0.3*H`.
6. `SetCameraPos(cam)`, `SetPositionRoundUp(true)`, then `SetCameraPos(floor(cam.x), floor(cam.y))`.

- This is an instant dead zone at 40-60 % horizontally and 30-70 % vertically (at 1024x768: x 409.6-614.4, y 230.4-537.6). There is no smoothing.
- **There are no level bounds.** The camera can show any world position, and the black background shows beyond the art.
- The camera is shared: players are not constrained to the screen in PvP. In the campaign, the princess dies after 3 s off-screen, which is handled in `controlCharacters.as:342-359`.

**`startEarthquake(f)`**: `earthquake = abs(f)`.

**`doEarthquake()`** runs when `earthquake != 0` and `GetTime()-lastTremble >= 70`:
- If `eqForth`: `AddToCameraPos((0, +earthquake))` and `backValue = earthquake`.
- Otherwise: `AddToCameraPos((0, -backValue))` and `earthquake *= 0.5`.
- Toggle `eqForth` and set `lastTremble = now`.
- Then, if `|earthquake| < 1`, set it to 0.

For example, force 8 gives +8, -8, +4, -4, +2, -2, +1, -1, one step per 70 ms, about 560 ms in total. The shake is vertical only.

**Earthquake callers:**

| Force | Source |
|---|---|
| 8 | Spawn with `showUpSfx` (`setupScene.as:305`) |
| 2.5 | Air jump (`playerInput.as:333`) |
| 7 | Sword combo (`playerInput.as:371`) |
| 10 | Hard landing (`controlCharacters.as:162`) |
| 20 | Princess vanishing (`controlCharacters.as:352`) |
| min(10, damage) | Damage taken (`doDamage.as:141`) |
| 20 | Lava shooter (`lavaShooter.as:117`) |
| 20 | Explosion (`spells.as:142`) |

The earthquake state persists across scenes.

---

## 9. Messages: `MessageManager` (`messageManager.as:43-219`)

**State:**
- `maxMessages = 10`, `maxDamageMessages = 15`, `messageTime = 4000` ms.
- `dict`: keys `"k0".."k9"` map to `Message{time, message, alpha(uint8), origin}`.
- `damage[15]`: a ring buffer with `damageIndex`.
- `lastMessage`: a string.

**`addMessage(string msg)`**:
1. If `!SampleExists(help.mp3)`, load it.
2. **If `lastMessage != msg`, `PlaySample("soundfx/help.mp3")`.** This happens before the duplicate check.
3. If any slot already holds the same text, return. The time is not refreshed.
4. Otherwise store `{time=now, message=msg}` in the first free slot k0..k9. If all ten are full, the message is silently dropped.

**`addMessage(int value, vector2 pos)`**, for damage numbers (called from `doDamage.as:166` with `-damage`):
- Write into `damage[damageIndex]` (wrapping at 15): `time=now`, `text=""+value` (so `"-20"`), `alpha=255`, `origin=pos+(-10,-32)` in world coordinates.
- Then `damageIndex++`.

**`showMessages(pos=(10,70), "Arial", 30, 203,203,228, cam)`**, called each frame from `doLoop` only:
1. `processMessages()`:
   - For each slot: `e = now - time`. If `e < 4000`, `alpha = 255 - e*255/4000` (uint math). Otherwise delete the slot.
   - For each damage entry with `e < 1333` (4000/3): `alpha = 255 - e*255/1333` and `origin.y -= UnitsPerSecond(15)`, so it rises at 15 px/s.
2. For each existing slot t = 0..9:
   - Track `latest`, the message with the greatest `time`; `<=` means ties go to the later slot. `lastMessage` is set to `latest.message`.
   - Draw `shadowText(pos + (0, t*30), msg, "Arial", 30, alpha, UI)`. Messages are positioned by **slot index**, so gaps stay empty (y = 70, 100, …, 340).
3. If there is a `latest`: `charPos = cam.getMainCharPos(0) - GetCameraPos() + (-64, +32)` and `shadowText(charPos, latest.message, "Arial", 15, latest.alpha/2, UI)`. This echoes the newest message under P1 (the bruxo) at half alpha. Otherwise `lastMessage = ""`.
4. For each damage entry with `e < 1333`: `shadowText(origin - GetCameraPos(), text, "Arial", 15, alpha, UI)`.

**Consequences:**
- A help point re-adds its message every frame. The sound plays once, the message lasts 4 s, and if the player is still standing there it reappears with the sound again, because `lastMessage` resets to "" once the list is empty.
- Messages persist across scene loads (the global dictionary), but they fade by wall-clock time.

**Message sources:**
- In my files: `help` entities (`main.as:157`) and `"Checkpoint..."` (`main.as:185`).
- Elsewhere: mana, summon and vanish warnings in `playerInput.as:375,393,416,420,436` and `controlCharacters.as:348,683,695,722,729`.

---

## 10. Environment (`environment.as`)

**`positionEnvironmentElements(e)`** (`:122-143`), every frame:
1. `e.SetPosition(cam.x+64, cam.y+350, 0)`. The emitter follows the camera; the clouds and fog particle systems spawn fog sprites that drift right.
2. If the entity has `bgImage`:
   - `SetBackgroundImage("entities/"+bgImage)`.
   - If it is `"planets.png"`: `SetBackgroundAlphaAdd()` and `PositionBackgroundImage((W*0.6, 100), (W*0.6+329, 429))`. That is (614.4,100)-(943.4,429) at 1024 wide: an additive 329² planet in the sky.
   - Otherwise: `PositionBackgroundImage((0,0), (W,H))`.

**`ETHCallback_clouds(e)`** (`:43-113`): storm lightning.
- `lightningTime = 1600`.
- `peaks[15] = {0.7, 0.8, 0.9, 1.0, 1.0, 0.8, 0.6, 0.4, 0.8, 1.0, 0.9, 0.8, 0.6, 0.3, 0.1}`.
- In order:
  1. `positionEnvironmentElements(e)`.
  2. First call only: `ambientR/G/B = GetAmbientLight()` (floats), `lastLightning = now`, `nextLightning = 10000`. Each key is initialised only if missing.
  3. `e_ms = now - lastLightning`.
  4. If `e_ms >= nextLightning`: `PlaySample("soundfx/thunder.mp3")`, `lastLightning = now`, `nextLightning = 6000 + rand(6000)` (6-12 s). Nothing else is set this frame.
  5. Else if `e_ms < 1600`: `i = min(14, floor(e_ms/1600*15))` and `p = peaks[i]` (about 106.7 ms per step).
     - `SetBackgroundColor(ARGB(255, p*255, p*255, p*255))`, truncated to uint8. The sky flashes grey to white.
     - `SetAmbientLight((1-p) * savedAmbient)` per channel. The world goes darker, down to silhouettes at p=1.
  6. Else: if `bgColor` exists, `SetBackgroundColor(bgColor)`. Then `SetAmbientLight(savedAmbient)`.
- **Consequences:**
  - Every clouds scene starts with a 1.6 s flash sequence with **no** thunder sound, mostly hidden by the 3 s fade-in.
  - The first thunder comes at 10 s. After that, the thunder sounds on the frame the flash starts.
- Scenes with clouds: level2, level3, pvp_lv3, pvp_lv4, and **gameover.esc**. `gameover.esc` has a raw `clouds.ent` at (307,7,-62) with no `bgColor`, so after each flash the background stays at the last flash grey (about (25,25,25) at p=0.1).

**`ETHCallback_fog(e)`** (`:115-120`): `positionEnvironmentElements`, then `SetBackgroundColor(bgColor)` if present. No lightning. Used in pvp_lv6.

**`ETHCallback_dawn(e)`** (`:145-155`): `SetBackgroundColor(bgColor)` if present. If `bgImage` exists: `SetBackgroundImage("entities/"+bgImage)` and `PositionBackgroundImage((0,0), (W,H))`, which stretches `dawn.png` (64x64) over the whole screen. The entity is not repositioned. Used in pvp_lv1.

**Asset notes:**
- `clouds.ent`: type 5, 70 `fog.dds` particles, StartPoint (-300,0), RandStartPoint (10,600), Direction (4.7,0), colour (0.4,0.4,0.4) with alpha 0.6→0, lifeTime 4500 ± 3300, size 256 ± 256.
- `fog.ent`: 40 particles, alpha 0.4, StartPoint (-272,-89), layerDepth 1.

---

## 11. Events and triggers

### 11.1 `ETHCallback_help(e)` (`main.as:147-161`)

Static entity, so it runs when visible.
- For each entity in **e's own bucket**: if it is within 30 px of `e` and is a main character, `g_messages.addMessage(e.message)`.
- The message is the string in custom data `message`.
- There are 16 help entities: 11 in level1, 5 in level3. Examples: 'Pulo duplo: para cima enquanto estiver no ar', 'Próxima fase: seta para baixo', 'É uma boa hora para ligar a luz' (x5 in a row).

### 11.2 `ETHCallback_story(e)` (`main.as:163-168`)

`shadowText(e.GetPositionXY() - GetCameraPos(), data.enml global[e.name], "Arial Narrow", 16, 100, 203,203,228)`.

This is world-anchored lore text at alpha 100, drawn whenever the entity is visible. There are 14 entries: story01-04, soldados, fun, comboTip, bridge, annoying, flyingWall, portalToCastle, warning, nights, wisdom (`data.enml:34-88`).

### 11.3 `ETHCallback_checkpoint(e)` (`main.as:170-192`)

`checkpoint.ent`: static, `Lapide_Bitmap2.png` gravestone, green light, black smoke particles. There are 10: level1 has 3, level2 has 4, level3 has 3.

For each entity in e's bucket that is within 30 px, is a main character, and has `playerId == 0`, in this order:
1. `player.hasCheckpoint = 1u`.
2. `player.lives = g_lives` (int). This is written but never read.
3. `SaveScene("scenes/checkpoint.esc")`.
4. `g_messages.addMessage("Checkpoint...")`.
5. `DeleteEntity(e)`.
6. `AddEntity("checkpoint_effect.ent", e.GetPosition()+(0,0,10), 0)`. This is a one-shot particle burst whose particle `SoundEffect` is `checkpoint.mp3` at `soundVolume` 0.5.

### 11.4 `ETHCallback_next_level(e)` (`main.as:194-229`)

`next_level`: static, `passage.png` portal with a red light and blood particles.

- **If `fadeOut` is missing:** for each entity in e's bucket within **80** px that is a main character with `playerId==0`, if `getPlayerXYAxis(0).y > 0` (K_DOWN, or P1's stick with any positive y), set `e.fadeOut = GetTime()`.
- **Otherwise:**
  - `bias = 0`. If `fadeOut(e.fadeOut, bias)` has finished (3 s): `UsePixelShaders(true)` if supported, then `LoadScene("scenes/" + e.name, "setupScene", "levelLoop")`.
  - `SetSampleVolume("soundfx/fase.mp3", 1-bias)`. On the final frame this is 1.0.
  - `loadingMessage()`.
- Gameplay continues during the fade, so the player can still be hurt.
- Because the entity is static, the fade freezes if the portal scrolls off-screen.

### 11.5 `ETHCallback_play(e)` (`setupScene.as:43-50`)

- `LoadMusic("soundfx/"+name)`, `PlaySample`, `LoopSample(true)`, then `DeleteEntity(e)`.
- All three are `fase.mp3`: level1 at (3981,-598), which starts mid-level; level2 at (205,492); level3 at (444,190).

### 11.6 `ETHCallback_event01(e)` (`events.as:43-64`)

The king's arena. `level3` has `event01` at (10920,1408): static, **collidable** 128x256 box, bucket (42,5), which covers x 10752-11008 and y 1280-1536.

For each main character in e's bucket (no distance test), in order:
1. `StopSample(fase.mp3)`.
2. `PlaySample(laugh_king.mp3)`.
3. `PlaySample(chefao.mp3)` and `LoopSample(chefao,true)`.
4. `AddEntity("summon.ent", e.pos, 0)`.
5. `AddEntity("king.ent", e.pos+(0,0,-10), @h)` and `spawn(h,"king")`.
6. `DeleteEntity(e)`.
7. `AddEntity("invisible_wall.ent", (10112,1408,0), 0)`. This is a static collidable 256x256 box that closes the way back.

There is no `break` after this, so two players in that bucket on the same frame spawn two kings.

**King death:** while the king's `isDead` is running, `g_gameFinished = true` (`controlCharacters.as:670`). The king is fed by `summoner(king, "warrior", "summon", 5000)`, which uses the 4 renamed `summon` markers in level3 at (10367,1315), (10632,1304), (10875,1307), (11223,1313).

### 11.7 `ETHCallback_falling_bridge(e)` (`events.as:66-106`)

`falling_bridge.ent`: dynamic, collidable 64x32, `single_stone.png`. There are 43 in total (level3 16, pvp arenas 27).

- **If `falling` is missing:**
  - Gather entities from e's bucket and the bucket above (0,-1).
  - `stone = absolute collision box`, then `size.y += 6`, `size.x *= 0.9`, `pos.y -= 3`.
  - If any main character's absolute box overlaps (inclusive AABB, `util.as:350-372`): `gravity = 0.0f` and `falling = now`.
- **Else if `now - falling > 600`:**
  - `SetCollision(false)`.
  - If `gravity == 0`: `AddEntity("bridge_fall.ent", pos+(0,0,-2), 0)`, a dust burst with `brige_fall.ogg`.
  - `gravity += UnitsPerSecond(1200)`.
  - `AddToPositionXY((0, gravity/fps))`, where `fps = GetFPSRate()` or 60 if that is 0.
  - If `!isInScreen(e)`, `DeleteEntity(e)`.

The stones named `fontFall`, `dontFall` and `step` have no callback, so they never fall.

---

## 12. Lives, death, respawn, game over, ending

This section covers cross-file code in `controlCharacters.as:406-468` (`isMainCharDead`), because it drives the flow.

- **Invulnerability after the game is won:** if `g_gameFinished`, the bruxo and princess callbacks force `hp = 100` every frame. In PvP, `isMainCharDead` returns true, which freezes both players.
- **Death sequence**, when `hp <= 0`:
  - The first frame sets `deathTime`, spawns `fade_out_beam.ent`, kills particle system 0, sets colour black and turns collision off.
  - Each frame: `a = deadTime/3000`. If `fase.mp3` is playing, its volume is set to `1-a`. The entity alpha becomes `1-a`, and a full-screen black rectangle is drawn at alpha a. When `a > 0.7` and `g_lives >= 0`, `loadingMessage()` is shown.
  - At `deadTime >= 3000`: `g_lives--`.
    - If `g_lives >= 0` or PvP: reload the same scene file. If the player has `hasCheckpoint` (campaign), load `scenes/checkpoint.esc` instead.
    - Otherwise load `gameover.esc`.
  - With 13 lives you can die 14 times. The HUD shows 0 during the last life.
  - Summoning the princess also costs a life (`controlCharacters.as:725`), which can make the HUD show -1.
- **PvP:** a death of either player reloads the whole arena after 3 s. Points and the timer persist, and the players come back with 500/500 hp.
- **Checkpoint restore** (`scenes/checkpoint.esc`) is a full world snapshot taken at the moment of touching the gravestone:
  - It contains every entity with its custom data: enemies, `spawn` markers not yet spawned, remaining pickups, bridge state, and the added `clouds.ent` with its `ambientR/G/B`, `lastLightning` and `nextLightning`.
  - It contains the bruxo with `hasCheckpoint=1` and the hp and mp he had then.
  - It contains **the checkpoint entity itself**, because it is deleted after `SaveScene`.
  - The scene properties (ambient) are saved too.
  - On restore, `setupScene` runs again. It plays `respawn.mp3` and fades in. There are no environment markers or flashlights left, and only the unspawned `spawn` markers are collected.
  - The bruxo appears within 30 px of the saved gravestone, so on the first visible frame it **triggers again**: it re-saves, shows "Checkpoint..." and plays the effect and sound.
  - The `play` music entity already deleted itself before the save, so nothing restarts `fase.mp3` (open question).
  - Saved uint timestamps are absolute `GetTime()` values.
- **Game over** (`gameover.as`):
  - `gameOverPreLoop`, in order:
    1. `LoadSoundEffect` `thunder.mp3`, `laugh_king.mp3`, `gameover.mp3`.
    2. `LoadSprite("entities/gameover.png")`.
    3. `PlaySample(laugh_king.mp3)` and `PlaySample(gameover.mp3)`.
    4. `SetBackgroundColor(0xFF000000)` and `SetAmbientLight((0,0,0))`.
    5. `AddEntity("bruxo_dead.ent", (W/2, H/2, 0), 0)`: the dead bruxo sprite (frame 8) with rising black smoke, a silhouette at the screen centre.
    6. `g_gameOverStartTime = now`.
  - `gameOverLoop` each frame: `DrawSprite(gameover.png, (W/2-285, H/2-200), 0xFFFFFFFF)`, `fadeIn(g_gameOverStartTime)`, and `waitForInputToMenu()` (P1's ESC or `JK_09`).
  - The scene's `clouds.ent` provides flashes and thunder that light the black "Game Over" letters.
- **Campaign ending:** see Section 6.3 step 9. The run time runs from `newGame` to the first frame after the king's hp reaches ≤ 0, and includes all fades, deaths and loads. Afterwards the player can still move; ESC or cancel returns to the menu.

---

## 13. High scores (`scores.as`, `timer.as`)

`hs.enml` lives next to the exe (`GetAbsolutePath`). The shipped content (CRLF, tab indent) is `hs\r\n{\r\n\ths0 = 3599000;\r\n … hs4 = 3599000;\r\n}\r\n\r\n`. The values are milliseconds, and 3599000 is "59:59".

**`getTimeString(ms)`** (`timer.as:43-52`): `(ms/1000)/60` (minutes, not wrapped, not padded) + `":"` + two-digit `(ms/1000)%60`. Integer maths. Examples: 65000 is "1:05", 720000 is "12:00".

**`addNewRecordTime(elapsed)`** (`scores.as:45-90`):
1. Parse `hs.enml`.
2. `list[0..4] = getUint("hs","hs"+t)`. A missing key keeps the previous value; if the file is missing, all five are 0.
3. `list[5] = elapsed`.
4. Sort with the triple loop "for i, for j, (for k x6 redundant): if list[i] < list[j] swap". This sorts **ascending**.
5. `highScores.clear()`, then write the first 5 into entity `hs` (keys `hs0..hs4`), then `writeToFile`.
6. Returns whether `elapsed` survived; the return value is unused.

Lower times are better. `hs0` is the best time.

**`getRecordTimeList()`** (`:92-107`): five lines of `(t+1) + "    " + getTimeString(hs_t) + "\n"`, with 4 spaces.

**`getGetBestTime()`** (`:110-120`): `hs0`, or 0 if the file is missing. That makes `score <= 0` false, so **deleting `hs.enml` unlocks the locked arenas**.

**Timer** (`timer.as:54-80`): `start()` stores `GetTime()`. `getElapsedTime() = GetTime()-start`. `showTimer` uses `shadowText` with "Arial Narrow".

---

## 14. Input mapping (`playerInput.as`, cross-file)

Joystick mapping by `g_controls` (`playerInput.as:43-53`):

| `g_controls` | Menu label (image) | Player 0 joystick | Player 1 joystick | `hasASecondController()` |
|---|---|---|---|---|
| 0 (default) | keyboard P1, joystick P2 | index 1 | index 0 | joystick 0 detected |
| 1 | joystick P1, joystick P2 | index 0 | index 1 | joystick 1 detected |

In the table below, "Joystick" means the joystick of that player's own index, and it is used only if that joystick is detected. Player 0 checks the keyboard first. Player 1 has no keyboard at all.

| Action | P1 keyboard | Joystick button | Used in my files |
|---|---|---|---|
| Axis | arrow keys (last one wins, per axis: RIGHT over LEFT, DOWN over UP) plus analog stick added on top | stick X/Y | Cursor movement (5 px/frame), next_level (y > 0) |
| Confirm | RETURN (only while ALT is `KS_UP`), LMOUSE, RMOUSE (first key down wins) | `JK_10` | Menu buttons, switches, back arrow, video modes. P2's confirm summons the princess |
| Cancel | ESC | `JK_09` | `waitForInputToMenu` |
| Jump | UP or CTRL | `JK_03` | other dimension |
| Sword | S | `JK_04` | other dimension |
| Fire | D | `JK_02` | other dimension |
| Light | SPACE | `JK_01` | other dimension |
| Raw keys | K_ESC hit (level to menu), K_J held (show joysticks), K_2/K_3/K_PAGEUP held at new-game time, ALT+RETURN (fullscreen) | | |

---

## 15. Sound table (flow and UI files)

| Sample | Load | Play / volume | Where |
|---|---|---|---|
| menu.mp3 | LoadMusic in menuPreLoop and screenModesPreLoop | Play plus loop on every menu, arena and video scene load. Volume 1-bias during new-game fade | `menu.as:43-48,356`, `videoModes.as:54` |
| help.mp3 | LoadSoundEffect in menuPreLoop (vol **0.3**), in setupScene, and lazily | Hover over a new button; `addMessage` when msg != newest | `menu.as:54-55,343`, `messageManager.as:84-92` |
| newgame.mp3 | menuPreLoop | Confirm new game or unlocked arena | `menu.as:295,334` |
| fail.ogg | menuPreLoop | Versus without a 2nd controller; locked arena | `menu.as:276,337` |
| respawn.mp3 | setupScene | Every level, arena or checkpoint load | `setupScene.as:179` |
| fase.mp3 | LoadMusic in setupScene; `play` entity | Loop from `play`. Stopped by event01. Volume by next_level fade and death fade | `setupScene.as:43-50`, `main.as:226`, `events.as:52` |
| chefao.mp3 | LoadMusic in setupScene | Loop in pvpLoop and at event01. Stopped at the campaign or PvP end | `setupScene.as:233-236,368,379`, `events.as:54-55` |
| laugh_king.mp3 | setupScene, gameOverPreLoop | event01; game over | `events.as:53`, `gameover.as:53` |
| gameover.mp3 | gameOverPreLoop | Game over | `gameover.as:54` |
| thunder.mp3 | setupScene, gameOverPreLoop | Clouds lightning | `environment.as:89` |
| death_king.ogg | setupScene | Campaign end | `setupScene.as:369` |
| pvp_win.ogg | setupScene | PvP win | `setupScene.as:357` |
| horror.mp3 | setupScene | Two `play_sound` markers (level2) | `setupScene.as:324` |
| creature_show_up.mp3 / paladin_appear.mp3 | setupScene | Spawn of master_knight / paladin (showUpSfx) | `setupScene.as:304` |
| checkpoint.mp3 | setupScene | Particle SoundEffect of `checkpoint_effect.ent` and `summon.ent` (soundVolume 0.5) | via particles |
| brige_fall.ogg | (particle) | `bridge_fall.ent` particle SoundEffect | via particles |
| blast_attack.ogg | setupScene | Volume set to 0.5 | `setupScene.as:158` |

---

## 16. Custom-data keys used by these files

| Entity | Key (type) | Meaning |
|---|---|---|
| cursor.ent | lastButton (string) | Last hovered button name, initially "none" |
| cursor.ent | menuStartTime (uint) | Fade-in origin |
| cursor.ent | newGame (uint) | Fade-out origin; its presence means the menu is locked |
| cursor.ent | scene (string) | "CAMPAIGN" or "pvp_lvN.esc" |
| thumbnail | name (string) | Arena number |
| thumbnail | title (string) | Arena title |
| thumbnail | score (uint) | Unlock threshold in ms |
| spawn | name (string) | Entity file name without ".ent" |
| spawn | complete (int) | Presence only: skip `spawn()`; hp and maxHp x5 in PvP |
| spawned | waitBeforeAttack, lastTimeAlive (uint 0) | |
| spawned | pvpMode (uint 1) | |
| spawned | spawn() keys | See Section 6.4 |
| spawned | showUpSfx (string, read) | |
| environment marker | name (string) | .ent file to add |
| environment marker | bgColor (uint ARGB) | |
| environment marker | bgImage (string) | |
| clouds / fog / dawn | bgColor, bgImage | Copied from the marker |
| clouds | ambientR, ambientG, ambientB (float) | Saved ambient |
| clouds | lastLightning, nextLightning (uint) | Lightning timing |
| help | message (string) | |
| story | name (string) | `data.enml` global key |
| play / play_sound | name (string) | soundfx file |
| checkpoint | writes player.hasCheckpoint (uint 1), player.lives (int) | |
| next_level | name (string) | Scene file |
| next_level | fadeOut (uint) | |
| falling_bridge | falling (uint), gravity (float) | |
| bruxo / princess | playerId (uint 0/1) | |
| bruxo / princess | hp, maxHp, mp, maxMp (int), 100 each in the .ent | |
| bruxo / princess | pvpMode | |
| bruxo / princess | lastMpIncr (uint) | |
| bruxo / princess | hasCheckpoint | |

---

## 17. Engine API usage (this dimension)

| API | Used for |
|---|---|
| `LoadScene(file, preLoop, loop[, bucket])` | Deferred scene switch (Section 2). Resets the camera to (0,0) [2013] |
| `SaveScene(file)` | Checkpoint world snapshot, including custom data and scene properties |
| `GetSceneFileName()` | Returns the string passed to LoadScene, for example "scenes/menu.esc" |
| `AddEntity(file,pos,angle)` / `AddEntity(file,pos,@h)` | Spawning, effects, king, walls, environment, bruxo_dead |
| `DeleteEntity(e)` | Kill; `IsAlive()` becomes false |
| `SeekEntity("cursor.ent")` | First entity with that name |
| `GetEntityArray(name, arr)` | Exact-name scan of all buckets |
| `GetEntitiesFromBucket(bucket, arr)` | Appends entities in one bucket (help, checkpoint, next_level, event01, falling_bridge) |
| `e.GetCurrentBucket()` | `floor(pos/bucketSize)` |
| `CollideDynamic(e, @out)` | Menu hit test (Section 4.4) |
| `e.GetPosition` / `GetPositionXY` / `SetPosition` / `SetPositionXY` / `AddToPositionXY` | |
| `e.GetEntityName` / `IsAlive` / `SetColor(vec3)` / `SetCollision(bool)` / `GetCollisionBox` | |
| `e.CheckCustomData` (DT_NODATA) / `Get*Data` / `Add*Data` | `Add*Data` also overwrites |
| `GetCameraPos` / `SetCameraPos` / `AddToCameraPos` / `SetPositionRoundUp(true)` | Camera |
| `SetBorderBucketsDrawing(bool)` | true in PvP, false otherwise |
| `SetAmbientLight` / `GetAmbientLight` / `SetBackgroundColor(ARGB)` | Environment |
| `SetBackgroundImage` / `PositionBackgroundImage(min,max)` / `SetBackgroundAlphaAdd()` | Screen-space background quad behind the scene; additive for planets |
| `DrawText(pos,text,font,size,ARGB)` | System TTF ("Arial Narrow", "Arial", "Arial Black"). Multi-line with \n, Latin-1 |
| `DrawSprite(file,pos,ARGB)` | Top-left anchored, native size |
| `DrawShapedSprite(file,pos,size,ARGB)` | Stretched |
| `DrawRectangle(pos,size,c0,c1,c2,c3)` | Per-corner colours |
| `GetSpriteSize` / `LoadSprite` / `GetScreenSize` | |
| `LoadMusic` / `LoadSoundEffect` / `PlaySample` / `LoopSample` / `StopSample` / `SetSampleVolume` / `IsSamplePlaying` / `SampleExists` | Named single-voice samples |
| `GetTime` / `UnitsPerSecond` / `GetFPSRate` / `rand(int)` | |
| `GetInputHandle` / `GetKeyState` / `KeyDown` / `GetCursorPos` / `GetCursorAbsolutePos` / `SetCursorPos` / `DetectJoysticks` / `GetJoystickStatus` / `JoyButtonState` / `GetJoystickXY` | |
| `SetWindowProperties` / `Windowed` / `GetVideoModeCount` / `GetVideoMode` / `IsPixelShaderSupported` / `UsePixelShaders` / `HideCursor` / `Exit` | |
| `GetStringFromFile` / `GetAbsolutePath` | |
| `enmlFile.parseString/getInt/getUint/getFloat/get/clear/addEntity/writeToFile`, `enmlEntity.add` | |
| `dictionary` get/set/exists/delete/deleteAll | |
| `ARGB`, `abs(float)`, `floor` | |

---

## 18. Quirks to preserve (P) or fix (F); my recommendation for each

1. **(P)** `complete` counts by presence, whatever its value. All player spawns are complete, so they get x5 hp in PvP.
2. **(P)** `pvp_lv5`'s environment marker has no `name`, so its bgColor is ignored.
3. **(P)** Only the renamed `play_sound` markers fire. The 5 instances named `play_sound.ent` are dead, and so is `spawn2`.
4. **(P or F)** Respawning on a checkpoint re-triggers it: it re-saves, shows the message, and plays the effect and sound.
5. **(F)** `player.lives` is written and never read.
6. **(P)** At the end of a fade (menu or next_level), the music volume snaps back to 1.0 for the frame before the load.
7. **(F?)** A level reload after death may leave `fase.mp3` silent (see openQuestions).
8. **(F)** At PvP end, `pvpLoop` restarts `chefao.mp3` every frame and `doLoop` stops it. Treat it as silent.
9. **(P)** A PvP tie shows "Jogador 2". A tie can only happen through the difference rule? No: a tie cannot end the match. Irrelevant.
10. **(F)** `data.enml` has no `lv20`. At level 20 the HUD's `maxXp` is 0 (division by zero), and `addToExp` jumps straight to 21. Past `lv30`, `addToExp` (`util.as:406-418`) loops forever. Guard both.
11. **(P)** If `hs.enml` is missing, the list shows five "0:00", no time is ever recorded, and the arenas are unlocked.
12. **(P)** Messages are laid out by slot, so gaps show.
13. **(P)** `addMessage` plays `help.mp3` whenever msg != newest message. A help point whose text is listed but is not the newest restarts `help.mp3` every frame, and single-voice restarts make this audible. With both players near a help point it is called twice per frame.
14. **(F)** `event01` has no `break`, so two players can spawn two kings.
15. **(P)** `next_level` fades freeze when the portal is off-screen, and play continues during the fade.
16. **(P)** Help, checkpoint and next_level proximity tests only look in the trigger's own 256 px bucket. At bucket edges a close player can be missed.
17. **(P)** The camera has no bounds or smoothing, and several of its parameters are unused.
18. **(P)** The first 1.6 s of any clouds scene flashes without thunder. After a flash, the game-over background stays grey.
19. **(P or F)** Cursor speed is 5 px per frame, so it depends on frame rate. The menu cursor lags one frame.
20. **(P)** The selected switch row reads "[]" and the unselected one "[ ]".
21. **(P)** Settings (`g_enablePS`, `g_windowed`, `g_controls`) are not saved.
22. **(P)** `melhores_tempos` hover reads the file every frame. It is fine to cache, as long as the cache is invalidated on write.
23. **(P)** Held-key cheats at the moment the fade ends: K_2 or K_3 for level select, PAGEUP for level 15.
24. **(P)** Summoning can drive lives negative.

---

## 19. Suggested port structure (game-side, engine untouched)

**Game-side shims:**
- An `EthRuntime`-like layer with the named-sample audio model, the monotonic millisecond clock, the deferred scene request, a per-frame immediate draw list in call order, bucket helpers, and custom-data maps per entity.
- A scene "snapshot" writer and reader for `checkpoint.esc` semantics. It could be JSON in the port's save directory; the original file location is not needed.
- An ENML reader and writer for `data.enml` and `hs.enml`.

**Coordination:**
- Another session is editing Supersonic for a different game. Any engine-side addition (OGG decoding, a TTF text path, gradient quads, a second gamepad) must be coordinated with that session, or kept game-side.
- This task was read-only. I changed nothing in any of the three trees.

## Key facts

- Boot: main() loads scenes/menu.esc (menuPreLoop/menuLoop, bucket 1024x256), hides the OS cursor, parses data.enml (lives=13) and opens a 1024x768 windowed PF32BIT window titled 'Penumbra e o Castelo das Sombras - Ethanon Engine' (main.as:124-145).
- The menu is 7 renamed collidable entities using menu_buttons.png frames 0-6 (novo_jogo, versus, como_jogar, melhores_tempos, opcoes_de_video, creditos, sair) plus cursor.ent. ETHCallback_cursor does all the logic via CollideDynamic, and hovering a button draws showData() over the right 38.2% of the screen (menu.as:217-360).
- Confirming 'Novo jogo' or an arena starts a 3000 ms fade-out with 'Carregando...' and a menu.mp3 fade, then newGame(). Level = 1, or 2/3 if K_2/K_3 is held; PAGEUP held gives character level 15 (main.as:99-122).
- Versus requires hasASecondController(). goToPvp() resets data and loads arena_select.esc (default 256 buckets). Six thumbnails; arena 5 (score 720000 = 12:00) and arena 6 (900000 = 15:00) stay locked while hs0 >= threshold.
- setupScene runs on every level, arena and checkpoint load. It loads sprites and sounds, black background, collects exact-name 'spawn' and 'play_sound' entities, plays respawn.mp3, deletes 'flashlight.ent', turns 'environment' markers into clouds/dawn/fog entities carrying bgColor/bgImage, and draws one black frame (setupScene.as:114-224).
- doLoop order: [pvp: SetBorderBucketsDrawing(true)], fadeIn 3000 ms, spawn markers within screen±76 px (z-10; players with 'complete' skip spawn(); PvP hp x5 = 500), play_sound one-shots, messages, camera, then timer (W-50,0) size 25 or end screens, ESC to menu, J held shows joysticks (setupScene.as:248-395).
- PvP: g_pvpPoints; the match ends when either player has >= 3 or |p1-p0| >= 3, then pvp_win.ogg plays and gameover.png plus 'Jogador N é o vencedor!' show (a tie shows 2). Any death reloads the whole arena after 3 s; points persist.
- Campaign ends when the king starts dying (g_gameFinished). The final time is g_timer (since newGame, including all fades and loads) and is stored with addNewRecordTime into hs.enml (5 lowest ms values, ascending, keys hs0..hs4). The end screen shows the time at font size 256.
- Checkpoint: player 0 within 30 px (same bucket) sets hasCheckpoint, then SaveScene('scenes/checkpoint.esc') saves the whole world including the checkpoint entity itself, then 'Checkpoint...', delete, checkpoint_effect. Death with hasCheckpoint reloads checkpoint.esc and the player re-triggers the save.
- Lives: 13 from data.enml. Decremented when the main character's 3000 ms death fade completes; reload while >= 0, game over below 0. Summoning the princess also costs a life. HUD skull at (448,0), count in 'Arial Black' 17.
- HUD per player at x offset 226*playerId: three 200x16 bars (rail + hp/mp/xp stretched) at y 0/16/32, labels 'hp: N' / 'mp: N' / 'lv: N' in Arial Narrow 16 colour 0xD0000000, frame.png (226x74) at alpha 0xA0 and blend.png on top. XP max = data.enml global lvN.
- Camera: instant dead zone at 40-60% x and 30-70% y on the P1 position (or the P1/P2 midpoint), floored, no bounds, no smoothing, no split screen. Earthquake: vertical ±E every 70 ms, halving per cycle until below 1.
- Messages: 10 slots k0..k9 drawn at (10,70+30*slot), Arial 30, fading linearly over 4000 ms; the newest is echoed at P1 screen position + (-64,+32), size 15, half alpha. 15 damage numbers rise at 15 px/s and fade over 1333 ms, size 15. help.mp3 plays when msg != newest message.
- Lightning (clouds.ent): first thunder at 10 s, then every 6000+rand(6000) ms. A 1600 ms flash over 15 peaks (0.7..0.1) sets the background grey to p*255 and the ambient to (1-p)*saved. A flash with no thunder happens at every clouds scene start, including gameover.esc.
- event01 (level3, bucket 42,5): stops fase.mp3, plays laugh_king and loops chefao, adds summon.ent and king.ent (spawn 'king' hp 3500) and an invisible_wall at (10112,1408), deletes itself.
- Working engine model: dynamic-entity callbacks run every frame before the loop function; static-entity callbacks (help, story, checkpoint, next_level, play, event01) run only when in visible buckets, after the loop. Draws are a top layer in call order, so the HUD fades with the scene and story text does not.
- Samples are single-voice per name: PlaySample restarts, volume persists per sample, LoopSample toggles repeat [2013-src Audiere]. GetTime is app-lifetime ms and appears inside saved checkpoint custom data.
- Joysticks are WinMM: confirm = JK_10, cancel = JK_09, sticks derive JK_UP/DOWN/LEFT/RIGHT at ±0.8. g_controls=0 gives P1 keyboard + joystick 1 and P2 joystick 0; g_controls=1 gives P1 joystick 0 and P2 joystick 1.

## Engine gaps

- No OGG Vorbis decoder: AudioClip::Load handles only .wav and .mp3, and mp3 only on Windows via Media Foundation. The game uses many .ogg files (fail, pvp_win, death_king, blast_attack, cast_fire_spell, fall, explosion, hit01, potion_pick, minion, sword_combo, vanish, jump01/02, brige_fall).
- No Ethanon named-sample audio model: the engine hands out one voice per Play. Needed: restart-on-PlaySample, per-sample stored volume (SetSampleVolume on a playing stream), LoopSample toggle, IsSamplePlaying and SampleExists by name. Can be a game-side shim over AudioEngine::Play/SetVoiceParameters/Stop/IsVoicePlaying.
- No system TrueType text: the engine has BitmapFont (BMFont .fnt) plus ImGui text only. The game needs 'Arial Narrow', 'Arial' and 'Arial Black' at sizes 15, 16, 17, 20, 25, 30, 40, 50, 60 and 256 px, multi-line, with Latin-1/cp1252 accents. Needs a TTF rasteriser (stb_truetype is not vendored) or pre-baked BMFont atlases per size.
- ScreenOverlay::Quad has a single colour, so it cannot draw DrawRectangle's per-corner gradients (showData's 190-to-55 alpha vertical gradient). Needs per-corner colour or a pre-made gradient texture.
- Input supports only GLFW_JOYSTICK_1 as a gamepad (InputPolling.cpp:211, Input::IsGamepadConnected with no index). Penumbra needs two independent joysticks (indexes 0 and 1), per-joystick detection, raw button numbering JK_01..JK_10 (WinMM) and stick-derived D-pad states with the 0.8 threshold.
- No OS cursor warp: there is no SetCursorPos, and Input has only CursorMode. The menu moves the real mouse cursor with the arrow keys or stick (5 px per frame).
- No runtime fullscreen/windowed toggle (Alt+Enter), no display-mode enumeration (GetVideoMode list filtered to 32-bit and >= 800x600), no runtime resolution change.
- No Ethanon background layer: SetBackgroundColor, SetBackgroundImage, PositionBackgroundImage(min,max) and SetBackgroundAlphaAdd (additive screen-space quad behind the scene) have no direct counterpart. Could be done game-side with a screen-space quad drawn before the world.
- No runtime world snapshot preserving arbitrary per-entity custom data (Ethanon SaveScene to checkpoint.esc and reload). The engine's SceneSerializer is for its own JSON component scenes; checkpoint save and restore must be a game-side snapshot of the entities and their custom-data maps.
- No bucket-grid spatial queries (GetEntitiesFromBucket, GetCurrentBucket with 256 or 1024x256 buckets) and no visible-bucket-only callback scheduling for static entities. Both must be emulated game-side for exact trigger semantics.
- No global ambient-light and per-entity 2D light model with the Ethanon 'lightIntensity/ambient' semantics is confirmed here (Light2D exists; this belongs to the rendering dimension). Lightning needs SetAmbientLight(vec3) every frame.
- Coordination: the user's relayed request says another Claude session edits Supersonic for a different game. Every gap above should preferably be solved game-side in the Penumbra repository; any engine change must be coordinated with that session.

## Open questions

- Callback scheduling in the 2010 runtime: is it really 'dynamic entities always, static entities only in visible buckets' (the 2013 ETHScene::Update model)? Evidence for: the RunCallbackScript string on ETHRenderEntity, level1's static 'play' at x=3981, and checkpoint restore needing the off-screen dynamic bruxo to run. This decides whether the HUD fades with the scene and whether next_level fades freeze off-screen. Ivan could verify on a COPY of extracted/app, since running it writes checkpoint.esc and hs.enml.
- Order of dynamic-entity callbacks relative to the loop function in 2010: before it (2013) or after? This affects whether fadeIn covers the HUD and whether camera-dependent draws lag a frame.
- Does LoadScene in 2010 release all audio samples on a scene change? In 2013 resources are released only when the file name differs. If 2010 also behaves that way, reloading the same level after a death leaves fase.mp3 at the faded volume (~0), so the music is silent after the first death. Test on a copy: die once in level2 (music starts near the spawn) and listen.
- After a checkpoint reload (checkpoint.esc), does fase.mp3 play at all? The 'play' entity deleted itself before the save, so nothing restarts it unless the previous stream survives the scene change.
- Does a lightning flash appear in the first 1.6 s of level2, level3, pvp_lv3, pvp_lv4 and gameover.esc? The script says yes, but it is mostly under the 3 s fade-in.
- Does SaveScene in 2010 include entities killed earlier in the same frame, and 'temporary' particle-only entities (jumpfx, summon.ent, checkpoint_effect)? This decides what reappears on checkpoint restore.
- Does LoadScene in 2010 reset the camera to (0,0) as the 2013 code does (ETHScriptWrapper.Scene.cpp:607)? This affects the first frame after a checkpoint restore and which markers are 'in screen' on frame 1.
- rand(6000): is the result in [0,6000] inclusive (2013 MTRand::randInt) or [0,6000)? This decides whether the thunder interval is 6000-12000 inclusive.
- DrawText size semantics in 2010: D3DXCreateFontA Height = size, positive (cell height) or negative (character height)? What weight, quality and antialiasing? The port needs a size mapping to match text widths (menu panel, 256 px end-screen clock).
- 5th bool of SetWindowProperties(title,w,h,windowed,?,PF32BIT): vsync in 2013; unverified for 2010.
- CollideDynamic in 2010 is native. Does it include the z-axis overlap and the 3x3 bucket neighbourhood like the 2013 script reimplementation, and with strict or inclusive edges? It matters only where thumbnail hit zones overlap.
- Joystick button semantics for a modern XInput pad: the original uses WinMM raw buttons (JK_01 light, JK_02 fire, JK_03 jump, JK_04 sword, JK_09 cancel/select, JK_10 confirm/start). Which GLFW gamepad buttons should these map to in the port (design decision for Ivan)?
- Draw order among top-layer items versus entity callbacks: is the 2010 top layer strictly call-order? Is DrawSprite anchored top-left and unaffected by the camera? Assumed yes.
- Is the menu's 1024x256 bucket size significant beyond CollideDynamic's neighbourhood (for example border-bucket rendering of the menu art)? Probably not visible.
- Heredoc trimming in the 2010 AngelScript version: are the leading and trailing CRLF of the como_jogar/config/creditos/novo_jogo strings removed as in the modern AS manual? This affects the vertical position of the panel text by one line.
- Should the port keep the original's known bugs (x5 PvP hp via 'complete' presence, checkpoint re-trigger on respawn, help.mp3 restarting every frame, a possible double king, missing lv20 and infinite loop past lv30, PvP-end music stutter)? The recommendation is given per item in report section 18; this needs Ivan's decision.
