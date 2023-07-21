#pragma once

#include "rendering/model/StaticModelInstance.h"
#include "rendering/model/StaticMeshInstanceData.h"
#include "rendering/model/MaterialRenderPassType.h"

#include <boost/container/flat_map.hpp>

#include "tile/TileHandle.h"

struct TileContainerEvent;
class Camera3D;
class InstancedStaticModelGatherer;
class ModelRepository;

DECL_VG(class GLProgram);

struct TileModelPositionKey {
    bool operator<(const TileModelPositionKey& rhs) const { return tileIndex < rhs.tileIndex; }
    TileIndex tileIndex;
};

// Allows us to look up the specific model at a position for a tile container
typedef std::map<TileModelPositionKey, TileModelInstance> SpatialInstanceDataMap;

// TODO: RENAME InstancedStaticMeshManager
class InstancedStaticModelManager
{
public:
    InstancedStaticModelManager();
    ~InstancedStaticModelManager();

    void frameUpdate(const Camera3D& camera, f32 elapsedSec);

    void addInstanceAtPosition(TileContainerID containerId, TileIndex tileIndex, ModelID modelId, const f32v3& position, f32 rotation);
    void removeInstanceAtPosition(TileContainerID containerId, TileIndex tileIndex);
    bool getInstancesAtPosition(LiteTileHandle tileHandle, OUT TileModelInstance* outInstances[e_cast(MaterialRenderPassType::COUNT)]);
    bool hasInstanceAtPosition(LiteTileHandle tileHandle);
    void addInstancesFromGatherer(InstancedStaticModelGatherer& gatherer);
    void removeInstancesFromContainer(TileContainerID containerId);
    ui32 getNumModels() const;

    void playAnimationOnInstanceAtPosition(LiteTileHandle targetTile, StaticModelAnimationTypes animType, f32v2 direction);

    // UNUSED
    void onContainerEditEvent(const TileContainerEvent& evnt);

    void onTileDamagedEvent(const TileContainerEvent& evnt);

    const ModelInstanceMap& getModelInstanceMapForRenderPass(MaterialRenderPassType renderPassType) const { return mModelsToInstances[e_cast(renderPassType)]; }
    const ModelInstanceMap* getAllModelInstanceMaps() const { return mModelsToInstances; }
private:
    void updateAnimatedModels(f32 elapsedSec);
    void removeTileModelInstanceInternal(int renderPassIndex, TileModelInstance& instance);

    boost::container::flat_map<LiteTileHandle, StaticMeshAnimation> mAnimatedInstances;
    ModelInstanceMap mModelsToInstances[e_count(MaterialRenderPassType)];
    std::map<TileContainerID, SpatialInstanceDataMap> mTileContainerModels[e_count(MaterialRenderPassType)];
    GLBuffer mGpuCullingUniformBuffer;
    const ModelRepository& mModelRepository;

    const vg::GLProgram* mCullingComputeShader = nullptr;
};

