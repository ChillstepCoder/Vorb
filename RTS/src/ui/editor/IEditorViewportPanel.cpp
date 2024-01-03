#include "stdafx.h"
#include "IEditorViewportPanel.h"

#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/BlendState.h>
#include <Vorb/ui/InputDispatcher.h>

#include <glm/gtx/rotate_vector.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <Vorb/graphics/FullscreenTriangleVAO.h>

#include "resources/ResourceManager.h"
#include "resources/CubemapRepository.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/MaterialRenderer.h"
#include "resources/TextureRepository.h"
#include "rendering/MaterialUtils.h"
#include "rendering/Skybox.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/material/BrdfLUT.h"
#include "rendering/model/ModelUtil.h"

#include "definitions/rendering/CubemapDef.h"

#include "camera/SimpleCamera.h"

std::unique_ptr<vg::GBuffer> IEditorViewportPanel::sGBuffers[3];

IEditorViewportPanel::IEditorViewportPanel() {
    mCameraPositioner = std::make_unique<CameraPositioner_FirstPerson>(f32v3(0.0f, -5.0f, 1.5f), f32v3(0.0f, 0.0f, 0.5f), f32v3(0.0f, 0.0f, 1.0f));
    mCamera = std::make_unique<SimpleCamera>(*mCameraPositioner);

    glCreateVertexArrays(1, &mGridVao);
}

IEditorViewportPanel::~IEditorViewportPanel() {
    glDeleteVertexArrays(1, &mGridVao);
}

void IEditorViewportPanel::renderCenterPanel(i32AABB2* outImageRect) {

    const i32v2 imageDims = i32v2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);

    glDisable(GL_CULL_FACE);
    vg::DepthState::FULL.set();

    renderSkybox();

    const MaterialShaderDef* shader = getShader();
    if (shader) {
        renderGrid(mCamera->getViewProjectionMatrix());

        ui32 textureUnit;
        MaterialRenderer::bindMaterialShaderForRender(*shader, &textureUnit);
        uploadShaderUniforms(shader, textureUnit);


        if (mDisableBackfaceCulling) {
            glDisable(GL_CULL_FACE);
        }
        else {
            glEnable(GL_CULL_FACE);
        }

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
    }
    else {
        // If no shader, simpy renderMesh as we are doing world rendering or no rendering
        renderMesh();
    }
    vg::GBuffer::unuse();

    renderCenterPanelImage(outImageRect, getFinalOutputTexture());
}

void IEditorViewportPanel::clearFramebuffers() {
    assert(sGBuffers[0]);
    // Clear framebuffers
    for (int i = 0; i < 3; ++i) {
        sGBuffers[i]->clearAttachment(vg::GBufferAttachmentIndex::ALBEDO, mClearColor);
        sGBuffers[i]->clearAttachment(vg::GBufferAttachmentIndex::NORMALS);
        sGBuffers[i]->clearDepth();
    }
    sGBuffers[0]->use();
}

void IEditorViewportPanel::renderSkybox() {
    if (mSkybox->tryGetCubemap()) {
        if (mShowSkyboxIrradiance) {
            mSkybox->renderIrradianceDebug(mCamera->getViewProjectionMatrixNoTranslation());
        }
        else if (mShowSkyboxPrecomputedMap) {
            mSkybox->renderPrecomputedMapDebug(mCamera->getViewProjectionMatrixNoTranslation(), mPrecomputedLOD);
        }
        else {
            mSkybox->render(mCamera->getViewProjectionMatrixNoTranslation());
        }
    }
}

void IEditorViewportPanel::renderCenterPanelImage(i32AABB2* outImageRect, VGTexture displayTexture) {
    f32v2 imageDims = f32v2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
    const ImVec2 uv0(0, 1);
    const ImVec2 uv1(1, 0);
    const ImVec2 dims(imageDims.x, imageDims.y);
    ImGui::Image((ImTextureID)displayTexture, dims, uv0, uv1);

    if (outImageRect) {
        ImVec2 imageRectMin = ImGui::GetItemRectMin();
        outImageRect->dims = imageDims;
        outImageRect->pos = f32v2(imageRectMin.x, imageRectMin.y);
    }
}

