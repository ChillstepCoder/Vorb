#include "stdafx.h"
#include "ModelEditorViewportPanel.h"

#include "definitions/ModelDef.h"

#include "resources/ModelRepository.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialUtils.h"
#include "rendering/Mesh/MeshDrawer.h"
#include "rendering/post_process/ShadowDetail.h"
#include "rendering/mesh/LineMesh.h"

#include <imgui.h>
#include <imgui_internal.h>
#include "ui/ImguiUtil.hpp"

#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/FullscreenTriangleVAO.h>

#include "camera/SimpleCamera.h"

ModelEditorViewportPanel::ModelEditorViewportPanel()
{
}

ModelEditorViewportPanel::~ModelEditorViewportPanel()
{
}

void ModelEditorViewportPanel::updateAndRenderPrimaryControls(f32 ySize)
{
    ImGui::BeginChild("Model Editor Controls", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
    ImGui::Text("Model Editor Controls");
    ImGui::Separator();
    updateAndRenderSharedControls();
    ImGui::Separator();

    bool changed = false;

    if (mAssetData) {
        ImGui::Text("Name: %s", mAssetData->getName().toString().c_str());
        updateAndRenderSaveButton();
        changed |= updateAndRenderImguiControls(*mAssetData);

        ImGui::SliderInt("LOD", &mLod, e_cast(MeshLODLevel::Highest), e_cast(MeshLODLevel::Lowest));
        // TODO: Tooltip button utility
        ImGui::SameLine(); ImGui::Button("?");
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip("LOD is auto generated");
        }
        ImGui::Text("MeshCount %d", mAssetData->getNumMeshes());
        int polyCount = 0;
        for (int i = 0; i < mAssetData->getNumMeshes(); ++i) {
            const Mesh& mesh = mAssetData->getMesh(i);
            polyCount += mesh.mGpuData.mLODData.getDrawInfoForLOD(MeshLODLevel(mLod)).indexCount / 3;
        }
        ImGui::Text("Polygons %d", polyCount);
        ImGui::Separator();
        if (mAssetData->getNumMeshes()) {
            ImGui::Checkbox("Edit Submesh", &mShowSingle);
            if (mShowSingle) {
                ImGui::Spacing(); ImGui::SameLine();
                ImGui::Text("Submesh Edit");
                ImGui::Spacing(); ImGui::SameLine();
                ImGui::SliderInt("Index", &mSingleIndex, 0, mAssetData->getNumMeshes() - 1);
                ModelSubmeshData& subMeshData = mAssetData->mSubmeshesData[mSingleIndex];

                changed |= ImguiUtil::EnumCombo("Wind Type", subMeshData.windType);
                  
                ImGui::Separator();
            }
        }
        else {
            ImGui::Text("*EMPTY MODEL*");
        }

        updateAndRenderTweakers();
    }

    if (changed) {
        mDirtyModelData = true;
        ModelRepository::get().onAssetChangedByEditor(mAssetData->getID());
    }
    
    ImGui::EndChild();
}

const MaterialShaderDef* ModelEditorViewportPanel::getShader() {
    return getModelRenderShader();
}

void ModelEditorViewportPanel::uploadCustomShaderUniforms(const MaterialShaderDef* shader, ui32 availableTextureUnit) {
    UNUSED(shader, availableTextureUnit);
}

void ModelEditorViewportPanel::renderMesh() {
    if (mAssetData) {
      
        const MaterialShaderDef* shader = getShader();
        if (mShowSingle) {
            mSingleIndex = glm::min((int)mAssetData->getNumMeshes() - 1, mSingleIndex);
            MeshDrawer::draw(mAssetData->getMesh(mSingleIndex).mGpuData, MeshLODLevel(mLod));
        }
        else {
            // First LOD
            glUniform4f(shader->getUniform("unPosOffset"), 0.0f, 0.0f, 0.0f, 0.0f);
            for (int i = 0; i < mAssetData->getNumMeshes(); ++i) {
                MeshDrawer::draw(mAssetData->getMesh(i).mGpuData, MeshLODLevel(mLod));
            }
            int x = 1;
            const ModelLodParams& params = ModelRepository::get().getLodParams(mAssetData->getID());
            for (int l = e_cast(MeshLODLevel::Highest) + 1; l < e_count(MeshLODLevel); ++l) {
                glUniform4f(shader->getUniform("unPosOffset"), x * 5, sqrt(params.lodDistancesSQ[l - 1]), 0.0f, 0.0f);
                for (int i = 0; i < mAssetData->getNumMeshes(); ++i) {
                    MeshDrawer::draw(mAssetData->getMesh(i).mGpuData, MeshLODLevel(i));
                }
                ++x;
            }
        }

        // Render AABB
        AssetHandlePtr<MaterialShaderDef> handle = MaterialShaderRepository::get().getAssetHandle(CStrToken("simple_color"));
        if (const MaterialShaderDef* def = handle->tryGetLoadedAsset()) {
            if (!mAABBMesh) {
                mAABBMesh = std::make_unique<LineMesh>();
            }
            std::vector<LineVertex> aabbVerts;
            LineMeshBuilders::addAABBLines(aabbVerts, mAssetData->mAABB, color4(1.0f, 0.0f, 0.0f, 0.5f));
            mAABBMesh->initialize(aabbVerts);

            MaterialRenderer::bindMaterialShaderForRender(*def);
            glUniformMatrix4fv(def->getUniform("unVP"), 1, false, &(mCamera->getViewProjectionMatrix()[0][0]));
            mAABBMesh->bind();
            mAABBMesh->drawLines(0);
        }
    }
}
