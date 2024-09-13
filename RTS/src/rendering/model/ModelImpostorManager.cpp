#include "stdafx.h"
#include "ModelImpostorManager.h"

#include "resources/ResourceManager.h"
#include "resources/TextureRepository.h"
#include "resources/ModelRepository.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/MaterialRenderer.h"
#include "resources/MaterialRepository.h"
#include "rendering/RenderContext.h"
#include "rendering/texture/TextureConvert.h"

#include "io/PngLoader.h"

#include "filesystem/FileSystem.h"

#include <extern/bc7enc/lodepng.h>
#include <Vorb/graphics/GBuffer.h>

constexpr ui32 MIN_BILLBOARD_RES = 64;
constexpr ui32 MAX_BILLBOARD_RES = 512;
constexpr f32 BILLBOARD_METERS_TO_PIXELS = 32.f;
constexpr int MAX_IMPOSTOR_IMAGES = 9;

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

ModelImpostorManager::ModelImpostorManager(ModelImpostorRepository& impostorRepo) : mImpostorRepository(impostorRepo) {
    ASSERT_GAME_THREAD(); // Currently runs on game thread
}

ModelImpostorManager::~ModelImpostorManager() = default;

void ModelImpostorManager::frameBegin() {
    constexpr ui32 FUZZ = 1024;
    // TODO: This will never shrink due to mMaxCapacity
    const ui32 desiredCapacity = std::max(mNumBillboards + 128, mMaxCapacity);
    GpuStreamingDataBuffer::reallocateFuzzedIfNeeded(mBillboardDataBuffer, desiredCapacity, sizeof(ModelBillboardData), FUZZ);
    mMaxCapacity = mBillboardDataBuffer->getMaxElements();
    mNumBillboards = 0;
    mBillboardDataThisFrame = static_cast<ModelBillboardData*>(mBillboardDataBuffer->frameBeginAndGetDataForUpdate());
}

void ModelImpostorManager::addBillboard(AssetID modelID, f32v3 position, f32v2 dims, f32 crossfade) {
    if (mNumBillboards < mMaxCapacity) {
        const ui32 rnd = Random::getThreadSafe((ui32)position.x, (ui32)position.y);
        const f32 xFlip = (f32)(rnd & 1);
        const ImpostorSpan span = mImpostorRepository.getImpostorSpanForModel(modelID);
        assert(span.numBillboards != 0);
        const ImpostorIndex index = span.index + (ImpostorIndex)(rnd % span.numBillboards);
        // We only append billboards while less than max capacity. If we blow capacity, new space will be allocated next frame
        mBillboardDataThisFrame[mNumBillboards] = ModelBillboardData{ position, xFlip, dims * 0.5f, index, crossfade };
    }
    // Always increment
    ++mNumBillboards;
}

void ModelImpostorManager::flushDataAndIncrementFrame() {
    if (mBillboardDataBuffer) {
        // numBillboards can exceed capacity
        mBillboardDataBuffer->flushDataAndIncrementFrame(std::min(mNumBillboards, mMaxCapacity));
    }
}

ui32 ModelImpostorManager::getNumBillboards() const {
    // numBillboards can exceed capacity
    return std::min(mNumBillboards, mMaxCapacity);
}

void ModelImpostorManager::bindMaterialBuffer() const {
    mImpostorRepository.bindMaterialBuffer();
}

ModelImpostorRepository::ModelImpostorRepository() {
    assert(!mModelAssets.isLockedByAssetLoader());
};
ModelImpostorRepository::~ModelImpostorRepository() = default;

void ModelImpostorRepository::allocateImpostorIndicesForModels(ui32 numModels) {
    if (numModels >= std::numeric_limits<ImpostorIndex>::max()) {
        panic("Too many models for impostor indices, need to extend to ui32 in ModelImpostorRepository");
    }
    mModelDefImpostorSpans.resize(numModels, {0, 0});
}

