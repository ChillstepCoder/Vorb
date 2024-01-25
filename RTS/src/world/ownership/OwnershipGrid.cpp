#include "stdafx.h"
#include "OwnershipGrid.h"

OwnershipGrid::OwnershipGrid(ui32 worldWidthTiles) {
    mWidthBlocks = worldWidthTiles / BLOCK_WIDTH;
    mWidthChunks = mWidthBlocks / CHUNK_SIZE;
    mSpatialGrid.init(BLOCK_WIDTH, mWidthBlocks);
    mTotalVertices = SQ(mWidthBlocks);
    mBlockOwners = std::make_unique<OwnershipData[]>(mTotalVertices);
    mChunkOwners = std::make_unique<OwnershipData[]>(SQ(mWidthChunks));
    LOG_DEBUG("Ownership grid allocated {} mb data",
        ((mTotalVertices + SQ(mWidthChunks)) * sizeof(OwnershipData)) / 1024.f / 1024.f);
}

OwnershipGrid::~OwnershipGrid() = default;

OwnershipData OwnershipGrid::getChunkSettlementOwnerData(ChunkID chunkId) const {
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

void OwnershipGrid::setChunkSettlementOwnerData(ChunkID chunkId, OwnershipData ownerData) {
    mChunkOwners[chunkId] = ownerData;
}

void OwnershipGrid::setWorldPosEntityOwnerData(f32v2 worldPos, OwnershipData ownerData) {
    worldPos /= BLOCK_WIDTH;
    i32v2 blockPos = i32v2(worldPos);
    assert(blockPos.x >= 0 && blockPos.y >= 0 && blockPos.x < mWidthBlocks && blockPos.y < mWidthBlocks);
    mBlockOwners[blockPos.y * mWidthBlocks + blockPos.x] = ownerData;
}
