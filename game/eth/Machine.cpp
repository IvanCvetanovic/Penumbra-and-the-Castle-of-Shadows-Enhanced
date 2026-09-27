// The Ethanon 0.7.12 machine: the frame (ETHEngine::StartEngine,
// reference/eth-0.7.12/src/ETHEngine.cpp:565-623), the deferred scene load
// (:823-912), the render-time bookkeeping of ETHScene::RenderScene
// (ETHScene.cpp:601-643, :758-965, :1091-1132) turned into a RenderSnapshot,
// and the callback pass (ETHScene.cpp:972-1019).

#include "eth/Machine.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <system_error>

#include "core/Log.hpp"
#include "eth/ImageInfo.hpp"

namespace Penumbra::Eth {

namespace {

Machine* g_current = nullptr;

// One Ethanon frame per 60 Hz tick.
constexpr float kFrameSeconds = 1.0f / 60.0f;
// _ETH_EMPTY_SCENE_STRING (ETHScene.h:54): LoadScene("") loads nothing.
const char* const kEmptyScene = "empty";
constexpr const char* kCallbackPrefix = "ETHCallback_";

string FileName(const string& path) {
    const auto slash = path.find_last_of("/\\");
    return slash == string::npos ? path : path.substr(slash + 1);
}

string ForwardSlashes(string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

// The callback registry is keyed by what follows "ETHCallback_".
string CallbackKey(const string& name) {
    const string prefix = kCallbackPrefix;
    return name.compare(0, prefix.size(), prefix) == 0 ? name.substr(prefix.size()) : name;
}

bool IsAbsolutePath(const string& path) {
    return std::filesystem::path(path).is_absolute() || (!path.empty() && (path[0] == '/' || path[0] == '\\'));
}

// `path` under `root` (slashes of either kind, case-insensitive as Windows
// paths are): the part after the root.
bool UnderRoot(const string& path, const string& root, string& relative) {
    if (root.empty()) return false;
    string p = ForwardSlashes(path);
    string r = ForwardSlashes(root);
    while (!r.empty() && r.back() == '/') r.pop_back();
    if (p.size() <= r.size() + 1 || p[r.size()] != '/') return false;
    for (std::size_t i = 0; i < r.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(p[i])) != std::tolower(static_cast<unsigned char>(r[i]))) {
            return false;
        }
    }
    relative = p.substr(r.size() + 1);
    return true;
}

vector2 ToScreenPos(const vector3& pos, const vector2& zAxisDirection) {
    // ETHGlobal::ToScreenPos (ETHCommon.h:423-426).
    return vector2(pos.x, pos.y) + zAxisDirection * pos.z;
}

// ETHScene::IsSphereInScreen (ETHScene.cpp:1077-1089): the y is always y - z,
// whatever the scene's z axis.
bool IsSphereInScreen(const vector3& pos, const float radius, const vector2& camera, const vector2& screen) {
    const vector2 p(pos.x, pos.y - pos.z);
    if (p.x < camera.x - radius || p.x > camera.x + screen.x + radius) return false;
    if (p.y < camera.y - radius || p.y > camera.y + screen.y + radius) return false;
    return true;
}

std::shared_ptr<SampleBank> NewSfxToken(SampleBank& samples) {
    // An aliasing pointer: it owns a fresh control block and points at the
    // bank, so every weak copy expires when the next load (or the machine's
    // end) replaces it, while the bank itself lives on.
    return std::shared_ptr<SampleBank>(std::make_shared<char>('\0'), &samples);
}

} // namespace

// --- Life -----------------------------------------------------------------------

Machine::Machine(MachineConfig config)
    : m_config(std::move(config)), m_samples(m_config.gameRoot), m_rng(m_config.seed) {
    m_screenSize = m_config.screenSize;
    m_sfxBank = NewSfxToken(m_samples);
}

Machine::~Machine() {
    // The scene first, while the bank its entities point at still exists.
    m_staticCallbacks.clear();
    m_scene.reset();
    m_sfxBank.reset();
    if (g_current == this) g_current = nullptr;
}

Machine& Machine::Current() {
    if (g_current == nullptr) throw std::logic_error("Penumbra::Eth: no current Machine (use Machine::Scope)");
    return *g_current;
}

Machine* Machine::CurrentOrNull() { return g_current; }

Machine::Scope::Scope(Machine& machine) : m_previous(g_current) { g_current = &machine; }

Machine::Scope::~Scope() { g_current = m_previous; }

// --- Registration -------------------------------------------------------------------

void Machine::RegisterFunction(const string& name, ScriptFunction fn) { m_functions[name] = std::move(fn); }

