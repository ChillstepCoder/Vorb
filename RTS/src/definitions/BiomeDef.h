#pragma once

#include "world/biome/LivingBiomeType.h"
#include "definitions/TileDistributionDef.h"

using LivingBiomeID = ui16;
constexpr LivingBiomeID INVALID_LIVING_BIOME_ID = std::numeric_limits<LivingBiomeID>::max();

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

enum class TileVariantSelectionType : ui8 {
    Random,
    Voronoi,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(TileVariantSelectionType,
    ENUM_FIELD_SIMPLE(TileVariantSelectionType, Random),
    ENUM_FIELD_SIMPLE(TileVariantSelectionType, Voronoi)
);

struct BiomePossibleVariant {
    ui32 tileVariantIndex = 0;
    f32 weight = 1.0f;
};
SERIALIZABLE_IMGUI_CONTROLLED(BiomePossibleVariant,
    make_field(o.tileVariantIndex, "var"sv),
    make_field(o.weight, "weight"sv)
);

struct BiomePossibleTile {
    TileAssetRef tile;
    f32 weight = 1.0f;
    TileVariantSelectionType variantSelectionType = TileVariantSelectionType::Random;
    std::vector<BiomePossibleVariant> variants;
    
};
SERIALIZABLE_IMGUI_CONTROLLED(BiomePossibleTile,
    make_field(o.tile, "tile"sv),
    make_field(o.weight, "weight"sv),
    make_field(o.variantSelectionType, "var_sel"sv),
    make_field(o.variants, "var_ind"sv)
);

// TODO: Have a generic DataAssetRepository for things like Distributions and such.
// Pure data assets dont need a complicated loader and can be always loaded

struct BiomeTileGenCategory {
    nString name = "NewCategory";
    std::vector<BiomePossibleTile> tiles;
    TileDistributionAssetRef distribution;
    f32 minHeight = 0.1f;
    f32 maxHeight = 10000.0f;
    f32v2 slopeRange = f32v2(0.0f, 1.0f); // 0 = flat, 1 = vertical
    f32 probabilityMult = 1.0f;
};
SERIALIZABLE_IMGUI_CONTROLLED(BiomeTileGenCategory,
    make_field(o.name, "name"sv),
    make_field(o.tiles, "tiles"sv),
    make_field(o.distribution, "dist"sv),
    make_field(o.minHeight, "min_h"sv),
    make_field(o.maxHeight, "max_h"sv),
    make_field(o.slopeRange, "slope_r"sv),
    make_field(o.probabilityMult, "prob"sv)
);

struct PossibleTileGeneration {
    TileID tileId;
    TileVariantSelectionType variantSelectionType;
    ui8 variantCount;
    f32 weightThreshold;
    ui32 variantStartIndex;
};
static_assert(sizeof(PossibleTileGeneration) == 12, "Keep small");

struct VariantWithWeightThreshold {
    ui8 tileVariant;
    f32 weightThreshold;
};
static_assert(sizeof(VariantWithWeightThreshold) == 8, "Keep small");

// Cache + lookup friendly structure created by BiomeRepository::fixupAsset
struct OptimizedBiomeTileGenCategoryData {
    std::vector<PossibleTileGeneration> tiles;
    std::vector<VariantWithWeightThreshold> allVariants; // Cache friendly list
    const TileDistributionDef* distributionPtr = nullptr;
    f32 minHeight;
    f32 maxHeight;
    f32v2 slopeRange;
    f32 probabilityMult;
};

class BiomeDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(BiomeDef, AssetType::Biome);

    // Assigned based on name
    nString displayName = "UNKNOWN";
    BiomeUniqueID uniqueId = BiomeUniqueID::INVALID;
    f32 priority = 0.0f; // ??
    TextureAssetRef colorMapTexture;
    // If a sub biome, this will be set by file. If corrupt, this will be set automatically
    BiomeAssetRef parentBiomeRef;
    BiomeDef* parentBiome = nullptr;
    BiomeDef* mutatedVersions[e_count(LivingBiomeType)] = {};
    LivingBiomeType corruptType = LivingBiomeType::COUNT;
    // Index in the color map texture array
    ui32 colorMapTextureIndex = 0;
    bool isCorruptable = false;
    color3 debugColor = color3(255, 255, 255);
    // Tile spawning
    std::vector<BiomeTileGenCategory> tileGenCategories;
    // Efficient generation data, created by BiomeRepository::fixupAsset
    std::vector<OptimizedBiomeTileGenCategoryData> tileGenerationData;
};
SERIALIZABLE_IMGUI_CONTROLLED(BiomeDef,
    make_field(o.displayName, "name"sv),
    make_field(o.priority, "priority"sv),
    make_field(o.colorMapTexture, "col_map"sv),
    make_field(o.parentBiomeRef, "parent"sv),
    make_field(o.isCorruptable, "corruptable"sv),
    make_field(o.debugColor, "dbg_col"sv),
    make_field(o.tileGenCategories, "categories"sv)
);