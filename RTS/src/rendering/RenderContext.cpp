#include "stdafx.h"
#include "RenderContext.h"
#include "ResourceManager.h"
#include "TextureAtlas.h"
#include "World.h"
#include "world/HeightmapTerrainQuadtree.h"
#include "world/TileRepository.h"
#include "pathfinding/NavGraph.h"
#include "pathfinding/NavThread.h"

#include "services/Services.h"

#include "DebugRenderer.h"
#include "EntityComponentSystemRenderer.h"
#include "rendering/BuildingRenderer.h"
#include "rendering/CharacterRenderer.h"
#include "rendering/ChunkRenderer.h"
#include "rendering/ChunkGrassQuadtree.h"
#include "rendering/CityDebugRenderer.h"
#include "rendering/CloudRenderer.h"
#include "rendering/post_process/AmbientOcclusionPostProcess.h"
#include "rendering/post_process/DepthOfFieldPostProcess.h"
#include "rendering/ItemRenderer.h"
#include "rendering/LightRenderer.h"
#include "rendering/MaterialManager.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/ParticleSystemRenderer.h"
#include "rendering/QuadMesh.h"
#include "rendering/mesh/TerrainMesh.h"
#include "rendering/Skybox.h"
#include "rendering/post_process/ShadowRenderer.h"
#include "rendering/RenderStats.h"
#include "rendering/TerrainRenderer.h"
#include "rendering/MaterialUtils.h"
#include "TextureManip.h"

#include "ui/UIContext.h"

#include "editor/WorldEditor.h"

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

// Opengl debugging
void APIENTRY glDebugOutput(GLenum source,
    GLenum type,
    unsigned int id,
    GLenum severity,
    GLsizei length,
    const char* message,
    const void* userParam) {
    // ignore non-significant error/warning codes
    // 131218 - performance - recompiling shader... hmmm
    if (/*id == 131169 || */id == 131185 || id == 131218 /*|| id == 131204*/) return;

    std::cout << "---------------" << std::endl;
    std::cout << "Debug message (" << id << " - " << std::hex << "0x" << id << "): " << message << std::endl;

    switch (source)
    {
        case GL_DEBUG_SOURCE_API:             std::cout << "Source: API"; break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   std::cout << "Source: Window System"; break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: std::cout << "Source: Shader Compiler"; break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:     std::cout << "Source: Third Party"; break;
        case GL_DEBUG_SOURCE_APPLICATION:     std::cout << "Source: Application"; break;
        case GL_DEBUG_SOURCE_OTHER:           std::cout << "Source: Other"; break;
    } std::cout << std::endl;

    switch (type)
    {
        case GL_DEBUG_TYPE_ERROR:               std::cout << "Type: Error"; break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: std::cout << "Type: Deprecated Behaviour"; break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  std::cout << "Type: Undefined Behaviour"; break;
        case GL_DEBUG_TYPE_PORTABILITY:         std::cout << "Type: Portability"; break;
        case GL_DEBUG_TYPE_PERFORMANCE:         std::cout << "Type: Performance"; break;
        case GL_DEBUG_TYPE_MARKER:              std::cout << "Type: Marker"; break;
        case GL_DEBUG_TYPE_PUSH_GROUP:          std::cout << "Type: Push Group"; break;
        case GL_DEBUG_TYPE_POP_GROUP:           std::cout << "Type: Pop Group"; break;
        case GL_DEBUG_TYPE_OTHER:               std::cout << "Type: Other"; break;
    } std::cout << std::endl;

    switch (severity)
    {
        case GL_DEBUG_SEVERITY_HIGH:         std::cout << "Severity: high"; break;
        case GL_DEBUG_SEVERITY_MEDIUM:       std::cout << "Severity: medium"; break;
        case GL_DEBUG_SEVERITY_LOW:          std::cout << "Severity: low"; break;
        case GL_DEBUG_SEVERITY_NOTIFICATION: std::cout << "Severity: notification"; break;
    } std::cout << std::endl;
    std::cout << std::endl;
}

