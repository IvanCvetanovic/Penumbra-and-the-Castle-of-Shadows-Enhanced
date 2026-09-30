# Ethanon 0.7.12 runtime semantics that the Penumbra scripts depend on

## 0. Provenance: which engine the game ran on, and what was read

**Pinned version.**
- `extracted/app/machine.exe` has VERSIONINFO FileVersion **0.7.12** ("Ethanon engine executable", ASANTEE).
- Its write time is 2010-11-03 12:41.
- Its PDB path is `c:\Users\Andre\Documents\Projects\ethanon\trunk\Ethanon Machine\machine.pdb`.
- `GameSpace.dll` is **GameSpaceLib 1.6.4.1**.
- Bundled alongside: cg.dll, cgD3D9.dll, d3dx9_42.dll and audiere.dll.

**Where the source came from.**
- **GitHub** `https://github.com/asantee/ethanon.git`, cloned in full to `(full clone of github.com/asantee/ethanon)` (839 commits).
  - History starts at `b6bff3f` (2012-05-01), and `1c7b0ea` is "Repository move" from SourceForge. So it has no 2010 code.
  - It does contain one key artefact. Commit `816e8d6` (2012-05-01) had `testbed/PenumbraTest/`, which `60b2d4e` removed on 2012-05-08.
  - Those `.as` files are **identical to the shipped scripts**; only the encoding differs (UTF-8+BOM vs Latin-1). The one exception is a `DrawText(pos,str,font,size,color){}` shim in eth_util.as.
  - Its `___fixlist.txt` lists the needed fonts: "Arial Narrow" 15,16,20,25,30,40,50,60,256 (256 = only '0'-'9' and ':'); "Arial Black" 17; "Arial" 30; "Verdana" 15.
  - So the 2012 engine was API-compatible with Penumbra except for text.
- **SourceForge SVN** is still online at `https://svn.code.sf.net/p/ethanon/code/` (HEAD r1397).
  - Tags (dates via WebDAV PROPFIND): `v0-7-11` r476 (2010-11-02), `v0-7-11-fixed` r478, `v0-7-12` r484 (2010-11-04, "The same as 0.7.11 but with AngelScript included", a copy of trunk r483), `v0-9-0` r1331 (2011-12-12).
  - r481 (2010-11-03 03:12Z) is "Version updated to 0.7.12". r485 (2010-11-06) is "migrating from the old GameSpaceLib to the new GS2D lib".
  - **So machine.exe is tag v0-7-12 = trunk r483.**
  - I downloaded it to `reference/eth-0.7.12` with `reference/analysis/svncrawl.py` (174 text files; AngelScript sources and binaries skipped).
- **GameSpaceLib source is unavailable**: the Google Code archive for `code.google.com/p/gamespacelib` returns 403, and web search finds nothing.
  - As a proxy I downloaded **GS2D at SVN r485** (`reference/gs2d-r485`), Andre's port of GameSpaceLib from three days later.
  - Load-bearing GS2D claims were re-checked by disassembling the shipped `GameSpace.dll` with `objdump`; the full listing is at `reference/analysis/gs.asm`.
- Each claim is tagged by where it was checked:
  - **[src]** = read in the 0.7.12 Ethanon source;
  - **[dis]** = verified in GameSpace.dll machine code;
  - **[gs2d]** = inferred from GS2D r485 only.

**Citation keys.**
- `E:` = `reference/eth-0.7.12/src/`
- `G:` = `reference/gs2d-r485/`
- `DLL@0x…` = virtual address in `extracted/app/GameSpace.dll` (ImageBase 0x10000000)
- `A:` = `extracted/app/`
- Data tables came from the scratch scripts `reference/analysis/cbscan.py`, `assetscan.py`, `depthscan.py` and `coverage.py`. These are reproducible.

**Startup.**
- `machine.exe` is run with no args, so `testing=false` and `#if TESTING` is off. `-testing` defines TESTING; `-norun` only compiles (`E:machine/main.cpp:77-99`).
- The entry script is `main.as`, or `game.bin` if main.as is absent (`E:ETHEngine.cpp:77-78, 438-523`).
- CScriptBuilder handles `#include` and `#if`.
- AngelScript runs with ASCII scanner and 8-bit strings (`:277-284`), so strings are raw Latin-1 bytes.

---

## 1. Frame loop (exact order)

`ETHEngine::StartEngine`, `E:ETHEngine.cpp:525-643` [src].

**Step 0 — before any window exists.**
- `PrepareScriptingEngine` registers the API, builds the module and **runs `main()`** (`:267-436`).
- Inside `main()`:
  - `LoadScene` only records a request (`m_nextScene`, `:853-883`).
  - `SetWindowProperties` only stores parameters, because there is no device yet (`:144-166`).
  - `HideCursor(true)` works immediately: it stores a flag and calls Win32 `ShowCursor(FALSE)` [dis DLL@0x10006900].
  - `GetTime()` would return 0 and log "Invalid device" [dis DLL@0x1000c770]. Penumbra's main() does not call it.
- Then `StartApplication` creates the device:
  - the window is centred (`:540-546`);
  - black clear colour;
  - quit keys are only Alt+F4 [gs2d G:Video/Direct3D9/gs2dD3D9.cpp:453-456];
  - shaders are loaded from `data/`.

