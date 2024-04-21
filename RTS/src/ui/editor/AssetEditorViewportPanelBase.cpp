#include "stdafx.h"
#include "AssetEditorViewportPanelBase.h"

#include "resources/AnimationRepository.h"
#include "resources/ModelRepository.h"
#include "rendering/MaterialShaderRepository.h"

#include "definitions/AnimationDef.h"
#include "definitions/ModelDef.h"
#include "definitions/RigDef.h"
#include "rendering/model/skeletal/SkeletalAnimator.h"
#include "rendering/Mesh/MeshDrawer.h"
#include "rendering/mesh/Vertex.h"


AssetEditorViewportPanelBase::AssetEditorViewportPanelBase() = default;
AssetEditorViewportPanelBase::~AssetEditorViewportPanelBase() = default;

const MaterialShaderDef* AssetEditorViewportPanelBase::getModelRenderShader() const
{
    switch (mDrawMode) {
        case EditorViewportDrawMode::PBRTest:
            if (!mPbrMaterial) mPbrMaterial = MaterialShaderRepository::get().getAssetHandle(CStrToken("editor_model_pbr"));
            return mPbrMaterial->tryGetLoadedAsset();
        case EditorViewportDrawMode::BlendTest:
        case EditorViewportDrawMode::EdgeTest:
        case EditorViewportDrawMode::Lit:
        case EditorViewportDrawMode::Unlit:
        case EditorViewportDrawMode::Normals:
        case EditorViewportDrawMode::Tangents:
        case EditorViewportDrawMode::AO:
        case EditorViewportDrawMode::Metallic:
        case EditorViewportDrawMode::Roughness:
        case EditorViewportDrawMode::UVs:
            if (!mEditorMaterial) mEditorMaterial = MaterialShaderRepository::get().getAssetHandle(CStrToken("editor_model"));
            return mEditorMaterial->tryGetLoadedAsset();
        case EditorViewportDrawMode::Wireframe:
            if (!mWireframeMaterial) mWireframeMaterial = MaterialShaderRepository::get().getAssetHandle(CStrToken("mesh_wireframe"));
            return mWireframeMaterial->tryGetLoadedAsset();
        default:
            assert(false);
            break;
    }
    static_assert(e_cast(EditorViewportDrawMode::COUNT) == 12);
    return nullptr;
}

void AssetEditorViewportPanelBase::renderMeshStatic(const ModelDef* modelAsset, int variantIndex, int lod, bool showSingleSubmesh, int singleSubmeshIndex) {
    if (!modelAsset) return;
    const MaterialShaderDef* shader = getShader();
    glUniform1i(shader->getUniform("unVariantIndex"), variantIndex);
    glUniform4f(shader->getUniform("unPosOffset"), 0.0f, 0.0f, 0.0f, 0.0f);
    if (showSingleSubmesh) {
        singleSubmeshIndex = glm::min((int)modelAsset->getNumMeshes() - 1, singleSubmeshIndex);
        const Mesh& mesh = modelAsset->getMesh(singleSubmeshIndex);
        mesh.unbindStaticModelAttribs(); // Editor doesnt use these
        assert(mesh.mVariantDataUbo);
        glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MODEL_VARIANT_DATA_UBO, mesh.mVariantDataUbo);
        MeshDrawer::draw(mesh.mGpuData, MeshLODLevel(lod));
        mesh.bindStaticModelAttribs(); // Main game does
    }
    else {
        for (int i = 0; i < modelAsset->getNumMeshes(); ++i) {
            const Mesh& mesh = modelAsset->getMesh(i);
            mesh.unbindStaticModelAttribs(); // Editor doesnt use these
            assert(mesh.mVariantDataUbo);
            glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MODEL_VARIANT_DATA_UBO, mesh.mVariantDataUbo);
            MeshDrawer::draw(mesh.mGpuData, MeshLODLevel(lod));
            mesh.bindStaticModelAttribs(); // Main game does
        }
        int x = 1;
        const ModelLodParams& params = ModelRepository::get().getLodParams(modelAsset->getID());
        for (int l = e_cast(MeshLODLevel::Highest) + 1; l < e_count(MeshLODLevel); ++l) {
            glUniform4f(shader->getUniform("unPosOffset"), x * 5, sqrt(params.lodDistancesSQ[l - 1]), 0.0f, 0.0f);
            for (int i = 0; i < modelAsset->getNumMeshes(); ++i) {
                const Mesh& mesh = modelAsset->getMesh(i);
                mesh.unbindStaticModelAttribs(); // Editor doesnt use these
                assert(mesh.mVariantDataUbo);
                glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MODEL_VARIANT_DATA_UBO, mesh.mVariantDataUbo);
                MeshDrawer::draw(mesh.mGpuData, MeshLODLevel(i));
                mesh.bindStaticModelAttribs(); // Main game does
            }
            ++x;
        }
    }
    checkGlError("AssetEditorViewportPanelBase::renderMeshStatic");
}

