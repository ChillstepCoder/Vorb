#include "stdafx.h"
#include "IChunkGrid.h"

#include "generation/ChunkGenerator.h"
#include "world/World.h"
#include "tile/TileContainerRepository.h"

#include "world/ecosystem/FishEcosystem.h"

// TODO: SrvChunkGrid?

#include "services/Services.h"
#include "pathfinding/NavWorld.h"

#include "options/DebugOptions.h"

// Meshing
#include "rendering/mesh/mesher/ChunkMesher.h"

// How many tiles the load center has to move before we force update edge chunks
constexpr f32 DISTANCE_SQ_CHANGE_UNTIL_FORCE_UPDATE_EDGES = SQ(16.0f);
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

bool isChunkInLoadRange(const f32v2& worldPos, const f32v2& loadCenter) {
    const f32v2 centerPos = worldPos + f32v2(HALF_CHUNK_WIDTH);
    const f32 distSq = glm::length2(centerPos - loadCenter);
    return distSq <= sDebugOptions.mLoadRangeSq;
}

IChunkGrid::IChunkGrid() = default;

IChunkGrid::~IChunkGrid() {
    // Decref all chunks before destroying them
    for (ChunkID id : mActiveChunks) {
        mChunks[id].decRef();
    }
}

void IChunkGrid::onWorldBegin(const f32v2& loadCenter) {

    IHeightmapGrid& heightmapGrid = mWorld->getHeightmapGrid();
    heightmapGrid.registerIHeightmapGridListeners(mHeightmapGridListeners);
    heightmapGrid.addEditVertsListener(mHeightmapGridListeners, [this](const HeightmapGridEvent& gridEvent) {
        assert(gridEvent.mEventType == HeightmapGridEventType::EditVerts);
        assert(gridEvent.mModifiedVerts);
        onTerrainModified(*gridEvent.mModifiedVerts);
    });

    // Update the grid until we have no further updates
    do {
        updateGridEdges(loadCenter);
    } while (mForceUpdateEdgeChunks);
}

