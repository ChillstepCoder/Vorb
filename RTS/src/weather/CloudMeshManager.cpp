#include "stdafx.h"
#include "CloudMeshManager.h"

#include "debugging/DebugRenderer.h"

#include "rendering/mesh/mesher/builder/BillboardMeshBuilder.h"
#include "rendering/RenderThreadTasks.h"

#include "world/World.h"
#include "resources/ResourceManager.h"
#include "resources/MaterialRepository.h"

#include "math/Random.h"

#include "options/DebugOptions.h"

#include "generation/ChunkGenerator.h"
 
constexpr ui32 CHUNK_STRIDE_PER_CLOUD_BATCH = 8; // POWER OF TWO ONLY
constexpr i32 CLOUD_BATCH_WIDTH = CHUNK_WIDTH * CHUNK_STRIDE_PER_CLOUD_BATCH;
const float CLOUD_DIAGONAL_RADIUS = (float)(sqrt(SQ(CLOUD_BATCH_WIDTH) + SQ(CLOUD_BATCH_WIDTH)) / 2.0);
const f32 CLOUD_LOAD_RANGE = CHUNK_LOAD_RANGE * 2.0;
const f32 CLOUD_LOAD_RANGE_SQ = SQ(CLOUD_LOAD_RANGE);

constexpr int CLOUD_GEN_STRIDE = 16;
constexpr int MAX_CLOUDS_PER_BATCH = SQ(CLOUD_BATCH_WIDTH / CLOUD_GEN_STRIDE);
static_assert(MAX_CLOUDS_PER_BATCH < UINT16_MAX);
constexpr int CLOUD_DEBUG_DRAW_TIME = 500;

typedef ui32 CloudID;

constexpr ui32 MAX_MESH_RECYCLES = 32;
constexpr int CLOUD_DIR_LEFT  = -1;
constexpr int CLOUD_DIR_DOWN  = -1;
constexpr int CLOUD_DIR_RIGHT = 1;
constexpr int CLOUD_DIR_UP    = 1;

constexpr StrToken CLOUD_SIL_TOKEN = CStrToken("cloud_sil");

// TODO: Singleton pool?
struct CloudBatchTaskData {
    BillboardMeshBuilder meshBuilder;
    CloudMeshManager* cloudManager;
    CloudBatch* cloudBatch;
    ui32 index;
};

CloudMeshManager::CloudMeshManager(ChunkGenerator& worldGenerator) : mWorldGenerator(worldGenerator) {
    MaterialRepository& materialRepo = MaterialRepository::get();
    mAssets.addAssetHandle(materialRepo.getAssetHandle(CLOUD_SIL_TOKEN));
}

CloudMeshManager::~CloudMeshManager()
{

}

void CloudMeshManager::init(i32 worldWidthChunks, const f32v2& loadCenter) {
    mNeedsInit = false;
    mWorldWidthCloudBatches = worldWidthChunks / CHUNK_STRIDE_PER_CLOUD_BATCH;
    const ui32 WORLD_SIZE_CLOUD_BATCHES = SQ(mWorldWidthCloudBatches);

    mSpatialGrid2D.init(CLOUD_BATCH_WIDTH, mWorldWidthCloudBatches);

    ScopedTimer timer("Cloud init");

    const CloudID centerCloudID = mSpatialGrid2D.getIDAtWorldPos(loadCenter);
    const i32v2 centerGridPos = mSpatialGrid2D.getGridXYFromID(centerCloudID);
    const i32v2 centerWorldPos = mSpatialGrid2D.getWorldPosXYFromID(centerCloudID);

    // Initial variables
    mLastCenterPosition = centerGridPos;

    std::map<ui32 /*ycoord*/, CloudID /*leftMost*/> spawnLookup;

    // TODO: Optimize iteration
    // TODO: This wont generate clouds off map if we spawn at edge of world
    for (CloudID id = 0; id < WORLD_SIZE_CLOUD_BATCHES; ++id) {
        const i32v2 worldPos = mSpatialGrid2D.getWorldPosXYFromID(id);
        if (glm::length2(f32v2(worldPos - centerWorldPos)) < CLOUD_LOAD_RANGE_SQ) {
            const i32v2 gridPos = mSpatialGrid2D.getGridXYFromID(id);
            tryGenerateCloudBatchAt(gridPos);
            // See if this is a spawn position
            auto&& it = spawnLookup.find(gridPos.y);
            if (it == spawnLookup.end()) {
                spawnLookup[gridPos.y] = id;
            }
            else {
                // We are leftmost
                if (gridPos.x < mSpatialGrid2D.getGridXYFromID(it->second).x) {
                    it->second = id;
                }
            }
        }
    }

    // Build spawn positions
    mCloudSpawnOffsets.reserve(spawnLookup.size());
    for (auto&& id : spawnLookup) {
        const i32v2 spawnPos = mSpatialGrid2D.getGridXYFromID(id.second);
        const i32v2 offset(spawnPos.x - centerGridPos.x, spawnPos.y - centerGridPos.y);
        mCloudSpawnOffsets.push_back(offset);
    }
    // Debug draw
    for (auto&& it : mCloudSpawnOffsets) {
        mCloudBoundsCheckMap[it.y] = it.x;
    }

}

