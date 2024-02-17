#include "stdafx.h"
#include "SimChunkTileGrid.h"

SimChunkTileGrid::SimChunkTileGrid(ui32 worldWidthTiles) {
    mWidthChunks = worldWidthTiles / CHUNK_WIDTH;
    initInternal();
}

SimChunkTileGrid::~SimChunkTileGrid() {

}

ui32 SimChunkTileGrid::getApproxMemoryUsageBytes() const {
   return mTotalChunks * sizeof(SimChunkTileContainer) + mTotalSimulatingChunks * (sizeof(SimChunkTileData));
}

void SimChunkTileGrid::initInternal() {
    mSpatialGrid.init(CHUNK_WIDTH, mWidthChunks);
    mTotalChunks = SQ(mSpatialGrid.getGridWidthCells());
    mChunkData = std::make_unique<SimChunkTileContainer[]>(mTotalChunks);
    LOG_DEBUG("Sim chunk grid allocated {} mb data",
        (mTotalChunks * sizeof(SimChunkTileContainer)) / 1024.f / 1024.f);
    for (ChunkID id = 0; id < mTotalChunks; ++id) {
        mChunkData[id].chunkId = id;
    }
}

bool SimChunkTileContainer::allocate() {
    std::lock_guard lock(mutex);
    if (state != SimChunkTileContainerState::Allocated) {
        state = SimChunkTileContainerState::Allocated;
        data = std::make_unique<SimChunkTileData>();
        return true;
    }
    return false;
}
