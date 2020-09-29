#include "stdafx.h"
#include "RenderContext.h"
#include "ResourceManager.h"
#include "TextureAtlas.h"

void RenderContext::BeginFrame(const Camera2D& camera, const ResourceManager& resourceManager) {
    mRenderData.mainCamera = &camera;
    mRenderData.atlas = resourceManager.getTextureAtlas().getAtlasTexture();
}
