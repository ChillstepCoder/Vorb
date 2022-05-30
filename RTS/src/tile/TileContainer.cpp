#include "stdafx.h"
#include "TileContainer.h"

#include "pathfinding/NavThread.h"

void TileContainer::init(ui32v3 rootPos, ui32v3 dims, ui32 floorHeight) {
    mRootPos = rootPos;
    mDims = dims;
    mFloorHeight = floorHeight;
    mTiles.resize(dims.x * dims.y * dims.z);
}

void TileContainer::freeTiles() {
    std::vector<Tile>().swap(mTiles);
}

void TileContainer::updateMainThread() {
    assert(IS_MAIN_THREAD());
    if (mTilesNeedingThreadSafeCopy.size() && mReadLockCount == 0) {
        for (TileIndex& id : mTilesNeedingThreadSafeCopy) {
            mTiles[id].updateThreadSafeLayers();
        }
        mTilesNeedingThreadSafeCopy.clear();
        mDirtyMesh = true;
        mDirtyNav = true; // TODO: Make this smarter
    }
}

void TileContainer::setTileAt(TileIndex i, Tile tile) {
    assert(i < CHUNK_SIZE);
    const bool readLocked = isReadLocked();
    Tile& oldTile = mTiles[i];
    if (readLocked && !oldTile.isUpdateQueued()) {
        mTilesNeedingThreadSafeCopy.push_back(i);
    }
    TileFlags newFlags = TileFlags(oldTile.tileFlags.getBits() | tile.tileFlags.getBits());
    oldTile = tile;
    oldTile.setTileFlags(newFlags, readLocked); // Union tile flags
    // Update collision
    updateTileCollisionAt(i, tile.topLayer, readLocked);
    // Only dirty nav graph and mesh if we actually updated data
    if (!readLocked) {
        mDirtyNav = true;
        mDirtyMesh = true;
    }
}

bool TileContainer::canAddTile(TileIndex i, const TileData& tileData) const
{
    return mTiles[i].canAddTile(tileData);

}

void TileContainer::addTile(TileIndex i, const TileData& tileData)
{
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    if (readLocked && !tile.isUpdateQueued()) {
        mTilesNeedingThreadSafeCopy.push_back(i);
    }
    tile.addTile(tileData, readLocked);
    if (!readLocked) {
        mDirtyMesh = true;
        if (tileData.layer != TILE_LAYER_MID) {
            mDirtyNav = true;
        }
    }
}

bool TileContainer::tryAddTile(TileIndex i, const TileData& tileData)
{
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    if (readLocked && !tile.isUpdateQueued()) {
        bool success = tile.tryAddTile(tileData, readLocked);
        if (success) {
            mTilesNeedingThreadSafeCopy.push_back(i);
        }
        return success;
    }
    else {
        return tile.tryAddTile(tileData, readLocked);
    }
}

void TileContainer::setTileLayer(TileIndex i, TileLayer layer, TileID id) {
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    if (readLocked && !tile.isUpdateQueued()) {
        mTilesNeedingThreadSafeCopy.push_back(i);
    }
    tile.setTileLayer(layer, id, readLocked);
    // Only top has collision
    if (layer == TileLayer::Top) {
        // TODO: Use proper floor for this
        updateTileCollisionAt(i, tile.topLayer, readLocked);
    }
    if (layer != TileLayer::Mid) {
        // Top and bottom can change nav graph
        // TODO: Make this smarter
        if (!readLocked) {
            mDirtyNav = true;
        }
    }
    if (!readLocked) {
        mDirtyMesh = true;
    }
}

void TileContainer::setTileFlag(TileIndex i, TileFlags flag) {
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    if (readLocked && !tile.isUpdateQueued()) {
        mTilesNeedingThreadSafeCopy.push_back(i);
    }
    tile.setTileFlag(flag, readLocked);
}

void TileContainer::setTileFlags(TileIndex i, TileFlags flags) {
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    if (readLocked && !tile.isUpdateQueued()) {
        mTilesNeedingThreadSafeCopy.push_back(i);
    }
    tile.setTileFlags(flags, readLocked);
}

void TileContainer::clearTileFlag(TileIndex i, TileFlags flag) {
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    if (readLocked && !tile.isUpdateQueued()) {
        mTilesNeedingThreadSafeCopy.push_back(i);
    }
    tile.clearTileFlag(flag, readLocked);
}

void TileContainer::clearTileFlags(TileIndex i) {
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    if (readLocked && !tile.isUpdateQueued()) {
        mTilesNeedingThreadSafeCopy.push_back(i);
    }
    tile.clearTileFlags(readLocked);
}

void TileContainer::clearTileCollisionFlags(TileIndex i) {
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    if (readLocked && !tile.isUpdateQueued()) {
        mTilesNeedingThreadSafeCopy.push_back(i);
    }
    tile.clearTileCollisionFlags(readLocked);
}

void TileContainer::setTilePathWeight(TileIndex i, ui8 weight) {
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    if (readLocked && !tile.isUpdateQueued()) {
        mTilesNeedingThreadSafeCopy.push_back(i);
    }
    tile.setPathWeight(weight, readLocked);
}

void TileContainer::setTileGroundZPosition(TileIndex i, f32 groundZPosition) {
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    if (readLocked && !tile.isUpdateQueued()) {
        mTilesNeedingThreadSafeCopy.push_back(i);
    }
    tile.setGroundZPosition(groundZPosition, readLocked);
    if (!readLocked) {
        mDirtyMesh = true;
    }
}

void TileContainer::updateTileCollisionAt(TileIndex i, TileID tileId, bool readLocked) {
    Tile& tile = mTiles[i];
    if (readLocked && !tile.isUpdateQueued()) {
        mTilesNeedingThreadSafeCopy.push_back(i);
    }
    tile.updateCollision(readLocked);
}

const bool TileContainer::isReadLocked() const {
    return mReadLockCount.load() > 0 || Services::NavThread::ref().isRunningPathfind();
}
