#pragma once
#include "IWorldGenerationStage.h"
#include "world/biome/BiomeCorruptions.h"
#include "world/World.h"

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
    void beginChunkMarkupGen();
    void generateInitialMarkup();
    void onFinished();

    MarkupGenerationStageState mState = MarkupGenerationStageState::InitialMarkup;
    std::atomic_int mFinishedThreads = 0;
    int mRunningThreads = 0;
    int mTotalBodies = 0;
    std::unique_ptr<World>& mWorldPtr;
    PreciseTimer mTotalTimer;
};