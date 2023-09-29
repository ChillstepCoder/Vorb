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

#include "filesystem/FileSystem.h"

#include <resources/ResourceManager.h>
#include <gli/convert.hpp>
#include <gli/gli.hpp>

constexpr StrToken GENERATE_TEXT = CStrToken("generate");

SERIALIZABLE_SIMPLE(MaterialDef,
    make_field(o.albedoTexture, "albedo"sv),
    make_field(o.normalTexture, "normal"sv),
    make_field(o.ambientOcclusionTexture, "ao"sv),
    make_field(o.displacementTexture, "disp"sv),
    make_field(o.roughnessTexture, "rough"sv),
    make_field(o.metalTexture, "metal"sv),
    make_field(o.renderPass, "render_pass"sv),
    make_field(o.samplerState, "sampler_state"sv),
    make_field(o.emissiveColor, "emissive_color"sv),
    make_field(o.albedoColor, "albedo_color"sv),
    make_field(o.roughness, "roughness"sv),
    make_field(o.transparencyFactor, "transparency"sv),
    make_field(o.alphaTest, "alpha_test"sv),
    make_field(o.metallicFactor, "metallic"sv),
    make_field(o.castsShadow, "cast_shadow"sv),
    make_field(o.receivesShadow, "receive_shadow"sv),
    make_field(o.flipV, "flipv"sv)
)

struct MaterialLoadUserData {
    gli::texture2d aoMetalRoughData;
};

MaterialRepository::MaterialRepository(vio::IOManager& ioManager) : IAssetRepository<MaterialDef>(ioManager) {
}

MaterialRepository::~MaterialRepository() = default;

void MaterialRepository::init() {
    mMaterialTextureGenerator = std::make_unique<MaterialTextureGenerator>();
    mMaterialTextureGenerator->init();
}

const MaterialGpuData& MaterialRepository::getMaterialGpuData(StrToken materialName) const {
    auto&& it = mAssetLookup.find(materialName);
    assert(it != mAssetLookup.end());
    return mMaterialGpuData[it->second];
}

MaterialGpuData& MaterialRepository::getMutableMaterialGpuData(StrToken materialName) {
    auto&& it = mAssetLookup.find(materialName);
    assert(it != mAssetLookup.end());
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

const MaterialDesc& MaterialRepository::getMaterialDesc(StrToken materialName) const {
    auto&& it = mAssetLookup.find(materialName);
    assert(it != mAssetLookup.end());
    return mMaterialDescs[it->second];
}

const MaterialDesc& MaterialRepository::getMaterialDesc(MaterialID materialId) const {
    assert(materialId < mMaterialDescs.size());
    return mMaterialDescs[materialId];
}

void MaterialRepository::bindMaterialBuffer() const {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_GLOBAL_MATERIAL_SSBO, mMaterialDataBuffer.getHandle());
}

