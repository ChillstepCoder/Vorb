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

enum class ChunkState : ui8 {
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

    // =========== Main methods  ===========
	void init(const ChunkID& chunkId, WorldGrid& worldGrid);
	void allocateTiles();
	void freeTiles();
	void dispose();


    // =========== Accessors  ===========
	const i32v2& getChunkPos() const { return mChunkId.pos; }
    const f32v2& getWorldPos() const { return mWorldPos; }
	const f32v3 getWorldPos3D() const { return f32v3(mWorldPos.x, mWorldPos.y, 0.0f); }
    f32v3 getWorldPosCenter3D() const { return f32v3(mWorldPos.x + HALF_CHUNK_WIDTH, mWorldPos.y + HALF_CHUNK_WIDTH, 0.0f); }
	ChunkState getState() const { return (ChunkState)mState.load(); }
    const ChunkID& getChunkID() const { return mChunkId; }
    ui8 getGrassAt(const TileIndex index) const { return mGrass[index]; }
    const f32AABB3& getAABB() const { return mAABB; }


    // =========== Tile handles  ===========
	TileHandle getTileHandleAt(const TileIndex index) const;
	TileHandle getLeftTileHandle(const TileIndex index) const;
	TileHandle getRightTileHandle(const TileIndex index) const;
	TileHandle getTopTileHandle(const TileIndex index) const;
	TileHandle getBottomTileHandle(const TileIndex index) const;


    // =========== Neighbor access  ===========
	// Get neighbors starting from top left
    void getTileNeighbors8(const TileIndex index, OUT Tile neighbors[8]) const;
    void getTileNeighbors4(const TileIndex index, OUT TileHandle neighbors[4]) const;
	Chunk& getLeftNeighbor() const;
	Chunk& getTopNeighbor() const;
	Chunk& getRightNeighbor() const;
	Chunk& getBottomNeighbor() const;


    // =========== Items  ===========
	std::map<TileIndex, ItemStack>& getItemsOnGround() { return mItemsOnGround; }
	void dropItemStackOnGround(ItemStack item);
	ItemStack getItemStackOnGround(TileIndex pos);

    // =========== State  ===========
	bool isInvalid() const { return mState == enum_cast(ChunkState::INVALID); }
	bool isDataReady() const { return mState == enum_cast(ChunkState::FINISHED); }
	bool isFinished() const { return mState == enum_cast(ChunkState::FINISHED) && mDataReadyNeighborCount == CHUNK_NEIGHBOR_COUNT; }
	bool isVisible() const { return mChunkRenderData.mIsVisible; }

	void setState(ChunkState state) { mState = enum_cast(state); }
	void setGrassAt(const TileIndex index, ui8 grass);

    // =========== Terrain update  ===========
	void onTerrainDataChanged(const f32v2& editPosition, f32 editRadius);


    // =========== Tile accessors  ===========
    Tile& getMutableTileAt(TileIndex i) {
		assert(IS_MAIN_THREAD());
        assert(i < CHUNK_SIZE);
        assert(mState == enum_cast(ChunkState::FINISHED));
		return mTiles[i];
	}

    const Tile& getTileAt(TileIndex i) const {
        assert(i < CHUNK_SIZE);
        assert(mState == enum_cast(ChunkState::FINISHED));
        return mTiles[i];
    }

	const Tile& getTileAtNoAssert(TileIndex i) const {
        return mTiles[i];
	}


    // =========== Dirtyness  ===========
	void dirtyNavGraph() { mDirtyNavGraph = true; }
	void dirtyMesh() { mChunkRenderData.mMeshDirty = true; }


	// =========== Tile mutators ===========
    void setTileAt(TileIndex i, Tile tile);
    bool canAddTile(TileIndex i, const TileData& tileData) const;
    void addTile(TileIndex i, const TileData& tileData);
    bool tryAddTile(TileIndex i, const TileData& tileData);
    void setTileLayer(TileIndex i, TileLayer layer, TileID id);
    void setTileFlag(TileIndex i, TileFlags flag);
    void setTileFlags(TileIndex i, TileFlags flags);
    void clearTileFlag(TileIndex i, TileFlags flag);
    void clearTileFlags(TileIndex i);
    void clearTileCollisionFlags(TileIndex i);
    void setTilePathWeight(TileIndex i, ui8 weight);
    void setTileBaseZPosition(TileIndex i, f32 baseZPosition);

    // =========== Ref counting  ===========
	inline void incRef() const {
		assert(IS_MAIN_THREAD()); // Only main thread is allowed to incref
		assert(mRefCount.load() < 2000u); // This is probably a sign of something really awful
		++mRefCount;
        if (mRefCount > 200) {
            std::cout << "DETECTED " << mRefCount << " REF COUNTS ON CHUNK " << std::endl;
            assert(false);
        }
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
	void incReadLock() const { ++mReadLockCount; assert(isDataReady()); }
	void decReadLock() const { assert(mReadLockCount.load() > 0);  --mReadLockCount; }
    void incReadLockAndRefCount() const { incRef(); incReadLock();  }
    void decReadLockAndRefCount() const { decReadLock(); decRef(); }

    // =========== Events ===========
	Event<Chunk*> onDispose;

private:
    // =========== Read lock ===========
	bool isReadLocked() const;

    // =========== Generation ===========
	void setTileFromGeneration(TileIndex i, Tile&& tile) {
		mTiles[i] = tile;
		updateTileCollisionAt(i, tile.layers[TILE_LAYER_TOP], false);
	}

    // =========== Collision ===========
	void updateTileCollisionAt(TileIndex i, TileID tileId, bool readLocked);

    // =========== Members ===========
	ChunkID mChunkId;
	f32v2 mWorldPos = f32v2(0.0f);
	f32AABB3 mAABB = f32AABB3(0.0f); // TODO: Combine with worldpos?
	std::atomic_uint8_t mState = (ui8)ChunkState::INVALID;
	bool mDirtyNavGraph = false;


	// Refcount for threading
    mutable std::atomic_uint32_t mRefCount = 0;
    mutable std::atomic_uint32_t mReadLockCount = 0;

	// Atomic tasking checks
	std::atomic_bool mIsNavmeshing = false;

	ui8 mDataReadyNeighborCount = 0;
	WorldGrid* mWorldGrid = nullptr;

    std::vector<Tile> mTiles; // TODO: Memory recycler
    std::vector<ui8> mGrass; // Grass densities
    // All tiles that need to update when read lock is free
    std::vector<TileIndex> mTilesNeedingThreadSafeCopy;
	std::map<TileIndex, ItemStack> mItemsOnGround;

	// For use by ChunkRenderer
	mutable ChunkRenderData mChunkRenderData; // TODO: Make this a unique_ptr? Most chunks will keep these pointers invalid

};
#ifdef DEBUG // Release has different size
//static_assert(sizeof(Chunk) == 296, "These are permanently allocated, so keep small");
#endif