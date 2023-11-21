#pragma once

#include "generation/WorldGenerationData.h"

class IHeightmapGrid;
class HeightmapPatch;
class HostWorldData;

enum class WorldGenerationState {
    None,
    GeneratingBaseHeightmap,
    PropagatingBiomes,
    Done,
    COUNT
};

struct PendingHeightGeneration {
    ~PendingHeightGeneration();
    GLsync sync = 0;
    ui32 rowIndexStart = 0;
    ui32 numRows = 0;
    bool generateStarted = false;
};


// Generates full world data on the GPU, with some back and forth with CPU
// Stage 1 - Generate base height on GPU
// Stage 2 - Seed base biomes on CPU (Can be done in parallel with stage 1)
// Stage 3 - Propagate biomes on GPU with checkerboard cellular automata
// Stage 4 - Generate biome height on GPU -> Download height to CPU on finish per patch
// Stage 5 - Carve rivers on CPU
class WorldDataGPUGenerator
{
public:
    WorldDataGPUGenerator();
    ~WorldDataGPUGenerator();

    void beginGeneration(HostWorldData& worldData, const WorldGenerationData& generationData, i32 resolution, std::function<void(HeightmapPatchID)> onPatchFinished);

    // Call before generating agian
    void cleanup();

    WorldGenerationState update();
    WorldGenerationState getState() const { return mState; }
    bool getAllGenerationSentThisStep() const { return mAllGenerationSentThisStep; }

    VGBuffer getHeightSSBO() const { return mSsbo; }
private:
    bool initResourcesIfNeeded(i32 resolution);

    // ============== Generation Stages ==============
    void updateGenerateBaseHeightmap();

    // ============== Finish methods ==============
    void finishPendingHeightGeneration(PendingHeightGeneration& generation);

    WorldGenerationState mState;
    HostWorldData* mWorldData = nullptr;
    IHeightmapGrid* mHeightGrid = nullptr;

    f32v2 mWorldCenter;
    WorldGenerationData mGenerationData;

    ui32 mNextGenerationIndex = 0;
    ui32 mNextRowToGenerate = 0;
    std::vector<PendingHeightGeneration> mGPUTerrainGenerations;
    std::function<void(HeightmapPatchID)> mOnPatchFinished;

    VGBuffer mSsbo = 0;
    GLfloat* mMappedHeights = nullptr;
    f32 mWorldSeed = 0.f;
    bool mAllGenerationSentThisStep = false;
};

