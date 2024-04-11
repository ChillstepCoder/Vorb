#include "stdafx.h"
#include "ModelEditorViewportPanel.h"

#include "definitions/ModelDef.h"
#include "definitions/RigDef.h"

#include "resources/ModelRepository.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/MaterialRenderer.h"
#include "rendering/MaterialUtils.h"
#include "rendering/Mesh/MeshDrawer.h"
#include "rendering/post_process/ShadowDetail.h"
#include "rendering/mesh/LineMesh.h"
#include "rendering/model/skeletal/SkeletalAnimator.h"
#include "rendering/mesh/Vertex.h"

#include "resources/AnimationRepository.h"

#include <imgui.h>
#include <imgui_internal.h>
#include "ui/ImguiUtil.hpp"
#include "ui/imgui_controls/ObjectVector.h"

#include <Vorb/graphics/GBuffer.h>
#include <Vorb/graphics/DepthState.h>
#include <Vorb/graphics/FullscreenTriangleVAO.h>

#include "camera/SimpleCamera.h"

ModelEditorViewportPanel::ModelEditorViewportPanel()
{
    mSkeletalAnimator = std::make_unique<SkeletalAnimator>();
}

ModelEditorViewportPanel::~ModelEditorViewportPanel()
{
}

void ModelEditorViewportPanel::updateAndRenderPrimaryControls(f32 ySize)
{
    ImGui::BeginChild("Model Editor Controls", ImVec2(0.0f, ySize), true, ImGuiWindowFlags_NoCollapse/* | ImGuiWindowFlags_NoScrollbar*/);
    ImGui::Text("Model Editor Controls");
    ImGui::Separator();

    bool changed = false;

    if (mAssetData) {
        ImGui::Text("Name: %s", mAssetData->getName().toString().c_str());
        updateAndRenderSaveButton();
        if (mAssetData && mAssetData->isSkeletalModel()) {
            ImGui::Checkbox("Skeletal Edit", &mSkeletalEditMode);
            if (mSkeletalEditMode) {
                if (ImguiUtil::updateAndRenderSoftAssetReference("Preview Anim", mPreviewAnim)) {
                    mPreviewAnimTime = 0.0f;
                }
                AssetHandlePtr<AnimationDef> previewHandle = mPreviewAnim.getAssetHandle<AnimationDef>();
                if (previewHandle) {
                    if (const AnimationDef* def = previewHandle->tryGetLoadedAsset()) {
                        ImGui::SliderFloat("Anim Time", &mPreviewAnimTime, 0.0f, def->mAnimation.duration());
                    }
                }
            }
        }
        ImGui::Text("MeshCount %d", mAssetData->getNumMeshes());
        int polyCount = 0;
        for (int i = 0; i < mAssetData->getNumMeshes(); ++i) {
            const Mesh& mesh = mAssetData->getMesh(i);
            polyCount += mesh.mGpuData.mLODData.getDrawInfoForLOD(MeshLODLevel(mLod)).indexCount / 3;
        }
        ImGui::Text("Polygons %d", polyCount);

        if (ImGui::CollapsingHeader("Properties")) {
            changed |= updateAndRenderImguiControls(*mAssetData);

            ImGui::SliderInt("LOD", &mLod, e_cast(MeshLODLevel::Highest), e_cast(MeshLODLevel::Lowest));
            // TODO: Tooltip button utility
            ImGui::SameLine(); ImGui::Button("?");
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip("LOD is auto generated");
            }
        }

        ImGui::Separator();
        if (mAssetData->getNumMeshes()) {
            ImGui::Checkbox("Edit Submesh", &mShowSingle);
            if (mShowSingle) {
                ImGui::Spacing(); ImGui::SameLine();
                ImGui::Text("Submesh Edit");
                ImGui::Spacing(); ImGui::SameLine();
                ImGui::SliderInt("Index", &mSingleIndex, 0, mAssetData->getNumMeshes() - 1);
                ModelSubmeshData& subMeshData = mAssetData->mSubmeshesData[mSingleIndex];

                changed |= updateAndRenderImguiControls(subMeshData);

                ImGui::Separator();
            }
            ImGui::Separator();
            if (ImGui::CollapsingHeader("Variants")) {
                ImGui::SliderInt("Preview Variant", &mVariantIndex, 0, mAssetData->mVariants.size() - 1);
                // Variant controls
                changed |= ImguiUtil::ObjectVector<ModelVariantData>("Variants", mAssetData->mVariants,
                    [](ModelVariantData& o, ui32) {
                        bool changed = false;
                        changed |= updateAndRenderImguiControls(o);
                        changed |= ImguiUtil::ObjectVector<std::array<SoftAssetReference, MATERIAL_SLOT_COUNT>>("Materials", o.submeshMaterials,
                            [](std::array<SoftAssetReference, MATERIAL_SLOT_COUNT>& o, ui32 i) {
                                bool changed = false;
                                for (auto&& r : o) {
                                    changed |= ImguiUtil::updateAndRenderSoftAssetReference(nullptr, r);
                                }
                                return changed;
                            }, false /*resizable*/
                        );
                        return changed;
                    }, true /*resizable*/, mAssetData->mVariants[0]
                );
            }
        }
        else {
            ImGui::Text("*EMPTY MODEL*");
        }
        ImGui::Separator();
        updateAndRenderSharedControls();
        ImGui::Separator();
        updateAndRenderTweakers();
    }

    if (changed) {
        mDirtyModelData = true;
        ModelRepository::get().onAssetChangedByEditor(mAssetData->getID());
    }
    
    ImGui::EndChild();
}