**Per-frame steps.**
1. `m_video.SetupProc()`: message pump. On device-lost or minimised it returns SKIP and the whole frame is skipped (`:568-572`).
2. `m_input.UpdateInputData()`: input is sampled **once per frame** (`:575`).
3. The **scene loop function** runs, if set (`:578-579`).
4. **Deferred scene load.** If a LoadScene is pending (requested by main, by this loop, **or by the previous frame's callbacks**), `ETHEngine::LoadScene` runs now (`:582-586`; details in §2.10). This runs the new **preLoop inside this step**.
5. `m_timer.CalcLastFrame()` (`:589`).
6. `BeginSpriteScene()` [gs2d G:…gs2dD3D9.cpp:522-581]:
   - clears colour to SetBackgroundColor and Z to 1.0;
   - `ZFUNC=LESSEQUAL`;
   - mip filter POINT;
   - `SetAlphaMode(PIXEL)`.
7. **Background image**, if set (`:594-608`):
   - drawn with the camera forced to (0,0);
   - rectangle `m_v2BGMin`..`m_v2BGMax`;
   - alpha mode `m_bgAlpha`;
   - Z test and Z write are off, so it never occludes.
8. `m_pScene->RenderScene(m_roundUpPosition)` (`E:ETHScene.cpp:601-643`):
   - a. `CheckTemporaryEntities()` (`:445-498`). It updates particle systems of every dynamic-with-callback or temporary entity, visible or not. It also deletes finished temporary entities (particle-only, finite repeat, no sprite).
   - b. `RenderList` (`:758-965`). It collects the entities of the **visible buckets** and sorts them by drawHash (§3.1). Then, per entity in that order:
     - ambient pass;
     - queue its halo;
     - queue its particles;
     - queue its callback, if the entity is static;
     - light passes and shadows.
   - c. `RenderParticleList` (`:1091-1132`). If any particle entity was visible, it then calls `gsSeed(ms)`.
   - d. Halos (`:697-710`).
   - e. Z test and Z write off.
9. `DrawTopLayer()` (`E:ETHEngine.cpp:645-672`):
   - every queued HUD primitive is drawn **in submission order**, then removed (DrawText lives exactly one draw);
   - camera (0,0), depth 0;
   - Z test and Z write are switched on for this step, then restored to off.
10. `EndSpriteScene()`: Present, then `ComputeFPSRate()` [dis DLL@0x10006500].
11. **`m_pScene->RunCallbacksFromList()`**. **All `ETHCallback_*` functions run here, after Present** (`E:ETHScene.cpp:972-1019`):
    - first, every entry of `m_dynamicOrTempEntities` (dynamic entities with a callback, plus temporaries), in insertion order, **visible or not**;
    - then the static entities with callbacks that were **drawn this frame**, in draw order.
12. `GarbageCollect(asGC_ONE_STEP)`.

### Consequences the port must reproduce

**What is on screen in frame N.**
- It shows entity state from **frame N−1's callbacks**, with the camera set by **frame N's loop**.
- So anything a callback does appears one frame later: moves, SetFrame, SetColor, SetAmbientLight, SetBackgroundColor, PositionBackgroundImage.
- Example: `positionEnvironmentElements` (`A:environment.as:122-143`) pins the clouds/fog layer to the camera from inside a callback, so that layer lags one frame of camera motion.

**When HUD primitives are drawn.**
- Primitives queued **in callbacks** are drawn in the **next** frame's DrawTopLayer. Examples:
  - `drawPlayerStatus` (`A:interface.as:50-95`);
  - `ETHCallback_story`'s `shadowText`;
  - `next_level`'s `fadeOut` and `loadingMessage`.
- Primitives queued in the loop and in preLoop are drawn in the same frame.
- Draw order within one DrawTopLayer is: callbacks(N−1), then loop(N), then preLoop(N).
  - So `doLoop`'s `fadeIn()` rectangle (`A:setupScene.as:255`, `A:util.as:379-390`) covers the player-status HUD.
  - `showMessages` and `showTimer`, queued after it in the same loop, draw **on top of** the fade.

**Primitives across a scene switch.**
- The primitive list is **not cleared by LoadScene**, so old-scene callback HUD would draw over the new scene's first frame.
- In practice this is hidden: both `setupScene` and `menuPreLoop` call `drawRect(0xFF000000)` (`A:setupScene.as:220`, `A:menu.as:64`), which is submitted **last** in that frame and covers everything.
- Reproduce that ordering; do not "fix" it into a visible flash.

**Where a LoadScene request comes from matters.**
- From a **callback** (for example `ETHCallback_next_level` `A:main.as:224`, or respawn/death `A:controlCharacters.as:438-449`): the **old scene's loop runs once more** on the next frame, and the load happens after it.
- From the **loop**: the new scene loads, runs preLoop, renders and runs its callbacks, all in that same frame.

**Mid-list changes during callbacks.**
- `AddEntity` inside a dynamic callback appends to the list being iterated (`std::list::push_back`). A new dynamic entity with a callback therefore **runs its callback the same frame**.
- An entity deleted earlier in the same callback pass has `IsAlive()==false`, so its own callback is skipped (`:977-985`, `:999-1006`).

**Static callback entities only tick while "visible".**
- This covers checkpoint, help, story, next_level, play, event01, potions and shooter (§2.6).
- "Visible" means the entity's *origin bucket* is in the visible set and it is not Hide()-den.
- Having no sprite is irrelevant: invisible trigger entities still get callbacks.

### 1.1 The three clocks

**`uint GetTime()`** [dis DLL@0x1000c770]
- Returns `GetTickCount() - startTime` in ms; startTime is taken at StartApplication.
- Resolution is about 15.6 ms (no timeBeginPeriod call).
- `E:ETHEngine.cpp:1002-1005`.

**`float UnitsPerSecond(float x)`** [src; clock resolution inferred]
- Returns `x × seconds between the last two CalcLastFrame() calls` (`E:ETHSpeedTimer.cpp:55-65`).
- `ETHSpeedTimer : boost::timer` uses `std::clock()`. VS2008's static CRT implements that on GetSystemTimeAsFileTime, so resolution is about 15.6 ms.
- **There is no clamp.**
- On the frame after a LoadScene it is ≈0, because CalcLastFrame runs back-to-back (`E:ETHEngine.cpp:849`, then `:589`).

**`float GetFPSRate()`** [dis DLL@0x10006500/0x10006560]
- Starts at 0.
- On every EndSpriteScene: if `GetTickCount()-last > 500`, then `fps = counter*2; counter = 0; last = now`; otherwise `counter++`.
- So it reads about 60 at a 60 Hz vsync, and updates every ~0.5 s.
- The counters are static and survive scene changes.

**`GetLastFrameElapsedTime`** is **absent in 0.7.12**, and the scripts never call it.

**How the scripts mix the clocks** (paths under `A:`):
- **Gravity**: `applyForce(…, UnitsPerSecond(GRAVITY=1200))` changes velocity per frame (`controlCharacters.as:291,334,487,511,535,559,583,607,661`).
- **Movement**: `move()` divides velocity by `GetFPSRate()`, treating 0 as 60 (`util.as:195-196`). So position integration is frame-based while velocity integration is real-time. Same pattern in `events.as:98-100` and `lavaShooter.as:67-71`.
- **Per-frame constants**:
  - `knockVector *= 0.8f` (`util.as:211`);
  - a per-frame step clamp of `min(size)*0.4` (`util.as:199-203`).
- **Earthquake** halves every 70 ms of GetTime (`cameraManager.as:84-104`).
- **Timers and fades** use GetTime, for example `util.as:379-404` with LIVE_FADE_IN_TIME=3000.

The game only behaves as designed at about 60 fps. The original always ran vsynced: `SetWindowProperties(…, sync=true, …)` at `A:main.as:144`.

**Port.** Run scripts at a fixed 60 Hz tick, as Magic Portals does (`Magic-Portals-Remake/game/MagicPortalsLayer.cpp:198-203`, `SimulationClock.fixedDelta`):
- `UnitsPerSecond(x) = x/60`, except **0 on the first tick after a scene load**;
- `GetFPSRate() = 60`;
- `GetTime()` = simulated ms since window creation, as uint32.

---

## 2. Scene and entity model

### 2.1 Buckets

**Where the bucket size comes from.**
- It comes **only** from `LoadScene(scene, pre, loop, vector2 size)` (`E:ETHEngine.cpp:872-883`).
- Default is **256×256** (`E:ETHCommon.h:95`). Minimum is 128; smaller values are rejected and the default kept (`E:ETHScene.cpp:1848-1858`).
- The 1- and 3-argument overloads always get 256, because `m_nextScene` resets after each load (`E:ETHEngine.cpp:69-75`, `:585`).
- Penumbra passes `(1024,256)` only for `scenes/menu.esc` (`A:main.as:126`, `A:menu.as:387,398`). Every other scene, including arena_select, is 256×256.
- **There is no `GetBucketSize`** in 0.7.12.

**Mapping a position to a bucket.**
- `GetBucket(p) = (floor(p.x/bw), floor(p.y/bh))`, using x and y only (`E:ETHCommon.h:418-421`).
- `ETHEntity.GetCurrentBucket()` applies it to the current position (`E:ETHRenderEntity.cpp:1328-1331`).
- A global `vector2 GetBucket(vector2)` also exists but is unused.

**Storage and iteration order.**
- The container is `boost::unordered_map<vector2, std::list<entity*>>` (`E:ETHRenderEntity.h:59-60`).
- Anything that walks all buckets therefore iterates in **arbitrary hash order**: SeekEntity(name/id), GetEntityArray(name), the global Add*Data(entityName,…), and SaveScene.
- Within one bucket list:
  - **type-0 (horizontal) entities are `push_front`-ed**;
  - all other types are `push_back`-ed.
  - This applies both on add (`E:ETHScene.cpp:395-406`) and on bucket change in MoveEntity (`:1822-1826`).

**Culling works on the origin bucket, never on sprite bounds.**
- Visible buckets are `GetIntersectingBuckets(camPos, screenSize, border, border)` (`E:ETHScene.cpp:786-789`, `E:ETHCommon.h:452-473`):
  - it covers every bucket from `floor(cam/b)` to `floor((cam+screen)/b)`, **inclusive at both ends**;
  - border drawing on adds a ±1 ring;
  - iteration is row-major, top-to-bottom;
  - there is a cap of 128 buckets.
- At 1024×768 with 256-px buckets and the border off, the visible set is **always 5 columns × 4 rows = 20 buckets**, whether or not the camera is aligned.
- The border flag defaults to true for each new scene (`E:ETHScene.cpp:96`):
  - Penumbra sets it **false** in `setupScene` and `menuPreLoop` (`A:setupScene.as:167`, `A:menu.as:62`);
  - it sets it **true** every frame in pvp (`A:setupScene.as:250-251`).
- A big sprite whose origin bucket is off-screen **is not drawn**.

### 2.2 IDs, names, callback binding

**IDs** (`int`).
- Scene files keep their own `id` values, and `m_idCounter` becomes `max(id)+1` (`E:ETHScene.cpp:307,335`).
- A new scene starts the counter at 0 (`:88`).
- `AddEntity` hands out `m_idCounter++` (`:372-376`).
- IDs are never reused, and SaveScene persists them.

**Names.**
- `GetEntityName()` returns `sFileName` (`E:ETHEntity.cpp:71-74`).
- For a scene instance it is the `<EntityName>` text, and Penumbra renames many of them: "spawn" ×271, "help", "story", "play", "next_level", "environment", "play_sound", "novo_jogo", "sair", "picker", "thumbnail".
- For `AddEntity("x.ent",…)` it is the basename **including ".ent"** (`E:ETHEntityFile.cpp:533`).
- So scripts compare against strings like `"bruxo.ent"`, `"npc_wall.ent"` and `"spawn"`.

**Callback binding** (`E:ETHScene.cpp:1315-1360`), done once at AddEntity:
- The engine first looks for `ETHCallback_<ID>`.
- If there is none, it uses `ETHCallback_<EntityName cut at the FIRST '.'>` (`RemoveExtension`, `E:ETHCommon.h:309-322`).
- The signature is `void f(ETHEntity@)`, and the entity itself is the argument (`E:ETHRenderEntity.cpp:1132-1164`).
- Penumbra defines no numeric callbacks. `ETHCallback_potion` is dead code, because no entity is named "potion".

### 2.3 Static vs dynamic entities

**Dynamic** (`static="0"`) entities with a callback are put in `m_dynamicOrTempEntities` at AddEntity (`E:ETHScene.cpp:420-425`). They run their callback every frame.

**Static** entities with a callback run it only when drawn (§1).

`TurnDynamic` exists but does not move an entity between the lists, and it is unused.

### 2.4 AddEntity / DeleteEntity / SeekEntity / lookups

**AddEntity** (`E:ETHEngine.cpp:674-792`) [src]
- Overloads: `(file, vector3 pos, float angle)`, `(file, pos, ETHEntity@ &out)`, `(file, pos, string altName)`.
- Reads `entities\<file>` fresh on every call and places the entity at `pos`.
- Colour is white; frame is the .ent's startFrame.
- Returns the ID, or −1 if the file is missing.
- Restarts the entity's particle SFX (`E:ETHScene.cpp:429`).
- The `@out` handle is AddRef'd.
- The entity is in its bucket immediately.

**DeleteEntity** (`ETHEntity@ DeleteEntity(ETHEntity@)`; `E:ETHEngine.cpp:794-804`, `E:ETHScene.cpp:1273-1301`) [src]
- Finds the entity in the bucket of its *current* position.
- Calls `Kill()`, so IsAlive becomes false, and drops the bucket's reference.
- Returns null on success. If the entity is not found it logs and returns the handle.
- **Script handles stay valid and fully readable and writable after deletion**, and IsAlive() is false. Penumbra relies on this:
  - `ETHCallback_checkpoint` calls `thisEntity.GetPosition()` right after `DeleteEntity(thisEntity)` (`A:main.as:186-187`);
  - `g_spawn[t].IsAlive()` and `g_sounds[t].IsAlive()` are checked across frames (`A:setupScene.as:257-326`);
  - `ETHEntityArray` holds references (`E:ETHEntityArray.cpp:73-77`).
- The port needs soft-delete: a dead flag, with the ECS entity destroyed only when the last handle drops.
- On final release, the entity's particle sounds stop (`E:ETHRenderEntity.cpp:88-113`).

**SeekEntity(int id)** and **SeekEntity(string name)** (`E:ETHEngine.cpp:229-265`, `E:ETHScene.cpp:1245-1271`)
- Linear scan in hash order; returns the first match.
- Returns **null for "temporary" entities** (no sprite and every particle system finite) and for deleted ones.
- Uses: `SeekEntity("cursor.ent")` (`A:menu.as:66`); `SeekEntity(ownerID)` (`A:doDamage.as:175,192`).

**GetEntityArray(name, arr)** (`E:ETHScene.cpp:1620-1639`)
- **Appends** exact name matches in hash order.
- Returns false if none were added.

**GetEntitiesFromBucket(bucket, arr)** (`:1696-1711`)
- **Appends** the bucket's list in list order (horizontals newest-first).
- Returns false if the bucket is empty or missing.
- 43 call sites build every neighbour and collision query from it, for example `findAmongNeighbourEntities` (`A:util.as:43-68`) and `findDestinationBuckets` (`A:util.as:326-348`).
- The port must return exactly the entities whose origin is in that bucket.

**GetVisibleEntities(arr)** (`:1714-1739`)
- Appends all entities of the currently visible buckets (hidden ones included).
- Uses the camera at the time of the call and the border flag.

### 2.5 Position and getters [src]

**Setters.**
- `SetPosition`, `SetPositionXY`, `AddToPositionXY` and `AddToPosition` apply immediately.
- If the bucket changes, the entity is relinked into the new bucket's list; horizontals go to the front (`E:ETHRenderEntity.cpp:956-982`).
- They are registered as CDECL_OBJFIRST methods (`E:ETHEngine.cpp:211-215`).

**Getters.**
- `GetSize()` = `GetCurrentSize()` (`E:ETHRenderEntity.cpp:1004-1055`):
  - with a sprite: the frame size `(W/cutX, H/cutY)` using integer strides, or the full bitmap if there is one frame;
  - without a sprite: the halo size if there is a light with a halo, 32×32 if there is a light, the collision xy size if collidable, otherwise 32×32.
- `GetCollisionBox()` returns `{pos,size}` **relative to the entity**, or zeros if not collidable (`E:ETHEntity.cpp:122-129`).
- `Collidable()` and `SetCollision(bool)` read and write the collidable flag.

### 2.6 Custom data, and callback static-ness census

**Behaviour** (`E:ETHDataManager.cpp`, `E:ETHEntityFile.cpp:612-697`):
- `Add*Data` always succeeds, overwriting both the value and its type (`:103-121`).
- `Get*Data` returns 0, 0u, 0.0f or "" when the name is missing **or holds a different type**, and logs (`:123-189`, `E:ETHEntityFile.cpp:636-662`).
  - The scripts rely on "missing = 0", for example in `knockBack` (`A:util.as:161-167`).
  - Types must be preserved, because a type mismatch also reads back as 0.
- `CheckCustomData` returns `DT_NODATA`=0, `DT_FLOAT`=1, `DT_INT`=2, `DT_UINT`=3 or `DT_STRING`=4.
- Custom data comes from `<CustomData>` in the scene instance or the .ent, and SaveScene writes it back.
- The global per-name `Add*Data(entityName,…)` forms are unused.

**Callback static-ness census** (`reference/analysis/cbscan.py`):

| Callback entities | static? | Runs |
|---|---|---|
| checkpoint (10), help (16), story (14), next_level (2), play (3), event01 (1), potion_small (54), potion_large (AddEntity), shooter (18) | 1 | only while the origin bucket is visible |
| bruxo, princess, warrior, knight, minion, impy, paladin, master_knight, king (AddEntity), falling_bridge (43), clouds/fog/dawn, cursor, thumbnail, picker, swords, fire_ball, light_spell, combo_* | 0 | every frame |

### 2.7 Engine collision queries

Three calls share one algorithm and differ only in which entities qualify: `Collide(e[, @out])` (any), `CollideStatic` (static only) and `CollideDynamic` (dynamic only) (`E:ETHScene.cpp:1362-1528`; box test `E:ETHCommon.cpp:128-157`) [src].
- Returns false if the caller is not collidable.
- **Buckets scanned:** from `bucket(box.pos − box.size/2 + pos)` to `bucket(that + 2·box.size)`. This is asymmetric: it reaches further to the right and down.
- **Candidates:** walked in reverse list order, collidable, not the caller's ID.
- **Test:** 3-D **inclusive** AABB overlap on x, then y, then z. Touching counts as a hit.
- **Result:** the first hit, returned with AddRef.

Only `CollideDynamic` is used, once, for the menu cursor (`A:menu.as:246`):
- cursor box: pos (0,0,−64), size (31,31,148);
- menu buttons: dynamic, z=10, box pos (−18,4,0), size (369,25,25).

All gameplay collision is script-side: `GetCollisionBox()+GetPosition()` checked with `checkBoxHit` (2-D inclusive, `A:util.as:350-372`), over `GetEntitiesFromBucket` results.

**`GetClosestContact` does not exist** in 0.7.12.

### 2.8 SetFrame and sprite cut

**`SetFrame(f)`** (`E:ETHEntity.cpp:131-162`)
- It rejects only `f > cutX*cutY`, so the out-of-range value `f == cutX*cutY` is **accepted**. When rejected it sets 0 and returns false.
- When drawing, `GS_SPRITE::SetRect(f)` fails and keeps the shared sprite's previous rect.
- The shared sprite belongs to the last entity drawn with the same image.
- Penumbra only uses frames 4..11 of 4×4 sheets (`A:controlCharacters.as:222-267`), so this never triggers.

**`SetFrame(col,row)`** = `row*cutX + col`.

**Rects** are row-major with integer strides of `W/cutX × H/cutY`, using the **original** image size (§3.5) (`G:Video/Direct3D9/gs2dD3D9Sprite.cpp:217-277`).

### 2.9 Colour and alpha

**`SetColor(vector3)` / `SetAlpha(float)`** set `v4Color` (`E:ETHEntity.cpp:331-341`), which multiplies the ambient and light passes.

**Death fade** (`A:controlCharacters.as:381,399,424,457`)
- Characters die with `SetColor(0,0,0)` plus a SetAlpha fade.
- On **blendMode 2** characters (bruxo.ent, king.ent, princess.ent) alpha does not blend. They stay an opaque black silhouette until texel alpha × alpha ≤ 1/255.
- blendMode 0 characters fade normally.

`SetLayerDepth` would force the type to LAYERABLE; it is unused.

### 2.10 LoadScene / SaveScene / GetSceneFileName

**LoadScene is deferred** (§1 step 4). `ETHEngine::LoadScene` (`E:ETHEngine.cpp:823-851`) runs these steps in order:
1. Creates a new ETHScene. The old one's destructor releases its entities **and every sprite and sample**.
2. Releases both resource managers again (`:826-827`). As a result:
   - **all sounds and music stop**;
   - volumes and loop flags are lost;
   - every loaded sprite is dropped, so preLoops reload them (`A:setupScene.as:114-161`, `A:menu.as:50-72`).
3. Loads the `.esc`. `"empty"` or `""` means no file.
4. Stores `m_sceneFileName` verbatim; `GetSceneFileName()` returns this string (compared at `A:menu.as:385`).
5. Generates lightmaps for static entities.
6. **Resets the camera to (0,0).**
7. Clears the background image and resets the BG alpha mode to PIXEL.
8. Runs preLoop and sets the loop function (`:885-912`).
9. Resets the UnitsPerSecond timer.

State that survives a scene load:
- SetBackgroundColor, UsePixelShaders, SetPositionRoundUp, window state and script globals **persist**.
- Per-scene values start fresh: ambient and zAxisDirection come from the file (zAxisDirection default (0,−1)), and the border flag resets to true.

**SaveScene(path)** (`E:ETHScene.cpp:182-221`)
- **What it writes:** XML in hash order containing the **live** state of every entity:
  - position, colour/alpha, spriteFrame, id;
  - the full definition including CustomData;
  - the **current** ambient light (SetAmbientLight changes the scene properties).
- **What it omits:** hidden flag, particle state, sounds, the camera and globals.
- It returns false if the scene has no entities.
- The path is relative to the program directory: `SaveScene("scenes/checkpoint.esc")` at `A:main.as:184`, reloaded by `A:controlCharacters.as:444`. The port must redirect it to a writable directory.
- The checkpoint entity saves **before** deleting itself (`A:main.as:184-186`), so the checkpoint file still contains that checkpoint.

### 2.11 ENML, files, AngelScript addons [src]

**ENML**
- `enmlFile.get(entity,attr)` returns "" if missing.
- `getInt`, `getUint` and `getFloat` use sscanf `%d`, `%u`, `%f`. **They return false and leave `out` unwritten** when the key is missing (`E:enml.h:663-712`).
- The only missing key the game can hit is `global.lv20`: data.enml jumps from lv19 to lv21. That happens in `addToExp` (`A:util.as:406-418`).
- `spawn()` lookups always hit, and bruxo/princess spawn as `complete`, which skips them.
- `writeToFile` writes `hs.enml` (`A:scores.as:88`).

**Files**
- `GetAbsolutePath(f)` = `programPath + "/" + f` (`E:ETHEngine.cpp:1616-1619`).
- `GetStringFromFile` returns "" if the file is missing (`E:enml.h:107-125`).

**String**
- `string + number` goes through `ostringstream <<`: 6 significant digits, `%g`-style.
- Methods: `length()`, `resize()`, `[]`.

**Random numbers** [dis DLL@0x10004bd0/0x10004f90]
- `rand(n)` is MT19937 over **[0,n] inclusive**.
- `randF(x)` is in [0,x].
- The generator is reseeded from `GetTickCount` ms after any frame with visible particles (`E:ETHScene.cpp:1130`).

**Math**
- `GetAngle(v) = atan2(v.x, v.y)` mapped to [0,2π). The **arguments are swapped**, so the angle is measured from +Y towards +X (`E:ETHScriptObjRegister.cpp:105-109`). `findDirection` depends on this (`A:util.as:242-265`).
- `radianToDegree(a) = a/(2π)·360`.
- `normalize` uses rsqrt, so a zero vector yields NaN.

---

## 3. Rendering semantics

### 3.1 Depth and draw order

These values are computed per entity per frame (`E:ETHScene.cpp:832-855`, `E:ETHRenderEntity.cpp:642-663`).

**depth**

| Type | depth |
|---|---|
| 0 horizontal, 2 vertical | `(z − minH)/(maxH − minH)` |
| 1 ground decal, 4 opaque decal | `(z + 0.1 − minH)/(maxH − minH)` |
| 3 overall | `1.0` |
| 5 layerable | `max(0.001, layerDepth)` |

- `minH` and `maxH` are running extremes that only grow.
- They start at 0 and at **screen height (768)** when the scene loads (`E:ETHScene.cpp:174, 247-248`).
- They are grown by each added or visible entity: `z + spriteH` and `z − spriteH`, plus particle spheres and halo z (`E:ETHRenderEntity.cpp:1166-1192`).

**drawHash** (ascending `std::multimap` key)

| Type | drawHash |
|---|---|
| horizontal | `depth/2` |
| vertical | `0.5 + depth + (pos.y − cam.y)` (raw pixels) |
| decals | `depth/2 + 0.01` |
| overall / layerable | `depth` |

Equal keys come out in insertion order: visible-bucket order, then bucket-list order. That relies on MSVC 2008 multimap placing equal keys at the upper bound; see Open questions.

**Z buffer.**
- **On, with Z-write on, for every ambient pass, blended or not.**
- Ethanon's shaders output `z_clip = 1 − depth` (`A:data/defaultVS.cg:115`, `A:data/defaultStaticAmbientVS.cg:115`), so a **larger depth is nearer**.
- The test is LESSEQUAL against a clear value of 1.0. Among equal depths, the later draw wins.

**Worked example: level1.esc** (`reference/analysis/depthscan.py`)
- Contents:
  - type 0 ×518 and type 5 ×112; all type-5 have `layerDepth=1`: black.ent and instant_death.ent;
  - blendMode 2 ×497, 0 ×133;
  - z spans −86..48, with walls at −84 and 324 entities at z=0.
- minH is about −(84 + wall height); maxH is 768.
- Resulting order, back to front:
  - **walls** (z=−84): hash ≈ 0.115, drawn first and farthest;
  - **characters**, spawned at `spawn.z − 10` (`A:setupScene.as:279`), i.e. z=−10: hash ≈ 0.149;
  - **tiles** (z=0): hash ≈ 0.153, drawn after the characters and nearer, so a tile covers an overlapping character;
  - **black layerables**: depth 1.0, drawn last and nearest.
- Same-z tile overlaps resolve to "later draw wins", where "later" follows bucket row-major order and then reverse insertion order within a bucket.

**Menus** (menu.esc, arena_select.esc, videoModes.esc) use `ZAxisDirection (0,−1)` and type-2 vertical entities. There:
- `screen = (x, y − z)` (`E:ETHCommon.h:423-426`);
- the origin is bottom-centre;
- the hash adds raw screen y.

### 3.2 Passes

**Ambient pass** (`E:ETHRenderEntity.cpp:680-712`)
- Colour: `clamp01(sceneAmbient + emissive.rgb) · v4Color`, with `a = v4Color.a`.
- `ConvertToDW` then normalises the RGB vector if any component exceeds 1 (`E:ETHCommon.h:363-384`).
- Emissive (1,1,1) appears in 124 definitions (fully "unlit") and (0.5,0.5,0.5) in 56.
- Blend mode is the entity's blendMode; opaque decals are forced to ALPHA_TEST.
- Vertical entities never rotate.

**Lights**
- Only visible-bucket entities with `light.active` contribute, and only if they pass `IsSphereInScreen` (`E:ETHScene.cpp:814-829, 1077-1089`).
- A light on an entity whose particle slot 0 is active is dimmed by active/total particles.
- For each light within `range + max(w,h)`, lit entities get an **additive** light pass (`E:ETHShaderManager.cpp:213-359`), followed by shadows.
- Details belong to the lighting report.

**Particles**
- Drawn after all entities (`E:ETHScene.cpp:1091-1132`).
- Each manager has its own timer: `frameSpeed = min(dt·60, 2)` (`E:ETHParticleManager.cpp:650-654`).
- Particle managers updated twice in one frame see dt≈0 on the second update, so there is **no double speed**.

**Halos** (`E:ETHRenderEntity.cpp:736-765`, `E:ETHShaderManager.cpp:392-410`)
- Drawn after particles, with **ADD** blending, at **depth 1.0**, so always in front.
- Size is haloSize².
- Centre is `entityPos + light.pos`.
- Colour is `ConvertToDW(light.color · haloBrightness · particleRatio)`. No halo in the data exceeds 1, so normalisation never triggers.
- No rotation.
- Halo bitmaps are 8-bit grey BMPs (halo.bmp, halo1.bmp, halo2.bmp) loaded with a **black** colour key.

### 3.3 Blend modes (`blendMode` = `GS_ALPHA_MODE`)

Verified by disassembling GameSpaceLib `SetAlphaMode` [dis DLL@0x10006170, jump table DLL@0x10006448]; the enum is at `G:gs2d.h:74-81`.
- Device vtable offsets: `+0xE4` is SetRenderState and `+0x10C` is SetTextureStageState.
- The texture stage is `MODULATE(TEXTURE, DIFFUSE)` for both colour and alpha.
- After each pass the previous alpha mode is restored, so HUD primitives draw with PIXEL.

| Value | Name | D3D9 state | Used by |
|---|---|---|---|
| 0 | PIXEL | blend ON, `SRCBLEND=SRCALPHA(5)`, `DESTBLEND=INVSRCALPHA(6)`, alpha test ON, `ALPHAREF=1`, `GREATER` | 68 .ent: characters, lights, effects |
| 1 | ADD | `SRCBLEND=ONE(2)`, `DESTBLEND=ONE(2)` (colour **not** weighted by alpha), alpha test `>1` | light passes, halos, BG "planets" |
| 2 | ALPHA_TEST | blend OFF, alpha test ON, `REF=1`, `GREATER` (hard cutout) | 76 .ent: tiles, walls, black covers |
| 3 | NONE | blend and alpha test OFF | – |
| 4 | MODULATE | `SRCBLEND=ZERO(1)`, `DESTBLEND=SRCCOLOR(3)`, alpha test `>1` | – |

### 3.4 Coordinates and camera

**Camera.** The camera is the **world position of the screen's top-left**: `screen = world − cam`, with y pointing down (GameSpaceLib's embedded `transformSprite` shader) [dis].

