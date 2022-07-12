#pragma once

#include "tile/Tile.h"

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

// TODO: Memory recycler?
class TileContainer
{
    friend struct TileRef;
    friend struct TileHandle;
public:
    TileContainer() = default;
    ~TileContainer() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(TileContainer);

    void init(ui32v3 rootPos, ui32v3 dims, ui32 floorHeight);
    void freeData();

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
    void setTilePathWeight(TileIndex i, ui8 weight);
    void setTileGroundZPosition(TileIndex i, f32 groundZPosition);
    void setWallAt(TileIndex index, Cartesian dir, TileWall wall);
    void setWallsAt(TileIndex index, TileWalls walls);

    const TileWalls& getWallsMainThread(TileIndex i) const { return mWalls[i].walls; }
    const TileWalls& getWallsThreadSafe(TileIndex i) const { return mWalls[i].wallsThreadSafe; }

    const std::vector<DynamicTile>& getDynamicTiles() const { return mDynamicTiles; }

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


    // =========== Thread safety  ===========
    void incReadLock() const { ++mReadLockCount; }
    void decReadLock() const { assert(mReadLockCount.load() > 0);  --mReadLockCount; }
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

    // =========== Accessors  ===========
    const ui32v2& getWorldPos2D() const { return reinterpret_cast<const ui32v2&>(mRootPos); }
    const ui32v3& getWorldPos3D() const { return mRootPos; }
    const ui32v3& getDims() const { return mDims; }
    f32 getFloorHeight() const { return mFloorHeight; }

    ui32 getReadLockCount() const { return mReadLockCount; }
    ui32 getRefCount() const { return mRefCount; }

    const std::vector<Tile>& getTiles() const { return mTiles; }
    const std::vector<TileWallContainer>& getWalls() const { return mWalls; }

    // =========== Rendering  ===========
    bool isVisible() const { return mRenderData.mIsVisible; }
    TileContainerRenderData& getRenderData() const { return mRenderData; }

private:
    void addDoor(Cartesian doorSide, TileIndex tileIndex);
    void removeDoor(Cartesian doorSide, TileIndex tileIndex);

    std::vector<Tile> mTiles; // TODO: Memory recycler and or compression
    std::vector<TileWallContainer> mWalls; // TODO: Memory recycler and or compression
    std::vector<DynamicTile> mDynamicTiles; // TODO: Memory recycler and or compression
    std::vector<ui16> mActiveDynamicTiles; // Iterate and update
    // All tiles that need to update when read lock is free
    std::vector<TileIndex> mTilesNeedingThreadSafeCopy;
    ui32v3 mDims;
    ui32v3 mRootPos;
    ui32 mFloorHeight = 3u;
    mutable std::atomic_uint32_t mReadLockCount = 0u;
    mutable std::atomic_uint32_t mRefCount = 0u;

    mutable TileContainerRenderData mRenderData;
    mutable bool mDirtyNav = false;
};