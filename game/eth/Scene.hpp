#pragma once

// ETHScene: the entities of the loaded scene, the bucket grid they are indexed
// by, the id counter and the dynamic-or-temporary callback list.
//
// Buckets are the original's only spatial structure and the scripts build all
// of their collision and neighbour queries on them (43 GetEntitiesFromBucket
// calls). Membership is by ORIGIN: floor(position.xy / bucketSize). Within a
// bucket, horizontal entities are pushed to the FRONT and every other type to
// the back, on add and on every move across a bucket boundary
// (ETHScene.cpp:393-406, :1822-1826). The bucket map is ordered here (0.7.12's
// was a hash map, so any walk over all buckets was in an arbitrary order the
// port does not reproduce; ordered is at least deterministic).

#include <list>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "eth/Defs.hpp"
#include "eth/Entity.hpp"

namespace Penumbra::Eth {

class Machine;

class Scene {
public:
    using BucketKey = std::pair<int, int>;   // (x, y)

    Scene(Machine& machine, string fileName, vector2 bucketSize);
    ~Scene();   // unlinks every entity (handles held by scripts stay valid, dead)

    const string& FileName() const { return m_fileName; }
    vector2 BucketSize() const { return m_bucketSize; }
    BucketKey BucketOf(const vector2& pos) const;

    SceneProperties& Properties() { return m_properties; }
    const SceneProperties& Properties() const { return m_properties; }

    // Build from a parsed file: every placement becomes an entity with the
    // file's id, name, frame, colour and position and its INLINE definition;
    // the id counter becomes max(id)+1.
    void Populate(const SceneFile& file);

    // AddEntity: a new id (m_idCounter++), bound to ETHCallback_<name without
    // its extension> if the Machine has one; dynamic-with-callback and
    // temporary entities join the every-frame list; its particle systems are
    // created and their sound effects started. Returns the new entity.
    std::shared_ptr<Entity> Add(const string& name, EntityDef def, const vector3& pos, float angle,
                                int forcedId = -1, uint frame = UINT32_MAX, glm::vec4 color = glm::vec4(1.0f));
    // DeleteEntity: kill and unlink. The object lives on in any handle.
    bool Delete(Entity& entity);

    // Called by Entity's position setters.
    void Relink(Entity& entity, const vector3& oldPosition);

    // Queries, with the original's order semantics.
    bool GetEntitiesFromBucket(const BucketKey& bucket, ETHEntityArray& out) const;
    bool GetEntityArray(const string& name, ETHEntityArray& out) const;
    ETHEntity SeekEntity(int id) const;
    ETHEntity SeekEntity(const string& name) const;
    // Every visible bucket (camera rect, inclusive at both ends, +1 ring when
    // border drawing is on), row-major top to bottom.
    std::vector<BucketKey> VisibleBuckets(const vector2& camera, const vector2& screen) const;
    void GetVisibleEntities(const vector2& camera, const vector2& screen, ETHEntityArray& out) const;
    // Collide/CollideStatic/CollideDynamic (ETHScene.cpp:1362-1528): which = 0
    // any, 1 static only, 2 dynamic only.
    bool Collide(const Entity& entity, int which, ETHEntity* out) const;

    void SetBorderBucketsDrawing(bool draw) { m_borderBuckets = draw; }
    bool IsDrawingBorderBuckets() const { return m_borderBuckets; }

    int NumEntities() const;
    int LastId() const { return m_idCounter - 1; }

    // The running extremes of z (ETHScene.cpp:174,247-248): they start at 0 and
    // at the screen height, and only grow.
    float MinHeight() const { return m_minHeight; }
    float MaxHeight() const { return m_maxHeight; }
    void GrowHeightRange(float z, float spriteHeight);

    // Every live entity, in bucket order (for SaveScene and the snapshot).
    std::vector<std::shared_ptr<Entity>> AllEntities() const;
    // The every-frame callback list (dynamic entities with a callback, and
    // temporaries), in insertion order. Entities are appended during iteration.
    std::vector<std::shared_ptr<Entity>>& DynamicOrTemporary() { return m_dynamicOrTemp; }

private:
    Machine& m_machine;
    string m_fileName;
    vector2 m_bucketSize;
    SceneProperties m_properties;
    std::map<BucketKey, std::list<std::shared_ptr<Entity>>> m_buckets;
    std::vector<std::shared_ptr<Entity>> m_dynamicOrTemp;
    int m_idCounter = 0;
    bool m_borderBuckets = true;
    float m_minHeight = 0.0f;
    float m_maxHeight = 768.0f;
};

} // namespace Penumbra::Eth
