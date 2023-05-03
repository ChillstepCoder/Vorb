#pragma once

#include "rendering/model/StaticModelInstance.h"
#include "rendering/model/StaticModelInstanceData.h"
#include "rendering/model/MaterialRenderPassType.h"

struct TileContainerEvent;
class Camera3D;
class InstancedStaticModelGatherer;

DECL_VG(class GLProgram);

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

// Allows us to look up the specific model at a position for a tile container
typedef std::map<TileModelPositionKey, TileModelInstance> SpatialInstanceDataMap;

class InstancedStaticModelManager
{
public:
    InstancedStaticModelManager();
    ~InstancedStaticModelManager();

    void frameUpdate(const Camera3D& camera);

    void addInstanceAtPosition(TileContainerID containerId, TileIndex tileIndex, ModelID modelId, const f32v3& position, f32 rotation);
    void removeInstanceAtPosition(TileContainerID containerId, TileIndex tileIndex);
    void addInstancesFromGatherer(InstancedStaticModelGatherer& gatherer);
    void removeInstancesFromContainer(TileContainerID containerId);
    ui32 getNumModels() const;

    void onContainerEditEvent(const TileContainerEvent& evnt);

    const ModelInstanceMap& getModelInstanceMapForRenderPass(MaterialRenderPassType renderPassType) const { return mModelsToInstances[e_cast(renderPassType)]; }
    const ModelInstanceMap* getAllModelInstanceMaps() const { return mModelsToInstances; }
private:
    void removeTileModelInstanceInternal(TileModelInstance& instance);

    ModelInstanceMap mModelsToInstances[e_cast(MaterialRenderPassType::COUNT)];
    std::map<TileContainerID, SpatialInstanceDataMap> mTileContainerModels;
    GLBuffer mGpuCullingUniformBuffer;

    const vg::GLProgram* mCullingComputeShader = nullptr;
};