void CloudMeshManager::frameUpdate(const f32v2& loadCenter) {
    if (mNeedsInit) {
        init(mWorldGenerator.getWorld().getWidthChunks(), loadCenter);
    }

    // Handle any new cloud spawns from grid shift
    updateGridShift(loadCenter);

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

void CloudMeshManager::updateGridShift(const f32v2& loadCenter) {
    ASSERT_RENDER_THREAD();
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

void CloudMeshManager::tryGenerateCloudBatchAt(i32v2 cloudPos) {
    ASSERT_RENDER_THREAD();

    const f32v2 pos(cloudPos.x * CLOUD_BATCH_WIDTH, cloudPos.y * CLOUD_BATCH_WIDTH);
    const f32 size = 10.0f;
    
    const ui32 index = ++mGeneratingIndexLast;
    CloudBatch& newBatch = mGeneratingBatches[index];
    newBatch.mRootPos = f32v3(pos.x + mDx, pos.y + mDy, 110.0f);
    newBatch.mBoundsRadius = CLOUD_DIAGONAL_RADIUS + 10.0f;
    newBatch.mMesh = std::make_unique<Mesh>();

    const f64v2 genPos(pos.x - mDxTotal + mDx, pos.y - mDyTotal + mDy);

    // TODO: Cache this?
    const MaterialID& cloudMaterialId = MaterialRepository::get().getAssetID(CLOUD_SIL_TOKEN);

    CloudBatchTaskData* data = new CloudBatchTaskData{ {}, this, &newBatch, index };

    Services::Threadpool::ref().addTask([size, genPos, this, cloudMaterialId, data]() {
        const NoiseFunction& cloudNoiseFunction = mWorldGenerator.getGenerationData().mCloudsNoise;
        const NoiseFunction& cloudHeightFunction = mWorldGenerator.getGenerationData().mCloudHeightNoise;
        for (int y = -CLOUD_BATCH_WIDTH / 2; y <= CLOUD_BATCH_WIDTH / 2; y += CLOUD_GEN_STRIDE) {
            for (int x = -CLOUD_BATCH_WIDTH / 2; x <= CLOUD_BATCH_WIDTH / 2; x += CLOUD_GEN_STRIDE) {
                const f64v2 trueGenPos((f64)genPos.x + x, (f64)genPos.y + y);
                const f32 n = cloudNoiseFunction.compute((f32)trueGenPos.x, (f32)trueGenPos.y);
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
                    if (Random::getThreadSafe((ui32)trueGenPos.x, (ui32)trueGenPos.y) % 80 == 0) {
                        newSize += 30.0f;
                    }
                    newSize *= 1.3f;// TALIA SIZE TESTING
                    const f32 heightOffset = cloudHeightFunction.compute((f32)trueGenPos.x, (f32)trueGenPos.y) * 50.0f;
                    const f32v3 quadPos(x + xr, y + yr, zr + sr * 0.5f + nSize + heightOffset);
                    // TODO: Fix clouds
                    data->meshBuilder.addBillboard(quadPos, f32v2(newSize * 1.952f, (newSize) * (1.0f - stretchr) * 1.472f), cloudMaterialId, true /*randFlip*/);
                }
            }
        }

        RenderThreadTasks::getInstance().addGenericTask([](RenderContext& c, void* vData) {
            CloudBatchTaskData* data = static_cast<CloudBatchTaskData*>(vData);
            CloudBatch* batch = data->cloudBatch;
            CloudMeshManager* manager = data->cloudManager;
            data->meshBuilder.finishMesh(batch->mMesh, f32v3(0.0f), 0 /*bufferFlags*/);
            auto&& it = manager->mGeneratingBatches.find(data->index);
            assert(it != manager->mGeneratingBatches.end());
            if (batch->mMesh) { // If we actually generated a cloud mesh, store it as active
                manager->mCloudBatches.emplace_back(std::move(it->second));
            }
            manager->mGeneratingBatches.erase(it);
            delete data;
        }, data);
    });
}

void CloudMeshManager::destroyCloudBatch(CloudBatch& batch)
{
    ui32 index = ((const char*)&batch - (const char*)&mCloudBatches[0]) / sizeof(CloudBatch); // Get the index in our vector
    assert(&batch == &mCloudBatches[index]);
    mCloudBatches[index] = std::move(mCloudBatches.back());
    mCloudBatches.pop_back();
}

void CloudMeshManager::spawnNewCloudWaveX(i32 dir) {
    assert(dir == -1 || dir == 1);
    mDx += CLOUD_BATCH_WIDTH * dir;
    for (auto&& it : mCloudSpawnOffsets) {
        i32v2 pos(mLastCenterPosition.x + -dir * it.x, mLastCenterPosition.y + it.y);
        tryGenerateCloudBatchAt(pos);
        if (sDebugOptions.mDebugClouds) {
            DebugRenderer::drawFilledQuad(f32v3(pos.x * CLOUD_BATCH_WIDTH + mDx, pos.y * CLOUD_BATCH_WIDTH + mDy, 1.0f), f32v2(CLOUD_BATCH_WIDTH), color4(0.0f, 1.0f, 0.0f, 0.5f), CLOUD_DEBUG_DRAW_TIME);
        }
    }
}

void CloudMeshManager::spawnNewCloudWaveY(i32 dir) {
    assert(dir == -1 || dir == 1);
    mDy += CLOUD_BATCH_WIDTH * dir;
    for (auto&& it : mCloudSpawnOffsets) {
        i32v2 pos(mLastCenterPosition.x + it.y, mLastCenterPosition.y + -dir * it.x);
        tryGenerateCloudBatchAt(pos);
        if (sDebugOptions.mDebugClouds) {
            DebugRenderer::drawFilledQuad(f32v3(pos.x * CLOUD_BATCH_WIDTH + mDx, pos.y * CLOUD_BATCH_WIDTH + mDy, 1.0f), f32v2(CLOUD_BATCH_WIDTH), color4(1.0f, 0.0f, 0.0f, 0.5f), CLOUD_DEBUG_DRAW_TIME);
        }
    }
}

CloudBatch::~CloudBatch()
{

}
