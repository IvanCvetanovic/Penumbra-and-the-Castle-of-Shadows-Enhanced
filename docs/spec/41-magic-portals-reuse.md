# What Magic Portals already solved, and what a Penumbra port should reuse

Scope: read-only survey of `C:\Users\icvet\Desktop\Magic-Portals-Remake` (MPR). It is set against the engine at `C:\Users\icvet\Desktop\Supersonic-Engine` (SE) and the extracted Penumbra original at `...\Penumbra-and-the-Castle-of-Shadows-Enhanced\extracted\app` (PEN). Nothing was modified. Snapshot taken 2026-09-27, about 18:10.

## 0. The main finding

MPR's knowledge is a decode of the Dec-2013 Ethanon source (`reference/ethanon`, HEAD `c140bd3`). It was measured against an Android GLES2 build: an RGB565 surface, UTF-16LE `.ent`/`.esc` files, `app.enml` density tiers, and Box2D bodies. Penumbra is a 2010 PC build instead: D3D9 plus NVIDIA Cg, GameSpaceLib 2008-2010, an older XML writer, and ASCII/Latin-1 files.

- **What carries over is mostly the pattern:** the repository and build layout, the discipline, strict readers, data with provenance, pure `sim/` functions pinned by suites, and the engine seam (quads, `Sprite2DLight`, `Light2DComponent`, `ScreenOverlay`, `AudioEngine`).
- **Many of MPR's numbers and rules are Android- or 2013-specific.** Copied blindly, they would mis-render Penumbra. Sections 3 and 4 flag each one.

State of things right now:
- SE `main` HEAD is `4bfcf67` ("The games moved to their own repositories"). That is exactly MPR's submodule pin, and SE's working tree was clean when checked. The other session may move it at any time.
- Another agent is already scaffolding the Penumbra repo:
  - `git status` shows `A .gitmodules`, `A engine` (submodule at `4bfcf67`), and untracked `.gitattributes`, `.gitignore` and `tools/build.bat`.
  - `tools/build.bat` is a near-verbatim copy of MPR's `port_build.bat` (same vcvars path and configure line).
  - Do not duplicate that work.

---

## 1. Repository and build pattern to copy

### 1.1 Layout (MPR `README.md:96-110`, `CLAUDE.md:160-187`)
```
CMakeLists.txt     engine/ as a subproject, then game/ and tests/
engine/            SE as a git submodule pinned to one commit (never edited here)
game/              main.cpp, <Game>Layer.cpp/.hpp, LevelVisit (dev), sim/, data/ (port's own JSON)
tests/             test_<prefix>_*.cpp suites + CMakeLists.txt
data/              remake's gameplay JSON (committed)
tools/             converter (Python), parity scripts, port_build.bat
docs/              ethanon-formats.md, original-gameplay.md, parity*.md, permission.md, planning/
build/ out/ reference/   gitignored
```
- MPR has 183 commits.
- Sizes: `MagicPortalsLayer.cpp` 7,164 lines and `.hpp` 2,123. Port code in `game/*.{c,h}pp` plus `game/sim/*` is 34,722 lines.
- There are 44 `test_mp_*` suites. `DEVLOG.md` is 2,394 lines. The remaster planning doc is 16,568 lines.

### 1.2 Top-level CMake (MPR `CMakeLists.txt:1-47`)
- `cmake_minimum_required(VERSION 3.20)`, `project(MagicPortals LANGUAGES C CXX)`, C++20 required with no extensions (`:25-27`).
- **Submodule guard** (`:29-34`): the configure fails with a "run `git submodule update --init`" message when `engine/CMakeLists.txt` is missing. Then `add_subdirectory(engine)` (`:35`).
- Built as a subproject, SE turns its editor, script plugin and suites off by default. Each option defaults to `SUPERSONIC_IS_TOP_LEVEL` (SE `CMakeLists.txt:241`, `:538`, `:568`).
- SE bakes `SUPERSONIC_ASSET_ROOT` to its checkout (`:437-452`), so the game finds the engine's shaders in `engine/`.
- `option(SUPERSONIC_BUILD_MAGICPORTALS ... ON)` gates only the executables (`:39`). `option(MAGICPORTALS_BUILD_TESTS ... ON)` calls `enable_testing()` then `add_subdirectory(tests)` (`:42-47`). The engine only enables testing for its own suites, so the game must call it.

### 1.3 Game targets and how the port finds its data (`game/CMakeLists.txt`, `game/sim/CMakeLists.txt`)
Targets:
- `MagicPortalsSim` is a STATIC library of every `sim/*.cpp`, renderer-free and always built, "because the tests link it" (`game/sim/CMakeLists.txt:105-170`).
- `MagicPortalsGame` is a STATIC library holding only `MagicPortalsLayer.cpp`, PUBLIC-linked to `SupersonicCore` and `MagicPortalsSim` (`game/CMakeLists.txt:68-73`). Being a library lets `test_mp_layer` attach the layer to a bare registry with no window and no Vulkan (`tests/CMakeLists.txt:181-184`).
- The executable `MagicPortals` is built from `main.cpp` plus `LevelVisit.cpp` (dev only). A measurement tool, `MagicPortalsSpike`, sits beside it (`game/CMakeLists.txt:85-96`).
- Every target is compiled with `/W4` under MSVC and `-Wall -Wextra` elsewhere (`game/CMakeLists.txt:75-79, 98-104`; `sim/CMakeLists.txt:228-232`).

Data paths are CMake cache variables baked into compile definitions (`game/sim/CMakeLists.txt:172-223`):
- `_magicPortalsRepo` is set to `${CMAKE_CURRENT_SOURCE_DIR}/../..` (`:173`).
- `SUPERSONIC_MAGICPORTALS_LEVELS` is a `CACHE PATH` defaulting to `<repo>/out/levels` (`:184-185`). `_DATA` defaults to `<repo>/data` (`:192-193`) and `_CHAPTERS` to `_LEVELS/../data/chapters.json` (`:197-198`).
- `_ORIGINAL` defaults to `_LEVELS/../../reference/extracted/assets` (`:207-208`) and `_ACHIEVEMENTS` to `_LEVELS/../data/achievements.json` (`:214-215`).
- `target_compile_definitions(MagicPortalsSim PUBLIC MAGICPORTALS_ORIGINAL_DIR=... MAGICPORTALS_LEVELS_DIR=... MAGICPORTALS_DATA_DIR=... MAGICPORTALS_CHAPTERS_FILE=... MAGICPORTALS_ACHIEVEMENTS_FILE=... MAGICPORTALS_PORT_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../data")` (`:217-223`).
- The rationale is at `:175-182`: absolute paths because ctest runs from the build tree, so relative paths would be "right for nobody".
- These are only compile definitions, so the build compiles without the data. Suites that need the data exit 77.

Consumed in `MagicPortalsLayer::Paths` as defaults, e.g. `std::string levels = MAGICPORTALS_LEVELS_DIR;` (`MagicPortalsLayer.hpp`, struct `Paths`). `main.cpp` overrides them with `--levels`, `--art`, `--data` and `--saves` (`main.cpp:252-276`).

**Penumbra adaptation.** The originals are committed under `extracted/app`, so a `PENUMBRA_ORIGINAL_DIR` default of `<repo>/extracted/app` is always present in a clone. The skip-with-77 machinery below then matters much less. It is still worth keeping for anything generated into `out/`.

### 1.4 Tests layout (`tests/CMakeLists.txt`, SE `cmake/SupersonicTesting.cmake`, SE `tests/TestHarness.hpp`)
- Each suite is one `.cpp` built by SE's `supersonic_add_test(name)`. It links `Supersonic::TestHarness` (header-only) and `SupersonicCore`, compiles at `/W4`, and calls `add_test` (SupersonicTesting.cmake:26-45).
- MPR wraps this in `add_mp_test(name [SKIPS_WITH_77])` (`tests/CMakeLists.txt:26-36`). The wrapper links `MagicPortalsSim`, sets `SKIP_RETURN_CODE 77` when marked, and refuses unknown arguments.
  - The mark sits on each suite's own line because a separate list had "nine suites added without" it. That was SE `80a4ed2`, DEVLOG session 18 (`DEVLOG.md:~2008-2016`).
