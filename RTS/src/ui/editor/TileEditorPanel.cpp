#include "stdafx.h"
#include "TileEditorPanel.h"

#include "resources/ResourceManager.h"
#include "resources/ModelRepository.h"
#include "resources/MaterialRepository.h"
#include "resources/TileGrassRepository.h"
#include "resources/FishRepository.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/MaterialRenderer.h"
#include "resources/TileRepository.h"
#include "resources/ParticleSystemRepository.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl2.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/GBuffer.h>

#include <Vorb/graphics/FullscreenTriangleVAO.h>

const ui32v2 PREVIEW_RESOLUTION(128);
constexpr f32 ROW_MIN_HEIGHT = 20.0f;
constexpr ImGuiTableFlags TABLE_FLAGS =
ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable
| ImGuiTableFlags_Sortable
| ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_NoBordersInBody
| ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY
| ImGuiTableFlags_SizingFixedFit;

TileEditorPanel::TileEditorPanel() {
}

TileEditorPanel::~TileEditorPanel() {
}

TileEditorPanelResult TileEditorPanel::updateAndRender(float ySize) {

    TileEditorPanelResult returnValue = std::make_pair(TileEditorPanelResultCode::NONE, TileEditorPanelResultVariant{});

    ImGui::BeginChild("Tile Editor", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
    ImGui::Text("Tile Editor");
    if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None)) {

        updateAndRenderModelsTab(returnValue);
        updateAndRenderMaterialsTab(returnValue);
        updateAndRenderFoliageTab(returnValue);
        updateAndRenderBiomeTab(returnValue);
        updateAndRenderFishingTab(returnValue);
        updateAndRenderParticlesTab(returnValue);

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
        ModelRepository& modelRepository = ModelRepository::get();
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
            modelRepository.forEachRegisteredAsset([&](ModelDef* def, const AssetRegistryEntry& entry) {
                ImGui::PushID(++ID);
                ImGui::TableNextRow(ImGuiTableRowFlags_None, ROW_MIN_HEIGHT);
                // Name
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(entry.mName.toString().c_str());
                // ID
                ImGui::TableSetColumnIndex(1);
                char label[32];
                sprintf_s(label, "%04d", entry.mID);
                ImGui::Text(label);
                // Type
                ImGui::TableSetColumnIndex(2);
                if (def) {
                    if (def->mRig) {
                        ImGui::Text("Skinned");
                    }
                    else {
                        ImGui::Text("Static");
                    }
                }
                else
                {
                    ImGui::Text("UNLOADED");
                }
                // Action
                ImGui::TableSetColumnIndex(3);
                if (ImGui::Button("Edit")) {
                    result.first = TileEditorPanelResultCode::EDIT_MODEL;
                    result.second = entry.mID;
                }

                ImGui::PopID();
                return false;
            });
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
    static AssetHandlePtr<MaterialShaderDef> shaderDef = MaterialShaderRepository::get().getAssetHandle(CStrToken("material_preview"));
    const MaterialShaderDef* previewShader = shaderDef->tryGetAsset();
    if (!previewShader) return;

    if (ImGui::BeginTabItem("Materials")) {

        constexpr f32 FIXED_WIDTH = 75.0f;
        ImGui::Text("Materials");
        MaterialRepository& materialRepository = MaterialRepository::get();

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
            vg::DepthState::NONE.set();
            MaterialRenderer::bindMaterialShaderForRender(*previewShader);

            ui32 ID = 250;
            ui32 previewIndex = 0;
            materialRepository.forEachRegisteredAsset([&](MaterialDef* def, const AssetRegistryEntry& entry) {
                ImGui::PushID(++ID);
                ImGui::TableNextRow(ImGuiTableRowFlags_None, ROW_MIN_HEIGHT);

                // Name
                ImGui::TableSetColumnIndex(0);
                ui32 strSize = 0;
                char nameBuf[MAX_CHARS_IN_STRTOKEN];
                entry.mName.toString(nameBuf, &strSize);
                ImGui::Text(nameBuf);

                // ID
                ImGui::TableSetColumnIndex(1);
                char label[32];
                sprintf_s(label, "%04d", entry.mID);
                ImGui::Text(label);

                // Preview
                ImGui::TableSetColumnIndex(2);
                const ImVec2 uv0(0, 1);
                const ImVec2 uv1(1, 0);
                const ImVec2 dims(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().x);
                const bool visibleImage = ImGui::IsRectVisible(dims);
                if (visibleImage && def) {
                    const MaterialGpuData& material = materialRepository.getMaterialGpuData(entry.mID);
                    const VGTexture texture = renderMaterialPreview(previewShader, previewIndex++, material);
                    ImGui::Image((ImTextureID)texture, dims, uv0, uv1);
                }
                else {
                    ImGui::Image((ImTextureID)0, dims, uv0, uv1);
                }

                // Action
                ImGui::TableSetColumnIndex(3);
                if (def) {
                    if (ImGui::Button("Edit")) {
                        result.first = TileEditorPanelResultCode::EDIT_MATERIAL;
                        result.second = entry.mID;
                    }
                }
                else {
                    // Force load
                    if (ImGui::Button("UNLOADED")) {
                        mForceLoadedAssets.addAssetHandle(materialRepository.getAssetHandle(entry.mID));
                    }
                }

                ImGui::PopID();
                return false;
            });

            vg::DepthState::restorePrevious();

            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }
}

