# Penumbra (2010 Ethanon): particles, lighting, shadows and lightmaps, decoded

Scope: `effects/*.par`, `data/*.cg`, `data/shadow.dds`, and the lighting and shadow fields of `.ent`/`.esc`, all from `<Desktop>\Penumbra-and-the-Castle-of-Shadows-Enhanced\extracted\app`. Cross-checked against the Dec 2013 Ethanon source in `<Desktop>\Magic-Portals-Remake\reference\ethanon\toolkit\Source\src` and against the Magic Portals (MP) port.

## 0. Evidence levels and method

- **[2010 primary]** The `.cg` files that ship in `data/` are the 2010 shaders. The engine compiles them at run time: `machine.exe` references `/defaultVS.cg`, `/hPixelLight.cg`, `/vPixelLight.cg`, `/pixelLightVS.cg`, `/hVertexLightShader.cg`, `/vVertexLightShader.cg`, `/defaultStaticAmbientVS.cg`, `/dynaShadowVS.cg`, `/shadow.dds` and `/default_nm.png`, and the entry points `sprite_ppl`, `verticalSprite_ppl`, `mainSpecular`, `sprite_pvl`, `particle` and `vertical`.
- **[2010 binary]** These are strings from `machine.exe` (VC++ 2008, x87, image base 0x400000) and `GameSpace.dll` (image base 0x10000000), plus targeted disassembly with `objdump -d -M intel` (Strawberry mingw objdump). Addresses are cited as `@0x…`.
  - Method note for the other session: MSVC stores float literals such as `0.8f` and `2.2f` widened to double (`0.800000011920929`). A byte scan for the exact double `0.8` misses them. Scan for `struct.pack('<d', float32(x))`, or disassemble.
- **[2013 source]** C++ citations are relative to `toolkit/Source/src/`. Anything tagged **2013 only** is unverified for 2010.

The game is a **side-view 2D platformer** drawn with Ethanon's top-down lighting model:
- The "horizontal" plane is the screen plane.
- `z` points toward the viewer.
- Background walls sit at z = -84 (e.g. `wall00.ent` ×125 at -84).
- Gameplay tiles are at z = 0.
- Torches sit at z ≈ -70..-32, with their light 8 units in front.
- `ZAxisDirection` is (0,0) in every level and (0,-1) only in `menu.esc` and `arena_select.esc`.
- `lightIntensity` = 2 in all 13 scenes.
- The back buffer is 32-bit (`PF32BIT`, `main.as:144`, `menu.as:111`, `videoModes.as:100-110`), so every blend saturates at 8 bits per channel. MP's RGB565 finding does **not** apply.

---

## 1. `effects/*.par`: the particle definition format

### 1.1 Container

