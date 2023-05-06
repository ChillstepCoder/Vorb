#include "stdafx.h"
#include "RenderContext.h"
#include "resources/ResourceManager.h"
#include "resources/MaterialRepository.h"
#include "resources/TextureRepository.h"
#include "resources/FontRepository.h"
#include "world/IWorld.h"
#include "world/cli/CliWorldInterface.h"
#include "world/HeightmapTerrainQuadtree.h"
#include "resources/TileRepository.h"
#include "pathfinding/NavWorld.h"
// Performance counters
#include "pathfinding/NavThread.h"
#include "gamethread/GameThread.h"

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
#include "rendering/model/InstancedStaticModelManager.h"
#include "rendering/renderstate/RenderStateManager.h"
#include "rendering/StencilBufferIDs.h"
#include "rendering/renderer/WorldRenderer.h"
#include "rendering/renderdata/WorldRenderDataManager.h"
#include "weather/CloudMeshManager.h"

#include "gamethread/GameThreadTasks.h"

#include "screens/ScreenState.h"
#include "network/srv/GameServer.h"

#include "structure/StructureManager.h"

#include "ui/UIContext.h"

#include "tile/TileContainerRepository.h"

#include "editor/WorldEditorPanel.h"

// TODO: Move to renderer?
#include "city/CityQuartermaster.h"
#include "item/ItemStockpileRegistry.h"

#include "camera/Camera3D.h"
#include "camera/CameraController.h"
#include "physics/PhysicsWorld.h"

#include "time/TimeOfDayManager.h" // TODO: Move to WorldRenderer
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
    //assert(false);
}

constexpr ui32 CAMERA_MATRICES_BYTE_SIZE = sizeof(f32m4) * 6 /*camera matrices*/;

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

void RenderContext::initPostLoad() {

    const ResourceManager& resourceManager = Services::ResourceManager::ref();
    const MaterialShaderManager& materialManager = resourceManager.getMaterialShaderManager();

  
    mWorldRenderer = std::make_unique<WorldRenderer>(mScreenResolution);
    mWorldRenderer->initPostLoad();

    BrdfLUT::loadOrComputeTexture();

    // Visual logging
    VisualLogger::setDefaultFont(&Services::ResourceManager::ref().getFontRepository().getFont("titilium_semibold"));

}

