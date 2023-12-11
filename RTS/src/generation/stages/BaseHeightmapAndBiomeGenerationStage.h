#pragma once
#include "IWorldGenerationStage.h"

struct PendingBaseHeightAndBiomeGeneration {
    ~PendingBaseHeightAndBiomeGeneration();
    GLsync sync = 0;
    ui32 rowIndexStart = 0;
    ui32 numRows = 0;
    bool generateStarted = false;
};

class BaseHeightmapAndBiomeGenerationStage : public IWorldGenerationStage {
public:
    using IWorldGenerationStage::IWorldGenerationStage;

    void begin() override;

    const char* getStageName() const override { return "Base Height and Biomes"; }

    bool update() override;

private:

    void finishGeneration(PendingBaseHeightAndBiomeGeneration& generation);

    ui32 mNextGenerationIndex = 0;
    ui32 mNextRowToGenerate = 0;
    std::vector<PendingBaseHeightAndBiomeGeneration> mGPUGenerations;

    std::atomic<ui32> mFinishedPatchesThisStep = 0;
    bool mAllGenerationSentThisStep = false;
};

