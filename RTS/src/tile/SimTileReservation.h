#pragma once

#include "tile/ChunkLiteTileHandle.h"

class SimChunkTileReservation {
    friend class SimTileReservation;
public:
    ~SimChunkTileReservation();

    VORB_NON_COPYABLE(SimChunkTileReservation);
    POOLED_ALLOC_DECL();

    ChunkLiteTileHandle getLiteTileHandle() const { return mTileHandle; }

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
};
