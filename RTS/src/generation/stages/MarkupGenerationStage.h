#pragma once
#include "IWorldGenerationStage.h"
#include "world/biome/BiomeCorruptions.h"
#include "world/World.h"


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
    void generateWorldMarkupBlock();

    std::atomic_bool mThreadFinished = false;
    int mTotalBodies = 0;
    std::unique_ptr<World>& mWorldPtr;
    PreciseTimer mTotalTimer;
};