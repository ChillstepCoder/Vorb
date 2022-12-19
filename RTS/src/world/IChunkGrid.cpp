#include "stdafx.h"
#include "IChunkGrid.h"

#include "world/IHeightmapGrid.h"
#include "world/ChunkGenerator.h"
#include "world/IWorld.h"

#include "services/Services.h"
#include "pathfinding/NavThread.h"

#include "options/DebugOptions.h"

// Meshing
#include "rendering/mesh/TileContainerMesher.h"

// How many tiles the load center has to move before we force update edge chunks
constexpr f32 DISTANCE_SQ_CHANGE_UNTIL_FORCE_UPDATE_EDGES = SQ(16.0f);


// REFRESH MAIN THREAD(Update when load center moves N tiles from previous position)
// IWORLD
// 0. Iterate all mLoadingChunks and mActiveChunks, if they are out of range, add them to mDestroyingChunksand clear their world bits
// 1. Efficient iterate and set bitmask, findand set new bits in rangeand add their chunks to mLoading
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

IChunkGrid::IChunkGrid() {
    assert(!sChunkGrid);
    sChunkGrid = this;
    for (ui32 i = 0; i < WorldData::WORLD_SIZE_CHUNKS; ++i) {
        mChunks[i].init(ChunkID(i));
    }
}

void IChunkGrid::onWorldBegin(const f32v2& loadCenter) {
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
        Chunk& chunk = *mLoadingChunks[i];
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
                // Copy height data
                // TODO: Minimum size instead of entire block
                f32* heightData = new f32[HEIGHTMAP_VERT_SIZE_PER_PATCH];
                const f32* srcData = sHeightmapGrid->getHeightDataAt(chunk.getHeightmapPatchID())->data;
                memcpy(heightData, srcData, sizeof(f32) * HEIGHTMAP_VERT_SIZE_PER_PATCH);
                TileContainerMesher::initMeshAndPhysicsAsync(*chunk.getTileContainer(), heightData);
                Services::NavThread::ref().addNavgraphBuildTask(*chunk.mTileContainer);
                ++i;
                break;
            }
            case e_cast(ChunkState::WAITING_MESH_PHYSICS_NAV): {
                if (chunk.mTileContainer->didInitMeshPhysicsAndNav()) {
                    mLoadingChunks[i] = mLoadingChunks.back();
                    mLoadingChunks.pop_back();
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
        Chunk& chunk = *mDestroyingChunks[i];
        if (chunk.getRefCount() == 0) {
            dispatchDestroy(chunk);
            chunk.dispose();
            mDestroyingChunks[i] = mDestroyingChunks.back();
            mDestroyingChunks.pop_back();
        }
        else {
            ++i;
        }
    }
}

// TODO: DELETE
bool compareLoadingChunk(Chunk* a, Chunk* b) { // return type is bool
    return a->getDistanceFromLoadCenterSQ() < b->getDistanceFromLoadCenterSQ();
}

