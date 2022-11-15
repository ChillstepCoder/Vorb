#pragma once

typedef ui16 TileID;
typedef ui32 TileContainerID;
typedef ui32 TileIndex;
constexpr TileIndex INVALID_TILE_INDEX = UINT32_MAX;
constexpr TileContainerID INVALID_TILE_CONTAINER_ID = UINT32_MAX;

constexpr TileID TILE_ID_NONE = UINT16_MAX;
constexpr TileID TILE_ID_BLOCKED_BY_NORTH_EAST = TILE_ID_NONE - 1;
constexpr TileID TILE_ID_BLOCKED_BY_NORTH      = TILE_ID_NONE - 2;
constexpr TileID TILE_ID_BLOCKED_BY_NORTH_WEST = TILE_ID_NONE - 3;
constexpr TileID TILE_ID_BLOCKED_BY_EAST       = TILE_ID_NONE - 4;
constexpr TileID TILE_ID_BLOCKED_BY_WEST       = TILE_ID_NONE - 5;
constexpr TileID TILE_ID_BLOCKED_BY_SOUTH_EAST = TILE_ID_NONE - 6;
constexpr TileID TILE_ID_BLOCKED_BY_SOUTH      = TILE_ID_NONE - 7;
constexpr TileID TILE_ID_BLOCKED_BY_SOUTH_WEST = TILE_ID_NONE - 8;
constexpr TileID TILE_ID_BLOCKED               = TILE_ID_NONE - 9;

inline bool isTileBlocked(TileID tile) { return tile >= TILE_ID_BLOCKED && tile != TILE_ID_NONE; }
inline bool isTileBlockedOrNone(TileID tile) { return tile >= TILE_ID_BLOCKED; }

constexpr int TILE_LAYER_GROUND = 0;
constexpr int TILE_LAYER_MID = 1;
constexpr int TILE_LAYER_TOP = 2;
constexpr int TILE_LAYER_COUNT = 3;
