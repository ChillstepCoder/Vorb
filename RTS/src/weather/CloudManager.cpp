#include "stdafx.h"
#include "CloudManager.h"

#include "rendering/QuadMesh.h"
#include "rendering/SpriteData.h"
#include "DebugRenderer.h"

#include "World.h"
#include "ResourceManager.h"

#include "Random.h"

#include "options/DebugOptions.h"

#include "generation/WorldGeneration.h"
 
constexpr ui32 CHUNK_STRIDE_PER_CLOUD_BATCH = 8; // POWER OF TWO ONLY
constexpr i32 CLOUD_BATCH_WIDTH = CHUNK_WIDTH * CHUNK_STRIDE_PER_CLOUD_BATCH;
constexpr ui32 WORLD_WIDTH_CLOUD_BATCHES = WorldData::WORLD_WIDTH_CHUNKS / CHUNK_STRIDE_PER_CLOUD_BATCH;
constexpr ui32 WORLD_SIZE_CLOUD_BATCHES = SQ(WORLD_WIDTH_CLOUD_BATCHES);
const float CLOUD_DIAGONAL_RADIUS = (float)(sqrt(SQ(CLOUD_BATCH_WIDTH) + SQ(CLOUD_BATCH_WIDTH)) / 2.0);
const f32 CLOUD_LOAD_RANGE = CHUNK_LOAD_RANGE * 2.0;
const f32 CLOUD_LOAD_RANGE_SQ = SQ(CLOUD_LOAD_RANGE);

constexpr int CLOUD_GEN_STRIDE = 16;
constexpr int MAX_CLOUDS_PER_BATCH = SQ(CLOUD_BATCH_WIDTH / CLOUD_GEN_STRIDE);
static_assert(MAX_CLOUDS_PER_BATCH < UINT16_MAX);


typedef GridID<WORLD_WIDTH_CLOUD_BATCHES, CLOUD_BATCH_WIDTH> CloudID;

constexpr ui32 MAX_MESH_RECYCLES = 32;
constexpr int CLOUD_DIR_LEFT  = -1;
constexpr int CLOUD_DIR_DOWN  = -1;
constexpr int CLOUD_DIR_RIGHT = 1;
constexpr int CLOUD_DIR_UP    = 1;

#define DEBUG_CLOUD_RENDER 0

CloudManager::CloudManager(const World& world) : mWorld(world)
{

}

CloudManager::~CloudManager()
{

}

