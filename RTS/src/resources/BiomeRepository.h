#pragma once

#include "resources/IAssetRepository.h"

#include <gli/texture2d.hpp>
#include "definitions/BiomeDef.h"


class BiomeRepository : public IAssetRepository<BiomeDef> {
public:
    ASSET_REPOSITORY_COMMON_CODE(BiomeRepository, BiomeDef, AssetType::Biome)

    bool saveAsset(AssetID assetId) override { panic("Cannot save biomes yet"); }

    StrToken getAssetExtension() const override { return CStrToken("biome"); }
    const char* const getAssetTypeDisplayName() const override { return "Biome"; }

    const BiomeDef& getBiomeFromUniqueID(ui32 uniqueId) { return getLoadedOrUnloadedAsset(mUniqueIDMap[uniqueId]); }
    VGTexture getBiomeColorMapsArrayTexture() const { return mBiomeColorMapsArrayTexture; }
    VGBuffer getBiomeColorMapsShaderLookupBuffer() const { return mBiomeColorMapsShaderLookupBuffer; }
protected:
    AssetLoadFunc getAssetLoadFunc() override;
    void onRegisteredAsset(AssetID id) override;
    void onAllAssetTypesRegistered() override;
    void generateBiomesGLSLFile();

    // Allow us to persist biome data consistently by
    // assigning a static ID per biome
    // Key = uniqueID, Value = AssetID
    std::vector<AssetID> mUniqueIDMap;
    VGTexture mBiomeColorMapsArrayTexture = 0;
    VGBuffer mBiomeColorMapsShaderLookupBuffer = 0;
};

