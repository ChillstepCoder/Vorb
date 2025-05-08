#pragma once

namespace JPH {
    class Shape;
}

#include <variant>

#include "tile/TileHandle.h"
#include "physics/PhysicsBodyUserData.h"
#include "physics/PhysicsShapeUserData.h"

struct PhysHitResult {
    PhysBodyID mHitBody = INVALID_PHYS_BODY_ID;
    f32v3 mPosition;
    f32v3 mNormal; // Not normalized for raycasts
    f32 mTime = 1.0f;
    f32 mPenetrationDepth = 0.0f;
    PhysicsBodyUserData mBodyUserData;
    const JPH::Shape* mShape = nullptr;

    bool didHit() const { return mTime < 1.0f; }
};

enum class PhysicsPickQueryFlags : ui8 {
    QUERY_TILE_INFO = BIT(0)
};

struct PhysicsQueryResult {
    const JPH::Shape* mShape = nullptr;
    PhysicsBodyUserData mBodyUserData;
    PhysBodyID mPhysBody = INVALID_PHYS_BODY_ID;
    f32v3 mCenterOfMassPosition = f32v3(0);
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