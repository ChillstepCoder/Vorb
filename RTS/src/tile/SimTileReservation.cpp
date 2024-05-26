#include "stdafx.h"
#include "SimTileReservation.h"

#include "world/World.h"
#include "world/chunk/SimChunk.h"

POOLED_ALLOC_DEF_THREADSAFE(SimChunkTileReservation, 512);

SimChunkTileReservation::~SimChunkTileReservation() {
    assert(sGameWorld);
    SimChunk& simChunk = mTileHandle.getSimChunk(*sGameWorld);
    simChunk.freeTileReservation(mTileHandle.index);
}

SimChunkTileReservation::SimChunkTileReservation(ChunkLiteTileHandle tileHandle) : mTileHandle(tileHandle) {

}

SimChunkTileReservationHandle SimTileReservation::tryReserveSimTileForChunk(ChunkLiteTileHandle tileHandle) {
    assert(sGameWorld);
    SimChunk& simChunk = tileHandle.getSimChunk(*sGameWorld);
    if (simChunk.tryReserveNonEmptyTile(tileHandle.index)) {
        return std::unique_ptr<SimChunkTileReservation>(new SimChunkTileReservation(tileHandle));
    }
    return nullptr;
}

SimChunkTileReservationHandle SimTileReservation::tryReserveHarvestableSimTileForChunk(ChunkLiteTileHandle tileHandle, TileHarvestable requiredHarvestable) {
    assert(sGameWorld);
    SimChunk& simChunk = tileHandle.getSimChunk(*sGameWorld);
    if (simChunk.tryReserveHarvestableTile(tileHandle.index, requiredHarvestable)) {
        return std::unique_ptr<SimChunkTileReservation>(new SimChunkTileReservation(tileHandle));
    }
    return nullptr;
}
