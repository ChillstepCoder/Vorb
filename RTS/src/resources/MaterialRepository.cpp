#include "stdafx.h"
#include "MaterialRepository.h"
#include "resources/TextureRepository.h"

#include <Vorb/graphics/SamplerState.h>

#include "Vorb/io/YAML.h"
#include "Vorb/io/YAMLImpl.h"
#include <Vorb/io/FileOps.h>
#include <Vorb/io/IOManager.h>

#include "rendering/texture/MaterialTextureGenerator.h"
#include "rendering/texture/TextureConvert.h"

#include <gli/convert.hpp>

const char* GENERATE_TEXT = "GENERATE";

struct MaterialFileData {
    nString albedoTexture;
    nString normalTexture;
    nString ambientOcclusionTexture;
    nString displacementTexture;
    nString roughnessTexture;
    nString metalTexture;
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
KEG_TYPE_DEF_SAME_NAME(MaterialFileData, kt) {
    kt.addValue("albedo", keg::Value::basic(offsetof(MaterialFileData, albedoTexture), keg::BasicType::STRING));
    kt.addValue("normal", keg::Value::basic(offsetof(MaterialFileData, normalTexture), keg::BasicType::STRING));
    kt.addValue("ao", keg::Value::basic(offsetof(MaterialFileData, ambientOcclusionTexture), keg::BasicType::STRING));
    kt.addValue("disp", keg::Value::basic(offsetof(MaterialFileData, displacementTexture), keg::BasicType::STRING));
    kt.addValue("rough", keg::Value::basic(offsetof(MaterialFileData, roughnessTexture), keg::BasicType::STRING));
    kt.addValue("metal", keg::Value::basic(offsetof(MaterialFileData, metalTexture), keg::BasicType::STRING));
    kt.addValue("sampler_state", keg::Value::custom(offsetof(MaterialFileData, samplerState), "SamplerStateType", true));
    kt.addValue("emissive_color", keg::Value::basic(offsetof(MaterialFileData, emissiveColor), keg::BasicType::F32_V4));
    kt.addValue("albedo_color", keg::Value::basic(offsetof(MaterialFileData, albedoColor), keg::BasicType::F32_V4));
    kt.addValue("roughness", keg::Value::basic(offsetof(MaterialFileData, roughness), keg::BasicType::F32_V2));
    kt.addValue("transparency", keg::Value::basic(offsetof(MaterialFileData, transparencyFactor), keg::BasicType::F32));
    kt.addValue("alpha_test", keg::Value::basic(offsetof(MaterialFileData, alphaTest), keg::BasicType::F32));
    kt.addValue("metallic", keg::Value::basic(offsetof(MaterialFileData, metallicFactor), keg::BasicType::F32));
    kt.addValue("cast_shadow", keg::Value::basic(offsetof(MaterialFileData, castsShadow), keg::BasicType::BOOL));
    kt.addValue("receive_shadow", keg::Value::basic(offsetof(MaterialFileData, receivesShadow), keg::BasicType::BOOL));
    kt.addValue("flipv", keg::Value::basic(offsetof(MaterialFileData, flipV), keg::BasicType::BOOL));
}

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
    if (!mIoManager.parseFileAsKegObject((ui8*)&fileData, filePath, &KEG_GLOBAL_TYPE(MaterialFileData), true /*allowEmpty*/)) {
        LOG_CRITICAL("Failed to parse material {}", filePath.getString());
        return false;
    }

    // TODO: Evaluate if we should always be using this. This fixes crash when dimensions are not divisible by 4
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // Handle weird texture dimensions


    assert(mMaterialGpuData.size() < UINT16_MAX && "Too many materials! Increase vertex material index to 32 bits");
    const MaterialID materialId = mMaterialGpuData.size();
    MaterialData& materialData = mMaterialData.emplace_back();
    materialData.id = materialId;
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
            GLTexture uploadedTexture = textureRepository.uploadTexture(generatedTexture, vg::TextureTarget::TEXTURE_2D, *samplerState, INT_MAX);
            materialGpuData.aoMetallicRoughnessMap = uploadedTexture.getHandleBindless();
            mGeneratedAOMetallicRoughnessTextures[materialName] = std::move(uploadedTexture);
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

const MaterialGpuData& MaterialRepository::getMaterial(const nString& materialName) const {
    auto&& it = mMaterialIDLookup.find(materialName);
    assert(it != mMaterialIDLookup.end());
    return mMaterialGpuData[it->second];
}

MaterialGpuData& MaterialRepository::getMutableMaterial(const nString& materialName) {
    auto&& it = mMaterialIDLookup.find(materialName);
    assert(it != mMaterialIDLookup.end());
    return mMaterialGpuData[it->second];
}

const MaterialGpuData& MaterialRepository::getMaterial(MaterialID materialId) const {
    assert(materialId < mMaterialGpuData.size());
    return mMaterialGpuData[materialId];
}

MaterialGpuData& MaterialRepository::getMutableMaterial(MaterialID materialId){
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
    handle.data = &getMutableMaterial(handle.materialId);
    handle.name = materialName;
    return handle;
}

MaterialData MaterialRepository::getMaterialData(const nString& materialName) const {
    auto&& it = mMaterialIDLookup.find(materialName);
    assert(it != mMaterialIDLookup.end());
    return mMaterialData[it->second];
}

MaterialData MaterialRepository::getMaterialData(MaterialID materialId) const {
    assert(materialId < mMaterialData.size());
    return mMaterialData[materialId];
}

void MaterialRepository::uploadMaterialData() {
    mMaterialDataBuffer.allocate(sizeof(MaterialGpuData) * mMaterialGpuData.size(), mMaterialGpuData.data(), 0);
}

void MaterialRepository::bindMaterialBuffer() const {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_GLOBAL_MATERIAL_SSBO, mMaterialDataBuffer.getHandle());
}
