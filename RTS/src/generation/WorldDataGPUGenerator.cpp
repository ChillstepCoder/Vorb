#include "stdafx.h"
#include "WorldDataGPUGenerator.h"

#include "world/IHeightmapGrid.h"
#include "world/biome/BiomeGrid.h"
#include "world/host/HostWorldData.h"

#include "rendering/MaterialShaderRepository.h"

// Match the shader
constexpr int LOCAL_GROUP_SIZE = 16;
constexpr ui32 ROWS_TO_GENERATE_PER_FRAME = 32; // POWER OF TWO REQUIRED

static std::atomic<ui32> sGenerationUID = 0;

WorldDataGPUGenerator::WorldDataGPUGenerator() = default;

WorldDataGPUGenerator::~WorldDataGPUGenerator() {
    cleanup();

    if (mTerrainSSBO) {
        glUnmapNamedBuffer(mTerrainSSBO);
        glDeleteBuffers(1, &mTerrainSSBO);
        mTerrainSSBO = 0;

        glUnmapNamedBuffer(mBiomeSSBO);
        glDeleteBuffers(1, &mBiomeSSBO);
        mBiomeSSBO = 0;

        glDeleteTextures(1, &mHeightTexture);
        glDeleteTextures(1, &mBiomeTexture);
    }
}

void WorldDataGPUGenerator::beginGeneration(HostWorldData& worldData, const WorldGenerationData& generationData, i32 resolution, std::function<void(HeightmapPatchID)> onPatchFinished) {
    mAllGenerationSentThisStep = false;
    mWorldData = &worldData;
    mGenerationData = generationData;
    mHeightGrid = worldData.heightmapGrid.get();
    mBiomeGrid = worldData.biomeGrid.get();
    mWorldCenter = f32v2(worldData.worldWidth * 0.5f);
    mWorldSeed = mGenerationData.getSeedHash();

    initResourcesIfNeeded(resolution);

    mOnPatchFinished = onPatchFinished;

    mNextGenerationIndex = 0;
    mGPUTerrainGenerations.resize(mHeightGrid->getWidthPatches() / ROWS_TO_GENERATE_PER_FRAME);

    mState = WorldGenerationState::GeneratingBaseHeightmapAndBiomes;
}

void WorldDataGPUGenerator::cleanup() {
    mNextGenerationIndex = 0;
    mNextRowToGenerate = 0;
    // Trigger all threads to stop functioning
    ++sGenerationUID;
    Services::Threadpool::ref().clearTasks();
    // Wait for all threads to finish
    while (Services::Threadpool::ref().getNumRunningThreads()) {
        Sleep(1);
    }

    mHeightGrid = nullptr;
    std::vector<PendingHeightGeneration>().swap(mGPUTerrainGenerations);

    mState = WorldGenerationState::None;
}

WorldGenerationState WorldDataGPUGenerator::update() {
    switch (mState)
    {
        case WorldGenerationState::None:
            break;
        case WorldGenerationState::GeneratingBaseHeightmapAndBiomes:
            updateGenerateBaseHeightmap();
            break;
        case WorldGenerationState::PropagatingBiomes:
            break;
        case WorldGenerationState::Done:
            break;
        default:
            panic("Unhandled state in WorldDataGPUGenerator::update");
            break;
    }

    return mState;
}


