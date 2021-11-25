#include "stdafx.h"
#include "CloudManager.h"

#include "rendering/QuadMesh.h"
#include "rendering/SpriteData.h"
#include "DebugRenderer.h"

#include "World.h"
#include "ResourceManager.h"

#include "Random.h"

#include "options/DebugOptions.h"

#include "generation/WorldGenerationData.h"

constexpr ui32 MAX_MESH_RECYCLES = 32;
constexpr int CLOUD_DIR_LEFT  = -1;
constexpr int CLOUD_DIR_DOWN  = -1;
constexpr int CLOUD_DIR_RIGHT = 1;
constexpr int CLOUD_DIR_UP    = 1;

#define DEBUG_CLOUD_RENDER 1

CloudManager::CloudManager(const World& world) : mWorld(world)
{

}

CloudManager::~CloudManager()
{

}

void CloudManager::init() {

    PreciseTimer timer;

    const f32v2& loadCenter = mWorld.getLoadCenter();
    ChunkID centerChunkID = ChunkID(loadCenter);
    f32v2 centerPos(centerChunkID.pos.x * CHUNK_WIDTH, centerChunkID.pos.y * CHUNK_WIDTH);

    // Initial variables
    mLastCenterPosition = i32v2(centerChunkID.pos.x, centerChunkID.pos.y);
    mLoadRangeSQ = sDebugOptions.mLoadRangeSq;
    mCloudSpriteData = &mWorld.getResourceManager().getSprite("cloud");

    std::map<ui32 /*ycoord*/, ChunkID /*leftMost*/> spawnLookup;

    // TODO: Optimize iteration
    // TODO: This wont generate clouds off map if we spawn at edge of world
    for (ui32 i = 0; i < WorldData::WORLD_SIZE_CHUNKS; ++i) {
        ChunkID id(i);
        f32v2 pos(id.getWorldPos());
        if (glm::length2(pos - centerPos) < mLoadRangeSQ) {
            tryGenerateCloudBatchAt(i32v2(id.pos.x, id.pos.y));
            // See if this is a spawn position
            auto&& it = spawnLookup.find(id.pos.y);
            if (it == spawnLookup.end()) {
                spawnLookup[id.pos.y] = id;
            }
            else {
                // We are leftmost
                if (id.pos.x < it->second.pos.x) {
                    it->second = id;
                }
            }
        }
    }

    // Build spawn positions
    mCloudSpawnOffsets.reserve(spawnLookup.size());
    for (auto&& id : spawnLookup) {
        const i32v2 offset(id.second.pos.x - centerChunkID.pos.x, id.second.pos.y - centerChunkID.pos.y);
        mCloudSpawnOffsets.push_back(offset);
    }
    // Debug draw
    for (auto&& it : mCloudSpawnOffsets) {
        mCloudBoundsCheckMap[it.y] = it.x;
    }

    std::cout << "Clouds initialized in " << timer.stop() << " ms\n";
}

void CloudManager::update() {

    // Handle any new cloud spawns from grid shift
    updateGridShift();

    // Update clouds
    constexpr ui32 BOUNDS_CHECK_TICK_RATE = 30u;
    if (++mTickCount % BOUNDS_CHECK_TICK_RATE == 0) {
        // Update with more expensive bounds check
        for (ui32 i = 0; i < (ui32)mCloudBatches.size();) {
            CloudBatch& batch = mCloudBatches[i];
            batch.mRootPos.x += sDebugOptions.mCloudSpeed;
            i32v2 offset = i32v2(floor(batch.mRootPos.x / CHUNK_WIDTH), floor(batch.mRootPos.y / CHUNK_WIDTH)) - mLastCenterPosition;
            auto&& it = mCloudBoundsCheckMap.find(offset.y);
            if (it == mCloudBoundsCheckMap.end()) {
                // Out of range in the Y direction
                destroyCloudBatch(batch);
            }
            else {
                // Check bounds in X direction
                if (offset.x < it->second || offset.x > -it->second) {
                    destroyCloudBatch(batch);
                }
                else {
                    ++i;
                }
            }
        }
    }
    else {
        // Standard update with no bounds check
        for (auto&& batch : mCloudBatches) {
            batch.mRootPos.x += sDebugOptions.mCloudSpeed;
        }
    }

    // Spawn new waves
    mDx += sDebugOptions.mCloudSpeed;
    if (mDx >= CHUNK_WIDTH) {
        spawnNewCloudWaveX(CLOUD_DIR_LEFT);
    }
    else if (mDx < 0.0f) {
        spawnNewCloudWaveX(CLOUD_DIR_RIGHT);
    }
    if (mDy >= CHUNK_WIDTH) {
        spawnNewCloudWaveY(CLOUD_DIR_LEFT);
    }
    else if (mDy < 0.0f) {
        spawnNewCloudWaveY(CLOUD_DIR_RIGHT);
    }
}

