#include "stdafx.h"
#include "WorldGenScreen.h"

#include "App.h"

#include "screens/ScreenState.h"

#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include "ui/ImguiUtil.hpp"


#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/BlendState.h>
#include <Vorb/graphics/GBuffer.h>

#include "world/World.h"
#include "world/WorldDefaults.h"
#include "world/simulation/host/HostSimContext.h"
#include "world/simulation/host/SimThread.h"
#include "world/chunk/SimChunkTileGrid.h"
#include "generation/WorldDataGenerator.h"
#include "generation/WorldGenerationBlackboard.h"

#include "rendering/MaterialShaderRepository.h"
#include "rendering/RenderContext.h"

#include "rendering/ShaderLoader.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/mesh/LineMesh.h"
#include "rendering/mesh/AxisAlignedQuadMesh.h"

#include "time/TimeOfDayManager.h"

#include <Vorb/graphics/ShaderManager.h>
#include <Vorb/graphics/FullscreenTriangleVAO.h>
#include <Vorb/ui/InputDispatcher.h>

#include "camera/OrthoCamera.h"

#include "math/Random.h"

// Possible human readable characters to generate a game seed with
constexpr int RANDOM_SEED_VALUES_COUNT = 95;
constexpr char RANDOM_SEED_VALUES[RANDOM_SEED_VALUES_COUNT] = {
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
    'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
    'u', 'v', 'w', 'x', 'y', 'z', 'A', 'B', 'C', 'D',
    'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', '!', '"', '#', '$', '%', '&', '\'', '(',
    ')', '*', '+', ',', '-', '.', '/', ':', ';', '<',
    '=', '>', '?', '@', '[', '\\', ']', '^', '_', '`',
    '{', '|', '}', '~', ' ',
};

    
// UTIL
namespace ImGui {

    bool BufferingBar(const char* label, float value, const ImVec2& size_arg, const ImU32& bg_col, const ImU32& fg_col) {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);

        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = size_arg;
        size.x -= style.FramePadding.x * 2;

        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
        ItemSize(bb, style.FramePadding.y);
        if (!ItemAdd(bb, id))
            return false;

        // Render
        const float circleStart = size.x * 0.7f;
        const float circleEnd = size.x;
        const float circleWidth = circleEnd - circleStart;

        window->DrawList->AddRectFilled(bb.Min, ImVec2(pos.x + circleStart, bb.Max.y), bg_col);
        window->DrawList->AddRectFilled(bb.Min, ImVec2(pos.x + circleStart * value, bb.Max.y), fg_col);

        const float t = g.Time;
        const float r = size.y / 2;
        const float speed = 1.5f;

        const float a = speed * 0;
        const float b = speed * 0.333f;
        const float c = speed * 0.666f;

        const float o1 = (circleWidth + r) * (t + a - speed * (int)((t + a) / speed)) / speed;
        const float o2 = (circleWidth + r) * (t + b - speed * (int)((t + b) / speed)) / speed;
        const float o3 = (circleWidth + r) * (t + c - speed * (int)((t + c) / speed)) / speed;

        window->DrawList->AddCircleFilled(ImVec2(pos.x + circleEnd - o1, bb.Min.y + r), r, bg_col);
        window->DrawList->AddCircleFilled(ImVec2(pos.x + circleEnd - o2, bb.Min.y + r), r, bg_col);
        window->DrawList->AddCircleFilled(ImVec2(pos.x + circleEnd - o3, bb.Min.y + r), r, bg_col);
    }
}


constexpr int SCREEN_TEXTURE_RES = 4096;

WorldGenScreen::WorldGenScreen(App* const app) : IAppScreen<App>(app) {
}

WorldGenScreen::~WorldGenScreen()
{

}

i32 WorldGenScreen::getNextScreen() const
{
    return e_cast(RegisteredScreens::Gameplay);
}

i32 WorldGenScreen::getPreviousScreen() const
{
    return e_cast(RegisteredScreens::MainMenu);
}

void WorldGenScreen::build()
{
    mCamera = std::make_unique<OrthoCamera>();
    mCamera->setDirection(f32v3(0.f, 0.f, -1.f));
    mCamera->setRight(f32v3(1.f, 0.f, 0.f));
    mCamera->setUp(f32v3(0.f, 1.f, 0.f));
    mCamera->setXYDims(f32v2(2.0f, 2.0f));
    //mCamera->setDims(f32v3(32768.f, 32768.0, 0.0f));
}

void WorldGenScreen::destroy(const vui::GameTime& gameTime)
{

}

