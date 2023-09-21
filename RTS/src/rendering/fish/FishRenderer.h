#pragma once

class IWorld;
class Camera3D;
class GLIndirectBuffer;
class Mesh;
class MaterialShader;

#include <boost/container/flat_map.hpp>

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

class FishInstanceData {
public:
    FishInstanceData();
    ~FishInstanceData();

    FishGPUData* mMappedInstanceDataBuffer = nullptr;
    VGBuffer mInstanceDataBuffer = 0;
    const Mesh* mMesh = nullptr;
    AssetHandlePtr<FishDef> mHandle;
};

class FishRenderer {
public:
    FishRenderer();
    ~FishRenderer();

    void renderFishEcosystem(const Camera3D& camera, const IWorld& world);
    void debugRenderFishEcosystem(const IWorld& world);

private:
    void addFishInstance(AssetID fish, f32v3 pos, f32v2 yawPitch, f32 scale, f32 turn, f32 time);
    int mDebugTickCounter = 0;

    // TODO: We really need to batch multiple fish models into a single VBO
    // One buffer per fish ID
    boost::container::flat_map<AssetID, FishInstanceData> mFishInstanceData;
    std::vector<ui32> mInstanceCountsThisFrame;
    GLsync mFence[3] = { 0 };
    int mFrameIndex = 0;
    const MaterialShader* mFishShader;
};

