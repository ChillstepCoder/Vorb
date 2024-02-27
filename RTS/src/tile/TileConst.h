#pragma once

typedef ui16 TileID;
typedef ui32 TileContainerID;
typedef ui32 TileIndex;
typedef ui32 DTileIndex;
constexpr TileIndex INVALID_TILE_INDEX = UINT32_MAX;
constexpr TileContainerID INVALID_TILE_CONTAINER_ID = UINT32_MAX;

constexpr TileID TILE_ID_NONE = UINT16_MAX;
inline bool isTileNone(TileID tile) { return tile == TILE_ID_NONE; }

constexpr int TILE_LAYER_GROUND = 0;
constexpr int TILE_LAYER_MAIN = 1;
constexpr int TILE_LAYER_COUNT = 2;
