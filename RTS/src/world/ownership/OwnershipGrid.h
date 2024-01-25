#pragma once

#include "util/SpatialGrid2D.h"

struct OwnershipData {
    entt::entity owner;
    f32 propertyValue;
};
static_assert(sizeof(OwnershipData) == 8, "Keep tiny");

// Keeps track of which entities own which blocks and chunks
// Block is 8x8, chunk is 128x128
class OwnershipGrid
{
public:
    OwnershipGrid(ui32 worldWidthTiles);
    ~OwnershipGrid();

    VORB_NON_COPYABLE(OwnershipGrid);

    ui32 getTotalVertices() const { return mTotalVertices; }
    ui32 getWidthVertices() const { return mSpatialGrid.getGridWidthCells(); }

    OwnershipData getChunkSettlementOwnerData(ChunkID chunkId) const;
    OwnershipData getWorldPosEntityOwnerData(f32v2 worldPos) const;

    void setChunkSettlementOwnerData(ChunkID chunkId, OwnershipData ownerData);
    void setWorldPosEntityOwnerData(f32v2 worldPos, OwnershipData ownerData);

private:
    // Probably tie it to each body

    std::unique_ptr<OwnershipData[]> mBlockOwners; //8x8 blocks tiling across the world
    std::unique_ptr<OwnershipData[]> mChunkOwners;
    ui32 mTotalVertices = 0;
    ui32 mWidthBlocks = 0;
    ui32 mWidthChunks = 0;
    SpatialGrid2D mSpatialGrid;
};