void Machine::RegisterCallback(const string& name, CallbackFunction fn) {
    m_callbacks[CallbackKey(name)] = std::move(fn);
}

bool Machine::HasCallback(const string& name) const { return m_callbacks.count(CallbackKey(name)) != 0; }

void Machine::RunGuarded(const string& where, const std::function<void()>& fn) {
    // AngelScript's failure mode: an exception aborts the running function and
    // the engine carries on with the next (ETHScene.cpp:1881-1906).
    try {
        fn();
    } catch (const ScriptException& e) {
        ++m_scriptAborts;
        if (m_loggedAborts.insert(where + ": " + e.what()).second) {
            SUPERSONIC_LOG_WARN("Penumbra") << "script exception in " << where << ": " << e.what()
                                            << " (the function was aborted; logged once)";
        }
    }
}

bool Machine::RunCallback(const string& name, ETHEntity entity) {
    const auto it = m_callbacks.find(CallbackKey(name));
    if (it == m_callbacks.end()) return false;
    const CallbackFunction fn = it->second;
    RunGuarded(kCallbackPrefix + CallbackKey(name), [&fn, &entity] { fn(entity); });
    return true;
}

void Machine::Boot(const ScriptFunction& main) {
    RunGuarded("main", main);
}

// --- The frame -----------------------------------------------------------------------

void Machine::Frame(const InputFrame& input) {
    // GetTime: whole milliseconds of simulated time, from the frame count so
    // that no rounding accumulates; uint32, never reset (wraps as GetTickCount).
    ++m_frameIndex;
    m_timeMs = static_cast<uint>(static_cast<std::uint64_t>(m_frameIndex) * 1000u / 60u);

    // 1. input (ETHEngine.cpp:575)
    m_input.Update(input);

    // 2. loop (:578-579)
    if (!m_loopFunction.empty()) {
        const auto it = m_functions.find(m_loopFunction);
        if (it != m_functions.end()) {
            const ScriptFunction loop = it->second;
            RunGuarded(m_loopFunction, loop);
        }
    }

    // 3. a pending load (:582-586). The request is reset AFTER the load, so one
    // made by the new scene's preLoop is dropped, as in 0.7.12.
    if (m_pendingLoad) {
        const PendingLoad request = *m_pendingLoad;
        DoLoad(request);
        m_pendingLoad.reset();
    }

    // 4. timer (:589): LoadScene had just called CalcLastFrame (:849), so the
    // frame of a load measures ~0.
    m_frameSeconds = m_justLoaded ? 0.0f : kFrameSeconds;
    m_justLoaded = false;

    // 5. render (:592-617) and 6. callbacks (:620)
    Render();
    RunCallbacks();
}

void Machine::DoLoad(const PendingLoad& request) {
    // ETHEngine::LoadScene (ETHEngine.cpp:823-851).
    m_screenSize = m_config.screenSizeForScene ? m_config.screenSizeForScene(request.file) : m_config.screenSize;

    // ResetScene, then both resource managers released (:825-827): every
    // sample stops, every sprite is forgotten.
    m_staticCallbacks.clear();
    m_scene.reset();
    m_samples.ReleaseAll();
    m_sprites.clear();
    m_sfxBank = NewSfxToken(m_samples);
    m_scene = std::make_unique<Scene>(*this, request.file, request.bucketSize);

    if (request.file != kEmptyScene && !request.file.empty()) {
        const string path = ReadPath(request.file);
        const std::optional<SceneFile> file = ReadSceneFile(path);
        if (!file) {
            // :835-839 returns before the name, camera, background and scripts:
            // the new scene stays empty and the old loop keeps running.
            SUPERSONIC_LOG_ERROR("Penumbra") << "Couldn't load the scene " << request.file << " (" << path << ")";
            return;
        }
        m_scene->Populate(*file);
    }

    m_sceneFileName = request.file;
    m_camera = vector2(0.0f);
    m_backgroundImage.clear();
    m_backgroundAdditive = false;

    // LoadSceneScripts (:885-912): preLoop now; the loop from the next frame.
    // An empty loop name leaves the old loop installed; an unknown one clears it.
    if (!request.onLoad.empty()) {
        const auto it = m_functions.find(request.onLoad);
        if (it != m_functions.end()) {
            const ScriptFunction preLoop = it->second;
            RunGuarded(request.onLoad, preLoop);
        } else {
            SUPERSONIC_LOG_WARN("Penumbra") << "LoadScene: no function " << request.onLoad;
        }
    }
    if (!request.onLoop.empty()) {
        if (m_functions.count(request.onLoop) != 0) {
            m_loopFunction = request.onLoop;
        } else {
            SUPERSONIC_LOG_WARN("Penumbra") << "LoadScene: no function " << request.onLoop;
            m_loopFunction.clear();
        }
    }
    m_justLoaded = true;
}

