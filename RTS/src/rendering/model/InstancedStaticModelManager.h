#pragma once

#include "rendering/model/StaticModelInstance.h"
#include "rendering/model/StaticMeshInstanceData.h"
#include "rendering/model/MaterialRenderPassType.h"

#include "definitions/ModelDef.h"

#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>

#include "tile/TileHandle.h"

struct TileContainerEvent;
class Camera3D;
class InstancedStaticModelGatherer;
class ModelRepository;
class MaterialShaderDef;
class ModelBillboardLodManager;

DECL_VG(class GLProgram);

struct TileModelPositionKey {
    bool operator<(const TileModelPositionKey& rhs) const { return tileIndex < rhs.tileIndex; }
    TileIndex tileIndex;
};

// Allows us to look up the specific model at a position for a tile container
typedef std::map<TileModelPositionKey, TileModelInstance> SpatialInstanceDataMap;

struct ModelDefRef {
    AssetHandlePtr<ModelDef> handle;
    int refCount = 1;
};

struct PendingModelInstance {
    TileContainerID containerId;
    TileIndex tileIndex;
    f32m4 transform;
};

// TODO: RENAME InstancedStaticMeshManager
class InstancedStaticModelManager
{
public:
    InstancedStaticModelManager();
    ~InstancedStaticModelManager();

    void frameUpdate(const Camera3D& camera, f32 elapsedSec);

    void addInstanceAtPosition(TileContainerID containerId, TileIndex tileIndex, ModelID modelId, f32v3 position, f32 rotation);
    void removeInstanceAtPosition(TileContainerID containerId, TileIndex tileIndex);
    TileModelInstance* getInstanceAtPosition(LiteTileHandle tileHandle);
    bool hasInstanceAtPosition(LiteTileHandle tileHandle);
    void addInstancesFromGatherer(InstancedStaticModelGatherer& gatherer);
    void removeInstancesFromContainer(TileContainerID containerId);
    ui32 getNumModels() const;

    void playAnimationOnInstanceAtPosition(LiteTileHandle targetTile, StaticModelAnimationTypes animType, f32v2 direction);

    // UNUSED
    void onContainerEditEvent(const TileContainerEvent& evnt);

    void onTileDamagedEvent(const TileContainerEvent& evnt);

    // TODO: This is more data than the renderer needs?
    const ModelInstanceMap& getModelInstanceMap() const { return mModelsToInstances; }
private:
    void updatePendingModelDefs();
    void addInstanceAtPositionInternal(const ModelDef& modelDef, TileContainerID containerId, TileIndex tileIndex, const f32m4& transform);

    void updateAnimatedModels(f32 elapsedSec);
    void removeTileModelInstanceInternal(TileModelInstance& instance);
    void decrefModelDef(ModelID modelId, int decCount);

    boost::container::flat_map<LiteTileHandle, StaticMeshAnimation> mAnimatedInstances;
    ModelInstanceMap mModelsToInstances;
    std::map<TileContainerID, SpatialInstanceDataMap> mTileContainerModels;
    GLBuffer mGpuCullingUniformBuffer;

    // Refcount ModelDefs
    std::unordered_map<ModelID, ModelDefRef> mModelDefRefs;
    // Tracks models that are awaiting ModelDef load
    boost::container::flat_map<ModelID, std::vector<PendingModelInstance>> mPendingInstances;
    // Used to track removal of instances from containers while we wait for ModelDef load
    boost::container::flat_map<TileContainerID, boost::container::flat_set<ModelID>> mPendingInstanceForContainer;

    AssetHandlePtr<MaterialShaderDef> mCullingComputeShader;

    std::unique_ptr<ModelBillboardLodManager> mBillboardLodManager;
};

