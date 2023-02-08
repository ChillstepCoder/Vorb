#include "stdafx.h"
#include "RenderContext.h"
#include "resources/ResourceManager.h"
#include "resources/MaterialRepository.h"
#include "resources/TextureRepository.h"
#include "world/IWorld.h"
#include "world/cli/CliWorldInterface.h"
#include "world/HeightmapTerrainQuadtree.h"
#include "resources/TileRepository.h"
#include "pathfinding/NavWorld.h"
#include "pathfinding/NavThread.h"

#include "debugging/DebugRenderer.h"
#include "debugging/VisualLogger.h"
#include "EntityComponentSystemRenderer.h"
#include "rendering/CharacterRenderer.h"
#include "rendering/renderer/GrassRenderer.h"
#include "rendering/TileContainerRenderer.h"
#include "rendering/ChunkGrassQuadtree.h"
#include "rendering/CityDebugRenderer.h"
#include "rendering/CloudRenderer.h"
#include "rendering/post_process/AmbientOcclusionPostProcess.h"
#include "rendering/post_process/DepthOfFieldPostProcess.h"
#include "rendering/ItemRenderer.h"
#include "rendering/LightRenderer.h"
#include "rendering/MaterialShaderManager.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/ParticleSystemRenderer.h"
#include "rendering/Skybox.h"
#include "rendering/post_process/ShadowRenderer.h"
#include "rendering/post_process/SmudgeRenderer.h"
#include "rendering/post_process/TonemapRenderer.h"
#include "rendering/RenderStats.h"
#include "rendering/TerrainRenderer.h"
#include "rendering/material/BrdfLUT.h"
#include "rendering/MaterialUtils.h"
#include "rendering/RenderThreadTasks.h"
#include "rendering/mesh/mesher/builder/ProceduralMeshBuilder.h"
#include "rendering/mesh/mesher/builder/TerrainMeshBuilder.h"
#include "rendering/renderstate/RenderStateManager.h"
#include "rendering/model/InstancedStaticModelRenderer.h"
#include "rendering/StencilBufferIDs.h"
#include "weather/CloudManager.h"

#include "gamethread/GameThreadTasks.h"

#include "screens/ScreenState.h"
#include "network/srv/GameServer.h"

#include "structure/StructureManager.h"

#include "ui/UIContext.h"

#include "editor/WorldEditorPanel.h"

// TODO: Move to renderer?
#include "city/CityQuartermaster.h"
#include "item/ItemStockpileRegistry.h"

#include "camera/Camera3D.h"
#include "camera/CameraController.h"
#include "physics/PhysicsWorld.h"

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

#include <Vorb/io/IOManager.h>

#include "options/DebugOptions.h"

#define USE_STENCIL 1

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
    if (/*id == 131169 || */id == 131185 || id == 131218 || id == 131204) return;

    std::stringstream ss;

    ss << "---------------" << std::endl;
    ss << "Debug message (" << id << " - " << std::hex << "0x" << id << "): " << message << std::endl;

    switch (source)
    {
        case GL_DEBUG_SOURCE_API:             ss << "Source: API"; break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   ss << "Source: Window System"; break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: ss << "Source: Shader Compiler"; break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:     ss << "Source: Third Party"; break;
        case GL_DEBUG_SOURCE_APPLICATION:     ss << "Source: Application"; break;
        case GL_DEBUG_SOURCE_OTHER:           ss << "Source: Other"; break;
    } ss << std::endl;

    switch (type)
    {
        case GL_DEBUG_TYPE_ERROR:               ss << "Type: Error"; break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: ss << "Type: Deprecated Behaviour"; break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  ss << "Type: Undefined Behaviour"; break;
        case GL_DEBUG_TYPE_PORTABILITY:         ss << "Type: Portability"; break;
        case GL_DEBUG_TYPE_PERFORMANCE:         ss << "Type: Performance"; break;
        case GL_DEBUG_TYPE_MARKER:              ss << "Type: Marker"; break;
        case GL_DEBUG_TYPE_PUSH_GROUP:          ss << "Type: Push Group"; break;
        case GL_DEBUG_TYPE_POP_GROUP:           ss << "Type: Pop Group"; break;
        case GL_DEBUG_TYPE_OTHER:               ss << "Type: Other"; break;
    } ss << std::endl;

    switch (severity)
    {
        case GL_DEBUG_SEVERITY_HIGH:         ss << "Severity: high"; break;
        case GL_DEBUG_SEVERITY_MEDIUM:       ss << "Severity: medium"; break;
        case GL_DEBUG_SEVERITY_LOW:          ss << "Severity: low"; break;
        case GL_DEBUG_SEVERITY_NOTIFICATION: ss << "Severity: notification"; break;
    } ss << std::endl;
    ss << std::endl;
    LOG_CRITICAL("{}", ss.str());
}