void ModelImpostorRepository::registerModelForImpostor(const ModelDef& modelDef) {
    assert(!mModelAssets.isLockedByAssetLoader());

    // Check if we have a bb texture for this model
    ModelRepository& modelRepo = ModelRepository::get();

    vio::Path folderPath = modelRepo.getAssetFilePath(modelDef.getID());
    const nString fileName = Utils::getFilenameNoExtension(folderPath.getString());

    folderPath.trimEnd();
    folderPath /= nString("bb");
    const nString baseName = folderPath.getString() + "/" + fileName;
    const nString albedoName = baseName + "_bb.png";
    const char* ddsExt = ".dds";
    if (fs::exists(albedoName)) {
        ResourceManager& resourceManager = ResourceManager::get();
        const fs::path& resourceRoot(resourceManager.getResourceRoot().getString());
        const fs::path& cacheRoot(resourceManager.getCacheRoot().getString());
        // Save DDS
        const nString ddsName = Utils::removeExtension(albedoName);
        fs::path ddsAlbedoPath(ddsName + ddsExt);
        ddsAlbedoPath = cacheRoot / ddsAlbedoPath.lexically_relative(resourceRoot);

        if (!fs::exists(ddsAlbedoPath) || fs::last_write_time(ddsAlbedoPath) < fs::last_write_time(albedoName)) {
            updateCachedDDS(baseName, 0);
        }

        // Update other DDS if needed
        const char* pngExt = ".png";
        for (int imposterIndex = 1; imposterIndex < MAX_IMPOSTOR_IMAGES; ++imposterIndex) {
            char numberChar[2] = { '\0', '\0' };
            // Note that this purposefully skips to 2 after 0 because artists are used to 1-based indexing
            numberChar[0] = (char)imposterIndex + '1';
            const char* numberStr = numberChar;
            const nString albedoName2 = baseName + "_bb" + numberStr + pngExt;
            if (!fs::exists(albedoName2)) {
                break;
            }
            fs::path ddsAlbedoPath2(ddsName + numberStr + ddsExt);
            ddsAlbedoPath2 = cacheRoot / ddsAlbedoPath2.lexically_relative(resourceRoot);
            if (!fs::exists(ddsAlbedoPath2) || fs::last_write_time(ddsAlbedoPath2) < fs::last_write_time(albedoName2)) {
                updateCachedDDS(baseName, imposterIndex);
            }
        }

        // Load DDS
        loadDDSTexturesAndSetImpostor(modelDef, baseName);
        return;
    }

    AssetHandlePtr<ModelDef> assetHandle = ModelRepository::get().getAssetHandle(modelDef.getID());
    mModelsToBuild.emplace_back(&assetHandle->getLoadedOrUnloadedAsset());
    mModelAssets.addAssetHandle(std::move(assetHandle));
}

template <typename T>
void dumpPngAndFillPixelBuffer(
    const ModelDef& model, ui32v2 resolution, VGTexture textureSource, GLuint format, const char* path, std::vector<T>& pixelBuffer, std::vector<ui8v3>& pixelBufferRGB, bool dropAlpha = false
) {
    size_t size = 0;
    LodePNGColorType colorType;
    switch (format) {
        case GL_RGBA:
            size = sizeof(ui8v4);
            colorType = LCT_RGBA;
            break;
        case GL_RGB:
            size = sizeof(ui8v3);
            colorType = LCT_RGB;
            break;
        case GL_RG:
            size = sizeof(ui8v2);
            colorType = LCT_GREY_ALPHA;
            break;
        case GL_RED:
            size = sizeof(ui8);
            colorType = LCT_GREY;
            break;
        default:
            panic("Unsupported format for billboard PNG dump");
    }
    assert(size == sizeof(T));
    gli::texture2d texture(gli::format::FORMAT_RGBA8_UNORM_PACK8, gli::extent2d(resolution.x, resolution.y));
    pixelBuffer.resize(resolution.x * resolution.y);
    glGetTextureImage(textureSource, 0, format, GL_UNSIGNED_BYTE, (resolution.x * resolution.y) * size, pixelBuffer.data());

    std::vector<unsigned char> png;
    png.reserve(resolution.x * resolution.y * size);

    if (dropAlpha) {
        assert(format == GL_RGBA);
        pixelBufferRGB.resize(pixelBuffer.size());
        for (size_t i = 0; i < pixelBuffer.size(); i++) {
            pixelBufferRGB[i] = ui8v3(pixelBuffer[i]);
        }
        if (unsigned error = lodepng::encode(png, &pixelBufferRGB[0].x, resolution.x, resolution.y, LCT_RGB, 8)) {
            panic("Billboard PNG encoding error: {} for model {}", lodepng_error_text(error), model.getName().toString());
        }
    }
    else {
        if (unsigned error = lodepng::encode(png, (const unsigned char*)&pixelBuffer[0], resolution.x, resolution.y, colorType, 8)) {
            panic("Billboard PNG encoding error: {} for model {}", lodepng_error_text(error), model.getName().toString());
        }
    }

    if (unsigned error = lodepng::save_file(png, path)) {
        panic("Billboard PNG save error: {} for model {}", lodepng_error_text(error), model.getName().toString());
    }
};


