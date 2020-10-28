#pragma once

class ResourceManager;
class Camera2D;
class Material;
class MaterialRenderer;
class EntityComponentSystemRenderer;
class ChunkRenderer;
class LightRenderer;
class GPUTextureManipulator;
class World;

#include <Vorb/graphics/GBuffer.h>

DECL_VG(class SpriteBatch);
DECL_VG(class SpriteFont);

struct GlobalRenderData {
    VGTexture atlas;
    f32 time;
    f32 sunHeight;
    f32 sunPosition;
    f32 timeOfDay;
    f32v3 sunColor;
    const Camera2D* mainCamera = nullptr;
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
    void renderFrame(const Camera2D& camera);

    void reloadShaders();
    void selectNextDebugShader();

    const GlobalRenderData& getRenderData() const { return mRenderData; }
    ChunkRenderer& getChunkRenderer() const { return *mChunkRenderer; }
    MaterialRenderer& getMaterialRenderer() const { return *mMaterialRenderer; }
    const vg::GBuffer& getActiveGBuffer() const { return mGBuffers[mActiveGBuffer]; }
    const vg::GBuffer& getPrevGBuffer() const { return mGBuffers[mPrevGBuffer]; }
    const vg::GBuffer& getShadowGBuffer() const { return mShadowGBuffer; }
    const f32v2& getCurrentFramebufferDims() const { return mCurrentFramebufferDims; }

private:
    void renderUI();

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

    // UI
    std::unique_ptr<vg::SpriteBatch> mSb;
    std::unique_ptr<vg::SpriteFont> mSpriteFont;

    int mPrevGBuffer = 1;
    int mActiveGBuffer = 0;
    vg::GBuffer mGBuffers[2];
    vg::GBuffer mShadowGBuffer;
    const World& mWorld;

    // TODO: Use this?
    std::unique_ptr<GPUTextureManipulator> mTextureManipulator;

    int mPassthroughRenderMode = 0;
    std::vector<const Material*> mPassthroughMaterials;
    const Material* mSunShadowMaterial = nullptr;
    const Material* mSunLightMaterial = nullptr;
    const Material* mLightPassThroughMaterial = nullptr;
};

