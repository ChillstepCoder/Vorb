#pragma once

#include "rendering/texture/GLTexture.h"
#include "resources/asset/AssetHandleBundle.h"

DECL_VG(class GBuffer);

class ModelDef;

// This should work
struct alignas(16) ModelBillboardData {
    f32v3 position;
    f32 xFlip; // TODO: ui8?
    f32v2 dims;
    ui32 material;
    f32 PADDING;
};
static_assert(sizeof(ModelBillboardData) == 32);

class ModelBillboardLodManager {
public:
    ModelBillboardLodManager(const UnorderedFlatMap<AssetID, ui32>& billboardTextures);
    ~ModelBillboardLodManager();

    void frameBegin();

    void addBillboard(AssetID modelID, f32v3 position, f32v2 dims);

    void flushDataAndIncrementFrame();

    ui32 getNumBillboards() const;

    const GpuStreamingDataBuffer* getBillboardDataBuffer() const { return mBillboardDataBuffer.get(); }

private:
    std::unique_ptr<GpuStreamingDataBuffer> mBillboardDataBuffer;
    const UnorderedFlatMap<AssetID, ui32>& mBillboardTextures;
    ModelBillboardData* mBillboardDataThisFrame;
    ui32 mNumBillboards = 0;
    ui32 mMaxCapacity = 0;

};

class ModelBillboardLodBuilder {
public:
    ModelBillboardLodBuilder();
    ~ModelBillboardLodBuilder();

    void addBillboardTextureToBuild(AssetID modelID);
    void buildAllBillboards();

    const UnorderedFlatMap<AssetID, ui32>& getBillboardTextures() const { return mBillboardTextures; }
private:

    // Maps models to billboards
    UnorderedFlatMap<AssetID, ui32> mBillboardTextures;
    std::vector<const ModelDef*> mModelsToBuild;
    AssetHandleBundle mModelAssets;
};