#pragma once

#include "physics/TrackedStaticRigidBodyGatherer.h"

class NewPhysicsWorld;

class StaticPhysicsMeshBuilder
{
    friend class NewPhysicsWorld;
public:
    StaticPhysicsMeshBuilder(TileContainerID tileContainerOwner) : mTrackedRigidBodyGatherer(tileContainerOwner) {};
    ~StaticPhysicsMeshBuilder();
    VORB_NON_COPYABLE_BUT_MOVABLE(StaticPhysicsMeshBuilder);

    void setRootPos(f32v3 rootPos) { mRootPos = rootPos; }
    f32v3 getRootPos() const { return mRootPos; }
    
    void reserveQuadCount(ui32 count);
    void addTileQuad(f32v3 tilePosition, const f32v2& xyDims, CubeFacing axis);
    void addQuadBetweenPoints(const f32v3 vertPoints[4]);
    void addQuadBetweenPoints(const f32v3& v0, const f32v3& v1, const f32v3& v2, const f32v3& v3);
    void addTriangleBetweenPoints(const f32v3 vertPoints[3]);
    void addTrackedStaticRigidBody(TileIndex ownerTilePosition, TileID id, ui8 layer, const f32v3& pos, CollisionShapeID shapeId) { mTrackedRigidBodyGatherer.addRigidBody(ownerTilePosition, id, layer, pos, shapeId); }
    TileContainerID getOwnerTileContainerID() const { return mTrackedRigidBodyGatherer.getOwnerTileContainerID(); }

    bool hasAnyCollision();
    void finish(NewPhysicsWorld& physicsWorld);

private:
    TrackedStaticRigidBodyGatherer mTrackedRigidBodyGatherer;
    f32v3 mRootPos = f32v3(0.0f);
    std::vector<f32v3> mVerts;
    std::vector<ui32> mIndices;
};

