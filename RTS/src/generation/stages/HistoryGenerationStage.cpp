#include "stdafx.h"
#include "HistoryGenerationStage.h"

#include "world/host/HostWorldData.h"
#include "generation/WorldGenerationData.h"
#include "generation/WorldGenerationBlackboard.h"

#include "rendering/MaterialShaderRepository.h"
#include "math/Random.h"

#include <random>

constexpr ui32 ROWS_PER_ROW_BLOCK = 16;
constexpr ui32 ROW_BLOCKS_PER_COMPUTE = 8;
void HistoryGenerationStage::begin()
{
    RandomGenerator generator(ui32(mWorldSeed * 100.0f) + 1523u);

    // Initialize desired event counts
    std::map<HistoryEventType, ui32> desiredHistoryEventCounts;
    desiredHistoryEventCounts[HistoryEventType::BanshiraSpawn] = generator.getRandomUIntInRange(3, 6);
    desiredHistoryEventCounts[HistoryEventType::ChernobogSpawn] = generator.getRandomUIntInRange(3, 6);

    // Add all events
    for (auto& [eventType, desiredCount] : desiredHistoryEventCounts) {
        for (ui32 i = 0; i < desiredCount; ++i) {
            HistoryEvent& newEvent = mHistoryEvents.emplace_back();
            newEvent.type = eventType;
        }
    }

    // Shuffle all events together
    auto rng = std::default_random_engine{};
    rng.seed((int)(mWorldSeed * 100.0f) + 14223);
    std::shuffle(std::begin(mHistoryEvents), std::end(mHistoryEvents), rng);

    // Assign event times (TODO: Make this more interesting)
    const f32 averageTimeBetweenEvents = 1.0f / mHistoryEvents.size();

    f32 time = 0.0f;
    for (size_t i = 0; i < mHistoryEvents.size() - 1; ++i) {
        mHistoryEvents[i].time = time;
        time += averageTimeBetweenEvents;
    }
    // Last event exactly at 1.0f
    mHistoryEvents.back().time = 1.0f;

    // Minus 1 for padding
    mGrowPassRowsPerPass = mBiomeGrid->getWidthVertices() / ROWS_PER_ROW_BLOCK - 1;
    mGrowPassCount = mGenerationData.mBiomeGrowPassCount;
}

bool HistoryGenerationStage::update() {

    updateBiomes();

    if (!allEventsTriggered) {
        mHistoryProgress += mTickTime;
        while (mNextEventIndex < mHistoryEvents.size() && mHistoryEvents[mNextEventIndex].time <= mHistoryProgress) {
            handleHistoryEvent(mHistoryEvents[mNextEventIndex]);
            ++mNextEventIndex;
        }

        if (mHistoryProgress >= 1.0f) {
            allEventsTriggered = true;
            mHistoryProgress = 1.0f;
        }
    }
    else if (mGrowPassCount == 0 && !mCurBiomeGrowPass.sync) {
        return true;
    }
    return false;
}

void HistoryGenerationStage::handleHistoryEvent(HistoryEvent& event) {
    switch (event.type) {
        case HistoryEventType::ChernobogSpawn:
            break;
        case HistoryEventType::BanshiraSpawn:
            break;
        default:
            panic("Unhandled history event type");
    }
    assert(e_count(HistoryEventType) == 2);

}

void HistoryGenerationStage::updateBiomes()
{
    if (mCurBiomeGrowPass.sync) {
        GLenum waitResult = glClientWaitSync(mCurBiomeGrowPass.sync, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
        if (waitResult == GL_ALREADY_SIGNALED || waitResult == GL_CONDITION_SATISFIED) {
            glDeleteSync(mCurBiomeGrowPass.sync);
            mCurBiomeGrowPass.sync = 0;
            downloadBiomes();
        }
        else {
            return;
        }
    }
    if (mGrowPassCount != 0) {
        growBiomesStep();
    }
}

void HistoryGenerationStage::growBiomesStep() {
    if (!mCurBiomeGrowPass.sync) {

        const MaterialShaderDef* def = MaterialShaderRepository::get().tryGetLoadedAsset(CStrToken("biome_grow"));
        if (!def) {
            panic("biome_grow.comp was not loaded. Make sure it exists and is in assets.preload");
        }
        def->useCompute();
        const ui32 gridWidth = mBiomeGrid->getWidthVertices();

        const ui32 seed = Random::getCachedRandomSpecific(mGrowPassCount * 7);

        glUniform1ui(def->getUniform("unBiomeMapWidth"), gridWidth);
        glUniform1ui(def->getUniform("unIsOdd"), (ui32)mGrowPassIsOdd);
        glUniform1ui(def->getUniform("unYStart"), mGrowPassRowBlockIndex * ROWS_PER_ROW_BLOCK);
        glUniform1ui(def->getUniform("unSeed"), seed);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mBiomeSSBO);
        glBindImageTexture(0, mBiomeTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8);

        constexpr ui32 LOCAL_GROUP_SIZE = 16;
        const ui32 blockCount = glm::min(((gridWidth - ROWS_PER_ROW_BLOCK) - mGrowPassRowBlockIndex * ROWS_PER_ROW_BLOCK) / ROWS_PER_ROW_BLOCK, ROW_BLOCKS_PER_COMPUTE);
        assert(blockCount > 0);

        // Dispatch half as many columns as we will offset into checkerboard in shader
        // Minus 1 for 8 pixel padded border so we never access outside the grid
        glDispatchCompute((gridWidth / LOCAL_GROUP_SIZE) / 2 - 1, blockCount, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        mCurBiomeGrowPass.sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        mCurBiomeGrowPass.isOdd = mGrowPassIsOdd;
        mCurBiomeGrowPass.row = mGrowPassRowBlockIndex;
        mCurBiomeGrowPass.numBlocks = blockCount;

        checkGlError("HistoryGenerationStage::growBiomesStep");

        if (mGrowPassRowBlockIndex < mGrowPassRowsPerPass - ROW_BLOCKS_PER_COMPUTE) {
            mGrowPassRowBlockIndex += ROW_BLOCKS_PER_COMPUTE;
        }
        else {
            mGrowPassIsOdd = !mGrowPassIsOdd;
            mGrowPassRowBlockIndex = 0;
            --mGrowPassCount;
        }
    }
}

void HistoryGenerationStage::downloadBiomes() {
    PreciseTimer timer;
    bool isOdd = mCurBiomeGrowPass.isOdd;
    const ui32 widthVerts = mBiomeGrid->getWidthVertices();
    // Padded by 8 with checkerboard pattern
    const int isOddOffset = isOdd ? 1 : 0;
    const i32 yStart = 8 + mCurBiomeGrowPass.row * ROWS_PER_ROW_BLOCK;
    for (i32 y = yStart; y < yStart + ROWS_PER_ROW_BLOCK * mCurBiomeGrowPass.numBlocks; ++y) {
        const ui32 yStride = y * widthVerts;
        for (i32 x = 8 + ((y + isOdd) % 2); x < widthVerts - 8; x += 2) {
            const ui32 index = yStride + x;
            BiomeVertex& vertex = mBiomeGrid->getVertexForGeneration(index);
            vertex.biomeUniqueId = mMappedBiomes[index];
        }
    }
    LOG_CRITICAL("Finished in {} ms", timer.stop());
}

BiomeGrowPass::~BiomeGrowPass() {
    if (sync) {
        glDeleteSync(sync);
    }
}
