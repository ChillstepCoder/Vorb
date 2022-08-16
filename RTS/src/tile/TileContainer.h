#pragma once

#include "tile/Tile.h"
#include "tile/TileHandle.h"
#include "util/BitArray.h"

class Mesh;

struct TileWallContainer {
    TileWalls walls; // Cartesian
    TileWalls wallsThreadSafe; // Cartesian

    void copyThreadSafeData() {
        wallsThreadSafe = walls;
    }
};
static_assert(sizeof(TileWallContainer) == 32, "Keep small");

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

struct TileContainerRenderData {
    TileContainerRenderData() = default;
    ~TileContainerRenderData();
    std::unique_ptr<Mesh> mStaticMesh = nullptr;
    std::unique_ptr<Mesh> mDynamicMesh = nullptr;
    bool mIsBuildingStaticMesh = false; // When true, we are waiting for our mesh to be completed
    bool mIsVisible = false;
    bool mDirtyStaticMesh = false;
    bool mDirtyDynamicMesh = false;

    void reset();
};

struct TileContainerEntranceEdge {
    ui16 adjacentIndex;
    ui16 distanceTiles;
};

struct TileContainerEntrance {
    mutable std::vector<TileContainerEntranceEdge> adjacentEntrances; // TODO: Smaller data structure
    TileIndex tileIndex;
    bool isLocked; // TODO: Access type enum?
};

enum class TileFineNavEdgeType {
    NONE = 0,
    DOWN = 1,
    UP = 2,
    EXTERIOR = 3,
};

struct TileFineNavData {

    void setCanAccessDirection(Cartesian8 dir8, bool canAccess) {
        const ui8 bitShift = e_cast(dir8);
        const ui8 bitMask = 1ui8 << bitShift;
        accessBits = (accessBits & (~bitMask)) | (canAccess << bitShift);
    }
    bool canAccessDirection(Cartesian8 dir8) const {
        return accessBits & 1ui8 << e_cast(dir8);
    }
    void setEdgeType(Cartesian dir, TileFineNavEdgeType edgeType) {
        const ui8 bitShift = e_cast(dir) * 2ui8;
        const ui8 bitMask = 0b11 << bitShift;
        edgeTypeCartesian = (edgeTypeCartesian & (~bitMask)) | (e_cast(edgeType) << bitShift);
    }
    TileFineNavEdgeType getEdgeType(Cartesian dir) const {
        const ui8 bitShift = e_cast(dir) * 2ui8;
        const ui8 bitMask = 0b11 << bitShift;
        return TileFineNavEdgeType((edgeTypeCartesian & bitMask) >> bitShift);
    }

    void reset() {
        accessBits = 0;
        edgeTypeCartesian = 0;
        pathWeight = 255;
    }

    ui8 accessBits = 0; // from diagonal left to diagonal up right
    ui8 edgeTypeCartesian = 0; // Each cartesian gets 2 bits 0 = flat, 1 = down, 2 = up, 3 = exterior
    ui8 pathWeight = 255;

};
static_assert(sizeof(TileFineNavData) == 3, "Keep tiny");

class TileContainer;
// Static class
class TileContainerRepository {
public:
    static TileContainer* getNewTileContainer(const ui32v3& rootPos, const ui32v3& dims, ui32 floorHeight, bool isTerrain);
    static void destroyTileContainer(TileContainer* container);
    
    static TileContainer* getTileContainer(TileContainerID id);

    static std::vector<std::unique_ptr<TileContainer>>& getTileContainers();
};

// TODO: Memory recycler?
class TileContainer
{
public:
    friend struct TileRef;
    friend struct TileHandle;
    friend class TileContainerRepository;
    friend class ChunkGenerator;
    friend class NavThread; // TODO: Too many friends?
    friend class NavWorld;
    friend class PathFinder;
    TileContainer() = default;
    ~TileContainer() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(TileContainer);

private:

    void init(TileContainerID id, ui32v3 rootPos, ui32v3 dims, ui32 floorHeight, bool isTerrain);
    void freeData();

public:
    void allocateData();

