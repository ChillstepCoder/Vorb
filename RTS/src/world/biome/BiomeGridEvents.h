#pragma once


class BiomeGrid;

class BiomeGridEvent {
public:
    BlockCoord blockPos;
    LivingBiomeType livingBiomeType;
    BiomeUniqueID biomeUniqueId;
};

enum class BIOME_GRID_EVENT_TYPE {
    OnCorruption,
};
EVENT_DISPATCHER_TYPE(BiomeGrid, BIOME_GRID_EVENT_TYPE, BiomeGridEvent&);
