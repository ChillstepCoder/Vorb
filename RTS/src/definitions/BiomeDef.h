#pragma once

#include "generation/NoiseFunction.hpp"

#include "world/biome/BiomeCorruptions.h"
#include "definitions/TileDistributionDef.h"

// .biome file names should match exactly
// Corrupt biomes should be _B and _C and always come
// directly after the base biome in alphabetical order
enum class BiomeUniqueID : ui8 {
    Ocean = 0,
    Plains = 1,
    Plains_B = 2,
    Plains_C = 3,
    Mountains = 4,
    Mountains_B = 5,
    Mountains_C = 6,
    Forest = 7,
    Forest_B = 8,
    Forest_C = 9,
    Hotsprings = 10,
    Hotsprings_B = 11,
    Hotsprings_C = 12,
    COUNT,
    INVALID = UINT8_MAX
};
SERIALIZABLE_ENUM_SAME_NAME(BiomeUniqueID,
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Ocean),
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Plains),
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Plains_B),
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Plains_C),
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Mountains),
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Mountains_B),
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Mountains_C),
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Forest),
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Forest_B),
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Forest_C),
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Hotsprings),
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Hotsprings_B),
    ENUM_FIELD_SIMPLE(BiomeUniqueID, Hotsprings_C),
);
static_assert(e_count(BiomeUniqueID) == 13);

constexpr ui8 INVALID_BIOME_ID = e_cast(BiomeUniqueID::INVALID);

struct BiomePossibleTile {
    SoftAssetReference tile = AssetType::Tile;
    f32 weight = 0.0f;
};
SERIALIZABLE_SIMPLE(BiomePossibleTile,
    make_field(o.tile, "tile"sv),
    make_field(o.weight, "weight"sv)
);



// TODO: Have a generic DataAssetRepository for things like Distributions and such.
// Pure data assets dont need a complicated loader and can be always loaded

struct BiomeTileGenCategory {
    std::vector<BiomePossibleTile> tiles;
    SoftAssetReference distribution = AssetType::TileDistribution;
    f32 minHeight = 0.0f;
    f32 maxHeight = FLT_MAX;
    f32 spacing = 2.0f; // 0 means eval every tile
    // Probability of spawning a tile in this category if passing density + spacing check
    f32 probability = 1.0f;
};
SERIALIZABLE_IMGUI_CONTROLLED(BiomeTileGenCategory,
    make_field(o.tiles, "tiles"sv),
    make_field(o.distribution, "dist"sv),
    make_field(o.minHeight, "min_h"sv),
    make_field(o.maxHeight, "max_h"sv),
    make_field(o.spacing, "spacing"sv),
    make_field(o.probability, "prob"sv)
);

class BiomeDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(BiomeDef, AssetType::Biome);

    // Assigned based on name
    BiomeUniqueID uniqueId = BiomeUniqueID::INVALID;
    f32 priority = 0.0f; // ??
    nString displayName = "UNKNOWN";
    SoftAssetReference colorMapTexture = AssetType::Texture;
    // If a sub biome, this will be set by file. If corrupt, this will be set automatically
    SoftAssetReference parentBiomeRef = AssetType::Biome;
    BiomeDef* parentBiome = nullptr;
    BiomeDef* corruptVersions[e_count(BiomeCorruptions)] = {};
    BiomeCorruptions corruptType = BiomeCorruptions::COUNT;
    // Index in the color map texture array
    ui32 colorMapTextureIndex = 0;
    bool isCorruptable = false;
    color3 debugColor = color3(255, 255, 255);
    // Tile spawning
    std::vector<BiomeTileGenCategory> tileGenCategories;
};
SERIALIZABLE_IMGUI_CONTROLLED(BiomeDef,
    make_field(o.priority, "priority"sv),
    make_field(o.displayName, "name"sv),
    make_field(o.colorMapTexture, "col_map"sv),
    make_field(o.parentBiomeRef, "parent"sv),
    make_field(o.isCorruptable, "corruptable"sv),
    make_field(o.debugColor, "dbg_col"sv),
    make_field(o.tileGenCategories, "categories"sv)
);