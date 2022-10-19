#pragma once

#include "ChunkID.h"
#include "tile/TileHandle.h"
#include "tile/TileContainer.h"

#include "item/ItemStack.h"
#include "util/AABB.hpp"
#include "util/TinyThreadsafeVector.hpp"

class Chunk;
class Mesh;
class BillboardMesh;
class TBOBillboardMesh;
class GrassBillboardMesh;
class ChunkGrassQuadtree;
class NavWorld;
class Structure;

#define USE_INSTANCED_BILLBOARDS 1
#if USE_INSTANCED_BILLBOARDS == 1
typedef TBOBillboardMesh ChunkBillboardMesh;
#else
typedef BillboardMesh ChunkBillboardMesh;
#endif

constexpr ui32 CHUNK_NEIGHBOR_COUNT = 4;

class IWorldGrid;

enum class ChunkState : ui8 {
	INVALID,
	WAITING_HEIGHT,
	LOADING_TILES, // Only worker thread can change from LOADING_TILES to TILE_LOAD_FINISHED
	TILE_LOAD_FINISHED,
	FINISHED,
};

enum class ChunkFlags : ui8 {
	IN_DESTROY_LIST = 1 << 0,
};

// TODO: Meshcomponent for cache friendly iterate?
struct ChunkRenderData {
	ChunkRenderData() = default;
	~ChunkRenderData();
	std::unique_ptr<ChunkGrassQuadtree> mGrassLod = nullptr;
};

typedef TinyThreadsafeVector<Structure*> ChunkStructureVector;
typedef std::pair<Structure*const *, ui16> StructureArrayPtr;

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
	friend class IWorld;
	friend class IWorldGrid;
	friend class WorldEditor;
	friend class ChunkGenerator;
	friend class TileContainerRenderer;
	friend class ChunkMesher;
	friend class IChunkGrid;
    friend class RenderContext; // For debug rendering of neighbors only
    friend class NavWorld;
	friend class NavThread;
	friend struct TileHandle;
	friend struct TileRef;
public:
	Chunk();
	~Chunk();

    // =========== Main methods  ===========
	void init(const ChunkID& chunkId);
	void allocateTileContainer();
	void freeTiles();
	void dispose();
	void updateMainThread();


    // =========== Accessors  ===========
	const i32v2& getChunkPos() const { return mChunkId.pos; }
    const f32v2& getWorldPos() const { return mChunkId.getWorldPos(); }
	const f32v3 getWorldPos3D() const { const f32v2& worldPos = getWorldPos(); return f32v3(worldPos.x, worldPos.y, 0.0f); }
    f32v3 getWorldPosCenter3D() const { const f32v2& worldPos = getWorldPos(); return f32v3(worldPos.x + HALF_CHUNK_WIDTH, worldPos.y + HALF_CHUNK_WIDTH, 0.0f); }
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
    TileContainer* getTileContainer() { return mTileContainer; }
    const TileContainer* getTileContainer() const { return mTileContainer; }


    // =========== Dirtyness  ===========
	void dirtyNavGraph() { mTileContainer->setDirtyNav(true); }


    // =========== Ref counting  ===========
	void incReadLock() const { mTileContainer->incReadLock(); }
	void decReadLock() const { mTileContainer->decReadLock(); }
	inline void incRef() const { mTileContainer->incRef(); }
	inline void decRef() const { mTileContainer->decRef(); }
    void incReadLockAndRefCount() const { incRef(); mTileContainer->incReadLock();  }
    void decReadLockAndRefCount() const { mTileContainer->decReadLock(); decRef(); }
    ui32 getRefCount() const { return mTileContainer->getRefCount(); }

private:
    // =========== Read lock ===========
	bool isReadLocked() const { return mTileContainer->isReadLocked(); }

    // =========== Members ===========
	ChunkID mChunkId;
	f32AABB3 mAABB = f32AABB3(0.0f);
    std::atomic_uint8_t mState = (ui8)ChunkState::INVALID;
	BitFlags<ChunkFlags> mFlags;

	TileContainer* mTileContainer = nullptr;
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