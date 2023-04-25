#include "stdafx.h"
#include "TileEditorPanel.h"

#include "resources/ResourceManager.h"
#include "resources/ModelRepository.h"
#include "resources/MaterialRepository.h"
#include "resources/TileGrassRepository.h"
#include "rendering/MaterialShaderManager.h"
#include "rendering/MaterialRenderer.h"
#include "resources/TileRepository.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/GBuffer.h>

const ui32v2 PREVIEW_RESOLUTION(128);
constexpr f32 ROW_MIN_HEIGHT = 20.0f;
constexpr ImGuiTableFlags TABLE_FLAGS =
ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable
| ImGuiTableFlags_Sortable
| ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_NoBordersInBody
| ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY
| ImGuiTableFlags_SizingFixedFit;

TileEditorPanel::TileEditorPanel() {
    glCreateVertexArrays(1, &mPreviewVAO);
}

TileEditorPanel::~TileEditorPanel() {
    glDeleteVertexArrays(1, &mPreviewVAO);
}

TileEditorPanelResult TileEditorPanel::updateAndRender(float ySize) {

    TileEditorPanelResult returnValue = std::make_pair(TileEditorPanelResultCode::NONE, TileEditorPanelResultVariant{});

    ImGui::BeginChild("Tile Editor", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
    ImGui::Text("Tile Editor");
    if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None)) {

        updateAndRenderModelsTab(returnValue);
        updateAndRenderMaterialsTab(returnValue);
        updateAndRenderFoliageTab(returnValue);

        ImGui::EndTabBar();
    }

    ImGui::EndChild();

    return returnValue;
}

// =====================================================================================
// =                                    MODELS                                         =
// =====================================================================================
void TileEditorPanel::updateAndRenderModelsTab(TileEditorPanelResult& result) {
    if (ImGui::BeginTabItem("Models")) {
        constexpr f32 FIXED_WIDTH = 75.0f;

        ImGui::PushID(123);
        ModelRepository& modelRepository = Services::ResourceManager::ref().getModelRepository();
        // Submit table
        if (ImGui::BeginTable("modelTable", 4, TABLE_FLAGS, ImVec2(0, 0), 0.0f))
        {
            // Declare columns
            // We use the "user_id" parameter of TableSetupColumn() to specify a user id that will be stored in the sort specifications.
            // This is so our sort function can identify a column given our own identifier. We could also identify them based on their index!
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, FIXED_WIDTH);
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, FIXED_WIDTH);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, FIXED_WIDTH);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, FIXED_WIDTH);
            ImGui::TableSetupScrollFreeze(1, 1);

            ImGui::TableHeadersRow();

            ui32 ID = 250;
            for (auto&& it : modelRepository.mModelIdLookup) {
                ModelDef& def = *modelRepository.mModelDefs[it.second];
                ImGui::PushID(++ID);
                ImGui::TableNextRow(ImGuiTableRowFlags_None, ROW_MIN_HEIGHT);
                // Name
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(it.first.c_str());
                // ID
                ImGui::TableSetColumnIndex(1);
                char label[32];
                sprintf_s(label, "%04d", def.mModelId);
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
                    result.first = TileEditorPanelResultCode::EDIT_MODEL;
                    result.second = &def;
                }

                ImGui::PopID();

            }
            ImGui::EndTable();
        }
        ImGui::PopID();
        ImGui::EndTabItem();
    }
}

// =====================================================================================
// =                                    MATERIALS                                      =
// =====================================================================================
void TileEditorPanel::updateAndRenderMaterialsTab(TileEditorPanelResult& result)
{
    if (ImGui::BeginTabItem("Materials")) {

        constexpr f32 FIXED_WIDTH = 75.0f;
        ImGui::Text("Materials");
        MaterialRepository& materialRepository = Services::ResourceManager::ref().getMaterialRepository();

        // Submit table
        if (ImGui::BeginTable("materialTable", 4, TABLE_FLAGS, ImVec2(0, 0), 0.0f)) {

            // TODO: Sortable table https://pthom.github.io/imgui_manual_online/manual/imgui_manual.html
            /*ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs();
            if (sortSpecs && sortSpecs->SpecsDirty) {
                for (int i = 0; i < sortSpecs->SpecsCount; ++i) {
                    const ImGuiTableColumnSortSpecs& spec = sortSpecs->Specs[i];
                    spec.
                }
            }*/

            // Declare columns
            // We use the "user_id" parameter of TableSetupColumn() to specify a user id that will be stored in the sort specifications.
            // This is so our sort function can identify a column given our own identifier. We could also identify them based on their index!
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, FIXED_WIDTH * 2.0f);
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, FIXED_WIDTH);
            ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, FIXED_WIDTH);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, FIXED_WIDTH);
            ImGui::TableSetupScrollFreeze(1, 1);

            ImGui::TableHeadersRow();


            // Rendering
            const MaterialShader* previewShader = Services::ResourceManager::ref().getMaterialShaderManager().getMaterialShader("material_preview");
            vg::DepthState::NONE.set();
            MaterialRenderer::bindMaterialForRender(*previewShader);

            ui32 ID = 250;
            ui32 previewIndex = 0;
            for (auto&& it : materialRepository.mMaterialIDLookup) {
                MaterialGpuData& material = materialRepository.mMaterialGpuData[it.second];
                ImGui::PushID(++ID);
                ImGui::TableNextRow(ImGuiTableRowFlags_None, ROW_MIN_HEIGHT);

                // Name
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(it.first.c_str());

                // ID
                ImGui::TableSetColumnIndex(1);
                char label[32];
                sprintf_s(label, "%04d", it.second);
                ImGui::Text(label);

                // Preview
                ImGui::TableSetColumnIndex(2);
                const ImVec2 uv0(0, 1);
                const ImVec2 uv1(1, 0);
                const ImVec2 dims(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().x);
                const bool visibleImage = ImGui::IsRectVisible(dims);
                if (visibleImage) {
                    const VGTexture texture = renderMaterialPreview(previewShader, previewIndex++, material);
                    ImGui::Image((ImTextureID)texture, dims, uv0, uv1);
                }
                else {
                    ImGui::Image((ImTextureID)0, dims, uv0, uv1);
                }

                // Action
                ImGui::TableSetColumnIndex(3);
                if (ImGui::Button("Edit")) {
                    result.first = TileEditorPanelResultCode::EDIT_MATERIAL;
                    result.second = std::make_unique<MaterialHandle>(materialRepository.getMutableMaterialHandle(it.first));
                }

                ImGui::PopID();

            }

            vg::DepthState::restorePrevious();

            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }
}