**Camera calls.**
- `SetCameraPos` floors the position when rounding is on [dis DLL@0x10006830]. Rounding defaults to on, and `A:cameraManager.as:139` sets it every frame.
- `AddToCameraPos` does **not** floor [dis DLL@0x100068a0]. The earthquake uses it; then `roundUpCameraPos()` floors the camera again (`A:cameraManager.as:111-146`).

**Origin** (`E:ETHCommon.h:222-241`; `E:ETHRenderEntity.cpp:604-631`)
- Centre for types 0, 1, 3, 4 and 5.
- **Bottom-centre** for type 2.
- `+ pivotAdjust` in both cases.

**Rounding.**
- Entity draw position is `floor(pos) − 0.5` when rounding is on (`G:Video/Direct3D9/gs2dD3D9Sprite.cpp:568-581`).
- Particles, halos and HUD primitives are drawn unrounded (only the −0.5 applies).

**ZAxisDirection** is (0,0) in all levels and pvp, and (0,−1) in the menus.

### 3.5 Textures

**Resource keys** are the **basename** only (`E:ETHResourceManager.cpp:63-118`). `LoadSprite("interface/hp.png")` and `DrawSprite("interface/hp.png")` both resolve to "hp.png".

**Loader** [dis DLL@0x1000cbc0, call at 0x1000cd1b]
- The call is `D3DXCreateTextureFromFileExA` with:
  - `D3DX_DEFAULT_NONPOW2` × 2 for width and height;
  - `D3DX_DEFAULT` mips;
  - `D3DFMT_UNKNOWN`, `D3DPOOL_MANAGED`;
  - `D3DX_DEFAULT` filters;
  - `ColorKey`, `&info`.