- The harness provides `CHECK`, `CHECK_MSG`, `CHECK_NEAR` and `CHECK_EQ`, each evaluating its arguments once. `test::summary(suite, minChecks)` fails a suite that ran fewer checks than its floor, which is the guard against "the suite skipped work" (TestHarness.hpp:37-47).
- Suite style: a header comment states the oracle and where each pinned number came from, e.g. `test_mp_levels.cpp:1-18`, "counts taken with grep on 10 September 2026 ... independently of this reader".
  - Hand-written fixtures run anywhere (`test_mp_tscn.cpp:1-11`). Suites that read the real data skip with 77 and print the path they looked in.
- The bar: zero warnings (SE `CONTRIBUTING.md` "Zero warnings", MPR `CLAUDE.md:246-255, 278`). Run the touched suites directly, once each, and record the check counts.

### 1.5 `tools/parity/port_build.bat` (all 18 lines; CRLF)
- Line 9 derives `REPO` from `%~dp0..\..`.
- Line 10: `call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"`.
- Lines 11-14 are the submodule guard.
- Lines 15-17 configure only when `build\CMakeCache.txt` is absent:
  `cmake -S "%REPO%" -B "%REPO%\build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DSUPERSONIC_ENABLE_VALIDATION=ON -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DGLSL_COMPILER=C:/VulkanSDK/1.4.357.0/Bin/glslc.exe`
  - `cl` is named outright because CMake would otherwise pick Strawberry Perl's GCC from PATH.
- Line 18: `cmake --build "%REPO%\build" %*`.
- From Git Bash, call it as `cmd //c "tools\parity\port_build.bat --target X"`.
- Environment (`CLAUDE.md:126-158`):
  - MSVC Build Tools 18, toolset 14.50 x64.
  - CMake 3.29.2, Ninja 1.12.0 and ctest from `C:\Strawberry\c\bin`.
  - Vulkan SDK 1.4.357.0.
  - Python 3.14.3.
  - No `D:` drive.
  - Windows Smart App Control in enforce mode.
- The port build never recompiles the engine's shaders; the committed `engine/assets/shaders/*.spv` are used.

### 1.6 `.gitignore`, `.gitattributes`, `.gitmodules`
- `.gitignore:1-31` holds the long comment block plus `reference/`. It is deliberately unanchored so it matches at any depth.
- Also ignored: `build/` (`:51`), `out/` (`:71`), and `**/game/assets/` (`:101`; the `**/` is required because an internal slash anchors a pattern).
- Python caches (`:54-62`) and OS/editor cruft (`:110-114`) are ignored too.
- `.gitattributes`:
  - `*.bat`/`*.cmd` are `text eol=crlf` (`:9-10`); a LF `.bat` mis-parses in cmd.exe.
  - `.tscn`, `.json`, `.py`, `.toml`, `.md` and `.sh` are LF (`:12-17`).
  - The dotfiles `.gitignore`, `.gitattributes` and `.gitmodules` are pinned to LF (`:22-24`) so git stops warning on every touch.
  - `* text=auto` (`:27`).
- `.gitmodules` has **no `branch =`**: "the pin is the contract" (DEVLOG session 19 decision, `DEVLOG.md:~2170`).
- The draft in the Penumbra repo already has these ideas. Its `.gitattributes` adds `*.cpp`, `*.hpp` and similar as LF, and notes that `extracted/` was committed with `text=auto`.

### 1.7 `game/main.cpp` pattern (worth copying almost verbatim)
- Game flags are pulled out of `argv` before `Supersonic::LaunchOptions::Parse`, which refuses unknown flags (`main.cpp:30-40, 94-273, 278`). Usage text is appended to `LaunchOptions::Usage()` (`:279-313`).
- The engine owns `--fixed-step`, `--frames`, `--screenshot`, `--screenshot-every` and `--window`. The game adds its dev flags: `--hold`, `--tap`, `--drag`, `--press`, `--visit-levels` and `--light-masks-off`.
- The log goes to a file with `Supersonic::Log::SetFileSink(temp/magicportals.log)` (`:325-328`), because a WIN32 exe has no console.
- `GameManifest manifest; manifest.isGame = true; manifest.title=...; manifest.startupScene.clear();` (`:332-338`). Leaving `startupScene` at its default opens the editor demo scene beside the game.
- The save directory is resolved once in `main` (`Supersonic::UserDataDirectory(title)`) and injected into the layer's `Paths.saveDir` (`:340-357`). Suites build the layer bare with an empty `saveDir`, so they never write files.
- It logs `"Vulkan validation layers: ACTIVE"` or `"NOT LOADED"` (`:364-377`). The run fails if `VulkanContext::ValidationErrorCount() > 0` (`:407-413`).
- The layer is pushed with `app.PushLayer(std::move(game))` (`:386`). `LevelVisitLayer` is an optional second dev layer (`:387-389`).
- **Working-directory pitfall.** The game makes `engine/` its working directory. Every path flag, including `--screenshot`, must therefore be absolute (README "Run it", `CLAUDE.md:250-255`).

---

## 2. Commit, devlog and planning-doc conventions

### 2.1 Commits (`CLAUDE.md:264-270`)
- Author: `IvanCvetanovic <icvetanovic99@gmail.com>`, with **no AI trailers**: no `Co-Authored-By`, no session line, no mention of AI.
- Title: `Magic Portals: <what, in plain words>`. Docs-only commits use `Parity: ...` or `Docs: ...`.
- The body explains what changed, what was wrong before, the census numbers, suite check counts, sweep results, the Smart App Control refusals, and "Engine unchanged at `<sha>`".
- The last line is `Record: docs/planning/2026-09-11-magic-portals-remaster.md, step N`. See `git log -1` (step 92) for a full example.
- Before any commit, `git diff --cached --name-only | grep -iE '\.(png|bmp|jpg|jpeg|mp3|ogg|wav|esc|ent|enml|par|fnt)$'` must print nothing, and nothing from `reference/` or `out/` may be staged.
- Never force-push or `reset --hard`. An engine gap is fixed in SE first; `engine/` then moves to that commit in a separate commit (`CLAUDE.md:208-211`, rule 5).
- **Divergences in Penumbra, which Ivan must decide:**
  - Commit `89a684b` in the Penumbra repo is authored `Ivan Cvetanovic <a12029594@unet.univie.at.ac>`.
  - This session's harness asks for a `Co-Authored-By` trailer, but it also says a user CLAUDE.md rule takes precedence. MPR's CLAUDE.md forbids trailers; Penumbra has no CLAUDE.md yet.

### 2.2 DEVLOG (`DEVLOG.md:1-80`)
- The log is append-only: never edit a past entry, and correct it in a later one.
- Session start: run `Get-Date` and check that `reference/` has its inputs.
- Session end: append an entry from the template, run `git status` to confirm nothing from `reference/` or `out/` is staged, and record that check.
- Template fields: `### YYYY-MM-DD HH:MM — Session N: <goal>`, then **Built / Broke / dead ends / Decision / Assets check / [CLIP] / [BROLL] / Numbers / Inferred mood** (the mood is explicitly marked as a guess).
- A session that died is written up the next day "from git, the planning doc, the workflow journals", and says so in italics (sessions 14c, 16, 17b).

### 2.3 Planning doc (`docs/planning/2026-09-11-magic-portals-remaster.md`)
- **One live record.** Every change is a `## Step N - <title> (built)` section. It says what was built, the gates (numbered thresholds written before the run), and suite check counts, historically on both GCC 13.3 and MSVC 14.50.
- Owner rulings are recorded as R1..R25, along with "What the owner settled".
- A step that fails its gates three times is committed with **RECORDED DEVIATIONS** instead of being tuned, and the deviation also goes into `docs/parity-backlog.md`.
- Open items go under the step's "LEFT FOR THE OWNER".
- Around it sit a spike doc (`2026-09-10-magic-portals-spike.md`, thresholds set before measuring) and a port plan (`2026-09-10-magic-portals-port.md`).

### 2.4 Data with provenance (`CLAUDE.md:189-200`)
- Tunables live in JSON, never in code. Each block carries `"_about"`, `"_source"` (citations such as `Game.angelscript bytes 306783..306841` or `ETHRenderEntity.cpp:113-117`), `"_note"`, `"_guess": true` or `"_ruling"`. Example: `game/data/lighting.json` `darkest_ambient`.
- `docs/original-gameplay.md` tags every claim DATA / DECODED / IDENTIFIER / ABSENCE / OWNER / GUESS. OWNER ranks above IDENTIFIER.
- Readers are **strict**: a missing field is an error, never a silent default (e.g. `Lighting.hpp:43-53`, `Sounds.hpp:79-83`, `Art.hpp` "required, never defaulted").
- Penumbra adds a new provenance source that MPR never had: **readable AngelScript source**. Cite it as `file.as:line`, not bytecode offsets.

