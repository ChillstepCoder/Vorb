#include "stdafx.h"
#include "LocalChunkGrid.h"

#include "world/World.h"
#include "building/BuildingGrid.h"
#include "tile/TileContainerRepository.h"

#include "world/ecosystem/FishEcosystem.h"

// TODO: SrvChunkGrid?

#include "services/Services.h"
#include "pathfinding/NavWorld.h"

constexpr ui8 ALL_NEIGHBORS_ALIVE = 0xff;

// REFRESH MAIN THREAD(Update when load center moves N tiles from previous position)
// IWORLD
// 0. Iterate all mLoadingChunks and mActiveChunks, if they are out of range, add them to mDestroyingChunksand clear their world bits
// 1. Efficient iterate and set bitmask, find and set new bits in rangeand add their chunks to mLoading
// new 1 bits are added to mLoadingChunks state set to WAITING_HEIGHT or WAITING_GENERATION based on if height already exists
//
// If a loading chunk is added to mDestroyingChunks, thats fine, when iterating mDestroying or mLoading we always handle chunk state
// so we can wait for any threads to finish work before we move
// 3. Once a chunk finishes loading, it is moved to mActiveChunks
//
//Terrain quadtree is purely rendering

LocalChunkGrid::LocalChunkGrid() = default;

LocalChunkGrid::~LocalChunkGrid() {
    // Decref all chunks before destroying them
    for (ChunkID id : mActiveChunks) {
        mChunks[id].decRef();
    }
}

void LocalChunkGrid::onWorldBegin() {

    IHeightmapGrid& heightmapGrid = mWorld->getHeightmapGrid();
    heightmapGrid.registerIHeightmapGridListeners(mHeightmapGridListeners);
    heightmapGrid.addEditVertsListener(mHeightmapGridListeners, [this](const HeightmapGridEvent& gridEvent) {
        assert(gridEvent.mEventType == HeightmapGridEventType::EditVerts);
        assert(gridEvent.mModifiedVerts);
        onTerrainModified(*gridEvent.mModifiedVerts);
    });
}

void LocalChunkGrid::tick(f32v2 localPlayerPosition) {
    ASSERT_GAME_THREAD();

    int activationsRemainingThisFrame = 5;
    for (size_t i = 0; (i < mWantActivateChunks.size()) && activationsRemainingThisFrame;) {
        LocalChunk& chunk = mChunks[mWantActivateChunks[i]];
        if (chunk.mState == ChunkState::DESTROYING_ON_SIM) {
            ++i;
            continue;
        }
        else {
            // Begin load
            chunk.mFlags.clearBit(ChunkFlags::IN_WANT_ACTIVATE_LIST);
            chunk.allocateData();

            addChunkToActivatingList(chunk);

            ChunkGridEvent evnt(chunk);
            dispatchBeginActivate(evnt);
            mWantActivateChunks[i] = mWantActivateChunks.back();
            mWantActivateChunks.pop_back();
            --activationsRemainingThisFrame;
        }
    }

    // Update all activating chunks
    updateActivatingChunks();

    // Update all destroying chunks
    IHeightmapGrid& heightGrid = mWorld->getHeightmapGrid();
    for (size_t i = 0; i < mWantDeactivateChunks.size();) {
        LocalChunk& chunk = mChunks[mWantDeactivateChunks[i]];

        // Prevent a very rare race condition
        if (chunk.mTileContainer) {
            chunk.mTileContainer->mLifetimeMutex.lock(); // LOCK
            if (chunk.getRefCount() == 0) {
                // See TileContainer::tryAquireThreadSafe for why we need this lock
                chunk.mTileContainer->mLifetimeMutex.unlock(); // UNLOCK
                // Release height and notify only if we were ever valid
                if (chunk.mState != ChunkState::DEACTIVATED) {
                    ChunkGridEvent evnt(chunk);
                    dispatchDeactivated(evnt);
                }
                chunk.dispose();
                mWantDeactivateChunks[i] = mWantDeactivateChunks.back();
                mWantDeactivateChunks.pop_back();
            }
            else {
                chunk.mTileContainer->mLifetimeMutex.unlock(); // UNLOCK
                ++i;
            }
        }
        else {
            if (chunk.getRefCount() == 0) {
                // Release height and notify only if we were ever valid
                if (chunk.mState != ChunkState::DEACTIVATED) {
                    ChunkGridEvent evnt(chunk);
                    dispatchDeactivated(evnt);
                }
                chunk.dispose();
                mWantDeactivateChunks[i] = mWantDeactivateChunks.back();
                mWantDeactivateChunks.pop_back();
            }
            else {
                ++i;
            }
        }
    }
}

