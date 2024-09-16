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

#include "ecs/factory/EntityFactory.h"

#include "building/building.h"

#include "debugging/DebugRenderer.h"

// For item placement
#include "definitions/ModelDef.h"

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
    mHarvestableRegistry.init(*this);
}

void TileContainer::freeData() {
    ASSERT_GAME_THREAD();
    assert(mRefCount.load() == 0);
    std::vector<Tile>().swap(mTiles);
    std::vector<DynamicTile>().swap(mDynamicTiles);
    std::vector<TileID>().swap(mFloorIds);
    mTileWallsContainer.destroy();
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

bool TileContainer::canSetTile(TileIndex i, const TileDef& tileData) const
{
    return mTiles[i].canAddTileData(tileData);
}

void TileContainer::setTile(TileIndex i, const TileDef& tileData) {
    assert(isReady());
    if (tileData.layer == e_cast(TileLayer::Main)) {
        setTile(i, (TileID)tileData.getID(), 0);
    }
    else {
        setFloorTile(i, tileData.getID());
    }
}

void TileContainer::setFloorTile(TileIndex i, TileID id) {
    {
        std::lock_guard lock(mSharedMutex);
        mFloorIds.resize(mTiles.size());
        mFloorIds[i] = id;
    }
}

bool TileContainer::trySetTile(TileIndex i, const TileDef& tileData) {
    assert(isReady());
    if (tileData.layer == e_cast(TileLayer::Main)) {
        Tile& tile = mTiles[i];
        if (!tile.canAddTileData(tileData)) {
            return false;
        }
        setTile(i, (TileID)tileData.getID(), 0);
    }
    else {
        setFloorTile(i, tileData.getID());
    }
    return true;
}

void TileContainer::setTile(TileIndex i, TileID id, ui8 variant) {
    assert(isReady());
    Tile& tile = mTiles[i];
    TileID prevId = tile.mainLayer;
    if (prevId == id) {
        return;
    }

    // Build notify
    TileContainerEvent evnt;
    TileContainerEditLayerEventData eventData;

    TileContainerEditEvent editEvent;
    editEvent.type = TileContainerEditEventType::ChangeTileID;
    editEvent.changeLayerArray = &eventData;

    evnt.varEvent = editEvent;
    evnt.container = this;
    evnt.containerId = mId;
    eventData.worldPosition = getTileCenterWorldPosition(i);
    eventData.tileIndex = i;
    eventData.prevId = prevId;
    eventData.newVariant = variant;
    eventData.newId = id;
    eventData.typeData = tile.typeDataCopy;
    // Edit
    {
        std::lock_guard lock(mSharedMutex);
        tile.mainLayer = id;
    }
    // Dispatch notify
    mHarvestableRegistry.onTileLayerChanged(editEvent);
    mWorld.getTileContainerRepository().dispatchEditTiles(evnt);
    dispatchEditTiles(evnt);

    onTileChanged(i);
}

bool TileContainer::tryTransformTile(TileIndex i, MutationType type) {
    assert(isReady());
    Tile& tile = mTiles[i];
    const TileID prevId = tile.mainLayer;
    const TileDef& tileDef = TileRepository::get().getLoadedOrUnloadedAsset(prevId);

    // TODO: real variant
    ui8 newVariant = 0;

    const TileID newId = tileDef.transformations[e_cast(type)];
    if (newId == TILE_ID_NONE) {
        return false;
    }

    // Build notify
    TileContainerEvent evnt;
    TileContainerEditLayerEventData eventData;

    TileContainerEditEvent editEvent;
    editEvent.type = TileContainerEditEventType::ChangeTileID;
    editEvent.changeLayerArray = &eventData;

    evnt.container = this;
    evnt.containerId = mId;
    evnt.varEvent = editEvent;
    eventData.tileIndex = i;
    eventData.worldPosition = getTileCenterWorldPosition(i);
    eventData.prevId = prevId;
    eventData.newId = newId;
    eventData.mutationType = type;
    eventData.newVariant = newVariant;

    // Edit
    {
        std::lock_guard lock(mSharedMutex);
        tile.mainLayer = eventData.newId;
        tile.variant = newVariant;
    }

    // Dispatch notify
    mHarvestableRegistry.onTileLayerChanged(editEvent);
    mWorld.getTileContainerRepository().dispatchEditTiles(evnt);
    dispatchEditTiles(evnt);

    onTileChanged(i);
    return true;
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
        evnt.containerId = mId;
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
        evnt.containerId = mId;
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
        evnt.containerId = mId;
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
        evnt.containerId = mId;
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
        eventData.tileId = tile.mainLayer;

        evnt.container = this;
        evnt.containerId = mId;
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
    evnt.containerId = mId;
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
        currEventData.tileId = mTiles[tileIndex].mainLayer;
        currEventData.worldPosition = getTileCenterWorldPosition(currEventData.tileIndex);
        onTileChanged(tileIndex);
    }

    mWorld.getTileContainerRepository().dispatchEditTiles(evnt);
    dispatchEditTiles(evnt);
}

