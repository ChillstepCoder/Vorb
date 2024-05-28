#include "stdafx.h"
#include "BiomeRepository.h"

#include "resources/ResourceManager.h"
#include "resources/TextureRepository.h"

#include "definitions/TileDef.h"
#include "definitions/TileDistributionDef.h"
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

    linkCorruptedBiomes();

    // TODO: Load custom mapping file so we can persist biome IDs for mods?

    generateBiomesGLSLFile();
}

void BiomeRepository::linkCorruptedBiomes() {
    // Link corruptions automatically
    const auto& enumNameMap = getGlobalEnumNameMap<BiomeUniqueID>();
    for (size_t uniqueId = 0; uniqueId < mUniqueIDMap.size(); ++uniqueId) {
        const AssetID assetId = mUniqueIDMap[uniqueId];
        BiomeDef& def = *mAssets[assetId];
        static_assert(e_count(BiomeCorruptions) == 2);
        if (def.isCorruptable) {
            std::string_view baseName = enumNameMap.at(BiomeUniqueID(uniqueId));
            if (uniqueId + e_count(BiomeCorruptions) >= mUniqueIDMap.size()) {
                panic("Bounds overflow in biome map with corruptable biomes when evaluating {}", baseName);
            }
            // Make sure we have both children in the enum
            std::string_view banshiraName = enumNameMap.at(BiomeUniqueID(uniqueId + 1));
            std::string_view chernobogName = enumNameMap.at(BiomeUniqueID(uniqueId + 2));
            if (std::memcmp(baseName.data(), banshiraName.data(), baseName.size()) != 0 || banshiraName.back() != 'B') {
                panic("Biome {} corruptable but next biome {} does not match required name {}_B", baseName, banshiraName, baseName);
            }
            if (std::memcmp(baseName.data(), chernobogName.data(), baseName.size()) != 0 || chernobogName.back() != 'C') {
                panic("Biome {} corruptable but next biome {} does not match required name {}_C", baseName, chernobogName, baseName);
            }
            BiomeDef& banshira = *mAssets[mUniqueIDMap[uniqueId + 1]];
            BiomeDef& chernobog = *mAssets[mUniqueIDMap[uniqueId + 2]];
            def.corruptVersions[e_cast(BiomeCorruptions::Banshira)] = mAssets[mUniqueIDMap[uniqueId + 1]].get();
            def.corruptVersions[e_cast(BiomeCorruptions::Chernobog)] = mAssets[mUniqueIDMap[uniqueId + 2]].get();
            for (int i = 0; i < e_count(BiomeCorruptions); ++i) {
                BiomeDef& child = *def.corruptVersions[i];
                assert(!child.parentBiomeRef.isValid() || child.parentBiomeRef.getAssetID() == def.getID());
                child.parentBiomeRef.setAssetID(def.getID());
                child.parentBiome = &def;
                child.corruptType = BiomeCorruptions(i);
            }
        }
    }
}

