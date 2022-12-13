#pragma once

#include "rendering/model/StaticModelInstanceTransform.h"

// TODO: Allow chunks to reference each of their tiles part of a mesh buffer. Allow removing and compacting the mesh buffer instead of full rebuild
// Use TileIndex as key to reference their mesh data so we can dynamically update it.
//   Queue tile mesh updates, then do them all in a single pass then do a compaction pass on the buffer
struct TileModelInstance {
    ModelID mModelID = 0;
    ui32 mInstanceIndex = 0; // Index into the transforms array
};
static_assert(sizeof(TileModelInstance) == 8, "Keep tiny");

// Allows us to look up the specific model at a position
typedef std::map<f32v3 /*Position offset*/, TileModelInstance, f32v3cmp> InstanceDataMap;

class Camera3D;
class InstancedStaticModelGatherer;
class MaterialShader;
class GLIndirectBuffer;

DECL_VG(class GLProgram);


// Instance data for a specific model ID (TODO: Multiple models packed)
struct StaticModelInstanceData {
    StaticModelInstanceData();
    ~StaticModelInstanceData();

    // TODO: Optimize allocation
    std::vector<StaticModelInstanceTransform> mInstanceTransforms;
    std::vector<TileContainerID> mInstanceOwners;
    std::unique_ptr<GLIndirectBuffer> mDrawCommands;
    VGBuffer mTransformsVbo = 0;
    ui32 mTransformsVboSizeBytes = 0;
    ui32 mFirstDirtyInstance = UINT32_MAX;

    // TODO: Investigate why, hardware? Driver? - Compact indirect buffer is actually slower due to atomic operation and cpu-gpu sync
    // std::unique_ptr<GLIndirectBuffer> mOutDrawCommands;
    //GLBuffer mNumVisibleMeshesBuffer;
    //volatile uint32_t* mNumVisibleMeshesBufferPtr = nullptr;
   // GLsync mFenceSync = 0;
};

class InstancedStaticModelRenderer
{
public:
    InstancedStaticModelRenderer();
    ~InstancedStaticModelRenderer();

    void frameUpdate(const Camera3D& camera);

    void addInstance(ModelID modelId, const f32v3& position, f32 rotation);
    void removeInstanceAtPosition(TileContainerID containerId, const f32v3& position);
    void renderModels(const Camera3D& camera);
    void renderModelShadows(const Camera3D& camera, const f32* shadowDistances);
    void addInstancesFromGatherer(InstancedStaticModelGatherer& gatherer);
    void removeInstancesFromContainer(TileContainerID containerId);
    ui32 getNumModels() const;
private:
    std::map<ModelID, StaticModelInstanceData> mModelsToInstances;
    std::map<TileContainerID, InstanceDataMap> mTileContainerModels;
    GLBuffer mGpuCullingUniformBuffer;

    const MaterialShader* mStandardMaterial = nullptr;
    const MaterialShader* mShadowMapperMaterial = nullptr;
    const vg::GLProgram* mCullingComputeShader = nullptr;
};

