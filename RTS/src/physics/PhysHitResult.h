#pragma once

struct PhysHitResult {
    const class btCollisionObject* mCollisionObject = nullptr;
    f32v3 mPosition;
    f32v3 mNormal;
    f32 mTime;

    bool didHit() const { return mCollisionObject != nullptr; }
};

// Allows thread to request the physics engine to perform a pick that will be populated eventually.
// Intended for things like editor tile picking where we dont need instant response
class DeferredPhysicsPick {
public:

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
    std::mutex mMutex;
    bool mDone = false;
};