void Machine::UpdateParticles(Entity& entity, const uint slot) {
    ParticleManager* manager = entity.ParticleSlot(slot);
    if (manager == nullptr || slot >= entity.m_particleClock.size()) return;
    if (entity.m_particleUpdatedAt[slot] == m_frameIndex) return;

    // Each manager's own timer (ETHParticleManager.cpp:652-654): the frames
    // since its creation or last Update, capped at 2.
    const uint since = m_frameIndex - entity.m_particleClock[slot];
    const float frameSpeed = std::min(static_cast<float>(since), 2.0f);
    entity.m_particleClock[slot] = m_frameIndex;
    entity.m_particleUpdatedAt[slot] = m_frameIndex;

    const vector2 zAxis = m_scene ? m_scene->Properties().zAxisDirection : vector2(0.0f);
    const vector3 pos = entity.GetPosition();
    const vector2 pos2 = ToScreenPos(pos, zAxis);
    manager->Update(pos2, pos, entity.GetAngle(), GetTimeF(), frameSpeed, m_rng);

    // HandleSoundPlayback ran at the end of every Update whose sample loaded.
    const string& sound = manager->System().soundEffect;
    if (manager->HasSoundEffect() && m_samples.SampleExists(sound)) {
        const SoundDecision decision = manager->SoundPlayback(pos2, m_camera, m_screenSize, frameSpeed);
        if (decision.touch) m_samples.ApplyEffect(sound, decision.stop, decision.loop, decision.volume, decision.pan);
    }
}

