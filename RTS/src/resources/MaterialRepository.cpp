#include "stdafx.h"
#include "MaterialRepository.h"
#include "resources/TextureRepository.h"

#include <Vorb/graphics/SamplerState.h>
#include "serialization/VorbSerializableDefs.h"

#include "Vorb/io/YAML.h"
#include "Vorb/io/YAMLImpl.h"
#include <Vorb/io/FileOps.h>
#include <Vorb/io/IOManager.h>

#include "rendering/texture/MaterialTextureGenerator.h"
#include "rendering/texture/TextureConvert.h"

#include "filesystem/FileSystem.h"

#include <resources/ResourceManager.h>
#include <gli/convert.hpp>
#include <gli/gli.hpp>

const char* GENERATE_TEXT = "GENERATE";

struct MaterialFileData {
    nString albedoTexture;
    nString normalTexture;
    nString ambientOcclusionTexture;
    nString displacementTexture;
    nString roughnessTexture;
    nString metalTexture;
    MaterialRenderPassType renderPass = MaterialRenderPassType::Default;
    vg::SamplerStateType samplerState = vg::SamplerStateType::LINEAR_WRAP_MIPMAP;
    f32v4 emissiveColor = { 0.0f, 0.0f, 0.0f, 0.0f };
    f32v4 albedoColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    // UV anisotropic roughness (isotropic lighting models use only the first value). ZW values are ignored
    f32v2 roughness = { 1.0f, 1.0f };
    f32 transparencyFactor = 1.0f; // UNUSED
    f32 alphaTest = 0.01f;
    f32 metallicFactor = 0.0f;
    bool castsShadow = true;
    bool receivesShadow = true;
    bool flipV = false;

};

SERIALIZABLE_SIMPLE(MaterialFileData,
    o.albedoTexture, "albedo"sv,
    o.normalTexture, "normal"sv,
    o.ambientOcclusionTexture, "ao"sv,
    o.displacementTexture, "disp"sv,
    o.roughnessTexture, "rough"sv,
    o.metalTexture, "metal"sv,
    o.renderPass, "render_pass"sv,
    o.samplerState, "sampler_state"sv,
    o.emissiveColor, "emissive_color"sv,
    o.albedoColor, "albedo_color"sv,
    o.roughness, "roughness"sv,
    o.transparencyFactor, "transparency"sv,
    o.alphaTest, "alpha_test"sv,
    o.metallicFactor, "metallic"sv,
    o.castsShadow, "cast_shadow"sv,
    o.receivesShadow, "receive_shadow"sv,
    o.flipV, "flipv"sv
)

MaterialRepository::MaterialRepository(vio::IOManager& ioManager) : mIoManager(ioManager)
{
    mMaterialTextureGenerator = std::make_unique<MaterialTextureGenerator>();
    mMaterialTextureGenerator->init();
}

MaterialRepository::~MaterialRepository()
{

}

nString getImplicitAlbedoPath(const vio::Path& materialPath) {
    return materialPath.getFileNameNoExtension() + ".png";
}

nString getImplicitNormalPath(const vio::Path& materialPath) {
    return materialPath.getFileNameNoExtension() + "_norm.png";
}

