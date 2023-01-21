#include "stdafx.h"
#include "MaterialEditorPanel.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/DepthState.h>

#include "resources/ResourceManager.h"
//#include "resources/ModelRepository.h"
#include "rendering/MaterialShaderManager.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialUtils.h"

#include "camera/SimpleCamera.h"

MaterialEditorPanel::MaterialEditorPanel()
{
}

MaterialEditorPanel::~MaterialEditorPanel()
{
}

bool MaterialEditorPanel::updateAndRender()
{
    bool isOpen = true;
    ImGui::Begin("Material Editor", &isOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);

    ImVec2 mouseDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
    ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
    f32v2 imageDims = f32v2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);

    updateCamera(imageDims.x / imageDims.y);

    if (mCurrentMaterial.isValid()) {
        ImGui::Text(mCurrentMaterial.name.c_str());
    }
    else {
        ImGui::Text("NO MATERIAL");
    }

    // Lazy init so we don't use GPU memory when not in editor
    if (mGBuffers[0] == nullptr) {
        initGBuffers(imageDims);
    }

    glDisable(GL_CULL_FACE);
    vg::DepthState::FULL.set();

    mGBuffers[0]->use();
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    renderGrid();
    if (mCurrentMaterial.isValid()) {
        renderModelToTexture();
    }
    mGBuffers[0]->unuse();

    const ImVec2 uv0(0, 1);
    const ImVec2 uv1(1, 0);
    const ImVec2 dims(imageDims.x, imageDims.y);
    ImGui::Image((ImTextureID)mGBuffers[0]->getAlbedoTexture(), dims, uv0, uv1);

    ImGui::End();

    return isOpen;
}

void MaterialEditorPanel::updateAndRenderControls(f32 ySize) {
    ImGui::BeginChild("Material Editor Controls", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
    ImGui::Text("Material Editor Controls");
    ImGui::Separator();
    updateAndRenderDrawModeControl();
    ImGui::Separator();
    // Sphere
    // Plane
    // Cube
    // Cylinder
    // Custom Mesh
    // Draw mode
    static_assert(e_cast(PrimitiveShapeType::COUNT) == 4);
    const char* shapeTypes[e_cast(PrimitiveShapeType::COUNT)] = {
        "Sphere",
        "Plane",
        "Cube",
        "Cylinder"
    };
    if (ImGui::BeginCombo("Shape", shapeTypes[e_cast(mShapeType)])) {
        for (int i = 0; i < e_cast(PrimitiveShapeType::COUNT); ++i) {
            bool isSelected = e_cast(mShapeType) == i;
            ImGui::Selectable(shapeTypes[i], &isSelected);

            if (isSelected) {
                ImGui::SetItemDefaultFocus();
                mShapeType = (PrimitiveShapeType)i;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Separator();

    ImGui::EndChild();
}

void MaterialEditorPanel::renderModelToTexture() {
    Mesh& mesh = PrimitiveShapeMeshes::getOrGenerateShapeMesh(mShapeType);

    ResourceManager& resourceManager = Services::ResourceManager::ref();
    const MaterialShader* material = nullptr;
    switch (mDrawMode) {
        case EditorViewportDrawMode::Unlit:
        case EditorViewportDrawMode::Lit:
        case EditorViewportDrawMode::Normals:
        case EditorViewportDrawMode::UVs:
        case EditorViewportDrawMode::BlendTest: // TODO
        case EditorViewportDrawMode::EdgeTest: // TODO
        case EditorViewportDrawMode::PBRTest: // TODO
            material = resourceManager.getMaterialShaderManager().getMaterialShader("editor_material");
            break;
        case EditorViewportDrawMode::Wireframe:
            material = resourceManager.getMaterialShaderManager().getMaterialShader("mesh_wireframe");
            break;
        default:
            assert(false);
    }
    static_assert(e_cast(EditorViewportDrawMode::COUNT) == 8);


    MaterialRenderer::bindMaterialForRender(*material);
    VGUniform unVP = material->getUniform("unVP");
    VGUniform unPosOffset = material->getUniform("unPosOffset");
    glUniformMatrix4fv(unVP, 1, false, &(camera->getViewProjectionMatrix()[0][0]));
    glUniform4f(unPosOffset, 0.0f, 0.0f, 1.0f, 0.0f);

    if (mDrawMode != EditorViewportDrawMode::Wireframe) {
        MaterialUtils::uploadLightingUniforms(*material);
        VGUniform unRenderMode = material->getUniform("unRenderMode");
        VGUniform unMaterialIndex = material->getUniform("unMaterialIndex");

        glUniform1i(unRenderMode, (int)mDrawMode);
        glUniform1i(unMaterialIndex, mCurrentMaterial.materialId);
    }


    mesh.draw(MeshLODLevel(0));
}
