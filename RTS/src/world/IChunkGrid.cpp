#include "stdafx.h"
#include "IChunkGrid.h"

#include "world/ChunkGenerator.h"
#include "world/IWorld.h"

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

IChunkGrid* sChunkGrid = nullptr;

bool updateChunkLoadDistanceAndCheckIfInRange(Chunk& chunk, const f32v2& loadCenter) {
    const f32v2 centerPos = chunk.getWorldPos() + f32v2(HALF_CHUNK_WIDTH);
    f32 distSq = glm::length2(centerPos - loadCenter);
    chunk.setDistanceFromLoadCenterSQ(distSq);
    return distSq <= sDebugOptions.mLoadRangeSq;
}

bool isChunkInLoadRange(const ChunkID& id, const f32v2& loadCenter) {
    const f32v2 centerPos = id.getWorldPos() + f32v2(HALF_CHUNK_WIDTH);
    const f32 distSq = glm::length2(centerPos - loadCenter);
    return distSq <= sDebugOptions.mLoadRangeSq;
}

IChunkGrid::IChunkGrid() {
    assert(!sChunkGrid);
    sChunkGrid = this;
    for (ui32 i = 0; i < WorldData::WORLD_SIZE_CHUNKS; ++i) {
        mChunks[i].init(ChunkID(i));
    }
}

