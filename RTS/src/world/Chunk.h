#pragma once

#include "ChunkID.h"
#include "TileHandle.h"

#include "item/ItemStack.h"
#include "util/AABB.hpp"

extern bool IS_SHUTTING_DOWN;

class Chunk;
class QuadMesh;
class BillboardMesh;
class TBOBillboardMesh;
class GrassBillboardMesh;
class ChunkGrassQuadtree;
class NavGraph;

#define USE_INSTANCED_BILLBOARDS 1
#if USE_INSTANCED_BILLBOARDS == 1
typedef TBOBillboardMesh ChunkBillboardMesh;
#else
typedef BillboardMesh ChunkBillboardMesh;
#endif

constexpr ui32 CHUNK_NEIGHBOR_COUNT = 4;

class WorldGrid;

enum class ChunkState {
	INVALID,
	WAITING_HEIGHT,
	LOADING_TILES,
	FINISHED,
};

struct ChunkRenderData {
	ChunkRenderData() = default;
	~ChunkRenderData();
    std::unique_ptr<QuadMesh> mChunkMesh = nullptr;
    std::unique_ptr<ChunkBillboardMesh> mBillboardMesh = nullptr;
	std::unique_ptr<ChunkGrassQuadtree> mGrassLod = nullptr;
	bool mMeshDirty = true;
    bool mIsBuildingBaseMesh = false; // When true, we are waiting for our mesh to be completed
	bool mIsVisible = false;
};

enum class NeighborIndex4 {
	BOTTOM = 0,
	LEFT   = 1,
	RIGHT  = 2,
	TOP    = 3
};

enum class NeighborIndex8 {
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

class Chunk {
	friend class World;
	friend class WorldGrid;
	friend class WorldEditor;
	friend class ChunkGenerator;
	friend class ChunkRenderer;
	friend class ChunkMesher;
    friend class RenderContext; // For debug rendering of neighbors only
    friend class NavGraph;
	friend class NavThread;
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
	const f32v3 getWorldPos3D() const { return f32v3(mWorldPos.x, mWorldPos.y, 0.0f); }
    f32v3 getWorldPosCenter3D() const { return f32v3(mWorldPos.x + HALF_CHUNK_WIDTH, mWorldPos.y + HALF_CHUNK_WIDTH, 0.0f); }
	ChunkState getState() const { return mState; }
	const ChunkID& getChunkID() const { return mChunkId; }

	TileHandle getTileHandleAt(const TileIndex index) const;
	TileHandle getLeftTileHandle(const TileIndex index) const;
	TileHandle getRightTileHandle(const TileIndex index) const;
	TileHandle getTopTileHandle(const TileIndex index) const;
	TileHandle getBottomTileHandle(const TileIndex index) const;

	ui8 getGrassAt(const TileIndex index) const { return mGrass[index]; }
	// Get neighbors starting from top left
    void getTileNeighbors8(const TileIndex index, OUT Tile neighbors[8]) const;
    void getTileNeighbors4(const TileIndex index, OUT TileHandle neighbors[4]) const;

	Chunk& getLeftNeighbor() const;
	Chunk& getTopNeighbor() const;
	Chunk& getRightNeighbor() const;
	Chunk& getBottomNeighbor() const;

	const f32AABB3& getAABB() const { return mAABB; }

	// Items
	std::map<TileIndex, ItemStack>& getItemsOnGround() { return mItemsOnGround; }
	void dropItemStackOnGround(ItemStack item);
	ItemStack getItemStackOnGround(TileIndex pos);

	bool isInvalid() const { return mState == ChunkState::INVALID; }
	bool isDataReady() const { return mState == ChunkState::FINISHED; }
	bool isFinished() const { return mState == ChunkState::FINISHED && mDataReadyNeighborCount == CHUNK_NEIGHBOR_COUNT; }
	bool isVisible() const { return mChunkRenderData.mIsVisible; }

	void setState(ChunkState state) { mState = state; }
	void setGrassAt(const TileIndex index, ui8 grass);

	void onTerrainDataChanged(const f32v2& editPosition, f32 editRadius);

    Tile& getMutableTileAt(TileIndex i) {
        assert(i < CHUNK_SIZE);
        assert(mState == ChunkState::FINISHED);
		return mTiles[i];
	}

    const Tile& getTileAt(TileIndex i) const {
        assert(i < CHUNK_SIZE);
        assert(mState == ChunkState::FINISHED);
        return mTiles[i];
    }

	const Tile& getTileAtNoAssert(TileIndex i) const {
        return mTiles[i];
	}

	void dirtyNavGraph() { mDirtyNavGraph = true; }

	void dirtyMesh() {
        mChunkRenderData.mMeshDirty = true;
	}

    void setTileAt(TileIndex i, Tile tile);
	void setTileAt(TileIndex i, TileID tileId, TileLayer layer);
    void setTileFlagAt(TileIndex i, TileFlags flag);

	inline void incRef() const {
		assert(IS_MAIN_THREAD()); // Only main thread is allowed to incref
		assert(mRefCount.load() < 255u);
		++mRefCount;
	}

    inline void decRef() const {
		assert(mRefCount.load());
		--mRefCount;
	}

	void incRefNeighbors4() const;
	void decRefNeighbors4() const;
	void incReadLockNeighbors4() const;
	void decReadLockNeighbors4() const;
    void incReadLockAndRefCountNeighbors4AndSelf() const;
    void decReadLockAndRefCountNeighbors4AndSelf() const;
    void incReadLock() const { ++mReadLockCount; }
	void decReadLock() const { assert(mReadLockCount.load() > 0);  --mReadLockCount; }
    void incReadLockAndRefCount() const { incRef(); incReadLock();  }
    void decReadLockAndRefCount() const { decReadLock(); decRef(); }

	// Events
	Event<Chunk*> onDispose;

private:

	void setTileFromGeneration(TileIndex i, Tile&& tile) {
		mTiles[i] = tile;
		updateTileCollisionAt(i, tile.topLayer);
	}

	void updateTileCollisionAt(TileIndex i, TileID tileId);

	ChunkID mChunkId;
	f32v2 mWorldPos = f32v2(0.0f);
	f32AABB3 mAABB = f32AABB3(0.0f); // TODO: Combine with worldpos?
	ChunkState mState = ChunkState::INVALID;
	bool mDirtyNavGraph = false;

	// Refcount for threading
    mutable std::atomic_uchar mRefCount = 0;
    mutable std::atomic_uint32_t mReadLockCount = 0;

	// Atomic tasking checks
	std::atomic_bool mIsNavmeshing = false;

	ui8 mDataReadyNeighborCount = 0;
	WorldGrid* mWorldGrid = nullptr;

    std::vector<Tile> mTiles; // TODO: Memory recycler
	std::vector<ui8> mGrass; // Grass densities
	std::map<TileIndex, ItemStack> mItemsOnGround;

	// For use by ChunkRenderer
	mutable ChunkRenderData mChunkRenderData; // TODO: Make this a unique_ptr? Most chunks will keep these pointers invalid

	// Thread safety
	// TODO: Reader/writer lock
	// std::mutex mMutex;
};
#ifdef DEBUG // Release has different size
static_assert(sizeof(Chunk) == 272, "These are permanently allocated, so keep small");
#endif