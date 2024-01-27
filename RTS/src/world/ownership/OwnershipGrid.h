#pragma once

#include "util/SpatialGrid2D.h"
#include "util/BitArray.h"

#include <boost/container/flat_map.hpp>

class WorldMarkupGrid;

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
    OwnershipGrid(ui32 worldWidthTiles, WorldMarkupGrid& markupGrid);
    ~OwnershipGrid();

    VORB_NON_COPYABLE(OwnershipGrid);

    void setChunkOwner(ChunkID chunkId, entt::entity owner);
    entt::entity getChunkOwner(ChunkID chunkId) const;

    ui32 getTotalVertices() const { return mTotalVertices; }
    ui32 getWidthVertices() const { return mSpatialGrid.getGridWidthCells(); }

    OwnershipData getChunkSettlementOwnerData(ChunkID chunkId) const;
    OwnershipData getWorldPosEntityOwnerData(f32v2 worldPos) const;

    void setChunkSettlementOwnerData(ChunkID chunkId, OwnershipData ownerData);
    void setWorldPosEntityOwnerData(f32v2 worldPos, OwnershipData ownerData);

    bool isChunkIsClaimed(ChunkID chunkId) const;
    void claimChunk(ChunkID chunkId);
    void unclaimChunk(ChunkID chunkId);

    // TODO
    //STATIC_EVENT_LISTENER_FUNCS(OwnershipGrid, Destroy, ItemStockpileEventType::Destroy, const ItemStockpileEvent&);
    //STATIC_EVENT_DISPATCHER_DEF(OwnershipGrid);
private:
    WorldMarkupGrid& mMarkupGrid;

    std::unique_ptr<OwnershipData[]> mBlockOwners; //8x8 blocks tiling across the world
    std::unique_ptr<OwnershipData[]> mChunkOwners;
    BitArray mClaimedChunks; // Chunks that someone is planning to immigrate to 
    ui32 mTotalVertices = 0;
    ui32 mWidthBlocks = 0;
    ui32 mWidthChunks = 0;
    SpatialGrid2D mSpatialGrid;
};

