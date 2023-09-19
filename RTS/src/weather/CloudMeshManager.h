#pragma once

class Mesh;
class IWorldGenerator;

#include "world/ChunkID.h"
#include "util/SpatialGrid2D.h"

#include "resources/asset/AssetHandleBundle.h"

struct CloudBatch {
    CloudBatch() = default;
    ~CloudBatch();

    VORB_MOVABLE(CloudBatch);

    f32v3 mRootPos;
    f32 mBoundsRadius; // TODO: AABB
    f32 mFadeAlpha;
    std::unique_ptr<Mesh> mMesh;
};

class CloudMeshManager
{
public:
    friend class CloudRenderer;
    CloudMeshManager(IWorldGenerator& worldGenerator);
    ~CloudMeshManager();

    void init(i32 worldWidthChunks, const f32v2& loadCenter);
    void frameUpdate(const f32v2& loadCenter);

private:
    void updateGridShift(const f32v2& loadCenter);
    void tryGenerateCloudBatchAt(i32v2 cloudPos);
    void destroyCloudBatch(CloudBatch& batch);
    void spawnNewCloudWaveX(i32 dir);
    void spawnNewCloudWaveY(i32 dir);

    ui32 mWorldWidthCloudBatches = 0;
    std::vector<CloudBatch> mCloudBatches;
    std::map<ui32, CloudBatch> mGeneratingBatches; // Use this so we dont need synchronization
    std::vector<i32v2> mCloudSpawnOffsets;
    std::unordered_map<i32 /*yOffset*/, i32 /*xOffset*/> mCloudBoundsCheckMap;
    SpatialGrid2D mSpatialGrid2D;
    IWorldGenerator& mWorldGenerator;
    i32v2 mLastCenterPosition;
    // This is actually genius - TODO: Can this be used for chunks?
    f32 mDx = 0.0f;
    f32 mDxTotal = 0.0f;
    f32 mDy = 0.0f;
    f32 mDyTotal = 0.0f;
    ui32 mTickCount = 0;
    ui32 mGeneratingIndexLast = 0;
    //void addCloudAt(const f32v3& pos, f32 size);

    AssetHandleBundle mAssets;

    //mutable std::unique_ptr<TBOBillboardMesh> mCloudMesh;
    //std::vector<Cloud> mClouds;
};

