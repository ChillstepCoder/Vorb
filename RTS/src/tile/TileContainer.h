#pragma once

#include "tile/TileContainerConst.h"
#include "tile/TileHandle.h"
#include "util/BitArray.h"
#include "tile/TileContainerEvents.h"
#include "tile/TileContainerHarvestableRegistry.h"
#include "tile/TileSpatialGrid.h"
#include "tile/TileWallContainer.h"
#include "tile/TileDamageData.h"
#include "visibility/TileVisibilityContainer.h"

class Chunk;
class Building;
class World;

enum DynamicTileType : ui8 {
    // Walls (Keep first)
    WALL_SOUTH = e_cast(Cartesian::SOUTH),
    WALL_WEST = e_cast(Cartesian::WEST),
    WALL_EAST = e_cast(Cartesian::EAST),
    WALL_NORTH = e_cast(Cartesian::NORTH),
    WALL_TERM = WALL_NORTH,
    // Layers
    TILE_GROUND,
    TILE_MID,
    TILE_TOP
};
enum DynamicTileFlags : ui8 {
    ACTIVE = 1 << 0,
};

struct DynamicTile {
    TileIndex mTileIndex;
    BitFlags<DynamicTileFlags> mFlags;
    DynamicTileType mType;
};
static_assert(sizeof(DynamicTile) == 8, "Keep small");

// TODO: Memory recycler?
class TileContainer
{
public:
    friend struct TileRef;
    friend struct TileHandle;
    friend class TileContainerRepository;
    friend class TileContainerLoader;
    friend class ChunkGenerator;
    friend class BuildingBuilder; // ONLY FOR DEBUG GENERATION
    friend class NavThread; // TODO: Too many friends?
    friend class NavWorld; // TODO: Remove
    friend class IChunkGrid;
    friend class PathFinder;
    TileContainer(World& world);
    ~TileContainer();
    VORB_NON_COPYABLE_BUT_MOVABLE(TileContainer);

private:

    void init(TileContainerID id, ui32v3 rootPos, ui32v3 dims, ui32 floorHeight, VarTileContainerOwner owner);
    void freeData();

public:
    void allocateData();
    void updateActiveDynamicTiles();

    // =========== Tile mutators ===========
    bool canAddTileData(TileIndex i, const TileDef& tileData) const;
    void setTileLayer(TileIndex i, const TileDef& tileData);
    bool tryAddTileLayer(TileIndex i, const TileDef& tileData);
    void setTileLayer(TileIndex i, TileLayer layer, TileID id, ui8 variant);

    void setTileFlag(TileIndex i, TileFlags flag);
    void overwriteTileFlags(TileIndex i, TileFlags flags);
    void clearTileFlag(TileIndex i, TileFlags flag);
    void clearTileFlags(TileIndex i);
    void setTileGroundZPosition(TileIndex i, f32 groundZPosition);
    void bulkSetTileGroundZPosition(std::pair<TileIndex, f32>* editData, size_t count);
    void setTileOrientation(TileIndex i, Cartesian dir, TileLayer layer);
    void setWallAt(TileIndex index, Cartesian dir, TileWall wall);
    void setWallsAt(TileIndex index, TileWall walls[4]);

    // Returns true if tile was destroyed by this adjust (i.e. health becomes <= 0)
    bool adjustTileHealth(TileIndex index, TileLayer layer, int healthAdjust, f32v3 impactPosition, f32v3 impactNormal);

    const std::vector<DynamicTile>& getDynamicTiles() const { return mDynamicTiles; }
    const TileContainerHarvestableRegistry& getHarvestables() const { return mHarvestableRegistry; }

    // Tile indexing
    const TileSpatialGrid& getTileSpatialGrid() const { return mTileSpatialGrid; }
    i32v3 getWorldPos() const { return mTileSpatialGrid.getWorldPos(); }
    i32v3 getDims() const { return mTileSpatialGrid.getDims(); }

    f32v3 getTileCenterWorldPosition(TileIndex i) const {
        return mTileSpatialGrid.getTileCenterWorldPos3D(i, mTiles[i].groundZOffset);
    }

    // This needs to be floor(f32v3worldPos)
    TileHandle tryGetTileHandleAtWorldPos(const i32v3& worldPos) const;

    // =========== Tile accessors  ===========
    Tile& getMutableTileAt(TileIndex i) {
        ASSERT_GAME_THREAD();
        assert(i < mTiles.size());
        return mTiles[i];
    }
    const Tile& getTileAt(TileIndex i) const {
        assert(i < mTiles.size());
        return mTiles[i];
    }
    const Tile& getTileAt(ui32 offsetX, ui32 offsetY, ui32 offsetZ) const;
    const Tile& getTileAtNoAssert(TileIndex i) const { return mTiles[i]; }

    TileContainerID getId() const { return mId; }

    SubchunkIndex getSubchunkIndexFromTileIndex(TileIndex tileIndex) const {
        const i32v3 offset = mTileSpatialGrid.getTileXYZOffset(tileIndex);
        const i32v3 scoffset = offset / SUBCHUNK_WIDTH;
        const i32v3 subchunkDims = mTileSpatialGrid.getDims() / SUBCHUNK_WIDTH;
        return scoffset.z * subchunkDims.x * subchunkDims.y + scoffset.y * subchunkDims.x + scoffset.x;
    }

    // =========== State  ===========
    bool isReady() const { return mState == e_cast(TileContainerState::READY); }
    TileContainerState getState() const { return (TileContainerState)mState.load(); }
    void setState(TileContainerState state) const { mState = e_cast(state); }

