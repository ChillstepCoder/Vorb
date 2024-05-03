#include "stdafx.h"
#include "AnimMachineInstance.h"

#include "definitions/RigDef.h"

#include "resources/AnimMachineRepository.h"
#include "rendering/model/skeletal/SkeletalAnimator.h"


// 6 layers accounts for blending between two blendspace2D
constexpr ui32 MAX_ANIM_UPDATE_CONTEXT_LAYERS = 6;
struct AnimMachineUpdateContext {
    ozz::animation::BlendingJob::Layer layers[MAX_ANIM_UPDATE_CONTEXT_LAYERS];
    ozz::math::SoaTransform transforms[MAX_ANIM_UPDATE_CONTEXT_LAYERS][MAX_JOINTS_IN_RIG];
    int numLayers = 0;
    const AnimVariables* variables = nullptr;
};

AnimMachineInstance::AnimMachineInstance(AssetID animMachineID) {
    machineDefHandle = AnimMachineRepository::get().getAssetHandle(animMachineID);
    // Should already be loaded by the model
    initInternal(machineDefHandle->getLoadedAsset());
}

void AnimMachineInstance::update(f32 elapsedSec, const AnimVariables& animVariables, OzzMatrixSpan outModelMatrices) {
    assert(states);
    assert(currentStateID < numStates);
    AnimMachineInstanceState& currentState = states[currentStateID];

    // This is a very large stack allocation
    AnimMachineUpdateContext updateContext;
    updateContext.variables = &animVariables;

    // TODO: Non instant transitions, state blending
    /*if (currentTransition != INVALID_ANIM_TRANSITION) {

    }
    else {*/
        // Update the current state
    switch (currentState.stateType) {
        case AnimStateType::AnimSequence:
            updateLoopingAnimSequence(currentState, elapsedSec, updateContext);
            break;
        case AnimStateType::Blendspace1D:
            updateBlendspace1D(currentState, elapsedSec, updateContext);
            break;
        case AnimStateType::Blendspace2D:
            updateBlendspace2D(currentState, elapsedSec, updateContext);
            break;
        default:
            panic("Invalid state type");
    }
    static_assert(e_count(AnimStateType) == 3);
    //}

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

void AnimMachineInstance::initInternal(const AnimMachineDef& def) {
    assert(def.instanceTemplate);
    const AnimMachineInstance& defaultInstance = *def.instanceTemplate;

    // Fast initialization using the template
    machineDefHandle = AnimMachineRepository::get().getAssetHandle(def.getID());
    rigDef = defaultInstance.rigDef;
    numStates = defaultInstance.numStates;
    numBlendspace1DPlayers = defaultInstance.numBlendspace1DPlayers;
    states = std::make_unique<AnimMachineInstanceState[]>(numStates);
    blendspace1DPlayers = std::make_unique<Blendspace1DPlayer[]>(numBlendspace1DPlayers);
    memcpy(states.get(), defaultInstance.states.get(), numStates * sizeof(AnimMachineInstanceState));
    memcpy(blendspace1DPlayers.get(), defaultInstance.blendspace1DPlayers.get(), numBlendspace1DPlayers * sizeof(Blendspace1DPlayer));
}

void AnimMachineInstance::updateLoopingAnimSequence(AnimMachineInstanceState& state, f32 elapsedSec, AnimMachineUpdateContext& updateContext) {
    state.anim.time += elapsedSec;

    const AnimationDef& animDef = state.anim.animDef.getLoadedOrUnloadedAsset();

    if (state.anim.time > animDef.animation.duration()) [[unlikely]] {
        state.anim.time = fmod(state.anim.time, animDef.animation.duration());
    }

    // TODO: Cache context for performance!
    SkeletalAnimationSampleContext context;
    context.anim = &animDef.animation;
    context.time = state.anim.time;
    const int NUM_JOINTS = rigDef->mSkeleton.num_joints();
    const int NUM_SOA_JOINTS = rigDef->mSkeleton.num_soa_joints();
    context.samplingContext.Resize(NUM_JOINTS);

    assert(updateContext.numLayers < MAX_ANIM_UPDATE_CONTEXT_LAYERS);
    auto& layer = updateContext.layers[updateContext.numLayers];
    layer.weight = 1.0f;
    OzzSoaTransformSpan transforms = OzzSoaTransformSpan(updateContext.transforms[updateContext.numLayers], NUM_SOA_JOINTS);
    if (!SkeletalAnimator::samplePose(context, *rigDef, transforms)) [[unlikely]] {
        panic("Anim sample fail!");
    }
    layer.transform = transforms;
    ++updateContext.numLayers;
}

void AnimMachineInstance::updateBlendspace1D(AnimMachineInstanceState& state, f32 elapsedSec, AnimMachineUpdateContext& updateContext) {
    const int NUM_SOA_JOINTS = rigDef->mSkeleton.num_soa_joints();
    assert(state.blendspace1d.playerId < numBlendspace1DPlayers);
    Blendspace1DPlayer& player = blendspace1DPlayers[state.blendspace1d.playerId];
    AnimSampleBlendDataPair blendData = player.updateAndGetBlendData(*updateContext.variables, elapsedSec);
    for (int i = 0; i < blendData.validCount; ++i) {

        assert(updateContext.numLayers < MAX_ANIM_UPDATE_CONTEXT_LAYERS);
        auto& layer = updateContext.layers[updateContext.numLayers];
        const AnimSampleBlendData& data = blendData.arry[i];
        layer.weight = data.weight;

        // TODO: Cache context for performance!
        SkeletalAnimationSampleContext context;
        context.anim = &data.anim->animation;
        context.time = data.animTime;
        OzzSoaTransformSpan transforms = OzzSoaTransformSpan(updateContext.transforms[updateContext.numLayers], NUM_SOA_JOINTS);
        if (!SkeletalAnimator::samplePose(context, *rigDef, transforms)) [[unlikely]] {
            panic("Anim sample fail!");
        }
        layer.transform = transforms;
        ++updateContext.numLayers;
    }
}

void AnimMachineInstance::updateBlendspace2D(AnimMachineInstanceState& state, f32 elapsedSec, AnimMachineUpdateContext& updateContext) {
    panic("Implement updateBlendspace2D");
}