void AssetEditorViewportPanelBase::renderMeshSkeletal(const ModelDef* modelAsset, int variantIndex, int lod, const AnimationDef* previewAnim, f32 previewAnimTime) {
    if (!modelAsset) return;
    assert(modelAsset->isSkeletalModel());
    std::vector<AnimSampleBlendData> anims;
    if (previewAnim) {
        anims.emplace_back(AnimSampleBlendData{ previewAnim, 1.0f, previewAnimTime });
    }
    renderMeshSkeletalBlended(modelAsset, variantIndex, lod, anims);
}

void AssetEditorViewportPanelBase::renderMeshSkeletalBlended(const ModelDef* modelAsset, int variantIndex, int lod, const std::span<AnimSampleBlendData> anims) {
    if (!modelAsset) return;
    assert(modelAsset->isSkeletalModel());
    const MaterialShaderDef* shader = getShader();
    glUniform1i(shader->getUniform("unVariantIndex"), variantIndex);
    glUniform4f(shader->getUniform("unPosOffset"), 0.0f, 0.0f, 0.0f, 0.0f);
    f32m4 transform(1.0f);
    transform = glm::rotate(transform, DEG_TO_RAD(90.0f), f32v3(0.0f, 0.0f, 1.0f));
    transform = glm::rotate(transform, DEG_TO_RAD(90.0f), f32v3(1.0f, 0.0f, 0.0f));
    glUniformMatrix4fv(shader->getUniform("unM"), 1, false, &transform[0][0]);

    SkeletalAnimationSampleContext context;
    const RigDef& rig = *modelAsset->mRig;
    context.samplingContext.Resize(rig.mSkeleton.num_joints());
    OzzMatrixVector modelMatrices;
    if (anims.size()) {
        std::vector<OzzSoaTransformVector> soaTransforms(anims.size());
        std::vector<ozz::animation::BlendingJob::Layer> layers(anims.size());
        int totalLayers = 0;
        for (size_t i = 0; i < anims.size(); ++i) {
            const AnimationDef* def = anims[i].anim;
            if (!def) continue;
            context.anim = &def->animation;
            context.time = anims[i].animTime;
            soaTransforms[i].resize(rig.mSkeleton.num_soa_joints());
            if (!SkeletalAnimator::samplePose(context, rig, ozz::make_span(soaTransforms[i]))) {
                panic("Anim sample fail!");
            }
            layers[totalLayers].transform = make_span(soaTransforms[i]);
            layers[totalLayers].weight = anims[i].weight;
            ++totalLayers;
        }
        modelMatrices.resize(rig.mSkeleton.num_joints());
        if (totalLayers == 1) {
            if (!SkeletalAnimator::localToModel(layers[0].transform, rig, ozz::make_span(modelMatrices))) {
                panic("Anim LTM fail!");
            }
        }
        else if (totalLayers > 1) {
            OzzSoaTransformVector blendOutput;
            // Blending
            layers.resize(totalLayers);
            blendOutput.resize(rig.mSkeleton.num_soa_joints());
            if (!SkeletalAnimator::blendPoses(ozz::make_span(layers), rig, ozz::make_span(blendOutput))) {
                panic("Anim blend fail!");
            }
            if (!SkeletalAnimator::localToModel(ozz::make_span(blendOutput), rig, ozz::make_span(modelMatrices))) {
                panic("Blended Anim LTM fail!");
            }
        }
    }

    for (int i = 0; i < modelAsset->getNumMeshes(); ++i) {
        const SkeletalMesh& mesh = modelAsset->getSkeletalMesh(i);
        const MeshSkeletonData& skelData = mesh.getSkeletonData();
        // TODO: ANIM SHARE FOR LINKED SUBMESHES
        mesh.unbindStaticModelAttribs(); // Editor doesnt use these
        SkinnedModelVertex::bindVertexAttribs(mesh.mGpuData.mVao); // Have to do this or crash
        assert(mesh.mVariantDataUbo);
        glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MODEL_VARIANT_DATA_UBO, mesh.mVariantDataUbo);

        OzzMatrixVector skinningMatrices;
        if (modelMatrices.size()) {
            skinningMatrices.resize(skelData.mNumJoints);
            if (!SkeletalAnimator::skinModelMatricesToMesh(ozz::make_span(modelMatrices), skelData, ozz::make_span(skinningMatrices))) {
                panic("Anim skinning fail!");
            }
        }
        else {
            // Init to identity for T pose
            skinningMatrices.resize(skelData.mNumJoints, ozz::math::Float4x4::identity());
        }
        // Draw animated
        glUniformMatrix4fv(shader->getUniform("unBoneTransforms[0]"), skelData.mNumJoints, false, (const GLfloat*)&skinningMatrices[0].cols);
        // TODO: Indirect?
        MeshDrawer::draw(mesh.mGpuData, MeshLODLevel(lod));

        mesh.unbindSkeletalModelAttribs(); // Have to do this or crash
    }
    checkGlError("AssetEditorViewportPanelBase::renderMeshSkeletal");
}
