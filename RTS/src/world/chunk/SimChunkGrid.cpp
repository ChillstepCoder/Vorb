#include "stdafx.h"
#include "SimChunkGrid.h"

#include "ecs/factory/EntityFactory.h"
#include "gamethread/GameThreadTasks.h"

#include "world/World.h"

//// TODO: Shared utility?
//// Iterate in an outward spiral pattern from a start position
////  ... 12
////4 3 2 11
////5 0 1 10
////6 7 8 9
//void iterateGridSpiral(i32v2 startPos, int maxSteps, std::function<bool(i32v2 /*pos*/, int /*step*/)> func) {
//    int x = startPos.x;
//    int y = startPos.y;
//    int dx = 1;
//    int dy = 0;
//    int segmentLength = 1;
//    int step = 0;
//
//    func({ x, y }, step++); // Call the lambda for the start position
//
//    while (step < maxSteps) {
//        for (int i = 0; i < segmentLength && step < maxSteps; ++i) {
//            x += dx;
//            y += dy;
//            if (!func({ x, y }, step++)) {
//                return;
//            }
//        }
//
//        if (dy == 0) {
//            segmentLength++;
//        }
//
//        // Change direction: right -> up -> left -> down -> right -> ...
//        int temp = dx;
//        dx = -dy;
//        dy = temp;
//    }
//}

SimChunkGrid::SimChunkGrid(ui32 worldWidthTiles) {
    mWidthChunks = worldWidthTiles / CHUNK_WIDTH;
    initInternal();
}

SimChunkGrid::~SimChunkGrid() {

}

void SimChunkGrid::setWorld(World* world) {
    assert(world);
    mWorld = world;
}

ui32 SimChunkGrid::getApproxMemoryUsageBytes() const {
   return mTotalChunks * sizeof(SimChunk) + mTotalSimulatingChunks * (sizeof(SimChunkTileData));
}

SimTileDataWriteReservationPtr SimChunkGrid::tryReserveTileDataAtPosIfNotEmpty(ChunkID chunkId, TileIndex tileIndex) {
    SimChunk& container = mChunkData[chunkId];
    assert(container.isAllocated()); // TODO: Allow allocation later?

    { // Critical section
        std::unique_lock lock(container.mMutex);
        auto&& it = container.mTileData->tileIndexToTileData.find(tileIndex);
        if (it != container.mTileData->tileIndexToTileData.end()) {
            // Tile was tracked, only reserve if not empty
            SimTileData& existing = it->second;
            if (existing.tileId != TILE_ID_NONE) {
                existing.flags.setBit(SimTileDataFlags::Reserved);
                lock.unlock(); // UNLOCK - We are done modifying
                return std::make_unique<SimTileDataWriteReservation>(*this, chunkId, (ui16)tileIndex, existing);
            }
        }
    }
    return nullptr;
}

SimTileDataWriteReservationPtr SimChunkGrid::tryReserveTileDataAtPosIfNotEmpty(TileCoord tileCoord) {
    const auto[chunkId, tileIndex] = tileCoord.toChunkTileIndexAndChunkID(mWidthChunks);
    return tryReserveTileDataAtPosIfNotEmpty(chunkId, tileIndex);
}

void SimChunkGrid::releaseTileDataReservationAndCopyData(SimTileDataWriteReservation& reservation) {
    reservation.mDidRelease = true;
    SimChunk& container = mChunkData[reservation.mChunk];
    assert(container.isAllocated());
    const SimTileData newData = reservation.reservedCopy;

    TileRepository& tileRepo = TileRepository::get();

    { // Critical section
        std::lock_guard lock(container.mMutex);
        container.mTileData->changeTile(reservation.mTileIndex, newData.tileId, newData.variant);
    }
}

