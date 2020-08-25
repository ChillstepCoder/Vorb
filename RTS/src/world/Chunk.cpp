#include "stdafx.h"
#include "Chunk.h"


Chunk::Chunk() :
	mChunkId(CHUNK_ID_INVALID) {
}

void Chunk::init(ChunkID chunkId) {
	mChunkId = chunkId;
	mState = ChunkState::LOADING;
}

void Chunk::load() {
	memset(mTiles, 0, sizeof(mTiles));
	mState = ChunkState::FINISHED;
}

ChunkID::ChunkID(const f32v2 worldPos) {
	pos = i32v2(floor(worldPos.x / CHUNK_WIDTH), floor(worldPos.y / CHUNK_WIDTH));
}
