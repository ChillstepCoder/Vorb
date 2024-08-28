#pragma once

#include "rendering/texture/GLTexture.h"
#include "resources/asset/AssetHandleBundle.h"

#include <gli/texture2d.hpp>

DECL_VG(class GBuffer);

class ModelDef;
class ModelImpostorRepository;

using ImpostorIndex = ui16;

struct ImpostorSpan {
    ImpostorIndex index;
    ui16 numBillboards;
};

// This should work
struct alignas(16) ModelBillboardData {
    f32v3 position;
    f32 uFlip; // 0 or 1
    f32v2 dims;
    ui32 material;
    f32 PADDING;
};
static_assert(sizeof(ModelBillboardData) == 32);

class ModelImpostorManager {
public:
    ModelImpostorManager(ModelImpostorRepository& impostorRepo);
    ~ModelImpostorManager();

    void frameBegin();

    void addBillboard(AssetID modelID, f32v3 position, f32v2 dims);

    void flushDataAndIncrementFrame();

    ui32 getNumBillboards() const;

    const GpuStreamingDataBuffer* getBillboardDataBuffer() const { return mBillboardDataBuffer.get(); }

    void bindMaterialBuffer() const;

private:
    ModelImpostorRepository& mImpostorRepository;
    std::unique_ptr<GpuStreamingDataBuffer> mBillboardDataBuffer;
    ModelBillboardData* mBillboardDataThisFrame;
    ui32 mNumBillboards = 0;
    ui32 mMaxCapacity = 0;
};

class ModelImpostorRepository {
public:
    ModelImpostorRepository();
    ~ModelImpostorRepository();

    void allocateImpostorIndicesForModels(ui32 numModels);
    void registerModelForImpostor(const ModelDef& modelDef);
    void buildAllImpostors();

    void bindMaterialBuffer() const;
    ImpostorSpan getImpostorSpanForModel(AssetID modelID) const { return mModelDefImpostorSpans[modelID]; }

private:
    void updateCachedDDS(const nString& baseName, int impostorIndex);
    void saveDDSTextures(
        const nString& baseName, gli::texture2d& albedoTexture, gli::texture2d& normalTexture, gli::texture2d& amrTexture, int impostorIndex
    );
    void loadDDSTexturesAndSetImpostor(const ModelDef& modelDef, const nString& baseName);
    void uploadImposterGpuData();

    struct alignas(8) ModelImpostorGpuData final {
        // Albedo (RGB) Alpha (A)
        TextureHandle albedoMap = INVALID_TEXTURE_HANDLE;
        // Normal (RGB)
        TextureHandle normalMap = INVALID_TEXTURE_HANDLE;
        /// AO (R), Roughness (G), Metallic (B) https://github.com/KhronosGroup/glTF/issues/857
        TextureHandle aoMetallicRoughnessMap = INVALID_TEXTURE_HANDLE;
    };

    struct ModelImpostorTextureHandles {
        GLTexture albedoTexture;
        GLTexture normalTexture;
        GLTexture amrTexture;
    };

    GLBuffer mImpostorDataBuffer;

    // Maps models to billboards
    std::vector<ModelImpostorGpuData> mImpostorGpuData;
    std::vector<ImpostorSpan> mModelDefImpostorSpans; // One per modelID
    UnorderedFlatMap<AssetID, std::vector<std::unique_ptr<ModelImpostorTextureHandles>>> mImpostorTextureHandles;
    std::vector<const ModelDef*> mModelsToBuild;
    AssetHandleBundle mModelAssets;
};