---

## 3. Which C++ modules to lift, adapt, or leave

Legend: **LIFT** (near-verbatim), **ADAPT** (the logic is right but 2010/PC changes are needed), **PATTERN** (copy the shape, not the content), **SKIP**.

| MPR module | What it is | Verdict for Penumbra | Notes, with citations |
|---|---|---|---|
| `game/main.cpp` | flags, log sink, manifest, saveDir injection, validation gate | **LIFT** | §1.7 |
| `MagicPortalsLayer` (seam only) | `EngineLayer` with 60 Hz `SimulationClock.fixedDelta = kTick` (`.cpp:197-203`); `Game::AfterStep`/`BeforeStep` around the engine's physics step; `OnUpdate` for per-frame pictures; `SceneRendering()` (`.cpp:150-158`) | **PATTERN** | The 7,164-line body is Magic Portals-specific. Copy its skeleton: attach, load JSON rules strictly, build and sync drawables, emit the HUD per frame, latch sounds on the tick and play them on the frame. |
| `sim/Units.hpp` | 50 px/m, y flip, rotation flip | **ADAPT** | The 50 px/m comes from Ethanon's Box2D `DEFAULT_SCALE` (`Units.hpp:5-9`). Penumbra's schema has **no `shape` attribute at all** (0 occurrences in 13 `.esc` and 144 `.ent`): no Box2D bodies, only the legacy `collidable` + `<Collision>` AABB. So the rationale lapses, and the scale must be chosen for SE's solver or for kinematic movement. The y and rotation flips still apply. |
| `sim/Tscn.{hpp,cpp}` | strict reader for the Godot `.tscn` subset the Python converter writes | **Decision, not a default** | It exists for Godot-history reasons: "the remake's repository is an oracle ... its converter does not get a second emitter" (`Tscn.hpp:5-11`). Penumbra has no such history, so there are two options:<br>(a) parse `.esc`/`.ent`/`.par` directly in C++. SE vendors no XML library (third_party: Vulkan-Headers, VMA, entt, glfw, glm, imgui, imguizmo, stb {image, image_write}, tinygltf), and MPR's C++ readers are ad hoc string scanners (`Particles.cpp:50-91`: `Between`, `Attribute`).<br>(b) keep a Python converter into `out/` and read JSON with SE's `core/Json.hpp`.<br>Either way the strict, "refuse and name the line" discipline carries over. |
| `sim/Sprites.{hpp,cpp}` | a level's sprites; `ImageSize` reads PNG, BMP and JPEG headers without decoding | **ADAPT** | `ImageSize` is generic (PNG/BMP/baseline JPEG, `Sprites.hpp:106-112`). Penumbra also needs **DDS**: 9 uncompressed files, none compressed (§4.8). `Find`/`Drawn` are tied to `.tscn` and Tiers. |
| `sim/Tiers.{hpp,cpp}` | hd/fullhd tier choice | **SKIP** | Penumbra ships no `hd/`, `fullhd/`, `ld/` or `xld/` folders and no `app.enml` (`Penumbra.ethproj` is the 27-byte marker). One kept idea: a frame's units are `int(texels/D)/columns` (`Tiers.hpp:19-31`), i.e. integer strides. With D=1 this is `int(w/cols)`, and it still matters for uneven sheets (formats doc §9.1). |
| `sim/Lighting.{hpp,cpp}` | reads `eth_*` keys; the pure lighting arithmetic | **ADAPT heavily** | Worth keeping: `LightColour` (<Color> × lightIntensity × owner particle ratio), `HaloColour`, `OwnerScaled`, `ParticleRatio`, and the `ReceiverMask`/`LightLayer` mechanism. Four places that are wrong for Penumbra:<br>1. **`AmbientTerm` = `min(1, ambient+emissive)` per channel** (`Lighting.hpp:233-237`) is cited from 2013 `ETHRenderEntity.cpp:113-117`. The 2010 ambient pass computes colour on the CPU (`defaultStaticAmbientVS.cg`/`defaultVS.cg` just pass `color0`), inside the closed `GameSpace.dll`/`machine.exe`. **Unverified for 2010.**<br>2. **`ReceiverMask` skips static lights on static sprites "even where no file was baked"** (`Lighting.hpp:152-174`). Penumbra ships **no** `scenes/*-esc/add<id>` lightmap directories. Copied as-is, this is the **most dangerous rule**: every static sprite would lose its static lights. MPR's own `runtimeBake=true` path (static lights evaluated live on static sprites, `Lighting.hpp:169-174`) is the closer model if the 2010 engine bakes at load. Whether it does, and whether its bake uses normal maps, is unknown.<br>3. `Rules::framebufferRgb565` (`Lighting.hpp:140-141`) is Android-only (GL2JNIView/ConfigChooser). **Do not carry it.**<br>4. `normalMapGreenDown` (`Lighting.hpp:132-135`) matches the 2010 decode `-normalize(2*(n-0.5))` (`hPixelLight.cg`), but that decode also *renormalises*, which SE does not (§4.4). |
| `sim/Particles.{hpp,cpp}` | Ethanon's `ETHParticleManager` arithmetic, line by line: `Reset`, `Release`, `Step`, `Drawn`, `DrawColour`, `QuadPx`, `Finished`, `Scale`, `SlotFraction` | **ADAPT (the most reusable piece)** | Penumbra's `<ParticleSystem>` uses exactly the same 17 attributes (particles, allAtOnce, alphaMode, repeat, animationMode, boundingSphere, lifeTime, randomLifeTime, angleDir, randAngle, size, randomizeSize, growth, minSize, maxSize, angleStart, randAngleStart) and the same children, plus the legacy `<SoundEffect>` (18 in `.ent`). Must change:<br>(a) **`ReadUtf16` refuses any file without `FF FE`** (`Particles.cpp:22-48`). Every Penumbra `.ent`/`.esc`/`.par` is ASCII/Latin-1, so every one would be refused.<br>(b) `Drawable` refuses AM_MODULATE (`Particles.hpp:117-121`). Penumbra has **24** `alphaMode="4"` occurrences (89 "0", 104 "1", 24 "4", over scenes, `.ent` and `.par`).<br>(c) Whether the 2010 manager's arithmetic (frameSpeed, 250 ms cap, release schedule, draw order of the randoms) equals the 2013 one is unverified. The random stream is a `std::function<double(double,double)>` (`Particles.hpp:175`), so draw order is explicit. Keep that. |
| `sim/Effects` | script `AddEntity` call sites as JSON rows | **PATTERN** | Penumbra's 27 `AddEntity` calls are in readable `.as`, so the rows cite `file.as:line`. |
| `sim/Sounds` | AudioManager hooks as JSON (`files`, `both`, `volume`, `speed`/random/derived, `minIntervalMs` with a **shared timer** group); `events` map port events to hooks; latched on tick, played on frame | **PATTERN / near LIFT** | The shape fits Penumbra's `LoadSoundEffect` (35 calls), `PlaySample` (27), `SetSampleVolume` (5) and `LoadMusic` (4). The playback code (`MagicPortalsLayer.cpp:6245-6293`) is generic. The file format is the blocker (§4.7). |
| `sim/Art` | the `.ent` facts of script-added entities as JSON with required fields | **PATTERN** | Penumbra can read its `.ent` files directly: all 144 are in `entities/`, and none is referenced via `<FileName>`. |
| `sim/Hud`, `sim/UiLayer`, `Pause`, `Popup`, `LevelEnd`, `MainMenu`, `Loading`, `Credits`, `Dashboard`, `Selector`, `Locking` | pure arithmetic of ETHFramework `UISprite`/`UIButton` screens, pinned by suites | **PATTERN only** | Penumbra's UI is its own 2010 script (`interface.as`, `menu.as`, `videoModes.as`, `switch.as`, `messageManager.as`), not ETHFramework. Copy the split: pure functions of view size and age in `sim/`, and the layer only places `ScreenOverlay` quads. `Loading.hpp` (the MP LoadingScreen as data) is MP-specific. |
| `sim/Camera` | follow with lag and hold, clamped to level bounds | **SKIP / PATTERN** | Its numbers are `_guess`. Penumbra has `cameraManager.as` (159 lines) in source. `Camera::Clamp` is generic. |
| `sim/Scores` | medals with an injected `saveDir`; writes `scores.json`, not `scores.enml` | **PATTERN** | `Scores.hpp:12-16` deliberately avoided a C++ ENML parser. Penumbra needs one anyway, for `data.enml` (gameplay: `global`, `warrior`, `minion`, `knight`, `master_knight`, `impy`, `paladin`, `king` blocks) and `hs.enml`. The converter's `enml.py` is a faithful port of `File::ParseString` (`Enml.cpp:313-448`) and is the reference to port to C++. It reads UTF-8 (`enml.py:183`), which is **wrong** for `data.enml` (25 Latin-1 bytes in multi-line Portuguese story strings). The engine reads it with `GetAnsiFileString` (formats doc §8.2, lines 1698-1702: raw bytes, CR stripped, latin-1). |
| `game/LevelVisit` | DEV `--visit-levels`: walks levels in one process and fails unless material descriptor sets return to the pool | **LIFT (adapt names)** | Generic leak check for per-level `TextureRegistry::Invalidate` (`LevelVisit.hpp:1-30`). It found a real SE bug (`TextureRegistry.hpp` SetPool comment: 20 validation errors). |
| `Game`, `LevelBuilder`, `Roles`, `Portals`, `Player`, bosses, `Minions`, … | Magic Portals gameplay | **SKIP** | The behaviour layer is game-specific. MPR's method finding does carry over: an entity has behaviour iff an `ETHCallback_<name>` exists (DEVLOG session 8, `DEVLOG.md:1100-1106`). Penumbra's `.as` defines 36 of them, e.g. `ETHCallback_knight`, `_king`, `_minion`, `_potion`, `_fire_ball`, `_falling_bridge`. |
| `tools/asbc` | AngelScript bytecode reader | **SKIP** | Penumbra ships `.as` source (25 files, LGPL-3 header). |
| `tools/converter` (Python) | `enml.py`, `entity.py`, `scene.py`, `particles.py`, `tscn.py`, `textio.py` | **ADAPT or reimplement** | `entity.py`/`scene.py` implement the 2013 gated-attribute reads (formats doc §5.1). Penumbra's files are the **older writer's**, with every attribute written unconditionally (`castShadow`, `shadowScale`, `shadowLengthScale`, `shadowOpacity`, `specularPower`, `specularBrightness`, `soundVolume`, `layerDepth`, `startFrame`, `collidable`). The doc's §4.8 and §11.7 say this matches the testbed's `customDataTesting.esc` legacy form. `textio.read_ethanon_xml` falls back to `utf-8-sig` (`textio.py:41`) and **raises on `level1.esc` (3 bytes) and `level3.esc` (5 bytes)**, whose Latin-1 accents sit in CustomData `message` strings (e.g. "s\xe3o", "espa\xe7o", "\xc9 uma boa hora"). |