void Machine::Render() {
    RenderSnapshot& snap = m_snapshot;
    snap = RenderSnapshot{};
    snap.frameIndex = m_frameIndex;
    snap.timeMs = m_timeMs;
    snap.sceneFile = m_sceneFileName;
    snap.camera = m_camera;
    snap.screenSize = m_screenSize;
    snap.pixelShaders = m_pixelShaders;
    snap.roundUp = m_roundUp;
    snap.backgroundColor = m_backgroundColor;
    if (!m_backgroundImage.empty()) {
        if (const SpriteResource* bg = FindSpriteResource(m_backgroundImage)) {
            snap.backgroundImage = bg->path;
            snap.backgroundMin = m_backgroundMin;
            snap.backgroundMax = m_backgroundMax;
            snap.backgroundAdditive = m_backgroundAdditive;
        }
    }
    snap.cursorHidden = m_cursorHidden;
    m_staticCallbacks.clear();

    if (Scene* scene = m_scene.get()) {
        const SceneProperties& props = scene->Properties();
        snap.ambient = props.ambient;
        snap.lightIntensity = props.lightIntensity;
        snap.zAxisDirection = props.zAxisDirection;

        // a. CheckTemporaryEntities (ETHScene.cpp:445-498): the every-frame
        // list's particles advance, visible or not, and sprite-less entities
        // whose systems are all over are removed.
        auto& everyFrame = scene->DynamicOrTemporary();
        for (std::size_t i = 0; i < everyFrame.size();) {
            const std::shared_ptr<Entity> entity = everyFrame[i];
            const EntityDef& def = entity->Def();
            for (uint t = 0; t < 2 && t < def.particles.size(); ++t) {
                if (def.particles[t].nParticles > 0) UpdateParticles(*entity, t);
            }
            if (!entity->HasSpriteImage() && entity->AreParticlesOver()) {
                if (scene->RemoveFinishedTemporary(*entity) < 0) {
                    ++i;   // no bucket at its position: it stays listed (:466-470)
                    continue;
                }
                everyFrame.erase(everyFrame.begin() + static_cast<std::ptrdiff_t>(i));
                continue;
            }
            ++i;
        }

        // b. RenderList (ETHScene.cpp:758-965). The depth range grows as the
        // visible entities are walked and the drawHash of each uses the range
        // so far; the ambient pass itself drew with the range the frame began
        // with (DrawAmbientPass got m_maxSceneHeight/m_minSceneHeight, :877),
        // and so did the particles (:1122).
        const float frameMax = scene->MaxHeight();
        const float frameMin = scene->MinHeight();
        float maxHeight = frameMax;
        float minHeight = frameMin;
        struct Drawn {
            float drawHash;
            std::shared_ptr<Entity> entity;
        };
        std::vector<Drawn> drawn;
        for (const Scene::BucketKey& key : scene->VisibleBuckets(m_camera, m_screenSize)) {
            const Scene::EntityList* bucket = scene->Bucket(key);
            if (bucket == nullptr) continue;
            for (const auto& entity : *bucket) {
                maxHeight = std::max(maxHeight, entity->MaxHeight());
                minHeight = std::min(minHeight, entity->MinHeight());
                if (entity->IsHidden()) continue;

                const EntityDef& def = entity->Def();
                if (def.light.active) {
                    LightDraw light;
                    light.ownerId = entity->GetID();
                    light.position = entity->GetPosition() + def.light.position;
                    light.color = def.light.color;
                    light.range = def.light.range;
                    light.isStatic = def.light.isStatic;
                    light.castShadows = def.light.castShadows;
                    light.haloBrightness = def.light.haloBrightness;
                    light.haloSize = def.light.haloSize;
                    light.haloBitmap = entity->m_haloLoaded ? def.light.haloBitmap : string();
                    const ParticleManager* slot0 = entity->ParticleSlot(0);
                    if (slot0 != nullptr && slot0->NumParticles() > 0) {
                        light.particleRatio = static_cast<float>(slot0->NumActiveParticles()) /
                                              static_cast<float>(slot0->NumParticles());
                    }
                    snap.lights.push_back(light);
                }

                const float depth = entity->ComputeDepth(maxHeight, minHeight);
                float drawHash = depth;
                switch (entity->GetType()) {
                case ET_HORIZONTAL:
                    drawHash = depth / 2.0f;
                    break;
                case ET_VERTICAL:
                    drawHash = (0.5f + depth) + (entity->GetPosition().y - m_camera.y);
                    break;
                case ET_GROUND_DECAL:
                case ET_OPAQUE_DECAL:
                    drawHash = depth / 2.0f + 0.01f;
                    break;
                case ET_OVERALL:
                case ET_LAYERABLE:
                    drawHash = depth;
                    break;
                }
                drawn.push_back({drawHash, entity});
            }
        }
        // A multimap on drawHash: equal keys keep their insertion order.
        std::stable_sort(drawn.begin(), drawn.end(),
                         [](const Drawn& a, const Drawn& b) { return a.drawHash < b.drawHash; });

        std::vector<std::shared_ptr<Entity>> withParticles;
        for (const Drawn& item : drawn) {
            const Entity& e = *item.entity;
            const EntityDef& def = e.Def();

            SpriteDraw sprite;
            sprite.entityId = e.GetID();
            sprite.entityName = e.GetEntityName();
            if (e.HasSpriteImage()) {
                sprite.sprite = def.sprite;
                sprite.normal = def.normal;
                sprite.gloss = def.gloss;
            }
            sprite.type = def.type;
            sprite.blendMode = def.blendMode;
            sprite.isStatic = def.isStatic;
            sprite.applyLight = def.applyLight;
            sprite.castShadow = def.castShadow;
            sprite.shadowScale = def.shadowScale;
            sprite.shadowLengthScale = def.shadowLengthScale;
            sprite.shadowOpacity = def.shadowOpacity;
            sprite.specularPower = def.specularPower;
            sprite.specularBrightness = def.specularBrightness;
            sprite.emissive = def.emissiveColor;
            sprite.color = e.Color();
            sprite.position = e.GetPosition();
            sprite.size = e.GetSize();
            sprite.bitmapSize = e.m_bitmapSize;
            sprite.spriteCutX = def.spriteCutX;
            sprite.spriteCutY = def.spriteCutY;
            sprite.frame = e.GetFrame();
            // Vertical entities never rotate (ETHRenderEntity.cpp:705-706).
            sprite.angle = def.type == ET_VERTICAL ? 0.0f : e.GetAngle();
            sprite.depth = e.ComputeDepth(frameMax, frameMin);
            sprite.drawHash = item.drawHash;
            sprite.layerDepth = def.layerDepth;
            // The sprite's top-left: the POSITION is floored when round-up is
            // on (gs2dD3D9Sprite.cpp:568-576), then the origin - centre, or
            // bottom-centre for verticals, plus pivotAdjust - is subtracted
            // (ETHRenderEntity.cpp:604-640). GS2D's extra -0.5 is left out: it
            // moved D3D9's integer pixel centres to where a modern API's
            // half-integer ones already are, so the geometry is the same.
            vector2 at = ToScreenPos(e.GetPosition(), props.zAxisDirection);
            if (m_roundUp) at = vector2(std::floor(at.x), std::floor(at.y));
            const vector2 centre = def.type == ET_VERTICAL ? vector2(sprite.size.x / 2.0f, sprite.size.y)
                                                           : sprite.size / 2.0f;
            sprite.origin = at - (centre + def.pivotAdjust);
            snap.sprites.push_back(sprite);

            for (uint t = 0; t < 2 && t < def.particles.size(); ++t) {
                if (def.particles[t].nParticles > 0) {
                    withParticles.push_back(item.entity);
                    break;
                }
            }
            if (e.IsStatic() && e.m_hasCallback && !e.IsTemporary()) m_staticCallbacks.push_back(item.entity);
        }

        // c. RenderParticleList (ETHScene.cpp:1091-1132): a system off screen
        // advances only if it is finite, one on screen only if it is endless -
        // so a finite system of a visible static entity outside the every-frame
        // list stands still, as it did.
        for (const auto& entity : withParticles) {
            const EntityDef& def = entity->Def();
            for (uint t = 0; t < 2 && t < def.particles.size(); ++t) {
                const ParticleSystemDef& system = def.particles[t];
                if (system.nParticles <= 0) continue;
                const vector3 emitter = entity->GetPosition() + system.startPoint;
                if (!IsSphereInScreen(emitter, system.boundingSphere, m_camera, m_screenSize)) {
                    if (system.repeat > 0) UpdateParticles(*entity, t);
                    continue;
                }
                if (system.repeat <= 0) UpdateParticles(*entity, t);
                if (const ParticleManager* manager = entity->ParticleSlot(t)) {
                    manager->CollectDraws(entity->GetID(), props.ambient, frameMin, frameMax, entity->GetType(),
                                          props.zAxisDirection, entity->ComputeDepth(frameMax, frameMin),
                                          snap.particles);
                }
            }
        }
        // gsSeed(elapsed ms) after any frame that had particle entities in view
        // (ETHScene.cpp:1129-1130): the scripts' rand() is reseeded from time.
        if (!withParticles.empty()) m_rng.Seed(m_timeMs);

        // RenderScene keeps the grown range for the next frame (:640-641).
        scene->SetHeightRange(minHeight, maxHeight);
    }

    // 9. DrawTopLayer (ETHEngine.cpp:645-672): everything queued since the last
    // one, in submission order, once. A sprite is looked up by basename NOW,
    // so one queued before a load must have been reloaded by then.
    for (HudCmd cmd : m_hudQueue) {
        if (cmd.kind == HudCmd::Kind::Sprite || cmd.kind == HudCmd::Kind::ShapedSprite) {
            const SpriteResource* resource = FindSpriteResource(cmd.sprite);
            if (resource == nullptr) continue;   // not loaded: nothing drawn (ETHPrimitiveDrawer.cpp:158-168)
            cmd.sprite = resource->path;
            if (cmd.kind == HudCmd::Kind::Sprite || cmd.size == vector2(0.0f)) cmd.size = resource->size;
            cmd.spriteRectMin = vector2(0.0f);
            cmd.spriteRectMax = resource->size;
        }
        snap.hud.push_back(std::move(cmd));
    }
    m_hudQueue.clear();
}

