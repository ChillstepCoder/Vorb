#include "stdafx.h"
#include "RenderContext.h"
#include "ResourceManager.h"
#include "TextureAtlas.h"
#include "World.h"
#include "world/TileRepository.h"

#include "DebugRenderer.h"
#include "EntityComponentSystemRenderer.h"
#include "rendering/BuildingRenderer.h"
#include "rendering/CharacterRenderer.h"
#include "rendering/ChunkRenderer.h"
#include "rendering/CityDebugRenderer.h"
#include "rendering/CloudRenderer.h"
#include "rendering/DebugTweakerPanel.h"
#include "rendering/post_process/DepthOfFieldPostProcess.h"
#include "rendering/ItemRenderer.h"
#include "rendering/LightRenderer.h"
#include "rendering/MaterialManager.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/ParticleSystemRenderer.h"
#include "rendering/QuadMesh.h"
#include "rendering/Skybox.h"
#include "rendering/post_process/ShadowRenderer.h"
#include "rendering/RenderStats.h"
#include "TextureManip.h"

// TODO: Move to renderer?
#include "city/CityQuartermaster.h"
#include "item/ItemStockpileRegistry.h"

#include "camera/ICamera.h"
#include "camera/Camera3D.h"

#include "city/City.h"

#include <Vorb/ui/InputDispatcher.h>
#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/TextureCache.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/BlendState.h>
#include <Vorb/colors.h>
#include <Vorb/graphics/FullQuadVBO.h>

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include "options/DebugOptions.h"

// TODO: Render a string to the screen for these, Debug Render: %s (gone for pass_through)
// TODO: Instead of single shader these should be able to be shader chains.
const std::string sPassthroughMaterialNames[] = {
    "pass_through",
    "depth",
    "shadow_depth_debug",
    "motion_blur",
    "normals"
};

RenderContext* RenderContext::sInstance = nullptr;

RenderContext::RenderContext(ResourceManager& resourceManager, const World& world, const f32v2& screenResolution, SDL_Window* window) :
    mResourceManager(resourceManager),
    mWorld(world),
    mScreenResolution(screenResolution),
    mWindow(window)
{
    // Mesh init
    MeshBase::initStaticIBO();
    checkGlError("Meshbase init");

    // int UI resources
    mSb         = std::make_unique<vg::SpriteBatch>();
    mSpriteFont = std::make_unique<vg::SpriteFont>();
    mSb->init();
    mSpriteFont->init("data/fonts/chintzy.ttf", 32);
    checkGlError("SB init");

    sGlobalFullQuadVBO.init();

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
        mGBuffers[i].init(attachments[FBO_GEOMETRY_COLOR], &attachments[FBO_GEOMETRY_NORMAL], vg::TextureInternalFormat::RGBA16F);
        mGBuffers[i].initDepth(vg::TextureInternalFormat::DEPTH_COMPONENT32);
    }
    checkGlError("GBuffer init");

    // Shadow GBuffer
    //vg::GBufferAttachment shadowAttachments[1];
    //// Shadow alpha and source height
    //shadowAttachments[0].format = vg::TextureInternalFormat::R16;
    //shadowAttachments[0].number = 0;
    //shadowAttachments[0].pixelFormat = vg::TextureFormat::RED;
    //shadowAttachments[0].pixelType = vg::TexturePixelType::FLOAT;
    //mShadowGBuffer.setSize(ui32v2(mScreenResolution));
    //mShadowGBuffer.init(Array<vg::GBufferAttachment>(shadowAttachments, 1), vg::TextureInternalFormat::NONE);
    //checkGlError("Shadow GBuffer Init");

    // ZCutout GBuffer
    vg::GBufferAttachment zCutoutAttachment;
    // ZCutout alpha and source height
    zCutoutAttachment.format = vg::TextureInternalFormat::R8;
    zCutoutAttachment.number = 0;
    zCutoutAttachment.pixelFormat = vg::TextureFormat::RED;
    zCutoutAttachment.pixelType = vg::TexturePixelType::FLOAT;
    mZCutoutGBuffer.setSize(ui32v2(mScreenResolution));
    mZCutoutGBuffer.init(zCutoutAttachment, nullptr);
    checkGlError("Z Cutout GBuffer Init");

    int maxTextureSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    if (maxTextureSize < TEXTURE_ATLAS_WIDTH_PX) {
        pError("GFX card does not support 4k textures :(");
        assert(false);
    }

    // Debugging
    mDebugTweakerPanel = std::make_unique<DebugTweakerPanel>(screenResolution);
}

