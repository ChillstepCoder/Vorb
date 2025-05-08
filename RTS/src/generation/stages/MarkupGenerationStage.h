#pragma once
#include "IWorldGenerationStage.h"
#include "world/biome/LivingBiomeType.h"
#include "world/World.h"
#include <mutex>

enum class MarkupGenerationStageState {
    InitialMarkup,
    ChunkMarkup,
    COUNT
};

// Allocates the world and generates markup
class MarkupGenerationStage : public IWorldGenerationStage
{
public:
    MarkupGenerationStage(WorldDataGenerator& generator, std::unique_ptr<World>& worldPtr);
    ~MarkupGenerationStage();

    void begin() override;

    const char* getStageName() const override { return "Markup"; }

    bool update() override;

private:
    void allocateWorld();
    void beginInitialMarkupGen();
    void beginChunkAndBodyMarkupGen();
    void generateInitialMarkup();
    void generateChunkMarkup(ui32 jobIndex, ui32 chunkRowsPerJob);
    void generateBodyMarkup(ui32 bodyIndex);
    void onFinished();

    std::vector<std::vector<i16v2>> mBodyBorderSets;
    std::unique_ptr<std::mutex[]> mBodyChunkListMutexes; // Only needed during generation so we set them up here

    MarkupGenerationStageState mState = MarkupGenerationStageState::InitialMarkup;
    std::atomic_int mFinishedThreads = 0;
    int mRunningThreads = 0;
    int mTotalBodies = 0;
    std::unique_ptr<World>& mWorldPtr;
    PreciseTimer mTotalTimer;
};