void WorldGenScreen::onEntry(const vui::GameTime& gameTime) {
    mFrameTimer.start();
    mScreenShader = MaterialShaderRepository::get().getAssetHandle(CStrToken("generation_map"));
    mDebugLineShader = MaterialShaderRepository::get().getAssetHandle(CStrToken("river_gen_lines"));

    mMapScreenGBuffer = std::make_unique<vg::GBuffer>(SCREEN_TEXTURE_RES, SCREEN_TEXTURE_RES);
    mMapScreenGBuffer->initAttachment(vorb::graphics::GBufferAttachmentIndex::ALBEDO, vg::TextureInternalFormat::RGB8);

    if (mFirstEntry) {
        Services::initHost();
    }
    mThreadpoolSizePostEntry = Services::Threadpool::ref().getSize();
    Services::Threadpool::ref().setSize(std::thread::hardware_concurrency() - 1);


    beginWorldGeneration();
   
}

void WorldGenScreen::onExit(const vui::GameTime& gameTime) {
    Services::Threadpool::ref().setSize(mThreadpoolSizePostEntry);

    mRiverDebugMesh.reset();
    mRiverDebugVisitedMesh.reset();
    mRiverDebugLocalGroupMesh.reset();

    if (mCancelled) {
        sGameWorld.reset();
    }
    else {
        // Initialize world
        assert(mWorldGenerator);
        mWorldData->biomeGrid->setBiomeTexture(mWorldGenerator->releaseBiomeTexture());
        sGameWorld = mWorldGenerator->releaseWorld();
        sGameWorld->setDefaultWorldSpawn(mWorldData->playerStart);
    }
    mWorldData.reset();

    if (mWorldGenerator) {
        mWorldGenerator.reset();
    }

    //Force cycle a frame to avoid bug with deleting mScreenTexture as imgui uses it an extra frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame((SDL_Window*)m_app->getWindow().getHandle());
    ImGui::NewFrame();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGui::EndFrame();

    mMapScreenGBuffer.reset();
}

void WorldGenScreen::update(const vui::GameTime& gameTime) {

    // Update tasks
    Services::Threadpool::ref().mainThreadUpdate();
    RenderContext::getInstance().updateRenderThreadProcs();

    if (mWorldGenerator) {
        mWorldGenerator->update();

        switch (mGenState) {
            case WorldGenScreenState::Idle:
                break;
            case WorldGenScreenState::Generating:
                break;
            case WorldGenScreenState::Done:
                break;
            default:
                break;

        }
    }
    static_assert(e_count(WorldGenScreenState) == 3);

}