---

## 4. How MPR renders, lights, draws particles and text, and plays audio on SE

### 4.1 Scene encoding (`MagicPortalsLayer.cpp:125-158, 197-214`)
- `RenderSettings.encoding = SceneEncoding::DisplayEncoded` (SE `RenderSettings.hpp:117`), with a black `Background::Color` and `bloomIntensity = 0`. Textures are sampled as file bytes and blends work on encoded values, with no tone map or bloom.
- The reason is measured: blending in linear light scored 13.59/255 against 0.28 for encoded values over 1,359 unlit blocks (`.cpp:125-137`).
- `OutputQuantize::Rgb565` (`RenderSettings.hpp:124`) is switched on from `lighting.json` (`.cpp:207-214`). This is Android-only; see §3.
- **For Penumbra:** D3D9 on a 2010 game blends on non-sRGB values, so `DisplayEncoded` very likely applies. That is an inference; verify it against a capture. The RGB565 quantize should not be used unless a capture of the PC original shows banding.

### 4.2 Sprites are engine quads, not a 2D renderer (`MagicPortalsLayer.cpp:3096-3121`)
- `makeSprite` creates one entity per picture: `TagComponent`, `TransformComponent`, `MeshComponent{primitiveType="Quad"}`, and `MaterialComponent{unlit=true, transparent=true, blend=Alpha|Additive, albedoTexturePath=file}`, plus `RenderableComponent{castsShadow=false}`.
- `placeSprite` sets the position as `Units::ToWorld(px)` with z = draw slot, the scale as the size in metres, and the rotation.
- `SpriteAnimationComponent{columns, rows, firstFrame, frameCount, framesPerSecond, playing}` cuts sheets evenly. MPR notes this is 4.5 texels off on uneven hd sheets (`Tiers.hpp:33-40`).
- `InterpolatedTransformComponent` is added to anything that moves on the tick.
- SE blend modes are **only** `Alpha`, `Additive` and `Premultiplied` (`Components.hpp:804`). There is no multiply or modulate.
- `alphaCutoff` (`Components.hpp:825`) exists and could implement AM_ALPHA_TEST.
  - MPR never needed it. Its converter maps only blendMode 1 (and 4) (`tools/converter/src/ethanon2godot/tscn.py:68-71, 522-527`), so Magic Portals' 74 chapter-1 AM_ALPHA_TEST instances are drawn as plain alpha.
  - **In Penumbra, AM_ALPHA_TEST (`blendMode="2"`) is 2,077 of the 2,607 scene instances.** The rest are `blendMode="0"`, with 0 additive entities.
  - The D3D9 alpha reference value is unknown.
- Engine bug already fixed by MPR (remaster step 18, `docs/planning/...remaster.md:1742-1818`): `ModelLoader::GenerateQuad` never called `computeBounds()`, so every quad was frustum-culled as a point. This is fixed in SE `4bfcf67` and needs no action; it shows why an in-level "things vanish" symptom should first be checked in `RenderSystem`'s gather loop.

### 4.3 Lighting: engine `Light2DComponent`, not a game shader
- SE's 2D light path was **added to the engine for Magic Portals**: DEVLOG session 14c, "E2 the engine's overlay map, 2D sprite record and premultiplied blend (0656311, 6d55f68)" and "E3, the engine's 2D point light with a height and the shader loop that adds it (1aa34b9, e1c64f3)". MPR itself writes no shader.
- **Per sprite:** `MagicPortalsLayer::tint` (`.cpp:4648-4690`) fills `MaterialComponent::Sprite2DLight` (`Components.hpp:709-735`):
  - `enabled`;
  - `ambient = AmbientTerm(ambient, emissive)`;
  - `height = Units::ToMetres(receiver.z)`, the original's depth rather than the draw slot;
  - `normalYDown = lighting.normalMapGreenDown`;
  - `lightMask = ReceiverMask(isStatic, applyLight, runtimeBake)`;
  - `overlayStrength`.
  - It also sets `normalTexturePath` when the sprite applies light, `overlayTexturePath = lightmap` (the baked `add<id>.png`, added after the ambient multiply), and switches the blend from `Alpha` to `Premultiplied` when lit.
- **Per light:** `buildLights`/`placeLight`/`syncLights` (`.cpp:4724-4935`) create one entity with `Light2DComponent`:
  - `color = LightColour(...)`, which folds lightIntensity × particle ratio;
  - `intensity = 1`;
  - `range` in metres;
  - `height = ToMetres(ownerZ + offset.z)`;
  - `layers = LightLayer(ownerStatic)`, using bits `kLiveLights = 1<<0` and `kStaticLights = 1<<1` (`Lighting.hpp:161-162`);
  - `enabled = owner drawn`.
- **Halos** are separate additive quads, `makeSprite(..., additive=true)`, a quarter slot in front of the owner. Their colour is `HaloColour` (no ambient, no intensity).
- **The engine's formula** (`Light2D.hpp:92-100`, mirrored in `shader.frag`): `clamp(tint * light.color * (1 - d²/r²) * dot(L - P, N)/d, 0, 1)`. The normal is decoded to -1..1 and **NOT renormalised** (`Light2D.hpp:84-90`), and the cap is `kMaxLights2D = 64` per frame (`:19`).
- **Lightmaps** are acquired by the sprite's material and released per level with `TextureRegistry::Invalidate` (`releaseLightmaps`, `.cpp:638-649`).
- **Dev switch:** `--light-masks-off` sets `ForceLightMasksOff`.

### 4.4 The 2010 Cg shaders compared with SE's Light2D (from PEN `data/*.cg`)

