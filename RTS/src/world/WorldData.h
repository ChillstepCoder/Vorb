#pragma once

// Global static world-specific data
namespace WorldData {

    constexpr ui32 REGION_WIDTH_CHUNKS = 16;
    constexpr ui32 REGION_SIZE_CHUNKS = SQ(REGION_WIDTH_CHUNKS);

    constexpr ui32 WORLD_WIDTH_REGIONS = 16;
    constexpr ui32 WORLD_SIZE_REGIONS = SQ(WORLD_WIDTH_REGIONS);

    constexpr ui32 WORLD_WIDTH_CHUNKS = WORLD_WIDTH_REGIONS * REGION_WIDTH_CHUNKS;
    constexpr ui32 WORLD_SIZE_CHUNKS = SQ(WORLD_WIDTH_CHUNKS);

    constexpr ui32 WORLD_WIDTH_TILES = WORLD_WIDTH_CHUNKS * CHUNK_WIDTH;
    constexpr ui32 WORLD_SIZE_TILES = SQ(WORLD_WIDTH_TILES);

    constexpr ui32 REGION_WIDTH_TILES = REGION_WIDTH_CHUNKS * CHUNK_WIDTH;
    constexpr ui32 HALF_REGION_WIDTH_TILES = REGION_WIDTH_TILES / 2;
    static const f32 REGION_DIAGONAL_RADIUS = sqrtf(SQ(HALF_REGION_WIDTH_TILES) + SQ(HALF_REGION_WIDTH_TILES));

    const f32v2 WORLD_CENTER((WORLD_WIDTH_CHUNKS* CHUNK_WIDTH) / 2.0f);
}



static_assert(WorldData::WORLD_WIDTH_TILES < UINT16_MAX - 1, "World assumes that xy coordinates can fit inside a ui16v2, going higher also results in integer overflow in WORLD_SIZE_TILES");