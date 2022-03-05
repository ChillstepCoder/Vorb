#include "stdafx.h"
#include "CharacterRenderer.h"

#include "rendering/MaterialManager.h"

#include "rendering/MaterialRenderer.h"
#include "rendering/TileVertex.h"
#include "rendering/SpriteData.h"

#include "resources/ModelRepository.h"

#include "options/DebugOptions.h"

#include "Random.h"

#include "QuadMesh.h"
#include "camera/Camera3D.h"

#include "definitions/RigDef.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/span.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/sampling_job.h"

//void renderPart(vg::SpriteBatch& sb, const vg::Texture& body, const f32v2& pos, f32 zPos, const f32v2& offset, const f32v2& additionalOffset, f32v4& uvRect, float size, float depth, float alpha) {
//	//f32v2 sizeVec(size);
//	//f32v2 newPos = pos + offset - sizeVec.x * 0.5f;
//	//sb.draw(body.id, &uvRect, nullptr, newPos, -additionalOffset, sizeVec, 0.0f /*rotation*/, color4(1.0f, 1.0f, 1.0f, alpha), depth + zPos);
//
//    BillboardVertex verts[4];
//}

CharacterRenderer::CharacterRenderer(const MaterialManager& materialManager, const ModelRepository& modelRepo) :
    mMaterial(materialManager.getMaterial("character")), mModelRepo(modelRepo) {
}

CharacterRenderer::~CharacterRenderer() {

}

ozz::animation::SamplingJob::Context sContext;

void CharacterRenderer::addModel(const Camera3D& camera, const CharacterModel& model, const f32v3& position, float angle, float alpha, const MaterialRenderer& materialRenderer) {

    VGUniform offsetUniform = mMaterial->mProgram.getUniform("unOffset");
    VGUniform modelTransformUniform = mMaterial->mProgram.getUniform("unModelTransform");
    VGUniform diffuseTextureUniform = mMaterial->mProgram.getUniform("unDiffuse");
    VGUniform normalTextureUniform = mMaterial->mProgram.getUniform("unNormal");
    VGUniform specularTextureUniform = mMaterial->mProgram.getUniform("unSpecular");
    VGUniform scaleUniform = mMaterial->mProgram.getUniform("unScale");
    VGUniform boneUniform = mMaterial->mProgram.getUniform("unBoneTransforms[0]");

    const ModelDef& modelDef = mModelRepo.getModelDef(0);
    ui32 nextTextureIndex = 0;
    materialRenderer.bindMaterialForRender(*mMaterial, &nextTextureIndex);
    glUniform1i(diffuseTextureUniform, nextTextureIndex);
    glUniform1i(normalTextureUniform, nextTextureIndex + 1);
    glUniform1i(specularTextureUniform, nextTextureIndex + 2);
    glUniform1f(scaleUniform, 0.5f);

    // TODO: Optimize
    f32m4 transform(1.0f);
    transform = glm::rotate(transform, angle + DEG_TO_RAD(90.0f), f32v3(0.0f, 0.0f, 1.0f));
    transform = glm::rotate(transform, DEG_TO_RAD(90.0f), f32v3(1.0f, 0.0f, 0.0f));

    const f32v3 offset = position - camera.getPosition();
    glUniform3fv(offsetUniform, 1, &offset.x);
    glUniformMatrix4fv(modelTransformUniform, 1, false, &transform[0][0]);

    // Buffer of local transforms as sampled from animation_.
    ozz::vector<ozz::math::SoaTransform> locals;
    // Buffer of model space matrices.
    ozz::vector<ozz::math::Float4x4> models;
    // Buffer of skinning matrices, result of the joint multiplication of the
    // inverse bind pose with the model space matrix.
    ozz::vector<ozz::math::Float4x4> skinningMatrices;

   
    // Animation and skinning
    {
        const RigDef* rig = modelDef.mRig;
        if (rig) {
            // Allocates runtime buffers.
            // TODO: Cache
            const int num_soa_joints = rig->mSkeleton.num_soa_joints();
            locals.resize(num_soa_joints);
            const int num_joints = rig->mSkeleton.num_joints();
            models.resize(num_joints);

            // Allocates a context that matches animation requirements.
            sContext.Resize(num_joints);

            // TODO: cache
            ui8 num_skinning_matrices = 0;
            for (ui32 i = 0; i < modelDef.mModel.mNumMeshes; ++i) {
                num_skinning_matrices =
                    std::max(num_skinning_matrices, modelDef.mModel.mMeshes[i].getNumJoints());
            }

            // Allocates skinning matrices.
            skinningMatrices.resize(num_skinning_matrices);


            ozz::animation::SamplingJob sampling_job;
            sampling_job.animation = &rig->mAnimations[0];
            sampling_job.context = &sContext;
            sampling_job.ratio = sDebugOptions.mDebugFloat02;
            sampling_job.output = make_span(locals);
            if (!sampling_job.Run()) {
                pError("Sampling job error");
                return;
            }

            // Converts from local space to model space matrices.
            ozz::animation::LocalToModelJob ltm_job;
            ltm_job.skeleton = &rig->mSkeleton;
            ltm_job.input = make_span(locals);
            ltm_job.output = make_span(models);
            if (!ltm_job.Run()) {
                pError("Local to model job error");
                return;
            }

        }
    }

    for (ui32 i = 0; i < modelDef.mModel.mNumMeshes; ++i) {
        const auto& mesh = modelDef.mModel.mMeshes[i];
        const ozz::math::Float4x4* bindPoses = mesh.getInverseBindPoses();
        for (size_t i = 0; i < mesh.getNumJoints(); ++i) {
            skinningMatrices[i] = models[mesh.getJointRemaps()[i]] * bindPoses[i];
        }
        glActiveTexture(GL_TEXTURE0 + nextTextureIndex);
        glBindTexture(GL_TEXTURE_2D, mesh.getDiffuseTexture());
        glActiveTexture(GL_TEXTURE0 + nextTextureIndex + 1);
        glBindTexture(GL_TEXTURE_2D, mesh.getNormalTexture());
        glActiveTexture(GL_TEXTURE0 + nextTextureIndex + 2);
        glBindTexture(GL_TEXTURE_2D, mesh.getSpecularTexture());
        glUniformMatrix4fv(boneUniform, mesh.getNumJoints(), false, (const GLfloat*)&skinningMatrices[0].cols);

        mesh.draw(mMaterial->mProgram);
    }
}

void CharacterRenderer::renderBatch(const Camera3D& camera, const MaterialRenderer& materialRenderer) {
    /* mMesh->finishMesh(MeshDrawMode::STREAM);
     materialRenderer.bindMaterialForRender(*mMaterial);
     f32v3 offset = -camera.getPosition();
     glUniform3fv(mMaterial->mProgram.getUniform("unOffset"), 1, &offset.x);
     mMesh->draw(mMaterial->mProgram);*/
}

// Prevent rounding errors, 0.0001 is half a pixel
constexpr f32 UV_EPSILON = 0.0001f;
constexpr f32 UV_EPSILON_2 = 2.0f * UV_EPSILON;