| Aspect | 2010 `hPixelLight.cg` `main` (horizontal) | SE Light2D | Consequence |
|---|---|---|---|
| Normal decode | `-normalize(2*(n-0.5))`, **renormalised** | decoded, **not** renormalised (the 2013 GLES look) | engine gap: a flag to renormalise |
| Diffuse | `dot(normalize(P-L), n_neg)` = `dot(normalize(L-P), n)` | `dot(L-P, N)/d` | same |
| Attenuation | `1 - d²/max(d², r²)` | `1 - d²/r²`, zero beyond range | same |
| Output | `diffuse × color0 × dl × att × lightColor × lightIntensity × diffuse.a` (additive pass; the UNORM target clamps) | clamped to 0..1, × tint | the alpha multiply: open question |
| Specular | `mainSpecular`: gloss map × `pow(saturate(dot(n, halfVec)), specularPower)` × specularBrightness, with a `fakeEyePos` | **none** | engine gap. Penumbra scenes name `<Gloss>` 241 times (12 in `.ent`); `specularPower` is 50/60 on the sampled walls |
| Vertical entities | `vPixelLight.cg`: normal swizzled `.xzy`, `z *= -1`, no `× alpha`; `pixelLightVS.cg` `verticalSprite_ppl` builds a vertical 3D position | one constant `height` per sprite | engine gap (30 `type="2"` instances in scenes) |
| Depth | every VS writes `z = 1 - depth`; vertical sprites write **per-vertex** `(1-depth) - ((1-y)*rectSize.y)/spaceLength` (`pixelLightVS.cg`, `defaultVS.cg` `vertical`) | MPR orders by a slot table | engine or port gap (§4.6) |
| Shadows | `dynaShadowVS.cg` plus `data/shadow.dds`; 82 of 2,607 instances have `castShadow="1"`, and lights carry `castShadows` | none | engine gap |
| Fallback path | `hVertexLightShader.cg` `sprite_pvl`: per-vertex light, fixed normal `(0,0,-1)`, used when pixel shaders are off | n/a | `UsePixelShaders(true)` when supported (`main.as:103-104`); a video-options toggle (`videoModes.as:43, 125-126`; `setupScene.as:117-118`) can switch to per-vertex |

### 4.5 Particles (`MagicPortalsLayer.cpp:709-1030`, `sim/Particles`)
- A CPU simulation runs in `sim/Particles`, per frame in `OnUpdate`, never on the tick or in the state hash.
- **One engine quad entity per live particle**, drawn with `makeSprite(..., additive)`. Quads are pooled: `RenderableComponent.isVisible` is toggled rather than creating and destroying entities (`.cpp:955, 986`).
- `DrawColour`: an alpha-blended particle is multiplied by `min(1, luminance + ambient)`. An additive particle's alpha is forced to 1, because `GL_ONE, GL_ONE` never reads alpha and SE's additive pipeline weights by alpha (`Particles.hpp:235-240`).
- The quad is always square at `size` (`QuadPx`). Systems draw at their owner's slot plus `SlotFraction`, or at `Slot::LiftedSystems` when `StartPoint.z != 0` (step 92).
- The random stream is seeded per level from the level name (DEVLOG 17b item 9).
- Effects use their own `m_effectRandom`, separate from the placement stream (DEVLOG 18b), mirroring the original's per-frame `MTRand`.
- A light's brightness is scaled by its owner's live-particle share (`ParticleRatio`, from `ETHSpriteEntity::ComputeLightIntensity`). Check that the 2010 engine does the same.

### 4.6 Draw order
- MPR orders pictures by the converter's `z_index` rank, then file order. Script-added things sit at fractions between slots from one table: `enum class Slot` and `kSlotTable` (`MagicPortalsLayer.hpp:355-470`, e.g. `Picture 0`, `Halo +0.25`, `Systems +0.25 spread`, `Effect -0.5`).
- MPR says outright that this approximates Ethanon's `ComputeDepth` (formats doc §10.3). It was adequate because 830 of 840 particle carriers and nearly all sprites in Magic Portals were `type 0`.
- **It does not transfer to Penumbra.** In 2010, depth is written to a hardware z-buffer (`z = 1 - depth`). `ET_VERTICAL` sprites (30 instances) write a *sloped* per-vertex depth, and `ET_LAYERABLE` (`type="5"`, 212 instances) uses `layerDepth`. Two scenes, `arena_select.esc` and `menu.esc`, use `ZAxisDirection (0,-1)`: z moves sprites up the screen. The other 11 use `(0,0)`.
- MPR built only the `(0,0)` path, because "all 133 of Magic Portals' `.esc` files set (0,0)" (formats doc row 2, line 71; §7.4a, line 1586).

### 4.7 Text
- **MPR:** SE's `BitmapFont` reads the BMFont text `.fnt` format, using Magic Portals' own Matura fonts (`BitmapFont.hpp:1-26`; built in remaster step 19). Glyph quads go through `ScreenOverlay` (`uiFont`, `.cpp:5395`; `EmitMenu`, `.cpp:2739`).
- `ScreenOverlay` quads are display-referred, drawn after the tone map and in order, capped at `kMaxQuads = 4096` (`ScreenOverlay.hpp:104`). They take fractions of the image, a UV sub-rectangle, a colour and a `basis` rotation. HUD text drawn through the scene target would be tone-mapped: white reaches only 186/255, and the bloom pass haloes it (`ScreenOverlay.hpp:11-37`).
- **Penumbra:** text is `DrawText(pos, text, "<Windows font name>", size, ARGB)` through D3DX with **system TrueType fonts**: "Arial Narrow", "Arial Black" and "Verdana" (e.g. `interface.as:65-91`, `util.as:450-459`, `menu.as:101, 228-229`).
  - Sizes run from 15 to 60, and one call uses **256** (`setupScene.as:373`).
  - Shadowed text is drawn twice (`util.as:450-455`).
  - SE has no TTF rasteriser and no `stb_truetype` in third_party.
  - Strings are Latin-1 Portuguese.

### 4.8 Textures
- SE's `TextureRegistry::Acquire` loads with `stbi_load` (`TextureRegistry.cpp:249`): PNG, JPEG, BMP and TGA, but **no DDS**.
- Penumbra has 9 `.dds` files, all **uncompressed**, with no DXT:
  - 8 are 32-bit RGB+alpha (`dwFlags 0x41`): `entities/dirt.dds` 128², `thorn.dds` 256×64, `tree01_alpha` 206×246, `tree02_alpha` 156×116, `tree03_alpha` 70×292, `tree05_alpha` 145×159, `particles/black_sword.dds` 64², `particles/fog.dds` 256².
  - `data/shadow.dds` is 32×32 16-bit luminance+alpha (`0x20001`).
  - They are referenced 56 times across the scenes, `.ent` and `.par` files, e.g. `fog.dds` 16 times and `dirt.dds` 11.
- A port-side decoder plus `TextureRegistry::UploadRGBA(key, ...)` (`TextureRegistry.hpp:62`) would work. For `albedoTexturePath` to resolve, the key must equal Acquire's internal `"srgb:"`/`"data:" + path` key (`TextureRegistry.cpp:240`), which is an internal detail. Converting the files to PNG in the gitignored `out/` is the cheaper alternative.
- No Penumbra BMP or JPEG contains any magenta (`0xFF00FF`) pixel, so Ethanon's magenta colour key has nothing to cut. Halos (`halo.bmp`, `halo1.bmp`, `halo2.bmp`) are black-keyed, which additive drawing handles, as in MPR.

### 4.9 Audio (the engine plays WAV and MP3 only)
- `AudioClip::Load` dispatches `.wav` to `LoadWav` and `.mp3` to `LoadMp3`. Anything else is refused by name: "is not a sound this engine reads (.wav or .mp3)" (SE `AudioClip.cpp:84-91`).
- `LoadMp3` goes through **Windows Media Foundation** and is Windows-only (`AudioClip.hpp:47-60`). SE added it for Magic Portals in remaster step 16 (`...remaster.md:1927-1979`). The owner "chose the decoder rather than converting the files", keeping the original's mp3s in place with no derived copies. The plan also records that vendoring `dr_mp3`/`minimp3` "was not available: neither is on this machine".
- Playback: `AudioEngine::Play(path, loop, volume, pitch)` (`AudioEngine.hpp:71`); pitch is the sample speed. The engine's `AudioSystem` reaps finished voices (`AudioSystem.cpp:175`).
- MPR plays one-shots from `playLatched` (`MagicPortalsLayer.cpp:6245-6293`) and music through `updateMusic`/`stopMusic` (`:6295-6340`). With no audio device, latched events are **dropped**, not queued.
- **Penumbra:** `soundfx/` has **19 `.ogg` and 16 `.mp3`**. All three music tracks are mp3 (`menu.mp3`, `chefao.mp3`, `fase.mp3`). Many sound effects are ogg: `fail.ogg`, `cast_fire_spell.ogg`, `fall.ogg`, `pvp_win.ogg`, `death_king.ogg`, `explosion.ogg`, `hit01.ogg`, `potion_pick.ogg`, `minion.ogg`, `sword_combo.ogg`, `vanish.ogg`, `jump01.ogg`, `jump02.ogg`, `blast_attack.ogg`, and others.
  - Loads are in `setupScene.as:110-161`, `menu.as:45-58` and `gameover.as:47-49`.
  - `data.enml` also names `chaseSfx = minion.ogg`.
