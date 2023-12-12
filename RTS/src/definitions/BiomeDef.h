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

struct NoiseDistribution {
    NoiseFunction func;
    f32v2 range = f32v2(0.0f, 1.0f);
};
SERIALIZABLE_SIMPLE(NoiseDistribution,
    make_field(o.func, "func"),
    make_field(o.range, "range")
);

struct BiomePossibleTile {
    StrToken tileName;
    f32 spawnChance = 0.0f;
    f32 minHeight = 0.0f;
    f32 maxHeight = FLT_MAX;
    NoiseDistribution distribution;
    bool usesDistribution = false;
};

class BiomeDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(BiomeDef);

    BiomeUniqueID uniqueId = BiomeUniqueID::INVALID;
    f32 priority = 0.0f; // ??
    nString displayName = "UNKNOWN";
    SoftAssetReference colorMapTexture = AssetType::Texture;
    // Index in the color map texture array
    ui32 colorMapTextureIndex = 0;
};
SERIALIZABLE_SIMPLE(BiomeDef,
    make_field(o.uniqueId, "id"sv),
    make_field(o.priority, "priority"sv),
    make_field(o.displayName, "name"sv),
    make_field(o.colorMapTexture, "col_map"sv)
);