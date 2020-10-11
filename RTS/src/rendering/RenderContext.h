#pragma once

class ResourceManager;
class Camera2D;
class MaterialRenderer;
class EntityComponentSystemRenderer;
class ChunkRenderer;
class GPUTextureManipulator;
class World;

#include <Vorb/graphics/GBuffer.h>

DECL_VG(class SpriteBatch);
DECL_VG(class SpriteFont);

struct GlobalRenderData {
    VGTexture atlas;
    f32 time;
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
    void renderFrame(const Camera2D& camera, const ResourceManager& resourceManager);

    void reloadShaders();
    void selectNextDebugShader();

    const GlobalRenderData& getRenderData() const { return mRenderData; }
    ChunkRenderer& getChunkRenderer() const { return *mChunkRenderer; }
    MaterialRenderer& getMaterialRenderer() const { return *mMaterialRenderer; }
    const vg::GBuffer& getGBuffer() const { return mGBuffer; }
    const f32v2& getCurrentFramebufferDims() const { return mCurrentFramebufferDims; }

private:
    static RenderContext* sInstance;

    // Data
    GlobalRenderData mRenderData;
    f32v2 mScreenResolution;
    f32v2 mCurrentFramebufferDims;

    // Renderers
    mutable std::unique_ptr<ChunkRenderer> mChunkRenderer;
    mutable std::unique_ptr<MaterialRenderer> mMaterialRenderer;
    mutable std::unique_ptr<EntityComponentSystemRenderer> mEcsRenderer;

    // UI
    std::unique_ptr<vg::SpriteBatch> mSb;
    std::unique_ptr<vg::SpriteFont> mSpriteFont;

    vg::GBuffer mGBuffer;
    const World& mWorld;

    // TODO: Use this?
    std::unique_ptr<GPUTextureManipulator> mTextureManipulator;

    bool mRenderDepth = false;
};