void IEditorViewportPanel::updateFramebufferAndLazyInit(const i32v2& framebufferDims) {

    if (!mSkybox) {
        mSkybox = std::make_unique<Skybox>();
        mSkybox->init(nullptr);
    }
    if (sGBuffers[0] == nullptr || mCurrentGbufferDims != framebufferDims) {
        initGBuffers(framebufferDims);
        mCurrentGbufferDims = framebufferDims;
    }
}

VGTexture IEditorViewportPanel::getFinalOutputTexture() {
    if (mDrawMode == EditorViewportDrawMode::BlendTest) {
        switch (mBlendTestDisplayMode) {
            case 0:
                return sGBuffers[0]->getAlbedoTexture();
            case 1:
                return sGBuffers[0]->getNormalTexture();
            default:
                return sGBuffers[0]->getDepthTexture();
        }
    }
    else if (mDrawMode == EditorViewportDrawMode::EdgeTest) {
        switch (mEdgeTestDisplayMode) {
            case 0:
                return sGBuffers[0]->getAlbedoTexture();
            case 1:
                return sGBuffers[0]->getNormalTexture();
            default:
                return sGBuffers[0]->getDepthTexture();
        }
    }
    else {
        return sGBuffers[0]->getAlbedoTexture();
    }
    static_assert(e_cast(EditorViewportDrawMode::COUNT) == 12, "Make sure you don't need to set a custom output texture");
}

