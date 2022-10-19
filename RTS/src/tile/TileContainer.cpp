#include "stdafx.h"
#include "TileContainer.h"

#include "pathfinding/NavThread.h"

#include "rendering/mesh/Mesh.h"

#include "resources/TileRepository.h"
#include "world/IWorld.h"
#include "world/IChunkGrid.h"

std::vector<std::unique_ptr<TileContainer>> sTileContainers;
std::unordered_map<TileContainerID, TileContainer*> sTileContainerLookup;
TileContainerID sTileContainerIdGen = 0;


RUNTIME_INIT_FUNC(reserveTileContainerData) {
    sTileContainers.reserve(500);
    sTileContainerLookup.reserve(500);
}

TileContainer* TileContainerRepository::getNewTileContainer(const ui32v3& rootPos, const ui32v3& dims, ui32 floorHeight, bool isTerrain) {
    assert(IS_GAME_THREAD());
    std::unique_ptr<TileContainer> newContainer = std::make_unique<TileContainer>();
    TileContainer* rv = newContainer.get();
    newContainer->init(sTileContainerIdGen++, rootPos, dims, floorHeight, isTerrain);
    sTileContainerLookup[newContainer->mId] = rv;
    sTileContainers.push_back(std::move(newContainer));
    return rv;
}

void TileContainerRepository::destroyTileContainer(TileContainer* container) {
    assert(IS_GAME_THREAD());
    sTileContainerLookup.erase(container->mId);
    // TODO: Profile linear search
    for (size_t i = 0; i < sTileContainers.size(); ++i) {
        if (sTileContainers[i].get() == container) {
            // container->freeData();
            sTileContainers[i] = std::move(sTileContainers.back()); // TODO: We hit a crash here on destructor
            sTileContainers.pop_back(); 
            return;
        }
    }
    assert(false); // Not found
}



TileContainer* TileContainerRepository::getTileContainer(TileContainerID id) {
    assert(IS_GAME_THREAD() || IS_NAV_THREAD()); // Nav thread is allowed to access tile containers because the world is write locked during nav
    auto&& it = sTileContainerLookup.find(id);
    assert(it != sTileContainerLookup.end());
    return it->second;
}

std::vector<std::unique_ptr<TileContainer>>& TileContainerRepository::getTileContainers() {
    return sTileContainers;
}

void TileContainerRenderData::reset() {
    assert(IS_RENDER_THREAD());
    mHasMesh = false;
    mDirtyDynamicMesh = false;
}

void TileContainer::init(TileContainerID id, ui32v3 rootPos, ui32v3 dims, ui32 floorHeight, bool isTerrain) {
    mRootPos = rootPos;
    mDims = dims;
    mFloorHeight = floorHeight;
    mIsTerrain = isTerrain;
    mId = id;
}

void TileContainer::allocateData() {
    size_t numTiles = mDims.x * mDims.y * mDims.z;
    mTiles.resize(numTiles);
    mWalls.resize(numTiles);
    mFineNavData.resize(numTiles);
}

void TileContainer::freeData() {
    std::vector<Tile>().swap(mTiles);
    std::vector<TileWallContainer>().swap(mWalls);
    std::vector<DynamicTile>().swap(mDynamicTiles);
    std::vector<TileFineNavData>().swap(mFineNavData);
    mOwnedTiles.freeData();
}