// TODO: Apparently these constants may consume register memory which may actually negatively impact performance! We may
// instead want to put them in a UBO
void BiomeRepository::generateBiomesGLSLFile() {
    nString fileData = "//This file is generated at runtime by code, do not edit it (TODO: Turn this into a UBO For better perf)\n";
    if (mAssetRegistry.size() != e_count(BiomeUniqueID)) {
        panic("Number of biome data files is {} which does not match the code enum count of {}", mAssetRegistry.size(), e_count(BiomeUniqueID));
    }

    // Write biome const ints
    const auto& enumNameMap = getGlobalEnumNameMap<BiomeUniqueID>();
    for (size_t uniqueId = 0; uniqueId < mUniqueIDMap.size(); ++uniqueId) {
        fileData += "const int BIOME_" + nString(enumNameMap.at(BiomeUniqueID(uniqueId))) + " = " + std::to_string(uniqueId) + ";\n";
    }

    // Write biome spreadable
    fileData += "\nconst uint BIOME_CORRUPTION[" + std::to_string(mAssetRegistry.size()) + "] = {\n";
    for (size_t uniqueId = 0; uniqueId < mUniqueIDMap.size(); ++uniqueId) {
        const AssetID assetId = mUniqueIDMap[uniqueId];
        const BiomeDef& def = *mAssets[assetId];
        fileData += "    " + nString(def.corruptType == BiomeCorruptions::COUNT ? "0" : std::to_string((int)def.corruptType + 1)) + nString(", // ") + nString(enumNameMap.at(BiomeUniqueID(uniqueId))) + "\n";
    }
    fileData += "};\n";

    // Write biome transforms
    static_assert(e_count(BiomeCorruptions) == 2 && "Ensure this handles new corruption");
    fileData += "\nconst uvec3 BIOME_TRANSFORM[" + std::to_string(mAssetRegistry.size()) + "] = {\n";
    for (size_t uniqueId = 0; uniqueId < mUniqueIDMap.size(); ++uniqueId) {
        const AssetID assetId = mUniqueIDMap[uniqueId];
        const BiomeDef& def = *mAssets[assetId];
        const nString idStr = std::to_string(e_cast(def.uniqueId));
        if (def.isCorruptable) {
            const nString banshiraIdStr = std::to_string(e_cast(def.corruptVersions[e_cast(BiomeCorruptions::Banshira)]->uniqueId));
            const nString chernobogIdStr = std::to_string(e_cast(def.corruptVersions[e_cast(BiomeCorruptions::Chernobog)]->uniqueId));
            fileData += "    uvec3(" + idStr + "," + banshiraIdStr + "," + chernobogIdStr + "), // " + nString(enumNameMap.at(BiomeUniqueID(uniqueId))) + "\n";
        }
        else {
            fileData += "    uvec3(" + idStr + "," + idStr + "," + idStr + "), // " + nString(enumNameMap.at(BiomeUniqueID(uniqueId))) + "\n";
        }
    }
    fileData += "};\n";

    ////  Write biome overridable
    //fileData += "\nconst bool BIOME_OVERRIDABLE[" + std::to_string(mAssetRegistry.size()) + "] = {\n";
    //for (size_t uniqueId = 0; uniqueId < mUniqueIDMap.size(); ++uniqueId) {
    //    const AssetID assetId = mUniqueIDMap[uniqueId];
    //    const BiomeDef& def = *mAssets[assetId];
    //    fileData += "    " + nString(def.isCorruptable ? "true" : "false") + nString(", // ") + nString(enumNameMap.at(BiomeUniqueID(uniqueId))) + "\n";
    //}
    //fileData += "};\n";

    // Write biome colors
    fileData += "\nconst vec3 BIOME_COLORS[" + std::to_string(mAssetRegistry.size()) + "] = {\n";
    for (size_t uniqueId = 0; uniqueId < mUniqueIDMap.size(); ++uniqueId) {
        const AssetID assetId = mUniqueIDMap[uniqueId];
        const BiomeDef& def = *mAssets[assetId];
        const f32v3 debugColorf(def.debugColor.r / 255.0f, def.debugColor.g / 255.0f, def.debugColor.b / 255.0f);
        fileData += "    vec3(" + std::to_string(debugColorf.r) + ", " + std::to_string(debugColorf.g) + ", " + std::to_string(debugColorf.b) + "), // " + nString(enumNameMap.at(BiomeUniqueID(uniqueId))) + "\n";
    }
    fileData += "};\n";

    vio::Path filePath = ResourceManager::get().getResourceRoot() / vio::Path("shaders/const/biome_ids.glsl");
    if (!mIoManager.resolvePath(filePath, filePath)) {
        panic("Could not resolve {}", filePath.getCString());
    }

    std::ofstream outStream(filePath.getStdPath());

    if (!outStream) {
        panic("Error opening {}", filePath.getCString());
    }

    outStream << fileData;
}

