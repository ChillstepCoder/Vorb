#include "stdafx.h"
#include "TileContainer.h"

#include "pathfinding/NavThread.h"

#include "rendering/mesh/Mesh.h"

#include "resources/TileRepository.h"



TileContainerRenderData::~TileContainerRenderData()
{

}

void TileContainerRenderData::reset() {
    mIsVisible = false;
    mStaticMesh.reset();
    mDynamicMesh.reset();
}

void TileContainer::init(ui32v3 rootPos, ui32v3 dims, ui32 floorHeight) {
    mRootPos = rootPos;
    mDims = dims;
    mFloorHeight = floorHeight;
    mTiles.resize(dims.x * dims.y * dims.z);
    mWalls.resize(dims.x * dims.y * dims.z);
}

void TileContainer::freeData() {
    std::vector<Tile>().swap(mTiles);
    std::vector<TileWallContainer>().swap(mWalls);
    std::vector<DynamicTile>().swap(mDynamicTiles);
}

void TileContainer::updateMainThread() {
    assert(IS_MAIN_THREAD());
    if (mTilesNeedingThreadSafeCopy.size() && mReadLockCount == 0) {
        for (TileIndex& id : mTilesNeedingThreadSafeCopy) {
            mTiles[id].updateThreadSafeLayers();
            mWalls[id].copyThreadSafeData();
        }
        mTilesNeedingThreadSafeCopy.clear();
        mRenderData.mDirtyStaticMesh = true;
        mDirtyNav = true; // TODO: Make this smarter
    }
}

void TileContainer::updateActiveDynamicTiles() {
    // Update dynamic objects
    for (size_t i = 0; i < mActiveDynamicTiles.size();) {
        auto&& index = mActiveDynamicTiles[i];
        // Update
        DynamicTile& dynamicTile = mDynamicTiles[index];
        if (!dynamicTile.mFlags.isBitSet(DynamicTileFlags::ACTIVE)) {
            // Deactivate
            mActiveDynamicTiles[i] = mActiveDynamicTiles.back();
            mActiveDynamicTiles.pop_back();
        }
        else {
            ++i;
        }
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
        mRenderData.mDirtyStaticMesh = true;
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
        mRenderData.mDirtyStaticMesh = true;
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
        mRenderData.mDirtyStaticMesh = true;
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
        mRenderData.mDirtyStaticMesh = true;
    }
}

void TileContainer::setWallAt(TileIndex index, Cartesian dir, TileWall wall) {
    const bool readLocked = isReadLocked();
    TileWallContainer& tileWalls = mWalls[index];
    Tile& tile = mTiles[index];
    if (readLocked) {
        if (!tile.isUpdateQueued()) {
            tile.tileFlags.setBit(TileFlags::TILE_FLAG_QUEUED_THREADSAFE_UPDATE);
            mTilesNeedingThreadSafeCopy.push_back(index);
        }
    }
    else {
        tileWalls.wallsThreadSafe.walls[e_cast(dir)] = wall;
    }
    // Check for removed or added door (Dynamic object)
    // Old door
    TileID oldId = tileWalls.walls.walls[e_cast(dir)].wallID;
    if (oldId != TILE_ID_NONE && TileRepository::getTileData(oldId).shape == TileShape::DOOR) {
        removeDoor(dir, index);
    }
    // New door
    TileID newId = wall.wallID;
    if (newId != TILE_ID_NONE && TileRepository::getTileData(newId).shape == TileShape::DOOR) {
        addDoor(dir, index);
    }
    tileWalls.walls.walls[e_cast(dir)] = wall;
}

void TileContainer::setWallsAt(TileIndex index, TileWalls walls) {
    const bool readLocked = isReadLocked();
    TileWallContainer& tileWalls = mWalls[index];
    Tile& tile = mTiles[index];
    if (readLocked) {
        if (!tile.isUpdateQueued()) {
            tile.tileFlags.setBit(TileFlags::TILE_FLAG_QUEUED_THREADSAFE_UPDATE);
            mTilesNeedingThreadSafeCopy.push_back(index);
        }
    }
    else {
        tileWalls.wallsThreadSafe = walls;
    }
    // Check for any removed or added doors (Dynamic objects)
    for (int i = 0; i < 4; ++i) {
        // Old door
        TileID oldId = tileWalls.walls.walls[i].wallID;
        if (oldId != TILE_ID_NONE && TileRepository::getTileData(oldId).shape == TileShape::DOOR) {
            removeDoor(Cartesian(i), index);
        }
        // New door
        TileID newId = walls.walls[i].wallID;
        if (newId != TILE_ID_NONE && TileRepository::getTileData(newId).shape == TileShape::DOOR) {
            addDoor(Cartesian(i), index);
        }
    }
    tileWalls.walls = walls;
  
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

void TileContainer::addDoor(Cartesian doorSide, TileIndex tileIndex) {
    mDynamicTiles.emplace_back(DynamicTile{ tileIndex, {}/*flags*/, DynamicTileType(doorSide) });
    mRenderData.mDirtyDynamicMesh = true;
}

void TileContainer::removeDoor(Cartesian doorSide, TileIndex tileIndex) {
    mRenderData.mDirtyDynamicMesh = true;
    for (size_t i = 0; i < mDynamicTiles.size(); ++i) {
        if (mDynamicTiles[i].mTileIndex == tileIndex && mDynamicTiles[i].mType == e_cast(doorSide)) {
            mDynamicTiles[i] = mDynamicTiles.back();
            mDynamicTiles.pop_back();
            return;
        }
    }
    assert(false && "Couldn't find door in dynamic array");
}
