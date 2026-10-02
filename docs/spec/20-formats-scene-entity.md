# Penumbra (Ethanon 2010): content data formats and census

## 0. What was run and where the output is

- **Parser/census script:** `reference/analysis/esc_census.py`
  - Pure Python stdlib. It decodes XML as ISO-8859-1, reproduces the ENML grammar, reads image headers, and compares against the `machine.exe` vocabulary.
  - Run with `python esc_census.py [APP_DIR]`. It takes about 2 s.
- **Generated output**, in reference/analysis/:
  - `census_out\census.md`: about 2,100 lines covering every table below, plus the full schema table and image headers.
  - `census_out\census.json`
  - `machine_strings.txt`: `strings -t x` of `machine.exe`.
  - `compact_tables.md`: the .ent table.
  - `matrix.md`: the name × scene matrix.
- **Corpus:**
  - 13 `.esc`, 144 `.ent`, 43 `.par`.
  - 2,607 scene entity instances using 112 distinct `EntityName`s. 87 of those names are real `.ent` files and 25 are labels only.
  - 2 ENML files and 1 `.ethproj`.

**Confidence convention used below:**
- **[exe]**: confirmed from the 2010 `machine.exe` string table.
- **[data]**: observed in the files.
- **[2013]**: taken from the Dec-2013 Ethanon source (`toolkit/Source/src/`; not in this repository), whose semantics are assumed unchanged.
- **[script]**: from the game's .as sources.

---

## 1. Source of truth: the 2010 loader vocabulary

`machine.exe` holds a contiguous block of XML names that the loader uses, at `.rdata` offsets 0xc955c–0xc98b8 (`machine_strings.txt` lines 62302–62405). In order:

- **Scene properties and light:** `SceneProperties lightIntensity Ambient ZAxisDirection`, then `active static castShadows range haloBrightness haloSize Color HaloBitmap Light`.
- **Root and entity attributes:** `Ethanon type collidable applyLight castShadow startFrame blendMode layerDepth soundVolume shadowScale shadowLengthScale shadowOpacity specularPower specularBrightness`.
- **Entity children and scene instance:** `EmissiveColor SpriteCut PivotAdjust Sprite Normal Gloss Particles ParticleSystem Entity EntityName angle id spriteFrame`.
- **Particle system:** `particles allAtOnce alphaMode repeat boundingSphere lifeTime randomLifeTime angleDir randAngle size randomizeSize growth minSize maxSize angleStart randAngleStart animationMode Bitmap SoundEffect Gravity Direction RandomizeDir StartPoint RandStartPoint Color0 Color1 Luminance`.
- **CustomData** (at 0xc74d4): `CustomData Variable Type Name Value`, with the type words `float int uint string`.

**Absent from the 2010 exe [exe]:**
- `FileName`, so no reference form: the inline block is the only form a scene can use.
- `Scale`, so there is no entity scale.
- `parallaxIntensity`, `flipX`, `flipY`, `hide`.
- The Box2D set (`shape`, `sensor`, `density`, `friction`, `Polygon`, `Compound`, `Joints`) and `hideFromSceneEditor`.
- `shadowZ` does occur, but only in the shader-parameter block (`shadowLength entityZ shadowZ lightPos`, 0xc9bc0). It is not an XML attribute, and no data file uses it.

**Present in 2010, marked dead in the 2013 doc:** `startFrame`, `soundVolume`, `collidable` and `SoundEffect`. The data shows they carry meaning (§4.3, §9, §11).

**No data name is missing from the exe.** Every element and attribute name used by any `.esc`, `.ent` or `.par` appears as a NUL-terminated token in `machine.exe`.

**AngelScript enums registered in 2010 [exe]:**
- `ENTITY_TYPE {ET_HORIZONTAL, ET_VERTICAL, ET_OVERALL, ET_GROUND_DECAL, ET_OPAQUE_DECAL, ET_LAYERABLE}`. This is the registration order; the numeric values are not in the strings. The 2013 values are 0, 2, 3, 1, 4, 5.
- `DATA_TYPE {DT_NODATA, DT_INT, DT_UINT, DT_FLOAT, DT_STRING}`. There is no vector2/vector3 custom-data type in 2010.
- The blend enum is `GS_ALPHA_MODE` in GameSpace.dll. Its member names are not in the strings, so the blend/alpha numbers below are the 2013 `AM_*` values (assumed).

---

## 2. Encoding and XML rules

- **The declared encoding is wrong for two files.** Every `.esc`, `.ent` and `.par` begins `<?xml version="1.0" ?>`, which by the XML spec means UTF-8.
  - `level1.esc` contains the Latin-1 bytes `ã ç ó`, and `level3.esc` contains `É` (in CustomData `message` strings). These are invalid UTF-8, so a strict parser rejects both files.
  - **Rule: decode as ISO-8859-1 (Latin-1).** The other 11 scenes, all `.ent` and all `.par` files are pure ASCII. `data.enml` contains `áâãçéíóõ`.
- **Layout:** no BOM, CRLF line endings throughout (for example `level1.esc` has 14,625 CRLF and no bare LF), 4-space indentation, and numbers written compactly (`0`, `0.3`, `737.28`, never `%f` padding).
- **XML escapes exist,** so a real XML parser is needed, not text scraping. For example `level1.esc:6397`, `:7445` and `:8231` contain `Golpe de espada: tecla &apos;S&apos;`. There are no comments and no CDATA.
- **Attribute order is fixed** by the writer:
  - scene instance `<Entity id spriteFrame>`;
  - inner `<Entity type static collidable startFrame applyLight castShadow blendMode layerDepth soundVolume shadowScale shadowLengthScale shadowOpacity specularPower specularBrightness>`, all 14 always present;
  - `<Light active static castShadows range haloBrightness haloSize>`.
- **Child order is also fixed** (subsets of one sequence):
  - `.esc` instance: `EntityName > Color > Position > Entity`, 2,607 of 2,607.
  - Inner and `.ent` entity: `EmissiveColor > SpriteCut > PivotAdjust > [Sprite] > [Normal] > [Gloss] > Particles > Light > Collision > CustomData`. `EmissiveColor`, `SpriteCut`, `PivotAdjust`, `Particles`, `Light`, `Collision` and `CustomData` are always present, even when empty or inactive.
  - Observed inner sequences:

| sequence | instances |
|---|---|
| EmissiveColor > SpriteCut > PivotAdjust > Sprite > Normal > Particles > Light > Collision > CustomData | 1495 |
| EmissiveColor > SpriteCut > PivotAdjust > Particles > Light > Collision > CustomData | 596 |
| EmissiveColor > SpriteCut > PivotAdjust > Sprite > Particles > Light > Collision > CustomData | 275 |
| EmissiveColor > SpriteCut > PivotAdjust > Sprite > Normal > Gloss > Particles > Light > Collision > CustomData | 241 |

---

## 3. Complete XML schema as used

### 3.1 `.esc`

```
<Ethanon>
  <SceneProperties lightIntensity>                  lightIntensity = "2" in all 13 scenes
     <Ambient r g b/>                              r,b 0..0.4; g 0..0.3 (see per-scene table)
     <ZAxisDirection x y/>                         x=0 always; y = -1 (menu, arena_select) or 0 (11 scenes)
  <EntitiesInScene>                                 only <Entity> children
     <Entity id spriteFrame>                        id 0..1106 (unique per scene, sparse, NOT ascending in file); spriteFrame 0..20
        <EntityName>text</EntityName>               112 distinct; 25 have no .ent file (labels)
        <Color r g b a/>                            ALWAYS (1,1,1,1) in all 2607
        <Position x y z angle/>                     x -2624..12416, y -2142..3649, z -400..314; angle ALWAYS 0;
                                                    4 non-integer values (npc_wall y=480.5 x2 in pvp_lv1, y=144.5 x2 in pvp_lv4)
        <Entity ...14 attrs...>                     full inline copy of the entity definition (section 3.2)
```

There is no scene-level custom data, no scene name, and no parallax. `lightIntensity` is the only `SceneProperties` attribute.

### 3.2 Inner `<Entity>` and `.ent` `<Entity>`: attribute and element table

The value ranges below are observed values.

| name | .ent (144) | .esc instances (2607) | notes |
|---|---|---|---|
| @type | 0:127, 2:11, 5:6 | 0:2365, 5:212, 2:30 | 1 (ground decal), 3 (overall) and 4 (opaque decal) are unused. [2013] meaning: 0 = horizontal/flat, 2 = vertical (centre-bottom origin, no rotation), 5 = layerable (depth = layerDepth, ignores z) |
| @static | 1:82, 0:62 | 1:2006, 0:601 | |
| @collidable | 0:76, 1:68 | 1:1388, 0:1219 | runtime flag, see §13.2 |
| @startFrame | 0:123, 4:8, 1:5, 8:4, 9/3/2/6:1 each | 0:2471, 1:83, 9:32, 2:11, 3:10 | the definition's default frame (§4.3) |
| @applyLight | 1:82, 0:62 | 1:2018, 0:589 | |
| @castShadow | 0:133, 1:11 | 0:2525, 1:82 | |
| @blendMode | 2:76, 0:68 | 2:2077, 0:530 | only 0 and 2 occur. [2013] 0 = AM_PIXEL (alpha blend), 2 = AM_ALPHA_TEST (1-bit cutout) |
| @layerDepth | 0:138, 1:6 | 0:2386, 1:221 | only 0 or 1 |
| @soundVolume | 1:130, 0.5:7, 0.3:2, 0.2:2, 0.7:2, 0:1 | 1:2607 | non-1 only on entities whose particle system has a `<SoundEffect>` (§11). silent_sword.ent = 0 |
| @shadowScale | 0:134, 1.2, 1.5, 1.4, 0.8 (2 each), 0.3, 0.6 | 0:2375, 1.4:169, 1.5:37, 1.2:24, 0.3:2 | |
| @shadowLengthScale | 1:142, 9.5:1, 1.5:1 | 1:2572, 1.5:24, 9.5:11 | |
| @shadowOpacity | 1 always | 1 always | |
| @specularPower | 50:135, 71:3, 30:2, 60:2, 61:1, 100:1 | 50:2456, 60:74, 71:62, 20:7, 100:6, 30:2 | |
| @specularBrightness | 1 always | 1 always | |
| EmissiveColor r g b a | (0,0,0,0):126, (1,1,1,0):15, (0.5,…):2, (0.8,…):1 | (0,0,0,0):2438, (1,1,1,0):109, (0.5,…):54, (0.8,…):6 | a is always 0 |
| SpriteCut x y (cols, rows) | x 1/2/4/8, y 1/2/4/8 | 1x1:~2393 | combinations 1x1, 2x1, 1x2, 2x2, 4x1, 4x4, 8x8, 1x8 |
| PivotAdjust x y | x always 0; y 0 … −32 | same | y values 0, −8, −9, −10, −13, −14, −16, −25, −32 |
| Sprite (text) | 95 of 144 | 2011 of 2607 | resolved under `entities/` |
| Normal (text) | 71 | 1736 | resolved under `entities/normalmaps/` |
| Gloss (text) | 12 | 241 | white.bmp, gray.bmp, white_ground.jpg, menu_buttons_gloss.png, paladin_gloss.png. All exist in `entities/`. The directory is unconfirmed but plausible (§14) |
| Particles/ParticleSystem | 50 .ent have exactly 1 | 124 instances have exactly 1 | never 2 |
| Light @active @static @castShadows @range @haloBrightness @haloSize, Position xyz, Color rgb, [HaloBitmap] | active 1:37 | active 1:141 | Light@static equals Entity@static in 100% of .ent and .esc. Inactive blocks carry the defaults (range 256, halo 64/1, colour 1,1,1), except paladin.ent and silent_sword.ent. Colour r/g up to 2. Range 15…3530 |
| Collision: Position xyz, Size xyz | always present | always present | size is the full extent, and z is a depth thickness. Zero size on 70 .ent and 1101 instances |
| CustomData/Variable {Type, Name, Value} | 93 variables in 24 files | 453 variables | Type ∈ {int, uint, float, string}. Value is always element text |

### 3.3 `.par` (43 files, all in `effects/`)

- **Root:** `<Ethanon><ParticleSystem …>`.
- **Attributes (17):** `particles allAtOnce alphaMode repeat animationMode boundingSphere lifeTime randomLifeTime angleDir randAngle size randomizeSize growth minSize maxSize angleStart randAngleStart`.
  - `animationMode` is absent in 2 files: `explosion_particles.par` and `sword.par`.
- **Children:** `[SoundEffect] Bitmap Gravity Direction RandomizeDir [SpriteCut] StartPoint RandStartPoint Color0 Color1 Luminance`.
  - `SoundEffect` appears in 21 files. `SpriteCut` is absent in the same 2 files.
- **Observed values:**
  - `alphaMode` ∈ {0:14, 1:25, 4:4}. [2013] 0 = alpha, 1 = additive, 4 = modulate.
  - `repeat` ∈ {0, 1, 5, 7, 36}; `particles` 4..70; `animationMode` always 1.
  - `SpriteCut` 1x1 or 4x3 (explosion.png).
- **Odd names:** `sword03.ent.par` (a real .par file with a double extension) and `effects/sky.png` (an image in the effects folder, referenced by nothing).

---

## 4. Inline block vs `.ent` on disk, and what varies per instance

### 4.1 Result

- **2,254 instances** have an `EntityName` that is a `.ent` file. **2,250 of them are byte-for-byte identical** to the `.ent` after normalisation (floats, attribute order, CustomData as a map). That includes their CustomData.
- **353 instances** use label-only names with no `.ent` file.
- **Zero** instances differ only in CustomData. Per-instance CustomData occurs only on label-only names.
- **The four property differences:**

| entity | .ent mtime | scene | .esc mtime | id | difference |
|---|---|---|---|---|---|
| ground_fire.ent | 2010-10-30 12:42 | arena_select.esc | 2010-10-22 18:37 | 46 | Light@haloBrightness ent=0.65 esc=1; haloSize ent=286 esc=142. The .ent was edited after the scene was saved: stale copy |
| blue_light.ent | 2010-10-30 13:09 | level1.esc | 2010-11-07 13:58 | 423 | haloBrightness 0.3→0, haloSize 288→146, HaloBitmap halo1.bmp→halo.bmp. Instance placed before the .ent edit and never refreshed. The 2 menu.esc instances match |
| tile02.ent | 2010-10-04 21:48 | level3.esc | 2010-11-07 13:59 | 407 | Entity@static 1→0 (and Light@static follows). A per-instance tweak |
| fog_menu.ent | 2010-10-30 12:38 | menu.esc | 2010-10-30 13:30 | 100 | the .ent has `<Sprite>plant_small.png`; the instance has no Sprite |

