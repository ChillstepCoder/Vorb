#pragma once



typedef ui16 SubchunkIndex;
constexpr ui32 MAX_TILE_CONTAINER_WIDTH = CHUNK_WIDTH;
constexpr ui32 MAX_TILE_CONTAINER_HEIGHT = UINT16_MAX / SUBCHUNKS_PER_CHUNK_ROW;
static_assert(sizeof(SubchunkIndex) == sizeof(ui16));
