#pragma once

#include "rendering/texture/GLTexture.h"

// This should work
struct ModelBillboardData {
    f32v3 position;
    f32v2 dims;
    ui32 material;
};
static_assert(sizeof(ModelBillboardData) == 24);

class ModelBillboardLodManager {
public:
    ModelBillboardLodManager(const std::unordered_map<AssetID, GLTexture>& billboardTextures);
    ~ModelBillboardLodManager();

    void frameBegin();

    void addBillboard(AssetID modelID, const f32v3& position, const f32v2& dims);

private:

    const std::unordered_map<AssetID, GLTexture>& mBillboardTextures;
};

class ModelBillboardLodBuilder {
public:

    void initTextureForModel(AssetID modelID);

    const std::unordered_map<AssetID, GLTexture>& getBillboardTextures() const { return mBillboardTextures; }
private:

    // Maps models to billboards
    std::unordered_map<AssetID, GLTexture> mBillboardTextures;
};