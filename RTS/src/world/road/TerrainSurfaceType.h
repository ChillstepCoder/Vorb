#pragma once

// TODO: Data drive this?
enum class TerrainSurfaceType : ui8 {
    None,
    Dirt,
    FarmPlot,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(TerrainSurfaceType,
    ENUM_FIELD_SIMPLE(TerrainSurfaceType, None),
    ENUM_FIELD_SIMPLE(TerrainSurfaceType, Dirt),
    ENUM_FIELD_SIMPLE(TerrainSurfaceType, FarmPlot)
)
static_assert(e_count(TerrainSurfaceType) == 3);

enum class TerrainSurfaceOverlayType : ui8 {
    None,
    Seeds,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(TerrainSurfaceOverlayType,
    ENUM_FIELD_SIMPLE(TerrainSurfaceOverlayType, None),
    ENUM_FIELD_SIMPLE(TerrainSurfaceOverlayType, Seeds)
)
static_assert(e_count(TerrainSurfaceOverlayType) == 2);