constexpr ui32 CAMERA_MATRICES_BYTE_SIZE = sizeof(f32m4) * 6 /*camera matrices*/;

// TODO: Render a string to the screen for these, Debug Render: %s (gone for pass_through)
// TODO: Instead of single shader these should be able to be shader chains.
const std::string sPassthroughMaterialNames[] = {
    "pass_through",
    "depth_debug",
    //"motion_blur",
    "normals",
    "shadow_depth_debug",
    "roughness_debug",
};

RenderContext* RenderContext::sInstance = nullptr;

RenderContext::RenderContext(ResourceManager& resourceManager, const World& world, const f32v2& screenResolution, SDL_Window* window) :
    mResourceManager(resourceManager),
    mWorld(world),
    mScreenResolution(screenResolution),
    mWindow(window)
{
    // If we are in a debug context, initialize debug output
    // This is set via vui::MainGame::initSystems()
    {
        int flags;
        glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
        if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
            std::cout << "DEBUG CONTEXT DETECTED. INITIALIZING OUTPUT\n";
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(glDebugOutput, nullptr);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        }
    }

    // Mesh init
    MeshBase::initStaticIBO();
    TerrainMesh::initGlobalIBO();
    WaterMesh::initGlobalIBO();
    checkGlError("Meshbase init");

    // int UI resources
    mSb         = std::make_unique<vg::SpriteBatch>();
    mSpriteFont = std::make_unique<vg::SpriteFont>();
    mSb->init();
    mSpriteFont->init("data/fonts/chintzy.ttf", 32);
    checkGlError("SB init");

    sGlobalFullQuadVBO.init();

    // GBuffer
    vg::GBufferAttachment attachments[3];
    // Color
    attachments[FBO_GEOMETRY_COLOR].format = vg::TextureInternalFormat::RGB16F;
    attachments[FBO_GEOMETRY_COLOR].number = FBO_GEOMETRY_COLOR;
    attachments[FBO_GEOMETRY_COLOR].pixelFormat = vg::TextureFormat::RGB;
    attachments[FBO_GEOMETRY_COLOR].pixelType = vg::TexturePixelType::UNSIGNED_BYTE;
    // Normals
    attachments[FBO_GEOMETRY_NORMAL].format = vg::TextureInternalFormat::RGB8;
    attachments[FBO_GEOMETRY_NORMAL].number = FBO_GEOMETRY_NORMAL;
    attachments[FBO_GEOMETRY_NORMAL].pixelFormat = vg::TextureFormat::RGB;
    attachments[FBO_GEOMETRY_NORMAL].pixelType = vg::TexturePixelType::UNSIGNED_BYTE;
    // Normals
    attachments[FBO_GEOMETRY_ROUGHNESS].format = vg::TextureInternalFormat::R8;
    attachments[FBO_GEOMETRY_ROUGHNESS].number = FBO_GEOMETRY_ROUGHNESS;
    attachments[FBO_GEOMETRY_ROUGHNESS].pixelFormat = vg::TextureFormat::RED;
    attachments[FBO_GEOMETRY_ROUGHNESS].pixelType = vg::TexturePixelType::UNSIGNED_BYTE;
    // TODO: Third doesnt need a unique depth texture
    for (int i = 0; i < 2; ++i) {
        mGBuffers[i].setSize(ui32v2(mScreenResolution));
        mGBuffers[i].init(attachments[FBO_GEOMETRY_COLOR], &attachments[FBO_GEOMETRY_NORMAL], &attachments[FBO_GEOMETRY_ROUGHNESS]);
        if (i != 2) {
            mGBuffers[i].initDepth(vg::TextureInternalFormat::DEPTH_COMPONENT32);
        }
    }
    mTransparencyGBuffer.setSize(ui32v2(mScreenResolution));
    mTransparencyGBuffer.init(attachments[FBO_GEOMETRY_COLOR], nullptr, nullptr);

    checkGlError("GBuffer init");

    int maxTextureSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    if (maxTextureSize < TEXTURE_ATLAS_WIDTH_PX) {
        pError("GFX card does not support 4k textures :(");
        assert(false);
    }

    // UBO
    glGenBuffers(1, &mGlobalUbo);
    glBindBuffer(GL_UNIFORM_BUFFER, mGlobalUbo);
    glBufferData(GL_UNIFORM_BUFFER, CAMERA_MATRICES_BYTE_SIZE + sizeof(GlobalUboData), NULL, GL_STATIC_DRAW); // allocate 152 bytes of memory
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, mGlobalUbo);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

}

