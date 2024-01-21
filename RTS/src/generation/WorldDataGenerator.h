#pragma once

#include "generation/WorldGenerationData.h"

class IHeightmapGrid;
class HeightmapPatch;
class HostWorldData;
class BiomeGrid;
class IWorldGenerationStage;
class WorldGenerationBlackboard;
class World;
// Generates full world data on the GPU, with some back and forth with CPU
// Stage 1 - Generate base height on GPU as well as base biomes via noise
// Stage 2 - Seed corrupted biomes on CPU (Can be done in parallel with stage 1)
// Stage 3 - Propagate biomes on GPU with checkerboard cellular automata
// Stage 4 - Carve rivers on CPU
class WorldDataGenerator
{
public:
    WorldDataGenerator();
    ~WorldDataGenerator();

    void beginGeneration(HostWorldData& worldData, const WorldGenerationData& generationData, i32 resolution, std::function<void()> onFinished);

    const char* getCurrentStageName() const;
    f32 getCurrentStageProgress() const;
    bool isDone() const { return mFinished; }

    void currentStageDebugDraw();
    void renderCurrentStageImguiControls();

    // Call before generating again
    void cleanup();

    bool update();

    // ========== Accessors ==========
    VGBuffer getHeightSSBO() const { return mHeightSSBO; }
    VGTexture getHeightTexture() const { return mHeightTexture; }
    VGBuffer getBiomeSSBO() const { return mBiomeSSBO; }
    VGTexture getBiomeTexture() const { return mBiomeTexture; }
    HostWorldData* getWorldData() const { return mWorldData; }
    WorldGenerationData& getGenerationData() { return mGenerationData; }
    GLfloat* getMappedHeights() { return mMappedHeights; }
    ui32* getMappedBiomes() { return mMappedBiomes; }
    WorldGenerationBlackboard& getBlackboard() { return *mBlackboard; }

    // Caller takes ownership of the returned texture
    VGTexture releaseBiomeTexture() {
        assert(mBiomeTexture);
        VGTexture tex = mBiomeTexture;
        mBiomeTexture = 0;
        return tex;
    }
    std::unique_ptr<World> releaseWorld();

private:
    void initStages();
    IWorldGenerationStage* tryGetCurrentStage() const;
    void initResourcesIfNeeded(i32 resolution);

    // Final method
    void onCompletelyFinished();

    std::vector<std::unique_ptr<IWorldGenerationStage>> mStages;
    ui32 mCurrentStageIndex = 0;

    std::unique_ptr<WorldGenerationBlackboard> mBlackboard;
    HostWorldData* mWorldData = nullptr;
    WorldGenerationData mGenerationData;

    // Runs at very end
    std::function<void()> mOnFinished;

    VGBuffer mHeightSSBO = 0;
    VGTexture mHeightTexture = 0;
    VGBuffer mBiomeSSBO = 0;
    VGTexture mBiomeTexture = 0;
    GLfloat* mMappedHeights = nullptr;
    ui32* mMappedBiomes = nullptr;
    bool mFinished = false;

    std::vector<ui32v2> mPeakPositions;
    moodycamel::ConcurrentQueue<ui32v2> mPeakPositionsQueue;

    std::unique_ptr<World> mWorld;

    f32 mWorldSeed = 0.f;
};