const MaterialShaderDef* ModelEditorViewportPanel::getShader() {
    if (mAssetData && mAssetData->isSkeletalModel() && mSkeletalEditMode && mPreviewAnim.isValid()) {
        return MaterialShaderRepository::get().getAssetHandle(CStrToken("editor_model_skel"))->tryGetLoadedAsset();
    }
    return getModelRenderShader();
}

void ModelEditorViewportPanel::uploadCustomShaderUniforms(const MaterialShaderDef* shader, ui32 availableTextureUnit) {
    UNUSED(shader, availableTextureUnit);
}

void ModelEditorViewportPanel::renderMesh() {
    if (mAssetData) {
        if (mAssetData->isSkeletalModel()) {
            if (mSkeletalEditMode && mPreviewAnim.isValid()) {
                renderMeshSkeletal();
            }
            else {
                renderMeshStatic();
            }
        }
        else {
            renderMeshStatic();
        }

        // Render AABB
        AssetHandlePtr<MaterialShaderDef> handle = MaterialShaderRepository::get().getAssetHandle(CStrToken("simple_color"));
        if (const MaterialShaderDef* def = handle->tryGetLoadedAsset()) {
            if (!mAABBMesh) {
                mAABBMesh = std::make_unique<LineMesh>();
            }
            std::vector<LineVertex> aabbVerts;
            LineMeshBuilders::addAABBLines(aabbVerts, mAssetData->mAABB, color4(1.0f, 0.0f, 0.0f, 0.5f));
            mAABBMesh->initialize(aabbVerts);

            MaterialRenderer::bindMaterialShaderForRender(*def);
            glUniformMatrix4fv(def->getUniform("unVP"), 1, false, &(mCamera->getViewProjectionMatrix()[0][0]));
            mAABBMesh->bind();
            mAABBMesh->drawLines(0);
        }
    }
}

