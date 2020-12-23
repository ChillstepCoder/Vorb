#include "stdafx.h"
#include "RenderContext.h"
#include "ResourceManager.h"
#include "TextureAtlas.h"
#include "World.h"

#include "TextureManip.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/ChunkRenderer.h"
#include "rendering/LightRenderer.h"
#include "rendering/MaterialManager.h"
#include "rendering/ParticleSystemRenderer.h"
#include "TextureManip.h"
#include "DebugRenderer.h"
#include "EntityComponentSystemRenderer.h"

#include <Vorb/ui/InputDispatcher.h>
#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/TextureCache.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/BlendState.h>
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
    mMaterialRenderer       = std::make_unique<MaterialRenderer>(*this);
    mLightRenderer          = std::make_unique<LightRenderer>(resourceManager, *mMaterialRenderer);
    mChunkRenderer          = std::make_unique<ChunkRenderer>(resourceManager, *mMaterialRenderer);
    mEcsRenderer            = std::make_unique<EntityComponentSystemRenderer>(resourceManager, world);
    mParticleSystemRenderer = std::make_unique<ParticleSystemRenderer>(resourceManager, *mMaterialRenderer, screenResolution);
    checkGlError("Renderer init");

    mTextureManipulator = std::make_unique<GPUTextureManipulator>(resourceManager, *mMaterialRenderer);
    checkGlError("Init texture manipulator");

    // int UI resources
    mSb         = std::make_unique<vg::SpriteBatch>();
    mSpriteFont = std::make_unique<vg::SpriteFont>();
    mSb->init();
    mSpriteFont->init("data/fonts/chintzy.ttf", 32);
    checkGlError("SB init");

    // GBuffer
    vg::GBufferAttachment attachments[2];
    // Color
    attachments[FBO_GEOMETRY_COLOR].format = vg::TextureInternalFormat::RGB8;
    attachments[FBO_GEOMETRY_COLOR].number = FBO_GEOMETRY_COLOR;
    attachments[FBO_GEOMETRY_COLOR].pixelFormat = vg::TextureFormat::RGB;
    attachments[FBO_GEOMETRY_COLOR].pixelType = vg::TexturePixelType::UNSIGNED_BYTE;
    // Normals
    attachments[FBO_GEOMETRY_NORMAL].format = vg::TextureInternalFormat::RGB8;
    attachments[FBO_GEOMETRY_NORMAL].number = FBO_GEOMETRY_NORMAL;
    attachments[FBO_GEOMETRY_NORMAL].pixelFormat = vg::TextureFormat::RGB;
    attachments[FBO_GEOMETRY_NORMAL].pixelType = vg::TexturePixelType::UNSIGNED_BYTE;
    for (int i = 0; i < 2; ++i) {
        mGBuffers[i].setSize(ui32v2(mScreenResolution));
        mGBuffers[i].init(Array<vg::GBufferAttachment>(attachments, 2), vg::TextureInternalFormat::RGBA16F);
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
    mLightRenderer->InitPostLoad();
    mChunkRenderer->InitPostLoad();
    mTextureManipulator->InitPostLoad();

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

    mSunShadowMaterial = mResourceManager.getMaterialManager().getMaterial("shadow_apply");
    mSunLightMaterial = mResourceManager.getMaterialManager().getMaterial("sun_light");
    mLightPassThroughMaterial = mResourceManager.getMaterialManager().getMaterial("pass_through_light");
    mCopyDepthMaterial = mResourceManager.getMaterialManager().getMaterial("copy_depth");
}

