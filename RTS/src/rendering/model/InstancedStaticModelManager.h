#pragma once

#include "rendering/model/MaterialRenderPassType.h"

#include "tile/TileDamageData.h"
#include "tile/MutationDef.h"

#include "definitions/ModelDef.h"

#include "tile/TileHandle.h"

struct TileContainerEvent;
class Camera3D;
class World;
class InstancedStaticModelGatherer;
class ModelRepository;
class MaterialShaderDef;
class ModelImpostorManager;
class GLDrawCommandBuffer;
class Mesh;
struct ModelBatchSubmeshDrawData;
DECL_VG(class GLProgram);

struct ModelInstanceContainerOwner {
    TileContainerID containerId;
    TileIndex tileIndex;
};

typedef std::variant<ModelInstanceContainerOwner, StaticModelInstanceID, std::monostate> ModelInstanceOwnerVariant;

enum class StaticModelAnimationTypes : ui8 {
    HitWiggle,
    COUNT
};

constexpr f32 STATIC_MODEL_ANIM_DURATIONS_SEC[e_count(StaticModelAnimationTypes)] = {
    1.0f,
};
static_assert(e_count(StaticModelAnimationTypes) == 1);

// Index into the transforms array
using TileModelInstanceIndex = ui32;
constexpr TileModelInstanceIndex INVALID_TILE_MODEL_INSTANCE_INDEX = std::numeric_limits<TileModelInstanceIndex>::max();

using MutationDataIndex = ui16;
constexpr MutationDataIndex INVALID_MUTATION_DATA_INDEX = std::numeric_limits<MutationDataIndex>::max();

struct StaticMeshAnimation {
    f32 currentTimeSec;
    StaticModelAnimationTypes animType;
    f32v2 direction;
};

struct alignas(8) ModelDamageZoneGpuData {
    TileDamageZonesArray damageZones = {};
    f32v2 bottom = f32v2(0.0f);
    f32v2 top = f32v2(0.0f, 4.0f);
    f32v2 radii = f32v2(1.0f, 1.0f);
};
static_assert(sizeof(ModelDamageZoneGpuData) == 56, "Size mismatch with gpu");

// Allows us to look up the specific model at a position for a tile container
typedef UnorderedFlatMap<TileIndex, TileModelInstanceIndex> SpatialInstanceDataMap;

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
    InstancedStaticModelManager(World& world);
    ~InstancedStaticModelManager();

    void frameUpdate(const Camera3D& camera, f32 elapsedSec);

    const ModelImpostorManager& getBillboardLodManager() const { return *mBillboardLodManager; }

    // Tile models
    void addTileInstanceAtPosition(
        TileContainerID containerId, TileIndex tileIndex, ModelID modelId, f32v3 position, f32 rotation, ui8 variantIndex, TileDamageDataPtr damageData, f32 scale
    );
    void removeTileInstanceAtPosition(TileContainerID containerId, TileIndex tileIndex);
    TileModelInstanceIndex getTileInstanceIndexAtPosition(LiteTileHandle tileHandle);
    bool hasTileInstanceAtPosition(LiteTileHandle tileHandle, ModelID modelId);
    void addTileInstancesFromGatherer(InstancedStaticModelGatherer& gatherer);
    void removeTileInstancesFromContainer(TileContainerID containerId);
    ui32 getNumModels() const;
    ui32 getNumActiveLodTransitions() const { return mNumActiveLodTransitions; }
    
    // Return true on success
    bool playAnimationOnInstanceAtPosition(LiteTileHandle targetTile, StaticModelAnimationTypes animType, f32v2 direction, ModelID modelId);

    void onContainerEditEvent(const TileContainerEvent& evnt);

    void onTileDamagedEvent(const TileContainerEvent& evnt);

    StaticModelInstanceID addLooseModelInstance(ModelID modelId, const glm::quat& orient, f32v3 position, ui8 variantIndex, f32 scale);
    void removeLooseModelInstance(ModelID modelId, StaticModelInstanceID instanceId);
    void changeLooseModelInstanceScale(ModelID modelId, StaticModelInstanceID instanceId, const glm::quat& orient, f32v3 position, f32 scale);