SortedIntCoordDistanceSqMap SimChunkGrid::getClosestUnreservedHarvestablesToPoint(TileCoord worldPos, TileHarvestable harvestable, i32 maxDistance, i32 maxCount) {
    PROFILE_FUNCTION();
    assert(maxDistance < 45000); // Greater than this will overflow an i32 (46340 with padding)
    constexpr i32 MAX_ITERATIONS = 512;
    const i32 maxDistanceSq = SQ(maxDistance);

    ChunkCoord chunkCoord(worldPos);
    // Breadth first search
    // TODO: boost flat
    UnorderedFlatSet<ChunkID> closedList;
    closedList.reserve(MAX_ITERATIONS);

    // Stack allocated for efficiency
    ChunkCoord chunkQueue[MAX_ITERATIONS * 4 + 4]; // Extra padding to prevent stack corruption
    chunkQueue[0] = ChunkCoord(worldPos);
    closedList.insert(chunkQueue[0].toGridIDType(mWidthChunks));
    i32 back = 1;
    i32 front = 0;

    SortedIntCoordDistanceSqMap rv;

    i32 i = 0;
    do {
        // TODO: Need to check distanceSq to closest point on chunk to see if this chunk is completely out of range

        // Pop from stack 
        ChunkCoord coord = chunkQueue[front++];
        const ChunkID id = coord.toGridIDType(mWidthChunks);
        SimChunk& simChunk = mChunkData[id];
        {
            TileCoord chunkTilePos(coord);
            std::shared_lock readLock(simChunk.mMutex);
            if (simChunk.mTileData) {
                auto&& it = simChunk.mTileData->harvestables.find(harvestable);
                if (it != simChunk.mTileData->harvestables.end()) {
                    for (ChunkTileIndex tileIndex : it->second) {
                        const TileCoord tileCoord = chunkTilePos + TileCoord(tileIndex % CHUNK_WIDTH, tileIndex / CHUNK_WIDTH);
                        const i32v2 offset = tileCoord.v - worldPos.v;
                        const i32 distSq = offset.x * offset.x + offset.y * offset.y;
                        if (distSq > maxDistanceSq) {
                            continue;
                        }
                        if (!simChunk.mTileData->tileIndexToTileData.at(tileIndex).flags.isBitSet(SimTileDataFlags::Reserved)) {
                            rv.emplace(distSq, tileCoord.v);
                            if (rv.size() == maxCount) [[unlikely]] {
                                return rv;
                            }
                        }
                    }
                }
            }
            else {
                // If this is not a valid chunk, don't add neighbors
                if (++i == MAX_ITERATIONS) {
                    break;
                }
                continue;
            }
        }

        // Break BEFORE adding new things to the queue
        if (++i == MAX_ITERATIONS) {
            break;
        }
        if (coord.x > 0) {
            const ChunkID leftId = id - 1;
            if (!closedList.contains(leftId)) {
                closedList.insert(leftId);
                chunkQueue[back++] = ChunkCoord(coord.x - 1, coord.y);
            }
        }
        if (coord.x < mWidthChunks - 1) {
            const ChunkID rightId = id + 1;
            if (!closedList.contains(rightId)) {
                closedList.insert(rightId);
                chunkQueue[back++] = ChunkCoord(coord.x + 1, coord.y);
            }
        }
        if (coord.y > 0) {
            const ChunkID downId = id - mWidthChunks;
            if (!closedList.contains(downId)) {
                closedList.insert(downId);
                chunkQueue[back++] = ChunkCoord(coord.x, coord.y - 1);
            }
        }
        if (coord.y < mWidthChunks - 1) {
            const ChunkID upId = id + mWidthChunks;
            if (!closedList.contains(upId)) {
                closedList.insert(upId);
                chunkQueue[back++] = ChunkCoord(coord.x, coord.y + 1);
            }
        }
    } while (front != back);

    return rv;
}

SimChunkTileReservationHandle SimChunkGrid::tryReserveHarvestableAtTilePos(TileCoord worldPos, TileHarvestable harvestable) {
    const ChunkCoord chunkCoord(worldPos);
    const ChunkID id = chunkCoord.toGridIDType(mWidthChunks);
    const TileCoord offset = worldPos - TileCoord(chunkCoord);
    return mChunkData[id].tryReserveHarvestableAtTile(offset.y * CHUNK_WIDTH + offset.x, harvestable);
}

