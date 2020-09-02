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
	mChunkId = chunkId;
	mState = ChunkState::LOADING;
}

void Chunk::dispose() {
	if (mState == ChunkState::INVALID) {
		return;
	}

	if (mNeighborLeft) {
		mNeighborLeft->mNeighborRight = nullptr;
	}
	if (mNeighborRight) {
		mNeighborRight->mNeighborLeft = nullptr;
	}
	if (mNeighborTop) {
		mNeighborTop->mNeighborBottom = nullptr;
	}
	if (mNeighborBottom) {
		mNeighborBottom->mNeighborTop = nullptr;
	}

	mState = ChunkState::INVALID;
}

Tile* Chunk::getLeftTile(TileIndex index) {
	assert(false);
	return nullptr;
}

Tile* Chunk::getRightTile(TileIndex index) {
    assert(false);
    return nullptr;
}

Tile* Chunk::getTopTile(TileIndex index) {
    assert(false);
    return nullptr;
}

Tile* Chunk::getBottomTile(TileIndex index) {
    assert(false);
    return nullptr;
}

void Chunk::getNeighbors(TileIndex index, OUT Tile neighbors[8]) {
    assert(false);
}

ChunkID::ChunkID(const f32v2 worldPos) {
	pos = i32v2(floor(worldPos.x / CHUNK_WIDTH), floor(worldPos.y / CHUNK_WIDTH));
}