LocalChunk& LocalChunkGrid::getChunkAtPosition(const f32v2& worldPos) {
    return getChunk(getChunkIDFromWorldPos(worldPos));
}

const LocalChunk& LocalChunkGrid::getChunkAtPosition(const f32v2& worldPos) const {
    return getChunk(getChunkIDFromWorldPos(worldPos));
}

void LocalChunkGrid::getClosestChunksAtPosition(const f32v2& worldPos, OUT const LocalChunk* chunks[4]) const {
    const i32v2 worldPosInt(worldPos);
    ChunkID chunkId = getChunkIDFromWorldPos(worldPosInt);
    chunks[0] = &getChunk(chunkId);
    int i = 1;
    if (worldPosInt.x % CHUNK_WIDTH < HALF_CHUNK_WIDTH) {
        // West
        if (worldPosInt.y % CHUNK_WIDTH < HALF_CHUNK_WIDTH) {
            // South
            chunks[i++] = &getChunk(chunkId - mWidthChunks - 1);
            chunks[i++] = &getChunk(chunkId - mWidthChunks);
            chunks[i++] = &getChunk(chunkId - 1);
        }
        else {
            // North
            chunks[i++] = &getChunk(chunkId + mWidthChunks - 1);
            chunks[i++] = &getChunk(chunkId + mWidthChunks);
            chunks[i++] = &getChunk(chunkId - 1);
        }
    }
    else {
        // East
        if (worldPosInt.y % CHUNK_WIDTH < HALF_CHUNK_WIDTH) {
            // South
            chunks[i++] = &getChunk(chunkId - mWidthChunks + 1);
            chunks[i++] = &getChunk(chunkId - mWidthChunks);
            chunks[i++] = &getChunk(chunkId + 1);
        }
        else {
            // North
            chunks[i++] = &getChunk(chunkId + mWidthChunks + 1);
            chunks[i++] = &getChunk(chunkId + mWidthChunks);
            chunks[i++] = &getChunk(chunkId + 1);
        }
    }
}

LocalChunk& LocalChunkGrid::getChunkAtPosition(const i32v2& worldPos) {
    return getChunk(getChunkIDFromWorldPos(worldPos));
}

const LocalChunk& LocalChunkGrid::getChunkAtPosition(const i32v2& worldPos) const {
    return getChunk(getChunkIDFromWorldPos(worldPos));
}

LocalChunk& LocalChunkGrid::getChunkAtChunkOffset(const i32v2& chunkOffset) {
    assert(chunkOffset.x >= 0 && chunkOffset.y >= 0);
    return getChunk(ChunkID(chunkOffset.y * mWidthChunks + chunkOffset.x));
}

const LocalChunk& LocalChunkGrid::getChunkAtChunkOffset(const i32v2& chunkOffset) const {
    assert(chunkOffset.x >= 0 && chunkOffset.y >= 0);
    return getChunk(ChunkID(chunkOffset.y * mWidthChunks + chunkOffset.x));
}

ChunkID LocalChunkGrid::getChunkIDFromWorldPos(const i32v2& worldPos) const {
    assert(worldPos.x >= 0 && worldPos.y >= 0);
    return (worldPos.y / CHUNK_WIDTH) * mWidthChunks + worldPos.x / CHUNK_WIDTH;
}

ChunkID LocalChunkGrid::getChunkIDFromWorldPos(const f32v2& worldPos) const {
    assert(worldPos.x >= 0 && worldPos.y >= 0);
    return ((int)worldPos.y / CHUNK_WIDTH) * mWidthChunks + (int)worldPos.x / CHUNK_WIDTH;
}