RenderContext::~RenderContext() {

}

RenderContext& RenderContext::initInstance(ResourceManager& resourceManager, const World& world, const f32v2& screenResolution, SDL_Window* window) {
    if (!sInstance) {
        sInstance = new RenderContext(resourceManager, world, screenResolution, window);
    }
    return *sInstance;
}

RenderContext& RenderContext::getInstance() {
    // Is this thread safe???
    return *sInstance;
}

void RenderContext::initPostLoad() {

    // Initialize renderer after material assets are loaded
    mCharacterRenderer = std::make_unique<CharacterRenderer>(mResourceManager.getMaterialManager());
    // Init renderers
    mMaterialRenderer = std::make_unique<MaterialRenderer>(*this);
    mLightRenderer = std::make_unique<LightRenderer>(mResourceManager, *mMaterialRenderer);
    mChunkRenderer = std::make_unique<ChunkRenderer>(mResourceManager, *mMaterialRenderer);
    mEcsRenderer = std::make_unique<EntityComponentSystemRenderer>(mResourceManager, mWorld);
    mParticleSystemRenderer = std::make_unique<ParticleSystemRenderer>(mResourceManager, *mMaterialRenderer, mScreenResolution);
    mCityDebugRenderer = std::make_unique<CityDebugRenderer>();
    mItemRenderer = std::make_unique<ItemRenderer>(mResourceManager, *mMaterialRenderer);
    mBuildingRenderer = std::make_unique<BuildingRenderer>(mResourceManager, *mMaterialRenderer);
    mCloudRenderer = std::make_unique<CloudRenderer>(mResourceManager, *mMaterialRenderer, mScreenResolution);
    mDepthOfField = std::make_unique<DepthOfFieldPostProcess>(mResourceManager, *mMaterialRenderer, mScreenResolution);
    mShadowRenderer = std::make_unique<ShadowRenderer>(mResourceManager, *mMaterialRenderer, mScreenResolution);
    checkGlError("Renderer init");
    mTextureManipulator = std::make_unique<GPUTextureManipulator>(mResourceManager, *mMaterialRenderer);
    checkGlError("Init texture manipulator");

    // TODO: These can be eliminated and put into constructor???
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

    mSunLightMaterial = mResourceManager.getMaterialManager().getMaterial("sun_light");
    mLightPassThroughMaterial = mResourceManager.getMaterialManager().getMaterial("pass_through_light");
    mCopyDepthMaterial = mResourceManager.getMaterialManager().getMaterial("copy_depth");

    buildHorizonMesh();
    mSkyBox = std::make_unique<Skybox>();
    mSkyBox->init(mResourceManager.getMaterialManager().getMaterial("sky"));

}

void RenderContext::beginFrame(const Camera3D* camera, f32v3 playerPos) {
    RenderStats::clear();
    // Set renderData
    mRenderData.mainCamera = camera;
    mRenderData.atlas = mResourceManager.getTextureAtlas().getAtlasTexture();
    mRenderData.sunHeight = mWorld.getSunHeight();
    mRenderData.sunColor = mWorld.getSunColor();
    mRenderData.timeOfDay = mWorld.getTimeOfDay();
    const f32v3& sun = mWorld.getSunPosition();
    mRenderData.sunPositionWorld = sun;
    std::cout << "Sunposition " << sun.x << " " << sun.y << " " << sun.z << std::endl;
    mRenderData.sunPositionCameraRelative = glm::normalize(f32v3(camera->getViewMatrix() * f32v4(sun.x, sun.y, sun.z, 1.0f)));
    mRenderData.cameraZAngle = camera->getZAngle();
    mRenderData.playerPos = playerPos;
    mRenderData.skyRotMatrix = mWorld.getSkyRotMatrix();

    mRenderData.sunRight = glm::normalize(glm::cross(sun, f32v3(0.0f, 0.0f, 1.0f)));
    mRenderData.sunUp = glm::normalize(glm::cross(sun, mRenderData.sunRight));

    // Shadows
    mShadowRenderer->beginFrame(*camera, sun);
    mRenderData.shadowFrustumMatrices = mShadowRenderer->getShadowFrustumMatrices();
    mRenderData.shadowCascadePlaneDistances = mShadowRenderer->getShadowCascadePlaneDistances();
    mRenderData.shadowMap = mShadowRenderer->getShadowMap();
    mRenderData.shadowFrustumMatricesCount = MAX_SHADOW_CASCADE_LEVELS;

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(mWindow);
    ImGui::NewFrame();

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
}

