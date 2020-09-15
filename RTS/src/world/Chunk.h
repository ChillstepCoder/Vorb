#pragma once

#include "Tile.h"
// TODO: Removing this causes compile error
#include <Vorb/graphics/SpriteBatch.h>

class Chunk;
class QuadMesh;

const i64 CHUNK_ID_INVALID = INT64_MAX;

enum class ChunkState {
	INVALID,
	WAITING_FOR_INIT,
	LOADING,
	FINISHED,
};

struct ChunkRenderData {

	~ChunkRenderData();
	std::unique_ptr<QuadMesh> mChunkMesh;
	bool mBaseDirty = true;
};

struct ChunkID {

	ChunkID() : id(CHUNK_ID_INVALID) {}
	ChunkID(const ChunkID& other) { *this = other; }
	ChunkID(i64 id) : id(id) {};
	ChunkID(const i32v2& pos) : pos(pos) {};
	ChunkID(i32v2&& pos) : pos(pos) {};
	ChunkID(const f32v2 worldPos);

	// For std::map
	bool operator<(const ChunkID& other) const { return id < other.id; }
	bool operator!=(const ChunkID& other) const { return id != other.id; }
	void operator=(const ChunkID& other) { id = other.id; }
	void operator=(ChunkID&& other) noexcept { id = other.id; }

	f32v2 getWorldPos() const { return f32v2(pos.x * CHUNK_WIDTH, pos.y * CHUNK_WIDTH); }
	ChunkID getLeftID() const { return ChunkID(i32v2(pos.x - 1, pos.y)); }
	ChunkID getTopID() const { return ChunkID(i32v2(pos.x, pos.y + 1)); }
	ChunkID getRightID() const { return ChunkID(i32v2(pos.x + 1, pos.y)); }
	ChunkID getBottomID() const { return ChunkID(i32v2(pos.x, pos.y - 1)); }

	// Data
	union {
		i32v2 pos;
		i64 id;
	};
};

enum class NeighborIndex {
	BOTTOM_LEFT  = 0,
	BOTTOM       = 1,
	BOTTOM_RIGHT = 2,
	LEFT         = 3,
	RIGHT        = 4,
	TOP_LEFT     = 5,
	TOP          = 6,
	TOP_RIGHT    = 7,
	COUNT        = 8
};

struct TileHandle {

    TileHandle() {};
    TileHandle(const Chunk* chunk, TileIndex index) : chunk(chunk), index(index) {};
    TileHandle(const Chunk* chunk, TileIndex index, Tile tile) : chunk(chunk), index(index), tile(tile) {};

    bool isValid() const { return chunk != nullptr; }
	Chunk* getMutableChunk() { return const_cast<Chunk*>(chunk); }

    const Chunk* chunk = nullptr;
    TileIndex index;
    Tile tile;
};

class Chunk {
	friend class World;
	friend class ChunkGenerator;
	friend class ChunkRenderer;
	friend class ChunkMesher;
public:
	Chunk();
	~Chunk();

	// Position in cells
	void init(ChunkID chunkId);
	void dispose();

	const i32v2& getChunkPos() const { return mChunkId.pos; }
	f32v2 getWorldPos() const { return f32v2(mChunkId.pos) * (float)CHUNK_WIDTH; }
	ChunkState getState() const { return mState; }
	ChunkID getChunkID() const { return mChunkId; }

	TileHandle getLeftTile(const TileIndex index) const;
	TileHandle getRightTile(const TileIndex index) const;
	TileHandle getTopTile(const TileIndex index) const;
	TileHandle getBottomTile(const TileIndex index) const;
	// Get neighbors starting from top left
	void getTileNeighbors(const TileIndex index, OUT Tile neighbors[8]) const;

	bool isDataReady() const { return mState > ChunkState::LOADING; }
	bool canRender() const { return mDataReadyNeighborCount == 4; }

    Tile getTileAt(TileIndex i) const {
        assert(i < CHUNK_SIZE);
        assert(mState == ChunkState::FINISHED);
        return mTiles[i];
    }

    void setTileAt(TileIndex i, Tile tile) {
		assert(i < CHUNK_SIZE);
        mTiles[i] = tile;
        mChunkRenderData.mBaseDirty = true;
    }

	void setTileAt(TileIndex i, TileID tileId, TileLayer layer) {
        mTiles[i].layers[(int)layer] = tileId;
        mChunkRenderData.mBaseDirty = true;
	}

private:
	ChunkID mChunkId;
	Tile mTiles[CHUNK_SIZE];
	ChunkState mState = ChunkState::WAITING_FOR_INIT;

	union {
		struct {
			Chunk* mNeighborLeft;
			Chunk* mNeighborTop;
			Chunk* mNeighborRight;
			Chunk* mNeighborBottom;
		};
		Chunk* mNeighbors[4];
	};
	ui8 mDataReadyNeighborCount = 0;

	// For use by ChunkRenderer
	mutable ChunkRenderData mChunkRenderData;
};

