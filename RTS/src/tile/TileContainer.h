#pragma once

#include "tile/Tile.h"

class TileContainer
{
    friend class TileRef;
    friend class TileHandle;
public:

    void init(ui32v3 rootPos, ui32v3 dims);
    void freeTiles();

    void updateMainThread();

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
    const Tile& getTileAt(ui32 x, ui32 y, ui32 z) const {
        const TileIndex i = getTileIndexFromXYZOffset(x, y, z);
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
    bool isDirtyMesh() const { return mDirtyMesh; }
    bool isDirtyNav() const { return mDirtyNav; }
    void setDirtyMesh(bool dirty) const { mDirtyMesh = dirty; }
    void setDirtyNav(bool dirty) const { mDirtyNav = dirty; }



    // =========== Accessors  ===========
    const ui32v2& getWorldPos2D() const { return reinterpret_cast<const ui32v2&>(mRootPos); }
    const ui32v3& getWorldPos3D() const { return mRootPos; }
    const ui32v3& getDims() const { return mDims; }

    ui32 getReadLockCount() const { return mReadLockCount; }
    ui32 getRefCount() const { return mRefCount; }

    const std::vector<Tile>& getTiles() const { return mTiles; }

private:
    std::vector<Tile> mTiles; // TODO: Memory recycler
    // All tiles that need to update when read lock is free
    std::vector<TileIndex> mTilesNeedingThreadSafeCopy;
    ui32v3 mDims;
    ui32v3 mRootPos;
    mutable std::atomic_uint32_t mReadLockCount = 0;
    mutable std::atomic_uint32_t mRefCount = 0;
    mutable bool mDirtyMesh = false;
    mutable bool mDirtyNav = false;
};

