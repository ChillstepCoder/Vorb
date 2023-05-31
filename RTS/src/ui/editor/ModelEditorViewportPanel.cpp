#include "stdafx.h"
#include "ModelEditorViewportPanel.h"

#include "definitions/ModelDef.h"

#include "resources/ResourceManager.h"
//#include "resources/ModelRepository.h"
#include "rendering/MaterialShaderManager.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialUtils.h"
#include "rendering/Mesh/MeshDrawer.h"
#include "rendering/post_process/ShadowLodDetail.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/FullQuadVBO.h>

#include "camera/SimpleCamera.h"

ModelEditorViewportPanel::ModelEditorViewportPanel()
{
}

ModelEditorViewportPanel::~ModelEditorViewportPanel()
{
}

bool ModelEditorViewportPanel::updateAndRender() {
    bool isOpen = true;
    ImGui::Begin("Model Editor", &isOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);

    ImVec2 mouseDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
    ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
    f32v2 imageDims = f32v2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
    updateCamera(imageDims.x / imageDims.y);

    if (mCurrentModel) {
        ImGui::Text(mCurrentModel->mName);
    }
    else {
        ImGui::Text("NO MODEL");
    }

    // Lazy init so we don't use GPU memory when not in editor
    if (sGBuffers[0] == nullptr) {
        initGBuffers(imageDims);
    }
    
    renderCenterPanel(nullptr);

    ImGui::End();

    return isOpen;
}

void ModelEditorViewportPanel::updateAndRenderControls(f32 ySize)
{
    ImGui::BeginChild("Model Editor Controls", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
    ImGui::Text("Model Editor Controls");
    ImGui::Separator();
    updateAndRenderSharedControls();
    ImGui::Separator();

    if (mCurrentModel) {
        ImGui::Text("Name: %s", mCurrentModel->mName);
        if (ImGui::BeginCombo("Shadow detail", KEG_ENUM_STR(ShadowLodDetail, mCurrentModel->mShadowDetail))) {

            for (int i = e_cast(ShadowLodDetail::None); i <= e_cast(ShadowLodDetail::Highest); ++i) {
                bool isSelected = e_cast(mCurrentModel->mShadowDetail) == i;
                ImGui::Selectable(KEG_ENUM_STR(ShadowLodDetail, i), &isSelected);

                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                    if (e_cast(mCurrentModel->mShadowDetail) != i) {
                        mCurrentModel->mShadowDetail = (ShadowLodDetail)i;
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
        ImGui::Text("MeshCount %d", mCurrentModel->getNumMeshes());
        int polyCount = 0;
        for (int i = 0; i < mCurrentModel->getNumMeshes(); ++i) {
            const Mesh& mesh = mCurrentModel->getMesh(i);
            polyCount += mesh.mMainMesh.mLODData.getDrawInfoForLOD(MeshLODLevel(mLod)).indexCount / 3;
        }
        ImGui::Text("Polygons %d", polyCount);

        updateAndRenderTweakers();
    }

    ImGui::EndChild();
}

const MaterialShader* ModelEditorViewportPanel::getShader() {
    ResourceManager& resourceManager = Services::ResourceManager::ref();
    switch (mDrawMode) {
        case EditorViewportDrawMode::PBRTest:
            return resourceManager.getMaterialShaderManager().getMaterialShader("editor_model_pbr");
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
            return resourceManager.getMaterialShaderManager().getMaterialShader("editor_model");
        case EditorViewportDrawMode::Wireframe:
            return resourceManager.getMaterialShaderManager().getMaterialShader("mesh_wireframe");
        default:
            assert(false);
            break;
    }
    static_assert(e_cast(EditorViewportDrawMode::COUNT) == 12);
    return nullptr;
}

void ModelEditorViewportPanel::uploadCustomShaderUniforms(const MaterialShader* shader, ui32 availableTextureUnit) {
    UNUSED(shader, availableTextureUnit);
}

void ModelEditorViewportPanel::renderMesh() {
    if (mCurrentModel) {
        for (int i = 0; i < mCurrentModel->getNumMeshes(); ++i) {
            MeshDrawer::draw(mCurrentModel->getMesh(i).mMainMesh, MeshLODLevel(mLod));
        }
    }
}