void WorldGenScreen::draw(const vui::GameTime& gameTime)
{
    mFrameTimeThisFrame = mFrameTimer.stop();
    mFrameTimer.start();
    renderMapView();

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    vg::DepthState::NONE.set();
    vg::BlendState::set(vg::BlendStateType::ALPHA);

    glClearColor(0.5f, 0.5f, 0.5f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    vui::GameWindow& window = m_app->getWindow();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame((SDL_Window*)window.getHandle());
    ImGui::NewFrame();

    updateDockspace();

    //const ImVec2 screenSize = ImVec2(window.getWidth(), window.getHeight());
   // ImGui::SetNextWindowPos(ImVec2(0, 0));
   // ImGui::SetNextWindowSize(screenSize);
    ImGui::Begin("World Generator", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
    const ImVec2 availableSize = ImGui::GetContentRegionAvail();
    const f32 minAvailable = glm::min(availableSize.x, availableSize.y);
    mCurrentTextureSize = f32v2(minAvailable);
    ImGui::Image((ImTextureID)mMapScreenGBuffer->getAlbedoTexture(), ImVec2(minAvailable, minAvailable),
        ImVec2(0, 1),  ImVec2(1, 0));

    if (ImGui::IsItemHovered()) {
        updateMouseInput();
    }

    ImGui::End();

    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
    ImGui::Text(mWorldGenerator->getCurrentStageName());
    ImU32 backgroundColor = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);
    ImU32 foregroundColor = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_PlotHistogram]);
    // Progress bar
    if (mWorldGenerator->isDone()) {
        ImGui::Spacing();
    }
    else {
        f32 progress = mWorldGenerator->getCurrentStageProgress();
        if (progress) {
            ImGui::BufferingBar("%", mWorldGenerator->getCurrentStageProgress(), ImVec2(ImGui::GetContentRegionAvail().x, 20.0f), backgroundColor, foregroundColor);
        }
        else {
            ImGui::Spacing();
        }
    }
    ImGui::Text("%.2f ms", mFrameTimeThisFrame);
    const f32v2 playerSpawn = mWorldData->playerStart * f32(mWorldData->worldWidth);
    ImGui::Text("Spawn Position (%.1f, %.1f)", playerSpawn.x, playerSpawn.y);
    if (mGenState == WorldGenScreenState::Done) {
        ImGui::Text("Height: %.1f", mWorldData->heightmapGrid->computeHeightAtPoint<true>(playerSpawn));
        const BiomeDef* biome = mWorldData->biomeGrid->getBiomeDefAtPoint(playerSpawn);
        if (biome) {
            ImGui::Text("Biome: %s", biome->displayName.c_str());
        }
        else {
            ImGui::Text("Biome: OCEAN");
        }
    }
    if (mWorldData->markupGrid->isMarkupReady()) {
        const WorldBodyMarkupData* data = mWorldData->markupGrid->getBodyDataAtPoint(playerSpawn);
        if (data) {
            if (data->bodyIndex != mSelectedBody) {
                mSelectedBody = data->bodyIndex;
                mNeedsNewBorderMesh = true;
            }

            nString str;
            switch (data->bodyType) {
                case WorldMarkupBodyType::LargeIsland:
                    str = "Large Island";
                    break;
                case WorldMarkupBodyType::Island:
                    str = "Island";
                    break;
                case WorldMarkupBodyType::Lake:
                    str = "Lake";
                    break;
                case WorldMarkupBodyType::Ocean:
                    str = "Ocean";
                    break;
                default:
                    assert(false);

            }
            static_assert(e_count(WorldMarkupBodyType) == 4);
            ImGui::Text("  %s", data->name);
            ImGui::Text("  %s - Size: %d", str.c_str(), data->sizeBlocks);
            ImGui::Text("  Chunk Land Ratio: %f", mWorldData->markupGrid->getChunkMarkupAtPoint(playerSpawn)->landRatio);
            ImGui::Text("Total land chunks: %d / %d  %f",
                mWorldData->markupGrid->getTotalLandChunks(), SQ(mWorldData->worldWidth / CHUNK_WIDTH),
                f32(mWorldData->markupGrid->getTotalLandChunks()) / SQ(mWorldData->worldWidth / CHUNK_WIDTH));
            ImGui::Text("Sim Tile Memory Usage: %f mb", mWorldData->tileGrid->getApproxMemoryUsageBytes() / 1024.f / 1024.f);
        }
    }
    ImGui::Checkbox("Show Biomes", &mShowBiomes);
    ImGui::Checkbox("Show Height", &mShowHeight);
    ImGui::Checkbox("Show Rivers", &mShowRivers);
    ImGui::Checkbox("Show Chunks", &mShowChunks);
    ImGui::Checkbox("Show Body Border", &mShowSelectedBody);
    World* world = mWorldGenerator->tryGetWorld();
    if (world) {
        HostSimContext* simContext = world->tryGetHostSimContext();
        if (simContext) {
            ImGui::Text("Date Time: %s", TimeOfDayManager::convertTimestampMSToDateTime(simContext->getSimTime()).toString().c_str());
            ImGui::Checkbox("Draw characters", &mDrawCharacters);
        }
    }

    mWorldGenerator->renderCurrentStageImguiControls();

    ImGui::Separator();
    if (mGenState == WorldGenScreenState::Done) {
        if (ImGui::Button("Start Game")) {
            m_state = vorb::ui::ScreenState::CHANGE_NEXT;
        }
    }
    else {
        ImguiUtil::ScopedColor lockedColor(ImGuiCol_Button, ImguiColors::Theme::muted);
        ImGui::Button("Start Game");
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        mCancelled = true;
        m_state = vorb::ui::ScreenState::CHANGE_PREVIOUS;
    }

    if (ImGui::SliderFloat2("World Center", &mGenData.mWorldCenter.x, 0, 32768.f, "%.1f")) {
        mIsDirty = true;
    }
    if (ImGui::SliderInt("River Count", &mGenData.mDesiredRiverCount, 0, 300)) {
        mIsDirty = true;
    }
    if (ImGui::InputText("Seed", mGenData.mSeed, MAX_WORLD_GEN_SEED_SIZE)) {
        mGenData.mSeedHashed = mGenData.getSeedHash(mGenData.mSeed);
        mGenData.mSeedInt = mGenData.getSeedInt(mGenData.mSeedHashed);
        mWorldData->worldSeed = mGenData.mSeedInt;
        mIsDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Randomize")) {
        RandomGenerator randGen(std::chrono::system_clock::now().time_since_epoch().count() % (ui64)UINT32_MAX);
        for (size_t i = 0; i < MAX_WORLD_GEN_SEED_SIZE - 1; ++i) {
            mGenData.mSeed[i] = RANDOM_SEED_VALUES[randGen.getRandomUint() % RANDOM_SEED_VALUES_COUNT];
        }
        mGenData.mSeed[MAX_WORLD_GEN_SEED_SIZE - 1] = '\0';
        mGenData.mSeedHashed = mGenData.getSeedHash(mGenData.mSeed);
        mGenData.mSeedInt = mGenData.getSeedInt(mGenData.mSeedHashed);
        mWorldData->worldSeed = mGenData.mSeedInt;
        mIsDirty = true;
    }
    if (ImGui::SliderInt2("Corrupt Count Range", &mGenData.mCorruptSpawnCountRange.x, 0, 600)) {
        mIsDirty = true;
    }
    if (ImGui::SliderInt("Biome Grow Passes", &mGenData.mBiomeGrowPassCount, 0, 128)) {
        mIsDirty = true;
    }
    if (ImGui::SliderFloat("Continent Radius", &mGenData.mContinentRadius, 1000.0f, 16000.0f, "%.1f")) {
        mGenData.mContinentRadiusSq = SQ(mGenData.mContinentRadius);
        mIsDirty = true;
    }

    //if (mGenState == WorldGenScreenState::Done) {
    if (mScreenShader->isLoaded()) {
        if (ImGui::Button("Reload Generation Shaders")) {
            PreciseTimer loadTimer;
            vg::ShaderManager::disposeProgram("terrain_base");
            vg::ShaderManager::disposeProgram("generation_map.vertgeneration_map.frag");
            ShaderLoader::clearCachedProgram("generation_map.vert", "generation_map.frag");
            AssetHandleBasePtr newAssetPtrA = MaterialShaderRepository::get().reloadAsset(CStrToken("terrain_base"));
            AssetHandleBasePtr newAssetPtrB = MaterialShaderRepository::get().reloadAsset(CStrToken("generation_map"));
            AssetLoader& loader = AssetLoader::getInstance();
            while (!newAssetPtrA->isLoaded() || !newAssetPtrB->isLoaded()) {
                loader.update();
                RenderContext::getInstance().updateRenderThreadProcs();
            }

            mScreenShader = static_unique_pointer_cast<AssetHandle<MaterialShaderDef>>(std::move(newAssetPtrB));
            LOG_DEBUG("Reload shaders took {} ms", loadTimer.stop());
        }
        if (ImGui::Button("REGENERATE") || mIsDirty) {
            mIsDirty = false;
            beginWorldGeneration();
        }
    }

    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGui::EndFrame();
}

