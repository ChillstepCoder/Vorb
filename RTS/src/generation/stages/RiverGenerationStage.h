#pragma once
#include "IWorldGenerationStage.h"

struct RiverGenerationBufferData {
    ui32v2 cellPos;
    ui32 segmentStartIndex;
    ui32 numSegments;
};
static_assert(sizeof(RiverGenerationBufferData) == sizeof(ui32v4));

// Many cells are packed into a single buffer for one big compute dispatch
struct RiverGenerationPass {
    ~RiverGenerationPass();
    GLsync sync = 0;
    VGBuffer segmentsBuffer = 0;
    VGBuffer groupDataBuffer = 0;
    ui32 passIndex = 0;
    bool generateStarted = false;
    std::vector<f32v4> allSegments;
    std::vector<RiverGenerationBufferData> bufferData;
};

class RiverGenerationStage : public IWorldGenerationStage
{
public:
    using IWorldGenerationStage::IWorldGenerationStage;

    void begin() override;

    const char* getStageName() const override { return "Rivers"; }

    bool update() override;

protected:
    void generateRiverPath(size_t riverIndex);
    void onPassFinished(RiverGenerationPass& pass);

    ui32 mNextPassIndex = 0;
    // One per pass
    std::vector<RiverGenerationPass> mGPUGenerations;
    std::unordered_map<ui32v2 /*cellPos*/, ui32 /*passCount*/> mCellPassCounts;

    std::atomic<ui32> mNumFinishedRiverPaths = 0;
    bool mFinishedGeneratingPaths = false;
    bool mAllGpuGenerationsFinished = false;

    int mPendingHeightDownloads = 0;
    std::atomic<ui32> mFinishedHeightDownloads = 0;
};