void TileEditorPanel::updateAndRenderFoliageTab(TileEditorPanelResult& result) {
    if (ImGui::BeginTabItem("Foliage")) {

        constexpr f32 FIXED_WIDTH = 75.0f;
        ImGui::Text("Foliage");
        TileGrassRepository& grassRepository = Services::ResourceManager::ref().getTileGrassRepository();
        MaterialRepository& materialRepository = Services::ResourceManager::ref().getMaterialRepository();

        // Submit table
        if (ImGui::BeginTable("foliageTable", 4, TABLE_FLAGS, ImVec2(0, 0), 0.0f)) {

            // TODO: Sortable table https://pthom.github.io/imgui_manual_online/manual/imgui_manual.html
            /*ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs();
            if (sortSpecs && sortSpecs->SpecsDirty) {
                for (int i = 0; i < sortSpecs->SpecsCount; ++i) {
                    const ImGuiTableColumnSortSpecs& spec = sortSpecs->Specs[i];
                    spec.
                }
            }*/

            // Declare columns
            // We use the "user_id" parameter of TableSetupColumn() to specify a user id that will be stored in the sort specifications.
            // This is so our sort function can identify a column given our own identifier. We could also identify them based on their index!
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, FIXED_WIDTH * 2.0f);
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, FIXED_WIDTH);
            ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, FIXED_WIDTH);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, FIXED_WIDTH);
            ImGui::TableSetupScrollFreeze(1, 1);

            ImGui::TableHeadersRow();


            // Rendering
            const MaterialShader* previewShader = Services::ResourceManager::ref().getMaterialShaderManager().getMaterialShader("material_preview");
            vg::DepthState::NONE.set();
            MaterialRenderer::bindMaterialForRender(*previewShader);

            ui32 ID = 250;
            ui32 previewIndex = 0;
            for (auto&& it : grassRepository.mTileGrassData) {
                MaterialGpuData& material = materialRepository.mMaterialGpuData[it.mMaterialID];
                ImGui::PushID(++ID);
                ImGui::TableNextRow(ImGuiTableRowFlags_None, ROW_MIN_HEIGHT);

                // Name
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(it.mName.toString().c_str());

                // ID
                ImGui::TableSetColumnIndex(1);
                char label[32];
                sprintf_s(label, "%04d", it.mId);
                ImGui::Text(label);

                // Preview
                ImGui::TableSetColumnIndex(2);
                const ImVec2 uv0(0, 1);
                const ImVec2 uv1(1, 0);
                const ImVec2 dims(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().x);
                const bool visibleImage = ImGui::IsRectVisible(dims);
                if (visibleImage) {
                    const VGTexture texture = renderMaterialPreview(previewShader, previewIndex++, material);
                    ImGui::Image((ImTextureID)texture, dims, uv0, uv1);
                }
                else {
                    ImGui::Image((ImTextureID)0, dims, uv0, uv1);
                }

                // Action
                ImGui::TableSetColumnIndex(3);
                if (ImGui::Button("Edit")) {
                    result.first = TileEditorPanelResultCode::EDIT_FOLIAGE;
                    result.second = &it;
                }

                ImGui::PopID();

            }

            vg::DepthState::restorePrevious();

            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }
}

VGTexture TileEditorPanel::renderMaterialPreview(const MaterialShader* shader, int previewIndex, const MaterialGpuData& materialData) {
    // Allocate new gbuffer if needed
    assert(previewIndex <= (int)mMaterialPreviewGBuffers.size());
    if (previewIndex == (int)mMaterialPreviewGBuffers.size()) {
        vg::GBuffer& newGBuffer = *mMaterialPreviewGBuffers.emplace_back(std::make_unique<vg::GBuffer>(PREVIEW_RESOLUTION));

        newGBuffer.initAttachment(vg::GBufferAttachmentIndex::ALBEDO, vg::TextureInternalFormat::RGB8);
        newGBuffer.initDepth(vg::GBufferDepthFormat::DEPTH_16);

        checkGlError("TileEditorPanel::renderMaterialPreview");
    }
    vg::GBuffer& gBuffer = *mMaterialPreviewGBuffers[previewIndex];
    // Upload material uniforms
    GLint albedoUniform = glGetUniformLocation(shader->mProgram.getID(), "unMaterialData.albedoMap");
    assert(albedoUniform != -1);
    glUniform1ui64ARB(albedoUniform, materialData.albedoMap);

    // Render to texture
    gBuffer.use();
    glClear(GL_COLOR_BUFFER_BIT);

    glBindVertexArray(mPreviewVAO);
    glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, 6, 1, 0);

    gBuffer.unuse();
    return gBuffer.getAlbedoTexture();
}
