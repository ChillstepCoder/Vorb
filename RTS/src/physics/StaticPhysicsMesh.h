#pragma once

class btTriangleIndexVertexArray;
class btRigidBody;
class btBvhTriangleMeshShape;
class btIndexedMesh;

class StaticPhysicsMesh
{
    friend class PhysicsWorld;
public:
    StaticPhysicsMesh();
    ~StaticPhysicsMesh();
    VORB_NON_COPYABLE_BUT_MOVABLE(StaticPhysicsMesh);

    void setRootPos(const f32v3& rootPos) { mRootPos = rootPos; }
    const f32v3& getRootPos() const { return mRootPos; }
    
    void reserveQuadCount(ui32 count);
    void addTileQuad(f32v3 tilePosition, const f32v2& xyDims, CubeFacing axis);
    void addQuadBetweenPoints(const f32v3 vertPoints[4]);
    void addTriangleBetweenPoints(const f32v3 vertPoints[3]);

    void finish();

private:
    f32v3 mRootPos = f32v3(0.0f);
    std::vector<f32v3> mVerts;
    std::vector<ui32> mIndices;
    std::unique_ptr<btTriangleIndexVertexArray> mPhysicsMesh;
    std::unique_ptr<btBvhTriangleMeshShape> mShape;
    btRigidBody* mRigidBody = nullptr;
};

