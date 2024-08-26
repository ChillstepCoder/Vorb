#include "stdafx.h"
#include "ModelBillboardLodManager.h"

#include "resources/ResourceManager.h"
#include "resources/TextureRepository.h"
#include "resources/ModelRepository.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/MaterialRenderer.h"
#include "resources/MaterialRepository.h"
#include "rendering/RenderContext.h"

#include <extern/bc7enc/lodepng.h>
#include <Vorb/graphics/GBuffer.h>

constexpr ui32 MIN_BILLBOARD_RES = 64;
constexpr ui32 MAX_BILLBOARD_RES = 512;
constexpr f32 BILLBOARD_METERS_TO_PIXELS = 32.f;

// Finds closest power of two
inline ui32 roundToBillboardResolution(f32 value) {
    const ui32 toRound = (ui32)(value * BILLBOARD_METERS_TO_PIXELS);
    if (toRound < MIN_BILLBOARD_RES) return MIN_BILLBOARD_RES;

    ui32 upperPower = MIN_BILLBOARD_RES;
    while (upperPower < toRound) {
        upperPower <<= 1;
    }

    ui32 lowerPower = upperPower >> 1;

    // Pick closest
    if (upperPower - toRound > toRound - lowerPower) {
        return std::clamp(lowerPower, MIN_BILLBOARD_RES, MAX_BILLBOARD_RES);
    }
    else {
        return std::clamp(upperPower, MIN_BILLBOARD_RES, MAX_BILLBOARD_RES);
    }
}

ModelBillboardLodManager::ModelBillboardLodManager(const UnorderedFlatMap<AssetID, ui32>& billboardTextures) :
    mBillboardTextures(billboardTextures) {
}

ModelBillboardLodManager::~ModelBillboardLodManager() = default;

void ModelBillboardLodManager::frameBegin() {
    constexpr ui32 FUZZ = 1024;
    // TODO: This will never shrink due to mMaxCapacity
    const ui32 desiredCapacity = std::max(mNumBillboards + 128, mMaxCapacity);
    GpuStreamingDataBuffer::reallocateFuzzedIfNeeded(mBillboardDataBuffer, desiredCapacity, sizeof(ModelBillboardData), FUZZ);
    mMaxCapacity = mBillboardDataBuffer->getMaxElements();
    LOG_INFO("BillboardLodManager: Max: {} Num {}", mMaxCapacity, mNumBillboards);
    mNumBillboards = 0;
    mBillboardDataThisFrame = static_cast<ModelBillboardData*>(mBillboardDataBuffer->frameBeginAndGetDataForUpdate());
}

void ModelBillboardLodManager::addBillboard(AssetID modelID, f32v3 position, f32v2 dims) {
    if (mNumBillboards < mMaxCapacity) {
        // We only append billboards while less than max capacity. If we blow capacity, new space will be allocated next frame
        mBillboardDataThisFrame[mNumBillboards] = ModelBillboardData{ position, 0.0f /*TODO: xflip*/, dims * 0.5f, 0/*TODO: MATERIAL*/, };
    }
    // Always increment
    ++mNumBillboards;
}

void ModelBillboardLodManager::flushDataAndIncrementFrame() {
    if (mBillboardDataBuffer) {
        // numBillboards can exceed capacity
        mBillboardDataBuffer->flushDataAndIncrementFrame(std::min(mNumBillboards, mMaxCapacity));
    }
}

ui32 ModelBillboardLodManager::getNumBillboards() const {
    // numBillboards can exceed capacity
    return std::min(mNumBillboards, mMaxCapacity);
}

ModelBillboardLodBuilder::ModelBillboardLodBuilder() {
    assert(!mModelAssets.isLockedByAssetLoader());
};
ModelBillboardLodBuilder::~ModelBillboardLodBuilder() = default;

void ModelBillboardLodBuilder::addBillboardTextureToBuild(AssetID modelID) {
    assert(!mModelAssets.isLockedByAssetLoader());
    AssetHandlePtr<ModelDef> assetHandle = ModelRepository::get().getAssetHandle(modelID);
    mModelsToBuild.emplace_back(&assetHandle->getLoadedOrUnloadedAsset());
    mModelAssets.addAssetHandle(std::move(assetHandle));
}

