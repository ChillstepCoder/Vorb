#pragma once

enum class ChunkState : ui8 {
    INVALID,
    WAITING_HEIGHT,
    LOADING_TILES, // Only worker thread can change from LOADING_TILES to TILE_LOAD_FINISHED
    TILE_LOAD_FINISHED,
    WAITING_MESH_AND_PHYSICS, // TODO: This doesnt make sense on dedicated server
    FINISHED,
};

enum class ChunkFlags : ui8 {
    IN_DESTROY_LIST = 1 << 0,
};