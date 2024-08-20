#include "stdafx.h"
#include "ItemEditorViewportPanel.h"

#include "definitions/ModelDef.h"
#include "rendering/Mesh/MeshDrawer.h"
#include "rendering/MaterialShaderDef.h"

#include "item/ItemRepository.h"

#include "ui/imgui_controls/ObjectVector.h"

void ItemEditorViewportPanel::updateAndRenderPrimaryControls(f32 ySize) {
    ImGui::BeginChild("Item Editor Controls", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
    ImGui::Separator();
    updateAndRenderSharedControls();
    ImGui::Separator();

    bool changed = false;
   
    if (mAssetData) {
        ImGui::Text(mAssetData->getName().toString().c_str());
        updateAndRenderSaveButton();
        ImGui::Separator();
        changed |= updateAndRenderImguiControls(*mAssetData);
        changed |= ImguiUtil::ObjectVector<ModelAssetRef>("Models", mAssetData->mModelRefs,
            [](ModelAssetRef& o, ui32 index) {
            bool changed = o.updateAndRenderImgui(std::to_string(index).c_str(), nullptr);
            return changed;
        });
        if (mAssetData->mModelRefs.size() > 1) {
            ImGui::SliderInt("Preview Model", &mPreviewItemModel, 0, mAssetData->mModelRefs.size() - 1);
        }
    }

    if (changed) {
        ItemRepository::get().onAssetChangedByEditor(mAssetData->getID());
    }
    
    ImGui::EndChild();
}

const MaterialShaderDef* ItemEditorViewportPanel::getShader() {
    return getModelRenderShader();
}

void ItemEditorViewportPanel::renderMesh() {
    if (mAssetData) {
        if (mAssetData->mModelRefs.size()) {
            if (mPreviewItemModel >= mAssetData->mModelRefs.size()) {
                mPreviewItemModel = 0;
            }

            AssetHandlePtr<ModelDef> modelHandle = static_unique_pointer_cast<AssetHandle<ModelDef>>(mAssetData->mModelRefs[mPreviewItemModel].getAssetHandleBase());
            if (const ModelDef* def = modelHandle->tryGetLoadedAsset()) {
                // TODO: Variants?
                renderMeshStatic(def, 0, 0, false, 0);
            }
        }
    }
}