constexpr ui32 CAMERA_MATRICES_BYTE_SIZE = sizeof(f32m4) * 6 /*camera matrices*/;

// TODO: Instead of single shader these should be able to be shader chains.
const std::string sPassthroughMaterialNames[] = {
    "pass_through",
    "depth_debug",
    //"motion_blur",
    "normals",
    "shadow_depth_debug",
    "roughness_debug",
};

void RenderContext::addBillboardMesh(const Mesh* mesh) {
    assert(IS_RENDER_THREAD());
    assert(mBillboardMeshes.find(mesh) == mBillboardMeshes.end());
    mBillboardMeshes.insert(mesh);
    assert(mesh->isValid());
}

void RenderContext::removeBillboardMesh(const Mesh* mesh) {
    assert(IS_RENDER_THREAD());
    auto&& it = mBillboardMeshes.find(mesh);
    assert(it != mBillboardMeshes.end());
    mBillboardMeshes.erase(it);
}

RenderContext* RenderContext::sInstance = nullptr;

// TODO: Read http://iquilezles.org/articles/
RenderContext::RenderContext(const f32v2& screenResolution, SDL_Window* window) :
    mScreenResolution(screenResolution),
    mWindow(window)
{
    // State init
    RenderStateManager::initInstance();

    // Make sure we can filter cubemaps properly
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    initEventHandlers();

    // TODO: New depth - https://outerra.blogspot.com/2012/11/maximizing-depth-buffer-range-and.html

    // If we are in a debug context, initialize debug output
    // This is set via vui::MainGame::initSystems()
    {
        int flags;
        glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
        if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
            LOG_DEBUG("Debug gl context detected, initializing debug output");
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(glDebugOutput, nullptr);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        }
    }

    // Mesh init
    ProceduralMeshBuilder::initStaticIBOs();
    TerrainMeshBuilder::initStaticIBO();
    checkGlError("Meshbase init");

    // int UI resources
    mSb         = std::make_unique<vg::SpriteBatch>();
    mSpriteFont = std::make_unique<vg::SpriteFont>();
    mSb->init();

    vio::Path fontPath;
    if (!Services::ResourceManager::ref().getIoManager().resolvePath(vio::Path("data/fonts/titilium_semibold.ttf"), fontPath)) {
        pError("Unable to resolve titilium_semibold.ttf font path, try verifying game files");
    }
    mSpriteFont->init(fontPath.getCString(), 32);
    checkGlError("SB init");

    sGlobalFullQuadVBO.init();

    // GRAPHICS STUDIES
    // GTA5 https://www.adriancourreges.com/blog/2015/11/02/gta-v-graphics-study/
    // DOOM2016 https://www.adriancourreges.com/blog/2016/09/09/doom-2016-graphics-study/
    // Cyberpunk https://c0de517e.blogspot.com/2020/12/hallucinations-re-rendering-of.html
    // Metro exodus https://aschrein.github.io/2019/08/11/metro_breakdown.html
    // Others https://www.reddit.com/r/TheMakingOfGames/search?q=frame&restrict_sr=on
    // GBuffer
    for (int i = 0; i < 2; ++i) {
        mGBuffers[i] = std::make_unique<vg::GBuffer>(mScreenResolution);
        mGBuffers[i]->initAttachment(vg::GBufferAttachmentIndex::ALBEDO, vg::TextureInternalFormat::RGBA8); // Albedo + AO
        mGBuffers[i]->initAttachment(vg::GBufferAttachmentIndex::NORMALS, vg::TextureInternalFormat::RGB10_A2); // Normal
        mGBuffers[i]->initAttachment(vg::GBufferAttachmentIndex::TERTIARY, vg::TextureInternalFormat::RG8); // Roughness + Metallic
#if USE_STENCIL == 1
        mGBuffers[i]->initDepthStencil();
#else
        mGBuffers[i]->initDepth(vg::GBufferDepthFormat::DEPTH_32);
#endif
    }
    mHDRLightGBuffer = std::make_unique<vg::GBuffer>(mScreenResolution);
    mHDRLightGBuffer->initAttachment(vg::GBufferAttachmentIndex::ALBEDO, vg::TextureInternalFormat::RGB16F);

    checkGlError("GBuffer init");

    int maxTextureSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    if (maxTextureSize < 4096) {
        pError("GFX card does not support 4k textures :( please try updating drivers or refund the game");
        assert(false);
    }

    // UBO
    glCreateBuffers(1, &mGlobalUbo);
    glNamedBufferStorage(mGlobalUbo, CAMERA_MATRICES_BYTE_SIZE + sizeof(GlobalUboData), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_GLOBAL_UBO, mGlobalUbo);

    mCloudManager = std::make_unique<CloudManager>();

    // Improve depth precision (req for reverse depth buffer if we ever wanna do that)
    // https://www.danielecarbone.com/reverse-depth-buffer-in-opengl/
    glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
}

