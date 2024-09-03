#pragma once

enum class ChunkState : ui8 {
    DESTROYING_ON_SIM, // Being destroyed on simulation thread, do nothing until it is finished
    DEACTIVATED,
    WAITING_SIM_RELEASE,
    READY_TO_LOAD, // Sim thread sets this once it has released us
    //WAITING_BUILDINGS,
    LOADING_TILES, // Only worker thread can change from LOADING_TILES to TILE_LOAD_FINISHED
    WAITING_BUILDINGS, // Waiting for all dependant buildings to be loaded
    CAN_GENERATE_NAV, // All states after this point can generate navmesh
    LOADING_MESH_PHYSICS_NAV = CAN_GENERATE_NAV,
    ACTIVATED, // Fully simulated
    COUNT
};

enum class ChunkFlags : ui8 {
    IN_DESTROY_LIST = 1 << 0,
    IS_ACTIVATING = 1 << 1,
    IN_ACTIVE_LIST  = 1 << 2,
    IN_EDGE_LIST    = 1 << 3
};