void ModelImpostorRepository::buildAllImpostors() {
    if (mModelsToBuild.empty()) {
        uploadImposterGpuData();
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
            buffer.initAttachment(vg::GBufferAttachmentIndex::NORMALS, vg::TextureInternalFormat::RGBA8); // Need to have alpha due to glBindImageTexture not supporting GL_RGB8
            buffer.initAttachment(vg::GBufferAttachmentIndex::TERTIARY1, vg::TextureInternalFormat::R8); // Metallic
            buffer.initAttachment(vg::GBufferAttachmentIndex::TERTIARY2, vg::TextureInternalFormat::R8); // Roughness
            buffer.initAttachment(vg::GBufferAttachmentIndex::TERTIARY3, vg::TextureInternalFormat::R8); // AO
            buffer.initDepth(vg::GBufferDepthFormat::DEPTH_24);
            glCreateTextures(GL_TEXTURE_2D, 1, &floodFillAlbedoTexture);
            glCreateTextures(GL_TEXTURE_2D, 1, &floodFillNormalTexture);
            glTextureStorage2D(floodFillAlbedoTexture, 1, GL_RGBA8, resolution.x, resolution.y);
            glTextureStorage2D(floodFillNormalTexture, 1, GL_RGBA8, resolution.x, resolution.y);
        }
        ~GBufferWrapper() {
            glDeleteTextures(1, &floodFillAlbedoTexture);
            glDeleteTextures(1, &floodFillNormalTexture);
        }
        vg::GBuffer buffer;
        VGTexture floodFillAlbedoTexture;
        VGTexture floodFillNormalTexture;
    };

    FlatMap<ui32v2, std::unique_ptr<GBufferWrapper>> gBuffers;

    vg::DepthState::FULL.set();
    vg::BlendState::set(vg::BlendStateType::ALPHA);

    MaterialRepository::get().bindMaterialBuffer();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MODEL_VARIANT_DATA_SSBO, modelRepo.getModelVariantDataSSBO());

    std::vector<ui8v4> pixelBufferAlbedo;
    std::vector<ui8v4> pixelBufferNormal;
    std::vector<ui8> pixelBufferMetallic;
    std::vector<ui8> pixelBufferRoughness;
    std::vector<ui8> pixelBufferAo;
    std::vector<ui8v3> pixelBufferRGB;
    pixelBufferAlbedo.reserve(MAX_BILLBOARD_RES * MAX_BILLBOARD_RES);
    pixelBufferNormal.reserve(MAX_BILLBOARD_RES * MAX_BILLBOARD_RES);
    pixelBufferMetallic.reserve(MAX_BILLBOARD_RES * MAX_BILLBOARD_RES);
    pixelBufferRoughness.reserve(MAX_BILLBOARD_RES * MAX_BILLBOARD_RES);
    pixelBufferAo.reserve(MAX_BILLBOARD_RES * MAX_BILLBOARD_RES);
    pixelBufferRGB.reserve(MAX_BILLBOARD_RES * MAX_BILLBOARD_RES);
    glDisable(GL_BLEND);
    for (size_t i = 0; i < mModelsToBuild.size(); i++) {

        const ModelDef& model = *mModelsToBuild[i];
        vio::Path folderPath = modelRepo.getAssetFilePath(model.getID());
        const nString fileName = Utils::getFilenameNoExtension(folderPath.getString());

        folderPath.trimEnd();
        folderPath /= nString("bb");

        if (!FileSystem::createDirectories(folderPath.getStdPath())) {
            panic("Failed to create {} directory. Insufficient permissions?", folderPath.getCString());
        }
        const nString baseName = folderPath.getString() + "/" + fileName;
        const nString albedoPathString = baseName + "_bb.png";
        const nString normalPathString = baseName + "_bb_n.png";
        const nString roughPathString = baseName + "_bb_r.png";
        const nString metalPathString = baseName + "_bb_m.png";
        const nString aoPathString = baseName + "_bb_ao.png";

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
        glUniformMatrix4fv(shaderDef.getUniform("unV"), 1, GL_FALSE, &view[0][0]);
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
        gBuffer.clearAttachment(vg::GBufferAttachmentIndex::NORMALS, f32v4(0.5f, 0.5f, 1.f, 0.f));
        gBuffer.clearAttachment(vg::GBufferAttachmentIndex::TERTIARY1, f32v4(0.f)); // Metallic
        gBuffer.clearAttachment(vg::GBufferAttachmentIndex::TERTIARY2, f32v4(1.f)); // Roughness
        gBuffer.clearAttachment(vg::GBufferAttachmentIndex::TERTIARY3, f32v4(1.f)); // Ao
     
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
        const ui8v4 clearNormal = ui8v4(128, 128, 255, 255);
        glClearTexImage(it->second->floodFillAlbedoTexture, 0, GL_RGBA, GL_UNSIGNED_BYTE, &clearColor.x);
        glClearTexImage(it->second->floodFillNormalTexture, 0, GL_RGBA, GL_UNSIGNED_BYTE, &clearNormal.x);

        // We need to flood fill the black pixels so dither transparency works
        floodFillComputeShaderDef.useCompute();

        constexpr i32 FLOOD_FILL_ITERATIONS = 8;
        constexpr i32 WORK_GROUP_SIZE = 16;
        // Ping ping between two textures so we can read and write without undefined behavior
        for (i32 f = 0; f < FLOOD_FILL_ITERATIONS; ++f) {
            glBindImageTexture(0, gBuffer.getAlbedoTexture(), 0, false, 0, GL_READ_ONLY, GL_RGBA8);
            glBindImageTexture(1, it->second->floodFillAlbedoTexture, 0, false, 0, GL_WRITE_ONLY, GL_RGBA8);
            glBindImageTexture(2, gBuffer.getNormalTexture(), 0, false, 0, GL_READ_ONLY, GL_RGBA8);
            glBindImageTexture(3, it->second->floodFillNormalTexture, 0, false, 0, GL_WRITE_ONLY, GL_RGBA8);
            glDispatchCompute(desiredResolution.x / WORK_GROUP_SIZE, desiredResolution.y / WORK_GROUP_SIZE, 1);
            glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
            glBindImageTexture(0, it->second->floodFillAlbedoTexture, 0, false, 0, GL_READ_ONLY, GL_RGBA8);
            glBindImageTexture(1, gBuffer.getAlbedoTexture(), 0, false, 0, GL_WRITE_ONLY, GL_RGBA8);
            glBindImageTexture(2, it->second->floodFillNormalTexture, 0, false, 0, GL_READ_ONLY, GL_RGBA8);
            glBindImageTexture(3, gBuffer.getNormalTexture(), 0, false, 0, GL_WRITE_ONLY, GL_RGBA8);
            glDispatchCompute(desiredResolution.x / WORK_GROUP_SIZE, desiredResolution.y / WORK_GROUP_SIZE, 1);
            glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
        }

        dumpPngAndFillPixelBuffer(model, desiredResolution, gBuffer.getAlbedoTexture(), GL_RGBA, albedoPathString.c_str(), pixelBufferAlbedo, pixelBufferRGB);
        dumpPngAndFillPixelBuffer(model, desiredResolution, gBuffer.getNormalTexture(), GL_RGBA, normalPathString.c_str(), pixelBufferNormal, pixelBufferRGB, true);
        dumpPngAndFillPixelBuffer(model, desiredResolution, gBuffer.getTertiaryTexture1(), GL_RED, metalPathString.c_str(), pixelBufferMetallic, pixelBufferRGB);
        dumpPngAndFillPixelBuffer(model, desiredResolution, gBuffer.getTertiaryTexture2(), GL_RED, roughPathString.c_str(), pixelBufferRoughness, pixelBufferRGB);
        dumpPngAndFillPixelBuffer(model, desiredResolution, gBuffer.getTertiaryTexture3(), GL_RED, aoPathString.c_str(), pixelBufferAo, pixelBufferRGB);

        // Build GLI textures so we can convert to DDS
        gli::texture2d albedoTexture(gli::format::FORMAT_RGBA8_UNORM_PACK8, gli::extent2d(desiredResolution.x, desiredResolution.y));
        gli::texture2d normalTexture(gli::format::FORMAT_RGB8_UNORM_PACK8, gli::extent2d(desiredResolution.x, desiredResolution.y));
        gli::texture2d amrTexture(gli::format::FORMAT_RGB8_UNORM_PACK8, gli::extent2d(desiredResolution.x, desiredResolution.y));
        int p = 0;
        for (size_t y = 0; y < desiredResolution.y; y++) {
            for (size_t x = 0; x < desiredResolution.x; x++) {
                albedoTexture.data<ui8v4>()[p] = pixelBufferAlbedo[p];
                normalTexture.data<ui8v3>()[p] = ui8v3(pixelBufferNormal[p]);
                amrTexture.data<ui8v3>()[p] = ui8v3(pixelBufferAo[p], pixelBufferMetallic[p], pixelBufferRoughness[p]);
                ++p;
            }
        }

        saveDDSTextures(baseName, albedoTexture, normalTexture, amrTexture, 0);

        // Inefficient but who cares, this is the cold path
        loadDDSTexturesAndSetImpostor(model, baseName);
    }
    glEnable(GL_BLEND);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    vg::DepthState::restorePrevious();
    vg::BlendState::restorePrevious();

    vg::GBuffer::unuse();
    const ui32v2 screenResolution = RenderContext::getInstance().getScreenResolution();
    glViewport(0, 0, screenResolution.x, screenResolution.y);

    uploadImposterGpuData();

    LOG_DEBUG("Built billboards in {} ms", mModelsToBuild.size(), timer.elapsedMs());

    checkGlError("ModelBillboardLodBuilder::buildAllBillboards");
}

