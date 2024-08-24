#include "stdafx.h"
#include "AnimMachineInstance.h"

#include "definitions/RigDef.h"

#include "resources/AnimMachineRepository.h"
#include "rendering/model/skeletal/SkeletalAnimator.h"


// 14 layers accounts for blending between two blendspace2D + one shot and upper/lower splitting
constexpr ui32 MAX_ANIM_UPDATE_CONTEXT_LAYERS = 14;
struct AnimMachineUpdateContext {
    ozz::animation::BlendingJob::Layer layers[MAX_ANIM_UPDATE_CONTEXT_LAYERS];
    ozz::math::SoaTransform transforms[MAX_ANIM_UPDATE_CONTEXT_LAYERS][MAX_JOINTS_IN_RIG];
    int numLayers = 0;
    const AnimVariables* variables = nullptr;
    bool updatePose = false;
};

AnimMachineInstance::AnimMachineInstance(AssetID animMachineID) {
    machineDef = &AnimMachineRepository::get().getLoadedAsset(animMachineID);
    initInternal();
}

void AnimMachineInstance::update(f32 elapsedSec, const AnimVariables& animVariables, OzzMatrixSpan outModelMatrices) {
    PROFILE_FUNCTION();
    if (!numStates) [[unlikely]] {
        return;
    }

    assert(states);
    assert(currentStateID < numStates);

    // This is a very large stack allocation
    AnimMachineUpdateContext updateContext;
    updateContext.variables = &animVariables;
    updateContext.updatePose = outModelMatrices.size() != 0;

    AnimMachineInstanceState* currentState = &states[currentStateID];

    f32 currentStateWeight = 1.0f;

    // Check for valid transitions
    if (currentTransitionID == INVALID_ANIM_TRANSITION) {
        // Check for valid transitions, taking first valid
        for (size_t i = 0; i < currentState->transitions.size(); ++i) {
            const AnimTransition& transition = currentState->transitions[i];
            if (transition.condition.passesCondition(animVariables)) {
                assert(transition.toState < numStates);
                if (transition.transitionDuration) {
                    currentTransitionTime = 0.0f;
                    currentTransitionID = i;
                    onBeginState(states[transition.toState]);
                }
                else {
                    currentStateID = transition.toState;
                    currentState = &states[currentStateID];
                    onBeginState(*currentState);
                }
                break;
            }
        }
    }
    else if (currentTransitionID != INVALID_ANIM_TRANSITION) {
        // Update current transition, we will only start updating the transition a frame late,
        // but its not a big deal probably
        const AnimTransition& currentTransition = currentState->transitions[currentTransitionID];
        // Allow reversal if condition is no longer satisfied
        const bool isTransitioningForward = currentTransition.condition.passesCondition(animVariables);

        assert(currentTransition.transitionAnimID == INVALID_ASSET_ID && "NEED TO IMPLEMENT ANIM TRANSITION");
        assert(currentTransition.toState != INVALID_ANIM_STATE);
        if (isTransitioningForward) {
            currentTransitionTime += elapsedSec;
            if (currentTransitionTime >= currentTransition.transitionDuration) {
                currentTransitionID = INVALID_ANIM_TRANSITION;
                currentStateID = currentTransition.toState;
                currentState = &states[currentStateID];
            }
            else {
                const f32 nextStateWeight = (currentTransitionTime / currentTransition.transitionDuration);
                currentStateWeight = 1.0f - nextStateWeight;
                updateState(states[currentTransition.toState], elapsedSec, updateContext, nextStateWeight);
            }
        }
        else {
            currentTransitionTime -= elapsedSec;
            if (currentTransitionTime <= 0.0f) {
                // Abort the transition
                currentTransitionID = INVALID_ANIM_TRANSITION;
            }
            else {
                const f32 nextStateWeight = (currentTransitionTime / currentTransition.transitionDuration);
                currentStateWeight = 1.0f - nextStateWeight;
                updateState(states[currentTransition.toState], elapsedSec, updateContext, nextStateWeight);
            }
        }
    }

    // Update the current state
    updateState(*currentState, elapsedSec, updateContext, currentStateWeight);

    // Add one shot animation layer
    updateOneShot(elapsedSec, updateContext);

    if (updateContext.updatePose) {
        if (updateContext.numLayers == 1) {
            // No blend
            if (!SkeletalAnimator::localToModel(updateContext.layers[0].transform, *rigDef, outModelMatrices)) {
                panic("Anim LTM fail!");
            }
        }
        else {
            assert(updateContext.numLayers);
            // Blending
            ozz::math::SoaTransform soaTransforms[MAX_JOINTS_IN_RIG];
            OzzSoaTransformSpan blendOutput(soaTransforms, rigDef->mSkeleton.num_soa_joints());

            ozz::span<const ozz::animation::BlendingJob::Layer> layers(updateContext.layers, updateContext.numLayers);
            if (!SkeletalAnimator::blendPoses(layers, *rigDef, blendOutput)) {
                panic("Anim blend fail!");
            }
            if (!SkeletalAnimator::localToModel(blendOutput, *rigDef, outModelMatrices)) {
                panic("Blended Anim LTM fail!");
            }
        }
    }
}