void IChunkGrid::onWorldBegin(const f32v2& loadCenter) {

    IHeightmapGrid::registerIHeightmapGridListeners(mHeightmapGridListeners);
    IHeightmapGrid::addEditVertsListener(mHeightmapGridListeners, [this](const HeightmapGridEvent& gridEvent) {
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
    assert(IS_GAME_THREAD());

    // Check if we need to force update
    if (mForceUpdateEdgeChunks || (glm::length2(loadCenter - mPrevLoadCenter) > DISTANCE_SQ_CHANGE_UNTIL_FORCE_UPDATE_EDGES)) {
        updateGridEdges(loadCenter);
    }

    // Update all loading chunks
    for (size_t i = 0; i < mLoadingChunks.size();) {
        Chunk& chunk = mChunks[mLoadingChunks[i]];
        switch (chunk.mState) {
            case e_cast(ChunkState::WAITING_HEIGHT): {
                // Poll for generated height
                if (sHeightmapGrid->tryGetHeightDataAt(chunk.getHeightmapPatchID())) {
                    beginTileLoadForChunk(chunk);
                }
                ++i;
                break;
            }
            case e_cast(ChunkState::LOADING_TILES): {
                ++i;
                break;
            }
            case e_cast(ChunkState::TILE_LOAD_FINISHED): {
                chunk.mState = e_cast(ChunkState::WAITING_MESH_PHYSICS_NAV);
                chunk.mTileContainer->setState(TileContainerState::WAITING_MESH_AND_PHYSICS);

                // Cache harvestables
                chunk.mTileContainer->mHarvestableRegistry.refreshFromOwner();

                TileContainerEvent loadFinishedEvent;
                loadFinishedEvent.container = chunk.mTileContainer;
                TileContainerRepository::dispatchLoadFinished(loadFinishedEvent);

                assert(sNavWorld);
                sNavWorld->markContainerNavDirty(chunk.mTileContainer);
                ++i;
                break;
            }
            case e_cast(ChunkState::WAITING_MESH_PHYSICS_NAV): {
                if (chunk.mTileContainer->didInitMeshPhysicsAndNav()) {
                    mLoadingChunks[i] = mLoadingChunks.back();
                    mLoadingChunks.pop_back();
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

    // Tick all active chunks
   /* for (Chunk* chunk : mActiveChunks) {
        tickChunk(*chunk);
    }*/

    // Update all destroying chunks
    for (size_t i = 0; i < mDestroyingChunks.size();) {
        Chunk& chunk = mChunks[mDestroyingChunks[i]];
        if (chunk.getRefCount() == 0) {
            // Release height and notify only if we were ever valid
            if (chunk.mState != e_cast(ChunkState::INVALID)) {
                const HeightmapPatchID& heightId = chunk.getHeightmapPatchID();
                sHeightmapGrid->releaseHeightDataAt(heightId);
                dispatchDestroy(chunk);
            }
            chunk.dispose();
            mDestroyingChunks[i] = mDestroyingChunks.back();
            mDestroyingChunks.pop_back();
        }
        else {
            ++i;
        }
    }
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
                    tilePositionsNeedingUpdate[ChunkID::fromWorldI32v2(newPos).id].emplace_back(newPos);
                }
            }
        }
        static_assert(HEIGHTMAP_QUAD_SIZE == 2 && MAX_TILES_CHANGED_PER_POSITION == 16, "Update logic");
    }
    std::vector<std::pair<TileIndex, f32>> editData;
    for (auto&& it : tilePositionsNeedingUpdate) {
        Chunk& chunk = getChunk(it.first);
        if (chunk.isDataReady()) {
            editData.reserve(it.second.size());
            for (auto&& pos : it.second) {
                const ui32 x = (ui32)pos.x & (CHUNK_WIDTH - 1); // Fast modulus
                const ui32 y = (ui32)pos.y & (CHUNK_WIDTH - 1); // Fast modulus
                editData.emplace_back(std::make_pair(y * CHUNK_WIDTH + x, sHeightmapGrid->computeCenterHeightAtTile(pos)));
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
    ChunkID centerId(loadCenter);
    if (mAliveChunkBits.getBit(centerId.id) == false) {
        makeChunkAlive(centerId);
    }

    // Reverse iterate the edge positions so new edge chunks don't usually get processed this frame
    for (int i = (int)mEdgeChunkPositions.size() - 1; i >= 0; --i) {
        const LiteChunkID chunkId = mEdgeChunkPositions[i];
        if (isChunkInLoadRange(chunkId, loadCenter)) {
            // Try to load any unloaded neighbors
            const ui8& neighborBits = mNeighborBits[chunkId];
            const i32v2 xy = ChunkID::geti32XYFromId(chunkId);
            for (ui8 i = 0; i < 8; ++i) {
                if ((neighborBits & (1 << i)) == 0) {
                    const i32v2 neighborXy = xy + CARTESIAN8_DIR_OFFSETS[i];
                    const ChunkID neighborId(neighborXy);
                    if (isChunkInLoadRange(neighborId, loadCenter)) {
                        makeChunkAlive(neighborId);
                    }
                }
            }
        }
        else {
            // Destroying chunks are not edge chunks since they aren't alive
            Chunk& chunk = mChunks[chunkId];
            mEdgeChunkPositions[i] = mEdgeChunkPositions.back();
            mEdgeChunkPositions.pop_back();
            chunk.mFlags.clearBit(ChunkFlags::IN_EDGE_LIST);
            mForceUpdateEdgeChunks = true;
            // Chunk is now dead and destroying
            addChunkToDestroyList(chunk);
        }
    }
}

void IChunkGrid::makeChunkAlive(const ChunkID& chunkId) {
    PROFILE_FUNCTION();
    // Any time grid state changes we will update again
    mForceUpdateEdgeChunks = true;
    mAliveChunkBits.setBit(chunkId.id);

    // Check if we need to remove from destroy list first
    Chunk& chunk = mChunks[chunkId.id];
    if (chunk.mFlags.isBitSet(ChunkFlags::IN_DESTROY_LIST)) {
        removeChunkFromDestroyList(chunk);
    }

    ui8& neighborBits = mNeighborBits[chunkId.id];
    assert(neighborBits == 0);
    const i32v2 xy = chunkId.pos;
    for (ui8 i = 0; i < 8; ++i) {
        const i32v2 neighborXy = xy + CARTESIAN8_DIR_OFFSETS[i];
        const ChunkID neighborId(neighborXy);
        if (mAliveChunkBits.getBit(neighborId.id)) {
            // Create the alive neighbor connection
            neighborBits |= (1ui8 << i);
            ui8& adjacentNeighborBits = mNeighborBits[neighborId.id];
            adjacentNeighborBits |= (1ui8 << (ui8)CARTESIAN8_OPPOSITES[i]);
            if (adjacentNeighborBits == ALL_NEIGHBORS_ALIVE) {
                onAllNeighborsAlive(mChunks[neighborId.id]);
            }
        }
    }
    if (neighborBits == ALL_NEIGHBORS_ALIVE) {
        onAllNeighborsAlive(chunk);
    }
    else {
        mEdgeChunkPositions.emplace_back(chunkId.id);
        chunk.mFlags.setBit(ChunkFlags::IN_EDGE_LIST);
    }
}

void IChunkGrid::addChunkToActiveList(Chunk& chunk) {
    mActiveChunks.emplace_back(chunk.getChunkID().id);
    assert(!chunk.mFlags.isBitSet(ChunkFlags::IN_ACTIVE_LIST));
    chunk.mFlags.setBit(ChunkFlags::IN_ACTIVE_LIST);
}

void IChunkGrid::removeChunkFromActiveList(Chunk& chunk) {
    // TODO: Eliminate linear search? Do we care?
    LiteChunkID chunkId = chunk.getChunkID().id;
    for (size_t i = 0; i < mActiveChunks.size(); ++i) {
        if (mActiveChunks[i] == chunkId) {
            // Pop and swap
            mActiveChunks[i] = mActiveChunks.back();
            mActiveChunks.pop_back();
            chunk.mFlags.clearBit(ChunkFlags::IN_ACTIVE_LIST);
            break;
        }
    }
    // Make sure we removed
    assert(!chunk.mFlags.isBitSet(ChunkFlags::IN_ACTIVE_LIST));
}

void IChunkGrid::addChunkToLoadList(Chunk& chunk) {
    mLoadingChunks.emplace_back(chunk.getChunkID().id);
    assert(!chunk.mFlags.isBitSet(ChunkFlags::IN_LOAD_LIST));
    chunk.mFlags.setBit(ChunkFlags::IN_LOAD_LIST);
}

void IChunkGrid::removeChunkFromLoadList(Chunk& chunk) {
    // TODO: Eliminate linear search? Do we care?
    LiteChunkID chunkId = chunk.getChunkID().id;
    for (size_t i = 0; i < mLoadingChunks.size(); ++i) {
        if (mLoadingChunks[i] == chunkId) {
            // Pop and swap
            mLoadingChunks[i] = mLoadingChunks.back();
            mLoadingChunks.pop_back();
            chunk.mFlags.clearBit(ChunkFlags::IN_LOAD_LIST);
            break;
        }
    }
    // Make sure we removed
    assert(!chunk.mFlags.isBitSet(ChunkFlags::IN_LOAD_LIST));
}

void IChunkGrid::addChunkToDestroyList(Chunk& chunk) {
    PROFILE_FUNCTION();

    if (chunk.mFlags.isBitSet(ChunkFlags::IN_ACTIVE_LIST)) {
        removeChunkFromActiveList(chunk);
    }
    else if (chunk.mFlags.isBitSet(ChunkFlags::IN_LOAD_LIST)) {
        removeChunkFromLoadList(chunk);
    }
    const LiteChunkID id = chunk.getChunkID().id;
    // We are destroying so we have no neighbor bits
    mNeighborBits[id] = 0;
    mAliveChunkBits.clearBit(id);

    // Notify alive neighbors
    const i32v2 xy = chunk.getChunkID().pos;
    for (ui8 i = 0; i < 8; ++i) {
        const i32v2 neighborXy = xy + CARTESIAN8_DIR_OFFSETS[i];
        const ChunkID neighborId(neighborXy);
        if (mAliveChunkBits.getBit(neighborId.id)) {
            ui8& adjacentNeighborBits = mNeighborBits[neighborId.id];
            // If neighbor wasn't an edge, make him one
            if (adjacentNeighborBits == ALL_NEIGHBORS_ALIVE) {
                mEdgeChunkPositions.emplace_back(neighborId.id);
                mChunks[neighborId.id].mFlags.setBit(ChunkFlags::IN_EDGE_LIST);
                // TODO: Deactivate neighbor?
            }
            // Remove our bit
            adjacentNeighborBits &= ~(1ui8 << (ui8)CARTESIAN8_OPPOSITES[i]);
        }
    }

    assert(!chunk.mFlags.isBitSet(ChunkFlags::IN_DESTROY_LIST));
    chunk.mFlags.setBit(ChunkFlags::IN_DESTROY_LIST);
    mDestroyingChunks.emplace_back(id);
}

void IChunkGrid::removeChunkFromDestroyList(Chunk& chunk) {
    // TODO: Eliminate linear search? Do we care?
    LiteChunkID chunkId = chunk.getChunkID().id;
    for (size_t i = 0; i < mDestroyingChunks.size(); ++i) {
        if (mDestroyingChunks[i] == chunkId) {
            // Pop and swap
            mDestroyingChunks[i] = mDestroyingChunks.back();
            mDestroyingChunks.pop_back();
            chunk.mFlags.clearBit(ChunkFlags::IN_DESTROY_LIST);
            break;
        }
    }
    // Make sure we removed
    assert(!chunk.mFlags.isBitSet(ChunkFlags::IN_DESTROY_LIST));
}

void IChunkGrid::onAllNeighborsAlive(Chunk& chunk) {
    PROFILE_FUNCTION();
    // Once all neighbors are alive, we can begin loading
    // Only begin load if we are flagged as "Invalid" since otherwise we never disposed, and we can just keep our old state

    if (chunk.mFlags.isBitSet(ChunkFlags::IN_EDGE_LIST)) {
        LiteChunkID id = chunk.getChunkID().id;
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

    if (chunk.mState == e_cast(ChunkState::INVALID)) {
        // Begin load
        if (sHeightmapGrid->tryAquireHeightData(chunk.getHeightmapPatchID())) {
            beginTileLoadForChunk(chunk);
        }
        else {
            beginHeightLoadForChunk(chunk);
        }
        addChunkToLoadList(chunk);
    }
    else if (chunk.mState == e_cast(ChunkState::READY)) {
        // If we are already loaded, just insert us back into the active list
        addChunkToActiveList(chunk);
    }
    else {
        // Otherwise we are still loading
        addChunkToLoadList(chunk);
    }
}

void IChunkGrid::beginHeightLoadForChunk(Chunk& chunk) {
    assert(chunk.mState != e_cast(ChunkState::WAITING_HEIGHT));
    chunk.mState = e_cast(ChunkState::WAITING_HEIGHT);
    sHeightmapGrid->requestHeightDataGenAndAquireAt(chunk.getHeightmapPatchID(), nullptr);
}

void IChunkGrid::beginTileLoadForChunk(Chunk& chunk) {
    assert(chunk.mState != e_cast(ChunkState::LOADING_TILES));
    chunk.mState = e_cast(ChunkState::LOADING_TILES);
    generateChunkAsync(chunk);
}

// TODO: Move threadpool tasks


void IChunkGrid::generateChunkAsync(Chunk& chunk) {

    chunk.allocateTileContainer();
    chunk.incRef();

    // Make sure we dont lose height data
    // TODO: copy minimum
    f32* heightData = new f32[HEIGHTMAP_VERT_SIZE_PER_PATCH];
    const f32* srcData = sHeightmapGrid->getHeightDataAt(chunk.getHeightmapPatchID())->data;
    memcpy(heightData, srcData, sizeof(f32) * HEIGHTMAP_VERT_SIZE_PER_PATCH);
    Services::Threadpool::ref().addTask([&chunk, heightData](ThreadPoolWorkerData* workerData) {
        ChunkGenerator::GenerateChunk(chunk, heightData);
        assert(chunk.getState() == ChunkState::LOADING_TILES);
        chunk.setState(ChunkState::TILE_LOAD_FINISHED);
        chunk.decRef();
        delete heightData;
    }, nullptr);

}

void IChunkGrid::onChunkReady(Chunk& chunk)
{
    // Now we need nav
    addChunkToActiveList(chunk);
    chunk.mState = e_cast(ChunkState::READY);
    chunk.mTileContainer->setState(TileContainerState::READY);

    // Notify observers
    dispatchReady(chunk);
    TileContainerEvent event{ chunk.mTileContainer, {} };
    TileContainerRepository::dispatchReady(event);
}