ChunkID LocalChunkGrid::getChunkIDFromChunkOffset(const i32v2& chunkOffset) const {
    return (chunkOffset.y * mWidthChunks + chunkOffset.x);
}

i32v2 LocalChunkGrid::getWorldPosXYFromChunkID(ChunkID id) const {
    return i32v2((id % mWidthChunks) * CHUNK_WIDTH, (id / mWidthChunks) * CHUNK_WIDTH);
}

i32v2 LocalChunkGrid::getChunkOffsetFromChunkID(ChunkID id) const {
    return i32v2(id % mWidthChunks, id / mWidthChunks);
}

void LocalChunkGrid::setWorldAndAllocateChunks(World& world) {
    mWorld = &world;
    mWidthChunks = world.getWidthChunks();
    mTotalChunks = SQ(mWidthChunks);
    mAliveChunkBits.resizeAndZero(mTotalChunks);
    mChunks = std::make_unique<LocalChunk[]>(mTotalChunks);

    f32 chunksSize = mTotalChunks * sizeof(LocalChunk) / 1024.f;
    LOG_DEBUG("Local chunk grid allocated {} kb of chunks", chunksSize);
    //f32 worstCaseTileData = 0.5f * mTotalChunks * CHUNK_SIZE * sizeof(SimulatedChunk::ChunkHarvestableTile) / 1024.f;
    //LOG_DEBUG("                     {} kb of est sim tile data", worstCaseTileData);
    LOG_DEBUG("                     {} mb total", (chunksSize /*+ worstCaseTileData*/) / 1024.f);
    for (ChunkID i = 0; i < mTotalChunks; ++i) {
        mChunks[i].init(world, i, getWorldPosXYFromChunkID(i));
    }
}

void LocalChunkGrid::setChunkActive(ChunkID chunkId) {
    ASSERT_GAME_THREAD();
    PROFILE_FUNCTION();
    // Any time grid state changes we will update again
    LocalChunk& chunk = mChunks[chunkId];

    // We must wait for sim destroy to complete
    if (chunk.mState != ChunkState::DESTROYING_ON_SIM) {
        chunk.setState(ChunkState::WAITING_SIM_RELEASE);
    }

    assert(!mAliveChunkBits.getBit(chunkId)); // I added this as I suspect IChunkGrid::updateGridEdges can result in this
    mAliveChunkBits.setBit(chunkId);

    // Check if we need to remove from destroy list first
    if (chunk.mFlags.isBitSet(ChunkFlags::IN_DESTROY_LIST)) {
        removeChunkFromWantDeactivateList(chunk);
    }

    chunk.mFlags.setBit(ChunkFlags::IN_WANT_ACTIVATE_LIST);
    mWantActivateChunks.emplace_back(chunkId);
}

void LocalChunkGrid::setChunkInactive(ChunkID chunkId) {
    ASSERT_GAME_THREAD();
    PROFILE_FUNCTION();
    LocalChunk& chunk = mChunks[chunkId];
    assert(!chunk.mFlags.isBitSet(ChunkFlags::IS_ACTIVATING));
    assert(!chunk.mFlags.isBitSet(ChunkFlags::IN_DESTROY_LIST));

    if (chunk.mFlags.isBitSet(ChunkFlags::IN_ACTIVE_LIST)) {
        removeChunkFromActiveList(chunk);
    }

    if (chunk.mFlags.isBitSet(ChunkFlags::IN_WANT_ACTIVATE_LIST)) {
        removeChunkFromWantActivateList(chunk);
    }

    const ChunkID id = chunk.getChunkID();
    mAliveChunkBits.clearBit(id);

    chunk.mFlags.setBit(ChunkFlags::IN_DESTROY_LIST);
    if (chunk.mTileContainer) {
        chunk.mTileContainer->mPendingDestroy = true;
    }
    mWantDeactivateChunks.emplace_back(id);
}

