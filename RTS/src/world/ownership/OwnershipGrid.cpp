#include "stdafx.h"
#include "OwnershipGrid.h"

#include "world/markup/WorldMarkupGrid.h"

OwnershipGrid::OwnershipGrid(ui32 worldWidthTiles, WorldMarkupGrid& markupGrid) : mMarkupGrid(markupGrid){
    mWidthDTiles = worldWidthTiles / DTILE_WIDTH;
    mWidthChunks = worldWidthTiles / CHUNK_WIDTH;
    mTotalDTiles = SQ(mWidthDTiles);
    mChunkOwners = std::make_unique<ChunkOwnershipData[]>(SQ(mWidthChunks));
    mClaimedChunks.resizeAndZero(SQ(mWidthChunks));
}

OwnershipGrid::~OwnershipGrid() = default;

void OwnershipGrid::setChunkOwner(ChunkID chunkId, entt::entity owner) {
    ASSERT_SIM_THREAD();
    mChunkOwners[chunkId].owner = owner;
}

entt::entity OwnershipGrid::getChunkOwner(ChunkID chunkId) const {
    ASSERT_SIM_THREAD();
    return mChunkOwners[chunkId].owner;
}

const ChunkOwnershipData& OwnershipGrid::getChunkSettlementOwnerData(ChunkID chunkId) const {
    ASSERT_SIM_THREAD();
    return mChunkOwners[chunkId];
}

const DTileOwnershipData* OwnershipGrid::tryGetDTileOwnerData(DTileCoord dtilePosWorld) const {
    if (dtilePosWorld.x < 0 || dtilePosWorld.y < 0 || dtilePosWorld.x >= mWidthDTiles || dtilePosWorld.y >= mWidthDTiles) [[unlikely]] {
        return nullptr;
    }
    const ChunkID chunkId = (dtilePosWorld.y / CHUNK_WIDTH_DTILES) * mWidthChunks + (dtilePosWorld.x / CHUNK_WIDTH_DTILES);

    ChunkOwnershipData& data = mChunkOwners[chunkId];
    if (!data.dtileData) {
        return nullptr;
    }
    return &data.dtileData[(dtilePosWorld.y % CHUNK_WIDTH_DTILES) * CHUNK_WIDTH_DTILES + dtilePosWorld.x % CHUNK_WIDTH_DTILES];
}

const DTileOwnershipData* OwnershipGrid::tryGetDTileOwnerData(ChunkID chunkId, DTileIndex dtileIndex) const {
    ChunkOwnershipData& data = mChunkOwners[chunkId];
    if (!data.dtileData) {
        return nullptr;
    }
    return &data.dtileData[dtileIndex];
}

bool OwnershipGrid::isDTileOwned(DTileCoord dtilePosWorld) const {
    const DTileOwnershipData* data = tryGetDTileOwnerData(dtilePosWorld);
    if (!data) {
        return false;
    }
    return data->owner != entt::null;
}

bool OwnershipGrid::isChunkOwnedByAnySettlement(ChunkID chunkId) const {
    return mChunkOwners[chunkId].owner != entt::null;
}

bool OwnershipGrid::isChunkOwnedBySettlement(ChunkID chunkId, entt::entity settlementId) const {
    return mChunkOwners[chunkId].owner == settlementId;
}

entt::entity OwnershipGrid::getChunkSettlementOwner(ChunkID chunkId) const {
    return mChunkOwners[chunkId].owner;
}

void OwnershipGrid::setChunkSettlementOwner(ChunkID chunkId, entt::entity owner) {
    ChunkOwnershipData& data = mChunkOwners[chunkId];
    data.owner = owner;
    allocateTileDataIfNeeded(data);
    mClaimedChunks.setBit(chunkId);
}

void OwnershipGrid::setDTileOwner(i32v2 dtilePosWorld, entt::entity owner, DTileOwnerObjectType type, ui16 ownerObjectId, bool isSettlementOwned) {
    if (dtilePosWorld.x < 0 || dtilePosWorld.y < 0 || dtilePosWorld.x >= mWidthDTiles || dtilePosWorld.y >= mWidthDTiles) [[unlikely]] {
        LOG_CRITICAL("Tried to set dtile owner at world pos {} {} OUT OF BOUNDS", dtilePosWorld.x, dtilePosWorld.y);
        return;
    }
    const ChunkID chunkId = (dtilePosWorld.y / CHUNK_WIDTH_DTILES) * mWidthChunks + (dtilePosWorld.x / CHUNK_WIDTH_DTILES);

    ChunkOwnershipData& data = mChunkOwners[chunkId];
    allocateTileDataIfNeeded(data);
    DTileOwnershipData& tileData = data.dtileData[(dtilePosWorld.y % CHUNK_WIDTH_DTILES) * CHUNK_WIDTH_DTILES + dtilePosWorld.x % CHUNK_WIDTH_DTILES];
    tileData.owner = owner;
    tileData.ownerObjectId = ownerObjectId;
    tileData.ownerObjectType = type;
    if (isSettlementOwned) {
        tileData.flags.setBit(DTileOwnershipFlags::OwnedBySettlement);
    }
    else {
        tileData.flags.clearBit(DTileOwnershipFlags::OwnedBySettlement);
    }
}

bool OwnershipGrid::isChunkIsClaimed(ChunkID chunkId) const {
    ASSERT_SIM_THREAD();
    return mClaimedChunks.getBit(chunkId);
}

void OwnershipGrid::claimChunk(ChunkID chunkId) {
    ASSERT_SIM_THREAD();
    assert(!mClaimedChunks.getBit(chunkId));
    mClaimedChunks.setBit(chunkId);
}

void OwnershipGrid::unclaimChunk(ChunkID chunkId) {
    ASSERT_SIM_THREAD();
    assert(mClaimedChunks.getBit(chunkId));
    mClaimedChunks.clearBit(chunkId);
}

bool OwnershipGrid::allocateTileDataIfNeeded(ChunkOwnershipData& data) {
    if (!data.dtileData) {
        data.dtileData = std::make_unique<DTileOwnershipData[]>(CHUNK_SIZE);
        return true;
    }
    return false;
}
