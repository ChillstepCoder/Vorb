#pragma once

class SimChunkItemData;

class SimChunk;
class IFullECS;

//struct SimChunkTileItemHandle {
//    ChunkID chunkId = INVALID_CHUNK_ID;
//    ItemID itemID = INVALID_ITEM_ID;
//    TileItemUID itemUID = INVALID_TILE_ITEM_UID;
//};

// Reserve a specific tile item, non binding reservation and may end up
// reserving items that get removed from something else
class SimChunkTileItemReservation {
    friend class SimChunkItemData;
public:
    SimChunkTileItemReservation(ChunkTileIndex tileIndex, TileItemUID itemUID, ItemID itemID, ui32 count, SimChunk& owner)
        : mTileIndex(tileIndex), mItemUID(itemUID), mItemID(itemID), mReservedCount(count), mOwnerChunk(owner) { }
    ~SimChunkTileItemReservation();

    VORB_NON_COPYABLE(SimChunkTileItemReservation);

    POOLED_ALLOC_DECL();

    // Try to instantly retrieve items from the reservation. Returns number of items picked up. 
    // Caller must create resulting items itself
    [[nodiscard]] i32 tryPickupSimThread(i32 maxCount);
    [[nodiscard]] i32 tryPickupGameThread(i32 maxCount, IFullECS& ecs);

    bool isValid() const { return mReservedCount > 0; }
    ChunkTileIndex getTileIndex() const { return mTileIndex; }
    TileItemUID getItemUID() const { return mItemUID; }
    ItemID getItemID() const { return mItemID; }
    ui32 getReservedCount() const { return mReservedCount; }
    ChunkID getChunkID() const;
    // Return number of items picked up


    // Tries to return a new reservation split off from this one with splitCount
    std::unique_ptr<SimChunkTileItemReservation> trySplit(ui16 splitCount);

private:
    ChunkTileIndex mTileIndex;
    ItemID mItemID;
    TileItemUID mItemUID;
    ui16 mReservedCount;
    SimChunk& mOwnerChunk;
};
typedef std::unique_ptr<SimChunkTileItemReservation> SimChunkTileItemReservationPtr;