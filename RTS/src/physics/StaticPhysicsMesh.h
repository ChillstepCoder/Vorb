#pragma once

class btRigidBody;

#include "BulletCollision/CollisionShapes/btTriangleIndexVertexArray.h"
#include "BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h"

struct StaticPhysicsMesh {
    StaticPhysicsMesh() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(StaticPhysicsMesh);

    std::unique_ptr<btTriangleIndexVertexArray> mPhysicsMesh;
    std::unique_ptr<btBvhTriangleMeshShape> mShape;
    btRigidBody* mRigidBody = nullptr;

    // We hold onto these directly because the physics engine uses it
    std::vector<f32v3> mVerts;
    std::vector<ui32> mIndices;
};