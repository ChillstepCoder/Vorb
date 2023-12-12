#include "stdafx.h"
#include "BaseHeightmapAndBiomeGenerationStage.h"

#include "world/IHeightmapGrid.h"
#include "world/biome/BiomeGrid.h"
#include "world/host/HostWorldData.h"
#include "generation/WorldGenerationData.h"
#include "generation/WorldGenerationBlackboard.h"

#include "rendering/MaterialShaderRepository.h"

constexpr int BIOME_VERTEX_SPACING_DIFF = BIOME_VERTEX_STRIDE / HEIGHTMAP_QUAD_SIZE;

// Match the shader
constexpr int LOCAL_GROUP_SIZE = 16;
constexpr ui32 ROWS_TO_GENERATE_PER_FRAME = 32; // POWER OF TWO REQUIRED

PendingBaseHeightAndBiomeGeneration::~PendingBaseHeightAndBiomeGeneration() {
    if (sync) {
        glDeleteSync(sync);
    }
}

void BaseHeightmapAndBiomeGenerationStage::begin()
{
    mGPUGenerations.resize(mHeightGrid->getWidthPatches() / ROWS_TO_GENERATE_PER_FRAME);
}

bool BaseHeightmapAndBiomeGenerationStage::update()
{
    // Send next row
    const ui32 rowsToGenerate = glm::min(ROWS_TO_GENERATE_PER_FRAME, mHeightGrid->getWidthPatches() - mNextRowToGenerate);
    if (rowsToGenerate) {
        assert(rowsToGenerate == ROWS_TO_GENERATE_PER_FRAME); // Make sure it is evenly divisible
        const int numGroups = HEIGHTMAP_VERT_WIDTH_PER_PATCH / LOCAL_GROUP_SIZE;

        const MaterialShaderDef* def = MaterialShaderRepository::get().tryGetLoadedAsset(CStrToken("terrain_base"));
        if (!def) {
            panic("terrain_base.comp was not loaded. Make sure it exists and is in assets.preload");
        }
        def->useCompute();
        glProgramUniform2fv(def->mProgram.getID(), def->getUniform("unWorldCenter"), 1, &mGenerationData.mWorldCenter.x);
        //glProgramUniform1f(def->mProgram.getID(), def->getUniform("unContinentOutlineScale"), mGenerationData.mContinentOutlineScale);
        glProgramUniform1f(def->mProgram.getID(), def->getUniform("unContinentRadius"), mGenerationData.mContinentRadius);
        glProgramUniform1f(def->mProgram.getID(), def->getUniform("unSeed"), mWorldSeed);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mHeightSSBO);
        glBindImageTexture(0, mHeightTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mBiomeSSBO);
        glBindImageTexture(1, mBiomeTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8);

        HeightmapPatchID patchIdLeftmost = mNextRowToGenerate * mHeightGrid->getWidthPatches();
        const f32v2 rootPos = mHeightGrid->getSpatialGrid2D().getWorldPosXYFromID(patchIdLeftmost);
        const i32v2 vertXY = mHeightGrid->getSpatialGrid2D().getGridXYFromID(patchIdLeftmost) * HEIGHTMAP_VERT_WIDTH_PER_PATCH;

        glProgramUniform2fv(def->mProgram.getID(), def->getUniform("unPatchWorldPos"), 1, &rootPos.x);
        glProgramUniform1ui(def->mProgram.getID(), def->getUniform("unYStride"), mHeightGrid->getWidthPatches() * (ui32)HEIGHTMAP_VERT_WIDTH_PER_PATCH);
        glProgramUniform2i(def->mProgram.getID(), def->getUniform("unVertexOffset"), vertXY.x, vertXY.y);

        // Dispatch compute
        constexpr i32 MAX_COMPUTE_SIZE = 65535; // Minimum as according to openGL spec
        if (MAX_COMPUTE_SIZE < numGroups * mHeightGrid->getWidthPatches()) {
            panic("Heightmap gen compute attempted to dispatch {} groups, but max is {}", numGroups * mHeightGrid->getWidthPatches(), MAX_COMPUTE_SIZE);
        }
        glDispatchCompute(numGroups * mHeightGrid->getWidthPatches(), numGroups * rowsToGenerate, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        PendingBaseHeightAndBiomeGeneration& generation = mGPUGenerations[mNextRowToGenerate / ROWS_TO_GENERATE_PER_FRAME];
        generation.rowIndexStart = mNextRowToGenerate;
        generation.numRows = rowsToGenerate;
        generation.generateStarted = true;
        generation.sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        assert(generation.sync);

        checkGlError("TerrainGenerator::generateBaseHeightmapGPU");
        mNextRowToGenerate += rowsToGenerate;
    }
    else {
        mAllGenerationSentThisStep = true;
    }

    while (mNextGenerationIndex != mGPUGenerations.size()) {
        PendingBaseHeightAndBiomeGeneration& generation = mGPUGenerations[mNextGenerationIndex];
        if (!generation.generateStarted) {
            break;
        }
        GLenum waitResult = glClientWaitSync(generation.sync, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
        assert(generation.sync);
        if (waitResult == GL_ALREADY_SIGNALED || waitResult == GL_CONDITION_SATISFIED) {
            ++mNextGenerationIndex;
            finishGeneration(generation);
        }
        else if (waitResult == GL_TIMEOUT_EXPIRED) {
            // The GPU commands are not yet complete. Continue other tasks or loop back later.
            return false;
        }
        else {
            panic("Terrain generation sync failed with GL_WAIT_FAILED");
        }
    }

    return mFinishedPatchesThisStep == mTotalHeightPatches;
}


void BaseHeightmapAndBiomeGenerationStage::finishGeneration(PendingBaseHeightAndBiomeGeneration& generation) {
    glDeleteSync(generation.sync);
    generation.sync = 0;

    // One task per patch
    const ui32 genID = sGenerationUID;
    for (int r = 0; r < generation.numRows; ++r) {
        for (int x = 0; x < mHeightGrid->getWidthPatches(); ++x) {
            Services::Threadpool::ref().addTask([this, rowIndex = generation.rowIndexStart + r, x, genID]() {
                // Check for interrupt
                if (genID != sGenerationUID) {
                    return;
                }
                i32v2 highestPointLocation = i32v2(-1); // Height vert position
                f32 highestHeight = MIN_GENERATION_PEAK_HEIGHT;

                // Set height data
                const HeightmapPatchID patchId = rowIndex * mHeightGrid->getWidthPatches() + x;
                const i32v2 rootPos = mHeightGrid->getSpatialGrid2D().getWorldPosXYFromID(patchId);
                const i32v2 rootVertXY = rootPos / HEIGHTMAP_QUAD_SIZE;
                const ui32 totalWidthVerts = mHeightGrid->getWidthPatches() * HEIGHTMAP_QUAD_WIDTH_PER_PATCH;
                const i32v2 terrainRootXY = mHeightGrid->getSpatialGrid2D().getGridXYFromID(patchId);
                HeightmapPatch& patch = mHeightGrid->getPatchForGeneration(patchId);
                f32AABB3& aabb = patch.aabb;
                aabb.dims.x = HEIGHTMAP_PATCH_WIDTH;
                aabb.dims.y = HEIGHTMAP_PATCH_WIDTH;
                aabb.pos.x = (f32)rootPos.x;
                aabb.pos.y = (f32)rootPos.y;
                f32 minZ = FLT_MAX;
                f32 maxZ = -FLT_MAX;
                {
                    const i32 rootVert = terrainRootXY.y * totalWidthVerts * HEIGHTMAP_QUAD_WIDTH_PER_PATCH + terrainRootXY.x * HEIGHTMAP_QUAD_WIDTH_PER_PATCH;
                    for (i32 y = 0; y < HEIGHTMAP_VERT_WIDTH_PER_PATCH; ++y) {
                        const i32 yStrideSource = y * totalWidthVerts;
                        const i32 yStrideTarget = y * HEIGHTMAP_VERT_WIDTH_PER_PATCH;
                        for (i32 x = 0; x < HEIGHTMAP_VERT_WIDTH_PER_PATCH; ++x) {
                            const i32 targetVert = yStrideTarget + x;
                            const i32 sourceVert = rootVert + yStrideSource + x;
                            const float height = mMappedHeights[sourceVert];
                            if (height > highestHeight) [[unlikely]] {
                                highestHeight = height;
                                highestPointLocation = i32v2(rootVertXY.x + x, rootVertXY.y + y);
                            }
                            patch.setHeightAtNoClamp(targetVert, mMappedHeights[sourceVert]);
                        }
                    }
                }
                aabb.pos.z = minZ;
                aabb.dims.z = maxZ - minZ;
                patch.boundingSphere = boundingSphereFromAABB(aabb);
                mBlackboard.mPeakPositions[patchId] = highestPointLocation;

                // Set biome data
                constexpr i32 BIOME_VERT_WIDTH_PER_PATCH = HEIGHTMAP_VERT_WIDTH_PER_PATCH / BIOME_VERTEX_SPACING_DIFF;
                {
                    const i32v2 biomeRootXY = (terrainRootXY * HEIGHTMAP_QUAD_WIDTH_PER_PATCH) / BIOME_VERTEX_SPACING_DIFF;
                    for (i32 y = 0; y < BIOME_VERT_WIDTH_PER_PATCH; ++y) {
                        const i32 yPos = biomeRootXY.y + y;
                        for (i32 x = 0; x < BIOME_VERT_WIDTH_PER_PATCH; ++x) {
                            BiomeVertex newVertex;
                            const ui32 index = yPos * mBiomeGrid->getWidthVertices() + biomeRootXY.x + x;
                            BiomeVertex& vertex = mBiomeGrid->getVertexForGeneration(index);
                            vertex.biomeUniqueId = mMappedBiomes[index];
                        }
                    }
                }
                ++mFinishedPatchesThisStep;
            }, nullptr);
        }
    }

    checkGlError("TerrainGenerator::finishPendingGeneration");
}
static_assert(sizeof(f32) == sizeof(GLfloat), "God help us");
