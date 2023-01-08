#include "stdafx.h"
#include "ModelEditorPanel.h"

#include "definitions/ModelDef.h"

#include "resources/ResourceManager.h"
//#include "resources/ModelRepository.h"
#include "rendering/MaterialShaderManager.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/post_process/ShadowLodDetail.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/DepthState.h>

#include "camera/SimpleCamera.h"


ModelEditorPanel::ModelEditorPanel()
{
}

ModelEditorPanel::~ModelEditorPanel()
{
}

bool ModelEditorPanel::updateAndRender() {
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
    if (mGBuffer == nullptr) {
        initGBuffer(imageDims);
    }

    glDisable(GL_CULL_FACE);
    vg::DepthState::FULL.set();

    mGBuffer->useGeometry();
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    renderGrid();
    renderModelToTexture();
    mGBuffer->unuse();

    const ImVec2 uv0(0, 1);
    const ImVec2 uv1(1, 0);
    const ImVec2 dims(imageDims.x, imageDims.y);
    ImGui::Image((ImTextureID)mGBuffer->getGeometryTexture(), dims, uv0, uv1);

    ImGui::End();

    return isOpen;
}

void ModelEditorPanel::updateAndRenderControls(f32 ySize)
{
    ImGui::BeginChild("Model Editor Controls", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
    ImGui::Text("Model Editor Controls");
    ImGui::Separator();
    updateAndRenderDrawModeControl();
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
        Model3D& mModel = mCurrentModel->mModel;
        ImGui::Text("Polygons %d", mModel.getMesh()->mMainMesh.mLODData.getDrawInfoForLOD(MeshLODLevel(mLod)).indexCount / 3);
    }

    ImGui::EndChild();
}

void ModelEditorPanel::renderModelToTexture() {
    if (!mCurrentModel) {
        return;
    }

    ResourceManager& resourceManager = Services::ResourceManager::ref();

    // Render model
    if (mCurrentModel->mRig == nullptr) {
        const MaterialShader* staticModelMaterial = nullptr;

        switch (mDrawMode) {
            case EditorViewportDrawMode::Default:
                staticModelMaterial = resourceManager.getMaterialShaderManager().getMaterialShader("editor_model");
                break;
            case EditorViewportDrawMode::Wireframe:
                staticModelMaterial = resourceManager.getMaterialShaderManager().getMaterialShader("mesh_wireframe");
                break;
            case EditorViewportDrawMode::Normals:
                staticModelMaterial = resourceManager.getMaterialShaderManager().getMaterialShader("mesh_normals");
                break;
            default:
                assert(false);
                break;
        }
        static_assert(e_cast(EditorViewportDrawMode::COUNT) == 3);

        VGUniform unVP = staticModelMaterial->getUniform("unVP");
        MaterialRenderer::bindMaterialForRender(*staticModelMaterial);

        glUniformMatrix4fv(unVP, 1, false, &(camera->getViewProjectionMatrix()[0][0]));

        Model3D& mModel = mCurrentModel->mModel;
        mModel.getMesh()->draw(MeshLODLevel(mLod));
    }
    else {
        // Skinned mesh render
        const MaterialShader* staticModelMaterial = nullptr;

        switch (mDrawMode) {
            case EditorViewportDrawMode::Default:
                staticModelMaterial = resourceManager.getMaterialShaderManager().getMaterialShader("editor_model");
                break;
            case EditorViewportDrawMode::Wireframe:
                staticModelMaterial = resourceManager.getMaterialShaderManager().getMaterialShader("mesh_wireframe");
                break;
            case EditorViewportDrawMode::Normals:
                staticModelMaterial = resourceManager.getMaterialShaderManager().getMaterialShader("mesh_normals");
                break;
            default:
                assert(false);
                break;

                VGUniform unVP = staticModelMaterial->getUniform("unVP");
                MaterialRenderer::bindMaterialForRender(*staticModelMaterial);

                glUniformMatrix4fv(unVP, 1, false, &(camera->getViewProjectionMatrix()[0][0]));

                Model3D& mModel = mCurrentModel->mModel;
                mModel.getMesh()->draw(MeshLODLevel(mLod));
        }
        static_assert(e_cast(EditorViewportDrawMode::COUNT) == 3);
    }

}
