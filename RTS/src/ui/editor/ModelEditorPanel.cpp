#include "stdafx.h"
#include "ModelEditorPanel.h"

#include "definitions/ModelDef.h"

#include "resources/ResourceManager.h"
//#include "resources/ModelRepository.h"
#include "rendering/MaterialManager.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/post_process/ShadowLodDetail.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include <Vorb/ui/InputDispatcher.h>
#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/DepthState.h>

#include <glm/gtx/rotate_vector.hpp>

#include "camera/SimpleCamera.h"

static CameraPositioner_FirstPerson positioner(f32v3(0.0f, 5.0f, 1.5f), f32v3(0.0f, 0.0f, 0.5f), f32v3(0.0f, 0.0f, 1.0f));
static SimpleCamera camera(positioner);

ModelEditorPanel::ModelEditorPanel()
{

}

ModelEditorPanel::~ModelEditorPanel()
{

}

bool ModelEditorPanel::updateAndRender(const vg::GBuffer* activeGBuffer) {
    UNUSED(activeGBuffer);
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
    if (mModelGBuffer == nullptr) {
        initGBuffer(imageDims);
    }

    glDisable(GL_CULL_FACE);
    vg::DepthState::FULL.set();

    mModelGBuffer->useGeometry();
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    renderGrid();
    renderModelToTexture();

    const ImVec2 uv0(0, 1);
    const ImVec2 uv1(1, 0);
    const ImVec2 dims(imageDims.x, imageDims.y);
    ImGui::Image((ImTextureID)mModelGBuffer->getGeometryTexture(), dims, uv0, uv1);

    ImGui::End();

    return isOpen;
}

void ModelEditorPanel::updateAndRenderControls(f32 ySize)
{
    ImGui::BeginChild("Model Editor Controls", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
    ImGui::Text("Model Editor Controls");
    ImGui::Separator();
    // Draw mode
    static_assert(e_cast(ModelEditorPanelDrawMode::COUNT) == 3);
    const char* drawModes[e_cast(ModelEditorPanelDrawMode::COUNT)] = {
        "Default",
        "Wireframe",
        "Normals"
    };
    if (ImGui::BeginCombo("Draw Mode", drawModes[e_cast(mDrawMode)])) {
        for (int i = 0; i < e_cast(ModelEditorPanelDrawMode::COUNT); ++i) {
            bool isSelected = e_cast(mDrawMode) == i;
            ImGui::Selectable(drawModes[i], &isSelected);

            if (isSelected) {
                ImGui::SetItemDefaultFocus();
                mDrawMode = (ModelEditorPanelDrawMode)i;
            }
        }
        ImGui::EndCombo();
    }
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
        if (mCurrentModel->mModelType == Model3DType::STATIC) {
            StaticModel3D& mModel = mCurrentModel->getStaticModel();
            ui32 indexCountTotal = 0;
            const SubMeshData* submeshData = &mModel.getMesh()->mMainMesh;
            do {
                indexCountTotal += submeshData->mLODData.getDrawInfoForLOD(MeshLODLevel(mLod)).indexCount;
                submeshData = submeshData->mNextSubmesh;
            } while (submeshData != nullptr);
            ImGui::Text("Polygons %d", indexCountTotal / 3);
        }
    }

    ImGui::EndChild();
}

void ModelEditorPanel::updateCamera(f32 aspectRatio) {

    // Controls
    positioner.movement_.forward_ = vui::InputDispatcher::key.isKeyPressed(VKEY_W);
    positioner.movement_.backward_ = vui::InputDispatcher::key.isKeyPressed(VKEY_S);
    positioner.movement_.left_ = vui::InputDispatcher::key.isKeyPressed(VKEY_A);
    positioner.movement_.right_ = vui::InputDispatcher::key.isKeyPressed(VKEY_D);
    positioner.movement_.up_ = vui::InputDispatcher::key.isKeyPressed(VKEY_SPACE);
    positioner.movement_.down_ = vui::InputDispatcher::key.isKeyPressed(VKEY_LCTRL);
    positioner.movement_.fastSpeed_ = vui::InputDispatcher::key.isKeyPressed(VKEY_LSHIFT);

    // TODO: Deltatime
    positioner.update(1.0f / 60.0f, f32v2(ImGui::GetMousePos().x / ImGui::GetWindowWidth(), ImGui::GetMousePos().y / ImGui::GetWindowHeight()), ImGui::IsMouseDown(ImGuiMouseButton_Right), aspectRatio);
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

void ModelEditorPanel::renderModelToTexture() {
    if (!mCurrentModel) {
        return;
    }


    ResourceManager& resourceManager = Services::ResourceManager::ref();

    // Render model
    if (mCurrentModel->mModelType == Model3DType::STATIC) {
        const MaterialShader* staticModelMaterial = nullptr;

        switch (mDrawMode) {
            case ModelEditorPanelDrawMode::Default:
                staticModelMaterial = resourceManager.getMaterialManager().getMaterialShader("editor_model");
                break;
            case ModelEditorPanelDrawMode::Wireframe:
                staticModelMaterial = resourceManager.getMaterialManager().getMaterialShader("mesh_wireframe");
                break;
            case ModelEditorPanelDrawMode::Normals:
                staticModelMaterial = resourceManager.getMaterialManager().getMaterialShader("mesh_normals");
                break;
            default:
                assert(false);
                break;
        }

        VGUniform unVP = staticModelMaterial->getUniform("unVP");
        MaterialRenderer::bindMaterialForRender(*staticModelMaterial);

        glUniformMatrix4fv(unVP, 1, false, &(camera.getViewProjectionMatrix()[0][0]));

        StaticModel3D& mModel = mCurrentModel->getStaticModel();
        mModel.getMesh()->draw(MeshLODLevel(mLod));
    }

    mModelGBuffer->unuse();
}

void ModelEditorPanel::renderGrid()
{
    vg::DepthState::NONE.set();

    ResourceManager& resourceManager = Services::ResourceManager::ref();
    const MaterialShader* gridMaterial = resourceManager.getMaterialManager().getMaterialShader("grid");
    VGUniform unVP = gridMaterial->getUniform("unVP");
    MaterialRenderer::bindMaterialForRender(*gridMaterial);
    glUniformMatrix4fv(unVP, 1, false, &(camera.getViewProjectionMatrix()[0][0]));
    glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, 6, 1, 0);

    vg::DepthState::restorePrevious();
}
