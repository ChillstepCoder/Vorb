#include "stdafx.h"
#include "RenderContext.h"
#include "resources/ResourceManager.h"
#include "resources/MaterialRepository.h"
#include "resources/TextureRepository.h"
#include "resources/FontRepository.h"
#include "world/World.h"
#include "world/HeightmapTerrainQuadtree.h"
#include "resources/TileRepository.h"
#include "resources/AssetLoader.h"
#include "pathfinding/NavWorld.h"
// Performance counters
#include "pathfinding/NavThread.h"
#include "gamethread/GameThread.h"

#include "debugging/DebugRenderer.h"
#include "debugging/VisualLogger.h"
#include "ECSRenderer.h"
#include "rendering/CharacterRenderer.h"
#include "rendering/renderer/GrassRenderer.h"
#include "rendering/TileContainerRenderer.h"
#include "rendering/ChunkGrassQuadtree.h"
#include "rendering/CloudRenderer.h"
#include "rendering/post_process/AmbientOcclusionPostProcess.h"
#include "rendering/post_process/DepthOfFieldPostProcess.h"
#include "rendering/ItemRenderer.h"
#include "rendering/LightRenderer.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/Skybox.h"
#include "rendering/post_process/ShadowRenderer.h"
#include "rendering/post_process/TonemapRenderer.h"
#include "rendering/RenderStats.h"
#include "rendering/TerrainRenderer.h"
#include "rendering/material/BrdfLUT.h"
#include "rendering/MaterialUtils.h"
#include "rendering/RenderThreadTasks.h"
#include "rendering/mesh/mesher/builder/ProceduralMeshBuilder.h"
#include "rendering/mesh/mesher/builder/TerrainMeshBuilder.h"
#include "rendering/model/InstancedStaticModelManager.h"
#include "rendering/renderstate/GameRenderStateManager.h"
#include "rendering/StencilBufferIDs.h"
#include "rendering/renderer/WorldRenderer.h"
#include "rendering/renderdata/WorldRenderDataManager.h"
#include "rendering/UboHelpers.h"
#include "rendering/particle/CPUParticleSystem.h"
#include "rendering/model/ModelImpostorManager.h"
#include "weather/CloudMeshManager.h"

#include "world/simulation/host/HostSimContext.h"
#include "world/simulation/host/SimThread.h"

#include "gamethread/GameThreadTasks.h"

#include "screens/ScreenState.h"
#include "network/srv/GameServer.h"

#include "building/BuildingGrid.h"

#include "ui/UIContext.h"

#include "tile/TileContainerRepository.h"

#include "editor/WorldEditorPanel.h"

// TODO: Move to renderer?
#include "city/CityQuartermaster.h"
#include "item/ItemStockpileRegistry.h"

#include "camera/Camera3D.h"
#include "camera/CameraController.h"

#include "time/TimeOfDayManager.h" // TODO: Move to WorldRenderer

#include "input/InputDispatcher.h"
#include <Vorb/graphics/SpriteBatch.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/BlendState.h>
#include <Vorb/graphics/GBuffer.h>
#include <Vorb/colors.h>
#include <Vorb/graphics/FullscreenTriangleVAO.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_opengl3.h>


#include <Vorb/io/IOManager.h>

#include "options/DebugOptions.h"

#define USE_STENCIL 1

#include <psapi.h>
#pragma comment(lib, "psapi.lib")
// TODO: This video is cool for hallucination effect https://www.youtube.com/watch?v=f4s1h2YETNY