void WorldGenScreen::initWorldData() {
    if (!mWorldData) {
        mWorldData = std::make_unique<HostWorldData>();
        mWorldData->worldWidth = WorldDefaults::DEFAULT_WORLD_WIDTH_TILES;
    }
    mWorldData->heightmapGrid = std::make_unique<HostHeightmapGrid>(mWorldData->worldWidth);
    mWorldData->biomeGrid = std::make_unique<BiomeGrid>(mWorldData->worldWidth);
    mWorldData->markupGrid = std::make_unique<WorldMarkupGrid>(mWorldData->worldWidth, mWorldData->worldSeed);
    mWorldData->ownershipGrid = std::make_unique<OwnershipGrid>(mWorldData->worldWidth, *mWorldData->markupGrid);
    mWorldData->roadGrid = std::make_shared<RoadGrid>(mWorldData->worldWidth);
    mWorldData->tileGrid = std::make_shared<SimChunkTileGrid>(mWorldData->worldWidth);
    mWorldData->worldSeed = mGenData.mSeedInt;

    mTotalPatches = mWorldData->heightmapGrid->getTotalPatches();

    mGenState = WorldGenScreenState::Generating;
    
    mGenTimer.start();
    mPatchPixelDims = SCREEN_TEXTURE_RES / mWorldData->heightmapGrid->getSpatialGrid2D().getGridWidthCells();
    // Generate as fast as GPU can handle
    m_app->getWindow().setTemporaryUnlimitedFPS(true);
    mWorldGenerator->beginGeneration(*mWorldData, mGenData, mWorldData->worldWidth / HEIGHTMAP_QUAD_SIZE, [this]() {
        assert(mGenState != WorldGenScreenState::Done);
        mGenState = WorldGenScreenState::Done;
        m_app->getWindow().setTemporaryUnlimitedFPS(false);
        LOG_DEBUG("Generation finished in {} ms", mGenTimer.stop());
    });

}

