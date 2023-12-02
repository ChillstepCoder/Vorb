#include "stdafx.h"
#include "BiomeRepository.h"

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

    std::map<AssetID, ui32> colorMapTextureIDs;

    // Get all texture layers
    for (auto& asset : mAssetRegistry) {
        BiomeDef& def = *mAssets[asset.getId()];
        AssetID textureId = def.colorMapTexture.getAssetID();
        const auto& it = colorMapTextureIDs.find(textureId);
        if (it != colorMapTextureIDs.end()) {
            def.colorMapTextureIndex = it->second;
        }
        else {
            def.colorMapTextureIndex = colorMapTextureIDs.size();
            colorMapTextureIDs.emplace(textureId, colorMapTextureIDs.size());
        }
    }

    // Build array texture
    assert(!mBiomeColorMapsArrayTexture);
    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &mBiomeColorMapsArrayTexture);
    glTextureStorage3D(mBiomeColorMapsArrayTexture, 1, GL_RGB8, 256, 256, colorMapTextureIDs.size());

    for (auto& [assetId, layerIndex] : colorMapTextureIDs) {
        glTextureImage3DEXT(mBiomeColorMapsArrayTexture, GL_TEXTURE_2D_ARRAY, 0, GL_RGB8, 256, 256, layerIndex, 0, GL_RGBA, GL_UNSIGNED_BYTE, mAssets[assetId]->colorMapTexture.getTexture().getData());
    }

    vg::sSamplerStates.LINEAR_CLAMP.setForTexture(mBiomeColorMapsArrayTexture);

    // Build shader data buffer
    assert(!mBiomeColorMapsShaderLookupBuffer);
    glCreateBuffers(1, &mBiomeColorMapsShaderLookupBuffer);

    // TODO: Load mapping file so we can persist biome IDs for mods?
}