void ModelEditorViewportPanel::renderMeshStatic() {
    const MaterialShaderDef* shader = getShader();
    glUniform1i(shader->getUniform("unVariantIndex"), mVariantIndex);
    glUniform4f(shader->getUniform("unPosOffset"), 0.0f, 0.0f, 0.0f, 0.0f);
    if (mShowSingle) {
        mSingleIndex = glm::min((int)mAssetData->getNumMeshes() - 1, mSingleIndex);
        Mesh& mesh = mAssetData->getMesh(mSingleIndex);
        mesh.unbindStaticModelAttribs(); // Editor doesnt use these
        assert(mesh.mVariantDataUbo);
        glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MODEL_VARIANT_DATA_UBO, mesh.mVariantDataUbo);
        MeshDrawer::draw(mesh.mGpuData, MeshLODLevel(mLod));
        mesh.bindStaticModelAttribs(); // Main game does
    }
    else {
        for (int i = 0; i < mAssetData->getNumMeshes(); ++i) {
            Mesh& mesh = mAssetData->getMesh(i);
            mesh.unbindStaticModelAttribs(); // Editor doesnt use these
            assert(mesh.mVariantDataUbo);
            glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MODEL_VARIANT_DATA_UBO, mesh.mVariantDataUbo);
            MeshDrawer::draw(mesh.mGpuData, MeshLODLevel(mLod));
            mesh.bindStaticModelAttribs(); // Main game does
        }
        int x = 1;
        const ModelLodParams& params = ModelRepository::get().getLodParams(mAssetData->getID());
        for (int l = e_cast(MeshLODLevel::Highest) + 1; l < e_count(MeshLODLevel); ++l) {
            glUniform4f(shader->getUniform("unPosOffset"), x * 5, sqrt(params.lodDistancesSQ[l - 1]), 0.0f, 0.0f);
            for (int i = 0; i < mAssetData->getNumMeshes(); ++i) {
                Mesh& mesh = mAssetData->getMesh(i);
                mesh.unbindStaticModelAttribs(); // Editor doesnt use these
                assert(mesh.mVariantDataUbo);
                glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MODEL_VARIANT_DATA_UBO, mesh.mVariantDataUbo);
                MeshDrawer::draw(mesh.mGpuData, MeshLODLevel(i));
                mesh.bindStaticModelAttribs(); // Main game does
            }
            ++x;
        }
    }
}

void ModelEditorViewportPanel::renderMeshSkeletal() {
    const MaterialShaderDef* shader = getShader();
    glUniform1i(shader->getUniform("unVariantIndex"), mVariantIndex);
    glUniform4f(shader->getUniform("unPosOffset"), 0.0f, 0.0f, 0.0f, 0.0f);
    f32m4 transform(1.0f);
    transform = glm::rotate(transform, DEG_TO_RAD(90.0f), f32v3(0.0f, 0.0f, 1.0f));
    transform = glm::rotate(transform, DEG_TO_RAD(90.0f), f32v3(1.0f, 0.0f, 0.0f));
    glUniformMatrix4fv(shader->getUniform("unM"), 1, false, &transform[0][0]);

    SkeletalAnimationContext context;
    context.samplingContext.Resize(mAssetData->mRig->mSkeleton.num_joints());
    AssetHandlePtr<AnimationDef> animHandle = mPreviewAnim.getAssetHandle<AnimationDef>();
    if (animHandle && animHandle->isLoaded()) {
        const AnimationDef& animDef = animHandle->getLoadedAsset();
        context.anim = &animDef.mAnimation;
        context.time = mPreviewAnimTime;
        OzzSoaTransformVector soaTransforms;
        if (!SkeletalAnimator::samplePose(context, *mAssetData->mRig, soaTransforms)) {
            panic("Anim sample fail!");
        }
        OzzMatrixVector modelMatrices;
        if (!SkeletalAnimator::localToModel(soaTransforms, *mAssetData->mRig, modelMatrices)) {
            panic("Anim LTM fail!");
        }

        for (int i = 0; i < mAssetData->getNumMeshes(); ++i) {
            const SkeletalMesh& mesh = mAssetData->getSkeletalMesh(i);
            const MeshSkeletonData& skelData = mesh.getSkeletonData();
            // TODO: ANIM SHARE FOR LINKED SUBMESHES
            mesh.unbindStaticModelAttribs(); // Editor doesnt use these
            SkinnedModelVertex::bindVertexAttribs(mesh.mGpuData.mVao); // Have to do this or crash
            assert(mesh.mVariantDataUbo);
            glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MODEL_VARIANT_DATA_UBO, mesh.mVariantDataUbo);
           
            OzzMatrixVector skinningMatrices;
            if (!SkeletalAnimator::skinModelMatricesToMesh(skinningMatrices, modelMatrices, skelData)) {
                panic("Anim skinning fail!");
            }
            // Draw animated
            glUniformMatrix4fv(shader->getUniform("unBoneTransforms[0]"), skelData.mNumJoints, false, (const GLfloat*)&skinningMatrices[0].cols);
            // TODO: Indirect?
            MeshDrawer::draw(mesh.mGpuData, MeshLODLevel(mLod));

            mesh.unbindSkeletalModelAttribs(); // Have to do this or crash
        }
    }
}
