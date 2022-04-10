#pragma once

class BuildingRenderer;
class Camera3D;
class CharacterRenderer;
class ChunkRenderer;
class CityDebugRenderer;
class CloudRenderer;
class AmbientOcclusionPostProcess;
class DepthOfFieldPostProcess;
class EntityComponentSystemRenderer;
class GPUTextureManipulator;
class ICamera;
class ItemRenderer;
class LightRenderer;
class Material;
class MaterialRenderer;
class ParticleSystemRenderer;
class QuadMesh;
class ResourceManager;
class Skybox;
class ShadowRenderer;
class TerrainRenderer;
class UIContext;
class World;

struct SDL_Window;

#include <Vorb/graphics/GBuffer.h>
#include "rendering/GlobalUboData.h"

DECL_VG(class SpriteBatch);
DECL_VG(class SpriteFont);

struct GlobalRenderData {
    GlobalUboData globalUboData;
    VGTexture atlas;
    f32 cameraZAngle;
    f32m4 skyRotMatrix;
    const f32m4* shadowFrustumMatrices;
    const f32* shadowCascadePlaneDistances;
    VGTexture shadowMap;
    ui32 shadowFrustumMatricesCount;
    const ICamera* mainCamera = nullptr;
};

// Singleton
class RenderContext {
protected:
    RenderContext(ResourceManager& resourceManager, const World& world, const f32v2& screenResolution, SDL_Window* window);
    ~RenderContext();

public:
    RenderContext(RenderContext& other) = delete;
    void operator=(const RenderContext&) = delete;

    static RenderContext& initInstance(ResourceManager& resourceManager, const World& world, const f32v2& screenResolution, SDL_Window* window);
    static RenderContext& getInstance();

    void initPostLoad();

    void beginFrame(const Camera3D* camera, f32v3 playerPos); // Called automatically by beginFrame
    void renderFrame(const Camera3D& camera, f32v3 playerPos, f32 frameAlpha, f32 elapsedSec);
    void endFrame();

    void selectNextDebugShader();

    const GlobalRenderData& getRenderData() const { return mRenderData; }
    ChunkRenderer& getChunkRenderer() const { return *mChunkRenderer; }
    MaterialRenderer& getMaterialRenderer() const { return *mMaterialRenderer; }
    const vg::GBuffer& getActiveGBuffer() const { return *mActiveGBuffer; }
    const vg::GBuffer& getPrevFinalGBuffer() const { return mGBuffers[mPrevGBufferIndex]; }
    const f32v2& getCurrentFramebufferDims() const { return mCurrentFramebufferDims; }
    VGTexture getShadowTexture() const;
    VGTexture getSSAOTexture() const;

private:
    void renderDebug(const Camera3D& camera);
    void renderUI(const Camera3D& camera);
    void buildHorizonMesh();

    static RenderContext* sInstance;

    // Data
    GlobalRenderData mRenderData;
    f32v2 mScreenResolution;
    f32v2 mCurrentFramebufferDims;
    ResourceManager& mResourceManager;

    // Renderers
    mutable std::unique_ptr<MaterialRenderer> mMaterialRenderer;
    mutable std::unique_ptr<ChunkRenderer> mChunkRenderer;
    mutable std::unique_ptr<LightRenderer> mLightRenderer;
    mutable std::unique_ptr<EntityComponentSystemRenderer> mEcsRenderer;
    mutable std::unique_ptr<GPUTextureManipulator> mTextureManipulator;
    mutable std::unique_ptr<ParticleSystemRenderer> mParticleSystemRenderer;
    mutable std::unique_ptr<CityDebugRenderer> mCityDebugRenderer;
    mutable std::unique_ptr<ItemRenderer> mItemRenderer;
    mutable std::unique_ptr<CharacterRenderer> mCharacterRenderer;
    mutable std::unique_ptr<BuildingRenderer> mBuildingRenderer;
    mutable std::unique_ptr<CloudRenderer> mCloudRenderer;
    mutable std::unique_ptr<DepthOfFieldPostProcess> mDepthOfField;
    mutable std::unique_ptr<AmbientOcclusionPostProcess> mAmbientOcclusion;
    mutable std::unique_ptr<ShadowRenderer> mShadowRenderer;
    mutable std::unique_ptr<TerrainRenderer> mTerrainRenderer;

    // UI
    std::unique_ptr<vg::SpriteBatch> mSb;
    std::unique_ptr<vg::SpriteFont> mSpriteFont;
    SDL_Window* mWindow;

    int mPrevGBufferIndex = 1;
    int mActiveGBufferIndex = 0;
    vg::GBuffer* mActiveGBuffer = nullptr;
    vg::GBuffer mGBuffers[2];
    vg::GBuffer mTransparencyGBuffer;
    const World& mWorld;
    std::unique_ptr<QuadMesh> mHorizonQuad;
    std::unique_ptr<Skybox> mSkyBox;
    VGBuffer mGlobalUbo = 0;

    int mPassthroughRenderMode = 0;
    std::vector<const Material*> mPassthroughMaterials;
    const Material* mPassthroughMaterial = nullptr;
    const Material* mSceneLightingMaterial = nullptr;
    const Material* mCopyDepthMaterial = nullptr;
};

