#include "stdafx.h"
#include "RenderContext.h"
#include "ResourceManager.h"
#include "TextureAtlas.h"
#include "World.h"

#include "TextureManip.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/ChunkRenderer.h"
#include "rendering/MaterialManager.h"
#include "rendering/MaterialRenderer.h"
#include "DebugRenderer.h"
#include "EntityComponentSystemRenderer.h"

#include <Vorb/ui/InputDispatcher.h>
#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/TextureCache.h>
#include <Vorb/graphics/DepthState.h>

RenderContext* RenderContext::sInstance = nullptr;

RenderContext::RenderContext(ResourceManager& resourceManager, const World& world) :
    mWorld(world)
{
    // Init renderers
    mMaterialRenderer   = std::make_unique<MaterialRenderer>(*this);
    mChunkRenderer      = std::make_unique<ChunkRenderer>(resourceManager, *mMaterialRenderer);
    mEcsRenderer        = std::make_unique<EntityComponentSystemRenderer>(resourceManager, world);

    // int UI resources
    mSb         = std::make_unique<vg::SpriteBatch>();
    mSpriteFont = std::make_unique<vg::SpriteFont>();
    mSb->init();
    mSpriteFont->init("data/fonts/chintzy.ttf", 32);

    // Misc
    mTextureManipulator = std::make_unique<GPUTextureManipulator>();
}

RenderContext::~RenderContext() {

}

RenderContext& RenderContext::initInstance(ResourceManager& resourceManager, const World& world) {
    if (!sInstance) {
        sInstance = new RenderContext(resourceManager, world);
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

void RenderContext::renderFrame(const Camera2D& camera, const ResourceManager& resourceManager) {
    // Set renderData
    mRenderData.mainCamera = &camera;
    mRenderData.atlas = resourceManager.getTextureAtlas().getAtlasTexture();

    // Clear screen
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mChunkRenderer->renderWorld(mWorld, camera);

    if (sDebugOptions.mWireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // Axis render
    DebugRenderer::drawVector(f32v2(0.0f), f32v2(5.0f, 0.0f), color4(1.0f, 0.0f, 0.0f));
    DebugRenderer::drawVector(f32v2(0.0f), f32v2(0.0f, 5.0f), color4(0.0f, 1.0f, 0.0f));
    //DebugRenderer::drawVector(f32v2(0.0f), f32v2(0.0f, 1.0f) * 4.0f, color4(0.0f, 0.0f, 1.0f));

    //mEcsRenderer->renderPhysicsDebug(*m_camera2D);
    mEcsRenderer->renderSimpleSprites(camera);
    mEcsRenderer->renderCharacterModels(camera);

    if (sDebugOptions.mChunkBoundaries) {
        ChunkID id;
        const Chunk* chunk;
        while (mWorld.enumVisibleChunks(camera, id, &chunk)) {
            if (chunk) {
                DebugRenderer::drawBox(chunk->getWorldPos(), f32v2(CHUNK_WIDTH), color4(0.0f, 1.0f, 0.0f));
            }
        }
    }

    DebugRenderer::renderLines(camera.getCameraMatrix());

    const Material* testMaterial = resourceManager.getMaterialManager().getMaterial("normals");
    assert(testMaterial);
    mMaterialRenderer->renderMaterialToScreen(*testMaterial);
}

void RenderContext::reloadShaders() {
    mChunkRenderer->ReloadShaders();
}

void RenderContext::selectNextDebugShader() {
    mChunkRenderer->SelectNextShader();
}
