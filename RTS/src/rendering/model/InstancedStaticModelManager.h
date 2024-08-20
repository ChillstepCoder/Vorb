#pragma once

#include "rendering/model/StaticModelInstance.h"
#include "rendering/model/StaticModelRendererBatchData.h"
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

typedef FlatMap<StaticModelInstanceID, ui32 /*instanceIndex*/> InstanceIDToIndexMap;
//  std::unordered_map For pointer stability
typedef std::unordered_map<ModelID, InstanceIDToIndexMap> LooseStaticModelInstanceMap;


using InstanceVariantIndexType = ui32;

// Currently only supports one model instance per tile
class InstancedStaticModelManager
{
    friend class InstancedStaticModelRenderer;
public:
    InstancedStaticModelManager();
    ~InstancedStaticModelManager();

    void frameUpdate(const Camera3D& camera, f32 elapsedSec);

    // Tile models
    void addTileInstanceAtPosition(TileContainerID containerId, TileIndex tileIndex, ModelID modelId, f32v3 position, f32 rotation, ui8 variantIndex, TileDamageDataPtr damageData);
    void removeTileInstanceAtPosition(TileContainerID containerId, TileIndex tileIndex);
    TileModelInstance* getTileInstanceAtPosition(LiteTileHandle tileHandle);
    bool hasTileInstanceAtPosition(LiteTileHandle tileHandle, ModelID modelId);
    void addTileInstancesFromGatherer(InstancedStaticModelGatherer& gatherer);
    void removeTileInstancesFromContainer(TileContainerID containerId);
    ui32 getNumModels() const;
    
    // Entity models

    // Return true on success
    bool playAnimationOnInstanceAtPosition(LiteTileHandle targetTile, StaticModelAnimationTypes animType, f32v2 direction, ModelID modelId);

    // UNUSED
    void onContainerEditEvent(const TileContainerEvent& evnt);

    void onTileDamagedEvent(const TileContainerEvent& evnt);

    StaticModelInstanceID addLooseModelInstance(ModelID modelId, const glm::quat& orient, f32v3 position, ui8 variantIndex, f32 scale);
    void removeLooseModelInstance(ModelID modelId, StaticModelInstanceID instanceId);
    void changeLooseModelInstanceScale(ModelID modelId, StaticModelInstanceID instanceId, const glm::quat& orient, f32v3 position, f32 scale);
private:
    void updatePendingLooseModelInstances();

    void removeModelInstanceInternal(ui32 instanceIndex);

    void addTileInstanceInternal(const ModelDef& modelDef, TileContainerID containerId, TileIndex tileIndex, const f32m4& transform, ui8 variantIndex, TileDamageDataPtr damageData);
    void removeTileInstanceInternal(TileModelInstance& instance);

    void onTileInstanceDamageChanged(TileContainerID containerId, TileIndex tileIndex, const TileDamageData& damageData);

    void removeDamageModelInternal(ui32 damageModelIndex);

    void addLooseInstanceInternal(ModelID modelId, StaticModelInstanceID instanceId, const f32m4& transform, ui8 variantIndex);
    void removeLooseInstanceInternal(ModelID modelId, StaticModelInstanceID instanceId);
    void updateLooseInstanceTransformInternal(ModelID modelId, StaticModelInstanceID instanceId, const f32m4 transform);

    void updateAnimatedModels(f32 elapsedSec);
    void increfModelDef(ModelID modelId, int incCount);
    void decrefModelDef(ModelID modelId, int decCount);

    void incrementDrawCommandsCount(ModelBatchSubmeshDrawDataSpanKey drawDataKey);
    void decrementDrawCommandsCount(ModelBatchSubmeshDrawDataSpanKey drawDataKey);

    struct InstanceDrawData {
        InstanceDrawData() = default;
        InstanceDrawData(ModelBatchSubmeshDrawDataSpanKey key, ModelID modelId) : key(key), modelId(modelId) {}
        ModelBatchSubmeshDrawDataSpanKey key;
        ModelID modelId;
    };

    std::vector<f32m4> mInstanceTransforms;

    struct InstanceGpuData {
        ui32 submeshDataIndex;
        ui32 variantIndex;
        ui32 damageModelIndex;
    };

    std::vector<InstanceGpuData> mInstanceGpuData;
    std::vector<ModelInstanceOwnerVariant> mInstanceSources;
    std::vector<InstanceDrawData> mInstanceDrawData;
    std::vector<ModelDamageZoneGpuData> mModelDamageZonesGpuData; // 0 index is default no damage

    std::unique_ptr<GLDrawCommandBuffer> mDrawCommands[e_count(MaterialRenderPassType)];
    std::unique_ptr<GLDrawCommandBuffer> mDrawCommandsShadows[e_count(MaterialRenderPassType)];
    ui32 mDrawCommandsCount[e_count(MaterialRenderPassType)] = {};
    ui32 mDrawCommandsShadowsCount[e_count(MaterialRenderPassType)] = {};

    VGBuffer mTransformsVbo = 0;
    VGBuffer mInstanceDataVbo = 0;
    VGBuffer mDamageZonesSSBO = 0;
    ui32 mTransformsVboSizeBytes = 0;
    ui32 mFirstDirtyInstance = UINT32_MAX;

    std::atomic<StaticModelInstanceID> mNextLooseInstanceID = 0;
    FlatMap<StaticModelInstanceID, ui32 /*instanceIndex*/> mLooseStaticModelInstances;

    FlatMap<LiteTileHandle, StaticMeshAnimation> mAnimatedTileInstances;
    // TODO: We are tracking modelID here and we no longer need it
    std::map<TileContainerID, SpatialInstanceDataMap> mTileContainerTrackedModels;
    GLBuffer mGpuCullingUniformBuffer;

    // Refcount ModelDefs
    UnorderedFlatMap<ModelID, ModelDefRef> mModelDefRefs;

    AssetHandlePtr<MaterialShaderDef> mCullingComputeShader;

    std::unique_ptr<ModelBillboardLodManager> mBillboardLodManager;

    struct PendingLooseModelInstance {
        glm::quat orient;
        f32v3 position;
        ModelID modelId;
        StaticModelInstanceID instanceId;
        ui8 variantIndex;
        enum class Type : ui8 {
            Add,
            Remove,
            ChangeTransform,
            COUNT
        } type = Type::Add;
        f32 scale;
    };
    moodycamel::ConcurrentQueue<PendingLooseModelInstance> mPendingLooseModelInstances;

};

