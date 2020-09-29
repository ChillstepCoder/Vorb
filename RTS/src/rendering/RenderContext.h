#pragma once

class ResourceManager;
class Camera2D;

struct GlobalRenderData {
    f32m4 viewProjectionMatrix;
    f32m4 worldMatrix;
    VGTexture atlas;
    f32 time;
    const Camera2D* mainCamera = nullptr;
};

class RenderContext {
public:

    void BeginFrame(const Camera2D& camera, const ResourceManager& resourceManager);

    const GlobalRenderData& GetRenderData() const { return mRenderData; }

private:
    GlobalRenderData mRenderData;
};

