#pragma once

// An Ethanon entity (ETHRenderEntity) and the script handle to it (ETHEntity@).
//
// HANDLE SEMANTICS ARE THE POINT. In 0.7.12 a script handle keeps an entity's
// object alive after DeleteEntity: the entity is unlinked from its bucket and
// IsAlive() turns false, but every getter and setter still works on the dead
// object, and the scripts rely on it (main.as:186-187 reads the position of a
// checkpoint it has just deleted; setupScene.as keeps arrays of handles across
// frames and asks IsAlive()). So entities are shared_ptr-owned: the scene holds
// one reference while the entity is alive, handles hold the others, and the
// object goes when the last one does.
//
// A null handle dereferenced throws ScriptException, which the Machine catches
// at the boundary of the running callback - AngelScript's own behaviour.

#include <array>
#include <memory>
#include <optional>
#include <vector>

#include "eth/Defs.hpp"
#include "eth/Particles.hpp"

namespace Penumbra::Eth {

class Scene;
class SampleBank;

class Entity {
public:
    Entity(int id, string name, EntityDef def);
    ~Entity();

    Entity(const Entity&) = delete;
    Entity& operator=(const Entity&) = delete;

    // --- The script API (ETHEntity methods the scripts call) ---------------
    // Semantics: docs/spec/30-ethanon-runtime.md §2.4-2.9.

    vector3 GetPosition() const { return m_position; }
    vector2 GetPositionXY() const { return vector2(m_position); }
    // The setters apply at once and relink the entity into the bucket of its
    // new position (horizontal entities to the FRONT of that bucket's list).
    void SetPosition(const vector3& pos);
    void SetPositionXY(const vector2& pos);
    void AddToPosition(const vector3& v);
    void AddToPositionXY(const vector2& v);
    // floor(pos.xy / bucketSize) of the scene it is in (256x256 unless the scene
    // was loaded with another size).
    vector2 GetCurrentBucket() const;

    // The frame size (W/cutX, H/cutY in whole pixels of the ORIGINAL image
    // size), or the whole bitmap for one frame; without a sprite: the halo size
    // if its light has a halo, 32x32 if it has a light, the collision xy if
    // collidable, else 32x32 (ETHRenderEntity.cpp:1004-1055).
    vector2 GetSize() const;
    uint GetFrame() const { return m_frame; }
    // Rejects only f > cutX*cutY (so f == cutX*cutY is accepted, as in 0.7.12);
    // on rejection sets 0 and returns false.
    bool SetFrame(uint frame);
    bool SetFrame(uint column, uint row);
    uint GetNumFrames() const;

    int GetID() const { return m_id; }
    // The .esc <EntityName> for a placement ("spawn", "help", "tile01.ent"...),
    // or the file name WITH ".ent" for an AddEntity'd one.
    string GetEntityName() const { return m_name; }
    ENTITY_TYPE GetType() const { return m_def.type; }
    bool IsStatic() const { return m_def.isStatic; }
    float GetAngle() const { return m_angle; }
    void SetAngle(float angle) { m_angle = angle; }

    bool Collidable() const { return m_def.collidable; }
    void SetCollision(bool enable) { m_def.collidable = enable; }
    // RELATIVE to the entity; zeros when not collidable (ETHEntity.cpp:122-129).
    collisionBox GetCollisionBox() const;

    // Custom data: Add* creates or overwrites value AND type; Get* returns
    // 0 / 0u / 0.0f / "" when the name is missing OR holds another type.
    bool AddFloatData(const string& name, float value);
    bool AddIntData(const string& name, int value);
    bool AddUIntData(const string& name, uint value);
    bool AddStringData(const string& name, const string& value);
    float GetFloatData(const string& name) const;
    int GetIntData(const string& name) const;
    uint GetUIntData(const string& name) const;
    string GetStringData(const string& name) const;
    DATA_TYPE CheckCustomData(const string& name) const;
    bool EraseData(const string& name);
    bool HasCustomData() const { return !m_def.customData.empty(); }

