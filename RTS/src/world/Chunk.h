#pragma once

#include "GridID.h"
#include "tile/TileHandle.h"
#include "tile/TileContainer.h"

#include "item/ItemStack.h"
#include "util/AABB.hpp"
#include "util/TinyThreadsafeVector.hpp"
#include "world/ChunkState.h"
#include "world/ChunkEvents.h"

#include "tile/TileGrass.h"

#include <shared_mutex>

class Chunk;
class BillboardMesh;
class TBOBillboardMesh;
class NavWorld;
class Building;
class TileContainerRepository;

#define USE_INSTANCED_BILLBOARDS 1
#if USE_INSTANCED_BILLBOARDS == 1
typedef TBOBillboardMesh ChunkBillboardMesh;
#else
typedef BillboardMesh ChunkBillboardMesh;
#endif

constexpr ui32 CHUNK_NEIGHBOR_COUNT = 4;

class IWorldGrid;

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

// Stores all tile containers that exist on a chunk and stores references to them.
// Can be copied to worker threads to allow fast lookups and guarentee containers will not be destroyed
class ChunkTileContainersLookup {
public:
	TileContainerID structureContainers[CHUNK_SIZE];
	TileContainerID chunkContainerID;
	std::unordered_map<TileContainerID, TileContainerRef> structureContainerRefs;
};


// TODO: Use
class SimulatedChunk {
public:

	struct SimBiomeContents {
		//BiomeUniqueID biomeId;
		
	};

	//std::vector<ChunkHarvestableTile> mHarvestableTiles[e_cast(TileHarvestable::COUNT)];
};


// TODO: Chunks and structures both have base class "TileContainer" ???
class Chunk {
	friend class World;
	friend class IWorldGrid;
	friend class WorldEditorPanel;
	friend class ChunkGenerator;
	friend class ITileContainerMesher;
    friend class IChunkGrid;
    friend class CliChunkGrid;
    friend class RenderContext; // For debug rendering of neighbors only
    friend class NavWorld;
	friend class NavThread;
	friend class TileContainerLoader;
	friend class TileContainerRepository;
	friend struct TileHandle;
	friend struct TileRef;
public:
	Chunk();
	~Chunk();

    // =========== Main methods  ===========

    void init(World& world, ChunkID chunkId, i32v2 worldPos);
	void allocateData();
	void freeData();
	void dispose();


    // =========== Accessors  ===========
    const i32v2 getWorldPos() const { return mAABB.pos; }
	const i32v2 getChunkOffset() const { return i32v2(mAABB.pos.x / CHUNK_WIDTH, mAABB.pos.y / CHUNK_WIDTH); }
    const i32v3 getWorldPos3D() const { return i32v3(mAABB.pos.x, mAABB.pos.y, 0); }
    i32v2 getWorldPosCenter2D() const { return i32v2(mAABB.pos.x + HALF_CHUNK_WIDTH, mAABB.pos.y + HALF_CHUNK_WIDTH); }
    i32v3 getWorldPosCenter3D() const { return i32v3(mAABB.pos.x + HALF_CHUNK_WIDTH, mAABB.pos.y + HALF_CHUNK_WIDTH, 0); }
	ChunkState getState() const { ASSERT_GAME_THREAD(); return mState; }
    const ChunkID& getChunkID() const { return mChunkId; }
	const HeightmapPatchID getHeightmapPatchID() const;
    const i32AABB3& getAABB() const { return mAABB; }
    i32v2 getTileWorldPos2D(TileIndex i) const {
        return i32v2(mAABB.pos.x + (i % CHUNK_WIDTH), mAABB.pos.y + i / CHUNK_WIDTH);
    }

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

    // =========== State  ===========
	bool isDeactivated() const { ASSERT_GAME_THREAD(); return mState == ChunkState::DEACTIVATED || mState == ChunkState::DESTROYING_ON_SIM; }
	bool isActivated() const { ASSERT_GAME_THREAD(); return mState == ChunkState::ACTIVATED; }

	void setState(ChunkState state) { mState = state; }
	
    void setGrassAt(const TileIndex index, TileGrassID grassId, ui8 density);
    void clearGrassAt(const TileIndex index);
	const TileGrass& getGrassAt(const TileIndex index) const { /*ASSERT_GAME_THREAD(); */return mGrass[index]; } // TODO: Game thread assert
	const ui8 getGrassDensityAt(const TileIndex index, TileGrassID grassId) const;
	// Return false if grass is not valid as chunk is destroying
	bool copyPaddedGrassDataWorkerThread(TileGrass outGrassData[PADDED_CHUNK_WIDTH][PADDED_CHUNK_WIDTH]) const;

    // =========== Terrain update  ===========
	// // TODO: This wasnt hooked up to anything, is it needed?5
	//void onTerrainDataChanged(const i32v2& editPosition, f32 editRadius);

    // =========== Tiles  ===========
	TileContainer* getTileContainer() { return mTileContainer; }
	const TileContainer* getTileContainer() const { return mTileContainer; }

    // =========== Ref counting  ===========
	// Try incref on another thread. Fails if chunk is being destroyed
	inline bool tryAquireThreadSafe() const {
		// Situations:
		// 1. Main thread about to destroy, we succeed lock, but fail aquire because refcount == 0
		// 2. Main thread about to create, we succeed lock, but fail because tileContainer is null
		std::shared_lock lock(mTileContainerLifetimeMutex);
		if (!mTileContainer) return false;
		return mTileContainer->tryAquireThreadSafe();
    }
    // Only game thread can incref but any thread can decref
	inline void incRef() const { ASSERT_GAME_THREAD(); assert(mTileContainer); mTileContainer->incRef(); }
	inline void decRef() const { assert(mTileContainer);  mTileContainer->decRef(); }
    ui32 getRefCount() const { ASSERT_GAME_THREAD(); return mTileContainer ? mTileContainer->getRefCount() : 0; }

	World& getWorld() const { return *mWorld; }

	EVENT_LISTENER_FUNCS(Chunk, GrassEdit, ChunkEventType::GrassEdit, const ChunkEvent&);

private:

    // =========== Members ===========
	ChunkID mChunkId;
	i32AABB3 mAABB = i32AABB3(0);
	std::atomic<ChunkState> mState = ChunkState::DEACTIVATED;
	BitFlags<ChunkFlags> mFlags;

	World* mWorld = nullptr;
	TileContainer* mTileContainer = nullptr;
	mutable std::shared_mutex mTileContainerLifetimeMutex;
    std::vector<TileGrass> mGrass; // Grass densities
    mutable std::shared_mutex mSharedGrassMutex;

	std::unique_ptr<ChunkTileContainersLookup> mTileContainersLookup;
	std::atomic<ui32> mTileContainersLookupVersion = 0;

	EVENT_DISPATCHER_DEF(Chunk);
};
#ifdef DEBUG // Release has different size
//static_assert(sizeof(Chunk) == 296, "These are permanently allocated, so keep small");
#endif
