#pragma once

#include "GlobalRenderData.h"
#include "camera/Camera3D.h"
#include "camera/Camera3DGameThreadData.h"
#include "world/WorldEvents.h"

class AmbientOcclusionPostProcess;
class CameraController;
class CharacterRenderer;
class CityDebugRenderer;
class CloudMeshManager;
class CloudRenderer;
class DepthOfFieldPostProcess;
class ECSRenderer;
class GrassMesh;
class GrassRenderer;
class ICamera;
class InstancedStaticModelGatherer;
class World;
class ItemRenderer;
class LightRenderer;
class MaterialShaderDef;
class Mesh;
class ModelBillboardLodBuilder;
class ParticleSystemRenderer;
class WorldRenderState;
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

    void beginFrame(const WorldRenderState* renderState, f32v3 playerPos, f32 frameAlpha); // Called automatically by beginFrame
    void renderFrame(CameraController& cameraController, f32 frameAlpha, f32 elapsedSec);
    void endFrame();

    void selectNextDebugShader();

    VGBuffer getCameraUbo() const { return mCameraUbo; }
    const GlobalRenderData& getRenderData() const { return mRenderData; }
    vg::GBuffer& getActiveGBuffer() const { return *mActiveGBuffer; }
    const vg::GBuffer& getPrevFinalGBuffer() const { return *mGBuffers[mPrevGBufferIndex]; }
    const ui32v2& getCurrentFramebufferDims() const { return mCurrentFramebufferDims; }
    VGTexture getShadowTexture() const;
    vg::SpriteFont& getSpriteFont() const { return *mSpriteFont; }
    vg::SpriteBatch& getSpriteBatch() const { return *mSb; }
    const ui32v2& getScreenResolution() const { return mScreenResolution; }
    const Camera3D* getCamera() const { ASSERT_RENDER_THREAD(); return &mCamera; }
    Camera3DGameThreadData getGameThreadCameraData() const;
    CameraController* getCameraController() const { return mCameraController; }

    ModelBillboardLodBuilder& getModelBillboardLodBuilder() const { return *mModelBillboardLodBuilder; }
    void removeLooseModelInstance(World& world, ModelID modelId, StaticModelInstanceID instanceId);

    // Renderers
    TileContainerRenderer& getTileContainerRenderer() const;
    CharacterRenderer& getCharacterRenderer() const;
    WorldRenderer& getWorldRenderer() const { return *mWorldRenderer; }
    WorldRenderDataManager& getRenderDataManagerForWorld(World& world) const;
    vg::SpriteFont& getDebugFont() { return *mSpriteFont; }

    f32 getCurrentFrameAlpha() const { return mCurrentFrameAlpha; }
    f32 getCurrentFrameElapsedSec() const { return mCurrentFrameElapsedSec; }

    // Callable from world renderer
    void renderPassWorldDebug(const Camera3D& camera) const;
    
    void updateRenderThreadProcs();
private:
    void updateCamera(f32 frameAlpha);
    void initEvents();
    void initImguiStyle();

    // Render passes
    void renderPassUI(const Camera3D& camera, const WorldRenderState& renderState);

    static RenderContext* sInstance;
    
    // Data
    GlobalRenderData mRenderData;
    const WorldRenderState* mCurrentRenderState = nullptr;
    ui32v2 mScreenResolution;
    ui32v2 mCurrentFramebufferDims;
    CameraController* mCameraController = nullptr;

    mutable std::mutex mCameraLock;
    Camera3D mCamera;
    f32 mCurrentFrameAlpha;
    f32 mCurrentFrameElapsedSec;

    // World
    World* mActiveWorld = nullptr;
    //WorldListeners mWorldEventListeners;
    std::unique_ptr<WorldRenderer> mWorldRenderer;
    std::unique_ptr<ModelBillboardLodBuilder> mModelBillboardLodBuilder;

    // UI
    std::unique_ptr<vg::SpriteBatch> mSb;
    std::unique_ptr<vg::SpriteFont> mSpriteFont;
    SDL_Window* mWindow;

    int mPrevGBufferIndex = 1;
    int mActiveGBufferIndex = 0;
    vg::GBuffer* mActiveGBuffer = nullptr;
    std::unique_ptr<vg::GBuffer> mGBuffers[2];
    VGBuffer mGlobalUbo = 0;
    VGBuffer mCameraUbo = 0;

};

