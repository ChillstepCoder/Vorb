#pragma once

#include "physics/CollisionShapes.h"

struct TrackedStaticModelCollider {
    f32v3 position;
    f32q orientation;
    ModelID modelId;
    TileIndex ownerTilePosition;
    TileID tileId;
    f32 scale;
};
static_assert(sizeof(TrackedStaticModelCollider) == 44, "Keep small");

// Gathers static rigid bodies to track for a particular tile container
class TrackedStaticModelColliderGatherer {
public:
    friend class PhysicsWorld;
    TrackedStaticModelColliderGatherer() = delete;
    VORB_NON_COPYABLE_BUT_MOVABLE(TrackedStaticModelColliderGatherer);

    TrackedStaticModelColliderGatherer(TileContainerID containerId) : mContainerId(containerId) {};
    void addTileModelCollider(TileIndex ownerTilePosition, TileID id, f32v3 pos, f32q orientation, f32 scale, ModelID modelId) {
        assert(modelId != INVALID_MODEL_ID);
        mRigidBodiesToAdd.emplace_back(TrackedStaticModelCollider{ pos, orientation, modelId, ownerTilePosition, id, scale });
    }
    TileContainerID getOwnerTileContainerID() const { return mContainerId; }
    size_t getNumStaticObjectsToAdd() const { return mRigidBodiesToAdd.size(); }
private:
    std::vector<TrackedStaticModelCollider> mRigidBodiesToAdd;
    TileContainerID mContainerId;
};