bool AnimMachineInstance::tryPlayOneShot(const AnimationDef& animDef) {
    if (oneShotAnim) {
        return false;
    }
    oneShotAnim = &animDef;
    oneShotTime = 0.0f;
    return true;
}

void AnimMachineInstance::initInternal() {
    assert(machineDef->instanceTemplate);
    const AnimMachineInstance& defaultInstance = *machineDef->instanceTemplate;

    // Fast initialization using the template
    rigDef = defaultInstance.rigDef;
    if (defaultInstance.numStates) {
        numStates = defaultInstance.numStates;
        states = std::make_unique<AnimMachineInstanceState[]>(numStates);
        memcpy(states.get(), defaultInstance.states.get(), numStates * sizeof(AnimMachineInstanceState));
    }
    if (defaultInstance.numBlendspace1DPlayers) {
        numBlendspace1DPlayers = defaultInstance.numBlendspace1DPlayers;
        blendspace1DPlayers = std::make_unique<Blendspace1DPlayer[]>(numBlendspace1DPlayers);
        memcpy(blendspace1DPlayers.get(), defaultInstance.blendspace1DPlayers.get(), numBlendspace1DPlayers * sizeof(Blendspace1DPlayer));
    }
}

void AnimMachineInstance::updateState(AnimMachineInstanceState& state, f32 elapsedSec, AnimMachineUpdateContext& updateContext, f32 weight) {
    switch (state.stateType) {
        case AnimStateType::AnimSequence:
            updateLoopingAnimSequence(state, elapsedSec, updateContext, weight);
            break;
        case AnimStateType::Blendspace1D:
            updateBlendspace1D(state, elapsedSec, updateContext, weight);
            break;
        case AnimStateType::Blendspace2D:
            updateBlendspace2D(state, elapsedSec, updateContext, weight);
            break;
        default:
            panic("Invalid state type");
    }
    static_assert(e_count(AnimStateType) == 3);
}

void AnimMachineInstance::updateLoopingAnimSequence(AnimMachineInstanceState& state, f32 elapsedSec, AnimMachineUpdateContext& updateContext, f32 weight) {
    state.anim.time += elapsedSec;

    const AnimationDef& animDef = state.anim.animDef.getLoadedOrUnloadedAsset<AnimationDef>();

    if (state.anim.time > animDef.animation.duration()) [[unlikely]] {
        state.anim.time = fmod(state.anim.time, animDef.animation.duration());
    }

    if (updateContext.updatePose) {
        // TODO: Cache context for performance!
        SkeletalAnimationSampleContext context;
        context.anim = &animDef.animation;
        context.time = state.anim.time;
        const int NUM_JOINTS = rigDef->mSkeleton.num_joints();
        const int NUM_SOA_JOINTS = rigDef->mSkeleton.num_soa_joints();
        context.samplingContext.Resize(NUM_JOINTS);

        assert(updateContext.numLayers < MAX_ANIM_UPDATE_CONTEXT_LAYERS);
        auto& layer = updateContext.layers[updateContext.numLayers];
        layer.weight = weight;
        OzzSoaTransformSpan transforms = OzzSoaTransformSpan(updateContext.transforms[updateContext.numLayers], NUM_SOA_JOINTS);
        if (!SkeletalAnimator::samplePose(context, *rigDef, transforms)) [[unlikely]] {
            panic("Anim sample fail!");
        }
        layer.transform = transforms;
        ++updateContext.numLayers;
    }
}

