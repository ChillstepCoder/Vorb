#include "stdafx.h"
#include "IChunkGrid.h"

#include "world/IHeightmapGrid.h"
#include "world/ChunkGenerator.h"

#include "services/Services.h"
#include "pathfinding/NavThread.h"

#include "options/DebugOptions.h"

// RENDERING
#include "rendering/ChunkGrassQuadtree.h"
#include "rendering/ChunkMesher.h"

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

bool isChunkInLoadDistance(const ChunkID& chunkPos, const f32v2& loadCenter) {
    const f32v2 centerPos = chunkPos.getWorldPos() + f32v2(HALF_CHUNK_WIDTH);
    return glm::length2(centerPos - loadCenter) <= sDebugOptions.mLoadRangeSq;
}

IChunkGrid::IChunkGrid() {
    assert(!sChunkGrid);
    sChunkGrid = this;
    for (ui32 i = 0; i < WorldData::WORLD_SIZE_CHUNKS; ++i) {
        mChunks[i].init(ChunkID(i));
    }
}

void IChunkGrid::tick() {

    for (size_t i = 0; i < mActiveChunks.size();) {
        Chunk& chunk = *mActiveChunks[i];
        if (tickChunk(chunk)) {
            sHeightmapGrid->releaseHeightDataAt(chunk.getHeightmapPatchID());
            chunk.dispose();
            mActiveChunks[i] = mActiveChunks.back();
            mActiveChunks.pop_back();
            continue;
        }
        ++i;
    }
}

