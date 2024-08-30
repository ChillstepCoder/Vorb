#pragma once

#include "physics/TrackedStaticModelColliderGatherer.h"

class PhysicsWorld;

class StaticPhysicsMeshBuilder
{
    friend class PhysicsWorld;
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
    void addTrackedTileModelCollider(TileIndex ownerTilePosition, TileID id, ui8 layer, f32v3 pos, f32q orientation, f32 scale, ModelID modelId) { mTrackedRigidBodyGatherer.addTileModelCollider(ownerTilePosition, id, layer, pos, orientation, scale, modelId); }
    TileContainerID getOwnerTileContainerID() const { return mTrackedRigidBodyGatherer.getOwnerTileContainerID(); }

    bool hasAnyCollision();
    void finish(PhysicsWorld& physicsWorld);

private:
    TrackedStaticModelColliderGatherer mTrackedRigidBodyGatherer;
    f32v3 mRootPos = f32v3(0.0f);
    std::vector<f32v3> mVerts;
    std::vector<ui32> mIndices;
};

