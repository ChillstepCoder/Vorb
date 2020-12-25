#pragma once

// Global static world-specific data
namespace WorldData {
    // Uncomment to switch to map mode
    constexpr ui32 WORLD_CHUNKS_PADDING = 1;
    constexpr ui32 WORLD_CHUNKS_WIDTH = 100 + WORLD_CHUNKS_PADDING * 2;
    constexpr ui32 WORLD_CHUNKS_SIZE = WORLD_CHUNKS_WIDTH * WORLD_CHUNKS_WIDTH;

    const f32v2 WORLD_CENTER((WORLD_CHUNKS_WIDTH* CHUNK_WIDTH) / 2.0f);
}