AssetLoadFunc MaterialRepository::getAssetLoadFunc() {
    return [&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr, userData) {
        MaterialDef& materialDef = *static_cast<MaterialDef*>(assetDataPtr);
        TextureRepository& textureRepo = TextureRepository::get();
        MaterialLoadUserData& loadData = std::any_cast<MaterialLoadUserData&>(userData);

        if (assetID > UINT16_MAX) panic("Too many materials detected in getAssetLoadFunc. Max UINT16_MAX");

        const MaterialID materialId = assetID;
        MaterialDesc& materialData = mMaterialDescs.at(assetID);
        materialData.id = materialId;
        materialData.renderPass = materialDef.renderPass;
        MaterialGpuData& materialGpuData = mMaterialGpuData.at(assetID);
        const nString materialName = filePath.getFileNameNoExtension();
        StrToken tokenName(materialName);
        assert(tokenName == materialDef.getName());

        LOG_INFO("LOADING MATERIAL {} {}", filePath.getString(), materialId);

        // Filepath Fallbacks
        if (!materialDef.albedoTexture.isValid()) {
            if (textureRepo.isAssetRegistered(tokenName)) {
                materialDef.albedoTexture = tokenName;
            } else {
                panic("Material {} does not have an albedo texture", materialName);
            }
        }
        if (!materialDef.normalTexture.isValid()) {
            StrToken name(materialName + "_norm");
            if (textureRepo.isAssetRegistered(name)) {
                materialDef.normalTexture = name;
            }
        }
        if (!materialDef.displacementTexture.isValid()) {
            StrToken name(materialName + "_disp");
            if (textureRepo.isAssetRegistered(name)) {
                materialDef.displacementTexture = name;
            }
        }

        materialDef.addDependency(TextureRepository::get().getAssetHandle(materialDef.albedoTexture));

        // Normal
        // Generated vs loaded normals
        // TODO RGTC compression https://www.reddit.com/r/opengl/comments/dyedbv/when_to_use_compressed_textures/
        if (materialDef.normalTexture.isValid()) {
            if (materialDef.normalTexture != GENERATE_TEXT) {
                materialDef.addDependency(TextureRepository::get().getAssetHandle(materialDef.normalTexture));
            }
        }

        // Displacement
        if (materialDef.displacementTexture.isValid()) {
            materialDef.addDependency(TextureRepository::get().getAssetHandle(materialDef.displacementTexture));
        }

        // Ambient Occlusion, Metallic, Roughness
        {
            if (!materialDef.ambientOcclusionTexture.isValid()) {
                // Fall back to using the file name as the normal, so we can just specify an empty .material file
                materialDef.ambientOcclusionTexture = StrToken(materialName + "_ao");
            }
            if (!materialDef.roughnessTexture.isValid()) {
                // Fall back to using the file name as the normal, so we can just specify an empty .material file
                materialDef.roughnessTexture = StrToken(materialName + "_r");
            }
            if (!materialDef.metalTexture.isValid()) {
                // Fall back to using the file name as the normal, so we can just specify an empty .material file
                materialDef.metalTexture = StrToken(materialName + "_m");
            }

            vio::Path folderPath = filePath;
            --folderPath;
            const vio::Path ambientOcclusionTexturePath = folderPath / materialDef.ambientOcclusionTexture.toString();
            const vio::Path roughnessTexturePath = folderPath / materialDef.roughnessTexture.toString();
            const vio::Path metalTexturePath = folderPath / materialDef.metalTexture.toString();

            fs::path ddsPath(textureRepo.getAssetFilePath(materialDef.albedoTexture).getCString());
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

            gli::texture2d aoData;
            gli::texture2d roughnessData;
            gli::texture2d metalData;

            if (needsGenerateDDS) {
                // TODO: Free maps
                bool hasTexture = false;
                // AO
                if (mIoManager.fileExists(ambientOcclusionTexturePath)) {
                    aoData = textureRepo.loadRawPngData(ambientOcclusionTexturePath, materialDef.flipV);
                    if (!aoData.size()) {
                        panic("Failed to load AO texture {} for material {}", materialDef.ambientOcclusionTexture.toString(), filePath.getString());
                    }
                    if (aoData.format() != gli::FORMAT_R8_UNORM_PACK8) {
                        LOG_WARN("Converting {} to 8 bit depth. Consider re-exporting file to 8 bits", ambientOcclusionTexturePath.getString());
                        aoData = TextureConvert::convertToR8(aoData);
                    }
                    hasTexture = true;
                }

                // Roughness
                if (mIoManager.fileExists(roughnessTexturePath)) {
                    roughnessData = textureRepo.loadRawPngData(roughnessTexturePath, materialDef.flipV);
                    if (!roughnessData.size()) {
                        panic("Failed to load roughness texture {} for material {}", materialDef.roughnessTexture.toString(), filePath.getString());
                    }
                    if (roughnessData.format() != gli::FORMAT_R8_UNORM_PACK8) {
                        LOG_WARN("Converting {} to 8 bit depth. Consider re-exporting file to 8 bits", roughnessTexturePath.getString());
                        roughnessData = TextureConvert::convertToR8(roughnessData);
                    }
                    hasTexture = true;
                }

                // Metallic
                if (mIoManager.fileExists(metalTexturePath)) {
                    metalData = textureRepo.loadRawPngData(metalTexturePath, materialDef.flipV);
                    if (!metalData.size()) {
                        panic("Failed to load Metallic texture {} for material {}", materialDef.metalTexture.toString(), filePath.getString());
                    }
                    if (metalData.format() != gli::FORMAT_R8_UNORM_PACK8) {
                        LOG_WARN("Converting {} to 8 bit depth. Consider re-exporting file to 8 bits", metalTexturePath.getString());
                        metalData = TextureConvert::convertToR8(metalData);
                    }
                    hasTexture = true;
                }

                if (hasTexture) {
                    LOG_WARN("Generating AoRoughnessMetallicTexture");
                    gli::texture2d generatedTexture = mMaterialTextureGenerator->combineAoRoughnessMetallicTextureData(aoData, roughnessData, metalData);
                    // DDS convert
                    loadData.aoMetalRoughData = TextureConvert::convertToDDS(generatedTexture);
                    gli::save(loadData.aoMetalRoughData, ddsPath.string());
                }

            }
            else {
                // LOAD FROM FILE
                  // Assume 2d texture (potentially unsafe?)
                LOG_INFO("Loading cached dds...");
                loadData.aoMetalRoughData = static_cast<gli::texture2d>(gli::load(ddsPath.string()));
            }
        }

        // Copy properties
        materialGpuData.emissiveColor = materialDef.emissiveColor;
        materialGpuData.albedoColor = materialDef.albedoColor;
        materialGpuData.roughness.x = materialDef.roughness.x;
        materialGpuData.roughness.y = materialDef.roughness.y;
        materialGpuData.transparencyFactor = materialDef.transparencyFactor;
        materialGpuData.alphaTest = materialDef.alphaTest;
        materialGpuData.metallicFactor = materialDef.metallicFactor;
        materialGpuData.flags = (MaterialFlags_CastShadow * (int)materialDef.castsShadow) | (MaterialFlags_ReceiveShadow * (int)materialDef.receivesShadow);

        // Finish with GPU lambda after dependant textures loaded
        assetLoader.requestAssetLoadWithDependencies(nullptr, [&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr, userData) {
            MaterialDef& materialDef = *static_cast<MaterialDef*>(assetDataPtr);
            MaterialLoadUserData& loadData = std::any_cast<MaterialLoadUserData&>(userData);
            // TODO: Evaluate if we should always be using this. This fixes crash when dimensions are not divisible by 4
           // TODO: MOVE
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // Handle weird texture dimensions

            const vg::SamplerState* samplerState = &vg::sSamplerStates.STATE_ARRAY[e_cast(materialDef.samplerState)];

            // Grab dependencies
            AssetHandleBundle& dependencies = *materialDef.getDependencies();
            const TextureDef& albedo = dependencies.getLoadedAsset<TextureDef>(materialDef.albedoTexture);
            

            const ui32v2 textureDims = albedo.gpuTexture.getDims();
            materialGpuData.albedoMap = albedo.gpuTexture.getHandleBindless();

            if (materialDef.normalTexture.isValid()) {
                if (materialDef.normalTexture == GENERATE_TEXT) {
                    VGTexture normalTexture = mMaterialTextureGenerator->generateNormalTexture(albedo.gpuTexture.getHandle(), textureDims, *samplerState);
                    GLTexture& normalGLTexture = mGeneratedNormalTextures[tokenName];
                    normalGLTexture.init(normalTexture, vg::TextureTarget::TEXTURE_2D, textureDims);
                    materialGpuData.normalMap = normalGLTexture.getHandleBindless();
                }
                else {
                    const TextureDef& normal = dependencies.getLoadedAsset<TextureDef>(materialDef.normalTexture);
                    materialGpuData.normalMap = normal.gpuTexture.getHandleBindless();
                    if (normal.gpuTexture.getDims() != textureDims) {
                        panic("Normal texture {} for material {} doesn't match albedo dims", materialDef.normalTexture.toString(), filePath.getString());
                    }
                }
            }
            else {
                // TODO
                LOG_WARN("Missing normal texture for {}", filePath.getString());
            }

            if (materialDef.displacementTexture.isValid()) {
                const TextureDef& disp = dependencies.getLoadedAsset<TextureDef>(materialDef.displacementTexture);

                if (disp.gpuTexture.getDims() != textureDims) {
                    panic("Disp texture {} for material {} doesn't match albedo dims", materialDef.displacementTexture.toString(), filePath.getString());
                }
                materialGpuData.displacementMap = disp.gpuTexture.getHandleBindless();
            }

            // Upload roughness metallic spec
            if (!loadData.aoMetalRoughData.empty()) {
                GLTexture& uploadedTexture = mGeneratedAOMetallicRoughnessTextures[tokenName];
                uploadedTexture = textureRepo.uploadDDSTexture(loadData.aoMetalRoughData, vg::TextureTarget::TEXTURE_2D, *samplerState, INT_MAX);
                materialGpuData.aoMetallicRoughnessMap = uploadedTexture.getHandleBindless();
            }

            // Update material data
            const size_t bufferCapacity = sizeof(MaterialGpuData) * mMaterialGpuData.size();
            if (mMaterialDataBuffer.getCapacity() != bufferCapacity) {
                mMaterialDataBuffer.allocate(bufferCapacity, mMaterialGpuData.data(), GL_DYNAMIC_STORAGE_BIT);
            }
            else {
                mMaterialDataBuffer.updateSubData(assetID * sizeof(MaterialGpuData), sizeof(MaterialGpuData), &materialGpuData);
            }

            return true;
        },
            assetID,
            assetDataPtr,
            filePath,
            mLoadedAssets[assetID].get(),
            std::move(userData),
            materialDef.getDependencies()
        );

        return false;
    };
}

void MaterialRepository::onRegisteredAsset(AssetID id) {
    assert(mMaterialGpuData.size() < UINT16_MAX && "Too many materials! Increase vertex material index to 32 bits");

    // Load file data immediately so we can build material desc
    MaterialDef& materialDef = *mAssets[id];
    YmlSerializer::readFileData(readFileToString(mAssetRegistry[id].mFilePath), materialDef);

    // Material desc is always available
    mMaterialDescs.emplace_back(MaterialDesc{.id=(MaterialID)id, .renderPass=materialDef.renderPass });
    mMaterialGpuData.emplace_back();
}

std::any MaterialRepository::getUserData(AssetID id) {
    return MaterialLoadUserData();
}
