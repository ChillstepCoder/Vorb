#include "stdafx.h"
#include "ItemEditorViewportPanel.h"

void ItemEditorViewportPanel::updateAndRenderPrimaryControls(f32 ySize)
{
    ImGui::BeginChild("Item Editor Controls", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
    ImGui::Separator();
   // updateAndRenderSharedControls();
   // ImGui::Separator();
   
    if (updateAndRenderImguiControls(*mAssetData)) {
        // Mark dirty
        LOG_CRITICAL("TODO MARK DIRTY");
    }

    ImGui::EndChild();
}