- The recorded bitmap size is `info.Width/Height`, i.e. the **original** size. So non-power-of-two sheets keep exact frames: characters are 128×192 with 4×4 → 32×48 frames; master_knight is 320×384 → 80×96.

**ColorKey** (`E:ETHResourceManager.cpp:106`)
- `0xFFFF00FF`: magenta becomes transparent. Used for all sprites.
- `0xFF000000`: black becomes transparent. Used for halos and ADD particles.
- `entities/skull.png` and `interface/skull_interface.png` each contain 177 exact magenta pixels, so **the port must colour-key them**.

**Filtering** is linear min/mag with point mips (`G:…gs2dD3D9.cpp:328, 571-575, 852-873`). At 1:1 with floor and −0.5 texel alignment this is pixel-exact.

### 3.6 HUD primitives (DrawTopLayer: screen space, depth 0)

Common rules:
- Screen space, depth 0.
- The GameSpaceLib default vertex shader sets `z = depth = 0` [dis embedded shader], so these are always in front.
- Drawn in submission order.

| API | Semantics |
|---|---|
| `DrawSprite(name, pos, argb)` | The sprite must be LoadSprite-ed first; otherwise nothing is drawn. Drawn at `pos` with the sprite's shared origin (top-left for HUD-only images) and its current rect, at bitmap size. The colour applies to all four vertices. (`E:ETHEngine.cpp:1157-1161`, `E:ETHPrimitiveDrawer.cpp:158-168`) |
| `DrawShapedSprite(name,pos,size,argb)` | Same, stretched to `size`. |
| `GetSpriteSize(name)` | Bitmap size if loaded, else (0,0). Never loads. |
| `LoadSprite(path)` | Loads with the magenta key; released by every LoadScene. |
| `DrawRectangle(pos,size,c0,c1,c2,c3)` | Untextured quad. Corner colours: **c0 top-left, c1 top-right, c2 bottom-left, c3 bottom-right**, bilinear [dis: `getVertexColor`]. |
| `SetBackgroundColor(argb)` | Clear colour; persists across scenes. |
| `SetBackgroundImage(path)` | Loaded with the magenta key; default rect (0,0)..bitmap size. Cleared by LoadScene. |
| `PositionBackgroundImage(min,max)` | Screen rectangle; min and max are sorted per component. |
| `SetBackgroundAlphaAdd()` | The background is drawn with ADD blending for this scene. |

