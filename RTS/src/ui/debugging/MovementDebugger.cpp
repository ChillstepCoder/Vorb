#include "stdafx.h"
#include "MovementDebugger.h"

#include "options/GlobalMovementSettings.h"

void MovementDebugger::updateAndRenderImGui(bool *pOpen) {
    if (ImGui::Begin("Movement Debugger", pOpen, ImGuiWindowFlags_NoDocking)) {

        ImGui::SeparatorText("Settings Tweaker");
        ImGui::SliderFloat("Speed Multiplier", &GlobalMovementSettings::speedMult, 0.0f, 10.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Acceleration Multiplier", &GlobalMovementSettings::accelMult, 0.0f, 10.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Jump Power", &GlobalMovementSettings::jumpPower, 0.0f, 10.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Rotate Speed", &GlobalMovementSettings::rotateSpeed, 0.0f, 10.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::End();
    }
}
