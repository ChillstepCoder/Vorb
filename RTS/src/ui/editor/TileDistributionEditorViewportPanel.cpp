#include "stdafx.h"
#include "TileDistributionEditorViewportPanel.h"

#include "rendering/MaterialShaderRepository.h"
#include "rendering/MaterialRenderer.h"

#include <vorb/graphics/FullscreenTriangleVAO.h>

void TileDistributionEditorViewportPanel::updateAndRenderPrimaryControls(f32 ySize) {
    ImGui::BeginChild("Tile Distribution Controls", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
    ImGui::Separator();
    updateAndRenderSharedControls();
    ImGui::Separator();

    bool changed = false;
    if (mAssetData) {
        ImGui::Text(mAssetData->getName().toString().c_str());
        updateAndRenderSaveButton();
        ImGui::Separator();
        if (updateAndRenderImguiControls(*mAssetData)) {
            // Mark dirty
            changed = true;
        }
    }

    ImGui::EndChild();
}

void TileDistributionEditorViewportPanel::renderMesh() {
    if (!mImageShader) {
        mImageShader = MaterialShaderRepository::get().getAssetHandle(CStrToken("editor_tile_dist"));
    }

    if (const MaterialShaderDef* shaderDef = mImageShader->tryGetLoadedAsset()) {
        ui32 textureIndex = 0;
        MaterialRenderer::bindMaterialShaderForRender(*shaderDef, &textureIndex);

        sGlobalFullTriangleVAO.draw();
    }
}
