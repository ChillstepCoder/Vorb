#pragma once

class btRigidBody;

#include "BulletCollision/CollisionShapes/btTriangleIndexVertexArray.h"
#include "BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h"

struct StaticPhysicsMesh {
    std::unique_ptr<btTriangleIndexVertexArray> mPhysicsMesh;
    std::unique_ptr<btBvhTriangleMeshShape> mShape;
    btRigidBody* mRigidBody = nullptr;
};