void AnimMachineInstance::updateBlendspace1D(AnimMachineInstanceState& state, f32 elapsedSec, AnimMachineUpdateContext& updateContext, f32 weight) {
    assert(state.blendspace1d.playerId < numBlendspace1DPlayers);
    Blendspace1DPlayer& player = blendspace1DPlayers[state.blendspace1d.playerId];
    AnimSampleBlendDataPair blendData = player.updateAndGetBlendData(*updateContext.variables, elapsedSec);

    if (updateContext.updatePose) {
        const int NUM_JOINTS = rigDef->mSkeleton.num_joints();
        const int NUM_SOA_JOINTS = rigDef->mSkeleton.num_soa_joints();
        for (int i = 0; i < blendData.validCount; ++i) {

            assert(updateContext.numLayers < MAX_ANIM_UPDATE_CONTEXT_LAYERS);
            auto& layer = updateContext.layers[updateContext.numLayers];
            const AnimSampleBlendData& data = blendData.arry[i];
            layer.weight = data.weight * weight;

            // TODO: Cache context for performance!
            SkeletalAnimationSampleContext sampleContext;
            sampleContext.anim = &data.anim->animation;
            sampleContext.time = data.animTime;
            sampleContext.samplingContext.Resize(NUM_JOINTS);
            OzzSoaTransformSpan transforms = OzzSoaTransformSpan(updateContext.transforms[updateContext.numLayers], NUM_SOA_JOINTS);
            if (!SkeletalAnimator::samplePose(sampleContext, *rigDef, transforms)) [[unlikely]] {
                panic("Anim sample fail!");
            }
            layer.transform = transforms;
            ++updateContext.numLayers;
        }
    }
}

void AnimMachineInstance::updateBlendspace2D(AnimMachineInstanceState& state, f32 elapsedSec, AnimMachineUpdateContext& updateContext, f32 weight) {
    panic("Implement updateBlendspace2D");
}

