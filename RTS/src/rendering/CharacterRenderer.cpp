#include "stdafx.h"
#include "CharacterRenderer.h"

#include "ecs/component/PhysicsComponent.h"
#include "ecs/component/LocomotionComponent.h"

#include "rendering/MaterialManager.h"

#include "rendering/MaterialRenderer.h"
#include "rendering/TileVertex.h"
#include "rendering/SpriteData.h"

#include "resources/ModelRepository.h"
#include "ResourceManager.h"

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
#include "ozz/animation/runtime/blending_job.h"

//void renderPart(vg::SpriteBatch& sb, const vg::Texture& body, const f32v2& pos, f32 zPos, const f32v2& offset, const f32v2& additionalOffset, f32v4& uvRect, float size, float depth, float alpha) {
//	//f32v2 sizeVec(size);
//	//f32v2 newPos = pos + offset - sizeVec.x * 0.5f;
//	//sb.draw(body.id, &uvRect, nullptr, newPos, -additionalOffset, sizeVec, 0.0f /*rotation*/, color4(1.0f, 1.0f, 1.0f, alpha), depth + zPos);
//
//    BillboardVertex verts[4];
//}

CharacterRenderer::CharacterRenderer() :
    mMaterial(Services::ResourceManager::ref().getMaterialManager().getMaterial("character")) {
}

CharacterRenderer::~CharacterRenderer() {

}

void updateAnimationStates(const PhysicsComponent& physCmp, const LocomotionComponent& motionCmp, CharacterModelComponent& cmp, f32 elapsedSec) {

    // Update feel
    cmp.updateFootstepAlpha(elapsedSec, motionCmp.mMode);

    constexpr f32 FADE_IN_SLOW = 0.3f;
    constexpr f32 FADE_IN_MEDIUM = 0.2f;
    constexpr f32 FADE_IN_FAST = 0.1f;
    const bool isTransitioning = (motionCmp.mMode != cmp.mPrevLocomotionMode);

    // Update transition
    if (isTransitioning) {

        // Fade out previous state
        if (cmp.mAnimState.mPrimaryStateTrack != UINT8_MAX) {
            cmp.mAnimState.mTracks[cmp.mAnimState.mPrimaryStateTrack].fadeOut(FADE_IN_SLOW);
        }

        // TODO: Array lookup mapping instead of switch?
        switch (motionCmp.mMode) {
            case LocomotionMode::IDLE: {
                cmp.mAnimState.fadeInStateTrack(AnimMachineState::IDLE, FADE_IN_SLOW);
                break;
            }
            case LocomotionMode::WALK: {
                cmp.mAnimState.fadeInStateTrack(AnimMachineState::WALK_FRONT, FADE_IN_SLOW);
                break;
            }
            case LocomotionMode::RUN: {
                cmp.mAnimState.fadeInStateTrack(AnimMachineState::RUN_FRONT, FADE_IN_SLOW);
                break;
            }
            case LocomotionMode::SPRINT: {
                cmp.mAnimState.fadeInStateTrack(AnimMachineState::SPRINT_FRONT, FADE_IN_SLOW);
                break;
            }
            case LocomotionMode::DODGE: {

                break;
            }
            case LocomotionMode::BEGIN_JUMP:
                assert(false && "We should never try to play Begin Jump anim");
                break;
            case LocomotionMode::JUMPING: {
                cmp.mAnimState.fadeInStateTrack(AnimMachineState::JUMPING, FADE_IN_FAST);
                break;
            }
            case LocomotionMode::FALLING: {
                cmp.mAnimState.fadeInStateTrack(AnimMachineState::FALLING, FADE_IN_MEDIUM);
                break;
            }
            case LocomotionMode::LANDING: {
                cmp.mAnimState.fadeInStateTrack(AnimMachineState::LANDING, FADE_IN_FAST);
                break;
            }
            default:
                assert(false && "Invalid LocomotionMode");
                break;

        }
    }
    static_assert(e_cast(LocomotionMode::COUNT) == 9, "Update anim mapping");

    // Any transitions are now over
    cmp.mPrevLocomotionMode = motionCmp.mMode;
}

