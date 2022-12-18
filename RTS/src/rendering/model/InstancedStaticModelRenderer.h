#pragma once

#include "rendering/model/StaticModelInstance.h"

#include "tile/TileContainerEvents.h"

// TODO: Allow chunks to reference each of their tiles part of a mesh buffer. Allow removing and compacting the mesh buffer instead of full rebuild
// Use TileIndex as key to reference their mesh data so we can dynamically update it.
//   Queue tile mesh updates, then do them all in a single pass then do a compaction pass on the buffer
struct TileModelInstance {
    ModelID mModelID = 0;
    ui32 mInstanceIndex = 0; // Index into the transforms array
};
static_assert(sizeof(TileModelInstance) == 8, "Keep tiny");

struct TileModelPositionKey {
    bool operator<(const TileModelPositionKey& rhs) const { return tileIndex < rhs.tileIndex; }
    TileIndex tileIndex;
};

struct ModelInstanceOwner {
    TileContainerID containerId;
    TileIndex tileIndex;
};

// Allows us to look up the specific model at a position for a tile container
typedef std::map<TileModelPositionKey, TileModelInstance> SpatialInstanceDataMap;

class Camera3D;
class InstancedStaticModelGatherer;
class MaterialShader;
class GLIndirectBuffer;

DECL_VG(class GLProgram);

struct TileContainerModelEditEvent;

// Instance data for a specific model ID (TODO: Multiple models packed)
struct StaticModelInstanceData {
    StaticModelInstanceData();
    ~StaticModelInstanceData();

    // TODO: Optimize allocation
    std::vector<f32m4> mInstanceTransforms;
    std::vector<ModelInstanceOwner> mInstanceOwners;
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

    void addInstanceAtPosition(TileContainerID containerId, TileIndex tileIndex, ModelID modelId, const f32v3& position, f32 rotation);
    void removeInstanceAtPosition(TileContainerID containerId, TileIndex tileIndex);
    void renderModels(const Camera3D& camera);
    void renderModelShadows(const Camera3D& camera, const f32* shadowDistances);
    void addInstancesFromGatherer(InstancedStaticModelGatherer& gatherer);
    void removeInstancesFromContainer(TileContainerID containerId);
    ui32 getNumModels() const;

private:
    void initEventHandlers();
    void onModelEditEvent(TileContainerModelEditEvent& evnt);
    void removeTileModelInstanceInternal(TileModelInstance& instance);

    std::map<ModelID, StaticModelInstanceData> mModelsToInstances;
    std::map<TileContainerID, SpatialInstanceDataMap> mTileContainerModels;
    GLBuffer mGpuCullingUniformBuffer;

    const MaterialShader* mStandardMaterial = nullptr;
    const MaterialShader* mShadowMapperMaterial = nullptr;
    const vg::GLProgram* mCullingComputeShader = nullptr;

    TileContainerListeners mTileContainerEventListeners;
};

