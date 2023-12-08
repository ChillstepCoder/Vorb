#include "stdafx.h"
#include "WorldDataGenerator.h"

#include "world/IHeightmapGrid.h"
#include "world/biome/BiomeGrid.h"
#include "world/host/HostWorldData.h"

#include "rendering/MaterialShaderRepository.h"

#include "generation/stages/BaseHeightmapAndBiomeGenerationStage.h"
#include "generation/stages/RiverGenerationStage.h"
#include "generation/WorldGenerationBlackboard.h"

WorldDataGenerator::WorldDataGenerator() = default;

WorldDataGenerator::~WorldDataGenerator() {
    cleanup();

    if (mHeightSSBO) {
        glUnmapNamedBuffer(mHeightSSBO);
        glDeleteBuffers(1, &mHeightSSBO);
        mHeightSSBO = 0;

        glUnmapNamedBuffer(mBiomeSSBO);
        glDeleteBuffers(1, &mBiomeSSBO);
        mBiomeSSBO = 0;

        glDeleteTextures(1, &mHeightTexture);
        if (mBiomeTexture) {
            glDeleteTextures(1, &mBiomeTexture);
        }
    }
}

void WorldDataGenerator::beginGeneration(HostWorldData& worldData, const WorldGenerationData& generationData, i32 resolution, std::function<void()> onFinished) {
    assert(mState == WorldGenerationState::None && "Make sure cleanup was called before generating again");
    
    mWorldData = &worldData;
    mGenerationData = generationData;
    mWorldSeed = generationData.mSeedHashed;

    initResourcesIfNeeded(resolution);

    mOnFinished = onFinished;

    mState = WorldGenerationState::GeneratingBaseHeightmapAndBiomes;

    mBlackboard = std::make_unique<WorldGenerationBlackboard>(mWorldData->heightmapGrid->getWidthPatches());

    initStages();
}

const char* WorldDataGenerator::getCurrentStageName() const
{
    if (IWorldGenerationStage* stage = tryGetCurrentStage()) {
        return stage->getStageName();
    }
    return "Finished Generating";
}

void WorldDataGenerator::cleanup() {
    if (IWorldGenerationStage* stage = tryGetCurrentStage()) {
        stage->abort();
    }
    std::vector<std::unique_ptr<IWorldGenerationStage>>().swap(mStages);

    // Abort current stage
    mState = WorldGenerationState::None;
}

WorldGenerationState WorldDataGenerator::update() {

    if (mState == WorldGenerationState::Done) {
        return mState;
    }
    if (IWorldGenerationStage* stage = tryGetCurrentStage()) {
        if (stage->update()) {
            ++mCurrentStageIndex;
            if (mCurrentStageIndex >= mStages.size()) {
                onCompletelyFinished();
                return mState;
            }
            else {
                // Deallocate prev stage and trigger next one
                mStages[mCurrentStageIndex - 1].reset();
                mStages[mCurrentStageIndex]->begin();
            }
        }
    }

    switch (mState)
    {
        case WorldGenerationState::None:
            break;
        case WorldGenerationState::GeneratingBaseHeightmapAndBiomes:
            break;
        case WorldGenerationState::SeedCorruptedBiomes:
            break;
        case WorldGenerationState::PropagatingBiomes:
            break;
        case WorldGenerationState::DetectPeaks:
            break;
        case WorldGenerationState::CarveRivers:
            break;
        case WorldGenerationState::Done:
            break;
        default:
            panic("Unhandled state in WorldDataGPUGenerator::update");
            break;
    }
    static_assert(e_count(WorldGenerationState) == 7);

    return mState;
}


void WorldDataGenerator::initStages() {

    mStages.reserve(2);

    mStages.emplace_back(std::make_unique<BaseHeightmapAndBiomeGenerationStage>(*this));
    mStages.emplace_back(std::make_unique<RiverGenerationStage>(*this));

    mCurrentStageIndex = 0;
    mStages[0]->begin();
}

IWorldGenerationStage* WorldDataGenerator::tryGetCurrentStage() const {
    if (mCurrentStageIndex < mStages.size()) {
        return mStages[mCurrentStageIndex].get();
    }
    return nullptr;
}

bool WorldDataGenerator::initResourcesIfNeeded(i32 resolution)
{
    GLint maxTextureSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    if (maxTextureSize < resolution) {
        panic("max supported texture size {} is less than required of {} for gpu gen", maxTextureSize, resolution);
    }

    if (!mHeightSSBO) {
        IHeightmapGrid* heightGrid = mWorldData->heightmapGrid.get();
        BiomeGrid* biomeGrid = mWorldData->biomeGrid.get();
        // Terrain
        const ui32 totalPatches = heightGrid->getTotalPatches();
        assert(!mHeightSSBO);
        glCreateBuffers(1, &mHeightSSBO);
        glNamedBufferStorage(mHeightSSBO, sizeof(f32) * HEIGHTMAP_VERT_SIZE_PER_PATCH * totalPatches, nullptr, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
        mMappedHeights = (GLfloat*)glMapNamedBufferRange(mHeightSSBO, 0, sizeof(f32) * HEIGHTMAP_VERT_SIZE_PER_PATCH * totalPatches, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
        glCreateTextures(GL_TEXTURE_2D, 1, &mHeightTexture);
        const ui32 hWidth = heightGrid->getWidthPatches() * HEIGHTMAP_VERT_WIDTH_PER_PATCH;
        glTextureStorage2D(mHeightTexture, 1, GL_R8, hWidth, hWidth);
        vg::sSamplerStates.LINEAR_CLAMP.setForTexture(mHeightTexture);

        // Biomes
        const ui32 biomesSizeBytes = biomeGrid->getTotalVertices() * sizeof(ui32);
        assert(!mBiomeSSBO);
        glCreateBuffers(1, &mBiomeSSBO);
        glNamedBufferStorage(mBiomeSSBO, biomesSizeBytes, nullptr, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
        mMappedBiomes = (ui32*)glMapNamedBufferRange(mBiomeSSBO, 0, biomesSizeBytes, GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
        glCreateTextures(GL_TEXTURE_2D, 1, &mBiomeTexture);
        const ui32 bWidth = biomeGrid->getWidthVertices();
        glTextureStorage2D(mBiomeTexture, 1, GL_R8, bWidth, bWidth);
        vg::sSamplerStates.POINT_CLAMP.setForTexture(mBiomeTexture);
    }
}

void WorldDataGenerator::onCompletelyFinished() {
    mOnFinished();
    mState = WorldGenerationState::Done;
}