    void updateMainThread();
    void updateActiveDynamicTiles();

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
    void setTileGroundZPosition(TileIndex i, f32 groundZPosition);
    void setWallAt(TileIndex index, Cartesian dir, TileWall wall);
    void setWallsAt(TileIndex index, TileWalls walls);

    const TileWalls& getWallsMainThread(TileIndex i) const { return mWalls[i].walls; }
    const TileWalls& getWallsThreadSafe(TileIndex i) const { return mWalls[i].wallsThreadSafe; }
    const std::vector<TileWallContainer>& getTileWallContainers() const { return mWalls; }

    const std::vector<DynamicTile>& getDynamicTiles() const { return mDynamicTiles; }

    // This needs to be floor(f32v3worldPos)
    TileHandle tryGetTileHandleAtWorldPos(const i32v3& worldPos) const;

    // =========== Generation ===========
    void setTileFromGeneration(TileIndex i, Tile&& tile) {
        mTiles[i] = tile;
        updateTileCollisionAt(i, tile.layers[TILE_LAYER_TOP], false);
    }

    // =========== Collision ===========
    void updateTileCollisionAt(TileIndex i, TileID tileId, bool readLocked);

    // =========== Tile accessors  ===========
    Tile& getMutableTileAt(TileIndex i) {
        assert(IS_MAIN_THREAD());
        assert(i < mTiles.size());
        return mTiles[i];
    }

    const Tile& getTileAt(TileIndex i) const {
        assert(i < mTiles.size());
        return mTiles[i];
    }
    const Tile& getTileAt(ui32 offsetX, ui32 offsetY, ui32 offsetZ) const {
        const TileIndex i = getTileIndexFromXYZOffset(offsetX, offsetY, offsetZ);
        assert(i < mTiles.size());
        return mTiles[i];
    }

    const Tile& getTileAtNoAssert(TileIndex i) const {
        return mTiles[i];
    }
    ui32v3 getTileXYZOffsetWithZScale(TileIndex i) const {
        const ui32 layerSize = mDims.x * mDims.y;
        return ui32v3(i % mDims.x, (i % layerSize) / mDims.x, (i / layerSize) * mFloorHeight);
    }
    ui32v3 getTileXYZOffset(TileIndex i) const {
       const ui32 layerSize = mDims.x * mDims.y;
       return ui32v3(i % mDims.x, (i % layerSize) / mDims.x, i / layerSize);
    }
    ui32v2 getTileXYOffset(TileIndex i) const {
        const ui32 layerSize = mDims.x * mDims.y;
        return ui32v2(i % mDims.x, (i % layerSize) / mDims.x);
    }
    TileIndex getTileIndexFromXYZOffset(const ui32v3& xyz) const {
        return xyz.x + xyz.y * mDims.x + xyz.z * mDims.x * mDims.y;
    }
    TileIndex getTileIndexFromXYZOffset(ui32 x, ui32 y, ui32 z) const {
        return x + y * mDims.x + z * mDims.x * mDims.y;
    }
    TileContainerID getId() const { return mId; }
    bool isTerrain() const { return mIsTerrain; }

    // =========== Ownership  ===========
    bool isTileOwned(TileIndex index) const { return mOwnedTiles.getNumBits() == 0 || mOwnedTiles.getBit(index); }
    const BitArray& getOwnedTiles() const { return mOwnedTiles; }
    void allocateOwnedTiles() { mOwnedTiles.resizeAndZero(mDims.x * mDims.y * mDims.z); assert(!mOwnedTiles.isEmpty()); }
    void setOwnedTile(TileIndex index) { mOwnedTiles.setBit(index); }
    void clearOwnedTile(TileIndex index) { mOwnedTiles.clearBit(index); }
    void setOwnedTileTo(TileIndex index, bool isOwned) { mOwnedTiles.setBitTo(index, isOwned); }