bool IsRunningUnderNsight() {
    HMODULE hMods[1024];
    DWORD cbNeeded;
    unsigned int i;

    static int cachedValue = 0;
    if (cachedValue == 1) return true;
    if (cachedValue == 2) return false;

    // Get a handle to the current process
    HANDLE hProcess = GetCurrentProcess();

    // Get a list of all the modules in this process
    if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded))
    {
        for (i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
        {
            TCHAR szModName[MAX_PATH];

            // Get the full path to the module's file
            if (GetModuleFileNameEx(hProcess, hMods[i], szModName, sizeof(szModName) / sizeof(TCHAR)))
            {
                // Convert to lowercase for case-insensitive comparison
                std::string moduleName = szModName;
                std::transform(moduleName.begin(), moduleName.end(), moduleName.begin(), ::tolower);

                //LOG_DEBUG("Module name: {}", moduleName.c_str());

                // Check if the module name contains "injection"
                if (moduleName.find("injection") != std::wstring::npos)
                {
                    cachedValue = 1;
                    return true;  // Running under Nsight
                }
            }
        }
    }
    cachedValue = 2;
    return false;  // Not running under Nsight
}

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
    if (id == 131169/*driver allocated storage**/ || id == 131185 || id == 131218 || id == 131204 || id == 131186/*Buffer Performance warning*/) return;

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
    // Debug break crashes NSight
    if (!IsRunningUnderNsight()) {
        __debugbreak();
    }
    //panic(ss.str()); //  Dont want to crash on program link errors
    //assert(false);
}

RenderContext* RenderContext::sInstance = nullptr;

// TODO: Read http://iquilezles.org/articles/
RenderContext::RenderContext(const f32v2& screenResolution, SDL_Window* window) :
    mScreenResolution(screenResolution),
    mWindow(window)
{
    // State init
    GameRenderStateManager::initInstance();

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

    // Init UI resources
    mSb         = std::make_unique<vg::SpriteBatch>();
    mSpriteFont = std::make_unique<vg::SpriteFont>();
    mSb->init();

    vio::Path fontPath;
    if (!Services::ResourceManager::ref().getIoManager().resolvePath(vio::Path("data/ui/fonts/titilium_semibold.ttf"), fontPath)) {
        panic("Unable to resolve titilium_semibold.ttf font path, try verifying game files");
    }
    mSpriteFont->init(fontPath.getCString(), 32);
    checkGlError("SB init");

    sGlobalFullTriangleVAO.init();


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
        mGBuffers[i]->initAttachment(vg::GBufferAttachmentIndex::TERTIARY1, vg::TextureInternalFormat::RG8); // Roughness + Metallic
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

    UboHelpers::allocateGlobalUbo(mGlobalUbo);
    UboHelpers::allocateCameraUbo(mCameraUbo);

    // Improve depth precision (req for reverse depth buffer if we ever wanna do that)
    // https://www.danielecarbone.com/reverse-depth-buffer-in-opengl/
    glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

    initImguiStyle();
}

RenderContext::~RenderContext() {
    glDeleteBuffers(1, &mGlobalUbo);
    glDeleteBuffers(1, &mCameraUbo);
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
  
    mWorldRenderer = std::make_unique<WorldRenderer>(mScreenResolution);
    mWorldRenderer->initPostLoad();

    BrdfLUT::loadOrComputeTexture();

    // Visual logging
    VisualLogger::setDefaultFont(&Services::ResourceManager::ref().getFontRepository().getFont("titilium_semibold"));

    initEvents();
}

void RenderContext::beginFrame(WorldRenderState* renderState, f32v3 playerPos, f32 frameAlpha) {

    PROFILE_FUNCTION();

    updateCamera(frameAlpha);

    // Update thread msg queue
    updateRenderThreadProcs();

    mWorldRenderer->onBeginFrame(renderState, playerPos);

    RenderStats::clear();

    // Misc renderData
    const TimeOfDayManager& timeOfDayManager = mActiveWorld->getTimeOfDayManager();
    mRenderData.cameraZAngle = mCamera.getZAngle();
    mRenderData.skyRotMatrix = timeOfDayManager.getSkyRotMatrix();

    updateGlobalUbo(playerPos, mCamera);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(mWindow);
    ImGui::NewFrame();

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

}

