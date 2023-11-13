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

    initWorldData();

    mCancelled = false;
    assert(!sGameWorld);
}

void WorldGenScreen::onExit(const vui::GameTime& gameTime) {
    if (mCancelled) {
        sGameWorld.reset();
    }
    else {
        // Initialize world
        sGameWorld = std::make_unique<World>(WorldNetMode::Host, WorldDefaults::DEFAULT_WORLD_WIDTH_TILES, WorldGeneratorType::Default, mWorldData.get());
    }
    mWorldData.reset();
}

void WorldGenScreen::update(const vui::GameTime& gameTime)
{

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
    ImGui::End();

    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
    if (ImGui::Button("Confirm")) {
        m_state = vorb::ui::ScreenState::CHANGE_NEXT;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        mCancelled = true;
        m_state = vorb::ui::ScreenState::CHANGE_PREVIOUS;
    }
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGui::EndFrame();
}

void WorldGenScreen::initWorldData() {
    mWorldData = std::make_unique<HostWorldData>();
    mWorldData->heightmapGrid = std::make_unique<HostHeightmapGrid>(WorldDefaults::DEFAULT_WORLD_WIDTH_TILES);
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
