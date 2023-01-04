#include "stdafx.h"
#include "TileEditorPanel.h"

#include "resources/ResourceManager.h"
#include "resources/ModelRepository.h"
#include "resources/MaterialRepository.h"
#include "resources/TileRepository.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

TileEditorPanelResult TileEditorPanel::updateAndRender(float ySize) {

    constexpr f32 rowMinHeight = 20.0f;
    static const ImGuiTableFlags tableFlags =
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable
        | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti
        | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_NoBordersInBody
        | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY
        | ImGuiTableFlags_SizingFixedFit;

    TileEditorPanelResult returnValue = std::make_pair(TileEditorPanelResultCode::NONE, TileEditorPanelResultVariant{});
    ui32 ID = 10;

    ImGui::BeginChild("Tile Editor", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
    ImGui::Text("Tile Editor");
    if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Models")) {

            ImGui::PushID(++ID);
            ModelRepository& modelRepository = Services::ResourceManager::ref().getModelRepository();
            // Submit table
            if (ImGui::BeginTable("modelTable", 4, tableFlags, ImVec2(0, 0), 0.0f))
            {
                // Declare columns
                // We use the "user_id" parameter of TableSetupColumn() to specify a user id that will be stored in the sort specifications.
                // This is so our sort function can identify a column given our own identifier. We could also identify them based on their index!
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 1.0f);
                ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 1.0f);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 1.0f);
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 1.0f);
                ImGui::TableSetupScrollFreeze(1, 1);

                ImGui::TableHeadersRow();

                for (auto&& it : modelRepository.mModelIdLookup) {
                    ModelDef& def = *modelRepository.mModelDefs[it.second];
                    ImGui::PushID(++ID);
                    ImGui::TableNextRow(ImGuiTableRowFlags_None, rowMinHeight);
                    // Name
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text(it.first.c_str());
                    // ID
                    ImGui::TableSetColumnIndex(1);
                    char label[32];
                    sprintf(label, "%04d", def.mModelId);
                    ImGui::Text(label);
                    // Type
                    ImGui::TableSetColumnIndex(2);
                    if (def.mRig) {
                        ImGui::Text("Skinned");
                    }
                    else {
                        ImGui::Text("Static");
                    }
                    // Action
                    ImGui::TableSetColumnIndex(3);
                    if (ImGui::Button("Edit")) {
                        returnValue.first = TileEditorPanelResultCode::EDIT_MODEL;
                        returnValue.second = &def;
                    }

                    ImGui::PopID();

                }
                ImGui::EndTable();
            }
            ImGui::PopID();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Materials")) {
            ImGui::Text("Materials");
            const MaterialRepository& materialRepository = Services::ResourceManager::ref().getMaterialRepository();
            // Submit table
            if (ImGui::BeginTable("materialTable", 4, tableFlags, ImVec2(0, 0), 0.0f))
            {
                // Declare columns
                // We use the "user_id" parameter of TableSetupColumn() to specify a user id that will be stored in the sort specifications.
                // This is so our sort function can identify a column given our own identifier. We could also identify them based on their index!
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 1.0f);
                ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 1.0f);
                //ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 1.0f);
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 1.0f);
                ImGui::TableSetupScrollFreeze(1, 1);

                ImGui::TableHeadersRow();

                for (auto&& it : materialRepository.mMaterialIDLookup) {
                    const MaterialData& def = materialRepository.mMaterials[it.second];
                    ImGui::PushID(++ID);
                    ImGui::TableNextRow(ImGuiTableRowFlags_None, rowMinHeight);
                    // Name
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text(it.first.c_str());
                    // ID
                    ImGui::TableSetColumnIndex(1);
                    char label[32];
                    sprintf(label, "%04d", def.mModelId);
                    ImGui::Text(label);
                    // Type
                    ImGui::TableSetColumnIndex(2);
                    if (def.mRig) {
                        ImGui::Text("Skinned");
                    }
                    else {
                        ImGui::Text("Static");
                    }
                    // Action
                    ImGui::TableSetColumnIndex(3);
                    if (ImGui::Button("Edit")) {
                        returnValue.first = TileEditorPanelResultCode::EDIT_MODEL;
                        returnValue.second = &def;
                    }

                    ImGui::PopID();

                }
                ImGui::EndTable();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::EndChild();

    return returnValue;
}
