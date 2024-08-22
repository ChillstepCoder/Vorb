#pragma once

class World;
class Camera3D;
class GLDrawCommandBuffer;
class Mesh;
class MaterialShaderDef;

#include "definitions/FishDef.h"

struct FishGPUData {
    f32v3 mPosition;
    f32 mTurn;
    f32 mYaw;
    f32 mPitch;
    f32 mScale;
    f32 mTime;
};
static_assert(sizeof(FishGPUData) == 32);

class FishInstanceBatch {
public:
    FishInstanceBatch();
    ~FishInstanceBatch();

    VORB_NON_COPYABLE_BUT_MOVABLE(FishInstanceBatch);

    FishGPUData* mMappedInstanceDataBuffer = nullptr;
    VGBuffer mInstanceDataBuffer = 0;
    ModelID mModelID = INVALID_MODEL_ID;
    AssetHandlePtr<FishDef> mHandle;
};

class FishRenderer {
public:
    FishRenderer();
    ~FishRenderer();

    void renderFishEcosystem(const Camera3D& camera, const World& world);
    void debugRenderFishEcosystem(const World& world);

private:
    void addFishInstance(AssetID fish, f32v3 pos, f32v2 yawPitch, f32 scale, f32 turn, f32 time);
    int mDebugTickCounter = 0;

    // TODO: We really need to batch multiple fish models into a single VBO
    // One buffer per fish ID
    FlatMap<AssetID, ui32> mFishInstanceDataIndexThisFrame;
    std::vector<FishInstanceBatch> mFishInstanceBatches;
    std::vector<ui32> mInstanceCountsThisFrame;
    GLsync mFence[3] = { 0 };
    int mFrameIndex = 0;
    AssetHandlePtr<MaterialShaderDef> mFishShaderHandle;
    const MaterialShaderDef* mFishShader = nullptr;
};

