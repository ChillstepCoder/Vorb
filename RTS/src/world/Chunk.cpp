#include "stdafx.h"
#include "Chunk.h"

#include "rendering/QuadMesh.h"

#include "world/WorldGrid.h"

#include "services/Services.h"
#include "ResourceManager.h"
#include "item/ItemRepository.h"
#include "item/Item.h"

ChunkRenderData::~ChunkRenderData() {
    // Empty
    // TODO: RAII Wrapper for safety
    if (mLODTexture) {
        glDeleteTextures(1, &mLODTexture);
    }
}

Chunk::Chunk() {
}

Chunk::~Chunk() {
	dispose();
}

void Chunk::init(const ChunkID& chunkId, WorldGrid& worldGrid) {
    mWorldGrid = &worldGrid;
	assert(mState == ChunkState::INVALID);
	mChunkId = chunkId;
    mWorldPos = chunkId.getWorldPos();
    mAABB.x = mWorldPos.x;
    mAABB.y = mWorldPos.y;
    mAABB.z = -2.0f;
    mAABB.width = CHUNK_WIDTH;
    mAABB.depth = CHUNK_WIDTH;
    mAABB.height = 4.0f;
}

void Chunk::allocateTiles() {
    // TODO: Not always
    mTiles.resize(CHUNK_SIZE);
}

void Chunk::freeTiles() {
    std::vector<Tile>().swap(mTiles);
}

void Chunk::dispose() {

    if (isDataReady()) {
        Chunk& leftNeighbor = getLeftNeighbor();
        if (leftNeighbor.isDataReady()) {
            --leftNeighbor.mDataReadyNeighborCount;
        }
        Chunk& rightNeighbor = getRightNeighbor();
        if (rightNeighbor.isDataReady()) {
            --rightNeighbor.mDataReadyNeighborCount;
        }
        Chunk& topNeighbor = getTopNeighbor();
        if (topNeighbor.isDataReady()) {
            --topNeighbor.mDataReadyNeighborCount;
        }
        Chunk& bottomNeighbor = getBottomNeighbor();
        if (bottomNeighbor.isDataReady()) {
            --bottomNeighbor.mDataReadyNeighborCount;
        }
    }
    mDataReadyNeighborCount = 0;
	mState = ChunkState::INVALID;
    
    // Reset render data
    mChunkRenderData.mLODDirty = true;
    mChunkRenderData.mMeshDirty = true;
    mChunkRenderData.mHighDetailFloraMeshDirty = true;
}

TileHandle Chunk::getTileHandleAt(const TileIndex index) const {
    return TileHandle(this, index);
}

TileHandle Chunk::getLeftTileHandle(const TileIndex index) const {
    const ui16 x = index.getX();
    if (x > 0) {
        return TileHandle(this, index - 1);
    }
    const Chunk& leftNeighbor = getLeftNeighbor();
    if (leftNeighbor.isDataReady()) {
        return TileHandle(&leftNeighbor, index + CHUNK_WIDTH - 1);
    }
	return TileHandle();
}

TileHandle Chunk::getRightTileHandle(const TileIndex index) const {
    const ui16 x = index.getX();
    if (x < CHUNK_WIDTH - 1) {
        return TileHandle(this, index + 1);
    }

    const Chunk& rightNeighbor = getRightNeighbor();
    if (rightNeighbor.isDataReady()) {
        return TileHandle(&rightNeighbor, index - CHUNK_WIDTH + 1);
    }
    return TileHandle();
}

TileHandle Chunk::getTopTileHandle(const TileIndex index) const {
    const ui16 y = index.getY();
    if (y < CHUNK_WIDTH - 1) {
        return TileHandle(this, index + CHUNK_WIDTH);
    }

    Chunk& topNeighbor = getTopNeighbor();
	if (topNeighbor.isDataReady()) {
        return TileHandle(&topNeighbor, index + CHUNK_WIDTH - CHUNK_SIZE);
	}
    return TileHandle();
}

TileHandle Chunk::getBottomTileHandle(const TileIndex index) const {
    const ui16 y = index.getY();
    if (y > 0) {
        return TileHandle(this, index - CHUNK_WIDTH);
    }

    Chunk& bottomNeighbor = getBottomNeighbor();
    if (bottomNeighbor.isDataReady()) {
        return TileHandle(&bottomNeighbor, index - CHUNK_WIDTH + CHUNK_SIZE);
    }
	return TileHandle();
}

void Chunk::getTileNeighbors(const TileIndex index, OUT Tile neighbors[8]) const {

    // TODO: Branchless interior nodes? :thinkies:

	{ // Bottom 3
		TileHandle bottom = getBottomTileHandle(index);
		if (bottom.isValid()) {
			neighbors[(int)NeighborIndex::BOTTOM] = bottom.tile;
			TileHandle bottomLeft = bottom.chunk->getLeftTileHandle(bottom.index);
            neighbors[(int)NeighborIndex::BOTTOM_LEFT] = bottomLeft.tile;
            TileHandle bottomRight = bottom.chunk->getRightTileHandle(bottom.index);
            neighbors[(int)NeighborIndex::BOTTOM_RIGHT] = bottomRight.tile;
		}
	}

	// Left
    neighbors[(int)NeighborIndex::LEFT] = getLeftTileHandle(index).tile;

    // Right
    neighbors[(int)NeighborIndex::RIGHT] = getRightTileHandle(index).tile;

    { // Top 3
        TileHandle top = getTopTileHandle(index);
        if (top.isValid()) {
            neighbors[(int)NeighborIndex::TOP] = top.tile;
            TileHandle topLeft = top.chunk->getLeftTileHandle(top.index);
            neighbors[(int)NeighborIndex::TOP_LEFT] = topLeft.tile;
            TileHandle topRight = top.chunk->getRightTileHandle(top.index);
            neighbors[(int)NeighborIndex::TOP_RIGHT] = topRight.tile;
        }
    }

}

Chunk& Chunk::getLeftNeighbor() const {
    return mWorldGrid->getChunk(mChunkId.id - 1);
}

Chunk& Chunk::getTopNeighbor() const {
    return mWorldGrid->getChunk(mChunkId.id + WorldData::WORLD_WIDTH_CHUNKS);
}

Chunk& Chunk::getRightNeighbor() const {
    return mWorldGrid->getChunk(mChunkId.id + 1);
}

Chunk& Chunk::getBottomNeighbor() const {
    return mWorldGrid->getChunk(mChunkId.id - WorldData::WORLD_WIDTH_CHUNKS);
}