RenderContext::~RenderContext() {
    glDeleteBuffers(1, &mGlobalUbo);
}

RenderContext& RenderContext::initInstance(const f32v2& screenResolution, SDL_Window* window) {
    if (!sInstance) {
        sInstance = new RenderContext(screenResolution, window);
    }
    RenderThreadTasks::initInstance();
    return *sInstance;
}

RenderContext& RenderContext::getInstance() {
    assert(sInstance);
    return *sInstance;
}

void RenderContext::onWorldBegin(const f32v2& worldCenter) {
    // We require client interface to function
    mCliWorld = dynamic_cast<CliWorldInterface*>(sWorld);
    assert(mCliWorld);

    // Initialize renderer after material assets are loaded
    {
        // Init renderers
        ScopedTimer timer("renderer allocations", 2);
        mCharacterRenderer = std::make_unique<CharacterRenderer>();
        mTileContainerRenderer = std::make_unique<TileContainerRenderer>();
        mLightRenderer = std::make_unique<LightRenderer>();
        mEcsRenderer = std::make_unique<EntityComponentSystemRenderer>();
        mParticleSystemRenderer = std::make_unique<ParticleSystemRenderer>(mScreenResolution);
        mCityDebugRenderer = std::make_unique<CityDebugRenderer>();
        mItemRenderer = std::make_unique<ItemRenderer>();
        mCloudRenderer = std::make_unique<CloudRenderer>(mScreenResolution);
        mDepthOfField = std::make_unique<DepthOfFieldPostProcess>(mScreenResolution);
        mAmbientOcclusion = std::make_unique<AmbientOcclusionPostProcess>(mScreenResolution);
        mShadowRenderer = std::make_unique<ShadowRenderer>(mScreenResolution);
        mTerrainRenderer = std::make_unique<TerrainRenderer>();
        mGrassRenderer = std::make_unique<GrassRenderer>();
        mStaticModelRenderer = std::make_unique<InstancedStaticModelRenderer>();
        mSmudgeRenderer = std::make_unique<SmudgeRenderer>(mScreenResolution);
        mTonemapRenderer = std::make_unique<TonemapRenderer>();
        checkGlError("Renderer init");
    }

 
    mCloudManager->init(worldCenter);
}

void RenderContext::initPostLoad() {

    const ResourceManager& resourceManager = Services::ResourceManager::ref();
    const MaterialShaderManager& materialManager = resourceManager.getMaterialShaderManager();

    // Init all passthrough materials
    {
        ScopedTimer timer("Passthrough init", 2);
        for (int i = 0; i < std::size(sPassthroughMaterialNames); ++i) {
            const MaterialShader* material = materialManager.getMaterialShader(sPassthroughMaterialNames[i]);
            if (material) {
                mPassthroughMaterials.emplace_back(material);
            }
            else {
                pError("Missing material for pass through: " + std::string(sPassthroughMaterialNames[i]));
            }
        }
    }


    mSceneLightingMaterial = materialManager.getMaterialShader("scene_lighting");
    mCopyDepthMaterial = materialManager.getMaterialShader("copy_depth");
    mPassthroughMaterial = materialManager.getMaterialShader("pass_through");

    {
        ScopedTimer timer("Skybox init", 2);
        buildHorizonMesh();
        mSkyBox = std::make_unique<Skybox>();
        mSkyBox->init(materialManager.getMaterialShader("sky"), &resourceManager.getTextureRepository().getCubemap("graycloud"));
    }

    BrdfLUT::loadOrComputeTexture();

}

