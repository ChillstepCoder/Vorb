#pragma once

#include "physics/CollisionShapes.h"

struct TrackedStaticRigidBody {
    f32v3 position;
    CollisionShapeID shapeId;
    TileIndex ownerTilePosition;
};
static_assert(sizeof(TrackedStaticRigidBody) == 20, "Keep tiny");

// Gathers static rigid bodies to track for a particular tile container
class TrackedStaticRigidBodyGatherer {
public:
    friend class PhysicsWorld;
    TrackedStaticRigidBodyGatherer() = default;
    ~TrackedStaticRigidBodyGatherer() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(TrackedStaticRigidBodyGatherer);

    TrackedStaticRigidBodyGatherer(TileContainerID containerId) : mContainerId(containerId) {};
    void addRigidBody(TileIndex ownerTilePosition, const f32v3& pos, CollisionShapeID shapeId) { mRigidBodiesToAdd.emplace_back(TrackedStaticRigidBody{ pos, shapeId, ownerTilePosition }); }
private:
    std::vector<TrackedStaticRigidBody> mRigidBodiesToAdd;
    TileContainerID mContainerId;
};