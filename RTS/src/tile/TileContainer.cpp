#include "stdafx.h"
#include "TileContainer.h"

#include "pathfinding/NavThread.h"

#include "rendering/mesh/Mesh.h"
#include "rendering/RenderThreadTasks.h"

#include "resources/TileRepository.h"
#include "world/IWorld.h"
#include "world/IChunkGrid.h"

#include "physics/PhysicsWorld.h"

// TODO: The vector is pointless, every container is a cache miss anyways
std::vector<std::unique_ptr<TileContainer>> sTileContainers;
std::unordered_map<TileContainerID, TileContainer*> sTileContainerLookup;
TileContainerID sTileContainerIdGen = 0;

RUNTIME_INIT_FUNC(reserveTileContainerData) {
    sTileContainers.reserve(500);
    sTileContainerLookup.reserve(500);
}

TileContainer* TileContainerRepository::getNewTileContainer(const ui32v3& rootPos, const ui32v3& dims, ui32 floorHeight, VarTileContainerOwner owner) {
    assert(IS_GAME_THREAD());
    std::unique_ptr<TileContainer> newContainer = std::make_unique<TileContainer>();
    TileContainer* rv = newContainer.get();
    newContainer->init(sTileContainerIdGen++, rootPos, dims, floorHeight, owner);
    // Clamp to int to prevent issues with PhysicsWorld storing these as signed integers
    if (sTileContainerIdGen > INT32_MAX) {
        sTileContainerIdGen = 0;
    }
    sTileContainerLookup[newContainer->mId] = rv;
    sTileContainers.push_back(std::move(newContainer));
    return rv;
}

void TileContainerRepository::destroyTileContainer(TileContainer* container) {
    // TODO: Maybe just dont destroy this on the game thread
    assert(IS_GAME_THREAD() || IS_SHUTTING_DOWN);
    sTileContainerLookup.erase(container->mId);

    assert(container->mRefCount == 0);
    // TODO: Profile linear search
    for (size_t i = 0; i < sTileContainers.size(); ++i) {
        if (sTileContainers[i].get() == container) {
            // container->freeData();
            TileContainerEvent event{ container, {} };
            TileContainerRepository::dispatchDestroy(event);
            sTileContainers[i] = std::move(sTileContainers.back()); // TODO: We hit a crash here on destructor
            sTileContainers.pop_back(); 
            return;
        }
    }
}

TileContainer* TileContainerRepository::getTileContainer(TileContainerID id) {
    assert(IS_GAME_THREAD());
    auto&& it = sTileContainerLookup.find(id);
    assert(it != sTileContainerLookup.end());
    return it->second;
}

TileContainer* TileContainerRepository::tryGetTileContainer(TileContainerID id) {
    auto&& it = sTileContainerLookup.find(id);
    if (it == sTileContainerLookup.end()) {
        return nullptr;
    }
    return it->second;
}

std::vector<std::unique_ptr<TileContainer>>& TileContainerRepository::getTileContainers() {
    return sTileContainers;
}

TileContainer::~TileContainer() {

}

void TileContainer::init(TileContainerID id, ui32v3 rootPos, ui32v3 dims, ui32 floorHeight, VarTileContainerOwner owner) {
   
    mRootPos = rootPos;
    mDims = dims;
    mFloorHeight = floorHeight;
    mId = id;
    mOwner = owner;
    if (std::holds_alternative<Chunk*>(owner)) {
        mOwnerType = TileContainerOwnerType::CHUNK;
        assert(dims.x == CHUNK_WIDTH && dims.y == CHUNK_WIDTH && dims.z == 1u);
    }
    else if (std::holds_alternative<Building*>(owner)) {
        mOwnerType = TileContainerOwnerType::BUILDING;
        assert(dims.x < CHUNK_WIDTH && dims.y < CHUNK_WIDTH);
    }
    else {
        assert(false && "invalid tile container owner");
    }
    static_assert(std::variant_size_v<VarTileContainerOwner> == 2);
    static_assert(e_cast(TileContainerOwnerType::COUNT) == 2);
}