void ModelImpostorRepository::bindMaterialBuffer() const {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MODEL_IMPOSTOR_MATERIALS_SSBO, mImpostorDataBuffer.getHandle());
}

void ModelImpostorRepository::updateCachedDDS(const nString& baseName, int impostorIndex) {

    const char* pngExt = ".png";
    char numberChar[2] = { '\0', '\0' };
    if (impostorIndex != 0) {
        // Note that this purposefully skips to 2 after 0 because artists are used to 1-based indexing
        numberChar[0] = (char)impostorIndex + '1';
    }
    const char* numberStr = numberChar;

    const nString albedoPathString = baseName + "_bb" + numberStr + pngExt;
    const nString normalPathString = baseName + "_bb_n" + numberStr + pngExt;
    const nString roughPathString = baseName + "_bb_r" + numberStr + pngExt;
    const nString metalPathString = baseName + "_bb_m" + numberStr + pngExt;
    const nString aoPathString = baseName + "_bb_ao" + numberStr + pngExt;

    gli::texture2d albedoSource = PngLoader::loadPng(albedoPathString, true);
    gli::texture2d normalSource;
    gli::texture2d roughSource;
    gli::texture2d metalSource;
    gli::texture2d aoSource;

    // Optional
    bool hasAMR = false;
    if (fs::exists(normalPathString)) {
        normalSource = PngLoader::loadPng(normalPathString, true);
        assert(albedoSource.extent() == normalSource.extent());
    } else {
        assert(impostorIndex != 0 && "First impostor must have full material");
    }
    if (fs::exists(roughPathString)) {
        hasAMR = true;
        roughSource = PngLoader::loadPng(roughPathString, true);
        assert(albedoSource.extent() == roughSource.extent());
    }
    else {
        assert(impostorIndex != 0 && "First impostor must have full material");
    }
    if (fs::exists(metalPathString)) {
        hasAMR = true;
        metalSource = PngLoader::loadPng(metalPathString, true);
        assert(albedoSource.extent() == metalSource.extent());
    }
    else {
        assert(impostorIndex != 0 && "First impostor must have full material");
    }
    if (fs::exists(aoPathString)) {
        hasAMR = true;
        aoSource = PngLoader::loadPng(aoPathString, true);
        assert(albedoSource.extent() == aoSource.extent());
    }
    else {
        assert(impostorIndex != 0 && "First impostor must have full material");
    }

    // Build AMR texture
    gli::texture2d amrTexture(gli::format::FORMAT_RGB8_UNORM_PACK8, albedoSource.extent());
    int p = 0;
    for (size_t y = 0; y < albedoSource.extent().y; y++) {
        for (size_t x = 0; x < albedoSource.extent().x; x++) {
            ui8v3& pixel = amrTexture.data<ui8v3>()[p];
            pixel.x = aoSource.empty() ? 255 : aoSource.data<ui8>()[p];
            pixel.y = metalSource.empty() ? 0 : metalSource.data<ui8>()[p];
            pixel.z = roughSource.empty() ? 255 : roughSource.data<ui8>()[p];
            ++p;
        }
    }

    saveDDSTextures(baseName, albedoSource, normalSource, amrTexture, impostorIndex);
}

