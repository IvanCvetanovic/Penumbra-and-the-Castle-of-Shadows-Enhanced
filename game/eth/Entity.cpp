// ETHEntity / ETHRenderEntity: the per-entity half of the 0.7.12 runtime
// (reference/eth-0.7.12/src/ETHEntity.cpp, ETHRenderEntity.cpp).

#include "eth/Entity.hpp"

#include <algorithm>
#include <cmath>

#include "eth/Audio.hpp"
#include "eth/Machine.hpp"
#include "eth/Scene.hpp"

namespace Penumbra::Eth {

namespace {

// Two particle systems per entity (ETH_MAX_PARTICLE_SYS_PER_ENTITY, ETHCommon.h:67).
constexpr uint kMaxParticleSystems = 2;

// ETH_SMALL_NUMBER (ETHCommon.h:70): decals sit a tenth of a unit above their z.
constexpr float kDecalRise = 0.1f;
// m_layrableMinimumDepth (ETHRenderEntity.cpp:55).
constexpr float kLayerableMinimumDepth = 0.001f;

// The machine an entity's time, randomness and scene settings come from: its
// scene's while it is linked, else the current one (0.7.12 kept using the
// globals for a dead entity).
Machine* MachineOf(const Entity& entity) {
    if (entity.m_scene != nullptr) return &entity.m_scene->GetMachine();
    return Machine::CurrentOrNull();
}

// The bucket size GetCurrentBucket uses: 0.7.12 always passed the CURRENT
// scene (ETHEngine.cpp:1502-1505), whether or not the entity was in it.
vector2 BucketSizeFor(const Entity& entity) {
    if (entity.m_scene != nullptr) return entity.m_scene->BucketSize();
    const Machine* machine = Machine::CurrentOrNull();
    if (machine != nullptr && machine->CurrentScene() != nullptr) return machine->CurrentScene()->BucketSize();
    return vector2(256.0f);
}

vector2 ZAxisFor(const Entity& entity) {
    if (entity.m_scene != nullptr) return entity.m_scene->Properties().zAxisDirection;
    const Machine* machine = Machine::CurrentOrNull();
    if (machine != nullptr && machine->CurrentScene() != nullptr) {
        return machine->CurrentScene()->Properties().zAxisDirection;
    }
    return vector2(0.0f);
}

} // namespace

Entity::Entity(const int id, string name, EntityDef def)
    : m_id(id), m_name(std::move(name)), m_def(std::move(def)) {}

Entity::~Entity() {
    if (!m_stopSfxWhenDestroyed) return;
    const std::shared_ptr<SampleBank> bank = m_sfxBank.lock();
    if (!bank) return;
    for (const auto& manager : m_particles) {
        if (manager && manager->HasSoundEffect()) {
            bank->ApplyEffect(manager->System().soundEffect, true, false, 0.0f, 0.0f);
        }
    }
}

// --- Position ------------------------------------------------------------------

void Entity::SetPosition(const vector3& pos) {
    const vector3 old = m_position;
    m_position = pos;
    if (m_scene != nullptr) m_scene->Relink(*this, old);
}

void Entity::SetPositionXY(const vector2& pos) {
    SetPosition(vector3(pos.x, pos.y, m_position.z));
}

void Entity::AddToPosition(const vector3& v) {
    SetPosition(m_position + v);
}

void Entity::AddToPositionXY(const vector2& v) {
    SetPositionXY(vector2(m_position) + v);
}

vector2 Entity::GetCurrentBucket() const {
    // ETHGlobal::GetBucket (ETHCommon.h:418-421).
    const vector2 size = BucketSizeFor(*this);
    return vector2(std::floor(m_position.x / size.x), std::floor(m_position.y / size.y));
}

// --- Size and frames -------------------------------------------------------------

vector2 Entity::GetSize() const {
    // GetCurrentSize (ETHRenderEntity.cpp:1004-1055).
    if (!HasSpriteImage()) {
        if (m_def.light.active) {
            if (m_def.light.haloSize > 0.0f && m_haloLoaded) {
                return vector2(m_def.light.haloSize, m_def.light.haloSize);
            }
            return vector2(32.0f, 32.0f);
        }
        if (m_def.collidable) return vector2(m_def.collisionSize);
        return vector2(32.0f, 32.0f);
    }
    if (GetNumFrames() <= 1) return m_bitmapSize;
    // SetupSpriteRects: integer strides over the ORIGINAL bitmap size
    // (gs2dD3D9Sprite.cpp:217-245).
    const auto width = static_cast<uint>(m_bitmapSize.x);
    const auto height = static_cast<uint>(m_bitmapSize.y);
    const auto cutX = static_cast<uint>(std::max(1, m_def.spriteCutX));
    const auto cutY = static_cast<uint>(std::max(1, m_def.spriteCutY));
    return vector2(static_cast<float>(width / cutX), static_cast<float>(height / cutY));
}

uint Entity::GetNumFrames() const {
    return static_cast<uint>(m_def.spriteCutX * m_def.spriteCutY);
}

bool Entity::SetFrame(const uint frame) {
    // ETHEntity.cpp:131-145: only frame > cutX*cutY is refused, so the one
    // past the end is accepted.
    if (frame > GetNumFrames()) {
        m_frame = 0;
        return false;
    }
    m_frame = frame;
    return true;
}

bool Entity::SetFrame(const uint column, const uint row) {
    // ETHEntity.cpp:147-162.
    const auto cutX = static_cast<uint>(m_def.spriteCutX);
    const auto cutY = static_cast<uint>(m_def.spriteCutY);
    if (column >= cutX || row >= cutY) {
        m_frame = 0;
        return false;
    }
    m_frame = row * cutX + column;
    return true;
}

collisionBox Entity::GetCollisionBox() const {
    collisionBox box;
    if (m_def.collidable) {
        box.pos = m_def.collisionPos;
        box.size = m_def.collisionSize;
    }
    return box;
}

// --- Custom data (ETHDataManager.cpp) ----------------------------------------------

bool Entity::AddFloatData(const string& name, const float value) {
    CustomValue& v = m_def.customData[name];
    v.type = DT_FLOAT;
    v.f = value;
    return true;
}

bool Entity::AddIntData(const string& name, const int value) {
    CustomValue& v = m_def.customData[name];
    v.type = DT_INT;
    v.i = value;
    return true;
}

bool Entity::AddUIntData(const string& name, const uint value) {
    CustomValue& v = m_def.customData[name];
    v.type = DT_UINT;
    v.u = value;
    return true;
}

bool Entity::AddStringData(const string& name, const string& value) {
    CustomValue& v = m_def.customData[name];
    v.type = DT_STRING;
    v.s = value;
    return true;
}

// A missing name OR another type reads as 0 / "" (ETHDataManager.cpp:123-189;
// 0.7.12 logged each miss, which the scripts cause every frame by design).
float Entity::GetFloatData(const string& name) const {
    const auto it = m_def.customData.find(name);
    return (it != m_def.customData.end() && it->second.type == DT_FLOAT) ? it->second.f : 0.0f;
}

int Entity::GetIntData(const string& name) const {
    const auto it = m_def.customData.find(name);
    return (it != m_def.customData.end() && it->second.type == DT_INT) ? it->second.i : 0;
}

uint Entity::GetUIntData(const string& name) const {
    const auto it = m_def.customData.find(name);
    return (it != m_def.customData.end() && it->second.type == DT_UINT) ? it->second.u : 0u;
}

string Entity::GetStringData(const string& name) const {
    const auto it = m_def.customData.find(name);
    return (it != m_def.customData.end() && it->second.type == DT_STRING) ? it->second.s : string();
}

DATA_TYPE Entity::CheckCustomData(const string& name) const {
    const auto it = m_def.customData.find(name);
    return it != m_def.customData.end() ? it->second.type : DT_NODATA;
}

bool Entity::EraseData(const string& name) {
    return m_def.customData.erase(name) != 0;
}

// --- Particle systems ------------------------------------------------------------------

bool Entity::HasParticleSystem() const {
    for (uint t = 0; t < kMaxParticleSystems && t < m_def.particles.size(); ++t) {
        if (m_def.particles[t].nParticles > 0) return true;
    }
    return false;
}

bool Entity::HasParticleSystem(const uint n) const {
    return n < kMaxParticleSystems && n < m_def.particles.size() && m_def.particles[n].nParticles > 0;
}

void Entity::KillParticleSystem(const uint n) {
    if (ParticleManager* manager = ParticleSlot(n)) manager->Kill(true);
}

bool Entity::ParticlesKilled(const uint n) const {
    const ParticleManager* manager = ParticleSlot(n);
    return manager != nullptr && manager->Killed();
}

bool Entity::MirrorParticleSystemX(const uint n, const bool mirrorGravity) {
    ParticleManager* manager = ParticleSlot(n);
    if (manager == nullptr) return false;
    manager->MirrorX(mirrorGravity);
    return true;
}

bool Entity::PlayParticleSystem(const uint n) {
    // ETHRenderEntity.cpp:1256-1268: Kill(false), then Play at the owner's
    // screen position; it reports true whether or not the slot exists.
    ParticleManager* manager = ParticleSlot(n);
    Machine* machine = MachineOf(*this);
    if (manager == nullptr || machine == nullptr) return true;
    manager->Kill(false);
    const vector2 zAxis = ZAxisFor(*this);
    const vector2 screenPos = vector2(m_position) + zAxis * m_position.z;
    manager->Play(screenPos, m_position, m_angle, machine->GetTimeF(), machine->Rng());
    // Play() restarted the system's sample itself (ETHParticleManager.cpp:788-789).
    if (manager->HasSoundEffect() && machine->Samples().SampleExists(manager->System().soundEffect)) {
        machine->Samples().PlaySample(manager->System().soundEffect);
    }
    return true;
}

bool Entity::AreParticlesOver() const {
    // ETHRenderEntity.cpp:1239-1254: every EXISTING system finished; an entity
    // without any is never "over".
    uint existent = 0;
    uint finished = 0;
    for (const auto& manager : m_particles) {
        if (!manager) continue;
        ++existent;
        if (manager->Finished()) ++finished;
    }
    return existent != 0 && finished == existent;
}

bool Entity::IsTemporary() const {
    // ETHEntity.cpp:98-115: at least one system with particles, every such
    // system finite, and no <Sprite> NAME (the string, not the loaded image).
    uint existent = 0;
    uint temporary = 0;
    for (uint t = 0; t < kMaxParticleSystems && t < m_def.particles.size(); ++t) {
        if (m_def.particles[t].nParticles > 0) {
            if (m_def.particles[t].repeat > 0) ++temporary;
            ++existent;
        }
    }
    return existent != 0 && temporary == existent && m_def.sprite.empty();
}

// --- Depth ---------------------------------------------------------------------------------

float Entity::MaxHeight() const {
    float maxHeight = m_position.z + GetSize().y;
    for (uint t = 0; t < kMaxParticleSystems && t < m_def.particles.size(); ++t) {
        const ParticleSystemDef& system = m_def.particles[t];
        if (system.nParticles > 0) {
            maxHeight = std::max(maxHeight, m_position.z + system.startPoint.z + system.boundingSphere * 2.0f);
        }
    }
    // The halo's z is the light's RELATIVE z, not added to the entity's
    // (ETHRenderEntity.cpp:1176-1177).
    if (m_def.light.active && !m_def.light.haloBitmap.empty()) maxHeight = std::max(maxHeight, m_def.light.position.z);
    return maxHeight;
}

float Entity::MinHeight() const {
    float minHeight = m_position.z - GetSize().y;
    for (uint t = 0; t < kMaxParticleSystems && t < m_def.particles.size(); ++t) {
        const ParticleSystemDef& system = m_def.particles[t];
        if (system.nParticles > 0) {
            minHeight = std::min(minHeight, m_position.z + system.startPoint.z - system.boundingSphere * 2.0f);
        }
    }
    if (m_def.light.active && !m_def.light.haloBitmap.empty()) minHeight = std::min(minHeight, m_def.light.position.z);
    return minHeight;
}

float Entity::ComputeDepth(const float maxHeight, const float minHeight) const {
    // ETHGlobal::ComputeDepth (ETHCommon.h:543-546) per type.
    switch (m_def.type) {
    case ET_VERTICAL:
    case ET_HORIZONTAL:
        return (m_position.z - minHeight) / (maxHeight - minHeight);
    case ET_OPAQUE_DECAL:
    case ET_GROUND_DECAL:
        return (m_position.z + kDecalRise - minHeight) / (maxHeight - minHeight);
    case ET_OVERALL:
        return 1.0f;
    case ET_LAYERABLE:
        return std::max(kLayerableMinimumDepth, m_def.layerDepth);
    }
    return 0.0f;
}

} // namespace Penumbra::Eth