RenderContext::~RenderContext() {
    glDeleteBuffers(1, &mGlobalUbo);
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

    mMaterialRenderer = std::make_unique<MaterialRenderer>(*this);

    // TODO: These can be eliminated and put into constructor???
    {
        ScopedTimer timer("Finish atlas normals and mips", 2);
        mTextureManipulator = std::make_unique<GPUTextureManipulator>(mResourceManager, *mMaterialRenderer);
        mTextureManipulator->InitPostLoad();
        checkGlError("Init texture manipulator");
    }

    // Initialize renderer after material assets are loaded
    {
        // Init renderers
        ScopedTimer timer("renderer allocations", 2);
        mCharacterRenderer = std::make_unique<CharacterRenderer>(mResourceManager.getMaterialManager(), mResourceManager.getModelRepository());
        mChunkRenderer = std::make_unique<ChunkRenderer>(mWorld.getWorldGrid(), mResourceManager, *mMaterialRenderer);
        mLightRenderer = std::make_unique<LightRenderer>(mResourceManager, *mMaterialRenderer);
        mEcsRenderer = std::make_unique<EntityComponentSystemRenderer>(mResourceManager, mWorld);
        mParticleSystemRenderer = std::make_unique<ParticleSystemRenderer>(mResourceManager, *mMaterialRenderer, mScreenResolution);
        mCityDebugRenderer = std::make_unique<CityDebugRenderer>();
        mItemRenderer = std::make_unique<ItemRenderer>(mWorld.getWorldGrid(), mResourceManager, *mMaterialRenderer);
        mBuildingRenderer = std::make_unique<BuildingRenderer>(mResourceManager, *mMaterialRenderer);
        mCloudRenderer = std::make_unique<CloudRenderer>(mResourceManager, *mMaterialRenderer, mScreenResolution);
        mDepthOfField = std::make_unique<DepthOfFieldPostProcess>(mResourceManager, *mMaterialRenderer, mScreenResolution);
        mAmbientOcclusion = std::make_unique<AmbientOcclusionPostProcess>(mResourceManager, *mMaterialRenderer, mScreenResolution);
        mShadowRenderer = std::make_unique<ShadowRenderer>(mResourceManager, *mMaterialRenderer, mScreenResolution);
        mTerrainRenderer = std::make_unique<TerrainRenderer>(mResourceManager, *mMaterialRenderer);
        checkGlError("Renderer init");
    }

    // Init all passthrough materials
    {
        ScopedTimer timer("Passthrough init", 2);
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

    {
        ScopedTimer timer("Chunk renderer init", 2);
        mChunkRenderer->InitPostLoad();
        mLightRenderer->InitPostLoad();
    }

    mSceneLightingMaterial = mResourceManager.getMaterialManager().getMaterial("scene_lighting");
    mCopyDepthMaterial = mResourceManager.getMaterialManager().getMaterial("copy_depth");
    mPassthroughMaterial = mResourceManager.getMaterialManager().getMaterial("pass_through");

    {
        
        ScopedTimer timer("Skybox init", 2);
        buildHorizonMesh();
        mSkyBox = std::make_unique<Skybox>();
        mSkyBox->init(mResourceManager.getMaterialManager().getMaterial("sky"));
    }

}

void RenderContext::beginFrame(const Camera3D* camera, f32v3 playerPos) {
    GlobalUboData& uboData = mRenderData.globalUboData;
    RenderStats::clear();
    // Misc renderData
    mRenderData.mainCamera = camera;
    mRenderData.atlas = mResourceManager.getTextureAtlas().getAtlasTexture();
    mRenderData.cameraZAngle = camera->getZAngle();
    mRenderData.skyRotMatrix = mWorld.getSkyRotMatrix();
    // Ubo data
    uboData.Time = sTotalTimeSeconds;
    uboData.TimeOfDay = mWorld.getTimeOfDay();
    uboData.PlayerPosWorld = playerPos;

    // Sun
    const f32v3& sun = mWorld.getSunPosition();
    mShadowRenderer->beginFrame(*camera, sun);

    const f32v3 lastSunPosition = mShadowRenderer->getLastUpdatedSunPosition();
    uboData.SunColor = mWorld.getSunColor();
    uboData.SunHeight = mWorld.getSunHeight();
    uboData.SunPosition = lastSunPosition;
    uboData.SunPositionCameraRelative = glm::normalize(f32v3(camera->getViewMatrix() * f32v4(lastSunPosition.x, lastSunPosition.y, lastSunPosition.z, 1.0f)));
    uboData.SunRight = glm::normalize(glm::cross(lastSunPosition, f32v3(0.0f, 0.0f, 1.0f)));
    uboData.SunUp = glm::normalize(glm::cross(lastSunPosition, uboData.SunRight));

    // Camera data
    uboData.CameraPos = camera->getPosition();
    uboData.CameraFront = camera->getFrontVector();
    uboData.CameraRight = camera->getRightVector();
    uboData.CameraUp = camera->getUpVector();
    uboData.CameraZRange = f32v2(camera->getZNear(), camera->getZFar());

    // Shadows
    mRenderData.shadowFrustumMatrices = mShadowRenderer->getShadowFrustumMatrices();
    mRenderData.shadowCascadePlaneDistances = mShadowRenderer->getShadowCascadePlaneDistances();
    mRenderData.shadowMap = mShadowRenderer->getShadowMap();
    mRenderData.shadowFrustumMatricesCount = MAX_SHADOW_CASCADE_LEVELS;

    // Update ubo
    glBindBuffer(GL_UNIFORM_BUFFER, mGlobalUbo);
    // Camera matrices
    glBufferSubData(GL_UNIFORM_BUFFER, 0, CAMERA_MATRICES_BYTE_SIZE, &camera->getViewMatrix()[0][0]);
    // Rest of the UBO
    glBufferSubData(GL_UNIFORM_BUFFER, CAMERA_MATRICES_BYTE_SIZE, sizeof(GlobalUboData), &uboData);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(mWindow);
    ImGui::NewFrame();

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
}

void RenderContext::renderFrame(const Camera3D& camera, f32v3 playerPos, f32 frameAlpha, f32 elapsedSec) {

    // TODO: Map texels to pixels?
    if (camera.getScale() < 1.5f) {
        // TODO: Remove for 2D
        //lodState = ChunkRenderLOD::LOD_TEXTURE;
    }

    // TODO: Should this happen here? Maybe assert instead?
    beginFrame(&camera, playerPos);
    checkGlError("RenderContext::Begin Frame");
    
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
        // TODO: Can we not do GL_COLOR_BUFFER_BIT? (IT causes clouds issues rn)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    // TODO: Replace With BlendState
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Tiles
    mChunkRenderer->renderTiles(mWorld, camera);

    //mEcsRenderer->renderSimpleSprites(camera);
    mEcsRenderer->renderInteractUI(camera);

    // Render stockpiles
    for (auto&& stockPilePtr : mWorld.getItemStockpileRegistry().getAllStockpiles()) {
        if (stockPilePtr->isVisible()) {
            mItemRenderer->renderStockpile(*stockPilePtr, camera);
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

    // Ambient occlusion
    mAmbientOcclusion->render(mActiveGBuffer);

    // === Post AO passes ===
    // Grass + billboards
    mChunkRenderer->renderBillboards(mWorld, camera);
    if (!sDebugOptions.mHideGrass) {
        mChunkRenderer->renderGrass(mWorld, camera, playerPos);
    }

    // Terrain
    if (!sDebugOptions.mDisableTerrain) {
        mTerrainRenderer->renderTerrain(camera, mWorld.getTerrainQuadtrees());
    }

    if (!sDebugOptions.mHideCharacters) {
        mEcsRenderer->renderCharacterModels(*mCharacterRenderer, *mMaterialRenderer, camera, frameAlpha, elapsedSec);
    }
    if (sDebugOptions.mShowPhysicsDebug) {
        mEcsRenderer->renderPhysicsDebug(camera);
    }
    if (sDebugOptions.mShowBusinessDebug) {
        mEcsRenderer->renderBusinessDebug(camera);
    }

    // Clouds
   /* if (!sDebugOptions.mDisableClouds) {
        mCloudRenderer->renderClouds(mWorld.getCloudManager(), mActiveGBuffer, camera);
    }*/

    // Editor brushes
    UIContext::getInstance().renderEditorBrushDecals(camera);

    // Sky
    glEnable(GL_DEPTH_CLAMP);
    glDisable(GL_CULL_FACE); // TODO: Fix geometry so we dont have to disable cull face
    mSkyBox->render(*mMaterialRenderer);
    glEnable(GL_CULL_FACE);
    glDisable(GL_DEPTH_CLAMP);

    // Horizon
    //mMaterialRenderer->renderMesh(*mHorizonQuad, *mResourceManager.getMaterialManager().getMaterial("simple_color"));

    // Shadows
    if (mRenderData.globalUboData.SunHeight > 0.01f && !sDebugOptions.mDisableShadows) {
        if (mShadowRenderer->shouldUpdateShadowsThisFrame()) {
            mShadowRenderer->useShadowBuffer();
            glEnable(GL_DEPTH_CLAMP);

            vg::DepthState::FULL.set();
            // Render all shadow casters
            //glCullFace(GL_FRONT);
            mChunkRenderer->renderWorldShadows(mWorld, camera, mShadowRenderer->getMaxDistance());

            //glCullFace(GL_BACK);
            // TODO: Frustum cull
            if (!sDebugOptions.mDisableClouds) {
                mCloudRenderer->renderCloudShadows(mWorld.getCloudManager(), camera, mShadowRenderer->getMaxDistance());
            }

            const CityGraph& cities = mWorld.getCities();
            for (auto&& city : cities.mNodes) {
                const std::vector<Building>& buildings = city->getBuildings();
                for (auto& building : buildings) {
                    mBuildingRenderer->renderBuildingRoofShadows(building);
                }
            }


            glDisable(GL_DEPTH_CLAMP);
        }

        vg::DepthState::NONE.set();
        mActiveGBuffer = mShadowRenderer->renderShadows(mActiveGBuffer, camera.getPosition());

        mActiveGBuffer->useGeometry();
    }
    else {
        // No shadow bleed from previous frames
        mShadowRenderer->clearShadowTexture(mActiveGBuffer);
    }

    // Particles
    //if (lodState == ChunkRenderLOD::FULL_DETAIL) {
    //    vg::DepthState::READ.set();
    //    // TODO: Replace With BlendState
    //    mParticleSystemRenderer->renderParticleSystems(camera, mActiveGBuffer, true);
    //    vg::BlendState::set(vorb::graphics::BlendStateType::ALPHA);
    //    vg::DepthState::FULL.set();
    //}

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


    // *** Post processes ***

    // Depth of field
    vg::DepthState::NONE.set();

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


    //// Disable depth testing for post processing
    //// TODO: Swap chains?
    //// TODO: This should be at top
    //mActiveGBuffer->unuse();
    //mCurrentFramebufferDims = mScreenResolution;
    //// *** Lighting ***
    //mActiveGBuffer->useLight();
    //glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    //glClear(GL_COLOR_BUFFER_BIT);

    //// Sun Light
    //// TODO: Collapse this into lightPassThrough?
    //mMaterialRenderer->renderFullScreenQuad(*mSunLightMaterial);

    ////  Dynamic  light
    //glBlendFunc(GL_ONE, GL_ONE);
    //mEcsRenderer->renderDynamicLightComponents(camera, *mLightRenderer);


    mActiveGBuffer->unuse();
    mCurrentFramebufferDims = mScreenResolution;
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    vg::DepthState::NONE.set();

    // Final render for pre-transparency
    mTransparencyGBuffer.useGeometry();
    // Share values
    mTransparencyGBuffer.setRoughnessTexture(mActiveGBuffer->getRoughnessTexture());
    mTransparencyGBuffer.setNormalTexture(mActiveGBuffer->getNormalTexture());
    mTransparencyGBuffer.setDepthTexture(mActiveGBuffer->getDepthTexture());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mActiveGBuffer->getDepthTexture(), 0);

    // Final Lighting
    mMaterialRenderer->bindMaterialForRender(*mSceneLightingMaterial);
    MaterialUtils::uploadLightingUniforms(*mSceneLightingMaterial);
    sGlobalFullQuadVBO.draw();

    // Render clouds without shadows
    if (!sDebugOptions.mDisableClouds) {
        mCloudRenderer->renderClouds(mWorld.getCloudManager(), &mTransparencyGBuffer, camera);
    }

    // === Transparency ===
    // Water (No depth write)
    if (!sDebugOptions.mDisableWater) {
        mTerrainRenderer->renderWater(camera, mWorld.getTerrainQuadtrees());
    }

    // Update active
    mActiveGBuffer = &mTransparencyGBuffer;

    // Depth of field
    vg::DepthState::NONE.set();
    mActiveGBuffer = mDepthOfField->render(mActiveGBuffer);

    // Final render to screen
    mActiveGBuffer->unuse();
    mMaterialRenderer->renderFullScreenQuad(*mPassthroughMaterial);

   
    // Final Pass through process
    // Debug (kinda broken, need swap chain). This should also not be reading from same FBO it writes to...
    if (mPassthroughRenderMode > 1) {
        const Material* postMat = mPassthroughMaterials[mPassthroughRenderMode];
        assert(postMat);

        // TODO: Swap chain for this to work
        mMaterialRenderer->renderFullScreenQuad(*postMat);
    }

    // Debug rendering
    renderDebug(camera);

    // UI last
    renderUI(camera);

    // Debugging
    UIContext::getInstance().updateAndRenderUI(mWorld.getECS(), mActiveGBuffer, camera.getAspectRatio());

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

VGTexture RenderContext::getShadowTexture() const {
    return mShadowRenderer->getShadowTexture();
}

VGTexture RenderContext::getSSAOTexture() const {
    return mAmbientOcclusion->getSSAOTexture();
}

void RenderContext::renderDebug(const Camera3D& camera) {
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
            color4 color = COLOR_WHITE;
            switch (chunk.getState()) {
                case ChunkState::INVALID:
                    color = color4(1.0f, 0.0f, 0.0f);
                    break;
                case ChunkState::WAITING_HEIGHT:
                    color = color4(0.0f, 0.0f, 0.0f);
                    break;
                case ChunkState::LOADING_TILES:
                    color = color4(0.0f, 1.0f, 1.0f);
                    break;
                case ChunkState::FINISHED:
                    color = color4(0.0f, 1.0f, 0.0f);
                    break;
                default:
                    break;
            }

            DebugRenderer::drawWireQuad(chunk.getWorldPos(), f32v2(CHUNK_WIDTH), color);
            if (chunk.isDataReady()) {
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
            // Count refs
            const int refCount = chunk.mRefCount.load();
            const int readCount = chunk.mReadLockCount.load();
            constexpr f32 REF_BOX_WIDTH = 1.0f;
            constexpr ui32 REF_ROW_WIDTH = (CHUNK_WIDTH - 1) / REF_BOX_WIDTH;
            for (int i = 0; i < refCount; ++i) {
                DebugRenderer::drawWireQuad(chunk.getWorldPos() + f32v2(REF_BOX_WIDTH) + f32v2(i % REF_ROW_WIDTH, (i / REF_ROW_WIDTH) * 2) * REF_BOX_WIDTH, f32v2(REF_BOX_WIDTH), color4(1.0f, 0.0f, 1.0f));
            }
            for (int i = 0; i < readCount; ++i) {
                DebugRenderer::drawWireQuad(chunk.getWorldPos() + f32v2(REF_BOX_WIDTH, REF_BOX_WIDTH * 2.0f) + f32v2(i % REF_ROW_WIDTH, (i / REF_ROW_WIDTH) * 2) * REF_BOX_WIDTH, f32v2(REF_BOX_WIDTH), color4(0.0f, 1.0f, 1.0f));
            }
        });
    }

    // Grass LOD debug
    if (sDebugOptions.mDebugGrassLod) {
        mWorld.enumVisibleChunks([&camera](const Chunk& chunk) {
            if (chunk.isDataReady()) {

                if (chunk.mChunkRenderData.mGrassLod) {
                    chunk.mChunkRenderData.mGrassLod->renderDebug(camera);
                }
            }
        });
    }
    // Nav graph (Render is slow so we only build the line meshes when toggle changes)
    constexpr int NAVGRAPH_ID = 44432;
    static bool wasRenderingNavGraph = false;
    if (sDebugOptions.mShowNavGraph) {
        if (!wasRenderingNavGraph) {
            ScopedTimer timer("Debug Draw Navgraph");
            DebugRenderer::reserveLines(mWorld.getNumActiveChunks() * 1024, MAX_DEBUG_RENDER_LIFETIME, NAVGRAPH_ID);
            mWorld.enumActiveChunks([&camera, this, NAVGRAPH_ID](const Chunk& chunk) {
                if (chunk.isDataReady() && !chunk.mIsNavmeshing) {
                    mWorld.getNavGraph().debugDrawNavGraphForChunk(chunk, MAX_DEBUG_RENDER_LIFETIME, NAVGRAPH_ID);
                }
            });
            wasRenderingNavGraph = true;
        }
    }
    else if (wasRenderingNavGraph) {
        wasRenderingNavGraph = 0;
        DebugRenderer::clearAllMeshesWithId(NAVGRAPH_ID);
    }

    // Terrain LOD debug
    if (sDebugOptions.mDebugTerrainLod) {
        for (auto&& terrainQuadtree : mWorld.getTerrainQuadtrees()) {
            terrainQuadtree.renderDebug(camera);
        }
    }

    // Debug
    DebugRenderer::render(camera.getPosition(), camera.getVPMatrix());
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

    sprintf_s(buffer, sizeof(buffer), "Jobs: %d", (int)Services::Threadpool::ref().getTasksSizeApprox());
    mSb->drawString(mSpriteFont.get(), buffer, f32v2(0.0f, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
    yOffset += GAP_SIZE;

    sprintf_s(buffer, sizeof(buffer), "MainQueue: %d", (int)Services::Threadpool::ref().getMainThreadQueuedProcsApprox() + Services::NavThread::ref().getMainThreadQueuedProcsApprox());
    mSb->drawString(mSpriteFont.get(), buffer, f32v2(0.0f, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
    yOffset += GAP_SIZE;

    sprintf_s(buffer, sizeof(buffer), "NavQueue: %d", (int)Services::NavThread::ref().getTasksSizeApprox());
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
    const color4 waterColor(0, 0, 255, 255);

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
