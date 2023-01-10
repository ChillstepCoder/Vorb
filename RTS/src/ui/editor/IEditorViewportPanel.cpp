#include "stdafx.h"
#include "IEditorViewportPanel.h"

#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/ui/InputDispatcher.h>

#include <glm/gtx/rotate_vector.hpp>

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include "resources/ResourceManager.h"
#include "rendering/MaterialShaderManager.h"
#include "rendering/MaterialRenderer.h"

#include "camera/SimpleCamera.h"

IEditorViewportPanel::IEditorViewportPanel() {
    positioner = std::make_unique<CameraPositioner_FirstPerson>(f32v3(0.0f, 5.0f, 1.5f), f32v3(0.0f, 0.0f, 0.5f), f32v3(0.0f, 0.0f, 1.0f));
    camera = std::make_unique<SimpleCamera>(*positioner);

    glCreateVertexArrays(1, &mGridVao);
}

IEditorViewportPanel::~IEditorViewportPanel() {

    glDeleteVertexArrays(1, &mGridVao);
}

void IEditorViewportPanel::updateAndRenderDrawModeControl() {
    // Draw mode
    const char* drawModes[e_cast(EditorViewportDrawMode::COUNT)] = {
        "Default",
        "Wireframe",
        "Normals",
        "UVs"
    };
    static_assert(e_cast(EditorViewportDrawMode::COUNT) == 4);
    if (ImGui::BeginCombo("Draw Mode", drawModes[e_cast(mDrawMode)])) {
        for (int i = 0; i < e_cast(EditorViewportDrawMode::COUNT); ++i) {
            bool isSelected = e_cast(mDrawMode) == i;
            ImGui::Selectable(drawModes[i], &isSelected);

            if (isSelected) {
                ImGui::SetItemDefaultFocus();
                mDrawMode = (EditorViewportDrawMode)i;
            }
        }
        ImGui::EndCombo();
    }
}

void IEditorViewportPanel::updateCamera(f32 aspectRatio) {

    // Controls
    positioner->movement_.forward_ = vui::InputDispatcher::key.isKeyPressed(VKEY_W);
    positioner->movement_.backward_ = vui::InputDispatcher::key.isKeyPressed(VKEY_S);
    positioner->movement_.left_ = vui::InputDispatcher::key.isKeyPressed(VKEY_A);
    positioner->movement_.right_ = vui::InputDispatcher::key.isKeyPressed(VKEY_D);
    positioner->movement_.up_ = vui::InputDispatcher::key.isKeyPressed(VKEY_SPACE);
    positioner->movement_.down_ = vui::InputDispatcher::key.isKeyPressed(VKEY_LCTRL);
    positioner->movement_.fastSpeed_ = vui::InputDispatcher::key.isKeyPressed(VKEY_LSHIFT);

    // TODO: Deltatime
    positioner->update(1.0f / 60.0f, f32v2(ImGui::GetMousePos().x / ImGui::GetWindowWidth(), ImGui::GetMousePos().y / ImGui::GetWindowHeight()), ImGui::IsMouseDown(ImGuiMouseButton_Right), aspectRatio);
}

void IEditorViewportPanel::initGBuffer(f32v2 imageDims) {
    mGBuffer = std::make_unique<vg::GBuffer>();

    vg::GBufferAttachment attachment;
    // Color
    attachment.format = vg::TextureInternalFormat::RGB8;
    attachment.number = FBO_GEOMETRY_COLOR;
    attachment.pixelFormat = vg::TextureFormat::RGB;
    attachment.pixelType = vg::TexturePixelType::UNSIGNED_BYTE;

    mGBuffer->setSize(imageDims);
    mGBuffer->init(attachment, nullptr, nullptr);
    mGBuffer->initDepth(vg::TextureInternalFormat::DEPTH_COMPONENT16);

    checkGlError("ModelEditorPanel::initGBuffer");
}

void IEditorViewportPanel::renderGrid() {
    vg::DepthState::NONE.set();

    ResourceManager& resourceManager = Services::ResourceManager::ref();
    const MaterialShader* gridMaterial = resourceManager.getMaterialShaderManager().getMaterialShader("grid");
    VGUniform unVP = gridMaterial->getUniform("unVP");
    MaterialRenderer::bindMaterialForRender(*gridMaterial);
    glUniformMatrix4fv(unVP, 1, false, &(camera->getViewProjectionMatrix()[0][0]));

    glBindVertexArray(mGridVao);
    glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, 6, 1, 0);
    glBindVertexArray(0);

    vg::DepthState::restorePrevious();
}
