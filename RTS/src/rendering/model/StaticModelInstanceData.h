#pragma once

class GLIndirectBuffer;

struct ModelInstanceOwner {
    TileContainerID containerId;
    TileIndex tileIndex;
};

class StaticModelInstanceData
{
public:
    // Instance data for a specific model ID (TODO: Multiple models packed)
    StaticModelInstanceData();
    ~StaticModelInstanceData();

    // TODO: Optimize allocation
    std::vector<f32m4> mInstanceTransforms;
    std::vector<ModelInstanceOwner> mInstanceOwners;
    std::unique_ptr<GLIndirectBuffer> mDrawCommands;
    std::unique_ptr<GLIndirectBuffer> mDrawCommandsShadows;
    ui32 mShadowDrawCommandsCount = 0;
    VGBuffer mTransformsVbo = 0;
    ui32 mTransformsVboSizeBytes = 0;
    ui32 mFirstDirtyInstance = UINT32_MAX;

    // TODO: Investigate why, hardware? Driver? - Compact indirect buffer is actually slower due to atomic operation and cpu-gpu sync
    // std::unique_ptr<GLIndirectBuffer> mOutDrawCommands;
    //GLBuffer mNumVisibleMeshesBuffer;
    //volatile uint32_t* mNumVisibleMeshesBufferPtr = nullptr;
    // GLsync mFenceSync = 0;
};

// Stores all specific instances of a given model in the world
typedef std::map<ModelID, StaticModelInstanceData> ModelInstanceMap;