void IChunkGrid::tick(const f32v2& loadCenter) {
    ASSERT_GAME_THREAD();

    // Check if we need to force update
    if (mForceUpdateEdgeChunks || (glm::length2(loadCenter - mPrevLoadCenter) > DISTANCE_SQ_CHANGE_UNTIL_FORCE_UPDATE_EDGES)) {
        updateGridEdges(loadCenter);
    }

    // Update all loading chunks
    updateLoadingChunks();

    // Update all destroying chunks
    IHeightmapGrid& heightGrid = mWorld->getHeightmapGrid();
    for (size_t i = 0; i < mWantDeactivateChunks.size();) {
        Chunk& chunk = mChunks[mWantDeactivateChunks[i]];

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


Chunk& IChunkGrid::getChunkAtPosition(const f32v2& worldPos) {
    return getChunk(getChunkIDFromWorldPos(worldPos));
}

const Chunk& IChunkGrid::getChunkAtPosition(const f32v2& worldPos) const {
    return getChunk(getChunkIDFromWorldPos(worldPos));
}

void IChunkGrid::getClosestChunksAtPosition(const f32v2& worldPos, OUT const Chunk* chunks[4]) const {
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

Chunk& IChunkGrid::getChunkAtPosition(const i32v2& worldPos) {
    return getChunk(getChunkIDFromWorldPos(worldPos));
}

const Chunk& IChunkGrid::getChunkAtPosition(const i32v2& worldPos) const {
    return getChunk(getChunkIDFromWorldPos(worldPos));
}

Chunk& IChunkGrid::getChunkAtChunkOffset(const i32v2& chunkOffset) {
    assert(chunkOffset.x >= 0 && chunkOffset.y >= 0);
    return getChunk(ChunkID(chunkOffset.y * mWidthChunks + chunkOffset.x));
}

const Chunk& IChunkGrid::getChunkAtChunkOffset(const i32v2& chunkOffset) const {
    assert(chunkOffset.x >= 0 && chunkOffset.y >= 0);
    return getChunk(ChunkID(chunkOffset.y * mWidthChunks + chunkOffset.x));
}

ChunkID IChunkGrid::getChunkIDFromWorldPos(const i32v2& worldPos) const {
    assert(worldPos.x >= 0 && worldPos.y >= 0);
    return (worldPos.y / CHUNK_WIDTH) * mWidthChunks + worldPos.x / CHUNK_WIDTH;
}

ChunkID IChunkGrid::getChunkIDFromWorldPos(const f32v2& worldPos) const {
    assert(worldPos.x >= 0 && worldPos.y >= 0);
    return ((int)worldPos.y / CHUNK_WIDTH) * mWidthChunks + (int)worldPos.x / CHUNK_WIDTH;
}

ChunkID IChunkGrid::getChunkIDFromChunkOffset(const i32v2& chunkOffset) const {
    return (chunkOffset.y * mWidthChunks + chunkOffset.x);
}

i32v2 IChunkGrid::getWorldPosXYFromChunkID(ChunkID id) const {
    return i32v2((id % mWidthChunks) * CHUNK_WIDTH, (id / mWidthChunks) * CHUNK_WIDTH);
}

i32v2 IChunkGrid::getChunkOffsetFromChunkID(ChunkID id) const {
    return i32v2(id % mWidthChunks, id / mWidthChunks);
}

void IChunkGrid::setWorldAndAllocateChunks(World& world) {
    mWorld = &world;
    mWidthChunks = world.getWidthChunks();
    mTotalChunks = SQ(mWidthChunks);
    mAliveChunkBits.resizeAndZero(mTotalChunks);
    mNeighborBits = std::make_unique<ui8[]>(mTotalChunks);
    memset(mNeighborBits.get(), 0, sizeof(ui8) * mTotalChunks);
    mChunks = std::make_unique<Chunk[]>(mTotalChunks);

    f32 chunksSize = mTotalChunks * sizeof(Chunk) / 1024.f;
    LOG_DEBUG("Chunk grid allocated {} kb of chunks", chunksSize);
    f32 simSize = mTotalChunks * sizeof(SimulatedChunk) / 1024.f;
    LOG_DEBUG("                     {} kb of simulated chunks", simSize);
    //f32 worstCaseTileData = 0.5f * mTotalChunks * CHUNK_SIZE * sizeof(SimulatedChunk::ChunkHarvestableTile) / 1024.f;
    //LOG_DEBUG("                     {} kb of est sim tile data", worstCaseTileData);
    LOG_DEBUG("                     {} mb total", (chunksSize + simSize /*+ worstCaseTileData*/) / 1024.f);
    for (ChunkID i = 0; i < mTotalChunks; ++i) {
        mChunks[i].init(world, i, getWorldPosXYFromChunkID(i));
    }
}

void IChunkGrid::updateLoadingChunks() {
    IHeightmapGrid& heightGrid = mWorld->getHeightmapGrid();
    for (size_t i = 0; i < mActivatingChunks.size();) {
        Chunk& chunk = mChunks[mActivatingChunks[i]];
        switch (chunk.mState) {
            case ChunkState::LOADING_TILES: {
                ++i;
                break;
            }
            case ChunkState::LOADING_MESH_PHYSICS_NAV_VISIBILITY: {
                if (chunk.mTileContainer->didInitMeshPhysicsAndNav()) {
                    mActivatingChunks[i] = mActivatingChunks.back();
                    mActivatingChunks.pop_back();
                    chunk.mFlags.clearBit(ChunkFlags::IN_LOAD_LIST);
                    onChunkReady(chunk);
                }
                else {
                    ++i;
                }
                break;
            }
            default:
                assert(false);
        }
    }
}

bool IChunkGrid::isChunkXYInBounds(const i32v2& xy) {
    return (xy.x >= 0 && xy.y >= 0 && xy.x < mWidthChunks && xy.y < mWidthChunks);
}

void IChunkGrid::onTerrainModified(const boost::container::flat_set<i32v2>& modifiedPositions) {
    PROFILE_FUNCTION();
    boost::container::flat_map<GridIdType, std::vector<i32v2>> tilePositionsNeedingUpdate;
    {
        constexpr ui32 MAX_TILES_CHANGED_PER_POSITION = SQ(HEIGHTMAP_QUAD_SIZE * HEIGHTMAP_QUAD_SIZE);
        tilePositionsNeedingUpdate.reserve(modifiedPositions.size() * MAX_TILES_CHANGED_PER_POSITION);
        for (const i32v2& pos : modifiedPositions) {
            // Insert the 16 surrounding tiles
            for (int y = -2; y < 2; ++y) {
                for (int x = -2; x < 2; ++x) {
                    const i32v2 newPos = pos + i32v2(x, y);
                    tilePositionsNeedingUpdate[getChunkIDFromWorldPos(newPos)].emplace_back(newPos);
                }
            }
        }
        static_assert(HEIGHTMAP_QUAD_SIZE == 2 && MAX_TILES_CHANGED_PER_POSITION == 16, "Update logic");
    }
    IHeightmapGrid& heightmapGrid = mWorld->getHeightmapGrid();

    std::vector<std::pair<TileIndex, f32>> editData;
    for (auto&& it : tilePositionsNeedingUpdate) {
        Chunk& chunk = getChunk(it.first);
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

void IChunkGrid::updateGridEdges(const f32v2& loadCenter) {
    PROFILE_FUNCTION();
    // This will be set if we mutate any data
    mForceUpdateEdgeChunks = false;
    mPrevLoadCenter = loadCenter;

    // Make sure center chunk is alive
    const ChunkID centerId = getChunkIDFromWorldPos(loadCenter);
    if (mAliveChunkBits.getBit(centerId) == false) {
        tryMarkChunkAlive(centerId);
    }

    // Reverse iterate the edge positions so new edge chunks don't usually get processed this frame
    for (int i = (int)mEdgeChunkPositions.size() - 1; i >= 0; --i) {
        // This can happen because makeChunkAlive mutates the edge positions array
        if (i >= mEdgeChunkPositions.size()) i = mEdgeChunkPositions.size() - 1;

        const ChunkID chunkId = mEdgeChunkPositions[i];
        if (isChunkInLoadRange(getWorldPosXYFromChunkID(chunkId), loadCenter)) {
            // Try to load any unloaded neighbors
            const ui8& neighborBits = mNeighborBits[chunkId];
            const i32v2 xy = getChunkOffsetFromChunkID(chunkId);
            for (ui8 i = 0; i < 8; ++i) {
                if ((neighborBits & (1 << i)) == 0) {
                    const i32v2 neighborXy = xy + CARTESIAN8_DIR_OFFSETS[i];
                    if (isChunkXYInBounds(neighborXy)) {
                        const ChunkID neighborId = getChunkIDFromChunkOffset(neighborXy);
                        if (isChunkInLoadRange(getWorldPosXYFromChunkID(neighborId), loadCenter)) {
                            tryMarkChunkAlive(neighborId);
                        }
                    }
                }
            }
        }
        else {
            // Destroying chunks are not edge chunks since they aren't alive
            Chunk& chunk = mChunks[chunkId];
            if (!chunk.mFlags.isBitSet(ChunkFlags::IN_LOAD_LIST)) {
                mEdgeChunkPositions[i] = mEdgeChunkPositions.back();
                mEdgeChunkPositions.pop_back();
                chunk.mFlags.clearBit(ChunkFlags::IN_EDGE_LIST);
                mForceUpdateEdgeChunks = true;
                // Chunk is now dead and destroying
                addChunkToWantDeactivateList(chunk);
            }
        }
    }
}

bool IChunkGrid::tryMarkChunkAlive(const ChunkID& chunkId) {
    PROFILE_FUNCTION();
    // Any time grid state changes we will update again
    Chunk& chunk = mChunks[chunkId];

    // We must wait for sim destroy to complete
    if (chunk.mState == ChunkState::DESTROYING_ON_SIM) {
        return false;
    }

    mForceUpdateEdgeChunks = true;
    assert(!mAliveChunkBits.getBit(chunkId)); // I added this as I suspect IChunkGrid::updateGridEdges can result in this
    mAliveChunkBits.setBit(chunkId);

    // Check if we need to remove from destroy list first
    if (chunk.mFlags.isBitSet(ChunkFlags::IN_DESTROY_LIST)) {
        removeChunkFromWantDeactivateList(chunk);
    }

    ui8& neighborBits = mNeighborBits[chunkId];
    assert(neighborBits == 0);
    const i32v2 xy = getChunkOffsetFromChunkID(chunkId);
    for (ui8 i = 0; i < 8; ++i) {
        const i32v2 neighborXy = xy + CARTESIAN8_DIR_OFFSETS[i];
        if (isChunkXYInBounds(neighborXy)) {
            const ChunkID neighborId = getChunkIDFromChunkOffset(neighborXy);
            if (mAliveChunkBits.getBit(neighborId)) {
                // Create the alive neighbor connection
                neighborBits |= (1ui8 << i);
                ui8& adjacentNeighborBits = mNeighborBits[neighborId];
                adjacentNeighborBits |= (1ui8 << (ui8)CARTESIAN8_OPPOSITES[i]);
                if (adjacentNeighborBits == ALL_NEIGHBORS_ALIVE) {
                    onAllNeighborsAlive(mChunks[neighborId]);
                }
            }
        }
    }
    if (neighborBits == ALL_NEIGHBORS_ALIVE) {
        onAllNeighborsAlive(chunk);
    }
    else {
        mEdgeChunkPositions.emplace_back(chunkId);
        chunk.mFlags.setBit(ChunkFlags::IN_EDGE_LIST);
    }

    return true;
}

void IChunkGrid::addChunkToActiveList(Chunk& chunk) {
    mActiveChunks.emplace_back(chunk.getChunkID());
    assert(!chunk.mFlags.isBitSet(ChunkFlags::IN_ACTIVE_LIST));
    chunk.mFlags.setBit(ChunkFlags::IN_ACTIVE_LIST);
    chunk.incRef();
}

void IChunkGrid::removeChunkFromActiveList(Chunk& chunk) {
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
    chunk.decRef();
    // Make sure we removed
    assert(!chunk.mFlags.isBitSet(ChunkFlags::IN_ACTIVE_LIST));
}

void IChunkGrid::addChunkToActivatingList(Chunk& chunk) {
    mActivatingChunks.emplace_back(chunk.getChunkID());
    assert(!chunk.mFlags.isBitSet(ChunkFlags::IN_LOAD_LIST));
    chunk.mFlags.setBit(ChunkFlags::IN_LOAD_LIST);
}

void IChunkGrid::addChunkToWantDeactivateList(Chunk& chunk) {
    PROFILE_FUNCTION();

    assert(!chunk.mFlags.isBitSet(ChunkFlags::IN_LOAD_LIST));
    assert(!chunk.mFlags.isBitSet(ChunkFlags::IN_DESTROY_LIST));

    if (chunk.mFlags.isBitSet(ChunkFlags::IN_ACTIVE_LIST)) {
        removeChunkFromActiveList(chunk);
    }
   /* else if (chunk.mFlags.isBitSet(ChunkFlags::IN_DORMANT_LIST)) {
        removeChunkFromDormantList(chunk);
    }*/

    const ChunkID id = chunk.getChunkID();
    // We are destroying so we have no neighbor bits
    mNeighborBits[id] = 0;
    mAliveChunkBits.clearBit(id);

    // Notify alive neighbors
    const i32v2 xy = chunk.getChunkOffset();
    for (ui8 i = 0; i < 8; ++i) {
        const i32v2 neighborXy = xy + CARTESIAN8_DIR_OFFSETS[i];
        if (isChunkXYInBounds(neighborXy)) {
            const ChunkID neighborId = getChunkIDFromChunkOffset(neighborXy);
            if (mAliveChunkBits.getBit(neighborId)) {
                ui8& adjacentNeighborBits = mNeighborBits[neighborId];
                // If neighbor wasn't an edge, make him one
                if (adjacentNeighborBits == ALL_NEIGHBORS_ALIVE) {
                    mEdgeChunkPositions.emplace_back(neighborId);
                    mChunks[neighborId].mFlags.setBit(ChunkFlags::IN_EDGE_LIST);
                    // TODO: Deactivate neighbor?
                }
                // Remove our bit
                adjacentNeighborBits &= ~(1ui8 << (ui8)CARTESIAN8_OPPOSITES[i]);
            }
        }
    }

    chunk.mFlags.setBit(ChunkFlags::IN_DESTROY_LIST);
    if (chunk.mTileContainer) {
        chunk.mTileContainer->mPendingDestroy = true;
    }
    mWantDeactivateChunks.emplace_back(id);
}

void IChunkGrid::removeChunkFromWantDeactivateList(Chunk& chunk) {
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

void IChunkGrid::onAllNeighborsAlive(Chunk& chunk) {
    PROFILE_FUNCTION();
    // Once all neighbors are alive, we can begin loading
    // Only begin load if we are flagged as "Invalid" since otherwise we never disposed, and we can just keep our old state

    IHeightmapGrid& heightGrid = mWorld->getHeightmapGrid();

    if (chunk.mFlags.isBitSet(ChunkFlags::IN_EDGE_LIST)) {
        ChunkID id = chunk.getChunkID();
        for (size_t i = 0; i < mEdgeChunkPositions.size(); ++i) {
            if (mEdgeChunkPositions[i] == id) {
                mEdgeChunkPositions[i] = mEdgeChunkPositions.back();
                mEdgeChunkPositions.pop_back();
                chunk.mFlags.clearBit(ChunkFlags::IN_EDGE_LIST);
                break;
            }
        }
        assert(!chunk.mFlags.isBitSet(ChunkFlags::IN_EDGE_LIST));
    }
    // If we are still in any load or active list, its because we lost and regained a neighbor at some point, just ignore
    if (chunk.mFlags.isBitSet(ChunkFlags::IN_ACTIVE_LIST) || chunk.mFlags.isBitSet(ChunkFlags::IN_LOAD_LIST)) {
        return;
    }

    ChunkState state = chunk.mState;
    switch (state) {
        case ChunkState::DEACTIVATED: {
            // Begin load
            chunk.beginActivate();
            addChunkToActivatingList(chunk);
            // TileContainerLoader will listen for this event and begin loading
            ChunkGridEvent evnt(chunk);
            dispatchBeginActivate(evnt);
            break;
        }
        case ChunkState::LOADING_TILES:
            panic("Tried to re-load chunk already being loaded");
            break;
        case ChunkState::DORMANT:
            break;
        case ChunkState::LOADING_MESH_PHYSICS_NAV_VISIBILITY:
            panic("Tried to re-load chunk already being loaded (mesh)");
            break;
        case ChunkState::ACTIVATED:
            // If we are already loaded, just insert us back into the active list
            addChunkToActiveList(chunk);
            break;
        case ChunkState::DESTROYING_ON_SIM:
            panic("Chunk is being destroyed on sim but marked as all neighbors active");
        default:
            assert(false);
            break;

    }
    static_assert(e_count(ChunkState) == 6);
}

void IChunkGrid::onChunkReady(Chunk& chunk) {
    assert(chunk.getTileContainer()->getState() == TileContainerState::READY);

    chunk.mState = ChunkState::ACTIVATED;
    addChunkToActiveList(chunk);

    // Notify observers
    ChunkGridEvent evnt(chunk);
    dispatchActivated(evnt);
    TileContainerEvent event{ chunk.mTileContainer, {} };
    mWorld->getTileContainerRepository().dispatchActivated(event);
}
