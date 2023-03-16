#include "stdafx.h"
#include "IEditorViewportPanel.h"

#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/BlendState.h>
#include <Vorb/ui/InputDispatcher.h>

#include <glm/gtx/rotate_vector.hpp>

#include <Vorb/ui/imgui/imgui.h>
#include <Vorb/ui/imgui/backends/imgui_impl_sdl.h>
#include <Vorb/ui/imgui/backends/imgui_impl_opengl3.h>
#include <Vorb/graphics/FullQuadVBO.h>

#include "resources/ResourceManager.h"
#include "rendering/MaterialShaderManager.h"
#include "rendering/MaterialRenderer.h"
#include "resources/TextureRepository.h"
#include "rendering/MaterialUtils.h"
#include "rendering/Skybox.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/material/BrdfLUT.h"
#include "rendering/model/ModelUtil.h"

#include "camera/SimpleCamera.h"

IEditorViewportPanel::IEditorViewportPanel() {
    positioner = std::make_unique<CameraPositioner_FirstPerson>(f32v3(0.0f, -5.0f, 1.5f), f32v3(0.0f, 0.0f, 0.5f), f32v3(0.0f, 0.0f, 1.0f));
    camera = std::make_unique<SimpleCamera>(*positioner);

    glCreateVertexArrays(1, &mGridVao);
}

IEditorViewportPanel::~IEditorViewportPanel() {

    glDeleteVertexArrays(1, &mGridVao);
}

void IEditorViewportPanel::renderCenterPanel() {
    // Lazy init resources
    if (!mSkybox) {
        mSkybox = std::make_unique<Skybox>();
        mSkybox->init(Services::ResourceManager::ref().getMaterialShaderManager().getMaterialShader("sky"), nullptr);
    }

    glDisable(GL_CULL_FACE);
    vg::DepthState::FULL.set();

    // Clear framebuffers
    for (int i = 0; i < 3; ++i) {
        mGBuffers[i]->clearAttachment(vg::GBufferAttachmentIndex::ALBEDO, f32v4(1.0f, 1.0f, 1.0f, 1.0f));
        mGBuffers[i]->clearAttachment(vg::GBufferAttachmentIndex::NORMALS);
        mGBuffers[i]->clearDepth();
    }
    mGBuffers[0]->use();

    if (mSkybox->hasTexture()) {
        if (mShowSkyboxIrradiance) {
            mSkybox->renderIrradianceDebug(camera->getViewProjectionMatrixNoTranslation());
        }
        else if (mShowSkyboxPrecomputedMap) {
            mSkybox->renderPrecomputedMapDebug(camera->getViewProjectionMatrixNoTranslation(), mPrecomputedLOD);
        }
        else {
            mSkybox->render(camera->getViewProjectionMatrixNoTranslation());
        }
    }

    if (mRenderGrid) {
        renderGrid();
    }

    VGTexture displayTexture = 0;
    const MaterialShader* shader = getShader();
    if (shader) {
        ui32 textureUnit;
        MaterialRenderer::bindMaterialForRender(*shader, &textureUnit);
        uploadShaderUniforms(shader, textureUnit);

        if (mRenderArray && mDrawMode == EditorViewportDrawMode::PBRTest) {
            renderPBRArray(shader);
        }
        else {
            renderMesh();

            if (mDrawMode == EditorViewportDrawMode::EdgeTest) {
                postProcessEdgeTest();
            }
            else if (mDrawMode == EditorViewportDrawMode::BlendTest) {
                postProcessBlendTest();
            }
        }
        displayTexture = getFinalOutputTexture();
    }
    vg::GBuffer::unuse();

    f32v2 imageDims = f32v2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
    const ImVec2 uv0(0, 1);
    const ImVec2 uv1(1, 0);
    const ImVec2 dims(imageDims.x, imageDims.y);
    ImGui::Image((ImTextureID)displayTexture, dims, uv0, uv1);
}

