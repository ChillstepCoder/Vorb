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
    TrackedStaticRigidBodyGatherer() = delete;
    VORB_NON_COPYABLE_BUT_MOVABLE(TrackedStaticRigidBodyGatherer);

    TrackedStaticRigidBodyGatherer(TileContainerID containerId) : mContainerId(containerId) {};
    void addRigidBody(TileIndex ownerTilePosition, const f32v3& pos, CollisionShapeID shapeId) { assert(shapeId != INVALID_COLLISION_SHAPE_ID); mRigidBodiesToAdd.emplace_back(TrackedStaticRigidBody{ pos, shapeId, ownerTilePosition }); }
    TileContainerID getOwnerTileContainerID() const { return mContainerId; }
    size_t getNumStaticObjectsToAdd() const { return mRigidBodiesToAdd.size(); }
private:
    std::vector<TrackedStaticRigidBody> mRigidBodiesToAdd;
    TileContainerID mContainerId;
};