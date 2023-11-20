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

#include "world/World.h"
#include "world/WorldDefaults.h"
#include "generation/TerrainGenerator.h"

#include "rendering/MaterialShaderRepository.h"
#include "rendering/RenderContext.h"

#include "rendering/ShaderLoader.h"
#include <Vorb/graphics/ShaderManager.h>

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

}

void WorldGenScreen::destroy(const vui::GameTime& gameTime)
{

}

void WorldGenScreen::onEntry(const vui::GameTime& gameTime) {

    if (mFirstEntry) {
        Services::initHost();
    }
    mThreadpoolSizePostEntry = Services::Threadpool::ref().getSize();
    Services::Threadpool::ref().setSize(std::thread::hardware_concurrency() - 1);

    beginWorldGeneration();
   
}

void WorldGenScreen::onExit(const vui::GameTime& gameTime) {

    Services::Threadpool::ref().setSize(mThreadpoolSizePostEntry);

    if (mTerrainGenerator) {
        mTerrainGenerator.reset();
    }
    if (mCancelled) {
        sGameWorld.reset();
    }
    else {
        // Initialize world
        sGameWorld = std::make_unique<World>(WorldNetMode::Host, WorldDefaults::DEFAULT_WORLD_WIDTH_TILES, WorldGeneratorType::Default, mWorldData.get());
    }
    mWorldData.reset();

    //Force cycle a frame to avoid bug with deleting mScreenTexture as imgui uses it an extra frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame((SDL_Window*)m_app->getWindow().getHandle());
    ImGui::NewFrame();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGui::EndFrame();

    // Free screen texture after flushing imgui
    glDeleteTextures(1, &mScreenTexture);
    mScreenTexture = 0;
    mScreenTextureData = gli::texture2d();
}

void WorldGenScreen::update(const vui::GameTime& gameTime) {

    // Update tasks
    Services::Threadpool::ref().mainThreadUpdate();

    switch (mGenState) {
        case WorldGenScreenState::Idle:
            break;
        case WorldGenScreenState::GeneratingTerrain: {
            updateTerrainGen();
            break;
        }
        case WorldGenScreenState::Done:
            break;
        default:
            break;

    }
    static_assert(e_count(WorldGenScreenState) == 3);

}

void WorldGenScreen::draw(const vui::GameTime& gameTime)
{
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
    ImGui::Image((ImTextureID)mScreenTexture, ImVec2(minAvailable, minAvailable));
    ImGui::End();

    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
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
    if (ImGui::InputText("Seed", mGenData.mSeed, MAX_WORLD_GEN_SEED_SIZE)) {
        mIsDirty = true;
    }

    //if (mGenState == WorldGenScreenState::Done) {
        if (ImGui::Button("REGENERATE") || mIsDirty) {
            mIsDirty = false;
            // Reload compute shader
            //ShaderLoader::clearCachedProgram("terrain_base.comp");
            vg::ShaderManager::disposeProgram("terrain_base");
            AssetHandleBasePtr newAssetPtr = MaterialShaderRepository::get().reloadAsset(CStrToken("terrain_base"));
            AssetLoader& loader = AssetLoader::getInstance();
            while (!newAssetPtr->isLoaded()) {
                loader.update();
                Sleep(1);
                RenderContext::getInstance().updateRenderThreadProcs();
            }
            beginWorldGeneration();
        }
    //}


    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGui::EndFrame();
}