VGTexture IEditorViewportPanel::getFinalOutputTexture() {
    if (mDrawMode == EditorViewportDrawMode::BlendTest) {
        switch (mBlendTestDisplayMode) {
            case 0:
                return mGBuffers[0]->getAlbedoTexture();
            case 1:
                return mGBuffers[0]->getNormalTexture();
            default:
                return mGBuffers[0]->getDepthTexture();
        }
    }
    else if (mDrawMode == EditorViewportDrawMode::EdgeTest) {
        switch (mEdgeTestDisplayMode) {
            case 0:
                return mGBuffers[0]->getAlbedoTexture();
            case 1:
                return mGBuffers[0]->getNormalTexture();
            default:
                return mGBuffers[0]->getDepthTexture();
        }
    }
    else {
        return mGBuffers[0]->getAlbedoTexture();
    }
    static_assert(e_cast(EditorViewportDrawMode::COUNT) == 12, "Make sure you don't need to set a custom output texture");
}

void IEditorViewportPanel::updateAndRenderSharedControls() {
    // Draw mode
    const char* drawModes[e_cast(EditorViewportDrawMode::COUNT)] = {
        "Lit",
        "Unlit",
        "Normals",
        "Tangents",
        "AO",
        "Metallic",
        "Roughness",
        "UVs",
        "Blend Test",
        "Edge Test",
        "PBR Test",
        "Wireframe", // Always last
    };
    static_assert(e_cast(EditorViewportDrawMode::COUNT) == 12);
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

    // Skybox
    // TODO: Cache this?
    TextureRepository& textureRepository = Services::ResourceManager::ref().getTextureRepository();
    const std::map<nString, CubemapID>& cubemapIds = textureRepository.getCubemapIDs();
    std::vector<nString> cubemapNames;
    cubemapNames.reserve(cubemapIds.size() + 1);
    cubemapNames.emplace_back("NONE");
    for (auto&& it : cubemapIds) {
        cubemapNames.emplace_back(it.first);
    }
    if (ImGui::BeginCombo("Skybox", cubemapNames[mSelectedSkyboxIndex].c_str())) {
        for (size_t i = 0; i < cubemapNames.size(); ++i) {
            bool isSelected = mSelectedSkyboxIndex == i;
            ImGui::Selectable(cubemapNames[i].c_str(), &isSelected);

            if (isSelected) {
                ImGui::SetItemDefaultFocus();
                mSelectedSkyboxIndex = i;
                if (i == 0) {
                    mSkybox->setCubemap(nullptr);
                }
                else {
                    auto&& it = cubemapIds.find(cubemapNames[i]);
                    assert(it != cubemapIds.end());
                    mSkybox->setCubemap(&textureRepository.getCubemap(it->second));
                }
            }
        }
        ImGui::EndCombo();
    }

    if (mSelectedSkyboxIndex != 0) {
        ImGui::Checkbox("Skybox Irradiance", &mShowSkyboxIrradiance);
        if (mShowSkyboxIrradiance) {
            mShowSkyboxPrecomputedMap = false;
        }
        ImGui::Checkbox("Skybox Precomputed map", &mShowSkyboxPrecomputedMap);
        if (mShowSkyboxPrecomputedMap) {
            ImGui::SliderInt("Level", &mPrecomputedLOD, 0, 10);
            mShowSkyboxIrradiance = false;
        }
    }

    // Grid
    ImGui::Checkbox("Show Grid", &mRenderGrid);

    // Transform
    ImGui::SliderFloat("Yaw", &mYaw, 0.0f, M_2_PIF);
    ImGui::Checkbox("Rotate 90", &mRotate90);
}

