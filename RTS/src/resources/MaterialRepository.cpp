#include "stdafx.h"
#include "MaterialRepository.h"
#include "resources/TextureRepository.h"

#include <Vorb/graphics/SamplerState.h>

#include "Vorb/io/YAML.h"
#include "Vorb/io/YAMLImpl.h"
#include <Vorb/io/FileOps.h>
#include <Vorb/io/IOManager.h>

#include "rendering/texture/NormalMapGenerator.h"

const char* GENERATE_TEXT = "GENERATE";

struct MaterialFileData {
    nString albedoTexture;
    nString normalTexture;
    nString ambientOcclusionTexture;
    nString displacementTexture;
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
    kt.addValue("ambient_occlusion", keg::Value::basic(offsetof(MaterialFileData, ambientOcclusionTexture), keg::BasicType::STRING));
    kt.addValue("displacement", keg::Value::basic(offsetof(MaterialFileData, displacementTexture), keg::BasicType::STRING));
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
    mNormalMapGenerator = std::make_unique<NormalMapGenerator>();
    mNormalMapGenerator->init();
}

MaterialRepository::~MaterialRepository()
{

}


bool MaterialRepository::loadMaterial(const vio::Path& filePath, TextureRepository& textureRepository) {
    MaterialFileData fileData;
    if (!mIoManager.parseFileAsKegObject((ui8*)&fileData, filePath, &KEG_GLOBAL_TYPE(MaterialFileData), true /*allowEmpty*/)) {
        LOG_CRITICAL("Failed to parse material {}", filePath.getString());
        return false;
    }

    const MaterialID materialId = mMaterials.size();
    assert(materialId < UINT16_MAX && "Too many materials! Increase vertex material index to 32 bits");
    MaterialData& materialData = mMaterials.emplace_back();
    const nString materialName = filePath.getFileNameNoExtension();
    vio::Path folderPath = filePath;
    --folderPath;

    // Albedo
    if (fileData.albedoTexture.empty()) {
        // Fall back to using the file name as the albedo, so we can just specify an empty .material file
        fileData.albedoTexture = filePath.getFileNameNoExtension() + ".png";
    }
    const vg::SamplerState* samplerState = &vg::sSamplerStates.STATE_ARRAY[e_cast(fileData.samplerState)];
    const TextureData* albedoTextureData = textureRepository.loadTextureNew(folderPath / fileData.albedoTexture, vg::TextureTarget::TEXTURE_2D, samplerState, fileData.flipV);
    if (!albedoTextureData) {
        LOG_CRITICAL("Failed to load albedo texture {} for material {}", fileData.albedoTexture, filePath.getString());
        return false;
    }
    materialData.albedoMap = albedoTextureData->texture.getHandleBindless();

    // Normal
    if (fileData.normalTexture.empty()) {
        // Fall back to using the file name as the normal, so we can just specify an empty .material file
        vio::Path implicitPath = filePath.getFileNameNoExtension() + ".norm.png";
        if (mIoManager.fileExists(folderPath / implicitPath)) {
            fileData.normalTexture = implicitPath.getString();
        }
    }
    // Generated vs loaded normals
    if (fileData.normalTexture == GENERATE_TEXT) {
        VGTexture normalTexture = mNormalMapGenerator->generateNormalTexture(albedoTextureData->texture.getHandle(), albedoTextureData->texture.getDims(), *samplerState);
        GLTexture& normalGLTexture = mGeneratedNormalTextures[materialName];
        normalGLTexture.init(normalTexture, vg::TextureTarget::TEXTURE_2D, albedoTextureData->texture.getDims());
        materialData.normalMap = normalGLTexture.getHandleBindless();
    }
    else if (fileData.normalTexture.size()) {
        const TextureData* normalTextureData = textureRepository.loadTextureNew(folderPath / fileData.normalTexture, vg::TextureTarget::TEXTURE_2D, samplerState, fileData.flipV);
        if (!normalTextureData) {
            LOG_CRITICAL("Failed to load normal texture {} for material {}", fileData.normalTexture, filePath.getString());
            return false;
        }
        materialData.normalMap = normalTextureData->texture.getHandleBindless();
    }

    // Ambient Occlusion
    if (fileData.ambientOcclusionTexture.size()) {
        assert(false); // TODO
    }

    // Displacement
    if (fileData.displacementTexture.size()) {
        assert(false); // TODO
    }

    // Copy properties
    materialData.emissiveColor = fileData.emissiveColor;
    materialData.albedoColor = fileData.albedoColor;
    materialData.roughness.x = fileData.roughness.x;
    materialData.roughness.y = fileData.roughness.y;
    materialData.transparencyFactor = fileData.transparencyFactor;
    materialData.alphaTest = fileData.alphaTest;
    materialData.metallicFactor = fileData.metallicFactor;
    materialData.flags = (MaterialFlags_CastShadow * (int)fileData.castsShadow) | (MaterialFlags_ReceiveShadow * (int)fileData.receivesShadow);

    mMaterialIDLookup[materialName] = materialId;
    return true;
}

const MaterialData& MaterialRepository::getMaterial(const nString& materialName) const {
    auto&& it = mMaterialIDLookup.find(materialName);
    assert(it != mMaterialIDLookup.end());
    return mMaterials[it->second];
}

const MaterialData& MaterialRepository::getMaterial(MaterialID materialId) const {
    assert(materialId < mMaterials.size());
    return mMaterials[materialId];
}

MaterialID MaterialRepository::getMaterialId(const nString& materialName) const {
    auto&& it = mMaterialIDLookup.find(materialName);
    assert(it != mMaterialIDLookup.end());
    return it->second;
}

void MaterialRepository::uploadMaterialData() {
    mMaterialDataBuffer.allocate(sizeof(MaterialData) * mMaterials.size(), mMaterials.data(), 0);
}

void MaterialRepository::bindMaterialBuffer() const {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_GLOBAL_MATERIAL_SSBO, mMaterialDataBuffer.getHandle());
}