void WorldGenScreen::updateDockspace()
{
    ImGuiViewport* viewport;
    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    {
        viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    }
    // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background and handle the pass-thru hole, so we ask Begin() to not render a background.
    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
        window_flags |= ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar();
    ImGui::PopStyleVar(2);
    ImGuiID dockspaceId = ImGui::GetID("GenDockspace");
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockspace_flags);
    //ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoCloseButton | ImGuiDockNodeFlags_NoWindowMenuButton);

    // Rebuild Dockspace

    if (mRebuildDockspace)
    {
        mRebuildDockspace = false;
        ImGuiID rootId = dockspaceId;
        ImGui::DockBuilderRemoveNode(rootId); // clear any previous layout
        ImGui::DockBuilderAddNode(rootId, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(rootId, viewport->Size);

        //ImGuiID dockIdUp;
        auto dockIdLeft = ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.2f, nullptr, &dockspaceId);
       
        // we now dock our windows into the docking node we made above
        ImGui::DockBuilderDockWindow("Controls", dockIdLeft);
        ImGui::DockBuilderDockWindow("World Generator", dockspaceId);
        ImGui::DockBuilderFinish(rootId);
    }

    ImGui::End(); // End dockspace
}

void WorldGenScreen::beginWorldGeneration() {
    if (mWorldGenerator) {
        mWorldGenerator->cleanup();
    }
    else {
        mWorldGenerator = std::make_unique<WorldDataGenerator>();
    }
    mGenState = WorldGenScreenState::Idle;

    initWorldData();

    mCancelled = false;
    assert(!sGameWorld);

    mRiverDebugMesh.reset();
    mRiverDebugVisitedMesh.reset();
    mRiverDebugLocalGroupMesh.reset();
    mCurrentBodyBorderMesh.reset();
    mPrevCharacterRequest.reset();
    mCurrentCharacterRequest.reset();
    mCharacterQuadMesh.reset();
    mNeedsRebuildCharacterQuadMesh = true;
    mSelectedBody = UINT32_MAX;
    mNeedsNewBorderMesh = true;
}

void WorldGenScreen::updateCamera()
{
    assert(mCamera);
    // TDOO: DeltaTime
    mCamera->update();
}

void WorldGenScreen::renderMapView() {
    const MaterialShaderDef* def = mScreenShader->tryGetLoadedAsset();
    if (!def) {
        return;
    }

    updateCamera();

    mMapScreenGBuffer->use();
    MaterialRenderer::bindMaterialShaderForRender(*def, nullptr);

    const i32v2 dims = i32v2(mWorldData->heightmapGrid->getSpatialGrid2D().getGridWidthCells() * HEIGHTMAP_VERT_WIDTH_PER_PATCH);
    //glProgramUniform2iv(def->mProgram.getID(), def->getUniform("unHeightDataDims"), 1, &dims.x);
    glUniformMatrix4fv(def->getUniform("unInverseVP"), 1, GL_FALSE, &mCamera->getInverseVPMatrix()[0][0]);
    glUniform2f(def->getUniform("unPosition"), mCamera->getPosition().x, mCamera->getPosition().y);
    glUniform2f(def->getUniform("unSpawnPoint"), mWorldData->playerStart.x, mWorldData->playerStart.y);
    glUniform1f(def->getUniform("unZoom"), mCamera->getZoom());
    glUniform1i(def->getUniform("unShowBiomes"), mShowBiomes);
    glUniform1i(def->getUniform("unShowHeight"), mShowHeight);

    glBindTextureUnit(0, mWorldGenerator->getHeightTexture());
    glBindTextureUnit(1, mWorldGenerator->getBiomeTexture());

    sGlobalFullTriangleVAO.draw();

    if (mShowRivers) {
        debugDrawRivers();
    }
    if (mShowChunks) {
        debugDrawChunkLines();
    }
    if (mShowSelectedBody) {
        debugDrawBodyBorder();
    }
    if (mDrawCharacters) {
        debugDrawCharacters();
    }
    else {
        mCurrentCharacterRequest.reset();
        mPrevCharacterRequest.reset();
    }

    mWorldGenerator->currentStageDebugDraw(*mCamera);

    mMapScreenGBuffer->unuse();
}