bool MaterialRepository::loadMaterial(const vio::Path& filePath, TextureRepository& textureRepository) {
    MaterialFileData fileData;

    const nString fileStr = mIoManager.readFileToString(filePath);
    if (fileStr.size()) {
        YmlSerializer::deserializeYml(fileStr, fileData);
    }

    // TODO: Evaluate if we should always be using this. This fixes crash when dimensions are not divisible by 4
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // Handle weird texture dimensions

    assert(mMaterialGpuData.size() < UINT16_MAX && "Too many materials! Increase vertex material index to 32 bits");
    const MaterialID materialId = mMaterialGpuData.size();
    MaterialDesc& materialData = mMaterialDescs.emplace_back();
    materialData.id = materialId;
    materialData.renderPass = fileData.renderPass;
    MaterialGpuData& materialGpuData = mMaterialGpuData.emplace_back();
    const nString materialName = filePath.getFileNameNoExtension();
    vio::Path folderPath = filePath;
    --folderPath;

    LOG_INFO("LOADING MATERIAL {} {}", filePath.getString(), materialId);

    // Filepath Fallbacks
    if (fileData.albedoTexture.empty()) {
        // Fall back to using the file name as the albedo, so we can just specify an empty .material file
        fileData.albedoTexture = filePath.getFileNameNoExtension() + ".png";
    }
    const vio::Path albedoTexturePath = folderPath / fileData.albedoTexture;
    if (fileData.normalTexture.empty()) {
        // Fall back to using the file name as the normal, so we can just specify an empty .material file
        fileData.normalTexture = filePath.getFileNameNoExtension() + "_norm.png";
    }
    const vio::Path normalTexturePath = folderPath / fileData.normalTexture;
    if (fileData.ambientOcclusionTexture.empty()) {
        // Fall back to using the file name as the normal, so we can just specify an empty .material file
        fileData.ambientOcclusionTexture = filePath.getFileNameNoExtension() + "_ao.png";
    }
    const vio::Path ambientOcclusionTexturePath = folderPath / fileData.ambientOcclusionTexture;
    if (fileData.displacementTexture.empty()) {
        // Fall back to using the file name as the normal, so we can just specify an empty .material file
        fileData.displacementTexture = filePath.getFileNameNoExtension() + "_disp.png";
    }
    const vio::Path displacementTexturePath = folderPath / fileData.displacementTexture;
    if (fileData.roughnessTexture.empty()) {
        // Fall back to using the file name as the normal, so we can just specify an empty .material file
        fileData.roughnessTexture = filePath.getFileNameNoExtension() + "_rough.png";
    }
    const vio::Path roughnessTexturePath = folderPath / fileData.roughnessTexture;
    if (fileData.metalTexture.empty()) {
        // Fall back to using the file name as the normal, so we can just specify an empty .material file
        fileData.metalTexture = filePath.getFileNameNoExtension() + "_metal.png";
    }
    const vio::Path metalTexturePath = folderPath / fileData.metalTexture;

    const vg::SamplerState* samplerState = &vg::sSamplerStates.STATE_ARRAY[e_cast(fileData.samplerState)];
    // TODO: Texture Compression
    const TextureData* albedoTextureData = textureRepository.loadTexture(albedoTexturePath, vg::TextureTarget::TEXTURE_2D, samplerState, fileData.flipV);
    if (!albedoTextureData) {
        LOG_CRITICAL("Failed to load albedo texture {} for material {}", fileData.albedoTexture, filePath.getString());
        return false;
    }
    const ui32v2 textureDims = albedoTextureData->texture.getDims();
    materialGpuData.albedoMap = albedoTextureData->texture.getHandleBindless();
    materialData.albedoTexture = albedoTextureData->texture.getHandle();

    // Normal
    // Generated vs loaded normals
    // TODO RGTC compression https://www.reddit.com/r/opengl/comments/dyedbv/when_to_use_compressed_textures/
    if (fileData.normalTexture == GENERATE_TEXT) {
        VGTexture normalTexture = mMaterialTextureGenerator->generateNormalTexture(albedoTextureData->texture.getHandle(), albedoTextureData->texture.getDims(), *samplerState);
        GLTexture& normalGLTexture = mGeneratedNormalTextures[materialName];
        normalGLTexture.init(normalTexture, vg::TextureTarget::TEXTURE_2D, albedoTextureData->texture.getDims());
        materialGpuData.normalMap = normalGLTexture.getHandleBindless();
    }
    else if (mIoManager.fileExists(normalTexturePath)) {
        const TextureData* normalTextureData = textureRepository.loadTexture(normalTexturePath, vg::TextureTarget::TEXTURE_2D, samplerState, fileData.flipV);
        if (!normalTextureData) {
            LOG_CRITICAL("Failed to load normal texture {} for material {}", fileData.normalTexture, filePath.getString());
            return false;
        }
        if (normalTextureData->texture.getDims() != textureDims) {
            LOG_CRITICAL("Roughness texture {} for material {} doesn't match albedo dims", fileData.roughnessTexture, filePath.getString());
            return false;
        }
        materialGpuData.normalMap = normalTextureData->texture.getHandleBindless();
    }
    
    // Displacement
    if (mIoManager.fileExists(displacementTexturePath)) {
        const TextureData* displacementTextureData = textureRepository.loadTexture(displacementTexturePath, vg::TextureTarget::TEXTURE_2D, samplerState, fileData.flipV);
        if (!displacementTextureData) {
            LOG_CRITICAL("Failed to load displacement texture {} for material {}", fileData.displacementTexture, filePath.getString());
            return false;
        }
        if (displacementTextureData->texture.getDims() != textureDims) {
            LOG_CRITICAL("Roughness texture {} for material {} doesn't match albedo dims", fileData.roughnessTexture, filePath.getString());
            return false;
        }
        materialGpuData.displacementMap = displacementTextureData->texture.getHandleBindless();
    }

    // Ambient Occlusion, Metallic, Roughness
    {
        gli::texture2d aoData;
        gli::texture2d roughnessData;
        gli::texture2d metalData;
        fs::path ddsPath(albedoTexturePath.getString());
        const fs::path& resourceRoot(Services::ResourceManager::ref().getResourceRoot().getString());
        ddsPath.replace_extension("_AMR.dds");
        ddsPath = ddsPath.lexically_relative(resourceRoot);
        ddsPath = resourceRoot / "_cache" / ddsPath;

        bool needsGenerateDDS = false;
        time_t fileLastWriteTime = 0;
        if (fs::exists(ddsPath)) {
            fileLastWriteTime = FileSystem::getLastFileWriteTime(ddsPath);
        }
        else {
            needsGenerateDDS = true;
        }

        fs::path stdAoPath(ambientOcclusionTexturePath.getString());
        fs::path stdRoughnessPath(roughnessTexturePath.getString());
        fs::path stdMetalPath(metalTexturePath.getString());

        // Find target path
        if (fs::exists(stdAoPath) && fs::is_regular_file(stdAoPath)) {
            needsGenerateDDS |= FileSystem::getLastFileWriteTime(stdAoPath) >= fileLastWriteTime;
        }
        if (fs::exists(stdRoughnessPath) && fs::is_regular_file(stdRoughnessPath)) {
            needsGenerateDDS |= FileSystem::getLastFileWriteTime(stdRoughnessPath) >= fileLastWriteTime;
        }
        if (fs::exists(stdMetalPath) && fs::is_regular_file(stdMetalPath)) {
            needsGenerateDDS |= FileSystem::getLastFileWriteTime(stdMetalPath) >= fileLastWriteTime;
        }

        if (needsGenerateDDS) {
            // TODO: Free maps
            bool hasTexture = false;
            // AO
            if (mIoManager.fileExists(ambientOcclusionTexturePath)) {
                aoData = textureRepository.loadRawPngData(ambientOcclusionTexturePath, fileData.flipV);
                if (!aoData.size()) {
                    LOG_CRITICAL("Failed to load AO texture {} for material {}", fileData.ambientOcclusionTexture, filePath.getString());
                    return false;
                }
                if (ui32v2(aoData.extent().x, aoData.extent().y) != textureDims) {
                    LOG_CRITICAL("AO texture {} for material {} doesn't match albedo dims", fileData.ambientOcclusionTexture, filePath.getString());
                    return false;
                }
                if (aoData.format() != gli::FORMAT_R8_UNORM_PACK8) {
                    LOG_WARN("Converting {} to 8 bit depth. Consider re-exporting file to 8 bits", ambientOcclusionTexturePath.getString());
                    aoData = TextureConvert::convertToR8(aoData);
                }
                hasTexture = true;
            }

            // Roughness
            if (mIoManager.fileExists(roughnessTexturePath)) {
                roughnessData = textureRepository.loadRawPngData(roughnessTexturePath, fileData.flipV);
                if (!roughnessData.size()) {
                    LOG_CRITICAL("Failed to load roughness texture {} for material {}", fileData.roughnessTexture, filePath.getString());
                    return false;
                }
                if (ui32v2(roughnessData.extent().x, roughnessData.extent().y) != textureDims) {
                    LOG_CRITICAL("Roughness texture {} for material {} doesn't match albedo dims", fileData.roughnessTexture, filePath.getString());
                    return false;
                }
                if (roughnessData.format() != gli::FORMAT_R8_UNORM_PACK8) {
                    LOG_WARN("Converting {} to 8 bit depth. Consider re-exporting file to 8 bits", roughnessTexturePath.getString());
                    roughnessData = TextureConvert::convertToR8(roughnessData);
                }
                hasTexture = true;
            }

            // Metallic
            if (mIoManager.fileExists(metalTexturePath)) {
                metalData = textureRepository.loadRawPngData(metalTexturePath, fileData.flipV);
                if (!metalData.size()) {
                    LOG_CRITICAL("Failed to load Metallic texture {} for material {}", fileData.metalTexture, filePath.getString());
                    return false;
                }
                if (ui32v2(metalData.extent().x, metalData.extent().y) != textureDims) {
                    LOG_CRITICAL("Metallic texture {} for material {} doesn't match albedo dims", fileData.metalTexture, filePath.getString());
                    return false;
                }
                if (metalData.format() != gli::FORMAT_R8_UNORM_PACK8) {
                    LOG_WARN("Converting {} to 8 bit depth. Consider re-exporting file to 8 bits", metalTexturePath.getString());
                    metalData = TextureConvert::convertToR8(metalData);
                }
                hasTexture = true;
            }

            if (hasTexture) {
                LOG_WARN("Generating AoRoughnessMetallicTexture");
                gli::texture2d generatedTexture = mMaterialTextureGenerator->generateAoRoughnessMetallicTexture(aoData, roughnessData, metalData, textureDims, *samplerState);
                // DDS convert
                gli::texture2d ddsTexture = TextureConvert::convertToDDS(generatedTexture);
                GLTexture uploadedTexture = textureRepository.uploadDDSTexture(ddsTexture, vg::TextureTarget::TEXTURE_2D, *samplerState, INT_MAX);
                gli::save(ddsTexture, ddsPath.string());

                materialGpuData.aoMetallicRoughnessMap = uploadedTexture.getHandleBindless();
                mGeneratedAOMetallicRoughnessTextures[materialName] = std::move(uploadedTexture);
            }

        }
        else {
            // LOAD FROM FILE
              // Assume 2d texture (potentially unsafe?)
            LOG_INFO("Loading cached dds...");
            gli::texture2d ddsTexture(gli::load(ddsPath.string()));
            textureRepository.uploadDDSTexture(ddsTexture, vg::TextureTarget::TEXTURE_2D, *samplerState, INT_MAX);
        }
    }

    // Copy properties
    materialGpuData.emissiveColor = fileData.emissiveColor;
    materialGpuData.albedoColor = fileData.albedoColor;
    materialGpuData.roughness.x = fileData.roughness.x;
    materialGpuData.roughness.y = fileData.roughness.y;
    materialGpuData.transparencyFactor = fileData.transparencyFactor;
    materialGpuData.alphaTest = fileData.alphaTest;
    materialGpuData.metallicFactor = fileData.metallicFactor;
    materialGpuData.flags = (MaterialFlags_CastShadow * (int)fileData.castsShadow) | (MaterialFlags_ReceiveShadow * (int)fileData.receivesShadow);

    mMaterialIDLookup[materialName] = materialId;
    return true;
}

