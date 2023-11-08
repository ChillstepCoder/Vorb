#pragma once

enum class ChunkState : ui8 {
    INVALID,
    WAITING_HEIGHT,
    LOADING_TILES, // Only worker thread can change from LOADING_TILES to TILE_LOAD_FINISHED
    LOADING_MESH_PHYSICS_NAV_VISIBILITY,
    READY,
};

enum class ChunkFlags : ui8 {
    IN_DESTROY_LIST = 1 << 0,
    IN_LOAD_LIST    = 1 << 1,
    IN_ACTIVE_LIST  = 1 << 2,
    IN_EDGE_LIST    = 1 << 3
};