- An engine-free route exists: decode OGG in the port with a vendored decoder (e.g. `stb_vorbis`, which must be fetched) and register the PCM with `AudioEngine::AddClip(name, clip)` (`AudioEngine.hpp:65`). "A game may generate its sound" was added for Wolf Brigade. `Play` then resolves the name from the cache, and SE is not touched.

---

## 5. Pitfalls recorded in MPR's DEVLOG and planning doc

### 5.1 From the Supersonic era (sessions 14-19, `DEVLOG.md:1422-2394`)
1. **Smart App Control refuses freshly linked exes, and every refusal pops a Windows notification.** Counts: at least 101 refused launches in session 16 (`:1690-1694`), 71 in 17b, 25 in 18b and 10 in 18c. Session 15b says a relink-and-retry ctest loop "fired dozens (37 in one round)" (`:1597-1600`).
   - Rule (`CLAUDE.md:146-152`): relink only what you need, launch each suite at most once, never loop ctest or relinks, and report a refused exe as **not run**.
   - `ctest --test-dir build -N` lists tests without launching anything.
2. **Standby and unclean shutdown.** The laptop slept through a ctest (29,660 s, of which 29,618 s was `test_mp_zerog`; `:2342-2344`).
   - Five wakes killed running agents (`:1880-1884`).
   - An unclean shutdown NUL-padded files: a 1,084,417-byte PNG of NULs and a capture log with 98 NULs (`:1965-1974`).
   - Lesson: re-verify artefacts by md5 or a NUL scan before reusing them. A half-done merge must be continued, never `--abort`ed over uncommitted work (`:2345-2348`).
3. **Every quad culled as a point.** This was an engine bug (remaster step 18). `test_mp_layer`'s cull check agreed with the bug because it never ran `SyncResources` and so read the component defaults. "A test that shares the code under test's own wrong assumption agrees with it."
4. **`ValidationLayersActive()` reported intent, not reality.** It is fixed in SE, but a silent log still proves nothing unless it says ACTIVE (`main.cpp:364-377`).
5. **A pointer into a vector returned by value.** `DialOf` returned a pointer into `TimerReports()`'s temporary: six use-after-free reads, so a "966/0" pass was undefined behaviour (`:1667-1670`).
6. **Two random draws inside one `glm::dvec2{rand(), rand()}`.** C++ leaves the order unspecified, and MSVC drew y first. Putting them in statement order moved 1-01's f420 md5 (`:1663-1664`). Draw randoms in explicit statements, x then y.
7. **The script clock floors each frame's milliseconds.** Ethanon's `Update(float)` truncates, so at 60 Hz the script advances 16 ms a frame and the port runs 4.17% fast (`:1878-1880`; left unemulated). Penumbra's `timer.as` and every stride-based animation will hit this.
8. **Relative paths resolve inside `engine/`.** The game changes its working directory there, so a relative `--screenshot` lands in the submodule, whose `.gitignore` does not cover it (README "Capture flags"; `CLAUDE.md:250-255`).
9. **CRLF baselines.** An md5 list with `\r` made every frame read as different until it was stripped (`:2161-2162`).
10. **Parallel agents cross wires.** Another role's `ctest -N` output, `ctestN.txt`, was written into MPR's root (`:2163-2164, 2244-2245`). A workflow chose the wrong build script because `tree.includes('worktree')` matched the main tree's own description (`:1678-1680`).
    - With another Claude session editing SE right now, use exact paths. Never write into the other repositories. Build only your own `build/`.
    - MPR's two-track work used separate worktrees, planning step numbers offset by 80, and one merge agent (`:1720-1724`). Session 18c shipped engine changes on a branch (`games-out`) and fast-forwarded SE `main` only after `git ls-remote` confirmed it (`:2206-2213`).
11. **Records were wrong more often than builds.** Fireball step 7.2 failed two checks "on the RECORD, not the build" (`:2325-2331`), and 00_order baselines named a superseded capture (`:1684-1687`). Reproduce every number before writing it into a step.
12. **The first lighting-rules path pointed at the wrong data directory.** The quantise never applied until `test_mp_layer` caught it (`:1601`). `requires` as a member name is a C++20 keyword (`:1606`).
13. **Git and Windows.** A submodule does not inherit `core.longpaths`, so a deep clone needs `git -c core.longpaths=true submodule update --init` (`:2158-2160`, README step 5). A CMake object path over 250 characters needed `subst`.
14. **`port_build.bat` must stay CRLF on checkout.** A tool rewrote the working copy as LF (`:2076-2079`).

### 5.2 General method lessons, stated once (Godot era, sessions 1-13)
- **Mutation-test any assertion you intend to quote.** Four "proof" tests passed vacuously: a rotation test truncated with `[:12]`, `free()` vs `queue_free()`, a fall-through test that passed by timing out, and a child-count check (`:679-712, 794-836, 926-966`).
- **Treat "0 of N" as a possibly broken selector.** `<Polygon>` and `<Joints>` nest **inside `<Collision>`**, not under `<Entity>`; "every ENML payload lives under the collision root" (`:539-560, 1247-1255`). The same session had `.ent`-append and branch-B errors of the same shape (`:469-535`).
- **Encoding is silent when wrong.** MPR's parser stubs originally hard-coded UTF-8 (`:1-9` of the persisted excerpt, session 1). Here the inverse applies: Penumbra is Latin-1, so both the MPR Python fallback and the C++ `ReadUtf16` fail.
- `<CustomData>` is child elements (`<Variable><Type/><Name/><Value/>`), not attributes. Parsing it as attributes gave empty dicts for every entity, "the single most costly bug" (`:852-861`).
- **Never write lookup tables from memory**; generate them from the source (`:1351-1355`). **Don't edit source through shell heredocs** containing escapes (`:1072-1075`).
- **Existence proofs, not ranges.** Every frequency in the formats doc was measured on the wrong corpus (the 11-scene testbed) until the audit (`formats doc :50-79`).
- **A confident sentence is the dangerous one.** "AM_ADD never appears" turned out to be 111 occurrences.
- **PowerShell 5.1 eats `$LASTEXITCODE`** with `*>$null`; use `Start-Process -Wait -PassThru` (`:10-14` of the session 1 excerpt). Run captures from Git Bash; PowerShell mangles the arguments (`CLAUDE.md:246-255`).

### 5.3 Rules from `docs/ethanon-formats.md` that do apply to a 2010 PC game
Hedged: the doc cites 2013 source, and 2010 must be re-checked.
- Direct-child element lookups only (§3.5): `<SpriteCut>`, `<Position>`, `<Color>` and `<Entity>` recur at several depths.
- Numbers are parsed as `int(float(s))` (§3.2), booleans as ints (§3.3), and a missing attribute takes the Reset() default (§3.4).
- `.esc` file order is bucket-hash order, not draw order (§4.6). `<EntityName>` is a label, not a file reference (§4.5); Penumbra has 0 `<FileName>`, so every placement is inline.
- **`flipY` defaults to `flipX`** (§4.3, `ETHEntity.cpp:217`). Penumbra has 0 flip attributes, so it is moot.
- `ET_VERTICAL` never rotates and anchors centre-bottom (§5.3, §9.3). Pivot: `originPx = typeAnchor + pivotAdjust`, with +y down; a negative `PivotAdjust.y` moves art down (§9.3).
- Integer-stride sheet cut, row-major, clamped (§9.1).
- `ToScreenPos = (x,y) + zAxisDirection·z` (§9.4) is **live** in Penumbra's `menu.esc` and `arena_select.esc`.
- Particles (§7):
  - `particles` is a fixed pool, released over `lifeTime + randomLifeTime`.
  - Random widths are full peak-to-peak, except `randAngleStart`, which draws U(0, max).
  - Velocity ×60, gravity ×3600, times in ms, motion capped at 250 ms.
  - `Luminance` is live under AM_PIXEL; a missing bitmap skips the system; at most 2 systems per entity (§7.2 cites 2013 `ETH_MAX_PARTICLE_SYS_PER_ENTITY`).
  - `.par` files in `effects/` are runtime assets loaded by script (`PlayParticleEffect`).