TileItemUID SimChunkGrid::tryDropItemStackOnGroundSimThread(ItemStack stack, TileCoord worldPos) {
    ASSERT_SIM_THREAD();
    const ChunkCoord chunkCoord(worldPos);
    const ChunkID chunkId = chunkCoord.toGridIDType(mWidthChunks);
    const TileCoord offset = worldPos - TileCoord(chunkCoord);
    TileItemUID uid = mChunkData[chunkId].tryDropItemStackOnGround(stack, offset.y * CHUNK_WIDTH + offset.x);
    // TODO: Handle dropping inside buildings
    if (!mChunkData[chunkId].isSimulating()) {
        GameThreadTasks::getInstance().addGenericTask([world = mWorld, worldPos, stack, uid]() {
            f32v2 worldPos2d(worldPos.x + 0.5f, worldPos.y + 0.5f);
            EntityFactory::createItemOnGround(*world, f32v3(worldPos2d.x, worldPos2d.y, world->getTerrainHeightAtPoint(worldPos2d)), stack, uid);
        });
    }
    return uid;
}

TileItemUID SimChunkGrid::tryDropItemStackOnGroundGameThread(ItemStack stack, f32v3 worldPos) {
    ASSERT_GAME_THREAD();
    const ChunkCoord chunkCoord = ChunkCoord::fromTilePos(worldPos);
    const ChunkID chunkId = chunkCoord.toGridIDType(mWidthChunks);
    const TileCoord offset = TileCoord(worldPos) - TileCoord(chunkCoord);
    TileItemUID uid = mChunkData[chunkId].tryDropItemStackOnGround(stack, offset.y * CHUNK_WIDTH + offset.x);
    // TODO: handle worldPos.z as in try to place it inside buildings?
    if (uid != INVALID_TILE_ITEM_UID) {
        EntityFactory::createItemOnGround(*mWorld, f32v3(worldPos.x, worldPos.y, mWorld->getTerrainHeightAtPoint(worldPos)), stack, uid);
    }
    else {
        assert(false);
    }
    return uid;
}

TileItemUID SimChunkGrid::onItemProjectileLandGameThread(ItemStack stack, f32v3 worldPos) {
    ASSERT_GAME_THREAD();
    const ChunkCoord chunkCoord = ChunkCoord::fromTilePos(worldPos);
    const ChunkID chunkId = chunkCoord.toGridIDType(mWidthChunks);
    const TileCoord offset = TileCoord(worldPos) - TileCoord(chunkCoord);
    TileItemUID uid = mChunkData[chunkId].tryDropItemStackOnGround(stack, offset.y * CHUNK_WIDTH + offset.x);
    return uid;
}

SimChunkTileItemReservationPtr SimChunkGrid::tryReserveItemStack(TileCoord worldPos, TileItemUID uid, ItemID itemId, ui16 quantity) {
    assert(uid != INVALID_TILE_ITEM_UID);
    const ChunkCoord chunkCoord(worldPos);
    const ChunkID id = chunkCoord.toGridIDType(mWidthChunks);
    return mChunkData[id].tryReserveItemStack(uid, itemId, quantity);
}

bool SimChunkGrid::hasBlockingTileAtWorldPos(TileCoord worldPos) const {
    const auto [chunkId, tileIndex] = worldPos.toChunkTileIndexAndChunkID(mWidthChunks);
    return mChunkData[chunkId].hasBlockingTileAtIndex(tileIndex);
}

void SimChunkGrid::initInternal() {
    mSpatialGrid.init(CHUNK_WIDTH, mWidthChunks);
    mTotalChunks = SQ(mSpatialGrid.getGridWidthCells());
    mChunkData = std::make_unique<SimChunk[]>(mTotalChunks);
    LOG_DEBUG("Sim chunk grid allocated {} mb data",
        (mTotalChunks * sizeof(SimChunk)) / 1024.f / 1024.f);
    for (ChunkID id = 0; id < mTotalChunks; ++id) {
        mChunkData[id].mChunkID = id;
    }
}

SimTileDataWriteReservation::SimTileDataWriteReservation(SimChunkGrid& grid, ChunkID chunk, ChunkTileIndex tileIndex, SimTileData data) :
    mGrid(&grid),
    mChunk(chunk),
    mTileIndex(tileIndex),
    reservedCopy(data) {

}

SimTileDataWriteReservation::~SimTileDataWriteReservation() {
    if (!mDidRelease) {
        copyBackAndRelease();
    }
}

void SimTileDataWriteReservation::copyBackAndRelease() {
    assert(!mDidRelease);
    mGrid->releaseTileDataReservationAndCopyData(*this);
}

POOLED_ALLOC_DEF_THREADSAFE(SimTileDataWriteReservation, 128);