void RenderContext::renderFrame(CameraController& cameraController, f32 frameAlpha, f32 elapsedSec) {
    PROFILE_FUNCTION();
    WorldRenderState& renderState = GameRenderStateManager::getInstance().getRenderStateForRender();
    mCurrentFrameAlpha = frameAlpha;
    mCurrentFrameElapsedSec = elapsedSec;
    mCameraController = &cameraController;
    mCurrentRenderState = &renderState;
    
    mActiveWorld = World::tryGetWorld(renderState.getWorldId());
    if (!mActiveWorld) {
        return;
    }

    beginFrame(&renderState, renderState.getCameraOwningEntityPos(), frameAlpha);
    checkGlError("RenderContext::Begin Frame");
    
    mActiveGBuffer = mGBuffers[mActiveGBufferIndex].get();

    // Main geometry pass
    mActiveGBuffer->use();
    mCurrentFramebufferDims = mActiveGBuffer->getSize();


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
        mWorldRenderer->renderWorld(&mCamera, mRenderData, mActiveGBuffer, frameAlpha, elapsedSec, nullptr/*targetGBuffer*/);
        // World Debug rendering
        renderPassWorldDebug(mCamera);
    }

    // UI
    renderPassUI(mCamera, renderState);

    // Swap
    mPrevGBufferIndex = mActiveGBufferIndex;
    mActiveGBufferIndex = !mActiveGBufferIndex;

    checkGlError("RenderContext::FrameEnd");

    renderState.onRenderThreadFinished();
}

void RenderContext::endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGui::EndFrame();
}

void RenderContext::selectNextDebugShader() {
    mWorldRenderer->selectNextDebugShader();
}

void RenderContext::updateGlobalUbo(f32v3 playerPos, const Camera3D& camera) {

    const TimeOfDayManager* timeOfDayManager = mActiveWorld ? &mActiveWorld->getTimeOfDayManager() : nullptr;
    ShadowRenderer& shadowRenderer = mWorldRenderer->getShadowRenderer();

    // Ubo data
    UboHelpers::uploadGlobalUbo(mGlobalUbo, camera, playerPos, shadowRenderer.getLastUpdatedSunPosition(), timeOfDayManager);
    UboHelpers::uploadCameraUbo(mCameraUbo, camera);
}

VGTexture RenderContext::getShadowTexture() const {
    return mWorldRenderer->getShadowRenderer().getShadowTexture();
}

Camera3DGameThreadData RenderContext::getGameThreadCameraData() const {
    Camera3DGameThreadData rv;
    {
        std::lock_guard lock(mCameraLock);
        rv.yaw = mCamera.getYaw();
        rv.direction = mCamera.getDirection();
        rv.worldPos = mCamera.getPosition();
    }
    return rv;
}

void RenderContext::removeLooseModelInstance(World& world, ModelID modelId, StaticModelInstanceID instanceId) {
    // TODO: This incurs a mutex lock in getRenderDataManagerForWorld, and it also could crash during shutdown if the render data manager is destroyed after we access it
    InstancedStaticModelManager& modelMgr = getRenderDataManagerForWorld(world).getInstancedStaticModelManager();
    modelMgr.removeLooseModelInstance(modelId, instanceId);
}

TileContainerRenderer& RenderContext::getTileContainerRenderer() const {
    return mWorldRenderer->getTileContainerRenderer();
}

CharacterRenderer& RenderContext::getCharacterRenderer() const {
    return mWorldRenderer->getCharacterRenderer();
}

WorldRenderDataManager& RenderContext::getRenderDataManagerForWorld(World& world) const {
    return mWorldRenderer->getRenderDataManagerForWorld(world);
}

void RenderContext::updateCamera(f32 frameAlpha) {
    // Update camera
    f32v3 cameraPos = mCurrentRenderState->getCameraOwningEntityPos();
    if (!mCurrentRenderState->isCameraOwned() || UIContext::getInstance().isEditorCameraActive()) {
        cameraPos = UIContext::getInstance().getEditorCameraPosition();
        mCameraController->setEditorMode(true);
        mCameraController->setCameraDirection(UIContext::getInstance().getEditorCameraDirection());
    }
    else {
        mCameraController->setEditorMode(false);
    }
    mCameraController->update(1.0f /*TODO DELTATIME*/, frameAlpha, cameraPos);

    // Copy camera from the controller threadsafe
    {
        std::lock_guard lock(mCameraLock);
        memcpy(&mCamera, &mCameraController->getOwnedCamera(), sizeof(Camera3D));

        // Water clipping
        constexpr f32 CAMERA_SURFACE_CLAMP = 0.01f;
        constexpr f32 CAMERA_DEPTH_CLAMP = -0.2f;
        const f32v3 newCameraPos = mCamera.getPosition();
        if (newCameraPos.z > CAMERA_DEPTH_CLAMP) {
            if (newCameraPos.z <= CAMERA_SURFACE_CLAMP) {
                mCamera.setPosition(f32v3(newCameraPos.x, newCameraPos.y, CAMERA_DEPTH_CLAMP));
                sDebugOptions.mIsCameraUnderwater = !sDebugOptions.mDisableWater;
            }
            else {
                sDebugOptions.mIsCameraUnderwater = false;
            }
        }
        else {
            sDebugOptions.mIsCameraUnderwater = true;
        }
    }
}

