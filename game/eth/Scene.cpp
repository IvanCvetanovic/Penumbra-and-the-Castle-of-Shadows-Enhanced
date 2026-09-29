// ETHScene (reference/eth-0.7.12/src/ETHScene.cpp): entity storage, buckets,
// ids, callback binding and the scene-side queries the scripts build on.

#include "eth/Scene.hpp"

#include <algorithm>
#include <climits>
#include <cmath>
#include <string>

#include "core/Log.hpp"
#include "eth/Audio.hpp"
#include "eth/Machine.hpp"

namespace Penumbra::Eth {

namespace {

constexpr float kDefaultBucketSize = 256.0f;   // _ETH_DEFAULT_BUCKET_SIZE (ETHCommon.h:95)
constexpr float kMinBucketSize = 128.0f;       // _ETH_MIN_BUCKET_SIZE (ETHCommon.h:93)
constexpr std::size_t kMaxBuckets = 128;       // _ETH_MAX_BUCKETS (ETHCommon.h:94)
constexpr uint kMaxParticleSystems = 2;

// floor(v) as a bucket coordinate. A NaN or huge position (a script can
// normalize a zero vector into one) must still give SOME key; 0.7.12's float
// key just never matched again, which the port does not copy.
int ToBucketCoord(const float v) {
    if (!(v == v)) return INT_MIN;
    const double f = std::floor(static_cast<double>(v));
    if (f <= static_cast<double>(INT_MIN)) return INT_MIN;
    if (f >= static_cast<double>(INT_MAX)) return INT_MAX;
    return static_cast<int>(f);
}

// ETHGlobal::RemoveExtension (ETHCommon.h:309-322): cut at the FIRST '.', so
// "tile01.ent" binds ETHCallback_tile01 and "a.b.ent" binds ETHCallback_a.
string RemoveExtension(const string& name) {
    const auto dot = name.find('.');
    return dot == string::npos ? name : name.substr(0, dot);
}

string BaseName(const string& path) {
    const auto slash = path.find_last_of("/\\");
    return slash == string::npos ? path : path.substr(slash + 1);
}

// ETH_COLLISION_BOX::Collide (ETHCommon.cpp:128-157): 3-D, inclusive, x then
// y then z.
bool BoxesCollide(const vector3& pos0, const vector3& size0, const vector3& at0, const vector3& pos1,
                  const vector3& size1, const vector3& at1) {
    const vector3 half0 = size0 / 2.0f;
    const vector3 half1 = size1 / 2.0f;
    const vector3 min0 = pos0 + at0 - half0;
    const vector3 max0 = pos0 + at0 + half0;
    const vector3 min1 = pos1 + at1 - half1;
    const vector3 max1 = pos1 + at1 + half1;
    if (min0.x > max1.x) return false;
    if (min0.y > max1.y) return false;
    if (max0.x < min1.x) return false;
    if (max0.y < min1.y) return false;
    if (max0.z < min1.z) return false;
    if (min0.z > max1.z) return false;
    return true;
}

} // namespace

Scene::Scene(Machine& machine, string fileName, vector2 bucketSize)
    : m_machine(machine), m_fileName(std::move(fileName)), m_bucketSize(kDefaultBucketSize) {
    // SetBucketSize refuses anything under 128 and keeps the default
    // (ETHScene.cpp:1848-1858).
    if (bucketSize.x < kMinBucketSize || bucketSize.y < kMinBucketSize) {
        SUPERSONIC_LOG_WARN("Penumbra") << "ETHScene::SetBucketSize: invalid bucket size: (" << bucketSize.x << ","
                                        << bucketSize.y << ") (must be >= 128)";
    } else {
        m_bucketSize = bucketSize;
    }
    // ETH_SCENE_PROPERTIES() (ETHEntityFile.cpp:111-116): what an "empty" scene
    // keeps; a scene file overwrites it in Populate.
    m_properties.ambient = vector3(0.3f);
    m_properties.lightIntensity = 2.0f;
    m_properties.zAxisDirection = vector2(0.0f, -1.0f);
    m_minHeight = 0.0f;
    m_maxHeight = machine.GetScreenSize().y;
}

Scene::~Scene() {
    // The scene's references go; the objects live on in any handle. Their
    // particle samples are stopped by the load's ReleaseAll, not one by one.
    for (auto& [key, list] : m_buckets) {
        for (const auto& entity : list) {
            entity->m_scene = nullptr;
            entity->m_stopSfxWhenDestroyed = false;
        }
    }
    for (const auto& entity : m_dynamicOrTemp) {
        entity->m_scene = nullptr;
        entity->m_stopSfxWhenDestroyed = false;
    }
}

Scene::BucketKey Scene::BucketOf(const vector2& pos) const {
    return {ToBucketCoord(pos.x / m_bucketSize.x), ToBucketCoord(pos.y / m_bucketSize.y)};
}

bool Scene::ToKey(const vector2& bucket, BucketKey& out) {
    if (!(bucket.x == bucket.x) || !(bucket.y == bucket.y)) return false;
    if (std::floor(bucket.x) != bucket.x || std::floor(bucket.y) != bucket.y) return false;
    out = {ToBucketCoord(bucket.x), ToBucketCoord(bucket.y)};
    return true;
}

void Scene::Populate(const SceneFile& file) {
    // ReadFromXMLFile (ETHScene.cpp:287-316): the properties, then every
    // placement in file order with its own id.
    m_properties = file.properties;
    for (const ScenePlacement& placement : file.entities) {
        m_idCounter = std::max(m_idCounter, placement.id + 1);
        Add(placement.entityName, placement.def, placement.position, placement.angle, placement.id,
            placement.spriteFrame, placement.color);
    }
}

void Scene::Link(const std::shared_ptr<Entity>& entity, const BucketKey& key) {
    EntityList& list = m_buckets[key];
    if (entity->GetType() == ET_HORIZONTAL) {
        list.push_front(entity);
    } else {
        list.push_back(entity);
    }
}

void Scene::BindCallback(Entity& entity) const {
    // AssignCallbackScript (ETHScene.cpp:1315-1360): ETHCallback_<id> first,
    // then ETHCallback_<name up to its first '.'>.
    const string byId = std::to_string(entity.GetID());
    if (m_machine.HasCallback(byId)) {
        entity.m_hasCallback = true;
        entity.m_callbackName = byId;
        return;
    }
    const string byName = RemoveExtension(entity.GetEntityName());
    if (m_machine.HasCallback(byName)) {
        entity.m_hasCallback = true;
        entity.m_callbackName = byName;
    }
}

std::shared_ptr<Entity> Scene::Add(const string& name, EntityDef def, const vector3& pos, const float angle,
                                   const int forcedId, const uint frame, const glm::vec4 color) {
    // ETHScene::AddEntity (ETHScene.cpp:370-438) with the ETHRenderEntity it
    // was handed (ETHRenderEntity::Create, ETHRenderEntity.cpp:318-348).
    const int id = forcedId < 0 ? m_idCounter++ : forcedId;
    auto entity = std::make_shared<Entity>(id, name, std::move(def));
    Entity& e = *entity;
    e.m_position = pos;
    e.m_angle = angle;
    e.m_color = color;
    e.SetFrame(frame == UINT32_MAX ? e.m_def.startFrame : frame);
    LoadImages(e);

    // LoadParticleSystem (ETHRenderEntity.cpp:350-381): at the entity's xy
    // (not its screen position), at the current time.
    const auto systems = static_cast<uint>(std::min<std::size_t>(e.m_def.particles.size(), kMaxParticleSystems));
    e.m_particles.resize(systems);
    for (uint t = 0; t < systems; ++t) {
        const ParticleSystemDef& system = e.m_def.particles[t];
        if (system.nParticles <= 0) continue;
        e.m_particles[t] = std::make_unique<ParticleManager>(system, vector2(pos), pos, angle, e.m_def.soundVolume,
                                                             m_machine.GetTimeF(), m_machine.Rng());
        e.m_particleClock[t] = m_machine.FrameIndex();
        e.m_particleUpdatedAt[t] = 0;
    }
    e.m_sfxBank = m_machine.SfxBank();

    Link(entity, BucketOf(vector2(pos)));
    e.m_scene = this;

    m_maxHeight = std::max(m_maxHeight, e.MaxHeight());
    m_minHeight = std::min(m_minHeight, e.MinHeight());

    BindCallback(e);
    if ((e.m_hasCallback && !e.IsStatic()) || e.IsTemporary()) m_dynamicOrTemp.push_back(entity);

    // StartSFX (ETHScene.cpp:427-429): an explosion's sound starts over with it.
    for (const auto& manager : e.m_particles) {
        if (manager && manager->HasSoundEffect()) {
            m_machine.Samples().StartEffect(manager->System().soundEffect, e.m_def.soundVolume);
        }
    }
    return entity;
}

void Scene::LoadImages(Entity& e) const {
    // The shared basename cache, and the sizes GetSize() needs.
    if (!e.m_def.sprite.empty()) {
        const string sprite = "entities/" + BaseName(e.m_def.sprite);
        if (m_machine.AddSpriteResource(sprite)) {
            e.m_bitmapSize = m_machine.ImageSize(sprite);
            e.m_def.spriteCutX = std::max(1, e.m_def.spriteCutX);
            e.m_def.spriteCutY = std::max(1, e.m_def.spriteCutY);
        }
    }
    if (!e.m_def.normal.empty()) m_machine.AddSpriteResource("entities/normalmaps/" + BaseName(e.m_def.normal));
    if (!e.m_def.gloss.empty()) m_machine.AddSpriteResource("entities/" + BaseName(e.m_def.gloss));
    if (!e.m_def.light.haloBitmap.empty()) {
        e.m_haloLoaded = m_machine.AddSpriteResource("entities/" + BaseName(e.m_def.light.haloBitmap));
    }
}

std::shared_ptr<Entity> Scene::AddBackdrop(const ScenePlacement& placement) {
    const int id = kBackdropIdBase + static_cast<int>(m_backdrop.size());
    EntityDef def = placement.def;
    // Drawn, and nothing more: a particle system would have to be advanced,
    // and advancing one draws from the scripts' generator.
    def.particles.clear();
    auto entity = std::make_shared<Entity>(id, placement.entityName, std::move(def));
    Entity& e = *entity;
    e.m_position = placement.position;
    e.m_angle = placement.angle;
    e.m_color = placement.color;
    e.SetFrame(placement.spriteFrame);
    LoadImages(e);
    m_backdrop.push_back(entity);
    return entity;
}

bool Scene::Delete(Entity& entity) {
    const auto it = m_buckets.find(BucketOf(entity.GetPositionXY()));
    if (it != m_buckets.end()) {
        EntityList& list = it->second;
        // Searched from the back (ETHScene.cpp:1283-1299).
        for (auto rit = list.rbegin(); rit != list.rend(); ++rit) {
            if (rit->get() != &entity) continue;
            entity.MarkDead();
            entity.m_scene = nullptr;
            list.erase(std::next(rit).base());
            return true;
        }
    }
    SUPERSONIC_LOG_WARN("Penumbra") << "Couldn't find the entity to delete: ID" << entity.GetID();
    return false;
}

int Scene::RemoveFinishedTemporary(Entity& entity) {
    const auto it = m_buckets.find(BucketOf(entity.GetPositionXY()));
    if (it == m_buckets.end()) return -1;
    EntityList& list = it->second;
    for (auto lit = list.begin(); lit != list.end(); ++lit) {
        if (lit->get() != &entity) continue;
        // StopSFXWhenDestroyed(false), Kill, unlink (ETHScene.cpp:482-486): its
        // sound plays out.
        entity.m_stopSfxWhenDestroyed = false;
        entity.MarkDead();
        entity.m_scene = nullptr;
        list.erase(lit);
        return 1;
    }
    return 0;
}

void Scene::Relink(Entity& entity, const vector3& oldPosition) {
    const BucketKey from = BucketOf(vector2(oldPosition));
    const BucketKey to = BucketOf(entity.GetPositionXY());
    if (from == to) return;
    const auto it = m_buckets.find(from);
    if (it == m_buckets.end()) {
        SUPERSONIC_LOG_WARN("Penumbra") << "The current bucket doesn't exist: (" << from.first << ","
                                        << from.second << ")";
        return;
    }
    EntityList& list = it->second;
    for (auto lit = list.begin(); lit != list.end(); ++lit) {
        if (lit->get() != &entity) continue;
        std::shared_ptr<Entity> moved = *lit;
        list.erase(lit);
        Link(moved, to);
        return;
    }
    SUPERSONIC_LOG_WARN("Penumbra") << "Couldn't find entity ID " << entity.GetID() << " to move";
}

bool Scene::GetEntitiesFromBucket(const BucketKey& bucket, ETHEntityArray& out) const {
    const auto it = m_buckets.find(bucket);
    if (it == m_buckets.end()) return false;
    for (const auto& entity : it->second) out.push_back(ETHEntity(entity));
    return out.size() != 0;
}

bool Scene::GetEntityArray(const string& name, ETHEntityArray& out) const {
    const uint before = out.size();
    for (const auto& [key, list] : m_buckets) {
        for (const auto& entity : list) {
            if (entity->GetEntityName() == name) out.push_back(ETHEntity(entity));
        }
    }
    return out.size() != before;
}

std::shared_ptr<Entity> Scene::SeekEntity(const int id) const {
    for (const auto& [key, list] : m_buckets) {
        for (const auto& entity : list) {
            if (entity->GetID() == id) return entity;
        }
    }
    return nullptr;
}

std::shared_ptr<Entity> Scene::SeekEntity(const string& name) const {
    for (const auto& [key, list] : m_buckets) {
        for (const auto& entity : list) {
            if (entity->GetEntityName() == name) return entity;
        }
    }
    return nullptr;
}

std::vector<Scene::BucketKey> Scene::VisibleBuckets(const vector2& camera, const vector2& screen) const {
    std::vector<BucketKey> out;
    const int ring = m_borderBuckets ? 1 : 0;
    const BucketKey min = BucketOf(camera);
    const BucketKey max = BucketOf(camera + screen);
    for (long long y = static_cast<long long>(min.second) - ring; y <= static_cast<long long>(max.second) + ring; ++y) {
        for (long long x = static_cast<long long>(min.first) - ring; x <= static_cast<long long>(max.first) + ring;
             ++x) {
            out.emplace_back(static_cast<int>(x), static_cast<int>(y));
            // 0.7.12 stopped after the 129th (ETHCommon.h:465-469).
            if (out.size() > kMaxBuckets) return out;
        }
    }
    return out;
}

void Scene::GetVisibleEntities(const vector2& camera, const vector2& screen, ETHEntityArray& out) const {
    for (const BucketKey& key : VisibleBuckets(camera, screen)) {
        const auto it = m_buckets.find(key);
        if (it == m_buckets.end()) continue;
        for (const auto& entity : it->second) out.push_back(ETHEntity(entity));
    }
}

bool Scene::Collide(const Entity& entity, const int which, ETHEntity* out) const {
    if (!entity.Collidable()) return false;
    const collisionBox box = entity.GetCollisionBox();
    const vector3 at = entity.GetPosition();
    // The scanned rectangle starts at the box's top-left and is TWICE the box
    // (ETHScene.cpp:1376-1378): it reaches further right and down.
    const vector2 scanPos = vector2(box.pos - box.size / 2.0f + at);
    const vector2 scanSize = vector2(box.size * 2.0f);
    const BucketKey min = BucketOf(scanPos);
    const BucketKey max = BucketOf(scanPos + scanSize);
    std::size_t scanned = 0;
    for (long long y = min.second; y <= max.second; ++y) {
        for (long long x = min.first; x <= max.first; ++x) {
            if (++scanned > kMaxBuckets + 1) return false;
            const auto it = m_buckets.find({static_cast<int>(x), static_cast<int>(y)});
            if (it == m_buckets.end()) continue;
            // Walked from the back of each list.
            for (auto rit = it->second.rbegin(); rit != it->second.rend(); ++rit) {
                const Entity& other = **rit;
                if (which == 1 && !other.IsStatic()) continue;
                if (which == 2 && other.IsStatic()) continue;
                if (!other.Collidable()) continue;
                const collisionBox otherBox = other.GetCollisionBox();
                if (!BoxesCollide(box.pos, box.size, at, otherBox.pos, otherBox.size, other.GetPosition())) continue;
                if (other.GetID() == entity.GetID()) continue;
                if (out != nullptr) *out = ETHEntity(*rit);
                return true;
            }
        }
    }
    return false;
}

int Scene::NumEntities() const {
    std::size_t n = 0;
    for (const auto& [key, list] : m_buckets) n += list.size();
    return static_cast<int>(n);
}

std::vector<std::shared_ptr<Entity>> Scene::AllEntities() const {
    std::vector<std::shared_ptr<Entity>> out;
    for (const auto& [key, list] : m_buckets) out.insert(out.end(), list.begin(), list.end());
    return out;
}

const Scene::EntityList* Scene::Bucket(const BucketKey& key) const {
    const auto it = m_buckets.find(key);
    return it == m_buckets.end() ? nullptr : &it->second;
}

} // namespace Penumbra::Eth
