#pragma once

#include "Tile.h"
// TODO: Removing this causes compile error
#include <Vorb/graphics/SpriteBatch.h>

const int CHUNK_WIDTH = 128;
const int CHUNK_SIZE = CHUNK_WIDTH * CHUNK_WIDTH;
const i64 CHUNK_ID_INVALID = INT64_MAX;


enum class ChunkState {
	INVALID,
	LOADING,
	FINISHED
};


struct ChunkRenderData {
	std::unique_ptr<vg::SpriteBatch> mSb = nullptr; // Compile error here without SpriteBatch.h
	bool mDirty = true;
};

struct ChunkID {

	ChunkID(const ChunkID& other) { *this = other; }
	ChunkID(i64 id) : id(id) {};
	ChunkID(const i32v2& pos) : pos(pos) {};
	ChunkID(i32v2&& pos) : pos(pos) {};
	ChunkID(const f32v2 worldPos);

	// For std::map
	bool operator<(const ChunkID& other) const { return id < other.id; }
	bool operator!=(const ChunkID& other) const { return id != other.id; }
	void operator=(const ChunkID& other) { id = other.id; }
	void operator=(ChunkID&& other) { id = other.id; }

	// Data
	union {
		i32v2 pos;
		i64 id;
	};
};

class Chunk {
	friend class ChunkGenerator;
	friend class ChunkRenderer;
public:
	Chunk();

	// Position in cells
	void init(ChunkID chunkId);

	const i32v2& getChunkPos() const { return mChunkId.pos; }
	f32v2 getWorldPos() const { return f32v2(mChunkId.pos) * (float)CHUNK_WIDTH; }
	ChunkState getState() const { return mState; }

	Tile getTileAt(unsigned x, unsigned y) const {
		assert(x < CHUNK_WIDTH && y < CHUNK_WIDTH);
		assert(mState == ChunkState::FINISHED);
		return mTiles[y * CHUNK_WIDTH + x];
	}

	void setTileAt(unsigned x, unsigned y, Tile tile) {
		assert(x < CHUNK_WIDTH && y < CHUNK_WIDTH);
		mTiles[y * CHUNK_WIDTH + x] = tile;
		mChunkRenderData.mDirty = true;
	}

private:
	ChunkID mChunkId;
	Tile mTiles[CHUNK_SIZE];
	ChunkState mState = ChunkState::INVALID;

	// For use by ChunkRenderer
	mutable ChunkRenderData mChunkRenderData;
};

