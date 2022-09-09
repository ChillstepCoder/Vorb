#pragma once

typedef ui16 TileID;
typedef ui32 TileContainerID;
typedef ui32 TileIndex;
constexpr TileIndex INVALID_TILE_INDEX = UINT32_MAX;
constexpr TileContainerID INVALID_TILE_CONTAINER_ID = UINT32_MAX;
constexpr ui16 TILE_ID_NONE = UINT16_MAX;
constexpr int TILE_LAYER_GROUND = 0;
constexpr int TILE_LAYER_MID = 1;
constexpr int TILE_LAYER_TOP = 2;
constexpr int TILE_LAYER_COUNT = 3;
