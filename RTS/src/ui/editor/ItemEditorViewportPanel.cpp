#include "stdafx.h"
#include "ItemEditorViewportPanel.h"

#include "definitions/ModelDef.h"
#include "rendering/Mesh/MeshDrawer.h"

void ItemEditorViewportPanel::updateAndRenderPrimaryControls(f32 ySize) {
    ImGui::BeginChild("Item Editor Controls", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
    ImGui::Separator();
    updateAndRenderSharedControls();
    ImGui::Separator();
   
    if (mAssetData) {
        ImGui::Text(mAssetData->getName().toString().c_str());
        updateAndRenderSaveButton();
        ImGui::Separator();
        if (updateAndRenderImguiControls(*mAssetData)) {
            // Mark dirty
            LOG_CRITICAL("TODO MARK DIRTY");
        }
    }

    ImGui::EndChild();
}

const MaterialShaderDef* ItemEditorViewportPanel::getShader() {
    return getModelRenderShader();
}

void ItemEditorViewportPanel::renderMesh() {
    if (mAssetData) {
        AssetHandlePtr<ModelDef> modelHandle = static_unique_pointer_cast<AssetHandle<ModelDef>>(mAssetData->mModelRef.getAssetHandle());
        if (const ModelDef* modelDef = modelHandle->tryGetLoadedAsset()) {
            for (int i = 0; i < modelDef->getNumMeshes(); ++i) {
                MeshDrawer::draw(modelDef->getMesh(i).mMainMesh, MeshLODLevel(0));
            }
        }
    }
}