void RenderContext::initEvents()
{
}

void RenderContext::initImguiStyle()
{
    // https://github.com/ocornut/imgui/issues/707
    ImVec4* colors = ImGui::GetStyle().Colors;
    ImGuiStyle& style = ImGui::GetStyle();

    style.Colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 0.90f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.05f, 0.05f, 0.10f, 0.85f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.70f, 0.70f, 0.70f, 0.65f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.00f, 0.00f, 0.01f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.90f, 0.80f, 0.80f, 0.40f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.90f, 0.65f, 0.65f, 0.45f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.83f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.40f, 0.40f, 0.80f, 0.20f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.00f, 0.00f, 0.00f, 0.87f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.01f, 0.01f, 0.02f, 0.80f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.20f, 0.25f, 0.30f, 0.60f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.55f, 0.53f, 0.55f, 0.51f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.56f, 0.56f, 0.56f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.91f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.90f, 0.90f, 0.83f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.70f, 0.70f, 0.70f, 0.62f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.30f, 0.30f, 0.30f, 0.84f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.48f, 0.72f, 0.89f, 0.49f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.50f, 0.69f, 0.99f, 0.68f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.80f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.30f, 0.69f, 1.00f, 0.53f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.44f, 0.61f, 0.86f, 1.00f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.38f, 0.62f, 0.83f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.85f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.60f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.90f);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.00f, 0.00f, 1.00f, 0.35f);

    style.SeparatorTextBorderSize = 3;
    style.WindowPadding = ImVec2(8.00f, 8.00f);
    style.FramePadding = ImVec2(5.00f, 2.00f);
    style.CellPadding = ImVec2(6.00f, 6.00f);
    style.ItemSpacing = ImVec2(6.00f, 6.00f);
    style.ItemInnerSpacing = ImVec2(6.00f, 6.00f);
    style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
    style.IndentSpacing = 25;
    style.ScrollbarSize = 15;
    style.GrabMinSize = 10;
    style.WindowBorderSize = 1;
    style.ChildBorderSize = 1;
    style.PopupBorderSize = 1;
    style.FrameBorderSize = 1;
    style.TabBorderSize = 1;
    style.WindowRounding = 7;
    style.ChildRounding = 4;
    style.FrameRounding = 3;
    style.PopupRounding = 4;
    style.ScrollbarRounding = 9;
    style.GrabRounding = 3;
    style.LogSliderDeadzone = 4;
    style.TabRounding = 4;
}

void RenderContext::updateRenderThreadProcs() {
    PROFILE_FUNCTION();
    ASSERT_RENDER_THREAD();

    RenderThreadTasks::getInstance().processRenderThread(*this);
}