void AnimMachineInstance::updateOneShot(f32 elapsedSec, AnimMachineUpdateContext& updateContext) {
    if (!oneShotAnim) {
        return;
    }
    oneShotTime += elapsedSec;

    if (!updateContext.updatePose) {
        return;

    }
    const int NUM_JOINTS = rigDef->mSkeleton.num_joints();
    const int NUM_SOA_JOINTS = rigDef->mSkeleton.num_soa_joints();

    const ozz::animation::Animation& anim = oneShotAnim->animation;
    if (oneShotTime > anim.duration()) {
        oneShotAnim = nullptr;
        oneShotTime = 0;
        return;
    }

    f32 oneShotWeight;
    if (oneShotTime <= oneShotAnim->blendInDuration) {
        oneShotWeight = oneShotTime / oneShotAnim->blendInDuration;
    }
    else if (oneShotTime < anim.duration() - oneShotAnim->blendOutDuration) {
        oneShotWeight = 1.0f;
    }
    else {
        oneShotWeight = (anim.duration() - oneShotTime) / oneShotAnim->blendOutDuration;
    }

    // TODO: Need class for blend that handles max blend speed, this blends out too fast
    constexpr f32 SPEED_BLEND = 1.0f;
    f32 oneShotLowerBodyWeight = 0.0f;
    if (updateContext.variables->speed < SPEED_BLEND) {
        const f32 lowerRatio = 1.0f - updateContext.variables->speed / SPEED_BLEND;
        oneShotLowerBodyWeight = oneShotWeight * lowerRatio;
    }

    blendAndSplitLayersForOneShot(updateContext, oneShotWeight, oneShotLowerBodyWeight);

    assert(updateContext.numLayers < MAX_ANIM_UPDATE_CONTEXT_LAYERS);
    auto& layer = updateContext.layers[updateContext.numLayers];
    layer.weight = oneShotWeight;
    // TODO: Cache context for performance!
    SkeletalAnimationSampleContext sampleContext;
    sampleContext.anim = &anim;
    sampleContext.time = oneShotTime;
    sampleContext.samplingContext.Resize(NUM_JOINTS);
    OzzSoaTransformSpan transforms = OzzSoaTransformSpan(updateContext.transforms[updateContext.numLayers], NUM_SOA_JOINTS);
    if (!SkeletalAnimator::samplePose(sampleContext, *rigDef, transforms)) [[unlikely]] {
        panic("One shot anim sample fail!");
    }
    layer.transform = transforms;
    ++updateContext.numLayers;

    if (oneShotLowerBodyWeight == 0.0f) {
        // If no lower body, set to upper only
        layer.joint_weights = ozz::make_span(rigDef->mUpperBodyJointWeights);
    }
    else if (oneShotLowerBodyWeight != oneShotWeight) {
        layer.joint_weights = ozz::make_span(rigDef->mUpperBodyJointWeights);
        // Split out lower at separate weight

        assert(updateContext.numLayers < MAX_ANIM_UPDATE_CONTEXT_LAYERS);
        auto& lowerLayer = updateContext.layers[updateContext.numLayers];
        lowerLayer.weight = oneShotLowerBodyWeight;
        lowerLayer.transform = layer.transform;
        lowerLayer.joint_weights = ozz::make_span(rigDef->mLowerBodyJointWeights);
        ++updateContext.numLayers;
    }
}

void AnimMachineInstance::blendAndSplitLayersForOneShot(AnimMachineUpdateContext& updateContext, f32 oneShotUpperWeight, f32 oneShotLowerWeight) {
    const f32 upperBodyWeight = 1.0f - oneShotUpperWeight;
    const f32 lowerBodyWeight = 1.0f - oneShotLowerWeight;
    // We must split layers into upper and lower body so that we can blend the one shot
    // with each layer separately
    const int numLayers = updateContext.numLayers;
    for (i32 i = 0; i < numLayers; ++i) {
        if (oneShotUpperWeight == 1.0f) {
            // One shot completely dominates upper body,
            // so just blend against lower body
            auto& layer = updateContext.layers[i];
            layer.joint_weights = ozz::make_span(rigDef->mLowerBodyJointWeights);
            layer.weight *= lowerBodyWeight;
        }
        else {
            // Split into two layers for upper and lower portion
            auto& upperLayer = updateContext.layers[i];

            // Lower
            auto& lowerLayer = updateContext.layers[updateContext.numLayers++];
            lowerLayer.joint_weights = ozz::make_span(rigDef->mLowerBodyJointWeights);
            lowerLayer.weight = upperLayer.weight * lowerBodyWeight;
            lowerLayer.transform = upperLayer.transform;
           
            // Upper
            upperLayer.joint_weights = ozz::make_span(rigDef->mUpperBodyJointWeights);
            upperLayer.weight *= upperBodyWeight;
        }
    }
}

void AnimMachineInstance::onBeginState(AnimMachineInstanceState& state) {
    switch (state.stateType) {
        case AnimStateType::AnimSequence:
            state.anim.time = 0.0f;
            break;
        case AnimStateType::Blendspace1D:
            blendspace1DPlayers[state.blendspace1d.playerId].resetSyncAlpha();
            break;
        case AnimStateType::Blendspace2D:
            panic("implement onBeginState for blendspace 2D");
            break;
        default:
            panic("Invalid state type");
    }
    static_assert(e_count(AnimStateType) == 3);
}