void Machine::RunCallbacks() {
    // RunCallbacksFromList (ETHScene.cpp:972-1019).
    std::vector<std::shared_ptr<Entity>> statics;
    statics.swap(m_staticCallbacks);
    Scene* scene = m_scene.get();
    if (scene == nullptr) return;

    // The every-frame list, in insertion order. A callback may AddEntity (the
    // list grows and the newcomer runs this very pass) or DeleteEntity (a dead
    // entry is dropped when reached), so walk by index and hold a reference.
    auto& everyFrame = scene->DynamicOrTemporary();
    for (std::size_t i = 0; i < everyFrame.size();) {
        const std::shared_ptr<Entity> entity = everyFrame[i];
        if (!entity->IsAlive()) {
            everyFrame.erase(everyFrame.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        if (entity->m_hasCallback) RunCallback(entity->m_callbackName, ETHEntity(entity));
        ++i;
    }

    // Then the static entities drawn this frame, in draw order.
    for (const auto& entity : statics) {
        if (!entity->IsAlive()) continue;
        if (entity->m_hasCallback) RunCallback(entity->m_callbackName, ETHEntity(entity));
    }
}

// --- Scenes -----------------------------------------------------------------------

void Machine::LoadScene(const string& file) {
    // LoadSceneInScript (ETHEngine.cpp:853-859): Reset() first.
    PendingLoad request;
    request.file = file.empty() ? string(kEmptyScene) : file;
    m_pendingLoad = request;
}

void Machine::LoadScene(const string& file, const string& onLoad, const string& onLoop) {
    // :861-870: the bucket size of a request already made this frame stays.
    if (!m_pendingLoad) m_pendingLoad = PendingLoad{};
    m_pendingLoad->file = file.empty() ? string(kEmptyScene) : file;
    m_pendingLoad->onLoad = onLoad;
    m_pendingLoad->onLoop = onLoop;
}

void Machine::LoadScene(const string& file, const string& onLoad, const string& onLoop, const vector2& bucketSize) {
    LoadScene(file, onLoad, onLoop);
    m_pendingLoad->bucketSize = bucketSize;
}

bool Machine::SaveScene(const string& file) {
    // ETHScene::SaveToFile (ETHScene.cpp:182-221): the live state of every
    // linked entity, and the scene properties as they are now.
    if (!m_scene) return false;
    const std::vector<std::shared_ptr<Entity>> entities = m_scene->AllEntities();
    if (entities.empty()) {
        SUPERSONIC_LOG_WARN("Penumbra") << "ETHScene::Save: there are no entities to save";
        return false;
    }
    SceneFile scene;
    scene.properties = m_scene->Properties();
    scene.entities.reserve(entities.size());
    for (const auto& entity : entities) {
        ScenePlacement placement;
        placement.id = entity->GetID();
        placement.spriteFrame = entity->GetFrame();
        placement.entityName = entity->GetEntityName();
        placement.color = entity->Color();
        placement.position = entity->GetPosition();
        placement.angle = entity->GetAngle();
        placement.def = entity->Def();
        scene.entities.push_back(std::move(placement));
    }
    const string path = WritePath(file);
    if (path.empty()) {
        SUPERSONIC_LOG_WARN("Penumbra") << "SaveScene: no user directory, " << file << " not written";
        return false;
    }
    const string text = WriteSceneFile(scene);
    std::ofstream out(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
    if (!out) {
        SUPERSONIC_LOG_WARN("Penumbra") << "SaveScene: cannot write " << path;
        return false;
    }
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(out);
}

string Machine::GetSceneFileName() const { return m_sceneFileName; }

// --- Entities -----------------------------------------------------------------------

int Machine::AddEntity(const string& file, const vector3& pos, const float angle) {
    // ETHEngine::AddEntity (ETHEngine.cpp:674-792): entities/<file>, read
    // (here: cached) on every call, named by its basename WITH ".ent".
    if (!m_scene) return -1;
    const EntityDef* def = EntityDefinition(file);
    if (def == nullptr) {
        SUPERSONIC_LOG_WARN("Penumbra") << "AddEntity - Entity not found: " << file;
        return -1;
    }
    return m_scene->Add(FileName(file), *def, pos, angle)->GetID();
}

int Machine::AddEntity(const string& file, const vector3& pos, ETHEntity& out) {
    // AngelScript hands an @&out a null temporary: a failed call leaves null.
    out = nullptr;
    if (!m_scene) return -1;
    const EntityDef* def = EntityDefinition(file);
    if (def == nullptr) {
        SUPERSONIC_LOG_WARN("Penumbra") << "AddEntity - Entity not found: " << file;
        return -1;
    }
    std::shared_ptr<Entity> entity = m_scene->Add(FileName(file), *def, pos, 0.0f);
    const int id = entity->GetID();
    out = ETHEntity(std::move(entity));
    return id;
}

int Machine::AddEntity(const string& file, const vector3& pos, const string& alternativeName) {
    // The callback binds to the ALTERNATIVE name: ChangeEntityName precedes
    // AssignCallbackScript (ETHScene.cpp:388-418).
    if (!m_scene) return -1;
    const EntityDef* def = EntityDefinition(file);
    if (def == nullptr) {
        SUPERSONIC_LOG_WARN("Penumbra") << "AddEntity - Entity not found: " << file;
        return -1;
    }
    return m_scene->Add(alternativeName, *def, pos, 0.0f)->GetID();
}

ETHEntity Machine::DeleteEntity(ETHEntity entity) {
    // ETHEngine.cpp:794-804: null on success, the handle when not found.
    if (entity == nullptr) return nullptr;
    if (m_scene && m_scene->Delete(*entity)) return nullptr;
    return entity;
}

ETHEntity Machine::SeekEntity(const int id) const {
    // ETHEngine.cpp:229-246: the first match, and never a temporary.
    if (!m_scene) return nullptr;
    std::shared_ptr<Entity> entity = m_scene->SeekEntity(id);
    if (!entity || entity->IsTemporary()) return nullptr;
    return ETHEntity(std::move(entity));
}

ETHEntity Machine::SeekEntity(const string& name) const {
    if (!m_scene) return nullptr;
    std::shared_ptr<Entity> entity = m_scene->SeekEntity(name);
    if (!entity || entity->IsTemporary()) return nullptr;
    return ETHEntity(std::move(entity));
}

bool Machine::GetEntityArray(const string& name, ETHEntityArray& out) const {
    return m_scene && m_scene->GetEntityArray(name, out);
}

bool Machine::GetEntitiesFromBucket(const vector2& bucket, ETHEntityArray& out) const {
    Scene::BucketKey key;
    if (!m_scene || !Scene::ToKey(bucket, key)) return false;
    return m_scene->GetEntitiesFromBucket(key, out);
}

void Machine::GetVisibleEntities(ETHEntityArray& out) const {
    if (!m_scene) {
        SUPERSONIC_LOG_WARN("Penumbra") << "GetVisibleEntities: this function can't be called before the scene is "
                                           "completely loaded.";
        return;
    }
    m_scene->GetVisibleEntities(m_camera, m_screenSize, out);
}

bool Machine::Collide(const ETHEntity& entity, const int which, ETHEntity* out) const {
    // The entity is passed by reference (`const ETHEntity &in`): a null handle
    // is AngelScript's null-pointer exception.
    const Entity& self = *entity;
    if (!self.IsAlive() || !m_scene) return false;
    return m_scene->Collide(self, which, out);
}

uint Machine::GetNumEntities() const { return m_scene ? static_cast<uint>(m_scene->NumEntities()) : 0u; }

int Machine::GetLastID() const { return m_scene ? m_scene->LastId() : 0; }

// --- Camera, scene look ---------------------------------------------------------------------

void Machine::SetCameraPos(const vector2& pos) {
    // Floored when round-up is on [dis DLL@0x10006830].
    m_camera = m_roundUp ? vector2(std::floor(pos.x), std::floor(pos.y)) : pos;
}

void Machine::AddToCameraPos(const vector2& v) {
    // Never floored [dis DLL@0x100068a0].
    m_camera += v;
}

void Machine::SetBorderBucketsDrawing(const bool draw) {
    if (!m_scene) {
        SUPERSONIC_LOG_WARN("Penumbra") << "SetBorderBucketsDrawing: this function can't be called before the "
                                           "scene is completely loaded.";
        return;
    }
    m_scene->SetBorderBucketsDrawing(draw);
}

bool Machine::IsDrawingBorderBuckets() const { return m_scene && m_scene->IsDrawingBorderBuckets(); }

void Machine::SetAmbientLight(const vector3& color) {
    if (!m_scene) {
        SUPERSONIC_LOG_WARN("Penumbra") << "SetAmbientLight: this function can't be called before the scene is "
                                           "completely loaded.";
        return;
    }
    m_scene->Properties().ambient = color;
}

vector3 Machine::GetAmbientLight() const { return m_scene ? m_scene->Properties().ambient : vector3(0.0f); }

bool Machine::SetBackgroundImage(const string& path) {
    // ETHEngine.cpp:1431-1455: loaded into the sprite cache, rectangle reset to
    // the bitmap; "" clears it.
    if (path.empty()) {
        m_backgroundImage.clear();
        return true;
    }
    if (!AddSpriteResource(path)) {
        m_backgroundImage.clear();
        return false;
    }
    m_backgroundImage = path;
    PositionBackgroundImage(vector2(0.0f), FindSpriteResource(path)->size);
    return true;
}

void Machine::PositionBackgroundImage(const vector2& min, const vector2& max) {
    // Sorted per component (ETHEngine.cpp:1457-1464).
    m_backgroundMin = vector2(std::min(min.x, max.x), std::min(min.y, max.y));
    m_backgroundMax = vector2(std::max(min.x, max.x), std::max(min.y, max.y));
}

// --- Top layer ----------------------------------------------------------------------------

void Machine::DrawText(const vector2& pos, const string& text, const string& font, const float size,
                       const uint color) {
    HudCmd cmd;
    cmd.kind = HudCmd::Kind::Text;
    cmd.pos = pos;
    cmd.text = text;
    cmd.font = font;
    cmd.fontSize = size;
    cmd.color = color;
    m_hudQueue.push_back(std::move(cmd));
}

void Machine::LoadSprite(const string& path) {
    AddSpriteResource(path);
}

void Machine::DrawSprite(const string& path, const vector2& pos, const uint color) {
    HudCmd cmd;
    cmd.kind = HudCmd::Kind::Sprite;
    cmd.pos = pos;
    cmd.sprite = path;
    cmd.color = color;
    m_hudQueue.push_back(std::move(cmd));
}

void Machine::DrawShapedSprite(const string& path, const vector2& pos, const vector2& size, const uint color) {
    HudCmd cmd;
    cmd.kind = HudCmd::Kind::ShapedSprite;
    cmd.pos = pos;
    cmd.size = size;
    cmd.sprite = path;
    cmd.color = color;
    m_hudQueue.push_back(std::move(cmd));
}

vector2 Machine::GetSpriteSize(const string& path) const {
    // Never loads (ETHEngine.cpp:1149-1155).
    const SpriteResource* resource = FindSpriteResource(path);
    return resource != nullptr ? resource->size : vector2(0.0f);
}

void Machine::DrawRectangle(const vector2& pos, const vector2& size, const uint c0, const uint c1, const uint c2,
                            const uint c3) {
    HudCmd cmd;
    cmd.kind = HudCmd::Kind::Rectangle;
    cmd.pos = pos;
    cmd.size = size;
    cmd.color = c0;
    cmd.color1 = c1;
    cmd.color2 = c2;
    cmd.color3 = c3;
    m_hudQueue.push_back(std::move(cmd));
}

// --- Window --------------------------------------------------------------------------------

void Machine::SetWindowProperties(const string& title, const uint width, const uint height, const bool windowed,
                                  const bool sync, const PIXEL_FORMAT format) {
    (void)sync;     // always vsynced on the engine's tick
    (void)format;   // always 32-bit
    m_window.title = title;
    m_window.width = width;
    m_window.height = height;
    m_window.windowed = windowed;
    m_window.changed = true;
}

uint Machine::GetVideoModeCount() const { return static_cast<uint>(m_videoModes.size()); }

videoMode Machine::GetVideoMode(const uint index) const {
    // An invalid mode reads 0x0, unknown format (ETHEngine.cpp:1403-1414).
    if (index >= m_videoModes.size()) return videoMode{0, 0, PFUNKNOWN};
    return m_videoModes[index];
}

// --- Files ----------------------------------------------------------------------------------

string Machine::GetAbsolutePath(const string& relative) const { return m_config.gameRoot + "/" + relative; }

bool Machine::IsGamePath(const string& path) const {
    string rel;
    return !IsAbsolutePath(path) || UnderRoot(path, m_config.gameRoot, rel);
}

string Machine::ReadPath(const string& relative) const {
    string rel;
    if (IsAbsolutePath(relative)) {
        if (!UnderRoot(relative, m_config.gameRoot, rel)) return relative;
    } else {
        rel = ForwardSlashes(relative);
    }
    if (!m_config.userRoot.empty()) {
        const string user = m_config.userRoot + "/" + rel;
        std::error_code ec;
        if (std::filesystem::is_regular_file(std::filesystem::path(user), ec)) return user;
    }
    return m_config.gameRoot + "/" + rel;
}

string Machine::WritePath(const string& relative) const {
    if (m_config.userRoot.empty()) return string();
    string rel;
    if (IsAbsolutePath(relative)) {
        if (!UnderRoot(relative, m_config.gameRoot, rel)) return string();
    } else {
        rel = ForwardSlashes(relative);
    }
    const string target = m_config.userRoot + "/" + rel;
    // Whatever the configuration, never a path inside the original's folder.
    string inside;
    if (UnderRoot(target, PENUMBRA_ORIGINAL_DIR, inside) || UnderRoot(target, m_config.gameRoot, inside)) {
        SUPERSONIC_LOG_ERROR("Penumbra") << "refusing to write into the original's folder: " << target;
        return string();
    }
    std::error_code ec;
    const std::filesystem::path parent = std::filesystem::path(target).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);
    return target;
}

const EntityDef* Machine::EntityDefinition(const string& file) {
    const auto it = m_definitions.find(file);
    if (it != m_definitions.end()) return it->second.get();
    std::optional<EntityDef> def = ReadEntityFile(ReadPath("entities/" + file));
    auto& slot = m_definitions[file];
    if (def) slot = std::make_unique<EntityDef>(std::move(*def));
    return slot.get();
}

vector2 Machine::ImageSize(const string& relativePath) {
    const auto it = m_imageSizes.find(relativePath);
    if (it != m_imageSizes.end()) return it->second;
    const vector2 size = ReadImageSize(ReadPath(relativePath));
    m_imageSizes.emplace(relativePath, size);
    return size;
}

bool Machine::AddSpriteResource(const string& relativePath) {
    const string key = FileName(relativePath);
    if (key.empty()) return false;
    if (m_sprites.count(key) != 0) return true;
    const vector2 size = ImageSize(relativePath);
    if (size.x <= 0.0f || size.y <= 0.0f) {
        SUPERSONIC_LOG_WARN("Penumbra") << "(Not loaded) " << relativePath;
        return false;
    }
    m_sprites.emplace(key, SpriteResource{relativePath, size});
    return true;
}

const Machine::SpriteResource* Machine::FindSpriteResource(const string& path) const {
    const auto it = m_sprites.find(FileName(path));
    return it == m_sprites.end() ? nullptr : &it->second;
}

} // namespace Penumbra::Eth
