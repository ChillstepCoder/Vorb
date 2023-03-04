#pragma once

// Comment out for larger chunks
// #define USE_SMALL_CHUNK_WIDTH
#ifdef USE_SMALL_CHUNK_WIDTH
constexpr int CHUNK_WIDTH = 64;
static_assert(CHUNK_WIDTH == 64, "Adjust bitwise operators below");
constexpr float CHUNK_DIAGONAL_RADIUS = 45.255f;
#define TILE_INDEX_Y_SHIFT 6
#define TILE_INDEX_X_MASK 0x3f
#else
constexpr int CHUNK_WIDTH = 128;
static_assert(CHUNK_WIDTH == 128, "Adjust bitwise operators below");
constexpr float CHUNK_DIAGONAL_RADIUS = 90.51f;
#define TILE_INDEX_Y_SHIFT 7
#define TILE_INDEX_X_MASK 0x7f
#endif

constexpr int SUBCHUNK_WIDTH = 16;
constexpr int SUBCHUNK_WIDTH_SQ = SQ(SUBCHUNK_WIDTH);
constexpr int SUBCHUNKS_PER_CHUNK_ROW = CHUNK_WIDTH / SUBCHUNK_WIDTH;
constexpr int SUBCHUNKS_PER_CHUNK = SQ(SUBCHUNKS_PER_CHUNK_ROW);
/*
constexpr int MIN_SUBCHUNKS_PER_CHUNK_ROW = CHUNK_WIDTH / SUBCHUNK_WIDTH;
constexpr int MIN_SUBCHUNKS_PER_CHUNK = SQ(MIN_SUBCHUNKS_PER_CHUNK_ROW);*/

constexpr int HALF_CHUNK_WIDTH = CHUNK_WIDTH / 2;
constexpr int CHUNK_SIZE = CHUNK_WIDTH * CHUNK_WIDTH;

typedef ui16 SubchunkIndex;
constexpr ui32 MAX_TILE_CONTAINER_WIDTH = CHUNK_WIDTH;
constexpr ui32 MAX_TILE_CONTAINER_HEIGHT = UINT16_MAX / SUBCHUNKS_PER_CHUNK_ROW;
static_assert(sizeof(SubchunkIndex) == sizeof(ui16));