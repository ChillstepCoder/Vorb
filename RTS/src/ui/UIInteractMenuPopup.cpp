#include "stdafx.h"
#include "UIInteractMenuPopup.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>
//
//static bool view(NoiseFunction& n) {
//    bool changed = false;
//    ImGui::Text(n.label.c_str());
//    changed |= ImGui::SliderInt((n.label + "_octaves").c_str(), &n.octaves, 1, 15);
//    changed |= ImGui::SliderScalar((n.label + "_persist").c_str(), ImGuiDataType_Double, &n.persistence, &f64_zero, &f64_one);
//    changed |= ImGui::SliderScalar((n.label + "_freq").c_str(), ImGuiDataType_Double, &n.frequency, &f64_zero, &f64_one, "%.10f", ImGuiSliderFlags_Logarithmic);
//    changed |= ImGui::SliderScalarN((n.label + "_offset").c_str(), ImGuiDataType_Double, &(n.offset.x), 2, &MIN_NOISE_OFFSET, &MAX_NOISE_OFFSET, "%.10f");
//    return changed;
//}

UIInteractMenuPopup::UIInteractMenuPopup(const f32v2& screenPos, SDL_Window* window) : mScreenPos(screenPos), mWindow(window)
{

}

UIInteractMenuPopup::~UIInteractMenuPopup()
{

}

UIInteractMenuResultFlags UIInteractMenuPopup::updateAndRender()
{
    ui32 resultFlags = 0;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(mWindow);
    ImGui::NewFrame();
    const ImVec2 buttonSize(100, 25);
    
    ImGui::SetNextWindowPos(ImVec2(mScreenPos.x, mScreenPos.y));
    ImGui::SetNextWindowSize(ImVec2(buttonSize.x + 16, INTERACT_MENU_RESULT_COUNT * buttonSize.y + 45));
    ImGui::Begin("Interact", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);
    
    if (ImGui::Button("Go Here", buttonSize)) {
        resultFlags |= INTERACT_MENU_RESULT_PATHFIND;
    }
    if (ImGui::Button("Inspect", buttonSize)) {
        resultFlags |= INTERACT_MENU_RESULT_INSPECT;
    }
    if (ImGui::Button("Clear Tile", buttonSize)) {
        resultFlags |= INTERACT_MENU_RESULT_CLEAR_TILE;
    }
    if (ImGui::Button("Plant Tree", buttonSize)) {
        resultFlags |= INTERACT_MENU_RESULT_PLANT_TREE;
    }
    if (ImGui::Button("Build Wall", buttonSize)) {
        resultFlags |= INTERACT_MENU_RESULT_BUILD_WALL;
    }
    static_assert(INTERACT_MENU_RESULT_COUNT == 5, "update");
    /*sWorldGenData.mIsDirty |= view(sWorldGenData.mBaseNoise);
    sWorldGenData.mIsDirty |= view(sWorldGenData.mContinentOutlineNoise);
    sWorldGenData.mIsDirty |= view(sWorldGenData.mHumidityNoise);
    sWorldGenData.mIsDirty |= view(sWorldGenData.mTemperatureNoise);*/
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGui::EndFrame();
    return static_cast<UIInteractMenuResultFlags>(resultFlags);
}
