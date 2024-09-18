#include "stdafx.h"
#include "HistoryGenerationStage.h"

#include "world/World.h"
#include "world/host/HostWorldData.h"
#include "world/simulation/host/HostSimContext.h"
#include "generation/WorldGenerationData.h"

#include "resources/BiomeRepository.h"

#include "rendering/MaterialShaderRepository.h"


#include <random>

constexpr ui32 ROWS_PER_ROW_BLOCK = 16;
constexpr ui32 ROW_BLOCKS_PER_COMPUTE = 8;

HistoryGenerationStage::HistoryGenerationStage(WorldDataGenerator& generator, const std::unique_ptr<World>& worldPtr) :
    IWorldGenerationStage(generator), mWorldPtr(worldPtr)
{
    mRandomGen.setSeed(mWorldSeedInt + 1523u);
}

HistoryGenerationStage::~HistoryGenerationStage() = default;

void HistoryGenerationStage::begin()
{

    // Initialize desired event counts
    std::map<HistoryEventType, ui32> desiredHistoryEventCounts;
    desiredHistoryEventCounts[HistoryEventType::BanshiraSpawn] 
        = mRandomGen.getRandomUIntInRange(mGenerationData.mCorruptSpawnCountRange.x, mGenerationData.mCorruptSpawnCountRange.y);
    desiredHistoryEventCounts[HistoryEventType::ChernobogSpawn] 
        = mRandomGen.getRandomUIntInRange(mGenerationData.mCorruptSpawnCountRange.x, mGenerationData.mCorruptSpawnCountRange.y);

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

    assert(mWorldPtr);
    mHostSimContext = mWorldPtr->tryGetHostSimContext();
    assert(mHostSimContext);
    mHostSimContext->beginHistorySimulation();
}

bool HistoryGenerationStage::update() {

    mHistoryProgress = (f32)((f64)mHostSimContext->getSimTime() / (f64)mHistoryDurationMS);
    ++mTickCount;
    while (mNextEventIndex < mHistoryEvents.size() && mHistoryEvents[mNextEventIndex].time <= mHistoryProgress) {
        handleHistoryEvent(mHistoryEvents[mNextEventIndex]);
        ++mNextEventIndex;
    }

    if (mHistoryProgress >= 1.0f) {
        mHistoryProgress = 1.0f;
        mHostSimContext->endHistorySimulation();
        return true;
    }
    return false;
}

void HistoryGenerationStage::handleHistoryEvent(HistoryEvent& event) {
    switch (event.type) {
        case HistoryEventType::ChernobogSpawn:
            handleCorruptSpawn(LivingBiomeType::Chernobog);
            break;
        case HistoryEventType::BanshiraSpawn:
            handleCorruptSpawn(LivingBiomeType::Banshira);
            break;
        default:
            panic("Unhandled history event type");
    }
    assert(e_count(HistoryEventType) == 2);
}

void HistoryGenerationStage::handleCorruptSpawn(LivingBiomeType type) {
    // For now, generate a random coordinate and search for land
    RandomGenerator generator(mWorldSeedInt + 9853 + mTickCount);
    constexpr ui32 MAX_RETRY_COUNT = 16;
    ui32 retryCount = 0;
    const ui32 widthVerts = mBiomeGrid->getWidthVertices();
    do {
        BlockCoord blockPos(
            generator.getRandomUIntInRange(widthVerts * .05, widthVerts * .95),
            generator.getRandomUIntInRange(widthVerts * .05, widthVerts * .95)
        );

        // Find a valid space to put our biome
        if (mBiomeGrid->canBlockBeCorrupted(blockPos, type)) {
            mHostSimContext->addSimThreadTask([biomeGrid = mBiomeGrid, blockPos, type]() {
                biomeGrid->trySpawnLivingBiomeAtBlockPos(blockPos, type);
            });
            return;
        }
    } while (retryCount++ < MAX_RETRY_COUNT);
}

