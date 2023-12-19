#include "stdafx.h"
#include "BiomeRepository.h"

#include "resources/TextureRepository.h"
#include "util/GlobalEnumNameMap.h"

#include <vorb/io/IOManager.h>

constexpr int COLOR_MAP_DIM_X = 128;
constexpr int COLOR_MAP_DIM_Y = 256;

AssetLoadFunc BiomeRepository::getAssetLoadFunc() {
    return[&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {
        assert(false);
        BiomeDef& def = *static_cast<BiomeDef*>(assetDataPtr);
        return true;
    };
}

void BiomeRepository::onRegisteredAsset(AssetID id) {
    assert(id < UINT8_MAX);

    BiomeDef& def = *mAssets[id];
    { // Assigne unique ID
        const auto& enumNameMap = getGlobalEnumNameMap<BiomeUniqueID>();
        const nString uniqueName = mAssetRegistry[id].mFilePath.getFileNameNoExtension();
        for (auto&& it : enumNameMap) {
            if (it.second == uniqueName) {
                def.uniqueId = it.first;
                break;
            }
        }
        if (def.uniqueId == BiomeUniqueID::INVALID) {
            panic("Biome {} file name does not match any code enum name", mAssetRegistry[id].mFilePath.getString());
        }
    }

    YmlSerializer::readFileData(readFileToString(mAssetRegistry[id].mFilePath), def);
    if (e_cast(def.uniqueId) == UINT32_MAX) {
        panic("Biome {} has no id", mAssetRegistry[id].mFilePath.getString());
    }
    if (e_cast(def.uniqueId) >= mUniqueIDMap.size()) {
        mUniqueIDMap.resize(e_cast(def.uniqueId) + 1, UINT32_MAX);
    }
    if (mUniqueIDMap[e_cast(def.uniqueId)] != UINT32_MAX) {
        panic("Biome {} has duplicate id {}", mAssetRegistry[id].mFilePath.getString(), e_cast(def.uniqueId));
    }
    mUniqueIDMap[e_cast(def.uniqueId)] = id;
    mLoadedAssets[id]->store(true);
}

void BiomeRepository::onAllAssetTypesRegistered() {
    ASSERT_RENDER_THREAD();

    std::vector<AssetID> colorMapTextureIDs;
    std::vector<ui32> ssboData;

    // Get all unique texture layers
    colorMapTextureIDs.reserve(mAssetRegistry.size());
    ssboData.resize(mAssetRegistry.size());
    for (auto& asset : mAssetRegistry) {
        BiomeDef& def = *mAssets[asset.getId()];
        AssetID textureId = def.colorMapTexture.getAssetID();
        const auto& it = std::find(colorMapTextureIDs.begin(), colorMapTextureIDs.end(), textureId);
        if (it != colorMapTextureIDs.end()) {
            def.colorMapTextureIndex = *it;
        }
        else {
            def.colorMapTextureIndex = colorMapTextureIDs.size();
            colorMapTextureIDs.emplace_back(textureId);
        }
        assert(e_cast(def.uniqueId) < ssboData.size());
        ssboData[e_cast(def.uniqueId)] = def.colorMapTextureIndex;
    }

    // Build array texture
    assert(!mBiomeColorMapsArrayTexture);
    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &mBiomeColorMapsArrayTexture);
    glTextureStorage3D(mBiomeColorMapsArrayTexture, 1, GL_RGB8, COLOR_MAP_DIM_X, COLOR_MAP_DIM_Y, colorMapTextureIDs.size());

    for (int i = 0; i < colorMapTextureIDs.size(); ++i) {

        gli::texture2d layerData = TextureRepository::get().loadRawPngData(colorMapTextureIDs[i], false);
        assert(layerData.extent().x == COLOR_MAP_DIM_X);
        assert(layerData.extent().y == COLOR_MAP_DIM_Y);
        assert(layerData.format() == gli::format::FORMAT_RGB8_UNORM_PACK8);

        glTextureSubImage3D(mBiomeColorMapsArrayTexture,
            0,
            0,
            0,
            i,
            COLOR_MAP_DIM_X,
            COLOR_MAP_DIM_Y,
            1,
            GL_RGB,
            GL_UNSIGNED_BYTE,
            layerData.data()
        );
    }

    vg::sSamplerStates.LINEAR_CLAMP.setForTexture(mBiomeColorMapsArrayTexture);

    // Build shader data buffer
    assert(!mBiomeColorMapsShaderLookupBuffer);
    glCreateBuffers(1, &mBiomeColorMapsShaderLookupBuffer);
    glNamedBufferStorage(mBiomeColorMapsShaderLookupBuffer, sizeof(ui32) * ssboData.size(), ssboData.data(), 0);

    // TODO: Load custom mapping file so we can persist biome IDs for mods?

    generateBiomesGLSLFile();
}

