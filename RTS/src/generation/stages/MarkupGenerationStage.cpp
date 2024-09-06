#include "stdafx.h"
#include "MarkupGenerationStage.h"

#include "world/host/HostWorldData.h"
#include "generation/WorldGenerationData.h"

// TODO: Remove??
#include "generation/ChunkGenerator.h"
#include "serialization/GameSaveManager.h"

MarkupGenerationStage::MarkupGenerationStage(WorldDataGenerator& generator, std::unique_ptr<World>& worldPtr) :
    IWorldGenerationStage(generator), mWorldPtr(worldPtr)
{

}

MarkupGenerationStage::~MarkupGenerationStage()
{

}

void MarkupGenerationStage::begin() {
    allocateWorld();

    mTotalTimer.start();

    beginInitialMarkupGen();
}

bool MarkupGenerationStage::update() {

    if (mRunningThreads == mFinishedThreads) {
        switch (mState) {
            case MarkupGenerationStageState::InitialMarkup:
                beginChunkAndBodyMarkupGen();
                break;
            case MarkupGenerationStageState::ChunkMarkup:
                onFinished();
                return true;
            default:
                assert(false);
                break;

        }
        static_assert(e_count(MarkupGenerationStageState) == 2);
    }

    return false;
}

void MarkupGenerationStage::allocateWorld() {
    assert(!mWorldPtr);
    ASSERT_RENDER_THREAD(); // World ptr not thread safe rn
    mWorldPtr = std::make_unique<World>(WorldNetMode::Host, mWorldData);
}

void MarkupGenerationStage::beginInitialMarkupGen() {
    mFinishedThreads = 0;
    mRunningThreads = 1;
    mState = MarkupGenerationStageState::InitialMarkup;
    const ui32 genID = sGenerationUID;
    Services::Threadpool::ref().addTask([this, genID]() {
        // Check for interrupt
        if (genID != sGenerationUID) {
            return;
        }
        generateInitialMarkup();
        ++mFinishedThreads;
    });
}

void MarkupGenerationStage::beginChunkAndBodyMarkupGen() { 
    constexpr ui32 JOBS = 32; // Must be POT
    assert((mWorldData->worldWidth / CHUNK_WIDTH) >= JOBS);
    const ui32 chunksRowsPerjob = (mWorldData->worldWidth / CHUNK_WIDTH) / JOBS;
    mFinishedThreads = 0;
    mRunningThreads = JOBS;
    mState = MarkupGenerationStageState::ChunkMarkup;
    const ui32 genID = sGenerationUID;
    for (ui32 i = 0; i < JOBS; ++i) {
        Services::Threadpool::ref().addTask([this, i, chunksRowsPerjob, genID]() {
            // Check for interrupt
            if (genID != sGenerationUID) {
                return;
            }
            generateChunkMarkup(i, chunksRowsPerjob);
            ++mFinishedThreads;
        });
    }

    // Distribute body markup jobs, one per body is fine
    assert(mBodyBorderSets.size() == mTotalBodies);
    mRunningThreads += mTotalBodies;

    for (ui32 i = 0; i < mTotalBodies; ++i) {
        Services::Threadpool::ref().addTask([this, i, genID]() {
            // Check for interrupt
            if (genID != sGenerationUID) {
                return;
            }
            generateBodyMarkup(i);
            ++mFinishedThreads;
        });
    }
}