**DrawText** (GameSpaceLib `PrintText`) [dis DLL@0x10010000]
1. It first tries `fopen(font,"r")`; if a file by that name opens, a bitmap font is used. That never happens for Penumbra.
2. Otherwise it builds (and caches per face and size) a font via `D3DXCreateFontA(dev, Height=(int)size, Width=0, **Weight=1000**, Mip=0, Italic=0, ANSI_CHARSET, OUT_TT_PRECIS, ANTIALIASED_QUALITY, 0, face)`:
   - Arial Narrow comes out as Arial Narrow **Bold**;
   - a positive height means GDI **cell height**, not em size.
3. Then it calls `ID3DXFont::DrawTextA(NULL, text, −1, {(int)pos.x, (int)pos.y, …}, DT_EXPANDTABS|DT_NOCLIP, argb)`:
   - top-left anchored;
   - `\n` breaks the line;
   - tabs expand;
   - alpha-blended;
   - drawn immediately, in HUD submission order.
4. In practice it is always on top, since D3DX's internal sprite is believed to disable Z (not verified).
5. The text is CP1252 (Portuguese).
6. Sizes used are the fixlist set in §0.

**shadowText** = `DrawText(pos + 0.1·size, text, font, size, ARGB(a/2,0,0,0))`, then the coloured text (`A:util.as:450-455`).

