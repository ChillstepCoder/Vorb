#include "stdafx.h"
#include "EditorOnlyScreen.h"

#include "screens/ScreenState.h"
#include "resources/AssetLoader.h"
#include "rendering/RenderContext.h"
#include "resources/MaterialRepository.h"
#include "resources/ResourceManager.h"

#include "ui/UIContext.h"
#include "camera/Camera3D.h"
#include "options/DebugOptions.h"

#include "App.h"

#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_opengl3.h>

#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/BlendState.h>

#include "input/InputDispatcher.h"

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
    sDebugOptions.mShowEditor = true;
    UIContext::getInstance().onEditorOpen();
}

void EditorOnlyScreen::onExit(const vui::GameTime& gameTime) {
    sDebugOptions.mShowEditor = false;
    // Notify panels that we are losing or gaining context
    UIContext::getInstance().onEditorClose();
}

void EditorOnlyScreen::update(const vui::GameTime& gameTime) {
    if (vui::InputDispatcher::key.isKeyDown(VKEY_ESCAPE)) {
        m_state = vorb::ui::ScreenState::CHANGE_PREVIOUS;
    }

    static bool wasReloadPressed = false;
    if (vui::InputDispatcher::key.isKeyDown(VKEY_R) && vui::InputDispatcher::key.isKeyDown(VKEY_LSHIFT)) {
        if (!wasReloadPressed) {
            Services::ResourceManager::ref().reloadMaterials(vui::InputDispatcher::key.isKeyDown(VKEY_LCTRL));
            wasReloadPressed = true;
        }
    } else {
        wasReloadPressed = false;
    }

    // Keep preloading assets
    AssetLoader::getInstance().update();
    RenderContext::getInstance().updateRenderThreadProcs();
}

void EditorOnlyScreen::draw(const vui::GameTime& gameTime) {

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    vg::DepthState::FULL.set();
    vg::BlendState::set(vg::BlendStateType::ALPHA);

    glClearColor(0.5f, 0.5f, 0.5f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // TODO: hmm this is a lot of stuff to remember
    MaterialRepository::get().bindMaterialBuffer();

    vui::GameWindow& window = m_app->getWindow();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame((SDL_Window*)window.getHandle());
    ImGui::NewFrame();

    Camera3D camera;
    const bool isShowEditor = sDebugOptions.mShowEditor;
    sDebugOptions.mShowEditor = true;
    UIContext::getInstance().updateAndRenderUI(nullptr, gameTime.elapsedSec, camera);
    sDebugOptions.mShowEditor = isShowEditor;

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGui::EndFrame();
}
