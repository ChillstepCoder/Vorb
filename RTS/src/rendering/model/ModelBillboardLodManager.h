#pragma once
class ModelBillboardLodManager {
public:
    ModelBillboardLodManager();
    ~ModelBillboardLodManager();

private:
    // Maps models to billboards
    std::unordered_map<AssetID, VGTexture> mBillboardTextures;
};

