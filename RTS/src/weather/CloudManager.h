#pragma once


class World;
class TBOBillboardMesh;
struct SubTexture;

#include "world/ChunkID.h"

struct CloudBatch {
    CloudBatch() = default;
    ~CloudBatch();

    VORB_MOVABLE(CloudBatch);

    f32v3 mRootPos;
    f32 mBoundsRadius; // TODO: AABB
    f32 mFadeAlpha;
    std::unique_ptr<TBOBillboardMesh> mMesh;
};

class CloudManager
{
public:
    friend class CloudRenderer;
    CloudManager(const World& world);
    ~CloudManager();

    void init();
    void update();

private:
    void updateGridShift();
    void tryGenerateCloudBatchAt(i32v2 cloudPos);
    void destroyCloudBatch(CloudBatch& batch);
    void spawnNewCloudWaveX(i32 dir);
    void spawnNewCloudWaveY(i32 dir);

    const SubTexture* mCloudTexture; // TODO: Mesher?
    std::vector<CloudBatch> mCloudBatches;
    std::map<ui32, CloudBatch> mGeneratingBatches; // Use this so we dont need synchronization
    std::vector<std::unique_ptr<TBOBillboardMesh>> mRecycledMeshes;
    std::vector<i32v2> mCloudSpawnOffsets;
    std::unordered_map<i32 /*yOffset*/, i32 /*xOffset*/> mCloudBoundsCheckMap;


    i32v2 mLastCenterPosition;
    // This is actually genius - TODO: Can this be used for chunks?
    f32 mDx = 0.0f;
    f32 mDxTotal = 0.0f;
    f32 mDy = 0.0f;
    f32 mDyTotal = 0.0f;
    ui32 mTickCount = 0;
    ui32 mGeneratingIndexLast = 0;
    //void addCloudAt(const f32v3& pos, f32 size);

    //mutable std::unique_ptr<TBOBillboardMesh> mCloudMesh;
    //std::vector<Cloud> mClouds;
    const World& mWorld;
};