void LocalChunkGrid::updateActivatingChunks() {
    IHeightmapGrid& heightGrid = mWorld->getHeightmapGrid();
    // Reverse iteration for simplicity of pop swap
    for (int i = (int)mActivatingChunks.size() - 1; i >= 0; --i) {
        LocalChunk& chunk = mChunks[mActivatingChunks[i]];
        switch (chunk.mState) {
            case ChunkState::WAITING_SIM_RELEASE: {
                break;
            }
            case ChunkState::READY_TO_LOAD: {
                chunk.setState(ChunkState::LOADING_TILES);
                // TileContainerLoader and BuildingGrid will listen for this event and begin loading
                ChunkGridEvent evnt(chunk);
                dispatchBeginLoad(evnt);
                break;
            }
            case ChunkState::LOADING_TILES: {
                break;
            }
            case ChunkState::WAITING_BUILDINGS: {
                
                if (mWorld->getBuildingGrid().allBuildingsLoadedAtChunk(chunk.getChunkID())) {

                    // TODO: Why does this cause a crash due to corruption when we run in threadpool?
                    chunk.mTileContainer->mHarvestableRegistry.refreshFromOwner();
                    assert(chunk.mTileContainer->mHarvestableRegistry.getRegistryCount() == 64);

                    // Ecosystem
                    // TODO: This can be partially async in the TileContainerLoader step?
                    mWorld->getFishEcosystem().initChunkFish(chunk);

                    // Begin nav load
                    if (NavWorld* navWorld = mWorld->tryGetNavWorld()) {
                        navWorld->markContainerNavDirty(chunk.mTileContainer);
                    }
                    else {
                        // TODO: THIS IS ONLY FOR EDITOR WORLD
                        chunk.mTileContainer->setDidInitNav();
                    }

                    // Tile container loaded
                    chunk.mTileContainer->setState(TileContainerState::READY);

                    // Connect buildings
                    mWorld->getBuildingGrid().connectBuildingsToChunk(chunk);


                    // Dispatch load finished
                    TileContainerEvent loadFinishedEvent;
                    loadFinishedEvent.container = chunk.mTileContainer;
                    loadFinishedEvent.containerId = chunk.mTileContainer->getId();
                    mWorld->getTileContainerRepository().dispatchLoadFinished(loadFinishedEvent);

                    chunk.setState(ChunkState::LOADING_MESH_PHYSICS_NAV);
                }
                break;
            }
            case ChunkState::LOADING_MESH_PHYSICS_NAV: {
                if (chunk.mTileContainer->didInitMeshPhysicsAndNav()) {
                    mActivatingChunks[i] = mActivatingChunks.back();
                    mActivatingChunks.pop_back();
                    activateChunk(chunk);
                }
                break;
            }
            default:
                assert(false);
        }
    }
}

void LocalChunkGrid::onTerrainModified(const boost::container::flat_set<i32v2>& modifiedPositions) {
    PROFILE_FUNCTION();
    UnorderedFlatMap<GridIdType, std::vector<TileCoord>> tilePositionsNeedingUpdate;
    {
        constexpr ui32 MAX_TILES_CHANGED_PER_POSITION = SQ(HEIGHTMAP_QUAD_SIZE * HEIGHTMAP_QUAD_SIZE);
        tilePositionsNeedingUpdate.reserve(modifiedPositions.size() * MAX_TILES_CHANGED_PER_POSITION);
        for (const i32v2& pos : modifiedPositions) {
            // Insert the 16 surrounding tiles
            for (int y = -2; y < 2; ++y) {
                for (int x = -2; x < 2; ++x) {
                    const TileCoord newPos(pos + i32v2(x, y));
                    tilePositionsNeedingUpdate[getChunkIDFromWorldPos(newPos.v)].emplace_back(newPos);
                }
            }
        }
        static_assert(HEIGHTMAP_QUAD_SIZE == 2 && MAX_TILES_CHANGED_PER_POSITION == 16, "Update logic");
    }
    IHeightmapGrid& heightmapGrid = mWorld->getHeightmapGrid();

    std::vector<std::pair<TileIndex, f32>> editData;
    for (auto&& it : tilePositionsNeedingUpdate) {
        LocalChunk& chunk = getChunk(it.first);
        if (chunk.isActivated()) {
            editData.reserve(it.second.size());
            for (auto&& pos : it.second) {
                const ui32 x = (ui32)pos.x & (CHUNK_WIDTH - 1); // Fast modulus
                const ui32 y = (ui32)pos.y & (CHUNK_WIDTH - 1); // Fast modulus
                editData.emplace_back(std::make_pair(y * CHUNK_WIDTH + x, heightmapGrid.computeCenterHeightAtTile<false>(pos)));
            }
            chunk.getTileContainer()->bulkSetTileGroundZPosition(editData.data(), editData.size());
            editData.clear();
        }
    }
}