void CloudManager::updateGridShift() {
    const f32v2& loadCenter = mWorld.getLoadCenter();
    i32v2 centerChunkPos = i32v2(floor(loadCenter.x / CHUNK_WIDTH), floor(loadCenter.y / CHUNK_WIDTH));
    i32v2 offsetSinceLastTick = centerChunkPos - mLastCenterPosition;
    if (offsetSinceLastTick.x != 0) {
        if (offsetSinceLastTick.x < 0) {
            // We went left
            mDx += -offsetSinceLastTick.x * CHUNK_WIDTH;
        }
        else {
            // We went right
            mDx -= offsetSinceLastTick.x * CHUNK_WIDTH;
        }
    }
    if (offsetSinceLastTick.y != 0) {
        if (offsetSinceLastTick.y < 0) {
            // We went back
            mDy += -offsetSinceLastTick.y * CHUNK_WIDTH;
        }
        else {
            // We went forward
            mDy -= offsetSinceLastTick.y * CHUNK_WIDTH;
        }
    }
    mLastCenterPosition = centerChunkPos;
}

void CloudManager::tryGenerateCloudBatchAt(i32v2 chunkPos) {
    f32v2 pos(chunkPos.x * CHUNK_WIDTH, chunkPos.y * CHUNK_WIDTH);
    const f32 size = 20.0f;
    
    CloudBatch& newBatch = mCloudBatches.emplace_back();
    newBatch.mRootPos = f32v3(pos.x + mDx + (rand() % 1000) / 30.0f, pos.y + mDy + CHUNK_WIDTH / 2, 20.0f);
    newBatch.mBoundsRadius = 10.0f;
    if (mRecycledMeshes.size()) {
        newBatch.mMesh = std::move(mRecycledMeshes.back());
        mRecycledMeshes.pop_back();
    }
    else {
        newBatch.mMesh = std::make_unique<TBOBillboardMesh>();
    }
    f32v3 quadPos(0.0f);
    newBatch.mMesh->addQuad(quadPos, f32v2(size), f32v2(0.0f), mCloudSpriteData->atlasPage, mCloudSpriteData->uvs, COLOR_WHITE, true, 0u, 240u);
    newBatch.mMesh->finishMesh(MeshDrawMode::STATIC);
}

void CloudManager::destroyCloudBatch(CloudBatch& batch)
{
    ui32 index = ((const char*)&batch - (const char*)&mCloudBatches[0]) / sizeof(CloudBatch); // Get the index in our vector
    assert(&batch == &mCloudBatches[index]);
    if (mRecycledMeshes.size() < MAX_MESH_RECYCLES) {
        batch.mMesh->clearForRecycleRetainMemory();
        mRecycledMeshes.push_back(std::move(batch.mMesh));
    }
    mCloudBatches[index] = std::move(mCloudBatches.back());
    mCloudBatches.pop_back();
}

void CloudManager::spawnNewCloudWaveX(i32 dir) {
    assert(dir == -1 || dir == 1);
    mDx += CHUNK_WIDTH * dir;
    for (auto&& it : mCloudSpawnOffsets) {
        i32v2 pos(mLastCenterPosition.x + -dir * it.x, mLastCenterPosition.y + it.y);
        tryGenerateCloudBatchAt(pos);
        if (IsEnabled<DEBUG_CLOUD_RENDER>()) {
            DebugRenderer::drawFilledQuad(f32v3(pos.x * CHUNK_WIDTH + mDx, pos.y * CHUNK_WIDTH + mDy, 1.0f), f32v2(CHUNK_WIDTH), color4(0.0f, 1.0f, 0.0f, 0.5f), 1000);
        }
    }
}

void CloudManager::spawnNewCloudWaveY(i32 dir) {
    assert(dir == -1 || dir == 1);
    mDy += CHUNK_WIDTH * dir;
    for (auto&& it : mCloudSpawnOffsets) {
        i32v2 pos(mLastCenterPosition.x + it.y, mLastCenterPosition.y + -dir * it.x);
        tryGenerateCloudBatchAt(pos);
        if (IsEnabled<DEBUG_CLOUD_RENDER>()) {
            DebugRenderer::drawFilledQuad(f32v3(pos.x * CHUNK_WIDTH + mDx, pos.y * CHUNK_WIDTH + mDy, 1.0f), f32v2(CHUNK_WIDTH), color4(1.0f, 0.0f, 0.0f, 0.5f), 1000);
        }
    }
}

CloudBatch::~CloudBatch()
{

}