void WorldGenScreen::updateMouseInput() {
    const int middleDrag = -ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle).y;
    ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
    f32 zoom = mCamera->getZoom();
    zoom += middleDrag * 0.01f * zoom;
    zoom = glm::clamp(zoom, 1.0f, 100.f);
    mCamera->setZoom(zoom);


    const ImVec2 rightDrag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
    ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);

    const f32 MOVE_SPEED = 0.0005f;
    f32v3 pos = mCamera->getPosition();
    pos += f32v3(-rightDrag.x * MOVE_SPEED, rightDrag.y * MOVE_SPEED, 0.f) / mCamera->getZoom();
    pos.x = glm::clamp(pos.x, -1.0f, 1.0f);
    pos.y = glm::clamp(pos.y, -1.0f, 1.0f);
    mCamera->setXYPos(pos);


    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && mCurrentTextureSize.x > 0) {
        ImVec2 cursorPosPixels = ImGui::GetMousePos() - ImGui::GetItemRectMin();
        f32v2 uv = f32v2(cursorPosPixels.x, cursorPosPixels.y) / f32v2(mCurrentTextureSize);
        uv.y = 1.0 - uv.y;
        f32v2 worldPos = mCamera->screenToWorld(uv * 2.0f - 1.0f);
        mWorldData->playerStart = (worldPos + 1.0f) * 0.5f;
        LOG_INFO("{} {} {} {}", cursorPosPixels.x, cursorPosPixels.y, mWorldData->playerStart.x, mWorldData->playerStart.y);
    }
}

void WorldGenScreen::debugDrawRivers() {
    if (mWorldGenerator->getBlackboard().mRiverSplinesGenerated) {
        const auto& paths = mWorldGenerator->getBlackboard().mRiverPaths;
        if (!mRiverDebugMesh) {
            const color4 riverColor = color::Aqua;
            const color4 failRiverColor = color::Red;
            mRiverDebugMesh = std::make_unique<LineMesh>();
            mRiverDebugVisitedMesh = std::make_unique<LineMesh>();
            mRiverDebugLocalGroupMesh = std::make_unique<LineMesh>();
            const f32 worldWidthVerts = mWorldData->heightmapGrid->getWidthPatches() * HEIGHTMAP_VERT_WIDTH_PER_PATCH;
          
            size_t totalPoints = 0;
            size_t totalVisited = 0;
            size_t totalGroups = 0;
            for (const RiverPath& path : paths) {
                totalPoints += path.splinePath.size();
                totalVisited += path.visited.size();
                totalGroups += path.affectedLocalGroups.size();
            }

            std::vector<LineVertex> pathLines;
            std::vector<LineVertex> visitedPoints;
            std::vector<LineVertex> groupQuads;
            pathLines.reserve(totalPoints);
            visitedPoints.reserve(totalVisited);
            groupQuads.reserve(totalGroups * 8); // 2x4 segments

            for (const RiverPath& path : paths) {
                // Path
                for (f32v2 point : path.splinePath) {
                    LineVertex& vertex = pathLines.emplace_back();
                    // [-1, 1]
                    vertex.pos.x = ((f32)point.x / worldWidthVerts) * 2.0f - 1.0f;
                    vertex.pos.y = ((f32)point.y / worldWidthVerts) * 2.0f - 1.0f;
                    vertex.pos.z = 0.0f;
                    vertex.color = path.isValid ? riverColor : failRiverColor;
                }
                // Visited
                int k = 0;
                for (i16v2 point : path.visited) {
                    LineVertex& vertex = visitedPoints.emplace_back();
                    // [-1, 1]
                    vertex.pos.x = ((f32)point.x / worldWidthVerts) * 2.0f - 1.0f;
                    vertex.pos.y = ((f32)point.y / worldWidthVerts) * 2.0f - 1.0f;
                    vertex.pos.z = 0.0f;
                    vertex.color = path.isValid ? riverColor : failRiverColor;
                    vertex.color.a = 100;
                    vertex.color.g = int(k * 0.05) % 255;
                    vertex.color.b = int(k * 0.05) % 255;
                    ++k;
                }
                // Local group
                const f32 CELL_WIDTH = (RIVER_CARVE_LOCAL_GROUP_SIZE / (f32)worldWidthVerts) * 2.0f;
                const color4 cellColor = color4(255, 0, 0, 25);
                for (auto&& it : path.affectedLocalGroups) {
                    // Add a quad with duplicate verts
                    const f32v2 pos = (f32v2(it.first) / worldWidthVerts) * 2.0f - 1.0f;
                    // Left segment
                    groupQuads.emplace_back(LineVertex{ f32v3(pos.x, pos.y, 0.0f), cellColor });
                    groupQuads.emplace_back(LineVertex{ f32v3(pos.x, pos.y + CELL_WIDTH, 0.0f), cellColor });
                    // Top segment
                    groupQuads.emplace_back(LineVertex{ f32v3(pos.x, pos.y + CELL_WIDTH, 0.0f), cellColor });
                    groupQuads.emplace_back(LineVertex{ f32v3(pos.x + CELL_WIDTH, pos.y + CELL_WIDTH, 0.0f), cellColor });
                    // Right segment
                    groupQuads.emplace_back(LineVertex{ f32v3(pos.x + CELL_WIDTH, pos.y + CELL_WIDTH, 0.0f), cellColor });
                    groupQuads.emplace_back(LineVertex{ f32v3(pos.x + CELL_WIDTH, pos.y, 0.0f), cellColor });
                    // Bottom segment
                    groupQuads.emplace_back(LineVertex{ f32v3(pos.x + CELL_WIDTH, pos.y, 0.0f), cellColor });
                    groupQuads.emplace_back(LineVertex{ f32v3(pos.x, pos.y, 0.0f), cellColor });
                }
            }

            mRiverDebugMesh->initialize(pathLines);
            mRiverDebugVisitedMesh->initialize(visitedPoints);
            mRiverDebugLocalGroupMesh->initialize(groupQuads);
        }

        const MaterialShaderDef* def = mDebugLineShader->tryGetLoadedAsset();
        if (def) {
            MaterialRenderer::bindMaterialShaderForRender(*def);

            glEnable(GL_LINE_SMOOTH); // Antialiasing
            glUniformMatrix4fv(def->getUniform("unVP"), 1, GL_FALSE, &mCamera->getVPMatrix()[0][0]);
            glUniform2f(def->getUniform("unCameraPos"), mCamera->getPosition().x, mCamera->getPosition().y);
            glLineWidth(2.0f + mCamera->getZoom());
            mRiverDebugMesh->bind();
            int start = 0;
            for (const RiverPath& path : paths) {
                if (path.splinePath.size()) {
                    mRiverDebugMesh->drawLineStrip(start, path.splinePath.size());
                    start += path.splinePath.size();
                }
            }

            start = 0;
            mRiverDebugVisitedMesh->bind();
            glPointSize(0.75f + mCamera->getZoom() * 0.25f);
            for (const RiverPath& path : paths) {
                if (path.visited.size()) {
                    mRiverDebugVisitedMesh->drawPoints(start, path.visited.size());
                    start += path.visited.size();
                }
            }
            glLineWidth(mCamera->getZoom());
            start = 0;
            mRiverDebugLocalGroupMesh->bind();
            for (const RiverPath& path : paths) {
                if (path.affectedLocalGroups.size()) {
                    mRiverDebugLocalGroupMesh->drawLines(start, path.affectedLocalGroups.size() * 8);
                    start += path.affectedLocalGroups.size() * 8;
                }
            }
        }
    }
}