- Asset paths are basename-flattened into fixed subdirectories (§8.6). Audio is the exception: Penumbra's scripts pass `"soundfx/<file>"`.
- The ENML grammar (§6.1): one `/` starts a comment, only `\;` and `\\` are escapes, trailing whitespace before `;` is kept, and any error discards the whole file. Read ENML as latin-1 with CR stripped (§8.2).
- Animation lives only in script (§10.8). In Penumbra it is readable `.as`, e.g. `controlCharacters.as` (732 lines).

---

## 6. Penumbra facts measured during this survey (used above)
- 13 scenes: `arena_select`, `gameover`, `level1-3`, `menu`, `pvp_lv1-6`, `videoModes`. They hold 2,607 outer instances (level1 630, level2 429, level3 866), and all are inline.
- 144 `.ent` and 43 `.par`. All `.esc`/`.ent`/`.par` files are ASCII with CRLF line endings and no BOM, except the Latin-1 bytes in `level1.esc` and `level3.esc` CustomData.
- Every scene has `lightIntensity="2"`. Ambients range from (0,0,0) to (0.4,0.2,0.4).
- Entity types: 2,365 type 0, 30 type 2, 212 type 5.
- 82 `castShadow="1"`, 1,736 `<Normal>`, 241 `<Gloss>` and 97 `<HaloBitmap>` in scenes.
- 124 `<ParticleSystem>` in scenes and 50 in `.ent`.
- No `shape` attribute anywhere, no tier folders, and no lightmap folders. `Penumbra.ethproj` is the marker file and there is no `app.enml`.
- `data.enml` holds the gameplay tables and Portuguese story strings; `hs.enml` holds high scores.
- Scripts: 25 `.as` files, 36 `ETHCallback_*` functions and 27 `AddEntity` calls.
- The original's DLLs are present: `GameSpace.dll`, `audiere.dll` (the 2010 audio library), `cg.dll`, `cgD3D9.dll` and `d3dx9_42.dll`, plus `machine.exe`.

## Key facts

