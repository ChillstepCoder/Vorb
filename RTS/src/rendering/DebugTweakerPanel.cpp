#include "stdafx.h"
#include "DebugTweakerPanel.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include "options/DebugOptions.h"

DebugTweakerPanel::DebugTweakerPanel(const f32v2& screenDims) : mScreenDims(screenDims)
{
}

void DebugTweakerPanel::updateAndRender()
{
    constexpr float WINDOW_WIDTH = 400.0f;
    constexpr float WINDOW_HEIGHT = 200.0f;
    ImGui::SetNextWindowPos(ImVec2(mScreenDims.x - WINDOW_WIDTH - 5, 150.0f));
    ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH, WINDOW_HEIGHT));
    const ImVec2 buttonSize(WINDOW_WIDTH, 25);

    ImGui::Begin("Value Tweaker", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);
    
    ImGui::BeginGroup();
    ImGui::Text("Clouds");
    ImGui::SliderInt("Cloud Blur Passes", &sDebugOptions.mCloudBlurPasses, 0, 15);
    ImGui::SliderFloat("Cloud Blur Radius", &sDebugOptions.mCloudBlurRadius, 0.0f, 15.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Ambient", &sDebugOptions.mCloudAmbient, 0.0f, 1.0f);
    ImGui::EndGroup();

    ImGui::BeginGroup();
    ImGui::Text("Depth of Field");
    ImGui::SliderInt("DoF Blur Passes", &sDebugOptions.mDepthOfFieldBlurPasses, 0, 15);
    ImGui::SliderFloat("DoF Blur Radius", &sDebugOptions.mDepthOfFieldBlurRadius, 0.0f, 15.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
    ImGui::EndGroup();

    ImGui::End();
}