void IEditorViewportPanel::updateAndRenderTweakers() {

    ImGui::Separator();

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
    else if (mDrawMode == EditorViewportDrawMode::PBRTest) {
        ImGui::Checkbox("Set Metallic Roughness", &mOverrideMetallicRoughness);
        if (mOverrideMetallicRoughness) {
            ImGui::SliderFloat("Metallic", &mMetallic, 0.0f, 1.0f, "%.3f");
            ImGui::SliderFloat("Roughness", &mRoughness, 0.0f, 1.0f, "%.3f");
            ImGui::Separator();
        }
        ImGui::SliderFloat("Height Scale", &mHeightScale, 0.0f, 3.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Ambient", &mAmbient, 0.0f, 3.0f, "%.3f");
        ImGui::SliderFloat("Exposure", &mExposure, 0.0f, 3.0f, "%.3f");
        ImGui::SliderFloat("Sun Intensity", &mSunIntensity, 0.0f, 25.0f, "%.3f");
        ImGui::SliderFloat2("Sun Dir", &mLightDir.x, -2.0f, 2.0f);
        ImGui::ColorPicker3("Sun Color", &mLightColor.x, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueBar);
        ImGui::Checkbox("Array View", &mRenderArray);
        if (mRenderArray) {
            ImGui::Checkbox("Preview Follow Axis", &mFollowAxis);
        }
    }
    static_assert(e_cast(EditorViewportDrawMode::COUNT) == 12, "Make sure you don't need any property editors");
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

void IEditorViewportPanel::initGBuffers(ui32v2 imageDims) {

    // TODO: PBR https://www.hiagodesena.com/blog/physically-based-deferred-renderer
    // https://learnopengl.com/PBR/Theory
    // https://learnopengl.com/PBR/Lighting
    // https://learnopengl.com/PBR/IBL/Diffuse-irradiance
    // https://learnopengl.com/PBR/IBL/Specular-IBL
    // 
    // TODO: Tile based deferred rendering (not clustered) see compute at page 35-36 https://www.digipen.edu/sites/default/files/public/docs/theses/denis-ishmukhametov-master-of-science-in-computer-science-thesis-efficient-tile-based-deferred-shading-pipeline.pdf

    // TODO: RG16F normals or R11F_G11F_B10F?? https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
    for (int i = 0; i < 3; ++i) {
        mGBuffers[i] = std::make_unique<vg::GBuffer>(imageDims);
        mGBuffers[i]->initAttachment(vg::GBufferAttachmentIndex::ALBEDO, vg::TextureInternalFormat::RGBA8);
        mGBuffers[i]->initAttachment(vg::GBufferAttachmentIndex::NORMALS, vg::TextureInternalFormat::RGB10_A2);
        mGBuffers[i]->initDepthStencil(vg::GBufferDepthStencilFormat::DEPTH_24_STENCIL_8);
    }

    checkGlError("ModelEditorPanel::initGBuffer");
}

void IEditorViewportPanel::renderGrid() {
    vg::DepthState::NONE.set();
    vg::sBlendStates.ALPHA.set();

    ResourceManager& resourceManager = Services::ResourceManager::ref();
    const MaterialShader* gridMaterial = resourceManager.getMaterialShaderManager().getMaterialShader("grid");
    VGUniform unVP = gridMaterial->getUniform("unVP");
    MaterialRenderer::bindMaterialForRender(*gridMaterial);
    glUniformMatrix4fv(unVP, 1, false, &(camera->getViewProjectionMatrix()[0][0]));

    glBindVertexArray(mGridVao);
    glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, 6, 1, 0);

    vg::DepthState::restorePrevious();
    vg::BlendState::restorePrevious();
}

void IEditorViewportPanel::uploadShaderUniforms(const MaterialShader* shader, ui32 availableTextureUnit) {
    assert(availableTextureUnit == 0);

    if (mRotate90) {
        const f32m4 modelMatrix = ModelUtil::computeTransformMatrixForModel(f32v3(0.0f), mYaw);
        glUniformMatrix4fv(shader->getUniform("unM"), 1, false, &(modelMatrix[0][0]));
    }
    else {
        const f32m4 modelMatrix(1.0f);
        glUniformMatrix4fv(shader->getUniform("unM"), 1, false, &(modelMatrix[0][0]));
    }
    glUniformMatrix4fv(shader->getUniform("unVP"), 1, false, &(camera->getViewProjectionMatrix()[0][0]));

    if (mDrawMode == EditorViewportDrawMode::PBRTest) {
        glUniform1f(shader->getUniform("unAmbient"), mAmbient);
        glUniform1f(shader->getUniform("unMetallic"), mMetallic);
        glUniform1f(shader->getUniform("unRoughness"), mRoughness);
        glUniform1f(shader->getUniform("unExposure2"), mExposure);
        glUniform1f(shader->getUniform("unSunIntensity"), mSunIntensity);
        f32v3 lightDir = glm::normalize(f32v3(mLightDir.x, -1.0f, mLightDir.y));
        glUniform3fv(shader->getUniform("unLightDir"), 1, &lightDir.x);
        glUniform3fv(shader->getUniform("unCameraPos"), 1, &camera->getPosition()[0]);
        glUniform3fv(shader->getUniform("unSunColor"), 1, &mLightColor.x);
        glUniform1i(shader->getUniform("unOverrideMR"), mOverrideMetallicRoughness);
        if (const VGUniform* un = shader->tryGetUniform("unHeightScale")) {
            glUniform1f(*un, mHeightScale);
        }

        if (mSkybox->hasTexture()) {
            glUniform1i(shader->getUniform("unIrradianceMap"), availableTextureUnit);
            glBindTextureUnit(availableTextureUnit++, mSkybox->getCubemap()->getIrradianceTexture());
            glUniform1i(shader->getUniform("unPrefilterMap"), availableTextureUnit);
            glBindTextureUnit(availableTextureUnit++, mSkybox->getCubemap()->getPrefilterMap());
        }
        else {
            // TODO: empty textures?
            availableTextureUnit += 2;
        }
        glUniform1i(shader->getUniform("unBrdfLUT"), availableTextureUnit);
        glBindTextureUnit(availableTextureUnit++, BrdfLUT::getTexture());

        MaterialUtils::uploadTonemapUniforms(*shader);
    }
    else if (mDrawMode <= EditorViewportDrawMode::UVs) {
        // Every basic material
        MaterialUtils::uploadLightingUniforms(*shader);
    }
    if (const VGUniform* unRenderMode = shader->tryGetUniform("unRenderMode")) {
        glUniform1i(*unRenderMode, (int)mDrawMode);
    }
    static_assert(e_cast(EditorViewportDrawMode::COUNT) == 12, "Make sure you don't need to set any uniforms");

    uploadCustomShaderUniforms(shader, 3);
}

void IEditorViewportPanel::renderPBRArray(const MaterialShader* shader) {


    VGUniform metallicUniform = shader->getUniform("unMetallic");
    VGUniform roughnessUniform = shader->getUniform("unRoughness");
    const float span = 13.0f;
    const float halfSpan = span / 2.0f;
    const int count = 6;
    const float dist = span / (count - 1);
    const float step = 1.0f / (count - 1);

    // Render one in front using material parameters
    glUniform4f(shader->getUniform("unPosOffset"), (-halfSpan + mRoughness * span) * (float)mFollowAxis, -4.0f, (mMetallic * span) * (float)mFollowAxis, 0.0f);
    renderMesh();

    for (int y = 0; y < count; ++y) {
        const float metallic = y * step;
        glUniform1f(metallicUniform, metallic);
        for (int x = 0; x < count; ++x) {
            const float roughness = x * step;
            glUniform1f(roughnessUniform, roughness);

            glUniform4f(shader->getUniform("unPosOffset"), -halfSpan + x * dist, 0.0f, 0.0f + y * dist, 0.0f);
            renderMesh();
        }
    }
}

void IEditorViewportPanel::postProcessBlendTest() {
    ResourceManager& resourceManager = Services::ResourceManager::ref();

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

}

void IEditorViewportPanel::postProcessEdgeTest() {
    ResourceManager& resourceManager = Services::ResourceManager::ref();

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
}