---

## 4. Input, audio, window

### 4.1 Input (`ETHInput @GetInputHandle()`; `E:ETHScriptObjRegister.cpp:221-251`, `E:ETHEngine.cpp:202-207`)

**Key state machine** [dis DLL@0x10007290]
- Sampled once per frame with Win32 `GetKeyState(vk) & 0x80`.
- If the window lacks focus, keys read UP and their counters reset.
- States:
  - **HIT** (1) on the first frame down;
  - **DOWN** (2) on later frames down;
  - **RELEASE** (3) on the first frame up;
  - **UP** (0) otherwise.
- `KeyDown(k)` is true for HIT or DOWN [dis DLL@0x100077b0].
- VK map: `G:Input/Win/gs2dWinInput.cpp:73-170`.
- Alt+Enter and Esc are handled by the scripts, not the engine (`A:menu.as:108,396`).
- Keys the game uses: arrows, CTRL, S, D, SPACE, RETURN, ALT, ESC, J, 2, 3, PAGEUP, BACKSPACE, LMOUSE and RMOUSE.

**Cursor**
- `GetCursorPos` returns client-area pixels.
- `GetCursorAbsolutePos` returns desktop pixels.
- `SetCursorPos(v)` takes **desktop** coordinates and applies them at the next update.
- The menu and picker cursors move the OS cursor by joystick axis ×5 per frame (`A:menu.as:239`, `A:videoModes.as:61`). The port needs a virtual cursor.

**GetLastCharInput** returns the frame's last WM_CHAR, excluding Backspace, Esc and Enter; Tab becomes 4 spaces. It is unused by the game.

**Joystick** (winmm `joyGetPosEx`, up to 4 pads)
- `DetectJoysticks()` re-enumerates. It is called every frame by the menu cursor (`A:menu.as:74-90`) and whenever J is held (`A:setupScene.as:393-394`).
- `GetJoystickStatus`: 0 = detected, 1 = not detected, 2 = invalid.
- `GetJoystickXY` returns `(pos − range/2)/(range/2)`, clamped to [−1,1], y down, with a dead zone of |v| < 0.01.
- `JoyButtonState(i, JK_n)` reads winmm button bit n−1 through the same HIT/DOWN/RELEASE machine. Buttons beyond the pad's count read UP.
- Game usage: JK_01..04 for gameplay, JK_10 for start/confirm (`A:playerInput.as:176-290`).
- `JK_UP/DOWN/LEFT/RIGHT` are synthesised from the XY axis at **|axis| ≥ 0.8** [dis DLL@0x10007200; constants at 0x1002f1d0].
- How a modern pad's buttons number under winmm is an open question.

### 4.2 Audio (`E:ETHEngine.cpp:1204-1318`; cache keyed by basename)

**Loading**
- `LoadSoundEffect` and `LoadMusic` each open **one Audiere stream per file**: SFX are decoded into memory, music is streamed.
- Loading an already-loaded basename is a no-op.
- A new sample starts at volume 1 with no loop.

**Playback** [dis DLL@0x100018a0..0x10001ae0]
- **PlaySample** uses **one voice per file**:
  - if the file is already playing, it is reset and restarted;
  - if paused, it resumes;
  - it never auto-loads, and an unknown file logs and returns false.
- `LoopSample` sets Audiere repeat. The game calls it after Play (`A:menu.as:45-47`).
- `StopSample` stops and resets to the start.
- `SetSampleVolume` clamps to [0,1], linear.
- `IsSamplePlaying` stays true while a repeating sample loops.
- `SampleExists` reports whether the file is in the cache.

**Every LoadScene releases (stops) all samples**, so music is re-loaded and re-played:
- `ETHCallback_play` does this for level music (`A:setupScene.as:43-50`);
- `pvpLoop` restarts `chefao.mp3` (`:231-240`).

The game ships **16 mp3 and 19 ogg** files.

### 4.3 Window and video

**SetWindowProperties** (`E:ETHEngine.cpp:144-166`)
- Before the device exists, it only stores the parameters.
- After that, it calls `ResetVideoMode`, sets the title and re-centres the window.
- The game opens at **1024×768, windowed, vsync on, 32-bit**, titled "Penumbra e o Castelo das Sombras - Ethanon Engine" (`A:main.as:144`, `A:constants.as:47`).
- It is called again for Alt+Enter and from the video-mode menu (`A:menu.as:111`, `A:videoModes.as:110`).

**Other queries**
- `Windowed()` reports the current mode.
- `GetScreenSize()` returns the backbuffer size, which is the logical coordinate space; there is **no virtual resolution**.
- `GetVideoModeCount()` / `GetVideoMode(i)` return adapter modes.
- `Exit()` quits after the current frame.

**Pixel shaders**
- `IsPixelShaderSupported()` is true when the device supports PS ≥ 2.0 (`E:ETHShaderManager.cpp:95`).
- `UsePixelShaders(bool)` chooses per-pixel or per-vertex light passes, and the choice persists across scenes.
- The game turns it on at New Game and level change, and off when the "no shaders" option is set (`A:main.as:103-104,222-223`, `A:setupScene.as:117-118`, `A:videoModes.as:126`).

---

## 5. Master table: API → exact semantics → port requirement

All 118 engine-registered identifiers the scripts call (from `reference/analysis/coverage.py`) are below. SetWindowProperties is registered at `E:ETHEngine.cpp:320-322`.

