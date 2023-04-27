#pragma once

class AmbientOcclusionPostProcess;
class Camera3D;
class CameraController;
class CharacterRenderer;
class CityDebugRenderer;
class CloudManager;
class CloudRenderer;
class DepthOfFieldPostProcess;
class EntityComponentSystemRenderer;
class GrassMesh;
class GrassRenderer;
class ICamera;
class InstancedStaticModelGatherer;
class InstancedStaticModelRenderer;
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

struct SDL_Window;

#include "rendering/GlobalUboData.h"

DECL_VG(class SpriteBatch);
DECL_VG(class SpriteFont);
DECL_VG(class GBuffer);

struct GlobalRenderData {
    GlobalUboData globalUboData;
    f32 cameraZAngle;
    f32m4 skyRotMatrix;
    const f32m4* shadowFrustumMatrices; // TODO: Delete this stuff???
    const f32* shadowCascadePlaneDistances; // TODO: Delete this stuff???
    VGTexture shadowMap; // TODO: Delete this stuff???
    ui32 shadowFrustumMatricesCount; // TODO: Delete this stuff???
    const ICamera* mainCamera = nullptr; // TODO: Delete this stuff???
};

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

    void registerWorld(IWorld* world);
    void setActiveWorld(IWorld* world);
    void onWorldBegin(const f32v2& worldCenter);

    void initPostLoad();

    void beginFrame(const Camera3D* camera, f32v3 playerPos); // Called automatically by beginFrame
    void renderFrame(CameraController& camera, f32 frameAlpha, f32 elapsedSec);
    void endFrame();

    void selectNextDebugShader();

    const GlobalRenderData& getRenderData() const { return mRenderData; }
    TileContainerRenderer& getChunkRenderer() const { return *mTileContainerRenderer; }
    const vg::GBuffer& getActiveGBuffer() const { return *mActiveGBuffer; }
    const vg::GBuffer& getPrevFinalGBuffer() const { return *mGBuffers[mPrevGBufferIndex]; }
    const ui32v2& getCurrentFramebufferDims() const { return mCurrentFramebufferDims; }
    VGTexture getShadowTexture() const;
    VGTexture getSSAOTexture() const;
    vg::SpriteFont& getSpriteFont() const { return *mSpriteFont; }
    vg::SpriteBatch& getSpriteBatch() const { return *mSb; }
    const ui32v2& getScreenResolution() const { return mScreenResolution;}
    const Camera3D* getCamera() const { return mCamera; }
    InstancedStaticModelRenderer& getInstancedStaticModelRenderer() { return *mStaticModelRenderer; }
    TileContainerRenderer& getTileContainerRenderer() { return *mTileContainerRenderer; }

    // Meshing
    void addTerrainMesh(const TerrainMesh* mesh) { assert(IS_RENDER_THREAD()); mTerrainMeshes.insert(mesh); }
    void removeTerrainMesh(const TerrainMesh* mesh) { assert(IS_RENDER_THREAD()); mTerrainMeshes.erase(mesh); }
    void addTerrainWaterMesh(const TerrainMesh* mesh) { assert(IS_RENDER_THREAD()); mTerrainWaterMeshes.insert(mesh); }
    void removeTerrainWaterMesh(const TerrainMesh* mesh) { assert(IS_RENDER_THREAD()); mTerrainWaterMeshes.erase(mesh); }
    void addGrassMesh(const GrassMesh* mesh) { assert(IS_RENDER_THREAD()); mGrassMeshes.insert(mesh); }
    void removeGrassMesh(const GrassMesh* mesh) { assert(IS_RENDER_THREAD()); mGrassMeshes.erase(mesh); }
    
    // Static models
    void addStaticModelInstancesFromGatherer(InstancedStaticModelGatherer& gatherer);

    // Character models
    CharacterRenderer& getCharacterRenderer() { return *mCharacterRenderer; }
private:
    void initEventHandlers();
    void updateRenderThreadProcs();

    // Render passes
    void renderPassSky(const Camera3D& camera);
    void renderPassShadows(const Camera3D& camera, const RenderState& renderState);
    void renderPassTransparent(const Camera3D& camera, const RenderState& renderState);
    void renderPassDebug(const Camera3D& camera, const RenderState& renderState);
    void renderPassUI(const Camera3D& camera, const RenderState& renderState);

    // Mesh management
    void buildHorizonMesh();

    static RenderContext* sInstance;
    
    // Data
    GlobalRenderData mRenderData;
    ui32v2 mScreenResolution;
    ui32v2 mCurrentFramebufferDims;
    const Camera3D* mCamera = nullptr;

    // Renderers
    mutable std::unique_ptr<TileContainerRenderer> mTileContainerRenderer;
    mutable std::unique_ptr<LightRenderer> mLightRenderer;
    mutable std::unique_ptr<EntityComponentSystemRenderer> mEcsRenderer;
    mutable std::unique_ptr<ParticleSystemRenderer> mParticleSystemRenderer;
    mutable std::unique_ptr<CityDebugRenderer> mCityDebugRenderer;
    mutable std::unique_ptr<ItemRenderer> mItemRenderer;
    mutable std::unique_ptr<CharacterRenderer> mCharacterRenderer;
    mutable std::unique_ptr<CloudRenderer> mCloudRenderer;
    mutable std::unique_ptr<DepthOfFieldPostProcess> mDepthOfField;
    mutable std::unique_ptr<AmbientOcclusionPostProcess> mAmbientOcclusion;
    mutable std::unique_ptr<ShadowRenderer> mShadowRenderer;
    mutable std::unique_ptr<TerrainRenderer> mTerrainRenderer;
    mutable std::unique_ptr<GrassRenderer> mGrassRenderer;
    mutable std::unique_ptr<InstancedStaticModelRenderer> mStaticModelRenderer;
    mutable std::unique_ptr<SmudgeRenderer> mSmudgeRenderer;
    mutable std::unique_ptr<TonemapRenderer> mTonemapRenderer;

    // Worlds
    IWorld* mActiveWorld = nullptr;
    std::map<IWorld*, std::unique_ptr<WorldRenderer>> mWorldRenderers;

    // Clouds
    std::unique_ptr<CloudManager> mCloudManager;

    // TODO: Profile vector instead (linear removal vs logn but better iteration performance)
    std::set<const GrassMesh*> mGrassMeshes;
    std::set<const TerrainMesh*> mTerrainMeshes;
    std::set<const TerrainMesh*> mTerrainWaterMeshes;

    // UI
    std::unique_ptr<vg::SpriteBatch> mSb;
    std::unique_ptr<vg::SpriteFont> mSpriteFont;
    SDL_Window* mWindow;

    int mPrevGBufferIndex = 1;
    int mActiveGBufferIndex = 0;
    vg::GBuffer* mActiveGBuffer = nullptr;
    std::unique_ptr<vg::GBuffer> mGBuffers[2];
    std::unique_ptr<vg::GBuffer> mHDRLightGBuffer;
    std::unique_ptr<Mesh> mHorizonQuad;
    std::unique_ptr<Skybox> mSkyBox;
    VGBuffer mGlobalUbo = 0;

    int mPassthroughRenderMode = 0;
    std::vector<const MaterialShader*> mPassthroughMaterials;
    const MaterialShader* mPassthroughMaterial = nullptr;
    const MaterialShader* mSceneLightingMaterial = nullptr;
    const MaterialShader* mCopyDepthMaterial = nullptr;
};

