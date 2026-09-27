# Penumbra port spec: enemies, hazards, world interaction, utilities

Scope: characterAI.as, lavaShooter.as, switch.as, util.as, eth_util.as, constants.as, the per-enemy tables in data.enml, and every `ETHCallback_*` in the project. I also cover the parts of controlCharacters.as, doDamage.as, swords.as, spells.as, events.as, main.as, potions.as and setupScene.as that enemies and hazards run through. All paths are relative to `extracted/app/`. Everything was read-only.

The same report is saved at `reference/analysis/report_enemies_hazards.txt`. The scratch parsers are next to it: `entdump.py`, `scenedump.py`, `scene2.py` and `api_sigs.txt`.

**Provenance tags** (every semantic claim carries one):
- **[S file:line]**: read in the Penumbra AngelScript.
- **[D]**: read in `.ent`/`.esc`/`.enml` data.
- **[B]**: read in the 2010 `machine.exe` string table (its registered AngelScript signatures).
- **[E13 file:line]**: read in the Dec-2013 Ethanon source at `Magic-Portals-Remake/reference/ethanon/toolkit/Source/src/`. It is assumed to match the 2010 engine, but that is not verified.
- **[I]**: my inference.

**Encoding:** the text files are **Windows-1252, not Latin-1**. `switch.as:76` and `menu.as:124,133` contain byte 0x95, which is "•" in cp1252 and an invisible C1 control character in Latin-1. Decode everything as cp1252.

---

## 1. Engine semantics the port must emulate

### 1.1 Time and frame units
- `GetTime()` returns uint milliseconds since start **[B]**. It keeps running across `LoadScene`; the script globals survive scene loads too **[I]**.
- `UnitsPerSecond(x) = x * lastFrameElapsedMs / 1000`, uncapped **[E13 engine/Script/ETHScriptWrapper.System.cpp:95-98]**. The frame delta is an integer millisecond count, so at 60 fps it alternates between 16 and 17 ms.
- `GetFPSRate()` is the video's measured FPS **[E13 ETHScriptWrapper.System.cpp:130-133]**. The scripts use `fps = (GetFPSRate()==0) ? 60 : GetFPSRate()` **[S util.as:195, lavaShooter.as:70, events.as:99]**.
- Movement mixes two timing styles:
  - Gravity accumulates as px/s via UnitsPerSecond.
  - Displacement divides by fps.
  - **Knockback is applied and decays per frame** (see 7.2).

  **Recommendation:** run the port at a fixed 60 Hz tick with UnitsPerSecond(x) = x/60 and fps = 60. The behaviour is frame-rate dependent otherwise.
- `GetLastFrameElapsedTime` does not exist in the 2010 API **[B]**, and none of my files use it.

### 1.2 How callbacks bind
- The engine calls `ETHCallback_<name>(ETHEntity@)` every frame, where `<name>` is the entity's **EntityName with its extension removed** **[E13 engine/Util/ETHASUtil.cpp:63-79; B has the string "ETHCallback_"]**.
- EntityName is the **scene label**. It need not be a file name: `help`, `story`, `event01`, `play`, `next_level`, `thumbnail` and `picker` have no `.ent` of that name, yet they bind **[D]**.
- An entity added with `AddEntity("x.ent")` gets the label `x.ent` and binds `ETHCallback_x`.
- There are no constructor or destructor callbacks: 2010 has only the `ETHCallback_` prefix **[B]**.

### 1.3 Which callbacks run each frame (OPEN; see openQuestions #1)
The 2013 model **[E13 Scene/ETHActiveEntityHandler.cpp:70-74,112-163]** is:
- A **dynamic** entity (`static="0"`) with a callback, or a temporary entity, is "always active": its callback runs every frame, on screen or not.
- A **static** entity with a callback runs only while it is in a visible bucket. Visible buckets are those intersecting the screen rectangle, plus one ring of border buckets only if `SetBorderBucketsDrawing(true)` **[E13 Scene/ETHScene.cpp:523]**.
- Penumbra calls `SetBorderBucketsDrawing(false)` in setupScene and `(true)` every frame in pvp **[S setupScene.as:167,251]**.

Evidence that 2010 gated static callbacks by visibility **[I]**: level1's `play` entity (starts `fase.mp3`) sits at (3981,-598), about 3600 px from the player spawn at (410,-42). That placement only makes sense if its callback waits until it is seen.

**Port default until tested:** use the 2013 model.

Static entities with callbacks in my scope:
- `shooter.ent` (all instances).
- `help`, `story`, `checkpoint.ent`, `next_level`, `play`.
- `potion_small.ent`.
- `event01` (static="1" in level3).

Dynamic ones: all enemies, `fire_shoot.ent`, `falling_bridge.ent`, weapons and spells **[D]**.

**Execution order within a frame [E13 ETHScene.cpp:459-500]:** always-active callbacks run, then the scene loop function (`levelLoop`/`pvpLoop` → `doLoop`), then static visible callbacks. Order among entities is bucket/list order and cannot be recovered. The port should use scene-file order.

