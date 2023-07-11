#pragma once

#include <variant>

#include "tile/TileHandle.h"

class btCollisionObject;

enum class PhysicsHitObjectType : ui8 {
    Tile,
    Entity,
    Terrain,
    Invalid,
    COUNT
};

struct PhysHitResult {
    const btCollisionObject* mCollisionObject = nullptr;
    f32v3 mPosition;
    f32v3 mNormal;
    f32 mTime;
    entt::entity mSelectedEntity = INVALID_ENTITY;
    TileContainerID mContainerID = INVALID_TILE_CONTAINER_ID;
    TileIndex mTileIndex = INVALID_TILE_INDEX;

    bool didHit() const { return mCollisionObject != nullptr; }
};

enum class PhysicsPickQueryFlags : ui8 {
    QUERY_TILE_INFO = BIT(0)
};

struct PhysicsQueryResult {
    const btCollisionObject* mCollisionObject = nullptr;
    std::variant<entt::entity, LiteTileHandle> mObject;
};

// Allows thread to request the physics engine to perform a pick that will be populated eventually.
// Intended for things like editor tile picking where we don't need instant response
class DeferredPhysicsPick {
public:

    void setQueryFlags(BitFlags<PhysicsPickQueryFlags> queryFlags) {
        mQueryFlags = queryFlags;
    }
    BitFlags<PhysicsPickQueryFlags> getQueryFlags() const { return mQueryFlags; }

    void setPickResult(const PhysHitResult& hitResult) {
        std::lock_guard lock(mMutex);
        lastResult = hitResult;
        mDone = true;
    }

    bool isDone() {
        std::lock_guard lock(mMutex);
        return mDone;
    }

    PhysHitResult getLastPickResult() {
        std::lock_guard lock(mMutex);
        return lastResult;
    }

    void clearDone() {
        mDone = false;
    }
private:
    PhysHitResult lastResult;
    std::mutex mMutex; // TODO: Too heavy?
    bool mDone = false;
    BitFlags<PhysicsPickQueryFlags> mQueryFlags;
};