private:
    void init();
    void updatePendingLooseModelInstances();

    color4 getMutationColor(MutationType type);

    void removeModelInstanceInternal(TileModelInstanceIndex instanceIndex);

    TileModelInstanceIndex addTileInstanceInternal(
        const ModelDef& modelDef, TileContainerID containerId, TileIndex tileIndex, const f32m4& transform, ui8 variantIndex, TileDamageDataPtr damageData, f32 scale
    );

    void onTileInstanceDamageChanged(TileContainerID containerId, TileIndex tileIndex, const TileDamageData& damageData);

    void removeDamageModelInternal(ui32 damageModelIndex);

    void addLooseInstanceInternal(ModelID modelId, StaticModelInstanceID instanceId, const f32m4& transform, ui8 variantIndex, f32 scale);
    void removeLooseInstanceInternal(StaticModelInstanceID instanceId);
    void updateLooseInstanceTransformInternal(StaticModelInstanceID instanceId, const f32m4 transformm, f32 scale);

    void updateAnimatedModels(f32 elapsedSec);
    void increfModelDef(ModelID modelId, int incCount);
    void decrefModelDef(ModelID modelId, int decCount);

    void incrementDrawCommandsCount(ModelBatchSubmeshDrawDataSpanKey drawDataKey);
    void decrementDrawCommandsCount(ModelBatchSubmeshDrawDataSpanKey drawDataKey);

    void onDirtyModelInstance(TileModelInstanceIndex instanceIndex);

    struct InstanceDrawData {
        InstanceDrawData() = default;
        InstanceDrawData(ModelBatchSubmeshDrawDataSpanKey key, ModelID modelId) : key(key), modelId(modelId) {}
        ModelBatchSubmeshDrawDataSpanKey key;
        ModelID modelId;
    };

    struct InstanceGpuData {
        ui32 submeshDataIndex;
        ui32 variantIndex;
        ui32 damageModelIndex;
    };

    struct MutationData {
        TileModelInstanceIndex fromIndex;
        TileModelInstanceIndex toIndex;
        MutationType type;
        bool isFrom; // If true, we are the fromIndex
    };

    // std430 layout
    struct ModelMutationGpuData {
        color4 color;
        f32 crossfade;
    };

    struct InstanceCrossfadeData {
        bool isActive() { return mCrossfade != 0.0f; }

        f32 mCrossfade = 0.0f; // 0 = not transitioning, positive = crossfade in, negative = crossfade out
        MeshLODLevel mCurrentLOD = MeshLODLevel::INVALID;
        MeshLODLevel mTargetLOD = MeshLODLevel::INVALID;
        MutationDataIndex mMutationDataIndex = INVALID_MUTATION_DATA_INDEX;
    };
    static_assert(sizeof(InstanceCrossfadeData) == 8, "Keep small");

    // Instance SOA data
    std::vector<f32m4> mInstanceTransforms;
    std::vector<InstanceGpuData> mInstanceGpuData;
    std::vector<ModelInstanceOwnerVariant> mInstanceSources;
    std::vector<InstanceDrawData> mInstanceDrawData;
    std::vector<f32> mInstanceScales; // Only used for billboards
    std::vector<InstanceCrossfadeData> mInstanceCrossfadeData;

    // Damage
    std::vector<ModelDamageZoneGpuData> mModelDamageZonesGpuData; // 0 index is default no damage

    // Draw commands
    std::unique_ptr<GLDrawCommandBuffer> mDrawCommands[e_count(MaterialRenderPassType)];
    std::unique_ptr<GLDrawCommandBuffer> mDrawCommandsCrossfade[e_count(MaterialRenderPassType)];
    std::unique_ptr<GLDrawCommandBuffer> mDrawCommandsShadows[e_count(MaterialRenderPassType)];
    std::unique_ptr<GLDrawCommandBuffer> mDrawCommandsMutations[e_count(MaterialRenderPassType)];
    ui32 mDrawCommandsCount[e_count(MaterialRenderPassType)] = {};
    ui32 mDrawCommandsShadowsCount[e_count(MaterialRenderPassType)] = {};

    // GPU buffers
    VGBuffer mTransformsVbo = 0;
    VGBuffer mInstanceDataVbo = 0;
    VGBuffer mDamageZonesSSBO = 0;
    ui32 mInstancesCapacity = 0;
    ui32 mFirstDirtyInstance = INT32_MAX;
    ui32 mLastDirtyInstance = 0;
    // Crossfade Transitions
    std::unique_ptr<GpuStreamingDataBuffer> mCrossfadeBuffers[e_count(MaterialRenderPassType)];
    std::unique_ptr<GpuStreamingDataBuffer> mMutationBuffers[e_count(MaterialRenderPassType)];
    std::vector<MutationData> mMutationData;
    i32 mNumActiveLodTransitions = 0;
    i32 mNumActiveTransformationTransitions = 0;

    // Loose instances
    std::atomic<StaticModelInstanceID> mNextLooseInstanceID = 0;
    FlatMap<StaticModelInstanceID, ui32 /*instanceIndex*/> mLooseStaticModelInstances;

    // Animated instances
    FlatMap<LiteTileHandle, StaticMeshAnimation> mAnimatedTileInstances;
    FlatMap<LiteTileHandle, MutationDataIndex> mTransformingTileInstances;

    std::map<TileContainerID, SpatialInstanceDataMap> mTileContainerTrackedModels;
    GLBuffer mGpuCullingUniformBuffer;

    // Refcount ModelDefs
    UnorderedFlatMap<ModelID, ModelDefRef> mModelDefRefs;

    AssetHandlePtr<MaterialShaderDef> mCullingComputeShader;

    std::unique_ptr<ModelImpostorManager> mBillboardLodManager;

    World& mWorld;

    bool mNeedsInit = true;

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

