#pragma once

#include <Vorb/ui/IGameScreen.h>
#include <Vorb/blockingconcurrentqueue.h>

#include <gli/gli.hpp>

#include "generation/WorldGenerationData.h"

DECL_VG(class GBuffer);

class App;
class HostWorldData;
class WorldDataGPUGenerator;
class MaterialShaderDef;
class OrthoCamera;

enum class WorldGenScreenState {
    Idle,
    GeneratingBaseHeight,
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
    void updateBaseHeightGeneration();

    void beginWorldGeneration();

    void updateCamera();
    void renderMapView();
    void updateMouseInput();

    bool mRebuildDockspace = true;
    bool mCancelled = false;
    bool mFirstEntry = true;

    std::unique_ptr<HostWorldData> mWorldData;
    std::unique_ptr<WorldDataGPUGenerator> mWorldGenerator;

    WorldGenerationData mGenData;
    WorldGenScreenState mGenState = WorldGenScreenState::Idle;
    //moodycamel::ConcurrentQueue<HeightmapPatchID> mFinishedTerrainGPUPatches;

    // Rendering
    std::unique_ptr<vg::GBuffer> mMapScreenGBuffer;
    AssetHandlePtr<MaterialShaderDef> mScreenShader;
    std::unique_ptr<OrthoCamera> mCamera;
    f32v2 mCurrentTextureSize = f32v2(0.0f);
    PreciseTimer mFrameTimer;
    float mFrameTimeThisFrame = 0.0f;

    ui32 mPatchPixelDims = 0;
    std::atomic<ui32> mFinishedPatchCount = 0;
    ui32 mTotalPatches = 0;
    ui32 mThreadpoolSizePostEntry = 0;
    bool mIsDirty = false;

    // Controls
    bool mShowBiomes = false;
    bool mShowHeight = false;

    PreciseTimer mGenTimer;
};

