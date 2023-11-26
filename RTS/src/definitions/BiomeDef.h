#pragma once

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

class BiomeDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(BiomeDef);

    BiomeUniqueID uniqueId = BiomeUniqueID::INVALID;
    f32 priority = 0.0f;
    nString displayName = "UNKNOWN";
};
SERIALIZABLE_SIMPLE(BiomeDef,
    make_field(o.uniqueId, "id"sv),
    make_field(o.priority, "priority"sv),
    make_field(o.displayName, "name"sv)
);