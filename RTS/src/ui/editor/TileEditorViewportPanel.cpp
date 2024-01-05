#include "stdafx.h"
#include "TileEditorViewportPanel.h"

#include "definitions/ModelDef.h"
#include "rendering/Mesh/MeshDrawer.h"
#include "rendering/MaterialShaderDef.h"

void TileEditorViewportPanel::updateAndRenderPrimaryControls(f32 ySize)
{
    ImGui::BeginChild("Tile Editor Controls", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
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

const MaterialShaderDef* TileEditorViewportPanel::getShader()
{
    return getModelRenderShader();
}

void TileEditorViewportPanel::renderMesh()
{
    if (mAssetData) {
        if (mAssetData->modelRef.isValid()) {
            const MaterialShaderDef* shader = getShader();
            glUniform1i(shader->getUniform("unVariantIndex"), 0);
            glUniform4f(shader->getUniform("unPosOffset"), 0.0f, 0.0f, 0.0f, 0.0f);
            AssetHandlePtr<ModelDef> modelHandle = static_unique_pointer_cast<AssetHandle<ModelDef>>(mAssetData->modelRef.getAssetHandle());
            if (const ModelDef* modelDef = modelHandle->tryGetLoadedAsset()) {
                for (int i = 0; i < modelDef->getNumMeshes(); ++i) {
                    modelDef->getMesh(i).unbindModelAttribs(); // Editor doesnt use these
                    glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MODEL_VARIANT_DATA_UBO, modelDef->getMesh(i).mVariantDataUbo);
                    MeshDrawer::draw(modelDef->getMesh(i).mGpuData, MeshLODLevel(0));
                }
            }
        }
    }
}
