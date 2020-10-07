#include "stdafx.h"
#include "RenderContext.h"
#include "ResourceManager.h"
#include "TextureAtlas.h"

#include "rendering/MaterialRenderer.h"
#include "rendering/ChunkRenderer.h"

RenderContext* RenderContext::sInstance = nullptr;

RenderContext::RenderContext(ResourceManager& resourceManager) {
    mMaterialRenderer = std::make_unique<MaterialRenderer>(*this);
    mChunkRenderer = std::make_unique<ChunkRenderer>(resourceManager, *mMaterialRenderer);
}

RenderContext::~RenderContext() {

}

RenderContext& RenderContext::initInstance(ResourceManager& resourceManager) {
    if (!sInstance) {
        sInstance = new RenderContext(resourceManager);
    }
    return *sInstance;
}

RenderContext& RenderContext::getInstance() {
    // Is this thread safe???
    return *sInstance;
}

void RenderContext::initPostLoad() {
    mChunkRenderer->InitPostLoad();
}

void RenderContext::beginFrame(const Camera2D& camera, const ResourceManager& resourceManager) {
    mRenderData.mainCamera = &camera;
    mRenderData.atlas = resourceManager.getTextureAtlas().getAtlasTexture();
}
