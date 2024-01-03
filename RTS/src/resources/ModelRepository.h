#pragma once

#include "definitions/ModelDef.h"
#include "rendering/mesh/FBXRawMesh.h"

#include "resources/IAssetRepository.h"

DECL_VIO(class IOManager);
DECL_VG(class TextureCache);
DECL_VG(class Texture);

class FBXLoadContext;

namespace ozz::animation {
    class Skeleton;
};

struct ModelLodParams {
    f32 lodDistancesSQ[4];
    f32 boundingSphereRadius;
    ShadowModelDetail shadowLodDetail;
};

class ModelRepository : public IAssetRepository<ModelDef> {
    friend class TileEditorPanel; // TODO: Remove?
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
    void loadRawModelFromFBX(FBXLoadContext& loadContext, FBXRawMesh& rawFbxMesh, const vio::Path& filePath, const ozz::animation::Skeleton* skeleton);

    void onRegisteredAsset(AssetID id) override;
    void onAllAssetTypesRegistered() override;
    void updateModelFlyweightData(AssetID id);
    void updateModelVariantData(AssetID id);
    void updateMaterialDependencies(AssetID id);

    std::mutex mRawModelsMutex;
    std::map<StrToken, std::unique_ptr<FBXRawMesh>> mRawModels;

    std::mutex mFbxSdkMutex; // FBX SDK IS NOT THREAD SAFE >_<
    // TODO: Pooled allocate
    std::vector<std::unique_ptr<ModelBatch>> mModelBatches;

    // Stored separately for cache friendliness on render
    std::vector<ModelLodParams> mLODParameters;
    // We cant delete these multithreaded...
    //std::map<const vio::Path, std::shared_ptr<FBXLoadContext>> mFbxLoadContexts;
};