void CloudManager::init() {

    PreciseTimer timer;

    const f32v2& loadCenter = mWorld.getLoadCenter();
    CloudID centerCloudID = CloudID(loadCenter);
    f32v2 centerPos(centerCloudID.pos.x * CLOUD_BATCH_WIDTH, centerCloudID.pos.y * CLOUD_BATCH_WIDTH);

    // Initial variables
    mLastCenterPosition = i32v2(centerCloudID.pos.x, centerCloudID.pos.y);
    mCloudSpriteData = &Services::ResourceManager::ref().getSprite("cloud");

    std::map<ui32 /*ycoord*/, CloudID /*leftMost*/> spawnLookup;

    // TODO: Optimize iteration
    // TODO: This wont generate clouds off map if we spawn at edge of world
    for (ui32 i = 0; i < WORLD_SIZE_CLOUD_BATCHES; ++i) {
        CloudID id(i);
        f32v2 pos(id.getWorldPos());
        if (glm::length2(pos - centerPos) < CLOUD_LOAD_RANGE_SQ) {
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
        const i32v2 offset(id.second.pos.x - centerCloudID.pos.x, id.second.pos.y - centerCloudID.pos.y);
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
            i32v2 offset = i32v2(floor(batch.mRootPos.x / CLOUD_BATCH_WIDTH), floor(batch.mRootPos.y / CLOUD_BATCH_WIDTH)) - mLastCenterPosition;
            auto&& it = mCloudBoundsCheckMap.find(offset.y);
            if (it == mCloudBoundsCheckMap.end()) {
                // Out of range in the Y direction
                destroyCloudBatch(batch);
            }
            else {
                // Check bounds in X direction
                if ((offset.x < it->second || offset.x > -it->second)) {
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

    // Lightweight update for all generating batches
    for (auto&& it : mGeneratingBatches) {
        it.second.mRootPos.x += sDebugOptions.mCloudSpeed;
    }

    // Spawn new waves
    mDx += sDebugOptions.mCloudSpeed;
    mDxTotal += sDebugOptions.mCloudSpeed;
    if (mDx >= CLOUD_BATCH_WIDTH) {
        spawnNewCloudWaveX(CLOUD_DIR_LEFT);
    }
    else if (mDx < 0.0f) {
        spawnNewCloudWaveX(CLOUD_DIR_RIGHT);
    }
    if (mDy >= CLOUD_BATCH_WIDTH) {
        spawnNewCloudWaveY(CLOUD_DIR_LEFT);
    }
    else if (mDy < 0.0f) {
        spawnNewCloudWaveY(CLOUD_DIR_RIGHT);
    }
}

void CloudManager::updateGridShift() {
    const f32v2& loadCenter = mWorld.getLoadCenter();
    i32v2 centerCloudPos = i32v2(floor(loadCenter.x / CLOUD_BATCH_WIDTH), floor(loadCenter.y / CLOUD_BATCH_WIDTH));
    i32v2 offsetSinceLastTick = centerCloudPos - mLastCenterPosition;
    if (offsetSinceLastTick.x != 0) {
        if (offsetSinceLastTick.x < 0) {
            // We went left
            mDx += -offsetSinceLastTick.x * CLOUD_BATCH_WIDTH;
            mDxTotal += -offsetSinceLastTick.x * CLOUD_BATCH_WIDTH;
        }
        else {
            // We went right
            mDx -= offsetSinceLastTick.x * CLOUD_BATCH_WIDTH;
            mDxTotal -= offsetSinceLastTick.x * CLOUD_BATCH_WIDTH;
        }
    }
    if (offsetSinceLastTick.y != 0) {
        if (offsetSinceLastTick.y < 0) {
            // We went back
            mDy += -offsetSinceLastTick.y * CLOUD_BATCH_WIDTH;
            mDyTotal += -offsetSinceLastTick.y * CLOUD_BATCH_WIDTH;
        }
        else {
            // We went forward
            mDy -= offsetSinceLastTick.y * CLOUD_BATCH_WIDTH;
            mDyTotal += -offsetSinceLastTick.y * CLOUD_BATCH_WIDTH;
        }
    }
    mLastCenterPosition = centerCloudPos;
}

void CloudManager::tryGenerateCloudBatchAt(i32v2 cloudPos) {
    f32v2 pos(cloudPos.x * CLOUD_BATCH_WIDTH, cloudPos.y * CLOUD_BATCH_WIDTH);
    const f32 size = 10.0f;
    
    ui32 index = ++mGeneratingIndexLast;
    CloudBatch& newBatch = mGeneratingBatches[index];
    newBatch.mRootPos = f32v3(pos.x + mDx, pos.y + mDy, 110.0f);
    newBatch.mBoundsRadius = CLOUD_DIAGONAL_RADIUS + 10.0f;
    if (mRecycledMeshes.size()) {
        newBatch.mMesh = std::move(mRecycledMeshes.back());
        mRecycledMeshes.pop_back();
    }
    else {
        newBatch.mMesh = std::make_unique<TBOBillboardMesh>();
    }

    f64v2 genPos(pos.x - mDxTotal + mDx, pos.y - mDyTotal + mDy);

    TBOBillboardMesh* mesh = newBatch.mMesh.get();
    Services::Threadpool::ref().addTask([mesh, size, genPos, this](ThreadPoolWorkerData*) {
        for (int y = -CLOUD_BATCH_WIDTH / 2; y <= CLOUD_BATCH_WIDTH / 2; y += CLOUD_GEN_STRIDE) {
            for (int x = -CLOUD_BATCH_WIDTH / 2; x <= CLOUD_BATCH_WIDTH / 2; x += CLOUD_GEN_STRIDE) {
                const f64v2 trueGenPos((f64)genPos.x + x, (f64)genPos.y + y);
                const f32 n = sWorldGen.mCloudsNoise.compute(trueGenPos.x, trueGenPos.y);
                if (n > 0.3f) {
                    constexpr f32 RAND_OFFSET_FACTOR = 8.0f;
                    constexpr f32 HEIGHT_OFFSET_FACTOR = 8.0f;
                    constexpr f32 SCALE_OFFSET_FACTOR = 10.0f;
                    const float xr = Random::getThreadSafef((ui32)(genPos.x + x), (ui32)(genPos.y + y)) * RAND_OFFSET_FACTOR;
                    const float yr = Random::getThreadSafef((ui32)(genPos.y + x), (ui32)(genPos.x - y)) * RAND_OFFSET_FACTOR;
                    const float zr = Random::getThreadSafef((ui32)(genPos.x + y), (ui32)(genPos.y - x)) * HEIGHT_OFFSET_FACTOR;
                    const float sr = Random::getThreadSafef((ui32)(genPos.y - x), (ui32)(genPos.x + genPos.y + y)) * SCALE_OFFSET_FACTOR;
                    const float stretchr = Random::getThreadSafef((ui32)(-4152.0 + genPos.x - y), (ui32)(24152.0 -genPos.x - genPos.y + x)) * 0.6f;
                    const f32 nSize = n * 12.0f;
                    f32 newSize = size + sr + nSize;
                    if (Random::getThreadSafe(trueGenPos.x, trueGenPos.y) % 80 == 0) {
                        newSize += 30.0f;
                    }
                    newSize *= 1.3f;// TALIA SIZE TESTING
                    const f32 heightOffset = sWorldGen.mCloudHeightNoise.compute(trueGenPos.x, trueGenPos.y) * 50.0f;
                    const f32v3 quadPos(x + xr, y + yr, zr + sr * 0.5f + nSize + heightOffset);
                    mesh->addQuad(quadPos, f32v2(newSize * 1.952f, (newSize) * (1.0f - stretchr) * 1.472f), f32v2(0.0f), mCloudSpriteData->atlasPage, mCloudSpriteData->uvs, COLOR_WHITE, true, 0u, 240u);
                }
            }
        }
    }, [&newBatch, index, this]() {
        newBatch.mMesh->finishMesh(MeshDrawMode::STATIC);
        auto&& it = mGeneratingBatches.find(index);
        if (newBatch.mMesh->isValid()) { // If we actually generated a cloud mesh, store it as active
            mCloudBatches.emplace_back(std::move(it->second));
        }
        mGeneratingBatches.erase(it);
    });
}

void CloudManager::destroyCloudBatch(CloudBatch& batch)
{
    ui32 index = ((const char*)&batch - (const char*)&mCloudBatches[0]) / sizeof(CloudBatch); // Get the index in our vector
    assert(&batch == &mCloudBatches[index]);
    if (batch.mMesh->isValid() && mRecycledMeshes.size() < MAX_MESH_RECYCLES) {
        batch.mMesh->clearForRecycleRetainMemory();
        mRecycledMeshes.push_back(std::move(batch.mMesh));
    }
    mCloudBatches[index] = std::move(mCloudBatches.back());
    mCloudBatches.pop_back();
}

void CloudManager::spawnNewCloudWaveX(i32 dir) {
    assert(dir == -1 || dir == 1);
    mDx += CLOUD_BATCH_WIDTH * dir;
    for (auto&& it : mCloudSpawnOffsets) {
        i32v2 pos(mLastCenterPosition.x + -dir * it.x, mLastCenterPosition.y + it.y);
        tryGenerateCloudBatchAt(pos);
        if (IsEnabled<DEBUG_CLOUD_RENDER>()) {
            DebugRenderer::drawFilledQuad(f32v3(pos.x * CLOUD_BATCH_WIDTH + mDx, pos.y * CLOUD_BATCH_WIDTH + mDy, 1.0f), f32v2(CLOUD_BATCH_WIDTH), color4(0.0f, 1.0f, 0.0f, 0.5f), 1000);
        }
    }
}

void CloudManager::spawnNewCloudWaveY(i32 dir) {
    assert(dir == -1 || dir == 1);
    mDy += CLOUD_BATCH_WIDTH * dir;
    for (auto&& it : mCloudSpawnOffsets) {
        i32v2 pos(mLastCenterPosition.x + it.y, mLastCenterPosition.y + -dir * it.x);
        tryGenerateCloudBatchAt(pos);
        if (IsEnabled<DEBUG_CLOUD_RENDER>()) {
            DebugRenderer::drawFilledQuad(f32v3(pos.x * CLOUD_BATCH_WIDTH + mDx, pos.y * CLOUD_BATCH_WIDTH + mDy, 1.0f), f32v2(CLOUD_BATCH_WIDTH), color4(1.0f, 0.0f, 0.0f, 0.5f), 1000);
        }
    }
}

CloudBatch::~CloudBatch()
{

}
