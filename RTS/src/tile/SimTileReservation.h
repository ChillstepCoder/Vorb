#pragma once

#include "tile/ChunkLiteTileHandle.h"
#include "tile/TileHarvestable.h"
#include "tile/SimTileData.h"

class SimChunkTileReservation {
    friend class SimTileReservation;
public:
    ~SimChunkTileReservation();

    VORB_NON_COPYABLE(SimChunkTileReservation);
    POOLED_ALLOC_DECL();

    ChunkLiteTileHandle getLiteTileHandle() const { return mTileHandle; }
    // Lock chunk mutex and get a copy of the current tile data
    [[nodiscard]] SimTileData getCurrentTileDataCopy() const;

    // If this is a harvestable of the expected type still, clear it and return its tile ID
    // Otherwise, fail and return TILE_ID_NONE. In either case, frees reservation.
    // Further calls on this handle are invalid after this call
    [[nodiscard]] TileID tryClearHarvestable(TileHarvestable expectedHarvestable);

    bool isValid() const { return mTileHandle.isValid(); }

private:
    SimChunkTileReservation(ChunkLiteTileHandle tileHandle);

    ChunkLiteTileHandle mTileHandle;
};

typedef std::unique_ptr<SimChunkTileReservation> SimChunkTileReservationHandle;

class SimTileReservation {
    friend class SimChunk;
public:

private:
    // SimChunk use only
    static SimChunkTileReservationHandle tryReserveSimTileForChunk(ChunkLiteTileHandle tileHandle);
    static SimChunkTileReservationHandle tryReserveHarvestableSimTileForChunk(ChunkLiteTileHandle tileHandle, TileHarvestable requiredHarvestable);
};