- There are 43 `.par` files plus `effects/readme.txt` ("All *.PAR effect files must be in this folder.") and one stray `effects/sky.png`.
- All 43 are **ASCII XML with CRLF**. None has a UTF-16 BOM (MP's corpus was UTF-16LE+BOM). `.ent` and `.esc` files are ASCII too; other agents say their text is Latin-1.
- Shape: `<?xml version="1.0" ?>` → `<Ethanon>` → exactly one `<ParticleSystem …>`, which has 16 or 17 attributes and 9 to 11 child elements. No file has a second system.
- **No `.par` is loaded at run time.** No `.as`, `.esc` or `.ent` names a `.par`: a grep for `.par` finds nothing, and the 2010 API has no `PlayParticleEffect`.
- The `.par` files are the **editor library**. The editor copied a system into an entity's `<Particles>` block, scaled by the editor's scale factor (see §1.6). At run time the game only uses the copies embedded in `.ent`/`.esc`, up to `ETH_MAX_PARTICLE_SYS_PER_ENTITY` = 2 per entity (the 2010 string "ETHRenderEntity::PlayParticleSystem: n > ETH_MAX_PARTICLE_SYS_PER_ENTITY"). Every Penumbra carrier has exactly 1.

### 1.2 Field table: names, meaning, and value ranges across all 43 files

Semantics are the same as MP `docs/ethanon-formats.md` §7.2–7.6, except where §1.4 says otherwise. The 2010 reader's attribute order, from the `machine.exe` string table: `particles allAtOnce alphaMode repeat boundingSphere lifeTime randomLifeTime angleDir randAngle size randomizeSize growth minSize maxSize angleStart randAngleStart animationMode Bitmap SoundEffect Gravity Direction RandomizeDir StartPoint RandStartPoint Color0 Color1 Luminance`. `animationMode` comes last, which suggests it was added late. That fits the two legacy files that lack it.

| Field | Presence | Values seen (value×count) | Meaning (units at 60 fps) |
|---|---|---|---|
| `particles` | 43/43 | 16×9, 12×7, 45×6, 9×5, 19×3, 15×2, 40×2, 4, 5, 13, 14, 17, 18, 23, 29, 70 | fixed pool size (not a rate) |
| `allAtOnce` | 43 | 0×30, 1×13 | 1 = release the whole pool on frame 1 |
| `alphaMode` | 43 | **1×25 (AM_ADD), 0×14 (AM_PIXEL), 4×4 (AM_MODULATE)** | gs2d `GS_ALPHA_MODE`, verified for 2010 (§2.1) |
| `repeat` | 43 | 1×26, 0×14, 36, 5, 7 | lives per particle; 0 = endless |
| `animationMode` | **41** (missing in `explosion_particles.par`, `sword.par`) | 1×41 | 1 PLAY_ANIMATION, 2 PICK_RANDOM_FRAME |
| `boundingSphere` | 43 | 512×21, 354×6, 409.6×4, 64×4, 2036×3, 142×2, 158, 364, 700 | culling diameter; also widens the scene's min/max height |
| `lifeTime` | 43 | 100 … 4500 ms (450×6, 150×5 …) | mean life, ms |
| `randomLifeTime` | 43 | 0 … 3300 ms | full peak-to-peak spread |
| `angleDir` | 43 | 0×36, 7.4×3, -10.6, 0.7, 1.4, 3.4 | deg/frame |
| `randAngle` | 43 | 0×21, 1×6, 20×4, 0.5×3, 8.1×3, 3.9×2, 11.7, 2.4, 3, 3.1 | full spread |
| `size` | 43 | 4 … 345 px (168×6, 38×5, 20×4, 66.4×4 …) | square quad side, px |
| `randomizeSize` | 43 | 0×31, 64×6, 256×3, 25×2, 4 | full spread |
| `growth` | 43 | -6 … 3.4 | px/frame |
| `minSize` / `maxSize` | 43 | min 0×38, 2×4, 1; max 1000×38, 800×4, 62 | clamps |
| `angleStart` / `randAngleStart` | 43 | start 0×41, 349, 70; rand 360×20, 0×19, 235×2, 246, 254 | deg; rand is asymmetric U(0,max) |
| `<SoundEffect>` | **21/43** | explosion.ogg×3, hit01.ogg×3, sword01.mp3×7, brige_fall.ogg, cast_fire_spell.ogg, checkpoint.mp3, creature_dying.mp3, dark_hit.ogg, fall.mp3, potion_pick.ogg, vanish.ogg | **live in 2010**, see §1.4 |
| `<Bitmap>` | 43 | explosion.JPG×8, sword.png×8, fog.dds×7, explosion.png×6, black_opaque.png×4, flash.bmp×3, particle_harder.png×3, particle.png×2, black_sword.dds, blood.png | file in `particles/` |
| `<Gravity x y>` | 43 | x: 0×34, -3.04×4, -3.8×4, -2.6; y: 0×40, -0.04, -0.1, 0.11 | px/frame² |
| `<Direction x y>` | 43 | x -2.2 … 15.7; y -3.1 … 2.7 | px/frame |
| `<RandomizeDir x y>` | 43 | 0 … 20 | full width, per axis |
| `<SpriteCut x y>` | **41** (missing in the same 2 files) | 1×1 ×35, 4×3 ×6 (explosion.png) | columns × rows |
| `<StartPoint x y z>` | 43 | x 0×36, -13×4, -500×3; y 0×40, -89×3; **z always 0** | emitter offset |
| `<RandStartPoint x y>` | 43 | x 0…63, y 0…600 | full width |
| `<Color0 r g b a>` / `<Color1 …>` | 43 | all channels in [0,1] (none > 1, so the 2013 highlight-PS path never triggers) | birth and death colour, linear lerp |
| `<Luminance r g b>` | 43 | (1,1,1)×36, **(0,0,0)×7** | emissive floor for AM_PIXEL/AM_ALPHA_TEST |

Luminance 0 appears in: `bridgefall` (PIXEL), `checkpoint` (PIXEL), `life_shadow_beam` and `life_shadow_beam_bigger` (PIXEL), `lava`, `lava_drops` and `nextlv` (ADD, where it has no effect).

Per-file attribute table:

|file|particles|allAtOnce|alphaMode|repeat|anim|bSphere|life|randLife|angleDir|randAngle|size|randSize|growth|min|max|angStart|randAngStart|
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
|big_explosion|12|1|1|1|1|354|600|500|0|1|168|64|3.4|0|1000|0|360|
|blood|45|1|0|1|1|512|550|0|0|0|25|25|-1.3|0|1000|0|0|
|bridgefall|9|1|0|1|1|512|700|450|1.4|3|5|0|1.6|0|1000|0|360|
|checkpoint|5|0|0|1|1|512|1050|550|0|0|345|0|-6|2|1000|0|0|
|clouds|70|0|0|0|1|2036|4500|3300|0|0.5|256|256|0.05|0|1000|0|360|
|combo_fire_ball|23|0|1|7|1|64|250|150|0|20|20|0|-1.5|2|1000|0|360|
|enemy_dark_sword|16|0|0|1|1|512|130|0|0|0|71|0|1.5|0|1000|0|0|
|enemy_sword|16|0|1|1|1|512|130|0|0|0|71|0|1.5|0|1000|0|0|
|enemy_sword_beam|9|0|0|1|1|512|200|100|7.4|8.1|31|0|0.8|0|1000|0|360|
|enemy_sword_hit|12|1|1|1|1|354|450|400|0|1|168|64|3.4|0|1000|0|360|
|explosion_particles|13|1|1|1|-|700|1150|1000|0|3.1|18|0|-0.35|1|1000|0|360|
|fade_out_shadow_beam|45|0|**4**|1|1|512|1750|1300|0|0|38|0|-0.5|0|1000|0|0|
|fade_out_shadow_beam_hard|45|0|0|1|1|512|1750|1300|0|0|38|0|-0.5|0|1000|0|0|
|fall|9|1|0|1|1|512|200|100|7.4|8.1|18|0|1|0|1000|0|360|
|fire_shoot|19|0|1|0|1|64|250|150|0|20|20|0|-1.5|2|1000|0|360|
|fireball|19|0|1|5|1|64|250|150|0|20|20|0|-1.5|2|1000|0|360|
|fog|40|0|0|0|1|2036|4500|3300|0|0.5|341|256|0.05|0|1000|0|360|
|fog_menu|40|0|0|0|1|2036|4500|3300|0|0.5|341|256|0.05|0|1000|0|360|
|heavy_sword|16|0|1|1|1|512|150|0|0|0|83|0|1.5|0|1000|0|0|
|hit_fail|4|1|1|1|1|354|250|200|0|1|168|64|3.4|0|1000|0|360|
|jumpfx|9|1|0|1|1|512|200|100|7.4|8.1|18|0|1|0|1000|0|360|
|king_beam|19|0|1|0|1|142|1250|850|0|3.9|38|0|-0.5|0|1000|0|246|
|lava|14|0|1|0|1|158|1200|1000|0.7|2.4|4|0|3.1|0|62|0|360|
|lava_drops|9|1|1|1|1|512|950|550|0|0|10|4|-0.2|0|1000|0|0|
|life_shadow_beam|12|0|0|0|1|512|450|350|0|0|38|0|-1.3|0|1000|0|0|
|life_shadow_beam_bigger|12|0|0|0|1|512|450|350|0|0|40|0|-1.3|0|1000|0|0|
|light_spell|15|0|1|36|1|512|600|500|0|0|44|0|-1.2|0|1000|0|235|
|light_spell_infinite|15|0|1|0|1|512|600|500|0|0|44|0|-1.2|0|1000|0|235|
|nextlv|17|0|1|0|1|512|750|500|0|11.7|53|0|-1.3|0|1000|0|254|
|paladin_sword|16|0|1|1|1|512|130|0|0|0|71|0|1.5|0|1000|0|0|
|portal_red|29|0|0|0|1|364|450|350|0|3.9|33|0|0|0|1000|0|360|
|potion_pick|12|1|1|1|1|354|450|400|0|1|168|64|3.4|0|1000|0|360|
|shadow_beam|45|0|**4**|0|1|142|1250|850|0|0|38|0|-0.5|0|1000|0|0|
|shadow_beam_wide|45|0|**4**|0|1|512|1300|850|0|0|43|0|-0.5|0|1000|0|0|
|small_explosion|12|1|1|1|1|354|600|500|0|1|168|64|3.4|0|1000|0|360|
|sword|16|0|1|1|-|512|100|0|0|0|83|0|1.5|0|1000|0|0|
|sword01|16|0|1|1|1|409.6|150|0|0|0|66.4|0|1.2|0|800|0|0|
|sword02|16|0|1|1|1|409.6|150|0|0|0|66.4|0|1.2|0|800|0|0|
|sword03.ent|16|0|1|1|1|409.6|150|0|-10.6|0|66.4|0|1.2|0|800|70|0|
|sword04|16|0|1|1|1|409.6|150|0|3.4|0|66.4|0|1.2|0|800|349|0|
|sword_beam|45|1|**4**|1|1|512|550|0|0|0|25|25|-1.3|0|1000|0|0|
|sword_hit|12|1|1|1|1|354|450|400|0|1|168|64|3.4|0|1000|0|360|
|torch_fire|18|0|1|0|1|64|850|700|0|20|20|0|-0.5|0|1000|0|360|

The child values per file are in the census script `reference/analysis/par.py` (session scratch, not in the repo). The ranges above are exact.

### 1.3 Particle bitmaps (`particles/`)

| bitmap | format | notes |
|---|---|---|
| explosion.JPG | 211×211 RGB, no alpha | AM_ADD fire (torch, fireball, lava) |
| explosion.png | 256×192 RGB, cut 4×3 → 64×64 frames | AM_ADD explosions and hits |
| fog.dds | 256×256 RGBA, A max 192 | AM_PIXEL fog, clouds, dust puffs |
| black_opaque.png | 48×48 paletted, black blob on white, no alpha | **AM_MODULATE** dark smoke: white multiplies by 1, black darkens |
| particle.png / particle_harder.png | 32×32 RGBA white blobs | AM_PIXEL, tinted by Color |
| sword.png | 88×91 L8 white on black | AM_ADD sword trails |
| black_sword.dds | 64×64 RGBA black with alpha | AM_PIXEL dark sword trail |
| flash.bmp | 265×253 L8 | AM_ADD; non-square, so it is stretched onto a square quad (≈4.7% squeeze) |
| blood.png | 41×41 paletted with alpha | AM_PIXEL portal and blood |
| unused | black.bmp, dust.png, particle.bmp, white.bmp, black_sword.png | |

### 1.4 Compared with Magic Portals: same schema, older variant

**Same.**
- The 17 attributes and 10 children, their names and meanings.
- Release scheduling: `releaseTime = (lifeTime + randomLifeTime) * id / nParticles`.
- Birth randomisation: half-widths for everything except `randAngleStart`.
- Colour lerp, sprite-sheet animation, square quads, scaled fields, and mirroring.
- The Luminance rule, verified in the 2010 binary `@0x41f3fd–0x41f477`: if `alphaMode == 0 || alphaMode == 2`, then `finalAmbient = min(emissive + sceneAmbient, 1)` per channel; otherwise (1,1,1). The colour is then converted ×255 to bytes.
- The vertical-owner depth shift of 10. The 2010 binary `@0x41f700–0x41f717` computes `(startPoint.y - pos.y) + 10.0 + z`, which is the 2013 `INDIVIDUAL_OFFSET` with `_ETH_PARTICLE_DEPTH_SHIFT` = 10.

**Different in 2010.**
1. **`<SoundEffect>` is live.** 21 `.par` files and 18 `.ent` embeds carry one.
   - The 2010 reader lists `SoundEffect`. The particle-load function `@0x421e90–0x422550` resolves the bitmap under `particles\` (`@0x421f93`) and the sound under `soundfx\` (`@0x422318`).
   - The entity has `bool HasSoundEffect() const` and a `soundVolume` attribute. `silent_sword.ent` has `soundVolume="0"`; other values are 0.2, 0.3, 0.5, 0.7 and 1.
   - The 2013 source removed both `SoundEffect` and `soundVolume`, and MP documents the child as dead data.
   - The port must play the embedded `SoundEffect` at the entity's `soundVolume` when the system starts. The exact trigger is an open question.
2. **Encoding:** ASCII/CRLF, not UTF-16LE+BOM.
3. **Legacy files:** `explosion_particles.par` and `sword.par` lack `animationMode` and `<SpriteCut>`. Defaults apply: animationMode 1 and cut (1,1). Neither file is embedded anywhere.
4. **Frame-speed cap.** The 2010 `ETHParticleManager` update at `@0x41e370` measures its own delta: `dt = (now - m_lastTime)/1000.0` (the double 1000 at `0x4d1220`). It computes `frameSpeed = min(dt*60.0, 2.0)` (60.0 at `0x4d12e8`, 2.0f at `0x4d1228`), so motion is capped at **2 frame-units, about 33.3 ms**. 2013 caps at 250 ms, or 15 units (`Particles/ETHParticleManager.cpp:195-196`), and the MP port copies 250 (`game/sim/Particles.cpp:315`).
   - Particle age is `now - particle.startTime` (`@0x41e5b3–0x41e5cd`), an uncapped absolute clock. That is equivalent to 2013's uncapped `elapsed +=`.
   - At 60 fps both versions give identical motion. Below 30 fps, 2010 particles slow down while still ageing in real time.
5. **Entity scale is always 1 at run time.** There is no `<Scale>` element in the 2010 entity schema (no "Scale" string among the XML names). The editor scale was baked into the embedded copy (§1.6). `ScaleParticleSystem` exists in the API but scripts never call it.
6. Script-used particle API: `MirrorParticleSystemX(0, true)` at `swords.as:85-86` and `doDamage.as:228` (swords and beams facing left), `KillParticleSystem(0)` at death (`controlCharacters.as:379,422`), and `HasParticleSystem(0)`.

**What MP's reader (`game/sim/Particles.cpp`) would do with Penumbra data.**
- `ReadUtf16` (`:22-48`) rejects every Penumbra `.ent` and `.par`, because none starts with `FF FE`.
- `Parse` (`:245-281`) looks for `<Particles>…</Particles>`. A standalone `.par` has no wrapper, so it would return **zero systems silently**. Embedded `.ent` blocks do have the wrapper.
- `ParseSystem` (`:93-230`) demands `animationMode` and `<SpriteCut>`. It would refuse the two legacy files, which are unused anyway.
- It tolerates `<SoundEffect>`, because it searches substrings, but ignores it.
- `Drawable` (`:283-285`) refuses alphaMode 4. That drops every AM_MODULATE aura.
- `Step` uses the 250 ms cap.

### 1.5 Blend, colour and depth of a particle (2010 plus 2013)

- Vertex shader: `defaultVS.cg:particle` (lines 122-150). It outputs `oColor = color0`, the per-particle colour, and `z = 1 - depth`. Its `ambientLight`, `lightPos`, `lightColor` and `lightRange` uniforms are unused; that code is commented out at `:138-147`. There is no pixel shader: fixed function computes `tex × vertexColour`.
- Blend by `alphaMode`. Verified 2010, `GameSpace.dll` `SetAlphaMode` `@0x10006170`, jump table `@0x10006448`:
  - 0 = SrcAlpha/InvSrcAlpha
  - 1 = One/One
  - 2 = no blend
  - 3 = no blend and no test
  - 4 = Zero/SrcColor
  - Every mode except 3 alpha-tests `alpha > 1/255` (ALPHAREF 1, GREATER).
  - Stage 0 is `tex × diffuse` for both colour and alpha.
- Colour: `particle.color × (alphaMode∈{0,2} ? min(Luminance + sceneAmbient, 1) : 1)` (2010 verified).
  - Under AM_ADD, alpha only feeds the alpha test, so RGB is not faded by alpha. Of the 25 AM_ADD systems, those that fade do so through RGB → 0.
  - Under AM_MODULATE: `dst = dst × tex.rgb × color.rgb`.
- Placement (2013, `Particles/ETHParticleManager.cpp:337-417`, `Entity/ETHSpriteEntity.cpp:571-578`):
  - Emitter = `ToScreenPos(entityPos, zAxisDir)`.
  - Particle drawn at `pos + zAxisDir × startPoint.z`, angle `particle.angle`, quad `size×size`, origin centre.
  - Depth:
    - horizontal owner: `ComputeDepth(entityZ + startPoint.z)`
    - vertical owner (type 2): `ComputeDepth(entityZ + startPoint.z + (startY - posY) + 10)` per particle
    - layerable owner (type 5): the owner's depth, i.e. `max(layerDepth, 0.001)`
- Consequences for Penumbra:
  - `clouds.ent` and `dawn.ent` (type 5, layerDepth 0) draw at depth 0.001, behind everything.
  - `fog.ent` (type 5, layerDepth 1) draws in front of everything.
  - Torch flames have StartPoint z = 8, so they draw 8 units in front of the torch.
- 2013 only: an AM_ADD bitmap is loaded with a black colour key (`cutOutBlackPixels`, `ETHParticleManager.cpp:107-108`). This has no visible effect under One/One.

### 1.6 What each `.par` is used for

The embedded copies are **scaled** by `ETHParticleSystem::Scale`, which multiplies the 11 length fields: boundingSphere, Gravity, Direction, RandomizeDir, StartPoint, RandStartPoint, size, randomizeSize, growth, minSize and maxSize. For example, `torch.ent` = `torch_fire.par` ×1.2: size 20→24, bSphere 64→76.8, growth -0.5→-0.6, maxSize 1000→1200, Gravity.y -0.04→-0.048, RandomizeDir (0.3,0.8)→(0.36,0.96), RandStartPoint.x 3→3.6. The start point was then hand-edited to (0,-8,8).

Matching was done by bitmap and field-wise comparison after scaling. "Exact" means every field matches after scaling.

| .par | embedded in (scale; edits) | how it appears in game |
|---|---|---|
| big_explosion | big_explosion.ent (×1, exact) | combo-fireball impact, `spells.as:120` (manageFireBall → `spells.as:141`) |
| blood | — **orphan** | — |
| bridgefall | bridge_fall.ent (×1) | `events.as:97` |
| checkpoint | checkpoint_effect.ent (×1), summon.ent (×1) | `main.as:187` checkpoint; summons at `controlCharacters.as:634,723`, `events.as:56` |
| clouds | clouds.ent (StartPoint (-300,0,0) vs (-500,-89,0); bSphere 2000 vs 2036) | environment spawn `setupScene.as:196-210` (level2, level3, pvp_lv3, pvp_lv4) and placed in gameover.esc; `environment.as` lightning |
| combo_fire_ball | combo_fire_ball.ent (×2.15) | `playerInput.as:384` |
| enemy_dark_sword | dark_sword.ent (StartPoint.x -10) | master_knight melee, `controlCharacters.as:584` |
| enemy_sword | enemy_sword.ent (×0.64; StartPoint.x -10) | warrior/minion/knight melee, `controlCharacters.as:488,512,536` |
| enemy_sword_beam | enemy_sword_beam.ent (×1) | `characterAI.as:188` |
| enemy_sword_hit | blood.ent (×0.2816) | `hit` CustomData of dark_sword, enemy_sword and paladin_sword → `doDamage.as:225` (enemy hits player) |
| explosion_particles | — **orphan**, legacy | — |
| fade_out_shadow_beam (AM_MODULATE) | fade_out_beam.ent (×1) | death effect, `controlCharacters.as:289,326,328,485…` |
| fade_out_shadow_beam_hard | fade_out_beam_large.ent (×1.8) | master_knight death, `controlCharacters.as:581` |
| fall | fall.ent and jumpfx.ent (×1). **Tie**: `jumpfx.par` is identical except it has no `<SoundEffect>` | `controlCharacters.as:161` (fall), `:118` (jump) |
| fire_shoot | fire_shoot.ent (×1) | `lavaShooter.as:54` |
| fireball | fire_ball.ent (×1) | `playerInput.as:428`; impy `controlCharacters.as:560`; paladin `:662` |
| fog | fog.ent (StartPoint.x -272) | environment spawn in pvp_lv6 |
| fog_menu | fog_menu.ent (×1) | menu.esc |
| heavy_sword | — **orphan** | — |
| hit_fail | hit_fail.ent (×0.192) | `doDamage.as:234` |
| jumpfx | tie with fall.par | see fall |
| king_beam | king.ent (×0.72; StartPoint.z -4); vert_king.ent (×0.72; StartPoint.y -18) | king boss, `events.as:58`; the `vert_*` entities are never used |
| lava | shooter.ent (StartPoint.z -6) | placed: level1 ×9, level3 ×5, pvp_lv2 ×4 |
| lava_drops | lava_drops.ent (×1) | `lavaShooter.as:126` |
| life_shadow_beam | potion_small.ent (StartPoint.z -2) | placed: level1 ×13, level2 ×15, level3 ×26 |
| life_shadow_beam_bigger | potion_large.ent (StartPoint.z -2) | entity never placed or spawned |
| light_spell | light_spell.ent (×0.8) | `playerInput.as:413` |
| light_spell_infinite | cursor.ent (×0.6) | menu.esc, arena_select.esc |
| nextlv | nextlv.ent (×1) | entity never referenced |
| paladin_sword | paladin_sword.ent (StartPoint.x -18) | `controlCharacters.as:608,662` |
| portal_red | inline `next_level` (level1, level2), `decoration` (level2), `deco` ×2 (level3): ×0.96, StartPoint (0,70,-1) | level-exit portals |
| potion_pick | potion_pick.ent (×0.24) | `potions.as:57` |
| shadow_beam (AM_MODULATE) | bruxo.ent, princess.ent, bruxo_dead.ent (StartPoint (0,-1,-2), bSphere 512); minion.ent (×0.6, z -4); vert_bruxo.ent | **player auras** (`constants.as:87-88`); `gameover.as:58`; minion enemies |
| shadow_beam_wide (AM_MODULATE) | checkpoint.ent (×1.44, z -4); master_knight.ent (×1.536, StartPoint (0,20,-2)); vert_master_knight | checkpoints (placed 3/4/3 in level1/2/3); master_knight |
| small_explosion | — **orphan** | — |
| sword | — **orphan**, legacy | — |
| sword01 | sword1.ent and silent_sword.ent (exact); sword0.ent (recoloured: Color0 (0.5,1,1)…); combo_sword.ent (×1.4, recoloured) | player sword `playerInput.as:402` ("sword"+player+".ent"), combo `:367`; spawned at +z 8, `swords.as:81` |
| sword02, sword03.ent, sword04 | — **orphans** (sword01 variants) | — |
| sword_beam (AM_MODULATE) | sword_beam.ent (×1) | `swords.as:82` |
| sword_hit | enemy_blood.ent (×0.24), explosion.ent (×0.576) | `hit` of player swords → `doDamage.as:225`; fireball impact `spells.as:115`; lava shooter `lavaShooter.as:116` |
| torch_fire | torch.ent, torch_higher_range.ent, foggy_torch.ent (×1.2; StartPoint (0,-8,8)); ground_fire.ent (×1; StartPoint (0,6,20)) | torches placed in levels (10/1/9), pvp_lv1/2/6; menu |

Placed particle carriers per scene:
- level1: checkpoint 3, next_level 1, potion_small 13, shooter 9, torch 10
- level2: checkpoint 4, decoration 1, next_level 1, potion_small 15, torch 1
- level3: checkpoint 3, deco 2, potion_small 26, shooter 5, torch 9
- menu: cursor 1, fog_menu 1, ground_fire 3
- arena_select: cursor 1, ground_fire 1
- gameover: clouds 1
- pvp_lv1: torch 1
- pvp_lv2: shooter 4, torch_higher_range 3
- pvp_lv6: foggy_torch 4
- videoModes: picker 1 (flash.bmp)

Every placement's inline copy equals its `.ent`'s copy.

---

## 2. The 2010 rendering and lighting model

### 2.1 Entity types, origin, vertex transform and depth

- **`type` enum (verified 2010).** 2010 `ETHSpriteEntity::ComputeDepth` `@0x4230f6` switches over 6 cases (jump table `@0x42319c`):
  - 0 and 2 → `(z - minH)/(maxH - minH)`
  - 1 and 4 → `(z + 0.1 - minH)/(maxH - minH)` (`ETH_SMALL_NUMBER` 0.1)
  - 3 → 1.0
  - 5 → `max(layerDepth, 0.001)`
  - This is the 2013 enum (`Entity/ETHEntityProperties.h:82-90`): **0 ET_HORIZONTAL, 1 ET_GROUND_DECAL, 2 ET_VERTICAL, 3 ET_OVERALL, 4 ET_OPAQUE_DECAL, 5 ET_LAYERABLE**.
  - Penumbra uses 0 (2492 of 2751 definitions), 5 (218: black, clouds, dawn, fog, instant_death, instant_death3) and 2 (41: barrel, cursor, devil, fog_menu, gamelogo, ground_fire, pilar, thumbnail, vert_bruxo, vert_king, vert_master_knight).
  - 2010 `BeginLightPass` and `DrawShadow` test `type == 2` for vertical (`cmp [ent+0x4c],2`).
- **Origin** (2013 `Entity/ETHEntity.cpp:26-44, 270-296`): types 0, 1, 3, 4 and 5 are centred, `center = size/2`; type 2 is centre-bottom, `center = (w/2, h)`. Both add `+ pivotAdjust`. Pivot values seen: (0,-14) barril, (0,-8) torch_small, (0,-13) devil_statue, otherwise 0.
- **Vertex transform** (every VS, e.g. `defaultStaticAmbientVS.cg:75-92`):
  - `q = R · (pos01·size - center) + entityPos - screenSize/2 - cameraPos`
  - `q.y = -q.y`
  - `clip = ortho(2/w, 2/h) · q`, then `z = 1 - depth` (overwritten after the matrix)
  - `entityPos = ToScreenPos(worldPos) = (x, y) + ZAxisDirection · z` (2013 `Util/ETHASUtil.cpp:123-126`). In levels, z has no screen effect.
  - The texture UV is `pos01·rectSize/bitmapSize + rectPos/bitmapSize`.
  - The 2010 ambient VS ignores `flipAdd`/`flipMul` even though it declares them.
- **Depth buffer** (2013 `Scene/ETHScene.cpp:499-514`; `gs2d/src/Video/Direct3D9/D3D9Video.cpp:543`):
  - z-test and z-write are on, with `LESSEQUAL`, so a larger world z is in front.
  - Blending and alpha test: all alpha modes except AM_NONE discard `alpha ≤ 1/255` (verified 2010 above).
  - `minH` starts at 0 and `maxH` at the screen height. They widen with every entity: `z ± size.y`, `z + startPoint.z ± 2·boundingSphere`, and the halo light z (2013 `Scene/ETHScene.cpp:95-100, 279-280`; `Entity/ETHSpriteEntity.cpp:462-490`). Clouds and fog, with bSphere 2000/2036, push the span to about ±4000.
- **Vertical depth.**
  - Formula, from `defaultStaticAmbientVS.cg:132`, `pixelLightVS.cg:129` and `vVertexLightShader.cg:115`: `z = (1 - depth) - (1 - v)·rectSize.y / spaceLength`, with `spaceLength = maxH - minH`. That equals `1 - ComputeDepth(z + (1 - v)·rectSize.y)`.
  - So each row of a vertical sprite has depth **z + its height above the sprite's bottom edge, in UNSCALED texture-rect pixels**. The 3D lighting position uses the scaled `size`; scale is always 1 in Penumbra.
- **Draw order.** 2013 only (`Renderer/ETHEntityRenderingManager.cpp:34-144`): one multimap of pieces keyed by `drawHash`. Pieces are the sprite (ambient pass, then per-light light and shadow passes), the halo, and each particle system.
  - horizontal: `depth · screenH · 100`
  - vertical: `+ (y - camY)·100/(screenH·100) + 0.1`
  - decals: `+ 0.1`
  - The 2010 ordering was not verified. The z-buffer makes most results order-independent, except where translucent pieces overlap.

### 2.2 Ambient pass (every sprite)

The VS is `defaultStaticAmbientVS.cg:main` (types 0, 5) or `:vertical` (type 2). There is no pixel shader; the fixed-function stages are:

```
stage0 = saturate(T · Cv)                  Cv = ( min(1, A + E).rgb , 1 ) · C      (per channel)
stage1 = saturate(stage0.rgb + LM)         only if entity static && lightmap exists && lightmaps enabled
blend  = entity blendMode (0: SrcAlpha/InvSrcAlpha + test; 2: opaque + alpha test >1/255)
```

- T is the sprite texel.
- A is the scene `<Ambient>`, which scripts may change: `environment.as:105,111` lightning and `gameover.as:56`.
- E is `<EmissiveColor>` rgb; its alpha is ignored.
- C is the instance colour, `<Color>` on the placement. All placements are (1,1,1,1); scripts call `SetColor(0,0,0)` on death (`controlCharacters.as:381,424`).
- LM is the lightmap. It is sampled with **raw 0..1 quad UVs** (`oTexCoord1 = texCoord`, `defaultStaticAmbientVS.cg:119,136`), not the frame-rect UVs.
- Sources: 2013 `Entity/ETHRenderEntity.cpp:68-135` (`BM_ADD` on stage 1 = `D3DTOP_ADD`, `gs2d/src/Video/Direct3D9/D3D9Video.cpp:1172-1179`). The 2010 `GameSpace.dll` exports `SetBlendMode(uint, GS_BLEND_MODE)` and `GS_SPRITE::SetAsTexture(uint)`.
- Vertical sprites are drawn at angle 0 (2013 `:105`).
- Emissive census: (0,0,0) ×2564; (1,1,1) ×124 (crystals, lava, flashlight, mario_bg, gamelogo…); 0.5 ×56; 0.8 ×7.
- `applyLight=0` sprites still get `min(1, A + E)`. Trees, dirt, thorns, black and statue have E = 0, so they render **at ambient only** (very dark: ambient is (0.2, 0.1, 0.2) in level1).

### 2.3 Per-pixel light pass (default; `UsePixelShaders(true)` at `main.as:103-104`)

- One additive pass per (receiver, light).
- Blend AM_ADD = One/One with alpha test `>1/255`. The 8-bit render target clamps each pass's output to [0,1], so a negative N·L becomes 0. There is **no** `saturate` in the shaders.
- VS `pixelLightVS.cg` (`sprite_ppl :99-115`, `verticalSprite_ppl :118-135`) gives the pixel's 3D position:

```
horizontal: P = topLeft3DPos + (u·w, v·h, 0),      topLeft3DPos = entityPos3D - (ox, oy, 0)
vertical:   P = topLeft3DPos + (u·w, 0, -v·h),     topLeft3DPos = entityPos3D - (ox, 0, -oy)
```

- Here (u,v) ∈ [0,1] is the quad coordinate, (w,h) the scaled size and (ox,oy) the absolute origin, from 2013 `Shader/ETHPixelLightDiffuseSpecular.cpp:106-116`. A vertical sprite therefore stands upright in the XZ plane at y = entity y, with its bottom at z = entity z and its top at z + h.
- This uses world coordinates. `ZAxisDirection` does not enter the lighting.

**Normal decoding.** From `hPixelLight.cg:69,113` and `vPixelLight.cg:67-69,111-113`:

```
n  = -normalize(2·(nm.rgb - 0.5))          // RENORMALISED
nv = (n.x, n.z, -n.y)                      // vertical only
```

- Equivalently, with `N = normalize(2·nm - 1)` and `dot(normalize(P - L), n) = dot(normalize(L - P), N)`, the normal map is in **world space**: +x right, +y **down the screen**, +z toward the viewer. Green therefore points toward increasing texture v, the DirectX "Y-down" convention.
- For vertical sprites the world normal is `(N.x, N.z, -N.y)`. A flat map faces +y (down-screen), so a vertical sprite is lit only by lights whose y is below its base line.
- Missing `<Normal>` uses `data/default_nm.png`, an 8×8 image of (127,127,255).
- `rotMatrix` is passed in 2010 but its use is commented out (`hPixelLight.cg:70`). A rotated horizontal entity instead has the light position rotated by -angle about the entity (2013 `ETHPixelLightDiffuseSpecular.cpp:119-128`; there is a matching branch in 2010 `@0x43a577`). All Penumbra placements have angle 0.

**Diffuse, no gloss** (`hPixelLight.cg:main 49-81`, `vPixelLight.cg:main 48-80`):

```
Lv    = P - L
d     = dot(normalize(Lv), n)                          (n or nv)
att   = 1 - |Lv|² / max(|Lv|², r²)          = max(0, 1 - dist²/r²),   dist is 3-D
h:  out = T · C · d · att · Lc · LI · T.a            (rgb AND alpha; alpha therefore has T.a²)
v:  out = T · C · d · att · Lc · LI                  (not alpha-weighted)
```

- `Lc` is `(light.color, 1)`. `LI` is the scene `lightIntensity` (= 2).
- Verified 2010 at `BeginLightPass` `@0x43a1b0–0x43a7c7`: `lightColor` is set from the raw light colour (light struct +0x10), `lightIntensity` is a separate uniform (the last one set, `@0x43a79f`), and `lightRange` comes from light +0x1c.
- 2013 folds `LI` into `lightColor` (`ETHPixelLightDiffuseSpecular.cpp:146`).

**Diffuse plus specular** (`mainSpecular`, used when `<Gloss>` is present: 253 definitions; files gray.bmp ×65, white.bmp ×68, white_ground.jpg ×111, menu_buttons_gloss.png ×8, paladin_gloss.png ×1):

```
Hv   = normalize( normalize(P - L) + normalize(P - Eye) )
spec = Lc · pow( saturate(dot(n, Hv)), specularPower )           // == Blinn with N and (L̂+Ê)
G    = gloss(uv) · specularBrightness
out  = ( T·C·d·Lc·LI  +  spec · T.a · G ) · att
```

- `hPixelLight.cg:83-127` and `vPixelLight.cg:82-126`.
- In 2010 the **specular term is NOT multiplied by LI**, because `Lc` is the raw colour. In 2013 it is.
- The diffuse term of `mainSpecular` is not alpha-weighted, even in `h`.
- `specularPower` values: 50 ×2591, 60, 71, 100, 20, 30, 61. `specularBrightness` is always 1.
- `Eye`: `fakeEyePos` is set in 2010 (`@0x439713`). From 2013 `Shader/ETHFakeEyePositionManager.cpp:26-61`, with 768.0f present twice in the 2010 `.rdata`:
  - real time: `Eye = (L.x, 2·camY + 1.5·screenH - L.y, 768)`
  - lightmap bake (camera 0, `drawToTarget`): `Eye = (L.x, 1.5·screenH, 768)` **in the bake's moved frame**, where the receiver's origin sits at (0, 0, 0) and every light is shifted by the same offset (§3.2 step 2). In the world that is `Eye = (L.x, top + 1.5·screenH, R.z + 768)`, with `top = R.y - (centre + pivotAdjust).y` - the receiver's top edge with no ZAxisDirection shift and no rounding - so the baked highlight is fixed to the receiver and never moves with the camera (0.7.12 `ETHShaderManager.cpp:76-90`, `ETHScene.cpp:561-589`). Verified in the port (render/Lighting.cpp `LightmapBakeEye`): the menu's left devil sees its fire's highlight from (110, 1312, 768).
  - Both are rotated by -angle about the light for rotated entities.

**Which lights hit which sprite.** Structure from 2013 `Renderer/ETHEntityRenderingManager.cpp:110-116, 161-183` and `Renderer/ETHEntitySpriteRenderer.cpp:61-101`; the checks are confirmed in the 2010 binary.

- Lights come only from owners in **visible buckets**. Campaign levels call `SetBorderBucketsDrawing(false)` (`setupScene.as:167`); PvP turns it back on (`setupScene.as:251`).
- Each light is built as `pos = owner.pos + light.pos`. In 2013 a non-static owner has its colour multiplied by `activeParticles/totalParticles` of its system in slot 0 (`ETHSpriteEntity.cpp:595-609`), which makes spell lights flicker.
- A light with colour (0,0,0) is dropped. A dynamic light whose sphere is off screen is dropped.
- For each receiver, the pass is **skipped** when receiver static, light static and lightmaps enabled. Static lights on static sprites exist only in the lightmap.
- 2010 `BeginLightPass` `@0x43a1f3–0x43a2a0` also requires:
  - `light.active`
  - `receiver.applyLight` (entity byte +0x4a)
  - `|entityPos - lightPos|² ≤ (range + max(w,h))²`
- Each pass is scissored to a square of side 2·range around the light (2013 `Entity/ETHLight.cpp:95-104`).

### 2.4 Vertex-light fallback (menu option "Desativa pixel shaders", `videoModes.as:43,126`; `setupScene.as:117-118`)

- VS `hVertexLightShader.cg:sprite_pvl 104-132` and `vVertexLightShader.cg:105-133`, with no pixel shader.
- Per vertex: `Cv = C · dot(normalize(V - L), n0) · att · Lc · LI`, with n0 = (0,0,-1) for h and (0,-1,0) for v. `V` is the corner's 3D position.
- The colour is clamped per vertex and interpolated. Fixed function computes `T·Cv` with One/One.
- No normal map, no specular.

### 2.5 Halos

2013 only: `Entity/ETHRenderEntity.cpp:354-388`, `Shader/ETHShaderManager.cpp:195-213`, `Renderer/ETHEntityRenderingManager.cpp:78-86`.

- A halo is drawn only if `<HaloBitmap>` is set. It uses AM_ADD (One/One) and `defaultVS:main`.
- The quad is centred at `ToScreenPos(entity.pos + light.pos)`, sized `haloSize × haloSize` (× scale = 1).
- Colour: `(light.color, 1) × haloBrightness × ratio`, where `ratio` is `active/total` particles of system slot 0. For halos this applies **even to static owners** such as torches.
- Depth: `ComputeDepth(entity.z + (vertical ? size.y : 0))`.
- Bitmaps are in `entities/`: `halo.bmp` 265×253 L8 (non-square, stretched), `halo1.bmp` 123×123, `halo2.bmp` 123×123.
- Torch: colour (1, 0.7, 0.3) × 0.5, size 144, `halo1.bmp`.
- The 2010 API has `SetHaloRotation(bool)`; no script calls it and its behaviour is unknown.

### 2.6 Dynamic projected shadows (`dynaShadowVS.cg` plus `shadow.dds`)

The C++ that feeds the shader is 2013 `Entity/ETHRenderEntity.cpp:210-352` (`DrawProjShadow`). The 2010 `@0x423aa0–0x4241fc` has the same structure and constants: 0.8 and 2.2 widened to double at `0x4d12d8`/`0x4d12d0`, 8.0f/2.0f, the 0.001 depth bias, `/3` ambient average (`0x4d12b8`), ×255, the `alpha < 8` cutoff, origin (0.5f, 0.79f) at `0x4d12ac`/`0x4d12b0`, and rect mode 1.

For a caster E (position `pe`, current size `(w,h)`, properties `shadowScale`, `shadowLengthScale`, `shadowOpacity`) and a light L (`pl`, range `r`, colour `c`):

```
if pl.z < pe.z                         -> no shadow
d2 = |pe - pl|² (3-D);  if d2 > r²     -> no shadow
scale   = shadowScale  > 0 ? shadowScale  : 1
opacity = shadowOpacity > 0 ? shadowOpacity : 1
W = w · 0.8 · scale                                  (_ETH_SHADOW_SCALEX)
if (pe.z + h) < pl.z:                                (light above the caster's top)
    planar = |pe.xy - pl.xy| ; vert = |pe.z + h - pl.z|
    len = min( planar/vert·|pl.z| - planar , 2.2·h ) (_ETH_SHADOW_FAKE_STRETCH)
else:
    len = h · (lightmapBake ? 8 : 2)                 (_ETH_SHADOW_SCALEY, /4 in real time)
len = max(len, h)
VS uniforms: shadowLength = len · shadowLengthScale ; entityZ = max(shadowZ, pe.z) ; shadowZ = shadowZ(=0) ; lightPos = pl
depth = vertical ? ComputeDepth(shadowZ) : max(0, ComputeDepth(pe.z) - 0.001)
alpha:  bake (maxOpacity): a = 1
        real time: a = (1 - d2/max(d2,r²)) · min(max(c.r,c.g,c.b),1)
                   a = min(a · (1 - (A.r+A.g+A.b)/3), 1)
                   a = a · clamp(1 - pe.z / max(h,1), 0, 1)
alpha8 = uint8(a·255·opacity);  if alpha8 < 8 -> no shadow
sprite: shadow.dds, colour (alpha8,255,255,255), origin (0.5, 0.79), quad size (W, 1),
        rotation GetAngle(pl.xy - pe.xy) = atan2(dx, dy) [+ target angle], rect mode THREE_TRIANGLES,
        blend AM_PIXEL (SrcAlpha/InvSrcAlpha, test >1/255), drawn at 2-D position pe.xy (NOT ToScreenPos)
```

**Geometry.** `RM_THREE_TRIANGLES` is a triangle strip of 5 vertices with (pos = uv): (0,0), (0,1), (0.5,0), (1,1), (1,0) (`gs2d/src/Video/Direct3D9/D3D9VideoInfo.cpp:177-183`). The shader is `dynaShadowVS.cg:83-124`:

```
q          = R·((pos01·(W,1)) - (0.5W, 0.79)) + pe.xy
lightVec   = normalize(pe.xy - pl.xy)
shadowDir  = normalize(q.xy - pl.xy) · shadowLength · (1 - v)      // the 3 top vertices (v=0) are extruded radially
q         += -lightVec · (shadowLength/6 - entityZ) + shadowDir    // "push back" toward the light
screen     = (q - screenSize/2 - cameraPos), y = -y + shadowZ;  z = 1 - depth;  uv = pos01
```

The base segment is perpendicular to the light direction. The far edge is a 3-vertex fan, so the shadow widens radially.

**`shadow.dds`.** 32×32 **A8L8**, 2176 bytes (128-byte header + 2048), no mips. Luminance is 0 everywhere, i.e. black.
- Alpha by row: rows 0-4 = 0; rows 5-21 ramp 3, 9, 17, 25, 35, 45, 58, 71, 86, 100, 118, 134, 154, 173, 196, 217, 238; rows 22-26 = 255; rows 27-31 = 0.
- Columns 0 and 31 are about 0; columns 1-2 and 29-30 are soft (e.g. 71/211 at the opaque rows).
- The base (v = 1) starts **transparent for 5/32 ≈ 0.156 of the length**. That offset almost exactly cancels the `len/6 ≈ 0.167` push-back, so the opaque band begins at the caster.

**Real time vs baked.**
- Real time: the shadow is drawn right after that entity's light pass for that light. It alpha-darkens whatever is already in the framebuffer under it: ambient, lightmap and other lights alike. It is depth-tested at the caster depth minus 0.001, so it never darkens the caster itself.
- Baked: the shadow darkens only its own light's temporary target (§3).

**Who casts in Penumbra.**
- `castShadow=1` placements: tile_shadow ×24, single_tile_short_shadow ×26, barrel ×13, single_tile_shadow ×11 (shadowLengthScale 9.5), thumbnail ×6 (menu, dynamic, vertical), devil ×2. All are static except the thumbnails. `crate.ent` and the `vert_*` entities are never placed.
- Lights with `castShadows=1`: all static torches and crystals; dynamic `light_spell`, `sword0/1`, `combo_sword`, `potion_pick` and `cursor`.
- Result: static shadows are baked from static lights. Real-time shadows appear when the player's light spell or sword light passes static casters, and from the menu cursor over the thumbnails.
- Real-time shadows are on by default in 2013 (`Resource/ETHResourceProvider.cpp:35`). `EnableRealTimeShadows` exists in 2010 but is never called.

---

## 3. Static lighting: lightmaps (ETHLightmapGen)

### 3.1 When

- 2010 has **no lightmap files**: no `*-esc` directories and no lightmap images.
- Lightmaps are **rendered at scene load**. The 2010 `ETHRenderEntity::GenerateLightmap` at `@0x422560` has these error strings:
  - "…lightmaps can't be generated during application render."
  - "…this scene has no light sources."
  - "coudn't create the render target"
  - "coudn't create temporary render target"
  - "coudn't set render target"
  - "coudn't render to temporary target"
- Scripts never call `GenerateLightmaps()` or `EnableLightmaps()`, so the automatic path runs. In 2013 that path is `LoadLightmaps()` inside `LoadScene`, before the scene scripts (`Script/ETHScriptWrapper.Scene.cpp:607-609`). 2013 also regenerates on device restore (`ETHEngine.cpp:512-518`).
- Warning strings show that 2010 did not bake static entities added later by `AddEntity`: "AddEntity - This is a static entity and its lightmap has not been rendered yet… It might be incorrectly lit".

### 3.2 Algorithm

2013 `Scene/ETHScene.cpp:336-427` plus `Shader/ETHLightmapGen.cpp:27-175`. The 2010 structure at `@0x422560–0x422c87` matches.

- **Receivers:** every static entity with a sprite. Both versions check `static`; 2013 also requires `applyLight`. In 2010 `BeginLightPass` refuses `applyLight = 0`, so such receivers get a black map, which adds nothing. Same result.
- **Lights:** every entity in the scene, in any bucket, that is static and has an active light. The light is placed at `owner.pos + light.pos·scale.y` with `range·scale.y`. 2010 checks the light's static byte (+1).
- For each receiver R (frame size `(w,h)`: `GetNumRects() ≤ 1 ? bitmapSize : rectSize`, unscaled):
  1. Create render target LM of size w×h (8-bit ARGB presumed) and clear it to black.
  2. Place R so its top-left is at RT (0,0) with z = 0: `newPos = (absOrigin, 0)`. Set the camera to (0,0) and z-buffer off. Shift every light by `(newPos - oldPos)`, which preserves relative 3D offsets.
  3. For each static light Lk:
     - Create a temporary RT, cleared black.
     - Draw R's **light pass** for Lk, the §2.3 shader, at angle 0 and scale 1. BeginLightPass uses `maxH = screenH`, `minH = 0`, the scene `lightIntensity`, `drawToTarget = true`, and the eye `(L.x, 1.5·screenH, 768)` in this moved frame, i.e. `(L.x, top + 1.5·screenH, R.z + 768)` in the world (§2.3).
     - If R is **not vertical**: for every static entity S in the scene, including R itself, shifted the same way, if `S.castShadow && Lk.castShadows`, draw S's projected shadow into the same temporary RT with `maxOpacity = true` (a = 1 × shadowOpacity) and `drawToTarget = true` (length factor 8, not 2).
     - Add the temporary RT into LM with AM_ADD (2010 `push 1 → SetAlphaMode(1)`). The 8-bit target saturates.
  4. `LM->GenerateBackup()`.
- **Resolution:** **one lightmap texel per sprite-frame pixel**, at scale 1. Most static tiles use frames of 32×32 up to 256×256. Budget if every static lit sprite gets its own RGBA8 map:

| scene | static lit sprites | texels | RGBA8 |
|---|---|---|---|
| level1 | 391 | 5,315,368 | ~20.3 MiB |
| level2 | 294 | 7,177,512 | ~27.4 MiB |
| level3 | 531 | 12,052,870 | ~46.0 MiB |
| menu | 56 | 2,569,222 | ~9.8 MiB |
| arena_select | 33 | 2,097,936 | ~8.0 MiB |
| pvp_lv1…6 | 34/99/39/32/54/69 | 0.41–2.0 M | 1.6–7.7 MiB |
| videoModes | 24 | 1,114,112 | ~4.2 MiB |

Many maps are all black and can be dropped. Static lit sprites with SpriteCut > 1 number about 255 placements. Their map covers **one frame**, the placement's `spriteFrame`, but is sampled with 0..1 quad UVs.

### 3.3 What the port must do to look the same

**1. Bake offline** with a CPU re-implementation of exactly §2.3 plus §2.6. This needs no engine change. For each static receiver R, texel (i,j) with `u = (i+0.5)/w`, `v = (j+0.5)/h` (the D3D9 half-pixel alignment is an open question):

```
P = R horizontal: (R.x-ox+u·w, R.y-oy+v·h, R.z) ; vertical: (R.x-ox+u·w, R.y, R.z+oy-v·h)
LM = 0
for each static active light L (pos = owner.pos + light.pos, colour c, range r, castShadows):
    if |R.pos - L.pos|² > (r + max(w,h))²: continue
    x = shade(T(u,v), N(u,v), P, L, LI=scene lightIntensity, C=instance colour, Eye=(L.x, top+1.5·screenH, R.z+768))   // §2.3, h/v, main/mainSpecular; world coordinates, top = R.y-(centre+pivot).y
    x = clamp(x, 0, 1);   if alpha(x) <= 1/255: x = 0        // alpha-tested pass
    if R not vertical: for each static caster S with castShadow (incl. R) and L.castShadows:
        a = shadowAlpha_S(P.xy)     // rasterise §2.6 strip in R-relative coords, bake variant (len factor 8, a=opacity), bilinear shadow.dds
        x = x · (1 - a)             // AM_PIXEL black over the temp target, 8-bit per step
    LM = clamp(LM + x, 0, 1)
```

**2. Runtime composite** for static sprites: `clamp(T·C·min(1, A + E) + LM)`, with LM sampled with 0..1 **quad** UVs.

**3. Live passes.** Add a live light pass only for pairs where the receiver or the light is dynamic. Examples: characters and enemies lit by torches and crystals; spells, swords, fireballs and explosions lighting everything.

**4. Real-time shadows.** Draw them only for those same pairs, using the real-time alpha formula.

**5. Blend in encoded 8-bit values, not linear light.** MP measured this for the 2013 build, and the 2010 D3D9 fixed-function and 8-bit pipeline is the same kind. Use the engine's `DisplayEncoded` mode.

**6. Resolution.** Match the original's default window of 1024×768 (`main.as:144`). The fake eye uses `screenH`, and the initial `maxH` is `screenH`.

---

## 4. Other census facts relevant to lighting

- Active lights placed per scene:
  - level1: 44 static (35 cast shadows) + 3 dynamic flashlights
  - level2: 20 static
  - level3: 23 static + 15 flashlights
  - menu: 6 static + cursor
  - pvp_lv1: 1; pvp_lv2: 9; pvp_lv3: 4 + 1 flashlight; pvp_lv4: 2 flashlights; pvp_lv5: 2 (`huge_ambient_light`, range 3530); pvp_lv6: 5
- `flashlight.ent` placements are **deleted in `setupScene`** (`setupScene.as:182-190`). They are editor aids, and dynamic lights never enter the bake, so they never render.
- Typical lights:
  - torch: (1, 0.7, 0.3), range 227.5, offset (0,-14,8)
  - crystal: (0.3, 0.7, 1), range 254.5
  - red_light: (1.3, 0, 0)
  - next_level: (2, 0, 0); colours above 1 are allowed
  - light_spell: (0.6, 0.6, 1), range 319
- **Derived, not observed — verify on the running original.** Characters spawn at z = -10 (`setupScene.as:279`). Wall-torch lights sit at z ≈ -62..-24, behind them. With a mostly flat normal, N·L < 0, so characters receive essentially **no** torch light; they are lit by ambient, lava-shooter lights and spell/sword lights. Wall tiles at z = -84 are lit.

---

## 5. Engine gaps and port notes (Supersonic, read-only survey)

The engine's `shadeSprite2D` (`assets/shaders/shader.frag:421-492`) already computes:
- `base = clamp(T·tint·ambient + overlay·strength)`
- per-light `clamp(T·tint·colour·(1 - d²/r²)·dot(L - P, N)/d)`, with P = (x, y, lighting height) and a `kNormalYDown` flag
- 64 `Light2DComponent`s at most, with layers

That matches Penumbra's horizontal diffuse model and composite. The mismatches are listed in `engineGaps` below. The other session is also changing the engine, so this report proposes no engine changes; every item is phrased as a gap.

Things the port can do without the engine:
- the offline lightmap baker
- particles drawn as their own depth-sorted quads, as MP does
- `SoundEffect` playback
- the 2.0 frame-speed cap
- halos as additive quads
- AM_ADD emulated with alpha 1 / premultiplied alpha 0 plus a 1/255 cutoff

## Key facts

- effects/*.par: 43 files, ASCII XML with CRLF (not UTF-16), one <ParticleSystem> each. The schema is the same 17 attributes + 10 children as Magic Portals, plus a LIVE <SoundEffect> child in 21 .par and 18 .ent embeds.
- No .par is loaded at run time: no script, scene or entity names one. The runtime uses only copies embedded in .ent/.esc <Particles>, max 2 per entity (Penumbra uses 1). The copies were pre-scaled by the editor, e.g. torch.ent = torch_fire.par x1.2 with StartPoint edited to (0,-8,8).
- 8 .par files are unreferenced: blood, explosion_particles, heavy_sword, small_explosion, sword, sword02, sword03.ent, sword04. fall.par and jumpfx.par are identical except for SoundEffect (a tie). explosion_particles.par and sword.par lack animationMode and SpriteCut (legacy).
- alphaMode census over 43 .par: 1 (AM_ADD, One/One) x25, 0 (AM_PIXEL) x14, 4 (AM_MODULATE, Zero/SrcColor) x4. AM_MODULATE draws the dark black_opaque.png auras: player bruxo/princess, minion, master_knight, checkpoints, sword_beam, fade_out_beam.
- Verified 2010 GS_ALPHA_MODE (GameSpace.dll SetAlphaMode @0x10006170, jump table @0x10006448): 0 SrcAlpha/InvSrcAlpha, 1 One/One, 2 no blend, 3 none, 4 Zero/SrcColor. Every mode except 3 alpha-tests alpha > 1/255 (ALPHAREF 1, GREATER). Identical to 2013 Video.h.
- Verified 2010 entity type enum (ComputeDepth @0x4230f6): 0 horizontal, 1 ground decal, 2 vertical, 3 overall (depth 1), 4 opaque decal (z+0.1), 5 layerable (depth max(layerDepth, 0.001)). Penumbra uses 0 (2492 defs), 5 (218), 2 (41).
- 2010 particle motion step = min(dt*60, 2.0) frame units, i.e. capped at about 33 ms (@0x41e370). 2013 and the MP port cap at 250 ms. Particle age uses an uncapped absolute clock. Luminance/ambient rule and vertical depth shift of 10 verified identical in 2010 (@0x41f3fd, @0x41f711).
- Depth: z_clip = 1 - (z - minH)/(maxH - minH), z-buffer LESSEQUAL, z-write on. Vertical sprites subtract (1-v)*rectSize.y/spaceLength per vertex, so each row's depth = z + height above the bottom in unscaled texture pixels. ZAxisDirection is (0,0) in all levels and (0,-1) only in menu.esc and arena_select.esc.
- Ambient pass: clamp(T*C*min(1, Ambient+Emissive) + Lightmap). The lightmap goes on texture stage 1 with D3DTOP_ADD and is sampled with raw 0..1 quad UVs (defaultStaticAmbientVS.cg:119,136).
- Pixel-light pass (One/One per light, alpha test): att = max(0, 1 - dist^2/range^2) with 3-D distance; d = dot(normalize(P-L), -normalize(2*nm-1)), renormalised. h: out = T*C*d*att*Lc*LI*T.a. v: same without T.a, and the normal is swizzled (n.x, n.z, -n.y). Negative values are clamped by the 8-bit target, not the shader.
- Specular (mainSpecular, used when <Gloss> is present, 253 defs): out = (T*C*d*Lc*LI + Lc*pow(sat(dot(n,H)), specularPower)*T.a*gloss*specularBrightness)*att. H is the Blinn half-vector with a fake eye at (L.x, 2*camY + 1.5*screenH - L.y, 768). In 2010 lightColor is raw and lightIntensity is separate (BeginLightPass @0x43a1b0), so specular is NOT multiplied by lightIntensity. 2013 differs here.
- Horizontal pixel 3-D position P = (x - ox + u*w, y - oy + v*h, z). Vertical P = (x - ox + u*w, y, z + oy - v*h): the sprite stands in the XZ plane with origin at centre-bottom, and a flat normal faces +y (down-screen).
- lightIntensity = 2 in all 13 scenes. The back buffer is PF32BIT (8-bit clamps everywhere), default 1024x768 (main.as:144). Pixel shaders are on by default. The 'Desativa pixel shaders' option switches to per-vertex lighting (h/vVertexLightShader) with no normal map and no specular.
- Dynamic shadow = shadow.dds (32x32 A8L8, luminance 0; alpha rows 0-4 = 0, 5-21 ramp 3..238, 22-26 = 255, 27-31 = 0). Drawn as a 5-vertex triangle strip whose 3 top vertices are extruded radially from the light by shadowLength and pushed toward the light by (shadowLength/6 - entityZ). Base width = w*0.8*shadowScale. Length = min(geometric, 2.2h), or h*8 (bake) / h*2 (real time); at least h; then times shadowLengthScale.
- Real-time shadow alpha = (1 - d^2/r^2)*min(maxColor,1)*(1 - avgAmbient)*clamp(1 - z/h), times 255*shadowOpacity; skipped if < 8. Depth = caster depth - 0.001. Blend AM_PIXEL darkens the whole framebuffer under the shadow. Constants 0.8, 2.2, 8, 2, 0.79/0.5, 0.001, /3 and <8 verified in the 2010 DrawShadow @0x423aa0.
- Lightmaps: no files in 2010; rendered at scene load for every static sprite (2010 GenerateLightmap @0x422560). Render target = sprite frame size (1 texel per frame pixel, scale 1). For each static light: temp target = light pass of that light + (non-vertical receivers only) shadows of all static castShadow entities at full opacity (length factor 8); added into the lightmap with One/One and 8-bit saturation.
- Static lights never produce live passes on static sprites; they exist only in the lightmap. Live passes are dynamic light x any receiver, and static light x dynamic receiver. Lights come only from owners in visible buckets (SetBorderBucketsDrawing(false), setupScene.as:167). The pass is skipped if |R-L|^2 > (range + max(w,h))^2 or applyLight = 0.
- Halo (2013): One/One quad of haloSize, colour = light.color * haloBrightness * (activeParticles/total of slot-0 system, even for static torches), depth at entity z (+h if vertical). Bitmaps halo.bmp 265x253, halo1.bmp 123x123, halo2.bmp 123x123.
- Lightmap texel budget if every static lit sprite is baked: level1 5.3M texels (~20 MiB RGBA8), level2 7.2M (~27 MiB), level3 12.1M (~46 MiB). About 255 static lit placements use SpriteCut > 1 (lightmap covers only their frame).
- flashlight.ent placements (levels, pvp_lv3/4) are deleted in setupScene.as:182-190 and never render. Characters spawn at z = -10, in front of wall-torch lights at z about -62..-24. By the formulas N.L < 0, so torches should not light characters; verify visually.
- Method note: MSVC stores float literals widened to double (0.8f becomes 0.800000011920929). Scanning machine.exe for exact doubles misses them; scan for float32-widened values or disassemble.

## Engine gaps

- No AM_MODULATE blend. The engine has Mix (SrcAlpha,1-SrcAlpha), Add (SrcAlpha,One) and Premultiplied (One,1-SrcAlpha), but not Zero/SrcColor multiply. Penumbra needs it for the player, minion, master_knight and checkpoint dark auras, sword_beam and fade_out_beam (4 .par, 11 carrier .ent). MP's Particles::Drawable refuses alphaMode 4.
- Ethanon AM_ADD is One/One, where alpha only feeds the alpha test. The engine Add is SrcAlpha/One. It must be emulated (alpha forced to 1, or premultiplied with alpha 0) plus a 1/255 discard.
- Normal maps: the 2010 Cg renormalises (-normalize(2*(nm-0.5))). The engine's shadeSprite2D deliberately does not renormalise. Needs a per-material switch.
- Vertical (type 2) lighting space is missing. Its pixel position runs along XZ (z = baseZ + height above bottom, y constant) with the normal swizzled (n.x, n.z, -n.y). The engine's 2D light uses P = (x, y, constant height) only.
- No specular/gloss term. Needed: Blinn-Phong with a gloss map, specularPower (20..100), specularBrightness, and a fake eye (L.x, 2camY + 1.5screenH - L.y, 768). Used by 253 entity definitions.
- Light-pass alpha weighting differs by variant. hPixelLight main multiplies the light by tex.a (matches the non-premultiplied path). vPixelLight main and both mainSpecular diffuse terms add at full weight (premultiplied path). mainSpecular's specular term is weighted by tex.a*gloss. The light pass is also alpha-tested (alpha = tex.a^2*C.a*d*att*LI <= 1/255 means no light); the engine's cutoff applies only to albedo alpha. **Modelled since 2026-09-28** (engine Sprite2DLight::lightAlphaTest, per-variant pass alpha, Light2DAlphaTest for LI; port kLightPassAlphaTest).
- No projected dynamic shadow primitive. The original draws a 5-vertex strip extruded per vertex away from the light (dynaShadowVS), alpha-blended black, depth-tested at caster depth - 0.001, darkening the whole framebuffer. The engine has no custom 2D vertex extrusion; it would need CPU-built arbitrary 2D mesh draws with depth.
- No runtime render-to-texture lightmap bake. The engine only loads an overlay file (MaterialComponent::overlayTexturePath). An offline CPU baker in the port repo that writes PNG overlays avoids any engine change. **The bake's shadows modelled since 2026-09-28** without a bake: a static light carries its static casters' strips (engine Light2DShadowsComponent, scene binding 13, shadow.dds's alpha as Light2DShadowMask) and a static flat receiver multiplies that light's add by what survives them (Sprite2DLight::lightShadows; port kBakedShadowsOwnLight), which is §3.2 step 3's temporary target per fragment.
- Overlay UVs: the engine samples overlayMap with the albedo's transformed UV. The original samples the lightmap with raw 0..1 quad UVs, and its lightmap is frame-sized. For the ~255 static placements with SpriteCut > 1, the baker must emit sheet-sized overlays with only the frame rect filled, or the engine needs a separate overlay UV.
- Per-pixel depth for vertical sprites (depth rising along the sprite's height via the z-buffer). Engine 2D sprites sort by a single quad z. **Modelled game-side since 2026-09-28**: render/DrawOrder cuts a vertical sprite into bands of rows wherever a sprite or particle overlapping it has a depth strictly inside its rows (the cursor's sparkles over the arena thumbnails, fog over the menu logo and barrels). Not modelled: a translucent texel's depth write, which rejected pieces drawn later behind it outright.
- Particles are sorted among themselves and drawn after all transparents in the engine. The original interleaves particle pieces with sprites by depth: layerable clouds at depth 0.001 behind everything, fog at layerDepth 1 in front, torch flames at z+8. The port should draw particles as its own depth-sorted quads (MP approach).
- Light gathering: the engine sends every enabled Light2D (cap 64). The original uses only lights whose owners are in visible buckets, each pass scissored to a 2*range square. level1 alone places 44 static lights plus dynamic ones; use layers (static lights excluded from static receivers, as in MP's ReceiverMask) and/or culling to stay under 64.
- No 2010 particle SoundEffect/soundVolume support anywhere. It is gameplay/audio code the port must add. Also the 2010 particle frame-speed cap (2.0 units) differs from MP's Particles.cpp (250 ms).
- MP's Particles.cpp reader cannot be reused as-is. It requires a UTF-16LE BOM (Penumbra is ASCII), silently returns nothing for standalone .par (no <Particles> wrapper), demands animationMode and SpriteCut, and rejects alphaMode 4.

## Open questions

- When exactly is a particle system's <SoundEffect> played in 2010: once at creation, on every PlayParticleSystem, or per repeat? Is the volume exactly soundVolume, and is it positional? The load path (@0x422318) is known; the play site is not.
- Render-target format of 2010 lightmaps and temporary targets: CreateRenderTarget gets a GS_TARGET_FORMAT argument pushed as 0. A8R8G8B8 is presumed.
- Are lightmaps generated before or after setupScene's UsePixelShaders(false) when the player disables pixel shaders? That decides whether baked lighting is per-pixel or per-vertex in that mode. newGame calls UsePixelShaders(true) before LoadScene. Does an Alt+Enter device reset regenerate lightmaps in 2010 (2013 does, in ETHEngine::Restore)?
- Default and behaviour of the 2010-only SetHaloRotation(bool). No script calls it.
- 2010 draw order: the 2013 depth-keyed piece multimap (sprite/halo/particles interleaved by drawHash) versus an older per-pass scheme. It matters only for overlapping translucent pieces.
- Default of real-time shadows in 2010 (2013: m_usingRTShadows = true). EnableRealTimeShadows is never called by the scripts.
- Does 2010 dim dynamic lights by the owner's slot-0 active/total particle ratio (2013 ComputeLightIntensity), and halos by the same ratio? The halo code comment suggests yes; not verified in the binary.
- D3D9 half-pixel alignment when the entity is drawn into the lightmap render target (quad at 0..w, 0..h without a -0.5 offset). This may shift or blur the baked lightmap by half a texel relative to the sprite.
- Characters at z = -10 versus wall-torch lights at z about -62..-24: the formulas say torches do not light characters (N.L < 0). Confirm on the running original.
- gs2d 2010 texture filtering for shadow.dds and lightmaps (bilinear presumed), and whether GS_SPRITE flip is implemented through the texture rect, since the 2010 ambient VS ignores flipAdd/flipMul.
