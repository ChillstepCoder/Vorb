#include "stdafx.h"
#include "TileEditorViewportPanel.h"

#include "definitions/ModelDef.h"
#include "rendering/Mesh/MeshDrawer.h"
#include "rendering/MaterialShaderDef.h"

#include "resources/ModelRepository.h"
#include "resources/TileRepository.h"

#include "ui/imgui_controls/ObjectVector.h"

void TileEditorViewportPanel::updateAndRenderPrimaryControls(f32 ySize)
{
    ImGui::BeginChild("Tile Editor Controls", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
    ImGui::Separator();
    updateAndRenderSharedControls();
    ImGui::Separator();

    bool changed = false;

    if (mAssetData) {
        ImGui::Text(mAssetData->getName().toString().c_str());
        updateAndRenderSaveButton();
        ImGui::Separator();
        if (ImGui::CollapsingHeader("Properties")) {
            if (updateAndRenderImguiControls(*mAssetData)) {
                changed = true;
            }
        }
        if (mAssetData->modelRef.isValid() && ImGui::CollapsingHeader("Model Variants")) {

            ImGui::SliderInt("Preview Variant", &mVariantIndex, 0, mAssetData->modelVariants.size() - 1);

            const ModelDef& modelDef = ModelRepository::get().getLoadedOrUnloadedAsset(mAssetData->modelRef.getAssetID());
            if (ImguiUtil::ObjectVector<ui8>("Variant Indices", mAssetData->modelVariants, [&modelDef](ui8& v, ui32 i) {
                int vi = v;
                bool changed = ImGui::SliderInt(std::to_string(i).c_str(), &vi, 0, modelDef.mVariants.size() - 1);
                v = (ui8)vi;
                return changed;
            }, true, 0)) {
                changed = true;
            }
        }
    }

    if (changed) {
        TileRepository::get().onAssetChangedByEditor(mAssetData->getID());
    }

    ImGui::EndChild();
}

const MaterialShaderDef* TileEditorViewportPanel::getShader() {
    return getModelRenderShader();
}

void TileEditorViewportPanel::renderMesh() {
    if (mAssetData) {
        if (mAssetData->modelRef.isValid()) {
            const MaterialShaderDef* shader = getShader();
            glUniform4f(shader->getUniform("unPosOffset"), 0.0f, 0.0f, 0.0f, 0.0f);
            glUniform1i(shader->getUniform("unVariantIndex"), mAssetData->modelVariants[mVariantIndex]);
            AssetHandlePtr<ModelDef> modelHandle = static_unique_pointer_cast<AssetHandle<ModelDef>>(mAssetData->modelRef.getAssetHandleBase());
            if (const ModelDef* modelDef = modelHandle->tryGetLoadedAsset()) {
                for (int i = 0; i < modelDef->getNumMeshes(); ++i) {
                    modelDef->getMesh(i).unbindModelAttribs(); // Editor doesnt use these
                    glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MODEL_VARIANT_DATA_UBO, modelDef->getMesh(i).mVariantDataUbo);
                    MeshDrawer::draw(modelDef->getMesh(i).mGpuData, MeshLODLevel(0));
                    modelDef->getMesh(i).bindModelAttribs(); // Main game does
                }
            }
        }
    }
}