| API | Exact semantics | Port must |
|---|---|---|
| `LoadScene` (1/3/4 args) | Deferred; releases all sprites/samples; camera (0,0); clears BG; pre then loop; bucket 256 (min 128) unless given. §1, §2.10 | Queue the request and apply it at the same point, with the same releases and resets. |
| `SaveScene(path)` | Live XML dump incl. custom data, current ambient, ids, frames; false if empty. §2.10 | Serialize live state to a writable dir and reload it identically. |
| `GetSceneFileName()` | Returns the verbatim string passed to LoadScene. | Store it verbatim. |
| `AddEntity` (angle / @out) | Fresh .ent load; new id; name-bound callback; dynamic+callback → every-frame list; −1 on failure. | Keep ids as max(file)+1…. |
| `DeleteEntity(@e)` | Kill and unlink; handles stay readable; IsAlive false; returns null. | Soft delete. |
| `SeekEntity(int/string)` | Linear scan; first match; null for temporary and deleted entities. | Same. |
| `GetEntityArray(name, arr)` | Appends matches; false if none. | Same. |
| `GetEntitiesFromBucket(b, arr)` | Appends the origin-bucket list (horizontals newest-first); false if empty. | Exact floor(pos/bucket) index. |
| `GetVisibleEntities(arr)` | Entities of the visible buckets. | Same bucket math. |
| `CollideDynamic(e, @out)` | §2.7. | Same scan and 3-D inclusive test, dynamic only. |
| `SetBorderBucketsDrawing(b)` | ±1 ring for culling, static callbacks and GetVisibleEntities; defaults true per scene. | Honour it. |
| `SetPositionRoundUp(b)` | Floors the camera (Set) and entity draw positions. | Floor at draw time. |
| `ETHEntity.GetPosition/GetPositionXY/SetPosition/SetPositionXY/AddToPositionXY/GetCurrentBucket` | Immediate; relinks buckets. | Same. |
| `ETHEntity.GetID/GetEntityName/GetSize/GetFrame/SetFrame/GetCollisionBox/Collidable/SetCollision` | §2.2, §2.5, §2.8. | Same. |
| `ETHEntity.Add/Get{Float,Int,UInt,String}Data`, `CheckCustomData` | Typed; missing or mismatched returns 0/"". | Typed map. |
| `ETHEntity.SetColor/SetAlpha/IsAlive` | §2.9. | Tint; per-entity blend mode. |
| `ETHEntity.HasParticleSystem(n)/KillParticleSystem(n)/MirrorParticleSystemX(n,bool)` | Slots 0..1; Kill stops emission; Mirror flips X (and gravity). | See the particle report. |
| `GetInputHandle` + `GetKeyState/KeyDown/JoyButtonState/GetJoystickStatus/GetJoystickXY/DetectJoysticks/GetCursorPos/GetCursorAbsolutePos/SetCursorPos/GetLastCharInput` | §4.1. | 4-state machine, virtual cursor, pad layer. |
| `GetTime` / `UnitsPerSecond` / `GetFPSRate` | §1.1. | Tick ms / x/60 (0 after load) / 60. |
| `rand(n)`, `randF(x)` | Inclusive ranges. | Any RNG with inclusive bounds. |
| `GetAngle(v)` | `atan2(x,y)` in [0,2π). | Keep the swapped arguments. |
| `normalize`, `radianToDegree`, `ARGB`, `abs/floor/sqrt/min/max` | As in §2.11. | Standard. |
| `GetScreenSize` | Backbuffer size. | Logical 1024×768 or the chosen mode. |
| `SetCameraPos/AddToCameraPos/GetCameraPos` | Floor on Set only. | Same. |
| `SetAmbientLight/GetAmbientLight` | Scene property; saved by SaveScene; reset on load. | Same. |
| `SetBackgroundColor/SetBackgroundImage/PositionBackgroundImage/SetBackgroundAlphaAdd` | §3.6. | BG quad before the world; the clear colour persists. |
| `DrawText/DrawSprite/DrawShapedSprite/DrawRectangle` | §3.6; one-shot, submission order; next frame if queued from a callback. | HUD queue flushed at the equivalent point. |
| `LoadSprite/GetSpriteSize` | Basename cache; magenta key; per-scene. | Same. |
| `LoadSoundEffect/LoadMusic/PlaySample/LoopSample/StopSample/SetSampleVolume/IsSamplePlaying/SampleExists` | §4.2. | One voice per file; restart on play; OGG+MP3; stop all on load. |
| `SetWindowProperties/Windowed/GetVideoModeCount/GetVideoMode/HideCursor/Exit` | §4.3. | Window API. |
| `UsePixelShaders/IsPixelShaderSupported` | Chooses the light-pass shader. | Return true; the flag switches normal-mapped vs flat lighting. |
| `GetAbsolutePath/GetStringFromFile`, `enmlFile.parseString/get/getInt/getUint/getFloat/writeToFile/exists/addEntity`, `enmlEntity.add/clear` | §2.11. | ENML reader and writer; hs.enml in a writable dir. |
| `ETHEntityArray` (size, [], +=, clear, push_back), `dictionary` (get/set/delete/deleteAll), `array<T>`, `string` | 2010 addons; the array refcounts its entities. | C++ containers. |
| `collisionBox{pos,size}`, `videoMode{width,height,format}`, `vector2/3` | Value types. | – |

**Enums the scripts use** (`E:ETHScriptObjRegister.cpp:406-587`):
- `KEY_STATE`: KS_UP 0, HIT 1, DOWN 2, RELEASE 3.
- `J_STATUS`.
- `J_KEY`: JK_01..32, plus JK_UP, DOWN, LEFT and RIGHT.
- `KEY`.
- `DATA_TYPE`.
- `PIXEL_FORMAT`: PF32BIT, PF16BIT.
- `ENTITY_TYPE`: 0 HORIZONTAL, 1 GROUND_DECAL, 2 VERTICAL, 3 OVERALL, 4 OPAQUE_DECAL, 5 LAYERABLE.

**Not in 0.7.12, and not called**: GetLastFrameElapsedTime, GetBucketSize, GetClosestContact, SetZAxisDirection, bitmap-font DrawText.

---

## 6. 2010 (0.7.12) vs Dec-2013 (Magic Portals reference): do not reuse MP glue blindly

The 2013 source is `Magic-Portals-Remake/reference/ethanon/toolkit/Source/src/engine`.

| Topic | 0.7.12 | 2013 |
|---|---|---|
| Callback timing | Loop → load → render → callbacks after Present; static callbacks in draw order | `ETHEngine::Update` (`ETHEngine.cpp:141-172`) → `ETHScene::Update` (`Scene/ETHScene.cpp:459-497`): always-active callbacks → loop → map → static callbacks → then RenderFrame |
| Scene load | After the loop, same frame | Start of the next Update (`:153,174-191`) |
| UnitsPerSecond | clock()-based seconds | truncated ms/1000 (`Script/ETHScriptWrapper.System.cpp:95-98`) |
| GetLastFrameElapsedTime | absent | present |
| drawHash | horizontal depth/2; vertical 0.5+depth+(y−camY); decals depth/2+0.01 | horizontal depth; vertical depth+(y−camY)/screenH+0.1; decals depth+0.1 (MP `docs/ethanon-formats.md` §5.3) |
| Visible buckets | exact (+1 ring if border) | adds a clearance factor (`Scene/ETHScene.cpp:525-532`) |
| DrawText | D3DX system font, weight 1000 | bitmap .fnt |
| Physics | none (script AABB) | Box2D |
| Resolution | backbuffer | fixedWidth/fixedHeight virtual space |
| blendMode enum | same numbering | same numbering |

---

## 7. Script-defined symbols (NOT engine)

**eth_util.as** is identical to the engine's shipped `Ethanon Machine/eth_util.as`. It defines `Vector2ToString`, `Vector3ToString`, `FormatToString`, `stringInput`, and `frameTimer` (`Set`/`Get`, built on GetTime).

**Game classes and names:**
- Classes: `CameraManager`, `MessageManager`, `Message`, `Timer`, `Combo`, `Switch`.
- `Switch` members: `Set`, `Get`, `getCurrent`, `setCurrent`, `put`.
- The remaining 163 helper names listed by coverage.py.

**Constants** live in `A:constants.as`, for example `endl`, `GRAVITY=1200`, `SIZE_TOLERANCE=76`, `MAIN_CHARACTER_ENTITY0="bruxo.ent"`.

**Callbacks:** every `ETHCallback_*` function is script-defined.

---

## 8. Supersonic engine gaps

Read-only sources: `AGENTS.md`; `README.md` (Features, "How a frame runs"); `ARCHITECTURE.md` §4, §10 and §11; `src/core/AudioClip.hpp`.

**Another project (Magic Portals) ports a different game on the same engine**, so every item below should be an **opt-in addition**, never a change to existing behaviour. See `engineGaps` for the list.

## Key facts

