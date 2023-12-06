#pragma once

#include "generation/WorldGenerationData.h"

class IHeightmapGrid;
class HeightmapPatch;
class HostWorldData;
class BiomeGrid;

enum class WorldGenerationState {
    None,
    GeneratingBaseHeightmapAndBiomes,
    PropagatingBiomes,
    Done,
    COUNT
};

struct PendingBaseHeightAndBiomeGeneration {
    ~PendingBaseHeightAndBiomeGeneration();
    GLsync sync = 0;
    ui32 rowIndexStart = 0;
    ui32 numRows = 0;
    bool generateStarted = false;
};


// Generates full world data on the GPU, with some back and forth with CPU
// Stage 1 - Generate base height on GPU as well as base biomes via noise
// Stage 2 - Seed corrupted biomes on CPU (Can be done in parallel with stage 1)
// Stage 3 - Propagate biomes on GPU with checkerboard cellular automata
// Stage 4 - Generate biome height on GPU -> Download height to CPU on finish per patch
// Stage 5 - Carve rivers on CPU
class WorldDataGenerator
{
public:
    WorldDataGenerator();
    ~WorldDataGenerator();

    void beginGeneration(HostWorldData& worldData, const WorldGenerationData& generationData, i32 resolution, std::function<void()> onFinished);

    // Call before generating agian
    void cleanup();

    WorldGenerationState update();
    WorldGenerationState getState() const { return mState; }
    bool getAllGenerationSentThisStep() const { return mAllGenerationSentThisStep; }

    VGBuffer getHeightSSBO() const { return mTerrainSSBO; }
    VGTexture getHeightTexture() const { return mHeightTexture; }
    VGBuffer getBiomeSSBO() const { return mBiomeSSBO; }
    VGTexture getBiomeTexture() const { return mBiomeTexture; }

    // Caller takes ownership of the returned texture
    VGTexture releaseBiomeTexture() {
        assert(mBiomeTexture);
        VGTexture tex = mBiomeTexture;
        mBiomeTexture = 0;
        return tex;
    }
private:
    bool initResourcesIfNeeded(i32 resolution);

    // ============== Generation Stages ==============
    void updateGenerateBaseHeightmapAndBiomes(); // Stage 1
    //void updatePropagateBiomes(); // Stage 2

    // ============== Finish methods ==============
    void finishPendingBaseHeightAndBiomeGeneration(PendingBaseHeightAndBiomeGeneration& generation); // Stage 1

    // Final method
    void onCompletelyFinished();

    WorldGenerationState mState;
    HostWorldData* mWorldData = nullptr;
    IHeightmapGrid* mHeightGrid = nullptr;
    BiomeGrid* mBiomeGrid = nullptr;

    f32v2 mWorldCenter;
    WorldGenerationData mGenerationData;

    ui32 mNextGenerationIndex = 0;
    ui32 mNextRowToGenerate = 0;
    std::vector<PendingBaseHeightAndBiomeGeneration> mGPUBaseHeightAndBiomeGenerations;

    // Runs at very end
    std::function<void()> mOnFinished;

    VGBuffer mTerrainSSBO = 0;
    VGTexture mHeightTexture = 0;
    VGBuffer mBiomeSSBO = 0;
    VGTexture mBiomeTexture = 0;
    GLfloat * mMappedHeights = nullptr;
    ui32* mMappedBiomes = nullptr;
    f32 mWorldSeed = 0.f;
    ui32 mTotalPatches = 0;
    std::atomic<ui32> mFinishedPatchesThisStep = 0;
    bool mAllGenerationSentThisStep = false;
};