void WorldGenScreen::debugDrawChunkLines()
{
    if (!mChunkDebugMesh) {
        mChunkDebugMesh = std::make_unique<LineMesh>();
        const f32 worldWidthChunks = mWorldData->worldWidth / CHUNK_WIDTH;
        const f32 step = 2.0f / worldWidthChunks;
        const color4 cellColor = color4(0, 50, 100, 100);

        std::vector<LineVertex> pathLines;
        pathLines.reserve((worldWidthChunks + 1) * 4);
      
        for (ui32 x = 0; x <= worldWidthChunks; ++x) {
            pathLines.emplace_back(LineVertex{ f32v3(-1.0f + step * x, -1.0f, 0.0f), cellColor });
            pathLines.emplace_back(LineVertex{ f32v3(-1.0f + step * x, 1.0f, 0.0f), cellColor });
        }
        for (ui32 y = 0; y <= worldWidthChunks; ++y) {
            pathLines.emplace_back(LineVertex{ f32v3(-1.0f, -1.0f + step * y, 0.0f), cellColor });
            pathLines.emplace_back(LineVertex{ f32v3(1.0f, -1.0f + step * y, 0.0f), cellColor });
        }
 
        mChunkDebugMesh->initialize(pathLines);
    }

    const MaterialShaderDef* def = mDebugLineShader->tryGetLoadedAsset();
    if (def) {
        MaterialRenderer::bindMaterialShaderForRender(*def);

        glEnable(GL_LINE_SMOOTH); // Antialiasing
        glUniformMatrix4fv(def->getUniform("unVP"), 1, GL_FALSE, &mCamera->getVPMatrix()[0][0]);
        glUniform2f(def->getUniform("unCameraPos"), mCamera->getPosition().x, mCamera->getPosition().y);
        glLineWidth(2.0f + mCamera->getZoom());
        mChunkDebugMesh->bind();
        mChunkDebugMesh->drawLines();
    }
}

