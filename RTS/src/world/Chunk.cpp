#include "stdafx.h"
#include "Chunk.h"


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
		mNeighborLeft = nullptr;
	}
	if (mNeighborRight) {
		mNeighborRight->mNeighborLeft = nullptr;
		mNeighborRight = nullptr;
	}
	if (mNeighborTop) {
		mNeighborTop->mNeighborBottom = nullptr;
		mNeighborTop = nullptr;
	}
	if (mNeighborBottom) {
		mNeighborBottom->mNeighborTop = nullptr;
		mNeighborBottom = nullptr;
	}

	mState = ChunkState::INVALID;
}

Tile* Chunk::getLeftTile(TileIndex index) const {
	assert(false);
	return nullptr;
}

Tile* Chunk::getRightTile(TileIndex index) const {
    assert(false);
    return nullptr;
}

Tile* Chunk::getTopTile(TileIndex index) const {
    assert(false);
    return nullptr;
}

Tile* Chunk::getBottomTile(TileIndex index) const {
    assert(false);
    return nullptr;
}

void Chunk::getTileNeighbors(TileIndex index, OUT Tile neighbors[8]) const {
	const ui16 x = index.getX();
	const ui16 y = index.getY();

	// Bottom row
	if (y > 0) {
		//TODO: TileIndex::bottom()
		ui16 bottomIndex = index - CHUNK_WIDTH;
		// Bottom left
		if (x > 0) {
			neighbors[(int)NeighborIndex::BOTTOM_LEFT] = mTiles[bottomIndex - 1];
		}
		neighbors[(int)NeighborIndex::BOTTOM] = mTiles[bottomIndex];
		if (x < CHUNK_WIDTH - 1) {
			neighbors[(int)NeighborIndex::BOTTOM_RIGHT] = mTiles[bottomIndex + 1];
		}
	}

	// Left
	if (x > 0) {
		neighbors[(int)NeighborIndex::LEFT] = mTiles[index - 1];
	}

	// Right
	if (x < CHUNK_WIDTH - 1) {
		neighbors[(int)NeighborIndex::RIGHT] = mTiles[index + 1];
	}

	// Top
    if (y < CHUNK_WIDTH - 1) {
        ui16 topIndex = index + CHUNK_WIDTH;
		if (x > 0) {
			neighbors[(int)NeighborIndex::TOP_LEFT] = mTiles[topIndex - 1];
		}
        neighbors[(int)NeighborIndex::TOP] = mTiles[topIndex];
		if (x < CHUNK_WIDTH - 1) {
			neighbors[(int)NeighborIndex::TOP_RIGHT] = mTiles[topIndex + 1];
		}
	}

}

ChunkID::ChunkID(const f32v2 worldPos) {
	pos = i32v2(floor(worldPos.x / CHUNK_WIDTH), floor(worldPos.y / CHUNK_WIDTH));
}
