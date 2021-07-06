#pragma once

#include "Tile.h"
#include "world/WorldData.h"
#include "item/ItemStack.h"

class Chunk;
class QuadMesh;

const ui32 CHUNK_ID_INVALID = UINT32_MAX;
constexpr ui32 CHUNK_NEIGHBOR_COUNT = 4;

class WorldGrid;

enum class ChunkState {
	INVALID,
	LOADING,
	FINISHED,
};

struct ChunkRenderData {
	ChunkRenderData() = default;
	~ChunkRenderData();
    std::unique_ptr<QuadMesh> mChunkMesh = nullptr;
    std::unique_ptr<QuadMesh> mFloraMesh = nullptr;
	VGTexture mLODTexture = 0;
	bool mBaseDirty = true;
	bool mLODDirty = true;
	bool mFloraDirty = true;
	bool mIsBuildingBaseMesh = false; // When true, we are waiting for our mesh to be completed
	bool mIsBuildingFloraMesh = false;
};

struct ChunkID {
    ChunkID() : id(CHUNK_ID_INVALID), pos(CHUNK_ID_INVALID) {}
    ChunkID(const ChunkID& other) { *this = other; }
	ChunkID(ui32 id);
    ChunkID(const ui32v2& pos) : pos(pos) { initIdFromPos(); };
    ChunkID(ui32v2&& pos) : pos(pos) { initIdFromPos(); };
    ChunkID(ui32 xPos, ui32 yPos) : pos(xPos, yPos) { initIdFromPos(); };
    ChunkID(const f32v2 worldPos);

    // For std::map
    bool operator<(const ChunkID& other) const { return id < other.id; }
    bool operator!=(const ChunkID& other) const { return id != other.id; }
    bool operator==(const ChunkID& other) const { return id == other.id; }

    f32v2 getWorldPos() const { return f32v2(pos.x * CHUNK_WIDTH, pos.y * CHUNK_WIDTH); }
    ChunkID getLeftID() const { return ChunkID(ui32v2(pos.x - 1, pos.y)); }
    ChunkID getTopID() const { return ChunkID(ui32v2(pos.x, pos.y + 1)); }
    ChunkID getRightID() const { return ChunkID(ui32v2(pos.x + 1, pos.y)); }
    ChunkID getBottomID() const { return ChunkID(ui32v2(pos.x, pos.y - 1)); }

	// Return true if we should never load
	bool isSentinelID() const {
		return pos.x == 0 || pos.y == 0 || pos.x == WorldData::WORLD_WIDTH_CHUNKS - 1 || pos.y == WorldData::WORLD_WIDTH_CHUNKS - 1;
	}

	ui32v2 pos;
	ui32 id;

private:
	inline void initIdFromPos() {
		id = pos.y * WorldData::WORLD_WIDTH_CHUNKS + pos.x;
	}
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

struct LiteTileHandle {
    LiteTileHandle() {};
    LiteTileHandle(ChunkID chunkID, TileIndex index) : chunkID(chunkID), index(index) {};

	f32v2 getWorldPos() const {
		f32v2 rv = chunkID.getWorldPos();
		rv.x += index.getX();
		rv.y += index.getY();
		return rv;
	}

	ChunkID chunkID;
	TileIndex index;
};

struct TileHandle {

    TileHandle() {};
    TileHandle(const Chunk* chunk, TileIndex index);

    bool isValid() const { return chunk != nullptr; }
	Chunk* getMutableChunk() { return const_cast<Chunk*>(chunk); }

	TileHandle& operator=(const TileHandle& other) {
		chunk = other.chunk;
		const_cast<TileIndex&>(index) = other.index;
		const_cast<Tile&>(tile) = other.tile;
		return *this;
	}

    const Chunk* chunk = nullptr;
    const TileIndex index;
    const Tile tile;
};

// DOES NOT PROVIDE THREAD SAFE READ/WRITE
struct TileRef {
    TileRef(TileHandle handle);
    TileRef(Chunk* chunk, TileIndex index);
    ~TileRef() { release(); }
    void release();

    TileRef& operator=(const TileRef& other) = delete;