bool updateAnimation(const PhysicsComponent& physCmp, CharacterModelComponent& cmp, const LocomotionComponent& motionCmp, ozz::vector<ozz::math::Float4x4>& models, f32 elapsedSec) {

    // Speed blend, run/walk/sprint
    updateAnimationStates(physCmp, motionCmp, cmp, elapsedSec);

    // Buffer of local transforms as sampled from animation_.
    // TODO: Stack allocate these with joint limits and stop using make_span? Or if too large, shared heap memory
    ozz::vector<ozz::math::SoaTransform> locals[NUM_ANIM_STATE_TRACKS + 1];
    f32 blendWeights[NUM_ANIM_STATE_TRACKS + 1];
    ozz::vector<ozz::math::SoaTransform> blendedLocals;

    assert(cmp.mModel);
    assert(cmp.mModel->mAnimMachine);
    assert(cmp.mModel->mRig);

    const ModelDef& modelDef = *cmp.mModel;
    const AnimMachineDef& machine = *modelDef.mAnimMachine;
    const RigDef& rig = *modelDef.mRig;

    // Animation and skinning

    // Allocates runtime buffers.
    // TODO: Cache
    const int numSoaJoints = rig.mSkeleton.num_soa_joints();
    blendedLocals.resize(numSoaJoints);
    const int numJoints = rig.mSkeleton.num_joints();
    models.resize(numJoints);

    // TODO: cache
    ui8 num_skinning_matrices = 0;
    for (ui32 i = 0; i < modelDef.mModel.getNumMeshes(); ++i) {
        num_skinning_matrices =
            std::max(num_skinning_matrices, modelDef.mModel.getMeshes()[i].getNumJoints());
    }

    ui32 numValidTracks = 0;
    for (ui32 i = 0; i < NUM_ANIM_STATE_TRACKS; ++i) {
        AnimTrack& currentTrack = cmp.mAnimState.mTracks[i];
        // If our weightScale made us inactive, make sure to fully disable
        if (!currentTrack.isActive()) {
            // Always force fadeout
            if (currentTrack.mFlags.isBitSet(AnimTrackFlags::IS_FADING_OUT)) {
                currentTrack.mWeight = 0;
                currentTrack.mFlags.clearBit(AnimTrackFlags::IS_FADING_OUT);
            }
            continue;
        }
        // Allocate buffers
        locals[numValidTracks].resize(numSoaJoints);
        // Sample animation
        ozz::animation::SamplingJob sampling_job;
        sampling_job.animation = machine.mAnimsArray[i];
        sampling_job.context = currentTrack.mContext.get();
        sampling_job.ratio = currentTrack.mTime / currentTrack.mDuration;
        sampling_job.output = make_span(locals[numValidTracks]);
        blendWeights[numValidTracks] = currentTrack.getTotalWeight();
        ++numValidTracks;
        if (!sampling_job.Run()) {
            pError("Sampling job error");
            return false;
        }

        // Increment timers
        currentTrack.update(elapsedSec, cmp.mFootstepAlpha);
    }

    // One shot animation
    f32 oneShotWeight = 0.0f;
    AnimTrack& oneShotTrack = cmp.mAnimState.mCurrentOneShotTrack;
    if (oneShotTrack.isActive()) {
        oneShotWeight = oneShotTrack.getTotalWeight();
        // Allocate buffers
        locals[numValidTracks].resize(numSoaJoints);
        // Sample animation
        ozz::animation::SamplingJob sampling_job;
        sampling_job.animation = cmp.mAnimState.mCurrentOneShotAnimation;
        sampling_job.context = oneShotTrack.mContext.get();
        sampling_job.ratio = oneShotTrack.mTime / oneShotTrack.mDuration;
        sampling_job.output = make_span(locals[numValidTracks]);
        blendWeights[numValidTracks] = oneShotWeight;
        if (!sampling_job.Run()) {
            pError("Sampling job error");
            return false;
        }

        // Increment timers
        oneShotTrack.update(elapsedSec, cmp.mFootstepAlpha);
    }

    // Converts from local space to model space matrices.
    ozz::animation::LocalToModelJob ltm_job;
    ltm_job.skeleton = &rig.mSkeleton;

    // Blending
    if (numValidTracks > 1 || oneShotWeight) {

        const f32 inverseOneShotWeightMult = 1.0f - oneShotWeight;
        int totalLayers = 0;
        // Prepares blending layers.
        ozz::animation::BlendingJob::Layer layers[(NUM_ANIM_STATE_TRACKS + 1) * 2]; // Account for splitting layers

        // While one shots are active, blending is more complex as we must split lower and upper body blending
        if (oneShotWeight) {
            // Split all layers into upper and lower body portions based on the one shot weight
            if (numValidTracks > 0) {
                if (oneShotWeight == 1.0f) {
                    // If we are at full weight, we have no upper body layers
                    for (ui32 i = 0; i < numValidTracks; ++i) {
                        layers[totalLayers].transform = make_span(locals[i]);
                        layers[totalLayers].weight = blendWeights[i];
                        layers[totalLayers].joint_weights = make_span(rig.mLowerBodyJointWeights);
                        ++totalLayers;
                    }
                }
                else {
                    // Split into two layers for upper and lower portion
                    const f32 upperBodyWeight = 1.0f - oneShotWeight;
                    for (ui32 i = 0; i < numValidTracks; ++i) {
                        // Lower body
                        layers[totalLayers].transform = make_span(locals[i]);
                        layers[totalLayers].weight = blendWeights[i];
                        layers[totalLayers].joint_weights = make_span(rig.mLowerBodyJointWeights);
                        ++totalLayers;

                        // Upper body
                        layers[totalLayers].transform = make_span(locals[i]);
                        layers[totalLayers].weight = blendWeights[i] * upperBodyWeight;
                        layers[totalLayers].joint_weights = make_span(rig.mUpperBodyJointWeights);
                        ++totalLayers;
                    }
                }

                // The final layer is the one shot layer, flag it upper body only if we are in motion
                // TODO: Need to fade this in as well to prevent pop?
                if (motionCmp.mMode != LocomotionMode::IDLE) {
                    layers[totalLayers].joint_weights = make_span(rig.mUpperBodyJointWeights);
                }
            }

            // Add one shot locals
            layers[totalLayers].transform = make_span(locals[numValidTracks]);
            layers[totalLayers].weight = blendWeights[numValidTracks];
            ++totalLayers;
        }
        else {
            // No one shot, standard, cheap full blending for each anim
            for (ui32 i = 0; i < numValidTracks; ++i) {
                layers[totalLayers].transform = make_span(locals[i]);
                layers[totalLayers].weight = blendWeights[i];
                ++totalLayers;
            }
        }

        // Setups blending job.
        ozz::animation::BlendingJob blend_job;
        blend_job.threshold = 0.015f;
        blend_job.layers = ozz::span{layers, size_t(totalLayers)};
        blend_job.rest_pose = rig.mSkeleton.joint_rest_poses();
        blend_job.output = make_span(blendedLocals);

        // Blends.
        if (!blend_job.Run()) {
            pError("Blending job error");
            return false;
        }
        ltm_job.input = make_span(blendedLocals);
    }
    else {
        ltm_job.input = make_span(locals[0]);
    }

    // Run the final job
    if (numValidTracks) {
        ltm_job.output = make_span(models);
        if (!ltm_job.Run()) {
            pError("Local to model job error");
            return false;
        }
        return true;
    }
    return false;

}

