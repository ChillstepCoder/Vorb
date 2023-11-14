#pragma once

class IHeightmapGrid;
class HeightmapPatch;

enum class TerrainGenerationState {
    None,
    GeneratingBaseHeightmap,
    GeneratingBaseHeightmapDone,
    COUNT
};

class TerrainGenerator
{
public:
    void init(IHeightmapGrid& heightGrid);
    void destroy();

    TerrainGenerationState tick();
    // Pass 1
    void generateBaseHeightmap();

private:
    void generateHeightDataPatch(HeightmapPatch& patch, const f32v2& position);

    std::atomic<int> mFinishedRows = 0;
    TerrainGenerationState mState;
    IHeightmapGrid* mHeightGrid = nullptr;
};

