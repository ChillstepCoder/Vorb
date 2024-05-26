#include "stdafx.h"
#include "WorldDataGenerator.h"
#include "world/World.h"
#include "world/IHeightmapGrid.h"
#include "world/biome/BiomeGrid.h"
#include "world/host/HostWorldData.h"
#include "world/WorldDestroyer.h"

#include "rendering/MaterialShaderRepository.h"

#include "generation/stages/BaseHeightmapAndBiomeGenerationStage.h"
#include "generation/stages/RiverGenerationStage.h"
#include "generation/stages/MarkupGenerationStage.h"
#include "generation/stages/HistoryGenerationStage.h"
#include "generation/WorldGenerationBlackboard.h"

#include "serialization/GameSaveManager.h"

#include "screens/ScreenState.h"

constexpr ui32 HISTORY_STAGE_INDEX = 3;

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
    assert(!mFinished && "Make sure cleanup was called before generating again");

    mSkipToHistory = 0;
    mCurrentStageIndex = 0;
    mWorldData = &worldData;
    mGenerationData = generationData;
    mWorldSeed = generationData.mSeedHashed;

    initResourcesIfNeeded(resolution);

    mOnFinished = onFinished;

    mBlackboard = std::make_unique<WorldGenerationBlackboard>(mWorldData->heightmapGrid->getWidthPatches());

    if (MainMenuScreenGlobalState::startGameType == StartGameType::NewWorldFromTemplate) {
        assert(!sGameWorld);
        sGameWorld = std::make_unique<World>(WorldNetMode::Host, mWorldData);

        if (!GameSaveManager::get().loadWorld(*sGameWorld, MainMenuScreenGlobalState::loadWorldPath)) {
            panic("Failed to load world template {}", MainMenuScreenGlobalState::loadWorldPath.string());
        }

        // Asychronous build texture pixel data, as it is a lot of pixels and can take a while for a single thread
        std::vector <ui8> biomePixelData;
        std::vector <ui8> heightPixelData;
        std::atomic_int runningTasks = 0;
        constexpr ui32 NUM_BIOME_TASKS = 8; // Must be power of two
        constexpr ui32 NUM_HEIGHT_TASKS = 16; // Must be power of two

        BiomeGrid& biomeGrid = *mWorldData->biomeGrid;
        const ui32 bWidth = biomeGrid.getWidthVertices();
        IHeightmapGrid& heightGrid = *mWorldData->heightmapGrid;
        const ui32 patchesWidth = heightGrid.getWidthPatches();
        const ui32 patchWidthVerts = heightGrid.getPatchWidthVerts();
        const ui32 tWidth = patchesWidth * patchWidthVerts;

        biomePixelData.resize(SQ(bWidth));
        heightPixelData.resize(SQ(tWidth));

        LOG_INFO("Updating biome + height textures...");
        PreciseTimer timer;
        // Biome
        const ui32 B_ROWS = bWidth / NUM_BIOME_TASKS;
        const ui32 T_ROWS = patchesWidth / NUM_HEIGHT_TASKS;
        assert(B_ROWS > 0);
        assert(T_ROWS > 0);
        for (ui32 i = 0; i < NUM_BIOME_TASKS; ++i) {
            ++runningTasks;
            Services::Threadpool::ref().addTask([this, &runningTasks, &biomePixelData, B_ROWS, start = i * B_ROWS, &biomeGrid, bWidth]() {
                for (ui32 y = start; y < start + B_ROWS; ++y) {
                    for (ui32 x = 0; x < bWidth; ++x) {
                        const BiomeUniqueID id = biomeGrid.getVertexForGenerationFromBlockPos(i32v2(x, y)).biomeUniqueId;
                        biomePixelData[y * bWidth + x] = (ui8)id;
                        mMappedBiomes[y * bWidth + x] = (ui32)id;
                    }
                }
                --runningTasks;
            });
        }
        for (ui32 i = 0; i < NUM_HEIGHT_TASKS; ++i) {
            ++runningTasks;
            Services::Threadpool::ref().addTask([this, &runningTasks, &heightPixelData, T_ROWS, start = i * T_ROWS, tWidth, patchesWidth, patchWidthVerts, &heightGrid]() {
                // Cache efficient iterate
                for (ui32 py = start; py < start + T_ROWS; ++py) {
                    const ui32 pyOffset = py * patchWidthVerts;
                    for (ui32 px = 0; px < patchesWidth; ++px) {
                        const ui32 pxOffset = px * patchWidthVerts;
                        HeightmapPatch& patch = heightGrid.getPatchForGeneration(py * patchesWidth + px);
                        for (ui32 y = 0; y < patchWidthVerts; ++y) {
                            for (ui32 x = 0; x < patchWidthVerts; ++x) {
                                heightPixelData[(pyOffset + y) * tWidth + pxOffset + x] = (ui8)glm::clamp((patch.getHeightAt(y * patchWidthVerts + x) + 127.0f) - 100.0f, 0.0f, 255.0f);
                            }
                        }
                    }
                }
                --runningTasks;
            });
        }
        do {
            Sleep(16);
        } while (runningTasks);

        mSkipToHistory = true;
        LOG_DEBUG("  Took {} ms", timer.stop());
        glFlushMappedNamedBufferRange(mBiomeSSBO, SQ(bWidth), sizeof(ui32));
        glTextureSubImage2D(
            mBiomeTexture,
            0, 0, 0,
            bWidth, bWidth,
            GL_RED, GL_UNSIGNED_BYTE,
            biomePixelData.data()
        );
        glTextureSubImage2D(
            mHeightTexture,
            0, 0, 0,
            tWidth, tWidth,
            GL_RED, GL_UNSIGNED_BYTE,
            heightPixelData.data()
        );

        LOG_INFO("Done.");
    }
    else {
        assert(MainMenuScreenGlobalState::startGameType == StartGameType::NewWorld);
    }
    initStages();

}

