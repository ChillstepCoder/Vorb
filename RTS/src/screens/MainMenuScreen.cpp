#include "stdafx.h"
#include "MainMenuScreen.h"

#include "App.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/BlendState.h>

MainMenuScreen::MainMenuScreen(App* const app) : IAppScreen<App>(app) {

}

MainMenuScreen::~MainMenuScreen()
{

}

i32 MainMenuScreen::getNextScreen() const
{
    return 1;
}

i32 MainMenuScreen::getPreviousScreen() const
{
    return 0;
}

void MainMenuScreen::build()
{

}

void MainMenuScreen::destroy(const vui::GameTime& gameTime)
{

}

void MainMenuScreen::onEntry(const vui::GameTime& gameTime)
{

}

void MainMenuScreen::onExit(const vui::GameTime& gameTime)
{

}

void MainMenuScreen::update(const vui::GameTime& gameTime)
{

}

const ImVec2 buttonSize(200, 50);

bool ButtonCenteredOnLine(const char* label, ImVec2 size) {
    ImGuiStyle& style = ImGui::GetStyle();

    float avail = ImGui::GetContentRegionAvail().x;

    float off = (avail - size.x) * 0.5f;
    if (off > 0.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);

    return ImGui::Button(label, size);
}

void MainMenuScreen::draw(const vui::GameTime& gameTime)
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

    constexpr float WINDOW_WIDTH = 400.0f;
    constexpr float WINDOW_HEIGHT = 300.0f;
    const ui32v2 screenDims = window.getViewportDims();
    ImGui::SetNextWindowPos(ImVec2(screenDims.x * 0.5f - WINDOW_WIDTH * 0.5f, screenDims.y * 0.5 - WINDOW_HEIGHT * 0.5f));
    ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH, WINDOW_HEIGHT));

    ImGui::Begin("MAIN MENU", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);
    ui32 ID = 10;

    switch (mState) {
        case MainMenuState::MAIN:
            drawMainState();
            break;
        case MainMenuState::MULTIPLAYER:
            drawMultiplayerState();
            break;
        case MainMenuState::OPTIONS:
            break;
        default:
            break;

    }

    ImGui::End();


    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGui::EndFrame();
}


void MainMenuScreen::drawMainState() {
    ImGui::Spacing();
    if (ButtonCenteredOnLine("Singleplayer", buttonSize)) {
        m_state = vorb::ui::ScreenState::CHANGE_NEXT;
    }
    ImGui::Spacing();
    if (ButtonCenteredOnLine("Multiplayer", buttonSize)) {
        mState = MainMenuState::MULTIPLAYER;
    }
    ImGui::Spacing();
    if (ButtonCenteredOnLine("Options", buttonSize)) {

    }
    ImGui::Spacing();
    if (ButtonCenteredOnLine("Exit", buttonSize)) {
        m_state = vorb::ui::ScreenState::EXIT_APPLICATION;
    }
}

void MainMenuScreen::drawMultiplayerState() {
    ImGui::Spacing();
    if (ButtonCenteredOnLine("LAN", buttonSize)) {

    }
    ImGui::Spacing();
    if (ButtonCenteredOnLine("Online", buttonSize)) {

    }
    ImGui::Spacing();
    if (ButtonCenteredOnLine("Back", buttonSize)) {
        mState = MainMenuState::MAIN;
    }
}