void MarkupGenerationStage::generateInitialMarkup()
{
    // Generates useful data that will speed up simulation, such as whether this is land, the current continent (or ocean/lake) index (disjoint set)
   // Same resolution of the biome grid
    PreciseTimer timer;
    LOG_DEBUG("Begin markup generation");
    mTotalBodies = 0;

    const i32 markupWidth = mMarkupGrid->getWidthVertices();
    assert(markupWidth == mBiomeGrid->getWidthVertices()); // THIS NEEDS TO ALWAYS BE TRUE FOR THIS PROCESS
    BitArray visited(mMarkupGrid->getTotalVertices());
    std::vector<i16v2> stack;
    std::vector<ui32> currentSet;
    std::vector<i16v2> currentBorderSet;
    mBodyBorderSets.reserve(2000); // Usually lots of bodies
    // Blowing this reserve should be rare
    stack.reserve(mMarkupGrid->getTotalVertices());
    currentSet.reserve(mMarkupGrid->getTotalVertices());

    // Find all bodies (Water vs Land)
    i16v2 start(0, 0);
    do {
        i32 startIndex = start.y * markupWidth + start.x;
        startIndex = visited.getIndexOfFirstUnsetBit(startIndex);
        if (startIndex == UINT32_MAX) [[unlikely]] break;
        start.x = startIndex % markupWidth;
        start.y = startIndex / markupWidth;
        const bool groupIsWater = mBiomeGrid->getVertexForGenerationFromBlockPos(start).isWater();
        stack.push_back(start);
        currentSet.resize(0);
        currentBorderSet.resize(0);
        currentBorderSet.reserve(256); // Most are < 100 but some are 10000+ like oceans or huge islands
        do {
            i16v2 pos = stack.back();
            stack.pop_back();
            while (pos.x >= 0 && mBiomeGrid->getVertexForGenerationFromBlockPos(pos).isWater() == groupIsWater) --pos.x;
            ++pos.x;

            i32 index = pos.y * markupWidth + pos.x;
            bool spanBelow = false;
            bool spanAbove = false;
            // Don't double process
            if (!visited.getBit(index)) {
                bool isBorder = true; // First is always border
                do {
                    currentSet.push_back(index);
                    visited.setBit(index);
                    mMarkupGrid->getMarkupForGeneration(index).bodyId = mTotalBodies;
                    if (pos.y > 0) [[likely]] {
                        if (!spanBelow && mBiomeGrid->getVertexForGenerationFromBlockPos(pos - i16v2(0, 1)).isWater() == groupIsWater) {
                            stack.emplace_back(pos.x, pos.y - 1);
                            spanBelow = true;
                        }
                        else if (mBiomeGrid->getVertexForGenerationFromBlockPos(pos - i16v2(0, 1)).isWater() != groupIsWater) {
                            isBorder = true;
                            spanBelow = false;
                        }
                    }
                    else {
                        isBorder = true;
                    }
                    if (pos.y < markupWidth - 1) [[likely]] {
                        if (!spanAbove && mBiomeGrid->getVertexForGenerationFromBlockPos(pos + i16v2(0, 1)).isWater() == groupIsWater) {
                            stack.emplace_back(pos.x, pos.y + 1);
                            spanAbove = true;
                        }
                        else if (mBiomeGrid->getVertexForGenerationFromBlockPos(pos + i16v2(0, 1)).isWater() != groupIsWater) {
                            isBorder = true;
                            spanAbove = false;
                        }
                    }
                    else {
                        isBorder = true;
                    }
                    if (isBorder) {
                        currentBorderSet.emplace_back(pos);
                        isBorder = false;
                    }
                    ++pos.x;
                    ++index;
                } while (pos.x < markupWidth && mBiomeGrid->getVertexForGenerationFromBlockPos(pos).isWater() == groupIsWater);
                // Right side border
                const i16v2 prevPos = i16v2(pos.x - 1, pos.y);
                assert(currentBorderSet.size());
                // Check if already flagged as border
                if (currentBorderSet.back() != prevPos) {
                    currentBorderSet.emplace_back(prevPos);
                }
            }
        } while (stack.size());
        if (currentSet.size()) {
            ++mTotalBodies;
            mBodyBorderSets.emplace_back(std::move(currentBorderSet));
            // Set up the body
            WorldBodyMarkupData bodyData;
            bodyData.sizeBlocks = currentSet.size();

            WorldMarkupFlags bodyBit;
            if (groupIsWater) {
                constexpr ui32 SIZE_THRESHOLD = 65536 * 3;
                if (bodyData.sizeBlocks > SIZE_THRESHOLD) {
                    bodyData.bodyType = WorldMarkupBodyType::Ocean;
                    bodyBit = WorldMarkupFlags::Ocean;
                }
                else {
                    bodyData.bodyType = WorldMarkupBodyType::Lake;
                    bodyBit = WorldMarkupFlags::Lake;
                }
            }
            else {
                constexpr ui32 SIZE_THRESHOLD = 65536;
                if (bodyData.sizeBlocks > SIZE_THRESHOLD) {
                    bodyData.bodyType = WorldMarkupBodyType::LargeIsland;
                    bodyBit = WorldMarkupFlags::LargeIsland;
                }
                else {
                    bodyData.bodyType = WorldMarkupBodyType::Island;
                    bodyBit = WorldMarkupFlags::Island;
                }
            }
            for (ui32 pos : currentSet) {
                mMarkupGrid->getMarkupForGeneration(pos).flags.setBit(bodyBit);
            }
            mMarkupGrid->addBodyFromGeneration(bodyData);
        }
        if (start.x == markupWidth - 1) {
            start.x = 0;
            ++start.y;
        }
        else {
            ++start.x;
        }
    } while (start.y != markupWidth);

    mBodyChunkListMutexes = std::make_unique<std::mutex[]>(mTotalBodies);
    LOG_DEBUG("Initial markup took {} total ms and found {} islands", timer.elapsedMs(), mTotalBodies);
    LOG_DEBUG("End markup generation");
}

