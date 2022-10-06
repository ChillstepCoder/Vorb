#include "stdafx.h"
#include "PauseMenuPanel.h"

#include "ui/ImguiUtil.hpp"

#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include <Vorb/ui/GameWindow.h>

// TODO: Util
const ImVec2 buttonSize(200, 50);


PauseMenuPanelResult PauseMenuPanel::updateAndRender() {
    const f32v2 panelDims(500.0f, 750.0f);
    const f32v2 screenRes = f32v2(sMainGameWindowHandle->getViewportDims());
    const f32v2 clampedScreenPos = sMainGameWindowHandle->clampBoxPosToWindow(screenRes * 0.5f - panelDims * 0.5f, panelDims);
    ImGui::SetNextWindowPos(ImVec2(clampedScreenPos.x, clampedScreenPos.y));
    ImGui::SetNextWindowSize(ImVec2(panelDims.x, panelDims.y));

    PauseMenuPanelResult result = PauseMenuPanelResult::NONE;

    ImGui::Begin("Main Menu", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);
    ImGui::Separator();
    if (ImguiUtil::ButtonCenteredOnLine("Resume", buttonSize)) {
        result = PauseMenuPanelResult::RESUME;
    }
    ImGui::Separator();
    if (ImguiUtil::ButtonCenteredOnLine("Exit to Main Menu", buttonSize)) {
        result = PauseMenuPanelResult::EXIT_TO_MENU;
    }
    ImGui::Separator();
    if (ImguiUtil::ButtonCenteredOnLine("Exit to Desktop", buttonSize)) {
        result = PauseMenuPanelResult::EXIT_TO_DESKTOP;
    }
    ImGui::Separator();

    ImGui::End();
    return result;
}
