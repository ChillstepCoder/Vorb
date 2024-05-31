#pragma once

#include "rendering/model/StaticModelInstance.h"
#include "rendering/model/StaticModelBatchData.h"
#include "rendering/model/MaterialRenderPassType.h"

#include "definitions/ModelDef.h"

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
    ui8 variantIndex;
};

// Currently only supports one model instance per tile
class InstancedStaticModelManager
{
public:
    InstancedStaticModelManager();
    ~InstancedStaticModelManager();

    void frameUpdate(const Camera3D& camera, f32 elapsedSec);

    // Tile models
    void addTileInstanceAtPosition(TileContainerID containerId, TileIndex tileIndex, ModelID modelId, f32v3 position, f32 rotation, ui8 variantIndex);
    void removeTileInstanceAtPosition(TileContainerID containerId, TileIndex tileIndex);
    TileModelInstance* getTileInstanceAtPosition(LiteTileHandle tileHandle);
    bool hasTileInstanceAtPosition(LiteTileHandle tileHandle);
    void addTileInstancesFromGatherer(InstancedStaticModelGatherer& gatherer);
    void removeTileInstancesFromContainer(TileContainerID containerId);
    ui32 getNumModels() const;
    
    // Entity models

    void playAnimationOnInstanceAtPosition(LiteTileHandle targetTile, StaticModelAnimationTypes animType, f32v2 direction);

    // UNUSED
    void onContainerEditEvent(const TileContainerEvent& evnt);

    void onTileDamagedEvent(const TileContainerEvent& evnt);

    // TODO: This is more data than the renderer needs?
    const ModelInstanceMap& getModelInstanceMap() const { return mModelsToInstances; }
private:
    void addInstanceAtPositionInternal(const ModelDef& modelDef, TileContainerID containerId, TileIndex tileIndex, const f32m4& transform, ui8 variantIndex);

    void updateAnimatedModels(f32 elapsedSec);
    void removeTileModelInstanceInternal(TileModelInstance& instance);
    void decrefModelDef(ModelID modelId, int decCount);

    boost::container::flat_map<LiteTileHandle, StaticMeshAnimation> mAnimatedInstances;
    ModelInstanceMap mModelsToInstances;
    std::map<TileContainerID, SpatialInstanceDataMap> mTileContainerModels;
    GLBuffer mGpuCullingUniformBuffer;

    // Refcount ModelDefs
    std::unordered_map<ModelID, ModelDefRef> mModelDefRefs;

    AssetHandlePtr<MaterialShaderDef> mCullingComputeShader;

    std::unique_ptr<ModelBillboardLodManager> mBillboardLodManager;

    std::unordered_map<ModelID, boost::container::flat_map<StaticModelInstanceID, StaticModelInstance>> mStaticModelInstances;
};