bool WorldDataGPUGenerator::initResourcesIfNeeded(i32 resolution)
{
    GLint maxTextureSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    if (maxTextureSize < resolution) {
        panic("max supported texture size {} is less than required of {} for gpu gen", maxTextureSize, resolution);
    }

    if (!mTerrainSSBO) {
        // Terrain
        const ui32 totalPatches = mHeightGrid->getTotalPatches();
        assert(!mTerrainSSBO);
        glCreateBuffers(1, &mTerrainSSBO);
        glNamedBufferStorage(mTerrainSSBO, sizeof(f32) * HEIGHTMAP_VERT_SIZE_PER_PATCH * totalPatches, nullptr, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
        mMappedHeights = (GLfloat*)glMapNamedBufferRange(mTerrainSSBO, 0, sizeof(f32) * HEIGHTMAP_VERT_SIZE_PER_PATCH * totalPatches, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
        glCreateTextures(GL_TEXTURE_2D, 1, &mHeightTexture);
        const ui32 hWidth = mHeightGrid->getWidthPatches() * HEIGHTMAP_VERT_WIDTH_PER_PATCH;
        glTextureStorage2D(mHeightTexture, 1, GL_R8, hWidth, hWidth);
        vg::sSamplerStates.LINEAR_CLAMP.setForTexture(mHeightTexture);

        // Biomes
        const ui32 biomesSizeBytes = mBiomeGrid->getTotalVertices() * sizeof(ui32);
        assert(!mBiomeSSBO);
        glCreateBuffers(1, &mBiomeSSBO);
        glNamedBufferStorage(mBiomeSSBO, biomesSizeBytes, nullptr, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
        mMappedBiomes = (ui32*)glMapNamedBufferRange(mBiomeSSBO, 0, biomesSizeBytes, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
        glCreateTextures(GL_TEXTURE_2D, 1, &mBiomeTexture);
        const ui32 bWidth = mBiomeGrid->getWidthVertices();
        glTextureStorage2D(mBiomeTexture, 1, GL_R8, bWidth, bWidth);
        vg::sSamplerStates.POINT_CLAMP.setForTexture(mBiomeTexture);
    }
}

void WorldDataGPUGenerator::updateGenerateBaseHeightmap() {
    // Send next row
    const ui32 rowsToGenerate = glm::min(ROWS_TO_GENERATE_PER_FRAME, mHeightGrid->mWidthPatches - mNextRowToGenerate);
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

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mTerrainSSBO);
        glBindImageTexture(0, mHeightTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mBiomeSSBO);
        glBindImageTexture(1, mBiomeTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8);

        HeightmapPatchID patchIdLeftmost = mNextRowToGenerate * mHeightGrid->mWidthPatches;
        const f32v2 rootPos = mHeightGrid->getSpatialGrid2D().getWorldPosXYFromID(patchIdLeftmost);
        const i32v2 vertXY = mHeightGrid->getSpatialGrid2D().getGridXYFromID(patchIdLeftmost) * HEIGHTMAP_VERT_WIDTH_PER_PATCH;

        glProgramUniform2fv(def->mProgram.getID(), def->getUniform("unPatchWorldPos"), 1, &rootPos.x);
        glProgramUniform1ui(def->mProgram.getID(), def->getUniform("unYStride"), mHeightGrid->mWidthPatches * (ui32)HEIGHTMAP_VERT_WIDTH_PER_PATCH);
        glProgramUniform2i(def->mProgram.getID(), def->getUniform("unVertexOffset"), vertXY.x, vertXY.y);

        // Dispatch compute
        glDispatchCompute(numGroups * mHeightGrid->mWidthPatches, numGroups * rowsToGenerate, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        PendingHeightGeneration& generation = mGPUTerrainGenerations[mNextRowToGenerate / ROWS_TO_GENERATE_PER_FRAME];
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

    while (mNextGenerationIndex != mGPUTerrainGenerations.size()) {
        PendingHeightGeneration& generation = mGPUTerrainGenerations[mNextGenerationIndex];
        if (!generation.generateStarted) {
            break;
        }
        GLenum waitResult = glClientWaitSync(generation.sync, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
        assert(generation.sync);
        if (waitResult == GL_ALREADY_SIGNALED || waitResult == GL_CONDITION_SATISFIED) {
            ++mNextGenerationIndex;
            finishPendingHeightGeneration(generation);
        }
        else if (waitResult == GL_TIMEOUT_EXPIRED) {
            // The GPU commands are not yet complete. Continue other tasks or loop back later.
            return;
        }
        else {
            panic("Terrain generation sync failed with GL_WAIT_FAILED");
        }
    }
}


void WorldDataGPUGenerator::finishPendingHeightGeneration(PendingHeightGeneration& generation) {
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
                // Build row
                const HeightmapPatchID patchId = rowIndex * mHeightGrid->getWidthPatches() + x;
                const f32v2 rootPos = mHeightGrid->getSpatialGrid2D().getWorldPosXYFromID(patchId);
                const ui32 totalWidthVerts = mHeightGrid->getWidthPatches() * HEIGHTMAP_QUAD_WIDTH_PER_PATCH;
                const i32v2 rootXY = mHeightGrid->getSpatialGrid2D().getGridXYFromID(patchId);
                HeightmapPatch& patch = mHeightGrid->mHeightData[patchId];
                if (!patch.mHeightData) {
                    patch.mHeightData = new HeightmapPatchData(patchId);
                }
                f32AABB3& aabb = patch.mHeightData->aabb;
                aabb.dims.x = HEIGHTMAP_PATCH_WIDTH;
                aabb.dims.y = HEIGHTMAP_PATCH_WIDTH;
                aabb.pos.x = rootPos.x;
                aabb.pos.y = rootPos.y;
                f32 minZ = FLT_MAX;
                f32 maxZ = -FLT_MAX;

                const i32 rootVert = rootXY.y * totalWidthVerts * HEIGHTMAP_QUAD_WIDTH_PER_PATCH + rootXY.x * HEIGHTMAP_QUAD_WIDTH_PER_PATCH;
                for (i32 y = 0; y < HEIGHTMAP_VERT_WIDTH_PER_PATCH; ++y) {
                    const i32 yStrideSource = y * totalWidthVerts;
                    const i32 yStrideTarget = y * HEIGHTMAP_VERT_WIDTH_PER_PATCH;
                    for (i32 x = 0; x < HEIGHTMAP_VERT_WIDTH_PER_PATCH; ++x) {
                        const i32 targetVert = yStrideTarget + x;
                        const i32 sourceVert = rootVert + yStrideSource + x;
                        patch.mHeightData->setHeightAtNoClamp(targetVert, mMappedHeights[sourceVert]);
                    }
                }
                aabb.pos.z = minZ;
                aabb.dims.z = maxZ - minZ;
                patch.mHeightData->boundingSphere = boundingSphereFromAABB(aabb);
                mOnPatchFinished(patch.mHeightData->id);
            }, nullptr);
        }
    }

    checkGlError("TerrainGenerator::finishPendingGeneration");
}
static_assert(sizeof(f32) == sizeof(GLfloat), "God help us");

PendingHeightGeneration::~PendingHeightGeneration()
{
    if (sync) {
        glDeleteSync(sync);
    }
}
