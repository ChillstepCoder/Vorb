#pragma once

class btTriangleIndexVertexArray;
class btRigidBody;
class btBvhTriangleMeshShape;
struct btIndexedMesh;
class PhysicsWorld;
struct StaticPhysicsMesh;

class StaticPhysicsMeshBuilder
{
    friend class PhysicsWorld;
public:
    StaticPhysicsMeshBuilder() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(StaticPhysicsMeshBuilder);

    void setRootPos(const f32v3& rootPos) { mRootPos = rootPos; }
    const f32v3& getRootPos() const { return mRootPos; }
    
    void reserveQuadCount(ui32 count);
    void addTileQuad(f32v3 tilePosition, const f32v2& xyDims, CubeFacing axis);
    void addQuadBetweenPoints(const f32v3 vertPoints[4]);
    void addQuadBetweenPoints(const f32v3& v0, const f32v3& v1, const f32v3& v2, const f32v3& v3);
    void addTriangleBetweenPoints(const f32v3 vertPoints[3]);

    void finish(PhysicsWorld& physicsWorld, OUT StaticPhysicsMesh& outMesh);

private:
    f32v3 mRootPos = f32v3(0.0f);
    std::vector<f32v3> mVerts;
    std::vector<ui32> mIndices;
};

