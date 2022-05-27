#pragma once

#include "ChunkID.h"
#include "tile/TileHandle.h"
#include "tile/TileContainer.h"

#include "item/ItemStack.h"
#include "util/AABB.hpp"
#include "util/TinyThreadsafeVector.hpp"

class Chunk;
class QuadMesh;
class Mesh;
class BillboardMesh;
class TBOBillboardMesh;
class GrassBillboardMesh;
class ChunkGrassQuadtree;
class NavGraph;
class Structure;

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

// TODO: Meshcomponent for cache friendly iterate?
struct ChunkRenderData {
	ChunkRenderData() = default;
	~ChunkRenderData();
    std::unique_ptr<Mesh> mChunkMesh = nullptr;
    std::unique_ptr<Mesh> mBillboardMesh = nullptr;
	std::unique_ptr<ChunkGrassQuadtree> mGrassLod = nullptr;
    bool mIsBuildingBaseMesh = false; // When true, we are waiting for our mesh to be completed
	bool mIsVisible = false;
};

typedef TinyThreadsafeVector<Structure*> ChunkStructureVector;
typedef std::pair<Structure*const*, ui16> StructureArrayPtr;

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

// TODO: Chunks and structures both have base class "TileContainer" ???
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
	void updateMainThread();


    // =========== Accessors  ===========
	const i32v2& getChunkPos() const { return mChunkId.pos; }
    const f32v2& getWorldPos() const { return mWorldPos; }
	const f32v3 getWorldPos3D() const { return f32v3(mWorldPos.x, mWorldPos.y, 0.0f); }
    f32v3 getWorldPosCenter3D() const { return f32v3(mWorldPos.x + HALF_CHUNK_WIDTH, mWorldPos.y + HALF_CHUNK_WIDTH, 0.0f); }
	ChunkState getState() const { return (ChunkState)mState.load(); }
    const ChunkID& getChunkID() const { return mChunkId; }
	const HeightmapPatchID getHeightmapPatchID() const { return heightmapPatchIDFromChunkID(mChunkId); }
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
	bool isInvalid() const { return mState == e_cast(ChunkState::INVALID); }
	bool isDataReady() const { return mState == e_cast(ChunkState::FINISHED); }
	bool isFinished() const { return mState == e_cast(ChunkState::FINISHED) && mDataReadyNeighborCount == CHUNK_NEIGHBOR_COUNT; }
	bool isVisible() const { return mChunkRenderData.mIsVisible; }

	void setState(ChunkState state) { mState = e_cast(state); }
	void setGrassAt(const TileIndex index, ui8 grass);

    // =========== Structures  ===========
    void setStructureAt(const TileIndex index, Structure* structure);
    void removeStructureAt(const TileIndex index, Structure* structure);
	StructureArrayPtr getStructuresAt(const TileIndex index) const;
	StructureArrayPtr getStructuresAtThreadSafe(const TileIndex index) const;

    // =========== Terrain update  ===========
	void onTerrainDataChanged(const f32v2& editPosition, f32 editRadius);


    // =========== Tiles  ===========
    TileContainer& getTileContainer() { return mTileContainer; }
    const TileContainer& getTileContainer() const { return mTileContainer; }


    // =========== Dirtyness  ===========
	void dirtyNavGraph() { mTileContainer.setDirtyNav(true); }
	void dirtyMesh() { mTileContainer.setDirtyMesh(true); }


    // =========== Ref counting  ===========
	void incReadLock() const { mTileContainer.incReadLock(); }
	void decReadLock() const { mTileContainer.decReadLock(); }
	inline void incRef() const { mTileContainer.incRef(); }
	inline void decRef() const { mTileContainer.decRef(); }
	void incRefNeighbors4() const;
	void decRefNeighbors4() const;
	void incReadLockNeighbors4() const;
	void decReadLockNeighbors4() const;
    void incReadLockAndRefCountNeighbors4AndSelf() const;
    void decReadLockAndRefCountNeighbors4AndSelf() const;
    void incReadLockAndRefCount() const { incRef(); mTileContainer.incReadLock();  }
    void decReadLockAndRefCount() const { mTileContainer.decReadLock(); decRef(); }

    // =========== Events ===========
	Event<Chunk*> onDispose;

private:
    // =========== Read lock ===========
	bool isReadLocked() const { return mTileContainer.isReadLocked(); }

    // =========== Members ===========
	ChunkID mChunkId;
	f32v2 mWorldPos = f32v2(0.0f);
	f32AABB3 mAABB = f32AABB3(0.0f); // TODO: Combine with worldpos?
	std::atomic_uint8_t mState = (ui8)ChunkState::INVALID;

	// Atomic tasking checks
	std::atomic_bool mIsNavmeshing = false;

	ui8 mDataReadyNeighborCount = 0;
	WorldGrid* mWorldGrid = nullptr;

	TileContainer mTileContainer;
    std::vector<ui8> mGrass; // Grass densities
	std::vector<ChunkStructureVector> mStructures; // TODO: List or something for multiple structures? idk
	std::vector<TileIndex> mStructuresNeedingThreadSafeCopy;
	std::map<TileIndex, ItemStack> mItemsOnGround;

	// For use by ChunkRenderer
	mutable ChunkRenderData mChunkRenderData; // TODO: Make this a unique_ptr? Most chunks will keep these pointers invalid

};
#ifdef DEBUG // Release has different size
//static_assert(sizeof(Chunk) == 296, "These are permanently allocated, so keep small");
#endif