void ModelImpostorRepository::saveDDSTextures(
    const nString& baseName, gli::texture2d& albedoTexture, gli::texture2d& normalTexture, gli::texture2d& amrTexture, int impostorIndex
) {
    ResourceManager& resourceManager = ResourceManager::get();

    const fs::path& resourceRoot(resourceManager.getResourceRoot().getString());
    const fs::path& cacheRoot(resourceManager.getCacheRoot().getString());

    // Convert
    const gli::texture2d albedoDDS = TextureConvert::convertToDDS(albedoTexture, true /*generateMipmaps*/);

    gli::texture2d normalDDS;
    gli::texture2d amrDDS;
    if (!normalTexture.empty()) {
        normalDDS = TextureConvert::convertToDDS(normalTexture, true /*generateMipmaps*/);
    }
    if (!amrTexture.empty()) {
        amrDDS = TextureConvert::convertToDDS(amrTexture, true /*generateMipmaps*/);
    }

    // Efficiently insert a number char if needed with no string allocate
    const char* ddsExt = ".dds";
    char numberChar[2] = { '\0', '\0' };
    if (impostorIndex != 0) {
        // Note that this purposefully skips to 2 after 0 because artists are used to 1-based indexing
        numberChar[0] = (char)impostorIndex + '1';
    }
    const char* numberStr = numberChar;

    // Save DDS
    fs::path ddsBasePath = cacheRoot / fs::path(baseName).lexically_relative(resourceRoot);
    fs::path ddsAlbedoPath(ddsBasePath.string() + "_bb" + numberStr + ddsExt);
    fs::path ddsNormPath(ddsBasePath.string() + "_bb_n" + numberStr + ddsExt);
    fs::path ddsAMRPath(ddsBasePath.string() + "_bb_AMR" + numberStr + ddsExt);

    if (!FileSystem::createDirectories(ddsAlbedoPath.parent_path())) {
        panic("Failed to create {} directory. Insufficient permissions?", ddsAlbedoPath.parent_path().string());
    }

    gli::save(albedoDDS, ddsAlbedoPath.string());
    if (!normalDDS.empty()) {
        gli::save(normalDDS, ddsNormPath.string());
    }
    if (!amrDDS.empty()) {
        gli::save(amrDDS, ddsAMRPath.string());
    }
}

