# Penumbra e o Castelo das Sombras: player characters and combat porting spec

**Scope.** This covers `controlCharacters.as` (732 lines), `playerInput.as` (441), `combo.as` (154), `swords.as` (136), `spells.as` (152), `potions.as` (75), `doDamage.as` (243) and `constants.as` (96), all in `<Desktop>\Penumbra-and-the-Castle-of-Shadows-Enhanced\extracted\app\`. I read every file in full. It also covers the helpers those files call that live elsewhere: `util.as`, `interface.as`, `eth_util.as` (frameTimer), `main.as`, `setupScene.as` (spawn and doLoop), `messageManager.as` and `cameraManager.as`. It also covers the relevant `.ent` and `.esc` data and the `data.enml` numbers.

**Where the engine semantics come from.** They are taken from the Dec-2013 Ethanon source at `<Desktop>\Magic-Portals-Remake\reference\ethanon\toolkit\Source\src`. Anything derived only from that source is labelled **[2013]**, because the game ran on a 2010 engine. I cross-checked the 2010 `machine.exe` API strings wherever possible.

**File encoding.** Text files are Latin-1. Every Portuguese string below is given with its accents; the port must keep them.

Nothing was modified. The only scratch output was the sprite-grid PNGs, written to the scratchpad.

---

## 0. Negative answers (what the game does NOT have)

A porter should not invent any of these:
- **No 8-direction movement.** Movement is horizontal walking plus jumping, with gravity. Up and down have no movement meaning:
  - UP is also the jump key.
  - DOWN only matters inside combos and at the `next_level` door (`main.as:208`).
- **No invulnerability frames** after taking a hit. The only protection is that a given attack entity damages a given target at most once (§12.4).
  - Dead entities are made non-collidable.
  - After the king dies, players get `hp = 100` every frame (`controlCharacters.as:285-288`, `317-320`).
- **No contact damage.** Two characters that overlap only knock each other back (§7.3).
- **No fall damage.** A hard landing is only a visual and audio effect (§7.4).
- **No HP regeneration.** MP regenerates at +1 per >350 ms (§14).
- **No cooldown on the fire spell or the light spell.** They are gated by mana only.
  - The light spell also cannot be recast while one is alive.
  - Only the sword has a cooldown (200 ms).
- **No mana potions.** Potions restore HP only (§13).
- **No attack, hurt or death animation frames.** Attacks are pure particle entities. Death is a black tint plus an alpha fade.
- **No kill plane.** Pits and lava kill only through `instant_death*.ent` collision boxes placed in the scenes. Falling anywhere else just continues forever.
- **No per-player keyboard for player 2.** Player 1 (the princess) is joystick-only.

---

## 1. Engine semantics the port must emulate

### 1.1 Time
- **`GetTime()`** is a uint of milliseconds since application start. Every timer in these files uses it, with unsigned subtraction.
- **`UnitsPerSecond(x)`** is `x * lastFrameElapsedMs / 1000` **[2013]** (`ETHScriptWrapper.System.cpp:95-98`).
- **`GetFPSRate()`** is **not** 1/dt **[2013]** (`D3D9Video.cpp:1127-1149`).
  - It is a frame counter sampled every 500 ms, returned as `counter*2`, clamped to at least 1.
  - `move()` divides by it, and substitutes 60 when it returns 0 (`util.as:195`).
- **Knockback and the step cap are per-frame quantities** (§7). The port should run the gameplay tick at a **fixed 60 Hz** and treat `GetFPSRate()` as 60 and `UnitsPerSecond(x)` as `x/60`.

### 1.2 Buckets (spatial grid)
- The bucket of an entity is `floor(position / 256)` per axis. `_ETH_DEFAULT_BUCKET_SIZE = 256` **[2013]**, and gameplay scenes are loaded without a bucket-size argument. Only the menu uses (1024, 256).
- An entity lives in exactly **one** bucket, chosen by its **origin position**. Its collision box is ignored for this.
- `GetEntitiesFromBucket(vector2 bucket, ETHEntityArray &arr)` **appends** that bucket's entities, in list order, to `arr`.
- `GetCurrentBucket()` returns the entity's bucket.
- Collision and damage in this game only ever see entities whose **origin** falls in the queried buckets. That is a gameplay-relevant limitation. For example, while a character is not moving horizontally, walls in the left and right neighbour buckets are not tested. The port should reproduce the exact bucket queries, not a superset.

### 1.3 Entity custom data
Each entity has a typed dictionary: int, uint, float, string.
- `CheckCustomData(name)` returns `DT_NODATA` when the key is absent. The game uses this as an "exists" test throughout.
- `AddXData(name, v)` creates or overwrites.
- `GetXData` on a missing key returns 0 or "".
- `.ent` `<CustomData>` provides the initial values.
- **Scene-placed entities carry their own full property block and custom data inside the `.esc`.** A `.ent` file is authoritative only for entities created at runtime with `AddEntity`. This is fine for combat, because players, attacks, spells and effects are all created with `AddEntity`. The potions are scene-placed, and all 54 instances in `level1`-`level3` carry `int hp = 20`, the same as `potion_small.ent`.

### 1.4 Temporary entities
An entity is **temporary** when all of the following hold **[2013]** (`ETHEntity.cpp:697-715`, `ETHActiveEntityHandler.cpp:171`):
- it has no sprite;
- it has at least one particle system;
- every one of its particle systems has `repeat > 0`.

It is deleted automatically once all its particle systems report finished. This is how the swords, fireballs, the light spell and every hit effect expire. **The sword's active hit window and the fireball's range are therefore set by particle timing** (§11.3, §12.3).

Particle timing **[2013]** (`ETHParticleManager.cpp:188-267`):
- Particle `i` of `N` is released once its elapsed time exceeds `(lifeTime + randomLifeTime) * i / N`. With `allAtOnce=1`, every particle is released on the first update.
- Each particle lives `lifeTime ± randomLifeTime/2`. It is recycled until it has completed `repeat` cycles.
- The system is finished when no particle has cycles left.

### 1.5 Particle `<SoundEffect>`
In this 2010 engine a particle system can carry `<SoundEffect>file</SoundEffect>`. The 2010 `machine.exe` exports `HasSoundEffect()`, `SetSoundVolume()` and `SilenceParticleSystems()`, and has the `soundVolume` XML attribute. The effect plays at the entity's `soundVolume` attribute.

**This is how every sword swing, hit, explosion, potion pickup and death sound in combat is played.** No script plays them: `sword01.mp3`, `sword_combo.ogg`, `hit01.ogg`, `explosion.ogg` and `potion_pick.ogg` are preloaded in `setupScene.as:145-155` but never passed to `PlaySample`. The 2013 source has dropped this feature. Whether the sound plays once or on every repeat cycle is an open question. Every combat entity that carries one has `repeat="1"`, so "once, at spawn" is the safe reading.

### 1.6 Samples (`PlaySample`)
**[2013]** (`AudiereSample::Play`):
- There is one voice per file name.
- `PlaySample` restarts the file from the start if it is already playing.
- Volume is the global volume times the per-sample volume.
- `SetSampleVolume(file, v)` sets that per-sample volume, and it persists.
- `IsSamplePlaying(file)` queries the single voice.

### 1.7 Input key states
Each key has four states:
- `KS_HIT`: the first frame down.
- `KS_DOWN`: held.
- `KS_RELEASE`: the first frame up.
- `KS_UP`.

`input.KeyDown(k)` is `state == KS_HIT || state == KS_DOWN` **[2013]** (`WinInput.cpp:159-163`).

Joysticks **[2013 WinMM]**:
- `JK_01`..`JK_32` are button numbers starting at 1 (bit `1<<(n-1)`).
- `JK_LEFT/RIGHT/UP/DOWN` are **derived from the X/Y axis with a threshold of ±0.8**, and have their own HIT/DOWN/RELEASE edges.
- `GetJoystickXY(j)` is the analog axis clamped to [-1, 1], with a dead zone of |v| < 0.01 set to 0.
- `GetJoystickStatus(j) == JS_DETECTED` gates every joystick read. `DetectJoysticks()` is re-run while `J` is held in gameplay (`setupScene.as:393-394`).

### 1.8 Callback model
`ETHCallback_<entityNameWithoutExt>(ETHEntity@)` runs once per frame per entity. The 2013 engine runs callbacks for every dynamic entity, and for static entities only when they are visible. The 2010 behaviour is an **open question**, and there is evidence that it ran callbacks only for entities in visible buckets:
- `g_numNpcs` is a per-frame counter of NPCs processed.
- Static potions have callbacks.
- The princess's `lastTimeAlive < 4000` guard (§16.2) only makes sense if callbacks can stop running.

### 1.9 Math helpers the scripts rely on
- **`GetAngle(v)`** is `atan2(v.x, v.y)`, wrapped to [0, 2π) (`ETHScriptObjRegister.cpp:103-107`). So (0, +1) (screen-down) is 0°, +x is 90°, (0, −1) is 180°, and −x is 270°. `GetAngle(0,0)` is 0.
- **`normalize(0,0)`** returns (0,0) (`GameMath.h:755-761`).
- **`GetSize()`** is the sprite **frame** size times scale **[2013]** (`ETHSpriteEntity.cpp:652-682`). For the players this is **32×48**, a 128×192 sheet cut 4×4.
- **`GetCollisionBox()`** returns `{pos, size}`. `pos` is the box offset relative to the entity origin, and scripts add `GetPosition()` to it. `size` is the full extent. Both are vector3; only x and y are used.
- **`checkBoxHit`** (`util.as:350-372`) is AABB overlap on x/y with *inclusive* edges. Boxes that merely touch count as a hit.

---

## 2. Global script state touched by this dimension

These live in `main.as:43-60`. **AngelScript globals persist across `LoadScene`.** Only `resetData()` (`main.as:62-72`) resets them, and it is called from `newGame` and `goToPvp`.

| Global | Type | Initial | Reset by resetData | Meaning |
|---|---|---|---|---|
| `g_lives` | int | from `data.enml` `global.lives` = **13** (`main.as:137`) | yes, back to 13 | Shared campaign lives. Decremented on each player-0 death and on each co-op summon. |
| `g_exp[2]` | int[] | 0,0 | yes, 0 | Experience per player. |
| `g_charLevel[2]` | int[] | **8,8** (`main.as:56`), then resetData sets 1 | yes, **1** | Character level. The Page Up key at campaign start sets it to 15 (`main.as:114-115`). |
| `g_castingLight[2]` | bool[] | false | yes | "A light spell is alive for player N". |
| `g_pvpPoints[2]` | int[] | 0 | yes | PvP score. |
| `g_comboManager[2]` | Combo[] | constructed once | **no** | Combo input buffers. |
| `g_gameFinished` | bool | false | yes | Set by the king's death or by a PvP win. |
| `g_frameTimers` | dictionary | | `deleteAll()` in setupScene (`setupScene.as:165`) | Per-entity animation timers, keyed `"id"+ID`. |
| `g_messages` | MessageManager | | no | HUD messages and floating damage numbers. |
| `g_camera` | CameraManager | | no; `setMainCharPos((0,0), 0/1)` is called in setupScene | Camera and screen shake. |
| `g_numNpcs` | uint | | zeroed every frame in doLoop | Debug counter only. |

---

## 3. Constants (`constants.as`, `util.as`, `combo.as`)

| Name | Value | Where |
|---|---|---|
| `GRAVITY` | 1200.0 (px/s²) | `constants.as:50` |
| `STANDING` / `CHASING` / `COOLDOWN` | 0 / 1 / 2 (AI `action`) | `constants.as:52-54` |
| `DEAD_FADE_OUT_TIME` | 3000 ms | `:56` |
| `LIVE_FADE_IN_TIME` | 3000 ms | `:57` |
| `PVP_MODE` / `CAMPAIGN` | 1 / 2 (unused here) | `:59-60` |
| `DEFAULT_CHARSPRITE_CUTX/Y` | 4.0 / 4.0 (unused) | `:62-63` |
| `SIZE_TOLERANCE` | 76 (`isInScreen` margin) | `:65` |
| `ATTACK_MODE_PHYSICAL` / `ATTACK_MODE_FIRE` | 0 / 1 | `:67-68` |
| `MAX_PLAYERS` | 2 | `:70` |
| `MAX_PVP_POINTS` | 3 | `:71` |
| `MAIN_CHARACTER_ENTITY0` / `1` | `"bruxo.ent"` / `"princess.ent"` | `:87-88` |
| `RIGHT` / `LEFT` / `DOWN` / `UP` | **0 / 1 / 2 / 3** | `util.as:238-241` |
| `MAX_COMBO_KEYS` | 5 | `combo.as:43` |
| `BUTTON_STRIDE` | 210 ms | `combo.as:44` |
| `CMD_NONE, UP, DOWN, LEFT, RIGHT, SWORD, SPELL` | 0, 1, 2, 3, 4, 5, 6 | `combo.as:46-52` |

**`isAMainCharacter(e)`** (`constants.as:90-97`) is true if and only if `e.GetEntityName()` is `"bruxo.ent"` or `"princess.ent"`.

---

## 4. The player entities

### 4.1 `entities/bruxo.ent` (player 0, the wizard) and `entities/princess.ent` (player 1)
The two files are identical except for the sprite, the normal map, `playerId`, and bruxo's extra `xp` key versus princess's extra `waitBeforeAttack` key.

- **Entity attributes.** `type=0`, `static=0`, `collidable=1`, `startFrame=8`, `applyLight=1`, `castShadow=0`, `blendMode=2`, `soundVolume=1`, `specularPower=50`, `specularBrightness=1`, `EmissiveColor 0,0,0,0`.
- **Sprite.** `SpriteCut x=4 y=4`. The sprite is `bruxo.png` or `princess.png`: 128×192 RGBA, so **each frame is 32×48**. The normal map is `bruxo_nm.png` (in `entities/normalmaps/`) or `princess_height.png`.
- **Collision.** `Position (0, 4, 0)`, `Size (14, 36, 8)`. The box centre is 4 px below the entity origin, and the box is 14×36.
- **Particle system 0, a black "shadow aura".**

  | Attribute | Value |
  |---|---|
  | particles | 45 |
  | allAtOnce | 0 |
  | alphaMode | 4 |
  | repeat | 0 (endless) |
  | lifeTime | 1250 |
  | randomLifeTime | 850 |
  | size | 38 |
  | growth | −0.5 |
  | Bitmap | `black_opaque.png` (48×48) |
  | Direction | (0, −0.4) |
  | RandomizeDir | (0.6, 1.3) |
  | StartPoint | (0, −1, −2) |
  | RandStartPoint | (0, 26) |
  | Colors | white |

  It is killed on death (`KillParticleSystem(0)`).
- **Light.** `active=0`, so there is no light.

**CustomData at load:**

| key | type | bruxo | princess | used for |
|---|---|---|---|---|
| coolDown | uint | 200 | 200 | Sword cooldown, ms |
| currentDir | uint | 0 (RIGHT) | 0 | Facing |
| damage | int | 20 | 20 | Base sword damage |
| forceX, forceY | float | 0 | 0 | Velocity, px/s |
| hp / maxHp | int | 100 / 100 | 100 / 100 | |
| mp / maxMp | int | 100 / 100 | 100 / 100 | |
| jumpForce | float | 507 | 507 | Jump velocity, px/s |
| jumps / maxJumps | int | 0 / 2 | 0 / 2 | Double jump |
| knockBackX/Y | float | 0 | 0 | Knockback, px **per frame** |
| lastSwordAttack | uint | 0 | 0 | ms timestamp |
| playerId | uint | 0 | 1 | |
| speed | float | 150 | 150 | Walk speed, px/s; also the spell speed base |
| stride | uint | 100 | 100 | Animation frame time, ms |
| touchingGround | uint | 0 | 0 | |
| walking | uint | 0 | 0 | Write-only (`controlCharacters.as:49`) |
| xp | int | 0 | n/a | Unused (the global `g_exp` is used instead) |
| waitBeforeAttack | uint | n/a | 0 | |

### 4.2 How players are created
- **Campaign and PvP.** Scenes contain `spawn` marker entities with custom data `string name = "bruxo"` or `"princess"` and **`int complete = 0`**. Player markers exist in level1, level2 and level3 (bruxo only) and in pvp_lv1..6 (both).
  - `doLoop` (`setupScene.as:257-312`) spawns a marker once `isInScreen(marker)` is true. It does `AddEntity(name+".ent", marker.GetPosition() - (0,0,10))`, so **player z = −10**, and deletes the marker.
  - It then sets `waitBeforeAttack = 0` and `lastTimeAlive = 0` (`:282-283`).
  - In PvP it also sets `pvpMode = 1`.
- **`complete` is tested only for existence** (`:276-277`, `CheckCustomData("complete") != DT_NODATA`). Players therefore count as "complete":
  - In PvP their **hp and maxHp are multiplied by 5, to 500/500** (`:289-293`).
  - **`spawn()` is NOT called for players** (`:308-309`). They keep the `.ent` values, facing RIGHT (currentDir 0). `spawn()` would have set LEFT and loaded `data.enml`, which has no bruxo or princess section.
  - `g_camera.setMainCharPos(pos, playerId)` is called (`:296-299`).
- **Campaign co-op princess.** She is created by `player1Summoner` with a plain `AddEntity` (§16). She has no pvpMode, no `waitBeforeAttack` from doLoop (her `.ent` has it at 0), and no `lastTimeAlive` until her first callback ends.
- **Checkpoint respawn** loads a scene saved at runtime (§15.3). The player entity comes back from that file, **not** from the `.ent`.

### 4.3 Sprite sheet layout
I rendered the grid and checked it visually. It is the classic RPG-Maker 4×4 layout, frame index `row*4 + col`, each frame 32×48:

| Frames | Row | Content | Used by the game |
|---|---|---|---|
| 0-3 | 0 | Facing the camera, walk cycle | **never** |
| 4-7 | 1 | Facing LEFT, walk cycle | walk left 4..7; idle left 4; rising left 4; **falling left 5** |
| 8-11 | 2 | Facing RIGHT, walk cycle | walk right 8..11; idle right 8 (and startFrame 8); rising right 8; **falling right 9** |
| 12-15 | 3 | Facing away, walk cycle | **never** |

---

## 5. Per-frame update order for the players

### 5.1 `ETHCallback_bruxo` (`controlCharacters.as:270-302`)
1. `g_camera.setMainCharPos(GetPositionXY(), 0)`. This is unconditional, so it runs even while dead.
2. Look up the frame timer `g_frameTimers["id"+ID]`. If it is missing, create one and call `Set(GetFrame(), GetFrame(), 0)` (`:277-283`).
3. If `g_gameFinished`, then `hp = 100` (`:285-288`).
4. If `!isMainCharDead(this, "fade_out_beam.ent")`, run in this order:
   1. `applyForce(this, (0, UnitsPerSecond(GRAVITY)))`, which is `forceY += 1200*dt`.
   2. `controlCharacter(this, 0)` (§6, §8, §10-§12).
   3. `move(this)` (§7).
   4. `doCharacterCollision(this, false)` (§7.2).
   5. `animateCharacter(this, timer)` (§9).
   6. `drawPlayerStatus(this)`: the HUD (§17).
   7. `doMpRecovery(this, 350, 1)` (§14).
   8. If not pvpMode, `player1Summoner(this)` (§16).

### 5.2 `ETHCallback_princess` (`:304-367`)
- She gets the same timer creation and the same `g_gameFinished` hp = 100.
- She is dead if `pvpMode ? isMainCharDead(...) : isDead(...)`. Both calls use `"fade_out_beam.ent"`.
- If she is alive:
  1. `setMainCharPos(pos, 1)`. Unlike bruxo, this happens only while alive.
  2. Gravity, `controlCharacter(this, 1)`, `move`, `doCharacterCollision(false)`, `animateCharacter`, `drawPlayerStatus`, `doMpRecovery(350, 1)`.
  3. The campaign-only screen leash (§16.2).
- If she is dead: `setMainCharPos((0,0), 1)`, so the camera drops her.
- **Always** at the end: `lastTimeAlive = GetTime()` (`:366`).

---

## 6. Input (`playerInput.as`)

### 6.1 Joystick assignment
`g_controls` is a `Switch` in `videoModes.as:45`, set in the video-options screen.
- `getPlayerJoystick(p)` (`:43-53`):
  - p=0 returns `g_controls.getCurrent()==0 ? 1 : 0`.
  - p=1 returns `g_controls.getCurrent()==0 ? 0 : 1`.
  - Any other p returns 2.
- **Default (0), "2º joystick para jogador 2":** player 0 is the keyboard plus joystick **1**; player 1 is joystick **0**.
- **Option 1, "1º joystick para jogador 1":** player 0 is the keyboard plus joystick 0; player 1 is joystick 1.
- `hasASecondController()` (`:159-164`) is true when the joystick assigned to player 1 is `JS_DETECTED`.

### 6.2 Button functions
Every function below has the same shape:
1. For player 0 only, check the keyboard keys in order. The first key that is `KeyDown` (HIT or DOWN) returns **its** `GetKeyState`.
2. Otherwise, if that player's joystick is detected, return `JoyButtonState(joy, button)`.
3. Otherwise return `KS_UP`.

**A held higher-priority key shadows a newly pressed lower-priority one.** For example, holding ↑ makes a Ctrl press invisible to jump.

| Function | Player-0 keys, in priority order | Joystick | Action | Lines |
|---|---|---|---|---|
| getLeftButtonStatus | `K_LEFT` | `JK_LEFT` | combo CMD_LEFT | 55-73 |
| getRightButtonStatus | `K_RIGHT` | `JK_RIGHT` | combo CMD_RIGHT | 75-93 |
| getUpButtonStatus | `K_UP` | `JK_UP` | combo CMD_UP | 95-113 |
| getDownButtonStatus | `K_DOWN` | `JK_DOWN` | combo CMD_DOWN | 115-133 |
| getJumpButtonStatus | `K_UP`, then `K_CTRL` | `JK_03` | jump | 176-199 |
| getAttack01ButtonStatus | `K_S` | `JK_04` | sword; combo CMD_SWORD | 201-219 |
| getAttack02ButtonStatus | `K_D` | `JK_02` | fire ball; combo CMD_SPELL | 221-239 |
| getAttack03ButtonStatus | `K_SPACE` | `JK_01` | light spell | 241-259 |
| getConfirmButtonStatus | `K_RETURN` (only if `GetKeyState(K_ALT)==KS_UP`), then `K_LMOUSE`, then `K_RMOUSE` | `JK_10` | co-op summon (player 1); menus | 261-287 |
| getCancelButtonStatus | `K_ESC` | `JK_09` | back to menu | 289-307 |

The in-game help text (`menu.as:123-149`) confirms this mapping: "Pulo: ^ ou CTRL (joystick 3)", "Ataque/espada: S (joystick 4)", "Ataque/fogo: D (joystick 2)", "Acionar Luz: ESPAÇO (joystick 1)", and "START no 2º controle" for the summon.

**Recommended mapping to a modern pad.** The numbering fits a PlayStation-style USB adapter: 1 = △, 2 = ○, 3 = ×, 4 = □, 9 = Select, 10 = Start. Mapped onto GLFW standard buttons, that gives:

| Original | Action | GLFW button |
|---|---|---|
| JK_03 | Jump | A (south) |
| JK_04 | Sword | X (west) |
| JK_02 | Fire | B (east) |
| JK_01 | Light | Y (north) |
| JK_10 | Summon / confirm | Start |
| JK_09 | Cancel | Back |

This is a design decision, not a fact from the source.

### 6.3 Movement axis
- **`getPlayerXYAxis(p)`** (`:135-157`) starts from r = (0,0).
  - For player 0, keyboard keys are checked in order: `K_LEFT` sets x = −1; then `K_RIGHT` sets x = +1, so right wins if both are held; `K_UP` sets y = −1; `K_DOWN` sets y = +1.
  - Then, if the player's joystick is detected, `r += GetJoystickXY(joy)`. The analog value is added without rounding.
- **`getInputDirection(p)`** (`:166-174`) returns RIGHT if x > 0, LEFT if x < 0, and otherwise **DOWN**.
- With the 0.01 dead zone, **any analog deflection gives full walking speed**. There is no analog speed scaling.

---

## 7. Movement physics

### 7.1 Forces and `move()` (`util.as:161-236`)
- `forceX` and `forceY` are velocities in px/s. `applyForce` adds to them. `setForceX/Y` sets them (`AddFloatData` overwrites).
- **Gravity:** `forceY += UnitsPerSecond(1200)` every frame, before input (§5).
- **Horizontal** (`playerInput.as:339-352`). Only the input direction sets these; there is no acceleration or friction.
  - LEFT: `currentDir = LEFT`, `forceX = −speed` (−150).
  - RIGHT: `currentDir = RIGHT`, `forceX = +150`.
  - Otherwise `forceX = 0`.

**`move(e)`** (`util.as:188-219`), in order:
1. If `touchingGround == 1 && forceY > 0`, then `forceY = 0`. On the ground, gravity never builds up.
2. `v = (forceX, forceY) / fps`, where fps is `GetFPSRate()`, or 60 if it returns 0. This is the displacement this frame.
3. `size = min(GetSize().x, GetSize().y) * 0.4`. For the players that is min(32, 48) × 0.4 = **12.8 px**.
4. If `|v| >= size`, then `v = normalize(v) * size * 0.9`, which is **11.52 px/frame**. At 60 fps that caps the speed at 691 px/s. It also slows horizontal motion during a fast fall, because the whole vector is scaled.
5. `k = (knockBackX, knockBackY)`. If `|k| >= size`, then `k = normalize(k) * size * 0.9`.
6. `k *= 0.8`. Set any component with `|c| < 0.1` to 0. Store `k` back to `knockBackX/Y`.
7. `AddToPositionXY(v + k)`.

**`knockBack(e, v)`** (`util.as:161-167`) does `knockBackX += v.x` and `knockBackY += min(0, v.y)`. **Only upward knock is kept**; any downward component is dropped. Knock values are **pixels per frame**.
- The per-frame decay is ×0.8 and happens before the move is applied, so an impulse `k0` travels about 0.8k0 + 0.64k0 + … ≈ **4·k0 px** in total. That is 16 px for a knock of 4, 28 px for 7, 48 px for 12, and 60 px for 15 before the cap.
- For a 32×48 character, any single-frame knock of 12.8 or more is first clamped to 11.52, giving about 46 px in total.

### 7.2 `doCharacterCollision(e, useNpcInvisibleWalls)` (`controlCharacters.as:43-220`)
The players call it with `false`, so `npc_wall.ent` is ignored. NPCs call it with `true`.

1. `force = (forceX, forceY)`. This is read **after** `move()` has run this frame.
2. `dir = findDirection(force)`, where `findDirection` (`util.as:242-265`) uses the angle `a = degrees(GetAngle(v))`:
   - 45 ≤ a < 135 gives RIGHT;
   - 135 ≤ a < 225 gives UP;
   - 225 ≤ a < 315 gives LEFT;
   - anything else gives DOWN, including the zero vector.
3. `walking = (dir == LEFT || dir == RIGHT) ? 1 : 0`. This is write-only.
4. **Candidate list:** call `GetEntitiesFromBucket(currentBucket)`, then append `findDestinationBuckets(e, force, true)` (`util.as:326-348`). That function adds these buckets, in this order:
   - (−1, 0) if force.x < 0;
   - (+1, 0) if force.x > 0;
   - (0, +1) always;
   - (0, −1) if force.y < 0;
   - (−1, +1);
   - (+1, +1).
5. `thisBox = GetCollisionBox()`, then `thisBox.pos += GetPosition()`. Set `thinnerBoxHit = false`.
6. For each candidate `c`, **in list order**:
   1. Skip it if `!c.Collidable()`.
   2. If `!useNpcInvisibleWalls` and `c.GetEntityName() == "npc_wall.ent"`, skip it.
   3. Skip it if it is the character itself (same ID).
   4. **Skip it if `c` has an `ownerID` key.** This excludes attack entities, fireballs and the light spell.
   5. Get `box` as `c`'s absolute collision box. If `checkBoxHit(thisBox, box)` is true:
      - If `c` is named `instant_death.ent`, `instant_death2.ent` or `instant_death3.ent`, set `e.hp = 0`. In addition, if `e` is a main character in **pvpMode**, do `g_pvpPoints[e.playerId]--` (a suicide penalty) and `e.dontFrag = 1`, which is write-only. Processing does **not** continue to the next candidate here: the instant-death box also acts as solid ground below.
      - If `c` has a `dontCollide` key, skip to the next candidate.
      - If `c` has an `hp` key (and `e` does too), the two characters bump. Call `knockBack(e, normalize(ePos − cPos) * 7)` and `knockBack(c, normalize(cPos − ePos) * 7)`. Add `"jumpfx.ent"` at `((ePos + cPos)/2, z = 0)` with angle 0. Then skip to the next candidate. There is no solid response and no damage. It happens **every frame** they overlap, and the other character's own callback does it again.
      - Otherwise solve it as a solid:
        - `thinnerBox = thisBox`, with `size.x *= 0.6`. Set `thinnerBoxHit = checkBoxHit(thinnerBox, box)`. This value is overwritten on every solid hit.
        - `currentPos = GetPositionXY()`, read afresh on each iteration.
        - `collDir = findBoxDirection(thisBox, box)` (§7.2.1).
        - **RIGHT** (the obstacle is on the right): `forceX = 0`. Set the position to `(box.pos.x − box.size.x/2 − thisBox.size.x/2 − 1, currentPos.y)`. Recompute `thisBox`.
        - **LEFT:** `forceX = 0`. Set the position to `(box.pos.x + box.size.x/2 + thisBox.size.x/2 + 1, currentPos.y)`. Recompute `thisBox`.
        - **DOWN** (the obstacle is below): only if `force.y > 0` **and** `thinnerBoxHit`.
          - If `force.y > 700`, play the hard-landing effect: `PlaySample("soundfx/fall.ogg")`, `AddEntity("fall.ent", (thisBox.pos.x, thisBox.pos.y + thisBox.size.y/2, 0), 0)`, and `g_camera.startEarthquake(10)`.
          - Then `forceY = 0`, set the position to `(currentPos.x, box.pos.y − box.size.y/2 − thisBox.size.y/2)`, and `touchingGround = 1`.
          - **`thisBox` is NOT refreshed after this snap.**
          - The snap sets the entity **origin**, ignoring the box's +4 y offset. A standing player's box therefore overlaps the floor by **4 px**, which keeps the floor colliding every frame.
        - **UP** (the obstacle is above): only if `thinnerBoxHit`. Set `forceY = 0` and the position to `(currentPos.x, box.pos.y + box.size.y/2 + thisBox.size.y/2)`. `thisBox` is not refreshed.
7. After the loop:
   - If `force.y < 0`, set `touchingGround = 0`.
   - Otherwise, if `!thinnerBoxHit` (the value from the **last** solid hit), set `touchingGround = 0`.
   - Otherwise `touchingGround` keeps its current value, whether set this frame or earlier.
8. If `touchingGround == 1`, set `jumps = 0`.

#### 7.2.1 `findBoxDirection(a, b)` (`util.as:267-303`)
`a` is the mover and `b` the obstacle. Take their positions `pA`, `pB` and half-sizes `hA`, `hB`.
- `horizontalyAligned` is `pB.y − hB.y < pA.y < pB.y + hB.y`. If true, the result is `pA.x < pB.x ? RIGHT : LEFT`. This test has precedence.
- `verticalyAligned` is `pB.x − hB.x < pA.x < pB.x + hB.x`. If true, the result is `pA.y < pB.y ? DOWN : UP`.
- Otherwise, if `pA.y + hA.y − 5 <= pB.y − hB.y`, the result is DOWN.
- Otherwise the result is `findDirection(b.pos − a.pos)`, the angle-based rule from step 2 of §7.2.

The response depends on order: the pushes and the stale `thisBox` make it depend on candidate order within the bucket lists.

### 7.3 Character bumping
This is covered in §7.2 step 6: a ±7 px/frame knock to each character, and a `jumpfx.ent` every frame of overlap. `jumpfx.ent` is 9 particles of `fog.dds`, allAtOnce, repeat 1, life 200 ± 50, with no sound.

### 7.4 Falling, pits and lava
- **A hard landing triggers when `forceY > 700` at contact.** `forceY` reaches 700 after 0.583 s of free fall from rest, which is 204 px of fall while below the 768 px/s cap.
  - It plays `soundfx/fall.ogg` (volume 1), spawns `fall.ent` (identical data to `jumpfx.ent`), and sets earthquake 10.
  - **It does no damage.**
- **Pits.** `instant_death.ent` in level1 (61 instances, 64×64 box, type 5, sprite `black.bmp`), `instant_death3.ent` in level3, pvp_lv1 and pvp_lv2 (64×64, no sprite), and `instant_death2.ent` in pvp_lv2 (256×256, static). Touching any of them sets hp = 0 (§7.2).
- **Lava.** `lava.ent` is only a static, non-collidable sprite. The instant_death3 boxes under it kill. The lava shooters (`lavaShooter.as`) deal 18 damage and knock 15 × pushBackBias to anything with hp, players included. `fire_shoot` hits skip the instant_death and npc_wall entities.
- **Falling bridges** (`events.as:67-106`) start to fall 600 ms after a main character's box touches the stone box.

---

## 8. Jumping (`playerInput.as:316-337`)
The jump fires on `getJumpButtonStatus(p) == KS_HIT`:
1. Set `n = jumps` and `grounded = (touchingGround != 0)`. The `touchingGround` value is the one from the previous frame's collision.
2. `mayJump = !(!grounded && mp < 4)`. An air jump needs at least 4 MP.
3. If `n < maxJumps (2) && mayJump`:
   - If `!grounded`, call `addToMp(−4)`.
   - Set `forceY = −jumpForce`, which is **−507 px/s**. This is set, not added.
   - Set `jumps = n + 1`.
   - If `!grounded`, call `g_camera.startEarthquake(2.5)` and `PlaySample("soundfx/jump0" + (p+1) + ".ogg")`. That is `jump01.ogg` for player 0 and `jump02.ogg` for player 1, at volume 1.
   - **Ground jumps are silent, cost nothing and do not shake the screen.**

**Numbers.** v0 = 507 px/s and g = 1200 px/s², so the apex is **107.1 px**, reached in 0.4225 s. A second jump resets the velocity to −507, giving up to about 214 px of height.

**Quirk to keep.** `jumps` resets only while grounded. Walking off a ledge leaves `jumps = 0`, so the player gets **two** air jumps, costing 4 MP each.

---

## 9. Animation (`animateCharacter`, `controlCharacters.as:222-268`; `frameTimer`, `eth_util.as:128-167`)
**`frameTimer.Set(first, last, stride)`:**
- If `(first, last)` differs from the stored range, jump to `first`, store the new range, set `lastTime = now`, and return.
- Otherwise, if `now − lastTime > stride` (strictly), advance the frame, wrapping from `last` back to `first`, and set `lastTime = now`.

`Get()` returns the current frame.

**Rules.** `stride = 100` ms for the players. The comparison is strict, so at 60 fps each frame shows for 116.7 ms. `force` is read after collision, so pushing against a wall shows the idle frame.

| grounded? | condition | frames |
|---|---|---|
| yes | forceX < 0 | 4→7 loop |
| yes | forceX > 0 | 8→11 loop |
| yes | forceX == 0, currentDir LEFT | 4 |
| yes | forceX == 0, currentDir RIGHT | 8 |
| no | LEFT, forceY > 0 | 5 |
| no | LEFT, forceY ≤ 0 | 4 |
| no | RIGHT, forceY > 0 | 9 |
| no | RIGHT, forceY ≤ 0 | 8 |

Any other `currentDir` leaves the range unchanged. Then `SetFrame(timer.Get())`.

---

## 10. Combos (`combo.as`; used at `playerInput.as:354-395`)

### 10.1 The `Combo` buffer
Each player has one `Combo`, holding `keys[5]`, `lastInput` and `index`, all starting at 0 / CMD_NONE.

**`updateInput(p)`** is called once per frame from `controlCharacter`:
1. If `index >= 5`, set `index = 4`. After five keys, further keys keep overwriting slot 4.
2. Check in this order, taking the **first** that is `KS_HIT` this frame: Left, Right, Up, Down, Attack01 (sword), Attack02 (spell). On a hit, set `keys[index] = CMD`, `lastInput = now`, increment `index`, and **return**. At most one command is recorded per frame, so pressing →+S together records only →.
3. If nothing was hit and `now − lastInput > 210`, call `zero()`. The code also checks `index == 5`, but step 1 clamps `index` first, so that branch can never be taken.

**`checkSequence(a, b, c)`** matches only when `keys[0]==a`, `keys[1]==b` and `keys[2]==c`, that is, the first three keys since the last reset. On a match it calls `zero()` and returns true. A five-key overload exists but is unused.

**Timing.** Every gap between consecutive key presses must be at most about 210 ms. The keys must be *taps* (HIT edges), so "→ →" means two separate presses. Direction taps also move the character.

### 10.2 The two combos
The `controlCharacter` order is: jump, horizontal movement (sets `currentDir`), `updateInput`, sword combo, spell combo, sword, light, fire.

| Combo | Sequence | Mana | Effect | Lines |
|---|---|---|---|---|
| **Sword combo** | ← ← Sword **or** → → Sword. Facing is not checked. | **5** | See below. | `playerInput.as:361-377` |
| **Spell combo** ("blast") | ↓ ← Spell **or** ↓ → Spell | **25** | See below. | `:380-395` |

**Sword combo.**
- If `mp >= 5`:
  - `addToMp(−5)`.
  - `swordAttack(e, "combo_sword.ent", "sword_beam.ent", true)`.
  - If `forceY > 0`, set `forceY = 0`.
  - `didSwordCombo = true`.
  - `startEarthquake(7)`.
- Otherwise show the message "É necessário ter 5 de mana para realizar este combo".
- **Quirk.** Mana is deducted and the screen shakes *before* `swordAttack` checks the 200 ms cooldown. A combo within 200 ms of the last swing wastes 5 MP and produces no combo sword.

**Spell combo.**
- `castSpell(e, "combo_fire_ball.ent", 25, (0,0,0), damage 225, speedMult 1)`.
- On success: `PlaySample("soundfx/blast_attack.ogg")`, played at volume **0.5** because `setupScene.as:158` calls `SetSampleVolume` on it. If `forceY > 0`, set `forceY = 0`. `didSpellCombo = true`.
- On failure: show "É necessário ter 25 mana para este combo".

A failed combo does **not** suppress the plain attack on the same frame. With 3 MP, for example, ↓→D shows the combo message, and then the normal fire spell is also attempted and fails with its own message.

---

## 11. Sword attacks (`swords.as`)

### 11.1 Player trigger (`playerInput.as:398-405`)
The sword fires when all of these hold:
- `getAttack01ButtonStatus(p) == KS_HIT`;
- `(now − lastSwordAttack) > coolDown` (200, strict);
- `!didSwordCombo`.

It then calls `swordAttack(e, "sword" + p + ".ent", "sword_beam.ent", true)`. That is `sword0.ent` for bruxo and `sword1.ent` for the princess. If `forceY > 0`, `forceY` is set to 0.

**Attacking in mid-air cancels the fall velocity.** Spamming attacks slows a fall. This applies to every attack and cast.

### 11.2 `swordAttack(owner, swordName, beamName, isInRange)` (`swords.as:43-107`)
1. If `!isInRange`, set `lastTimeDidntSee = now` and return false. This is used only by the AI.
2. Cooldown gate:
   - If `waitBeforeAttack == 0`: return false if `now − lastSwordAttack < coolDown`.
   - Otherwise: return false if `now − lastTimeDidntSee < coolDown`.
3. `pos = owner.GetPosition()`, including z.
   - If `currentDir == LEFT`, `swordPos = pos + (−28, 0, 10)`.
   - Otherwise (RIGHT or anything else), `swordPos = pos + (+28, 0, 10)`.
4. Add the entities. The sword goes at `swordPos + (0,0,8)`, so the offset is **(±28, 0, +18)**; for a player at z −10 that puts it at z = 8. The beam goes at `pos + (0,0,8)`.
5. If LEFT, call `MirrorParticleSystemX(0, true)` on both. **Only the particles are mirrored. The collision-box offset (−4, +4) is not.**
6. Set custom data on the sword:
   - `ownerX`, `ownerY` (float): the owner's position at attack time;
   - `ownerID` (int);
   - `direction` (uint, the facing);
   - `damage` (int, the owner's `damage`, 20 for players).
7. If the owner is a main character, also set `mainCharacter = 1`, `pvpMode = 1` if the owner has pvpMode, and `playerId`.
8. Set `owner.lastTimeDidntSee = now` and `owner.lastSwordAttack = now`. Return true.

**The sword does not follow its owner.** It stays where it was spawned for its whole life.

### 11.3 Sword entities and hitboxes
The box offset is relative to the sword origin and is not mirrored.

| Entity | Callback → doDamage args | Box offset / size | Absolute hitbox x-range, relative to owner | Particles | Light | Sound (vol) |
|---|---|---|---|---|---|---|
| `sword0.ent` | knock **4**, fromOwner, physical, ×1 (`:120-123`) | (−4, 4) / **65×38** | facing R: −8.5 … +56.5; facing L: −64.5 … +0.5; y −15 … +23 | 16 × `sword.png`, repeat 1, life 150, dir (12.56, 2.16), grav (−3.04, 0), start (−13, 0), color (0.5, 0.5, 1) | active, range 169, color (0.5, 0.5, 1), halo `halo1.bmp` (170, brightness 0.3), pos (19, 8, 16) | `sword01.mp3` @0.7 |
| `sword1.ent` | same as sword0 (`:124-127`) | same | same | same but color0 (1, 0.5, 0.5), color1 (1, 0, 1) | color (1, 0.5, 1) | `sword01.mp3` @0.7 |
| `combo_sword.ent` | knock **25**, fromOwner, physical, **×5** (`:128-131`) | (−4, 4) / **103×61** | facing R: −27.5 … +75.5; facing L: −83.5 … +19.5; y −26.5 … +34.5 | 16 × `sword.png` ×1.4 scale-equivalent (size 92.96), color (1, 1, 0.4) | color (2, 2, 1) | `sword_combo.ogg` @0.2 |
| `sword_beam.ent` | none (visual only) | none | none | 45 × `black_opaque.png`, allAtOnce, alphaMode 4, life 550, dir (−2.2, 0) | none | none |
| `enemy_sword.ent` (warrior, minion, knight) | knock 4, physical ×1 | (−1, 0) / 46×19 | | life 130 | none | `sword01.mp3` @0.5 |
| `dark_sword.ent` (master_knight) | knock 12 | (−1, 12) / 89×41 | | life 130, `black_sword.dds` | none | `dark_hit.ogg` @0.5 |
| `paladin_sword.ent` (paladin, king) | knock 15 | (−1, 0) / 64×19 | | life 130, red | red, range 149.5 | `sword01.mp3` @0.2 |

- All sword entities carry `int dontCollide = 0`. The hit effect is `string hit = "enemy_blood.ent"` for player swords and `"blood.ent"` for enemy swords.
- **Active window [2013 estimate].** 16 particles, `allAtOnce=0`, life 150, rand 0, repeat 1. The last particle is released after 150·15/16 = 140.6 ms and lives 150 ms, so the entity lives **≈ 291 ms** plus one or two frames. Enemy swords (life 130) last ≈ 252 ms.
- During the active window, `doDamage` runs every frame, and each target is hit at most once (§12.4).
- **Formula:** `window ≈ (lifeTime + randomLifeTime) × (N−1)/N + lifeTime × repeat`.

---

## 12. Spells (`spells.as`)

### 12.1 `castSpell(owner, spellName, mana, offset, damage, speedMult)` (`:43-81`)
1. If the owner has an `mp` key: return false if `mp < mana`; otherwise `addToMp(−mana)`. Owners without `mp` (NPCs) cast for free.
2. `AddEntity(spellName, owner.GetPosition() + offset)`. Set on it:
   - `spellOffsetX`, `spellOffsetY` (float);
   - `speed = owner.speed × speedMult` (float);
   - `direction = owner.currentDir` (uint);
   - `ownerX`, `ownerY`, `ownerID`;
   - `damage` (int);
   - `playerId` if the owner has one;
   - `mainCharacter = 1` and `pvpMode = 1` if they apply.
3. Return true.

There is **no cooldown**. Every KS_HIT press casts if mana allows.

### 12.2 Player casts (`playerInput.as:407-438`)

| Spell | Key / joystick | Mana | castSpell args | On success | On failure |
|---|---|---|---|---|---|
| **Light** | Space / JK_01 | **50** | `"light_spell.ent"`, offset **(0, −40, 14)**, damage 0, ×1 | `PlaySample("soundfx/light_spell.mp3")` | "É necessário ter 50 mana para esta magia" |
| **Fire ball** | D / JK_02 | **10** | `"fire_ball.ent"`, offset (0, 0, 0), damage **30**, ×1. Suppressed if `didSpellCombo`. | `PlaySample("soundfx/cast_fire_spell.ogg")`; if forceY > 0, forceY = 0 | "É necessário ter 10 mana para esta magia" |
| **Blast** (combo) | ↓ ←/→ D | **25** | `"combo_fire_ball.ent"`, offset 0, damage **225**, ×1 | §10.2 | §10.2 |

- **Light-spell lock.** If `g_castingLight[p]` is true, pressing Space shows "Esta mágica já está em execução" and does not cast.
- **`g_castingLight[p] = false` at the very end of `controlCharacter`** (`:439`). The light spell's callback sets it back to true each frame (§12.3), so it acts as a flag meaning "a light was alive during the last frame".

### 12.3 Spell entities

**`light_spell.ent`**, callback at `:83-111`:
- It is non-collidable.
- Each frame:
  1. Set `g_castingLight[this.playerId] = true`.
  2. Gather the buckets at offsets (0,0), (0,+1), (+1,+1), (−1,+1), (+1,0), (−1,0).
  3. Find the main character whose ID equals `ownerID`, and set this position to `owner.XY + (spellOffsetX, spellOffsetY)`, which is **(0, −40)**: it floats 40 px above the owner and follows them. z stays at spawn z + 14.
- Particles: 15 × `flash.bmp`, alphaMode 1, **repeat 36**, life 600 ± 250, color (0.6, 0.7, 1) fading to alpha 0.
- Light: range **319**, color (0.6, 0.6, 1), `castShadows=1`, halo `halo.bmp` (size 100, brightness 0.7). This is the game's "torch".
- **Duration [2013 estimate].** The last release comes after 1100·14/15 = 1027 ms, plus 36 × ~600 ms, so about 22.6 s on average, or about 23-24 s taking the latest particle. It does no damage.

**`fire_ball.ent` and `combo_fire_ball.ent`**, via `manageFireBall(e, explosionName)` (`:123-152`):
1. `dx = (direction == LEFT) ? −1 : +1`. Move `AddToPositionXY((dx, 0) × UnitsPerSecond(speed) × 1.5)`. For a player, speed 150 × 1 × 1.5 gives **225 px/s**. It flies horizontally, with no gravity.
2. `target = doDamage(e, knock 15, knockFromOwner false, collideEverything **true**, ATTACK_MODE_FIRE, ×1)`.
3. If `target != null`:
   - `explode = !(target has fireResistant && target.fireResistant != 0)`.
   - If it explodes: `AddEntity(explosionName, e.GetPosition(), 0)` and `startEarthquake(20)`.
   - `DeleteEntity(e)` and return.
4. Otherwise, if `e` has a `hitBy` key (set when another fireball's `doDamage` overlapped it), delete it without an explosion.

Other behaviour:
- **Explosions.** `fire_ball` spawns `explosion.ent`: 12 × `explosion.png` 4×3 animated, `hit01.ogg` @0.5, a light of range 167.5. `combo_fire_ball` spawns `big_explosion.ent`: size 168, `explosion.ogg` @0.5.
- **Fire balls explode on walls, instant_death boxes and other entities' sword swooshes**, because of `collideEverything`. A sword can therefore "parry" an enemy fireball.
- Boxes are 22×22 for `fire_ball` and 28×28 for `combo_fire_ball`.
- Lights: `fire_ball` has range 125.5, color (1, 0.7, 0.3), halo 82. `combo_fire_ball` has color (0.7, 0.7, 1), halo 108.
- **Lifetime and range [2013 estimate].**
  - `fire_ball`: 19 particles, repeat 5, life 250 ± 75. That is 379 ms plus 5 × 250 ms, about **1.6-1.8 s**, so a range of about **360-400 px**.
  - `combo_fire_ball`: 23 particles, repeat 7. That is 383 ms plus 1750 ms, about **2.1-2.3 s**, so about **480-520 px**.

---

## 13. Potions (`potions.as`)
The callbacks `ETHCallback_potion`, `_potion_small` and `_potion_large` all call `doPotion(e)` (`:43-61`):
1. For each entity `c` in **the potion's own bucket only**:
   - If `getDist(c.XY, potion.XY) < 20` and `c` is a main character:
     - If `c.hp >= c.maxHp`, **return**. A full-health player blocks the pickup, and the whole loop ends.
     - Otherwise:
       - `addToHp(c, potion.hp)`, clamped to `maxHp`;
       - `DeleteEntity(potion)`;
       - `AddEntity("potion_pick.ent", potion.GetPosition() + (0,0,16))`. This plays `potion_pick.ogg` at volume 1, with a red light of range 88.
   - There is no `break`. Both players standing on the same potion in the same frame would both heal, provided `DeleteEntity` is deferred.

The data:
- **`potion_small.ent`** has `hp = 20`. Its sprite is `skull.png` (18×20); it is static and non-collidable, with an endless sparkle particle system.
- **`potion_large.ent`** has `hp = 50`. Its sprite is `HP_Bitmap.png`. **It is not placed in any scene.**
- **54 small potions** are placed in the campaign: level1 13, level2 15, level3 26. None are placed in PvP.
- `potion.ent` does not exist.
- Potions restore **HP only**.

---

## 14. HP, MP, EXP and levels

**`addToHp(e, v)`** (`util.as:420-433`) sets hp to `max(0, min(hp + v, maxHp))`, or `max(0, hp + v)` if there is no `maxHp`. `addToMp` (`:435-448`) works the same way with `maxMp`.

**MP regeneration: `doMpRecovery(e, 350, 1)`** (`interface.as:98-109`).
- On the first call, `lastMpIncr = now`.
- On any later frame where `now − lastMpIncr > 350`, do `mp += 1` (clamped) and set `lastMpIncr = now`.
- At 60 fps that is +1 every 366.7 ms, about **2.73 MP/s**, so an empty 100 MP bar refills in about 37 s.

**Mana costs:**

| Action | Cost | Requirement |
|---|---|---|
| Air jump | 4 | mp ≥ 4 |
| Sword combo | 5 | mp ≥ 5 |
| Fire ball | 10 | mp ≥ 10 |
| Blast combo | 25 | mp ≥ 25 |
| Light | 50 | mp ≥ 50 |
| Co-op summon | 50 | **mp ≥ 51** (the test is `mp <= 50` fails) |

**Experience: `addToExp(p, x)`** (`util.as:406-418`):
1. `g_exp[p] += x`.
2. `next = data.enml global."lv"+g_charLevel[p]`.
3. While `g_exp[p] >= next`: `diff = g_exp[p] − next`; increment `g_charLevel[p]`; set `next = lv<newLevel>`; set `g_exp[p] = diff`.

- `g_exp` holds the experience **within the current level**.
- A missing key leaves `next` unchanged **[2013]** (`Enml.cpp:509-521`).
- The thresholds (`data.enml:1-35`) are:
  - lv1 200, lv2 400, lv3 800, lv4 1600, lv5 3200, lv6 4000, lv7 5000, lv8 6000, lv9 7000, lv10 8000, lv11 9000;
  - lv12 and lv13 10000;
  - lv14-19 11000;
  - **lv20 is missing** (`data.enml:22-23`);
  - lv21-30 11000.
- Reaching level 20 needs 131,200 total EXP. At level 20 or above 30, the HUD's XP bar divides by 0 (§17).

**What a level does.** It changes only the **player damage formula** (§12.4) and the "lv: N" text. It gives no HP or MP increase.

**EXP sources:**
- **Campaign.** When any non-main-character entity dies, `isDead` (`controlCharacters.as:384-389`) calls both `addToExp(0, expGiven)` and `addToExp(1, expGiven)`. Both players gain, whatever the cause of death, even if player 1 does not exist.
- **PvP.** An NPC killed by a main character's attack gives `expGiven` to the attacker only (`doDamage.as:190-199`).
- `expGiven` is set by `spawn()` to the NPC's `data.enml` hp: warrior 75, minion 45, knight 150, master_knight 1700, impy 75, paladin 400, king 3500.

### 12.4 (combat core) `doDamage(atk, knockForce, knockFromOwner, collideEverything, attackMode, damageMultiplier)` (`doDamage.as:45-242`)
Every attack entity calls this every frame. It returns the last entity that overlapped (`r`), or null.

1. `dir = atk.direction` and `mainChar = atk has "mainCharacter"`.
2. **Buckets.** Start with the attack's own bucket. Add (−1, 0) if `dir == LEFT`, otherwise (+1, 0). If `mainChar`, also add (0, +1) and then (0, −1).
3. `pvpMode = atk has "pvpMode"`.
4. **Damage.** Everything here is computed once per call, as an int.
   - If `mainChar`:
     - `lvl = g_charLevel[atk.playerId]`. The code falls back to index `MAX_PLAYERS = 2` if there is no `playerId`, which is out of bounds, but that never happens for a main character.
     - `damage = (D + int(float(D) × (float(lvl)/4))) × int(damageMultiplier)`, where `D = atk.damage`.
   - Otherwise `damage = D × int(damageMultiplier)`.
5. `ownerID = atk.ownerID`. Set `isResistant = false` **once, before the loop**.
6. For each candidate `c`, in list order:
   1. Skip it if it is not collidable, if it is `atk` itself, or if `c.ID == atk.ownerID`.
   2. Skip it if `c` has `ownerID` equal to `atk`'s `ownerID`. Two blows from the same owner never interact.
   3. Skip it if `c` has no `hp` and `!collideEverything`.
   4. If `collideEverything`, skip `npc_wall.ent`.
   5. If not pvpMode, skip `c` when `c` is a main character and `atk` has `mainCharacter` (no friendly fire).
   6. If not pvpMode, skip `c` when `c` has `mainCharacter` and so does `atk` (friendly attacks do not collide).
   7. If `checkBoxHit(c.absBox, atk.absBox)`:
      - Set `r = c` and `c.hitBy = atk.GetEntityName()` (a string).
      - If `c` is a main character, call `g_camera.startEarthquake(min(10, damage))`. This happens **every frame of overlap**.
      - If `c` has no `hp`, continue to the next candidate.
      - Let `key = "id" + c.ID`. If `atk` does **not** have `key` (the first hit on this target):
        - If `c` has `fireResistant`, `attackMode == FIRE` and `c.fireResistant != 0`, set `isResistant = true`. **It is never reset** for later targets in this loop.
        - Set `atk[key] = 0`, the per-target hit mark.
        - `damage = isResistant ? damage/5 : damage`. This is integer division, and it **writes back into `damage`**, so each further resistant target in the same call divides again.
        - `addToHp(c, −damage)`.
        - `g_messages.addMessage(−damage, c.XY)` shows a floating number.
        - **PvP scoring.** If `c` has `pvpMode` and `c.hp <= 0` after the hit:
          - If `c` is a main character: `attacker = SeekEntity(ownerID)`. If `isAMainCharacter(attacker)`, `g_pvpPoints[attacker.playerId]++`. Otherwise `g_pvpPoints[c.playerId]--`: killed by an NPC or a hazard projectile, you lose a point. Note that `isAMainCharacter(null)` would throw a script exception (§23).
          - Otherwise, if the attacker is a main character, `addToExp(attacker.playerId, c.expGiven)`.
        - **Knockback.**
          - `ownerPos` is `(atk.ownerX, atk.ownerY)` if `knockFromOwner`, otherwise `atk.XY`.
          - `back = normalize(c.XY − ownerPos) × knockForce`.
          - `bias` is 0.2 if `isResistant`, otherwise 1. It is overridden by `c.pushBackBias` if that key exists. `spawn()` gives every NPC one; players have none.
          - `knockBack(c, back × bias)`.
        - **Hit effect position.** `hp = atk.GetPosition() + ((dir == RIGHT) ? +atkBox.size.x/2 : −atkBox.size.x/2, 0, 0)`. This uses the entity origin, not the box centre.
        - If not resistant and `atk` has a `hit` key: `AddEntity(atk.hit, hp)`, and call `MirrorParticleSystemX(0, true)` if `back.x > 0`.
        - If resistant: `AddEntity("hit_fail.ent", hp)`. That entity is 4 particles and plays `cast_fire_spell.ogg` at volume 1.
        - `c.action = CHASING` (1), which aggroes an NPC.
7. Return `r`.

**Damage at level L** (`D` for the player is 20 for the sword; the spell `D` comes from castSpell):

| Attack | Formula | L=1 | L=2 | L=3 | L=5 | L=15 | vs fire-resistant |
|---|---|---|---|---|---|---|---|
| Sword (sword0/1) | 20 + trunc(5L) | **25** | 30 | 35 | 45 | 95 | n/a (physical) |
| Sword combo | (20 + 5L) × 5 | **125** | 150 | 175 | 225 | 475 | n/a |
| Fire ball | 30 + trunc(7.5L) | **37** | 45 | 52 | 67 | 142 | ÷5, so L1 = 7 |
| Blast | 225 + trunc(56.25L) | **281** | 337 | 393 | 506 | 1068 | ÷5, so L1 = 56 |

**NPC stats and damage to players** (`data.enml`). Fire-resistant NPCs are minion, impy and paladin.

| NPC | HP | Damage | Weapon |
|---|---|---|---|
| warrior | 75 | 5 | enemy_sword, knock 4 |
| minion | 45 | 20 | enemy_sword, knock 4 |
| knight | 150 | 10 | enemy_sword, knock 4 |
| master_knight | 1700 | 25 | dark_sword, knock 12 |
| impy | 75 | 18 | fire ball, knock 15 |
| paladin | 400 | 20 | paladin_sword, knock 15 |
| king | 3500 | 10 | paladin_sword, or fire ball ×1.5 speed |
| lava shooter | n/a | 18 | knock 15 |

- Player HP is 100 in the campaign and 500 in PvP.
- NPC attacks have no `mainCharacter`, so **they also damage other NPCs** (no NPC friendly-fire filter).

**Floating damage numbers** (`messageManager.as:70-80, 166-205`):
- The text is `"" + (−damage)`, for example "-25". It starts at `c.XY + (−10, −32)` in world coordinates.
- It rises by `UnitsPerSecond(15)` per frame and fades 255→0 over `4000/3 = 1333` ms.
- It is drawn with shadowText in font "Arial", size 15 (30/2), color (203, 203, 228), with a shadow at +(1.5, 1.5) in ARGB(a/2, 0, 0, 0).
- The pool is a 15-slot ring buffer.

---

## 15. Death, respawn and lives

### 15.1 `isMainCharDead(e, beam)` (`controlCharacters.as:406-469`)
This is used for bruxo always, and for the princess in PvP.
1. If `pvpMode && g_gameFinished`, return true. The player freezes, and no HUD is drawn.
2. If `hp <= 0`:
   - **First frame of death** (no `deathTime` yet):
     - `AddEntity(beam, e.GetPosition(), 0)`. `fade_out_beam.ent` is 45 black particles, life 1750 ± 650, and plays `vanish.ogg` at 0.5.
     - If there is a particle system 0, kill it.
     - `deathTime = now`, `SetColor((0,0,0))` (sprite tinted black), `SetCollision(false)`. The dead player can no longer be hit.
   - `dead = now − deathTime`.
   - **If `dead >= 3000`:**
     - Do `g_lives--`.
     - If `g_lives >= 0 || pvpMode`:
       - With no `hasCheckpoint`: `LoadScene(GetSceneFileName(), "setupScene", pvp ? "pvpLoop" : "levelLoop")`.
       - Otherwise: `LoadScene("scenes/checkpoint.esc", "setupScene", "levelLoop")`.
     - Otherwise: `LoadScene("scenes/gameover.esc", "gameOverPreLoop", "gameOverLoop")`.
     - The function does not return here. It relies on `LoadScene` being deferred (§23).
   - `a = dead/3000`. If `soundfx/fase.mp3` is playing, `SetSampleVolume("soundfx/fase.mp3", 1 − a)` (music fade).
   - `SetAlpha(1 − a)`.
   - `DrawRectangle((0,0), GetScreenSize(), c, c, c, c)` with `c = ARGB(uint8(a×255), 0, 0, 0)`: a full-screen fade to black.
   - If `a > 0.7 && g_lives >= 0`, call `loadingMessage()` (§17).
   - Return true.
3. Otherwise return false.

### 15.2 `isDead(e, beam)` (`:369-404`)
This is used for NPCs and for the campaign princess. It is the same first-frame handling, except that the **beam goes at pos + (0,0,4)**. It awards EXP to both players if the entity is not pvp and not a main character. The alpha fades 1→0 over 3000 ms, and then `DeleteEntity`. There is no screen fade and no life loss.

### 15.3 Lives, checkpoints and levels
- **Lives.**
  - `g_lives` starts at 13, and the HUD shows it.
  - Every player-0 death costs one life, and so does every co-op summon.
  - **Game over happens when a death leaves `g_lives < 0`.** With no summons, that is the 14th death. The game-over scene is `gameOverPreLoop`, which plays `laugh_king.mp3` and `gameover.mp3`.
- **Checkpoints** (`main.as:170-192`). A `checkpoint` entity within 30 px of **player 0**, checked in its own bucket, does the following, in this order:
  1. `player.hasCheckpoint = 1` and `player.lives = g_lives`. The `lives` key is write-only and never read.
  2. `SaveScene("scenes/checkpoint.esc")`.
  3. Show the message "Checkpoint...".
  4. Delete the checkpoint and add `checkpoint_effect.ent`, which plays `checkpoint.mp3` at 0.5.
- **What a checkpoint respawn restores [2013 `ETHScene::SaveToFile` writes every entity in the buckets].** The reloaded player is the **saved entity**. Its HP and MP are whatever they were at save time, and every custom-data key is kept, including `hasCheckpoint`.
  - Live NPCs are restored as they were. Already-consumed spawn markers are gone.
  - The checkpoint entity itself was saved *before* it was deleted, so it exists again, and it re-triggers immediately, which re-saves the scene.
  - Globals are **not** saved. `g_exp`, `g_charLevel` and `g_lives` carry on as they are.
- **Next level** (`main.as:194-229`). Player 0 within 80 px holds Down, which fades out and then loads `scenes/<name>`. The new level spawns a fresh player from `bruxo.ent`, so HP and MP reset to 100/100. EXP and level carry over (globals).

---

## 16. Two players

### 16.1 PvP
- The menu (`menu.as:262-286`) requires `hasASecondController()`. Player 0 uses the keyboard plus a pad; player 1 uses a pad.
- Both players spawn "complete" with **500 HP**, 100 MP and `pvpMode = 1`.
- In PvP, main-character attacks may hit the other player and the other player's attacks (§12.4 steps 6.5-6.6). They never hit their own owner, or blows from the same owner.
- **Scoring:**
  - Killing the other player gives +1 to the killer.
  - Dying to an NPC or a hazard gives −1 to the victim. The victim's attack need not be involved: the rule is "attacker is not a main character".
  - Touching instant_death gives −1 (`controlCharacters.as:95-99`).
  - Killing an NPC gives EXP to the killer.
- **Any player death** (after 3 s) reloads the whole arena scene. Both players respawn at full HP, and the scores persist.
- **Win condition** (`setupScene.as:342-359`): a player reaches 3 points, **or** `|p1 − p0| >= 3`. That sets `g_gameFinished`, plays `pvp_win.ogg`, and makes `isMainCharDead` return true for both, which freezes them.
- The winner text is "Jogador " + (p0 > p1 ? 1 : 2) + " é o vencedor!". Arial Narrow 50 at (200, 335), preceded by two newlines.
- The HUD shows the skull icon and `g_pvpPoints` under each player's frame (§17).
- There are NPCs in every arena (pvp_lv1..6). The AI's `findMainCharNeighbours` targets whichever player is nearer.

### 16.2 Campaign co-op: `player1Summoner(player0)` (`controlCharacters.as:674-732`)
This runs from bruxo's callback when not in PvP.
1. If `!hasASecondController()`, return.
2. The summon triggers on `getConfirmButtonStatus(1) == KS_HIT`, which is JK_10 (Start) on player 1's pad.
   - If `player0.mp <= 50`: show "É necessário de 50 mana para invocar a criatura" and return.
   - If any **visible** entity (`GetVisibleEntities`) is named `princess.ent`: show "Não é possível invocar 2 criaturas ao mesmo tempo" and return.
   - **Space check.** Gather player 0's bucket and the (+1, 0) bucket. Take `playerBox` as player 0's absolute box moved by `(size.x + 1, −6, 0)`, which is +15 px right and 6 px up.
   - If any collidable entity's absolute box hits `playerBox`: show "Impossível invocar criatura daqui".
   - Otherwise:
     - `addToMp(player0, −50)`;
     - show "Criatura mágica invocada";
     - `AddEntity("summon.ent", playerBox.pos, 0)`, which plays `checkpoint.mp3` at 0.5;
     - `AddEntity("princess.ent", playerBox.pos, 0)`, placing her at player0 + (15, −2) with the same z;
     - `g_lives--`.
3. **Campaign princess rules.**
   - She uses `isDead`, so her death costs nothing more and she fades out and is removed.
   - She gets EXP through the shared `addToExp(1, …)`.
   - **Screen leash** (`:342-360`): this applies when not in pvp, when she has `lastTimeAlive`, and when `now − lastTimeAlive < 4000`.
     - If `!isInScreen(princess)` (camera rectangle ± 76 px): show "A criatura invocada não pode sair do campo de visão da tela". If `now − vanishTime > 3000`, set `hp = 0` and `startEarthquake(20)`.
     - Otherwise set `vanishTime = now`.
   - The camera averages the two players while `mainCharPos1 != (0,0)` (`cameraManager.as:118-121`).
4. The menu help text says **only player 1 (bruxo) can take checkpoints and change levels**; the code confirms this is player 0 (`playerId == 0`).

---

## 17. Text and HUD drawn from player callbacks

**`drawPlayerStatus(e)`** (`interface.as:43-96`) is drawn in screen space every frame the player is alive.
- `idOffset = (226 × playerId, 0)`, because `frame.png` is 226×74.
- The rail length is 200 and the row height is 16.

| Row | y | Rail | Fill | Text |
|---|---|---|---|---|
| HP | 0 | `rail.png` shaped to (200, 16) | `hp.png` shaped to (hp/maxHp·200, 16) | "hp: "+hp at (hpLen − 45, 0) |
| MP | 16 | same | `mp.png` | "mp: "+mp at (mpLen − 45, 16) |
| XP | 32 | same | `xp.png` width g_exp/lv<level>·200 | "lv: "+level at (xpLen − 27, 32) |

- All text is "Arial Narrow" 16 in color 0xD0000000. Every sprite is tinted 0xFFFFFFFF.
- **Campaign, player 0:** `skull_interface.png` at (448, 0). `g_lives` is drawn in "Arial Black" 17 at (469.5, 1.5) in 0xF0000000, then at (468, 0) in ARGB(200, 203, 203, 228).
- **PvP:** the skull goes at (idOffset.x + 16, 90). The points are drawn at skull + (30, 0), twice at the same position: first 0xF0000000, then the text color.
- Last come `frame.png` at idOffset with tint 0xA0FFFFFF, and `blend.png` (200×48) with tint ARGB(200, 203, 203, 228).

**Other text:**
- **`loadingMessage()`** (`util.as:457-460`): shadowText "Carregando...\n" in "Arial Narrow" 60 at (20, screenH − 70), ARGB(255, 203, 203, 228), with the shadow at +(6, 6) in ARGB(127, 0, 0, 0).
- **Messages** (`g_messages.addMessage(string)`, `messageManager.as:82-126`):
  - Up to 10 messages are kept, and duplicates are ignored.
  - Each fades out over 4000 ms. They are drawn in "Arial" 30 at (10, 70 + 30·slot).
  - The latest message is also drawn at half size near player 0: (charScreenPos + (−64, +32)) at half alpha.
  - Adding a message plays `soundfx/help.mp3` if it differs from `lastMessage`. Its volume is 0.3, set in `menuPreLoop`.

**Messages emitted by this dimension**, exact Latin-1 text:

| Text | Trigger | Line |
|---|---|---|
| É necessário ter 5 de mana para realizar este combo | sword combo, mp < 5 | playerInput.as:375 |
| É necessário ter 25 mana para este combo | blast, mp < 25 | :393 |
| É necessário ter 50 mana para esta magia | light, mp < 50 | :416 |
| Esta mágica já está em execução | light already active | :420 |
| É necessário ter 10 mana para esta magia | fire ball, mp < 10 | :436 |
| A criatura invocada não pode sair do campo de visão da tela | princess off-screen | controlCharacters.as:348 |
| É necessário de 50 mana para invocar a criatura | summon, mp ≤ 50 | :683 |
| Não é possível invocar 2 criaturas ao mesmo tempo | princess visible | :695 |
| Criatura mágica invocada | summon OK | :722 |
| Impossível invocar criatura daqui | summon blocked | :729 |
| Checkpoint... | checkpoint | main.as:185 |

---

## 18. Screen shake

`g_camera.startEarthquake(f)` **sets** the amplitude to |f|; it does not add to it. The camera then moves +f px (down), and 70 ms later moves back, halving f each full cycle until f < 1 (`cameraManager.as:82-109`).

| Source | Amplitude |
|---|---|
| Air jump | 2.5 |
| Sword combo | 7 |
| Hard landing | 10 |
| Main character hit | min(10, damage), every frame of overlap |
| Explosion (fireball, blast, lava shot) | 20 |
| Princess vanish-kill | 20 |
| Spawn with `showUpSfx` | 8 |

---

## 19. Sounds in this dimension

| File | When | Volume | Source |
|---|---|---|---|
| soundfx/jump01.ogg / jump02.ogg | Air jump by player 0 / player 1 | 1 | playerInput.as:334 |
| soundfx/blast_attack.ogg | Blast cast | **0.5** (setupScene.as:158) | :386 |
| soundfx/light_spell.mp3 | Light cast | 1 | :414 |
| soundfx/cast_fire_spell.ogg | Fire ball cast (also impy AI and lava shooter) | 1 | :430 |
| soundfx/fall.ogg | Landing with forceY > 700 | 1 | controlCharacters.as:159 |
| soundfx/help.mp3 | New HUD message | 0.3 | messageManager.as:91 |
| soundfx/fase.mp3 | Volume faded 1→0 during the player death fade | music | controlCharacters.as:454-455 |
| sword01.mp3 (particle) | sword0/sword1 spawn | 0.7 | sword0.ent / sword1.ent |
| sword_combo.ogg (particle) | combo_sword spawn | 0.2 | combo_sword.ent |
| hit01.ogg (particle) | enemy_blood.ent (player sword hit) / blood.ent (enemy hit) | 0.3 | |
| hit01.ogg (particle) | explosion.ent | 0.5 | |
| explosion.ogg (particle) | big_explosion.ent | 0.5 | |
| cast_fire_spell.ogg (particle) | hit_fail.ent (resisted hit) | 1 | |
| potion_pick.ogg (particle) | potion_pick.ent | 1 | |
| vanish.ogg (particle) | fade_out_beam.ent (death) | 0.5 | |
| creature_dying.mp3 (particle) | fade_out_beam_large.ent (master_knight death) | 1 | |
| checkpoint.mp3 (particle) | summon.ent, checkpoint_effect.ent | 0.5 | |
| sword01.mp3 / dark_hit.ogg / sword01.mp3 (particle) | enemy_sword / dark_sword / paladin_sword | 0.5 / 0.5 / 0.2 | |

There are **no random calls** in the player code. The only `randF` in these files is the king's summon x-jitter, `randF(6)` in [0, 6] px (`controlCharacters.as:636`).

---

## 20. Entities spawned by this dimension

| Entity | Position (z) | When | Line |
|---|---|---|---|
| jumpfx.ent | midpoint of two bumping characters, z = 0 | every frame they overlap | controlCharacters.as:118 |
| fall.ent | (boxX, boxBottom, 0) | hard landing | :160-161 |
| fade_out_beam.ent | player pos; NPC or campaign princess pos + (0, 0, 4) | death | :420, :377 |
| fade_out_beam_large.ent | master_knight pos + (0, 0, 4) | its death | :581 |
| summon.ent, princess.ent | player0 box centre + (15, −6) | co-op summon | :723-724 |
| summon.ent, warrior.ent | summon marker pos; warrior at + (randF(6), 0, −10) | king, every 5000 ms | :634-637 |
| sword0/1.ent, combo_sword.ent (and enemy swords) | owner + (±28, 0, +18) | attack | swords.as:81 |
| sword_beam.ent (and enemy_sword_beam.ent) | owner + (0, 0, +8) | attack | swords.as:82 |
| fire_ball.ent, combo_fire_ball.ent | owner + (0, 0, 0) | cast | spells.as:56 |
| light_spell.ent | owner + (0, −40, 14); then follows at (0, −40) | cast | spells.as:56, :106 |
| explosion.ent, big_explosion.ent | fireball position | fireball hits a non-resistant target | spells.as:141 |
| `hit` (enemy_blood.ent / blood.ent) | atk origin + (±atkBox.w/2, 0, 0), mirrored if knock.x > 0 | first hit per target | doDamage.as:225-229 |
| hit_fail.ent | same | resisted hit | doDamage.as:234 |
| potion_pick.ent | potion + (0, 0, 16) | pickup | potions.as:57 |

---

## 21. NPC callbacks in `controlCharacters.as`
The AI itself belongs to another dimension. Every NPC callback does the same steps:
1. `g_numNpcs++`.
2. Create or get its frame timer.
3. If `!isDead(e, beam)`: gravity, then its AI, then `move`, `doCharacterCollision(e, **true**)`, `animateCharacter`.

| Callback | AI call | Weapon | Death beam | Line |
|---|---|---|---|---|
| warrior | meleeCharacterAI | enemy_sword.ent | fade_out_beam | 471-493 |
| minion | meleeCharacterAI | enemy_sword.ent | fade_out_beam | 495-517 |
| knight | meleeCharacterAI | enemy_sword.ent | fade_out_beam | 519-541 |
| impy | rangedCharacterAI(e, "fire_ball.ent", 1) | fire ball | fade_out_beam | 543-565 |
| master_knight | meleeCharacterAI | dark_sword.ent | **fade_out_beam_large** | 567-589 |
| paladin | meleeCharacterAI | paladin_sword.ent | fade_out_beam | 591-613 |
| king | mixedCharacterAI(e, "paladin_sword.ent", "fire_ball.ent", 100, 1.5), then `summoner(e, "warrior", "summon", 5000)`. **While dead: `g_gameFinished = true`.** | both | fade_out_beam | 645-672 |

**`summoner`** (`:615-643`):
- Initialise `lastSummon` if missing.
- Every 5000 ms or more, look in the bucket beside the king on his facing side for an entity named `"summon"`.
- Add `summon.ent`, add the creature, call `spawn()` on it, then `break`.
- `lastSummon` is reset in any case.

---

## 22. Every Ethanon API used in these files, and how it is used

**Global functions:**

| API | Usage |
|---|---|
| `GetEntitiesFromBucket(vector2, ETHEntityArray&)` | Appends one bucket's entities. The heart of collision, damage, potions, the light-spell owner search and the summon checks. |
| `AddEntity(string, vector3, float angle)` → int | Fire-and-forget effects. The angle is always 0. |
| `AddEntity(string, vector3, ETHEntity@ &out)` | When custom data must be written onto the new entity. |
| `DeleteEntity(ETHEntity@)` | Fireballs, potions, dead NPCs. |
| `SeekEntity(int id)` | Finding the PvP attacker. |
| `GetVisibleEntities(ETHEntityArray&)` | Summon uniqueness check. |
| `GetTime()` | Every timer. |
| `UnitsPerSecond(float)` | Gravity, fireball motion, the damage-number rise. |
| `GetFPSRate()` | Inside `move()`. |
| `PlaySample`, `IsSamplePlaying`, `SetSampleVolume` | Sounds and the music fade. |
| `LoadScene(file, preLoopFn, loopFn)`, `GetSceneFileName()` | Respawn and game over. |
| `DrawRectangle(pos, size, c0, c1, c2, c3)`, `GetScreenSize()`, `ARGB(a, r, g, b)` | The death fade. |
| `DrawText(pos, text, font, size, argb)`, `DrawShapedSprite`, `DrawSprite`, `GetSpriteSize` | HUD and text. |
| `GetCameraPos()` | isInScreen, messages. |
| `normalize(vector2)`, `GetAngle(vector2)`, `radianToDegree`, `min`/`max`/`abs`/`sqrt` | Math. |
| `randF(float)` | King summon jitter. |
| `SaveScene(file)` | Checkpoint (main.as). |

**`ETHEntity` methods:**

| API | Usage |
|---|---|
| `GetPosition()` / `GetPositionXY()` / `SetPositionXY()` / `AddToPositionXY()` | Movement and positioning. |
| `GetCollisionBox()` | Offset and size; scripts add the position. |
| `GetCurrentBucket()` | Bucket queries. |
| `GetID()`, `GetEntityName()` | Identity; `GetEntityName` includes ".ent" for `AddEntity`-made entities, and is the scene name otherwise. |
| `Collidable()`, `SetCollision(false)` | Dead entities become unhittable. |
| `CheckCustomData(name)` vs `DT_NODATA` | Existence tests. |
| `Get/Add{Int,UInt,Float,String}Data` | Custom data. |
| `GetFrame()`, `SetFrame(uint)` | Animation. |
| `GetSize()` | Frame size, for the step cap. |
| `SetColor(vector3)` | Black tint on death. |
| `SetAlpha(float)` | Death fade. |
| `HasParticleSystem(0)`, `KillParticleSystem(0)` | Stop the aura. |
| `MirrorParticleSystemX(0, true)` | Left-facing swords and beams; hit effects. |

**`ETHInput` methods:**

| API | Usage |
|---|---|
| `KeyDown(KEY)` | Keyboard polling. |
| `GetKeyState(KEY)` → KS_* | Edge detection. |
| `GetJoystickStatus(uint)` → JS_DETECTED | Gates joystick reads. |
| `JoyButtonState(uint, J_KEY)` | Buttons and axis-derived directions. |
| `GetJoystickXY(uint)` | Analog movement. |
| `DetectJoysticks()` | Re-detect on J (setupScene/menu). |

**Script-side types:**
- `dictionary` `get`/`set`/`deleteAll` for `g_frameTimers`.
- `enmlFile.getInt/getUint/getFloat/get`.
- `collisionBox {vector3 pos, size}`.
- `ETHEntityArray` with `+=`, `size()`, `[]`.

---

## 23. Quirks to reproduce, or to decide on consciously

1. Attacks and casts zero a positive `forceY` (air-stall).
2. The sword combo spends mana even when the cooldown blocks the swing.
3. `isResistant` is sticky, and `damage` is mutated, across targets within one `doDamage` call.
4. Sword hitbox offsets are not mirrored: a right swing covers 8.5 px behind the player, and a left swing covers 0.5 px behind.
5. Snapping a player onto the ground leaves a 4 px overlap by design. `thisBox` is not refreshed after an UP or DOWN snap.
6. `touchingGround` keeps its previous value when the last solid hit's thin box overlapped. Side-wall contact processed last can make it flicker.
7. `jumps` resets only while grounded, so walking off a ledge allows two paid air jumps.
8. Instant-death boxes are also solid after killing. In PvP every overlapping instant-death box costs a point in the frame of death.
9. Bumping characters are knocked apart every frame, with a `jumpfx` each frame.
10. A full-HP player standing on a potion ends the potion's check for everyone that frame.
11. `doDamage.as:176` calls `isAMainCharacter(attacker)` before the null check. If the owner has been deleted, the AngelScript null-handle exception aborts the rest of that callback. Recommendation: treat a null attacker as "no score change" and stop processing that attack for the frame.
12. The XP bar divides by `lv<level>`, and lv20 is missing (division by zero at level 20 and above 30). The port should guard this.
13. Because the per-frame step cap and knockback decay are per frame, the original's behaviour changes with frame rate. Fix the tick at 60 Hz.
14. After `DeleteEntity(potion)` the code still reads `potion.GetPosition()` (`potions.as:56-57`). This relies on deferred deletion.

## Key facts

- Movement is horizontal walk plus jump with gravity. Walk speed 150 px/s (bruxo.ent/princess.ent 'speed'), GRAVITY 1200 px/s^2 (constants.as:50), jumpForce 507 px/s, so the apex is 107.1 px at 0.4225 s. maxJumps 2; an air jump costs 4 MP and plays jump01.ogg (player 0) or jump02.ogg (player 1) with earthquake 2.5 (playerInput.as:316-337).
- move() (util.as:188-219) divides force by GetFPSRate(). It caps the per-frame step at 0.9*0.4*min(frameW,frameH) = 11.52 px for 32x48 frames. Knockback is stored in px per FRAME, decays x0.8 per frame (total travel about 4x the impulse), and knockBack() keeps only the upward y component. Needs a fixed 60 Hz tick.
- Player sprite sheet: 128x192 cut 4x4 into 32x48 frames, startFrame 8. Only rows 1-2 are used: 4-7 walk left, 8-11 walk right, idle 4/8, airborne rising 4/8, falling 5/9. Frame stride 100 ms with a strict '>' comparison. Rows 0 and 3 are unused.
- Player collision box: offset (0,4), size 14x36. Collision (controlCharacters.as:43-220) is AABB against entities from the current bucket plus the direction-dependent neighbour buckets (256 px grid, membership by origin), resolved in list order with a 60%-width 'thinner box'. Characters that overlap are knocked apart by 7 px/frame with a jumpfx.ent and take no damage.
- Keyboard (player 0 only): arrows move, Up or Ctrl jumps, S sword, D fire ball, Space light, Enter/LMB/RMB confirm, Esc cancel. Joystick: JK_03 jump, JK_04 sword, JK_02 fire, JK_01 light, JK_10 confirm/summon, JK_09 cancel. Player 1 (princess) is joystick-only. g_controls swaps which pad index belongs to which player (playerInput.as:43-53).
- Combos (combo.as): a 5-slot buffer, one command per frame (priority L,R,U,D,Sword,Spell), reset after 210 ms of no input. The match must be the first three entries. L,L,Sword or R,R,Sword is the combo sword (5 MP, x5 damage, knock 25, earthquake 7). Down,L/R,Spell is the blast (25 MP, combo_fire_ball, damage 225, blast_attack.ogg at volume 0.5).
- Sword: coolDown 200 ms. sword0.ent/sword1.ent spawn at owner + (+/-28,0,+18) with box offset (-4,4) (NOT mirrored) and size 65x38. The combo sword box is 103x61. The sword stays in place. Its active window is about 291 ms, derived from particle timing [2013 estimate]. Each attack entity damages each target once, tracked by an 'id<N>' key.
- Player damage = (D + int(D*level/4)) * int(multiplier). At level 1: sword 25, combo sword 125, fire ball 37, blast 281. Against fire-resistant targets (minion, impy, paladin) fire damage is divided by 5, and resistance is sticky within one doDamage call (doDamage.as:87,154-164).
- The fire ball costs 10 MP with no cooldown and flies at owner speed x1.5 = 225 px/s. It explodes on anything collidable (walls, instant_death, enemy swords) except npc_wall. Its lifetime is set by particles (repeat 5), about 1.6-1.8 s or about 380 px of range [2013 estimate]. The light spell costs 50 MP, floats at owner + (0,-40), has a light of range 319, cannot be recast while alive, and lasts about 23 s (repeat 36).
- MP: max 100, regenerates +1 per >350 ms (doMpRecovery). HP: max 100 (500 in PvP, because doLoop multiplies hp/maxHp by 5 for 'complete' spawns), with no regeneration. Potions (potion_small hp=20, 54 placed in the campaign; potion_large 50 is unused) heal HP within 20 px, only when not at full HP.
- Experience: data.enml lv1=200, lv2=400, lv3=800, lv4=1600, lv5=3200, lv6=4000, lv7=5000, lv8=6000, lv9=7000, lv10=8000, lv11=9000, lv12-13=10000, lv14-19=11000; lv20 is MISSING; lv21-30=11000. In the campaign, every NPC death gives its data.enml hp as EXP to BOTH players. A level only raises damage by +25% of base per level.
- Death: hp<=0 tints the sprite black, disables collision, spawns fade_out_beam.ent (vanish.ogg at 0.5) and fades alpha plus a full-screen black rect over 3000 ms. Then g_lives-- and the scene reloads (or checkpoint.esc, or gameover.esc when g_lives<0). Lives start at 13, so game over comes on the 14th death. There are no i-frames, no fall damage and no kill plane outside instant_death boxes.
- PvP: 500 HP each. Killing the opponent gives +1, dying to an NPC or instant_death gives -1. A win needs 3 points or a 3-point lead. Any player death reloads the arena after 3 s. Campaign co-op: player 1 presses Start (JK_10); this costs player 0 50 MP (needs >=51) plus one life, spawns princess.ent at player0 + (15,-2), and she dies if she stays off-screen for more than 3 s.
- Combat SFX mostly come from particle <SoundEffect> tags played at the entity's soundVolume. This is a 2010-only engine feature, confirmed by HasSoundEffect/SetSoundVolume in machine.exe. Examples: sword01.mp3 at 0.7 for player swords, sword_combo.ogg at 0.2, hit01.ogg at 0.3/0.5, potion_pick.ogg at 1.

## Engine gaps

- OGG Vorbis decoding: src/core/AudioClip.cpp:86-89 reads only .wav and .mp3, and most Penumbra combat SFX are .ogg (jump01/02, fall, blast_attack, cast_fire_spell, hit01, sword_combo, explosion, potion_pick, vanish, dark_hit, pvp_win). Adding a decoder is an engine change that must be coordinated with the other session editing Supersonic. The game-side alternative is to convert to .wav/.mp3 in the asset converter.
- Text fonts: the engine draws in-world and HUD text only from BMFont text-format fonts (src/core/BitmapFont.hpp), or ImGui. The game needs 'Arial Narrow' (sizes 15-60), 'Arial Black' 17, 'Arial' 15/30 and 'Verdana' 15, all with Latin-1 Portuguese glyphs. These must be generated as BMFont assets (tooling or asset work, not engine code), or any engine-side TTF support must be coordinated with the other session.
- Ethanon sample semantics (to emulate game-side over the mixer): one voice per file name; PlaySample restarts an already-playing file; per-file persistent volume via SetSampleVolume; IsSamplePlaying queries.
- Particle-system <SoundEffect> plus the entity soundVolume attribute (2010 feature). To emulate game-side: play the sound on spawn of the temporary effect entity.
- Ethanon spatial buckets (to emulate game-side): a 256x256 grid with membership by floor(origin/256); GetEntitiesFromBucket append semantics; stable per-bucket list order. Needed for faithful collision, damage, potion and summon queries.
- Per-entity typed custom-data dictionary (int/uint/float/string) with DT_NODATA existence semantics and create-or-overwrite Add*Data. To emulate as a game-side EnTT component.
- Temporary-entity auto-delete: delete an entity with no sprite, whose particle systems all have repeat>0, once they finish. To emulate game-side. It sets the sword hit window, fireball range and light-spell duration.
- An Ethanon-compatible particle emitter (to emulate game-side or map onto src/core/ParticleSystem): repeat>1 (fireball 5, blast 7, light 36); allAtOnce vs a release stagger over lifeTime+randomLifeTime; MirrorParticleSystemX (negate startPoint, direction, randomizeDir, randStartPoint); KillParticleSystem; alphaMode 1 (additive) and 4 (black 'shadow' particles); 4x3 animated explosion sheets; per-frame (@60) units.
- A fixed 60 Hz gameplay tick: knockback decay x0.8 per frame and the step cap of 0.4*min(frame size)*0.9 are per frame, and move() divides by GetFPSRate(). The game should run its own fixed-step logic loop rather than use the variable dt.
- Legacy joystick model: button numbers JK_01..JK_10 (PlayStation-style USB pad), and JK_LEFT/RIGHT/UP/DOWN edges derived from the X/Y axis at a threshold of 0.8 with their own HIT/RELEASE edges. The engine's Input uses the GLFW standard gamepad mapping, so the game needs a remap table plus per-pad edge tracking for axis-derived directions and two pads with swappable assignment.
- Per-key 4-state machine (KS_HIT/KS_DOWN/KS_RELEASE/KS_UP) with KeyDown = HIT||DOWN, and first-held-key shadowing in the button helpers. Implement game-side on top of Input.
- 2D point lights with halo sprites (light spell range 319, sword and fireball lights, castShadows flags) and normal-mapped character sprites (bruxo_nm.png). Light2D exists (64 max), but halo rendering and shadow casting are unverified.
- Runtime scene snapshot (SaveScene('scenes/checkpoint.esc') plus reload) for checkpoints. Implement as a game-side serialisation of live entities and custom data.
- Immediate-mode screen drawing: DrawRectangle with four vertex colours (death fade), DrawShapedSprite (HUD bars), DrawSprite with ARGB tint, and shadowed DrawText. ScreenOverlay likely covers the sprites; per-vertex gradient rects and text need checking.

## Open questions

- Callback execution model in the 2010 engine: do ETHCallback_* functions run for all dynamic entities (as in 2013) or only for entities in visible or border buckets? Evidence for visible-only: the per-frame g_numNpcs counter; static potions with callbacks; the princess 'lastTimeAlive < 4000' guard (controlCharacters.as:342-344) only makes sense if callbacks can stop running.
- Does an entity created with AddEntity during a callback run its own callback (and particle update) in the same frame? This shifts sword and fireball hit timing by one frame.
- Is DeleteEntity deferred to the end of the frame? potions.as:56-57 reads the deleted potion's position, and two players on one potion would both heal only if it is deferred.
- Is LoadScene deferred? isMainCharDead (controlCharacters.as:430-451) does g_lives-- on every frame with deadTime >= 3000 until the scene switches. With deferral that is exactly one decrement per death.
- Particle <SoundEffect>: is it played once at entity creation or on every repeat cycle? Is its volume soundVolume x the per-sample volume? Does it share the single per-file voice used by PlaySample (e.g. hit_fail.ent's cast_fire_spell.ogg)?
- 2010 particle timing semantics: the release stagger over lifeTime+randomLifeTime and the per-particle repeat counting are taken from the 2013 source. The sword window (about 291 ms), fireball range (about 380 px), blast range (about 500 px) and light duration (about 23 s) are estimates that should be confirmed by running machine.exe.
- Joystick direction source in 2010: are JK_LEFT/RIGHT/UP/DOWN derived from the X/Y axis with a 0.8 threshold (2013 WinMM) or from the POV hat? Is the GetJoystickXY dead zone 0.01? The physical layout intended for JK_01..JK_10 is assumed to be a PlayStation-style pad (1=triangle, 2=circle, 3=cross, 4=square, 9=Select, 10=Start).
- Entity order inside a bucket, and re-bucketing order when entities move between buckets. This affects collision resolution order, which target doDamage returns, and touchingGround flicker.
- 2010 SaveScene: does it write dynamic entities with their current custom data (player HP/MP at checkpoint time), write temporary effect entities, and preserve entity IDs (so ownerID links on a saved light spell stay valid)? My statements rely on the 2013 ETHScene::SaveToFile.
- GetSize() in 2010: assumed to be the sprite frame size (32x48), as in 2013 ETHSpriteEntity::GetCurrentSize. If it were the texture size (128x192) the step and knockback caps would effectively vanish.
- Level 20 and levels above 30 have no lvN key in data.enml. The HUD computes g_exp/0 there. What should the port show?
- help.mp3 volume: menuPreLoop sets it to 0.3. Do the later LoadSoundEffect calls (setupScene.as:147, messageManager.as:86) reset it to 1?
- Semantics of blendMode=2 on the character entities and alphaMode=4 on the black aura/beam particles in the 2010 renderer (modulate or 'shadow' blending?).
- Does 2010 GetFPSRate() use the same 500 ms sampled counter as 2013? It only matters if the port does not fix the tick at 60 Hz.
- doDamage.as:176 calls isAMainCharacter(attacker) before its null check. Confirm that the port should treat a missing attacker as 'no score change'; in the original, an AngelScript exception aborted the callback.
