#include "stdafx.h"
#include "TerrainGenerator.h"

#include "world/IHeightmapGrid.h"

void TerrainGenerator::init(IHeightmapGrid& heightGrid) {
    mHeightGrid = &heightGrid;
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

void TerrainGenerator::generateBaseHeightmap() {
    assert(mState != TerrainGenerationState::GeneratingBaseHeightmap);
    mState = TerrainGenerationState::GeneratingBaseHeightmap;
    assert(mHeightGrid);
    // Send each row to the threadpool
    // TODO: Allocate extra threads as needed
    for (ui32 i = 0; i < mHeightGrid->mWidthPatches; ++i) {
        Services::Threadpool::ref().addTask([this, i](ThreadPoolWorkerData*) {
            const ui32 rowOffset = i * mHeightGrid->mWidthPatches;
            for (ui32 j = 0; j < mHeightGrid->mWidthPatches; ++j) {
                HeightmapPatch& patch = mHeightGrid->mHeightData[rowOffset + j];
                delete patch.mHeightData;
                patch.mHeightData = new HeightmapPatchData(rowOffset + j);
                generateHeightDataPatch(patch, f32v2());
            }
            ++mFinishedRows;
            LOG_INFO("Finished generating terrain row {}", i);
        }, nullptr);
    }
}

void TerrainGenerator::generateHeightDataPatch(HeightmapPatch& patch, const f32v2& position) {
    // AABB calculation
    f32AABB3& aabb = patch.mHeightData->aabb;
    aabb.dims.x = HEIGHTMAP_QUAD_SIZE * HEIGHTMAP_QUAD_WIDTH_PER_PATCH;
    aabb.dims.y = HEIGHTMAP_QUAD_SIZE * HEIGHTMAP_QUAD_WIDTH_PER_PATCH;
    aabb.pos.x = position.x;
    aabb.pos.y = position.y;
    f32 minZ = FLT_MAX;
    f32 maxZ = -FLT_MAX;
    for (ui32 y = 0; y < HEIGHTMAP_VERT_WIDTH_PER_PATCH; ++y) {
        for (ui32 x = 0; x < HEIGHTMAP_VERT_WIDTH_PER_PATCH; ++x) {
            const f32v2 vertPos = f32v2(position.x + x * HEIGHTMAP_QUAD_SIZE, position.y + y * HEIGHTMAP_QUAD_SIZE);
            //f32 height = worldGenerator.getTerrainHeightAtPos(vertPos);
            f32 height = 3.f;
            if (height > maxZ) maxZ = height;
            if (height < minZ) minZ = height;
            patch.mHeightData->setHeightAt(y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + x, height);
        }
    }

    aabb.pos.z = minZ;
    aabb.dims.z = maxZ - minZ;
    patch.mHeightData->boundingSphere = boundingSphereFromAABB(aabb);
}