void TileContainer::updateMainThread() {
    assert(IS_GAME_THREAD());
    if (mTilesNeedingThreadSafeCopy.size() && mReadLockCount == 0) {
        for (TileIndex& id : mTilesNeedingThreadSafeCopy) {
            mTiles[id].updateThreadSafeLayers();
            mWalls[id].copyThreadSafeData();
        }
        mTilesNeedingThreadSafeCopy.clear();
        // Thread data is now updated, mesh and everything are marked dirty
        mDirtyData = true;
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
    
    TileFlags newFlags = TileFlags(oldTile.tileFlags.getBits() | tile.tileFlags.getBits());
    oldTile = tile;
    oldTile.setTileFlags(newFlags, readLocked); // Union tile flags
    onTileChanged(i, readLocked);
}

bool TileContainer::canAddTileData(TileIndex i, const TileData& tileData) const
{
    return mTiles[i].canAddTileData(tileData);

}

void TileContainer::addTile(TileIndex i, const TileData& tileData)
{
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    tile.addTileData(tileData, readLocked);
    onTileChanged(i, readLocked);
}

bool TileContainer::tryAddTile(TileIndex i, const TileData& tileData)
{
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    if (!tile.canAddTileData(tileData)) {
        return false;
    }
    tile.addTileData(tileData, readLocked);
    onTileChanged(i, readLocked);
    return true;
}

void TileContainer::setTileLayer(TileIndex i, TileLayer layer, TileID id) {
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    tile.setTileLayer(layer, id, readLocked);
    onTileChanged(i, readLocked);
}

void TileContainer::setTileFlag(TileIndex i, TileFlags flag) {
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    tile.setTileFlag(flag, readLocked);
    onTileChanged(i, readLocked);
}

void TileContainer::setTileFlags(TileIndex i, TileFlags flags) {
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    tile.setTileFlags(flags, readLocked);
    onTileChanged(i, readLocked);
}

void TileContainer::clearTileFlag(TileIndex i, TileFlags flag) {
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    tile.clearTileFlag(flag, readLocked);
    onTileChanged(i, readLocked);
}

void TileContainer::clearTileFlags(TileIndex i) {
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    tile.clearTileFlags(readLocked);
    onTileChanged(i, readLocked);
}

void TileContainer::setTileGroundZPosition(TileIndex i, f32 groundZPosition) {
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    tile.setGroundZPosition(groundZPosition, readLocked);
    onTileChanged(i, readLocked);
}

void TileContainer::setTileOrientation(TileIndex i, Cartesian dir, TileLayer layer) {
    const bool readLocked = isReadLocked();
    Tile& tile = mTiles[i];
    tile.setOrientation(dir, layer, readLocked);
    onTileChanged(i, readLocked);
}

void TileContainer::setWallAt(TileIndex index, Cartesian dir, TileWall wall) {
    const bool readLocked = isReadLocked();
    TileWallContainer& tileWalls = mWalls[index];
    Tile& tile = mTiles[index];
    if (!readLocked) {
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
    onTileChanged(index, readLocked);
}

void TileContainer::setWallsAt(TileIndex index, TileWalls walls) {
    const bool readLocked = isReadLocked();
    TileWallContainer& tileWalls = mWalls[index];
    Tile& tile = mTiles[index];
    if (!readLocked) {
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
    onTileChanged(index, readLocked);
}

TileHandle TileContainer::tryGetTileHandleAtWorldPos(const i32v3& worldPos) const
{
    i32v3 offset = worldPos - mRootPos;
    if (offset.x < 0 || offset.y < 0 || offset.z < 0 || offset.x >= mDims.x || offset.y >= mDims.y || offset.z >= mDims.z * (i32)mFloorHeight) {
        return TileHandle();
    }
    // Scale to floor height
    offset.z /= (i32)mFloorHeight;
    return TileHandle(this, getTileIndexFromXYZOffset(ui32v3(offset)));
}

const bool TileContainer::isReadLocked() const {
    return mReadLockCount.load() > 0 || (Services::isUsingNav() && Services::NavThread::ref().isRunningPathfind());
}

void TileContainer::addEntrance(TileIndex pos, bool isLocked) {
    assert(IS_GAME_THREAD());
    mDirtyNav = true;
    auto&& newEntrance = mEntrances.emplace_back();
    newEntrance.tileIndex = pos;
    newEntrance.isLocked = true;
}

void TileContainer::removeEntrance(TileIndex pos)
{
    for (size_t i = 0; i < mEntrances.size(); ++i) {
        if (mEntrances[i].tileIndex == pos) {
            mEntrances[i] = mEntrances.back();
            mEntrances.pop_back();
        }
    }
    mDirtyNav = true;
}

void TileContainer::onTileChanged(TileIndex tileIndex, bool isReadLocked)
{
    Tile& tile = mTiles[tileIndex];
    if (isReadLocked) {
        if (!tile.isUpdateQueued()) {
            mTilesNeedingThreadSafeCopy.push_back(tileIndex);
            tile.tileFlags.setBit(TileFlags::TILE_FLAG_QUEUED_THREADSAFE_UPDATE);
        }
    }
    else {
        // When not locked we can immediately mark dirty
        mDirtyNav = true;
        mDirtyData = true;
    }


    // Potentially block or free terrain below
    // TODO: Proper intersection
    if (!mIsTerrain) {
        IChunkGrid& chunkGrid = sWorld->getChunkGrid();
        const i32v3 offset = getTileXYZOffsetWithZScale(tileIndex);
        if (offset.z == 0) {
            const i32v2 worldPos2D(mRootPos.x + offset.x, mRootPos.y + offset.y);
            Chunk& chunk = chunkGrid.getChunk(ChunkID::fromWorldI32v2(worldPos2D));
            if (chunk.isDataReady()) {
                TileContainer* chunkTileContainer = chunk.getTileContainer();
                assert(chunkTileContainer);
                TileIndex chunkTileIndex = chunkTileContainer->getTileIndexFromXYZOffset(worldPos2D.x - chunkTileContainer->getWorldPos2D().x, worldPos2D.y - chunkTileContainer->getWorldPos2D().y, 0);
                if (tile.isEmptyMainThread()) {
                    chunkTileContainer->clearTileFlag(chunkTileIndex, TileFlags::TILE_FLAG_IS_BLOCKED_BY_STRUCTURE);
                }
                else {
                    chunkTileContainer->setTileFlag(chunkTileIndex, TileFlags::TILE_FLAG_IS_BLOCKED_BY_STRUCTURE);
                }
            }
            else {
                assert(false && "Building on invalid chunk");
            }
        }
    }
}
//#include "debugging/DebugRenderer.h" // TODO: REMOVE
void TileContainer::addDoor(Cartesian doorSide, TileIndex tileIndex) {
    mDynamicTiles.emplace_back(DynamicTile{ tileIndex, {}/*flags*/, DynamicTileType(doorSide) });
    mRenderData.mDirtyDynamicMesh = true;
    if (!mIsTerrain) {
        IChunkGrid& chunkGrid = sWorld->getChunkGrid();
        const i32v3 offset = getTileXYZOffsetWithZScale(tileIndex);
        if (offset.z == 0) {
            const i32v2 worldPos2D(mRootPos.x + offset.x, mRootPos.y + offset.y);

            //DebugRenderer::drawFilledQuad(f32v3(worldPos2D.x, worldPos2D.y, 5.0f), f32v2(1.0f), COLOR_RED, 10000000);
            bool isExterior = false;
            TileFlags forceFlag;
            switch (doorSide) {
                case Cartesian::SOUTH:
                    isExterior = ((offset.y == 0) || !isTileOwned(tileIndex - mDims.x));
                    forceFlag = TileFlags::TILE_FLAG_FORCE_EXTERNAL_EDGE_NORTH;
                    break;
                case Cartesian::WEST:
                    isExterior = ((offset.x == 0) || !isTileOwned(tileIndex - 1));
                    forceFlag = TileFlags::TILE_FLAG_FORCE_EXTERNAL_EDGE_EAST;
                    break;
                case Cartesian::EAST:
                    isExterior = ((offset.x == mDims.x - 1) || !isTileOwned(tileIndex + 1));
                    forceFlag = TileFlags::TILE_FLAG_FORCE_EXTERNAL_EDGE_WEST;
                    break;
                case Cartesian::NORTH:
                    isExterior = ((offset.y == mDims.y - 1) || !isTileOwned(tileIndex + mDims.x));
                    forceFlag = TileFlags::TILE_FLAG_FORCE_EXTERNAL_EDGE_SOUTH;
                    break;
                default:
                    break;
            }

            // Only exterior doors create a forced navmesh connection
            if (isExterior) {
                const i32v2 chunkTilePos = CARTESIAN_NORMALS[e_cast(doorSide)] + worldPos2D;
                Chunk& chunk = chunkGrid.getChunk(ChunkID::fromWorldI32v2(chunkTilePos));
                if (chunk.isDataReady()) {
                    TileContainer* chunkTileContainer = chunk.getTileContainer();
                    assert(chunkTileContainer);
                    TileIndex chunkTileIndex = chunkTileContainer->getTileIndexFromXYZOffset(chunkTilePos.x - chunkTileContainer->getWorldPos2D().x, chunkTilePos.y - chunkTileContainer->getWorldPos2D().y, 0);
                    chunkTileContainer->setTileFlag(chunkTileIndex, forceFlag);
                   // DebugRenderer::drawFilledQuad(f32v3(chunkTileContainer->getTileXYZOffsetWithZScale(chunkTileIndex) + chunkTileContainer->getWorldPos3D()), f32v2(1.0f), COLOR_WHITE, 10000000);
                }
                else {
                    assert(false && "Building on invalid chunk");
                }
            }
        }
    }
}

void TileContainer::removeDoor(Cartesian doorSide, TileIndex tileIndex) {
    assert(false); // Implement removing the navnode edge
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