void WorldGenScreen::initWorldData() {
    if (!mWorldData) {
        mWorldData = std::make_unique<HostWorldData>();
        mWorldData->worldWidth = WorldDefaults::DEFAULT_WORLD_WIDTH_TILES;
        mWorldData->heightmapGrid = std::make_unique<HostHeightmapGrid>(mWorldData->worldWidth);
    }

    mTotalPatches = mWorldData->heightmapGrid->getTotalPatches();

    mGenState = WorldGenScreenState::GeneratingTerrain;
    mTerrainGenerator->init(*mWorldData->heightmapGrid, f32v2(mWorldData->worldWidth * 0.5f));
    
    mGenTimer.start();
    mPatchPixelDims = SCREEN_TEXTURE_RES / mWorldData->heightmapGrid->getSpatialGrid2D().getGridWidthCells();
    // Generate as fast as GPU can handle
    m_app->getWindow().setTemporaryUnlimitedFPS(true);
    mTerrainGenerator->generateBaseHeightmapGPU(mWorldData->worldWidth / HEIGHTMAP_QUAD_SIZE, [this](HeightmapPatchID finishedPatchID) {
        HostHeightmapGrid& heightGrid = *mWorldData->heightmapGrid;
        const SpatialGrid2D& grid = heightGrid.getSpatialGrid2D();
        const i32v2 patchWorldPos = grid.getGridXYFromID(finishedPatchID) * HEIGHTMAP_QUAD_WIDTH_PER_PATCH;
        ui8v4* filledData = new ui8v4[mPatchPixelDims * mPatchPixelDims];

        // Build pixels for patch
        ui8v4 pixel;
        const i32 heightStride = HEIGHTMAP_QUAD_WIDTH_PER_PATCH / mPatchPixelDims;
        for (int y = 0; y < mPatchPixelDims; ++y) {
            const i32 yPosOffset = y * heightStride;
            const int yOffset = y * mPatchPixelDims;
            for (int x = 0; x < mPatchPixelDims; ++x) {
                const f32 height = heightGrid.getHeightAtVertexForGeneration(patchWorldPos + i32v2(x * heightStride, yPosOffset));
                ColorRGB8 lerpColor;
                if (height < 0.0f) {
                    const f32 depthMult = glm::min(-height * 0.025f, 1.0f);
                    lerpColor.lerp(ColorRGB8(4, 119, 162), ColorRGB8(3, 66, 122), depthMult);
                    pixel = ui8v4(lerpColor.r, lerpColor.g, lerpColor.b, 255);
                }
                else {
                    const f32 heightMult = glm::min(height * 0.01f, 1.0f);
                    lerpColor.lerp(ColorRGB8(40, 98, 41), ColorRGB8(255, 255, 255), heightMult);
                    pixel = ui8v4(lerpColor.r, lerpColor.g, lerpColor.b, 255);
                }
                filledData[yOffset + x] = pixel;
            }
        }
        mFinishedTerrainGPUPatches.enqueue(std::make_pair(finishedPatchID, filledData));
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

void WorldGenScreen::updateTerrainGen()
{
    const TerrainGenerationState terrainState = mTerrainGenerator->tick();
    switch (terrainState) {
        case TerrainGenerationState::None:
        case TerrainGenerationState::GeneratingBaseHeightmap:
            break;
        case TerrainGenerationState::GeneratingBaseHeightmapDone:
            break;
        default:
            break;
    }
    static_assert(e_count(TerrainGenerationState) == 3);
    constexpr int BULK_SIZE = 128;

    std::pair<HeightmapPatchID, ui8v4*> finishedPatches[BULK_SIZE];
    if (size_t count = mFinishedTerrainGPUPatches.try_dequeue_bulk(finishedPatches, BULK_SIZE)) {
        for (size_t i = 0; i < count; ++i) {
            onPatchFinishedGPU(finishedPatches[i]);
        }
    }
}

void WorldGenScreen::onPatchFinishedGPU(std::pair<HeightmapPatchID, ui8v4*> data) {

    HostHeightmapGrid& heightGrid = *mWorldData->heightmapGrid;
    const SpatialGrid2D& grid = heightGrid.getSpatialGrid2D();
    i32v2 patchPos = grid.getGridXYFromID(data.first);
    glTextureSubImage2D(mScreenTexture, 0, patchPos.x * mPatchPixelDims, patchPos.y * mPatchPixelDims, mPatchPixelDims, mPatchPixelDims, GL_RGBA, GL_UNSIGNED_BYTE, data.second);


    ++mFinishedPatchCount;
    if (mFinishedPatchCount >= mTotalPatches) {
        mGenState = WorldGenScreenState::Done;
        m_app->getWindow().setTemporaryUnlimitedFPS(false);
        LOG_DEBUG("Generation finished in {} ms", mGenTimer.stop());
    }

    delete[] data.second;
}

void WorldGenScreen::beginWorldGeneration() {
    if (mTerrainGenerator) {
        mTerrainGenerator->cleanup();
        // Flush
        constexpr int BULK_SIZE = 128;
        std::pair<HeightmapPatchID, ui8v4*> finishedPatches[BULK_SIZE];
        while (mFinishedTerrainGPUPatches.try_dequeue_bulk(finishedPatches, BULK_SIZE));
    }
    else {
        mTerrainGenerator = std::make_unique<TerrainGenerator>(mGenData);
    }
    mGenState = WorldGenScreenState::Idle;
    mFinishedPatchCount = 0;

    initWorldData();
    initScreenTexture();

    mCancelled = false;
    assert(!sGameWorld);
}

void WorldGenScreen::initScreenTexture() {
    gli::extent2d dimensions{ SCREEN_TEXTURE_RES, SCREEN_TEXTURE_RES };
    mScreenTextureData = gli::texture2d(gli::FORMAT_RGBA8_UNORM_PACK8, dimensions, 1);
    memset(mScreenTextureData.data(), 255, SCREEN_TEXTURE_RES * SCREEN_TEXTURE_RES * 4);

    if (!mScreenTexture) {
        glCreateTextures(GL_TEXTURE_2D, 1, &mScreenTexture);
        glTextureStorage2D(
            mScreenTexture,
            1,           // one level, no mipmaps
            GL_RGBA8,    // internal format
            SCREEN_TEXTURE_RES,
            SCREEN_TEXTURE_RES
        );
        glTextureSubImage2D(mScreenTexture, 0, 0, 0, SCREEN_TEXTURE_RES, SCREEN_TEXTURE_RES, GL_RGBA, GL_UNSIGNED_BYTE, mScreenTextureData.data());
    }
}