- MPR's decode is of the Dec-2013 Ethanon source (reference/ethanon c140bd3), measured on an Android GLES2 build: UTF-16LE .ent/.esc files, app.enml hd/fullhd tiers, Box2D, an RGB565 framebuffer. Penumbra is a 2010 D3D9/NVIDIA-Cg PC build with an older XML writer: ASCII/Latin-1 files, no tiers, no Box2D `shape` attribute, and `castShadow`/`specular*`/`soundVolume`/`startFrame` written unconditionally.
- Build pattern to copy: top-level CMakeLists.txt with a submodule guard, then add_subdirectory(engine), game/ and tests/ (MPR CMakeLists.txt:29-47). A static sim library holds all sim/*.cpp; a separate static game-layer library lets tests attach the layer to a bare registry (game/CMakeLists.txt:68-73). Every target builds at /W4.
- Data paths are CMake CACHE PATH variables baked into compile definitions on the sim library (game/sim/CMakeLists.txt:172-223, e.g. MAGICPORTALS_LEVELS_DIR, _ORIGINAL_DIR, _PORT_DATA_DIR). They are absolute because ctest runs from build/. main.cpp can override them with --levels/--art/--data/--saves.
- Tests: SE's supersonic_add_test (cmake/SupersonicTesting.cmake:26-45) is wrapped in add_mp_test(name [SKIPS_WITH_77]) with SKIP_RETURN_CODE 77 (tests/CMakeLists.txt:26-36). The TestHarness has CHECK/CHECK_EQ/CHECK_NEAR and summary(suite, minChecks), which fails a suite that skipped work. MPR has 44 suites, 27 of them marked SKIPS_WITH_77.
- tools/parity/port_build.bat (CRLF, 18 lines) calls C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat. It configures only once: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSUPERSONIC_ENABLE_VALIDATION=ON -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DGLSL_COMPILER=C:/VulkanSDK/1.4.357.0/Bin/glslc.exe`. The Penumbra repo already holds a copy as tools/build.bat (untracked, written by another agent).
- Commit convention (MPR CLAUDE.md:264-270): author IvanCvetanovic <icvetanovic99@gmail.com>, no AI trailers, title '<Game>: ...', last line 'Record: docs/planning/<doc>.md, step N', and a forbidden-extension grep over staged files before every commit. Engine gaps are fixed in SE first; the submodule pin moves in a separate commit.
- The DEVLOG is append-only, with fields Built / Broke / dead ends / Decision / Assets check / [CLIP] / [BROLL] / Numbers / Inferred mood. The planning doc is one live step record: '## Step N - title (built)' with gates, check counts, owner rulings R#, and RECORDED DEVIATIONS after three failed verify rounds. JSON data carries _about/_source/_guess/_ruling.
- MPR renders Ethanon lighting through the ENGINE: Light2DComponent (color, intensity, range, height, layers, enabled) plus MaterialComponent::Sprite2DLight (ambient, height, lightMask, normalYDown, overlayStrength), with overlayTexturePath for lightmaps and Premultiplied blend when lit. MPR itself has no game shader; SE's 2D light path (commits 0656311, 6d55f68, 1aa34b9, e1c64f3) was added for Magic Portals.
- SE Light2D formula: clamp(tint*color*(1-d²/r²)*dot(L-P,N)/d,0,1), normal NOT renormalised, 64 lights max (Light2D.hpp:19, 84-100). Penumbra's 2010 hPixelLight.cg differs: it renormalises the normal, multiplies by diffuse alpha, and has a specular variant with a gloss map. There is a vertical variant (xzy swizzle), dynamic shadows (dynaShadowVS.cg + shadow.dds), and a per-vertex fallback.
- MPR scene settings: RenderSettings::SceneEncoding::DisplayEncoded, black background, bloom 0 (MagicPortalsLayer.cpp:150-158), justified by a fit (13.59 linear vs 0.28 encoded). OutputQuantize::Rgb565 is Android-specific and must not be carried to Penumbra.
- Particles: sim/Particles is a line-by-line port of 2013 ETHParticleManager, drawn as one pooled engine quad entity per particle, ordered by a slot table (MagicPortalsLayer.hpp:410-470). Its reader ReadUtf16 (Particles.cpp:22-48) requires an FF FE BOM and so refuses every Penumbra file. Drawable refuses AM_MODULATE, and Penumbra has 24 alphaMode=4 occurrences.
- Text in MPR: SE BitmapFont (BMFont text .fnt only) through ScreenOverlay (display-referred, ordered, 4096-quad cap). Penumbra uses D3DX DrawText with Windows TrueType fonts 'Arial Narrow', 'Arial Black' and 'Verdana' at sizes 15-60 and up to 256. SE has no TTF rasteriser.
- Audio: SE AudioClip::Load accepts only .wav and .mp3 (AudioClip.cpp:84-91); MP3 goes through Windows Media Foundation, added for MPR in remaster step 16 so files are decoded in place and no converted copies exist. Penumbra soundfx has 19 .ogg and 16 .mp3. OGG can be decoded port-side and registered with AudioEngine::AddClip(name, clip) (AudioEngine.hpp:65) without touching SE.
- Textures: SE loads through stbi_load (TextureRegistry.cpp:249), so there is no DDS support. Penumbra's 9 .dds files are all uncompressed: 8 are 32-bit ARGB (flags 0x41) and data/shadow.dds is 16-bit L8A8. UploadRGBA (TextureRegistry.hpp:62) exists, but a material path resolves only if the key matches Acquire's internal 'srgb:'/'data:'+path key.
- Penumbra blends: 2,077 of 2,607 scene instances are blendMode=2 (AM_ALPHA_TEST), 530 are 0, and 0 are additive. MPR's converter never mapped AM_ALPHA_TEST (tscn.py:68-71). SE has alphaCutoff (Components.hpp:825) but only Alpha/Additive/Premultiplied blends (Components.hpp:804).
- Draw order: MPR's z_index slot table approximates depth and relied on ZAxisDirection (0,0) in all 133 Magic Portals scenes. Penumbra has 30 ET_VERTICAL instances whose 2010 shaders write per-vertex sloped depth, 212 ET_LAYERABLE instances, and ZAxisDirection (0,-1) in menu.esc and arena_select.esc.
- Lighting.hpp's ReceiverMask skips static lights on static sprites even without a baked lightmap. Penumbra ships no scenes/*-esc lightmap directories, so copying this rule unlit would leave every static sprite without its static lights. MPR's runtimeBake path, which evaluates static lights live, is the closer model.
- Encoding pitfall: MPR's converter reads non-BOM XML as utf-8-sig (textio.py:41) and ENML as UTF-8 (enml.py:183). Penumbra's level1.esc (3 bytes) and level3.esc (5 bytes) have Latin-1 accents in CustomData 'message' strings, and data.enml has 25 Latin-1 bytes. The correct model is raw bytes decoded as latin-1 with CR stripped (formats doc §8.2).
- Reusable as-is or near: game/main.cpp flag and log pattern, LevelVisit's descriptor-pool leak check, sim/Sounds' hook/event/shared-timer JSON pattern, Lighting's LightColour/HaloColour/OwnerScaled/ParticleRatio, and Particles' arithmetic once the reader is fixed. Skip for Penumbra: Tiers, Tscn (unless keeping the .tscn intermediate), asbc (the .as source is present), Camera numbers, and ETHFramework UI modules.
- Supersonic-era DEVLOG pitfalls: Smart App Control notification floods (at least 101 refused launches in one session; run each suite once and never loop ctest); standby and unclean shutdowns leaving NUL-padded files; the engine quad culled as a point, with a test blind to it; a pointer into a by-value vector; unspecified order of two rand() calls in one glm::dvec2; a script clock that floors to 16 ms per frame (the port ran 4.17% fast); a relative --screenshot landing in engine/; CRLF md5 baselines; parallel agents writing into the wrong repo or matching the wrong build script.
- State on 2026-09-27 at about 18:10: SE main HEAD 4bfcf67 equals MPR's pin, working tree clean. The Penumbra repo has .gitmodules and engine staged, and .gitattributes, .gitignore and tools/build.bat untracked, from another agent. extracted/ and penumbra_setup.exe are already committed and pushed, unlike MPR's hard rule against committing originals.

## Engine gaps

- No OGG decoding: AudioClip::Load refuses anything but .wav and .mp3 by extension (SE AudioClip.cpp:84-91), and third_party has no stb_vorbis. Penumbra has 19 .ogg in soundfx/. Workaround without touching SE: decode port-side and register via AudioEngine::AddClip(name, clip) (AudioEngine.hpp:65).
- MP3 decoding is Windows-only through Media Foundation (AudioClip.hpp:47-60). That is fine on Windows, silent on Linux.
- No TrueType text: SE's BitmapFont reads only BMFont text .fnt files, and there is no stb_truetype. Penumbra draws with system fonts ('Arial Narrow', 'Arial Black', 'Verdana') at sizes 15-60 and 256.
- No DDS loading: TextureRegistry::Acquire uses stbi_load (TextureRegistry.cpp:249). Penumbra's 9 DDS files are uncompressed (8 ARGB32, shadow.dds L8A8). A port-side decode plus UploadRGBA works, but a material path resolves only if the key matches Acquire's internal 'srgb:'/'data:'+path key (TextureRegistry.cpp:240). There is no public decoder hook.
- No multiply/modulate blend: MaterialComponent::BlendMode is {Alpha, Additive, Premultiplied} (Components.hpp:804). Penumbra has 24 particle systems with alphaMode=4 (AM_MODULATE).
- AM_ALPHA_TEST semantics: SE has alphaCutoff (Components.hpp:825), but its interaction with Premultiplied/lit sprites and the D3D9 alpha reference value are unverified. This matters for 2,077 of 2,607 Penumbra instances.
- Light2D does not renormalise the decoded normal (Light2D.hpp:84-90); the 2010 hPixelLight.cg does (-normalize(2*(n-0.5))). A per-sprite or per-scene switch would be needed.
- No specular term in the 2D light path. The 2010 mainSpecular uses a gloss map, specularPower, specularBrightness and a fakeEyePos; Penumbra scenes name <Gloss> 241 times.
- No vertical-surface lighting: Sprite2DLight has one constant height per sprite. The 2010 vPixelLight.cg and verticalSprite_ppl light ET_VERTICAL sprites as upright planes (normal .xzy, z*=-1).
- No per-vertex sloped depth for ET_VERTICAL sprites (2010 VS: z = (1-depth) - ((1-y)*rectSize.y)/spaceLength). SE orders quads by transform z.
- No 2D dynamic shadows: 2010 dynaShadowVS.cg plus data/shadow.dds; 82 of 2,607 Penumbra instances have castShadow=1, and lights carry castShadows.
- The 2D light cap is 64 per frame (Light2D.hpp:19). Penumbra's per-level light count has not been measured against it.
- No XML parser in SE third_party (only stb_image/stb_image_write, tinygltf, glm, entt, glfw, imgui, imguizmo, VMA, Vulkan headers). MPR's C++ readers are ad hoc string scanners. A direct .esc/.ent/.par reader needs a vendored or hand-written XML parser in the port.
- No ENML reader in SE or in MPR's C++. Penumbra needs one for data.enml (gameplay tables, Latin-1 multi-line strings) and hs.enml. The Python enml.py is the reference implementation to port.

## Open questions

- Does min(1, ambient+emissive) × colour (MPR Lighting::AmbientTerm, cited from 2013 ETHRenderEntity.cpp:113-117) hold in the 2010 engine? The 2010 ambient VS (defaultStaticAmbientVS.cg/defaultVS.cg) just passes color0, so the rule lives in the closed GameSpace.dll/machine.exe.
- Does the 2010 engine bake lightmaps at scene load (no scenes/*-esc directories ship)? If it does, does the bake use normal maps, and does it skip static lights on static sprites the way 2013 ETHEntitySpriteRenderer.cpp:70 does? This decides whether MPR's ReceiverMask or its runtimeBake path is the right model.
- Do the 2013 gated attribute reads (formats doc §5.1: specular* only if applyLight, shadow* only if castShadow) apply to the 2010 reader? Penumbra files write all attributes unconditionally.
- Was <SoundEffect> inside <ParticleSystem> live in 2010 (18 occurrences in Penumbra .ent)? The 2013 reader ignores it.
- Does the 2010 particle manager match the 2013 ETHParticleManager arithmetic that MPR's sim/Particles ports (frameSpeed ×60, 250 ms cap, release schedule, order of random draws, Luminance rule, 2 systems per entity)?
- In hPixelLight.cg the output is multiplied by diffuseColor.w, but vPixelLight.cg does not do this. Does SE's Light2D Contribution effectively include texel alpha, via tint and the premultiplied blend?
- What alpha-test reference value did D3D9 AM_ALPHA_TEST use in 2010 gs2d?
- Which lighting path is the parity reference: the pixel-shader path (UsePixelShaders(true) when supported, main.as:103-104) or the per-vertex fallback (the 'Desativa pixel shaders' option, videoModes.as:43,125-126)?
- Does machine.exe (D3D9 + Cg + audiere) run natively on Windows 11? If so, parity captures need no emulator rig, unlike MPR's docs/parity.md.
- What are the viewport and virtual resolution under videoModes.as (no app.enml; Penumbra.ethproj is the 27-byte marker)? How does the original scale to modern resolutions?
- Pipeline choice: keep MPR's Python converter to .tscn (or JSON) intermediate, or parse .esc/.ent/.par/.enml directly in C++ (which needs an XML parser)?
- Legal and repo policy: MPR's hard rule forbids committing originals, but Penumbra's repo has extracted/ and penumbra_setup.exe committed and pushed to origin (visibility not checked). Is there a written grant for Penumbra, and should MPR's reference/-gitignore model apply?
- Commit identity and trailers: MPR uses IvanCvetanovic <icvetanovic99@gmail.com> with no AI trailers; Penumbra commit 89a684b uses a univie address; this session's harness requests Co-Authored-By. Penumbra has no CLAUDE.md yet to settle this.
- Engine coordination: SE is being edited by another session. Should Penumbra's engine gaps (OGG, TTF, DDS, modulate, specular, vertical lighting, shadows) go to SE on a branch and be pinned later, as MPR did with games-out, or be solved port-side where possible (AddClip, UploadRGBA)?
- Does the 60 Hz fixed tick with a floored-millisecond script clock (Ethanon Update(float) truncation, which made MPR's port 4.17% fast) also apply to the 2010 PC engine?