    // v4Color: multiplies the ambient and light passes.
    void SetColor(const vector3& color) { m_color = glm::vec4(color, m_color.a); }
    void SetAlpha(float alpha) { m_color.a = alpha; }
    vector3 GetColor() const { return vector3(m_color); }
    float GetAlpha() const { return m_color.a; }

    bool IsAlive() const { return m_alive; }
    bool IsHidden() const { return m_hidden; }
    void Hide(bool hide) { m_hidden = hide; }

    bool HasParticleSystem() const;
    bool HasParticleSystem(uint n) const;
    // Kill stops the RE-release of particles whose life ended; those in flight
    // finish, and unreleased particles of the first wave still release
    // (ETHParticleManager.cpp:676-689). Mirror negates the start point,
    // direction and randomisation on X (and gravity when asked).
    void KillParticleSystem(uint n);
    bool ParticlesKilled(uint n) const;
    bool MirrorParticleSystemX(uint n, bool mirrorGravity);
    bool PlayParticleSystem(uint n);
    bool AreParticlesOver() const;

    bool HasLightSource() const { return m_def.light.active; }
    void SetLightRange(float range) { m_def.light.range = range; }
    float GetLightRange() const { return m_def.light.range; }
    void SetLightColor(const vector3& color) { m_def.light.color = color; }
    vector3 GetLightColor() const { return m_def.light.color; }
    // The alpha becomes 1 (E:ETHEntity.cpp:320-323).
    void SetEmissiveColor(const vector3& color) { m_def.emissiveColor = glm::vec4(color, 1.0f); }
    vector3 GetEmissiveColor() const { return vector3(m_def.emissiveColor); }

    // --- The runtime's side (Machine, Scene, snapshot); scripts never call these.

    const EntityDef& Def() const { return m_def; }
    EntityDef& MutableDef() { return m_def; }
    const glm::vec4& Color() const { return m_color; }
    void SetColorRGBA(const glm::vec4& color) { m_color = color; }

    // "Temporary" (ETHEntity.cpp:98-115): no sprite, and every particle system
    // has particles and a finite repeat. Deleted by the runtime when its
    // particles are over; SeekEntity never returns one.
    bool IsTemporary() const;

    // The ORIGINAL pixel size of the sprite image (0,0 without one), set by the
    // Scene when the entity is created (it reads image headers, not pixels).
    vector2 m_bitmapSize{0.0f};
    // Whether <HaloBitmap> could be read: GetSize() of a sprite-less light uses
    // the halo size only then (ETHRenderEntity.cpp:1011-1022).
    bool m_haloLoaded = false;

    // 0.7.12 tested the loaded sprite POINTER in some places (GetCurrentSize,
    // CheckTemporaryEntities, drawing) and the <Sprite> STRING in others
    // (IsTemporary): this is the pointer.
    bool HasSpriteImage() const {
        return !m_def.sprite.empty() && m_bitmapSize.x > 0.0f && m_bitmapSize.y > 0.0f;
    }

    // Particle systems, created from m_def.particles when the entity joins a
    // scene. Slot n holds a manager iff the n-th system has particles > 0
    // (LoadParticleSystem skipped empty ones, ETHRenderEntity.cpp:343-373).
    std::vector<std::unique_ptr<ParticleManager>> m_particles;
    ParticleManager* ParticleSlot(uint n) const {
        return n < m_particles.size() ? m_particles[n].get() : nullptr;
    }
    // Per slot, the Machine frame of the manager's creation or last Update,
    // and of its last Update (0 = never). 0.7.12 gave every manager its own
    // timer (ETHParticleManager.cpp:652-654), so frameSpeed is min(frames since
    // then, 2) - 0 for a system created and updated in one frame (a scene
    // load), 2 after a gap - and a second Update in one frame did nothing
    // worth repeating, so the runtime updates a system at most once a frame.
    std::array<uint, 2> m_particleClock{};
    std::array<uint, 2> m_particleUpdatedAt{};