void ModelBillboardLodBuilder::buildAllBillboards() {
    if (mModelsToBuild.empty()) {
        return;
    }
    PreciseTimer timer;
    LOG_DEBUG("Building {} billboards", mModelsToBuild.size());

    ModelRepository& modelRepo = ModelRepository::get();

    AssetHandlePtr<MaterialShaderDef> floodFillComputeHandle = MaterialShaderRepository::get().getAssetHandle(CStrToken("flood_fill_blk"));
    AssetHandlePtr<MaterialShaderDef> shaderHandle = MaterialShaderRepository::get().getAssetHandle(CStrToken("billboard_gen"));
    // Load assets until our shader is ready
    while (!shaderHandle->isLoaded() || !floodFillComputeHandle->isLoaded() || !mModelAssets.areAllAssetsLoaded()) {
        AssetLoader::getInstance().update();
        Sleep(1);
        RenderContext::getInstance().updateRenderThreadProcs();
    };
    const MaterialShaderDef& floodFillComputeShaderDef = floodFillComputeHandle->getLoadedAsset();
    const MaterialShaderDef& shaderDef = shaderHandle->getLoadedAsset();
    const VGUniform varUniform = shaderDef.getUniform("unVariantIndex");

    struct GBufferWrapper {
        GBufferWrapper(ui32v2 resolution) : buffer(resolution) { 
            buffer.initAttachment(vg::GBufferAttachmentIndex::ALBEDO, vg::TextureInternalFormat::RGBA8);
            buffer.initDepth(vg::GBufferDepthFormat::DEPTH_24);
            glCreateTextures(GL_TEXTURE_2D, 1, &floodFillTexture);
            glTextureStorage2D(floodFillTexture, 1, GL_RGBA8, resolution.x, resolution.y);
        }
        ~GBufferWrapper() {
            glDeleteTextures(1, &floodFillTexture);
        }
        vg::GBuffer buffer;
        VGTexture floodFillTexture;
    };

    FlatMap<ui32v2, std::unique_ptr<GBufferWrapper>> gBuffers;

    vg::DepthState::FULL.set();
    vg::BlendState::set(vg::BlendStateType::ALPHA);

    MaterialRepository::get().bindMaterialBuffer();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MODEL_VARIANT_DATA_SSBO, modelRepo.getModelVariantDataSSBO());

    std::vector<ui8v4> pixelBuffer;
    pixelBuffer.reserve(MAX_BILLBOARD_RES * MAX_BILLBOARD_RES);
    std::vector<unsigned char> png;
    glDisable(GL_BLEND);
    for (size_t i = 0; i < mModelsToBuild.size(); i++) {

        const ModelDef& model = *mModelsToBuild[i];
        nString pathString = Utils::removeExtension(modelRepo.getAssetFilePath(model.getID()).getString());
        pathString += "_b.png";

        ModelBatchSubmeshDrawDataSpanKey submeshSpanKey = modelRepo.getDrawDataSpanKeyForModel(model.getID());
        VariantIndexData variantIndexData = modelRepo.getVariantArrayIndexDataForModel(model.getID());

        const f32AABB3& aabb = model.mAABB;
        f32v3 halfDims = aabb.getHalfDims();
        // Aim down Y axis
        f32v3 center = aabb.getCenter();
        f32m4 projection = glm::ortho(-halfDims.x, +halfDims.x, -halfDims.z, +halfDims.z, -1000.f, 1000.f);
        f32m4 view = glm::lookAt(center - f32v3(0.0f, aabb.dims.y + 0.0001f, 0.0f), center, f32v3(0.0f, 0.0f, -1.0f));
        f32m4 VP = projection * view;

        MaterialRenderer::bindMaterialShaderForRender(shaderDef, nullptr);
        glUniformMatrix4fv(shaderDef.getUniform("unModelMatrix"), 1, GL_FALSE, &glm::translate(f32m4(1.f), f32v3(0.f, 100.f, 0.f))[0][0]);
        glUniformMatrix4fv(shaderDef.getUniform("unVP"), 1, GL_FALSE, &VP[0][0]);

        const ui32v2 desiredResolution(roundToBillboardResolution(aabb.dims.x), roundToBillboardResolution(aabb.dims.z));
        auto it = gBuffers.find(desiredResolution);
        if (it == gBuffers.end()) {
            it = gBuffers.emplace(desiredResolution, std::make_unique<GBufferWrapper>(desiredResolution)).first;
        }
        vg::GBuffer& gBuffer = it->second->buffer;
        gBuffer.use();
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
     
        for (int m = 0; m < model.getNumMeshes(); ++m) {
            // TODO: One per variant...
            int variantIndex = 0;
            assert(m < variantIndexData.stride);
            const ModelBatchSubmeshDrawData& drawData = modelRepo.getSubmeshDrawDataArrayForModel(submeshSpanKey)[m];
            const ModelBatch& modelBatch = modelRepo.getModelBatch(drawData.batchID);
            modelBatch.unbindCurrentAttribs(); // Dont use these attribs
            glBindVertexArray(modelBatch.getVao());

            glUniform1ui(varUniform, (ui32)(variantIndexData.offset + variantIndex * variantIndexData.stride));

            const MeshLODDrawInfo& drawInfo = drawData.lodDrawInfo[0];
            glDrawElementsBaseVertex(
                GL_TRIANGLES,
                drawInfo.indexCount,
                e_cast(modelBatch.getIndexType()),
                (const GLvoid*)(drawInfo.startIndex * modelBatch.getIndexSize()) /* offset */,
                drawData.baseVertex
            );
        }
        const ui8v4 clearColor = ui8v4(0, 0, 0, 0);
        glClearTexImage(it->second->floodFillTexture, 0, GL_RGBA, GL_UNSIGNED_BYTE, &clearColor.x);

        // We need to flood fill the black pixels so dither transparency works
        floodFillComputeShaderDef.useCompute();

        constexpr i32 FLOOD_FILL_ITERATIONS = 8;
        constexpr i32 WORK_GROUP_SIZE = 16;
        // Ping ping between two textures so we can read and write without undefined behavior
        for (i32 f = 0; f < FLOOD_FILL_ITERATIONS; ++f) {
            glBindImageTexture(0, gBuffer.getAlbedoTexture(), 0, false, 0, GL_READ_ONLY, GL_RGBA8);
            glBindImageTexture(1, it->second->floodFillTexture, 0, false, 0, GL_WRITE_ONLY, GL_RGBA8);
            glDispatchCompute(desiredResolution.x / WORK_GROUP_SIZE, desiredResolution.y / WORK_GROUP_SIZE, 1);
            glBindImageTexture(0, it->second->floodFillTexture, 0, false, 0, GL_READ_ONLY, GL_RGBA8);
            glBindImageTexture(1, gBuffer.getAlbedoTexture(), 0, false, 0, GL_WRITE_ONLY, GL_RGBA8);
            glDispatchCompute(desiredResolution.x / WORK_GROUP_SIZE, desiredResolution.y / WORK_GROUP_SIZE, 1);
        }

        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

        glFlush(); // Not necessary I think
        pixelBuffer.resize(desiredResolution.x * desiredResolution.y);
        glGetTextureImage(gBuffer.getAlbedoTexture(), 0, GL_RGBA, GL_UNSIGNED_BYTE, (desiredResolution.x * desiredResolution.y) * sizeof(ui8v4), pixelBuffer.data());
       
        png.clear();
        png.reserve(desiredResolution.x * desiredResolution.y * sizeof(ui8v4));
        unsigned error = lodepng::encode(png, &pixelBuffer[0].x, desiredResolution.x, desiredResolution.y, LCT_RGBA, 8);

        if (error) {
            panic("Billboard PNG encoding error: {} for model {}", lodepng_error_text(error), model.getName().toString());
        }
        error = lodepng::save_file(png, pathString);

        if (error) {
            panic("Billboard PNG save error: {} for model {}", lodepng_error_text(error), model.getName().toString());
        }
    }
    glEnable(GL_BLEND);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    vg::DepthState::restorePrevious();
    vg::BlendState::restorePrevious();

    vg::GBuffer::unuse();
    const ui32v2 screenResolution = RenderContext::getInstance().getScreenResolution();
    glViewport(0, 0, screenResolution.x, screenResolution.y);

    LOG_DEBUG("Built billboards in {} ms", mModelsToBuild.size(), timer.elapsedMs());

    checkGlError("ModelBillboardLodBuilder::buildAllBillboards");
}
