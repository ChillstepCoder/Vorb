#pragma once

// Global static world-specific data
namespace WorldDefaults {

    constexpr ui32 DEFAULT_WORLD_WIDTH_TILES = CHUNK_WIDTH * 256;
    constexpr ui32 DEFAULT_EDITOR_WORLD_WIDTH_TILES = DEFAULT_WORLD_WIDTH_TILES / 8;
    constexpr ui32 DEFAULT_WORLD_SIZE_TILES = SQ(DEFAULT_WORLD_WIDTH_TILES);

    constexpr ui32 DEFAULT_WORLD_WIDTH_CHUNKS = DEFAULT_WORLD_WIDTH_TILES / CHUNK_WIDTH;
    constexpr ui32 DEFAULT_WORLD_SIZE_CHUNKS = DEFAULT_WORLD_WIDTH_CHUNKS * DEFAULT_WORLD_WIDTH_CHUNKS;

    const i32v2 WORLD_ORIGIN(0);
}

constexpr ui32 MAX_WORLD_WIDTH_TILES = UINT16_MAX - 1;
static_assert(WorldDefaults::DEFAULT_WORLD_WIDTH_TILES < MAX_WORLD_WIDTH_TILES, "World assumes that xy coordinates can fit inside a ui16v2, going higher also results in integer overflow in WORLD_SIZE_TILES");