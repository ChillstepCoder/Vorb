#pragma once

#include "generation/WorldGenerationData.h"

class IHeightmapGrid;
class HeightmapPatch;
class HostWorldData;

enum class WorldGenerationState {
    None,
    GeneratingBaseHeightmap,
    GeneratingBaseBiomes,
    Done,
    COUNT
};

struct PendingGPUTerrainGeneration {
    ~PendingGPUTerrainGeneration();
    GLsync sync = 0;
    ui32 rowIndex;
    bool generateStarted = false;
};

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

    VGTexture getHeightmapTexture() const { return mHeightmapTexture; }
private:
    void updateGenerateBaseHeightmap();

    bool initResourcesIfNeeded(i32 resolution);
    void finishPendingGeneration(PendingGPUTerrainGeneration& generation);

    WorldGenerationState mState;
    HostWorldData* mWorldData = nullptr;
    IHeightmapGrid* mHeightGrid = nullptr;

    f32v2 mWorldCenter;
    WorldGenerationData mGenerationData;

    ui32 mNextGenerationIndex = 0;
    ui32 mNextRowToGenerate = 0;
    std::vector<PendingGPUTerrainGeneration> mGPUTerrainGenerations;
    std::function<void(HeightmapPatchID)> mOnPatchFinished;

    VGTexture mHeightmapTexture = 0;
    VGBuffer mSsbo = 0;
    GLfloat* mMappedHeights = nullptr;
    f32 mWorldSeed = 0.f;
};

