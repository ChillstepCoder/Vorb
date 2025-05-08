#include "stdafx.h"
#include "MaterialEditorViewportPanel.h"

#include <imgui.h>

#include "rendering/MaterialShaderRepository.h"
#include "rendering/mesh/MeshDrawer.h"

MaterialEditorViewportPanel::MaterialEditorViewportPanel()
{
    mRotate90 = false;
}

MaterialEditorViewportPanel::~MaterialEditorViewportPanel()
{
}

void MaterialEditorViewportPanel::updateAndRenderPrimaryControls(f32 ySize) {
    ImGui::BeginChild("Material Editor Controls", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
    ImGui::Text("Material Editor Controls");
    ImGui::Separator();
    updateAndRenderSharedControls();
    ImGui::Separator();
    static_assert(e_cast(PrimitiveShapeType::COUNT) == 4);
    const char* shapeTypes[e_cast(PrimitiveShapeType::COUNT)] = {
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

    if (mAssetData) {
        updateAndRenderTweakers();
    }
    ImGui::Separator();

    ImGui::EndChild();
}

const MaterialShaderDef* MaterialEditorViewportPanel::getShader()
{
    switch (mDrawMode) {
        case EditorViewportDrawMode::PBRTest: {
            if (!mPbrMaterial) mPbrMaterial = MaterialShaderRepository::get().getAssetHandle(CStrToken("editor_material_pbr"));
            return mPbrMaterial->tryGetLoadedAsset();
        }
        case EditorViewportDrawMode::Unlit:
        case EditorViewportDrawMode::Lit:
        case EditorViewportDrawMode::Normals:
        case EditorViewportDrawMode::Tangents:
        case EditorViewportDrawMode::AO:
        case EditorViewportDrawMode::Metallic:
        case EditorViewportDrawMode::Roughness:
        case EditorViewportDrawMode::UVs:
        case EditorViewportDrawMode::BlendTest:
        case EditorViewportDrawMode::EdgeTest: {
            if (!mEditorMaterial) mEditorMaterial = MaterialShaderRepository::get().getAssetHandle(CStrToken("editor_material"));
            return mEditorMaterial->tryGetLoadedAsset();
        }
        case EditorViewportDrawMode::Wireframe: {
            if (!mWireframeMaterial) mWireframeMaterial = MaterialShaderRepository::get().getAssetHandle(CStrToken("mesh_wireframe"));
            return mWireframeMaterial->tryGetLoadedAsset();
        }
        default:
            assert(false);
    }
    static_assert(e_cast(EditorViewportDrawMode::COUNT) == 12);
    return nullptr;
}

void MaterialEditorViewportPanel::uploadCustomShaderUniforms(const MaterialShaderDef* shader, ui32 availableTextureUnit) {
    if (!mAssetData) {
        return;
    }
    UNUSED(availableTextureUnit);
    if (mDrawMode != EditorViewportDrawMode::Wireframe) {
        VGUniform unMaterialIndex = shader->getUniform("unMaterialIndex");
        glUniform1i(unMaterialIndex, mAssetData->getID());
    }
    glUniform4f(shader->getUniform("unPosOffset"), 0.0f, 0.0f, 1.0f, 0.0f);
    if (const VGUniform* uniform = shader->tryGetUniform("unUvScale")) {
        glUniform2f(*uniform, mUvScale, mUvScale);
    }
}

void MaterialEditorViewportPanel::renderMesh() {
    glEnable(GL_CULL_FACE);
    Mesh& mesh = PrimitiveShapeMeshes::getOrGenerateShapeMesh(mShapeType);
    MeshDrawer::draw(mesh.mGpuData, MeshLODLevel(0));
}