void TileContainer::setTileOrientation(TileIndex i, Cartesian dir) {
    assert(isReady());
    assert(i < mTiles.size());
    Tile& tile = mTiles[i];

    if (tile.getOrientation() != dir) {

        TileContainerEvent evnt;
        TileContainerEditOrientationEventData eventData;

        TileContainerEditEvent editEvent;
        editEvent.type = TileContainerEditEventType::ChangeOrientation;
        editEvent.changeOrientationArray = &eventData;

        evnt.container = this;
        evnt.containerId = mId;
        evnt.varEvent = editEvent;
        eventData.prevOrientation = tile.orientation;
        {
            std::lock_guard lock(mSharedMutex);
            tile.setOrientation(dir);
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

bool TileContainer::adjustTileHealth(TileIndex index, int healthAdjust, f32v3 impactPosition, f32v3 impactNormal) {
    ASSERT_GAME_THREAD();

    healthAdjust = glm::clamp(healthAdjust , -(int)UINT16_MAX, (int)UINT16_MAX);

    assert(isReady());
    assert(index < mTiles.size());
    if (healthAdjust == 0) {
        return false;
    }

    const TileID tileId = mTiles[index].mainLayer;
    assert(tileId != TILE_ID_NONE && "Tried to damage empty tile");

    auto destroyTile = [&](TileContainerEvent evnt) {

        // Optional VFX
        const TileDef& tileDef = TileRepository::get().getLoadedOrUnloadedAsset(tileId);
        if (tileDef.destroyEffectRef.isValid()) {
            mWorld.getEffectContext().playParticleEffectAtPoint(tileDef.destroyEffectRef, impactPosition, f32q(), nullptr, BitFlags<EffectCreateFlags>());
        }
       
        const f32v3 worldPos = getTileCenterWorldPosition(index);

        std::get<TileDamagedEvent>(evnt.varEvent).wasDestroyed = true;
        // Destroy tile
        mTiles[index].clearTileFlag(TileFlags::IS_DAMAGED);
        setTile(index, TILE_ID_NONE, 0);
        // Damage + Death event
        dispatchTileDamaged(evnt);
        mWorld.getTileContainerRepository().dispatchTileDamaged(evnt);
        dispatchTileDestroyed(evnt);
        mWorld.getTileContainerRepository().dispatchTileDestroyed(evnt);

        // Item drops
       // TODO: Inventory Operations helper?
        constexpr i32 MAX_ROLL_RESULTS = 16;
        ItemRollTable::Result rollResults[MAX_ROLL_RESULTS];
        const i32 resultCount = tileDef.itemDrops.roll(std::span(rollResults, MAX_ROLL_RESULTS));
        f32 zSpan = tileDef.modelRef.getLoadedOrUnloadedAsset<ModelDef>().mAABB.dims.z * 0.9f;
        for (i32 i = 0; i < resultCount; ++i) {
            const ItemAssetRef itemAsset = rollResults[i].value;
            const i32 quantity = rollResults[i].quantity;
            for (i32 j = 0; j < quantity; ++j) {
                const f32v3 velocity = f32v3(Random::getCachedRandomf() * 2.0f - 1.0f, Random::getCachedRandomf() * 2.0f - 1.0f, 2.0f) * 2.0f;
                f32 zOffset = 0.25f + Random::getCachedRandomf() * zSpan;
                EntityFactory::createItemProjectile(mWorld, worldPos + f32v3(0.0f, 0.0f, zOffset), velocity, ItemStack(itemAsset.getAssetID(), 1));
            }
        }
    };

    // Check if already damaged
    TileDamageData* healthPtr = nullptr;
    auto&& it = mDamagedTiles.find(index);
    if (it == mDamagedTiles.end()) {
        // Tile is not damaged yet
        if (healthAdjust < 0) {
            const ui16 maxHealth = TileRepository::get().getLoadedOrUnloadedAsset(tileId).maxHealth;
            if (healthAdjust <= -(int)maxHealth) {
                // Instant death
                TileContainerEvent evnt;
                evnt.container = this;
                evnt.containerId = mId;
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
                healthPtr = mDamagedTiles.insert(std::make_pair(index, TileDamageData::create(maxHealth))).first->second.get();
                mTiles[index].setTileFlag(TileFlags::IS_DAMAGED);
            }
        }
        else {
            // Healing an already fully healed tile
            return false;
        }
    }
    else {
        // Tile already damaged, get current health
        healthPtr = it->second.get();
    }

    const int currentHealth = healthPtr->getCurrentHealth();
    assert(currentHealth != 0);
    if (healthAdjust < 0) {
        // Event data
        // TODO: Add source
        TileContainerEvent evnt;
        evnt.container = this;
        evnt.containerId = mId;
        evnt.varEvent = TileDamagedEvent{
            .tileIndex = index,
            .tileId = mTiles[index].mainLayer,
            .damageAmount = (ui16)-healthAdjust,
            .impactPosition = impactPosition,
            .impactNormal = impactNormal
        };

        // Damage event
        TileDamageResistances resistances; // Default for now
        const f32v3 tilePosWorld = mTileSpatialGrid.getTileCenterWorldPos3D(index, mTiles[index].getGroundZOffset());
        const f32v2 impactNormal2d = MathUtil::rotateVector2DRad(f32v2(impactNormal.x, impactNormal.y), -getTileModelRotationAtPosition(tilePosWorld));
        const i32 appliedDamage = healthPtr->applyDamageStrike(impactNormal2d, impactPosition.z - mTiles[index].groundZOffset, -healthAdjust, resistances);
        std::get<TileDamagedEvent>(evnt.varEvent).damageAmount = appliedDamage;

        if (healthPtr->getCurrentHealth() == 0) {
            mDamagedTiles.erase(it);
            destroyTile(evnt);
            return true;
        }

        std::get<TileDamagedEvent>(evnt.varEvent).damageData = *healthPtr;
        dispatchTileDamaged(evnt);
        mWorld.getTileContainerRepository().dispatchTileDamaged(evnt);
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
    const i32v3& rootPos = mTileSpatialGrid.getWorldPos();
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
    dataCopy.floorIds.resize(mFloorIds.size());
    dataCopy.walls.resizeForCopy(mTiles.size());
    {
        std::shared_lock lock(mSharedMutex);
        memcpy(dataCopy.tiles.data(), mTiles.data(), mTiles.size() * sizeof(Tile));
        if (mFloorIds.size()) {
            memcpy(dataCopy.floorIds.data(), mFloorIds.data(), mFloorIds.size() * sizeof(TileID));
        }
        dataCopy.walls.copyFrom(mTileWallsContainer);
        dataCopy.spatialGrid = mTileSpatialGrid;
        dataCopy.damageData.reserve(mDamagedTiles.size());
        for (auto&& [index, damageData] : mDamagedTiles) {
            dataCopy.damageData.emplace_hint(dataCopy.damageData.end(), index, *damageData);
        }
    } // End scope so profiler can do a mutex lock without having this lock, preventing potential deadlock
    // Simplify lookups
    if (dataCopy.floorIds.empty()) {
        dataCopy.floorIds.resize(dataCopy.tiles.size(), TILE_ID_NONE);
    }
}

void TileContainer::copyDataWorkerThread(OUT ContainerNavDataCopy& dataCopy) const {
    assert(!IS_GAME_THREAD());
    PROFILE_SCOPE("copyDataWorkerThread::NAV");
    // Allocate outside critical section
    dataCopy.harvestables.resize(mHarvestableRegistry.getRegistryCount());
    dataCopy.tiles.resize(mTiles.size());
    dataCopy.floorIds.resize(mFloorIds.size());
    dataCopy.walls.resizeForCopy(mTiles.size());
    {
        std::shared_lock lock(mSharedMutex);
        memcpy(dataCopy.tiles.data(), mTiles.data(), mTiles.size() * sizeof(Tile));
        if (mFloorIds.size()) {
            memcpy(dataCopy.floorIds.data(), mFloorIds.data(), mFloorIds.size() * sizeof(TileID));
        }
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
    // Simplify lookups
    if (dataCopy.floorIds.empty()) {
        dataCopy.floorIds.resize(dataCopy.tiles.size(), TILE_ID_NONE);
    }
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
        const i32v3& rootPos = mTileSpatialGrid.getWorldPos();
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
                TileIndex chunkTileIndex = chunkTileSpatialGrid.getBaseTileIndexFromXYOffset(worldPos2D.x - chunkTileSpatialGrid.getWorldPos().x, worldPos2D.y - chunkTileSpatialGrid.getWorldPos().y);
                if (tile.isEmpty()) {
                    chunkTileContainer->clearTileFlag(chunkTileIndex, TileFlags::IS_BLOCKED_BY_BUILDING);
                }
                else {
                    chunkTileContainer->setTileFlag(chunkTileIndex, TileFlags::IS_BLOCKED_BY_BUILDING);
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
