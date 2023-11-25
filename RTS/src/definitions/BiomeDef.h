#pragma once

// For now match .biome files
enum class BIOME_IDS {
    PLAINS = 0,
    MOUNTAINS = 1,
    FOREST = 2,
    HOTSPRINGS = 3,
};

class BiomeDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(BiomeDef);

    ui32 uniqueId = UINT32_MAX;
    f32 priority = 0.0f;
    nString displayName = "UNKNOWN";
};
SERIALIZABLE_SIMPLE(BiomeDef,
    make_field(o.uniqueId, "id"sv),
    make_field(o.priority, "priority"sv),
    make_field(o.displayName, "name"sv)
);