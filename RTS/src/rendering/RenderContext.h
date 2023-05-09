#pragma once

#include "GlobalRenderData.h"

class AmbientOcclusionPostProcess;
class Camera3D;
class CameraController;
class CharacterRenderer;
class CityDebugRenderer;
class CloudMeshManager;
class CloudRenderer;
class DepthOfFieldPostProcess;
class EntityComponentSystemRenderer;
class GrassMesh;
class GrassRenderer;
class ICamera;
class InstancedStaticModelGatherer;
class IWorld;
class ItemRenderer;
class LightRenderer;
class MaterialShader;
class Mesh;
class ParticleSystemRenderer;
class RenderState;
class ShadowRenderer;
class Skybox;
class SmudgeRenderer;
class TerrainMesh;
class TerrainRenderer;
class TileContainerRenderer;
class TonemapRenderer;
class WorldRenderer;
class WorldRenderDataManager;

struct SDL_Window;

DECL_VG(class SpriteBatch);
DECL_VG(class SpriteFont);
DECL_VG(class GBuffer);

constexpr ui32 INVALID_MESH_INDEX = UINT32_MAX;

// Singleton
class RenderContext {
    friend class RenderThreadTasks;
protected:
    RenderContext(const f32v2& screenResolution, SDL_Window* window);
    ~RenderContext();

public:
    RenderContext(RenderContext& other) = delete;
    void operator=(const RenderContext&) = delete;

    static RenderContext& initInstance(const f32v2& screenResolution, SDL_Window* window);
    static RenderContext& getInstance();
    static RenderContext* tryGetInstance() { return sInstance; }
    static bool exists() { return sInstance != nullptr; }

    void initPostLoad();

    void beginFrame(const RenderState* renderState, const Camera3D* camera, f32v3 playerPos); // Called automatically by beginFrame
    void renderFrame(CameraController& cameraController, f32 frameAlpha, f32 elapsedSec);
    void endFrame();

    void tickGameThread(IWorld& world);

    void selectNextDebugShader();

    const GlobalRenderData& getRenderData() const { return mRenderData; }
    vg::GBuffer& getActiveGBuffer() const { return *mActiveGBuffer; }
    const vg::GBuffer& getPrevFinalGBuffer() const { return *mGBuffers[mPrevGBufferIndex]; }
    const ui32v2& getCurrentFramebufferDims() const { return mCurrentFramebufferDims; }
    VGTexture getShadowTexture() const;
    vg::SpriteFont& getSpriteFont() const { return *mSpriteFont; }
    vg::SpriteBatch& getSpriteBatch() const { return *mSb; }
    const ui32v2& getScreenResolution() const { return mScreenResolution;}
    const Camera3D* getCamera() const { return mCamera; }
    CameraController* getCameraController() const { return mCameraController; }

    // Renderers
    TileContainerRenderer& getTileContainerRenderer() const;
    CharacterRenderer& getCharacterRenderer() const;
    WorldRenderer& getWorldRenderer() const { return *mWorldRenderer; }
    WorldRenderDataManager& getRenderDataManagerForWorld(IWorld& world) const;
    WorldRenderDataManager* tryGetRenderDataManagerForWorld(IWorld& world) const;

    f32 getCurrentFrameAlpha() const { return mCurrentFrameAlpha; }
    f32 getCurrentFrameElapsedSec() const { return mCurrentFrameElapsedSec; }

    // Callable from world renderer
    void renderPassWorldDebug(const Camera3D& camera) const;
private:
    void updateCamera(f32 frameAlpha);
    void updateRenderThreadProcs();

    // Render passes
    void renderPassUI(const Camera3D& camera, const RenderState& renderState);

    static RenderContext* sInstance;
    
    // Data
    GlobalRenderData mRenderData;
    const RenderState* mCurrentRenderState = nullptr;
    ui32v2 mScreenResolution;
    ui32v2 mCurrentFramebufferDims;
    CameraController* mCameraController = nullptr;
    const Camera3D* mCamera = nullptr;
    f32 mCurrentFrameAlpha;
    f32 mCurrentFrameElapsedSec;

    // World
    IWorld* mActiveWorld = nullptr;
    std::unique_ptr<WorldRenderer> mWorldRenderer;

    // UI
    std::unique_ptr<vg::SpriteBatch> mSb;
    std::unique_ptr<vg::SpriteFont> mSpriteFont;
    SDL_Window* mWindow;

    int mPrevGBufferIndex = 1;
    int mActiveGBufferIndex = 0;
    vg::GBuffer* mActiveGBuffer = nullptr;
    std::unique_ptr<vg::GBuffer> mGBuffers[2];
    VGBuffer mGlobalUbo = 0;


};

