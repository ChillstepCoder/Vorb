#pragma once

struct PhysHitResult {
    const class btCollisionObject* mCollisionObject = nullptr;
    f32v3 mPosition;
    f32v3 mNormal;
    f32 mTime;

    bool didHit() const { return mCollisionObject != nullptr; }
};
