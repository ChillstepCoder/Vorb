#pragma once

#include "rendering/model/StaticModelInstance.h"

// TODO: Allow chunks to reference each of their tiles part of a mesh buffer. Allow removing and compacting the mesh buffer instead of full rebuild
// Use TileIndex as key to reference their mesh data so we can dynamically update it.
//   Queue tile mesh updates, then do them all in a single pass then do a compaction pass on the buffer
struct InstanceDataBlock {
    // TODO: Pooled allocator
    std::vector<StaticModelInstance> data;
    ui32 commandBufferStartIndex;
    ui32 commandBufferLengthBytes;
};

typedef std::map<ModelID, InstanceDataBlock*> InstanceDataMap;

class Camera3D;
class InstancedStaticModelGatherer;
class Material;
class GLIndirectBuffer;

DECL_VG(class GLProgram);


// Instance data for a specific model ID (TODO: Multiple models packed)
struct StaticModelInstanceData {
    StaticModelInstanceData();
    ~StaticModelInstanceData();

    // TODO: Optimize allocation
    std::vector<std::unique_ptr<InstanceDataBlock>> mInstances;
    ui32 firstDirtyBlockIndex = ; //TODO
    std::unique_ptr<GLIndirectBuffer> mDrawCommands;
    VGBuffer mTransformsVbo = 0;
    ui32 mTransformsVboSizeBytes = 0;
    bool mDirtyDrawCommands = false;

    // TODO: Investigate why, hardware? Driver? - Compact indirect buffer is actually slower due to atomic operation and cpu-gpu sync
    // std::unique_ptr<GLIndirectBuffer> mOutDrawCommands;
    //GLBuffer mNumVisibleMeshesBuffer;
    //volatile uint32_t* mNumVisibleMeshesBufferPtr = nullptr;
    //GLsync mFenceSync = 0;
};

class InstancedStaticModelRenderer
{
public:
    InstancedStaticModelRenderer();
    ~InstancedStaticModelRenderer();

    void frameUpdate(const Camera3D& camera);

    void addInstance(ModelID modelId, const f32v3& position, f32 rotation);
    void renderModels(const Camera3D& camera);
    void renderModelShadows(const Camera3D& camera, const f32* shadowDistances);
    void addInstancesFromGatherer(InstancedStaticModelGatherer& gatherer);
    void removeInstancesFromContainer(TileContainerID containerId);
    ui32 getNumModels() const;
private:
    std::map<ModelID, StaticModelInstanceData> mInstances;
    std::map<TileContainerID, InstanceDataMap> mModelsPerTileContainer;
    GLBuffer mGpuCullingUniformBuffer;

    const Material* mStandardMaterial = nullptr;
    const Material* mShadowMapperMaterial = nullptr;
    const vg::GLProgram* mCullingComputeShader = nullptr;
};