void RenderContext::beginFrame(const Camera3D* camera, f32v3 playerPos) {

    PROFILE_FUNCTION();


    mCamera = camera;
    // Update thread msg queue
    updateRenderThreadProcs();

    // Allow model renderer to build indirect buffers
    mStaticModelRenderer->frameUpdate(*camera);

    GlobalUboData& uboData = mRenderData.globalUboData;
    RenderStats::clear();
    // Misc renderData
    mRenderData.mainCamera = camera;
    mRenderData.cameraZAngle = camera->getZAngle();
    mRenderData.skyRotMatrix = sWorld->getSkyRotMatrix();
    // Ubo data
    uboData.Time = sTotalTimeSeconds;
    uboData.TimeOfDay = sWorld->getTimeOfDay();
    uboData.PlayerPosWorld = playerPos;

    // Sun
    const f32v3& sun = sWorld->getSunPosition();
    mShadowRenderer->beginFrame(*camera, sun);

    const f32v3 lastSunPosition = mShadowRenderer->getLastUpdatedSunPosition();
    uboData.SunColor = sWorld->getSunColor();
    uboData.SunHeight = sWorld->getSunHeight();
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
    // Camera matrices
    glNamedBufferSubData(mGlobalUbo, 0, CAMERA_MATRICES_BYTE_SIZE, &camera->getViewMatrix()[0][0]);
    // Rest of the UBO
    glNamedBufferSubData(mGlobalUbo, CAMERA_MATRICES_BYTE_SIZE, sizeof(GlobalUboData), &uboData);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(mWindow);
    ImGui::NewFrame();

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

}