void RenderContext::beginFrame(const RenderState* renderState, const Camera3D* camera, f32v3 playerPos) {

    PROFILE_FUNCTION();

    mCamera = camera;
    // Update thread msg queue
    updateRenderThreadProcs();

    mWorldRenderer->onBeginFrame(renderState, playerPos);

    GlobalUboData& uboData = mRenderData.globalUboData;
    RenderStats::clear();
    // Misc renderData
    const TimeOfDayManager& timeOfDayManager = mActiveWorld->getTimeOfDayManager();
    mRenderData.cameraZAngle = camera->getZAngle();
    mRenderData.skyRotMatrix = timeOfDayManager.getSkyRotMatrix();
    // Ubo data
    uboData.Time = sTotalTimeSeconds;
    uboData.TimeOfDay = timeOfDayManager.getTimeOfDayHours();
    uboData.PlayerPosWorld = playerPos;
    
    ShadowRenderer& shadowRenderer = mWorldRenderer->getShadowRenderer();
    const f32v3 lastSunPosition = shadowRenderer.getLastUpdatedSunPosition();
    uboData.SunColor = timeOfDayManager.getSunColor();
    uboData.SunHeight = timeOfDayManager.getSunHeight();
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
    mCurrentFrameAlpha = frameAlpha;
    mCurrentFrameElapsedSec = elapsedSec;

    const RenderState& renderState = RenderStateManager::getInstance().getRenderStateForRender();
    mActiveWorld = renderState.getWorld();
    if (!mActiveWorld) {
        return;
    }

    // Update camera
    f32v3 cameraPos = renderState.getCameraOwningEntityPos();
    if (!renderState.isCameraOwned()) {
        cameraPos = UIContext::getInstance().getEditorCameraPosition();
        cameraController.setEditorMode(true);
        cameraController.setCameraDirection(UIContext::getInstance().getEditorCameraDirection());
    }
    else {
        cameraController.setEditorMode(false);
    }
    cameraController.update(1.0f /*TODO DELTATIME*/, frameAlpha, cameraPos);
    const Camera3D& camera = cameraController.getOwnedCamera();

    beginFrame(&renderState, &camera, cameraPos);
    checkGlError("RenderContext::Begin Frame");
    
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
        glClear(GL_DEPTH_BUFFER_BIT | (GL_STENCIL_BUFFER_BIT * USE_STENCIL));
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // World
    if (!UIContext::getInstance().shouldPauseGameRendering()) {
        mWorldRenderer->renderWorld(mCamera, mRenderData, mActiveGBuffer, frameAlpha, elapsedSec, nullptr/*targetGBuffer*/);
    }

    // Debug rendering
    renderPassDebug(*mCamera, renderState);

    // UI
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

void RenderContext::tickGameThread(IWorld& world) {
    WorldRenderDataManager* renderDataManager = mWorldRenderer->tryGetRenderDataManagerForWorld(world);
    if (renderDataManager) {
        renderDataManager->tickGameThread();
    }
}

void RenderContext::selectNextDebugShader() {
    mWorldRenderer->selectNextDebugShader();
}

VGTexture RenderContext::getShadowTexture() const {
    return mWorldRenderer->getShadowRenderer().getShadowTexture();
}

TileContainerRenderer& RenderContext::getTileContainerRenderer() const {
    return mWorldRenderer->getTileContainerRenderer();
}

CharacterRenderer& RenderContext::getCharacterRenderer() const {
    return mWorldRenderer->getCharacterRenderer();
}

WorldRenderDataManager& RenderContext::getRenderDataManagerForWorld(IWorld& world) const {
    return mWorldRenderer->getRenderDataManagerForWorld(world);
}

WorldRenderDataManager* RenderContext::tryGetRenderDataManagerForWorld(IWorld& world) const {
    return mWorldRenderer->tryGetRenderDataManagerForWorld(world);
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

void RenderContext::renderPassDebug(const Camera3D& camera, const RenderState& renderState) {
    PROFILE_FUNCTION();
    mWorldRenderer->renderDebug();

    // Debug
    DebugRenderer::render(camera.getPosition(), camera.getVPMatrix());

    // Visual logger
    if (sDebugOptions.mEnableVisualLogs) {
        VisualLogger::renderActiveLogs(camera.getPosition(), camera.getVPMatrix());
    }

}

constexpr ui32 STR_BUFFER_SIZE = 512;

color4 sprintfThreadStats(const ThreadUtilizationTimer& timer, const char* name, char* buffer) {
    const f32 utilization = timer.getUtilizationPercentage();
    const f32 frameTime = timer.getFrameTimeMS();
    sprintf_s(buffer, STR_BUFFER_SIZE, "%s:   %-5.2fms     %-3.0f%%", name, frameTime, utilization);
    if (frameTime > 15.0f) {
        return color::Red;
    }
    else if (frameTime > 8.0f) {
        return color::Orange;
    }
    return color::White;
}

void RenderContext::renderPassUI(const Camera3D& camera, const RenderState& renderState) {
    if (sDebugOptions.mShowDevHud) {
        mSb->begin(100);
        char buffer[STR_BUFFER_SIZE];
        f32 scales = 0.6f;
        const float GAP_SIZE = 35.0f * scales;
        const float START_MULT = 0.1f;
        float yOffset = 0.0f;
        const f32v2 scale(scales);
        const f32 xPos = 10.0f;

        sprintf_s(buffer, STR_BUFFER_SIZE, "FPS: %.0f", sFps);
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        // Thread stats
        {
            color4 drawColor;
            // Nav
            drawColor = sprintfThreadStats(Services::NavThread::ref().getThreadUtilizationTimer(), "NavThread", buffer);
            mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, drawColor);
            yOffset += GAP_SIZE;
            // Game
            drawColor = sprintfThreadStats(GameThread::getInstance().getThreadUtilizationTimer(), "GameThread", buffer);
            mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, drawColor);
            yOffset += GAP_SIZE;
        }

        sprintf_s(buffer, STR_BUFFER_SIZE, "Jobs: %d", (int)Services::Threadpool::ref().getTasksSizeApprox());
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        if (Services::isUsingNav()) {
            sprintf_s(buffer, STR_BUFFER_SIZE, "MainQueue: %d", (int)Services::Threadpool::ref().getMainThreadQueuedProcsApprox() + (int)Services::NavThread::ref().getMainThreadQueuedProcsApprox());
            mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
            yOffset += GAP_SIZE;

            sprintf_s(buffer, STR_BUFFER_SIZE, "NavQueue: %d", (int)Services::NavThread::ref().getTasksSizeApprox());
            mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
            yOffset += GAP_SIZE;
        }
        else {
            sprintf_s(buffer, STR_BUFFER_SIZE, "MainQueue: %d", (int)Services::Threadpool::ref().getMainThreadQueuedProcsApprox());
            mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
            yOffset += GAP_SIZE;
        }

        sprintf_s(buffer, STR_BUFFER_SIZE, "GameQueue: %d", (int)GameThreadTasks::getInstance().getQueuedProcsApprox());
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        sprintf_s(buffer, STR_BUFFER_SIZE, "RenderQueue: %d", (int)RenderThreadTasks::getInstance().getQueuedProcsApprox());
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        sprintf_s(buffer, STR_BUFFER_SIZE, "DrawCalls: %u", RenderStats::sDrawCalls);
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        sprintf_s(buffer, STR_BUFFER_SIZE, "Polygons: %u", RenderStats::sPolyCount);
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        sprintf_s(buffer, STR_BUFFER_SIZE, "Models: %u", mWorldRenderer->getRenderDataManagerForWorld(*mActiveWorld).getInstancedStaticModelManager().getNumModels());
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        sprintf_s(buffer, STR_BUFFER_SIZE, "Characters: %u", (ui32)renderState.getCharacterRenderState().size());
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        sprintf_s(buffer, STR_BUFFER_SIZE, "Static objects: %u", mActiveWorld->getPhysicsWorld().getNumStaticCollisionObjects());
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        sprintf_s(buffer, STR_BUFFER_SIZE, "Dynamic objects: %u", mActiveWorld->getPhysicsWorld().getNumDynamicCollisionObjects());
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        if (mActiveWorld->getPhysicsWorld().isProfiling()) {
            mSb->drawString(mSpriteFont.get(), "PHYSICS PROFILING ON", f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::Red);
            yOffset += GAP_SIZE;
        }

        if (sDebugOptions.mChunkBoundaries) {
            sprintf_s(buffer, STR_BUFFER_SIZE, "Chunks: %u", (ui32)renderState.getDebugChunks().size());
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
            sprintf_s(buffer, STR_BUFFER_SIZE, "Host IP: %s", buffer2);
            mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
            yOffset += GAP_SIZE;
        }

        /*sprintf_s(buffer, STR_BUFFER_SIZE, "SunHeight: %.2f", mWorld.getSunHeight());
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(0.0f, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        sprintf_s(buffer, STR_BUFFER_SIZE, "SunPosition: %.2f", mWorld.getSunPosition());
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(0.0f, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;*/

        const nString& passThroughName = mWorldRenderer->getCurrentPassthroughRenderStageName();
        if (passThroughName.size()) {
            sprintf_s(buffer, STR_BUFFER_SIZE, "DEBUG FBO: %s", passThroughName.c_str());
            mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
            yOffset += GAP_SIZE;
        }

        mSb->end();
        mSb->render(mScreenResolution);
    }
    UIContext::getInstance().updateAndRenderUI(mActiveGBuffer);
}
