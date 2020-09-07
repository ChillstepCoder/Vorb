#include "stdafx.h"
#include "Chunk.h"

#include "rendering/ChunkMesh.h"


ChunkRenderData::~ChunkRenderData() {
    // Empty
}

Chunk::Chunk() :
	mChunkId(CHUNK_ID_INVALID) {
	memset(mNeighbors, 0, sizeof(mNeighbors));
}

Chunk::~Chunk() {
	dispose();
}

void Chunk::init(ChunkID chunkId) {
	assert(mState == ChunkState::WAITING_FOR_INIT);
	mChunkId = chunkId;
	mState = ChunkState::LOADING;
}

void Chunk::dispose() {
	if (mState == ChunkState::INVALID) {
		return;
	}

	if (mNeighborLeft) {
        mNeighborLeft->mNeighborRight = nullptr;
        if (isDataReady()) {
            --mNeighborLeft->mDataReadyNeighborCount;
        }
	}
	if (mNeighborRight) {
        mNeighborRight->mNeighborLeft = nullptr;
        if (isDataReady()) {
            --mNeighborRight->mDataReadyNeighborCount;
        }
	}
	if (mNeighborTop) {
        mNeighborTop->mNeighborBottom = nullptr;
        if (isDataReady()) {
            --mNeighborTop->mDataReadyNeighborCount;
        }
	}
	if (mNeighborBottom) {
        mNeighborBottom->mNeighborTop = nullptr;
        if (isDataReady()) {
            --mNeighborBottom->mDataReadyNeighborCount;
        }
	}

	mState = ChunkState::INVALID;
}

TileHandle Chunk::getLeftTile(const TileIndex index) const {
    const ui16 x = index.getX();
    if (x > 0) {
        TileHandle handle(this, index - 1);
        handle.tile = mTiles[handle.index];
        return handle;
    }
    else if (mNeighborLeft && mNeighborLeft->isDataReady()) {
        TileHandle handle(mNeighborLeft, index + CHUNK_WIDTH - 1);
        handle.tile = mNeighborLeft->mTiles[handle.index];
        return handle;
    }
	return TileHandle();
}

TileHandle Chunk::getRightTile(const TileIndex index) const {
    const ui16 x = index.getX();
    if (x < CHUNK_WIDTH - 1) {
        TileHandle handle(this, index + 1);
        handle.tile = mTiles[handle.index];
        return handle;
    }
    else if (mNeighborRight && mNeighborRight->isDataReady()) {
        TileHandle handle(mNeighborRight, index - CHUNK_WIDTH + 1);
        handle.tile = mNeighborRight->mTiles[handle.index];
        return handle;
    }
    return TileHandle();
}

TileHandle Chunk::getTopTile(const TileIndex index) const {
    const ui16 y = index.getY();
    if (y < CHUNK_WIDTH - 1) {
        TileHandle handle(this, index + CHUNK_WIDTH);
        handle.tile = mTiles[handle.index];
        return handle;
	}
	else if (mNeighborTop && mNeighborTop->isDataReady()) {
        TileHandle handle(mNeighborTop, index + CHUNK_WIDTH - CHUNK_SIZE);
        handle.tile = mNeighborTop->mTiles[handle.index];
        return handle;
	}
    return TileHandle();
}

TileHandle Chunk::getBottomTile(const TileIndex index) const {
    const ui16 y = index.getY();
    if (y > 0) {
        TileHandle handle(this, index - CHUNK_WIDTH);
        handle.tile = mTiles[handle.index];
        return handle;
    }
    else if (mNeighborBottom && mNeighborBottom->isDataReady()) {
        TileHandle handle(mNeighborBottom, index - CHUNK_WIDTH + CHUNK_SIZE);
        handle.tile = mNeighborBottom->mTiles[handle.index];
        return handle;
    }
	return TileHandle();
}

void Chunk::getTileNeighbors(const TileIndex index, OUT Tile neighbors[8]) const {

    // TODO: Branchless interior nodes? :thinkies:

	{ // Bottom 3
		TileHandle bottom = getBottomTile(index);
		if (bottom.isValid()) {
			neighbors[(int)NeighborIndex::BOTTOM] = bottom.tile;
			TileHandle bottomLeft = bottom.chunk->getLeftTile(bottom.index);
            neighbors[(int)NeighborIndex::BOTTOM_LEFT] = bottomLeft.tile;
            TileHandle bottomRight = bottom.chunk->getRightTile(bottom.index);
            neighbors[(int)NeighborIndex::BOTTOM_RIGHT] = bottomRight.tile;
		}
	}

	// Left
    neighbors[(int)NeighborIndex::LEFT] = getLeftTile(index).tile;

    // Right
    neighbors[(int)NeighborIndex::RIGHT] = getRightTile(index).tile;

    { // Top 3
        TileHandle top = getTopTile(index);
        if (top.isValid()) {
            neighbors[(int)NeighborIndex::TOP] = top.tile;
            TileHandle topLeft = top.chunk->getLeftTile(top.index);
            neighbors[(int)NeighborIndex::TOP_LEFT] = topLeft.tile;
            TileHandle topRight = top.chunk->getRightTile(top.index);
            neighbors[(int)NeighborIndex::TOP_RIGHT] = topRight.tile;
        }
    }

}

ChunkID::ChunkID(const f32v2 worldPos) {
	pos = i32v2(floor(worldPos.x / CHUNK_WIDTH), floor(worldPos.y / CHUNK_WIDTH));
}