// Old GPU Method
//void HistoryGenerationStage::growBiomesStep() {
//    if (!mCurBiomeGrowPass.sync) {
//
//        mBiomeGrid->updateGrowth(mHistoryDurationMS);
//
//        const MaterialShaderDef* def = MaterialShaderRepository::get().tryGetLoadedAsset(CStrToken("biome_grow"));
//        if (!def) {
//            panic("biome_grow.comp was not loaded. Make sure it exists and is in assets.preload");
//        }
//        def->useCompute();
//        const ui32 gridWidth = mBiomeGrid->getWidthVertices();
//
//        const ui32 seed = Random::getCachedRandomSpecific(mGrowPassCount * 7);
//
//        glUniform1ui(def->getUniform("unBiomeMapWidth"), gridWidth);
//        glUniform1ui(def->getUniform("unIsOdd"), (ui32)mGrowPassIsOdd);
//        glUniform1ui(def->getUniform("unYStart"), mGrowPassRowBlockIndex * ROWS_PER_ROW_BLOCK);
//        glUniform1ui(def->getUniform("unSeed"), seed);
//
//        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mBiomeSSBO);
//        glBindImageTexture(0, mBiomeTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8);
//
//        constexpr ui32 LOCAL_GROUP_SIZE = 16;
//        const ui32 blockCount = glm::min(((gridWidth - ROWS_PER_ROW_BLOCK) - mGrowPassRowBlockIndex * ROWS_PER_ROW_BLOCK) / ROWS_PER_ROW_BLOCK, ROW_BLOCKS_PER_COMPUTE);
//        assert(blockCount > 0);
//
//        // Dispatch half as many columns as we will offset into checkerboard in shader
//        // Minus 1 for 8 pixel padded border so we never access outside the grid
//        glDispatchCompute((gridWidth / LOCAL_GROUP_SIZE) / 2 - 1, blockCount, 1);
//        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
//
//        mCurBiomeGrowPass.sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
//        mCurBiomeGrowPass.isOdd = mGrowPassIsOdd;
//        mCurBiomeGrowPass.row = mGrowPassRowBlockIndex;
//        mCurBiomeGrowPass.numBlocks = blockCount;
//
//        checkGlError("HistoryGenerationStage::growBiomesStep");
//
//        if (mGrowPassRowBlockIndex < mGrowPassRowsPerPass - ROW_BLOCKS_PER_COMPUTE) {
//            mGrowPassRowBlockIndex += ROW_BLOCKS_PER_COMPUTE;
//        }
//        else {
//            mGrowPassIsOdd = !mGrowPassIsOdd;
//            mGrowPassRowBlockIndex = 0;
//            --mGrowPassCount;
//        }
//    }
//}

// Old GPU method
//void HistoryGenerationStage::downloadBiomes() {
//    bool isOdd = mCurBiomeGrowPass.isOdd;
//    const ui32 widthVerts = mBiomeGrid->getWidthVertices();
//    // Padded by 8 with checkerboard pattern
//    const int isOddOffset = isOdd ? 1 : 0;
//    const i32 yStart = 8 + mCurBiomeGrowPass.row * ROWS_PER_ROW_BLOCK;
//    for (i32 y = yStart; y < yStart + ROWS_PER_ROW_BLOCK * mCurBiomeGrowPass.numBlocks; ++y) {
//        const ui32 yStride = y * widthVerts;
//        for (i32 x = 8 + ((y + isOdd) % 2); x < widthVerts - 8; x += 2) {
//            const ui32 index = yStride + x;
//            BiomeVertex& vertex = mBiomeGrid->getVertexForGenerationFromBlockPos(BlockCoord(x, y));
//            vertex.biomeUniqueId = BiomeUniqueID(mMappedBiomes[index]);
//        }
//    }
//}

void HistoryGenerationStage::renderImguiControls() {

}
