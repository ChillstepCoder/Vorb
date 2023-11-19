#pragma once

#include "generation/WorldGenerationData.h"

class IHeightmapGrid;
class HeightmapPatch;

enum class TerrainGenerationState {
    None,
    GeneratingBaseHeightmap,
    GeneratingBaseHeightmapDone,
    COUNT
};

struct PendingGPUTerrainGeneration {
    ~PendingGPUTerrainGeneration();
    GLsync sync = 0;
    ui32 rowIndex;
    bool generateStarted = false;
};

class TerrainGenerator
{
public:
    TerrainGenerator(const WorldGenerationData& generationData);
    ~TerrainGenerator();
    void init(IHeightmapGrid& heightGrid, f32v2 worldCenter);
    void destroy();

    TerrainGenerationState tick();
    // Pass 1 - Generate base heightmap
    void generateBaseHeightmapCPU(std::function<void(HeightmapPatchID)> onPatchFinished);
    void generateBaseHeightmapGPU(i32 resolution, std::function<void(HeightmapPatchID)> onPatchFinished);

    VGTexture getHeightmapTexture() const { return mHeightmapTexture; }
private:
    void generateHeightDataPatch(HeightmapPatch& patch, const f32v2& position);
    f32 generateHeightAtPos(const f32v2& worldPos);
    void finishPendingGeneration(PendingGPUTerrainGeneration& generation);

    std::atomic<int> mFinishedRows = 0;
    TerrainGenerationState mState;
    IHeightmapGrid* mHeightGrid = nullptr;

    f32v2 mWorldCenter;
    const WorldGenerationData& mGenerationData;

    ui32 mNextGenerationIndex = 0;
    ui32 mNextRowToGenerate = 0;
    std::vector<PendingGPUTerrainGeneration> mGPUTerrainGenerations;
    bool mIsGeneratingGPU = false;
    std::function<void(HeightmapPatchID)> mOnPatchFinished;

    VGTexture mHeightmapTexture = 0;
    VGBuffer mSsbo = 0;
    GLfloat* mMappedHeights = nullptr;
};