void BiomeRepository::fixupRegisteredAsset(AssetID id) {
    BiomeDef& def = *mAssets[id];

    // Construct cache + lookup friendly data for fast generation passes
    def.tileGenerationData.resize(def.tileGenCategories.size());
    for (size_t categoryIndex = 0; categoryIndex < def.tileGenCategories.size(); ++categoryIndex) {
        BiomeTileGenCategory& category = def.tileGenCategories[categoryIndex];
        OptimizedBiomeTileGenCategoryData& genData = def.tileGenerationData[categoryIndex];
        if (category.distribution.isValid()) {
            genData.distributionPtr = &ResourceManager::getAssetHandle<TileDistributionDef>(category.distribution.getAssetID())->getLoadedAsset();
        }
        else {
            genData.distributionPtr = nullptr;
        }
        genData.minHeight = category.minHeight;
        genData.maxHeight = category.maxHeight;
        // Invert for generation since we use normal
        genData.slopeRange = f32v2(1.0f) - category.slopeRange;
        genData.probabilityMult = category.probabilityMult;
        genData.tiles.resize(category.tiles.size());
        genData.allVariants.resize(0);
        f32 totalWeight = 0.0f;
        for (auto& tile : category.tiles) {
            totalWeight += tile.weight;
        }
        if (totalWeight == 0.0f) totalWeight = 0.01f; // Prevent / 0
        f32 cumulative = 0.0f;
        for (size_t i = 0; i < category.tiles.size(); ++i) {
            BiomePossibleTile& tile = category.tiles[i];
            const f32 weight = tile.weight / totalWeight;
            PossibleTileGeneration& tileGen = genData.tiles[i];
            tileGen.weightThreshold = cumulative + weight;
            if (tile.tile.isValid()) {
                tileGen.tileId = tile.tile.getAssetID();
                if (tileGen.tileId == INVALID_ASSET_ID) {
                    tileGen.tileId = TILE_ID_NONE;
                    LOG_ERROR("Tile {} in biome {} in category {} has invalid tile reference assigned", i, def.getName().toString(), category.name);
                }
            }
            else {
                tileGen.tileId = TILE_ID_NONE;
                LOG_WARN("Tile {} in biome {} in category {} has no tile assigned", i, def.getName().toString(), category.name);
            }

            assert(cumulative + weight <= 1.001f); // Epsilon
            cumulative += weight;

            // Variants
            if (tileGen.tileId != TILE_ID_NONE) {
                const TileDef& tileDef = ResourceManager::getAssetHandle<TileDef>(tileGen.tileId)->getLoadedAsset();
                tileGen.variantCount = tileDef.modelVariants.empty() ? 0 : tile.variants.size();
                tileGen.variantSelectionType = tile.variantSelectionType;
                if (tileGen.variantCount) {
                    tileGen.variantStartIndex = genData.allVariants.size();
                    genData.allVariants.resize(genData.allVariants.size() + tileGen.variantCount);
                    f32 totalVarWeight = 0.0f;
                    f32 cumulativeVar = 0.0f;
                    for (size_t j = tileGen.variantStartIndex; j < genData.allVariants.size(); ++j) {
                        BiomePossibleVariant& tileVariant = tile.variants[j - tileGen.variantStartIndex];
                        totalVarWeight += tileVariant.weight;
                        VariantWithWeightThreshold& variant = genData.allVariants[j];
                        if (tileVariant.tileVariantIndex >= tileDef.modelVariants.size()) {
                            LOG_ERROR("Tile variant index {} in biome {} in category {} for tile {} is out of bounds ({})", tileVariant.tileVariantIndex, def.getName().toString(), category.name, tileDef.getName().toString(), tileDef.modelVariants.size());
                            tileVariant.tileVariantIndex = tileDef.modelVariants.size() - 1;
                        }
                        variant.tileVariant = tileVariant.tileVariantIndex;
                    }
                    if (totalVarWeight == 0.0f) totalVarWeight = 0.01f; // Prevent / 0
                    for (size_t j = tileGen.variantStartIndex; j < genData.allVariants.size(); ++j) {
                        VariantWithWeightThreshold& variant = genData.allVariants[j];
                        const f32 varWeight = tile.weight / totalVarWeight;
                        variant.weightThreshold = cumulativeVar + varWeight;
                        assert(cumulativeVar + varWeight <= 1.001f); // Epsilon
                        cumulativeVar += varWeight;
                    }
                }
            }
            else {
                tileGen.variantCount = 0;
                tileGen.variantStartIndex = 0;
                tileGen.variantSelectionType = TileVariantSelectionType::Random;
            }
        }
        genData.allVariants.shrink_to_fit();
    }
}