    // =========== Ownership  ===========
    TileContainerOwnerType getOwnerType() const { return mOwnerType; }
    bool isTerrain() const { return mOwnerType == TileContainerOwnerType::CHUNK; }
    const VarTileContainerOwner& getOwnerVariant() const { return mOwner; }
    Chunk* getOwnerChunk() const;
    Building* getOwnerBuilding() const;

    bool isTileValid(TileIndex index) const { ASSERT_GAME_THREAD(); return !mTiles[index].isEmpty(); }
    bool isTileOwned(TileIndex index) const;
    static bool isTileOwned(const BitArray& ownedDTiles, TileIndex index2d, ui32v2 containerDimsDTiles);


    // =========== Refcount  ===========
    // Threads cannot incref while container is ready to destroy
    bool tryAquireThreadSafe() const {
        assert(!IS_GAME_THREAD());
        // When refcount is 0, main thread is about to destroy this container
        std::lock_guard lock(mLifetimeMutex);
        if (mRefCount) {
            // It is possible another thread decrefs here, resulting in a brief period of 0 ref,
            // in which the main thread can then decide to destroy. Hence the lock being needed.
            // With this lock, the main thread will block until we have increffed again, and we won't
            // have a race condition
            ++mRefCount;
             return true;
        }
        return false;
    }
    inline void incRef() const {
        ASSERT_GAME_THREAD(); // Only main thread is allowed to incref
        ++mRefCount;
        if (mRefCount > 2000u) { // This is probably a sign of something really awful
            LOG_CRITICAL("DETECTED {} REF COUNTS ON TILE CONTAINER", mRefCount.load());
            assert(false && "Too many container refcounts");
        }
    }
    inline void decRef() const {
        assert(mRefCount.load());
        --mRefCount;
    }
    ui32 getRefCount() const { return mRefCount; }

    bool didInitMeshPhysicsAndNav() const { return mDidInitNav && mDidInitMesh && mDidInitPhysics && mDidInitVisibility; }
    bool didInitMeshPhysics() const { return mDidInitMesh && mDidInitPhysics && mDidInitVisibility; }
    void setDidInitMesh() const { mDidInitMesh = true; }
    void setDidInitPhysics() const { mDidInitPhysics = true; }
    void setDidInitNav() const { mDidInitNav = true; }
    void setDidInitVisibility() const { mDidInitVisibility = true; }

    // =========== Dirty bits  ===========
    bool isDirtyData() const { return mDirtyData; }
    void setDirtyData() { mDirtyData = true; }
    void clearDirtyData() { mDirtyData = false; }

    // =========== Accessors  ===========
    const std::vector<Tile>& getTiles() const { return mTiles; }
    const TileWallContainer& getTileWallContainer() const { return  mTileWallsContainer; }
    size_t getNumTiles() const { return mTiles.size(); }
    TileContainerHarvestableRegistry& getHarvestableRegistry() { return mHarvestableRegistry; }

    bool isPendingDestroy() const { return mPendingDestroy; }

    void copyDataWorkerThread(OUT ContainerMeshDataCopy& dataCopy) const;
    void copyDataWorkerThread(OUT ContainerNavDataCopy& dataCopy) const;

    EVENT_LISTENER_FUNCS(TileContainer, EditTiles, TileContainerEventType::EditTiles, const TileContainerEvent&);
    EVENT_LISTENER_FUNCS(TileContainer, TileDamaged, TileContainerEventType::TileDamaged, const TileContainerEvent&);
    EVENT_LISTENER_FUNCS(TileContainer, TileDestroyed, TileContainerEventType::TileDestroyed, const TileContainerEvent&);
    EVENT_LISTENER_FUNCS(TileContainer, Destroy, TileContainerEventType::Destroy, const TileContainerEvent&);

    // =========== World  ===========
    World& getWorld() const { return mWorld; }

private:

    void onTileChanged(TileIndex tileIndex);

    void addDoor(Cartesian doorSide, TileIndex tileIndex);
    void removeDoor(Cartesian doorSide, TileIndex tileIndex);

    // Indexing
    TileSpatialGrid mTileSpatialGrid;

    mutable std::shared_mutex mSharedMutex;
    mutable std::mutex mLifetimeMutex;

    // Tile data
    std::vector<Tile> mTiles; // TODO: Memory recycler and or compression
    TileWallContainer mTileWallsContainer; // TODO: Pointer so we remove from chunk

    // Visibility
    mutable std::shared_mutex mVisibilityMutex;
    TileVisibilityContainer mTileVisibilityContainer;

    // Dynamic tiles
    std::vector<DynamicTile> mDynamicTiles; // TODO: Memory recycler and or compression
    std::vector<ui16> mActiveDynamicTiles; // Iterate and update

    FlatMap<TileIndex, TileDamageDataPtr> mDamagedTiles;

    TileContainerHarvestableRegistry mHarvestableRegistry;
    TileContainerID mId;
    mutable std::atomic_uint32_t mRefCount = 0u;
    mutable std::atomic_bool mDidInitMesh = false;
    mutable std::atomic_bool mDidInitPhysics = false;
    mutable std::atomic_bool mDidInitNav = false;
    mutable std::atomic_bool mDidInitVisibility = false;

    mutable std::atomic_uint8_t mState = e_cast(TileContainerState::LOADING);
    bool mDirtyData = false;
    bool mPendingDestroy = false;
    mutable bool mIsGeneratingNavmesh = false; // ONLY SET IN NAV THREAD
    std::variant<Chunk*, Building*> mOwner;
    TileContainerOwnerType mOwnerType = TileContainerOwnerType::COUNT;
    World& mWorld;

    EVENT_DISPATCHER_DEF(TileContainer);
};

class TileContainerRef {
public:
    TileContainerRef(TileContainer& container) : container(container) {
        container.incRef();
    }
    ~TileContainerRef() {
        container.decRef();
    }

    TileContainer& container;
};