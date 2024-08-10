#pragma once

#include "definitions/ModelDef.h"
#include "rendering/mesh/FBXRawModel.h"

#include "resources/IAssetRepository.h"

DECL_VIO(class IOManager);
DECL_VG(class TextureCache);
DECL_VG(class Texture);

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

    // TODO:?
    //void buildModelBatches();

    const ModelLodParams& getLodParams(AssetID id) const { return mLODParameters[id]; }

    void onAssetChangedByEditor(AssetID id) override;
private:
    AssetLoadFunc getAssetLoadFunc() override;

    void loadModelInternal(ModelDef& def, StrToken modelName, const vio::Path& modelPath);
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

    // TODO: Pooled allocate
    std::vector<std::unique_ptr<ModelBatch>> mModelBatches;

    // Stored separately for cache friendliness on render
    std::vector<ModelLodParams> mLODParameters;
    // We cant delete these multithreaded...
    //std::map<const vio::Path, std::shared_ptr<FBXLoadContext>> mFbxLoadContexts;
};

