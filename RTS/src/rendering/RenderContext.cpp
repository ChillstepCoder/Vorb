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
#include <Vorb/colors.h>

// TODO: Render a string to the screen for these, Debug Render: %s (gone for pass_through)
// TODO: Instead of single shader these should be able to be shader chains.
const std::string sPassthroughMaterialNames[] = {
    "pass_through",
    "depth",
    "outline",
    "motion_blur",
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
    checkGlError("Renderer init");

    // int UI resources
    mSb         = std::make_unique<vg::SpriteBatch>();
    mSpriteFont = std::make_unique<vg::SpriteFont>();
    mSb->init();
    mSpriteFont->init("data/fonts/chintzy.ttf", 32);
    checkGlError("SB init");

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
    checkGlError("GBuffer init");

    // Shadow GBuffer
    vg::GBufferAttachment shadowAttachments[1];
    // Shadow alpha and source height
    shadowAttachments[0].format = vg::TextureInternalFormat::R8;
    shadowAttachments[0].number = 0;
    shadowAttachments[0].pixelFormat = vg::TextureFormat::RED;
    shadowAttachments[0].pixelType = vg::TexturePixelType::FLOAT;
    mShadowGBuffer.setSize(ui32v2(mScreenResolution));
    mShadowGBuffer.init(Array<vg::GBufferAttachment>(shadowAttachments, 1), vg::TextureInternalFormat::NONE);
    mShadowGBuffer.initDepth(vg::TextureInternalFormat::DEPTH_COMPONENT24);
    checkGlError("Shadow GBuffer Init");

    // Misc
    mTextureManipulator = std::make_unique<GPUTextureManipulator>();
    checkGlError("Init texture manipulator");

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

    mShadowMaterial = mResourceManager.getMaterialManager().getMaterial("shadow_apply");
    mLightingMaterial = mResourceManager.getMaterialManager().getMaterial("lighting");
}

void RenderContext::renderFrame(const Camera2D& camera) {

    // Set renderData
    mRenderData.mainCamera = &camera;
    mRenderData.atlas = mResourceManager.getTextureAtlas().getAtlasTexture();
    mRenderData.sunHeight = mWorld.getSunHeight();
    mRenderData.sunColor = mWorld.getSunColor();
    mRenderData.timeOfDay = mWorld.getTimeOfDay();
    mRenderData.sunPosition = mWorld.getSunPosition();

    vg::GBuffer& activeGbuffer = mGBuffers[mActiveGBuffer];

    activeGbuffer.useGeometry();
    mCurrentFramebufferDims = activeGbuffer.getSize();

    // Clear screen
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDepthMask(true);
    glEnable(GL_DEPTH_TEST);

    // TODO: Replace With BlendState
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    mChunkRenderer->renderWorld(mWorld, camera);

    if (sDebugOptions.mWireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    //mEcsRenderer->renderPhysicsDebug(*m_camera2D);
    mEcsRenderer->renderSimpleSprites(camera);
    mEcsRenderer->renderCharacterModels(camera);

    // Shadows
    mShadowGBuffer.useGeometry();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // TODO: Replace With BlendState
    //glBlendFunc(GL_ONE, GL_ONE);
    //glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
    glBlendFunc(GL_ONE, GL_ZERO);
    mChunkRenderer->renderWorldShadows(mWorld, camera);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    activeGbuffer.useGeometry();

    vg::DepthState::FULL.set();

    // Debug Axis render
    DebugRenderer::drawVector(f32v2(0.0f), f32v2(5.0f, 0.0f), color4(1.0f, 0.0f, 0.0f));
    DebugRenderer::drawVector(f32v2(0.0f), f32v2(0.0f, 5.0f), color4(0.0f, 1.0f, 0.0f));
    //DebugRenderer::drawVector(f32v2(0.0f), f32v2(0.0f, 1.0f) * 4.0f, color4(0.0f, 0.0f, 1.0f));

    // Debug chunk boundaries
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

    // SHADOWS
    {
        mMaterialRenderer->renderMaterialToScreen(*mShadowMaterial);
    }
    
    // TODO: Swap chains?
    // Final pass through
    activeGbuffer.unuse();
    mCurrentFramebufferDims = mScreenResolution;

    mMaterialRenderer->renderMaterialToScreen(*mLightingMaterial);

    // UI last
    renderUI();

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

void RenderContext::renderUI() {
    mSb->begin();
    char buffer[255];
    const float GAP_SIZE = 64.0f;
    float yOffset = 0.0f;
    const f32v2 scale(2.0f);

    sprintf_s(buffer, sizeof(buffer), "FPS: %.0f", sFps);
    mSb->drawString(mSpriteFont.get(), buffer, f32v2(0.0f, 0.7f * mScreenResolution.y + yOffset), scale, color::White);
    yOffset += GAP_SIZE;

    sprintf_s(buffer, sizeof(buffer), "TimeOfDay: %.2f", mWorld.getTimeOfDay());
    mSb->drawString(mSpriteFont.get(), buffer, f32v2(0.0f, 0.7f * mScreenResolution.y + yOffset), scale, color::White);
    yOffset += GAP_SIZE;

    sprintf_s(buffer, sizeof(buffer), "SunHeight: %.2f", mWorld.getSunHeight());
    mSb->drawString(mSpriteFont.get(), buffer, f32v2(0.0f, 0.7f * mScreenResolution.y + yOffset), scale, color::White);
    yOffset += GAP_SIZE;

    sprintf_s(buffer, sizeof(buffer), "SunPosition: %.2f", mWorld.getSunPosition());
    mSb->drawString(mSpriteFont.get(), buffer, f32v2(0.0f, 0.7f * mScreenResolution.y + yOffset), scale, color::White);
    yOffset += GAP_SIZE;

    mSb->end();
    mSb->render(mScreenResolution);
}