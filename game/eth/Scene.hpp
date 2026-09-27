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
// port does not reproduce; ordered is at least deterministic). A bucket that
// empties keeps its (empty) list, as 0.7.12's did.

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
    using EntityList = std::list<std::shared_ptr<Entity>>;

    // `bucketSize` below 128 on either axis is refused and 256x256 kept
    // (ETHScene.cpp:1848-1858). The depth range starts at [0, screen height]
    // (ETHScene.cpp:174, :247-248), so the Machine's screen size must already
    // be the new scene's when this runs.
    Scene(Machine& machine, string fileName, vector2 bucketSize);
    // Unlinks every entity. Handles held by scripts stay valid AND IsAlive()
    // stays true: 0.7.12's Clear() only Released them (ETHScene.cpp:119-141),
    // it never Kill()ed them.
    ~Scene();

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    Machine& GetMachine() const { return m_machine; }
    const string& FileName() const { return m_fileName; }
    vector2 BucketSize() const { return m_bucketSize; }
    BucketKey BucketOf(const vector2& pos) const;
    // A script's vector2 bucket as a key; false when it is not integral (0.7.12
    // looked it up in a hash map, where such a key could never match).
    static bool ToKey(const vector2& bucket, BucketKey& out);

    SceneProperties& Properties() { return m_properties; }
    const SceneProperties& Properties() const { return m_properties; }

    // Build from a parsed file: every placement becomes an entity with the
    // file's id, name, frame, colour and position and its INLINE definition;
    // the id counter becomes max(id)+1 (ETHScene.cpp:287-316).
    void Populate(const SceneFile& file);

    // AddEntity (ETHScene.cpp:370-438): a new id (m_idCounter++) unless
    // forcedId >= 0; bound to ETHCallback_<id>, else ETHCallback_<name cut at
    // its first '.'>, if the Machine has one; dynamic-with-callback and
    // temporary entities join the every-frame list; its particle systems are
    // created and their sound effects started. Returns the new entity.
    // `frame` UINT32_MAX = the definition's startFrame.
    std::shared_ptr<Entity> Add(const string& name, EntityDef def, const vector3& pos, float angle,
                                int forcedId = -1, uint frame = UINT32_MAX, glm::vec4 color = glm::vec4(1.0f));
    // DeleteEntity (ETHScene.cpp:1273-1301): kill and unlink, searching the
    // bucket of the entity's CURRENT position. False when it is not there.
    // The object lives on in any handle.
    bool Delete(Entity& entity);
    // CheckTemporaryEntities' removal (ETHScene.cpp:461-495): -1 when the
    // bucket of the entity's position does not exist (it stays listed), 1 when
    // it was found and killed, 0 when the bucket exists but it is not in it.
    int RemoveFinishedTemporary(Entity& entity);

    // Called by Entity's position setters: MoveEntity (ETHScene.cpp:1786-1833).
    void Relink(Entity& entity, const vector3& oldPosition);

    // Queries, with the original's order semantics.
    // Appends the bucket's list; false when the bucket does not exist or `out`
    // is still empty afterwards (ETHScene.cpp:1696-1711).
    bool GetEntitiesFromBucket(const BucketKey& bucket, ETHEntityArray& out) const;
    bool GetEntityArray(const string& name, ETHEntityArray& out) const;
    // The scene's own seek (first match in bucket order); the Machine refuses
    // temporaries on top of it.
    std::shared_ptr<Entity> SeekEntity(int id) const;
    std::shared_ptr<Entity> SeekEntity(const string& name) const;
    // Every visible bucket (camera rect, inclusive at both ends, +1 ring when
    // border drawing is on), row-major top to bottom, at most 129
    // (GetIntersectingBuckets, ETHCommon.h:452-473).
    std::vector<BucketKey> VisibleBuckets(const vector2& camera, const vector2& screen) const;
    void GetVisibleEntities(const vector2& camera, const vector2& screen, ETHEntityArray& out) const;
    // Collide/CollideStatic/CollideDynamic (ETHScene.cpp:1362-1528): which = 0
    // any, 1 static only, 2 dynamic only.
    bool Collide(const Entity& entity, int which, ETHEntity* out) const;

    void SetBorderBucketsDrawing(bool draw) { m_borderBuckets = draw; }
    bool IsDrawingBorderBuckets() const { return m_borderBuckets; }

    int NumEntities() const;
    // 0.7.12's GetLastID returns the counter itself - the id the NEXT
    // AddEntity will get (ETHScene.cpp:1680-1683).
    int LastId() const { return m_idCounter; }

    // The running extremes of z (ETHScene.cpp:174,247-248): they start at 0 and
    // at the screen height, and only grow (on add, and at every render).
    float MinHeight() const { return m_minHeight; }
    float MaxHeight() const { return m_maxHeight; }
    void SetHeightRange(float minHeight, float maxHeight) {
        m_minHeight = minHeight;
        m_maxHeight = maxHeight;
    }

    // Every live entity, in bucket order (for SaveScene).
    std::vector<std::shared_ptr<Entity>> AllEntities() const;
    // The bucket list of a key (null when that bucket was never created).
    const EntityList* Bucket(const BucketKey& key) const;
    // The every-frame callback list (dynamic entities with a callback, and
    // temporaries), in insertion order. Entities are appended during iteration.
    std::vector<std::shared_ptr<Entity>>& DynamicOrTemporary() { return m_dynamicOrTemp; }

private:
    void Link(const std::shared_ptr<Entity>& entity, const BucketKey& key);
    void BindCallback(Entity& entity) const;

    Machine& m_machine;
    string m_fileName;
    vector2 m_bucketSize;
    SceneProperties m_properties;
    std::map<BucketKey, EntityList> m_buckets;
    std::vector<std::shared_ptr<Entity>> m_dynamicOrTemp;
    int m_idCounter = 0;
    bool m_borderBuckets = true;
    float m_minHeight = 0.0f;
    float m_maxHeight = 768.0f;
};

} // namespace Penumbra::Eth