void RenderContext::renderFrame(const Camera2D& camera) {

    ChunkRenderLOD lodState = ChunkRenderLOD::FULL_DETAIL;
    // TODO: Map texels to pixels?
    if (camera.getScale() < 3.0f) {
        lodState = ChunkRenderLOD::LOD_TEXTURE;
    }

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
    vg::DepthState::FULL.set();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT); // TODO: Remove color buffer clear, doing it just to reduce ghosting

    // TODO: Replace With BlendState
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (sDebugOptions.mWireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }


    mChunkRenderer->renderWorld(mWorld, camera, lodState);


    mEcsRenderer->renderPhysicsDebug(camera);
    //mEcsRenderer->renderSimpleSprites(camera);
    mEcsRenderer->renderCharacterModels(camera, vg::DepthState::FULL, 1.0f);

    // Particles
    // ISSUE: Particles are always in shadow, even if they have only vertical velocity.
    vg::DepthState::READ.set();
    // TODO: Replace With BlendState
    mParticleSystemRenderer->renderParticleSystems(camera, &activeGbuffer, true);
    vg::BlendState::set(vorb::graphics::BlendStateType::ALPHA);
    vg::DepthState::FULL.set();

    if (sDebugOptions.mWireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // Shadows
    mShadowGBuffer.useGeometry();
    if (lodState == ChunkRenderLOD::FULL_DETAIL) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // TODO: Replace With BlendState
        glBlendFunc(GL_ONE, GL_ZERO);
        mChunkRenderer->renderWorldShadows(mWorld, camera);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        activeGbuffer.useGeometry();
    }
    else {
        // TODO: Can we not do this every frame?
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        activeGbuffer.useGeometry();
    }

    // Debug Axis render
    DebugRenderer::drawVector(f32v2(0.0f), f32v2(5.0f, 0.0f), color4(1.0f, 0.0f, 0.0f));
    DebugRenderer::drawVector(f32v2(0.0f), f32v2(0.0f, 5.0f), color4(0.0f, 1.0f, 0.0f));
    //DebugRenderer::drawVector(f32v2(0.0f), f32v2(0.0f, 1.0f) * 4.0f, color4(0.0f, 0.0f, 1.0f));

    // Debug chunk boundaries
    if (sDebugOptions.mChunkBoundaries) {
        ChunkID id;
        const Chunk* chunk;
        mWorld.enumVisibleChunks(camera, [](const Chunk& chunk) {
            if (chunk.isDataReady()) {
                DebugRenderer::drawBox(chunk.getWorldPos(), f32v2(CHUNK_WIDTH), color4(0.0f, 1.0f, 0.0f));
                color4 neighborColor(1.0f, 0.0f, 0.0f);
                if (chunk.mNeighborBottom) {
                    DebugRenderer::drawLine(chunk.getWorldPos() + f32v2(CHUNK_WIDTH * 0.5f, 0.0f), f32v2(0.0f, 6.0f), neighborColor);
                }
                if (chunk.mNeighborTop) {
                    DebugRenderer::drawLine(chunk.getWorldPos() + f32v2(CHUNK_WIDTH * 0.5f, CHUNK_WIDTH), f32v2(0.0f, -6.0f), neighborColor);
                }
                if (chunk.mNeighborLeft) {
                    DebugRenderer::drawLine(chunk.getWorldPos() + f32v2(0.0f, CHUNK_WIDTH * 0.5f), f32v2(6.0f, 0.0f), neighborColor);
                }
                if (chunk.mNeighborRight) {
                    DebugRenderer::drawLine(chunk.getWorldPos() + f32v2(CHUNK_WIDTH, CHUNK_WIDTH * 0.5f), f32v2(-6.0f, 0.0f), neighborColor);
                }
            }
            else {
                DebugRenderer::drawBox(chunk.getWorldPos(), f32v2(CHUNK_WIDTH), color4(1.0f, 0.0f, 0.0f));
            }
        });
    }

    DebugRenderer::renderLines(camera.getCameraMatrix());

    // *** Post processes ***
    // Disable depth testing for post processing
    vg::DepthState::NONE.set();

    // Render characters that are behind geometry with some transparency
    mEcsRenderer->renderCharacterModels(camera, vg::DepthState::NONE, 0.20f);

    // Final Pass through process
    // Debug (kinda broken, need swap chain). This should also not be reading from same FBO it writes to...
    if (mPassthroughRenderMode != 0) {
        const Material* postMat = mPassthroughMaterials[mPassthroughRenderMode];
        assert(postMat);

        // TODO: Swap chain for this to work
        mMaterialRenderer->renderFullScreenQuad(*postMat);
    }

    // Disable depth testing for post processing
    // TODO: Swap chains?
    // TODO: This should be at top
    activeGbuffer.unuse();
    mCurrentFramebufferDims = mScreenResolution;

    // *** Lighting ***
    activeGbuffer.useLight();
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Sun Light
    mMaterialRenderer->renderFullScreenQuad(*mSunLightMaterial);

    // Sun Shadows
    if (mRenderData.sunHeight > 0.0f) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        mMaterialRenderer->renderFullScreenQuad(*mSunShadowMaterial);
    }

    //  Dynamic  light
    glBlendFunc(GL_ONE, GL_ONE);
    mEcsRenderer->renderDynamicLightComponents(camera, *mLightRenderer);

    activeGbuffer.unuse();
    mCurrentFramebufferDims = mScreenResolution;
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    vg::DepthState::NONE.set();

    // Final Lighting
    mMaterialRenderer->renderFullScreenQuad(*mLightPassThroughMaterial);

    // Copy depth for emissive rendering, so we can still depth test
    vg::DepthState::WRITE.set();
    mMaterialRenderer->renderFullScreenQuad(*mCopyDepthMaterial);
    vg::DepthState::READ.set();

    // Unlit Particles
    //mParticleSystemRenderer->renderParticleSystems(camera, &activeGbuffer, false);
    vg::DepthState::NONE.set();
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