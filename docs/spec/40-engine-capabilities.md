# What Supersonic Engine offers the Penumbra port, and what it lacks

Everything below was read at engine commit `4bfcf67` ("The games moved to their own repositories"). The working tree at `<Desktop>\Supersonic-Engine` was clean on 2026-09-27. Magic Portals' `engine/` submodule is pinned to the same commit.

All engine citations are relative to `<Desktop>\Supersonic-Engine`. Citations prefixed `MPR:` are relative to `<Desktop>\Magic-Portals-Remake`. Citations prefixed `PEN:` are relative to `...\Penumbra-and-the-Castle-of-Shadows-Enhanced\extracted\app`.

**Coordination note.** Another Claude session is editing this engine for a different game. Every gap below is therefore split into two groups:
- **ENGINE CHANGE REQUIRED**: this needs a change in Supersonic (C++ and/or shader SPIR-V), and the change must not break Magic Portals or the other game.
- **GAME-SIDE**: this can be done entirely in the port's repository with no engine edit.

**Recommendation.** The port should consume the engine the way MPR does: as a git submodule pinned to a commit (`4bfcf67` today). It should not `add_subdirectory` the live Desktop checkout, which the other session is changing.

---

## 0. Verdict in one paragraph

Supersonic already has the backbone a 2D Ethanon port needs. It was built out for Magic Portals, which is a later (2013, GLES2) Ethanon game. That backbone includes:
- an `EngineLayer` seam with a fixed-step tick;
- an orthographic camera;
- unlit textured quads with a 2D sprite record (ambient, tint, additive lightmap overlay, premultiplied/additive/alpha blends);
- `Light2DComponent`, a normal-mapped 2D point light that follows Ethanon's formula;
- sprite-sheet flipbooks on the tick;
- a display-encoded ("8-bit framebuffer") scene mode;
- a screen-space HUD overlay in display values;
- a BMFont reader;
- MP3 decoding (Windows Media Foundation) and WAV;
- named input actions with tick-latched edges;
- deterministic replay and a state hash;
- a tiny test harness usable from a game repository.