void IEditorViewportPanel::updateAndRenderSharedControls() {
    // Draw mode
    constexpr const char* drawModes[e_cast(EditorViewportDrawMode::COUNT)] = {
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
    if (ImGui::CollapsingHeader("Editor Controls")) {
        if (mShowDrawModeDropdown) {
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

        // Skybox
        // TODO: Cache this?
        std::vector<StrToken> cubemapNames;
        CubemapRepository& cubemapRepo = CubemapRepository::get();
        cubemapNames.reserve(cubemapRepo.getNumRegisteredAssets() + 1);
        cubemapNames.emplace_back("NONE");
        cubemapRepo.forEachRegisteredAsset([&](CubemapDef* def, const AssetMetadata& entry) {
            cubemapNames.emplace_back(entry.mName);
            return false;
        });

        if (mSkybox) {
            ui32 nameSize = 0;
            char nameBuffer[MAX_CHARS_IN_STRTOKEN];
            cubemapNames[mSelectedSkyboxIndex].toString(nameBuffer, &nameSize);
            if (ImGui::BeginCombo("Skybox", nameBuffer)) {
                for (size_t i = 0; i < cubemapNames.size(); ++i) {
                    bool isSelected = mSelectedSkyboxIndex == i;

                    cubemapNames[mSelectedSkyboxIndex].toString(nameBuffer, &nameSize);
                    ImGui::Selectable(nameBuffer, &isSelected);

                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                        mSelectedSkyboxIndex = i;
                        if (i == 0) {
                            mSkybox->setCubemap(nullptr);
                        }
                        else {
                            mSkybox->setCubemap(cubemapRepo.getAssetHandle(cubemapNames[i]));
                        }
                    }
                }
                ImGui::EndCombo();
            }
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

        // Culling
        ImGui::Checkbox("Disable Backface Cull", &mDisableBackfaceCulling);

        // Transform
        ImGui::SliderFloat("Yaw", &mYaw, 0.0f, M_2_PIF);
        ImGui::Checkbox("Rotate 90", &mRotate90);
    }
}

void IEditorViewportPanel::updateAndRenderTweakers() {

    ImGui::Separator();
    if (ImGui::CollapsingHeader("Render Options")) {
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
    }
    static_assert(e_cast(EditorViewportDrawMode::COUNT) == 12, "Make sure you don't need any property editors");
}

void IEditorViewportPanel::updateCamera(f32 aspectRatio) {

    // Controls
    mCameraPositioner->movement_.forward_ = vui::InputDispatcher::key.isKeyPressed(VKEY_W);
    mCameraPositioner->movement_.backward_ = vui::InputDispatcher::key.isKeyPressed(VKEY_S);
    mCameraPositioner->movement_.left_ = vui::InputDispatcher::key.isKeyPressed(VKEY_A);
    mCameraPositioner->movement_.right_ = vui::InputDispatcher::key.isKeyPressed(VKEY_D);
    mCameraPositioner->movement_.up_ = vui::InputDispatcher::key.isKeyPressed(VKEY_SPACE);
    mCameraPositioner->movement_.down_ = vui::InputDispatcher::key.isKeyPressed(VKEY_LALT);
    mCameraPositioner->movement_.fastSpeed_ = vui::InputDispatcher::key.isKeyPressed(VKEY_LSHIFT);

    // TODO: Deltatime
    mCameraPositioner->update(1.0f / 60.0f, f32v2(ImGui::GetMousePos().x / ImGui::GetWindowWidth(), ImGui::GetMousePos().y / ImGui::GetWindowHeight()), ImGui::IsMouseDown(ImGuiMouseButton_Right), aspectRatio);
}

void IEditorViewportPanel::initGBuffers(ui32v2 imageDims) {
    // PBR https://www.hiagodesena.com/blog/physically-based-deferred-renderer
    // https://learnopengl.com/PBR/Theory
    // https://learnopengl.com/PBR/Lighting
    // https://learnopengl.com/PBR/IBL/Diffuse-irradiance
    // https://learnopengl.com/PBR/IBL/Specular-IBL
    // 
    // TODO: Tile based deferred rendering (not clustered) see compute at page 35-36 https://www.digipen.edu/sites/default/files/public/docs/theses/denis-ishmukhametov-master-of-science-in-computer-science-thesis-efficient-tile-based-deferred-shading-pipeline.pdf

    // TODO: RG16F normals or R11F_G11F_B10F?? https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
    for (int i = 0; i < 3; ++i) {
        sGBuffers[i] = std::make_unique<vg::GBuffer>(imageDims);
        sGBuffers[i]->initAttachment(vg::GBufferAttachmentIndex::ALBEDO, vg::TextureInternalFormat::RGB8); // Final color
        sGBuffers[i]->initAttachment(vg::GBufferAttachmentIndex::NORMALS, vg::TextureInternalFormat::RGB10_A2); // Normal
        sGBuffers[i]->initDepthStencil(vg::GBufferDepthStencilFormat::DEPTH_24_STENCIL_8);
    }

    checkGlError("IEditorViewportPanel::initGBuffer");
}

void IEditorViewportPanel::renderGrid(const f32m4& VP) {

    if (!mRenderGrid) {
        return;
    }
    if (!mGridMaterial) mGridMaterial = MaterialShaderRepository::get().getAssetHandle(CStrToken("grid"));
    const MaterialShaderDef* gridMaterial = mGridMaterial->tryGetLoadedAsset();
    if (!gridMaterial) {
        return;
    }

    vg::DepthState::NONE.set();
    vg::sBlendStates.ALPHA.set();

    ResourceManager& resourceManager = Services::ResourceManager::ref();
    VGUniform unVP = gridMaterial->getUniform("unVP");
    MaterialRenderer::bindMaterialShaderForRender(*gridMaterial);
    glUniformMatrix4fv(unVP, 1, false, &(VP[0][0]));

    glBindVertexArray(mGridVao);
    glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, 6, 1, 0);

    vg::DepthState::restorePrevious();
    vg::BlendState::restorePrevious();
}

f32v3 IEditorViewportPanel::getCameraPosition() const {
    assert(mCamera);
    return mCamera->getPosition();
}

f32v3 IEditorViewportPanel::getCameraDirection() const {
    return mCamera->getDirection();
}

f32v3 IEditorViewportPanel::getCameraRight() const {
    return mCamera->getRight();
}

f32v3 IEditorViewportPanel::getCameraUp() const {
    return mCamera->getUp();
}

void IEditorViewportPanel::uploadShaderUniforms(const MaterialShaderDef* shader, ui32 availableTextureUnit) {
    assert(availableTextureUnit == 0);

    if (mRotate90) {
        const f32m4 modelMatrix = ModelUtil::computeTransformMatrixForModel(f32v3(0.0f), mYaw);
        glUniformMatrix4fv(shader->getUniform("unM"), 1, false, &(modelMatrix[0][0]));
    }
    else {
        const f32m4 modelMatrix(1.0f);
        glUniformMatrix4fv(shader->getUniform("unM"), 1, false, &(modelMatrix[0][0]));
    }
    glUniformMatrix4fv(shader->getUniform("unVP"), 1, false, &(mCamera->getViewProjectionMatrix()[0][0]));

    if (mDrawMode == EditorViewportDrawMode::PBRTest) {
        glUniform1f(shader->getUniform("unAmbient"), mAmbient);
        glUniform1f(shader->getUniform("unMetallic"), mMetallic);
        glUniform1f(shader->getUniform("unRoughness"), mRoughness);
        glUniform1f(shader->getUniform("unExposure2"), mExposure);
        glUniform1f(shader->getUniform("unSunIntensity"), mSunIntensity);
        f32v3 lightDir = glm::normalize(f32v3(mLightDir.x, -1.0f, mLightDir.y));
        glUniform3fv(shader->getUniform("unLightDir"), 1, &lightDir.x);
        glUniform3fv(shader->getUniform("unCameraPos"), 1, &mCamera->getPosition()[0]);
        glUniform3fv(shader->getUniform("unSunColor"), 1, &mLightColor.x);
        glUniform1i(shader->getUniform("unOverrideMR"), mOverrideMetallicRoughness);
        if (const VGUniform* un = shader->tryGetUniform("unHeightScale")) {
            glUniform1f(*un, mHeightScale);
        }

        if (const CubemapDef* cubeMap = mSkybox->tryGetCubemap()) {
            glUniform1i(shader->getUniform("unIrradianceMap"), availableTextureUnit);
            glBindTextureUnit(availableTextureUnit++, cubeMap->getIrradianceTexture());
            glUniform1i(shader->getUniform("unPrefilterMap"), availableTextureUnit);
            glBindTextureUnit(availableTextureUnit++, cubeMap->getPrefilterMap());
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

void IEditorViewportPanel::renderPBRArray(const MaterialShaderDef* shader) {


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

    static AssetHandlePtr<MaterialShaderDef> blendShaderDef = MaterialShaderRepository::get().getAssetHandle(CStrToken("blend_test"));

    const MaterialShaderDef* blendShader = blendShaderDef->tryGetLoadedAsset();
    if (!blendShader) return;

    ui32 freeTextureIndex;
    MaterialRenderer::bindMaterialShaderForRender(*blendShader, &freeTextureIndex);

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
    glUniform2f(blendShader->mProgram.getUniform("unScreenResolution"), sGBuffers[0]->getWidth(), sGBuffers[0]->getHeight());
    glUniform2f(blendShader->mProgram.getUniform("unCameraZRange"), SimpleCamera::ZNEAR, SimpleCamera::ZFAR);
    sGBuffers[0]->bindDepthTexture(freeTextureIndex + 2);
    vg::DepthState::NONE.set();
    if (!mBlendTestDisable) {
        for (int i = 0; i < mBlendTestPasses; ++i) {

            // Horizontal
            sGBuffers[0]->bindAlbedoTexture(freeTextureIndex);
            sGBuffers[0]->bindNormalTexture(freeTextureIndex + 1);
            sGBuffers[1]->use();
            // Replace normals TODO: Build into gbuffer
            glBlendFunci(e_cast(vg::GBufferAttachmentIndex::ALBEDO), GL_ONE, GL_ZERO);
            glBlendFunci(e_cast(vg::GBufferAttachmentIndex::NORMALS), GL_ONE, GL_ZERO);
            glUniform2f(dirUniform, mBlendTestRadius, 0.0f);
            sGlobalFullTriangleVAO.draw();

            // Vertical
            // Replace normals TODO: Build into gbuffer
            sGBuffers[1]->bindAlbedoTexture(freeTextureIndex);
            sGBuffers[1]->bindNormalTexture(freeTextureIndex + 1);
            sGBuffers[0]->use();
            glBlendFunci(e_cast(vg::GBufferAttachmentIndex::ALBEDO), GL_ONE, GL_ZERO);
            glBlendFunci(e_cast(vg::GBufferAttachmentIndex::NORMALS), GL_ONE, GL_ZERO);
            glUniform2f(dirUniform, 0.0f, mBlendTestRadius);
            sGlobalFullTriangleVAO.draw();
        }
    }
    vg::DepthState::restorePrevious();

}

void IEditorViewportPanel::postProcessEdgeTest() {
    ResourceManager& resourceManager = Services::ResourceManager::ref();

    vg::DepthState::NONE.set();
    ui32 freeTextureIndex;
    { // Edge test
        static AssetHandlePtr<MaterialShaderDef> shaderDef = MaterialShaderRepository::get().getAssetHandle(CStrToken("edge_test"));

        const MaterialShaderDef* edgeShader = shaderDef->tryGetLoadedAsset();
        if (!edgeShader) return;

        MaterialRenderer::bindMaterialShaderForRender(*edgeShader, &freeTextureIndex);

        glUniform1i(edgeShader->mProgram.getUniform("unNormalFbo"), freeTextureIndex);
        glUniform1i(edgeShader->mProgram.getUniform("unDepthFbo"), freeTextureIndex + 1);
        glUniform1f(edgeShader->mProgram.getUniform("unEdgeThreshold"), mEdgeTestThreshold);
        glUniform1f(edgeShader->mProgram.getUniform("unDepthThreshold"), mEdgeTestDepthThreshold);
        glUniform2f(edgeShader->mProgram.getUniform("unCameraZRange"), SimpleCamera::ZNEAR, SimpleCamera::ZFAR);
        sGBuffers[0]->bindNormalTexture(freeTextureIndex);
        sGBuffers[0]->bindDepthTexture(freeTextureIndex + 1);

        sGBuffers[1]->use();

        // Replace normals TODO: Build into gbuffer
        glBlendFunci(e_cast(vg::GBufferAttachmentIndex::ALBEDO), GL_ONE, GL_ZERO);
        sGlobalFullTriangleVAO.draw();
    }

    int sourceGBuffer = 1;
    int targetGBuffer = 2;
    { // Edge expand
        static AssetHandlePtr<MaterialShaderDef> shaderDef = MaterialShaderRepository::get().getAssetHandle(CStrToken("edge_expand"));

        const MaterialShaderDef* expandShader = shaderDef->tryGetLoadedAsset();
        if (!expandShader) return;

        MaterialRenderer::bindMaterialShaderForRender(*expandShader, &freeTextureIndex);
        // TODO: Profile just changing the uniform instead of changing the texture binding!
        glUniform1i(expandShader->mProgram.getUniform("unFbo"), freeTextureIndex);
        glUniform1i(expandShader->mProgram.getUniform("unDepthFbo"), freeTextureIndex + 1);
        glUniform1f(expandShader->mProgram.getUniform("unDepthThreshold"), mEdgeTestDepthThreshold);
        glUniform2f(expandShader->mProgram.getUniform("unCameraZRange"), SimpleCamera::ZNEAR, SimpleCamera::ZFAR);

        sGBuffers[0]->bindDepthTexture(freeTextureIndex + 1);
        for (int i = 0; i < mEdgeSize; ++i) {
            // Increments of 2 so it always ends up in gbuffer 2
            for (int j = 0; j < 2; ++j) {
                sGBuffers[sourceGBuffer]->bindAlbedoTexture(freeTextureIndex);
                sGBuffers[targetGBuffer]->use();

                // Replace normals TODO: Build into gbuffer
                // TODO: we dont need clear buffer because of this?
                glBlendFunci(e_cast(vg::GBufferAttachmentIndex::ALBEDO), GL_ONE, GL_ZERO);
                sGlobalFullTriangleVAO.draw();

                std::swap(sourceGBuffer, targetGBuffer);
            }
        }
    }

    { // Blur edges
        static AssetHandlePtr<MaterialShaderDef> shaderDef = MaterialShaderRepository::get().getAssetHandle(CStrToken("blend_test_v2"));

        const MaterialShaderDef* blendShader = shaderDef->tryGetLoadedAsset();
        if (!blendShader) return;

        MaterialRenderer::bindMaterialShaderForRender(*blendShader, &freeTextureIndex);
        const VGUniform& dirUniform = blendShader->mProgram.getUniform("unDirection");
        glUniform1i(blendShader->mProgram.getUniform("unAlbedoFbo"), freeTextureIndex);
        glUniform1i(blendShader->mProgram.getUniform("unNormalFbo"), freeTextureIndex + 1);
        glUniform1i(blendShader->mProgram.getUniform("unEdgeFbo"), freeTextureIndex + 2);
        glUniform1i(blendShader->mProgram.getUniform("unShowEdges"), mEdgeTestShowEdges);

        sGBuffers[2]->bindAlbedoTexture(freeTextureIndex + 2);
        glUniform2f(blendShader->mProgram.getUniform("unScreenResolution"), sGBuffers[0]->getWidth(), sGBuffers[0]->getHeight());
        //glUniform2f(blendShader->mProgram.getUniform("unCameraZRange"), SimpleCamera::ZNEAR, SimpleCamera::ZFAR);
        if (!mEdgeTestDisable) {
            for (int i = 0; i < mEdgeBlendPasses; ++i) {

                // Horizontal
                sGBuffers[0]->bindAlbedoTexture(freeTextureIndex);
                sGBuffers[0]->bindNormalTexture(freeTextureIndex + 1);
                sGBuffers[1]->use();
                // Replace normals TODO: Build into gbuffer
                glBlendFunci(e_cast(vg::GBufferAttachmentIndex::ALBEDO), GL_ONE, GL_ZERO);
                glBlendFunci(e_cast(vg::GBufferAttachmentIndex::NORMALS), GL_ONE, GL_ZERO);
                glUniform2f(dirUniform, mEdgeBlendRadius, 0.0f);
                sGlobalFullTriangleVAO.draw();

                // Vertical
                // Replace normals TODO: Build into gbuffer
                sGBuffers[1]->bindAlbedoTexture(freeTextureIndex);
                sGBuffers[1]->bindNormalTexture(freeTextureIndex + 1);
                sGBuffers[0]->use();
                glBlendFunci(e_cast(vg::GBufferAttachmentIndex::ALBEDO), GL_ONE, GL_ZERO);
                glBlendFunci(e_cast(vg::GBufferAttachmentIndex::NORMALS), GL_ONE, GL_ZERO);
                glUniform2f(dirUniform, 0.0f, mEdgeBlendRadius);
                sGlobalFullTriangleVAO.draw();
            }
        }
    }

    vg::DepthState::restorePrevious();
}