**Consequence:**
- A scene-placed entity must be built from its **inline block**. That is the only thing the 2010 loader can read, because `FileName` is absent from the exe.
- Entities spawned by scripts through `AddEntity("x.ent")` come from **`entities/x.ent`**.
- The two sources agree in 99.8% of cases, so a converter can dedupe inline blocks against the `.ent` and store overrides only for the 4 differences plus the label-only definitions.

### 4.2 Per-instance fields vs per-definition fields

- **Always per instance (outer element):** `id`, `spriteFrame`, `Position x y z` (angle is always 0), and `Color` (always 1,1,1,1).
- **Per instance through the definition:** CustomData on the label-only names `spawn`, `story`, `help`, `thumbnail`, `environment`, `play`, `play_sound`, `next_level`, `deco`, `summon` and `spawn2`.
- **Everything else is per definition,** apart from the 4 differences above.

### 4.3 `startFrame` (definition) vs `spriteFrame` (instance)

- They are equal in **2,585 of 2,607** instances.
- The 22 differences:
  - the 7 menu buttons (`novo_jogo` 0, `creditos` 1, `melhores_tempos` 2, `sair` 3, `opcoes_de_video` 4, `como_jogar` 5, `versus` 6). All share `menu_buttons.png` with a 1x8 cut and startFrame=0;
  - 5 `thumbnail` instances (frames 1–5);
  - `cano.ent` ×2 (frame 1);
  - `half_ground.ent` ×2;
  - `small_ground_tile.ent` ×7 in pvp_lv2 (frames 12/14/17/20, with startFrame 9).
- The **displayed frame must be `spriteFrame`**; otherwise every menu button would show the same frame. `startFrame` is the default for script-spawned entities: it is used by `cliff_right`/`wall03`/`wall05`/`wall09`/`wall10` (=1), `wall06` (=3), `wall07` (=2), the characters (4, 6 or 8), and `small_ground_tile` (=9).
- No frame index is ever ≥ cols×rows.

### 4.4 `EntityName` is a label

- **Callback lookup.** [2013] `ETHGlobal::FindCallbackFunction` (`engine/Util/ETHASUtil.cpp:63-68`) looks up `"ETHCallback_" + RemoveExtension(EntityName)`. So `help` and `help.ent` both bind `ETHCallback_help`.
- **Name lookups are exact.** [2013] `ETHBucketManager::GetEntityArrayByName` (`ETHBucketManager.cpp:488-503`) does an exact string compare.
- **The scripts use both spellings:**
  - `GetEntityArray("spawn")`, `("play_sound")`, `("environment")` without the extension (setupScene.as:172/175/195);
  - `GetEntityArray("flashlight.ent")` (setupScene.as:184) and `SeekEntity("cursor.ent")` (menu.as:66) with it;
  - `GetEntityName() == "npc_wall.ent"` / `"instant_death*.ent"` (controlCharacters.as:68/88-90);
  - menu buttons by label (menu.as:250-311);
  - `"summon"` as the summoner origin (controlCharacters.as:666).
- **Consequences:**
  - The 5 `play_sound.ent` instances (level2 ×3, level3 ×2) are **not** collected by `GetEntityArray("play_sound")` and have no callback, so they are inert. Only the 2 `play_sound` labels in level2 play `horror.mp3`.
  - Renaming is used to opt out of callbacks: `dontFall`, `fontFall` and `step` are `falling_bridge` copies without `ETHCallback_falling_bridge`, and `pilar02` is a `pilar02.ent` copy with applyLight=0.

**Inline definitions of the 25 label-only names**, one row per distinct shape:

| EntityName | type | static | collid | applyLight | Sprite | Normal | Light | Collision size | psys | CustomData keys | first seen |
|---|---|---|---|---|---|---|---|---|---|---|---|
| spawn | 0 | 0 | 0 | 1 | - | - | - | 0,0,0 | 0 | [int complete], string name | level1 |
| spawn2 | 0 | 0 | 0 | 1 | - | - | - | 0,0,0 | 0 | int complete, string name | level3 |
| help | 0 | 1 | 0 | 0 | - | - | - | 0 | 0 | string message | level1 |
| story | 0 | 1 | 0 | 0 | - | - | - | 0 | 0 | string name | level1 |
| play | 0 | 1 | 0 | 1 | - | - | - | 0 | 0 | string name | level1 |
| play_sound | 0 | 0 | 0 | 0 | - | - | - | 0 | 0 | string name | level2 |
| environment | 0 | 0 | 0 | 0 | - | - | - | 0 | 0 | uint bgColor, [string bgImage], [string name] | level2 |
| next_level / deco / decoration | 0 | 1 | 0 | 1 | passage.png | passage_nm.png | active r=182.5 | 0 | 1 | [string name] | level1/2/3 |
| summon | 0 | 0 | 0 | 1 | - | - | - | 0 | 0 | [string name] | level3, pvp_lv6 |
| event01 | 0 | 1 | 1 | 0 | - | - | - | 128,256,10 | 0 | - | level3 |
| pilar02 | 0 | 1 | 0 | 0 | Pilar_Bitmap_opaque.png | Pilar_Normal.png | - | 640,480,1 | 0 | - | level2 |
| dontFall / fontFall / step | 0 | 0 | 1 | 1 | single_stone.png | single_stone_nm.png | - | 64,32,11 | 0 | - | level3, pvp_lv1 |
| versus, novo_jogo, melhores_tempos, como_jogar, opcoes_de_video, sair, creditos | 0 | 0 | 1 | 1 | menu_buttons.png | menu_nm_buttons.png | - | 369,25,25 | 0 | - | menu |
| thumbnail | 2 | 0 | 1 | 1 | thumbnails.png | thumb_normal.png | - | 64,64,68 | 0 | string name, string title, [uint score] | arena_select |
| picker | 2 | 0 | 1 | 1 | - | - | active r=181 | 31,31,148 | 1 | - | videoModes |

Unused `.ent` twins of label names: `spawn.ent`, `help.ent`, `environment.ent`, `next_level.ent`, `thumbnail.ent` (0 instances) and `summon.ent` (only spawned as an effect).

---

## 5. Per-scene census

### 5.1 Summary

| scene | bytes | entities | distinct names | lightIntensity | Ambient rgb | ZAxisDir | bounds | ids | active lights | on static ent | on dynamic ent | active after setupScene | ents w/ particles |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| arena_select | 56018 | 41 | 5 | 2 | 0.2,0,0.2 | 0,-1 | x 110..1920; y 128..896; z 0..18 | 0..46 | 3 | 2 | 1 (cursor) | n/a (menuPreLoop) 3 | 2 |
| gameover | 2273 | 1 | 1 | 2 | 0.3,0.3,0.3 | 0,0 | x 307; y 7; z -62 | 1 | 0 | 0 | 0 | n/a 0 | 1 |
| level1 | 791978 | 630 | 33 | 2 | 0.2,0.1,0.2 | 0,0 | x -96..4384; y -2142..704; z -86..48 | 2..819 | 47 | 44 | 3 (flashlight ×3) | 44 | 36 |
| level2 | 550486 | 429 | 45 | 2 | 0.4,0.2,0.4 | 0,0 | x -2624..2112; y -2010..960; z -212..4 | 0..577 | 20 | 20 | 0 | 20 | 22 |
| level3 | 1084016 | 866 | 59 | 2 | 0,0,0 | 0,0 | x -640..12416; y -384..3649; z -400..62 | 0..1106 | 38 | 23 | 15 (flashlight ×15) | 23 | 45 |
| menu | 89676 | 69 | 16 | 2 | 0.2,0,0.2 | 0,-1 | x 45..1920; y 44..1152; z 0..314 | 0..146 | 7 | 6 | 1 (cursor) | n/a 7 | 5 |
| pvp_lv1 | 107086 | 90 | 20 | 2 | 0.3,0.3,0.3 | 0,0 | x -480..1504; y 168..1152; z -94..0 | 1..100 | 1 | 1 | 0 | 1 | 1 |
| pvp_lv2 | 179687 | 144 | 22 | 2 | 0.1,0.1,0.1 | 0,0 | x -640..1664; y 64..1008; z -100..0 | 3..178 | 9 | 9 | 0 | 9 | 7 |
| pvp_lv3 | 84746 | 69 | 15 | 2 | 0.2,0.1,0.2 | 0,0 | x -64..1088; y 192..960; z -86..0 | 0..97 | 5 | 4 | 1 (flashlight) | 4 | 0 |
| pvp_lv4 | 69178 | 57 | 15 | 2 | 0,0,0 | 0,0 | x -640..1664; y -128..896; z -28..70 | 0..66 | 2 | 0 | 2 (flashlight ×2) | 0 | 0 |
| pvp_lv5 | 118415 | 97 | 13 | 2 | 0.2,0.2,0.2 | 0,0 | x -128..1152; y -384..640; z -224..4 | 2..121 | 2 | 2 | 0 | 2 | 0 |
| pvp_lv6 | 111815 | 88 | 14 | 2 | 0.1,0.1,0.1 | 0,0 | x -576..1600; y -128..1152; z -96..0 | 0..131 | 5 | 5 | 0 | 5 | 4 |
| videoModes | 32127 | 26 | 9 | 2 | 0.3,0,0.3 | 0,0 | x 64..960; y 64..640; z -90..10 | 0..33 | 2 | 1 | 1 (picker) | n/a 2 | 1 |

**Every dynamic light placed in a gameplay scene is a `flashlight.ent` placeholder, and setupScene deletes all of them** (setupScene.as:181-190).
- After setup, level and pvp scenes have **only static lights**. Their dynamic lighting comes entirely from script-spawned entities: fire_ball, fire_shoot, combo_fire_ball, combo_sword, sword0/1, paladin_sword, light_spell, king, explosion/big_explosion/blood/enemy_blood/hit_fail and potion_pick.
- The menu scenes keep their dynamic `cursor`/`picker` light.
- Other logic markers are also replaced at load:
  - the 271 `spawn` (and 1 `spawn2`) are replaced by `name + ".ent"` at pos − (0,0,10) when on screen (setupScene.as:273-280);
  - the 7 `environment` are deleted and replaced by `name` (setupScene.as:195-216).
- `help`, `story`, `play`, `next_level`, `event01` and `thumbnail` are invisible trigger entities driven by callbacks.

### 5.2 Flag distributions per scene (value:count)

| scene | type | static | collidable | applyLight | castShadow | blendMode | layerDepth | startFrame | spriteFrame | angle |
|---|---|---|---|---|---|---|---|---|---|---|
| arena_select | 0:33, 2:8 | 0:7, 1:34 | 0:33, 1:8 | 0:1, 1:40 | 0:35, 1:6 | 0:8, 2:33 | 0:41 | 0:41 | 0:36, 1..5:1 each | 0 |
| gameover | 5:1 | 0:1 | 0:1 | 1:1 | 0:1 | 0:1 | 0:1 | 0:1 | 0:1 | 0 |
| level1 | 0:518, 5:112 | 0:129, 1:501 | 0:318, 1:312 | 0:189, 1:441 | 0:613, 1:17 | 0:133, 2:497 | 0:518, 1:112 | 0:630 | 0:630 | 0 |
| level2 | 0:429 | 0:99, 1:330 | 0:215, 1:214 | 0:43, 1:386 | 0:427, 1:2 | 0:113, 2:316 | 0:429 | 0:370, 1:44, 2:8, 3:7 | same | 0 |
| level3 | 0:801, 5:65 | 0:206, 1:660 | 0:388, 1:478 | 0:225, 1:641 | 0:864, 1:2 | 0:147, 2:719 | 0:801, 1:65 | 0:847, 1:15, 2:2, 3:2 | 0:846, 1:16, 2:2, 3:2 | 0 |
| menu | 0:48, 2:21 | 0:9, 1:60 | 0:45, 1:24 | 0:5, 1:64 | 0:54, 1:15 | 0:15, 2:54 | 0:68, 1:1 | 0:69 | 0:63, 1..6:1 each | 0 |
| pvp_lv1 | 0:64, 5:26 | 0:46, 1:44 | 0:36, 1:54 | 0:40, 1:50 | 0:90 | 0:19, 2:71 | 0:64, 1:26 | 0:87, 1:3 | 0:86, 1:4 | 0 |
| pvp_lv2 | 0:136, 5:8 | 0:18, 1:126 | 0:53, 1:91 | 0:35, 1:109 | 0:130, 1:14 | 0:19, 2:125 | 0:127, 1:17 | 0:107, 1:5, 9:32 | 0:107, 1:5, 9:25, 12:4, 14/17/20:1 | 0 |
| pvp_lv3 | 0:69 | 0:28, 1:41 | 0:27, 1:42 | 0:4, 1:65 | 0:69 | 0:11, 2:58 | 0:69 | 0:60, 1:9 | same | 0 |
| pvp_lv4 | 0:57 | 0:18, 1:39 | 0:29, 1:28 | 0:13, 1:44 | 0:57 | 0:15, 2:42 | 0:57 | 0:56, 1:1 | same | 0 |
| pvp_lv5 | 0:97 | 0:26, 1:71 | 0:21, 1:76 | 0:26, 1:71 | 0:97 | 0:31, 2:66 | 0:97 | 0:95, 2:1, 3:1 | 0:93, 1:2, 2:1, 3:1 | 0 |
| pvp_lv6 | 0:88 | 0:13, 1:75 | 0:40, 1:48 | 0:7, 1:81 | 0:62, 1:26 | 0:17, 2:71 | 0:88 | 0:86, 1:2 | same | 0 |
| videoModes | 0:25, 2:1 | 0:1, 1:25 | 0:13, 1:13 | 0:1, 1:25 | 0:26 | 0:1, 2:25 | 0:26 | 0:22, 1:4 | same | 0 |

