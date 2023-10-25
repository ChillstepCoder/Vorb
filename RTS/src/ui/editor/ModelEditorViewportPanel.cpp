#include "stdafx.h"
#include "ModelEditorViewportPanel.h"

#include "definitions/ModelDef.h"

#include "resources/ModelRepository.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialUtils.h"
#include "rendering/Mesh/MeshDrawer.h"
#include "rendering/post_process/ShadowLodDetail.h"

#include <imgui.h>
#include <imgui_internal.h>


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

void ModelEditorViewportPanel::updateAndRenderInternal(f32 elapsedSec) {
    renderCenterPanel(nullptr);
}

void ModelEditorViewportPanel::updateAndRenderPrimaryControls(f32 ySize)
{
    ImGui::BeginChild("Model Editor Controls", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
    ImGui::Text("Model Editor Controls");
    ImGui::Separator();
    updateAndRenderSharedControls();
    ImGui::Separator();

    if (mAssetData) {
        ImGui::Text("Name: %s", mAssetData->getName().toString().c_str());
        if (ImGui::BeginCombo("Shadow detail", ENUM_CSTR(ShadowLodDetail, mAssetData->mShadowDetail))) {

            for (int i = e_cast(ShadowLodDetail::None); i <= e_cast(ShadowLodDetail::Highest); ++i) {
                bool isSelected = e_cast(mAssetData->mShadowDetail) == i;
                ImGui::Selectable(ENUM_CSTR(ShadowLodDetail, (ShadowLodDetail)i), &isSelected);

                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                    if (e_cast(mAssetData->mShadowDetail) != i) {
                        mAssetData->mShadowDetail = (ShadowLodDetail)i;
                        mDirtyModelData = true;
                    }
                }
            }
            ImGui::EndCombo();
        }
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
            polyCount += mesh.mMainMesh.mLODData.getDrawInfoForLOD(MeshLODLevel(mLod)).indexCount / 3;
        }
        ImGui::Text("Polygons %d", polyCount);

        updateAndRenderTweakers();
    }

    ImGui::EndChild();
}

void ModelEditorViewportPanel::setModel(ModelID modelId)
{
    mAssetHandle = ModelRepository::get().getAssetHandle(modelId);
}

const MaterialShaderDef* ModelEditorViewportPanel::getShader() {
    switch (mDrawMode) {
        case EditorViewportDrawMode::PBRTest:
            if (!mPbrMaterial) mPbrMaterial = MaterialShaderRepository::get().getAssetHandle(CStrToken("editor_model_pbr"));
            return mPbrMaterial->tryGetLoadedAsset();
        case EditorViewportDrawMode::BlendTest:
        case EditorViewportDrawMode::EdgeTest:
        case EditorViewportDrawMode::Lit:
        case EditorViewportDrawMode::Unlit:
        case EditorViewportDrawMode::Normals:
        case EditorViewportDrawMode::Tangents:
        case EditorViewportDrawMode::AO:
        case EditorViewportDrawMode::Metallic:
        case EditorViewportDrawMode::Roughness:
        case EditorViewportDrawMode::UVs:
            if (!mEditorMaterial) mEditorMaterial = MaterialShaderRepository::get().getAssetHandle(CStrToken("editor_model"));
            return mEditorMaterial->tryGetLoadedAsset();
        case EditorViewportDrawMode::Wireframe:
            if (!mWireframeMaterial) mWireframeMaterial = MaterialShaderRepository::get().getAssetHandle(CStrToken("mesh_wireframe"));
            return mWireframeMaterial->tryGetLoadedAsset();
        default:
            assert(false);
            break;
    }
    static_assert(e_cast(EditorViewportDrawMode::COUNT) == 12);
    return nullptr;
}

void ModelEditorViewportPanel::uploadCustomShaderUniforms(const MaterialShaderDef* shader, ui32 availableTextureUnit) {
    UNUSED(shader, availableTextureUnit);
}

void ModelEditorViewportPanel::renderMesh() {
    if (mAssetData) {
        for (int i = 0; i < mAssetData->getNumMeshes(); ++i) {
            MeshDrawer::draw(mAssetData->getMesh(i).mMainMesh, MeshLODLevel(mLod));
        }
    }
}
