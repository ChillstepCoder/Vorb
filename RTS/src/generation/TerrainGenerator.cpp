#include "stdafx.h"
#include "TerrainGenerator.h"

#include "world/IHeightmapGrid.h"

void TerrainGenerator::init(IHeightmapGrid& heightGrid, f32v2 worldCenter) {
    mHeightGrid = &heightGrid;
    mWorldCenter = worldCenter;
}

void TerrainGenerator::destroy() {
    if (mHeightGrid) {
        switch (mState) {
            case TerrainGenerationState::None:
                break;
            case TerrainGenerationState::GeneratingBaseHeightmap:
                // Make sure threadpool is finished
                do {
                    Sleep(10);
                } while (mFinishedRows < mHeightGrid->mWidthPatches);
                break;
            case TerrainGenerationState::GeneratingBaseHeightmapDone:
                break;
            default:
                assert(false);
                break;
        }

        mHeightGrid = nullptr;
    }
}

TerrainGenerationState TerrainGenerator::tick() {
    switch (mState) {
        case TerrainGenerationState::None:
            break;
        case TerrainGenerationState::GeneratingBaseHeightmap:
            if (mFinishedRows >= mHeightGrid->mWidthPatches) {
                mFinishedRows = 0;
                mState = TerrainGenerationState::GeneratingBaseHeightmapDone;
            }
            break;
        case TerrainGenerationState::GeneratingBaseHeightmapDone:
            break;
        default:
            assert(false);
            break;
    }
    static_assert(e_count(TerrainGenerationState) == 3);
    return mState;
}

void TerrainGenerator::generateBaseHeightmapCPU(std::function<void(HeightmapPatchID)> onPatchFinished) {
    assert(mState != TerrainGenerationState::GeneratingBaseHeightmap);
    mState = TerrainGenerationState::GeneratingBaseHeightmap;
    assert(mHeightGrid);
    // Send each row to the threadpool
    // TODO: Allocate extra threads as needed
    for (ui32 y = 0; y < mHeightGrid->mWidthPatches; ++y) {
        Services::Threadpool::ref().addTask([this, y, onPatchFinished](ThreadPoolWorkerData*) {
            const ui32 rowOffset = y * mHeightGrid->mWidthPatches;
            const f32 yPos = y * HEIGHTMAP_PATCH_WIDTH;
            for (ui32 x = 0; x < mHeightGrid->mWidthPatches; ++x) {
                HeightmapPatch& patch = mHeightGrid->mHeightData[rowOffset + x];
                delete patch.mHeightData;
                patch.mHeightData = new HeightmapPatchData(rowOffset + x);
                generateHeightDataPatch(patch, f32v2(x * HEIGHTMAP_PATCH_WIDTH, yPos));
                if (onPatchFinished) {
                    onPatchFinished(rowOffset + x);
                }
            }
            ++mFinishedRows;
            LOG_INFO("Finished generating terrain row {}", y);
        }, nullptr);
    }
}

void TerrainGenerator::generateBaseHeightmapGPU(std::function<void(HeightmapPatchID)> onPatchFinished)
{
    assert(false);
}

void TerrainGenerator::generateHeightDataPatch(HeightmapPatch& patch, const f32v2& position) {
    // AABB calculation
    f32AABB3& aabb = patch.mHeightData->aabb;
    aabb.dims.x = HEIGHTMAP_PATCH_WIDTH;
    aabb.dims.y = HEIGHTMAP_PATCH_WIDTH;
    aabb.pos.x = position.x;
    aabb.pos.y = position.y;
    f32 minZ = FLT_MAX;
    f32 maxZ = -FLT_MAX;
    for (ui32 y = 0; y < HEIGHTMAP_VERT_WIDTH_PER_PATCH; ++y) {
        for (ui32 x = 0; x < HEIGHTMAP_VERT_WIDTH_PER_PATCH; ++x) {
            const f32v2 vertPos = f32v2(position.x + x * HEIGHTMAP_QUAD_SIZE, position.y + y * HEIGHTMAP_QUAD_SIZE);
            f32 height = generateHeightAtPos(vertPos);
            if (height > maxZ) maxZ = height;
            if (height < minZ) minZ = height;
            patch.mHeightData->setHeightAt(y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + x, height);
        }
    }

    aabb.pos.z = minZ;
    aabb.dims.z = maxZ - minZ;
    patch.mHeightData->boundingSphere = boundingSphereFromAABB(aabb);
}

f32 TerrainGenerator::generateHeightAtPos(const f32v2& worldPos) {
    // Base height
    f64 height = mGenerationData.mBaseNoise.compute((f64)worldPos.x, (f64)worldPos.y);

    f32v2 offsetToCenter(
        worldPos.x - mWorldCenter.x,
        worldPos.y - mWorldCenter.y
    );

    //  TODO: Precompute and interpolate, can cubic interpolate and others
    f64 distanceFromCenter2 = glm::length2(offsetToCenter);

    // Preturb the outline via noise
    distanceFromCenter2 += mGenerationData.CONTINENT_OUTLINE_SCALE * mGenerationData.mContinentOutlineNoise.compute(offsetToCenter.x, offsetToCenter.y);

    // Outline check
    if (distanceFromCenter2 > mGenerationData.CONTINENT_RADIUS_SQ) {
        // Ocean
        height -= (distanceFromCenter2 - mGenerationData.CONTINENT_RADIUS_SQ) * 0.0000001;
    }
    else {
        // Continent internals
        f64 lerp = (mGenerationData.CONTINENT_RADIUS_SQ - distanceFromCenter2) * 0.00000001;
        // Mountains
        f64 mountainDist = mGenerationData.mMountainsDistNoise.compute((f64)worldPos.x, (f64)worldPos.y);
        if (mountainDist > 0.0) {
            f64 mountain = mGenerationData.mMountainsNoise.compute((f64)worldPos.x, (f64)worldPos.y);
            height += lerp * mountain * glm::min(mountainDist, 1.0);
        }
    }
    return glm::clamp((f32)height, MIN_WORLD_GEN_HEIGHT, MAX_WORLD_GEN_HEIGHT);
}