const MaterialGpuData& MaterialRepository::getMaterialGpuData(const nString& materialName) const {
    auto&& it = mMaterialIDLookup.find(materialName);
    assert(it != mMaterialIDLookup.end());
    return mMaterialGpuData[it->second];
}

MaterialGpuData& MaterialRepository::getMutableMaterialGpuData(const nString& materialName) {
    auto&& it = mMaterialIDLookup.find(materialName);
    assert(it != mMaterialIDLookup.end());
    return mMaterialGpuData[it->second];
}

const MaterialGpuData& MaterialRepository::getMaterialGpuData(MaterialID materialId) const {
    assert(materialId < mMaterialGpuData.size());
    return mMaterialGpuData[materialId];
}

MaterialGpuData& MaterialRepository::getMutableMaterialGpuData(MaterialID materialId){
    assert(materialId < mMaterialGpuData.size());
    return mMaterialGpuData[materialId];
}

MaterialID MaterialRepository::getMaterialId(const nString& materialName) const {
    auto&& it = mMaterialIDLookup.find(materialName);
    assert(it != mMaterialIDLookup.end());
    return it->second;
}

MaterialHandle MaterialRepository::getMutableMaterialHandle(const nString& materialName) {
    MaterialHandle handle;
    handle.materialId = getMaterialId(materialName);
    handle.data = &getMutableMaterialGpuData(handle.materialId);
    handle.name = materialName;
    return handle;
}

const MaterialDesc& MaterialRepository::getMaterialDesc(const nString& materialName) const {
    auto&& it = mMaterialIDLookup.find(materialName);
    assert(it != mMaterialIDLookup.end());
    return mMaterialDescs[it->second];
}

const MaterialDesc& MaterialRepository::getMaterialDesc(MaterialID materialId) const {
    assert(materialId < mMaterialDescs.size());
    return mMaterialDescs[materialId];
}

void MaterialRepository::uploadMaterialData() {
    mMaterialDataBuffer.allocate(sizeof(MaterialGpuData) * mMaterialGpuData.size(), mMaterialGpuData.data(), 0);
}

void MaterialRepository::bindMaterialBuffer() const {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_GLOBAL_MATERIAL_SSBO, mMaterialDataBuffer.getHandle());
}