- `soundVolume` = 1 in every instance, and `shadowOpacity`/`specularBrightness` = 1 everywhere.
- `specularPower`:
  - arena_select 60:32, 100:6, 50:3;
  - menu 60:38, 50:22, 20:7, 30:2;
  - level2 71:20; level3 71:38; pvp_lv1/3/6 71:1–2; pvp_lv5 60:4;
  - 50 elsewhere.
- `EmissiveColor` (1,1,1,0) marks self-lit light props (crystals, lava, flashlight, gamelogo, …), and (0.5,0.5,0.5,0) marks potions.

**type × layerDepth:**
- type 5 with layerDepth 1: black.ent, instant_death.ent, instant_death3.ent, fog.ent (211 instances);
- type 5 with layerDepth 0: clouds.ent, dawn.ent;
- type 0 with layerDepth 1: instant_death2.ent (9);
- type 2 with layerDepth 1: fog_menu.ent;
- type 2 (upright props): barrel, cursor, devil, gamelogo, ground_fire, pilar, thumbnail, picker, vert_*.

### 5.3 Lights per scene (active = `active=1` and range > 0)

| scene | active | Light@static | castShadows | HaloBitmap | range | by entity |
|---|---|---|---|---|---|---|
| arena_select | 3 | 0:1, 1:2 | 1:3 | halo.bmp:2, halo1.bmp:1 | 181..284.5 | green_light_menu 1, cursor 1, ground_fire 1 |
| level1 | 47 | 0:3, 1:44 | 0:12, 1:35 | none:13, halo:20, halo1:14 | 47.5..454 | torch 10, shooter 9, red_light 5, crystal 5, crystal_high_range 5, crystal_green 4, checkpoint 3, flashlight 3, green_light 1, blue_light 1, next_level 1 |
| level2 | 20 | 1:20 | 1:20 | none:2, halo:9, halo1:9 | 47.5..454 | checkpoint 4, green_light 4, crystal_green 3, crystal_high_range 2, gob_face 2, red_light 2, next_level 1, decoration 1, torch 1 |
| level3 | 38 | 0:15, 1:23 | 0:20, 1:18 | none:22, halo:4, halo1:12 | 47.5..353.5 | flashlight 15, torch 9, shooter 5, checkpoint 3, red_light_high_range 2, deco 2, red_light 2 |
| menu | 7 | 0:1, 1:6 | 1:7 | halo:2, halo1:5 | 181..284.5 | ground_fire 3, blue_light 2, green_light_menu 1, cursor 1 |
| pvp_lv1 | 1 | 1:1 | 1:1 | halo1:1 | 227.5 | torch 1 |
| pvp_lv2 | 9 | 1:9 | 0:4, 1:5 | none:4, halo:2, halo1:3 | 125.5..355 | shooter 4, torch_higher_range 3, crystal_green 2 |
| pvp_lv3 | 5 | 0:1, 1:4 | 0:1, 1:4 | none:1, halo:2, halo1:2 | 301..454 | green_light 2, gob_face 2, flashlight 1 |
| pvp_lv4 | 2 | 0:2 | 0:2 | none:2 | 353.5 | flashlight 2 |
| pvp_lv5 | 2 | 1:2 | 0:2 | halo1:2 | 3530 | huge_ambient_light 2 |
| pvp_lv6 | 5 | 1:5 | 1:5 | halo1:4, halo2:1 | 145..178 | foggy_torch 4, foggy_crystal_green 1 |
| videoModes | 2 | 0:1, 1:1 | 1:2 | halo:2 | 181..382 | crystal_high_range 1, picker 1 |

- `Light@static` never disagrees with the entity's `static`, so the [2013] forcing rule (`light->staticLight = staticEntity`) makes no difference.
- The inactive `<Light>` blocks (2,466 instances) carry defaults and are discarded.

### 5.4 Particles per scene (all 124 systems are one per entity; `repeat=0` endless; `allAtOnce=0`)