void RenderContext::renderFrame(CameraController& cameraController, f32 frameAlpha, f32 elapsedSec) {
    PROFILE_FUNCTION();

    const RenderState& renderState = RenderStateManager::getInstance().getRenderStateForRender();

    // Update camera
    const f32v3& playerPos = renderState.getCameraOwningEntityPos();
    cameraController.update(1.0f /*TODO DELTATIME*/, frameAlpha, playerPos);
    const Camera3D& camera = cameraController.getOwnedCamera();

    beginFrame(&camera, playerPos);
    checkGlError("RenderContext::Begin Frame");

    // Update clouds
    mCloudManager->tick(renderState.getWorldLoadCenter());
    
    mActiveGBuffer = mGBuffers[mActiveGBufferIndex].get();

    // Main geometry pass
    mActiveGBuffer->use();
    mCurrentFramebufferDims = mActiveGBuffer->getSize();

    // Clear screen
    vg::DepthState::FULL.set();
    vg::BlendState::set(vg::BlendStateType::REPLACE);

    if (sDebugOptions.mWireframe) {
        glClearColor(0.5f, 0.5f, 0.5f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | (GL_STENCIL_BUFFER_BIT * USE_STENCIL));
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    else {
        // TODO: Can we not do GL_COLOR_BUFFER_BIT? (IT causes clouds issues rn)
        // TODO2: What issues? lol thanks for nothing previous self
        glClear(GL_DEPTH_BUFFER_BIT | (GL_STENCIL_BUFFER_BIT * USE_STENCIL));
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // Mark everything we draw as geometry
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, e_cast(StencilBufferIDs::GEOMETRY), 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    // Static meshes
    mTileContainerRenderer->renderStaticMeshes(mStaticMeshes, camera);

    if (!sDebugOptions.mHideCharacters) {
        mCharacterRenderer->renderCharacters(camera, renderState.getCharacterRenderState(), elapsedSec, frameAlpha);
    }

    // Instanced models
    Services::ResourceManager::ref().getMaterialRepository().bindMaterialBuffer();
    mStaticModelRenderer->renderModelPass(MaterialRenderPassType::Default, camera);

    // Smudge
    {
        mSmudgeRenderer->beginSmudgePass(mActiveGBuffer);
        mStaticModelRenderer->renderModelPass(MaterialRenderPassType::Smudge, camera);
        if (!sDebugOptions.mHideGrass && !sDebugOptions.mWireframe) {
            mGrassRenderer->renderGrass(camera, playerPos, mGrassMeshes);
        }
        mSmudgeRenderer->renderSmudge(mActiveGBuffer, camera);
    }

    // Render stockpiles
    for (auto&& stockPilePtr : sWorld->getItemStockpileRegistry().getAllStockpiles()) {
        if (stockPilePtr->isVisible()) {
            mItemRenderer->renderStockpile(*stockPilePtr, camera);
        }
    }

    // TODO: Render loose items


    // Ambient occlusion
    mAmbientOcclusion->render(mActiveGBuffer);

    // === Post AO passes ===
    // Grass + billboards

    mTileContainerRenderer->renderBillboards(mBillboardMeshes, camera);
    // PRE SMUDGE GRASS PASS
    /*if (!sDebugOptions.mHideGrass) {
        mGrassRenderer->renderGrass(camera, playerPos, mGrassMeshes);
    }*/
    // TODO: Where is this getting unset?
    glEnable(GL_CULL_FACE);

    {
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_ALWAYS, e_cast(StencilBufferIDs::GEOMETRY), 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        // Terrain
        if (!sDebugOptions.mDisableTerrain) {
            mTerrainRenderer->renderTerrain(camera, mTerrainMeshes);
        }
        glDisable(GL_STENCIL_TEST);
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

    // Horizon
    //mMaterialRenderer->renderMesh(*mHorizonQuad, *mResourceManager.getMaterialManager().getMaterial("simple_color"));


    renderPassShadows(camera, renderState);
    
    // TODO: Particles

    if (sDebugOptions.mWireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // *** Post processes ***
    // TODO: Bloom note (from acerola) https://www.youtube.com/watch?v=IMiiUEG-sLQ_
    // Contrast -> Brighness -> Saturation -> Gamma correction -> Bloom -> Bloom can be done via mipmapping (GPU DOWNSCALING then UPSCALING)

    vg::DepthState::NONE.set();

    // Render characters that are behind geometry with some transparency
    //mEcsRenderer->renderCharacterModels(*mCharacterRenderer, camera, 0.20f, frameAlpha);
        // Depth debug
    if (mPassthroughRenderMode == 1) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        const MaterialShader* postMat = mPassthroughMaterials[mPassthroughRenderMode];
        assert(postMat);

        // TODO: Swap chain for this to work
        MaterialRenderer::renderFullScreenQuad(*postMat);
    }


    mCurrentFramebufferDims = mScreenResolution;

    // Sky (non PBR version)
    if (!sDebugOptions.mUsingPBR) {
        renderPassSky(camera);
    }

    // Final render for pre-transparency
    mHDRLightGBuffer->use();
    // Share values
    mHDRLightGBuffer->setTertiaryTexture(mActiveGBuffer->getTertiaryTexture());
    mHDRLightGBuffer->setNormalTexture(mActiveGBuffer->getNormalTexture());
    mHDRLightGBuffer->setSharedDepthStencilTexture(mActiveGBuffer->getDepthStencilTexture());

    // Sunlight
    mLightRenderer->renderSunlight(*mActiveGBuffer, mShadowRenderer->getShadowTexture(), *mSkyBox->getCubemap());

    // Sky (PBR version)
    if (sDebugOptions.mUsingPBR) {
        renderPassSky(camera);
    }

    renderPassTransparent(camera, renderState);

    // Update active
    mActiveGBuffer = mHDRLightGBuffer.get();

    // Depth of field
    // TODO: THIS DOESNT WORK BECAUSE ITS NOT AN HDR BUFFER
    vg::DepthState::NONE.set();
    mActiveGBuffer = mDepthOfField->render(mActiveGBuffer);

    // Final render to screen, applying tonemap
    mActiveGBuffer->unuse();
    glViewport(0, 0, mScreenResolution.x, mScreenResolution.y);
    mTonemapRenderer->render(mHDRLightGBuffer->getAlbedoTexture());
    //MaterialRenderer::renderFullScreenQuad(*mPassthroughMaterial);

    // Final Pass through process
    // TODO: Make this work. When in debug, render tonemap to a new texture
    // FBODebugRenderer?
    if (mPassthroughRenderMode > 1) {
        const MaterialShader* postMat = mPassthroughMaterials[mPassthroughRenderMode];
        assert(postMat);

        // TODO: Swap chain for this to work
        MaterialRenderer::renderFullScreenQuad(*postMat);
    }

    // Debug rendering
    renderPassDebug(camera, renderState);

    // UI last
    renderPassUI(camera, renderState);

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

void RenderContext::addStaticModelInstancesFromGatherer(InstancedStaticModelGatherer& gatherer) {
    mStaticModelRenderer->addInstancesFromGatherer(gatherer);
}

void RenderContext::initEventHandlers() {
    // TODO: Remove?
}

void RenderContext::updateRenderThreadProcs() {
    PROFILE_FUNCTION();
    constexpr ui32 BULK_DEQUEUE_SIZE = 16;
    std::pair<RenderFunction, void*> procs[BULK_DEQUEUE_SIZE];
    PreciseTimer timer;
    // TODO: Use optik for profiling
    if (const size_t count = RenderThreadTasks::getInstance().mRenderThreadProcs.try_dequeue_bulk(procs, BULK_DEQUEUE_SIZE)) {
        for (size_t i = 0; i < count; ++i) {
            procs[i].first(*this, procs[i].second);
        }
    }
    if (timer.stop() > 20.0f) {
        std::cout << timer.stop() << " ms *** RENDER SPIKE WARNING ***\n";
    }
}

void RenderContext::renderPassSky(const Camera3D& camera) {
   // glEnable(GL_STENCIL_TEST);
   // glStencilFunc(GL_ALWAYS, e_cast(StencilBufferIDs::SKY), 0xFF);
    //glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    if (sDebugOptions.mUsingPBR) {
        mSkyBox->renderPbr(camera.getVPMatrix());
    }
    else {
        mSkyBox->render(camera.getVPMatrix());
    }
   // glDisable(GL_STENCIL_TEST);
}

void RenderContext::renderPassShadows(const Camera3D& camera, const RenderState& renderState) {
    PROFILE_FUNCTION();
    if (mRenderData.globalUboData.SunHeight > 0.01f && !sDebugOptions.mDisableShadows) {
        if (mShadowRenderer->shouldUpdateShadowsThisFrame()) {
            mShadowRenderer->useShadowBuffer();
            glEnable(GL_DEPTH_CLAMP);

            vg::DepthState::FULL.set();
            // Render all shadow casters
            //glCullFace(GL_FRONT);
            mTileContainerRenderer->renderWorldShadows(mStaticMeshes, camera, mShadowRenderer->getMaxDistance(ShadowLodDetail::High));

            // Instanced models
            Services::ResourceManager::ref().getMaterialRepository().bindMaterialBuffer();
            mStaticModelRenderer->renderModelShadows(camera, mShadowRenderer->getShadowCascadePlaneDistances());

            // TODO: Frustum cull
            if (!sDebugOptions.mDisableClouds) {
                mCloudRenderer->renderCloudShadows(*mCloudManager, camera, mShadowRenderer->getMaxDistance(ShadowLodDetail::Highest));
            }

            //const CityGraph& cities = sWorld->getCityGraph();
            //for (auto&& city : cities.mNodes) {
            //    const std::vector<std::unique_ptr<Building>>& buildings = city->getBuildings();
            //    for (auto& building : buildings) {
            //        mBuildingRenderer->renderBuildingShadows(*building, camera);
            //    }
            //}
            //for (auto&& structure : structures) {
            //    // TODO: List of buildings instead?
            //    if (structure->getType() == StructureType::Building) {
            //        mBuildingRenderer->renderBuildingShadows((Building&)*structure, camera);
            //    }
            //}

            glDisable(GL_DEPTH_CLAMP);
        }

        vg::DepthState::NONE.set();
        mShadowRenderer->renderShadows(camera.getPosition());
        mActiveGBuffer->use();
    }
    else {
        // No shadow bleed from previous frames
        mShadowRenderer->clearShadowTexture();
    }
}

void RenderContext::renderPassTransparent(const Camera3D& camera, const RenderState& renderState) {
    // Render clouds without shadows
    if (!sDebugOptions.mDisableClouds && !sDebugOptions.mWireframe) {
        mCloudRenderer->renderClouds(*mCloudManager, mHDRLightGBuffer->getDepthStencilTexture(), mHDRLightGBuffer.get(), camera, *mSkyBox->getCubemap());
    }

    // Water (No depth write)
    if (!sDebugOptions.mDisableWater && !sDebugOptions.mWireframe) {
        mTerrainRenderer->renderWater(camera, mTerrainWaterMeshes, *mSkyBox->getCubemap());
    }
    
    // Light transparent layer
}

void RenderContext::renderPassDebug(const Camera3D& camera, const RenderState& renderState) {
    PROFILE_FUNCTION();
    // City Debug
    if (sDebugOptions.mCities) {
        const CityGraph& cities = sWorld->getCityGraph();
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
        for (const auto& chunkDebugState : renderState.getDebugChunks()) {
            color4 color = COLOR_WHITE;
            if (chunkDebugState.mList == DebugChunkListIndex::DESTROYING) {
                color = color4(1.0f, 0.0f, 0.0f);
            }
            else if (chunkDebugState.mFlags.isBitSet(DebugChunkFlags::IS_NAVMESHING)) {
                color = color4(1.0f, 0.0f, 1.0f);
            }
            else {
                switch (chunkDebugState.mState) {
                    case ChunkState::INVALID:
                        color = color4(0.5f, 0.5f, 0.5f);
                        break;
                    case ChunkState::WAITING_HEIGHT:
                        color = color4(1.0f, 1.0f, 0.0f);
                        break;
                    case ChunkState::LOADING_TILES:
                        color = color4(0.0f, 1.0f, 1.0f);
                        break;
                    case ChunkState::TILE_LOAD_FINISHED:
                        color = color4(0.0f, 0.0f, 1.0f);
                        break;
                    case ChunkState::WAITING_MESH_PHYSICS_NAV:
                        color = color4(0.0f, 0.5f, 1.0f);
                        break;
                    case ChunkState::READY:
                        color = color4(0.0f, 1.0f, 0.0f);
                        break;
                    default:
                        break;
                }
            }

            const f32v2 worldPos = chunkDebugState.mId.getWorldPos();
            DebugRenderer::drawWireQuad(worldPos, f32v2(CHUNK_WIDTH), color);

            // Count refs
            constexpr f32 REF_BOX_WIDTH = 1.0f;
            constexpr ui32 REF_ROW_WIDTH = (CHUNK_WIDTH - 1) / REF_BOX_WIDTH;
            for (int i = 0; i < chunkDebugState.mRefCount; ++i) {
                DebugRenderer::drawWireQuad(worldPos + f32v2(REF_BOX_WIDTH) + f32v2(i % REF_ROW_WIDTH, (i / REF_ROW_WIDTH) * 2) * REF_BOX_WIDTH, f32v2(REF_BOX_WIDTH), color4(1.0f, 0.0f, 1.0f));
            }
        }
    }

    // Nav graph (Render is slow so we only build the line meshes when toggle changes)
    constexpr int NAVGRAPH_ID = 44432;
    constexpr f32 NAVGRAPH_RENDER_DISTANCE = 100.0f; // TODO: Move to debugoptions
    static bool wasRenderingNavGraph = false;
    if (sDebugOptions.mShowNavGraph) {
        if (!wasRenderingNavGraph) {
            ScopedTimer timer("Debug Draw Navgraph");
            DebugRenderer::reserveLines(sWorld->getNumActiveChunks() * 1024, MAX_DEBUG_RENDER_LIFETIME, NAVGRAPH_ID);
            const auto& containers = TileContainerRepository::getTileContainers();
            for (auto&& container : containers) {
                const f32v3 containerCenter = container->getWorldPosCenter3D();
                const f32v3& cameraPos = camera.getPosition();
                if (glm::length2(cameraPos - containerCenter) <= SQ(NAVGRAPH_RENDER_DISTANCE)) {
                    // TODO: make this only work on host world
                    assert(false);
                    //mWorld.getNavWorld().debugDrawCoarseNavGraphForContainer(*container, MAX_DEBUG_RENDER_LIFETIME, NAVGRAPH_ID);
                }
            }
            wasRenderingNavGraph = true;
        }
    }
    else if (wasRenderingNavGraph) {
        wasRenderingNavGraph = 0;
        DebugRenderer::clearAllMeshesWithId(NAVGRAPH_ID);
    }

    // Debug Shapes
    const std::vector<DebugWireQuadState>& debugQuads = renderState.getDebugQuads();
    for (const DebugWireQuadState& quad : debugQuads) {
        DebugRenderer::drawWireQuad(quad.origin, quad.dims, quad.color);
    }

    // Axis labels
    if (sDebugOptions.mShowDevHud) {
        const f32v3 axisOrigin = camera.getPosition() + camera.getFrontVector() * 5.0f + camera.getRightVector() * -5.0f + camera.getUpVector() * 2.5f;
        DebugRenderer::drawLine(axisOrigin, f32v3(1.0f, 0.0f, 0.0f), color4(1.0f, 0.0f, 0.0f)); //X
        DebugRenderer::drawLine(axisOrigin, f32v3(0.0f, 1.0f, 0.0f), color4(0.0f, 1.0f, 0.0f)); //Y
        DebugRenderer::drawLine(axisOrigin, f32v3(0.0f, 0.0f, 1.0f), color4(0.0f, 0.0f, 1.0f)); //Z
    }

    // Physics
    sWorld->getPhysicsWorld().debugRender();

    // Debug
    DebugRenderer::render(camera.getPosition(), camera.getVPMatrix());

    // Visual logger
    if (sDebugOptions.mEnableVisualLogs) {
        VisualLogger::renderActiveLogs(camera.getPosition(), camera.getVPMatrix());
    }

}

void RenderContext::renderPassUI(const Camera3D& camera, const RenderState& renderState) {
    if (sDebugOptions.mShowDevHud) {
        mSb->begin(100);
        char buffer[256];
        f32 scales = 0.6f;
        const float GAP_SIZE = 35.0f * scales;
        const float START_MULT = 0.1f;
        float yOffset = 0.0f;
        const f32v2 scale(scales);
        const f32 xPos = 10.0f;

        sprintf_s(buffer, sizeof(buffer), "FPS: %.0f", sFps);
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        sprintf_s(buffer, sizeof(buffer), "Jobs: %d", (int)Services::Threadpool::ref().getTasksSizeApprox());
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        if (Services::isUsingNav()) {
            sprintf_s(buffer, sizeof(buffer), "MainQueue: %d", (int)Services::Threadpool::ref().getMainThreadQueuedProcsApprox() + (int)Services::NavThread::ref().getMainThreadQueuedProcsApprox());
            mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
            yOffset += GAP_SIZE;

            sprintf_s(buffer, sizeof(buffer), "NavQueue: %d", (int)Services::NavThread::ref().getTasksSizeApprox());
            mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
            yOffset += GAP_SIZE;
        }
        else {
            sprintf_s(buffer, sizeof(buffer), "MainQueue: %d", (int)Services::Threadpool::ref().getMainThreadQueuedProcsApprox());
            mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
            yOffset += GAP_SIZE;
        }

        sprintf_s(buffer, sizeof(buffer), "GameQueue: %d", (int)GameThreadTasks::getInstance().getQueuedProcsApprox());
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        sprintf_s(buffer, sizeof(buffer), "RenderQueue: %d", (int)RenderThreadTasks::getInstance().getQueuedProcsApprox());
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        sprintf_s(buffer, sizeof(buffer), "DrawCalls: %u", RenderStats::sDrawCalls);
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        sprintf_s(buffer, sizeof(buffer), "Polygons: %u", RenderStats::sPolyCount);
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        sprintf_s(buffer, sizeof(buffer), "Models: %u", mStaticModelRenderer->getNumModels());
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        sprintf_s(buffer, sizeof(buffer), "Characters: %u", (ui32)renderState.getCharacterRenderState().size());
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        sprintf_s(buffer, sizeof(buffer), "Static objects: %u", sWorld->getPhysicsWorld().getNumStaticCollisionObjects());
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        sprintf_s(buffer, sizeof(buffer), "Dynamic objects: %u", sWorld->getPhysicsWorld().getNumDynamicCollisionObjects());
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        if (sWorld->getPhysicsWorld().isProfiling()) {
            mSb->drawString(mSpriteFont.get(), "PHYSICS PROFILING ON", f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::Red);
            yOffset += GAP_SIZE;
        }

        if (sDebugOptions.mChunkBoundaries) {
            sprintf_s(buffer, sizeof(buffer), "Chunks: %u", (ui32)renderState.getDebugChunks().size());
            mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
            yOffset += GAP_SIZE;
        }

        // If we are host, draw our server IP
        if (MainMenuScreenGlobalState::serverType != ServerType::NONE) {
            char buffer2[256];
            yojimbo::Address address = GameServer::getInstance().getServerAddress();
            yojimbo::Address addressNoPort;
            if (address.GetType() == yojimbo::ADDRESS_IPV4) {
                addressNoPort = yojimbo::Address(address.GetAddress4());
            }
            else {
                addressNoPort = yojimbo::Address(address.GetAddress6());

            }
            addressNoPort.ToString(buffer2, 256);
            sprintf_s(buffer, sizeof(buffer), "Host IP: %s", buffer2);
            mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
            yOffset += GAP_SIZE;
        }

        /*sprintf_s(buffer, sizeof(buffer), "SunHeight: %.2f", mWorld.getSunHeight());
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(0.0f, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        sprintf_s(buffer, sizeof(buffer), "SunPosition: %.2f", mWorld.getSunPosition());
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(0.0f, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;*/

        if (mPassthroughRenderMode != 0) {
            sprintf_s(buffer, sizeof(buffer), "DEBUG FBO: %s", sPassthroughMaterialNames[mPassthroughRenderMode].c_str());
            mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
            yOffset += GAP_SIZE;
        }

        mSb->end();
        mSb->render(mScreenResolution);
    }
    UIContext::getInstance().updateAndRenderUI(mActiveGBuffer);
}

void RenderContext::buildHorizonMesh()
{
    mHorizonQuad = std::make_unique<Mesh>();
    ProceduralMeshBuilder meshBuilder(true);
    constexpr float QUAD_WIDTH = 140000.0f;
    meshBuilder.addAxisAlignedQuad(f32v3(-QUAD_WIDTH, -QUAD_WIDTH, 0.0f), f32v2(QUAD_WIDTH * 2.0f), CubeFacing::TOP, MaterialData(), f32v4(0.0f, 0.0f, 1.0f, 1.0f), COLOR_WHITE);
    meshBuilder.finishMesh(mHorizonQuad, f32v3(0.0f));
}

TileContainerMeshData::~TileContainerMeshData()
{

}
