#pragma once

#include <Vorb/ui/IGameScreen.h>
#include <Vorb/blockingconcurrentqueue.h>

#include <gli/gli.hpp>

class App;
class HostWorldData;
class TerrainGenerator;

enum class WorldGenScreenState {
    Idle,
    GeneratingTerrain,
    Done,
    COUNT
};

class WorldGenScreen : public vui::IAppScreen<App>
{
public:
    WorldGenScreen(App* const app);
    ~WorldGenScreen();

    virtual i32 getNextScreen() const override;
    virtual i32 getPreviousScreen() const override;

    virtual void build() override;
    virtual void destroy(const vui::GameTime& gameTime) override;

    virtual void onEntry(const vui::GameTime& gameTime) override;
    virtual void onExit(const vui::GameTime& gameTime) override;

    virtual void update(const vui::GameTime& gameTime) override;

    virtual void draw(const vui::GameTime& gameTime) override;

protected:
    void initWorldData();
    void updateDockspace();
    void updateTerrainGen();
    void onPatchFinishedCPU(HeightmapPatchID patchId);
    void onPatchFinishedGPU(std::pair<HeightmapPatchID, ui8v4*> data);

    void initScreenTexture();

    bool mRebuildDockspace = true;
    bool mCancelled = false;
    bool mFirstEntry = true;

    std::unique_ptr<HostWorldData> mWorldData;
    std::unique_ptr<TerrainGenerator> mTerrainGenerator;

    WorldGenScreenState mGenState = WorldGenScreenState::Idle;
    moodycamel::ConcurrentQueue<HeightmapPatchID> mFinishedTerrainPatches;
    moodycamel::ConcurrentQueue<std::pair<HeightmapPatchID, ui8v4*>> mFinishedTerrainGPUPatches;

    VGTexture mScreenTexture = 0;
    gli::texture2d mScreenTextureData;
    ui32 mPatchPixelDims = 0;
    ui32 mFinishedPatchCount = 0;
    ui32 mTotalPatches = 0;
    ui32 mThreadpoolSizePostEntry = 0;

    PreciseTimer mGenTimer;
};

