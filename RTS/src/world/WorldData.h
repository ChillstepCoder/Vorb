#pragma once

// Global static world-specific data
namespace WorldData {
    constexpr ui32 REGION_WIDTH_CHUNKS = 16;
    constexpr ui32 REGION_SIZE_CHUNKS = SQ(REGION_WIDTH_CHUNKS);

    constexpr ui32 WORLD_WIDTH_REGIONS = 8;
    constexpr ui32 WORLD_SIZE_REGIONS = SQ(WORLD_WIDTH_REGIONS);

    constexpr ui32 WORLD_WIDTH_CHUNKS = WORLD_WIDTH_REGIONS * REGION_WIDTH_CHUNKS;
    constexpr ui32 WORLD_SIZE_CHUNKS = SQ(WORLD_WIDTH_CHUNKS);

    const f32v2 WORLD_CENTER((WORLD_WIDTH_CHUNKS* CHUNK_WIDTH) / 2.0f);
}