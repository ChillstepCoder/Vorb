#include "stdafx.h"
#include "ChunkGrid.h"

#include "world/WorldGrid.h"
#include "world/ChunkGenerator.h"

#include "services/Services.h"
#include "pathfinding/NavThread.h"

#include "options/DebugOptions.h"

// RENDERING
#include "rendering/ChunkGrassQuadtree.h"
#include "rendering/ChunkMesher.h"

const float CHUNK_UNLOAD_TOLERANCE = -10.0f; // How many extra blocks we add when checking unload distance

ChunkGrid::ChunkGrid(WorldGrid& worldGrid) : mWorldGrid(worldGrid) {

    for (ui32 i = 0; i < WorldData::WORLD_SIZE_CHUNKS; ++i) {
        mChunks[i].init(ChunkID(i));
    }
}

void ChunkGrid::tick(const f32v2& loadCenter) {
    mLoadCenter = loadCenter;

    // TODO: This now asserts out of bounds
    Chunk& playerChunk = mWorldGrid.getChunk(loadCenter);
    if (playerChunk.isInvalid()) {
        initChunk(playerChunk);
    }

    for (size_t i = 0; i < mActiveChunks.size();) {
        Chunk& chunk = *mActiveChunks[i];
        if (tickChunk(chunk)) {
            mWorldGrid.releaseHeightDataAt(chunk.getHeightmapPatchID());
            chunk.dispose();
            mActiveChunks[i] = mActiveChunks.back();
            mActiveChunks.pop_back();
            continue;
        }
        ++i;
    }
}


void ChunkGrid::initChunk(Chunk& chunk)
{
    const ChunkID& chunkId = chunk.getChunkID();
    // If this is a sentinel chunk, stop here
    if (chunkId.isSentinelID()) {
        return;
    }

    generateChunkAsync(chunk);
}

void ChunkGrid::generateChunkAsync(Chunk& chunk) {

    chunk.allocateTileContainer();
    chunk.incRef();
    // TODO: should we be inactive?
    mActiveChunks.push_back(&chunk);
    const HeightmapPatchID& id = chunk.getHeightmapPatchID();

    if (mWorldGrid.tryGetHeightDataAt(id)) {
        chunk.mState.store(e_cast(ChunkState::LOADING_TILES));
        const HeightmapPatchData* heightData = mWorldGrid.aquireHeightData(id);
        Services::Threadpool::ref().addTask([&, heightData](ThreadPoolWorkerData* workerData) {
            ChunkGenerator::GenerateChunk(chunk, mWorldGrid, heightData);
            chunk.decRef();
        }, nullptr);
    }
    else {
        chunk.mState.store(e_cast(ChunkState::WAITING_HEIGHT));
        mWorldGrid.requestHeightDataGenAndAquireAt(id, [this, &chunk]() {
            chunk.mState.store(e_cast(ChunkState::LOADING_TILES));
            const HeightmapPatchData* heightData = mWorldGrid.getHeightDataAt(chunk.getHeightmapPatchID());
            Services::Threadpool::ref().addTask([&, heightData](ThreadPoolWorkerData* workerData) {
                ChunkGenerator::GenerateChunk(chunk, mWorldGrid, heightData);
                chunk.decRef();
            }, nullptr);
        });
    }

}


bool ChunkGrid::tickChunk(Chunk& chunk) {

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
                    chunk.mChunkRenderData.mGrassLod = std::make_unique<ChunkGrassQuadtree>(chunk, mWorldGrid);
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


void ChunkGrid::onChunkDataReady(Chunk& chunk) {
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

void ChunkGrid::onChunkAllNeighborsDataReady(Chunk& chunk) {
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

void ChunkGrid::dataReadyTryNotifyNeighbor(Chunk& chunk, const ChunkID& id) {
    Chunk& neighbor = mWorldGrid.getChunk(id.id);
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

void ChunkGrid::tryCreateNeighbors(Chunk& chunk) {
    // Neighbors
    const ChunkID& myId = chunk.getChunkID();
    tryCreateNeighbor(chunk, myId.getLeftID());
    tryCreateNeighbor(chunk, myId.getTopID());
    tryCreateNeighbor(chunk, myId.getRightID());
    tryCreateNeighbor(chunk, myId.getBottomID());
}

void ChunkGrid::tryCreateNeighbor(Chunk& chunk, const ChunkID& id) {
    Chunk& neighbor = mWorldGrid.getChunk(id.id);
    if (neighbor.isInvalid() && isChunkInLoadDistance(id)) {
        // Create the chunk, but dont update neighbor count until its done
        initChunk(neighbor);
    }
}

bool ChunkGrid::isChunkInLoadDistance(const ChunkID& chunkPos, float addOffset /* = 0.0f*/)
{
    const f32v2 centerPos = chunkPos.getWorldPos() + f32v2(HALF_CHUNK_WIDTH);
    const f32v2 offset = centerPos - mLoadCenter;

    return glm::length2(offset) <= sDebugOptions.mLoadRangeSq + addOffset;
}