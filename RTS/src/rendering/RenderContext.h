#pragma once

class ResourceManager;
class Camera2D;
class MaterialRenderer;
class EntityComponentSystemRenderer;
class ChunkRenderer;
class GPUTextureManipulator;
class World;

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
    RenderContext(ResourceManager& resourceManager, const World& world);
    ~RenderContext();

public:
    RenderContext(RenderContext& other) = delete;
    void operator=(const RenderContext&) = delete;

    static RenderContext& initInstance(ResourceManager& resourceManager, const World& world);
    static RenderContext& getInstance();

    void initPostLoad();
    void renderFrame(const Camera2D& camera, const ResourceManager& resourceManager);

    void reloadShaders();
    void selectNextDebugShader();

    const GlobalRenderData& getRenderData() const { return mRenderData; }
    ChunkRenderer& getChunkRenderer() const { return *mChunkRenderer; }
    MaterialRenderer& getMaterialRenderer() const { return *mMaterialRenderer; }

private:
    static RenderContext* sInstance;

    // Data
    GlobalRenderData mRenderData;

    // Renderers
    mutable std::unique_ptr<ChunkRenderer> mChunkRenderer;
    mutable std::unique_ptr<MaterialRenderer> mMaterialRenderer;
    mutable std::unique_ptr<EntityComponentSystemRenderer> mEcsRenderer;

    // UI
    std::unique_ptr<vg::SpriteBatch> mSb;
    std::unique_ptr<vg::SpriteFont> mSpriteFont;

    const World& mWorld;

    // TODO: Use this?
    std::unique_ptr<GPUTextureManipulator> mTextureManipulator;
};