What it does **not** have, measured against Penumbra's 2010 D3D9/Cg data:
- OGG Vorbis decoding (19 files);
- DDS loading (8 referenced files);
- a usable particle system (the engine's draws untextured cubes);
- TrueType or per-font text (Penumbra names "Arial Narrow", "Arial Black", "Verdana", "Arial");
- a multiply/modulate blend (`alphaMode="4"` in 14 files);
- lighting for Ethanon **vertical** entities, where the per-pixel height runs up the sprite and the normal is swizzled `xzy` (11 `.ent` files of `type="2"`, 30 `.esc` overrides);
- specular/gloss lighting (12 `.ent` files);
- projected dynamic sprite shadows (`dynaShadowVS.cg`; 11 `.ent` with `castShadow="1"`, 82 `.esc` instances);
- a second or raw (non-gamepad) joystick;
- fullscreen or runtime video-mode switching (Penumbra calls `SetWindowProperties`/`GetVideoMode`).

---

## 1. Building a game against the engine

### 1.1 CMake contract (README.md:208-253, AGENTS.md "As a subproject of a game", cmake/SupersonicTesting.cmake)

A game's top-level `CMakeLists.txt` does the following. MPR's is the working template (MPR:CMakeLists.txt:1-52).

```cmake
cmake_minimum_required(VERSION 3.20)
project(Penumbra LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20) ; set(CMAKE_CXX_STANDARD_REQUIRED ON) ; set(CMAKE_CXX_EXTENSIONS OFF)
add_subdirectory(engine)                    # SupersonicCore + Supersonic::TestHarness + supersonic_add_test
add_executable(Penumbra main.cpp PenumbraLayer.cpp)
target_link_libraries(Penumbra PRIVATE SupersonicCore)
enable_testing() ; add_subdirectory(tests)  # supersonic_add_test(test_xxx) there
```

A subproject build contributes exactly the following (README.md:229-239):

| What | Detail |
|---|---|
| `SupersonicCore` | A STATIC library (CMakeLists.txt:237). Its PUBLIC interface carries the include roots (`src/`, vendored headers), GLM defines (`GLM_FORCE_DEPTH_ZERO_TO_ONE` must match), C++20 and the platform define. |
| `Supersonic::TestHarness` | An INTERFACE target that puts `tests/TestHarness.hpp` on the include path (SupersonicTesting.cmake:22-24). |
| `supersonic_add_test(name)` | Builds `<name>.cpp` in the calling directory, links TestHarness and SupersonicCore, compiles with `/W4` (MSVC) or `-Wall -Wextra`, and registers `add_test` (SupersonicTesting.cmake:26-47). |
| `SUPERSONIC_ASSET_ROOT` | Baked to the engine checkout when the engine is a subproject; empty when it is top level (CMakeLists.txt:437-452). |
| Options | `SUPERSONIC_BUILD_EDITOR`/`_SCRIPT_PLUGIN`/`_TESTS` default ON only when the engine is top level (CMakeLists.txt:241, 538, 568). `SUPERSONIC_ENABLE_VALIDATION` is `AUTO`/`ON`/`OFF` (CMakeLists.txt:351-367). |
| `Shaders` target | Only the editor depends on it. A game build uses the committed `assets/shaders/*.spv` unless it builds `Shaders` by name. **So any engine shader change (e.g. vertical-sprite lighting) must be recompiled and committed into the engine.** |

Windows audio links `xaudio2 ole32 mfplat mfreadwrite mfuuid` privately (CMakeLists.txt:402).

MPR splits the port into two static libraries plus an exe (MPR:game/CMakeLists.txt):
- `MagicPortalsSim`: pure logic, linked by the tests.
- `MagicPortalsGame`: the layer, a static library so tests can attach it to a bare registry.
- `MagicPortals`: `main.cpp` only.

The port should copy this shape.

### 1.2 main.cpp pattern (MPR:game/main.cpp:30-431)

1. Strip the game's own flags from `argv`. `LaunchOptions::Parse` rejects unknown flags, so the remaining args go to `Supersonic::LaunchOptions::Parse(argc, argv)` (src/core/LaunchOptions.hpp:129).
2. Optionally call `Supersonic::Log::SetFileSink(path)` (src/core/Log.hpp:54). A shipped WIN32 exe has no console.
3. **Declare a manifest.** Without this step the app opens the editor's demo scene in edit mode and never ticks:
   ```cpp
   Supersonic::GameManifest manifest; manifest.isGame = true; manifest.title = "Penumbra";
   manifest.startupScene.clear();   // no .scene file: the layer builds the world
   // manifest.width/height (defaults 1280x720, GameRuntime.hpp:43-49); --window overrides
   Supersonic::SupersonicApp app(options, &manifest);            // SupersonicApp.hpp:48
   app.PushLayer(std::make_unique<PenumbraLayer>(...));          // SupersonicApp.hpp:66
   app.Run();                                                     // SupersonicApp.hpp:55
   ```
4. After `Run`, check `Supersonic::VulkanContext::ValidationErrorCount()` (VulkanContext.hpp:48). MPR fails the process if it is greater than 0 and logs `ValidationLayersActive()`.
5. Save directory: `Supersonic::UserDataDirectory(title)` (src/platform/ExecutablePath.hpp:164) resolves `%APPDATA%\<title>`. This is the place for Penumbra's `hs.enml` high scores.
6. Quit from the game: `Supersonic::Application::RequestQuit()` (src/core/Application.hpp:27). This replaces Ethanon's `Exit()`.

### 1.3 The layer seam (src/core/EngineLayer.hpp, src/core/LayerStack.hpp)

`class EngineLayer` has these members:
- `Name()` (pure virtual, :37)
- `OnAttach(entt::registry&)` (:45) and `OnDetach` (:46)
- `OnFixedUpdate(entt::registry&, float fixedDelta)` (:60)
- `OnUpdate(entt::registry&, float deltaTime)` (:72)

Layers run in push order and are detached in reverse. The stack is walked by index, so a layer may push another layer mid-tick (LayerStack.hpp:52-70). The registry is handed out raw (`SupersonicApp::Registry()`, SupersonicApp.hpp:73).

Services are reached through `registry.ctx()` pointers published by the app:

| Service | Where it is published |
|---|---|
| `MeshRegistry*` | SupersonicApp.cpp:253/395 |
| `TextureRegistry*` | :396 |
| `UIImageStore*` | :394 |
| `ScreenOverlay*` | :392 |
| `WorldShapes*` | :389 |
| `SceneManager*` | :366 |
| `ContactTracker*` | :358 |
| `AudioEngine*` | via `AudioSystem::Attach` |
| `const RenderSystem::Stats*` | :405 |
| `SimulationClock` | a value, :497/:1370 |
| `RenderSettings` | a value, which the game inserts |

**There is no render callback.** A layer never draws directly. Visuals come from components in the registry (`MeshComponent` + `MaterialComponent` + `RenderableComponent` + `TransformComponent`, `Light2DComponent`, `UI*Component`). A layer can also fill the two immediate-mode buffers once per frame in `OnUpdate`:
- `ScreenOverlay::Add(Quad)`: the HUD in display values, cap `kMaxQuads = 4096` (ScreenOverlay.hpp:92,104).
- `WorldShapes::AddLine/AddCircle/AddGroundRect/AddBox`: world-space debug lines, cap `1<<18` vertices (WorldShapes.hpp:49-89).

The renderer clears both after drawing (VulkanRenderer.cpp:1937-1989).

### 1.4 Tick and frame order (src/core/SupersonicApp.cpp)

Constants:
- `kMaxFrameDelta = 0.10f` (:62)
- `kFixedPhysicsStep = 1/60` (:68)
- `kDefaultGameTick = 1/60` (:72)
- `kMaxPhysicsStepsPerFrame = 5` (:74)

The game tick is **authored** in `SimulationClock::fixedDelta` (SimulationClock.hpp:38). MPR sets it in `OnAttach` (MPR:game/MagicPortalsLayer.cpp:199-203). Physics substeps underneath: `lround(gameTick/kFixedPhysicsStep)` substeps (:1375-1378).

Per frame, while playing (a manifest game is always playing):

1. Poll input (`InputPolling::Poll`, :1308) and compute the frame delta: the real delta clamped to 0.1 s, or `--fixed-step` (:1262-1264).
2. The accumulator loop (:1383-1540) runs up to 5 ticks. Each tick does, in order:
   - `InterpolationSystem::BeginTick`
   - `Input::BeginTickInput()` (latches edges, :1416)
   - `UIInput::BeginTickClicks`
   - record/replay step (:1430)
   - physics substeps (:1436)
   - `++clock.tick`
   - **`SpriteAnimationSystem::Update(registry, gameTick)`** (:1460)
   - **`m_layers.FixedUpdate(registry, gameTick)`** (:1468)
   - `ContactTracker`, `ScriptEngine`, `EndTick`
   - a pending scene load breaks out of the loop (:1535).
3. Dropped time accumulates in `clock.droppedSeconds`. `clock.alpha` is computed and interpolation applied (:1542-1557).
4. Per-frame systems run on the **frame** delta:
   - `AudioSystem::Update` (:1560)
   - `AnimationSystem::Advance`
   - **`ParticleSystem::Update(registry, deltaTime)`** (:1562)
   - then **`m_layers.Update(registry, deltaTime)`** (:1567).
5. World transforms resolve. Then `EditorLayer::BuildUI`, which in game mode is `buildGameView`: it draws the offscreen image as one quad plus the ImGui UI canvas (src/editor/EditorLayer.cpp:338,426).
6. `SpriteAnimationSystem::Apply` writes the UVs (:1678). The renderer then draws the scene, bloom/tonemap composite, the ScreenOverlay, and the ImGui pass.

**Doc/code disagreement.** ARCHITECTURE.md:2248 says the `OnFixedUpdate` delta is "always `kFixedPhysicsStep`". The code passes the authored `clock.fixedDelta`, as `gameTick` (SupersonicApp.cpp:1373, :1468).

### 1.5 CLI (src/core/LaunchOptions.cpp:29-50, parse :76-206; LaunchOptions.hpp:20-131)

| Flag | Meaning |
|---|---|
| `--frames <n>` | Render exactly n frames, then exit. Exits non-zero on a validation error or crash. |
| `--scene <path>` | Load a `.scene` and enter Play. A layer-built game does not need it. |
| `--screenshot <path>` | Write a PNG of the last presented, composited frame. |
| `--screenshot-every <n>` | Also write `<stem>_f<frame><ext>` every n frames. Requires `--screenshot`. Warns without `--fixed-step`. |
| `--fixed-step [s]` | Constant frame delta. Default 1/60. Value must be in (0, 1]. |
| `--window WxH` | Window size and render resolution. Overrides the manifest. |
| `--record <path>` | Write each tick's input, plus a state hash every second. |
| `--replay <path>` | Feed a recording back and verify its hashes. Exits non-zero at the first divergent tick. Refused together with `--record`. |
| `--import-assets` | Mint `.meta` files, then exit. |
| `--help` | Print usage. |

**Path caveat.** At construction a game runs `AnchorAssetRoot()` (SupersonicApp.cpp:137; ExecutablePath.cpp:98-137). This **changes the working directory** to the first of:
1. the exe folder, if it holds `assets/shaders`;
2. the current working directory, if it does;
3. `SUPERSONIC_ASSET_ROOT` (the engine checkout).

Consequences:
- Relative `--screenshot`/`--record`/`--replay` paths resolve from the engine root, so pass absolute paths (README.md:248-253).
- **The port's own data paths must be absolute.** MPR bakes compile-time directories such as `MAGICPORTALS_LEVELS_DIR` (MPR:game/main.cpp:275). Penumbra's relative paths (`soundfx/x.ogg`, `entities/x.png`) must be joined to an absolute data root before they reach the texture or audio caches.

---

## 2. 2D rendering

### 2.1 Sprites are textured quads

There is no sprite batcher API. A sprite is an entity with the following components (the MPR pattern, MPR:game/MagicPortalsLayer.cpp:3096-3121):
- `TransformComponent`: position, Euler rotation in radians, scale (Components.hpp:143).
- `MeshComponent{primitiveType="Quad"}`: a unit quad, `kQuadWidth = kQuadHeight = 1` (ModelLoader.hpp:31-32), centred and facing +Z (MeshRegistry.cpp:238).
- `MaterialComponent{unlit=true, transparent=true, blend=..., albedoTexturePath=...}`.
- `RenderableComponent{castsShadow=false}`.

Size is set through `scale`. The pivot is the centre, so Ethanon's origin/pivot offsets become a translation.

Arbitrary geometry is also possible:
1. Upload it with `MeshRegistry::Upload(key, MeshData)` (MeshRegistry.hpp:100) or `Replace(id, MeshData)` (:155).
2. Point `MeshComponent::meshKey` at it (Components.hpp:911).

This is how per-vertex shapes (shadow trapezoids, gradient rectangles, text meshes) can be drawn. The vertex format has an RGB vertex colour that the shader multiplies in (`fragColor`, shader.frag:426/511). There is **no per-vertex alpha**; `Vertex::color` is a vec3 (Components.hpp:29-35).

Batching: consecutive compatible draws are instanced. The caps are:
- `kMaxInstances = 65536` draw records per frame (VulkanRenderer.hpp:286);
- 4095 UV-transform slots per frame (VulkanRenderer.hpp:277-289); every flipbooked or scrolled material takes one;
- 1024 material descriptor sets (`MaterialSets::kMaxSets`), one per distinct texture quadruple.

### 2.2 Textures (src/renderer/TextureRegistry.cpp)

- The loader is stb_image v2.30: `stbi_load(path, ..., STBI_rgb_alpha)` (TextureRegistry.cpp:13, :249). It reads **PNG, JPEG (baseline and progressive), BMP (uncompressed 1/4/8/16/24/32 bit, paletted included), TGA, GIF, PSD, HDR, PIC, PNM**.
- It reads **no DDS**. Penumbra references 8 DDS files: `thorn`, `dirt`, `tree01/02/03/05_alpha`, `black_sword`, `fog` (fog is referenced by 16 files). All are uncompressed A8R8G8B8 per `file`. `data/shadow.dds` (32x32, 16-bit luminance) is referenced by no content file. Only `black_sword.dds` has a PNG sibling.
- Penumbra's BMPs are 8-bit paletted (`halo*.bmp`, `flash.bmp`, `particle.bmp`), 24-bit and 32-bit. All are stb-readable. The JPEGs include one progressive file (`entities/face.jpg`), which stb reads.
- Cache key: `(srgb ? "srgb:" : "data:") + path` (TextureRegistry.cpp:240). In a `DisplayEncoded` scene, albedo and overlay are acquired with `srgb=false` (RenderSystem.cpp:341, :481, :504). The ScreenOverlay also always uses `"data:"` (VulkanRenderer.cpp:1956).
  - **Game-side DDS workaround:** decode the DDS yourself, then call `registry.ctx().find<TextureRegistry*>()` → `UploadRGBA("data:" + path, pixels, w, h, /*srgb*/false, filter)` (TextureRegistry.hpp:62) **before** any material names that path. `Acquire` then hits the cache.
  - The alternative is converting DDS to PNG offline.
- A mip chain is always generated and filtering is linear. `nearest` is available only through a `.meta` sidecar with `"Filter": "nearest"` (AssetDatabase.cpp:142-158). `UploadRGBA` takes the filter as a parameter.
- **Sampler address mode is always `eRepeat`**, the default in `VulkanImage::CreateSampler` (VulkanImage.hpp:82). There is no per-texture clamp. A linearly filtered sprite can bleed its opposite edge at the borders.
- Colour textures are `R8G8B8A8_SRGB` in linear scenes and `UNORM` when not decoded. Normal maps are always `UNORM`.

### 2.3 Camera (Components.hpp:258-347)

- `CameraComponent::projection = Projection::Orthographic` (:268).
- `orthoHeight` is the world units spanned vertically (:274). Width follows `aspect`, which the game view sets from the window.
- The camera looks along `front` (default `(0,0,-1)`) from `position`. World **+Y is up**. Ethanon's screen space is y-down, so the port converts (MPR `Units::ToWorld`).
- Set `nearPlane`/`farPlane` to cover the z range used for layering.
- **Set `flyControlsEnabled = false`** (:310). It defaults to true, and `CameraSystem` would otherwise fly the camera on WASD/arrows. MPR does this at MPR:MagicPortalsLayer.cpp:2892-2921.

### 2.4 Draw order

Transparent quads (`MaterialComponent::transparent`) go through the blended pass with **depth writes off**. They are sorted by:
1. `viewDepth` descending (back to front);
2. `RenderableComponent::sortKey` ascending (Components.hpp:999);
3. gather order.

The sort is `RenderSystem::SortTransparentDraws`, RenderSystem.cpp:158-176. Under the ortho camera, **a larger world z is nearer and is drawn later**. MPR draws every sprite as transparent and encodes draw order in z (`placeSprite`, MPR:MagicPortalsLayer.cpp:3113-3121).

Opaque quads (`transparent=false`) write depth with a `lessOrEqual` test and can use `alphaCutoff` (Components.hpp:825) for 1-bit cut-outs. This fits Ethanon `AM_ALPHA_TEST` if `blendMode="2"` means that in 2010 (see open questions): 76 `.ent` files carry `blendMode="2"`.

### 2.5 Blend modes (Components.hpp:804; VulkanPipeline.cpp:34-70; VulkanPipeline.hpp:302)

| `MaterialComponent::BlendMode` | Vulkan factors |
|---|---|
| `Alpha` (Mix) | colour: SrcAlpha, OneMinusSrcAlpha; alpha: One, OneMinusSrcAlpha |
| `Additive` (Add) | colour: **SrcAlpha, One** (not One, One); alpha: Zero, One |
| `Premultiplied` | colour: One, OneMinusSrcAlpha. The shader multiplies rgb by alpha. |

There is **no Multiply/Modulate** (`GL_ZERO, GL_SRC_COLOR`). Penumbra uses `alphaMode="4"` in 4 `.par` files (`fade_out_shadow_beam`, `shadow_beam`, `shadow_beam_wide`, `sword_beam`) and 10 `.ent` files (`bruxo`, `bruxo_dead`, `checkpoint`, `fade_out_beam`, `master_knight`, `minion`, `princess`, `sword_beam`, `vert_bruxo`, `vert_master_knight`). In the 2013 enum, 4 = `AM_MODULATE` (MPR:docs/ethanon-formats.md:1081-1090).

Additive with SrcAlpha equals Ethanon's One, One whenever alpha = 1. That covers every JPG/BMP and opaque PNG. It differs for PNGs whose alpha is below 1.

The ScreenOverlay pipeline is Mix only (VulkanRenderer.cpp:523-545).

### 2.6 Colour pipeline for 2D (src/core/RenderSettings.hpp)

The game puts a `RenderSettings` into `registry.ctx()` (MPR:MagicPortalsLayer.cpp:149-160, :205):
- `encoding = SceneEncoding::DisplayEncoded` (:117): texture bytes are display values, there is no sRGB decode, no bloom, no tonemap, and each sprite's base is clamped as on an 8-bit target;
- `background = Background::Color` with `backgroundColor` (:63-91), so no sky pass;
- `bloomIntensity = 0`;
- optional `quantize = OutputQuantize::Rgb565` (:124). Penumbra asks for `PF32BIT` (PEN:main.as:144), so this is not needed.

This is the right mode for a D3D9 8-bit-per-channel game.

### 2.7 Tint, ambient, emissive, lightmap: the 2D sprite record (Components.hpp:645-878; RenderSystem.cpp:198-226; shader.frag:421-481)

- `MaterialComponent::albedoColor` (vec4) is the per-sprite tint and alpha. On the plain unlit path it is **unclamped**, so values above 1 flash (Components.hpp:648-661).
- `MaterialComponent::sprite2D` (`Sprite2DLight`, :709) has these fields:
  - `enabled`
  - `ambient` (vec3; multiplies texel × tint)
  - `height` (the surface's lighting height, not z)
  - `lightMask` (uint8; 0 = no lights, and the normal map is not sampled)
  - `normalYDown` (DirectX-convention green)
  - `overlayStrength`
- `overlayTexturePath` (:697) is an additive map applied after the multiply (a baked lightmap).
- The base is `clamp(texel*vertexColour*tint*ambient + overlay*strength, 0, 1)`.
- Ethanon's emissive (`<EmissiveColor>`) is not a separate field. MPR folds `min(1, ambient + emissive)` into `sprite2D.ambient` on the CPU (MPR `Lighting::AmbientTerm`, MagicPortalsLayer.cpp:4655-4690). `MaterialComponent::emissiveColor/emissiveStrength` belong to the PBR path and are not used by sprites.

### 2.8 Light2D: normal-mapped 2D point lights, with no shadows

`Light2DComponent` (Components.hpp:411-431) has:
- `color` (vec3)
- `intensity` (folded into colour on the CPU; may exceed 1)
- `range` (world units; zero past it)
- `height` (the light's z in lighting space)
- `layers` (uint8 mask)
- `enabled`

The light's world x and y come from the entity's world transform. Gathered per frame by `Light2D::GatherLights2D` (Light2D.hpp:73) into scene binding 12. Cap is **`kMaxLights2D = 64` per frame** (Light2D.hpp:19); extra lights are dropped and logged, and zero-colour lights are skipped.

Shader (shader.frag:421-481), per light whose `layers & lightMask` is non-zero:
```
n   = decode(normalMap)*2-1  (NOT renormalised), y flipped if normalYDown,
      carried along normalize(model[0]) and normalize(model[1]); z = z
P   = (fragWorld.x, fragWorld.y, sprite2D.height)     // constant per draw
v   = L - P ; d2 = dot(v,v) ; if d2 >= r2 skip
lit += clamp(texel*tint_without_ambient*colour * (1 - d2/r2) * dot(v,n)/sqrt(d2), 0, 1)
out = premultiplied ? (base*alpha + lit, alpha) : (base + lit, alpha)
```
The CPU twin is `Light2D::WorldNormal`/`Light2D::Contribution` (Light2D.hpp:90-100), tested by `test_light2d`.

There are **no 2D shadows or occluders**. The 3D `LightComponent` shadow maps have no meaning for sprites.

**Compared against Penumbra's own Cg (PEN:data/hPixelLight.cg, vPixelLight.cg, pixelLightVS.cg):**

| Aspect | Ethanon 2010 (Cg) | Engine `shadeSprite2D` | Match? |
|---|---|---|---|
| Attenuation | `1 - d²/max(d², r²)` | `1 - d²/r²`, zero at or beyond r | yes |
| Facing | `dot(normalize(P-L), -N)` | `dot(normalize(L-P), N)` | yes (same sign) |
| Normal decode | `-normalize(2*(n-0.5))`, **renormalised** | decoded, **not** renormalised (MP/GLES2 behaviour) | **no**. Game-side fix: renormalise normal-map texels at load and upload them via `UploadRGBA` under `"data:"+path`. |
| Horizontal (`type 0`) P | `topLeft3DPos + (x*w, y*h, 0)`, constant z | `(fragX, fragY, height)` | yes |
| Horizontal output | `... * diffuse.w` (light weighted by texel alpha) | `Premultiplied` adds light at full weight; `Alpha` gives `(base+lit)*alpha` | Use **`Alpha`, not `Premultiplied`** (MP uses Premultiplied), assuming the 2010 light pass is One,One (open question) |
| **Vertical (`type 2`)** P | `topLeft3DPos + (x*w, 0, -y*h)`: the pixel's height varies up the sprite, y is constant at the base (pixelLightVS.cg `verticalSprite_ppl`) | height constant per draw | **no. ENGINE CHANGE.** |
| Vertical normal | swizzled `n.xzy`, `z *= -1` (vPixelLight.cg) | none | **no. ENGINE CHANGE.** |
| Vertical depth | per-vertex `z = (1-depth) - ((1-v)*rectSize.y)/spaceLength`: the sprite leans back in the z-buffer | flat quad | Partly game-side: tilt the quad about X under the ortho camera, but lighting then uses the wrong P |
| Specular (`mainSpecular`, `<Gloss>` map, `specularPower`/`specularBrightness`) | Blinn-Phong with a gloss map and a `fakeEyePos` | none | **no. ENGINE CHANGE**, or drop it (12 `.ent` files name a `<Gloss>`) |
| Dynamic shadow (`dynaShadowVS.cg`) | The sprite is redrawn with its top vertices extruded along `normalize(vertex - light)` by `shadowLength`, offset by `shadowZ`: a per-light trapezoid | none | Game-side possible: build the geometry per frame with `MeshRegistry::Replace` + `meshKey` (cost unmeasured). Otherwise an engine feature. |
| Vertex-lighting fallback (`h/vVertexLightShader.cg`, `UsePixelShaders(false)`) | per-vertex | n/a | The port can simply always use per-pixel lighting |

Entity-type census (PEN:entities/*.ent): `type="0"` ×127, `type="2"` ×11, `type="5"` ×6. The type-2 files are `barrel`, `cursor`, `devil`, `fog_menu`, `gamelogo`, `ground_fire`, `pilar`, `thumbnail`, `vert_bruxo`, `vert_king`, `vert_master_knight`. `.esc` inline overrides: `type="0"` ×2365, `type="2"` ×30, `type="5"` ×212. `applyLight="1"` ×82 and `"0"` ×62. 71 `<Normal>` maps. `<Light active="1">` in 37 `.ent` files (19 dynamic, 18 static).

### 2.9 Sprite sheets (SpriteAnimationComponent, Components.hpp:1618-1653; SpriteAnimationSystem.hpp)

Fields:
- `columns`, `rows`: cells run left to right, then top to bottom.
- `firstFrame`, `frameCount`: 0 means all cells.
- `framesPerSecond`, `loop`, `playing`.
- Tick state: `frame`, `elapsed`.

Behaviour:
- Advanced **on the tick** (SupersonicApp.cpp:1460).
- Hashed: the frame and accumulator are in `StateHash` (StateHash.cpp:203); the grid is not.
- Applied as `MaterialComponent::uvScale/uvOffset` by `SpriteAnimationSystem::Apply` (:1678). `CellTransform(columns, rows, cell, scale, offset)` is public.

For Ethanon's `SetFrame`/`SpriteCut`, MPR sets `playing=false`, `frameCount=1` and writes `firstFrame` itself (MPR:MagicPortalsLayer.cpp:975-989). The cut is always an even split. Ethanon strides whole pixels, which only matters when the sheet size is not a multiple of the cut.

UV-transform alternatives: `MaterialComponent::uvScale/uvRotation/uvOffset` (Components.hpp:848-866) for scrolling (Ethanon `scroll`/`multiply`). **Sprite flipping** is a negative `scale.x`/`scale.y`; `Light2D::WorldNormal` handles mirrored sprites.

### 2.10 How Magic Portals draws (reference pattern)

- Every visible thing is `makeSprite(...)`: an unlit, transparent Quad with Alpha or Additive blend and the albedo path (MPR:MagicPortalsLayer.cpp:3096-3111).
- It is placed by `placeSprite(centrePx, sizePx, z, rotation)` (:3113-3121).
- Each frame `tint(...)` (:4640-4690) writes `albedoColor`, `sprite2D` (ambient, height, `lightMask`, `normalYDown`), `overlayTexturePath` (lightmap), `normalTexturePath` and `blend`. Alpha becomes Premultiplied when lit. Each field is written only when it changes, to keep `SyncResources` signatures stable.
- Lights are `Light2DComponent` entities (:4694-4930). Halos are additive sprites.
- Particles are game-simulated, one quad entity per live particle (§3).
- The HUD is `ScreenOverlay` quads plus BitmapFont glyphs (§4, §5).

---

## 3. Particles

The engine's `ParticleEmitterComponent` (Components.hpp:1655-1669) plus `ParticleSystem::Update` (ParticleSystem.cpp:22-94) are **not usable** for Ethanon `.par` effects:

- Particles are drawn as **small untextured cubes** (the cube mesh with the white material set, RenderSystem.cpp:1480-1552). There is no bitmap, sprite cut, rotation, blend choice or lighting.
- Spawning uses a fixed velocity formula (ParticleSystem.cpp:56-59). Gravity is hard-coded at `1.5` (:85). Colour lerps from start to end. Size shrinks with age (RenderSystem.cpp:1509).
- It steps on the **frame delta**, not the tick (SupersonicApp.cpp:1562). It uses a process-static `std::mt19937{12345}` (ParticleSystem.cpp:14-18). It is **not in StateHash**.

What Penumbra needs: 93 particle systems (43 `.par` in `effects/`, the rest inline in `.ent`), a total pool of 2064 particles, at most 70 per system. They use:
- `alphaMode` 0/1/4 (ents: 14/26/10; pars: 14/25/4);
- `SpriteCut` up to 8×8;
- `animationMode`, `growth`, `minSize`/`maxSize`, `randAngle`, gravity, direction and randomisation, `Color0`→`Color1`, `Luminance`;
- a `<SoundEffect>` per system (e.g. blood.par plays `hit01.ogg`).

**GAME-SIDE (the MPR pattern, MPR:game/sim/Particles.hpp/.cpp, MagicPortalsLayer.cpp:940-1000):**
1. Port Ethanon's particle arithmetic as pure code, stepped on the tick with a seeded game RNG.
2. Draw each live particle as a pooled quad entity from `makeSprite`, with a `SpriteAnimationComponent` for cut sheets.
3. Hide a quad with `RenderableComponent::isVisible = false` rather than destroying it.

Budget: each flipbooked particle takes one of the 4095 UV slots per frame. 2064 pooled particles fit. Modulate-blended systems still need the engine blend in §2.5.

---

## 4. Text

### 4.1 BitmapFont (src/core/BitmapFont.hpp/.cpp)

- It reads **BMFont text `.fnt` only** (not the binary variant): `info`/`common`/`page`/`char` lines (BitmapFont.hpp:22-25; `Load` :74).
- It never rasterises and never loads textures. `Pages()` lists the page PNGs for the caller to upload.
- API:
  - `Find(codepoint)` (:88)
  - `Measure(text)`: widest line × `lineCount*lineHeight` (:96)
  - `BuildText(text, page, MeshData&)`: world-space glyph quads in font pixels, +x right, lines descending −y, UVs normalised (:110)
  - `LineHeight()`, `Base()`, `PageSize()`
- **Kerning is ignored** (BitmapFont.cpp:157-159).
- **Encoding: one byte per glyph.** Each `char` becomes `static_cast<unsigned char>` and is used as the code point (BitmapFont.cpp:201, :226).
  - Latin-1 bytes therefore map exactly onto Unicode U+0000–U+00FF: `ã`=0xE3, `ç`=0xE7, `é`=0xE9, `õ`=0xF5, `Ã`=0xC3, `É`=0xC9. Accents work **only if the string is fed as Latin-1** and the `.fnt` was generated with those Unicode ids.
  - **UTF-8 input is not decoded.** "ã" as UTF-8 (C3 A3) would draw as "Ã£".
  - Penumbra's `.as` strings are Latin-1, e.g. `"Não é possível invocar 2 criaturas ao mesmo tempo"`.
  - Game-side: keep strings as Latin-1, or convert UTF-8 to Latin-1 before `BuildText`/`Measure`.
- For the HUD, MPR lays out glyphs itself (MPR:game/sim/Hud.cpp:406-451, `LayOutText`/`LayOutCaption`). It emits one `ScreenOverlay::Quad` per glyph with `uvMin/uvMax` over the page texture (MPR:MagicPortalsLayer.cpp:2740-2870). World text would go through `BuildText` → `MeshRegistry::Upload` → `meshKey`.

### 4.2 Fonts Penumbra asks for

`DrawText(pos, text, fontName, size, colour)` names **system TrueType fonts**:
- "Arial Narrow" ×18
- "Arial Black" ×4
- "Verdana" ×2
- "Arial" ×1

Sizes run from 15 to 40 px (PEN:interface.as:65-91, util.as:450-454 `shadowText`, videoModes.as:95-129, menu.as:226-229). The extracted game ships **no `.fnt`**.

The engine has **no TrueType path for game text**. The only TTF rasteriser is ImGui's (1.93 WIP), and it loads only the embedded Inter and Font Awesome (EditorFonts.cpp:63-84, called for every app at SupersonicApp.cpp:230-235).

GAME-SIDE options:
- (a) Pre-bake BMFont `.fnt` + PNG pages for each face and size, covering at least 0x20–0xFF. Licensing of Arial or Verdana bitmaps is an open question; a metric-compatible free substitute (e.g. Liberation Sans Narrow for Arial Narrow) avoids it.
- (b) Accept Inter through `UITextComponent`.

`shadowText` (offset `size*0.1`, shadow alpha/2) must be reproduced by the game. `UITextComponent::shadow` has its own fixed offset.

### 4.3 UITextComponent (Components.hpp:1965-1985; UISystem.cpp:467-790)

It draws through ImGui's draw list with `ImGui::GetFont()` (Inter). Its fields are `text` (**UTF-8**), `anchor`, `offset`, `fontSize` (authored against 1080-px height), `color`, `wrapWidth`, `shadow`, `visible`, `worldSpace`. There is **no font-family field**. It draws above the ScreenOverlay, because ImGui is the last pass.

---

## 5. UI and HUD

- **ScreenOverlay** (src/core/ScreenOverlay.hpp): immediate quads in display values, drawn after the tonemap. Order is draw order.
  - `Quad{min,max}` in image fractions, top-left (0,0), +y down.
  - `uvMin/uvMax`.
  - `color` (display-referred multiply, alpha mixes).
  - `texture` (a path, acquired as `"data:"`, stb only).
  - `basis` (a 2×2 rotation; `ScreenOverlay::Rotation(radians, aspect)`).
  - Cap 4096 quads per frame. Mix blend only. One colour per quad, so Ethanon's 4-corner `DrawRectangle` gradient (used once, PEN:menu.as:226) needs a gradient texture or stacked quads.
  - Emit it in `OnUpdate` (once per frame), not `OnFixedUpdate`.
  - This covers Penumbra's `DrawSprite`/`DrawShapedSprite`/`DrawRectangle` HUD calls (10/6/5 uses) and its screen fades (`DrawRectangle(0,0,GetScreenSize(),c,c,c,c)` in util.as:376-400).
- **UICanvas/UISystem/UIInput** (ARCHITECTURE.md §10, :2722-2819):
  - Components: `UIPanelComponent`, `UIButtonComponent`, `UITextComponent`, `UIImageComponent` (nine-slice `border`, `texture` as a `UIImageStore` id), `UITextFieldComponent`, `UIStackComponent`, `UIShapeComponent`, `UIOrderComponent`.
  - Layout: a 3×3 `UIAnchor` grid, sizes authored against `kReferenceHeight = 1080` (UICanvas.hpp:50).
  - Buttons: armed on the press edge, clicked on release over the same button, disabled buttons keep their place. `clickedThisTick` is replay-safe.
  - A focused text field suppresses key-derived actions (`Input::SetTextCaptureActive`).
  - Drawn with ImGui (Inter). Penumbra's menus are custom-drawn text lists, so ScreenOverlay plus BitmapFont is the closer match; the UI canvas is optional.

---

## 6. Audio (src/core/AudioEngine.hpp, AudioClip.hpp/.cpp, AudioSystem.cpp; ARCHITECTURE.md §11 :2821-2929)

- **Backend:** XAudio2 on Windows, one source voice per sound on the mastering voice. There is no voice cap, so many simultaneous samples are fine.
- **Codecs:** `AudioClip::Load` dispatches **by extension, `.wav` and `.mp3` only**. Anything else is refused with "is not a sound this engine reads (.wav or .mp3)" (AudioClip.cpp:85-91).
  - WAV: a hand-written RIFF reader for PCM 8/16/32-bit, IEEE float and EXTENSIBLE, mono or stereo (ARCHITECTURE.md:2848-2862).
  - MP3: **Windows Media Foundation**, decoded to 16-bit PCM (AudioClip.cpp:93-160+). It fails on other OSes.
  - **OGG Vorbis: not supported.** Penumbra has 19 `.ogg` files (Vorbis, 44.1 kHz, mono and stereo) and 16 `.mp3` files.
  - **GAME-SIDE workaround:** vendor `stb_vorbis.c` (public domain) in the port. Decode to 16-bit PCM, build `AudioClip{channels, sampleRate, 16, pcm}`, and call `AudioEngine::AddClip(path, clip)` (AudioEngine.hpp:65) under the same key the game later passes to `Play`. `LoadClip` checks its cache before the extension (AudioEngine.cpp:400-403), so this works. Do it before the first `Play` of that key, because a failed load is cached as an empty clip.
- **Decoding cost:** clips are decoded whole into memory on the game thread at first use. There is no streaming (ARCHITECTURE.md:2867-2869). `fase.mp3` (2.4 MB, about 26 MB PCM) and `chefao.mp3` (2.0 MB) should be preloaded with `LoadClip` in `OnAttach` or on a loading screen.
- **API:**
  - `Play(path, loop, volume, pitch) → VoiceId` (:71)
  - `Stop(voice)` (:73)
  - `SetVoiceParameters(voice, volume, pitch, pan)` (:95): volume clamped to [0,1], pitch (frequency ratio) to [0.5,2] (AudioEngine.cpp:147-148), pan constant-power
  - `IsVoicePlaying` (:96)
  - `StopVoicesUsing`, `UnloadClip`
  - Finished one-shots are reaped every frame by `AudioSystem::Update` (AudioSystem.cpp:175), which runs whenever playing.
  - The engine is reached via `registry.ctx().find<AudioEngine*>()` (MPR:MagicPortalsLayer.cpp:6257).
- **Mapping to Penumbra's calls:** `PlaySample` ×27, `LoopSample` ×4, `SetSampleVolume` ×5, `IsSamplePlaying` ×4, `StopSample` ×3, `LoadSoundEffect` ×35, `LoadMusic` ×4. All of them map. There is **no master or global volume**; volume is per voice only.
- `AudioSourceComponent` (Components.hpp:1423) offers 3D inverse-distance attenuation with the listener at the camera. A 2D game can simply call `Play` directly.
- `AudioSystem::Update` runs on the frame delta and outside the replay. Audio is presentation only.

---

## 7. Input (src/core/Input.hpp, Input.cpp, src/platform/InputPolling.cpp)

- **Raw keys:** every GLFW key code is polled (InputPolling.cpp:50-75, 175-180). F1–F25, the keypad, Home/End/PageUp and the rest are all there. `Key::` names only a subset (Input.hpp:27-50); pass GLFW ints for the others, e.g. PageUp = 266.
  - `Input::IsKeyDown(key)` (:547) and `WasKeyPressed(key)` (:548) are **per frame**.
  - There is **no raw `WasKeyReleased`**.
- **Named actions and axes:**
  - `BindActionKey/MouseButton/PadButton` (:236-238) and `BindAxisKeys/BindAxisPad` (:241-242). Any bound source satisfies an action.
  - `IsDown` (:420), `WasPressed`/`WasReleased` (per frame, :421-422).
  - **Tick-latched edges** `TickWasPressed`/`TickWasReleased` (:268-269): a press is seen by exactly one tick.
  - `GetAxis` (:426), `MousePosition` (:428) in window pixels, `MouseDelta`, `Scroll`/`TickScroll`, `TypedCharacters`.
- **Ethanon KEY_STATE mapping** (Penumbra: `GetKeyState` ×22, `KeyDown` ×18; `KS_HIT` ×20, `KS_UP` ×11, `KS_DOWN` ×4):

  | Ethanon | Engine, inside `OnFixedUpdate` |
  |---|---|
  | `KS_HIT` | `TickWasPressed(action)` |
  | `KS_DOWN` | `IsDown(action)` |
  | `KS_RELEASE` | `TickWasReleased(action)` |
  | `KS_UP` | `!IsDown(action)` |

  Key mappings:
  - `K_CTRL` → bind `LeftControl` and `RightControl`
  - `K_ALT` → `LeftAlt` and `RightAlt`
  - `K_RETURN` → `Enter`, `K_ESC` → `Escape`, `K_BACKSPACE` → `Backspace`, `K_PAGEUP` → 266
  - `K_LMOUSE`/`K_RMOUSE` → `BindActionMouseButton(MouseButton::Left/Right)`
  - `GetCursorPos` → `MousePosition()`
- **Two players on one keyboard:** give each player distinct action names (e.g. `P1Left` → Left, `P2Left` → A). Actions are global, not per device.
- **Default bindings:** `Input::LoadDefaultBindings()` runs at startup (SupersonicApp.cpp:178; Input.cpp:322-356). It binds MoveX/MoveY/LookX/LookY/Jump/Fire/AltFire/Sprint/Interact/Crouch/Pause on WASD, arrows, Space, LShift, E, LCtrl, Esc and the mouse. Call `Input::ClearBindings()` (:244) and bind only the game's own, or those names simply sit unused.
- **Replay coverage:** only actions, axes, mouse position/delta, scroll, contacts and UI clicks are recorded or diverted (`TickInput`, :320-402; `BeginReplayedTick`, :405-417). **Raw `IsKeyDown`/`WasKeyPressed` are NOT replayed.** A port that wants `--record`/`--replay` must read input only through named actions inside `OnFixedUpdate`.
- **Gamepad:** only **`GLFW_JOYSTICK_1`**, and only if GLFW recognises it as a *gamepad* (an SDL-mapping-DB controller) (InputPolling.cpp:208-219).
  - It exposes 15 buttons (`Pad::A..DpadLeft`) and 6 axes (`LeftX..RightTrigger`), with a stick deadzone of 0.18 (Input.hpp:553).
  - Penumbra reads up to 2 joysticks (`GetJoystickStatus(0/1)`, playerInput.as:162-163), with DirectInput-style buttons `JK_01..JK_10` and `GetJoystickXY`.
  - **There is no second pad, no raw non-gamepad joystick, and no hot-plug event API** beyond `IsGamepadConnected()`. ENGINE CHANGE if two pads or raw joysticks are needed.
- **Touch:** the mouse is synthesised as contact 0. Irrelevant here.

---

## 8. Test harness and determinism

- **Harness** (tests/TestHarness.hpp): `CHECK(expr)`, `CHECK_MSG(expr,msg)`, `CHECK_NEAR(a,b)` (eps 1e-4), `CHECK_EQ(a,b)`. `test::summary(suite, minChecks)` returns non-zero on any failure **or when fewer than `minChecks` ran**. Each suite is a plain `main()`, registered with `supersonic_add_test(test_penumbra_xxx)`.
- The MPR test pattern: the sim library is pure and deterministic, and the layer is a static library attachable to a bare `entt::registry` (no device). Services that are absent from `ctx()` are skipped.
- **SimulationClock** (SimulationClock.hpp): `tick`, `fixedDelta` (authored), `alpha` (render interpolation, never read in a tick), `droppedSeconds`, and `Seconds()` = `tick*fixedDelta` in double.
  - Penumbra calls `GetTime()` 59 times (wall-clock ms). The port must derive all game time from the tick. Timers such as `timer.as` must count ticks.
- **StateHash** (StateHash.hpp): `Compute(registry)` (:62) walks every live entity: transforms, bodies, script state, sprite-animation frame, tilemap cells.
  - Floats are hashed by their bytes, order-independent, and seeded per entity index.
  - `RegisterContributor(name, fn(registry, Mixer&))` (:131) adds game-owned state.
  - `Mixer` has `U64` and similar methods (:96-110).
  - The tick-zero hash is necessary, not sufficient. A replay is scoped to one scene (README.md "Known and written down elsewhere").
- **Determinism:** the engine avoids libm in the simulation (`DetMath.hpp`: sin/cos/asin/atan2/pow). A game calling `std::sin` inside its tick gets its C runtime's last bit (README.md "Two limits remain"). Penumbra's AngelScript uses `sin`/`cos`/`rand`, so the port should use DetMath or an owned seeded RNG.
- **`--fixed-step` plus `--frames` plus `--screenshot[-every]`** gives reproducible frame captures. MPR measured the stamped frames as byte-identical to separate runs (ARCHITECTURE.md §8c).
- `tools/verify-replay.ps1` in the engine repository is the pattern for record → replay → corrupt → expect failure.

---

## 9. Constraints on this machine

- **Smart App Control is in enforce mode** (MPR:CLAUDE.md:146-150; README.md:285-288; DEVLOG.md:1529-2392; engine docs/planning/2026-09-10-migration-readiness.md:161-164).
  - It randomly refuses the first launch of freshly linked exes: "An Application Control policy has blocked this file", "Permission denied", ctest "Not Run", rc 126.
  - **Every refusal shows Ivan a Windows notification.**
  - Rules: relink nothing unnecessary; launch each suite at most once; never loop ctest or relinks; report a refused exe as not run; use `ctest --test-dir build -N` to list tests without launching them.
- **Toolchain** (MPR:tools/parity/port_build.bat; paths verified present):
  - `call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"`: VS Build Tools 18, MSVC toolset 14.50 (v145), x64.
  - CMake 3.29.2, Ninja 1.12.0 and ctest are Strawberry Perl's (`C:\Strawberry\c\bin`, on PATH). **Pass `-DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl`**, or CMake picks Strawberry's GCC.
  - Vulkan SDK 1.4.357.0 at `C:\VulkanSDK\1.4.357.0`: validation layers, plus `-DGLSL_COMPILER=C:/VulkanSDK/1.4.357.0/Bin/glslc.exe`.
  - Configure: `cmake -S <repo> -B <repo>\build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSUPERSONIC_ENABLE_VALIDATION=ON -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DGLSL_COMPILER=...`, then `cmake --build build --target <exe>`. Invoke it from bash as `cmd //c "tools\...\port_build.bat --target X"`.
  - Engine standard: `/W4` with zero warnings.
- **No `D:` drive.** Keep paths free of spaces.
- **Asset-root chdir** (§1.5): the port's data paths must be absolute.
- **Window:** the size comes from the manifest or `--window` only. There is no fullscreen (no `glfwSetWindowMonitor` anywhere), no runtime resize or video-mode enumeration API, and `Window` is not exposed to layers (src/platform/Window.hpp).
  - Present mode is Mailbox, else FIFO (VulkanSwapchain.cpp:66-72).
  - Penumbra defaults to 1024×768 and offers a windowed/fullscreen toggle plus a video-mode list (main.as:144, menu.as:111, videoModes.as:95-129).
- **Shared engine:** the engine is read-only for this port and another session edits it. The rest of this bullet is my inference, not something I checked: every ENGINE CHANGE below probably touches shared files (`shader.frag`, `Components.hpp`, `VulkanPipeline`, `AudioClip`, `InputPolling`), needs recompiled committed SPIR-V, and must keep `test_materials`/`test_light2d`/`test_screenoverlay` (which read `shader.frag`) green for MPR and the other game.

---

## 10. ENGINE GAPS for Penumbra, concrete

### 10.A ENGINE CHANGE REQUIRED (touches the shared engine)

1. **Modulate/multiply blend** (`GL_ZERO, GL_SRC_COLOR` / D3D `ZERO, SRCCOLOR`). Needed by 14 files with `alphaMode="4"`: 4 `.par` beam effects, plus `bruxo`, `master_knight`, `minion`, `princess`, `checkpoint` and others among the `.ent`. `BlendEquation` has only Mix/Add/Premultiplied (VulkanPipeline.hpp:302). A new mode needs an enum value, a pipeline variant and the transparent-pass run split. A partial game-side approximation exists: darken with Alpha-blended black at `(1 - luminance)`, but it is not exact.
2. **Vertical-entity lighting (Ethanon `type="2"`).** The per-pixel P must be `(x, baseY, heightAlongSprite)` and the normal swizzled `xzy` with `z *= -1` (pixelLightVS.cg `verticalSprite_ppl`, vPixelLight.cg). `shadeSprite2D` uses a constant height per draw (shader.frag:421-481). Affects 11 `.ent` and 30 `.esc` overrides (characters, pillars, barrels, bosses).
3. **Renormalised normals** (2010 Cg does `-normalize(2n-1)`; the engine deliberately does not). This could be a flag; the game-side alternative is renormalising texels at load (see 10.B).
4. **Specular/gloss term** (`mainSpecular`: gloss map, `specularPower`, `specularBrightness`, `fakeEyePos`). 12 `.ent` files name `<Gloss>`. Alternatively drop it as a fidelity loss.
5. **Projected 2D sprite shadows** (`dynaShadowVS.cg`, `castShadow`, `shadowScale`, `shadowLengthScale`, `shadowOpacity`, `shadowZ`). 11 `.ent` and 82 `.esc` instances. There is nothing in the engine. It can be done game-side with per-frame meshes (10.B), but a proper feature would be an engine change.
6. **Per-texture clamp-to-edge sampling.** Every texture uses `eRepeat` (VulkanImage.hpp:82). Linear-filtered sprite borders can bleed.
7. **Second gamepad and raw (non-gamepad) joysticks.** Only `GLFW_JOYSTICK_1` as a gamepad is polled (InputPolling.cpp:208-219). `RawInputState` has one pad (Input.hpp:118-131). There are no per-pad actions.
8. **Fullscreen and video modes at runtime.** Penumbra toggles windowed/fullscreen and lists video modes. The engine has neither, and the window is not reachable from a layer.
9. **Raw key release edge per tick, and raw keys in replay.** Only actions get `TickWasReleased`, and only actions are recorded. This is usable as-is if the port goes all-actions.
10. **(Optional) Engine OGG and DDS support.** Both are avoidable game-side (10.B). Only needed if the other game also wants them.
11. **(Optional) ScreenOverlay per-quad blend (additive) and per-vertex colours.** One Penumbra gradient; workarounds exist.

### 10.B GAME-SIDE (no engine edit)

- **OGG:** vendor stb_vorbis, then `AudioEngine::AddClip(key, clip)` before `Play(key)`.
- **DDS:** decode the uncompressed A8R8G8B8 DDS (a 128-byte header, then BGRA) and pre-seed with `TextureRegistry::UploadRGBA("data:"+path, ...)`. Or convert offline in a converter (it must not write into `extracted/app`).
- **Particles:** port Ethanon's particle math. One pooled quad entity per particle, stepped on the tick with a seeded RNG (the MPR `sim/Particles` pattern).
- **Fonts:** pre-baked BMFont `.fnt` + PNG for "Arial Narrow", "Arial Black", "Verdana", "Arial" or substitutes, at the sizes used (15–40 px), with glyphs 0x20–0xFF. Lay out glyphs as ScreenOverlay quads with the game's own `shadowText`. Feed Latin-1 bytes; convert any UTF-8.
- **Renormalised normal maps:** load with the game's own stb pass, renormalise, then `UploadRGBA("data:"+path, ..., srgb=false)`.
- **Horizontal light weighting:** use `BlendMode::Alpha` (not `Premultiplied`) to get `(base+lit)*alpha`, matching `hPixelLight`'s `*diffuse.w`.
- **Emissive:** fold `min(1, ambient + emissive)` into `sprite2D.ambient`.
- **Scene ambient** (`SetAmbientLight`, 3 calls): the same CPU fold.
- **Draw order:** assign z per sprite (layer depth and y-sort), plus `sortKey` for ties. Use `alphaCutoff` on opaque quads for alpha-test entities.
- **Vertical-sprite depth tilt:** a quad rotated about X under the ortho camera, if z-buffer interpenetration matters. Lighting stays wrong until 10.A.2 lands.
- **Dynamic shadows (approximate):** a per-shadow `MeshRegistry::Upload/Replace` trapezoid plus `MeshComponent::meshKey`, drawn Alpha-blended in black at `shadowOpacity`. Or skip them.
- **4-corner `DrawRectangle` gradient:** a small gradient PNG stretched as a ScreenOverlay quad.
- **Global volume:** multiply per voice.
- **Timing:** `SimulationClock` ticks instead of `GetTime`. Use DetMath and a seeded RNG instead of libm and `rand`.
- **Two keyboard players:** distinct action names per player. `ClearBindings()` first. Set `flyControlsEnabled=false`.
- **Saves:** `UserDataDirectory("Penumbra")`. **Quit:** `Application::RequestQuit()`.
- **Paths:** use absolute data paths, because of the asset-root chdir.

---

## 11. Numbers worth remembering

| Item | Value | Where |
|---|---|---|
| 2D lights per frame | 64 | Light2D.hpp:19 |
| UV-transform slots per frame | 4095 | VulkanRenderer.hpp:277-289 |
| Draw instances per frame | 65,536 | VulkanRenderer.hpp:286 |
| Material descriptor sets | 1024 | `MaterialSets::kMaxSets` (ARCHITECTURE.md:576-580) |
| ScreenOverlay quads per frame | 4096 | ScreenOverlay.hpp:104 |
| WorldShapes vertices | 262,144 | WorldShapes.hpp:89 |
| Ticks per frame max | 5 | SupersonicApp.cpp:74 |
| Frame-delta clamp | 0.1 s | SupersonicApp.cpp:62 |
| Default tick | 1/60 s | SupersonicApp.cpp:72 |
| Stick deadzone | 0.18 | Input.hpp:553 |
| Voice volume / pitch clamp | [0,1] / [0.5,2] | AudioEngine.cpp:147-148 |
| UI reference height | 1080 | UICanvas.hpp:50 |
| Manifest default window | 1280×720 | GameRuntime.hpp:48-49 |
| Penumbra: .ogg / .mp3 | 19 / 16 | PEN:soundfx |
| Penumbra: referenced .dds | 8 (+1 unreferenced shadow.dds) | PEN |
| Penumbra: particle systems / pooled particles / max per system | 93 / 2064 / 70 | PEN:effects, entities |
| Penumbra: alphaMode 4 files | 14 (4 .par + 10 .ent) | PEN |
| Penumbra: type=2 (vertical) .ent / .esc overrides | 11 / 30 | PEN |
| Penumbra: castShadow=1 .ent / .esc | 11 / 82 | PEN |
| Penumbra: `<Gloss>` .ent | 12 | PEN |
| Penumbra: DrawText font names | Arial Narrow ×18, Arial Black ×4, Verdana ×2, Arial ×1 | PEN:*.as |

## Key facts

- Engine read at commit 4bfcf67 ('The games moved to their own repositories'); clean tree on 2026-09-27; Magic-Portals-Remake's engine/ submodule pins the same commit. The port should consume the engine as a submodule pinned to a commit, not the live Desktop checkout another session is editing.
- Game build: add_subdirectory(engine) gives SupersonicCore (static), Supersonic::TestHarness and supersonic_add_test(name), which builds <name>.cpp at /W4 and calls add_test (cmake/SupersonicTesting.cmake:26-47). Editor, plugin and engine suites are off in a subproject build. The game uses the engine's committed SPIR-V; the Shaders target is editor-only.
- main.cpp: GameManifest{isGame=true, title, startupScene.clear()} -> SupersonicApp app(options,&manifest) -> app.PushLayer(make_unique<Layer>) -> app.Run(); then check VulkanContext::ValidationErrorCount() (MPR game/main.cpp:357-431).
- EngineLayer hooks (EngineLayer.hpp): OnAttach/OnDetach, OnFixedUpdate(registry, fixedDelta) inside the tick loop after physics and SpriteAnimationSystem::Update, and OnUpdate(registry, frameDelta) per frame. There is no render hook: visuals are components, plus ScreenOverlay/WorldShapes filled in OnUpdate.
- Tick rate is authored in registry.ctx() SimulationClock::fixedDelta (default 1/60), with at most 5 ticks per frame and the frame delta clamped to 0.1 s (SupersonicApp.cpp:62-74, 1370-1540). ARCHITECTURE.md:2248 wrongly says OnFixedUpdate always gets kFixedPhysicsStep.
- CLI: --frames N, --scene, --screenshot PATH, --screenshot-every N, --fixed-step [s], --window WxH, --record PATH, --replay PATH (exclusive with --record), --import-assets, --help (LaunchOptions.cpp:29-50). AnchorAssetRoot changes the working directory to the engine root for a subproject game, so pass absolute paths for screenshots/replays and for all game data.
- Sprites are unlit, transparent MeshComponent 'Quad' entities (a 1x1 unit quad centred facing +Z) with MaterialComponent albedoTexturePath, albedoColor tint and blend Alpha/Additive/Premultiplied. Additive is (SrcAlpha, One), not (One, One). There is no Multiply blend.
- The ortho camera is CameraComponent::Projection::Orthographic with orthoHeight world units vertically, +Y up. flyControlsEnabled defaults to true and must be set false.
- Blended draw order: viewDepth back to front, then RenderableComponent::sortKey, then gather order (RenderSystem.cpp:158-176). In 2D a larger z draws later. Transparent draws do not write depth; opaque quads with alphaCutoff do.
- Textures load through stb_image 2.30 (PNG, JPEG incl. progressive, BMP incl. 8-bit paletted, TGA, GIF, PSD, HDR, PNM). There is no DDS. Mips are always generated, the filter is linear unless the .meta says Filter:nearest, and the sampler is always eRepeat. The cache key is 'srgb:'/'data:'+path; a DisplayEncoded scene uses 'data:'.
- RenderSettings in ctx: encoding=DisplayEncoded (8-bit display-value arithmetic, no bloom or tonemap), background=Color, bloomIntensity=0, optional Rgb565 quantize. This is what MP uses.
- MaterialComponent::sprite2D {enabled, ambient, height, lightMask, normalYDown, overlayStrength} plus overlayTexturePath (an additive lightmap). Base = clamp(texel*tint*ambient + overlay, 0, 1).
- Light2DComponent {color, intensity, range, height, layers, enabled}, at most 64 per frame, is a normal-mapped 2D point light: clamp(texel*tint*color*(1-d2/r2)*dot(L-P,N)/d, 0, 1) per light, with P=(fragX, fragY, constant sprite height). The normal is NOT renormalised, and there are no shadows or occluders.
- Penumbra's own Cg (hPixelLight/vPixelLight/pixelLightVS): same attenuation and facing, but it renormalises the normal, weights light by texel alpha for horizontal sprites, and uses P varying along the sprite height plus an xzy normal swizzle for VERTICAL (type=2) entities. It also has specular (gloss maps) and extruded dynamic sprite shadows (dynaShadowVS). None of the vertical, specular or shadow features exist in the engine.
- SpriteAnimationComponent {columns, rows, firstFrame, frameCount, framesPerSecond, loop, playing; frame, elapsed} advances on the tick, is hashed, and is applied as uvScale/uvOffset. It takes one of 4095 UV slots per frame.
- The engine's ParticleEmitterComponent/ParticleSystem draws untextured cubes with a hard-coded gravity of 1.5, runs on the frame delta and a static mt19937(12345), and is not hashed: unusable for .par. MP simulates particles in game code, with one pooled quad entity per particle.
- BitmapFont reads BMFont TEXT .fnt only, rasterises nothing and ignores kerning. It maps each BYTE to a code point, so Latin-1 strings work (ã=0xE3, ç=0xE7) and UTF-8 does not. UITextComponent draws via ImGui with the embedded Inter font, UTF-8, and has no font-family field.
- Penumbra's DrawText names system TTFs: Arial Narrow x18, Arial Black x4, Verdana x2, Arial x1, sizes 15-40 px. The extracted game ships no .fnt.
- ScreenOverlay: immediate display-value quads after the tonemap, in fractions of the image with +y down, uv sub-rect, colour multiply, texture path, 2x2 basis rotation. Mix blend only, at most 4096 per frame, emitted in OnUpdate. MP draws its HUD and BitmapFont glyphs through it.
- Audio is XAudio2 with no voice cap. AudioClip::Load handles .wav and .mp3 only (MP3 via Windows Media Foundation) and refuses .ogg. AudioEngine::Play(path, loop, volume, pitch), Stop, SetVoiceParameters(volume [0,1], pitch [0.5,2], pan), IsVoicePlaying, AddClip(name, AudioClip). Finished voices are reaped each frame. There is no master volume and no streaming; clips are decoded whole on first use.
- Input: all GLFW keys are polled. IsKeyDown/WasKeyPressed are per frame, with no raw release edge. Named actions have IsDown and the tick-latched TickWasPressed/TickWasReleased; only actions, axes, mouse and UI clicks are recorded or replayed. The gamepad is only GLFW_JOYSTICK_1 as a GLFW gamepad. LoadDefaultBindings runs at startup (WASD/arrows/Space...); call ClearBindings().
- StateHash::Compute(registry) and StateHash::RegisterContributor(name, fn(registry, Mixer&)) cover game-owned state. Use DetMath for trig inside the tick. Penumbra's 59 GetTime() calls must become SimulationClock ticks.
- Machine: VS Build Tools 18 vcvars64 at C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat; CMake 3.29.2 and Ninja from C:\Strawberry\c\bin (force -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl); Vulkan SDK C:\VulkanSDK\1.4.357.0 with GLSL_COMPILER=.../Bin/glslc.exe; configure Ninja, Release, SUPERSONIC_ENABLE_VALIDATION=ON. Smart App Control is enforcing: fresh exes are randomly blocked and each block notifies Ivan, so launch once and never loop.
- Other useful APIs: Supersonic::Application::RequestQuit(); Supersonic::UserDataDirectory(title) for saves; MeshRegistry::Upload/Replace plus MeshComponent::meshKey for game-built geometry (text meshes, shadow trapezoids); TextureRegistry::UploadRGBA to pre-seed decoded images (e.g. DDS) under the 'data:'+path key.

## Engine gaps

- ENGINE CHANGE: no Multiply/Modulate blend (GL_ZERO, GL_SRC_COLOR). BlendEquation has only Mix/Add/Premultiplied (VulkanPipeline.hpp:302). Penumbra has alphaMode="4" in 4 .par (shadow_beam, shadow_beam_wide, fade_out_shadow_beam, sword_beam) and 10 .ent (bruxo, bruxo_dead, checkpoint, fade_out_beam, master_knight, minion, princess, sword_beam, vert_bruxo, vert_master_knight).
- ENGINE CHANGE: no lighting for Ethanon VERTICAL entities (type=2: 11 .ent, 30 .esc overrides). The 2010 Cg uses a per-pixel 3D position whose height varies up the sprite plus an xzy normal swizzle with z negated; shadeSprite2D uses a constant height per draw.
- ENGINE CHANGE (or game-side texel pre-pass): the Light2D normal is not renormalised, while 2010 Ethanon's Cg renormalises (-normalize(2n-1)).
- ENGINE CHANGE: no specular/gloss term in 2D lighting (Ethanon mainSpecular with a gloss map, specularPower, specularBrightness, fakeEyePos). 12 .ent files carry <Gloss>.
- ENGINE CHANGE (or approximate game-side with per-frame meshes): no projected dynamic sprite shadows (dynaShadowVS.cg; castShadow in 11 .ent and 82 .esc instances; shadowScale/shadowLengthScale/shadowOpacity/shadowZ). Light2D has no occluders or shadows at all.
- ENGINE CHANGE: all textures sample with eRepeat (VulkanImage.hpp:82) and there is no per-texture clamp-to-edge, so linear-filtered sprite borders can bleed from the opposite edge.
- ENGINE CHANGE: only one gamepad (GLFW_JOYSTICK_1, GLFW gamepad mapping only) is polled (InputPolling.cpp:208-219); no second pad, no raw non-gamepad joystick buttons/axes (Ethanon JK_01..JK_10, GetJoystickXY, two joysticks in playerInput.as).
- ENGINE CHANGE: no fullscreen, no runtime window resize and no video-mode enumeration, and Window is not reachable from a layer. Penumbra uses SetWindowProperties (windowed/fullscreen, 1024x768 default) and GetVideoMode/GetVideoModeCount.
- ENGINE LIMIT: raw IsKeyDown/WasKeyPressed are per frame, have no release edge, and are not recorded or replayed. Only named actions get TickWasPressed/TickWasReleased and replay coverage.
- GAME-SIDE (engine lacks it): no OGG Vorbis decoding; AudioClip::Load accepts only .wav/.mp3 (AudioClip.cpp:85-91), and Penumbra has 19 .ogg. Workaround: vendor stb_vorbis in the port and call AudioEngine::AddClip(path, clip) before Play(path).
- GAME-SIDE (engine lacks it): no DDS loader (stb_image only). 8 referenced DDS files (thorn, dirt, tree01/02/03/05_alpha, black_sword, fog), all uncompressed A8R8G8B8. Workaround: decode and pre-seed with TextureRegistry::UploadRGBA('data:'+path), or convert offline.
- GAME-SIDE (engine particle system unusable): ParticleEmitterComponent draws untextured cubes, hard-codes gravity, runs on the frame delta with a static RNG, and is unhashed. 93 Ethanon particle systems (2064 pooled particles) must be simulated in game code as pooled quad entities (the MPR pattern).
- GAME-SIDE (engine lacks it): no TrueType text for the game. BitmapFont reads pre-baked BMFont text .fnt only, with no kerning and byte-per-glyph (Latin-1 works, UTF-8 does not); UITextComponent is Inter-only via ImGui. Penumbra needs Arial Narrow/Arial Black/Verdana/Arial at 15-40 px, so .fnt files must be pre-baked.
- MINOR: ScreenOverlay is Mix-blend only with one colour per quad (no additive HUD, no 4-corner gradient DrawRectangle); the vertex colour in the scene path is RGB only (no per-vertex alpha).
- MINOR: no master/global audio volume (per voice only); no streaming (fase.mp3 is about 26 MB of PCM, decoded whole on first Play, so preload); MP3 loop points go through Media Foundation, whose encoder-delay handling is unverified.
- MINOR: 4095 UV-transform slots per frame (every flipbooked or scrolled sprite or particle takes one) and 64 Light2D per frame. Both are fine for Penumbra's counts but are hard caps.

## Open questions

- Does Penumbra's 2010 Ethanon use the same ALPHA_MODE enum as the 2013 source MPR decoded (0 PIXEL, 1 ADD, 2 ALPHA_TEST, 3 NONE, 4 MODULATE) for .ent blendMode and .par alphaMode? blendMode="2" (76 .ent) and alphaMode="4" (14 files) change meaning otherwise. No 2010 D3D9 gs2d source is in the tree to confirm.
- Does the 2010 ENTITY_TYPE enum match the 2013 one (0 HORIZONTAL, 1 GROUND_DECAL, 2 VERTICAL, 3 OVERALL, 4 OPAQUE_DECAL, 5 LAYERABLE, per MPR docs/ethanon-formats.md:2398)? Penumbra uses types 0, 2 and 5.
- What D3D9 blend state did 2010 Ethanon use for the per-pixel light pass (One,One vs SrcAlpha,One) and for AM_ADD? It decides whether horizontal sprites should use engine BlendMode::Alpha (matches hPixelLight's *diffuse.w under One,One) or something else, and what vPixelLight's missing *diffuse.w implies.
- Should vertical-entity lighting, specular, modulate blend, clamp sampling, dual pads and fullscreen become engine features, which means coordinating with the other session editing Supersonic and recompiling committed SPIR-V? Or should the port accept approximations or drop them?
- Fonts: is it acceptable to pre-bake BMFont atlases from Microsoft's Arial Narrow/Arial Black/Verdana (licensing), or should metric-compatible free substitutes be used (e.g. Liberation Sans Narrow for Arial Narrow; there is no exact free Arial Black clone)?
- Dynamic sprite shadows: is a per-frame MeshRegistry::Replace trapezoid per shadow-casting entity per light affordable (82 .esc instances x lights)? Not measured. Or should shadows be approximated with affine skewed quads or the unreferenced data/shadow.dds blob?
- Does the port need --record/--replay determinism? If yes, all input must go through named actions read inside OnFixedUpdate, because raw IsKeyDown/WasKeyPressed are neither tick-latched nor replayed.
- Will GLFW's gamepad mapping recognise the controllers Ivan wants to use (XInput pads yes; older DirectInput joysticks only if listed in the SDL controller DB)? Penumbra's original assumed DirectInput button numbering JK_01..JK_10.
- Is gapless looping of MP3 music (menu.mp3, fase.mp3, chefao.mp3) through Media Foundation clean, given the encoder delay/padding? Not tested.
- Should sprites be mip-mapped and linearly filtered (the engine always generates mips) or drawn nearest at 1:1? The .meta-based nearest switch requires sidecar files next to the textures, which must live outside the read-only extracted/app.