### 1.4 Buckets (critical for AI perception and collision candidates)
- `bucket = floor(pos.xy / bucketSize)` **[E13 Scene/ETHBucketManager.cpp:47-50]**.
- `GetEntitiesFromBucket(b, arr)` **appends** the entities whose *position* (centre) lies in bucket b **[B signature; E13]**. The entity's size is ignored: a 256x256 wall lives only in its centre's bucket.
- `GetCurrentBucket()` is the bucket of the entity's current position.
- **Bucket size:** levels are loaded with `LoadScene(file, "setupScene", "levelLoop")`, which takes no size, so they use the default. `_ETH_DEFAULT_BUCKET_SIZE = 256` **[E13 ETHTypes.h:58, per Magic-Portals docs/ethanon-formats.md:770-771]**. The menu passes (1024,256) explicitly **[S main.as:126, menu.as:387,398]**.
- The port must assume **256x256** (OPEN #2). The level3 boss layout fits it: the king at x≈10920 is in bucket 42; the summon markers at 10632 and 11223 are in buckets 41 and 43.
- **Consequence:** enemies "see" the player only if the player's centre is in the **same bucket row** and within ±2 bucket columns (see 3.1).

### 1.5 Custom data bag
- `AddFloatData/AddIntData/AddUIntData/AddStringData(name, v)` **create or overwrite, including changing the type** **[E13 Entity/ETHCustomDataManager.cpp:470-495]**.
- The getters `GetFloatData/GetIntData/GetUIntData/GetStringData` are **type-strict**. A missing key or a wrong type returns 0 / 0 / 0 / "" **[E13 ETHCustomDataManager.cpp:80-130; Entity/ETHEntity.cpp:851-862]**.
- `CheckCustomData(name)` returns `DT_NODATA` (0) when absent. Scripts only ever compare it with DT_NODATA, so **presence matters, not value**. For example, `complete='0'` still counts as complete **[S setupScene.as:276-277]**.
- `.ent` custom data is copied into each instance. In a scene, the instance's own `<CustomData>` merges over it per key **[E13, docs/ethanon-formats.md:1191-1234]**.

### 1.6 Entity lifecycle
- `AddEntity(file, vector3 pos, float angle)` and `AddEntity(file, vector3 pos, ETHEntity@ &out)` both return an int ID **[B]**. The file is resolved in `entities/`.
- `DeleteEntity(e)` marks the entity dead. `IsAlive()` then returns false, and its callback never runs again. The handle stays readable for the rest of the current callback: `lavaShooter.as:125-126`, `main.as:186-187` and `potions.as:56-57` all read `thisEntity` after deleting it **[S]**.
- **Temporary entities:** an entity with no sprite, where every particle system has `particles > 0` and `repeat > 0`, is removed automatically once all its particles have finished **[E13 Entity/ETHEntity.cpp:697-715; ETHActiveEntityHandler.cpp:169-189]**. That covers every weapon, projectile and effect: swords, fire_ball, beams, explosions, blood, summon.ent and so on. **A fire_ball's lifetime is its range** (see 1.7).
- `SetCollision(false)` makes `Collidable()` return false. Every script loop skips non-collidable entities.
- `SetColor(vec3)` tints the sprite. `SetAlpha(f)` sets its alpha.
- `KillParticleSystem(n)` stops emission; live particles finish **[E13 Entity/ETHSpriteEntity.cpp:812-819]**. `HasParticleSystem(n)`.
- `MirrorParticleSystemX(n, true)` negates the system's startPoint.x, direction.x, randomizeDir.x and randStartPoint.x **[E13 docs 7.3]**. It does **not** mirror the collision box.

### 1.7 Particle timing and temporary-entity lifetime [E13 Particles/ETHParticleManager.cpp:188-267]
- Particle `i` (0..N-1) is released when its elapsed time exceeds `(lifeTime + randomLifeTime) * i/N`, or immediately if `allAtOnce`.
- Each life lasts `lifeTime + U(-randomLifeTime/2, +randomLifeTime/2)` ms.
- After `repeat` lives the particle stops.
- The entity ends at `max_i( release_i + sum of repeat lives )`.

For N particles, lifetime L, random R and repeat k:

`T_end ≈ (L+R)*(N-1)/N + k*(L ± R/2)`

Computed values **[D + formula]**:

| entity | N | L | R | repeat | allAtOnce | lifetime ≈ |
|---|---|---|---|---|---|---|
| enemy_sword / dark_sword / paladin_sword | 16 | 130 | 0 | 1 | no | 121.9 + 130 = **~252 ms** |
| sword0 / sword1 / combo_sword (player) | 16 | 150 | 0 | 1 | no | 140.6 + 150 = ~291 ms |
| enemy_sword_beam | 9 | 200 | 100 | 1 | no | 266.7 + [150..250] ≈ 0.42-0.52 s |
| fire_ball | 19 | 250 | 150 | 5 | no | 378.9 + 5×[175..325] → typically **~1.6-1.9 s** |
| combo_fire_ball | 23 | 250 | 150 | 7 | no | 382.6 + 7×[175..325] ≈ 2.1-2.6 s |
| light_spell | 15 | 600 | 500 | 36 | no | 1026.7 + 36×[350..850] ≈ 22-23 s |
| explosion / enemy_blood / blood / potion_pick | 12 | 450 | 400 | 1 | yes | ≤650 ms |
| big_explosion | 12 | 600 | 500 | 1 | yes | ≤850 ms |
| hit_fail | 4 | 250 | 200 | 1 | yes | ≤350 ms |
| jumpfx / fall | 9 | 200 | 100 | 1 | yes | ≤250 ms |
| lava_drops | 9 | 950 | 550 | 1 | yes | ≤1225 ms |
| bridge_fall | 9 | 700 | 450 | 1 | yes | ≤925 ms |
| fade_out_beam / _large | 45 | 1750 | 1300 | 1 | no | 2982 + [1100..2400] ≈ 4.1-5.4 s |
| summon.ent / checkpoint_effect | 5 | 1050 | 550 | 1 | no | 1280 + [775..1325] ≈ 2.1-2.6 s |

Fireball range follows from the speed:
- Impy (speed 120): 180 px/s → ~290-340 px.
- King: 100 × 1.5 (speedMultiplier) × 1.5 (manageFireBall) = 225 px/s → ~360-430 px.
- Player (150): 225 px/s.

`fire_shoot.ent`, the enemy auras, `torch.ent` and `checkpoint.ent` have repeat=0 (infinite), so they are not temporary.

### 1.8 Particle `<SoundEffect>` (2010-only feature)
The 2013 engine has no reader for it **[docs/ethanon-formats.md:1476-1477]**, but the 2010 API has `HasSoundEffect()`, `SetSoundVolume(float)` and `SilenceParticleSystems(bool)` **[B]**.

The scripts depend on it:
- setupScene preloads `sword01.mp3`, `hit01.ogg`, `vanish.ogg`, `potion_pick.ogg`, `explosion.ogg`, `checkpoint.mp3`, `creature_dying.mp3` and `sword_combo.ogg` **[S setupScene.as:142-153]**.
- No script ever calls PlaySample on those files.
- They are exactly the `<SoundEffect>` files in the `.ent`s.

**[I]** The sample (resolved in `soundfx/`) plays **once when the entity is created**, at the entity's `soundVolume` attribute. The full list is in the sound table (section 12.2). Whether it shares a voice with a script `PlaySample` of the same file is OPEN #3.

### 1.9 Geometry helpers
- `collisionBox` is `{vector3 pos (offset from the entity), vector3 size}` **[B]**. Scripts add `GetPosition()` to get world space **[S util.as:127-132]**. Only x and y matter (`checkBoxHit` ignores z).
- `GetSize()` is the sprite frame size, i.e. the bitmap divided by SpriteCut. All 32x48 characters and the 80x96 master_knight are listed in 3.4.
- **z does not move anything on screen in gameplay scenes.** Every level and pvp `.esc` has `<ZAxisDirection x="0" y="0">`; only menu.esc and arena_select.esc use (0,-1) **[D]**. z still affects draw depth and lighting.
- `GetAngle(vector2 v)` = `atan2(v.x, v.y)` wrapped to [0, 2π) **[E13 engine/Script/ETHScriptObjRegister.cpp:103-107]**. So (1,0) is 90°, (0,-1) is 180°, (-1,0) is 270° and (0,0) is 0°.
- `normalize(0,0)` returns (0,0) **[E13 gs2d/src/Math/GameMath.h:755-761]**.
- `rand(n)` returns an int uniform on **[0, n] inclusive**. `randF(f)` returns a float uniform on [0, f] **[E13 gs2d/src/Math/Randomizer.cpp:30-45, MT19937]**. In 2013 the generator is reseeded every frame with the elapsed time **[E13 ETHScene.cpp:499]**; the 2010 behaviour is unknown.

### 1.10 Audio sample model [B signatures; semantics I]
- Samples are keyed by **file path string**: `LoadSoundEffect`/`LoadMusic`, then `PlaySample`, `LoopSample(path, bool)`, `StopSample`, `IsSamplePlaying`, `SetSampleVolume(path, v)` and `SampleExists`.
- There is one voice per path. `IsSamplePlaying` gates retriggers (e.g. `minion.ogg`).
- The volume is a per-sample property that persists (e.g. `help.mp3` is set to 0.3 in menu.as:55).
- The backend is Audiere (`audiere.dll`). Files are `.ogg` and `.mp3`.

### 1.11 Text
`DrawText(vector2 screenPos, string, string fontName, float size, uint ARGB)` **[B]** draws with **system TrueType fonts** by name at arbitrary pixel size. Names used: "Arial Narrow", "Arial", "Arial Black", "Verdana". `\n` breaks lines.

---

## 2. Every entity callback in the project

"Bound label" is the EntityName that binds; counts are per scene **[D]**. static/dyn is taken from the scene instance or `.ent` **[D]**.

| Callback | Defined at | Bound label (count) | Template .ent | static? | What it does |
|---|---|---|---|---|---|
| ETHCallback_warrior | controlCharacters.as:471 | `warrior.ent` (runtime spawn; spawn names L1 30, L2 17, L3 11, pvp1 3, pvp2 4, pvp5 6, plus king summons) | warrior.ent | dyn | melee AI with enemy_sword.ent (section 4) |
| ETHCallback_minion | :495 | `minion.ent` (L2 36, L3 31, pvp4 2) | minion.ent | dyn | melee AI with enemy_sword.ent |
| ETHCallback_knight | :519 | `knight.ent` (L1 15, L2 11, L3 19, pvp2 2, pvp3 4, pvp5 5, pvp6 4) | knight.ent | dyn | melee AI with enemy_sword.ent |
| ETHCallback_impy | :543 | `impy.ent` (L2 11, L3 4, pvp4 2, pvp5 2, pvp6 4) | impy.ent | dyn | ranged AI with fire_ball.ent ×1 (section 5) |
| ETHCallback_master_knight | :567 | `master_knight.ent` (L2 1, L3 7, pvp1 1, pvp3 4, pvp6 1) | master_knight.ent | dyn | melee AI with dark_sword.ent; death beam fade_out_beam_large.ent |
| ETHCallback_paladin | :591 | `paladin.ent` (L1 2, L2 10, L3 3, pvp4 2, pvp5 2) | paladin.ent | dyn | melee AI with paladin_sword.ent |
| ETHCallback_king | :645 | `king.ent` (added by event01, L3) | king.ent | dyn | mixed AI (paladin_sword / fire_ball ×1.5) plus summoner; sets g_gameFinished when dead (section 6) |
| ETHCallback_bruxo | :270 | `bruxo.ent` (spawn name, 1 per scene) | bruxo.ent | dyn | player 1. Other dimension. Calls player1Summoner (:674) |
| ETHCallback_princess | :304 | `princess.ent` | princess.ent | dyn | player 2 / summoned creature. Other dimension |
| ETHCallback_enemy_sword | swords.as:109 | `enemy_sword.ent` | same | dyn, temp | `doDamage(this, 4, true, false, PHYSICAL, 1)` |
| ETHCallback_dark_sword | swords.as:114 | `dark_sword.ent` | same | dyn, temp | `doDamage(this, 12, true, false, PHYSICAL, 1)` |
| ETHCallback_paladin_sword | swords.as:133 | `paladin_sword.ent` | same | dyn, temp | `doDamage(this, 15, true, false, PHYSICAL, 1)` |
| ETHCallback_sword0 / sword1 | swords.as:120/124 | player swords | same | dyn, temp | `doDamage(this, 4, true, false, PHYSICAL, 1)` |
| ETHCallback_combo_sword | swords.as:128 | player combo | same | dyn, temp | `doDamage(this, 25, true, false, PHYSICAL, 5)` |
| ETHCallback_fire_ball | spells.as:113 | `fire_ball.ent` (impy, king, player) | same | dyn, temp | `manageFireBall(this, "explosion.ent")` |
| ETHCallback_combo_fire_ball | spells.as:118 | player combo | same | dyn, temp | `manageFireBall(this, "big_explosion.ent")` |
| ETHCallback_light_spell | spells.as:83 | player spell | same | dyn, temp | follows its owner. Other dimension |
| ETHCallback_shooter | lavaShooter.as:43 | `shooter.ent` (L1 9, L3 5, pvp2 4) | same | **static** | periodic lava ball launcher (section 8.2) |
| ETHCallback_fire_shoot | lavaShooter.as:64 | `fire_shoot.ent` (runtime) | same | dyn | lava ball ballistic flight and hit (section 8.2) |
| ETHCallback_falling_bridge | events.as:67 | `falling_bridge.ent` (L3 16, pvp1 8, pvp2 2, pvp3 13, pvp4 4) | same | dyn | crumbling stone (section 8.3) |
| ETHCallback_event01 | events.as:43 | `event01` (L3 ×1 at (10920,1408)) | event.ent (128x256 box) | **static**, collidable | boss trigger (section 6.3) |
| ETHCallback_help | main.as:147 | `help` (L1 11, L3 5) | help.ent | static | tutorial message (section 9.3) |
| ETHCallback_story | main.as:163 | `story` (L1 5, L2 4, L3 5) | none | static | world-space lore text (section 9.4) |
| ETHCallback_checkpoint | main.as:170 | `checkpoint.ent` (L1 3, L2 4, L3 3) | same | static | save point (section 9.5) |
| ETHCallback_next_level | main.as:194 | `next_level` (L1 → level2.esc, L2 → level3.esc) | next_level.ent | static | level exit (section 9.6) |
| ETHCallback_potion_small | potions.as:68 | `potion_small.ent` (L1 13, L2 15, L3 26; custom hp=20) | same | static | heal pickup (section 9.7) |
| ETHCallback_potion_large | potions.as:73 | `potion_large.ent` (**none placed**; hp=50) | same | static | heal pickup |
| ETHCallback_potion | potions.as:63 | **nothing is bound** (no label "potion") | none | none | dead code |
| ETHCallback_play | setupScene.as:43 | `play` (L1 (3981,-598), L2 (205,492), L3 (444,190); all name='fase.mp3') | none | static | LoadMusic + PlaySample + LoopSample("soundfx/"+name), then DeleteEntity(self) |
| ETHCallback_clouds | environment.as:43 | `clouds.ent` (created from `environment` markers in L2, L3, pvp3, pvp4; placed directly in gameover.esc) | clouds.ent | dyn | follows the camera; lightning every 6000+rand(6000) ms (first at 10000 ms) with thunder.mp3. Environment dimension |
| ETHCallback_fog | environment.as:115 | `fog.ent` (pvp6 environment) | fog.ent | dyn | follows the camera; sets bg colour/image. Environment dimension |
| ETHCallback_dawn | environment.as:145 | `dawn.ent` (pvp1 environment) | dawn.ent | dyn | bg colour/image. Environment dimension |
| ETHCallback_cursor | menu.as:232 | `cursor.ent` (menu, arena_select) | same | dyn | menu cursor. Menu dimension |
| ETHCallback_thumbnail | menu.as:362 | `thumbnail` (arena_select ×6) | thumbnail.ent | dyn | darkens a locked arena. Menu dimension |
| ETHCallback_picker | videoModes.as:58 | `picker` (videoModes.esc ×1) | none | dyn | cursor in the video options screen |

**Labels in my scope that have NO callback and are handled elsewhere [D + S]:**
- `instant_death.ent` / `instant_death2.ent` / `instant_death3.ent`: kill volumes, handled in doCharacterCollision.
- `npc_wall.ent`: NPC-only walls.
- `invisible_wall.ent` / `invisible_wall_small.ent`: plain solids.
- `lava.ent` / `thorns.ent`: visuals only, non-collidable.
- `torch.ent` / `torch_higher_range.ent`: decoration and light.
- `spawn`: processed by doLoop.
- `summon`: king summon markers (L3 ×4, pvp6 ×1).
- `summon.ent`: effect.
- `play_sound`: processed by doLoop.
- `environment`: processed by setupScene.
- `bridge_fall.ent`, `lava_drops.ent`, `explosion.ent` and similar: effects.
- **`spawn2`** (L3 (9814,2180), a bruxo spawn): not collected, because only the label `spawn` is. It never spawns.
- **`dontFall`, `fontFall`** (L3) and **`step`** (pvp1 ×2): renamed falling_bridge stones, collidable 64x32 single_stone.png. They have no callback and **never fall**.
- **`play_sound.ent`** (L2 ×3, L3 ×2): **never play**, because `GetEntityArray("play_sound")` only matches the label `play_sound` (L2 ×2) **[S setupScene.as:175]**.
- `deco`, `decoration`, `pilar02`: decorations.
- `vert_bruxo.ent`, `vert_king.ent`, `vert_master_knight.ent`: unreferenced by any script, scene or ent (grep).
- `silent_sword.ent`: only in a commented-out line (setupScene.as:169).

**Unused particle presets [D]:** the `effects/*.par` files are editor presets; `.ent` files embed their systems inline. These are referenced by nothing: `king_beam`, `portal_red`, `shadow_beam`, `shadow_beam_wide`, `life_shadow_beam(_bigger)`, `fade_out_shadow_beam(_hard)`, `enemy_dark_sword`, `enemy_sword_hit`, `explosion_particles`, `fireball`, `heavy_sword`, `light_spell_infinite`, `nextlv`, `small_explosion`, `sword02`, `sword03.ent`, `sword04`, `sword_hit`, `torch_fire`, `bridgefall`. **There is no "king beam" attack in the game.**

---

## 3. Enemy definitions

### 3.1 Perception: `findMainCharNeighbours(this, horizontalOnly=true)` [S util.as:43-94]
1. `findAmongNeighbourEntities(this, name, true)` collects the buckets in the order current, (+1,0), (-1,0), (+2,0), (-2,0). With `horizontalOnly=false` it would also add (±1,±1); no caller does that. It returns the **first** entity whose `GetEntityName() == name`.
2. The function looks for `"bruxo.ent"` (MAIN_CHARACTER_ENTITY0).
3. If `!hasASecondController()`, it returns that result.
4. Otherwise it also looks for `"princess.ent"`:
   - If both are found, it returns the one with the smaller squared distance. On a tie it returns princess, because the test is `dist0<dist1`.
   - If only bruxo is found, it returns bruxo; otherwise it returns princess or null.
5. `hasASecondController()` = `(g_controls==1 && joystick 1 detected) || (g_controls==0 && joystick 0 detected)` **[S playerInput.as:159-164]**.

**Effective sensing area** (bucket 256): the player's centre must be in the enemy's own bucket row (a 256-px band, not centred on the enemy) and within 5 bucket columns (1280 px). Every AI then treats a target with `hp <= 0` as null.

### 3.2 data.enml tables [D data.enml:123-227], read by `spawn()` [S setupScene.as:52-112]

| key (type in entity) | warrior | minion | knight | master_knight | impy | paladin | king |
|---|---|---|---|---|---|---|---|
| hp (int) | 75 | 45 | 150 | 1700 | 75 | 400 | 3500 |
| speed (float, px/s) | 50 | 150 | 80 | 100 | 120 | 80 | 100 |
| stride (uint, ms per anim frame) | 150 | 150 | 110 | 150 | 100 | 120 | 120 |
| coolDown (uint ms) | 800 | 800 | 300 | 420 | 1500 | 550 | 1000 |
| viewRadius (float px) | 180 | 500 | 180 | 230 | 600 (unused) | 280 | 280 |
| attackRadius (float px) | 40 | 40 | 43 | 80 | 100 (= flee radius) | 47 | 35 |
| damage (int) | 5 | 20 | 10 | 25 | 18 | 20 | 10 |
| waitBeforeAttack (uint) | 0 | 0 | 0 | **1** | 0 | 0 | 0 |
| jumpBackAfterAttack (uint) | 0 | 0 | 1 | 0 | 0 | 1 | 1 |
| fireResistant (uint) | 0 | **1** | 0 | 0 | **1** | **1** | 0 |
| pushBackBias (float) | 1 | 1 | 0.1 | 0.05 | 0 | 0.05 | 0.15 |
| chaseSfx (string) | none | minion.ogg | none | none | none | none | none |
| expGiven (int) = hp | 75 | 45 | 150 | 1700 | 75 | 400 | 3500 |

`global.lives = 13` and `lv1..lv30` are the XP thresholds. **lv20 is missing** (lines 22-23 jump from lv19 to lv21).

### 3.3 `spawn(handle, name)` [S setupScene.as:52-112]
It writes, in this order:
1. `currentDir`=LEFT(1) (uint)
2. `forceX`=0 and `forceY`=0 (float)
3. `knockBackX`=0 and `knockBackY`=0 (float)
4. `touchingGround`=0 (uint)
5. `action`=STANDING(0) (uint)
6. `lastSwordAttack`=0 (uint)
7. `lastTimeDidntSee`=GetTime() (uint)
8. `fireResistant`=0 (uint)
9. then from data.enml: `hp`(int), `damage`(int), `speed`, `viewRadius`, `attackRadius`, `pushBackBias` (float), `stride`, `coolDown`, `waitBeforeAttack`, `jumpBackAfterAttack`, `fireResistant` (uint), `expGiven`(int)=hp
10. if `get(name,"chaseSfx") != ""`: `chaseSfx` = "soundfx/"+value (string) and `LoadSoundEffect` on it.

The temporaries are reused, so a missing key would silently inherit the previous key's value **[I]**. No enemy is missing a key.

### 3.4 Per-enemy entity data [D entities/*.ent]

| enemy | sprite (frame) | normal/gloss | startFrame | collision offset; size | own PS0 (aura) | light | showUpSfx | weapon | death beam |
|---|---|---|---|---|---|---|---|---|---|
| warrior | warrior.png 128x192 → 32x48 | warrior_nm.png | 6 | (0,0); 20x43 | none | off | none | enemy_sword.ent | fade_out_beam.ent |
| minion | LOS-Nac-Normal.png → 32x48 | none | 4 | (0,0); 32x48 | black smoke: black_opaque.png, 45p, repeat 0, alphaMode 4 | off | none | enemy_sword.ent | fade_out_beam.ent |
| knight | knight.png → 32x48 | knight_nm.png | 4 | (0,0); 22x41 | none | off | none | enemy_sword.ent | fade_out_beam.ent |
| impy | impy.png → 32x48 | impy_nm.png | 4 | (0,0); 22x43 | none | off | none | fire_ball.ent | fade_out_beam.ent |
| master_knight | master_knight.png 320x384 → **80x96** | none | 4 | **(0,4)**; 57x84 | big black smoke (45p) | off | creature_show_up.mp3 | dark_sword.ent | **fade_out_beam_large.ent** |
| paladin | paladin.png → 32x48 | paladino_nm.png + paladin_gloss.png, specularPower 61 | 4 | (0,0); 22x41 | none | inactive (range 15) | paladin_appear.mp3 | paladin_sword.ent | fade_out_beam.ent |
| king | king.png → 32x48, blendMode 2 | king_nm.png | 4 | (0,0); 23x48 | fire aura: explosion.JPG, 19p, repeat 0 | **active**, range 122.5, colour (1,0.4,0.2) | none | paladin_sword.ent (melee) / fire_ball.ent (ranged ×1.5) | fade_out_beam.ent |

All enemies are `type=0`, `static=0`, `collidable=1`, SpriteCut 4x4 (16 frames).

**Sheet layout used by the script:**
- frames 4-7: walk left
- frames 8-11: walk right
- 4: idle/rising, facing left
- 5: falling, facing left
- 8: idle/rising, facing right
- 9: falling, facing right
- frames 0-3 and 12-15: unused

Weapon entity data **[D]**:
- **enemy_sword.ent:** collision offset (-1,0), 46x19; custom `hit`="blood.ent", `dontCollide` (int); PS 16p, L130, repeat 1, sword.png; SoundEffect sword01.mp3 at soundVolume 0.5.
- **dark_sword.ent:** offset (-1,12), 89x41; hit=blood.ent; black_sword.dds; SoundEffect dark_hit.ogg at 0.5.
- **paladin_sword.ent:** offset (-1,0), 64x19; hit=blood.ent; red sword.png; active red light (range 149.5, colour (2,0,0)); SoundEffect sword01.mp3 at 0.2.
- **enemy_sword_beam.ent:** non-collidable; fog.dds wisp.
- **fire_ball.ent:** collidable 22x22; custom `dontCollide` (uint); PS explosion.JPG, 19p, repeat 5; active light (range 125.5, colour (1,0.7,0.3)); no SoundEffect.

---

## 4. Enemy per-frame pipeline

### 4.1 The common callback body (warrior/minion/knight/impy/master_knight/paladin/king) [S controlCharacters.as:471-672]
```
g_numNpcs++                                   // debug counter; doLoop zeroes it each frame (setupScene.as:331)
timer = g_frameTimers["id"+GetID()] or create: frameTimer.Set(GetFrame(),GetFrame(),0) and store
if (!isDead(this, BEAM)) {                    // BEAM = fade_out_beam.ent (master_knight: fade_out_beam_large.ent)
    applyForce(this, (0, UnitsPerSecond(1200)))          // forceY += 1200*dt
    <AI>(this, ...)                                      // melee / ranged / mixed
    move(this)
    doCharacterCollision(this, useNpcInvisibleWalls=true)
    animateCharacter(this, timer)
    [king only] summoner(this, "warrior", "summon", 5000)
} else if king: g_gameFinished = true        // every frame during the 3 s fade
```
`g_frameTimers` is cleared by `deleteAll()` in setupScene (:165).

### 4.2 `isDead(this, beam)`: death, fade and XP [S controlCharacters.as:369-404]
```
if hp <= 0:
  now = GetTime()
  if no "deathTime":
     if beam != "": AddEntity(beam, GetPosition()+(0,0,4), 0)
     if HasParticleSystem(0): KillParticleSystem(0)   // minion/master_knight smoke, king fire aura stop emitting
     deathTime = now (uint)
     SetColor((0,0,0))                                // black silhouette
     SetCollision(false)
     if no "pvpMode" and not a main character:
        addToExp(0, expGiven); addToExp(1, expGiven)  // BOTH players get XP = data hp
  deadTime = now - deathTime
  if deadTime >= 3000 (DEAD_FADE_OUT_TIME): DeleteEntity(this); return true
  SetAlpha(1 - deadTime/3000); return true
return false
```
**Enemies drop nothing.** There is no item or potion spawn on death anywhere in the scripts. The death sound comes from the beam's SoundEffect: vanish.ogg at 0.5, or creature_dying.mp3 at 1.0 for master_knight.

### 4.3 `meleeCharacterAI(this, swordEffect)` [S characterAI.as:117-199]
Used by warrior, minion and knight (enemy_sword.ent), master_knight (dark_sword.ent), paladin (paladin_sword.ent), and the king at close range (paladin_sword.ent).
```
speed=GetFloatData("speed"); action=GetUIntData("action")    // LOCAL COPY, read once
viewR=GetFloatData("viewRadius"); atkR=GetFloatData("attackRadius")
main = findMainCharNeighbours(this, true); if main && main.hp<=0: main=null
pos = GetPositionXY(); dist = main ? |main.pos - pos| : 0
if (dist >= 2*viewR || main == null): AddUIntData("action", STANDING)   // stored only; local `action` unchanged this frame
if action == STANDING:
    setForceX(0)
    if main && dist < viewR:
        AddUIntData("action", CHASING)
        if has "chaseSfx" and !IsSamplePlaying(chaseSfx): PlaySample(chaseSfx)   // minion: soundfx/minion.ogg
elif action == CHASING:
    if main:
        mp = main.pos
        if dist > atkR && isOnSight(this, main, 2.0) && |pos.x-mp.x| > 10 && |knockBack(this)| == 0:
            if pos.x < mp.x: currentDir=RIGHT; forceX=+speed else: currentDir=LEFT; forceX=-speed
        else: forceX = 0
        if swordAttack(this, swordEffect, "enemy_sword_beam.ent", dist <= atkR):
            if jumpBackAfterAttack != 0:
                knockBack(this, ( currentDir==LEFT ? +12 : -12 , -12 ))     // hop away from facing, upward
    // main == null: nothing, so forceX keeps last frame's value for this one frame
```
Consequences:
- The state machine has hysteresis: it enters CHASING below viewRadius and leaves at 2×viewRadius, or when the target is lost from the bucket row.
- `isOnSight(this, main, 2)` means |dy| ≤ 152 (SIZE_TOLERANCE 76 × 2).
- An enemy stops within 10 px horizontally (it does not jitter) and freezes steering while any knockback remains.
- currentDir only updates while stepping, so the swing goes the old way if the player jumps over the enemy while it is in range.
- Any `doDamage` hit sets the victim's `action`=CHASING (doDamage.as:236), which gives aggro on hit.
- There is **no pathfinding, no jumping and no ledge detection.** NPCs are held on platforms only by `npc_wall.ent` blocks (40x97, invisible) and by real walls. They walk off edges otherwise, and die in instant_death volumes.

### 4.4 `swordAttack(this, swordName, beamName, isInRange)` [S swords.as:43-107]
```
if !isInRange: lastTimeDidntSee = now; return false
if waitBeforeAttack == 0:  if now - lastSwordAttack < coolDown: return false     // fires when >= coolDown
else:                      if now - lastTimeDidntSee < coolDown: return false     // master_knight: must stay in range coolDown ms
pos = GetPosition(); dir = currentDir
swordPos = pos + (dir==LEFT ? (-28,0,10) : (+28,0,10))
AddEntity(swordName, swordPos + (0,0,8), @sword)          // i.e. owner + (±28, 0, +18)
AddEntity(beamName,  pos + (0,0,8),      @beam)
if dir == LEFT: sword.MirrorParticleSystemX(0,true); beam.MirrorParticleSystemX(0,true)
sword: ownerX, ownerY (float) = owner XY now; ownerID (int) = owner ID; direction (uint) = dir; damage (int) = owner damage
if owner is bruxo/princess: sword.mainCharacter=1 (uint); pvpMode=1 if owner has pvpMode; playerId (uint)
lastTimeDidntSee = now; lastSwordAttack = now; return true
```
- `lastSwordAttack` starts at 0, so the first swing is immediate.
- For master_knight the wait restarts after every swing, because lastTimeDidntSee is reset on attack. In continuous contact it swings every 420 ms, after a 420 ms wind-up on entry.
- The sword entity does not follow the owner. It is a hitbox that lives about 252 ms (1.7).
- **Its collision box is not mirrored** for LEFT swings (enemy_sword offset -1 stays -1).

### 4.5 `doDamage(this, knockForce, knockFromOwner, collideEverything, attackMode, damageMultiplier)` [S doDamage.as:45-242]
Called every frame by every weapon.
```
dir = GetUIntData("direction"); b = GetCurrentBucket(); mainChar = has "mainCharacter"
cands = bucket b + (dir==LEFT ? b+(-1,0) : b+(1,0));  if mainChar: + b+(0,1) + b+(0,-1)
isResistant = false; pvp = has "pvpMode"
damage = mainChar ? (dmg + int(dmg * (g_charLevel[playerId or 2]/4.0))) * int(mult) : dmg * int(mult)
ownerID = GetIntData("ownerID"); me = absolute box
for e in cands (in order):
  skip if !e.Collidable() || e.ID==me.ID || e.ID==ownerID
  skip if e has "ownerID" && e.ownerID == ownerID                      // same attacker's other blows
  skip if e has no "hp" && !collideEverything
  if collideEverything && e.name=="npc_wall.ent": skip
  if !pvp && isMain(e) && me has mainCharacter: skip                     // no friendly fire (players)
  if !pvp && e has mainCharacter && me has mainCharacter: skip           // player blows don't hit player blows
  if !checkBoxHit(box(e), me): continue
  r = e; e.hitBy = me.GetEntityName() (string)
  if isMain(e): g_camera.startEarthquake(min(10, damage))
  if e has no "hp": continue                                             // walls etc. (collideEverything only)
  key = "id"+e.ID; if me has key: continue                               // each target is damaged once per weapon
  r = e
  if e has "fireResistant" && attackMode==FIRE && e.fireResistant != 0: isResistant = true   // never reset
  me.AddIntData(key, 0)
  damage = isResistant ? damage/5 : damage                               // integer division; OVERWRITES damage
  addToHp(e, -damage); g_messages.addMessage(-damage, e.pos)             // floating "-N"
  [pvp scoring block, lines 169-201: see below]
  ownerPos = knockFromOwner ? (ownerX, ownerY) : me.pos
  back = normalize(e.pos - ownerPos) * knockForce
  bias = isResistant ? 0.2 : 1;  if e has "pushBackBias": bias = e.pushBackBias
  knockBack(e, back*bias)
  hitPos = me.GetPosition() + (dir==RIGHT ? +boxSize.x/2 : -boxSize.x/2, 0, 0)
  if !isResistant: if me has "hit": AddEntity(me.hit, hitPos, @p); if back.x > 0: p.MirrorParticleSystemX(0,true)
  else: AddEntity("hit_fail.ent", hitPos, 0)
  e.action = CHASING (uint)
return r
```
**PvP scoring block** (lines 169-201), which runs only if the victim has `pvpMode` and hp <= 0:
- **Victim is a main character:**
  - `attacker = SeekEntity(ownerID)`.
  - If the attacker is a main character, `g_pvpPoints[attacker.playerId]++`.
  - Otherwise `g_pvpPoints[victim.playerId]--`.
  - **[I]** A null attacker (owner already deleted) raises a null-handle exception in `isAMainCharacter(null)`.
- **Victim is an NPC:** if the attacker is a main character, `addToExp(attacker.playerId, victim.expGiven)`.

Rules that fall out of this:
- **Enemy weapons hit other enemies** (there is no NPC friendly-fire filter), e.g. a knight's sword damages a warrior in front of it.
- Enemy weapons hit the players.
- Weapons with `collideEverything=false` never touch walls or fireballs.

Hit effect by weapon:
- Enemy swords: `hit`="blood.ent" (blue-white burst, hit01.ogg at 0.3).
- Player swords: "enemy_blood.ent" (orange burst, hit01.ogg at 0.3).

### 4.6 Damage and knockback matrix [derived from S + D]

| Attacker → effect | damage | knockForce | knockFrom | notes |
|---|---|---|---|---|
| warrior / minion / knight sword | 5 / 20 / 10 | 4 | owner | victim pushBackBias (players have none, so 1) |
| master_knight dark sword | 25 | 12 | owner | box 89x41 at (-1,12) |
| paladin / king melee sword | 20 / 10 | 15 | owner | box 64x19 |
| impy / king fire_ball | 18 / 10 | 15 | fireball position | FIRE mode; resistant targets take /5 |
| lava fire_shoot | 18 | 15 × bias | shooter position | ignores fireResistant (section 8.2) |
| enemy-enemy or enemy-player body contact | 0 | 7 each way | midpoint | every overlapping frame, plus jumpfx.ent |
| instant_death contact | hp := 0 | none | none | section 8.1 |

Players take damage × 1 (they have no fireResistant).

**Fire resistance** (minion, impy, paladin) against FIRE attacks, i.e. `fire_ball`/`combo_fire_ball` via `manageFireBall`:
- damage /5 (integer division)
- the knockback bias is the target's pushBackBias (they all have one)
- `hit_fail.ent` spawns instead of blood (4 sparks, cast_fire_spell.ogg at 1.0)
- the fireball is deleted **without** explosion.ent or earthquake

### 4.7 `animateCharacter(this, timer)` [S controlCharacters.as:222-268] and `frameTimer` [S eth_util.as:128-167]
```
stride = GetUIntData("stride"); dir = currentDir; ground = touchingGround != 0; f = force
if ground:  f.x<0 → timer.Set(4,7,stride); f.x>0 → Set(8,11,stride); else dir==LEFT → Set(4,4) / RIGHT → Set(8,8)
else:       LEFT: f.y>0 → Set(5,5) else Set(4,4);  RIGHT: f.y>0 → Set(9,9) else Set(8,8)
SetFrame(timer.Get())
frameTimer.Set(first,last,stride): if (first,last) changed → frame=first, lastTime=now, return
                                   elif now-lastTime > stride (strict) → frame++ (wrap to first after last), lastTime=now
```

---

## 5. Ranged AI (impy, and the king when far)

### 5.1 `rangedCharacterAI(this, effectEntity, speedMultiplier)` [S characterAI.as:43-115]
```
if no "lastRangedAttack": lastRangedAttack = now        // first shot only after coolDown ms
speed, attackRadius read (viewRadius and action are read but UNUSED)
main = findMainCharNeighbours(this, true); if main && main.hp<=0: main=null
if main:
  if dist < attackRadius:                                // too close: flee
     if pos.x < main.x: currentDir=LEFT; forceX=-speed else: currentDir=RIGHT; forceX=+speed
     running = true
  else:
     currentDir = (pos.x > main.x) ? LEFT : RIGHT; forceX = 0
  if !running && isOnSight(this, main, 1.0)              // |dy| <= 76
     && now - lastRangedAttack > coolDown (strict):
       castSpell(this, effectEntity, 0, (0,0,0), GetIntData("damage"), speedMultiplier)
       lastRangedAttack = now
       PlaySample("soundfx/cast_fire_spell.ogg")
// main == null: NOTHING. forceX is NOT reset, so a fleeing impy keeps running until a wall zeroes forceX
```
- The impy has no view-radius gate. It fires every >1500 ms whenever the player is in its bucket window and |dy| ≤ 76. The window is the impy's bucket ±2 columns, which reaches between 512 and 768 px to each side depending on where the impy sits inside its bucket.
- It flees within 100 px at 120 px/s. It never chases.
- It is pushBackBias 0, so knockback does not move it.

### 5.2 `castSpell(owner, spellName, mana, offset, damage, speedMult)` [S spells.as:43-81]
```
if owner has "mp": if mp < mana: return false else addToMp(owner, -mana)   // enemies have no mp: always cast
AddEntity(spellName, owner.GetPosition() + offset, @h)
h: spellOffsetX/Y (float); speed (float) = owner.speed * speedMult; direction (uint) = owner.currentDir;
   ownerX/ownerY (float); ownerID (int); damage (int); playerId (uint) if owner has it;
   mainCharacter=1 and pvpMode=1 if the owner is a main character (pvp only when the owner has it)
return true
```

### 5.3 `manageFireBall(this, explosion)` [S spells.as:123-152], every frame
```
AddToPositionXY( (direction==LEFT ? -1 : +1, 0) * UnitsPerSecond(speed) * 1.5 )    // horizontal only, no gravity
t = doDamage(this, 15, knockFromOwner=false, collideEverything=true, FIRE, 1)
if t != null:
   explode = !(t has "fireResistant" && t.fireResistant != 0)
   if explode: AddEntity(explosion, GetPosition(), 0); g_camera.startEarthquake(20)
   DeleteEntity(this); return
if has "hitBy": DeleteEntity(this)             // another collideEverything weapon (e.g. an opposing fireball) touched it
```
- Speeds: impy 120×1×1.5 = **180 px/s**; king 100×1.5×1.5 = **225 px/s**.
- The fireball explodes on **any collidable entity** in its bucket and the forward bucket: walls, tiles, invisible_wall, event01, swords, fire_shoot. The exceptions are npc_wall.ent, its own owner, and blows from the same owner.
- If nothing is hit, it vanishes silently when its particles end (about 1.6-1.9 s; section 1.7).
- `explosion.ent` has SoundEffect hit01.ogg at 0.5, 12 frames of explosion.png, and an orange light (range 167.5).

---

## 6. King (final boss) and the boss trigger

### 6.1 `mixedCharacterAI(this, "paladin_sword.ent", "fire_ball.ent", distance=100, speedMultiplier=1.5)` [S characterAI.as:201-216]
```
main = findMainCharNeighbours(this, true)
if main:
   if |main.pos - pos| > 100: rangedCharacterAI(this, "fire_ball.ent", 1.5)
   else:                      meleeCharacterAI(this, "paladin_sword.ent")
// main == null: nothing (forceX not reset)
```
Note that the target's hp is not checked here (the sub-AIs do check it).

With the king's data:
- **Beyond 100 px:** he faces the player and stands still. The flee branch (dist < 35) is unreachable. He shoots a fire_ball (damage 10, 225 px/s) every >1000 ms when |dy| ≤ 76.
- **Within 100 px:** melee. viewRadius 280, so STANDING→CHASING triggers at once. He walks at 100 px/s and swings paladin_sword (damage 10, knock 15) when dist ≤ 35, every ≥1000 ms. After each swing he hops (jumpBack 12,-12).
- `lastSwordAttack` and `lastRangedAttack` are independent timers that share `coolDown` 1000.
- The fire aura and the active orange light come from king.ent.

### 6.2 `summoner(this, "warrior", "summon", 5000)` [S controlCharacters.as:615-643]
```
if no "lastSummon": lastSummon = now
if now - lastSummon >= 5000:
   ents = GetEntitiesFromBucket(currentBucket + (currentDir==LEFT ? (-1,0) : (+1,0)))
   for e in ents: if e.GetEntityName() == "summon":                       // marker label, not summon.ent
        AddEntity("summon.ent", e.GetPosition(), 0)                         // dark flash, checkpoint.mp3 at 0.5
        AddEntity("warrior.ent", e.GetPosition() + (0,0,-10) + (randF(6),0,0), @h)   // x jitter U[0,6]
        spawn(h, "warrior"); break
   lastSummon = now                                                       // reset even if no marker was found
```
- Level3 markers (label `summon`) are at (10367,1315), (10632,1304), (10875,1307) and (11223,1313).
- The king at x≈10920 (bucket 42) finds 10632 in bucket 41 when facing LEFT, or 11223 in bucket 43 when facing RIGHT.
- One warrior is summoned per 5 s, and without limit, as long as the king lives and a marker is in the adjacent bucket.
- pvp6 also has one `summon` marker, but no king.

### 6.3 `ETHCallback_event01` (boss trigger) [S events.as:43-64]
The `event01` label is at (10920,1408) in level3. It is static, collidable, 128x256, with no sprite; while it exists it blocks movement and projectiles like a wall.
```
for e in GetEntitiesFromBucket(own bucket):                  // no distance/box test: same 256x256 bucket is enough
   if isAMainCharacter(e):
      StopSample("soundfx/fase.mp3")
      PlaySample("soundfx/laugh_king.mp3")
      PlaySample("soundfx/chefao.mp3"); LoopSample("soundfx/chefao.mp3", true)
      AddEntity("summon.ent", GetPosition(), 0)
      AddEntity("king.ent", GetPosition() + (0,0,-10), @h); spawn(h, "king")
      DeleteEntity(this)
      AddEntity("invisible_wall.ent", (10112, 1408, 0), 0)   // 256x256 solid: seals the arena behind the player
   // NO break: if both players are in the bucket the same frame, two kings and two walls spawn
```
**King death:** `ETHCallback_king` sets `g_gameFinished = true` every frame while the king is fading. doLoop then:
- stops chefao.mp3,
- plays death_king.ogg,
- records the time.

The ending UI belongs to another dimension. The bruxo callback forces its hp to 100 each frame once the game is finished (controlCharacters.as:285-288).

---

## 7. Movement and collision shared by enemies (util.as + controlCharacters.as)

### 7.1 Force and knockback primitives [S util.as:161-236]
- `applyForce(e,v)`: `forceX += v.x; forceY += v.y`.
- `setForceX(e,f)` / `setForceY(e,f)`: overwrite.
- `getCurrentForce(e)` returns (forceX, forceY).
- `knockBack(e,v)`: `knockBackX += v.x; knockBackY += min(0, v.y)`. **Only the upward (negative) y component accumulates.**
- `getKnockBackVector` / `setKnockBackVector` read and write both keys.

### 7.2 `move(e)` [S util.as:188-219]
```
if touchingGround==1 && forceY > 0: forceY = 0
v = (forceX, forceY) / fps                                       // px this frame
size = min(GetSize().x, GetSize().y) * 0.4                       // 32x48 → 12.8; master_knight 80x96 → 32
if |v| >= size: v = normalize(v) * size * 0.9                    // → 11.52 (28.8)
k = (knockBackX, knockBackY); if |k| >= size: k = normalize(k)*size*0.9
k *= 0.8; if |k.x| < 0.1: k.x=0; if |k.y| < 0.1: k.y=0
setKnockBackVector(k)
AddToPositionXY(v + k)
```
Consequences at 60 fps:
- For 32x48 characters, a speed of 768 px/s or more (12.8 px/frame) is cut to 691.2 px/s (11.52 px/frame). Speeds below 768 pass unchanged, so a falling body's speed jumps from about 767 down to 691 px/s once forceY reaches 768.
- A knock impulse moves the entity about impulse×0.8/(1-0.8), roughly 4× the impulse. The knight/paladin/king jump-back of (±12,-12) is first clamped to (±8.15,-8.15), which gives about 32 px total on each axis over about 19 frames.

### 7.3 `doCharacterCollision(e, useNpcInvisibleWalls)` [S controlCharacters.as:43-220]
Enemies call this with true, players with false.
```
force = getCurrentForce(e); dir = findDirection(force); walking = (dir LEFT/RIGHT) ? 1 : 0 (uint, write-only)
cands = GetEntitiesFromBucket(current) ++ findDestinationBuckets(e, force, forceBottomBuckets=true)
me = absolute box; thinnerBoxHit=false
for c in cands:
  skip !c.Collidable(); if !useNpcInvisibleWalls && c.name=="npc_wall.ent": skip; skip self
  skip if c has "ownerID"                                      // weapons and projectiles
  if !checkBoxHit(me, box(c)): continue
  if c.name in {instant_death.ent, instant_death2.ent, instant_death3.ent}:
      e.hp = 0 (AddIntData)
      if isMain(e) && e has "pvpMode": g_pvpPoints[e.playerId]--; e.dontFrag = 1   // per overlapping tile, per frame
  if c has "dontCollide": continue
  if c has "hp" && e has "hp":                                 // body contact
      knockBack(e, normalize(e.pos - c.pos)*7); knockBack(c, normalize(c.pos - e.pos)*7)
      AddEntity("jumpfx.ent", (mid.x, mid.y, 0), 0); continue
  thin = me with size.x *= 0.6; thinnerBoxHit = checkBoxHit(thin, box(c))   // OVERWRITTEN each collided entity
  cur = e.pos; d = findBoxDirection(me, box(c))
  RIGHT: forceX=0; e.x = (c.left - me.w/2) - 1;  me refreshed
  LEFT:  forceX=0; e.x = (c.right + me.w/2) + 1; me refreshed
  DOWN:  if force.y > 0 && thinnerBoxHit:
             if force.y > 700: PlaySample("soundfx/fall.ogg"); AddEntity("fall.ent", (me.x, me.y+me.h/2, 0), 0); earthquake(10)
             forceY = 0; e.y = c.top - me.h/2; touchingGround = 1
  UP:    if thinnerBoxHit: forceY = 0; e.y = c.bottom + me.h/2
if force.y < 0: touchingGround = 0 elif !thinnerBoxHit: touchingGround = 0
if touchingGround == 1: jumps = 0 (int)
```
- **The snap positions ignore the collision-box offset.** They write the entity position as if the box were centred on it. master_knight (offset y +4) therefore sits 4 px inside the ground, and bruxo/princess (offset +4) do too. That keeps the thinner box overlapping, so it must be replicated.
- `force` is the value captured **after** `move()`.
- `me` is refreshed only after a LEFT/RIGHT snap.
- The "fall" landing (force.y > 700 px/s, i.e. about 0.58 s or about 204 px of free fall) also applies to enemies: a falling enemy plays fall.ogg and shakes the camera.

### 7.4 Direction helpers [S util.as:238-348]
- `RIGHT=0, LEFT=1, DOWN=2, UP=3`.
- `findDirection(v)`: take `deg = radianToDegree(GetAngle(v))` (atan2(x,y) in [0,360)).
  - 45 ≤ deg < 135 → RIGHT
  - 135 ≤ deg < 225 → UP
  - 225 ≤ deg < 315 → LEFT
  - else → DOWN, which includes (0,0)
- `findDirection(vector3)` calls it on (x,y).
- `findBoxDirection(a,b)` (a = mover, b = obstacle, both absolute):
  - If `a.y` is strictly inside (b.y - b.h/2, b.y + b.h/2): return `a.x < b.x ? RIGHT : LEFT`.
  - Else if `a.x` is strictly inside b's x-span: return `a.y < b.y ? DOWN : UP`.
  - Else if `a.y + a.h/2 - 5 <= b.y - b.h/2`: return DOWN.
  - Else: return `findDirection(b.pos - a.pos)`.
- `findDestinationBuckets(e, dir, forceBottom)` builds a new array:
  - dir.x<0 → (-1,0); dir.x>0 → (+1,0)
  - dir.y>0 or forceBottom → (0,+1); dir.y<0 → (0,-1)
  - forceBottom → (-1,+1) and (+1,+1)
  - It never includes (0,0); the caller adds that itself.
- `checkBoxHit(a,b)`: false if a.minX > b.maxX, a.minY > b.maxY, a.maxX < b.minX or a.maxY < b.minY. **Touching edges count as a hit.** z is ignored.
- `directionToString` returns "LEFT"/"RIGHT"/"UP"/"DOWN"/"". It is used only in a commented-out debug block.

---

## 8. Hazards

### 8.1 Kill volumes: `instant_death.ent`, `instant_death2.ent`, `instant_death3.ent` [D; S controlCharacters.as:88-101]

| name | type | static | box | sprite | placed |
|---|---|---|---|---|---|
| instant_death.ent | 5 (layerable) | 0 | 64x64 | black.bmp, blendMode 2 | L1 ×61 |
| instant_death2.ent | 0 | 1 | 256x256 | none | pvp2 ×9 |
| instant_death3.ent | 5 | 0 | 64x64 | none | L3 ×65, pvp1 ×26, pvp2 ×8 |

- Any character (player or enemy) whose box overlaps one is set to `hp = 0` on that frame. The normal death then runs next frame.
- They are collidable but also act as solids: they have no `dontCollide`, so the resolution below the kill check still runs. The character dies anyway.
- Excluded by: fire_shoot (lavaShooter.as:94-97). **Not** excluded by fire_ball/doDamage (collideEverything): a fireball hits a kill tile and explodes on it **[I from code]**.

**Lava (`lava.ent`) and thorns (`thorns.ent`) are visuals only:**
- lava: static, non-collidable, lava.jpg, emissive 1, blendMode 2; L3 ×19, pvp2 ×12.
- thorns: thorn.dds 256x64; pvp1 ×7.

The killing is done by the kill tiles laid over them. The lore line "Tentar jogar inimigos na piscina de lava pode ficar bem divertido" (data.enml:57-59) refers to knocking enemies into those tiles.

### 8.2 Lava shooter [S lavaShooter.as:43-128; D shooter.ent, fire_shoot.ent]
`shooter.ent`:
- static, non-collidable, no sprite
- PS: explosion.JPG ember glow, 14p, repeat 0
- light: active, static, range 125.5, colour (1,0.5,0.3)
- custom `coolDown`=2500 (uint), `force`=400 (float)
- Every placed instance keeps those values (L1 ×9, L3 ×5, pvp2 ×4).

```
ETHCallback_shooter:                                    // static, so it runs only while visible [E13 model]
  if no "lastShoot": lastShoot = now; coolDown = 1000 + rand(coolDown)   // rand inclusive → 1000..3500, fixed per shooter
  if now - lastShoot >= coolDown:
     AddEntity("fire_shoot.ent", GetPosition() + (0,0,15), @h)
     h.force (float) = 500 + randF(force)               // 500..900
     h.ownerX, h.ownerY (float) = shooter XY; h.damage (int) = 18
     lastShoot = now; PlaySample("soundfx/cast_fire_spell.ogg")

ETHCallback_fire_shoot:                                 // dynamic; collidable 22x22; custom dontCollide
  force -= UnitsPerSecond(1200) * 0.6                   // force decays at 720 /s
  AddToPositionXY( (0, (-force / fps) * 0.6) )          // vy = -0.6*force px/s
  cands = bucket, bucket+(0,1), bucket+(0,-1)           // vertical column only
  me = absolute box; ownerPos = (ownerX, ownerY)
  for c in cands:
     skip !Collidable, self (ID), same name (other lava balls), instant_death*.ent, npc_wall.ent
     if checkBoxHit(box(c), me):
        if c has "hp":
            addToHp(c, -damage)                         // 18, ignores fireResistant; no damage popup
            bias = c.pushBackBias if present else 1
            knockBack(c, normalize(c.pos - ownerPos) * 15 * bias)
        AddEntity("explosion.ent", GetPosition(), 0); g_camera.startEarthquake(20); DeleteEntity(this); return
  if GetPositionXY().y > ownerPos.y:                    // fell back below the launcher
     DeleteEntity(this)
     AddEntity("lava_drops.ent", (ownerX, ownerY - 22 /*box h*/, z = this.y /*sic*/), 0)
```
Ballistics: v0 = 0.6·F0 = 300-540 px/s upward, and the effective gravity is 0.6×720 = 432 px/s². So:
- apex height = v0²/864 → **104-338 px**
- time to apex 0.69-1.25 s
- total flight about 1.4-2.5 s

Other effects:
- It explodes on any collidable non-excluded entity: tiles, walls, characters, swords (so a sword swing destroys a lava ball) and fireballs.
- `explosion.ent` has the hit01.ogg SoundEffect at 0.5.
- `lava_drops.ent` is a splash of 9 embers, allAtOnce, about 1.2 s.
- **Bug-compat:** `lava_drops` gets **z = the fire ball's world y** (lavaShooter.as:126). That is harmless on screen (ZAxisDirection 0,0) but affects draw depth.

### 8.3 Falling bridge [S events.as:66-106; D falling_bridge.ent]
`falling_bridge.ent`: dynamic, collidable 64x32, single_stone.png with single_stone_nm.png, blendMode 2.
```
SIZE_ADDITION = 6
if no "falling":
   cands = own bucket + bucket (0,-1)
   s = absolute box; s.size.y += 6; s.size.x *= 0.9; s.pos.y -= 3      // 57.6 x 38, top raised 6 px, bottom unchanged
   for c in cands: if isAMainCharacter(c) && checkBoxHit(box(c), s):    // players only; enemies never trigger it
        gravity = 0.0 (float); falling = now (uint)                     // no break; re-set for a 2nd player same frame
elif now - falling > 600:                                                // stays solid and still for 600 ms
   SetCollision(false)
   g = gravity; if g == 0: AddEntity("bridge_fall.ent", GetPosition() + (0,0,-2), 0)   // dust + brige_fall.ogg at 1.0
   gravity = g + UnitsPerSecond(1200); AddToPositionXY((0, gravity / fps))              // 1200 px/s^2 fall
   if !isInScreen(this): DeleteEntity(this)
```
There is no warning shake. The stones labelled `dontFall`, `fontFall` and `step` are identical but never fall (section 2).

### 8.4 Walls [D; S]
- **`npc_wall.ent`:** dynamic, collidable, 40x97, no sprite. Placed L1 ×17, L2 ×6, L3 ×7, pvp1 ×2, pvp4 ×2, pvp5 ×8.
  - It blocks only entities running `doCharacterCollision(..., true)`, i.e. enemies.
  - Players pass through it (false).
  - `fire_shoot` ignores it (lavaShooter.as:99-100), and so does doDamage with collideEverything, i.e. fireballs (doDamage.as:117-121). Enemy swords never test it at all, because it has no hp.
- **`invisible_wall.ent`** (256x256) and **`invisible_wall_small.ent`** (128x256): static, collidable, no sprite.
  - They block everything, including fireballs and lava balls, which explode on them.
  - event01 adds one at (10112,1408).

---

## 9. World interaction

### 9.1 Enemy and player spawning [S setupScene.as:171-172, 257-312]
setupScene runs `g_spawn = GetEntityArray("spawn")` (the label `spawn` only). Each frame in doLoop, **before** messages and camera, it runs:
```
for s in g_spawn (array order):
  if !s.IsAlive(): continue
  if !isInScreen(s): continue                           // [TESTING-only forceSpawn for bruxo is compiled out]
  name = s.name; complete = s has "complete"            // presence test
  AddEntity(name+".ent", s.GetPosition() - (0,0,10), @h); DeleteEntity(s)
  h.waitBeforeAttack = 0 (uint); h.lastTimeAlive = 0 (uint)
  if pvp: h.pvpMode = 1; if complete: h.hp *= 5; h.maxHp *= 5     // players get 500 hp in pvp
  if isAMainCharacter(h): g_camera.setMainCharPos(h.pos, h.playerId)
  if h has "showUpSfx": PlaySample("soundfx/"+showUpSfx); g_camera.startEarthquake(8)   // master_knight, paladin
  if complete: continue
  spawn(h, name)                                        // overwrites waitBeforeAttack from data
```
- `isInScreen(pos)` [S util.as:96-125]: false if `x-76 > cam.x+W`, `x+76 < cam.x`, `y-76 > cam.y+H` or `y+76 < cam.y`, where W,H = `GetScreenSize()`.
- So a spawn fires when its point comes within 76 px of the screen. **This depends on resolution:** the game opens at 1024x768 (main.as:144) but the video-options screen can change it. The port should fix 1024x768 or replicate this.

### 9.2 Proximity sounds [S setupScene.as:174-175, 314-326]
- `g_sounds = GetEntityArray("play_sound")`.
- Each frame, for each alive one that is `isInScreen`: `PlaySample("soundfx/"+name)` (all are "horror.mp3"), then DeleteEntity.
- Only the 2 `play_sound` labels in L2 ever play; the `play_sound.ent` labels never do.

### 9.3 `ETHCallback_help` [S main.as:147-161]
```
for e in GetEntitiesFromBucket(own bucket): if |e.pos - pos| < 30 && isAMainCharacter(e): g_messages.addMessage(GetStringData("message"))
```
- It is never deleted, so the message is re-added every frame while you stand there.
- `MessageManager.addMessage(string)` **[S messageManager.as:82-126]**:
  - plays `soundfx/help.mp3` if msg != the last displayed message;
  - ignores duplicates of an active message;
  - holds at most 10 messages, each shown for 4000 ms with alpha fading 255→0, at (10, 70 + slot×30) in "Arial", 30, (203,203,228);
  - also shows the latest one at half size near player 1.

Messages (cp1252) **[D]**:
- L1: "Utilize as setas ou as direcionais do joystick para mover-se", "Pressione a seta para cima ou CTRL para pular", "Pulo duplo: para cima enquanto estiver no ar", "Evite tocar nas bolas de fogo", "Caveiras recuperam seu HP", "Pressione espaço para ativar a luz", "Golpe de espada: tecla 'S'" (×2), "Bola de fogo: tecla 'D'", "Alguns inimigos são mais fortes que outros", "Próxima fase: seta para baixo".
- L3: "É uma boa hora para ligar a luz" (×5).

### 9.4 `ETHCallback_story` [S main.as:163-168]
Every frame (while visible) it draws:
`shadowText(GetPositionXY() - GetCameraPos(), g_gameData.get("global", name), "Arial Narrow", 16, a=100, r=203, g=203, b=228)`

- The text is multi-line, from data.enml.
- It is anchored at its top-left on the entity's world position, so it scrolls with the world.
- Names used: story01, story02, soldados, fun, comboTip (L1); story03, flyingWall, annoying, portalToCastle (L2); story04, bridge, warning, nights, wisdom (L3).

### 9.5 `ETHCallback_checkpoint` [S main.as:170-192]
```
for e in GetEntitiesFromBucket(own bucket):
  if |e.pos - pos| < 30 && isAMainCharacter(e) && e.playerId == 0:          // only player 1 (bruxo)
     e.hasCheckpoint = 1 (uint); e.lives = g_lives (int, never read anywhere)
     SaveScene("scenes/checkpoint.esc")                                     // BEFORE deleting itself
     g_messages.addMessage("Checkpoint...")
     DeleteEntity(this); AddEntity("checkpoint_effect.ent", GetPosition() + (0,0,10), 0)   // checkpoint.mp3 at 0.5
```
- checkpoint.ent is a tombstone (Lapide_Bitmap2.png) with a black smoke PS and a green light (range 47.5, colour (0.3,1,0.7)).
- When bruxo dies with `hasCheckpoint`, the game loads `scenes/checkpoint.esc` (controlCharacters.as:435-445).
- **Bug-compat:** the saved scene still contains this checkpoint, and the player stands within 30 px of it, so on reload it re-triggers immediately: it saves again, shows "Checkpoint..." and plays the effect.
- The saved scene captures every entity's custom data. That includes shooters' randomized `coolDown`/`lastShoot`, unspawned `spawn` markers, and live enemies with their state.

### 9.6 `ETHCallback_next_level` [S main.as:194-229]
next_level.ent: passage.png with a red particle column and a red light.
```
if no "fadeOut":
   for e in own bucket: if |e.pos-pos| < 80 && isMain(e) && e.playerId==0 && getPlayerXYAxis(0).y > 0: fadeOut = now
else:
   bias=0; if fadeOut(fadeOutStart, bias): { if IsPixelShaderSupported(): UsePixelShaders(true); LoadScene("scenes/"+name, "setupScene", "levelLoop") }
   SetSampleVolume("soundfx/fase.mp3", 1 - bias); loadingMessage()
```
- Player 1 presses **Down** (the K_DOWN key, or joystick Y>0 on player 1's stick) within 80 px.
- `fadeOut` draws a full-screen black rectangle with alpha elapsed/3000.
- **Bug-compat:** on the final frame `bias` stays 0, so the music volume snaps back to 1.

### 9.7 Potions [S potions.as:43-76; D]
```
doPotion: for e in own bucket:
   if |e.pos-pos| < 20 && isMain(e):
      if e.hp >= e.maxHp: return                     // aborts the whole scan
      addToHp(e, GetIntData("hp")); DeleteEntity(this)
      AddEntity("potion_pick.ent", GetPosition() + (0,0,16), 0)   // red burst, potion_pick.ogg at 1.0
      // no break: a 2nd player in range the same frame is healed too
```
- potion_small.ent: skull.png, hp=20; the only potion actually placed (54 in total).
- potion_large.ent: HP_Bitmap.png, hp=50; placed nowhere.
- Both are static, non-collidable, 16x16, with a black particle aura.

### 9.8 `ETHCallback_play` [S setupScene.as:43-50]
`LoadMusic("soundfx/"+name); PlaySample(...); LoopSample(..., true); DeleteEntity(this)`

---

## 10. Utilities (full function list)

### 10.1 util.as [S util.as:43-475]

| function | lines | behaviour | called from |
|---|---|---|---|
| findAmongNeighbourEntities(e, name, horizontalOnly) | 43-68 | see 3.1 | util.as:73,78 |
| findMainCharNeighbours(e, horizontalOnly) | 70-94 | see 3.1 | characterAI.as:54,123,204 |
| isInScreen(ETHEntity@) / isInScreen(vector2) | 96-125 | camera rect ± SIZE_TOLERANCE 76 | controlCharacters.as:346, events.as:101, setupScene.as:269,321 |
| getAbsoluteCollisionBox(e) | 127-132 | box.pos += GetPosition() | controlCharacters.as:704,712; doDamage.as:96,133,217; events.as:74,83; lavaShooter.as:80,102 |
| getLength / getDist / getSquaredDist | 134-148 | Euclidean | characterAI.as:67,136,171,207; main.as:153,176,202; potions.as:49; util.as:82-83,200,206 |
| isOnSight(e, other, mult) | 150-159 | true iff \|other.y - e.y\| ≤ 76·mult (no x or occlusion test) | characterAI.as:104,170 |
| knockBack / getKnockBackVector / setKnockBackVector | 161-178 | see 7.1 | characterAI.as:171,194; controlCharacters.as:116-117; doDamage.as:215; lavaShooter.as:114; util.as:216 |
| applyForce / move / getCurrentForce / setForceX / setForceY | 180-236 | see 7.1-7.2 | every character callback in controlCharacters.as (applyForce/move at :291-293, :334-336, :487-489, … :661-663); characterAI.as; controlCharacters.as:132-176; playerInput.as:329-351; swords.as:67 |
| RIGHT=0, LEFT=1, DOWN=2, UP=3; findDirection(vector2/vector3); findBoxDirection; directionToString | 238-324 | see 7.4 | findDirection: controlCharacters.as:46; findBoxDirection: controlCharacters.as:129; directionToString: only in commented debug code (controlCharacters.as:214) |
| findDestinationBuckets | 326-348 | see 7.4 | controlCharacters.as:53 |
| checkBoxHit | 350-372 | see 7.4 | controlCharacters.as:84,126,713; doDamage.as:135; events.as:84; lavaShooter.as:104 |
| drawRect(color) | 374-377 | DrawRectangle((0,0), screenSize, c,c,c,c) | menu.as:64; setupScene.as:220 |
| fadeIn(start) | 379-390 | if elapsed < 3000: full-screen black at alpha 1-elapsed/3000, return false; else true | gameover.as:68; menu.as:359; setupScene.as:255 |
| fadeOut(start, out bias) | 392-404 | if elapsed < 3000: bias = elapsed/3000, draw black at that alpha, return false; else true (bias untouched) | main.as:220; menu.as:352 |
| addToExp(player, exp) | 406-418 | `g_exp[p] += exp; next = lv[level]; while g_exp[p] >= next: g_exp[p] -= next; level++; next = lv[level]` | controlCharacters.as:387-388; doDamage.as:196 |
| addToHp(e, v) | 420-433 | hp = max(0, hp+v), clamped to maxHp only if "maxHp" exists (enemies have none) | doDamage.as:165; lavaShooter.as:108; potions.as:55 |
| addToMp(e, v) | 435-448 | same, with mp/maxMp | controlCharacters.as:721; interface.as:106; playerInput.as:326,366; spells.as:52 |
| shadowText(pos, text, font, size, a, r, g, b) | 450-455 | `DrawText(pos + (0.1·size, 0.1·size), text, font, size, ARGB(a/2 (int), 0,0,0))`, then `DrawText(pos, text, font, size, ARGB(a,r,g,b))` | main.as:165; menu.as:101,228,229; messageManager.as:151,159,172; setupScene.as:372-374,387; switch.as:95; timer.as:76; util.as:459; videoModes.as:89,114 |
| loadingMessage() | 457-460 | shadowText((20, screenH-70), "Carregando...\n", "Arial Narrow", 60, 255, 203,203,228) | controlCharacters.as:463; main.as:227; menu.as:357 |
| findEntityInScreen(name) | 462-474 | first GetVisibleEntities() match | unused |

`addToExp` and the lv20 gap: a missing `lvN` makes `getInt` return false without writing its native pointer **[E13 gs2d/src/Enml/Enml.cpp:509-521]**. What the AngelScript `&out` temporary then copies back is unverified:
- If it keeps the previous value, lv20 and lv31+ reuse 11000.
- If it copies 0 or garbage, lv20 is skipped instantly, and from lv31 the loop could spin forever.

This is only reachable by farming the king's summons or with the PageUp cheat (main.as:114-115 sets level 15).

### 10.2 eth_util.as [S eth_util.as:48-167] (stock Ethanon helpers)
- `Vector3ToString`, `Vector2ToString`, `FormatToString`: string formatting. Unused.
- `class stringInput`: blinking-caret text field.
  - The caret toggles every 300 ms.
  - Characters are appended from `GetLastCharInput()`.
  - K_BACKSPACE or K_LEFT (KS_HIT) deletes the last character.
  - It draws `sText + ": " + ss + ("|" if caret)` with DrawText.
  - **Unused.**
- `class frameTimer`: animation stepping (4.7). Used by every character.

### 10.3 constants.as [S constants.as:43-97]
- PI, PI2, PIb (unused); endl="\n"; APPLICATION_TITLE="Penumbra e o Castelo das Sombras - Ethanon Engine".
- **GRAVITY=1200**.
- STANDING=0, CHASING=1, COOLDOWN=2 (COOLDOWN is never assigned).
- DEAD_FADE_OUT_TIME=3000, LIVE_FADE_IN_TIME=3000.
- PVP_MODE=1, CAMPAIGN=2 (unused; main.as compares against the string "CAMPAIGN").
- DEFAULT_CHARSPRITE_CUTX/Y=4 (unused).
- **SIZE_TOLERANCE=76**.
- ATTACK_MODE_PHYSICAL=0, ATTACK_MODE_FIRE=1.
- MAX_PLAYERS=2, MAX_PVP_POINTS=3.
- `actionToString` (unused).
- MAIN_CHARACTER_ENTITY0="bruxo.ent", MAIN_CHARACTER_ENTITY1="princess.ent".
- `isAMainCharacter(e)` compares the entity name with those two.

### 10.4 switch.as [S switch.as:43-117]: a UI radio widget, not a world switch
- `Switch(b0, b1)` / `Switch(b0, img0, b1, img1)` hold two labels (and images); `current` starts at 0.
- `put(pos, font, size, width)`, for t = 0..1:
  - if there are images, `LoadSprite(image[t])`
  - str = "[" + (current==t ? "•" (0x95) : " ") + "] " + (no images ? label : "")
  - the hit rect is (width × size) for text or GetSpriteSize(image) for images, measured from drawCursor
  - alpha is 100, or 200 on hover; `getConfirmButtonStatus(0)==KS_HIT` while hovering sets current=t
  - `shadowText(drawCursor, str, font, size, current==t ? 255 : alpha, 203,203,228)`
  - with images, `DrawSprite(image[t], drawCursor + (30,0), ARGB(alpha',255,255,255))`
  - drawCursor.y += rect height
- `getCurrent()` / `setCurrent()`.

The instances are in videoModes.as:43-45: `g_enablePS` ("Ativa/Desativa pixel shaders"), `g_windowed` ("Janela"/"Tela-cheia") and `g_controls` (images input_options1/2.png).

**Only gameplay effect:** `g_controls` drives joystick assignment **[S playerInput.as:43-53, 159-164]**.

| current | label | player 1 (index 0) | player 2 (index 1) | "second controller present" when |
|---|---|---|---|---|
| 0 (default) | "2º joystick para jogador 2" | keyboard + joystick 1 | joystick 0 | joystick 0 is detected |
| 1 | "1º joystick para jogador 1" | keyboard + joystick 0 | joystick 1 | joystick 1 is detected |

Nothing in the game is a physical switch or button entity. `buttons.ent` is menu art.

---

## 11. Bug-compatibility list (preserve unless deliberately fixing)
1. The melee AI's local `action` lags the stored value by one frame (characterAI.as:120 vs 141/151).
2. The ranged and mixed AIs never zero `forceX` when the target is lost, so a fleeing impy keeps running (characterAI.as:55-114, 204-215).
3. doDamage: `isResistant` is never reset and `damage` is overwritten inside the target loop, so later targets in the same frame inherit /5 and the 0.2 bias (doDamage.as:77, 158, 164).
4. Enemy weapons damage other enemies. There is no NPC faction filter (doDamage.as:98-131).
5. Sword collision boxes are not mirrored for LEFT swings (swords.as:83-87).
6. Snap resolution ignores the collision-box offset: 4 px ground penetration for bruxo, princess and master_knight (controlCharacters.as:133-178).
7. `thinnerBoxHit` reflects only the last collided entity, and so decides touchingGround (controlCharacters.as:126, 192).
8. Checkpoint calls SaveScene before DeleteEntity and re-triggers on reload (main.as:184-186).
9. event01 and potions do not break after DeleteEntity: two players in range spawn two kings or heal twice (events.as:48-63, potions.as:47-59).
10. pvp: overlapping N kill tiles in one frame costs N points (controlCharacters.as:88-100).
11. lava_drops gets z = the lava ball's world y (lavaShooter.as:126).
12. Knockback decays per frame, which is frame-rate dependent (util.as:205-218).
13. Spawn/despawn tests depend on window resolution (util.as:96-125).
14. `play_sound.ent` and `spawn2` are never processed. `dontFall`, `fontFall` and `step` never fall. `ETHCallback_potion` is dead code.
15. fadeOut's final frame leaves bias=0, so the fase.mp3 volume pops back to 1 (util.as:392-404, main.as:219-226).
16. event01 detection is "same bucket", not proximity (events.as:46-50).
17. fire_shoot damage ignores fireResistant and shows no damage number (lavaShooter.as:106-115).

---

## 12. Reference tables

### 12.1 Every AddEntity in scope

| file:line | entity | position | angle/handle |
|---|---|---|---|
| swords.as:81 | swordName | owner + (±28, 0, 18) (−28 if LEFT) | handle; mirrored if LEFT |
| swords.as:82 | beamName (enemy_sword_beam.ent) | owner + (0,0,8) | handle; mirrored if LEFT |
| spells.as:56 | spell (fire_ball.ent) | owner + offset (0,0,0) | handle |
| spells.as:141 | explosion.ent / big_explosion.ent | fireball position | 0 |
| doDamage.as:225 | weapon's `hit` (blood.ent / enemy_blood.ent) | weapon pos ± boxW/2 in x | handle; mirrored if back.x>0 |
| doDamage.as:234 | hit_fail.ent | same as above | 0 |
| controlCharacters.as:118 | jumpfx.ent | midpoint of the two characters, z=0 | 0 |
| controlCharacters.as:161 | fall.ent | (box.x, box bottom, 0) | 0 |
| controlCharacters.as:377 | death beam | entity + (0,0,4) | 0 |
| controlCharacters.as:634 | summon.ent | marker position | 0 |
| controlCharacters.as:636 | warrior.ent | marker + (randF(6), 0, −10) | handle → spawn() |
| lavaShooter.as:54 | fire_shoot.ent | shooter + (0,0,15) | handle |
| lavaShooter.as:116 | explosion.ent | lava ball position | 0 |
| lavaShooter.as:126 | lava_drops.ent | (ownerX, ownerY−22, z=ball y) | 0 |
| events.as:56 | summon.ent | event01 position | 0 |
| events.as:58 | king.ent | event01 + (0,0,−10) | handle → spawn("king") |
| events.as:61 | invisible_wall.ent | (10112, 1408, 0) | 0 |
| events.as:97 | bridge_fall.ent | stone + (0,0,−2) | 0 |
| main.as:187 | checkpoint_effect.ent | checkpoint + (0,0,10) | 0 |
| potions.as:57 | potion_pick.ent | potion + (0,0,16) | 0 |
| setupScene.as:279 | `<name>.ent` | spawn − (0,0,10) | handle |

### 12.2 Sounds in scope (files in soundfx/)

| sound | trigger | volume |
|---|---|---|
| cast_fire_spell.ogg | each impy/king ranged cast (characterAI.as:110); each lava shot (lavaShooter.as:60); hit_fail.ent SoundEffect | sample default; entity soundVolume 1.0 for hit_fail |
| minion.ogg | minion STANDING→CHASING, if not already playing (characterAI.as:152-159) | default |
| sword01.mp3 | SoundEffect of enemy_sword (0.5), paladin_sword (0.2), sword0/1 (0.7) | entity soundVolume |
| dark_hit.ogg | dark_sword SoundEffect | 0.5 |
| hit01.ogg | blood.ent / enemy_blood.ent (0.3); explosion.ent (0.5) | entity soundVolume |
| explosion.ogg | big_explosion.ent | 0.5 |
| vanish.ogg | fade_out_beam.ent (every enemy death except master_knight; also player death) | 0.5 |
| creature_dying.mp3 | fade_out_beam_large.ent (master_knight death) | 1.0 |
| creature_show_up.mp3 / paladin_appear.mp3 | spawn of master_knight / paladin (setupScene.as:302-306), plus earthquake 8 | default |
| checkpoint.mp3 | summon.ent (king summons, event01) and checkpoint_effect.ent | 0.5 |
| laugh_king.mp3, chefao.mp3 (looped) | event01 (events.as:52-55); fase.mp3 is stopped | default |
| brige_fall.ogg (sic) | bridge_fall.ent | 1.0 |
| fall.ogg | landing with forceY > 700 (controlCharacters.as:157-163) | default |
| potion_pick.ogg | potion_pick.ent | 1.0 |
| horror.mp3 | play_sound (setupScene.as:324) | default |
| fase.mp3 | play entity (setupScene.as:46-48): LoadMusic, loop | faded by next_level/death |
| help.mp3 | MessageManager.addMessage(string) | 0.3 once the menu has run (menu.as:55) |

### 12.2b Every random call in the scripts
There are only four script-level random calls. All other jitter comes from the particle parameters in the .ent files (section 1.7).
- `rand(coolDown)` at lavaShooter.as:48: shooter period 1000 + [0..2500] ms, drawn once per shooter.
- `randF(force)` at lavaShooter.as:55: lava ball launch force 500 + [0..400].
- `randF(6)` at controlCharacters.as:636: x jitter [0..6] px on each king-summoned warrior.
- `rand(6000)` at environment.as:91: next lightning in 6000 + [0..6000] ms (clouds).

### 12.3 Text drawn in scope

| where | string | font | size | colour/alpha | position |
|---|---|---|---|---|---|
| shadowText (util.as:450) | any | any | s | shadow ARGB(a/2,0,0,0) at +0.1s; text ARGB(a,r,g,b) | pos |
| loadingMessage (util.as:459) | "Carregando...\n" | Arial Narrow | 60 | 255, (203,203,228) | (20, screenH−70) |
| story (main.as:165) | data.enml global.<name> (multi-line) | Arial Narrow | 16 | a=100, (203,203,228) | entity world pos − camera |
| help (main.as:157) | per-instance `message` | Arial (MessageManager) | 30 (latest also at 15 near the player) | fading over 4 s | (10, 70+30·slot) |
| checkpoint (main.as:185) | "Checkpoint..." | same as above | same | same | same |
| damage popups (doDamage.as:166) | "-N" | Arial | 15 | alpha 255→0 over 1333 ms, rising 15 px/s | target pos + (−10,−32) − camera |
| Switch.put (switch.as:95) | "[•] label" / "[ ] label" | caller's (Arial Narrow) | caller's (25) | 255 selected, 200 hover, 100 idle; (203,203,228) | stacked from pos |
| fadeIn/fadeOut/drawRect | full-screen rectangle | none | none | black, alpha as computed | (0,0)..screen |

### 12.4 Input reads in scope
- **switch.as:** `GetCursorPos()` (mouse), plus `getConfirmButtonStatus(0)`, which is:
  - Return without Alt, else LMB, else RMB (player 1 only);
  - else player 1's joystick JK_10.
- **next_level:** `getPlayerXYAxis(0).y > 0`, i.e. K_DOWN, or player 1's joystick Y > 0.
- **Enemies:** `hasASecondController()` (joystick detection plus g_controls) only decides whether princess is a possible target.
- **Player 2** has no keyboard: joystick only, per the g_controls mapping in 10.4.

### 12.5 Custom-data key registry (scope)
- **Enemy:** currentDir, forceX, forceY, knockBackX, knockBackY, touchingGround, action, lastSwordAttack, lastTimeDidntSee, fireResistant, hp, damage, speed, viewRadius, attackRadius, pushBackBias, stride, coolDown, waitBeforeAttack, jumpBackAfterAttack, expGiven, chaseSfx, lastTimeAlive (unused for NPCs), pvpMode, lastRangedAttack, lastSummon, deathTime, walking (write-only), jumps, hitBy (write-only for NPCs), showUpSfx (.ent).
- **Weapons:** ownerX, ownerY, ownerID, direction, damage, mainCharacter, pvpMode, playerId, hit (.ent), dontCollide (.ent), id<N> (hit registry), and for spells: spellOffsetX/Y and speed.
- **fire_shoot:** force, ownerX, ownerY, damage, dontCollide.
- **shooter:** coolDown, force, lastShoot.
- **falling_bridge:** falling, gravity.
- **potions:** hp.
- **Triggers:** message (help), name (story/next_level/play/play_sound/spawn/environment), fadeOut (next_level), complete (spawn).
- **Written on players:** hasCheckpoint, lives, dontFrag, hp (kill tiles), action (by doDamage).

### 12.6 Engine APIs used in scope

| API | how it is used | semantics to emulate |
|---|---|---|
| GetEntitiesFromBucket(vector2, ETHEntityArray&) | all neighbour searches | append the entities whose centre is in that bucket (256) [E13] |
| ETHEntity.GetCurrentBucket() | same | floor(pos/256) |
| GetEntityArray(name, arr) | spawn, play_sound, flashlight.ent, environment | exact label match |
| GetVisibleEntities(arr) | findEntityInScreen (unused), player1Summoner | entities in visible buckets |
| AddEntity(file, vec3, float) / AddEntity(file, vec3, @out) | section 12.1 | create from entities/; label = file |
| DeleteEntity(e) | many | mark dead, handle still readable |
| SeekEntity(int id) | doDamage pvp | lookup by ID; null if gone |
| IsAlive(), GetID(), GetEntityName() | many | |
| GetPosition() / GetPositionXY() / SetPosition / SetPositionXY / AddToPositionXY | movement, snapping | immediate position change |
| GetCollisionBox(), Collidable(), SetCollision(bool) | collision | offset box; toggle |
| GetSize() | move() clamp | sprite frame size |
| GetFrame() / SetFrame(uint) | animation | 4x4 sheet, row-major |
| SetColor(vec3), SetAlpha(float) | death fade | tint and alpha |
| HasParticleSystem(0), KillParticleSystem(0), MirrorParticleSystemX(0, true) | death, weapon facing | see 1.6 |
| Add/Get Float/Int/UInt/String Data, CheckCustomData | everywhere | see 1.5 |
| GetTime(), UnitsPerSecond(), GetFPSRate() | timers, physics | see 1.1 |
| rand(int), randF(float) | shooter, summoner | inclusive ranges, see 1.9 |
| GetAngle(vector2), radianToDegree, normalize, sqrt, abs, min, max | math | see 1.9 |
| GetCameraPos(), GetScreenSize() | isInScreen, story text | camera = world pos of the screen's top-left |
| PlaySample, LoopSample, StopSample, IsSamplePlaying, SetSampleVolume, LoadSoundEffect, LoadMusic, SampleExists | audio | see 1.10 |
| DrawText, DrawRectangle, DrawSprite, LoadSprite, GetSpriteSize, ARGB | UI | see 1.11 |
| SaveScene(path), LoadScene(path, preLoop, loop[, bucketSize]), GetSceneFileName() | checkpoint, next level | full-state serialization |
| GetInputHandle(), GetCursorPos(), KeyDown, GetKeyState, JoyButtonState, GetJoystickXY, GetJoystickStatus, GetLastCharInput | Switch, input helpers | KS_HIT is the press edge |
| enmlFile.get / getInt / getUint / getFloat | spawn(), story, addToExp | see 10.1 for missing keys |

Not used anywhere in scope: SetSprite, SetEmissiveColor, SetLightRange, DrawShapedSprite (used only in interface.as), GetLastFrameElapsedTime (it does not exist in 2010).

---

## 13. Supersonic engine notes (read-only survey)
- **Audio is WAV-only.** ARCHITECTURE.md:132 lists `AudioClip` as "Uncompressed RIFF/WAVE"; README.md:79 mentions a "from-scratch WAV decoder". Every Penumbra sound is .ogg or .mp3, so they must be converted offline or a decoder added.
- **Text is BitmapFont (BMFont text .fnt) or ImGui.** There is no TrueType rasteriser (src/core/BitmapFont.hpp:1-26). DrawText with system fonts at sizes 15/16/25/30/40/50/60/256 needs pre-baked BMFonts or ImGui fonts.
- Not checked: whether `src/core/ParticleSystem.cpp` can reproduce the Ethanon release, repeat, mirror and alphaMode semantics, 2D normal-mapped lighting, and SpriteAnimationSystem.
- **Recommendation:** implement the Ethanon-emulation layer on the game side rather than in the engine. That layer covers:
  - the 256-px bucket grid,
  - the custom-data bag,
  - name-bound per-entity callbacks with static/dynamic gating,
  - temporary-entity auto-delete,
  - particle SoundEffect-on-spawn,
  - the path-keyed sample API,
  - the per-frame knockback.

  The Magic Portals port does the same (game/sim).
- Per the user: another session is porting a different game onto the same Supersonic engine. Any engine change must be coordinated with it. I made none.

## Key facts

- Script text files are Windows-1252, not Latin-1. switch.as:76 and menu.as:124,133 contain byte 0x95, the bullet character, which Latin-1 decodes as an invisible control character.
- Callbacks bind as ETHCallback_ plus the scene EntityName with its extension removed. Scene labels without a .ent file still bind: help, story, event01, play, next_level, thumbnail, picker. ETHCallback_potion binds to nothing.
- Enemies are not placed in the scenes. Label 'spawn' markers with custom string name are instantiated as <name>.ent when the marker comes within 76 px of the camera rectangle (setupScene.as:257-312), then spawn() copies the data.enml table. The trigger therefore depends on screen resolution; the game opens at 1024x768.
- Enemy stats from data.enml:123-227 (hp/speed/coolDown/viewRadius/attackRadius/damage). warrior 75/50/800/180/40/5. minion 45/150/800/500/40/20, fire resistant, plays minion.ogg on aggro. knight 150/80/300/180/43/10, jumps back after attacking. master_knight 1700/100/420/230/80/25, waits before attacking. impy 75/120/1500/600/100/18, ranged, fire resistant. paladin 400/80/550/280/47/20, fire resistant, jumps back. king 3500/100/1000/280/35/10, jumps back.
- Perception uses buckets: findMainCharNeighbours searches only the enemy's own 256-px bucket row, columns -2..+2 (util.as:43-94). The 256-px default bucket size is assumed from the 2013 source, not verified for 2010.
- Melee AI (characterAI.as:117-199): STANDING becomes CHASING when dist < viewRadius and reverts at dist >= 2*viewRadius or when the target is lost. It chases if |dy| <= 152, |dx| > 10 and there is no knockback. It swings when dist <= attackRadius and cooldown has elapsed. There is no pathfinding or jumping; npc_wall.ent blocks exist only to stop NPCs.
- Ranged AI (impy): flees within attackRadius. Otherwise it stands, faces the player and casts fire_ball every coolDown ms (strict >) when |dy| <= 76, playing cast_fire_spell.ogg. It never resets forceX when it loses the target.
- King = mixed AI. Beyond 100 px it fires fire_ball at 225 px/s; within 100 px it melees with paladin_sword. Every 5000 ms it summons one warrior at a 'summon' marker in the adjacent bucket it faces. It is spawned by the event01 trigger in level3, which also stops fase.mp3, plays laugh_king.mp3, loops chefao.mp3 and adds an invisible_wall at (10112,1408). There is no king beam: king_beam.par and most .par files are unused.
- Enemies drop nothing. On death: fade_out_beam.ent (fade_out_beam_large.ent for master_knight), aura particle system killed, black tint, collision off, 3000 ms alpha fade, then deleted. Both players receive XP equal to the data.enml hp.
- doDamage.as:45-242: each weapon damages each target once (id<N> key on the weapon), shows a damage number, knocks back from the owner, spawns the weapon's hit effect and sets target action=CHASING. Enemy weapons also hit other enemies. Fire-resistant targets take damage/5 with hit_fail.ent and no explosion.
- Weapons and projectiles are temporary particle entities that auto-delete when their particles finish. Enemy swords are about 252 ms hitboxes. A fire_ball lives about 1.6-1.9 s, so its range is about 290-340 px for the impy and 360-430 px for the king or player.
- Particle <SoundEffect> in .ent files (for example sword01.mp3, hit01.ogg, vanish.ogg) must play when the entity is created. The scripts rely on it: setupScene preloads these files but never PlaySamples them. The 2013 engine has no reader for this feature.
- Hazards: instant_death, instant_death2 and instant_death3 set hp=0 on overlap and are the only killers; lava.ent and thorns.ent are visual only. The lava shooter (static) fires fire_shoot every 1000+rand(2500) ms with force 500+randF(400), rising 104-338 px, damage 18, knockback 15, ignoring fire resistance. falling_bridge.ent drops 600 ms after a player touches it and accelerates at 1200 px/s^2.
- Only the 'play_sound' label is processed; play_sound.ent instances never play. 'spawn2' never spawns. Stones labelled dontFall/fontFall/step never fall.
- The checkpoint saves scenes/checkpoint.esc before deleting itself, so it re-triggers on reload. Potions (only potion_small with hp=20 is placed) heal within 20 px, but not at full HP.
- Physics is partly per-frame: knockback decays x0.8 per frame, and displacement is force/fps clamped to 0.4*min(sprite frame size). Use a fixed 60 Hz tick. Snap-to-ground ignores the collision-box offset, giving 4 px of penetration for master_knight and the players.
- switch.as is only a UI radio widget for the options screen. Its g_controls instance decides the joystick mapping: option 0 means player 2 uses joystick 0 and player 1 uses keyboard plus joystick 1.

## Engine gaps

- Audio decoding: Supersonic AudioClip reads only uncompressed RIFF/WAVE (ARCHITECTURE.md:132; README.md:79). Every Penumbra sound is .ogg or .mp3, so they need offline conversion or a decoder.
- Text: the engine has BMFont text (.fnt) through src/core/BitmapFont.hpp, plus ImGui, but no TrueType rasteriser. Ethanon DrawText uses system fonts (Arial Narrow, Arial, Arial Black, Verdana) at arbitrary sizes (15, 16, 17, 25, 30, 40, 50, 60, 256), so those need pre-baked fonts.
- Not checked: whether src/core/ParticleSystem.cpp can reproduce Ethanon particle semantics (staggered release, repeat count, allAtOnce, MirrorX, alphaMode, per-particle randomisation), temporary-entity auto-deletion, and SoundEffect-on-spawn.
- Not checked: 2D normal-mapped per-pixel point lights with halos, and sprite-sheet frame animation (SpriteAnimationSystem exists but was not read).
- Port-side layer needed rather than engine changes: the 256-px bucket grid with centre-based membership, the typed custom-data bag, callbacks bound by entity label with static/dynamic gating, the path-keyed sample API (play/loop/stop/isPlaying/volume) and camera earthquake.
- Coordination: per the user, another Claude session is porting a different game onto the same Supersonic engine. Any engine change must be coordinated with that session; this investigation changed nothing.

## Open questions

- #1 Callback gating in the 2010 machine.exe: do callbacks of dynamic entities run off-screen, and do static ones run only in visible buckets? Evidence: level1's 'play' entity at (3981,-598) suggests static callbacks are visibility-gated. Test on machine.exe: does fase.mp3 start at level1 spawn or only mid-level? Can an off-screen impy's cast_fire_spell.ogg be heard? Port default until answered: the 2013 model (dynamic always active, static only when visible).
- #2 Bucket size for levels loaded without an explicit size: 256x256 is assumed from the 2013 default (_ETH_DEFAULT_BUCKET_SIZE). The level3 king/summon-marker layout is consistent with it. Test: an enemy should ignore a player whose centre is just across a y multiple of 256 (different bucket row) even when very close.
- #3 Particle <SoundEffect> semantics in 2010: does it play exactly once at entity creation at the entity's soundVolume? Does it share a voice or volume with a script PlaySample of the same file (for example hit_fail.ent vs PlaySample of cast_fire_spell.ogg)? Do per-sample volumes persist across LoadScene (help.mp3 set to 0.3 in the menu, fase.mp3 after a fade)?
- enmlFile.getInt with a missing key (lv20, lv31+): does the AngelScript &out temporary leave nextExp unchanged or overwrite it with 0 or garbage? In the second case addToExp could skip lv20 and loop forever from lv31. This is reachable only by farming summons or with the PageUp cheat.
- Is the random generator reseeded every frame in 2010, as the 2013 ETHScene::Update does? This affects reproducing rand/randF sequences exactly.
- Per-frame order of entity callbacks relative to the scene loop (doLoop) in 2010, and the order among entities. This affects when g_numNpcs is counted, same-frame spawn/aggro interactions and message ordering.
- Does GetEntitiesFromBucket reflect entities moved earlier in the same frame, or does bucket membership update only at the end of the frame (2013 ResolveMoveRequests)? This affects collision candidate sets at bucket seams.
- After DeleteEntity in the same frame, do other callbacks' GetEntitiesFromBucket calls still return the deleted entity (for example a potion or checkpoint deleted mid-frame)?
- The exact 2010 behaviour of LoopSample/PlaySample on an already-playing sample (restart or continue), and whether IsSamplePlaying is true for looped music.
- Does machine.exe apply a 250 ms cap on the frame delta for particles or UnitsPerSecond (the 2013 particle code caps motion at 250 ms but UnitsPerSecond is uncapped)?
- Label/index mismatch in g_controls: the option labelled '2º joystick para jogador 2' gives player 2 joystick index 0. Should the port keep the original mapping or relabel it?