void ModelImpostorRepository::loadDDSTexturesAndSetImpostor(const ModelDef& modelDef, const nString& baseName) {
    ResourceManager& resourceManager = ResourceManager::get();
    TextureRepository& textureRepo = TextureRepository::get();
    const fs::path& resourceRoot(resourceManager.getResourceRoot().getString());

    const fs::path& cacheRoot(resourceManager.getCacheRoot().getString());
    fs::path ddsBasePath = cacheRoot / fs::path(baseName).lexically_relative(resourceRoot);

    ImpostorSpan& span = mModelDefImpostorSpans[modelDef.getID()];
    span.index = mImpostorGpuData.size();
    span.numBillboards = 0;

    std::vector<std::unique_ptr<ModelImpostorTextureHandles>>& handles = mImpostorTextureHandles[modelDef.getID()];

    const char* ddsExt = ".dds";
    for (int impostorIndex = 0; impostorIndex < MAX_IMPOSTOR_IMAGES; ++impostorIndex) {
        char numberChar[2] = { '\0', '\0' };
        if (impostorIndex != 0) {
            // Note that this purposefully skips to 2 after 0 because artists are used to 1-based indexing
            numberChar[0] = (char)impostorIndex + '1';
        }
        const char* numberStr = numberChar;


        std::unique_ptr<ModelImpostorTextureHandles>& impostorData = handles.emplace_back();
        assert(!impostorData);
        impostorData = std::make_unique<ModelImpostorTextureHandles>();

        // TODO: fmt is probably more efficient than string concat
        fs::path ddsAlbedoPath(ddsBasePath.string() + "_bb" + numberStr + ddsExt);
        if (!fs::exists(ddsAlbedoPath)) {
            // Failure case
            break;
        }
        gli::texture2d albedoTexture = static_cast<gli::texture2d>(gli::load(ddsAlbedoPath.string()));
        impostorData->albedoTexture = textureRepo.uploadDDSTexture(albedoTexture, vg::TextureTarget::TEXTURE_2D, vg::sSamplerStates.LINEAR_CLAMP_MIPMAP, INT_MAX);

        fs::path ddsNormPath(ddsBasePath.string() + "_bb_n" + numberStr + ddsExt);
        if (fs::exists(ddsNormPath)) {
            gli::texture2d normalTexture = static_cast<gli::texture2d>(gli::load(ddsNormPath.string()));
            impostorData->normalTexture = std::make_shared<GLTexture>(
                textureRepo.uploadDDSTexture(normalTexture, vg::TextureTarget::TEXTURE_2D, vg::sSamplerStates.LINEAR_CLAMP_MIPMAP, INT_MAX)
            );
        }
        else {
            assert(impostorIndex != 0);
            impostorData->normalTexture = handles[0]->normalTexture;
        }
        fs::path ddsAMRPath(ddsBasePath.string() + "_bb_AMR" + numberStr + ddsExt);
        if (fs::exists(ddsAMRPath)) {
            gli::texture2d amrTexture = static_cast<gli::texture2d>(gli::load(ddsAMRPath.string()));
            impostorData->amrTexture = std::make_shared<GLTexture>(
                textureRepo.uploadDDSTexture(amrTexture, vg::TextureTarget::TEXTURE_2D, vg::sSamplerStates.LINEAR_CLAMP_MIPMAP, INT_MAX)
            );
        }
        else {
            assert(impostorIndex != 0);
            impostorData->amrTexture = handles[0]->amrTexture;
        }

        // TODO: Only use bindless handles when we need to make textures resident
        ModelImpostorGpuData& gpuData = mImpostorGpuData.emplace_back();
        gpuData.albedoMap = impostorData->albedoTexture.getHandleBindless();
        // NOTE: These are potentially shared!
        gpuData.normalMap = impostorData->normalTexture->getHandleBindless();
        gpuData.aoMetallicRoughnessMap = impostorData->amrTexture->getHandleBindless();

        ++span.numBillboards;
    }
    assert(span.numBillboards);
}

void ModelImpostorRepository::uploadImposterGpuData() {
    mImpostorDataBuffer.allocate(mImpostorGpuData.size() * sizeof(ModelImpostorGpuData), mImpostorGpuData.data(), GL_DYNAMIC_STORAGE_BIT);
}