void LocalChunkGrid::addChunkToActiveList(LocalChunk& chunk) {
    mActiveChunks.emplace_back(chunk.getChunkID());
    assert(!chunk.mFlags.isBitSet(ChunkFlags::IN_ACTIVE_LIST));
    chunk.mFlags.setBit(ChunkFlags::IN_ACTIVE_LIST);
    chunk.incRef();
}

void LocalChunkGrid::removeChunkFromActiveList(LocalChunk& chunk) {
    // TODO: Eliminate linear search? Do we care?
    ChunkID chunkId = chunk.getChunkID();
    for (size_t i = 0; i < mActiveChunks.size(); ++i) {
        if (mActiveChunks[i] == chunkId) {
            // Pop and swap
            mActiveChunks[i] = mActiveChunks.back();
            mActiveChunks.pop_back();
            chunk.mFlags.clearBit(ChunkFlags::IN_ACTIVE_LIST);
            break;
        }
    }
    // TODO: Hit a crash here when quickly zooming around in a spiral
    chunk.decRef();
    // Make sure we removed
    assert(!chunk.mFlags.isBitSet(ChunkFlags::IN_ACTIVE_LIST));
}

void LocalChunkGrid::removeChunkFromWantActivateList(LocalChunk& chunk) {
    ChunkID chunkId = chunk.getChunkID();
    for (size_t i = 0; i < mWantActivateChunks.size(); ++i) {
        if (mWantActivateChunks[i] == chunkId) {
            // Pop and swap
            mWantActivateChunks[i] = mWantActivateChunks.back();
            mWantActivateChunks.pop_back();
            chunk.mFlags.clearBit(ChunkFlags::IN_WANT_ACTIVATE_LIST);
            break;
        }
    }
}

void LocalChunkGrid::addChunkToActivatingList(LocalChunk& chunk) {
    mActivatingChunks.emplace_back(chunk.getChunkID());
    assert(!chunk.mFlags.isBitSet(ChunkFlags::IS_ACTIVATING));
    chunk.mFlags.setBit(ChunkFlags::IS_ACTIVATING);
}

void LocalChunkGrid::removeChunkFromWantDeactivateList(LocalChunk& chunk) {
    // TODO: Eliminate linear search? Do we care?
    ChunkID chunkId = chunk.getChunkID();
    for (size_t i = 0; i < mWantDeactivateChunks.size(); ++i) {
        if (mWantDeactivateChunks[i] == chunkId) {
            // Pop and swap
            mWantDeactivateChunks[i] = mWantDeactivateChunks.back();
            mWantDeactivateChunks.pop_back();
            chunk.mFlags.clearBit(ChunkFlags::IN_DESTROY_LIST);
            break;
        }
    }
    // Make sure we removed
    assert(!chunk.mFlags.isBitSet(ChunkFlags::IN_DESTROY_LIST));
    if (chunk.mTileContainer) {
        chunk.mTileContainer->mPendingDestroy = false;
    }
}

void LocalChunkGrid::activateChunk(LocalChunk& chunk) {
    assert(chunk.getTileContainer()->getState() == TileContainerState::READY);
    assert(chunk.mFlags.isBitSet(ChunkFlags::IS_ACTIVATING));
    chunk.mFlags.clearBit(ChunkFlags::IS_ACTIVATING);
    chunk.mState = ChunkState::ACTIVATED;
    addChunkToActiveList(chunk);

    // Notify observers
    ChunkGridEvent evnt(chunk);
    dispatchActivated(evnt);
    TileContainerEvent event{ chunk.mTileContainer, {} };
    mWorld->getTileContainerRepository().dispatchActivated(event);
}
