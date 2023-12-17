#pragma once

#include "generation/NoiseFunction.hpp"

// For now match .biome files
enum class BiomeUniqueID : ui8 {
    Plains = 0,
    Mountains = 1,
    Forest = 2,
    Hotsprings = 3,
    COUNT,
    INVALID = UINT8_MAX
};
SERIALIZABLE_ENUM_SAME_NAME(BiomeUniqueID,
    pair{ BiomeUniqueID::Plains, "Plains"sv },
    pair{ BiomeUniqueID::Mountains, "Mountains"sv },
    pair{ BiomeUniqueID::Forest, "Forest"sv },
    pair{ BiomeUniqueID::Hotsprings, "Hotsprings"sv }
);
static_assert(e_count(BiomeUniqueID) == 4);

const ui8 INVALID_BIOME_ID = UINT8_MAX;

struct BiomePossibleTile {
    SoftAssetReference tile = AssetType::Tile;
    f32 weight = 0.0f;
};
SERIALIZABLE_SIMPLE(BiomePossibleTile,
    make_field(o.tile, "tile"sv),
    make_field(o.weight, "weight"sv)
);

// Optional noise function
YML_WRITE_DEF_PTR(NoiseFunction);
YML_READ_DEF_PTR(NoiseFunction);

struct BiomeTileGenCategory {
    std::vector<BiomePossibleTile> tiles;
    std::unique_ptr<NoiseFunction> densityFunction;
    f32 minHeight = 0.0f;
    f32 maxHeight = FLT_MAX;
    f32 spacing = 2.0f; // If 0 means eval every tile
    // Probability of spawning a tile in this category if passing density + spacing check
    f32 probability = 1.0f;
};
SERIALIZABLE_SIMPLE(BiomeTileGenCategory,
    make_field(o.tiles, "tiles"sv),
    make_field(o.densityFunction, "dens_f"sv),
    make_field(o.minHeight, "min_h"sv),
    make_field(o.maxHeight, "max_h"sv),
    make_field(o.spacing, "spacing"sv),
    make_field(o.probability, "prob"sv)
);

class BiomeDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(BiomeDef);

    BiomeUniqueID uniqueId = BiomeUniqueID::INVALID;
    f32 priority = 0.0f; // ??
    nString displayName = "UNKNOWN";
    SoftAssetReference colorMapTexture = AssetType::Texture;
    // Index in the color map texture array
    ui32 colorMapTextureIndex = 0;
    // Tile spawning
    std::vector<BiomeTileGenCategory> tileGenCategories;
};
SERIALIZABLE_SIMPLE(BiomeDef,
    make_field(o.uniqueId, "id"sv),
    make_field(o.priority, "priority"sv),
    make_field(o.displayName, "name"sv),
    make_field(o.colorMapTexture, "col_map"sv),
    make_field(o.tileGenCategories, "categories"sv)
);