void CharacterRenderer::addModel(const Camera3D& camera, CharacterModelComponent& cmp, const PhysicsComponent& physCmp, const LocomotionComponent& motionCmp, f32 elapsedSec, f32 frameAlpha, const MaterialRenderer& materialRenderer) {

    // Get physics info
    const f32 angle = atan2(physCmp.mDir.y, physCmp.mDir.x);
    f32v2 interpolatedXY = physCmp.getXYInterpolated(frameAlpha);
    f32 interpolatedZ = physCmp.getZInterpolated(frameAlpha);
    const f32v3 position(interpolatedXY.x, interpolatedXY.y, interpolatedZ);

    VGUniform offsetUniform = mMaterial->mProgram.getUniform("unOffset");
    VGUniform modelTransformUniform = mMaterial->mProgram.getUniform("unModelTransform");
    VGUniform diffuseTextureUniform = mMaterial->mProgram.getUniform("unDiffuse");
    VGUniform normalTextureUniform = mMaterial->mProgram.getUniform("unNormal");
    VGUniform specularTextureUniform = mMaterial->mProgram.getUniform("unSpecular");
    VGUniform scaleUniform = mMaterial->mProgram.getUniform("unScale");
    VGUniform boneUniform = mMaterial->mProgram.getUniform("unBoneTransforms[0]");

    const ModelDef& modelDef = *cmp.mModel;
    ui32 nextTextureIndex = 0;
    materialRenderer.bindMaterialForRender(*mMaterial, &nextTextureIndex);
    glUniform1i(diffuseTextureUniform, nextTextureIndex);
    glUniform1i(normalTextureUniform, nextTextureIndex + 1);
    glUniform1i(specularTextureUniform, nextTextureIndex + 2);
    glUniform1f(scaleUniform, 1.0f);

    // TODO: Optimize
    f32m4 transform(1.0f);
    transform = glm::rotate(transform, angle + DEG_TO_RAD(90.0f), f32v3(0.0f, 0.0f, 1.0f));
    transform = glm::rotate(transform, DEG_TO_RAD(90.0f), f32v3(1.0f, 0.0f, 0.0f));

    const f32v3 offset = position - camera.getPosition();
    glUniform3fv(offsetUniform, 1, &offset.x);
    glUniformMatrix4fv(modelTransformUniform, 1, false, &transform[0][0]);

    // Buffer of model space matrices.
    ozz::vector<ozz::math::Float4x4> models;
    // Buffer of skinning matrices, result of the joint multiplication of the
    // inverse bind pose with the model space matrix.
    ozz::vector<ozz::math::Float4x4> skinningMatrices;
    // Allocates skinning matrices.
    skinningMatrices.resize(modelDef.mModel.getNumSkinningMatrices());

    if (updateAnimation(physCmp, cmp, motionCmp, models, elapsedSec)) {
        // Draw animated
        for (ui32 i = 0; i < modelDef.mModel.getNumMeshes(); ++i) {
            const auto& mesh = modelDef.mModel.getMeshes()[i];
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
    else {
        // INVALID ANIMATION
        // Draw T pose
        for (ui32 i = 0; i < modelDef.mModel.getNumMeshes(); ++i) {
            const auto& mesh = modelDef.mModel.getMeshes()[i];
            for (size_t i = 0; i < mesh.getNumJoints(); ++i) {
                skinningMatrices[i] = ozz::math::Float4x4::identity();
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
