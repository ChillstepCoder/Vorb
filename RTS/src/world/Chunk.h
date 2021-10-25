#pragma once

#include "ChunkID.h"
#include "TileHandle.h"

#include "item/ItemStack.h"
#include "util/AABB.hpp"

class Chunk;
class QuadMesh;
class BillboardMesh;
class NavGraph;

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
	std::unique_ptr<QuadMesh> mHighDetailFloraMesh = nullptr;
	std::unique_ptr<BillboardMesh> mBillboardMesh = nullptr;
	VGTexture mLODTexture = 0;
	bool mMeshDirty = true;
	bool mHighDetailFloraMeshDirty = true;
	bool mLODDirty = true;
    bool mIsBuildingBaseMesh = false; // When true, we are waiting for our mesh to be completed
    bool mIsBuildingHighDetailFloraMesh = false; // When true, we are waiting for our mesh to be completed
	bool mIsVisible = false;
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
    f32v3 getWorldPosCenter3D() const { return f32v3(mWorldPos.x + HALF_CHUNK_WIDTH, mWorldPos.y + HALF_CHUNK_WIDTH, 0.0f); }
	ChunkState getState() const { return mState; }
	const ChunkID& getChunkID() const { return mChunkId; }

	TileHandle getTileHandleAt(const TileIndex index) const;
	TileHandle getLeftTileHandle(const TileIndex index) const;
	TileHandle getRightTileHandle(const TileIndex index) const;
	TileHandle getTopTileHandle(const TileIndex index) const;
	TileHandle getBottomTileHandle(const TileIndex index) const;

    const TileCollision& getTileCollisionAt(const TileIndex index) const;
	std::vector<TileCollision>& getAllTileCollision() { return mCollision; }
	// Get neighbors starting from top left
	void getTileNeighbors(const TileIndex index, OUT Tile neighbors[8]) const;

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
	bool isDataReady() const { return mState > ChunkState::LOADING; }
	bool isFinished() const { return isDataReady() && mDataReadyNeighborCount == 4; }
	bool isVisible() const { return mChunkRenderData.mIsVisible; }

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

	void dirtyNavGraph() { mDirtyNavGraph = true; }

	void dirtyMesh() {
		// TODO: Not both
        mChunkRenderData.mMeshDirty = true;
        mChunkRenderData.mHighDetailFloraMeshDirty = true;
	}

    void setTileAt(TileIndex i, Tile tile);
	void setTileAt(TileIndex i, TileID tileId, TileLayer layer);
	void setTileCollisionAt(TileIndex i, TileCollision collision);
    void setTileFlagAt(TileIndex i, TileFlags flag);
    void setTileCollisionNavFlagAt(TileIndex i, TileCollisionNavFlags flag);

	void incRef() const {
		++mRefCount;
	}

	void decRef() const {
		--mRefCount;
	}

	// Events
	Event<Chunk*> onDispose;

private:

	void setTileFromGeneration(TileIndex i, Tile&& tile) {
		mTiles[i] = tile;
		updateTileCollisionAt(i);
	}

	void updateTileCollisionAt(TileIndex i);

	ChunkID mChunkId;
	f32v2 mWorldPos = f32v2(0.0f);
	f32AABB3 mAABB = f32AABB3(0.0f);
	ChunkState mState = ChunkState::INVALID;
	bool mDirtyNavGraph = false;

	// Refcount for threading
	mutable ui8 mRefCount = 0;

	ui8 mDataReadyNeighborCount = 0;
	WorldGrid* mWorldGrid = nullptr;

    std::vector<Tile> mTiles; // TODO: Memory recycler
	std::vector<TileCollision> mCollision; // TODO: Don't keep this in memory when its not needed?
	std::map<TileIndex, ItemStack> mItemsOnGround;

	// For use by ChunkRenderer
	mutable ChunkRenderData mChunkRenderData;

	// Thread safety
	// TODO: Reader/writer lock
	std::mutex mMutex;
};
