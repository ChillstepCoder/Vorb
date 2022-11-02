#pragma once

class btRigidBody;

struct StaticPhysicsMesh {
    ~StaticPhysicsMesh();

    std::unique_ptr<btTriangleIndexVertexArray> mPhysicsMesh;
    std::unique_ptr<btBvhTriangleMeshShape> mShape;
    btRigidBody* mRigidBody = nullptr;
};