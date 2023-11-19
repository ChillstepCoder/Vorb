#include "stdafx.h"
#include "TerrainGenerator.h"

#include "world/IHeightmapGrid.h"

#include "rendering/MaterialShaderRepository.h"

// Match the shader
constexpr int LOCAL_GROUP_SIZE = 16;

TerrainGenerator::TerrainGenerator(const WorldGenerationData& generationData) : mGenerationData(generationData)
{

}

TerrainGenerator::~TerrainGenerator() {
    destroy();
}

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
    std::vector<PendingGPUTerrainGeneration>().swap(mGPUTerrainGenerations);

    if (mHeightmapTexture) {
        glDeleteTextures(1, &mHeightmapTexture);
        glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
        mHeightmapTexture = 0;
        glDeleteFramebuffers(1, &mFramebuffer);
    }
}

TerrainGenerationState TerrainGenerator::tick() {

    if (mIsGeneratingGPU) {
        
        // Send next row
        if (mNextRowToGenerate < mHeightGrid->mWidthPatches) {
            const int numGroups = HEIGHTMAP_VERT_WIDTH_PER_PATCH / LOCAL_GROUP_SIZE;

            const MaterialShaderDef* def = MaterialShaderRepository::get().tryGetLoadedAsset(CStrToken("terrain_base"));
            if (!def) {
                panic("terrain_base.comp was not loaded. Make sure it exists and is in assets.preload");
            }
            def->useCompute();
            glProgramUniform2fv(def->mProgram.getID(), def->getUniform("unWorldCenter"), 1, &mGenerationData.mWorldCenter.x);
            glProgramUniform1f(def->mProgram.getID(), def->getUniform("unContinentOutlineScale"), mGenerationData.mContinentOutlineScale);
            glProgramUniform1f(def->mProgram.getID(), def->getUniform("unContinentRadiusSq"), mGenerationData.mContinentRadiusSq);
            //glProgramUniform1f(def->mProgram.getID(), def->getUniform("unSeed"), );
            //glProgramUniform1ui(def->mProgram.getID(), def->getUniform("unYStride"), mHeightGrid->mWidthPatches);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mHeightmapTexture);
            for (ui32 x = 0; x < mHeightGrid->mWidthPatches; ++x) {
                HeightmapPatchID patchId = mNextRowToGenerate * mHeightGrid->mWidthPatches + x;
                PendingGPUTerrainGeneration& generation = mGPUTerrainGenerations[patchId];

                generation.patchID = patchId;
                // TODO: batch create?

                // Upload position
                const f32v2 rootPos = mHeightGrid->getSpatialGrid2D().getWorldPosXYFromID(patchId);
                const i32v2 vertXY = mHeightGrid->getSpatialGrid2D().getGridXYFromID(patchId) * HEIGHTMAP_VERT_WIDTH_PER_PATCH;

                glProgramUniform2fv(def->mProgram.getID(), def->getUniform("unPatchWorldPos"), 1, &rootPos.x);
                glProgramUniform2i(def->mProgram.getID(), def->getUniform("unVertexOffset"), vertXY.x, vertXY.y);

                glCreateBuffers(1, &generation.pbo);
                glNamedBufferStorage(generation.pbo, sizeof(f32) * HEIGHTMAP_VERT_SIZE_PER_PATCH, nullptr, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
                // Create buffer data
               // glCreateBuffers(1, &generation.ssbo);
                //glNamedBufferData(generation.ssbo, sizeof(f32) * HEIGHTMAP_VERT_SIZE_PER_PATCH, nullptr, GL_DYNAMIC_READ);
                //glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, generation.ssbo);

                // Dispatch compute
                glDispatchCompute(numGroups, numGroups, 1);
                generation.sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
                generation.generateStarted = true;

                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

                const i32v2 gridXY = mHeightGrid->getSpatialGrid2D().getGridXYFromID(generation.patchID);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, mFramebuffer);
                glBindBuffer(GL_PIXEL_PACK_BUFFER, generation.pbo);
                glReadPixels(gridXY.x * HEIGHTMAP_VERT_WIDTH_PER_PATCH, gridXY.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH, HEIGHTMAP_VERT_WIDTH_PER_PATCH, HEIGHTMAP_VERT_WIDTH_PER_PATCH, GL_RED, GL_FLOAT, 0);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            }
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
            checkGlError("TerrainGenerator::generateBaseHeightmapGPU");
            ++mNextRowToGenerate;
        }

        while (mNextGenerationIndex != mGPUTerrainGenerations.size()) {
            PendingGPUTerrainGeneration& generation = mGPUTerrainGenerations[mNextGenerationIndex];
            if (!generation.generateStarted) {
                break;
            }
            GLenum waitResult = glClientWaitSync(generation.sync, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
            if (waitResult == GL_ALREADY_SIGNALED || waitResult == GL_CONDITION_SATISFIED) {
                ++mNextGenerationIndex;
                finishPendingGeneration(generation);
            }
            else if (waitResult == GL_TIMEOUT_EXPIRED) {
                // The GPU commands are not yet complete. Continue other tasks or loop back later.
                return mState;
            }
            else {
                panic("Terrain generation sync failed with GL_WAIT_FAILED");
            }
        }
        mState = TerrainGenerationState::GeneratingBaseHeightmapDone;
        return mState;
    }

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
    mIsGeneratingGPU = false;
    assert(mState != TerrainGenerationState::GeneratingBaseHeightmap);
    mState = TerrainGenerationState::GeneratingBaseHeightmap;
    assert(mHeightGrid);
    // Send each row to the threadpool
    // TODO: Allocate extra threads as needed
    for (ui32 y = 0; y < mHeightGrid->mWidthPatches; ++y) {
        Services::Threadpool::ref().addTask([this, y, onPatchFinished]() {
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

void TerrainGenerator::generateBaseHeightmapGPU(i32 resolution, std::function<void(HeightmapPatchID)> onPatchFinished)
{

    GLint maxTextureSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    if (maxTextureSize < resolution) {
        panic("max supported texture size {} is less than required of {} for gpu gen", maxTextureSize, resolution);
    }

    // TODO: Only if resolution changes
    if (mHeightmapTexture) {
        glDeleteTextures(1, &mHeightmapTexture);
    }
    glCreateTextures(GL_TEXTURE_2D, 1, &mHeightmapTexture);
    glTextureStorage2D(
        mHeightmapTexture,
        1,           // one level, no mipmaps
        GL_R32F,    // internal format
        resolution,
        resolution
    );
    GLfloat clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    glClearTexImage(mHeightmapTexture, 0, GL_RED, GL_FLOAT, clearColor);
    glBindImageTexture(0, mHeightmapTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

    glCreateFramebuffers(1, &mFramebuffer);
    glNamedFramebufferTexture(mFramebuffer, GL_COLOR_ATTACHMENT0, mHeightmapTexture, 0);

    mOnPatchFinished = onPatchFinished;
    
    mIsGeneratingGPU = true;
    mNextGenerationIndex = 0;
    mGPUTerrainGenerations.resize(mHeightGrid->mWidthPatches * mHeightGrid->mWidthPatches);
    
}

void TerrainGenerator::cleanupPatchGPUData(HeightmapPatchID id) {
    if (id < mGPUTerrainGenerations.size()) {
        auto& gen = mGPUTerrainGenerations[id];
        if (gen.ssbo) {
            //glUnmapNamedBuffer(gen.ssbo);
            glDeleteBuffers(1, &gen.ssbo);
            gen.ssbo = 0;
        }
        if (gen.sync) {
            glDeleteSync(gen.sync);
            gen.sync = 0;
        }
    }
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
    distanceFromCenter2 += mGenerationData.mContinentOutlineScale * mGenerationData.mContinentOutlineNoise.compute(offsetToCenter.x, offsetToCenter.y);

    // Outline check
    if (distanceFromCenter2 > mGenerationData.mContinentRadiusSq) {
        // Ocean
        height -= (distanceFromCenter2 - mGenerationData.mContinentRadiusSq) * 0.0000001;
    }
    else {
        // Continent internals
        f64 lerp = (mGenerationData.mContinentRadiusSq - distanceFromCenter2) * 0.00000001;
        // Mountains
        f64 mountainDist = mGenerationData.mMountainsDistNoise.compute((f64)worldPos.x, (f64)worldPos.y);
        if (mountainDist > 0.0) {
            f64 mountain = mGenerationData.mMountainsNoise.compute((f64)worldPos.x, (f64)worldPos.y);
            height += lerp * mountain * glm::min(mountainDist, 1.0);
        }
    }
    return glm::clamp((f32)height, MIN_WORLD_GEN_HEIGHT, MAX_WORLD_GEN_HEIGHT);
}

void TerrainGenerator::finishPendingGeneration(PendingGPUTerrainGeneration& generation) {
    glDeleteSync(generation.sync);
    generation.sync = 0;

    const i32v2 gridXY = mHeightGrid->getSpatialGrid2D().getGridXYFromID(generation.patchID);

    GLfloat* heights = (GLfloat*)glMapNamedBufferRange(generation.pbo, 0, sizeof(f32) * HEIGHTMAP_VERT_SIZE_PER_PATCH, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

    Services::Threadpool::ref().addTask([this, &generation, heights]() {
        const f32v2 rootPos = mHeightGrid->getSpatialGrid2D().getWorldPosXYFromID(generation.patchID);
        const f32 widthVerts = mHeightGrid->getWidthPatches() * HEIGHTMAP_QUAD_WIDTH_PER_PATCH;
        const i32v2 rootXY = mHeightGrid->getSpatialGrid2D().getGridXYFromID(generation.patchID);
        HeightmapPatch& patch = mHeightGrid->mHeightData[generation.patchID];
        delete patch.mHeightData;
        patch.mHeightData = new HeightmapPatchData(generation.patchID);
        f32AABB3& aabb = patch.mHeightData->aabb;
        aabb.dims.x = HEIGHTMAP_PATCH_WIDTH;
        aabb.dims.y = HEIGHTMAP_PATCH_WIDTH;
        aabb.pos.x = rootPos.x;
        aabb.pos.y = rootPos.y;
        f32 minZ = FLT_MAX;
        f32 maxZ = -FLT_MAX;

      //  const i32 rootVert = rootXY.y * widthVerts + rootXY.x * HEIGHTMAP_QUAD_WIDTH_PER_PATCH;
        for (i32 y = 0; y < HEIGHTMAP_VERT_WIDTH_PER_PATCH; ++y) {
           // const i32 yStrideSource = y * widthVerts;
            const i32 yStrideTarget = y * HEIGHTMAP_VERT_WIDTH_PER_PATCH;
            for (i32 x = 0; x < HEIGHTMAP_VERT_WIDTH_PER_PATCH; ++x) {
                const i32 targetVert = yStrideTarget + x;
               // const i32 sourceVert = rootVert + yStrideSource + x;
                patch.mHeightData->setHeightAt(targetVert, heights[targetVert]);
            }
        }
        aabb.pos.z = minZ;
        aabb.dims.z = maxZ - minZ;
        patch.mHeightData->boundingSphere = boundingSphereFromAABB(aabb);

        mOnPatchFinished(patch.mHeightData->id);
    }, nullptr);

    checkGlError("TerrainGenerator::finishPendingGeneration");
}
static_assert(sizeof(f32) == sizeof(GLfloat), "God help us");

PendingGPUTerrainGeneration::~PendingGPUTerrainGeneration()
{
    if (ssbo) {
        glUnmapNamedBuffer(ssbo);
        glDeleteBuffers(1, &ssbo);
    }
    if (sync) {
        glDeleteSync(sync);
    }
    if (pbo) {
        glUnmapNamedBuffer(pbo);
        glDeleteBuffers(1, &pbo);
        pbo = 0;
    }
}