    // GetMaxHeight/GetMinHeight (ETHRenderEntity.cpp:1166-1192): the z extent
    // the scene's depth range grows by.
    float MaxHeight() const;
    float MinHeight() const;
    // ComputeDepth (ETHRenderEntity.cpp:642-663).
    float ComputeDepth(float maxHeight, float minHeight) const;

    // Set by Scene: which scene this entity is linked into (null once deleted
    // or when its scene is destroyed), and whether a callback is bound to it.
    Scene* m_scene = nullptr;
    bool m_hasCallback = false;
    string m_callbackName;          // the ETHCallback_<name> it binds to, if any

    // ForceSFXStop on the final release (ETHRenderEntity.cpp:88-113): the last
    // handle going stops this entity's particle samples - unless the runtime
    // removed it as a finished temporary (StopSFXWhenDestroyed(false),
    // ETHScene.cpp:482) or its scene was torn down by a load (which stops every
    // sample anyway). The bank is a token the Machine renews at every load, so
    // an entity from an earlier scene never stops a same-named later sample.
    bool m_stopSfxWhenDestroyed = true;
    std::weak_ptr<SampleBank> m_sfxBank;

    // Kill(): IsAlive false, unlinked. Only Scene calls it.
    void MarkDead() { m_alive = false; }

private:
    int m_id;
    string m_name;
    EntityDef m_def;
    vector3 m_position{0.0f};
    float m_angle = 0.0f;
    glm::vec4 m_color{1.0f};
    uint m_frame = 0;
    bool m_alive = true;
    bool m_hidden = false;

    friend class Scene;
};

// ETHEntity@ - a nullable, copyable handle. `h->Method()` throws
// ScriptException when h is null, as AngelScript does.
class ETHEntity {
public:
    ETHEntity() = default;
    ETHEntity(std::nullptr_t) {}
    explicit ETHEntity(std::shared_ptr<Entity> entity) : m_entity(std::move(entity)) {}

    Entity* operator->() const {
        if (!m_entity) throw ScriptException("null pointer access (ETHEntity@)");
        return m_entity.get();
    }
    Entity& operator*() const { return *operator->(); }

    explicit operator bool() const { return m_entity != nullptr; }
    bool operator==(std::nullptr_t) const { return m_entity == nullptr; }
    bool operator!=(std::nullptr_t) const { return m_entity != nullptr; }
    // Handle identity (`a is b`).
    bool operator==(const ETHEntity& other) const { return m_entity == other.m_entity; }
    bool operator!=(const ETHEntity& other) const { return m_entity != other.m_entity; }

    const std::shared_ptr<Entity>& Ptr() const { return m_entity; }
    Entity* Get() const { return m_entity.get(); }

private:
    std::shared_ptr<Entity> m_entity;
};

// ETHEntityArray: append-only in the scripts' use (push_back, +=, [], size, clear).
class ETHEntityArray : public std::vector<ETHEntity> {
public:
    using std::vector<ETHEntity>::vector;
    ETHEntityArray& operator+=(const ETHEntityArray& other) {
        insert(end(), other.begin(), other.end());
        return *this;
    }
    uint size() const { return static_cast<uint>(std::vector<ETHEntity>::size()); }
    // AngelScript bounds-checks: an out-of-range index aborts the function.
    ETHEntity& operator[](std::size_t i) {
        if (i >= std::vector<ETHEntity>::size()) throw ScriptException("array index out of bounds");
        return std::vector<ETHEntity>::operator[](i);
    }
    const ETHEntity& operator[](std::size_t i) const {
        if (i >= std::vector<ETHEntity>::size()) throw ScriptException("array index out of bounds");
        return std::vector<ETHEntity>::operator[](i);
    }
};

} // namespace Penumbra::Eth