void IChunkGrid::refresh(const f32v2& loadCenter) {
    // Remove any loading chunks
    for (size_t i = 0; i < mLoadingChunks.size();) {
        Chunk* chunk = mLoadingChunks[i];
        if (!isChunkInLoadDistance(chunk->getChunkID(), loadCenter)) {
            if (!chunk->mFlags.isBitSet(ChunkFlags::IN_DESTROY_LIST)) {
                chunk->mFlags.setBit(ChunkFlags::IN_DESTROY_LIST);
                mDestroyingChunks.emplace_back(chunk);
            }
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
        if (!isChunkInLoadDistance(chunk->getChunkID(), loadCenter)) {
            if (!chunk->mFlags.isBitSet(ChunkFlags::IN_DESTROY_LIST)) {
                chunk->mFlags.setBit(ChunkFlags::IN_DESTROY_LIST);
                mDestroyingChunks.emplace_back(chunk);
            }
            mActiveChunks[i] = mActiveChunks.back();
            mActiveChunks.pop_back();
        }
        else {
            ++i;
        }
    }

    // Iterate every chunk in range (TODO: Use offset mask for perfect iteration and no distance checks?)
    // Need OnChunkLoadDistanceChanged to update the static offset mask
    //ChunkID
    const f32v2 bottomLeft = loadCenter - f32v2(sDebugOptions.mLoadRange);
    const f32v2 topRight = loadCenter + f32v2(sDebugOptions.mLoadRange);
    const ChunkID bottomLeftPos(bottomLeft);
    const ChunkID topRightPos(topRight);
    for (ui32 y = bottomLeftPos.pos.y; y < topRightPos.pos.y; ++y) {
        for (ui32 x = bottomLeftPos.pos.x; x < topRightPos.pos.x; ++x) {
            ChunkID spot(x, y);
            if (isChunkInLoadDistance(spot, loadCenter)) {
                assert(false);
            }
        }
    }

}

void IChunkGrid::initChunk(Chunk& chunk)
{
    const ChunkID& chunkId = chunk.getChunkID();
    // If this is a sentinel chunk, stop here
    if (chunkId.isSentinelID()) {
        return;
    }

    generateChunkAsync(chunk);
}

void IChunkGrid::generateChunkAsync(Chunk& chunk) {

    chunk.allocateTileContainer();
    chunk.incRef();
    // TODO: should we be inactive?
    mActiveChunks.push_back(&chunk);
    const HeightmapPatchID& id = chunk.getHeightmapPatchID();

    if (sHeightmapGrid->tryGetHeightDataAt(id)) {
        chunk.mState.store(e_cast(ChunkState::LOADING_TILES));
        const HeightmapPatchData* heightData = sHeightmapGrid->aquireHeightData(id);
        Services::Threadpool::ref().addTask([&, heightData](ThreadPoolWorkerData* workerData) {
            ChunkGenerator::GenerateChunk(chunk, heightData);
            chunk.decRef();
        }, nullptr);
    }
    else {
        chunk.mState.store(e_cast(ChunkState::WAITING_HEIGHT));
        sHeightmapGrid->requestHeightDataGenAndAquireAt(id, [this, &chunk]() {
            chunk.mState.store(e_cast(ChunkState::LOADING_TILES));
            const HeightmapPatchData* heightData = sHeightmapGrid->getHeightDataAt(chunk.getHeightmapPatchID());
            Services::Threadpool::ref().addTask([&, heightData](ThreadPoolWorkerData* workerData) {
                ChunkGenerator::GenerateChunk(chunk, heightData);
                chunk.decRef();
            }, nullptr);
        });
    }

}


bool IChunkGrid::tickChunk(Chunk& chunk) {

    if (!isChunkInLoadDistance(chunk.getWorldPos(), CHUNK_UNLOAD_TOLERANCE)) {
        if (chunk.getTileContainer()->getRefCount()) {
            // Waiting on a thread or handle to release us
            return false;
        }
        // Unload
        return true;
    }

    if (chunk.isDataReady()) {
        // Check for new neighbors
        if (chunk.mDataReadyNeighborCount < CHUNK_NEIGHBOR_COUNT) {
            tryCreateNeighbors(chunk);
        }
        else if (chunk.mTileContainer->shouldBuildNavMesh()) {
            // Update nav graph when all neighbors are loaded
            // TODO: Async?
            Services::NavThread::ref().addNavgraphBuildTask(*chunk.mTileContainer);
        }
        else {
            // Update grass
            const f32v2 centerPos = chunk.getWorldPos() + f32v2(HALF_CHUNK_WIDTH);
            const f32v2 offset = centerPos - mLoadCenter;
            const f32 distSq = glm::length2(offset);

            // TODO: CLIENT ONLY
            if (chunk.mChunkRenderData.mGrassLod) {
                if (distSq > sDebugOptions.mGrassSettings.distanceSq + 10.0f) {
                    if (chunk.mChunkRenderData.mGrassLod->getRefCount() == 0) {
                        chunk.mChunkRenderData.mGrassLod.reset();
                    }
                }
                else {
                    chunk.mChunkRenderData.mGrassLod->update(mLoadCenter);
                }
            }
            else {
                if (distSq < sDebugOptions.mGrassSettings.distanceSq) {
                    chunk.mChunkRenderData.mGrassLod = std::make_unique<ChunkGrassQuadtree>(chunk);
                }
            }
        }
    }
    else if (chunk.getTileContainer()->getRefCount() == 0) {
        // If we are not in use, we are done generating
        onChunkDataReady(chunk);
    }

    return false;
}


void IChunkGrid::onChunkDataReady(Chunk& chunk) {
    assert(!chunk.isDataReady());

    chunk.setState(ChunkState::FINISHED);
    // Don't update neighbors until we are data ready
    assert(chunk.isDataReady());
    // Neighbors
    const ChunkID& myId = chunk.getChunkID();
    dataReadyTryNotifyNeighbor(chunk, myId.getBottomID());
    dataReadyTryNotifyNeighbor(chunk, myId.getLeftID());
    dataReadyTryNotifyNeighbor(chunk, myId.getRightID());
    dataReadyTryNotifyNeighbor(chunk, myId.getTopID());

    assert(chunk.mDataReadyNeighborCount <= CHUNK_NEIGHBOR_COUNT);
}

void IChunkGrid::onChunkAllNeighborsDataReady(Chunk& chunk) {
    assert(chunk.getBottomNeighbor().isDataReady());
    assert(chunk.getLeftNeighbor().isDataReady());
    assert(chunk.getRightNeighbor().isDataReady());
    assert(chunk.getTopNeighbor().isDataReady());

    // Dirty our nav graph
    chunk.mTileContainer->setDirtyNav(true);
    // Update our mesh
    chunk.dirtyMesh();
    ChunkMesher::updateMeshAndPhysics(chunk, f32v3(mLoadCenter, 0.0f));
}

void IChunkGrid::dataReadyTryNotifyNeighbor(Chunk& chunk, const ChunkID& id) {
    Chunk& neighbor = getChunk(id.id);
    if (neighbor.isDataReady()) {
        // Set up data ready ref counts
        ++neighbor.mDataReadyNeighborCount;
        assert(neighbor.mDataReadyNeighborCount <= CHUNK_NEIGHBOR_COUNT);
        ++chunk.mDataReadyNeighborCount;
        assert(chunk.mDataReadyNeighborCount <= CHUNK_NEIGHBOR_COUNT);
        if (neighbor.mDataReadyNeighborCount == CHUNK_NEIGHBOR_COUNT) {
            onChunkAllNeighborsDataReady(neighbor);
        }
        if (chunk.mDataReadyNeighborCount == CHUNK_NEIGHBOR_COUNT) {
            onChunkAllNeighborsDataReady(chunk);
        }
    }
    else if (neighbor.isInvalid() && isChunkInLoadDistance(id)) {
        // Create the chunk, but dont update neighbor count until its done
        initChunk(neighbor);
    }
}

void IChunkGrid::tryCreateNeighbors(Chunk& chunk) {
    // Neighbors
    const ChunkID& myId = chunk.getChunkID();
    tryCreateNeighbor(chunk, myId.getLeftID());
    tryCreateNeighbor(chunk, myId.getTopID());
    tryCreateNeighbor(chunk, myId.getRightID());
    tryCreateNeighbor(chunk, myId.getBottomID());
}

void IChunkGrid::tryCreateNeighbor(Chunk& chunk, const ChunkID& id) {
    Chunk& neighbor = getChunk(id.id);
    if (neighbor.isInvalid() && isChunkInLoadDistance(id)) {
        // Create the chunk, but dont update neighbor count until its done
        initChunk(neighbor);
    }
}