| scene | systems | by entity | bitmaps | nearest .par (+#field diffs) |
|---|---|---|---|---|
| arena_select | 2 | cursor 1, ground_fire 1 | explosion.JPG, flash.bmp | light_spell_infinite(+6), torch_fire(+2) |
| gameover | 1 | clouds 1 | fog.dds | clouds(+3) |
| level1 | 36 | potion_small 13, torch 10, shooter 9, checkpoint 3, next_level 1 | particle.png 13, explosion.JPG 19, black_opaque 3, blood 1 | life_shadow_beam(+1), torch_fire(+10), lava(+1), shadow_beam_wide(+9), portal_red(+9) |
| level2 | 22 | potion_small 15, checkpoint 4, next_level 1, decoration 1, torch 1 | particle 15, black_opaque 4, blood 2, explosion.JPG 1 | same families |
| level3 | 45 | potion_small 26, torch 9, shooter 5, checkpoint 3, deco 2 | particle 26, explosion.JPG 14, black_opaque 3, blood 2 | same |
| menu | 5 | ground_fire 3, fog_menu 1, cursor 1 | explosion.JPG 3, flash.bmp 1, fog.dds 1 | torch_fire(+2), fog_menu (exact), light_spell_infinite(+6) |
| pvp_lv1 | 1 | torch 1 | explosion.JPG | torch_fire(+10) |
| pvp_lv2 | 7 | shooter 4, torch_higher_range 3 | explosion.JPG 7 | lava(+1), torch_fire(+10) |
| pvp_lv3/4/5 | 0 | | | |
| pvp_lv6 | 4 | foggy_torch 4 | explosion.JPG | torch_fire(+10) |
| videoModes | 1 | picker 1 | flash.bmp | light_spell_infinite(+6) |

**Embedded particle systems are scaled copies of `.par` templates, not references.**
- The differences are consistent uniform factors on the scale-affected fields: boundingSphere, size, randomizeSize, growth, minSize, maxSize, Direction, Gravity, RandomizeDir and RandStartPoint.
  - Examples: torch ×1.2, checkpoint ×1.44, king ×0.72, enemy_blood ×0.24, combo_fire_ball ×2.1504.
- Some copies also have a hand-moved StartPoint, for example shooter z=−6 and potion z=−2.
- No script loads a `.par` (no `.par` literal in any .as file), so **`effects/*.par` are editor-only templates**. The port should use the embedded values.
- `fall.par` references the missing `fall.mp3` (only `fall.ogg` exists), but `fall.ent` embeds a `jumpfx.par` copy with no sound, so nothing breaks.

---

## 6. EntityName × scene instance counts (only names used at least once)

Columns: arena = arena_select, lv1..3 = level1..3, p1..p6 = pvp_lv1..6, video = videoModes.

| EntityName | .ent | total | arena | gameover | lv1 | lv2 | lv3 | menu | p1 | p2 | p3 | p4 | p5 | p6 | video | ETHCallback_ |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| spawn | NO | 271 |  |  | 48 | 87 | 76 |  | 6 | 8 | 10 | 8 | 17 | 11 |  |  |
| single_tile.ent | Y | 183 |  |  | 27 | 25 | 104 |  | 4 | 8 | 3 | 2 | 4 | 6 |  |  |
| tile01.ent | Y | 163 |  |  | 114 | 2 | 44 |  |  | 3 |  |  |  |  |  |  |
| wall00.ent | Y | 131 |  |  | 76 | 5 | 50 |  |  |  |  |  |  |  |  |  |
| floor03_2.ent | Y | 112 |  |  |  | 22 | 36 |  | 16 | 2 |  | 16 |  | 20 |  |  |
| instant_death3.ent | Y | 99 |  |  |  |  | 65 |  | 26 | 8 |  |  |  |  |  |  |
| wall12.ent | Y | 81 |  |  |  | 40 | 37 |  |  |  |  |  | 4 |  |  |  |
| white_ground.ent | Y | 70 | 32 |  |  |  |  | 38 |  |  |  |  |  |  |  |  |
| ground.ent | Y | 65 |  |  |  |  | 41 |  | 2 | 2 |  | 12 |  | 8 |  |  |
| instant_death.ent | Y | 61 |  |  | 61 |  |  |  |  |  |  |  |  |  |  |  |
| potion_small.ent | Y | 54 |  |  | 13 | 15 | 26 |  |  |  |  |  |  |  |  | potions.as:68 |
| invisible_wall.ent | Y | 52 |  |  |  |  | 34 |  |  |  |  | 4 | 14 |  |  |  |
| black.ent | Y | 51 |  |  | 51 |  |  |  |  |  |  |  |  |  |  |  |
| wall01.ent | Y | 46 |  |  | 27 | 1 | 8 |  |  | 10 |  |  |  |  |  |  |
| pilar02.ent | Y | 45 |  |  |  | 5 | 38 |  | 1 |  |  |  |  | 1 |  |  |
| falling_bridge.ent | Y | 43 |  |  |  |  | 16 |  | 8 | 2 | 13 | 4 |  |  |  | events.as:67 |
| npc_wall.ent | Y | 42 |  |  | 17 | 6 | 7 |  | 2 |  |  | 2 | 8 |  |  |  |
| arch01.ent | Y | 40 |  |  |  | 13 | 19 |  |  |  | 4 |  |  |  | 4 |  |
| wall03.ent | Y | 40 |  |  |  | 35 |  |  |  |  | 5 |  |  |  |  |  |
| black_entity.ent | Y | 37 |  |  | 3 | 4 | 29 |  |  |  |  |  |  | 1 |  |  |
| bloco_mario.ent | Y | 36 |  |  |  |  |  |  |  |  |  |  | 36 |  |  |  |
| tile03.ent | Y | 35 |  |  | 26 |  | 7 |  | 2 |  |  |  |  |  |  |  |
| half_ground.ent | Y | 34 |  |  |  |  | 29 |  | 2 | 3 |  |  |  |  |  |  |
| tile02.ent | Y | 33 |  |  | 13 | 3 | 17 |  |  |  |  |  |  |  |  |  |
| floor03.ent | Y | 32 |  |  |  | 13 | 15 |  |  |  |  |  |  |  | 4 |  |
| small_ground_tile.ent | Y | 32 |  |  |  |  |  |  |  | 32 |  |  |  |  |  |  |
| lava.ent | Y | 31 |  |  |  |  | 19 |  |  | 12 |  |  |  |  |  |  |
| wall11.ent | Y | 31 |  |  |  | 17 | 2 |  |  |  | 4 |  | 4 |  | 4 |  |
| wall02.ent | Y | 28 |  |  |  | 23 |  |  |  |  | 5 |  |  |  |  |  |
| single_tile_short_shadow.ent | Y | 26 |  |  |  |  |  |  |  |  |  |  |  | 26 |  |  |
| tile_shadow.ent | Y | 24 |  |  | 17 | 2 | 2 |  |  | 3 |  |  |  |  |  |  |
| half_wall02.ent | Y | 23 |  |  | 8 | 3 | 5 |  |  | 5 |  |  |  |  | 2 |  |
| window02.ent | Y | 23 |  |  |  | 11 | 4 |  |  |  | 8 |  |  |  |  |  |
| cliff_right.ent | Y | 22 |  |  |  |  | 11 |  | 3 | 5 |  | 1 |  | 2 |  |  |
| flashlight.ent | Y | 21 |  |  | 3 |  | 15 |  |  |  | 1 | 2 |  |  |  |  |
| half_wall01.ent | Y | 21 |  |  | 7 | 4 | 3 |  |  | 5 |  |  |  |  | 2 |  |
| invisible_wall_small.ent | Y | 21 |  |  |  | 5 | 12 |  |  |  |  |  |  | 4 |  |  |
| torch.ent | Y | 21 |  |  | 10 | 1 | 9 |  | 1 |  |  |  |  |  |  |  |
| wall13.ent | Y | 21 |  |  |  | 17 |  |  |  |  | 4 |  |  |  |  |  |
| floor01.ent | Y | 20 |  |  | 20 |  |  |  |  |  |  |  |  |  |  |  |
| cliff_left.ent | Y | 18 |  |  |  |  | 7 |  | 3 | 5 |  | 1 |  | 2 |  |  |
| shooter.ent | Y | 18 |  |  | 9 |  | 5 |  |  | 4 |  |  |  |  |  | lavaShooter.as:43 |
| window01.ent | Y | 17 |  |  | 15 |  |  |  |  | 2 |  |  |  |  |  |  |
| help | NO | 16 |  |  | 11 |  | 5 |  |  |  |  |  |  |  |  | main.as:147 |
| wall10.ent | Y | 16 |  |  |  | 6 | 2 |  |  |  | 4 |  |  |  | 4 |  |
| floor02.ent | Y | 14 |  |  | 14 |  |  |  |  |  |  |  |  |  |  |  |
| story | NO | 14 |  |  | 5 | 4 | 5 |  |  |  |  |  |  |  |  | main.as:163 |
| barrel.ent | Y | 13 |  |  |  |  |  | 13 |  |  |  |  |  |  |  |  |
| statue.ent | Y | 12 |  |  |  |  | 10 |  | 1 |  |  | 1 |  |  |  |  |
| single_tile_shadow.ent | Y | 11 |  |  |  |  |  |  |  | 11 |  |  |  |  |  |  |
| wall07.ent | Y | 11 |  |  |  | 8 | 2 |  |  |  |  |  | 1 |  |  |  |
| checkpoint.ent | Y | 10 |  |  | 3 | 4 | 3 |  |  |  |  |  |  |  |  | main.as:170 |
| dirt.ent | Y | 10 |  |  | 9 | 1 |  |  |  |  |  |  |  |  |  |  |
| pilar02 | NO | 10 |  |  |  | 10 |  |  |  |  |  |  |  |  |  |  |
| wall06.ent | Y | 10 |  |  |  | 7 | 2 |  |  |  |  |  | 1 |  |  |  |
| crystal_green.ent | Y | 9 |  |  | 4 | 3 |  |  |  | 2 |  |  |  |  |  |  |
| instant_death2.ent | Y | 9 |  |  |  |  |  |  |  | 9 |  |  |  |  |  |  |
| red_light.ent | Y | 9 |  |  | 5 | 2 | 2 |  |  |  |  |  |  |  |  |  |
| crystal_high_range.ent | Y | 8 |  |  | 5 | 2 |  |  |  |  |  |  |  |  | 1 |  |
| environment | NO | 7 |  |  |  | 1 | 1 |  | 1 |  | 1 | 1 | 1 | 1 |  |  |
| green_light.ent | Y | 7 |  |  | 1 | 4 |  |  |  |  | 2 |  |  |  |  |  |
| thorns.ent | Y | 7 |  |  |  |  |  |  | 7 |  |  |  |  |  |  |  |
| plant_big.ent | Y | 6 |  |  |  |  | 6 |  |  |  |  |  |  |  |  |  |
| thumbnail | NO | 6 | 6 |  |  |  |  |  |  |  |  |  |  |  |  | menu.as:362 |
| crystal.ent | Y | 5 |  |  | 5 |  |  |  |  |  |  |  |  |  |  |  |
| play_sound.ent | Y | 5 |  |  |  | 3 | 2 |  |  |  |  |  |  |  |  |  |
| summon | NO | 5 |  |  |  |  | 4 |  |  |  |  |  |  | 1 |  |  |
| tree05.ent | Y | 5 |  |  |  |  | 3 |  | 1 |  |  | 1 |  |  |  |  |
| wall05.ent | Y | 5 |  |  |  | 3 | 2 |  |  |  |  |  |  |  |  |  |
| cano.ent | Y | 4 |  |  |  |  |  |  |  |  |  |  | 4 |  |  |  |
| face_no_light.ent | Y | 4 |  |  |  |  |  |  |  |  |  |  |  |  | 4 |  |
| foggy_torch.ent | Y | 4 |  |  |  |  |  |  |  |  |  |  |  | 4 |  |  |
| gob_face.ent | Y | 4 |  |  |  | 2 |  |  |  |  | 2 |  |  |  |  |  |
| ground_fire.ent | Y | 4 | 1 |  |  |  |  | 3 |  |  |  |  |  |  |  |  |
| tree01.ent | Y | 4 |  |  |  |  | 3 |  |  |  |  | 1 |  |  |  |  |
| tree02.ent | Y | 4 |  |  |  |  | 2 |  | 1 |  |  | 1 |  |  |  |  |
| tree04.ent | Y | 4 |  |  |  |  | 3 |  | 1 |  |  |  |  |  |  |  |
| wall04.ent | Y | 4 |  |  |  | 2 | 2 |  |  |  |  |  |  |  |  |  |
| arrow_sign.ent | Y | 3 |  |  |  |  | 3 |  |  |  |  |  |  |  |  |  |
| blue_light.ent | Y | 3 |  |  | 1 |  |  | 2 |  |  |  |  |  |  |  |  |
| brick.ent | Y | 3 |  |  |  |  |  |  |  |  | 3 |  |  |  |  |  |
| pilar01.ent | Y | 3 |  |  |  | 3 |  |  |  |  |  |  |  |  |  |  |
| play | NO | 3 |  |  | 1 | 1 | 1 |  |  |  |  |  |  |  |  | setupScene.as:43 |
| torch_higher_range.ent | Y | 3 |  |  |  |  |  |  |  | 3 |  |  |  |  |  |  |
| tree03.ent | Y | 3 |  |  |  |  | 3 |  |  |  |  |  |  |  |  |  |
| cursor.ent | Y | 2 | 1 |  |  |  |  | 1 |  |  |  |  |  |  |  | menu.as:232 |
| deco | NO | 2 |  |  |  |  | 2 |  |  |  |  |  |  |  |  |  |
| devil.ent | Y | 2 |  |  |  |  |  | 2 |  |  |  |  |  |  |  |  |
| green_light_menu.ent | Y | 2 | 1 |  |  |  |  | 1 |  |  |  |  |  |  |  |  |
| huge_ambient_light.ent | Y | 2 |  |  |  |  |  |  |  |  |  |  | 2 |  |  |  |
| next_level | NO | 2 |  |  | 1 | 1 |  |  |  |  |  |  |  |  |  | main.as:194 |
| play_sound | NO | 2 |  |  |  | 2 |  |  |  |  |  |  |  |  |  |  |
| red_light_high_range.ent | Y | 2 |  |  |  |  | 2 |  |  |  |  |  |  |  |  |  |
| step | NO | 2 |  |  |  |  |  |  | 2 |  |  |  |  |  |  |  |
| clouds.ent | Y | 1 |  | 1 |  |  |  |  |  |  |  |  |  |  |  | environment.as:43 |
| como_jogar / creditos / melhores_tempos / novo_jogo / opcoes_de_video / sair / versus | NO | 1 each |  |  |  |  |  | 1 each |  |  |  |  |  |  |  |  |
| decoration | NO | 1 |  |  |  | 1 |  |  |  |  |  |  |  |  |  |  |
| dontFall / fontFall / event01 / spawn2 | NO | 1 each |  |  |  |  | 1 each |  |  |  |  |  |  |  |  | event01: events.as:43 |
| fog_menu.ent / gamelogo.ent | Y | 1 each |  |  |  |  |  | 1 each |  |  |  |  |  |  |  |  |
| foggy_crystal_green.ent | Y | 1 |  |  |  |  |  |  |  |  |  |  |  | 1 |  |  |
| mario_bg.ent | Y | 1 |  |  |  |  |  |  |  |  |  |  | 1 |  |  |  |
| picker | NO | 1 |  |  |  |  |  |  |  |  |  |  |  |  | 1 | videoModes.as:58 |

**Spawn marker contents** (CustomData `name` of `spawn`/`spawn2`; "complete" = has `int complete=0`):

| name | lv1 | lv2 | lv3 | p1 | p2 | p3 | p4 | p5 | p6 |
|---|---|---|---|---|---|---|---|---|---|
| bruxo (complete) | 1 | 1 | 1 (+1 spawn2) | 1 | 1 | 1 | 1 | 1 | 1 |
| princess (complete) | | | | 1 | 1 | 1 | 1 | 1 | 1 |
| warrior | 30 | 17 | 11 | 3 | 4 | | | 6 | |
| knight | 15 | 11 | 19 | | 2 | 4 | | 5 | 4 |
| minion | | 36 | 31 | | | | 2 | | |
| impy | | 11 | 4 | | | | 2 | 2 | 4 |
| paladin | 2 | 10 | 3 | | | | 2 | 2 | |
| master_knight | | 1 | 7 | 1 | | 4 | | | 1 |

The king is never a spawn marker. `event01` spawns `king.ent` (events.as:58).

**Logic markers:**
- **story / help:** the `story` name keys into `data.enml` `global` (story01–04, soldados, fun, comboTip, portalToCastle, flyingWall, annoying, bridge, warning, nights, wisdom). There are 16 `help.message` strings, which are literal Latin-1 Portuguese text.
- **play:** `name=fase.mp3` in level1–3.
- **next_level:** `name=level2.esc` (level1) and `level3.esc` (level2).
- **thumbnail:** `name` 1–6 → `data.enml` `arena1..6` via `"arena"+number` (menu.as:328), plus `title` Obelisco/Inferno/Cova/Vale/Templo Sagrado/Neblina and `score` 720000/900000 on 2 of them.
- **environment:** `bgColor` uint 0xFF4D4D4D (4283256141) ×4, 0xFF0A0A0A (4278848010) ×2, 0xFF000000 ×1; `bgImage` planets.png ×3, dawn.png ×1 (loaded from `entities/`, environment.as:129); `name` clouds.ent ×4, dawn.ent, fog.ent. pvp_lv5 has no name.

---

## 7. CustomData census

**Scene instances (453 variables):**

| key | type | instances | values | carried by |
|---|---|---|---|---|
| name | string | 312 | warrior 71, minion 69, knight 60, impy 24, paladin 19, master_knight 14, bruxo 10, horror.mp3 7, princess 6, clouds.ent 4, fase.mp3 3, 1..6, ENML story keys, level2.esc, level3.esc, dawn.ent, fog.ent, none (36 distinct) | spawn 271, story 14, thumbnail 6, environment 6, play_sound.ent 5, play 3, next_level 2, play_sound 2, deco 1, spawn2, summon |
| hp | int | 54 | 20 | potion_small.ent |
| coolDown | uint | 18 | 2500 | shooter.ent |
| force | float | 18 | 400 | shooter.ent |
| complete | int | 16 | 0 | spawn 15, spawn2 1 |
| message | string | 16 | 11 distinct Portuguese hints (Latin-1, `&apos;`) | help |
| bgColor | uint | 7 | 4283256141 ×4, 4278848010 ×2, 4278190080 ×1 | environment |
| bgImage | string | 4 | planets.png ×3, dawn.png | environment |
| title | string | 6 | 6 arena names | thumbnail |
| score | uint | 2 | 900000, 720000 | thumbnail |

**`.ent` files (93 variables in 24 files):**
- `bruxo`/`princess`/`vert_bruxo` carry the player block: coolDown 200, currentDir, damage 20, forceX/Y, hp 100, jumpForce 507 (390 in vert_bruxo), jumps, knockBackX/Y, lastSwordAttack, maxHp/maxMp 100, maxJumps 2, mp 100, playerId 0/1, speed 150, stride 100, touchingGround, walking, xp; princess also has waitBeforeAttack.
- Weapons carry `dontCollide` (int ×7, uint ×3) and `hit` (string ×7: blood.ent or enemy_blood.ent, the effect `.ent` spawned on hit, doDamage.as:225).
- `showUpSfx` (master_knight, vert_master_knight = creature_show_up.mp3; paladin = paladin_appear.mp3).
- Potion `hp` 20/50; shooter `coolDown`/`force`; `environment.ent`/`help.ent`/`next_level.ent`/`thumbnail.ent` hold placeholders (`none`/`0`).

**Rules:**
- Types used: int/uint/float/string only.
- Scalars are always element text.
- Writer order is alphabetical by name (consistent with a `std::map`).
- Scripts add more keys at runtime (for example `waitBeforeAttack`, `lastTimeAlive`, `pvpMode`, `hitBy`, `ownerID`, `newGame`, `scene`, `lastButton`, `chaseSfx`). These are never in the files, except in the runtime-written `checkpoint.esc`.

---

## 8. Every `.ent` (144)

Key: *collid* = collidable, *cut* = SpriteCut cols x rows, *pivot* = PivotAdjust x,y, *frame* = image size / cut, *Light* only if active (r = range, pos = offset, st = Light static, sh = castShadows, h = HaloBitmap, hs = haloSize, hb = haloBrightness), *Collision* "zero" = size 0,0,0, *Particles* `~x.par` = nearest template (+N differing fields), *placed* = scene instances, *script refs* = literal count / dyn = dynamic name, *callback* = `ETHCallback_<name>` location.

| ent | type | static | collid | aLight | cShadow | blend | lDepth | startFr | cut | pivot | Sprite (w x h -> frame) | Normal | Light (active) | Collision | Particles | CustomData | placed | refs | callback |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| arch01 | 0 | 1 | 0 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | arch.png 256x256 | arch_nm.png | - | zero | - | - | 40 | - | - |
| arrow_sign | 0 | 1 | 0 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | arrow_sign.png 126x143 | arrow_sign_nm.png | - | zero | - | - | 3 | - | - |
| barrel | 2 | 1 | 1 | 1 | 1 | 2 | 0 | 0 | 1x1 | 0,-14 | barril.png 48x58 | barril_nm.bmp | - | pos(0,-1,0) size(40,27,36) | - | - | 13 | - | - |
| big_explosion | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | r=167.5 col=(1,0.5,0.3) pos=(0,0,18) st0 sh0 | zero | explosion.png n=12 rep=1 =big_explosion.par | - | 0 | lit 1 | - |
| black | 5 | 1 | 0 | 0 | 0 | 2 | 1 | 0 | 1x1 | 0,0 | black2.bmp 128x128 | - | - | zero | - | - | 51 | - | - |
| black_entity | 0 | 1 | 0 | 0 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | black.bmp 64x64 | - | - | zero | - | - | 37 | - | - |
| bloco_mario | 0 | 1 | 1 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | Bloco_Bitmap.png 64x64 | Bloco_Normal.png | - | size(64,64,26) | - | - | 36 | - | - |
| blood | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | r=76 col=(0.3,0.7,1) pos=(0,0,18) | zero | explosion.png n=12 rep=1 ~enemy_sword_hit(+7) | - | 0 | dyn (hit) | - |
| blue_light | 0 | 1 | 0 | 0 | 0 | 2 | 0 | 0 | 1x1 | 0,-16 | - | - | r=232 col=(0.3,0.7,1) pos=(2,10,0) st1 sh1 h=halo1.bmp hs288 hb0.3 | zero | - | - | 3 | - | - |
| branches | 0 | 1 | 0 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | roots.bmp 256x256 | roots_normal.bmp | - | zero | - | - | 0 | UNREACHABLE | - |
| brick | 0 | 0 | 1 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | single_stone.png 64x32 | single_stone_nm.png | - | size(64,32,11) | - | - | 3 | - | - |
| bridge_fall | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | - | zero | fog.dds n=9 rep=1 =bridgefall.par | - | 0 | lit 1 | - |
| bruxo | 0 | 0 | 1 | 1 | 0 | 2 | 0 | 8 | 4x4 | 0,0 | bruxo.png 128x192 -> 32x48 | bruxo_nm.png | - | pos(0,4,0) size(14,36,8) | black_opaque.png n=45 rep=0 ~shadow_beam(+3) | player block, playerId=0 | 0 | lit 1, dyn (spawn) | controlCharacters.as:270 |
| bruxo_dead | 0 | 0 | 1 | 1 | 0 | 2 | 0 | 8 | 4x4 | 0,0 | bruxo.png -> 32x48 | bruxo_nm.png | - | pos(0,4,0) size(14,36,8) | as bruxo | - | 0 | lit 1 (gameover.as:58) | - |
| buttons | 0 | 0 | 1 | 1 | 0 | 0 | 0 | 0 | 1x8 | 0,0 | menu_buttons.png 512x512 -> 512x64 | menu_nm_buttons.png | - | pos(-18,4,0) size(369,25,25) | - | - | 0 | UNREACHABLE (menu uses labels) | - |
| cano | 0 | 1 | 1 | 1 | 0 | 0 | 0 | 0 | 2x1 | 0,0 | Tubo_Bitmap.png 256x128 -> 128x128 | Tubo_Normal.png | - | size(128,110,26) | - | - | 4 | - | - |
| checkpoint | 0 | 1 | 0 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | Lapide_Bitmap2.png 48x68 | lapide_Normal.png | r=47.5 col=(0.3,1,0.7) pos=(0,4,8) st1 sh1 h=halo1 hs184 hb0.2 | zero | black_opaque.png n=45 ~shadow_beam_wide(+9, x1.44) | - | 10 | - | main.as:170 |
| checkpoint_effect | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | - | zero | particle_harder.png n=5 rep=1 =checkpoint.par | - | 0 | lit 1 | - |
| cliff_left | 0 | 1 | 1 | 1 | 0 | 2 | 0 | 0 | 2x1 | 0,0 | cliff_left.png 256x256 -> 128x256 | ground_nm.png | - | pos(-54,0,0) size(20,250,16) | - | - | 18 | - | - |
| cliff_right | 0 | 1 | 1 | 1 | 0 | 2 | 0 | 1 | 2x1 | 0,0 | cliff_left.png -> 128x256 | ground_nm.png | - | pos(54,0,0) size(20,250,16) | - | - | 22 | - | - |
| clouds | 5 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | - | zero | fog.dds n=70 rep=0 ~clouds(+3) | - | 1 | lit 1, dyn (environment) | environment.as:43 |
| combo_fire_ball | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | r=125.5 col=(0.7,0.7,1) pos=(0,0,16) h=halo1 hs108 | size(28,28,5) | explosion.JPG n=23 rep=7 ~combo_fire_ball(+8, x2.15) | uint dontCollide=0 | 0 | lit 1 | spells.as:118 |
| combo_sword | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | r=169 col=(2,2,1) pos=(19,8,16) sh1 h=halo1 hs170 hb0.3 | pos(-4,4,0) size(103,61,12) | sword.png n=16 rep=1 ~sword01(+14) | dontCollide, hit=enemy_blood.ent | 0 | lit 1 | swords.as:128 |
| crate | 0 | 0 | 1 | 1 | 1 | 2 | 0 | 0 | 2x1 | 0,0 | crates.png 128x64 -> 64x64 | - | - | size(64,64,14) | - | - | 0 | UNREACHABLE | - |
| crystal | 0 | 1 | 0 | 0 | 0 | 2 | 0 | 0 | 1x1 | 0,-16 | sprite.png 32x32 | - | r=254.5 col=(0.3,0.7,1) pos=(2,10,0) st1 sh1 h=halo hs146 hb0.6 | zero | - | - | 5 | - | - |
| crystal_green | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | sprite_green.png 32x32 | - | r=236.5 col=(0.3,1,0.7) pos=(2,-4,0) h=halo hs126 hb0.45 | zero | - | - | 9 | - | - |
| crystal_high_range | 0 | 1 | 0 | 0 | 0 | 2 | 0 | 0 | 1x1 | 0,-16 | sprite.png | - | r=382 (else = crystal) | zero | - | - | 8 | - | - |
| cursor | 2 | 0 | 1 | 1 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | r=181 col=(1,0.7,1) st0 sh1 h=halo hs102 hb0.55 | pos(0,0,-64) size(31,31,148) | flash.bmp n=15 rep=0 ~light_spell_infinite(+6, x0.6) | - | 2 | lit 1 | menu.as:232 |
| dark_sword | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | - | pos(-1,12,0) size(89,41,5) | black_sword.dds n=16 rep=1 ~enemy_dark_sword(+1) | dontCollide, hit=blood.ent | 0 | lit 1 | swords.as:114 |
| dawn | 5 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | - | zero | - | - | 0 | dyn (environment name) | environment.as:145 |
| devil | 2 | 1 | 0 | 1 | 1 | 0 | 0 | 0 | 1x1 | 0,-13 | devil_statue.png 139x145 | nm_devil_statue.png | - | zero | - | - | 2 | - | - |
| dirt | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | dirt.dds 128x128 | - | - | zero | - | - | 10 | - | - |
| enemy_blood | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | r=76 col=(1,0.5,0.3) pos=(0,0,18) | zero | explosion.png n=12 ~sword_hit(+7, x0.24) | - | 0 | dyn (hit) | - |
| enemy_sword | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | - | pos(-1,0,0) size(46,19,5) | sword.png n=16 ~enemy_sword(+10, x0.64) | dontCollide, hit=blood.ent | 0 | lit 3 | swords.as:109 |
| enemy_sword_beam | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | - | zero | fog.dds n=9 =enemy_sword_beam.par | - | 0 | lit 1 | - |
| environment | 0 | 0 | 0 | 0 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | - | - | - | zero | - | bgColor=0, name=none | 0 | UNREACHABLE | - |
| event | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | - | size(128,256,10) | - | - | 0 | UNREACHABLE | - |
| explosion | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | r=167.5 col=(1,0.5,0.3) pos=(0,0,18) | zero | explosion.png n=12 ~sword_hit(+7) | - | 0 | lit 2 | - |
| face_no_light | 0 | 1 | 0 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | face.jpg 256x256 | face_norm.jpg | - | zero | - | - | 4 | - | - |
| fade_out_beam | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | - | zero | black_opaque.png n=45 rep=1 =fade_out_shadow_beam.par | - | 0 | lit 9 | - |
| fade_out_beam_large | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | - | zero | particle_harder.png n=45 ~fade_out_shadow_beam_hard(+8, x1.8) | - | 0 | lit 1 | - |
| fall | 0 | 0 | 0 | 0 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | - | - | - | zero | fog.dds n=9 =jumpfx.par | - | 0 | lit 1 | - |
| falling_bridge | 0 | 0 | 1 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | single_stone.png 64x32 | single_stone_nm.png | - | size(64,32,11) | - | - | 43 | - | events.as:67 |
| fire_ball | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | r=125.5 col=(1,0.7,0.3) pos=(0,0,16) h=halo1 hs82 | size(22,22,5) | explosion.JPG n=19 rep=5 =fireball.par | uint dontCollide | 0 | lit 3 | spells.as:113 |
| fire_shoot | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | r=125.5 col=(1,0.5,0.3) h=halo1 hs82 | size(22,22,5) | explosion.JPG n=19 rep=0 =fire_shoot.par | uint dontCollide | 0 | lit 1 | lavaShooter.as:64 |
| flashlight | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | cone.png 64x64 | - | r=353.5 col=(1,1,1) pos=(0,0,56) st0 sh0 | zero (collid=1) | - | - | 21 | lit 1 (deleted by setupScene) | - |
| floor01 | 0 | 1 | 1 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | floor.png 256x64 | floor_nm.png | - | size(256,64,21) | - | - | 20 | - | - |
| floor02 | 0 | 1 | 1 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | floor_half.png 128x64 | floor_half_nm.png | - | size(128,64,12) | - | - | 14 | - | - |
| floor03 | 0 | 1 | 1 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | floor03.png 256x256 | floor03_nm.png | - | size(256,256,43) | - | - | 32 | - | - |
| floor03_2 | 0 | 1 | 0 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | floor03_2.jpg 256x256 | nm_white_ground.jpg (actually a BMP) | - | zero | - | - | 112 | - | - |
| fog | 5 | 0 | 0 | 1 | 0 | 0 | 1 | 0 | 1x1 | 0,0 | - | - | - | zero | fog.dds n=40 rep=0 ~fog(+1) | - | 0 | dyn (environment) | environment.as:115 |
| fog_menu | 2 | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 1x1 | 0,0 | plant_small.png 43x25 (absent in menu instance) | - | - | zero | fog.dds n=40 =fog_menu.par | - | 1 | - | - |
| foggy_crystal_green | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | sprite_green.png | - | r=145 col=(0.3,1,0.7) pos=(2,-10,0) h=halo2 hs368 hb0.45 | zero | - | - | 1 | - | - |
| foggy_torch | 0 | 1 | 0 | 1 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | torch_small.png 28x28 | light01_normal.png | r=178 col=(1,0.7,0.3) pos=(0,-14,8) h=halo1 hs476 hb0.35 | zero | explosion.JPG n=18 ~torch_fire(+10, x1.2) | - | 4 | - | - |
| gamelogo | 2 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | gamelogo.png 567x145 | - | - | zero | - | - | 1 | - | - |
| gob_face | 0 | 1 | 0 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | face.jpg 256x256 | face_norm.jpg | r=301 col=(1,1,0.7) pos=(1,62,22) h=halo hs164 | size(640,480,1) but collid=0 | - | - | 4 | - | - |
| green_light | 0 | 1 | 0 | 0 | 0 | 2 | 0 | 0 | 1x1 | 0,-16 | - | - | r=454 col=(0.3,1,0.7) pos=(2,10,0) h=halo1 hs146 hb0 | zero | - | - | 7 | - | - |
| green_light_menu | 0 | 1 | 0 | 0 | 0 | 2 | 0 | 0 | 1x1 | 0,-16 | - | - | r=284.5 col=(0.3,1,0.7) h=halo hs90 hb1 | zero | - | - | 2 | - | - |
| ground | 0 | 1 | 1 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | ground.png 256x256 | ground_nm.png | - | size(256,250,12) | - | - | 65 | - | - |
| ground_fire | 2 | 1 | 1 | 1 | 0 | 0 | 0 | 0 | 1x1 | 0,-8 | torch_small.png | light01_normal.png | r=256 col=(1,0.5,0.3) pos=(0,0,16) h=halo1 hs286 hb0.65 | pos(0,-4,0) size(28,22,13) | explosion.JPG n=18 ~torch_fire(+2) | - | 4 | - | - |
| half_ground | 0 | 1 | 1 | 1 | 0 | 2 | 0 | 0 | 4x1 | 0,0 | half_ground.png 256x256 -> 64x256 | ground_nm.png | - | size(64,250,19) | - | - | 34 | - | - |
| half_wall01 | 0 | 1 | 0 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | half_wall02.png 128x128 | wall__height_nm.png | - | zero | - | - | 21 | - | - |
| half_wall02 | 0 | 1 | 0 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | half_wall01.png 128x128 | wall__height_nm.png | - | zero | - | - | 23 | - | - |
| help | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | - | zero | - | message=none | 0 | UNREACHABLE (label `help` used) | main.as:147 |
| hit_fail | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | r=83.5 col=(1,0.7,0.6) h=halo1 hb0.6 | zero | explosion.png n=4 ~hit_fail(+7) | - | 0 | lit 1 | - |
| huge_ambient_light | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | r=3530 col=(0.4,0.4,0.4) pos=(0,0,44) st1 sh0 h=halo1 hb0 | zero | - | - | 2 | - | - |
| impy | 0 | 0 | 1 | 1 | 0 | 0 | 0 | 4 | 4x4 | 0,0 | impy.png 128x192 -> 32x48 | impy_nm.png | - | size(22,43,13) | - | - | 0 | dyn (spawn) | controlCharacters.as:543 |
| instant_death | 5 | 0 | 1 | 0 | 0 | 2 | 1 | 0 | 1x1 | 0,0 | black.bmp 64x64 | - | - | size(64,64,28) | - | - | 61 | lit 2 | - |
| instant_death2 | 0 | 1 | 1 | 0 | 0 | 2 | 1 | 0 | 1x1 | 0,0 | - | - | - | size(256,256,37) | - | - | 9 | lit 2 | - |
| instant_death3 | 5 | 0 | 1 | 0 | 0 | 2 | 1 | 0 | 1x1 | 0,0 | - | - | - | size(64,64,28) | - | - | 99 | lit 2 | - |
| invisible_wall | 0 | 1 | 1 | 0 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | - | - | - | size(256,256,16) | - | - | 52 | lit 1 | - |
| invisible_wall_small | 0 | 1 | 1 | 0 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | - | - | - | size(128,256,16) | - | - | 21 | - | - |
| jumpfx | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | - | zero | fog.dds n=9 =jumpfx.par | - | 0 | lit 1 | - |
| king | 0 | 0 | 1 | 1 | 0 | 2 | 0 | 4 | 4x4 | 0,0 | king.png 128x192 -> 32x48 | king_nm.png | r=122.5 col=(1,0.4,0.2) | size(23,48,20) | explosion.JPG n=19 ~king_beam(+9, x0.72) | - | 0 | lit 1 (events.as:58) | controlCharacters.as:645 |
| knight | 0 | 0 | 1 | 1 | 0 | 0 | 0 | 4 | 4x4 | 0,0 | knight.png 128x192 -> 32x48 | knight_nm.png | - | size(22,41,7) | - | - | 0 | dyn | controlCharacters.as:519 |
| lava | 0 | 1 | 0 | 0 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | lava.jpg 128x128 | - | - | zero | - | - | 31 | - | - |
| lava_drops | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | - | zero | explosion.JPG n=9 =lava_drops.par | - | 0 | lit 1 | - |
| light_spell | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | r=319 col=(0.6,0.6,1) sh1 h=halo hs100 hb0.7 | zero | flash.bmp n=15 rep=36 ~light_spell(+6, x0.8) | - | 0 | lit 1 | spells.as:83 |
| mario_bg | 0 | 1 | 0 | 0 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | mario_BG.png 1024x768 | - | - | zero | - | - | 1 | - | - |
| master_knight | 0 | 0 | 1 | 1 | 0 | 0 | 0 | 4 | 4x4 | 0,0 | master_knight.png 320x384 -> 80x96 | - | - | pos(0,4,0) size(57,84,8) | black_opaque n=45 ~shadow_beam_wide(+10) | showUpSfx=creature_show_up.mp3 | 0 | dyn | controlCharacters.as:567 |
| minion | 0 | 0 | 1 | 1 | 0 | 0 | 0 | 4 | 4x4 | 0,0 | LOS-Nac-Normal.png 128x192 -> 32x48 | - | - | size(32,48,9) | black_opaque n=45 ~shadow_beam(+9) | - | 0 | dyn | controlCharacters.as:495 |
| next_level | 0 | 1 | 0 | 1 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | passage.png 128x144 | passage_nm.png | r=182.5 col=(2,0,0) pos=(0,30,4) | zero | blood.png n=29 ~portal_red(+9) | name=none | 0 | UNREACHABLE (label used) | main.as:194 |
| nextlv | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | r=113.5 col=(2,0,0) h=halo hs150 hb0.15 | zero | flash.bmp n=17 =nextlv.par | - | 0 | UNREACHABLE | - |
| npc_wall | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | - | size(40,97,9) | - | - | 42 | lit 3 | - |
| paladin | 0 | 0 | 1 | 1 | 0 | 0 | 0 | 4 | 4x4 | 0,0 | paladin.png 128x192 -> 32x48 (+Gloss paladin_gloss.png) | paladino_nm.png | - | size(22,41,7) | - | showUpSfx=paladin_appear.mp3 | 0 | dyn | controlCharacters.as:591 |
| paladin_sword | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | r=149.5 col=(2,0,0) pos=(2,0,0) | pos(-1,0,0) size(64,19,5) | sword.png n=16 ~paladin_sword(+1) | dontCollide, hit=blood.ent | 0 | lit 2 | swords.as:133 |
| pilar | 2 | 1 | 1 | 1 | 1 | 2 | 0 | 0 | 1x1 | 0,-9 | pilar_ct_small.png 32x192 | pilar_normal_ct_small.bmp | - | pos(0,-4,0) size(28,25,167) | - | - | 0 | UNREACHABLE | - |
| pilar01 | 0 | 1 | 0 | 1 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | Pilar_Bitmap.png 66x256 | Pilar_Normal.png | - | size(640,480,1) collid=0 | - | - | 3 | - | - |
| pilar02 | 0 | 1 | 0 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | Pilar_Bitmap_opaque.png 64x256 | Pilar_Normal.png | - | size(640,480,1) collid=0 | - | - | 45 | - | - |
| plans_small | 0 | 0 | 0 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | plant_small.png 43x25 | plant_normal.png | - | zero | - | - | 0 | UNREACHABLE | - |
| plant_big | 0 | 0 | 0 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | plant_big.png 107x62 | plant_normal.png | - | zero | - | - | 6 | - | - |
| play_sound | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | - | zero | - | name=horror.mp3 | 5 (inert) | - | - |
| potion_large | 0 | 1 | 0 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | HP_Bitmap.png 16x16 | HP_Normal.png | - | size(16,16,5) collid=0 | particle_harder n=12 ~life_shadow_beam_bigger(+1) | int hp=50 | 0 | UNREACHABLE | potions.as:73 |
| potion_pick | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | r=88 col=(2,0,0) sh1 | zero | explosion.png n=12 ~potion_pick(+7) | - | 0 | lit 1 | - |
| potion_small | 0 | 1 | 0 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | skull.png 18x20 | skull_nm.png | - | size(16,16,5) collid=0 | particle.png n=12 ~life_shadow_beam(+1) | int hp=20 | 54 | - | potions.as:68 |
| princess | 0 | 0 | 1 | 1 | 0 | 2 | 0 | 8 | 4x4 | 0,0 | princess.png 128x192 -> 32x48 | princess_height.png | - | pos(0,4,0) size(14,36,8) | black_opaque n=45 ~shadow_beam(+3) | player block, playerId=1 | 0 | lit 2, dyn | controlCharacters.as:304 |
| red_light | 0 | 1 | 0 | 0 | 0 | 2 | 0 | 0 | 1x1 | 0,-16 | - | - | r=232 col=(1.3,0,0) pos=(2,10,0) h=halo hs146 hb0 | zero | - | - | 9 | - | - |
| red_light_high_range | 0 | 1 | 0 | 0 | 0 | 2 | 0 | 0 | 1x1 | 0,-16 | - | - | r=325 (else = red_light) | zero | - | - | 2 | - | - |
| shooter | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | r=125.5 col=(1,0.5,0.3) pos=(0,0,8) st1 sh0 | zero | explosion.JPG n=14 ~lava(+1) | coolDown=2500, force=400 | 18 | - | lavaShooter.as:43 |
| silent_sword | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | - | pos(-4,4,0) size(65,13,26) collid=0 | sword.png =sword01.par | dontCollide, hit=enemy_blood.ent | 0 | UNREACHABLE (commented out setupScene.as:169) | - |
| single_tile | 0 | 1 | 1 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | single_tile.png 32x32 | single_tile_nm.png | - | size(32,32,10) | - | - | 183 | - | - |
| single_tile_shadow | 0 | 1 | 1 | 1 | 1 | 2 | 0 | 0 | 1x1 | 0,0 | single_tile.png | single_tile_nm.png | - | size(32,32,10) | - | - | 11 | - | - |
| single_tile_short_shadow | 0 | 1 | 1 | 1 | 1 | 2 | 0 | 0 | 1x1 | 0,0 | single_tile.png | single_tile_nm.png | - | size(32,32,10) | - | - | 26 | - | - |
| small_ground_tile | 0 | 1 | 1 | 1 | 0 | 2 | 0 | 9 | 8x8 | 0,0 | ground2.png 256x256 -> 32x32 | ground_nm.png | - | size(32,32,13) | - | - | 32 | - | - |
| spawn | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | - | zero | - | - | 0 | UNREACHABLE (label `spawn` used) | - |
| statue | 0 | 0 | 0 | 0 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | statue.png 96x98 | - | - | zero | - | - | 12 | - | - |
| summon | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | - | zero | particle_harder n=5 =checkpoint.par | - | 0 | lit 3 | - |
| sword0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | r=169 col=(0.5,0.5,1) pos=(19,8,16) sh1 h=halo1 hs170 hb0.3 | pos(-4,4,0) size(65,38,12) | sword.png ~sword01(+4 colours) | dontCollide, hit=enemy_blood.ent | 0 | dyn ("sword"+player) | swords.as:120 |
| sword1 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | r=169 col=(1,0.5,1) (else = sword0) | same | =sword01.par | same | 0 | dyn | swords.as:124 |
| sword_beam | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | - | - | - | zero | black_opaque n=45 =sword_beam.par | - | 0 | lit 2 | - |
| thorns | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | thorn.dds 256x64 | - | - | zero | - | - | 7 | - | - |
| thumbnail | 2 | 0 | 1 | 1 | 1 | 0 | 0 | 0 | 4x4 | 0,-32 | thumbnails.png 256x256 -> 64x64 | thumb_normal.png | - | pos(0,32,0) size(64,64,68) | - | name=0, title=0 | 0 | UNREACHABLE (label used) | menu.as:362 |
| tile01 | 0 | 1 | 1 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | bar3.png 64x64 | bar3_nm.png | - | size(64,64,21) | - | - | 163 | - | - |
| tile02 | 0 | 1 | 1 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | bar4.png 64x256 | bar4_nm.png | - | size(64,256,31) | - | - | 33 | - | - |
| tile03 | 0 | 1 | 1 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | bar2.png 256x64 | bar2_nm.png | - | size(256,64,19) | - | - | 35 | - | - |
| tile_shadow | 0 | 1 | 1 | 1 | 1 | 2 | 0 | 0 | 1x1 | 0,0 | bar3.png | bar3_nm.png | - | size(64,64,21) | - | - | 24 | - | - |
| torch | 0 | 1 | 0 | 1 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | torch_small.png 28x28 | light01_normal.png | r=227.5 col=(1,0.7,0.3) pos=(0,-14,8) h=halo1 hs144 hb0.5 | zero | explosion.JPG n=18 ~torch_fire(+10, x1.2) | - | 21 | - | - |
| torch_higher_range | 0 | 1 | 0 | 1 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | torch_small.png | light01_normal.png | r=355 (else = torch) | zero | as torch | - | 3 | - | - |
| tree01 / tree02 / tree03 / tree04 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | tree01_alpha.dds 206x246 / tree02_alpha.dds 156x116 / tree03_alpha.dds 70x292 / tree05_alpha.dds 145x159 | - | - | zero | - | - | 4/4/3/4 | - | - |
| tree05 | 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | tree_alpha.png 186x280 | - | - | zero | - | - | 5 | - | - |
| vert_bruxo | 2 | 0 | 1 | 1 | 1 | 2 | 0 | 8 | 4x4 | 0,-10 | bruxo.png -> 32x48 | bruxo_nm.png | - | pos(0,4,0) size(14,36,8) | ~shadow_beam(+2) | player block, jumpForce=390 | 0 | UNREACHABLE | - |
| vert_king | 2 | 0 | 1 | 1 | 1 | 2 | 0 | 4 | 4x4 | 0,-10 | king.png -> 32x48 | king_nm.png | r=122.5 pos=(0,2,8) | size(23,48,20) | ~king_beam(+9) | - | 0 | UNREACHABLE | - |
| vert_master_knight | 2 | 0 | 1 | 1 | 1 | 0 | 0 | 4 | 4x4 | 0,-25 | master_knight.png -> 80x96 | - | - | pos(0,4,0) size(57,84,8) | ~shadow_beam_wide(+9) | showUpSfx | 0 | UNREACHABLE | - |
| wall00 | 0 | 1 | 0 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | STONE03A.JPG 128x128 | wall__height_nm.png | - | zero | - | - | 131 | - | - |
| wall01 | 0 | 1 | 0 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | STONE03A4x.JPG 256x256 | wall__height4x_nm.png | - | zero | - | - | 46 | - | - |
| wall02 / wall03 | 0 | 1 | 1 | 1 | 0 | 2 | 0 | 0 / 1 | 2x2 | 0,0 | STONE03A4x2.JPG 256x256 -> 128x128 | wall__height4x_nm.png | - | pos(0,0,21) size(128,128,1) | - | - | 28 / 40 | - | - |
| wall04 / wall05 | 0 | 1 | 1 | 1 | 0 | 2 | 0 | 0 / 1 | 2x2 | 0,0 | STONE03A4x3.JPG -> 128x128 | wall__height4x_nm.png | - | same | - | - | 4 / 5 | - | - |
| wall06 / wall07 | 0 | 1 | 1 | 1 | 0 | 2 | 0 | 3 / 2 | 2x2 | 0,0 | STONE03A4x4.JPG -> 128x128 | wall__height4x_nm.png | - | same | - | - | 10 / 11 | - | - |
| wall08 / wall09 | 0 | 1 | 1 | 1 | 0 | 2 | 0 | 0 / 1 | 2x1 | 0,0 | STONE03A4x5.JPG 256x128 -> 128x128 | wall__height4x_nm.png | - | same | - | - | 0 / 0 | UNREACHABLE | - |
| wall10 / wall11 | 0 | 1 | 1 | 1 | 0 | 2 | 0 | 1 / 0 | 1x2 | 0,0 | STONE03A4x7.JPG 256x256 -> 256x128 | wall__height4x_nm.png | - | size(256,128,32) | - | - | 16 / 31 | - | - |
| wall12 | 0 | 1 | 1 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | STONE03A4x10.png 256x32 | wall__height4x2_nm.png | - | size(256,32,15) | - | - | 81 | - | - |
| wall13 | 0 | 1 | 1 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | STONE03A4x9.jpg 128x128 | wall__height_nm.png | - | size(128,128,14) | - | - | 21 | - | - |
| warrior | 0 | 0 | 1 | 1 | 0 | 0 | 0 | 6 | 4x4 | 0,0 | warrior.png 128x192 -> 32x48 | warrior_nm.png | - | size(20,43,7) | - | - | 0 | dyn (spawn/summoner) | controlCharacters.as:471 |
| white_ground | 0 | 1 | 0 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | white_ground.jpg 256x256 (+Gloss) | nm_white_ground.jpg | - | zero | - | - | 70 | - | - |
| window01 | 0 | 1 | 0 | 1 | 0 | 0 | 0 | 0 | 1x1 | 0,0 | window01.png 128x192 | window.png | - | zero | - | - | 17 | - | - |
| window02 | 0 | 1 | 0 | 1 | 0 | 2 | 0 | 0 | 1x1 | 0,0 | window02.png 256x256 | window02_nm.png | - | size(256,256,43) collid=0 | - | - | 23 | - | - |

**Secondary attributes that differ from the defaults** (soundVolume 1, shadowScale 0, shadowLengthScale 1, specularPower 50, EmissiveColor 0):
- **soundVolume:** big_explosion/checkpoint_effect/dark_sword/enemy_sword/explosion/fade_out_beam/summon 0.5; blood/enemy_blood 0.3; combo_sword/paladin_sword 0.2; sword0/sword1 0.7; silent_sword 0.
- **shadowScale:** crate 1.2, devil 0.3, single_tile_shadow 1.5 (length 9.5), single_tile_short_shadow 1.5, thumbnail 1.4, tile01 1.4 (but castShadow=0), tile_shadow 1.2 (length 1.5), vert_bruxo/vert_king 0.8, vert_master_knight 0.6.
- **specularPower:** buttons/devil 30, cano/white_ground 60, paladin 61, gob_face/pilar01/pilar02 71, thumbnail 100.
- **EmissiveColor (1,1,1,0):** blue_light, buttons, crystal, crystal_green, crystal_high_range, flashlight, foggy_crystal_green, gamelogo, green_light, green_light_menu, huge_ambient_light, lava, mario_bg, red_light, red_light_high_range. (0.5,0.5,0.5,0): potion_small, potion_large. (0.8,…): thumbnail.
- **Gloss (12 .ent):** bloco_mario/devil/white_ground → white_ground.jpg; cano/potion_small/potion_large/thumbnail → white.bmp; gob_face/pilar01/pilar02 → gray.bmp; buttons → menu_buttons_gloss.png; paladin → paladin_gloss.png. All have applyLight=1.

**Script-spawned `.ent` whose particle system carries `<SoundEffect>`** (all files exist in soundfx/, all repeat=1):

| ent | sound (volume) |
|---|---|
| big_explosion | explosion.ogg (0.5) |
| blood, enemy_blood | hit01.ogg (0.3) |
| bridge_fall | brige_fall.ogg (1) |
| checkpoint_effect, summon | checkpoint.mp3 (0.5) |
| combo_sword | sword_combo.ogg (0.2) |
| dark_sword | dark_hit.ogg (0.5) |
| enemy_sword | sword01.mp3 (0.5) |
| explosion | hit01.ogg (0.5) |
| fade_out_beam | vanish.ogg (0.5) |
| fade_out_beam_large | creature_dying.mp3 (1) |
| hit_fail | cast_fire_spell.ogg (1) |
| paladin_sword | sword01.mp3 (0.2) |
| potion_pick | potion_pick.ogg (1) |
| silent_sword | sword01.mp3 (0) |
| sword0, sword1 | sword01.mp3 (0.7) |

**Reachability:**
- **Unreachable `.ent`** (never placed, never spawned by literal, dynamic name or custom-data name), 19 files: branches, buttons, crate, environment, event, help, next_level, nextlv, pilar, plans_small, potion_large, silent_sword, spawn, thumbnail, vert_bruxo, vert_king, vert_master_knight, wall08, wall09.
- **Dynamic names resolved:**
  - `name + ".ent"` from spawn markers and the data.enml section names (setupScene.as:279);
  - `creature + ".ent"` = warrior (controlCharacters.as:636/666);
  - `"sword" + player + ".ent"` → sword0/1 (playerInput.as:402);
  - `GetStringData("hit")` → blood.ent/enemy_blood.ent (doDamage.as:225);
  - environment `name` → clouds/dawn/fog.ent (setupScene.as:203).

---

## 9. `.par` census (43)

| par | particles | repeat | alphaMode | allAtOnce | Bitmap | SoundEffect | exact embedded copies | nearest-only copies |
|---|---|---|---|---|---|---|---|---|
| big_explosion | 12 | 1 | 1 | 1 | explosion.png | explosion.ogg | 1 | 0 |
| blood | 45 | 1 | 0 | 1 | particle.png | hit01.ogg | 0 | 0 |
| bridgefall | 9 | 1 | 0 | 1 | fog.dds | brige_fall.ogg | 1 | 0 |
| checkpoint | 5 | 1 | 0 | 0 | particle_harder.png | checkpoint.mp3 | 2 | 0 |
| clouds | 70 | 0 | 0 | 0 | fog.dds | - | 0 | 2 |
| combo_fire_ball | 23 | 7 | 1 | 0 | explosion.JPG | - | 0 | 1 |
| enemy_dark_sword | 16 | 1 | 0 | 0 | black_sword.dds | dark_hit.ogg | 0 | 1 |
| enemy_sword | 16 | 1 | 1 | 0 | sword.png | sword01.mp3 | 0 | 1 |
| enemy_sword_beam | 9 | 1 | 0 | 0 | fog.dds | - | 1 | 0 |
| enemy_sword_hit | 12 | 1 | 1 | 1 | explosion.png | hit01.ogg | 0 | 1 |
| explosion_particles | 13 | 1 | 1 | 1 | explosion.JPG | explosion.ogg | 0 | 0 |
| fade_out_shadow_beam | 45 | 1 | 4 | 0 | black_opaque.png | vanish.ogg | 1 | 0 |
| fade_out_shadow_beam_hard | 45 | 1 | 0 | 0 | particle_harder.png | creature_dying.mp3 | 0 | 1 |
| fall | 9 | 1 | 0 | 1 | fog.dds | **fall.mp3 (missing)** | 0 | 0 |
| fire_shoot | 19 | 0 | 1 | 0 | explosion.JPG | - | 1 | 0 |
| fireball | 19 | 5 | 1 | 0 | explosion.JPG | - | 1 | 0 |
| fog | 40 | 0 | 0 | 0 | fog.dds | - | 0 | 1 |
| fog_menu | 40 | 0 | 0 | 0 | fog.dds | - | 2 | 0 |
| heavy_sword | 16 | 1 | 1 | 0 | sword.png | sword01.mp3 | 0 | 0 |
| hit_fail | 4 | 1 | 1 | 1 | explosion.png | cast_fire_spell.ogg | 0 | 1 |
| jumpfx | 9 | 1 | 0 | 1 | fog.dds | - | 2 | 0 |
| king_beam | 19 | 0 | 1 | 0 | explosion.JPG | - | 0 | 2 |
| lava | 14 | 0 | 1 | 0 | explosion.JPG | - | 0 | 19 |
| lava_drops | 9 | 1 | 1 | 1 | explosion.JPG | - | 1 | 0 |
| life_shadow_beam | 12 | 0 | 0 | 0 | particle.png | - | 0 | 55 |
| life_shadow_beam_bigger | 12 | 0 | 0 | 0 | particle_harder.png | - | 0 | 1 |
| light_spell | 15 | 36 | 1 | 0 | flash.bmp | - | 0 | 1 |
| light_spell_infinite | 15 | 0 | 1 | 0 | flash.bmp | - | 0 | 4 |
| nextlv | 17 | 0 | 1 | 0 | flash.bmp | - | 1 | 0 |
| paladin_sword | 16 | 1 | 1 | 0 | sword.png | sword01.mp3 | 0 | 1 |
| portal_red | 29 | 0 | 0 | 0 | blood.png | - | 0 | 6 |
| potion_pick | 12 | 1 | 1 | 1 | explosion.png | potion_pick.ogg | 0 | 1 |
| shadow_beam | 45 | 0 | 4 | 0 | black_opaque.png | - | 0 | 5 |
| shadow_beam_wide | 45 | 0 | 4 | 0 | black_opaque.png | - | 0 | 13 |
| small_explosion | 12 | 1 | 1 | 1 | explosion.png | - | 0 | 0 |
| sword | 16 | 1 | 1 | 0 | sword.png | explosion.ogg | 0 | 0 |
| sword01 | 16 | 1 | 1 | 0 | sword.png | sword01.mp3 | 2 | 2 |
| sword02 / sword03.ent / sword04 | 16 | 1 | 1 | 0 | sword.png | sword01.mp3 | 0 | 0 |
| sword_beam | 45 | 1 | 4 | 1 | black_opaque.png | - | 1 | 0 |
| sword_hit | 12 | 1 | 1 | 1 | explosion.png | hit01.ogg | 0 | 2 |
| torch_fire | 18 | 0 | 1 | 0 | explosion.JPG | - | 0 | 36 |

- **alphaMode 4 (modulate) in embedded systems:** 10 in `.ent` (bruxo, bruxo_dead, princess, vert_bruxo, master_knight, vert_master_knight, minion, checkpoint, fade_out_beam, sword_beam) and 10 scene instances (the checkpoint.ent copies).
- **alphaMode 1 (additive)** is used for fire, explosions, swords and flash.

---

## 10. ENML (`data.enml`, `hs.enml`)

### 10.1 Grammar

[2013] `gs2d::enml::File::ParseString` (`toolkit/Source/src/gs2d/src/Enml/Enml.cpp:313-448`, `ReadValue` at `:645-671`, in the Dec-2013 source). The 2010 script API is `[exe]` `enmlFile`/`enmlEntity` at 0xca558–0xca894: `parseString`, `get(entity,key)`, `getInt`, `getUint`, `getFloat`, `getDouble`, `exists`, `addEntity`, `generateString`, `writeToFile`, `parseFromFile`, `getEntityNames`, `getAttributeNames`, `clear`, `add`.

```
file      := ( ws | comment | section )*
section   := name ws* '{' ( ws | comment | attr )* '}'
attr      := name ws* '=' ws* value ';'
name      := [A-Za-z0-9_]+
value     := 1+ bytes up to the next unescaped ';'  (newlines, '/', '{', '}' are ordinary value bytes)
escape    := '\;' -> ';'   '\\' -> '\'   (any other backslash = parse error, whole file cleared)
comment   := '/' ... end-of-line  |  '/*' ... '*/'   (not recognised inside a value)
ws        := ' ' '\t' '\r' '\n'
```

- **Leading whitespace after `=` is skipped,** including CRLF. That is how `bridge = \r\nEssas pontes…` works.
- **Trailing whitespace before `;` is kept,** and so are **CRLF line breaks inside multi-line values.** The engine returns `"…subterrâneos\r\nde penumbra…"`, so the port's text renderer must treat `\r\n` as one line break.
- **Empty values are illegal.**
- **Storage is `std::map`,** so order is not preserved and duplicate keys resolve to the last one.
- **Typed getters leave the out-parameter unchanged** when the key is missing ([2013] `Enml.cpp:495-549`).
- **Files are Latin-1 with no BOM** (data.enml contains `áâãçéíóõ`), with CRLF endings: data.enml has 227 CRLF and 0 bare LF.

### 10.2 `data.enml` (3,949 bytes, parses cleanly with 8 sections)

| section | keys (value) |
|---|---|
| global | lives=13; lv1=200, lv2=400, lv3=800, lv4=1600, lv5=3200, lv6=4000, lv7=5000, lv8=6000, lv9=7000, lv10=8000, lv11=9000, lv12=10000, lv13=10000, lv14..lv19=11000, **lv20 MISSING**, lv21..lv30=11000; story01 (3 line breaks), story02 (2), story03, story04, soldados, fun, comboTip, bridge, annoying, flyingWall (0), portalToCastle, warning (0), nights, wisdom (0), arena1..arena6 (2–3 line breaks each) |
| warrior | hp 75, speed 50, stride 150, coolDown 800, viewRadius 180, attackRadius 40, damage 5, waitBeforeAttack 0, jumpBackAfterAttack 0, fireResistant 0, pushBackBias 1 |
| minion | hp 45, speed 150, stride 150, coolDown 800, viewRadius 500, attackRadius 40, damage 20, 0, 0, fireResistant 1, pushBackBias 1, chaseSfx=minion.ogg |
| knight | hp 150, speed 80, stride 110, coolDown 300, viewRadius 180, attackRadius 43, damage 10, 0, jumpBack 1, fire 0, pushBackBias 0.1 |
| master_knight | hp 1700, speed 100, stride 150, coolDown 420, viewRadius 230, attackRadius 80, damage 25, waitBeforeAttack 1, 0, 0, pushBackBias 0.05 |
| impy | hp 75, speed 120, stride 100, coolDown 1500, viewRadius 600, attackRadius 100, damage 18, 0, 0, fire 1, pushBackBias 0 |
| paladin | hp 400, speed 80, stride 120, coolDown 550, viewRadius 280, attackRadius 47, damage 20, 0, jumpBack 1, fire 1, pushBackBias 0.05 |
| king | hp 3500, speed 100, stride 120, coolDown 1000, viewRadius 280, attackRadius 35, damage 10, 0, jumpBack 1, fire 0, pushBackBias 0.15 |

**Consumers [script]:**
- main.as:135-137 (`lives`);
- setupScene.as:69-110 (section = spawned creature name; key names as above);
- util.as:410-415 and interface.as:54 (`"lv" + level`);
- main.as:166 (story text through the `story` entity's `name`);
- menu.as:328 (`"arena" + number`).

**The `lv20` gap:** at level 19→20, `addToExp` keeps nextExp = 11000 because `getInt` leaves it unchanged. `interface.as:54` initialises `nextExp = 0`, so at level 20, and above 30, the HUD's `maxXp` is 0.

### 10.3 `hs.enml` (97 bytes)

- Contents: `hs\r\n{\r\n\ths0 = 3599000;\r\n…\ths4 = 3599000;\r\n}\r\n\r\n`, i.e. 5 times of 3,599,000 ms = 59:59.000.
- It is **rewritten at runtime** by `highScores.writeToFile(GetAbsolutePath("hs.enml"))` (scores.as:88) in the game directory.
- The writer format ([2013] `GenerateString`, Enml.cpp:269-283) is `name\n{\n\tkey = value;\n}\n\n`, with keys in map order. The file on disk has CRLF, which suggests a Windows text-mode write.

---

## 11. `Penumbra.ethproj`

27 bytes of ASCII, `Ethanon Engine project file` (hex `457468616e6f6e20456e67696e652070726f6a6563742066696c65`). There is no newline and no data. It is a marker file for the editor/player and can be ignored.

---

## 12. Asset references: existence, missing, unused

### 12.1 Resolution rules used

| reference | resolved under |
|---|---|
| `Sprite` | `entities/` |
| `Normal` | `entities/normalmaps/` |
| `Gloss` | `entities/` (unconfirmed; every Gloss file exists only there) |
| `HaloBitmap` | `entities/` |
| particle `Bitmap` | `particles/` |
| particle `SoundEffect` | `soundfx/` |
| `bgImage` | `"entities/" + name` (environment.as:129) |
| script literals | relative to `app/` |

Engine-internal files ([exe] 0xcbcbc–0xcbdc0): `data/defaultVS.cg`, `hPixelLight.cg`, `vPixelLight.cg`, `pixelLightVS.cg`, `hVertexLightShader.cg`, `vVertexLightShader.cg`, `defaultStaticAmbientVS.cg`, `dynaShadowVS.cg`, `shadow.dds`, `default_nm.png`.

### 12.2 Result

- **245 references resolve with exact case,** so a case-sensitive filesystem is fine. Case-sensitive names that do match exactly: `explosion.JPG`, `STONE03A*.JPG`, `lapide_Normal.png`, `LOS-Nac-Normal.png`, `mario_BG.png`.
- **Missing: 1.** `soundfx/fall.mp3`, named only by `effects/fall.par`'s `<SoundEffect>`. That file is an editor template and nothing loads it at runtime; `fall.ogg` exists and is played by script (controlCharacters.as:159).
- **Runtime-generated (not missing):**
  - `scenes/checkpoint.esc`, written by `SaveScene("scenes/checkpoint.esc")` (main.as:184) and loaded by controlCharacters.as:444;
  - `hs.enml`, rewritten by scores.as:88.
- **No lightmap sidecar directories** (`scenes/*-esc/`) ship; `scenes/` holds only the 13 `.esc` plus readme.txt.
  - Yet `machine.exe` contains `ETHRenderEntity::GenerateLightmap` messages, including "lightmaps can't be generated during application render" and "this scene has no light sources".
  - Nothing on disk is keyed by instance `id`: no sidecars, and no CustomData value refers to an id. **Renumbering instance ids is safe;** only runtime `ownerID` values reference ids.
- **Present but referenced by nothing:**

| dir | files |
|---|---|
| entities | Lapide_Bitmap.png (494,355 B, 500x500, 16-bit RGBA), STONE03A4x10.jpg (the .png twin is used), STONE03A4x6.JPG, STONE03A4x8.jpg, arch_height.png, ground_height_map.png, king_height.png, princess_height.png (the normalmaps/ copy is used), tombstone.png |
| particles | black.bmp, black_sword.png, dust.png, particle.bmp, white.bmp |
| effects | sky.png |
| soundfx | fire.ogg, jump.ogg, sword01.ogg |

- **Referenced only by unreachable `.ent`:**
  - entities: HP_Bitmap.png (potion_large), STONE03A4x5.JPG (wall08/09), crates.png (crate), pilar_ct_small.png (pilar), roots.bmp (branches);
  - normalmaps: HP_Normal.png, pilar_normal_ct_small.bmp, roots_normal.bmp.
- **All 11 `interface/*.png`** are used by scripts. All 40 normal maps other than those 3 are used.
- **Sound at runtime:** 16 distinct `.ogg` of the 19 on disk, and 16 distinct `.mp3` of the 16 on disk.
  - .ogg: blast_attack, brige_fall, cast_fire_spell, dark_hit, death_king, explosion, fail, fall, hit01, jump01, jump02, minion, potion_pick, pvp_win, sword_combo, vanish.
- **Byte-identical duplicates:**
  - entities/black.bmp = particles/black.bmp;
  - ground.png = ground2.png = half_ground.png;
  - entities/halo.bmp = particles/flash.bmp;
  - entities/skull.png = interface/skull_interface.png;
  - entities/white.bmp = particles/white.bmp.
- **Extension does not match content:** `entities/normalmaps/nm_white_ground.jpg` is a **BMP** (32 bpp, 256x256). It is used by floor03_2 and white_ground: 182 instances.

### 12.3 Image formats that matter

- **DDS:** all 9 files are uncompressed, mips=0.
  - 8 are A8R8G8B8 (pf flags 0x41, masks R 0xff0000, G 0xff00, B 0xff, A 0xff000000): dirt, thorn, tree01/02/03/05_alpha, particles/black_sword, particles/fog.
  - `data/shadow.dds` is A8L8 (flags 0x20001, 16-bit, L mask 0xff, A mask 0xff00).
- **Halos:** `halo.bmp` (265x253), `halo1.bmp` and `halo2.bmp` (123x123), and `flash.bmp` are **8 bpp BMPs with no alpha,** so they need additive blending or luminance-as-alpha.
- **24-bpp BMPs with no alpha:** black.bmp, black2.bmp, gray.bmp, white.bmp, roots.bmp. Those used with blendMode 2 (alpha test), such as black.ent and black_entity.ent, are fully opaque.
- **PNG colour types vary:** palette PNGs (warrior.png, dawn.png, mario_BG.png, black_opaque.png, blood.png, arrow_sign_nm.png, mp.png, xp.png), gray (menu_buttons_gloss.png, paladin_gloss.png, sword.png, black_sword.png, dust.png), gray+alpha (arrow_sign.png, frame.png), 16-bit (Lapide_Bitmap.png, light01_normal.png). JPEGs include progressive ones (face.jpg, face_norm.jpg).

---

## 13. Semantics notes for the port (format-level)

1. **Coordinates:**
   - `Position x, y` are world pixels, with y growing down. `z` is depth: 1,916 instances have z=0; background walls, windows and arches sit at z −84/−80.
   - ZAxisDirection is (0,0) in all gameplay scenes, so z does not shift sprites on screen. In menu and arena_select it is (0,−1): z raises sprites by z pixels, with z up to 314 in menu.
   - Collision `Position` is the centre offset and `Size` the full extent. z-size is a depth thickness the scripts use for the AABB test (for example wall02–09 collide at z 21 with size z 1).
2. **Collision:**
   - The 2010 engine exposes `bool Collidable()` and `void SetCollision(const bool)` [exe]. The scripts filter candidates with `if (!collidableEntities[t].Collidable()) continue;` (controlCharacters.as:63) and read `GetCollisionBox()`.
   - 7 `.ent` (139 instances) have `collidable=0` with a non-zero box, and flashlight.ent has `collidable=1` with a zero box.
   - Potion pickup uses distance < 20 px, not collision (potions.as:48).
3. **Text:** every string (ENML values, `help.message`, thumbnail `title`, script literals) is Latin-1 and must be converted to UTF-8 when loaded. `help.message` contains `&apos;` after XML decoding.
4. **Blend modes:** only `blendMode` 0 and 2 occur on entities. Particle alphaMode uses 0, 1 and 4.

---

## 14. Things the census could not settle

These are listed in openQuestions. In short: the numeric enum values (2013 values assumed); whether `collidable=0` drops the box; Gloss directory resolution; whether and when static lightmaps are baked at runtime; the exact `SaveScene` output (checkpoint.esc) and what it round-trips; and whether `spriteFrame` or `startFrame` wins at scene load (the data strongly implies `spriteFrame`).

## Key facts

- Corpus: 13 .esc (2607 entity instances, 112 distinct EntityNames), 144 .ent, 43 .par, data.enml + hs.enml, Penumbra.ethproj (27-byte ASCII marker 'Ethanon Engine project file').
- The 2010 machine.exe XML vocabulary (strings at 0xc955c-0xc98b8) has no FileName, Scale, parallaxIntensity, flipX/flipY/hide, shape/Box2D or hideFromSceneEditor. Scenes therefore store ONLY full inline entity copies, and there is no entity scale.
- Every element/attribute name used by the data exists in machine.exe. startFrame, soundVolume, collidable and SoundEffect are live in 2010 (read by the loader), unlike the 2013 engine.
- Every XML file declares <?xml version="1.0" ?> (implying UTF-8) but level1.esc/level3.esc contain Latin-1 bytes (ã ç ó É). Decode everything as ISO-8859-1; files are CRLF, no BOM; &apos; escapes appear at level1.esc:6397/7445/8231.
- Inline block vs .ent: 2250 of 2254 instances whose name has a .ent are identical (including CustomData). 4 differ: ground_fire (arena_select, stale), blue_light (level1 id 423, halo), tile02 (level3 id 407, static 0), fog_menu (menu id 100, no Sprite). Scene-placed entities must use the inline block; AddEntity-spawned ones use entities/*.ent.
- 353 instances (25 names) use label-only EntityNames with no .ent: spawn 271, help 16, story 14, pilar02 10, environment 7, thumbnail 6, summon 5, play 3, 7 menu buttons, etc. Per-instance CustomData exists only on these labels.
- Callback binding is ETHCallback_ + RemoveExtension(EntityName) [2013 ETHASUtil.cpp:63-68]; GetEntityArray is an exact-name match [2013 ETHBucketManager.cpp:488]. So the 5 play_sound.ent instances are inert (the script collects only 'play_sound').
- Outer spriteFrame is the displayed per-instance frame (7 menu buttons share startFrame 0 but have spriteFrame 0-6); inner startFrame is the definition default. They are equal in 2585/2607 instances.
- Outer <Color> is always (1,1,1,1); Position angle is always 0; SceneProperties lightIntensity=2 everywhere; ZAxisDirection is (0,0) in 11 scenes and (0,-1) only in menu and arena_select.
- Instance ids are unique per scene, sparse (level3 0..1106 for 866 entities) and not in file order. Nothing on disk (no lightmap sidecars, no CustomData) references ids, so renumbering is safe.
- type distribution in scenes: 0:2365, 5:212 (black/instant_death overlays, layerDepth 1), 2:30 (upright props/cursor). blendMode is only 0 (530) or 2 (2077); Light@static always equals entity static.
- Active lights: 141 placed. All placed dynamic lights in gameplay scenes are flashlight.ent placeholders, which setupScene.as:181-190 deletes. After setup, gameplay scenes hold only static lights (level1 44, level2 20, level3 23, pvp 1/9/4/0/2/5); dynamic light comes from script-spawned spells/swords/explosions.
- Embedded particle systems (max 1 per entity) are uniformly scaled copies of .par templates (torch x1.2, checkpoint x1.44, king x0.72, enemy_blood x0.24). No script loads a .par, so effects/*.par are editor-only; use the embedded values.
- CustomData types are only int/uint/float/string (the 2010 DATA_TYPE enum has no vectors); scalar Value is element text; written alphabetically.
- Spawn markers: CustomData name = warrior 71, minion 69, knight 60, impy 24, paladin 19, master_knight 14, bruxo 10, princess 6; replaced at load by name+'.ent' at pos-(0,0,10) (setupScene.as:273-280).
- ENML: sections name{ key = value; }; leading ws skipped, trailing ws kept, CRLF kept inside multi-line values, escapes only \; and \\, '/' comments outside values; Latin-1. data.enml global lacks lv20 (lv19 -> lv21).
- hs.enml (5 x 3599000 ms) and scenes/checkpoint.esc are written at runtime (scores.as:88, main.as:184 SaveScene).
- All 245 asset references resolve with exact case. The only missing file is soundfx/fall.mp3, named only by editor-only effects/fall.par.
- Unused files: entities Lapide_Bitmap.png, STONE03A4x10.jpg, STONE03A4x6.JPG, STONE03A4x8.jpg, arch_height.png, ground_height_map.png, king_height.png, princess_height.png, tombstone.png; particles black.bmp, black_sword.png, dust.png, particle.bmp, white.bmp; effects/sky.png; soundfx fire.ogg, jump.ogg, sword01.ogg. 19 .ent are unreachable.
- entities/normalmaps/nm_white_ground.jpg is actually a BMP (182 instances use it). All 9 DDS are uncompressed (8x A8R8G8B8, shadow.dds A8L8). Halos (halo/halo1/halo2/flash.bmp) are 8-bpp BMPs without alpha.
- Runtime audio: 16 distinct .ogg and 16 .mp3 are played; .ent particle <SoundEffect> plus entity soundVolume (0-1) is a sound channel (silent_sword soundVolume=0).
- Scripts: esc_census.py and outputs census_out/census.md, census.json, machine_strings.txt, compact_tables.md and matrix.md are in reference/analysis/

## Engine gaps

- No OGG Vorbis decoder: src/core/AudioClip.cpp:87-89 accepts only .wav and .mp3, and .mp3 decodes through Windows-only Media Foundation. 16 distinct .ogg files are played at runtime (19 on disk). Either transcode offline or add a decoder.
- No DDS loader: textures go through stb_image, which cannot read DDS. 9 DDS files, all uncompressed (8x A8R8G8B8 + shadow.dds A8L8), so an offline PNG conversion is trivial.
- Extension-based decoder selection would fail on entities/normalmaps/nm_white_ground.jpg, which is a 32-bpp BMP (stb_image sniffs content, so it is OK there; an extension-dispatching tool is not).
- No XML reader in the engine: the README lists a hand-written JSON reader for .scene/.prefab only. Porting needs either an offline Ethanon->engine converter (as another port on this engine did, with a Python converter) or a TinyXML-grade parser with entity decoding (&apos;) and Latin-1 input.
- No multiply/modulate blend: MaterialComponent::BlendMode is {Alpha, Additive, Premultiplied} (src/core/Components.hpp:804). Particle alphaMode 4 (modulate) is used by 20 embedded systems (character shadow beams, checkpoint, fade_out_beam, sword_beam).
- No runtime scene writer in Ethanon .esc form: the game calls SaveScene("scenes/checkpoint.esc") (main.as:184) and reloads it (controlCharacters.as:444). The live scene, including CustomData added at runtime (waitBeforeAttack, lastTimeAlive, pvpMode, hitBy, ownerID...), must round-trip, and it needs a writable location. The same applies to rewriting hs.enml (scores.as:88).
- 2D sprite lighting likely lacks specular + gloss maps: Light2DComponent's formula (Components.hpp:403-410) has no specular term, but the 2010 hPixelLight.cg:87-126 uses glossMap*specularBrightness*pow(N.H, specularPower). 241 scene instances carry <Gloss>, and specularPower varies (20-100). Verify with the rendering dimension.
- Likely missing projected fake shadows (castShadow/shadowScale/shadowLengthScale/shadowOpacity, data/dynaShadowVS.cg + shadow.dds): 82 scene instances have castShadow=1. Not verified in the engine source; flag for the rendering dimension.
- Static-light baking: the 2010 exe has ETHRenderEntity::GenerateLightmap, but no lightmap sidecars ship. If the original bakes at scene load, the port must bake at load or light static entities dynamically (level1 alone has 44 static lights).
- Latin-1 text: ENML story/arena strings, help.message and script literals contain Portuguese accents in ISO-8859-1 and need conversion to UTF-8 before the UI text path; ENML multi-line values contain \r\n.

## Open questions

- Numeric values of ENTITY_TYPE and blend/alpha modes in the 2010 build: the exe confirms the names (ET_HORIZONTAL..ET_LAYERABLE; GS_ALPHA_MODE lives in GameSpace.dll with no name strings) but not the numbers. The report assumes the 2013 values (type 0 horizontal, 2 vertical, 5 layerable; blend 0 alpha, 2 alpha-test; particle alpha 1 additive, 4 modulate). The data is consistent with that, but it is unproven.
- collidable=0 with a non-zero Collision box (gob_face, pilar01, pilar02, potion_small, potion_large, silent_sword, window02; 139 instances): does the 2010 loader keep the box (2013 discards it via the collidable gate)? The scripts only test Collidable(), so this matters only if some script calls GetCollisionBox on those entities.
- Does the 2010 loader apply the 2013 read gates (specular* read only when applyLight=1, shadow* only when castShadow=1)? It matters for tile01 (shadowScale 1.4 with castShadow=0, 163 instances) and the pilar02 label (applyLight=0 with specularPower 71).
- Gloss directory: every <Gloss> file exists only in entities/ (white.bmp, gray.bmp, white_ground.jpg, menu_buttons_gloss.png, paladin_gloss.png), so entities/ is assumed. Not confirmed from 2010 code.
- At scene load, does spriteFrame (outer) override startFrame (inner)? The data (menu buttons, thumbnails, cano, small_ground_tile) implies spriteFrame is displayed; confirm by running machine.exe.
- Does the 2010 engine bake lightmaps for static entities at scene load (GenerateLightmap strings exist, no sidecars ship), and are the 8-bpp halo BMPs drawn additively or as luminance-alpha?
- Exact on-disk form of the runtime-written scenes/checkpoint.esc: which runtime CustomData keys, entity states and removed entities (deleted spawns and flashlights) it contains, and whether reloading re-runs setupScene on already-spawned enemies. This is needed to design the port's checkpoint save.
- Semantics of ZAxisDirection (0,0) vs (0,-1) in the 2010 renderer: with (0,0), does z affect only depth sorting? Menu z values go up to 314.
- Are the 5 play_sound.ent instances (level2 x3, level3 x2) intentionally inert (GetEntityArray('play_sound') is an exact match and there is no ETHCallback_play_sound), or did 2010 GetEntityArray match names without the extension?
- Rendering meaning of soundVolume in .ent files: it only differs from 1 on entities whose particle system has a <SoundEffect> (0-0.7), which strongly suggests it is the volume of that sound. The 2010 playback path is unverified.
