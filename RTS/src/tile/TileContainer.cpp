#include "stdafx.h"
#include "TileContainer.h"

#include "pathfinding/NavThread.h"

#include "tile/TileContainerRepository.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/RenderThreadTasks.h"
#include "effect/IEffectContext.h"

#include "resources/TileRepository.h"
#include "world/World.h"
#include "world/IChunkGrid.h"

#include "building/building.h"

#include "physics/PhysicsWorld.h"

TileContainer::TileContainer(World& world) : mWorld(world)
{

}

TileContainer::~TileContainer() {

}

void TileContainer::init(TileContainerID id, ui32v3 rootPos, ui32v3 dims, ui32 floorHeight, VarTileContainerOwner owner) {
    mTileSpatialGrid.init(rootPos, dims, floorHeight);
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
    size_t numTiles = mTileSpatialGrid.getNumTiles();
    mTiles.resize(numTiles);
    mTileWallsContainer.init(&mTileSpatialGrid);
    mTileItemContainer.init(&mTileSpatialGrid);
    mHarvestableRegistry.init(*this);
}

void TileContainer::freeData() {
    ASSERT_GAME_THREAD();
    assert(mRefCount.load() == 0);
    std::vector<Tile>().swap(mTiles);
    std::vector<DynamicTile>().swap(mDynamicTiles);
    mTileWallsContainer.destroy();
    mTileItemContainer.destroy();
    mTileVisibilityContainer.destroy();
    mHarvestableRegistry.destroy();
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

bool TileContainer::canAddTileData(TileIndex i, const TileDef& tileData) const
{
    return mTiles[i].canAddTileData(tileData);
}

void TileContainer::setTileLayer(TileIndex i, const TileDef& tileData) {
    assert(isReady());
    setTileLayer(i, (TileLayer)tileData.layer, (TileID)tileData.getID());
}

bool TileContainer::tryAddTileLayer(TileIndex i, const TileDef& tileData) {
    assert(isReady());
    Tile& tile = mTiles[i];
    if (!tile.canAddTileData(tileData)) {
        return false;
    }
    setTileLayer(i, (TileLayer)tileData.layer, (TileID)tileData.getID());
    return true;
}

void TileContainer::setTileLayer(TileIndex i, TileLayer layer, TileID id) {
    assert(isReady());
    Tile& tile = mTiles[i];
    TileID prevId = tile.layers[e_cast(layer)];
    if (prevId == id) {
        return;
    }

    // TODO: Array of blocking data so we dont need a cache miss lookup here??
    // Check if we need to block neighbors, such as for large tree
    NavBlockerType navBlockerType = NavBlockerType::NONE; 
    NavBlockerType prevNavBlockerType = NavBlockerType::NONE;
    if (!isTileNone(id)) {
        navBlockerType = TileRepository::get().getLoadedOrUnloadedAsset(id).navBlockerType;
    }
    if (!isTileNone(prevId)) {
        prevNavBlockerType = TileRepository::get().getLoadedOrUnloadedAsset(prevId).navBlockerType;
    }

    // TODO: Handle visibility on block
    if (navBlockerType != prevNavBlockerType) {
        if (prevNavBlockerType != NavBlockerType::NONE) {
            // Remove old blockage
            removeBlockerFromAdjTiles(i, prevNavBlockerType);
        }
        if (navBlockerType != NavBlockerType::NONE) {
            if (!tryBlockAdjTiles(i, navBlockerType)) {
                return; // TODO: RETURN FALSE
            }
        }
    }

    // Build notify
    TileContainerEvent evnt;
    TileContainerEditLayerEventData eventData;

    TileContainerEditEvent editEvent;
    editEvent.type = TileContainerEditEventType::ChangeLayer;
    editEvent.changeLayerArray = &eventData;

    evnt.varEvent = editEvent;
    evnt.container = this;
    eventData.worldPosition = getTileCenterWorldPosition(i);
    eventData.tileIndex = i;
    eventData.prevId = prevId;
    eventData.newId = id;
    eventData.layer = layer;
    // Edit
    {
        std::lock_guard lock(mSharedMutex);
        tile.layers[e_cast(layer)] = id;
    }
    // Dispatch notify
    mHarvestableRegistry.onTileLayerChanged(editEvent);
    mWorld.getTileContainerRepository().dispatchEditTiles(evnt);
    dispatchEditTiles(evnt);

    onTileChanged(i);
}

void TileContainer::setTileFlag(TileIndex i, TileFlags flag) {
    assert(isReady());
    Tile& tile = mTiles[i];
    // Build notify
    if (!tile.hasFlag(flag)) {
        TileContainerEvent evnt;
        TileContainerEditFlagsEventData eventData;

        TileContainerEditEvent editEvent;
        editEvent.type = TileContainerEditEventType::ChangeFlags;
        editEvent.changeFlagsArray = &eventData;

        evnt.container = this;
        evnt.varEvent = editEvent;
        eventData.prevFlags = tile.tileFlags;
        {
            std::lock_guard lock(mSharedMutex);
            tile.setTileFlag(flag);
        }
        eventData.newFlags = tile.tileFlags;
        eventData.tileIndex = i;
        eventData.worldPosition = getTileCenterWorldPosition(i);
        mWorld.getTileContainerRepository().dispatchEditTiles(evnt);
        dispatchEditTiles(evnt);
        onTileChanged(i);
    }
}

void TileContainer::overwriteTileFlags(TileIndex i, TileFlags flags) {
    assert(isReady());
    Tile& tile = mTiles[i];

    // Build notify
    if (tile.tileFlags.getBits() != e_cast(flags)) {
        TileContainerEvent evnt;
        TileContainerEditFlagsEventData eventData;

        TileContainerEditEvent editEvent;
        editEvent.type = TileContainerEditEventType::ChangeFlags;
        editEvent.changeFlagsArray = &eventData;

        evnt.container = this;
        evnt.varEvent = editEvent;
        eventData.prevFlags = tile.tileFlags;
        {
            std::lock_guard lock(mSharedMutex);
            tile.overwriteTileFlags(flags);
        }
        eventData.newFlags = tile.tileFlags;
        eventData.tileIndex = i;
        eventData.worldPosition = getTileCenterWorldPosition(i);
        mWorld.getTileContainerRepository().dispatchEditTiles(evnt);
        dispatchEditTiles(evnt);
        onTileChanged(i);
    }
}

void TileContainer::clearTileFlag(TileIndex i, TileFlags flag) {
    assert(isReady());
    Tile& tile = mTiles[i];

    if (tile.hasFlag(flag)) {

        TileContainerEvent evnt;
        TileContainerEditFlagsEventData eventData;

        TileContainerEditEvent editEvent;
        editEvent.type = TileContainerEditEventType::ChangeFlags;
        editEvent.changeFlagsArray = &eventData;

        evnt.container = this;
        evnt.varEvent = editEvent;

        eventData.prevFlags = tile.tileFlags;
        {
            std::lock_guard lock(mSharedMutex);
            tile.clearTileFlag(flag);
        }
        eventData.newFlags = tile.tileFlags;
        eventData.tileIndex = i;
        eventData.worldPosition = getTileCenterWorldPosition(i);
        mWorld.getTileContainerRepository().dispatchEditTiles(evnt);
        dispatchEditTiles(evnt);
        onTileChanged(i);
    }
}

void TileContainer::clearTileFlags(TileIndex i) {
    assert(isReady());
    Tile& tile = mTiles[i];

    // Build notify
    if (tile.tileFlags.getBits()) {
        TileContainerEvent evnt;
        TileContainerEditFlagsEventData eventData;

        TileContainerEditEvent editEvent;
        editEvent.type = TileContainerEditEventType::ChangeFlags;
        editEvent.changeFlagsArray = &eventData;

        evnt.container = this;
        evnt.varEvent = editEvent;
        eventData.prevFlags = tile.tileFlags;
        {
            std::lock_guard lock(mSharedMutex);
            tile.zeroTileFlags();
        }
        eventData.newFlags = tile.tileFlags;
        eventData.tileIndex = i;
        eventData.worldPosition = getTileCenterWorldPosition(i);
        mWorld.getTileContainerRepository().dispatchEditTiles(evnt);
        dispatchEditTiles(evnt);
        onTileChanged(i);
    }
}

void TileContainer::setTileGroundZPosition(TileIndex i, f32 groundZPosition) {
    assert(isReady());
    Tile& tile = mTiles[i];
    if (tile.getGroundZOffset() != groundZPosition) {

        TileContainerEvent evnt;
        TileContainerEditZPosEventData eventData;

        TileContainerEditEvent editEvent;
        editEvent.type = TileContainerEditEventType::ChangeZPos;
        editEvent.changeZPosArray = &eventData;

        evnt.container = this;
        evnt.varEvent = editEvent;

        eventData.prevGroundZOffset = tile.getGroundZOffset();
        {
            std::lock_guard lock(mSharedMutex);
            tile.setGroundZOffset(groundZPosition);
        }
        eventData.newGroundZOffset = tile.getGroundZOffset();
        eventData.worldPosition = getTileCenterWorldPosition(i);
        eventData.tileIndex = i;
        mWorld.getTileContainerRepository().dispatchEditTiles(evnt);
        dispatchEditTiles(evnt);
        onTileChanged(i);
    }
}

void TileContainer::bulkSetTileGroundZPosition(std::pair<TileIndex, f32>* editData, size_t count) {
    ASSERT_GAME_THREAD();
    assert(count);
    assert(count <= MAX_BULK_EDIT_EVENT_COUNT);
    TileContainerEvent evnt;
    static TileContainerEditZPosEventData sEventData[MAX_BULK_EDIT_EVENT_COUNT];

    TileContainerEditEvent editEvent;
    editEvent.type = TileContainerEditEventType::ChangeZPos;
    editEvent.changeZPosArray = sEventData;
    editEvent.editCount = count;

    evnt.container = this;
    evnt.varEvent = editEvent;
    { // Critical section
        std::lock_guard lock(mSharedMutex);
        for (size_t i = 0; i < count; ++i) {
            TileContainerEditZPosEventData& currEventData = sEventData[i];
            const TileIndex tileIndex = editData[i].first;
            const f32 zPosition = editData[i].second;
            Tile& tile = mTiles[tileIndex];
            currEventData.prevGroundZOffset = tile.groundZOffset;
            mTiles[tileIndex].setGroundZOffset(zPosition);
            currEventData.newGroundZOffset = tile.groundZOffset;
        }
    }

    // Move out to keep critical section small as possible
    for (size_t i = 0; i < count; ++i) {
        TileContainerEditZPosEventData& currEventData = sEventData[i];
        const TileIndex tileIndex = editData[i].first;
        currEventData.tileIndex = tileIndex;
        currEventData.worldPosition = getTileCenterWorldPosition(currEventData.tileIndex);
        onTileChanged(tileIndex);
    }

    mWorld.getTileContainerRepository().dispatchEditTiles(evnt);
    dispatchEditTiles(evnt);
}

void TileContainer::setTileOrientation(TileIndex i, Cartesian dir, TileLayer layer) {
    assert(isReady());
    assert(i < mTiles.size());
    Tile& tile = mTiles[i];

    if (tile.getOrientation(layer) != dir) {

        TileContainerEvent evnt;
        TileContainerEditOrientationEventData eventData;

        TileContainerEditEvent editEvent;
        editEvent.type = TileContainerEditEventType::ChangeOrientation;
        editEvent.changeOrientationArray = &eventData;

        evnt.container = this;
        evnt.varEvent = editEvent;
        eventData.prevOrientation = tile.orientation;
        {
            std::lock_guard lock(mSharedMutex);
            tile.setOrientation(dir, layer);
        }
        eventData.newOrientation = tile.orientation;
        eventData.worldPosition = getTileCenterWorldPosition(i);
        eventData.tileIndex = i;
        mWorld.getTileContainerRepository().dispatchEditTiles(evnt);
        dispatchEditTiles(evnt);
        onTileChanged(i);
    }
}

void TileContainer::setWallAt(TileIndex index, Cartesian dir, TileWall wall) {
    assert(isReady());
    assert(index < mTiles.size());
    ASSERT_GAME_THREAD();
    TileWall prevTileWall = mTileWallsContainer.getWallAtTile(index, dir);
    Tile& tile = mTiles[index];

    // Check for removed or added door (Dynamic object)
    // Old door
    TileID oldId = prevTileWall.wallID;
    if (oldId != TILE_ID_NONE && TileRepository::get().getLoadedOrUnloadedAsset(oldId).shape == TileShape::DOOR) {
        removeDoor(dir, index);
    }
    // New door
    TileID newId = wall.wallID;
    if (newId != TILE_ID_NONE && TileRepository::get().getLoadedOrUnloadedAsset(newId).shape == TileShape::DOOR) {
        addDoor(dir, index);
    }


    {
        std::lock_guard lock(mSharedMutex);
        mTileWallsContainer.setWallAtTile(index, wall, dir);
    }

    // Visibility
    mTileVisibilityContainer.refreshTileVisibility(index, prevTileWall, wall, dir);

    onTileChanged(index);
}

void TileContainer::setWallsAt(TileIndex index, TileWall walls[4]) {
    assert(isReady());
    assert(index < mTiles.size());
    ASSERT_GAME_THREAD();
    TileWalls prevTileWalls;
    mTileWallsContainer.getWallsAtTile(prevTileWalls, index);
    // Check for any removed or added doors (Dynamic objects)
    for (int i = 0; i < 4; ++i) {
        // Old door
        TileID oldId = prevTileWalls.walls[i].wallID;
        if (oldId != TILE_ID_NONE && TileRepository::get().getLoadedOrUnloadedAsset(oldId).shape == TileShape::DOOR) {
            removeDoor(Cartesian(i), index);
        }
        // New door
        TileID newId = walls[i].wallID;
        if (newId != TILE_ID_NONE && TileRepository::get().getLoadedOrUnloadedAsset(newId).shape == TileShape::DOOR) {
            addDoor(Cartesian(i), index);
        }
    }
    {
        std::lock_guard lock(mSharedMutex);
        mTileWallsContainer.setWallsAtTile(index, walls);
    }
    onTileChanged(index);
}

bool TileContainer::adjustTileHealth(TileIndex index, TileLayer layer, int healthAdjust, f32v3 impactPosition, f32v3 impactNormal) {

    healthAdjust = glm::clamp(healthAdjust , -(int)UINT16_MAX, (int)UINT16_MAX);

    assert(layer == TileLayer::Main && "Only main damage currently supported"); // TODO: Support other damage layers

    assert(isReady());
    assert(index < mTiles.size());
    ASSERT_GAME_THREAD();
    if (healthAdjust == 0) {
        return false;
    }

    const TileID tileId = mTiles[index].layers[e_cast(layer)];
    assert(tileId != INVALID_TILE_INDEX && "Tried to damage empty tile");

    auto destroyTile = [&](TileContainerEvent evnt) {

        // Optional VFX
        const TileDef& destroyedTile = TileRepository::get().getLoadedOrUnloadedAsset(tileId);
        if (destroyedTile.destroyEffectRef.isValid()) {
            mWorld.getEffectContext().playParticleEffectAtPoint(destroyedTile.destroyEffectRef.name, impactPosition, ParticleSystemInputs(), BitFlags<EffectCreateFlags>());
        }

        std::get<TileDamagedEvent>(evnt.varEvent).wasDestroyed = true;
        // Destroy tile
        setTileLayer(index, TileLayer::Main, TILE_ID_NONE);
        // Damage + Death event
        dispatchTileDamaged(evnt);
        mWorld.getTileContainerRepository().dispatchTileDamaged(evnt);
        dispatchTileDestroyed(evnt);
        mWorld.getTileContainerRepository().dispatchTileDestroyed(evnt);

       
    };

    // Check if already damaged
    ui16* healthPtr = nullptr;
    auto&& it = mDamagedTiles.find(index);
    if (it == mDamagedTiles.end()) {
        // Tile is not damaged yet
        if (healthAdjust < 0) {
            const ui16 maxHealth = TileRepository::get().getLoadedOrUnloadedAsset(tileId).maxHealth;
            if (healthAdjust <= -(int)maxHealth) {
                // Instant death
                TileContainerEvent evnt;
                evnt.container = this;
                evnt.varEvent = TileDamagedEvent{
                    .tileIndex = index,
                    .tileId = mTiles[index].mainLayer,
                    .damageAmount = (ui16)-healthAdjust,
                    .impactPosition = impactPosition,
                    .impactNormal = impactNormal
                };
                destroyTile(evnt);
                return true;
            }
            else {
                healthPtr = &mDamagedTiles.insert(std::make_pair(index, maxHealth)).first->second;
            }
        }
        else {
            // Healing an already fully healed tile
            return false;
        }
    }
    else {
        // Tile already damaged, get current health
        healthPtr = &it->second;
    }

    const int currentHealth = (int)*healthPtr;
    assert(currentHealth != 0);
    if (healthAdjust < 0) {
        // Event data
        TileContainerEvent evnt;
        evnt.container = this;
        evnt.varEvent = TileDamagedEvent{
            .tileIndex = index,
            .tileId = mTiles[index].mainLayer,
            .damageAmount = (ui16)-healthAdjust,
            .impactPosition = impactPosition,
            .impactNormal = impactNormal
        };

        if (healthAdjust <= -currentHealth) {
            assert(it != mDamagedTiles.end());
            mDamagedTiles.erase(it);
            destroyTile(evnt);
            return true;
        }
        else {
            // Damage event
            *healthPtr = (ui16)((int)*healthPtr + healthAdjust);
            dispatchTileDamaged(evnt);
            mWorld.getTileContainerRepository().dispatchTileDamaged(evnt);
        }
    }
    else {
        assert(false); // handle healing!
    }

    return false;
}

TileHandle TileContainer::tryGetTileHandleAtWorldPos(const i32v3& worldPos) const {
    if (!isReady()) {
        return TileHandle();
    }
    const i32v3& rootPos = mTileSpatialGrid.getWorldPos3D();
    const i32v3& dims = mTileSpatialGrid.getDims();
    i32v3 offset = worldPos - rootPos;
    if (offset.x < 0 || offset.y < 0 || offset.z < 0 || offset.x >= dims.x || offset.y >= dims.y || offset.z >= dims.z * (i32)mTileSpatialGrid.getFloorHeight()) {
        return TileHandle();
    }
    // Scale to floor height
    offset.z /= (i32)mTileSpatialGrid.getFloorHeight();
    return TileHandle(this, mTileSpatialGrid.getTileIndexFromXYZOffset(ui32v3(offset)));
}

const Tile& TileContainer::getTileAt(ui32 offsetX, ui32 offsetY, ui32 offsetZ) const {
    const TileIndex i = mTileSpatialGrid.getTileIndexFromXYZOffset(offsetX, offsetY, offsetZ);
    assert(i < mTiles.size());
    return mTiles[i];
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

bool TileContainer::isTileOwned(TileIndex index) const {
    if (mOwnerType == TileContainerOwnerType::CHUNK) return true;
    return getOwnerBuilding()->isTileOwned(index);
}

bool TileContainer::isTileOwned(const BitArray& ownedDTiles, TileIndex index2d, ui32v2 containerDimsDTiles) {
    if (ownedDTiles.getNumBits() == 0) return true;
    const DTileIndex dtileIndex = structureTileIndexToDTileIndex(index2d, containerDimsDTiles.x);
    return ownedDTiles.getBit(dtileIndex % (containerDimsDTiles.x * containerDimsDTiles.y));
}

void TileContainer::copyDataWorkerThread(OUT ContainerMeshDataCopy& dataCopy) const {
    assert(!IS_GAME_THREAD());
    PROFILE_SCOPE("copyDataWorkerThread::MESH");
    // Allocate outside critical section
    dataCopy.tiles.resize(mTiles.size());
    dataCopy.walls.resizeForCopy(mTiles.size());
    {
        std::shared_lock lock(mSharedMutex);
        memcpy(dataCopy.tiles.data(), mTiles.data(), mTiles.size() * sizeof(Tile));
        dataCopy.walls.copyFrom(mTileWallsContainer);
        dataCopy.spatialGrid = mTileSpatialGrid;
    } // End scope so profiler can do a mutex lock without having this lock, preventing potential deadlock
}

void TileContainer::copyDataWorkerThread(OUT ContainerNavDataCopy& dataCopy) const {
    assert(!IS_GAME_THREAD());
    PROFILE_SCOPE("copyDataWorkerThread::NAV");
    // Allocate outside critical section
    dataCopy.harvestables.resize(mHarvestableRegistry.getRegistryCount());
    dataCopy.tiles.resize(mTiles.size());
    dataCopy.walls.resizeForCopy(mTiles.size());
    {
        std::shared_lock lock(mSharedMutex);
        memcpy(dataCopy.tiles.data(), mTiles.data(), mTiles.size() * sizeof(Tile));
        dataCopy.walls.copyFrom(mTileWallsContainer);
        for (size_t i = 0; i < dataCopy.harvestables.size(); ++i) {
            // Only copying positions cause its all we care about when navving
            dataCopy.harvestables[i].mHarvestablePositions = mHarvestableRegistry.getRegistry(i).mHarvestablePositions;
        }
        dataCopy.spatialGrid = mTileSpatialGrid;
        if (Building* owner = getOwnerBuilding()) {
            dataCopy.ownedDTiles = owner->getOwnedDTiles();
        }
        else {
            dataCopy.ownedDTiles.resize(0);
        }
    } // End scope so profiler can do a mutex lock without having this lock, preventing potential deadlock
}


bool TileContainer::tryBlockAdjTiles(TileIndex i, NavBlockerType navBlockerType) {
    ASSERT_GAME_THREAD();
    // Check for if we can place here (NO BOUNDARIES)
    if (!canPlaceAdjNavBlockerTile(i)) {
        return false;
    }

    // Apply blockage
    constexpr int EDIT_COUNT = 9;
    TileContainerEvent flagsEvent;
    TileContainerEditFlagsEventData eventData[EDIT_COUNT];

    TileContainerEditEvent editEvent;
    editEvent.type = TileContainerEditEventType::ChangeFlags;
    editEvent.changeFlagsArray = eventData;
    editEvent.editCount = EDIT_COUNT;

    flagsEvent.container = this;
    flagsEvent.varEvent = editEvent;

    const i32v3& dims = mTileSpatialGrid.getDims();
    const TileIndex tileIndices[EDIT_COUNT] = {
        i - dims.x - 1 /*SW*/,
        i - dims.x     /*S*/,
        i - dims.x + 1 /*SE*/,
        i - 1           /*W*/,
        i               /*C*/,
        i + 1           /*E*/,
        i + dims.x - 1 /*NW*/,
        i + dims.x     /*N*/,
        i + dims.x + 1 /*NE*/,
    };
    for (int n = 0; n < EDIT_COUNT; ++n) {
        const TileIndex nindex = tileIndices[n];
        eventData[n].prevFlags = mTiles[nindex].tileFlags;
        eventData[n].tileIndex = nindex;
        eventData[n].worldPosition = getTileCenterWorldPosition(nindex);
    }
    {
        const TileFlags blockerFlag = (navBlockerType == NavBlockerType::MEDIUM) ? TileFlags::MEDIUM_BLOCKER : TileFlags::LARGE_BLOCKER;
        static_assert(e_cast(NavBlockerType::COUNT) == 3);
        if (blockerFlag == TileFlags::LARGE_BLOCKER) {
            std::lock_guard lock(mSharedMutex);
            mTiles[tileIndices[0]].setTileFlag(TileFlags(e_cast(TileFlags::HAS_DIAGONAL_BLOCKER))); // SW
            mTiles[tileIndices[1]].setTileFlag(TileFlags(e_cast(TileFlags::HAS_NORTH_BLOCKER) | e_cast(TileFlags::BLOCKED_BY_LARGE))); // S
            mTiles[tileIndices[2]].setTileFlag(TileFlags(e_cast(TileFlags::HAS_DIAGONAL_BLOCKER))); // SE
            mTiles[tileIndices[3]].setTileFlag(TileFlags(e_cast(TileFlags::HAS_EAST_BLOCKER) | e_cast(TileFlags::BLOCKED_BY_LARGE))); // W
            mTiles[tileIndices[4]].setTileFlag(blockerFlag); // C
            mTiles[tileIndices[5]].setTileFlag(TileFlags(e_cast(TileFlags::HAS_WEST_BLOCKER) | e_cast(TileFlags::BLOCKED_BY_LARGE))); // E
            mTiles[tileIndices[6]].setTileFlag(TileFlags(e_cast(TileFlags::HAS_DIAGONAL_BLOCKER))); // NW
            mTiles[tileIndices[7]].setTileFlag(TileFlags(e_cast(TileFlags::HAS_SOUTH_BLOCKER) | e_cast(TileFlags::BLOCKED_BY_LARGE))); // N
            mTiles[tileIndices[8]].setTileFlag(TileFlags(e_cast(TileFlags::HAS_DIAGONAL_BLOCKER))); // NE
        }
        else {
            std::lock_guard lock(mSharedMutex);
            mTiles[tileIndices[0]].setTileFlag(TileFlags(e_cast(TileFlags::HAS_DIAGONAL_BLOCKER))); // SW
            mTiles[tileIndices[1]].setTileFlag(TileFlags::HAS_NORTH_BLOCKER); // S
            mTiles[tileIndices[2]].setTileFlag(TileFlags(e_cast(TileFlags::HAS_DIAGONAL_BLOCKER))); // SE
            mTiles[tileIndices[3]].setTileFlag(TileFlags::HAS_EAST_BLOCKER); // W
            mTiles[tileIndices[4]].setTileFlag(blockerFlag); // C
            mTiles[tileIndices[5]].setTileFlag(TileFlags::HAS_WEST_BLOCKER); // E
            mTiles[tileIndices[6]].setTileFlag(TileFlags(e_cast(TileFlags::HAS_DIAGONAL_BLOCKER))); // NW
            mTiles[tileIndices[7]].setTileFlag(TileFlags::HAS_SOUTH_BLOCKER); // N
            mTiles[tileIndices[8]].setTileFlag(TileFlags(e_cast(TileFlags::HAS_DIAGONAL_BLOCKER))); // NE
        }
    }
    static_assert(e_cast(TileFlags::NAV_BLOCKED_MASK_TERM) == BIT(8), "Update");
    for (int n = 0; n < EDIT_COUNT; ++n) {
        const TileIndex nindex = tileIndices[n];
        eventData[n].newFlags = mTiles[nindex].tileFlags;
    }
    mWorld.getTileContainerRepository().dispatchEditTiles(flagsEvent);
    dispatchEditTiles(flagsEvent);
    return true;
}

bool TileContainer::tryBlockAdjTilesFromGeneration(TileIndex i, NavBlockerType navBlockerType) {
    assert(!IS_GAME_THREAD());
    // Check for if we can place here (NO BOUNDARIES)
    if (!canPlaceAdjNavBlockerTile(i)) {
        return false;
    }

    // Apply blockage
    constexpr int EDIT_COUNT = 10;
    const i32v3& dims = mTileSpatialGrid.getDims();
    const TileIndex tileIndices[EDIT_COUNT] = {
        i - dims.x - 1 /*SW*/,
        i - dims.x     /*S*/,
        i - dims.x + 1 /*SE*/,
        i - 1           /*W*/,
        i               /*C*/,
        i + 1           /*E*/,
        i + dims.x - 1 /*NW*/,
        i + dims.x     /*N*/,
        i + dims.x + 1 /*NE*/,
    };
    if (navBlockerType == NavBlockerType::LARGE) {
        mTiles[tileIndices[0]].tileFlags.setBit(TileFlags(e_cast(TileFlags::HAS_DIAGONAL_BLOCKER))); // SW
        mTiles[tileIndices[1]].tileFlags.setBit(TileFlags(e_cast(TileFlags::HAS_NORTH_BLOCKER) | e_cast(TileFlags::BLOCKED_BY_LARGE))); // S
        mTiles[tileIndices[2]].tileFlags.setBit(TileFlags(e_cast(TileFlags::HAS_DIAGONAL_BLOCKER))); // SE
        mTiles[tileIndices[3]].tileFlags.setBit(TileFlags(e_cast(TileFlags::HAS_EAST_BLOCKER) | e_cast(TileFlags::BLOCKED_BY_LARGE))); // W
        mTiles[tileIndices[4]].tileFlags.setBit(TileFlags::LARGE_BLOCKER); // C
        mTiles[tileIndices[5]].tileFlags.setBit(TileFlags(e_cast(TileFlags::HAS_WEST_BLOCKER) | e_cast(TileFlags::BLOCKED_BY_LARGE))); // E
        mTiles[tileIndices[6]].tileFlags.setBit(TileFlags(e_cast(TileFlags::HAS_DIAGONAL_BLOCKER))); // NW
        mTiles[tileIndices[7]].tileFlags.setBit(TileFlags(e_cast(TileFlags::HAS_SOUTH_BLOCKER) | e_cast(TileFlags::BLOCKED_BY_LARGE))); // N
        mTiles[tileIndices[8]].tileFlags.setBit(TileFlags(e_cast(TileFlags::HAS_DIAGONAL_BLOCKER))); // NE
    }
    else {
        mTiles[tileIndices[0]].tileFlags.setBit(TileFlags(e_cast(TileFlags::HAS_DIAGONAL_BLOCKER))); // SW
        mTiles[tileIndices[1]].tileFlags.setBit(TileFlags::HAS_NORTH_BLOCKER); // S
        mTiles[tileIndices[2]].tileFlags.setBit(TileFlags(e_cast(TileFlags::HAS_DIAGONAL_BLOCKER))); // SE
        mTiles[tileIndices[3]].tileFlags.setBit(TileFlags::HAS_EAST_BLOCKER); // W
        mTiles[tileIndices[4]].tileFlags.setBit(TileFlags::MEDIUM_BLOCKER); // C
        mTiles[tileIndices[5]].tileFlags.setBit(TileFlags::HAS_WEST_BLOCKER); // E
        mTiles[tileIndices[6]].tileFlags.setBit(TileFlags(e_cast(TileFlags::HAS_DIAGONAL_BLOCKER))); // NW
        mTiles[tileIndices[7]].tileFlags.setBit(TileFlags::HAS_SOUTH_BLOCKER); // N
        mTiles[tileIndices[8]].tileFlags.setBit(TileFlags(e_cast(TileFlags::HAS_DIAGONAL_BLOCKER))); // NE
    }
    static_assert(e_cast(TileFlags::NAV_BLOCKED_MASK_TERM) == BIT(8), "Update");
    static_assert(e_cast(NavBlockerType::COUNT) == 3);
    return true;
}

void TileContainer::removeBlockerFromAdjTiles(TileIndex i, NavBlockerType prevNavBlockerType) {
    assert(canPlaceAdjNavBlockerTile(i));

    // Remove blockage
    constexpr int EDIT_COUNT = 9;
    TileContainerEvent flagsEvent;
    TileContainerEditFlagsEventData eventData[EDIT_COUNT];

    TileContainerEditEvent editEvent;
    editEvent.editCount = EDIT_COUNT;
    editEvent.type = TileContainerEditEventType::ChangeFlags;
    editEvent.changeFlagsArray = eventData;

    flagsEvent.container = this;
    flagsEvent.varEvent = editEvent;

    const i32v3& dims = mTileSpatialGrid.getDims();
    const TileIndex tileIndices[EDIT_COUNT] = {
        i - dims.x - 1 /*SW*/,
        i - dims.x     /*S*/,
        i - dims.x + 1 /*SE*/,
        i - 1           /*W*/,
        i               /*C*/,
        i + 1           /*E*/,
        i + dims.x - 1 /*NW*/,
        i + dims.x     /*N*/,
        i + dims.x + 1 /*NE*/,
    };
    for (int n = 0; n < EDIT_COUNT; ++n) {
        const TileIndex nindex = tileIndices[n];
        eventData[n].prevFlags = mTiles[nindex].tileFlags;
        eventData[n].tileIndex = nindex;
        eventData[n].worldPosition = getTileCenterWorldPosition(nindex);
    }
    {
        const TileFlags blockerFlag = (prevNavBlockerType == NavBlockerType::MEDIUM) ? TileFlags::MEDIUM_BLOCKER : TileFlags::LARGE_BLOCKER;
        assert(mTiles[i].hasFlag(blockerFlag));
        static_assert(e_cast(NavBlockerType::COUNT) == 3);
        if (blockerFlag == TileFlags::LARGE_BLOCKER) {
            std::lock_guard lock(mSharedMutex);
            mTiles[tileIndices[0]].clearTileFlag(TileFlags(e_cast(TileFlags::HAS_DIAGONAL_BLOCKER))); // SW
            mTiles[tileIndices[1]].clearTileFlag(TileFlags(e_cast(TileFlags::HAS_NORTH_BLOCKER) | e_cast(TileFlags::BLOCKED_BY_LARGE))); // S
            mTiles[tileIndices[2]].clearTileFlag(TileFlags(e_cast(TileFlags::HAS_DIAGONAL_BLOCKER))); // SE
            mTiles[tileIndices[3]].clearTileFlag(TileFlags(e_cast(TileFlags::HAS_EAST_BLOCKER) | e_cast(TileFlags::BLOCKED_BY_LARGE))); // W
            mTiles[tileIndices[4]].clearTileFlag(blockerFlag); // C
            mTiles[tileIndices[5]].clearTileFlag(TileFlags(e_cast(TileFlags::HAS_WEST_BLOCKER) | e_cast(TileFlags::BLOCKED_BY_LARGE))); // E
            mTiles[tileIndices[6]].clearTileFlag(TileFlags(e_cast(TileFlags::HAS_DIAGONAL_BLOCKER))); // NW
            mTiles[tileIndices[7]].clearTileFlag(TileFlags(e_cast(TileFlags::HAS_SOUTH_BLOCKER) | e_cast(TileFlags::BLOCKED_BY_LARGE))); // N
            mTiles[tileIndices[8]].clearTileFlag(TileFlags(e_cast(TileFlags::HAS_DIAGONAL_BLOCKER))); // NE
        }
        else {
            std::lock_guard lock(mSharedMutex);
            mTiles[tileIndices[0]].clearTileFlag(TileFlags(e_cast(TileFlags::HAS_DIAGONAL_BLOCKER))); // SW
            mTiles[tileIndices[1]].clearTileFlag(TileFlags::HAS_NORTH_BLOCKER); // S
            mTiles[tileIndices[2]].clearTileFlag(TileFlags(e_cast(TileFlags::HAS_DIAGONAL_BLOCKER))); // SE
            mTiles[tileIndices[3]].clearTileFlag(TileFlags::HAS_EAST_BLOCKER); // W
            mTiles[tileIndices[4]].clearTileFlag(blockerFlag); // C
            mTiles[tileIndices[5]].clearTileFlag(TileFlags::HAS_WEST_BLOCKER); // E
            mTiles[tileIndices[6]].clearTileFlag(TileFlags(e_cast(TileFlags::HAS_DIAGONAL_BLOCKER))); // NW
            mTiles[tileIndices[7]].clearTileFlag(TileFlags::HAS_SOUTH_BLOCKER); // N
            mTiles[tileIndices[8]].clearTileFlag(TileFlags(e_cast(TileFlags::HAS_DIAGONAL_BLOCKER))); // NE
        }

        // Now update if the 4 corner neighbors actually still have a diagonal blocker
        updateTileDiagonalBlocked(tileIndices[0]); // SW
        updateTileDiagonalBlocked(tileIndices[2]); // SE
        updateTileDiagonalBlocked(tileIndices[6]); // NW
        updateTileDiagonalBlocked(tileIndices[8]); // NE
    }
    static_assert(e_cast(TileFlags::NAV_BLOCKED_MASK_TERM) == BIT(8), "Update");
    for (int n = 0; n < EDIT_COUNT; ++n) {
        const TileIndex nindex = tileIndices[n];
        eventData[n].newFlags = mTiles[nindex].tileFlags;
    }
    mWorld.getTileContainerRepository().dispatchEditTiles(flagsEvent);
    dispatchEditTiles(flagsEvent);
}

// Checks 4 diagonal corners for any blockers and adds TILE_DIAGONAL_BLOCKERS_MASK if needed
void TileContainer::updateTileDiagonalBlocked(TileIndex index) {
    const i32v3& dims = mTileSpatialGrid.getDims();
    const i32v3 offset = mTileSpatialGrid.getTileXYZOffset(index);
    if (offset.y > 0) {
        // SW
        if (offset.x > 0) {
            const TileIndex SW = index - 1 - dims.x;
            if (isTileOwned(SW) && mTiles[SW].hasFlagsMaskAny(TILE_DIAGONAL_BLOCKERS_MASK)) {
                std::lock_guard lock(mSharedMutex);
                mTiles[index].setTileFlag(TileFlags::HAS_DIAGONAL_BLOCKER);
                return;
            }
        }
        // SE
        if (offset.x < dims.x - 1) {
            const TileIndex SE = index + 1 - dims.x;
            if (isTileOwned(SE) && mTiles[SE].hasFlagsMaskAny(TILE_DIAGONAL_BLOCKERS_MASK)) {
                std::lock_guard lock(mSharedMutex);
                mTiles[index].setTileFlag(TileFlags::HAS_DIAGONAL_BLOCKER);
                return;
            }
        }
    }


    if (offset.y < dims.y - 1) {
        // NW
        if (offset.x > 0) {
            const TileIndex NW = index - 1 + dims.x;
            if (isTileOwned(NW) && mTiles[NW].hasFlagsMaskAny(TILE_DIAGONAL_BLOCKERS_MASK)) {
                std::lock_guard lock(mSharedMutex);
                mTiles[index].setTileFlag(TileFlags::HAS_DIAGONAL_BLOCKER);
                return;
            }
        }
        // NE
        if (offset.x < dims.x - 1) {
            const TileIndex NE = index + 1 + dims.x;
            if (isTileOwned(NE) && mTiles[NE].hasFlagsMaskAny(TILE_DIAGONAL_BLOCKERS_MASK)) {
                std::lock_guard lock(mSharedMutex);
                mTiles[index].setTileFlag(TileFlags::HAS_DIAGONAL_BLOCKER);
                return;
            }
        }
    }
}

bool TileContainer::canPlaceAdjNavBlockerTile(TileIndex i) {
    const i32v3& dims = mTileSpatialGrid.getDims();
    const i32v3 offset = mTileSpatialGrid.getTileXYZOffset(i);
    if (offset.x == 0) {
        return false;
    }
    if (offset.y == 0) {
        return false;
    }
    if (offset.x == dims.x - 1) {
        return false;
    }
    if (offset.y == dims.y - 1) {
        return false;
    }
    // Removed when we switched to DTile ownership
    //// Check ownership if needed
    //if (mOwnedTiles.getNumBits()) {
    //    // SW
    //    if (!mOwnedTiles.getBit(i - 1 - dims.x)) {
    //        return false;
    //    }
    //    // S
    //    if (!mOwnedTiles.getBit(i - dims.x)) {
    //        return false;
    //    }
    //    // SE
    //    if (!mOwnedTiles.getBit(i + 1 - dims.x)) {
    //        return false;
    //    }
    //    // W
    //    if (!mOwnedTiles.getBit(i - 1)) {
    //        return false;
    //    }
    //    // E
    //    if (!mOwnedTiles.getBit(i + 1)) {
    //        return false;
    //    }
    //    // NW
    //    if (!mOwnedTiles.getBit(i - 1 + dims.x)) {
    //        return false;
    //    }
    //    // N
    //    if (!mOwnedTiles.getBit(i + dims.x)) {
    //        return false;
    //    }
    //    // NE
    //    if (!mOwnedTiles.getBit(i + 1 + dims.x)) {
    //        return false;
    //    }
    //}
    return true;
}

void TileContainer::onTileChanged(TileIndex tileIndex) {
    ASSERT_GAME_THREAD();
    //assert(isReady());
    Tile& tile = mTiles[tileIndex];

    // When not locked we can immediately mark dirty and copy
    mDirtyData = true;

    // Potentially block or free terrain below
    // TODO: Proper intersection
    // TODO: Only when the layer changes
    if (!isTerrain()) {
        IChunkGrid& chunkGrid = mWorld.getChunkGrid();
        const i32v3& rootPos = mTileSpatialGrid.getWorldPos3D();
        const i32v3 offset = mTileSpatialGrid.getTileXYZOffsetWithZScale(tileIndex);
        if (offset.z == 0) {
            const i32v2 worldPos2D(rootPos.x + offset.x, rootPos.y + offset.y);

            Chunk& chunk = chunkGrid.getChunkAtPosition(worldPos2D);
            // Only notify chunk if it is activated
            // TODO: is it possible a building can change while under chunk is activated but not connected? Does it matter?
            if (chunk.isActivated()) {
                TileContainer* chunkTileContainer = chunk.getTileContainer();
                const TileSpatialGrid& chunkTileSpatialGrid = chunkTileContainer->getTileSpatialGrid();
                assert(chunkTileContainer);
                TileIndex chunkTileIndex = chunkTileSpatialGrid.getBaseTileIndexFromXYOffset(worldPos2D.x - chunkTileSpatialGrid.getWorldPos2D().x, worldPos2D.y - chunkTileSpatialGrid.getWorldPos2D().y);
                if (tile.isEmpty()) {
                    chunkTileContainer->clearTileFlag(chunkTileIndex, TileFlags::IS_BLOCKED_BY_STRUCTURE);
                }
                else {
                    chunkTileContainer->setTileFlag(chunkTileIndex, TileFlags::IS_BLOCKED_BY_STRUCTURE);
                    // TODO: Don't always clear grass?
                    chunk.clearGrassAt(chunkTileIndex);
                }
            }
        }
    }
}

void TileContainer::addDoor(Cartesian doorSide, TileIndex tileIndex) {
    std::lock_guard lock(mSharedMutex);
    assert(isReady());
    mDynamicTiles.emplace_back(DynamicTile{ tileIndex, {}/*flags*/, DynamicTileType(doorSide) });
}

void TileContainer::removeDoor(Cartesian doorSide, TileIndex tileIndex) {
    std::lock_guard lock(mSharedMutex);
    for (size_t i = 0; i < mDynamicTiles.size(); ++i) {
        if (mDynamicTiles[i].mTileIndex == tileIndex && mDynamicTiles[i].mType == e_cast(doorSide)) {
            mDynamicTiles[i] = mDynamicTiles.back();
            mDynamicTiles.pop_back();
            return;
        }
    }
    assert(false && "Couldn't find door in dynamic array");
}
