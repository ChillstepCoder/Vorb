#pragma once

enum class ChunkState : ui8 {
    DEACTIVATED,
    WAITING_SIM_RELEASE,
    READY_TO_LOAD, // Sim thread sets this once it has released us
    //WAITING_BUILDINGS,
    LOADING_TILES, // Only worker thread can change from LOADING_TILES to TILE_LOAD_FINISHED
    DORMANT, // LOD simulated but with visible LOD tile data
    LOADING_MESH_PHYSICS_NAV_VISIBILITY,
    ACTIVATED, // Fully simulated
    DESTROYING_ON_SIM, // Being destroyed on simulation thread, do nothing until it is finished
    COUNT
};

enum class ChunkFlags : ui8 {
    IN_DESTROY_LIST = 1 << 0,
    IS_ACTIVATING = 1 << 1,
    IN_DORMANT_LIST = 1 << 2,
    IN_ACTIVE_LIST  = 1 << 3,
    IN_EDGE_LIST    = 1 << 4
};