void RenderContext::renderFrame(const Camera3D& camera, f32v3 playerPos, f32 frameAlpha) {

    ChunkRenderLOD lodState = ChunkRenderLOD::FULL_DETAIL;
    // TODO: Map texels to pixels?
    if (camera.getScale() < 1.5f) {
        // TODO: Remove for 2D
        //lodState = ChunkRenderLOD::LOD_TEXTURE;
    }

    // TODO: Should this happen here? Maybe assert instead?
    beginFrame(&camera, playerPos);
    
    mActiveGBuffer = &mGBuffers[mActiveGBufferIndex];

    // Cutout pass (wtf is this?)
    /*if (lodState == ChunkRenderLOD::FULL_DETAIL) {
        mZCutoutGBuffer.useGeometry();
        vg::BlendState::set(vg::BlendStateType::REPLACE);
        glClear(GL_COLOR_BUFFER_BIT);

        mChunkRenderer->renderChunksZCutout(mWorld, camera2d);
    }*/

    // Main geometry pass
    mActiveGBuffer->useGeometry();
    mCurrentFramebufferDims = mActiveGBuffer->getSize();

    // Clear screen
    vg::DepthState::FULL.set();
    vg::BlendState::set(vg::BlendStateType::ALPHA);

    if (sDebugOptions.mWireframe) {
        glClearColor(0.5f, 0.5f, 0.5f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    else {
        glClear(GL_DEPTH_BUFFER_BIT);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    // TODO: Replace With BlendState
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    mChunkRenderer->renderWorld(mWorld, camera, lodState);
    
    // COMMENT OUT TO DISABLE CHARACTER
    mEcsRenderer->renderCharacterModels(*mCharacterRenderer, *mMaterialRenderer, camera, 1.0f, frameAlpha);
    mEcsRenderer->renderPhysicsDebug(camera);
    
    //mEcsRenderer->renderSimpleSprites(camera);
    mEcsRenderer->renderInteractUI(camera);

    // Render stockpiles
    for (auto&& stockPilePtr : mWorld.getItemStockpileRegistry().getAllStockpiles()) {
        if (stockPilePtr->isVisible()) {
            mItemRenderer->renderStockpile(*stockPilePtr);
        }
    }

    // Render loose items

    // Render building roofs
    // TODO: Frustum cull
    const CityGraph& cities = mWorld.getCities();
    for (auto&& city : cities.mNodes) {
        const std::vector<Building>& buildings = city->getBuildings();
        for (auto& building : buildings) {
            mBuildingRenderer->renderBuildingRoof(building);
        }
    }

    // Clouds
    mCloudRenderer->renderClouds(mWorld.getCloudManager(), mActiveGBuffer, camera);

    // Sky
    glEnable(GL_DEPTH_CLAMP);
    glDisable(GL_CULL_FACE); // TODO: Fix geometry so we dont have to disable cull face
    mSkyBox->render(*mMaterialRenderer);
    glEnable(GL_CULL_FACE);
    glDisable(GL_DEPTH_CLAMP);

    // Horizon
    mMaterialRenderer->renderMesh(*mHorizonQuad, *mResourceManager.getMaterialManager().getMaterial("simple_color"));

    // Shadows
    if (mRenderData.sunHeight > 0.01f) {
        mShadowRenderer->useShadowBuffer();
        glEnable(GL_DEPTH_CLAMP);

        vg::DepthState::FULL.set();
        // Render all shadow casters
        //glCullFace(GL_FRONT);
        mChunkRenderer->renderWorldShadows(mWorld, camera, lodState, mShadowRenderer->getMaxDistance());

        //glCullFace(GL_BACK);
        // TODO: Frustum cull
        mCloudRenderer->renderCloudShadows(mWorld.getCloudManager());

        const CityGraph& cities = mWorld.getCities();
        for (auto&& city : cities.mNodes) {
            const std::vector<Building>& buildings = city->getBuildings();
            for (auto& building : buildings) {
                mBuildingRenderer->renderBuildingRoofShadows(building);
            }
        }


        glDisable(GL_DEPTH_CLAMP);

        mActiveGBuffer = mShadowRenderer->renderShadows(mActiveGBuffer);

        mActiveGBuffer->useGeometry();
    }

    // Particles
    if (lodState == ChunkRenderLOD::FULL_DETAIL) {
        vg::DepthState::READ.set();
        // TODO: Replace With BlendState
        mParticleSystemRenderer->renderParticleSystems(camera, mActiveGBuffer, true);
        vg::BlendState::set(vorb::graphics::BlendStateType::ALPHA);
        vg::DepthState::FULL.set();
    }

    if (sDebugOptions.mWireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    //// Shadows
    //mShadowGBuffer.useGeometry();
    //if (lodState == ChunkRenderLOD::FULL_DETAIL) {
    //    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //    // TODO: Replace With BlendState
    //    glBlendFunc(GL_ONE, GL_ZERO);
    //    mChunkRenderer->renderWorldShadows(mWorld, camera2d);
    //    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //    activeGbuffer.useGeometry();
    //}
    //else {
    //    // TODO: Can we not do this every frame?
    //    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //    activeGbuffer.useGeometry();
    //}

    // City Debug
    if (sDebugOptions.mCities) {
        const CityGraph& cities = mWorld.getCities();
        for (auto&& city : cities.mNodes) {
            mCityDebugRenderer->renderCityPlannerDebug(city->getCityPlanner());
            mCityDebugRenderer->renderCityBuilderDebug(city->getCityBuilder());
            mCityDebugRenderer->renderCityPlotterDebug(city->getCityPlotter());
            mCityDebugRenderer->renderCityQuartermasterDebug(city->getCityQuartermaster());
        }
        mCityDebugRenderer->finishRenderFrame();
    }
    else {
        mCityDebugRenderer->clearMeshes();
    }

    if (sDebugOptions.mChunkBoundaries) {
        // Debug chunk boundaries
        mWorld.enumVisibleChunks([](const Chunk& chunk) {
            if (chunk.isDataReady()) {
                DebugRenderer::drawWireQuad(chunk.getWorldPos(), f32v2(CHUNK_WIDTH), color4(0.0f, 1.0f, 0.0f));
                color4 neighborColor(1.0f, 0.0f, 0.0f);
                if (chunk.mDataReadyNeighborCount == 1) {
                    neighborColor = color4(0.0f, 1.0f, 0.0f);
                }
                if (chunk.getBottomNeighbor().isDataReady()) {
                    DebugRenderer::drawLine(chunk.getWorldPos() + f32v2(CHUNK_WIDTH * 0.5f, 0.0f), f32v2(0.0f, 6.0f), neighborColor);
                }
                if (chunk.getTopNeighbor().isDataReady()) {
                    DebugRenderer::drawLine(chunk.getWorldPos() + f32v2(CHUNK_WIDTH * 0.5f, CHUNK_WIDTH), f32v2(0.0f, -6.0f), neighborColor);
                }
                if (chunk.getLeftNeighbor().isDataReady()) {
                    DebugRenderer::drawLine(chunk.getWorldPos() + f32v2(0.0f, CHUNK_WIDTH * 0.5f), f32v2(6.0f, 0.0f), neighborColor);
                }
                if (chunk.getRightNeighbor().isDataReady()) {
                    DebugRenderer::drawLine(chunk.getWorldPos() + f32v2(CHUNK_WIDTH, CHUNK_WIDTH * 0.5f), f32v2(-6.0f, 0.0f), neighborColor);
                }
            }
            else {
                DebugRenderer::drawWireQuad(chunk.getWorldPos(), f32v2(CHUNK_WIDTH), color4(1.0f, 0.0f, 1.0f));
            }
        });

        // Debug region boundaries
        mWorld.enumVisibleRegions(camera, [](const Region& region) {
            DebugRenderer::drawWireQuad(region.getWorldPos(), f32v2(WorldData::REGION_WIDTH_TILES), color4(1.0f, 0.0f, 0.0f));
        });
    }

    // Debug
    DebugRenderer::render(camera.getPosition(), camera.getVPMatrix());

    // *** Post processes ***
    // Disable depth testing for post processing
    vg::DepthState::NONE.set();

    mActiveGBuffer = mDepthOfField->render(mActiveGBuffer);

    // Render characters that are behind geometry with some transparency
    //mEcsRenderer->renderCharacterModels(*mCharacterRenderer, *mMaterialRenderer, camera, 0.20f, frameAlpha);
        // Depth debug
    if (mPassthroughRenderMode == 1) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        const Material* postMat = mPassthroughMaterials[mPassthroughRenderMode];
        assert(postMat);

        // TODO: Swap chain for this to work
        mMaterialRenderer->renderFullScreenQuad(*postMat);
    }


    // Disable depth testing for post processing
    // TODO: Swap chains?
    // TODO: This should be at top
    mActiveGBuffer->unuse();
    mCurrentFramebufferDims = mScreenResolution;

    // *** Lighting ***
    mActiveGBuffer->useLight();
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Sun Light
    // TODO: Collapse this into lightPassThrough?
    mMaterialRenderer->renderFullScreenQuad(*mSunLightMaterial);

    //  Dynamic  light
    glBlendFunc(GL_ONE, GL_ONE);
    mEcsRenderer->renderDynamicLightComponents(camera, *mLightRenderer);


    mActiveGBuffer->unuse();
    mCurrentFramebufferDims = mScreenResolution;
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    vg::DepthState::NONE.set();

    // Final Lighting
    mMaterialRenderer->renderFullScreenQuad(*mLightPassThroughMaterial);

    // Sky
   /* vg::DepthState::READ.set();
    renderSky(camera);
    vg::DepthState::FULL.set();*/

    // Copy depth for emissive rendering, so we can still depth test
    //vg::DepthState::WRITE.set();
    //mMaterialRenderer->renderFullScreenQuad(*mCopyDepthMaterial);
   // vg::DepthState::READ.set();

    // Unlit Particles
    //mParticleSystemRenderer->renderParticleSystems(camera, &activeGbuffer, false);
    vg::DepthState::NONE.set();


    // Final Pass through process
    // Debug (kinda broken, need swap chain). This should also not be reading from same FBO it writes to...
    if (mPassthroughRenderMode > 1) {
        const Material* postMat = mPassthroughMaterials[mPassthroughRenderMode];
        assert(postMat);

        // TODO: Swap chain for this to work
        mMaterialRenderer->renderFullScreenQuad(*postMat);
    }

    // UI last
    renderUI(camera);

    // Debugging
    if (sDebugOptions.mShowTweaker) {
        mDebugTweakerPanel->updateAndRender();
    }

    // Swap
    mPrevGBufferIndex = mActiveGBufferIndex;
    mActiveGBufferIndex = !mActiveGBufferIndex;

    checkGlError("RenderContext::FrameEnd");
}

void RenderContext::endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGui::EndFrame();
}

void RenderContext::selectNextDebugShader() {
    //mChunkRenderer->SelectNextShader();
    ++mPassthroughRenderMode;
    if (mPassthroughRenderMode >= mPassthroughMaterials.size()) {
        mPassthroughRenderMode = 0;
    }
}

void RenderContext::renderUI(const Camera3D& camera) {
    mSb->begin();
    char buffer[255];
    f32 scales = 1.0f;
    const float GAP_SIZE = 35.0f * scales;
    const float START_MULT = 0.75f;
    float yOffset = 0.0f;
    const f32v2 scale(scales);

    sprintf_s(buffer, sizeof(buffer), "FPS: %.0f", sFps);
    mSb->drawString(mSpriteFont.get(), buffer, f32v2(0.0f, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
    yOffset += GAP_SIZE;

    sprintf_s(buffer, sizeof(buffer), "ZoomScale: %.2f", camera.getScale());
    mSb->drawString(mSpriteFont.get(), buffer, f32v2(0.0f, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
    yOffset += GAP_SIZE;

    sprintf_s(buffer, sizeof(buffer), "Jobs: %d", (int)Services::Threadpool::ref().getTasksSizeApprox());
    mSb->drawString(mSpriteFont.get(), buffer, f32v2(0.0f, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
    yOffset += GAP_SIZE;

    sprintf_s(buffer, sizeof(buffer), "MainQueue: %d", (int)Services::Threadpool::ref().getMainThreadQueuedProcsApprox());
    mSb->drawString(mSpriteFont.get(), buffer, f32v2(0.0f, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
    yOffset += GAP_SIZE;

    sprintf_s(buffer, sizeof(buffer), "DrawCalls: %u", RenderStats::sDrawCalls);
    mSb->drawString(mSpriteFont.get(), buffer, f32v2(0.0f, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
    yOffset += GAP_SIZE;

    sprintf_s(buffer, sizeof(buffer), "Polygons: %u", RenderStats::sPolyCount);
    mSb->drawString(mSpriteFont.get(), buffer, f32v2(0.0f, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
    yOffset += GAP_SIZE;

    /*sprintf_s(buffer, sizeof(buffer), "SunHeight: %.2f", mWorld.getSunHeight());
    mSb->drawString(mSpriteFont.get(), buffer, f32v2(0.0f, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
    yOffset += GAP_SIZE;

    sprintf_s(buffer, sizeof(buffer), "SunPosition: %.2f", mWorld.getSunPosition());
    mSb->drawString(mSpriteFont.get(), buffer, f32v2(0.0f, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
    yOffset += GAP_SIZE;*/

    if (mPassthroughRenderMode != 0) {
        sprintf_s(buffer, sizeof(buffer), "DEBUG FBO: %s", sPassthroughMaterialNames[mPassthroughRenderMode].c_str());
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(0.0f, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;
    }

    mSb->end();
    mSb->render(mScreenResolution);
}

void RenderContext::buildHorizonMesh()
{
    mHorizonQuad = std::make_unique<QuadMesh>();
    TileVertex verts[4];
    constexpr float QUAD_WIDTH = 140000.0f;
    constexpr float Z_POS = -6.0f;
    const color3 waterColor3 = TileRepository::getTileData("water").spriteData.lodColor;
    const color4 waterColor(waterColor3.r, waterColor3.g, waterColor3.b, 255u);

    { // Bottom Left
        TileVertex& vbl = verts[0];
        vbl.pos.x = -QUAD_WIDTH + WorldData::WORLD_CENTER.x;
        vbl.pos.y = -QUAD_WIDTH + WorldData::WORLD_CENTER.y;
        vbl.pos.z = Z_POS;
        vbl.uvs.x = 0.0f;
        vbl.uvs.y = 0.0f;
        vbl.color = waterColor;
    }
    { // Bottom Right
        TileVertex& vbr = verts[1];
        vbr.pos.x = QUAD_WIDTH + WorldData::WORLD_CENTER.x;
        vbr.pos.y = -QUAD_WIDTH + WorldData::WORLD_CENTER.y;
        vbr.pos.z = Z_POS;
        vbr.uvs.x = 0.0f;
        vbr.uvs.y = 0.0f;
        vbr.color = waterColor;
    }
    { // Top Left
        TileVertex& vtl = verts[2];
        vtl.pos.x = -QUAD_WIDTH + WorldData::WORLD_CENTER.x;
        vtl.pos.y = QUAD_WIDTH + WorldData::WORLD_CENTER.y;
        vtl.pos.z = Z_POS;
        vtl.uvs.x = 0.0f;
        vtl.uvs.y = 0.0f;
        vtl.color = waterColor;
    }
    { // Top Right
        TileVertex& vtr = verts[3];
        vtr.pos.x = QUAD_WIDTH + WorldData::WORLD_CENTER.x;
        vtr.pos.y = QUAD_WIDTH + WorldData::WORLD_CENTER.y;
        vtr.pos.z = Z_POS;
        vtr.uvs.x = 0.0f;
        vtr.uvs.y = 0.0f;
        vtr.color = waterColor;
    }
    mHorizonQuad->setData(verts, 4, MeshDrawMode::STATIC);
}
