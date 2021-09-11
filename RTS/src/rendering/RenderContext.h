#pragma once

class ResourceManager;
class Camera3D;
class ICamera;
class Material;
class MaterialRenderer;
class ParticleSystemRenderer;
class CityDebugRenderer;
class BatchedItemRenderer;
class EntityComponentSystemRenderer;
class GPUTextureManipulator;
class ChunkRenderer;
class LightRenderer;
class World;
class QuadMesh;
class Skybox;

#include <Vorb/graphics/GBuffer.h>

DECL_VG(class SpriteBatch);
DECL_VG(class SpriteFont);

struct GlobalRenderData {
    VGTexture atlas;
    f32 time;
    f32 sunHeight;
    f32v3 sunPositionCameraRelative;
    f32 cameraZAngle;
    f32 timeOfDay;
    f32v3 sunColor;
    f32v3 playerPos;
    f32v2 mousePosWorld;
    f32m4 skyRotMatrix;
    const ICamera* mainCamera = nullptr;
};

// Singleton
class RenderContext {
protected:
    RenderContext(ResourceManager& resourceManager, const World& world, const f32v2& screenResolution);
    ~RenderContext();

public:
    RenderContext(RenderContext& other) = delete;
    void operator=(const RenderContext&) = delete;

    static RenderContext& initInstance(ResourceManager& resourceManager, const World& world, const f32v2& screenResolution);
    static RenderContext& getInstance();

    void initPostLoad();

    void beginFrame(const ICamera* camera, f32v3 playerPos, f32v2 mousePosWorld); // Called automatically by beginFrame
    void renderFrame(const Camera3D& camera, f32v3 playerPos, f32v2 mousePosWorld, f32 frameAlpha);

    void reloadShaders();
    void selectNextDebugShader();

    const GlobalRenderData& getRenderData() const { return mRenderData; }
    ChunkRenderer& getChunkRenderer() const { return *mChunkRenderer; }
    MaterialRenderer& getMaterialRenderer() const { return *mMaterialRenderer; }
    const vg::GBuffer& getActiveGBuffer() const { return mGBuffers[mActiveGBuffer]; }
    const vg::GBuffer& getPrevGBuffer() const { return mGBuffers[mPrevGBuffer]; }
    const vg::GBuffer& getZCutoutGBuffer() const { return mZCutoutGBuffer; }
    const f32v2& getCurrentFramebufferDims() const { return mCurrentFramebufferDims; }

private:
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
    mutable std::unique_ptr<BatchedItemRenderer> mBatchedItemRenderer;

    // UI
    std::unique_ptr<vg::SpriteBatch> mSb;
    std::unique_ptr<vg::SpriteFont> mSpriteFont;

    int mPrevGBuffer = 1;
    int mActiveGBuffer = 0;
    vg::GBuffer mGBuffers[2];
    vg::GBuffer mZCutoutGBuffer;
    const World& mWorld;
    std::unique_ptr<QuadMesh> mHorizonQuad;
    std::unique_ptr<Skybox> mSkyBox;

    int mPassthroughRenderMode = 0;
    std::vector<const Material*> mPassthroughMaterials;
    const Material* mSunShadowMaterial = nullptr;
    const Material* mSunLightMaterial = nullptr;
    const Material* mLightPassThroughMaterial = nullptr;
    const Material* mCopyDepthMaterial = nullptr;
};