void RenderContext::renderPassWorldDebug(const Camera3D& camera) const {
    PROFILE_FUNCTION();
    mWorldRenderer->renderDebug();

    // Debug
    AM::DebugRenderer::render(camera.getPosition(), camera.getVPMatrix());

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

void RenderContext::renderPassUI(const Camera3D& camera, const WorldRenderState& renderState) {
    PROFILE_FUNCTION();

    if (sDebugOptions.mShowDevHud) {
        mSb->begin(100);
        char buffer[STR_BUFFER_SIZE];
        f32 scales = 0.6f;
        const float GAP_SIZE = 35.0f * scales;
        const float START_MULT = 0.1f;
        float yOffset = 0.0f;
        const f32v2 scale(scales);
        const f32 xPos = 10.0f;

#ifdef DEBUG
        mSb->drawString(mSpriteFont.get(), "DEBUG MODE", f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color4(0, 200, 0, 255));
        yOffset += GAP_SIZE;
#endif

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
            // Sim
            if (HostSimContext* simContext = mActiveWorld->tryGetHostSimContext()) {
                if (SimThread* simThread = simContext->tryGetSimThread()) {
                    drawColor = sprintfThreadStats(simThread->getThreadUtilizationTimer(), "SimThread", buffer);
                    mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, drawColor);
                    yOffset += GAP_SIZE;
                }
            }

            yOffset += GAP_SIZE;
        }

        sprintf_s(buffer, STR_BUFFER_SIZE, "CPU Threads: %d", (int)std::thread::hardware_concurrency());
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        const vcore::ThreadPool& tp = Services::Threadpool::ref();
        sprintf_s(buffer, STR_BUFFER_SIZE, "Jobs: %d,%d,%d Workers: %d (%d)", 
            (int)tp.getTasksSizeApprox(TaskPriority::High), (int)tp.getTasksSizeApprox(TaskPriority::Normal), (int)tp.getTasksSizeApprox(TaskPriority::Low),
            (int)tp.getNumWorkers(), (int)tp.getNumRunningThreads());
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        if (Services::isUsingNav()) {
            sprintf_s(buffer, STR_BUFFER_SIZE, "GameQueue: %d", (int)GameThreadTasks::getInstance().getQueuedProcsApprox());
            mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
            yOffset += GAP_SIZE;

            sprintf_s(buffer, STR_BUFFER_SIZE, "NavQueue: %d", (int)Services::NavThread::ref().getTasksSizeApprox());
            mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
            yOffset += GAP_SIZE;
        }
        else {
            sprintf_s(buffer, STR_BUFFER_SIZE, "GameQueue: %d", (int)GameThreadTasks::getInstance().getQueuedProcsApprox());
            mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
            yOffset += GAP_SIZE;
        }

        if (HostSimContext* simContext = mActiveWorld->tryGetHostSimContext()) {
            if (SimThread* simThread = simContext->tryGetSimThread()) {
                sprintf_s(buffer, STR_BUFFER_SIZE, "SimQueue: %d", (int)simThread->getTasksSizeApprox());
                mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
                yOffset += GAP_SIZE;
            }
        }

        sprintf_s(buffer, STR_BUFFER_SIZE, "RenderQueue: %d", (int)RenderThreadTasks::getInstance().getQueuedProcsApprox());
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        sprintf_s(buffer, STR_BUFFER_SIZE, "AssetLoadQueue: %d", (int)AssetLoader::getInstance().getQueuedProcsApprox());
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE * 2.0f;

        sprintf_s(buffer, STR_BUFFER_SIZE, "DrawCalls: %u", RenderStats::sDrawCalls);
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        sprintf_s(buffer, STR_BUFFER_SIZE, "Polygons: %u", RenderStats::sPolyCount);
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        {
            WorldRenderDataManager* mgr = mWorldRenderer->tryGetRenderDataManagerForWorld(*mActiveWorld);
            if (mgr) {
                sprintf_s(buffer, STR_BUFFER_SIZE, "Models: %u", mgr->getInstancedStaticModelManager().getNumModels());
                mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
                yOffset += GAP_SIZE;
            }
        }

        sprintf_s(buffer, STR_BUFFER_SIZE, "Characters: %u", (ui32)renderState.getCharacterRenderState().size());
        mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
        yOffset += GAP_SIZE;

        //if (mActiveWorld->getPhysicsWorld().isProfiling()) {
        //    mSb->drawString(mSpriteFont.get(), "PHYSICS PROFILING ON", f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::Red);
        //    yOffset += GAP_SIZE;
        //}

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

        StrToken passThroughName = mWorldRenderer->getCurrentPassthroughRenderStageName();
        if (passThroughName.isValid()) {
            sprintf_s(buffer, STR_BUFFER_SIZE, "DEBUG FBO: %s", passThroughName.toString().c_str());
            mSb->drawString(mSpriteFont.get(), buffer, f32v2(xPos, START_MULT * mScreenResolution.y + yOffset), scale, color::White);
            yOffset += GAP_SIZE;
        }

        mSb->end();
        mSb->render(mScreenResolution, &vg::sSamplerStates.LINEAR_WRAP);
    }
    UIContext::getInstance().updateAndRenderUI(mActiveGBuffer, mCurrentFrameElapsedSec, camera);
}
