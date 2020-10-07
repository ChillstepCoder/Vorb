#pragma once

class ResourceManager;
class Camera2D;
class MaterialRenderer;
class ChunkRenderer;

struct GlobalRenderData {
    VGTexture atlas;
    f32 time;
    const Camera2D* mainCamera = nullptr;
};

// Singleton
class RenderContext {
protected:
    RenderContext(ResourceManager& resourceManager);
    ~RenderContext();

public:
    RenderContext(RenderContext& other) = delete;
    void operator=(const RenderContext&) = delete;

    static RenderContext& initInstance(ResourceManager& resourceManager);
    static RenderContext& getInstance();

    void initPostLoad();
    void beginFrame(const Camera2D& camera, const ResourceManager& resourceManager);

    const GlobalRenderData& getRenderData() const { return mRenderData; }
    ChunkRenderer& getChunkRenderer() const { return *mChunkRenderer; }
    MaterialRenderer& getMaterialRenderer() const { return *mMaterialRenderer; }

private:
    // Data
    GlobalRenderData mRenderData;

    static RenderContext* sInstance;

    // Renderers
    mutable std::unique_ptr<ChunkRenderer> mChunkRenderer;
    mutable std::unique_ptr<MaterialRenderer> mMaterialRenderer;
};

