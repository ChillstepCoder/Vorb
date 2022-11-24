#include "stdafx.h"
#include "ModelEditorPanel.h"

#include "definitions/ModelDef.h"

#include "resources/ResourceManager.h"
//#include "resources/ModelRepository.h"
#include "rendering/MaterialManager.h"
#include "rendering/MaterialRenderer.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/DepthState.h>

#include <glm/gtx/rotate_vector.hpp>

ModelEditorPanel::ModelEditorPanel()
{

}

ModelEditorPanel::~ModelEditorPanel()
{

}

bool ModelEditorPanel::updateAndRender(const vg::GBuffer* activeGBuffer) {
    UNUSED(activeGBuffer);
    bool isOpen = true;
    ImGui::Begin("Model Editor", &isOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav);

    ImVec2 mouseDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
    ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
    updateCamera(f32v2(mouseDelta.x, mouseDelta.y));
    LOG_CRITICAL("{} {}", mouseDelta.x, mouseDelta.y);

    if (mCurrentModel) {
        ImGui::Text(mCurrentModel->mName);
    }
    else {
        ImGui::Text("NO MODEL");
    }
    f32v2 imageDims = f32v2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);

    // Lazy init so we don't use GPU memory when not in editor
    if (mModelGBuffer == nullptr) {
        initGBuffer(imageDims);
    }

    renderModelToTexture(imageDims.x / imageDims.y);

    const ImVec2 uv0(0, 1);
    const ImVec2 uv1(1, 0);
    const ImVec2 dims(imageDims.x, imageDims.y);
    ImGui::Image((ImTextureID)mModelGBuffer->getGeometryTexture(), dims, uv0, uv1);

    ImGui::End();

    return isOpen;
}

void ModelEditorPanel::updateCamera(f32v2 mouseDelta)
{
    if (mouseDelta.x) {
        f32v2 camPosXY(mCamPos.x, mCamPos.y);
        camPosXY = glm::rotate(camPosXY, mouseDelta.x * 0.01f);
        mCamPos.x = camPosXY.x;
        mCamPos.y = camPosXY.y;
    }
}

void ModelEditorPanel::initGBuffer(f32v2 imageDims) {
    mModelGBuffer = std::make_unique<vg::GBuffer>();

    vg::GBufferAttachment attachment;
    // Color
    attachment.format = vg::TextureInternalFormat::RGB8;
    attachment.number = FBO_GEOMETRY_COLOR;
    attachment.pixelFormat = vg::TextureFormat::RGB;
    attachment.pixelType = vg::TexturePixelType::UNSIGNED_BYTE;

    mModelGBuffer->setSize(imageDims);
    mModelGBuffer->init(attachment, nullptr, nullptr);
    mModelGBuffer->initDepth(vg::TextureInternalFormat::DEPTH_COMPONENT16);
    
    checkGlError("ModelEditorPanel::initGBuffer");
}

void ModelEditorPanel::renderModelToTexture(f32 aspectRatio) {
    if (!mCurrentModel) {
        return;
    }

    glDisable(GL_CULL_FACE);
    vg::DepthState::FULL.set();

    ResourceManager& resourceManager = Services::ResourceManager::ref();

    mModelGBuffer->useGeometry();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render model
    if (mCurrentModel->mModelType == Model3DType::STATIC) {
        const Material* staticModelMaterial = resourceManager.getMaterialManager().getMaterial("editor_model");
        VGUniform unCameraPos = staticModelMaterial->getUniform("unCameraPos");
        VGUniform unVP = staticModelMaterial->getUniform("unVP");
        MaterialRenderer::bindMaterialForRender(*staticModelMaterial);

        f32m4 V;
        f32m4 VP;

        V = glm::lookAt(mCamPos, f32v3(0.0f, 0.0f, 0.5f), f32v3(0.0f, 0.0f, 1.0f));
        VP = glm::perspective(glm::radians(90.0f), aspectRatio, 0.001f, 100.0f) * V;

        glUniform3fv(unCameraPos, 1, &mCamPos.x);
        glUniformMatrix4fv(unVP, 1, false, &(VP[0][0]));

        StaticModel3D& mModel = mCurrentModel->getStaticModel();
        mModel.getMesh()->draw();
    }

    mModelGBuffer->unuse();
    vg::DepthState::restorePrevious();
}