void MarkupGenerationStage::generateChunkMarkup(ui32 jobIndex, ui32 chunkRowsPerJob) {
    const ui32 worldWidthChunks = mWorldData->worldWidth / CHUNK_WIDTH;
    const ui32 worldWidthBlocks = mWorldData->worldWidth / BLOCK_WIDTH;
    const ui32 startChunkRow = jobIndex * chunkRowsPerJob;
    constexpr ui32 WIDTH_BLOCKS = CHUNK_WIDTH / BLOCK_WIDTH;
    constexpr ui32 SIZE_BLOCKS = SQ(WIDTH_BLOCKS);

    RandomGenerator gen(mWorldData->worldSeed * jobIndex);

    // Allows us to count the number of bodies in each chunk
    FlatMap<ui32 /*body index*/, ui32 /*count*/> bodyCounts;
    bodyCounts.reserve(4); // This is more than we will need in almost every case

    PreciseTimer timer;
    for (ui32 chunkY = startChunkRow; chunkY < startChunkRow + chunkRowsPerJob; ++chunkY) {
        const ui32 blockYStart = chunkY * WIDTH_BLOCKS;
        for (ui32 chunkX = 0; chunkX < worldWidthChunks; ++chunkX) {
            const ui32 blockXStart = chunkX * WIDTH_BLOCKS;

            const ChunkID chunkID = chunkY * worldWidthChunks + chunkX;
            WorldChunkMarkupData& chunkMarkup = mMarkupGrid->getChunkMarkupForGeneration(chunkID);

            bodyCounts.clear();
            ui32 landBlocks = 0;
            for (ui32 bY = 0; bY < WIDTH_BLOCKS; ++bY) {
                const ui32 blockYOffset = (blockYStart + bY) * worldWidthBlocks;
                for (ui32 bX = 0; bX < WIDTH_BLOCKS; ++bX) {
                    const ui32 blockIndex = blockYOffset + blockXStart + bX;
                    WorldMarkupData& blockMarkup = mMarkupGrid->getMarkupForGeneration(blockIndex);
                    if (blockMarkup.flags.isMaskPartiallySet(WORLD_MARKUP_FLAGS_LAND_MASK)) {
                        ++landBlocks;
                    }
                    auto&& it = bodyCounts.find(blockMarkup.bodyId);
                    if (it == bodyCounts.end()) [[unlikely]] {
                        bodyCounts.emplace(blockMarkup.bodyId, 1);
                    }
                    else {
                        ++it->second;
                    }
                }
            }

            ui32 highestLandCount = 0;
            ui32 highestWaterCount = 0;
            for (auto&& it : bodyCounts) {
                if (mMarkupGrid->getBodyData(it.first).bodyType <= WorldMarkupBodyType::BODY_TYPE_LAND_TERM) {
                    if (it.second > highestLandCount) {
                        chunkMarkup.mainLandBodyID = it.first;
                        highestLandCount = it.second;
                    }
                }
                else {
                    if (it.second > highestWaterCount) {
                        chunkMarkup.mainWaterBodyID = it.first;
                        highestWaterCount = it.second;
                    }
                }
            }

            // Only generate chunk if we have a land chunk
            if (chunkMarkup.mainLandBodyID != UINT32_MAX) {
                mWorldPtr->getChunkGenerator().generateSimChunk(mWorldPtr->getSimChunkGrid().getChunkForGeneration(chunkID), *mWorldPtr);
            }

            if (chunkMarkup.mainLandBodyID != UINT32_MAX) {
                WorldBodyMarkupData& bodyData = mMarkupGrid->getBodyDataForGeneration(chunkMarkup.mainLandBodyID);
                assert(chunkMarkup.mainLandBodyID < mTotalBodies);
                std::lock_guard lock(mBodyChunkListMutexes[chunkMarkup.mainLandBodyID]);
                bodyData.chunks.emplace_back(chunkID);
            }
            chunkMarkup.landRatio = (f32)landBlocks / (f32)SIZE_BLOCKS;
            // TODO: How compute?
            chunkMarkup.settleDesirability = chunkMarkup.landRatio * gen.getRandomFloatUnsigned(); 
        }
    }
    //LOG_DEBUG("  Row {} processed in {} ms with {} blocks {}  {}", jobIndex, timer.elapsedMs(), chunkRowsPerJob * worldWidthChunks * SIZE_BLOCKS, count, chunkRowsPerJob);
}