void TileContainer::allocateData() {
    size_t numTiles = mDims.x * mDims.y * mDims.z;
    mTiles.resize(numTiles);
    mWalls.resize(numTiles);
}

void TileContainer::freeData() {
    assert(IS_GAME_THREAD());
    std::vector<Tile>().swap(mTiles);
    std::vector<TileWalls>().swap(mWalls);
    std::vector<DynamicTile>().swap(mDynamicTiles);
    mOwnedTiles.freeData();
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

bool TileContainer::canAddTileData(TileIndex i, const TileData& tileData) const
{
    return mTiles[i].canAddTileData(tileData);
}

void TileContainer::setTileLayer(TileIndex i, const TileData& tileData) {
    assert(isReady());
    setTileLayer(i, (TileLayer)tileData.layer, tileData.id);
}

bool TileContainer::tryAddTileLayer(TileIndex i, const TileData& tileData) {
    assert(isReady());
    Tile& tile = mTiles[i];
    if (!tile.canAddTileData(tileData)) {
        return false;
    }
    setTileLayer(i, (TileLayer)tileData.layer, tileData.id);
    return true;
}

void TileContainer::setTileLayer(TileIndex i, TileLayer layer, TileID id) {
    assert(isReady());
    Tile& tile = mTiles[i];
    TileID prevId = tile.layers[e_cast(layer)];
    if (prevId == id) {
        return;
    }
    // Build notify
    //LOG_TRACE("Sending edit = TileIndex {} Layer {} id {} prevId {}", i, e_cast(layer), id, tile.layers[e_cast(layer)]);
    TileContainerEvent evnt;
    evnt.container = this;
    evnt.edit.worldPosition = getTileCenterWorldPosition(i);
    evnt.edit.type = TileContainerEditEventType::ChangeLayer;
    evnt.edit.tileIndex = i;
    evnt.edit.changeLayer.prevId = prevId;
    evnt.edit.changeLayer.newId = id;
    evnt.edit.changeLayer.layer = layer;
    // Edit
    {
        std::lock_guard lock(mSharedMutex);
        tile.layers[e_cast(layer)] = id;
    }
    // Dispatch notify
    TileContainerRepository::dispatchEditTile(evnt);

    onTileChanged(i);
}

void TileContainer::setTileFlag(TileIndex i, TileFlags flag) {
    assert(isReady());
    Tile& tile = mTiles[i];
    // Build notify
    TileContainerEvent evnt;
    evnt.edit.changeFlags.prevFlags = tile.tileFlags;
    {
        std::lock_guard lock(mSharedMutex);
        tile.setTileFlag(flag);
    }
    evnt.edit.changeFlags.newFlags = tile.tileFlags;

    // Dispatch notify if changed
    if (evnt.edit.changeFlags.prevFlags.getBits() != evnt.edit.changeFlags.newFlags.getBits()) {
        evnt.container = this;
        evnt.edit.worldPosition = getTileCenterWorldPosition(i);
        evnt.edit.type = TileContainerEditEventType::ChangeFlags;
        evnt.edit.tileIndex = i;
        TileContainerRepository::dispatchEditTile(evnt);
        onTileChanged(i);
    }
}

void TileContainer::setTileFlags(TileIndex i, TileFlags flags) {
    assert(isReady());
    Tile& tile = mTiles[i];

    // Build notify
    TileContainerEvent evnt;
    evnt.edit.changeFlags.prevFlags = tile.tileFlags;
    {
        std::lock_guard lock(mSharedMutex);
        tile.setTileFlags(flags);
    }
    evnt.edit.changeFlags.newFlags = tile.tileFlags;
    if (evnt.edit.changeFlags.prevFlags.getBits() != evnt.edit.changeFlags.newFlags.getBits()) {
        evnt.container = this;
        evnt.edit.worldPosition = getTileCenterWorldPosition(i);
        evnt.edit.type = TileContainerEditEventType::ChangeFlags;
        evnt.edit.tileIndex = i;
        TileContainerRepository::dispatchEditTile(evnt);
        onTileChanged(i);
    }
}

void TileContainer::clearTileFlag(TileIndex i, TileFlags flag) {
    assert(isReady());
    Tile& tile = mTiles[i];

    // Build notify
    TileContainerEvent evnt;
    evnt.edit.changeFlags.prevFlags = tile.tileFlags;
    {
        std::lock_guard lock(mSharedMutex);
        tile.clearTileFlag(flag);
    }
    evnt.edit.changeFlags.newFlags = tile.tileFlags;
    if (evnt.edit.changeFlags.prevFlags.getBits() != evnt.edit.changeFlags.newFlags.getBits()) {
        evnt.container = this;
        evnt.edit.worldPosition = getTileCenterWorldPosition(i);
        evnt.edit.type = TileContainerEditEventType::ChangeFlags;
        evnt.edit.tileIndex = i;
        TileContainerRepository::dispatchEditTile(evnt);
        onTileChanged(i);
    }
}

void TileContainer::clearTileFlags(TileIndex i) {
    assert(isReady());
    Tile& tile = mTiles[i];

    // Build notify
    TileContainerEvent evnt;
    evnt.edit.changeFlags.prevFlags = tile.tileFlags;
    {
        std::lock_guard lock(mSharedMutex);
        tile.clearTileFlags();
    }
    evnt.edit.changeFlags.newFlags = tile.tileFlags;
    if (evnt.edit.changeFlags.prevFlags.getBits() != evnt.edit.changeFlags.newFlags.getBits()) {
        evnt.container = this;
        evnt.edit.worldPosition = getTileCenterWorldPosition(i);
        evnt.edit.type = TileContainerEditEventType::ChangeFlags;
        evnt.edit.tileIndex = i;
        TileContainerRepository::dispatchEditTile(evnt);
        onTileChanged(i);
    }
}

void TileContainer::setTileGroundZPosition(TileIndex i, f32 groundZPosition) {
    assert(isReady());
    Tile& tile = mTiles[i];
    TileContainerEvent evnt;
    if (tile.getGroundZOffset() != groundZPosition) {
        evnt.edit.changeZPos.prevGroundZOffset = tile.getGroundZOffset();
        {
            std::lock_guard lock(mSharedMutex);
            tile.setGroundZPosition(groundZPosition);
        }
        evnt.edit.changeZPos.newGroundZOffset = tile.getGroundZOffset();
        evnt.container = this;
        evnt.edit.worldPosition = getTileCenterWorldPosition(i);
        evnt.edit.type = TileContainerEditEventType::ChangeZPos;
        evnt.edit.tileIndex = i;
        TileContainerRepository::dispatchEditTile(evnt);
        onTileChanged(i);
    }
}

void TileContainer::setTileOrientation(TileIndex i, Cartesian dir, TileLayer layer) {
    assert(isReady());
    Tile& tile = mTiles[i];

    TileContainerEvent evnt;
    evnt.edit.changeOrientation.prevOrientation = tile.orientation;
    if (tile.getOrientation(layer) != dir) {
        {
            std::lock_guard lock(mSharedMutex);
            tile.setOrientation(dir, layer);
        }
        evnt.edit.changeOrientation.newOrientation = tile.orientation;
        evnt.container = this;
        evnt.edit.worldPosition = getTileCenterWorldPosition(i);
        evnt.edit.type = TileContainerEditEventType::ChangeOrientation;
        evnt.edit.tileIndex = i;
        TileContainerRepository::dispatchEditTile(evnt);
        onTileChanged(i);
    }
}

void TileContainer::setWallAt(TileIndex index, Cartesian dir, TileWall wall) {
    assert(isReady());
    assert(IS_GAME_THREAD());
    TileWalls& tileWalls = mWalls[index];
    Tile& tile = mTiles[index];

    // Check for removed or added door (Dynamic object)
    // Old door
    TileID oldId = tileWalls.walls[e_cast(dir)].wallID;
    if (oldId != TILE_ID_NONE && TileRepository::getTileData(oldId).shape == TileShape::DOOR) {
        removeDoor(dir, index);
    }
    // New door
    TileID newId = wall.wallID;
    if (newId != TILE_ID_NONE && TileRepository::getTileData(newId).shape == TileShape::DOOR) {
        addDoor(dir, index);
    }

    {
        std::lock_guard lock(mSharedMutex);
        tileWalls.walls[e_cast(dir)] = wall;
    }
    onTileChanged(index);
}

void TileContainer::setWallsAt(TileIndex index, TileWalls walls) {
    assert(isReady());
    assert(IS_GAME_THREAD());
    TileWalls& tileWalls = mWalls[index];
    // Check for any removed or added doors (Dynamic objects)
    for (int i = 0; i < 4; ++i) {
        // Old door
        TileID oldId = tileWalls.walls[i].wallID;
        if (oldId != TILE_ID_NONE && TileRepository::getTileData(oldId).shape == TileShape::DOOR) {
            removeDoor(Cartesian(i), index);
        }
        // New door
        TileID newId = walls.walls[i].wallID;
        if (newId != TILE_ID_NONE && TileRepository::getTileData(newId).shape == TileShape::DOOR) {
            addDoor(Cartesian(i), index);
        }
    }
    {
        std::lock_guard lock(mSharedMutex);
        tileWalls = walls;
    }
    onTileChanged(index);
}

TileHandle TileContainer::tryGetTileHandleAtWorldPos(const i32v3& worldPos) const {
    if (!isReady()) {
        return TileHandle();
    }
    i32v3 offset = worldPos - mRootPos;
    if (offset.x < 0 || offset.y < 0 || offset.z < 0 || offset.x >= mDims.x || offset.y >= mDims.y || offset.z >= mDims.z * (i32)mFloorHeight) {
        return TileHandle();
    }
    // Scale to floor height
    offset.z /= (i32)mFloorHeight;
    return TileHandle(this, getTileIndexFromXYZOffset(ui32v3(offset)));
}

Chunk* TileContainer::getOwnerChunk() const {
    Chunk*const* chunk = std::get_if<Chunk*>(&mOwner);
    if (chunk) {
        return *chunk;
    }
    return nullptr;
}

Building* TileContainer::getOwnerBuilding() const {
    Building* const* building = std::get_if<Building*>(&mOwner);
    if (building) {
        return *building;
    }
    return nullptr;
}

void TileContainer::addEntrance(TileIndex pos, bool isLocked) {
    assert(IS_GAME_THREAD());
    assert(isReady());
    {
        std::lock_guard lock(mSharedMutex);
        auto&& newEntrance = mEntrances.emplace_back();
        newEntrance.tileIndex = pos;
        newEntrance.isLocked = true;
    }
    LOG_CRITICAL("TODO: Update nav in TileContainer::addEntrance");
}

void TileContainer::removeEntrance(TileIndex pos) {
    assert(IS_GAME_THREAD());
    assert(isReady());
    std::lock_guard lock(mSharedMutex);
    for (size_t i = 0; i < mEntrances.size(); ++i) {
        if (mEntrances[i].tileIndex == pos) {
            mEntrances[i] = mEntrances.back();
            mEntrances.pop_back();
        }
    }
    LOG_CRITICAL("TODO: Update nav in TileContainer::removeEntrance");
}

void TileContainer::copyDataWorkerThread(OUT ContainerMeshDataCopy& dataCopy) const {
    assert(!IS_GAME_THREAD());
    PROFILE_FUNCTION();
    {
        std::shared_lock lock(mSharedMutex);
        dataCopy.mTiles = mTiles;
        dataCopy.mWalls = mWalls;
    } // End scope so profiler can do a mutex lock without having this lock, preventing potential deadlock
}

void TileContainer::copyDataWorkerThread(OUT ContainerNavDataCopy& dataCopy) const
{
    assert(!IS_GAME_THREAD());
    PROFILE_FUNCTION();
    {
        std::shared_lock lock(mSharedMutex);
        dataCopy.mTiles = mTiles;
        dataCopy.mWalls = mWalls;
        dataCopy.mOwnedTiles = mOwnedTiles;
    } // End scope so profiler can do a mutex lock without having this lock, preventing potential deadlock
}

void TileContainer::onTileChanged(TileIndex tileIndex) {
    assert(IS_GAME_THREAD());
    //assert(isReady());
    Tile& tile = mTiles[tileIndex];

    // When not locked we can immediately mark dirty and copy
    mDirtyData = true;

    // Potentially block or free terrain below
    // TODO: Proper intersection
    // TODO: Only when the layer changes
    if (!isTerrain()) {
        IChunkGrid& chunkGrid = sWorld->getChunkGrid();
        const i32v3 offset = getTileXYZOffsetWithZScale(tileIndex);
        if (offset.z == 0) {
            const i32v2 worldPos2D(mRootPos.x + offset.x, mRootPos.y + offset.y);
            Chunk& chunk = chunkGrid.getChunk(ChunkID::fromWorldI32v2(worldPos2D));
            if (chunk.isDataReady()) {
                TileContainer* chunkTileContainer = chunk.getTileContainer();
                assert(chunkTileContainer);
                TileIndex chunkTileIndex = chunkTileContainer->getTileIndexFromXYZOffset(worldPos2D.x - chunkTileContainer->getWorldPos2D().x, worldPos2D.y - chunkTileContainer->getWorldPos2D().y, 0);
                if (tile.isEmpty()) {
                    chunkTileContainer->clearTileFlag(chunkTileIndex, TileFlags::IS_BLOCKED_BY_STRUCTURE);
                }
                else {
                    chunkTileContainer->setTileFlag(chunkTileIndex, TileFlags::IS_BLOCKED_BY_STRUCTURE);
                    // TODO: Don't always clear grass?
                    chunk.setGrassAt(chunkTileIndex, 0);
                }
            }
            else {
                assert(false && "Building on invalid chunk");
            }
        }
    }
}

void TileContainer::addDoor(Cartesian doorSide, TileIndex tileIndex) {
    std::lock_guard lock(mSharedMutex);
    assert(isReady());
    mDynamicTiles.emplace_back(DynamicTile{ tileIndex, {}/*flags*/, DynamicTileType(doorSide) });
    if (!isTerrain()) {
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
                    forceFlag = TileFlags::FORCE_EXTERNAL_EDGE_NORTH;
                    break;
                case Cartesian::WEST:
                    isExterior = ((offset.x == 0) || !isTileOwned(tileIndex - 1));
                    forceFlag = TileFlags::FORCE_EXTERNAL_EDGE_EAST;
                    break;
                case Cartesian::EAST:
                    isExterior = ((offset.x == mDims.x - 1) || !isTileOwned(tileIndex + 1));
                    forceFlag = TileFlags::FORCE_EXTERNAL_EDGE_WEST;
                    break;
                case Cartesian::NORTH:
                    isExterior = ((offset.y == mDims.y - 1) || !isTileOwned(tileIndex + mDims.x));
                    forceFlag = TileFlags::FORCE_EXTERNAL_EDGE_SOUTH;
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
    std::lock_guard lock(mSharedMutex);
    assert(false); // Implement removing the navnode edge
    for (size_t i = 0; i < mDynamicTiles.size(); ++i) {
        if (mDynamicTiles[i].mTileIndex == tileIndex && mDynamicTiles[i].mType == e_cast(doorSide)) {
            mDynamicTiles[i] = mDynamicTiles.back();
            mDynamicTiles.pop_back();
            return;
        }
    }
    assert(false && "Couldn't find door in dynamic array");
}
