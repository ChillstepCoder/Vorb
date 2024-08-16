#pragma once

#include "definitions/ModelDef.h"
#include "rendering/mesh/FBXRawModel.h"

#include "resources/IAssetRepository.h"
#include "rendering/model/ModelBatch.h"

#include "util/fixed_capacity_vector.h"

DECL_VIO(class IOManager);
DECL_VG(class TextureCache);
DECL_VG(class Texture);

// If we need to go bigger, switch to a different container than FixedSizeVector as this is 24 bytes
constexpr ui8 MAX_DRAW_COMMANDS_PER_MODEL = 11;
using ModelDrawCommandsList = std::experimental::fixed_capacity_vector<ModelBatchSubmeshDrawDataID, MAX_DRAW_COMMANDS_PER_MODEL>;

class FbxLoadContext;

namespace ozz::animation {
    class Skeleton;
};

struct ModelLodParams {

    MeshLODLevel selectLOD(f32 distanceSQ) const {
        for (int i = 0; i < e_count(MeshLODLevel) - 1; i++) {
            if (distanceSQ < lodDistancesSQ[i]) return (MeshLODLevel)i;
        }
        return (MeshLODLevel)(e_count(MeshLODLevel) - 1);
    }

    f32 lodDistancesSQ[e_count(MeshLODLevel)];
    f32 boundingSphereRadius;
    ShadowModelDetail shadowLodDetail;
};

class ModelRepository : public IAssetRepository<ModelDef> {
    friend class AssetSelectPanel; // TODO: Remove?
public:
    ASSET_REPOSITORY_COMMON_CODE(ModelRepository, ModelDef, AssetType::Model)

    DEFAULT_ASSET_SAVE_FUNC();

    bool loadFbxFile(const vio::Path& filePath);

    StrToken getAssetExtension() const override { return CStrToken("model"); }
    const char* const getAssetTypeDisplayName() const override { return "Model"; }

    const ModelLodParams& getLodParams(AssetID id) const { return mLODParameters[id]; }

    void onAssetChangedByEditor(AssetID id) override;

    void loadAllModelData();
    bool allModelDataLoaded() {
        return mUnloadedModelDataCount == 0;
    }
    void buildModelBatches();

    const ModelBatch& getModelBatch(ModelBatchID id) {
        return mModelBatches[id];
    }
private:
    AssetLoadFunc getAssetLoadFunc() override;

    void loadModelDataInternal(ModelDef& def, StrToken modelName, const vio::Path& modelPath);
    void loadRawModelFromFBX(FbxLoadContext& loadContext, FBXRawModel& rawFbxMesh, const vio::Path& filePath, const ozz::animation::Skeleton* skeleton);
    void combineSubmeshesByRenderPass(OUT FBXRawModel& rawFbxMesh);
    void loadCachedRuntimeModel(ModelDef& def, const vio::Path& modelPath, const ozz::animation::Skeleton* skeleton);
    void saveCachedRuntimeModel(ModelDef& def, const vio::Path& modelPath);

    void onRegisteredAsset(AssetID id) override;
    void onAllAssetTypesRegistered() override;
    void updateModelFlyweightData(AssetID id);
    void updateModelVariantData(AssetID id);
    void updateMaterialDependencies(AssetID id);
    void updateModelCollision(AssetID id);

    std::atomic_int mUnloadedModelDataCount = INT32_MAX;
    std::atomic_int mTotalSubmeshCount = 0;

    // Stored separately for cache friendliness on render
    std::vector<ModelLodParams> mLODParameters;

    // Model batching
    std::unique_ptr<ModelBatch[]> mModelBatches;
    std::vector<ModelBatchSubmeshDrawData> mAllSubmeshDrawData;

    // One per ModelID (TODO: UniquePtr);
    // TODO: Modular character overrides
    std::vector<ModelDrawCommandsList> mModelDefaultDrawCommands;
};