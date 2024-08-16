#pragma once

#include "rendering/model/MaterialRenderPassType.h"
#include "tile/TileDamageData.h"

class GLDrawCommandBuffer;
class Mesh;

struct ModelInstanceContainerOwner {
    TileContainerID containerId;
    TileIndex tileIndex;
};

typedef std::variant<ModelInstanceContainerOwner, StaticModelInstanceID> ModelInstanceOwnerVariant;


enum class StaticModelAnimationTypes : ui8 {
    HitWiggle,
    COUNT
};

constexpr f32 STATIC_MODEL_ANIM_DURATIONS_SEC[e_count(StaticModelAnimationTypes)] = {
    1.0f,
};
static_assert(e_count(StaticModelAnimationTypes) == 1);

// TODO: Allow chunks to reference each of their tiles part of a mesh buffer. Allow removing and compacting the mesh buffer instead of full rebuild
// Use TileIndex as key to reference their mesh data so we can dynamically update it.
//   Queue tile mesh updates, then do them all in a single pass then do a compaction pass on the buffer
struct TileModelInstance {
    ModelID mModelID = 0;
    ui32 mInstanceIndex = 0; // Index into the transforms array
};
static_assert(sizeof(TileModelInstance) == 8, "Keep tiny");

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

class StaticModelRendererBatchData
{
public:
    // Instance data for a specific mesh
    StaticModelRendererBatchData();

    std::vector<f32m4> mInstanceTransforms;
    std::vector<ui8> mInstanceVariantIndices;
    std::vector<ui32> mInstanceDamageModelIndices;
    std::vector<ModelDamageZoneGpuData> mModelDamageZonesGpuData; // 0 index is default no damage
    std::vector<ModelInstanceOwnerVariant> mInstanceSources;
    std::unique_ptr<GLDrawCommandBuffer> mDrawCommands[e_count(MaterialRenderPassType)];
    std::unique_ptr<GLDrawCommandBuffer> mDrawCommandsShadows[e_count(MaterialRenderPassType)];
    VGBuffer mTransformsVbo = 0;
    VGBuffer mVariantsVbo = 0;
    VGBuffer mDamageModelIndexVbo = 0;
    VGBuffer mDamageZonesSSBO = 0; // TODO: Not every model needs one of these!
    ui32 mTransformsVboSizeBytes = 0;
    ui32 mFirstDirtyInstance = UINT32_MAX;
    const Mesh* mMesh[e_count(MaterialRenderPassType)] = {};
    int mMeshCount = 0;
    bool mMeshCastsShadow[e_count(MaterialRenderPassType)] = {};
    bool mIsLoaded = false;

    // TODO: Investigate why, hardware? Driver? - Compact GPU culled indirect buffer is actually slower due to atomic operation and cpu-gpu sync
    // std::unique_ptr<GLIndirectBuffer> mOutDrawCommands;
    //GLBuffer mNumVisibleMeshesBuffer;
    //volatile uint32_t* mNumVisibleMeshesBufferPtr = nullptr;
    // GLsync mFenceSync = 0;
};

// Stores all specific instances of a given model in the world
typedef std::map<ModelID, StaticModelRendererBatchData> ModelBatchMap;