void BiomeRepository::generateBiomesGLSLFile() {
    nString fileData = "//This file is generated at runtime by code, do not edit it\n";
    if (mAssetRegistry.size() != e_count(BiomeUniqueID)) {
        panic("Number of biome data files is {} which does not match the code enum count of {}", mAssetRegistry.size(), e_count(BiomeUniqueID));
    }

    // Write biome const ints
    const auto& enumNameMap = getGlobalEnumNameMap<BiomeUniqueID>();
    for (size_t uniqueId = 0; uniqueId < mUniqueIDMap.size(); ++uniqueId) {
        fileData += "const int BIOME_" + nString(enumNameMap.at(BiomeUniqueID(uniqueId))) + " = " + std::to_string(uniqueId) + ";\n";
    }

    // Write biome spreadable
    fileData += "\nconst bool BIOME_SPREADABLE[" + std::to_string(mAssetRegistry.size()) + "] = {\n";
    for (size_t uniqueId = 0; uniqueId < mUniqueIDMap.size(); ++uniqueId) {
        const AssetID assetId = mUniqueIDMap[uniqueId];
        const BiomeDef& def = *mAssets[assetId];
        fileData += "    " + def.isCorruption ? "true" : "false" + nString(", // ") + nString(enumNameMap.at(BiomeUniqueID(uniqueId))) + "\n";
    }
    fileData += "};\n";

    //  Write biome overridable
    fileData += "\nconst bool BIOME_OVERRIDABLE[" + std::to_string(mAssetRegistry.size()) + "] = {\n";
    for (size_t uniqueId = 0; uniqueId < mUniqueIDMap.size(); ++uniqueId) {
        const AssetID assetId = mUniqueIDMap[uniqueId];
        const BiomeDef& def = *mAssets[assetId];
        fileData += "    " + def.canBeSpreadTo ? "true" : "false" + nString(", // ") + nString(enumNameMap.at(BiomeUniqueID(uniqueId))) + "\n";
    }
    fileData += "};\n";

    // Write biome colors
    fileData += "\nconst vec3 BIOME_COLORS[" + std::to_string(mAssetRegistry.size()) + "] = {\n";
    for (size_t uniqueId = 0; uniqueId < mUniqueIDMap.size(); ++uniqueId) {
        const AssetID assetId = mUniqueIDMap[uniqueId];
        const BiomeDef& def = *mAssets[assetId];
        const f32v3 debugColorf(def.debugColor.r / 255.0f, def.debugColor.g / 255.0f, def.debugColor.b / 255.0f);
        fileData += "    vec3(" + std::to_string(debugColorf.r) + ", " + std::to_string(debugColorf.g) + ", " + std::to_string(debugColorf.b) + "), // " + nString(enumNameMap.at(BiomeUniqueID(uniqueId))) + "\n";
    }
    fileData += "};\n";

    if (!mIoManager.writeStringToFile(vio::Path("data/shaders/biome_ids.glsl"), fileData)) {
        panic("Could not write biome_ids.glsl file");
    }
}
