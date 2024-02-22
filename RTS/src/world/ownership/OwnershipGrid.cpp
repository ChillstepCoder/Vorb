#include "stdafx.h"
#include "OwnershipGrid.h"

#include "world/markup/WorldMarkupGrid.h"

OwnershipGrid::OwnershipGrid(ui32 worldWidthTiles, WorldMarkupGrid& markupGrid) : mMarkupGrid(markupGrid){
    mWidthBlocks = worldWidthTiles / BLOCK_WIDTH;
    mWidthChunks = mWidthBlocks / CHUNK_SIZE;
    mSpatialGrid.init(BLOCK_WIDTH, mWidthBlocks);
    mTotalVertices = SQ(mWidthBlocks);
    mBlockOwners = std::make_unique<OwnershipData[]>(mTotalVertices);
    mChunkOwners = std::make_unique<OwnershipData[]>(SQ(mWidthChunks));
    LOG_DEBUG("Ownership grid allocated {} mb data",
        ((mTotalVertices + SQ(mWidthChunks)) * sizeof(OwnershipData)) / 1024.f / 1024.f);

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

OwnershipData OwnershipGrid::getChunkSettlementOwnerData(ChunkID chunkId) const {
    ASSERT_SIM_THREAD();
    return mChunkOwners[chunkId];
}

OwnershipData OwnershipGrid::getWorldPosEntityOwnerData(f32v2 worldPos) const {
    worldPos /= BLOCK_WIDTH;
    i32v2 blockPos = i32v2(worldPos);
    
    if (blockPos.x < 0 || blockPos.y < 0 || blockPos.x >= mWidthBlocks || blockPos.y >= mWidthBlocks) [[unlikely]] {
        return OwnershipData();
    }
    return mBlockOwners[blockPos.y * mWidthBlocks + blockPos.x];
}

bool OwnershipGrid::isChunkOwnedBySettlement(ChunkID chunkId) const {
    return mChunkOwners[chunkId].owner != entt::null;
}

void OwnershipGrid::setChunkSettlementOwnerData(ChunkID chunkId, OwnershipData ownerData) {
    mChunkOwners[chunkId] = ownerData;
    mClaimedChunks.setBit(chunkId);
}

void OwnershipGrid::setWorldPosEntityOwnerData(f32v2 worldPos, OwnershipData ownerData) {
    worldPos /= BLOCK_WIDTH;
    i32v2 blockPos = i32v2(worldPos);
    assert(blockPos.x >= 0 && blockPos.y >= 0 && blockPos.x < mWidthBlocks && blockPos.y < mWidthBlocks);
    mBlockOwners[blockPos.y * mWidthBlocks + blockPos.x] = ownerData;
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
