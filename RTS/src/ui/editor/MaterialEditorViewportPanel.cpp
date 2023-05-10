#include "stdafx.h"
#include "MaterialEditorViewportPanel.h"

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

MaterialEditorViewportPanel::MaterialEditorViewportPanel()
{
}

MaterialEditorViewportPanel::~MaterialEditorViewportPanel()
{
}

bool MaterialEditorViewportPanel::updateAndRender()
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

    glDisable(GL_CULL_FACE);
    vg::DepthState::FULL.set();

    renderGrid(camera->getViewProjectionMatrix());
    renderCenterPanel(nullptr);


    ImGui::End();

    return isOpen;
}

void MaterialEditorViewportPanel::updateAndRenderControls(f32 ySize) {
    ImGui::BeginChild("Material Editor Controls", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
    ImGui::Text("Material Editor Controls");
    ImGui::Separator();
    updateAndRenderSharedControls();
    ImGui::Separator();
    // Sphere
    // Plane
    // Cube
    // Cylinder
    // Custom Mesh
    // Draw mode
    static_assert(e_cast(PrimitiveShapeType::COUNT) == 5);
    const char* shapeTypes[e_cast(PrimitiveShapeType::COUNT)] = {
        "IcoSphere",
        "UVSphere",
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

    ImGui::SliderFloat("UV Scale", &mUvScale, 0.0f, 4.0f);

    if (mCurrentMaterial.isValid()) {
        updateAndRenderTweakers();
    }
    ImGui::Separator();

    ImGui::EndChild();
}

const MaterialShader* MaterialEditorViewportPanel::getShader()
{
    ResourceManager& resourceManager = Services::ResourceManager::ref();
    switch (mDrawMode) {
        case EditorViewportDrawMode::PBRTest:
            return resourceManager.getMaterialShaderManager().getMaterialShader("editor_material_pbr");
        case EditorViewportDrawMode::Unlit:
        case EditorViewportDrawMode::Lit:
        case EditorViewportDrawMode::Normals:
        case EditorViewportDrawMode::Tangents:
        case EditorViewportDrawMode::AO:
        case EditorViewportDrawMode::Metallic:
        case EditorViewportDrawMode::Roughness:
        case EditorViewportDrawMode::UVs:
        case EditorViewportDrawMode::BlendTest:
        case EditorViewportDrawMode::EdgeTest:
            return resourceManager.getMaterialShaderManager().getMaterialShader("editor_material");
        case EditorViewportDrawMode::Wireframe:
            return resourceManager.getMaterialShaderManager().getMaterialShader("mesh_wireframe");
        default:
            assert(false);
    }
    static_assert(e_cast(EditorViewportDrawMode::COUNT) == 12);
    return nullptr;
}

void MaterialEditorViewportPanel::uploadCustomShaderUniforms(const MaterialShader* shader, ui32 availableTextureUnit) {
    UNUSED(availableTextureUnit);
    if (mDrawMode != EditorViewportDrawMode::Wireframe) {
        VGUniform unMaterialIndex = shader->getUniform("unMaterialIndex");
        glUniform1i(unMaterialIndex, mCurrentMaterial.materialId);
    }
    glUniform4f(shader->getUniform("unPosOffset"), 0.0f, 0.0f, 1.0f, 0.0f);
    if (const VGUniform* uniform = shader->tryGetUniform("unUvScale")) {
        glUniform2f(*uniform, mUvScale, mUvScale);
    }
}

void MaterialEditorViewportPanel::renderMesh() {
    glEnable(GL_CULL_FACE);
    Mesh& mesh = PrimitiveShapeMeshes::getOrGenerateShapeMesh(mShapeType);
    mesh.draw(MeshLODLevel(0));
}