void MarkupGenerationStage::generateBodyMarkup(ui32 bodyIndex) {

    PreciseTimer timer;
    WorldBodyMarkupData& bodyData = mMarkupGrid->getBodyDataForGeneration(bodyIndex);
    const ui32 widthVerts = mMarkupGrid->getWidthVertices();
    std::vector<i16v2>& borderSet = mBodyBorderSets[bodyIndex];
    FlatMap<ui32 /*body index*/, ui32 /*count*/> neighborBodyCounts;
    neighborBodyCounts.reserve(6); // This is more than we will need in almost every case
    boost::container::flat_set<ChunkID> borderChunks;
    borderChunks.reserve(16);

#define CHECK_NEIGHBOR(nIndex) \
    const ui32 neighborBodyIndex = mMarkupGrid->getMarkupForGeneration(nIndex).bodyId;  \
    if (neighborBodyIndex != bodyIndex) { \
        auto&& it = neighborBodyCounts.find(neighborBodyIndex);  \
        if (it == neighborBodyCounts.end()) {  \
            neighborBodyCounts.emplace(neighborBodyIndex, 1);  \
        }  \
        else {  \
            ++it->second;  \
        }  \
    }
    f64v2 avgPos = f64v2(0.0);
    // Process all border blocks
    for (i16v2 pos : borderSet) {
        const i32 chunkX = (pos.x * BLOCK_WIDTH) / CHUNK_WIDTH;
        const i32 chunkY = (pos.y * BLOCK_WIDTH) / CHUNK_WIDTH;
        ChunkID chunkId = GridIdUtil::getCellIndexFromWorldPos(ui32v2(pos) * (ui32)BLOCK_WIDTH, CHUNK_WIDTH, mMarkupGrid->mWidthChunks);
        borderChunks.insert(chunkId);

        avgPos += f64v2(pos);
        ui32 index = pos.y * widthVerts + pos.x;
        if (pos.x == 0) [[unlikely]] {
            bodyData.onMapEdge = true;
        }
        else {
            ui32 leftIndex = index - 1;
            CHECK_NEIGHBOR(leftIndex);
        }
        if (pos.x == widthVerts - 1) [[unlikely]] {
            bodyData.onMapEdge = true;
        }
        else {
            ui32 rightIndex = index + 1;
            CHECK_NEIGHBOR(rightIndex);
        }
        if (pos.y == 0) [[unlikely]] {
            bodyData.onMapEdge = true;
        }
        else {
            ui32 bottomIndex = index - widthVerts;
            CHECK_NEIGHBOR(bottomIndex);
        }
        if (pos.y == widthVerts - 1) [[unlikely]] {
            bodyData.onMapEdge = true;
        }
        else {
            ui32 topIndex = index + widthVerts;
            CHECK_NEIGHBOR(topIndex);
        }
    }
    avgPos /= (f64)borderSet.size();
    // Write data
    bodyData.neighborBodies.resize(neighborBodyCounts.size());
    size_t i = 0;
    for (auto&& it : neighborBodyCounts) {
        bodyData.neighborBodies[i].neighborBodyIndex = it.first;
        bodyData.neighborBodies[i].adjacentBlocks = it.second;
        ++i;
    }
    bodyData.averagePos = f32v2(avgPos);
    bodyData.borderBlocks = std::move(borderSet);
    bodyData.borderBlocks.shrink_to_fit();
    bodyData.borderChunks.reserve(borderChunks.size());
    for (auto& c : borderChunks) {
        bodyData.borderChunks.emplace_back(c);
    }

    //static std::atomic<ui32> TOTALSIZE = 0;
    //TOTALSIZE += bodyData.borderBlocks.size() * sizeof(i16v2);
    //LOG_DEBUG("   {} has {} neighbors checked in {} ms with {} checks {} mb total", bodyIndex, neighborBodyCounts.size(), timer.elapsedMs(), borderSet.size(), (f64)TOTALSIZE / 1024.0 / 1024.0);
}

void MarkupGenerationStage::onFinished() {
   
    mMarkupGrid->onGenerationComplete();
    mBodyChunkListMutexes.reset();

    mMarkupGrid->setMarkupReady();
    LOG_DEBUG("Finished markup generation in {} ms with {} bodies", mTotalTimer.elapsedMs(), mTotalBodies);

    // Only save when not a loaded world
    LOG_DEBUG("Saving template");
    GameSaveManager::get().saveWorld(*mWorldPtr, "debug_template", false /*blockUntilFinished*/);
}
