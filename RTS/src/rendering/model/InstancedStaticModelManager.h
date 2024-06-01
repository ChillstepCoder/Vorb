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

// Allows us to look up the specific model at a position for a tile container
typedef std::map<TileIndex, TileModelInstance> SpatialInstanceDataMap;

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
    const ModelBatchMap& getModelInstanceMap() const { return mModelBatches; }

    StaticModelInstanceID addLooseModelInstance(ModelID modelId, const glm::quat& orient, f32v3 position, ui8 variantIndex);
    void removeLooseModelInstance(ModelID modelId, StaticModelInstanceID instanceId);
private:
    void updatePendingLooseModelInstances();

    void removeModelInstanceInternal(StaticModelBatchData& batchData, ui32 instanceIndex, ModelID modelId);

    void addTileInstanceInternal(const ModelDef& modelDef, TileContainerID containerId, TileIndex tileIndex, const f32m4& transform, ui8 variantIndex);
    void removeTileInstanceInternal(TileModelInstance& instance);

    void addLooseInstanceInternal(ModelID modelId, StaticModelInstanceID instanceId, const f32m4& transform, ui8 variantIndex);
    void removeLooseInstanceInternal(ModelID modelId, StaticModelInstanceID instanceId);

    void updateAnimatedModels(f32 elapsedSec);
    void increfModelDef(ModelID modelId, int incCount);
    void decrefModelDef(ModelID modelId, int decCount);

    boost::container::flat_map<LiteTileHandle, StaticMeshAnimation> mAnimatedTileInstances;
    ModelBatchMap mModelBatches;
    std::map<TileContainerID, SpatialInstanceDataMap> mTileContainerTrackedModels;
    GLBuffer mGpuCullingUniformBuffer;

    // Refcount ModelDefs
    std::unordered_map<ModelID, ModelDefRef> mModelDefRefs;

    AssetHandlePtr<MaterialShaderDef> mCullingComputeShader;

    std::unique_ptr<ModelBillboardLodManager> mBillboardLodManager;

    struct PendingLooseModelInstance {
        glm::quat orient;
        f32v3 position;
        ModelID modelId;
        StaticModelInstanceID instanceId;
        ui8 variantIndex;
        bool isRemove;
    };
    moodycamel::ConcurrentQueue<PendingLooseModelInstance> mPendingLooseModelInstances;

    std::unordered_map<ModelID, boost::container::flat_map<StaticModelInstanceID, ui32 /*instanceIndex*/>> mLooseStaticModelInstances;

    std::mutex mLooseInstanceIDMutex;
    boost::container::flat_map<ModelID, StaticModelInstanceID> mNextLooseInstanceIDs;
};

