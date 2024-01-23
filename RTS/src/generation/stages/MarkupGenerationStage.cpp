#include "stdafx.h"
#include "MarkupGenerationStage.h"

#include "world/host/HostWorldData.h"
#include "generation/WorldGenerationData.h"

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
                beginChunkMarkupGen();
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
    mWorldPtr = std::make_unique<World>(WorldNetMode::Host, mWorldData);
}

void MarkupGenerationStage::beginInitialMarkupGen() {
    mFinishedThreads = 0;
    mRunningThreads = 1;
    const ui32 genID = sGenerationUID;
    Services::Threadpool::ref().addTask([this, genID]() {
        // Check for interrupt
        if (genID != sGenerationUID) {
            return;
        }
        generateInitialMarkup();
        ++mFinishedThreads;
    }, nullptr);
}

void MarkupGenerationStage::beginChunkMarkupGen() { 
    // TODO: THIS
    assert(false);
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
    std::vector<i16v2> currentSet;
    // Blowing this reserve should be rare
    stack.reserve(mMarkupGrid->getTotalVertices());
    currentSet.reserve(mMarkupGrid->getTotalVertices());

    int nodeChecks = 0;
    // Find all bodies (Water vs Land)
    i16v2 start(0, 0);
    do {
        i32 startIndex = start.y * markupWidth + start.x;
        startIndex = visited.getIndexOfFirstUnsetBit(startIndex);
        if (startIndex == UINT32_MAX) [[unlikely]] break;
        start.x = startIndex % markupWidth;
        start.y = startIndex / markupWidth;
        const bool groupIsWater = mBiomeGrid->getVertexForGeneration(start.y * markupWidth + start.x).isWater();
        stack.push_back(start);
        currentSet.resize(0);
        do {
            i16v2 pos = stack.back();
            stack.pop_back();
            const i32 startYOffset = pos.y * markupWidth;
            while (pos.x >= 0 && mBiomeGrid->getVertexForGeneration(startYOffset + pos.x).isWater() == groupIsWater) --pos.x;
            ++pos.x;

            i32 index = pos.y * markupWidth + pos.x;
            bool spanBelow = false;
            bool spanAbove = false;
            // Don't double process
            if (!visited.getBit(index)) {
                do {
                    currentSet.push_back(pos);
                    visited.setBit(index);
                    mMarkupGrid->getMarkupForGeneration(index).bodyIndex = mTotalBodies;
                    ++nodeChecks;

                    if (pos.y > 0) [[likely]] {
                        if (!spanBelow && mBiomeGrid->getVertexForGeneration(index - markupWidth).isWater() == groupIsWater) {
                            stack.emplace_back(pos.x, pos.y - 1);
                            spanBelow = true;
                        }
                        else if (spanBelow && mBiomeGrid->getVertexForGeneration(index - markupWidth).isWater() != groupIsWater) {
                            spanBelow = false;
                        }
                    }
                    if (pos.y < markupWidth - 1) [[likely]] {
                        if (!spanAbove && mBiomeGrid->getVertexForGeneration(index + markupWidth).isWater() == groupIsWater) {
                            stack.emplace_back(pos.x, pos.y + 1);
                            spanAbove = true;
                        }
                        else if (spanAbove && mBiomeGrid->getVertexForGeneration(index + markupWidth).isWater() != groupIsWater) {
                            spanAbove = false;
                        }
                    }

                    ++pos.x;
                    ++index;
                } while (pos.x < markupWidth && mBiomeGrid->getVertexForGeneration(index).isWater() == groupIsWater);
            }
        } while (stack.size());
        if (currentSet.size()) {
            ++mTotalBodies;
            // Set up the body
            WorldBodyMarkupData bodyData;
            bodyData.sizeCells = currentSet.size();
            if (groupIsWater) {
                constexpr ui32 SIZE_THRESHOLD = 65536 * 3;
                bodyData.bodyType = bodyData.sizeCells > SIZE_THRESHOLD ? WorldMarkupBodyType::Ocean : WorldMarkupBodyType::Lake;
            }
            else {
                constexpr ui32 SIZE_THRESHOLD = 65536;
                bodyData.bodyType = bodyData.sizeCells > SIZE_THRESHOLD ? WorldMarkupBodyType::LargeIsland : WorldMarkupBodyType::Island;
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

    LOG_DEBUG("Markup took {} ms and found {} islands in {} checks", timer.elapsedMs(), mTotalBodies, nodeChecks);
    LOG_DEBUG("End markup generation");
}

void MarkupGenerationStage::onFinished() {
    mMarkupGrid->setMarkupReady();
    LOG_DEBUG("Finished markup generation in {} ms with {} bodies", mTotalTimer.elapsedMs(), mTotalBodies);
}
