#pragma once

typedef ui8 TileGrassID;
constexpr TileGrassID INVALID_TILE_GRASS_ID = UINT8_MAX;
constexpr ui32 MAX_TILE_GRASS_IDS = 0xff;
constexpr int MAX_GRASS_TYPES_PER_TILE = 3;
constexpr ui8 MAX_GRASS_DENSITY = UINT8_MAX;
constexpr int MAX_GRASS_DETAIL = 12;

struct TileGrass {
    TileGrassID grassIDs[MAX_GRASS_TYPES_PER_TILE] = { INVALID_TILE_GRASS_ID, INVALID_TILE_GRASS_ID, INVALID_TILE_GRASS_ID };
    ui8 densities[MAX_GRASS_TYPES_PER_TILE] = { 0, 0, 0 };

    ui8 getDensity(TileGrassID id) const {
        for (int i = 0; i < MAX_GRASS_TYPES_PER_TILE; ++i) {
            if (grassIDs[i] == id) return densities[i];
        }
        return 0;
    }
};