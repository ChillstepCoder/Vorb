#pragma once

#include "util/SpatialGrid2D.h"
#include "util/BitArray.h"

#include <boost/container/flat_map.hpp>

class WorldMarkupGrid;

enum class DTileOwnerObjectType : ui8 {
    None,
    Plot,
    RoadEdge,
    Structure,
    COUNT
};

enum class DTileOwnershipFlags : ui8 {
    OwnedBySettlement = BIT(0), // True if owner is of entity type settlement
};
struct DTileOwnershipData {
    entt::entity owner = entt::null;
    ui16 ownerObjectId = UINT16_MAX;
    DTileOwnerObjectType ownerObjectType = DTileOwnerObjectType::None;
    BitFlags<DTileOwnershipFlags> flags = {};
};
static_assert(sizeof(DTileOwnershipData) == 8, "Keep small");

struct ChunkOwnershipData {
    entt::entity owner = entt::null;
    //ui32 padding; // TODO: Use?
    std::unique_ptr<DTileOwnershipData[]> dtileData = nullptr;
};
static_assert(sizeof(ChunkOwnershipData) == 16, "Keep small");

// Keeps track of which entities own which DTiles and chunks
// DTile is 2x2, chunk is 128x128
class OwnershipGrid
{
public:
    OwnershipGrid(ui32 worldWidthTiles, WorldMarkupGrid& markupGrid);
    ~OwnershipGrid();

    VORB_NON_COPYABLE(OwnershipGrid);

    void setChunkOwner(ChunkID chunkId, entt::entity owner);
    entt::entity getChunkOwner(ChunkID chunkId) const;

    ui32 getTotalDTiles() const { return mTotalDTiles; }
    ui32 getWidthDTiles() const { return mWidthDTiles; }

    const ChunkOwnershipData& getChunkSettlementOwnerData(ChunkID chunkId) const;
    const DTileOwnershipData* tryGetDTileOwnerData(DTileCoord dtilePosWorld) const;
    const DTileOwnershipData* tryGetDTileOwnerData(ChunkID chunkId, DTileIndex tileIndex) const;
    bool isDTileOwned(DTileCoord dtilePosWorld) const;

    bool isChunkOwnedBySettlement(ChunkID chunkId) const;

    void setChunkSettlementOwner(ChunkID chunkId, entt::entity owner);
    void setDTileOwner(i32v2 dtilePosWorld, entt::entity owner, DTileOwnerObjectType type, ui16 ownerObjectId, bool isSettlementOwned);

    bool isChunkIsClaimed(ChunkID chunkId) const;
    void claimChunk(ChunkID chunkId);
    void unclaimChunk(ChunkID chunkId);

    // TODO
    //STATIC_EVENT_LISTENER_FUNCS(OwnershipGrid, Destroy, ItemStockpileEventType::Destroy, const ItemStockpileEvent&);
    //STATIC_EVENT_DISPATCHER_DEF(OwnershipGrid);
private:
    bool allocateTileDataIfNeeded(ChunkOwnershipData& data);

    WorldMarkupGrid& mMarkupGrid;

    std::unique_ptr<ChunkOwnershipData[]> mChunkOwners;
    BitArray mClaimedChunks; // Chunks that someone is planning to immigrate to 
    ui32 mTotalDTiles = 0;
    ui32 mWidthDTiles = 0;
    ui32 mWidthChunks = 0;
};