    // =========== Thread safety and refcount  ===========
    void incReadLock() const { ++mReadLockCount; }
    void decReadLock() const { assert(mReadLockCount.load() > 0);  --mReadLockCount; }
    inline void incRef() const {
        assert(IS_MAIN_THREAD()); // Only main thread is allowed to incref
        assert(mRefCount.load() < 2000u); // This is probably a sign of something really awful
        ++mRefCount;
        if (mRefCount > 400) {
            std::cout << "DETECTED " << mRefCount << " REF COUNTS ON TILE CONTAINER " << std::endl;
            assert(false && "Too many container refcounts");
        }
    }
    inline void decRef() const {
        assert(mRefCount.load());
        --mRefCount;
    }
    void incReadLockAndRef() { incRef(); incReadLock(); }
    void decReadLockAndRef() {  decReadLock(); decRef(); }
    const bool isReadLocked() const;

    // =========== Dirty bits  ===========
    bool shouldBuildStaticMesh() const { return !isBuildingStaticMesh() && isDirtyStaticMesh(); }
    bool isBuildingStaticMesh() const { return mRenderData.mIsBuildingStaticMesh; }
    bool isDirtyStaticMesh() const { return mRenderData.mDirtyStaticMesh; }
    bool isDirtyDynamicMesh() const { return mRenderData.mDirtyDynamicMesh; }
    bool isDirtyNav() const { return mDirtyNav; }
    void setDirtyStaticMesh(bool dirty) const { mRenderData.mDirtyStaticMesh = dirty; }
    void setDirtyDynamicMesh(bool dirty) const { mRenderData.mDirtyDynamicMesh = dirty; }
    void setDirtyNav(bool dirty) const { mDirtyNav = dirty; }
    bool isNavMeshing() const { return mIsNavmeshing.load(/*memory order relaxed?*/); }
    bool shouldBuildNavMesh() const { return isDirtyNav() && !isNavMeshing(); }

    // =========== Accessors  ===========
    const i32v2& getWorldPos2D() const { return reinterpret_cast<const i32v2&>(mRootPos); }
    const i32v3& getWorldPos3D() const { return mRootPos; }
    const f32v3& getWorldPosCenter3D() const { return f32v3(mRootPos) + f32v3(mDims) * 0.5f; }
    const i32v3& getDims() const { return mDims; }
    f32 getFloorHeight() const { return mFloorHeight; }

    ui32 getReadLockCount() const { return mReadLockCount; }
    ui32 getRefCount() const { return mRefCount; }

    const std::vector<Tile>& getTiles() const { return mTiles; }
    const std::vector<TileWallContainer>& getWalls() const { return mWalls; }
    const std::vector<TileFineNavData>& getFineNavData() const { return mFineNavData; }

    // =========== Rendering  ===========
    bool isVisible() const { return mRenderData.mIsVisible; }
    TileContainerRenderData& getRenderData() const { return mRenderData; }

    const std::vector<TileContainerEntrance>& getEntrances() const { return mEntrances; }
    void addEntrance(TileIndex pos, bool isLocked);
    void removeEntrance(TileIndex pos);

private:
    void addDoor(Cartesian doorSide, TileIndex tileIndex);
    void removeDoor(Cartesian doorSide, TileIndex tileIndex);

    BitArray mOwnedTiles;
    // TODO: Can we use arrays instead of vectors to shrink these a bit?
    std::vector<Tile> mTiles; // TODO: Memory recycler and or compression
    std::vector<TileWallContainer> mWalls; // TODO: Memory recycler and or compression
    std::vector<DynamicTile> mDynamicTiles; // TODO: Memory recycler and or compression
    std::vector<ui16> mActiveDynamicTiles; // Iterate and update
    std::vector<TileFineNavData> mFineNavData;

    // All tiles that need to update when read lock is free
    std::vector<TileIndex> mTilesNeedingThreadSafeCopy;
    std::vector<TileContainerEntrance> mEntrances;
    std::vector<TileContainerEntrance> mEntrancesThreadSafeCopy;
    TileContainerID mId;
    i32v3 mDims;
    i32v3 mRootPos;
    ui32 mFloorHeight = 3u;
    mutable std::atomic_uint32_t mReadLockCount = 0u;
    mutable std::atomic_uint32_t mRefCount = 0u;

    mutable TileContainerRenderData mRenderData;
    std::atomic_bool mIsNavmeshing = false;
    mutable bool mDirtyNav = false;
    bool mIsTerrain = false;
};