- Penumbra runs Ethanon 0.7.12. machine.exe says FileVersion 0.7.12 and was built 2010-11-03. That build is SourceForge SVN tag v0-7-12, a copy of trunk r483 (https://svn.code.sf.net/p/ethanon/code/tags/v0-7-12). The graphics, input and audio layer is the closed GameSpaceLib 1.6.4.1, which I checked by disassembly. GitHub history only starts in May 2012.
- A 2012 Ethanon testbed (git 816e8d6, testbed/PenumbraTest) has scripts identical to the shipped ones except for a DrawText shim. Its fixlist names every font the game uses: Arial Narrow 15/16/20/25/30/40/50/60/256, Arial Black 17, Arial 30 and Verdana 15.
- Frame order: input → loop → deferred LoadScene (which runs preLoop) → render → HUD → Present → ALL ETHCallback_* functions. Callbacks run after Present. Dynamic callback entities go first, every frame; static ones go next, and only if their origin bucket was drawn. HUD drawn from callbacks therefore appears in the next frame.
- blendMode values: 0 = PIXEL (SRCALPHA/INVSRCALPHA), 1 = ADD (ONE/ONE), 2 = ALPHA_TEST (no blending, hard cutout), 3 = NONE, 4 = MODULATE (ZERO/SRCCOLOR). Every mode uses alpha test with ALPHAREF=1 GREATER. Verified at GameSpace.dll@0x10006170. Penumbra entities use only 0 and 2.
- Depth test: z_clip = 1 − depth with LESSEQUAL, and z-write is on for every ambient pass, blended or not. Draw order is an ascending sort on drawHash: horizontal = depth/2, vertical = 0.5 + depth + (y − camY), decal = depth/2 + 0.01, layerable/overall = depth. For types 0/2, depth = (z − minH)/(maxH − minH). For layerables, depth = max(0.001, layerDepth).
- Buckets: default 256×256, minimum 128. The menu is loaded with 1024×256. A bucket is floor(pos.xy/size). Culling and static callbacks go by the origin's bucket only. The visible set is inclusive at both ends, so 1024×768 always gives 5×4 buckets. SetBorderBucketsDrawing(false) is used in levels and menus; pvp sets it true. There is no GetBucketSize and no GetClosestContact.
- DeleteEntity kills and unlinks the entity immediately, but script handles stay readable afterwards; the checkpoint callback depends on this. The port needs soft-delete with refcounting.
- LoadScene is deferred and releases every sprite and every sample, so music stops and volumes reset to 1. It also resets the camera to (0,0) and clears the background image. The background colour, UsePixelShaders and round-up state persist.
- Three clocks: GetTime = GetTickCount ms since window creation. UnitsPerSecond(x) = x × clock()-based frame seconds, with no clamp, and ≈0 on the first frame after a load. GetFPSRate = a 500 ms window counter; it starts at 0 and the scripts treat 0 as 60. Movement divides by GetFPSRate, gravity uses UnitsPerSecond. Use a fixed 60 Hz tick.
- DrawText = D3DXCreateFontA(Height = (int)size, which is GDI cell height; Weight = 1000, i.e. bold; ANTIALIASED; ANSI) then DrawTextA with DT_EXPANDTABS|DT_NOCLIP, top-left at the truncated position. Text is CP1252. Everything renders bold.
- Audio is one Audiere stream per file, cached by basename. PlaySample restarts the sound if it is already playing, so there is no overlap. LoopSample sets repeat; SetSampleVolume is linear and clamped to 0..1. The game ships 16 mp3 and 19 ogg files.
- Textures load at their original non-power-of-two size (D3DX_DEFAULT_NONPOW2, size taken from D3DXIMAGE_INFO), so a 128×192 sheet cut 4×4 gives 32×48 frames. Colour keys: 0xFFFF00FF (magenta) for sprites, 0xFF000000 (black) for halos and ADD particles. skull.png and skull_interface.png contain exact magenta pixels.
- Input: keys are read once per frame via GetKeyState and go UP → HIT → DOWN → RELEASE. KeyDown means HIT or DOWN. Joysticks go through winmm joyGetPosEx: axes are normalised to [-1,1] with a 0.01 dead zone, and the JK_UP/DOWN/LEFT/RIGHT buttons fire at |axis| ≥ 0.8. SetCursorPos uses desktop coordinates.
- Custom data getters return 0 or "" when the key is missing or has the wrong type. enml getInt/getFloat leave the out-param unwritten when a key is missing, and data.enml lacks the lv20 key. GetAngle(v) = atan2(v.x, v.y), giving [0,2π). rand(n) returns 0..n inclusive.
- Differences from the 2013 engine that Magic Portals was built on: callbacks there run before render, the drawHash formulas differ, UnitsPerSecond uses truncated milliseconds, text uses bitmap fonts, and a virtual resolution exists. Magic Portals timing and draw-order glue does not transfer.

## Engine gaps

- Add an opt-in sprite depth mode that orders entities by drawHash and depth-tests with z = 1 − depth (LESSEQUAL). Depth must stay written for blended sprites too. Today's transparent pass sorts back to front with depth writes off. Make this a new flag, not a change to the existing pass.
- Add a new ONE,ONE blend equation for halos, light passes and the additive background. Do not redefine the existing Add equation, which is SrcAlpha,One. Optionally add ZERO,SRCCOLOR modulate. Every Ethanon mode also needs the alpha test at > 1/255; alphaCutoff can cover ALPHA_TEST.
- Add an opt-in per-texture colour key at load time: exact opaque 0xFFFF00FF or 0xFF000000 becomes transparent black. stb_image loading has none.
- Add an OGG Vorbis decoder: 19 of the 35 sounds are .ogg, and AudioClip::Load accepts only .wav and .mp3 (MP3 via Media Foundation on Windows). Also add a non-positional 2D play path and streaming music, since whole-clip decode on first play causes a hitch.
- Add a sample-semantics layer: one voice per file, replay restarts it, pause/resume, repeat flag, linear volume, stop everything on scene switch. The current AudioSystem is component/listener-distance based.
- Add a system TrueType text renderer: bold weight, GDI cell-height sizing, CP1252, multi-line with \n and tabs, anchored top-left. Required faces are Arial Narrow Bold, Arial Black, Arial Bold and Verdana Bold. The UI canvas uses only the embedded Inter font.
- Add a winmm-style joystick model: buttons numbered 1..32, axes normalised to [-1,1] with a 0.01 dead zone, synthesised arrow buttons at ±0.8, and the same 4-state HIT/DOWN/RELEASE/UP machine for keys and buttons. Also a virtual cursor with absolute set.
- Allow entities to be soft-deleted: they keep their data readable through script handles after deletion, with destruction deferred until the last reference is gone. EnTT destroy is immediate.
- Add an origin-bucket spatial index with exact floor(pos/bucketSize) semantics and Ethanon's list order (horizontal entities go to the front). Neither the physics world queries nor a radius query can stand in for GetEntitiesFromBucket.
- Add a post-render callback phase, and a HUD command queue that survives into the next frame and across scene switches. Both are needed to reproduce 0.7.12's callback and HUD timing.
- Add a deferred scene switch that releases every sprite and sound, plus live-state scene serialisation (SaveScene) to a user-writable path.
- Add runtime window control: switch between windowed and fullscreen, resize to an enumerated display mode, and list display modes (GetVideoModeCount/GetVideoMode).

## Open questions

- GameSpaceLib 1.6.4.1 source is unavailable, so these claims still come only from the GS2D r485 port and were not checked by disassembly: D3D9 BeginScene state (LESSEQUAL, POINT mip filter), linear min/mag filtering, WM_CHAR handling in GetLastCharInput, joystick axis normalisation and the 0.01 dead zone, the Audiere stream mode for music versus sound effects, and ResetVideoMode behaviour. The disassembly did confirm FPS, time, camera, keys, arrow thresholds, audio play/stop/loop/volume, blend states, texture loading and fonts.
- Actual clock() and GetTickCount granularity on players' 2010 machines (about 15.6 ms unless something called timeBeginPeriod). This sets how jittery UnitsPerSecond and GetTime were at 60 Hz.
- AngelScript 2010 &out parameters for primitive types go through an uninitialised temporary. When enml getInt misses a key (global.lv20 in addToExp at level 20), what value gets copied back into the script variable?
- How a modern (XInput) pad's buttons are numbered under winmm/DirectInput is unverified. If Start ends up as JK_08 and stick clicks as JK_09/JK_10, then JK_10 (the confirm/summon button, playerInput.as:283) would be a stick click. The port probably needs a remap by meaning (Start = confirm) rather than by button number.
- Does MSVC 2008's std::multimap::insert put equal drawHash keys at the upper bound, keeping insertion order? Among same-z tiles (324 at z=0 in level1) this decides which one draws on top.
- Does ID3DXFont::DrawText with a NULL sprite (D3DX's internal sprite) disable Z testing? In practice text always appears on top, but this was not verified.
- How to match GDI positive-height (cell height) font metrics and DrawText line advance (tmHeight) exactly with a TTF rasteriser. A pixel-comparison test is needed.
- SaveScene writes entities in unordered_map hash order. After reloading a checkpoint, bucket-list order can differ from the original level file, and with it equal-depth overlap order, spawn order (and so ID order) and callback order. Accept this divergence or fix it deterministically?
- What gsSeed-per-frame reseeding of the MT generator (from GetTickCount milliseconds) does to perceived randomness. Probably irrelevant, but it makes rand() effectively time-driven.
- The particle-system, lighting-shader (per-pixel versus per-vertex, shadows) and lightmap-generation semantics are only summarised here and belong to the other dimension reports: hPixelLight.cg/vPixelLight.cg math, shadow.dds projection, and the .par fields.
