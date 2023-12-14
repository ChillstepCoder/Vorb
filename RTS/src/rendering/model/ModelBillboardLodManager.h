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
    ModelBillboardLodManager();
    ~ModelBillboardLodManager();

    void initTextureForModelIfNeeded(AssetID modelID);
private:


    // Maps models to billboards
    std::unordered_map<AssetID, GLTexture> mBillboardTextures;
};

