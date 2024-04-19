#include "stdafx.h"
#include "EditorOnlyScreen.h"

#include "screens/ScreenState.h"

#include "ui/UIContext.h"
#include "camera/Camera3D.h"

#include "App.h"

#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_opengl3.h>

#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/BlendState.h>

#include <Vorb/ui/InputDispatcher.h>

EditorOnlyScreen::EditorOnlyScreen(App* const app) : IAppScreen<App>(app) {

}

EditorOnlyScreen::~EditorOnlyScreen() {

}

i32 EditorOnlyScreen::getNextScreen() const {
    return e_cast(RegisteredScreens::MainMenu);
}

i32 EditorOnlyScreen::getPreviousScreen() const {
    return e_cast(RegisteredScreens::MainMenu);
}

void EditorOnlyScreen::build() {

}

void EditorOnlyScreen::destroy(const vui::GameTime& gameTime) {

}

void EditorOnlyScreen::onEntry(const vui::GameTime& gameTime) {
    if (!UIContext::hasInstance()) {
        UIContext::initInstance(f32v2(m_app->getWindow().getWidth(), m_app->getWindow().getHeight()), static_cast<SDL_Window*>(m_app->getWindow().getHandle()));
    }
}

void EditorOnlyScreen::onExit(const vui::GameTime& gameTime) {

}

void EditorOnlyScreen::update(const vui::GameTime& gameTime) {
    if (vui::InputDispatcher::key.isKeyPressed(VKEY_ESCAPE)) {
        m_state = vorb::ui::ScreenState::CHANGE_PREVIOUS;
    }
}

void EditorOnlyScreen::draw(const vui::GameTime& gameTime) {

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    vg::DepthState::NONE.set();
    vg::BlendState::set(vg::BlendStateType::ALPHA);

    glClearColor(0.5f, 0.5f, 0.5f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    vui::GameWindow& window = m_app->getWindow();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame((SDL_Window*)window.getHandle());
    ImGui::NewFrame();

    Camera3D camera;
    UIContext::getInstance().updateAndRenderUI(nullptr, gameTime.elapsedSec, camera);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGui::EndFrame();
}
