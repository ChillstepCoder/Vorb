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
    GLsync sync;
    HeightmapPatchID patchID;
};

class TerrainGenerator
{
public:
    void init(IHeightmapGrid& heightGrid, f32v2 worldCenter);
    void destroy();

    TerrainGenerationState tick();
    // Pass 1 - Generate base heightmap
    void generateBaseHeightmapCPU(std::function<void(HeightmapPatchID)> onPatchFinished);
    void generateBaseHeightmapGPU(std::function<void(HeightmapPatchID)> onPatchFinished);

private:
    void generateHeightDataPatch(HeightmapPatch& patch, const f32v2& position);
    f32 generateHeightAtPos(const f32v2& worldPos);

    std::atomic<int> mFinishedRows = 0;
    TerrainGenerationState mState;
    IHeightmapGrid* mHeightGrid = nullptr;

    f32v2 mWorldCenter;
    WorldGenerationData mGenerationData;


};

