#pragma once

#include "ChunkID.h"
#include "tile/TileHandle.h"
#include "tile/TileContainer.h"

#include "item/ItemStack.h"
#include "util/AABB.hpp"
#include "util/TinyThreadsafeVector.hpp"
#include "world/ChunkState.h"

#include "tile/TileGrass.h"

#include <shared_mutex>

class Chunk;
class BillboardMesh;
class TBOBillboardMesh;
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

enum class ChunkEventType {
	Ready,
    Destroy,
};

// TODO: Chunks and structures both have base class "TileContainer" ???
class Chunk {
	friend class IWorld;
	friend class IWorldGrid;
	friend class WorldEditorPanel;
	friend class ChunkGenerator;
	friend class ITileContainerMesher;
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
	void init(const ChunkID& chunkId, i32v2 worldPos);
	void allocateTileContainer();
	void freeData();
	void dispose();
	// TODO: REMOVE
	void updateMainThread();


    // =========== Accessors  ===========
    const i32v2 getWorldPos() const { return mAABB.pos; }
    const i32v3 getWorldPos3D() const { return i32v3(mAABB.pos.x, mAABB.pos.y, 0); }
    i32v2 getWorldPosCenter2D() const { return i32v2(mAABB.pos.x + HALF_CHUNK_WIDTH, mAABB.pos.y + HALF_CHUNK_WIDTH); }
    i32v3 getWorldPosCenter3D() const { return i32v3(mAABB.pos.x + HALF_CHUNK_WIDTH, mAABB.pos.y + HALF_CHUNK_WIDTH, 0); }
	ChunkState getState() const { return (ChunkState)mState.load(); }
    const ChunkID& getChunkID() const { return mChunkId; }
	const HeightmapPatchID getHeightmapPatchID() const { return HeightmapPatchID::fromWorldI32v2(mAABB.pos); }
    const i32AABB3& getAABB() const { return mAABB; }
	const std::vector<StructureID>& getStructures() const { return mStructures; }


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
	bool isDataReady() const { return mState == e_cast(ChunkState::READY); }

	void setState(ChunkState state) { mState = e_cast(state); }
	
    void setGrassAt(const TileIndex index, TileGrassID grassId, ui8 density);
    void clearGrassAt(const TileIndex index);
	const TileGrass& getGrassAt(const TileIndex index) const { /*assert(IS_GAME_THREAD()); */return mGrass[index]; } // TODO: Game thread assert
	const ui8 getGrassDensityAt(const TileIndex index, TileGrassID grassId) const;
	void copyPaddedGrassDataWorkerThread(TileGrass outGrassData[PADDED_CHUNK_WIDTH][PADDED_CHUNK_WIDTH]) const;
    //void bulkSetGrassAt(std::pair<TileIndex, ui8>* editData, size_t count); // TODO: THIS + EVENTS

    // =========== Terrain update  ===========
	void onTerrainDataChanged(const f32v2& editPosition, f32 editRadius);

    // =========== Tiles  ===========
	TileContainer* getTileContainer() { assert(IS_GAME_THREAD()); return mTileContainer; }
    const TileContainer* getTileContainer() const { return mTileContainer; }

    // =========== Ref counting  ===========
	// Only game thread can incref but any thread can decref
	inline void incRef() const { assert(IS_GAME_THREAD());  mTileContainer->incRef(); }
	inline void decRef() const { mTileContainer->decRef(); }
    ui32 getRefCount() const { return mTileContainer ? mTileContainer->getRefCount() : 0; }

	void addStructure(Structure* structure);

private:

    // =========== Members ===========
	ChunkID mChunkId;
	i32AABB3 mAABB = i32AABB3(0);
	// TODO: Not atomic
    std::atomic_uint8_t mState = (ui8)ChunkState::INVALID;
	BitFlags<ChunkFlags> mFlags;

	TileContainer* mTileContainer = nullptr;
    std::vector<TileGrass> mGrass; // Grass densities
    mutable std::shared_mutex mSharedGrassMutex;
	std::vector<StructureID> mStructures;
	std::map<TileIndex, ItemStack> mItemsOnGround;
};
#ifdef DEBUG // Release has different size
//static_assert(sizeof(Chunk) == 296, "These are permanently allocated, so keep small");
#endif