const char* WorldDataGenerator::getCurrentStageName() const {
    if (IWorldGenerationStage* stage = tryGetCurrentStage()) {
        return stage->getStageName();
    }
    return "Finished Generating";
}

f32 WorldDataGenerator::getCurrentStageProgress() const {
    if (IWorldGenerationStage* stage = tryGetCurrentStage()) {
        return stage->getProgress();
    }
    return 0.0f;
}

void WorldDataGenerator::currentStageDebugDraw(const OrthoCamera& camera) {
    if (IWorldGenerationStage* stage = tryGetCurrentStage()) {
        stage->debugDraw(camera);
    }
}

void WorldDataGenerator::renderCurrentStageImguiControls() {
    if (IWorldGenerationStage* stage = tryGetCurrentStage()) {
        stage->renderImguiControls();
    }
}

void WorldDataGenerator::cleanup() {
    if (IWorldGenerationStage* stage = tryGetCurrentStage()) {
        stage->abort();
    }
    std::vector<std::unique_ptr<IWorldGenerationStage>>().swap(mStages);

    if (sGameWorld) {
        WorldDestroyer::shutdownWorld(*sGameWorld);
        sGameWorld.reset();
    }

    mFinished = false;
}

bool WorldDataGenerator::update() {

    if (mFinished) {
        return true;
    }
    if (IWorldGenerationStage* stage = tryGetCurrentStage()) {
        if (stage->update()) {
            ++mCurrentStageIndex;
            if (mCurrentStageIndex >= mStages.size()) {
                onCompletelyFinished();
                return true;
            }
            else {
                // Deallocate prev stage and trigger next one
                mStages[mCurrentStageIndex - 1].reset();
                mStages[mCurrentStageIndex]->begin();
            }
        }
    }

    return false;
}

World* WorldDataGenerator::tryGetWorld() {
    ASSERT_RENDER_THREAD(); // Not thread safe with allocate
    return sGameWorld.get();
}

void WorldDataGenerator::initStages() {

    mStages.reserve(2);

    if (!mSkipToHistory) {
        mStages.emplace_back(std::make_unique<BaseHeightmapAndBiomeGenerationStage>(*this));
        mStages.emplace_back(std::make_unique<RiverGenerationStage>(*this));
        mStages.emplace_back(std::make_unique<MarkupGenerationStage>(*this, sGameWorld));
    }
    mStages.emplace_back(std::make_unique<HistoryGenerationStage>(*this, sGameWorld));

    mStages[mCurrentStageIndex]->begin();
}

IWorldGenerationStage* WorldDataGenerator::tryGetCurrentStage() const {
    if (mCurrentStageIndex < mStages.size()) {
        return mStages[mCurrentStageIndex].get();
    }
    return nullptr;
}

void WorldDataGenerator::initResourcesIfNeeded(i32 resolution)
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
        glNamedBufferStorage(mBiomeSSBO, biomesSizeBytes, nullptr, GL_MAP_WRITE_BIT | GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
        mMappedBiomes = (ui32*)glMapNamedBufferRange(mBiomeSSBO, 0, biomesSizeBytes, GL_MAP_WRITE_BIT | GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
        glCreateTextures(GL_TEXTURE_2D, 1, &mBiomeTexture);
        const ui32 bWidth = biomeGrid->getWidthVertices();
        glTextureStorage2D(mBiomeTexture, 1, GL_R8, bWidth, bWidth);
        vg::sSamplerStates.POINT_CLAMP.setForTexture(mBiomeTexture);
    }
}

void WorldDataGenerator::onCompletelyFinished() {
    mOnFinished();
    mFinished = true;
}
