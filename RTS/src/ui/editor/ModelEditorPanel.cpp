#include "stdafx.h"
#include "ModelEditorPanel.h"

#include "definitions/ModelDef.h"

#include "resources/ResourceManager.h"
//#include "resources/ModelRepository.h"
#include "rendering/MaterialShaderManager.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialUtils.h"
#include "rendering/post_process/ShadowLodDetail.h"

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>

#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/FullQuadVBO.h>

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
    if (mGBuffers[0] == nullptr) {
        initGBuffers(imageDims);
    }

    glDisable(GL_CULL_FACE);
    vg::DepthState::FULL.set();

    // TODO: Put into gbuffer impl
    // Clear framebuffers
    f32v4 colorClear(1.0f, 1.0f, 1.0f, 0.0f);
    f32v4 normalClear(0.0f);
    for (int i = 0; i < 3; ++i) {
        mGBuffers[i]->clearAttachment(vg::GBufferAttachmentIndex::ALBEDO, f32v4(1.0f, 1.0f, 1.0f, 0.0f));
        mGBuffers[i]->clearAttachment(vg::GBufferAttachmentIndex::NORMALS);
        mGBuffers[i]->clearDepth();
    }
    mGBuffers[0]->use();

    renderGrid();
    VGTexture displayTexture = renderModelToTexture();
    vg::GBuffer::unuse();

    const ImVec2 uv0(0, 1);
    const ImVec2 uv1(1, 0);
    const ImVec2 dims(imageDims.x, imageDims.y);
    ImGui::Image((ImTextureID)displayTexture, dims, uv0, uv1);

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

        // Blend test controls
        if (mDrawMode == EditorViewportDrawMode::BlendTest) {
            ImGui::SliderInt("Blend Passes", &mBlendTestPasses, 0, 15);
            ImGui::SliderFloat("Blend Radius", &mBlendTestRadius, 0.0f, 15.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::SliderFloat("Blend Norm Threshold", &mBlendTestNormThreshold, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::SliderFloat("Blend Depth Threshold", &mBlendTestDepthThreshold, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::SliderInt("Blend Display", &mBlendTestDisplayMode, 0, 2);
            ImGui::Checkbox("Show Variance", &mBlendTestShowVariance);
            ImGui::Checkbox("Show Edges", &mBlendTestShowEdges);
            ImGui::Checkbox("Disable", &mBlendTestDisable);
        }
        else if (mDrawMode == EditorViewportDrawMode::EdgeTest) {
            ImGui::SliderFloat("Edge Test Threshold", &mEdgeTestThreshold, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::SliderFloat("Edge Depth Threshold", &mEdgeTestDepthThreshold, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::SliderInt("Edge Display", &mEdgeTestDisplayMode, 0, 2);
            ImGui::SliderInt("Edge Size", &mEdgeSize, 1, 15);
            ImGui::SliderInt("Edge Blur Passes", &mEdgeBlendPasses, 0, 15);
            ImGui::SliderFloat("Edge Blur Radius", &mEdgeBlendRadius, 0.0f, 15.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::Checkbox("Show Edges", &mEdgeTestShowEdges);
            ImGui::Checkbox("Disable", &mEdgeTestDisable);
        }
    }

    ImGui::EndChild();
}

VGTexture ModelEditorPanel::renderModelToTexture() {
    if (!mCurrentModel) {
        return mGBuffers[0]->getAlbedoTexture();
    }

    ResourceManager& resourceManager = Services::ResourceManager::ref();

    // Render model
    if (mCurrentModel->mRig == nullptr) {

        if (mDrawMode == EditorViewportDrawMode::BlendTest) {
            return renderModelToTextureBlendTest();
        }else if (mDrawMode == EditorViewportDrawMode::EdgeTest) {
            return renderModelToTextureEdgeTest();
        }
        else {

            const MaterialShader* staticModelMaterial = nullptr;

            switch (mDrawMode) {
                case EditorViewportDrawMode::Lit:
                case EditorViewportDrawMode::Unlit:
                case EditorViewportDrawMode::Normals:
                case EditorViewportDrawMode::UVs:
                    staticModelMaterial = resourceManager.getMaterialShaderManager().getMaterialShader("editor_model");
                    break;
                case EditorViewportDrawMode::Wireframe:
                    staticModelMaterial = resourceManager.getMaterialShaderManager().getMaterialShader("mesh_wireframe");
                    break;
                default:
                    assert(false);
                    break;
            }
            static_assert(e_cast(EditorViewportDrawMode::COUNT) == 7);

            VGUniform unVP = staticModelMaterial->getUniform("unVP");
            MaterialRenderer::bindMaterialForRender(*staticModelMaterial);

            glUniformMatrix4fv(unVP, 1, false, &(camera->getViewProjectionMatrix()[0][0]));

            if (mDrawMode != EditorViewportDrawMode::Wireframe) {
                MaterialUtils::uploadLightingUniforms(*staticModelMaterial);
                glUniform1i(staticModelMaterial->getUniform("unRenderMode"), (int)mDrawMode);
            }

            Model3D& mModel = mCurrentModel->mModel;
            mModel.getMesh()->draw(MeshLODLevel(mLod));
        }
    }
    else {
        // Skinned mesh render
        // TODO:
    }
    return mGBuffers[0]->getAlbedoTexture();
}

VGTexture ModelEditorPanel::renderModelToTextureBlendTest()
{

    ResourceManager& resourceManager = Services::ResourceManager::ref();
    {
        const MaterialShader* staticModelMaterial = resourceManager.getMaterialShaderManager().getMaterialShader("editor_model");

        VGUniform unVP = staticModelMaterial->getUniform("unVP");
        MaterialRenderer::bindMaterialForRender(*staticModelMaterial);

        glUniformMatrix4fv(unVP, 1, false, &(camera->getViewProjectionMatrix()[0][0]));

        MaterialUtils::uploadLightingUniforms(*staticModelMaterial);
        glUniform1i(staticModelMaterial->getUniform("unRenderMode"), (int)mDrawMode);
        // Replace normals
        glBlendFunci(e_cast(vg::GBufferAttachmentIndex::NORMALS), GL_ONE, GL_ZERO);

        Model3D& mModel = mCurrentModel->mModel;
        mModel.getMesh()->draw(MeshLODLevel(mLod));

        // Now we have the mesh normals, positions depth.
    }

    const MaterialShader* blendShader = resourceManager.getMaterialShaderManager().getMaterialShader("blend_test");
    ui32 freeTextureIndex;
    MaterialRenderer::bindMaterialForRender(*blendShader, &freeTextureIndex);

    const VGUniform& albedoUniform = blendShader->mProgram.getUniform("unAlbedoFbo");
    const VGUniform& normalUniform = blendShader->mProgram.getUniform("unNormalFbo");
    const VGUniform& depthUniform = blendShader->mProgram.getUniform("unDepthFbo");
    const VGUniform& dirUniform = blendShader->mProgram.getUniform("unDirection");
    glUniform1i(albedoUniform, freeTextureIndex);
    glUniform1i(normalUniform, freeTextureIndex + 1);
    glUniform1i(depthUniform, freeTextureIndex + 2);
    glUniform1f(blendShader->mProgram.getUniform("unNormalThreshold"), mBlendTestNormThreshold);
    glUniform1f(blendShader->mProgram.getUniform("unDepthThreshold"), mBlendTestDepthThreshold);
    glUniform1i(blendShader->mProgram.getUniform("unShowVariance"), mBlendTestShowVariance);
    glUniform1i(blendShader->mProgram.getUniform("unShowEdges"), mBlendTestShowEdges);
    glUniform2f(blendShader->mProgram.getUniform("unScreenResolution"), mGBuffers[0]->getWidth(), mGBuffers[0]->getHeight());
    glUniform2f(blendShader->mProgram.getUniform("unCameraZRange"), SimpleCamera::ZNEAR, SimpleCamera::ZFAR);
    mGBuffers[0]->bindDepthTexture(freeTextureIndex + 2);
    vg::DepthState::NONE.set();
    if (!mBlendTestDisable) {
        for (int i = 0; i < mBlendTestPasses; ++i) {

            // Horizontal
            mGBuffers[0]->bindAlbedoTexture(freeTextureIndex);
            mGBuffers[0]->bindNormalTexture(freeTextureIndex + 1);
            mGBuffers[1]->use();
            // Replace normals TODO: Build into gbuffer
            glBlendFunci(e_cast(vg::GBufferAttachmentIndex::ALBEDO), GL_ONE, GL_ZERO);
            glBlendFunci(e_cast(vg::GBufferAttachmentIndex::NORMALS), GL_ONE, GL_ZERO);
            glUniform2f(dirUniform, mBlendTestRadius, 0.0f);
            sGlobalFullQuadVBO.draw();

            // Vertical
            // Replace normals TODO: Build into gbuffer
            mGBuffers[1]->bindAlbedoTexture(freeTextureIndex);
            mGBuffers[1]->bindNormalTexture(freeTextureIndex + 1);
            mGBuffers[0]->use();
            glBlendFunci(e_cast(vg::GBufferAttachmentIndex::ALBEDO), GL_ONE, GL_ZERO);
            glBlendFunci(e_cast(vg::GBufferAttachmentIndex::NORMALS), GL_ONE, GL_ZERO);
            glUniform2f(dirUniform, 0.0f, mBlendTestRadius);
            sGlobalFullQuadVBO.draw();
        }
    }
    vg::DepthState::restorePrevious();

    switch (mBlendTestDisplayMode) {
        case 0:
            return mGBuffers[0]->getAlbedoTexture();
        case 1:
            return mGBuffers[0]->getNormalTexture();
        default:
            return mGBuffers[0]->getDepthTexture();
    }
}

VGTexture ModelEditorPanel::renderModelToTextureEdgeTest()
{
    ResourceManager& resourceManager = Services::ResourceManager::ref();
    {
        const MaterialShader* staticModelMaterial = resourceManager.getMaterialShaderManager().getMaterialShader("editor_model");

        VGUniform unVP = staticModelMaterial->getUniform("unVP");
        MaterialRenderer::bindMaterialForRender(*staticModelMaterial);

        glUniformMatrix4fv(unVP, 1, false, &(camera->getViewProjectionMatrix()[0][0]));

        MaterialUtils::uploadLightingUniforms(*staticModelMaterial);
        glUniform1i(staticModelMaterial->getUniform("unRenderMode"), (int)mDrawMode);
        // Replace normals
        glBlendFunci(e_cast(vg::GBufferAttachmentIndex::NORMALS), GL_ONE, GL_ZERO);

        Model3D& mModel = mCurrentModel->mModel;
        mModel.getMesh()->draw(MeshLODLevel(mLod));

        // Now we have the mesh normals, positions depth.
    }

    vg::DepthState::NONE.set();
    ui32 freeTextureIndex;
    { // Edge test
        const MaterialShader* edgeShader = resourceManager.getMaterialShaderManager().getMaterialShader("edge_test");
        MaterialRenderer::bindMaterialForRender(*edgeShader, &freeTextureIndex);

        glUniform1i(edgeShader->mProgram.getUniform("unNormalFbo"), freeTextureIndex);
        glUniform1i(edgeShader->mProgram.getUniform("unDepthFbo"), freeTextureIndex + 1);
        glUniform1f(edgeShader->mProgram.getUniform("unEdgeThreshold"), mEdgeTestThreshold);
        glUniform1f(edgeShader->mProgram.getUniform("unDepthThreshold"), mEdgeTestDepthThreshold);
        glUniform2f(edgeShader->mProgram.getUniform("unCameraZRange"), SimpleCamera::ZNEAR, SimpleCamera::ZFAR);
        mGBuffers[0]->bindNormalTexture(freeTextureIndex);
        mGBuffers[0]->bindDepthTexture(freeTextureIndex + 1);

        mGBuffers[1]->use();

        // Replace normals TODO: Build into gbuffer
        glBlendFunci(e_cast(vg::GBufferAttachmentIndex::ALBEDO), GL_ONE, GL_ZERO);
        sGlobalFullQuadVBO.draw();
    }

    int sourceGBuffer = 1;
    int targetGBuffer = 2;
    { // Edge expand
        const MaterialShader* expandShader = resourceManager.getMaterialShaderManager().getMaterialShader("edge_expand");
        MaterialRenderer::bindMaterialForRender(*expandShader, &freeTextureIndex);
        // TODO: Profile just changing the uniform instead of changing the texture binding!
        glUniform1i(expandShader->mProgram.getUniform("unFbo"), freeTextureIndex);
        glUniform1i(expandShader->mProgram.getUniform("unDepthFbo"), freeTextureIndex + 1);
        glUniform1f(expandShader->mProgram.getUniform("unDepthThreshold"), mEdgeTestDepthThreshold);
        glUniform2f(expandShader->mProgram.getUniform("unCameraZRange"), SimpleCamera::ZNEAR, SimpleCamera::ZFAR);

        mGBuffers[0]->bindDepthTexture(freeTextureIndex + 1);
        for (int i = 0; i < mEdgeSize; ++i) {
            // Increments of 2 so it always ends up in gbuffer 2
            for (int j = 0; j < 2; ++j) {
                mGBuffers[sourceGBuffer]->bindAlbedoTexture(freeTextureIndex);
                mGBuffers[targetGBuffer]->use();

                // Replace normals TODO: Build into gbuffer
                // TODO: we dont need clear buffer because of this?
                glBlendFunci(e_cast(vg::GBufferAttachmentIndex::ALBEDO), GL_ONE, GL_ZERO);
                sGlobalFullQuadVBO.draw();

                std::swap(sourceGBuffer, targetGBuffer);
            }
        }
    }

    { // Blur edges
        const MaterialShader* blendShader = resourceManager.getMaterialShaderManager().getMaterialShader("blend_test_v2");
        MaterialRenderer::bindMaterialForRender(*blendShader, &freeTextureIndex);
        const VGUniform& dirUniform = blendShader->mProgram.getUniform("unDirection");
        glUniform1i(blendShader->mProgram.getUniform("unAlbedoFbo"), freeTextureIndex);
        glUniform1i(blendShader->mProgram.getUniform("unNormalFbo"), freeTextureIndex + 1);
        glUniform1i(blendShader->mProgram.getUniform("unEdgeFbo"), freeTextureIndex + 2);
        glUniform1i(blendShader->mProgram.getUniform("unShowEdges"), mEdgeTestShowEdges);

        mGBuffers[2]->bindAlbedoTexture(freeTextureIndex + 2);
        glUniform2f(blendShader->mProgram.getUniform("unScreenResolution"), mGBuffers[0]->getWidth(), mGBuffers[0]->getHeight());
        //glUniform2f(blendShader->mProgram.getUniform("unCameraZRange"), SimpleCamera::ZNEAR, SimpleCamera::ZFAR);
        if (!mEdgeTestDisable) {
            for (int i = 0; i < mEdgeBlendPasses; ++i) {

                // Horizontal
                mGBuffers[0]->bindAlbedoTexture(freeTextureIndex);
                mGBuffers[0]->bindNormalTexture(freeTextureIndex + 1);
                mGBuffers[1]->use();
                // Replace normals TODO: Build into gbuffer
                glBlendFunci(e_cast(vg::GBufferAttachmentIndex::ALBEDO), GL_ONE, GL_ZERO);
                glBlendFunci(e_cast(vg::GBufferAttachmentIndex::NORMALS), GL_ONE, GL_ZERO);
                glUniform2f(dirUniform, mEdgeBlendRadius, 0.0f);
                sGlobalFullQuadVBO.draw();

                // Vertical
                // Replace normals TODO: Build into gbuffer
                mGBuffers[1]->bindAlbedoTexture(freeTextureIndex);
                mGBuffers[1]->bindNormalTexture(freeTextureIndex + 1);
                mGBuffers[0]->use();
                glBlendFunci(e_cast(vg::GBufferAttachmentIndex::ALBEDO), GL_ONE, GL_ZERO);
                glBlendFunci(e_cast(vg::GBufferAttachmentIndex::NORMALS), GL_ONE, GL_ZERO);
                glUniform2f(dirUniform, 0.0f, mEdgeBlendRadius);
                sGlobalFullQuadVBO.draw();
            }
        }
    }
     
    vg::DepthState::restorePrevious();

    switch (mEdgeTestDisplayMode) {
        case 0:
            return mGBuffers[0]->getAlbedoTexture();
        case 1:
            return mGBuffers[0]->getNormalTexture();
        default:
            return mGBuffers[0]->getDepthTexture();
    }
}
