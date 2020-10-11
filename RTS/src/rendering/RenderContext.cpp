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

const std::string sPassthroughMaterialNames[] = {
    "pass_through",
    //"depth",
    //"outline",
    //"motion_blur",
    "normals"
};

RenderContext* RenderContext::sInstance = nullptr;

RenderContext::RenderContext(ResourceManager& resourceManager, const World& world, const f32v2& screenResolution) :
    mResourceManager(resourceManager),
    mWorld(world),
    mScreenResolution(screenResolution)
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

    // GBuffer
    vg::GBufferAttachment attachments[1];
    attachments[0].format = vg::TextureInternalFormat::RGBA8;
    attachments[0].number = 0;
    attachments[0].pixelFormat = vg::TextureFormat::RGBA;
    attachments[0].pixelType = vg::TexturePixelType::UNSIGNED_BYTE;
    for (int i = 0; i < 2; ++i) {
        mGBuffers[i].setSize(ui32v2(mScreenResolution));
        mGBuffers[i].init(Array<vg::GBufferAttachment>(attachments, 1), vg::TextureInternalFormat::NONE);
        mGBuffers[i].initDepth(vg::TextureInternalFormat::DEPTH_COMPONENT24);
    }

    // Misc
    mTextureManipulator = std::make_unique<GPUTextureManipulator>();

    checkGlError("Init render context");

}

RenderContext::~RenderContext() {

}

RenderContext& RenderContext::initInstance(ResourceManager& resourceManager, const World& world, const f32v2& screenResolution) {
    if (!sInstance) {
        sInstance = new RenderContext(resourceManager, world, screenResolution);
    }
    return *sInstance;
}

RenderContext& RenderContext::getInstance() {
    // Is this thread safe???
    return *sInstance;
}

void RenderContext::initPostLoad() {
    mChunkRenderer->InitPostLoad();

    // Init all passthrough materials
    for (int i = 0; i < std::size(sPassthroughMaterialNames); ++i) {
        const Material* material = mResourceManager.getMaterialManager().getMaterial(sPassthroughMaterialNames[i]);
        if (material) {
            mPassthroughMaterials.emplace_back(material);
        }
        else {
            pError("Missing material for pass through: " + std::string(sPassthroughMaterialNames[i]));
        }
    }
}

void RenderContext::renderFrame(const Camera2D& camera) {

    // Set renderData
    mRenderData.mainCamera = &camera;
    mRenderData.atlas = mResourceManager.getTextureAtlas().getAtlasTexture();

    vg::GBuffer& activeGbuffer = mGBuffers[mActiveGBuffer];

    activeGbuffer.useGeometry();
    mCurrentFramebufferDims = activeGbuffer.getSize();

    // Clear screen
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDepthMask(true);
    glEnable(GL_DEPTH_TEST);

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
    // *** Post processes ***
    // Disable depth testing for post processing
    glDisable(GL_DEPTH_TEST);

    // Pass through process
    const Material* postMat = mPassthroughMaterials[mPassthroughRenderMode];
    assert(postMat);
    mCurrentFramebufferDims = mScreenResolution;

    mMaterialRenderer->renderMaterialToScreen(*postMat);

    // Final pass through
    activeGbuffer.unuse();
    // Pass through needs to always be  0
    const Material* finalMaterial = mPassthroughMaterials[0];
    assert(finalMaterial);
    mCurrentFramebufferDims = mScreenResolution;

    mMaterialRenderer->renderMaterialToScreen(*finalMaterial);

    // Swap
    mPrevGBuffer = mActiveGBuffer;
    mActiveGBuffer = !mActiveGBuffer;
}

void RenderContext::reloadShaders() {
    mChunkRenderer->ReloadShaders();
}

void RenderContext::selectNextDebugShader() {
    //mChunkRenderer->SelectNextShader();
    ++mPassthroughRenderMode;
    if (mPassthroughRenderMode >= mPassthroughMaterials.size()) {
        mPassthroughRenderMode = 0;
    }
}