void WorldGenScreen::debugDrawBodyBorder() {
    if (mSelectedBody == UINT32_MAX) {
        return;
    }

    if (mNeedsNewBorderMesh) {
        if (!mCurrentBodyBorderMesh) {
            mCurrentBodyBorderMesh = std::make_unique<AxisAlignedQuadMesh>();
            mDebugQuadShader = MaterialShaderRepository::get().getAssetHandle(CStrToken("map_debug_quads"));
        }
        const WorldBodyMarkupData& data = mWorldData->markupGrid->getBodyData(mSelectedBody);
        std::vector<AxisAlignedQuadData> quads;
        quads.resize(data.borderBlocks.size());
        f32v4 borderColor = data.isLand() ? color::White.toVec4() : color::LightBlue.toVec4();
        borderColor.a = 0.75f;
        const f32v2 dims = f32v2((2.0f * BLOCK_WIDTH) / mWorldData->worldWidth) * 2.f; // A little bigger
        for (size_t i = 0; i < quads.size(); ++i) {
            quads[i].color = borderColor; // TODO: Real Faction color
            quads[i].dims = dims;
            quads[i].pos = (((f32v2(data.borderBlocks[i]) + 0.5f) * (f32)BLOCK_WIDTH) / (f32)mWorldData->worldWidth) * 2.0f - 1.0f;
        }
        mCurrentBodyBorderMesh->initialize(quads);
        mNeedsNewBorderMesh = false;
    }
    // Draw
    const MaterialShaderDef* def = mDebugQuadShader->tryGetLoadedAsset();
    if (def) {
        MaterialRenderer::bindMaterialShaderForRender(*def);
        mCurrentBodyBorderMesh->bind(BUFFER_BASE_DEBUG_MESH_GENERIC_SSBO);
        glUniformMatrix4fv(def->getUniform("unVP"), 1, GL_FALSE, &mCamera->getVPMatrix()[0][0]);
        glUniform2f(def->getUniform("unCameraPos"), mCamera->getPosition().x, mCamera->getPosition().y);
        mCurrentBodyBorderMesh->drawQuads();
    }
}

void WorldGenScreen::debugDrawCharacters() {
    World* worldPtr = mWorldGenerator->tryGetWorld();
    if (!worldPtr) {
        return;
    }
    HostSimContext* simContext = worldPtr->tryGetHostSimContext();
    if (!simContext) {
        return;
    }
    SimThread* simThread = simContext->tryGetSimThread();
    if (!simThread) {
        return;
    }


    if (!mCurrentCharacterRequest) {
        mCurrentCharacterRequest = std::make_shared<SimThreadEntityRequest>();
        simThread->requestAllCharacters(mCurrentCharacterRequest);
    }
    else {
        if (mCurrentCharacterRequest->filled) {
            mNeedsRebuildCharacterQuadMesh = true;
            std::swap(mCurrentCharacterRequest, mPrevCharacterRequest);
            // When done, characters wont be moving anymore so dont need to be rebuilt
           
            // Re-use memory if we can
            if (!mCurrentCharacterRequest) {
                mCurrentCharacterRequest = std::make_shared<SimThreadEntityRequest>();
            } 
            if (mGenState == WorldGenScreenState::Done) {
                mCurrentCharacterRequest->filled = false; // Force permanently to never rebuild
            }
            else {
                simThread->requestAllCharacters(mCurrentCharacterRequest);
                LOG_CRITICAL("FILLED {}", mPrevCharacterRequest->entities.size());
            }
        }
    }

    if (mPrevCharacterRequest) {
        if (mNeedsRebuildCharacterQuadMesh) {
            if (!mCharacterQuadMesh) {
                mCharacterQuadMesh = std::make_unique<AxisAlignedQuadMesh>();
                mDebugQuadShader = MaterialShaderRepository::get().getAssetHandle(CStrToken("map_debug_quads"));
            }
            std::vector<AxisAlignedQuadData> quads;
            quads.resize(mPrevCharacterRequest->entities.size());
            const f32v4 factionColor = color::Cyan.toVec4();
            for (size_t i = 0; i < quads.size(); ++i) {
                quads[i].color = factionColor; // TODO: Real Faction color
                quads[i].dims = f32v2(0.001f);
                quads[i].pos = (f32v2(mPrevCharacterRequest->entities[i].pos) / (f32)mWorldData->worldWidth) * 2.0f - 1.0f;
            }
            mCharacterQuadMesh->initialize(quads);
        }
        // Draw
        const MaterialShaderDef* def = mDebugQuadShader->tryGetLoadedAsset();
        if (def) {
            MaterialRenderer::bindMaterialShaderForRender(*def);
            mCharacterQuadMesh->bind(BUFFER_BASE_DEBUG_MESH_GENERIC_SSBO);
            glUniformMatrix4fv(def->getUniform("unVP"), 1, GL_FALSE, &mCamera->getVPMatrix()[0][0]);
            glUniform2f(def->getUniform("unCameraPos"), mCamera->getPosition().x, mCamera->getPosition().y);
            mCharacterQuadMesh->drawQuads();
        }
    }
}