void TileEditorPanel::updateAndRenderFoliageTab(TileEditorPanelResult& result) {

    static AssetHandlePtr<MaterialShaderDef> shaderDef = MaterialShaderRepository::get().getAssetHandle(CStrToken("material_preview"));
    const MaterialShaderDef* previewShader = shaderDef->tryGetAsset();
    if (!previewShader) return;

    if (ImGui::BeginTabItem("Foliage")) {

        constexpr f32 FIXED_WIDTH = 75.0f;
        ImGui::Text("Foliage");
        TileGrassRepository& grassRepository = TileGrassRepository::get();
        MaterialRepository& materialRepository = MaterialRepository::get();

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
            vg::DepthState::NONE.set();
            MaterialRenderer::bindMaterialShaderForRender(*previewShader);

            ui32 ID = 250;
            ui32 previewIndex = 0;
            grassRepository.forEachRegisteredAsset([&](TileGrassDef* def, const AssetRegistryEntry& entry) {
                ImGui::PushID(++ID);
                ImGui::TableNextRow(ImGuiTableRowFlags_None, ROW_MIN_HEIGHT);

                // Name
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(entry.mName.toString().c_str());

                // ID
                ImGui::TableSetColumnIndex(1);
                char label[32];
                sprintf_s(label, "%04d", entry.mID);
                ImGui::Text(label);

                // Preview
                ImGui::TableSetColumnIndex(2);
                const ImVec2 uv0(0, 1);
                const ImVec2 uv1(1, 0);
                const ImVec2 dims(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().x);
                const bool visibleImage = ImGui::IsRectVisible(dims);
                if (visibleImage && def) {
                    // TODO: Material ID is valid here even if asset is not loaded
                    MaterialGpuData& material = materialRepository.mMaterialGpuData[def->mMaterialID];
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
                    result.second = entry.mID;
                }

                ImGui::PopID();
                return false;
            });

            vg::DepthState::restorePrevious();

            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }
}

void TileEditorPanel::updateAndRenderBiomeTab(TileEditorPanelResult& result) {
    if (ImGui::BeginTabItem("Biome")) {
        ImGui::Text("Biome");
        if (ImGui::Button("Open Editor")) {
            result.first = TileEditorPanelResultCode::EDIT_BIOME;
        }
        ImGui::EndTabItem();
    }
}

void TileEditorPanel::updateAndRenderFishingTab(TileEditorPanelResult& result)
{
    if (ImGui::BeginTabItem("Fishing")) {

        FishRepository& fishRepository = FishRepository::get();

        ImGui::Text("Fishing");
        // Submit table
        constexpr f32 FIXED_WIDTH = 75.0f;
        if (ImGui::BeginTable("fishTable", 4, TABLE_FLAGS, ImVec2(0, 0), 0.0f)) {

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
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, FIXED_WIDTH);
            ImGui::TableSetupScrollFreeze(1, 1);

            ImGui::TableHeadersRow();

            ui32 ID = 250;
            ui32 previewIndex = 0;
            fishRepository.forEachRegisteredAsset([&](FishDef* def, const AssetRegistryEntry& entry) {
                ImGui::PushID(++ID);
                ImGui::TableNextRow(ImGuiTableRowFlags_None, ROW_MIN_HEIGHT);

                // Name
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(entry.mName.toString().c_str());

                // ID
                ImGui::TableSetColumnIndex(1);
                char label[32];
                sprintf_s(label, "%04d", entry.mID);
                ImGui::Text(label);

                // Action
                ImGui::TableSetColumnIndex(2);
                if (ImGui::Button("Edit")) {
                    result.first = TileEditorPanelResultCode::EDIT_FISH;
                    result.second = entry.mID;
                }
                ImGui::PopID();
                return false;
            });

            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }
}

void TileEditorPanel::updateAndRenderParticlesTab(TileEditorPanelResult& result) {
    if (ImGui::BeginTabItem("Particles")) {
        ImGui::Text("Particle Systems");
        if (ImGui::Button("Open Editor")) {
            result.first = TileEditorPanelResultCode::EDIT_PARTICLE;
            result.second = INVALID_ASSET_ID;
        }

        ParticleSystemRepository& particleSystemRepository = ParticleSystemRepository::get();

        // Submit table
        constexpr f32 FIXED_WIDTH = 50.0f;
        if (ImGui::BeginTable("particleSystemTable", 4, TABLE_FLAGS, ImVec2(0, 0), 0.0f)) {

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
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, FIXED_WIDTH);
            ImGui::TableSetupScrollFreeze(1, 1);

            ImGui::TableHeadersRow();

            ui32 ID = 2998;
            ui32 previewIndex = 0;

            particleSystemRepository.forEachRegisteredAsset([&](ParticleSystemDef* asset, const AssetRegistryEntry& entry) {
                ImGui::PushID(++ID);
                ImGui::TableNextRow(ImGuiTableRowFlags_None, ROW_MIN_HEIGHT);

                // Name
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(entry.mName.toString().c_str());

                // ID
                ImGui::TableSetColumnIndex(1);
                char label[32];
                sprintf_s(label, "%04d", entry.mID);
                ImGui::Text(label);

                // Action
                ImGui::TableSetColumnIndex(2);
                if (ImGui::Button("Edit")) {
                    result.first = TileEditorPanelResultCode::EDIT_PARTICLE;
                    result.second = entry.mID;
                }

                ImGui::PopID();
                return false;
            });

            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }
}

VGTexture TileEditorPanel::renderMaterialPreview(const MaterialShaderDef* shader, int previewIndex, const MaterialGpuData& materialData) {
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

    sGlobalFullTriangleVAO.draw();

    gBuffer.unuse();
    return gBuffer.getAlbedoTexture();
}