    Chunk* chunk = nullptr;
    TileIndex index;
    Tile tile;
};

class Chunk {
	friend class World;
	friend class ChunkGenerator;
	friend class ChunkRenderer;
	friend class ChunkMesher;
	friend class RenderContext; // For debug rendering of neighbors only
	friend struct TileHandle;
	friend struct TileRef;
public:
	Chunk();
	~Chunk();

	// Position in cells
	void init(const ChunkID& chunkId, WorldGrid& worldGrid);
	void allocateTiles();
	void freeTiles();
	void dispose();

	const i32v2& getChunkPos() const { return mChunkId.pos; }
	const f32v2& getWorldPos() const { return mWorldPos; }
	ChunkState getState() const { return mState; }
	const ChunkID& getChunkID() const { return mChunkId; }

	TileHandle getTileHandleAt(const TileIndex index) const;
	TileHandle getLeftTileHandle(const TileIndex index) const;
	TileHandle getRightTileHandle(const TileIndex index) const;
	TileHandle getTopTileHandle(const TileIndex index) const;
	TileHandle getBottomTileHandle(const TileIndex index) const;
	// Get neighbors starting from top left
	void getTileNeighbors(const TileIndex index, OUT Tile neighbors[8]) const;

	Chunk& getLeftNeighbor() const;
	Chunk& getTopNeighbor() const;
	Chunk& getRightNeighbor() const;
	Chunk& getBottomNeighbor() const;

	bool isInvalid() const { return mState == ChunkState::INVALID; }
	bool isDataReady() const { return mState > ChunkState::LOADING; }
	bool isFinished() const { return isDataReady() && mDataReadyNeighborCount == 4; }

    Tile& getMutableTileAt(TileIndex i) {
        assert(i < CHUNK_SIZE);
        assert(mState == ChunkState::FINISHED);
		return mTiles[i];
	}

    Tile getTileAt(TileIndex i) const {
        assert(i < CHUNK_SIZE);
        assert(mState == ChunkState::FINISHED);
        return mTiles[i];
    }

    Tile getTileAtNoAssert(TileIndex i) const {
        return mTiles[i];
	}

    void setTileAt(TileIndex i, Tile tile) {
		assert(i < CHUNK_SIZE);
        mTiles[i] = tile;
        mChunkRenderData.mBaseDirty = true;
		// TODO: Don't dirty the flora mesh always
		mChunkRenderData.mFloraDirty = true;
    }

	void setTileAt(TileIndex i, TileID tileId, TileLayer layer) {
        mTiles[i].layers[(int)layer] = tileId;
        mChunkRenderData.mBaseDirty = true;
        // TODO: Don't dirty the flora mesh always
        mChunkRenderData.mFloraDirty = true;
	}

	void incRef() const {
		++mRefCount;
	}

	void decRef() const {
		--mRefCount;
	}

	// Items
	const ItemStack* tryGetItemStackAt(TileIndex i) const; // Do not hold onto this pointer, it will invalidate
	bool tryAddFullItemStackAt(TileIndex i, ItemStack itemStack);
	// Returns remaining item stack, which can be 0
	ItemStack tryAddPartialItemStackAt(TileIndex i, ItemStack itemStack);

private:

	void setTileFromGeneration(TileIndex i, Tile&& tile) {
		mTiles[i] = tile;
	}

	ChunkID mChunkId;
	f32v2 mWorldPos = f32v2(0.0f);
	std::vector<Tile> mTiles; // TODO: Memory recycler
	// TODO: Custom data structure?
	// TODO: Morton order + lower/upper bound to find closest?
	// TODO: Only track loose items, let stockpiles track their own items? Problem is 
	// then you run into item conflicts, have to check stockpile AND loose? or just pass ownership of loose
	// to stockpile even if it doesn't belong?
	std::map<TileIndex, ItemStack> mItemsOnFloor;
	ChunkState mState = ChunkState::INVALID;

	// Refcount for threading
	mutable ui8 mRefCount = 0;

	ui8 mDataReadyNeighborCount = 0;
	WorldGrid* mWorldGrid = nullptr;

	// For use by ChunkRenderer
	mutable ChunkRenderData mChunkRenderData;

	// Thread safety
	// TODO: Reader/writer lock
	std::mutex mMutex;
};
