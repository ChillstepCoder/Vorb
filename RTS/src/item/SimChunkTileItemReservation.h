#pragma once

class SimChunkItemData;

//struct SimChunkTileItemHandle {
//    ChunkID chunkId = INVALID_CHUNK_ID;
//    ItemID itemID = INVALID_ITEM_ID;
//    TileItemUID itemUID = INVALID_TILE_ITEM_UID;
//};

// Reserve a specific tile item, should be destroyed
// right before aquiring the tile items
class SimChunkTileItemReservation {
    friend class SimChunkItemData;
private:
    SimChunkTileItemReservation(ChunkTileIndex tileIndex, TileItemUID itemUID, ItemID itemID, ui32 count, SimChunk& owner)
        : mTileIndex(tileIndex), mItemUID(itemUID), mItemID(itemID), mCount(count), mOwnerChunk(owner) { }

public:
    ~SimChunkTileItemReservation();

    VORB_NON_COPYABLE(SimChunkTileItemReservation);

    POOLED_ALLOC_DECL();

    ChunkTileIndex getTileIndex() const { return mTileIndex; }
    TileItemUID getItemUID() const { return mItemUID; }
    ItemID getItemID() const { return mItemID; }
    ui32 getReservedCount() const { return mCount; }
    ChunkID getChunkID() const;

    // Tries to return a new reservation split off from this one with splitCount
    SimChunkTileItemReservationPtr trySplit(ui16 splitCount);

private:
    ChunkTileIndex mTileIndex;
    ItemID mItemID;
    TileItemUID mItemUID;
    ui16 mCount;
    SimChunk& mOwnerChunk;
};
typedef std::unique_ptr<SimChunkTileItemReservation> SimChunkTileItemReservationPtr;