void IChunkGrid::updateGridEdges(const f32v2& loadCenter) {
    PROFILE_FUNCTION();
    mPrevLoadCenter = loadCenter;

    // Remove any loading chunks
    for (size_t i = 0; i < mLoadingChunks.size();) {
        Chunk* chunk = mLoadingChunks[i];
        if (!updateChunkLoadDistanceAndCheckIfInRange(*chunk, loadCenter)) {
            markChunkForDestroy(*chunk);
            mLoadingChunks[i] = mLoadingChunks.back();
            mLoadingChunks.pop_back();
        }
        else {
            ++i;
        }
    }
    // Remove any active chunks
    for (size_t i = 0; i < mActiveChunks.size(); ++i) {
        Chunk* chunk = mActiveChunks[i];
        if (!updateChunkLoadDistanceAndCheckIfInRange(*chunk, loadCenter)) {
            markChunkForDestroy(*chunk);
            mActiveChunks[i] = mActiveChunks.back();
            mActiveChunks.pop_back();
        }
        else {
            ++i;
        }
    }

    // This will be set if we mutate any data
    mForceUpdateEdgeChunks = false;
    // Iterate every chunk in range (TODO: Use offset mask for perfect iteration and no distance checks?)
    // Need OnChunkLoadDistanceChanged to update the static offset mask
    //ChunkID
    f32v2 bottomLeft = loadCenter - f32v2(sDebugOptions.mLoadRange);
    f32v2 topRight = loadCenter + f32v2(sDebugOptions.mLoadRange);
    bottomLeft.x = glm::clamp(bottomLeft.x, 0.0f, (f32)WorldData::WORLD_WIDTH_TILES);
    bottomLeft.y = glm::clamp(bottomLeft.y, 0.0f, (f32)WorldData::WORLD_WIDTH_TILES);
    topRight.x = glm::clamp(topRight.x, 0.0f, (f32)WorldData::WORLD_WIDTH_TILES);
    topRight.y = glm::clamp(topRight.y, 0.0f, (f32)WorldData::WORLD_WIDTH_TILES);
    const ChunkID bottomLeftPos(bottomLeft);
    const ChunkID topRightPos(topRight);
    std::set<ChunkID> ids;

    // We will store these and load them after so we can sort any new chunks based on distance
    std::vector<Chunk*> chunksToBeginLoad;
    chunksToBeginLoad.reserve(128);

    // TODO: Spiral iterate, cache.
    for (ui32 y = bottomLeftPos.pos.y; y < topRightPos.pos.y; ++y) {
        for (ui32 x = bottomLeftPos.pos.x; x < topRightPos.pos.x; ++x) {
            ChunkID chunkId(x, y);
            assert(ids.find(chunkId) == ids.end());
            ids.insert(chunkId);
            // If there is no alive chunk here and we are in distance, add new alive chunk
            Chunk& chunk = mChunks[chunkId.id];
            if (!mAliveChunkBits.getBit(chunkId.id) && updateChunkLoadDistanceAndCheckIfInRange(chunk, loadCenter)) {
                assert(chunk.getChunkID() == chunkId.id);
                // Check if we need to remove from destroy list first
                if (chunk.mFlags.isBitSet(ChunkFlags::IN_DESTROY_LIST)) {
                    for (size_t i = 0; i < mDestroyingChunks.size(); ++i) {
                        if (mDestroyingChunks[i] == &chunk) {
                            // Pop and swap
                            mDestroyingChunks[i] = mDestroyingChunks.back();
                            mDestroyingChunks.pop_back();
                            chunk.mFlags.clearBit(ChunkFlags::IN_DESTROY_LIST);
                            mForceUpdateEdgeChunks = true;
                            break;
                        }
                    }
                    assert(!chunk.mFlags.isBitSet(ChunkFlags::IN_DESTROY_LIST));
                }
                mAliveChunkBits.setBit(chunkId.id);
                // Only begin load if we are flagged as "Invalid" since otherwise we never disposed, and we can just keep our old state
                if (chunk.mState == e_cast(ChunkState::INVALID)) {
                    chunksToBeginLoad.emplace_back(&chunk);
                    mLoadingChunks.emplace_back(&chunk);
                    mForceUpdateEdgeChunks = true;
                }
                else if (chunk.mState == e_cast(ChunkState::READY)) {
                    // If we are already loaded, just insert us back into the active list
                    mActiveChunks.emplace_back(&chunk);
                    mForceUpdateEdgeChunks = true;
                }
                else {
                    // Otherwise we are still loading
                    mLoadingChunks.emplace_back(&chunk);
                    mForceUpdateEdgeChunks = true;
                }
            }
        }
    }

    {
        PROFILE_SCOPE("Load chunk sort");

        // TODO: we could use a circular iteration above to avoid this sort
        std::sort(chunksToBeginLoad.begin(), chunksToBeginLoad.end(), compareLoadingChunk);
    }

    // Load any new chunks
    for (auto& chunk : chunksToBeginLoad) {
        if (sHeightmapGrid->tryAquireHeightData(chunk->getHeightmapPatchID())) {
            beginTileLoadForChunk(*chunk);
        }
        else {
            beginHeightLoadForChunk(*chunk);
        }
    }
}

void IChunkGrid::markChunkForDestroy(Chunk& chunk) {
    assert(!chunk.mFlags.isBitSet(ChunkFlags::IN_DESTROY_LIST));
    assert(chunk.getState() >= ChunkState::WAITING_HEIGHT); // Make sure we actually aquired height
    chunk.mFlags.setBit(ChunkFlags::IN_DESTROY_LIST);
    mDestroyingChunks.emplace_back(&chunk);
    mAliveChunkBits.clearBit(chunk.getChunkID().id);
    // Release height
    const HeightmapPatchID& heightId = chunk.getHeightmapPatchID();
    sHeightmapGrid->releaseHeightDataAt(heightId);

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
    mActiveChunks.emplace_back(&chunk);
    chunk.mState = e_cast(ChunkState::READY);
    chunk.mTileContainer->setState(TileContainerState::READY);

    // Notify observers
    dispatchReady(chunk);
    TileContainerEvent event{ chunk.mTileContainer, {} };
    TileContainerRepository::dispatchReady(event);
}
