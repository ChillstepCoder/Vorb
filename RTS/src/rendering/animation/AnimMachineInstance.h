#pragma once

#include "definitions/AnimMachineDef.h"
#include "Blendspace1DPlayer.h"

#include "rendering/model/skeletal/AnimVariableFloatBinding.h"
#include "rendering/model/skeletal/SkeletalAnimationSampleContext.h"

class RigDef;

struct AnimMachineInstanceState {
    AnimMachineInstanceState() {}
    ~AnimMachineInstanceState() {}
    union {
        struct {
            // TODO: Flyweight this too like blendspace?
            LiteAssetRef<AssetType::Animation> animDef;
            f32 time;
        } anim;
        struct {
            ui8 playerId;
        } blendspace1d;
        struct {
            //Blendspace2DPlayer* player;
        } blendspace2d;
    };
    // Points to the machine def, TODO: Will be invalid if we edit the machine at run time
    std::span<const AnimTransition> transitions;
    AnimStateType stateType = AnimStateType::INVALID;
};
static_assert(e_count(AnimStateType) == 3);

struct AnimMachineUpdateContext;

class AnimMachineInstance {
    friend class AnimMachineRepository;
public:
    AnimMachineInstance() = default;
    AnimMachineInstance(AssetID animMachineID);

    // Return model matrices, must be skinned via SkeletalAnimator::skinModelMatricesToMesh
    // outModelMatrices must be large enough for all skeletons or we assert
    void update(f32 elapsedSec, const AnimVariables& animVariables, OzzMatrixSpan outModelMatrices);

    bool isValid() const { return machineDefHandle != nullptr; }
    const RigDef& getRig() const { assert(rigDef); return *rigDef; }
    
    // Return false if there is already a one shot playing
    bool tryPlayOneShot(const AnimationDef& animDef);

private:
    // Initialize using the instanceTemplate on the AnimMachineDef
    void initInternal(const AnimMachineDef& def);

    void updateState(AnimMachineInstanceState& state, f32 elapsedSec, AnimMachineUpdateContext& updateContext, f32 weight);
    void updateLoopingAnimSequence(AnimMachineInstanceState& state, f32 elapsedSec, AnimMachineUpdateContext& updateContext, f32 weight);
    void updateBlendspace1D(AnimMachineInstanceState& state, f32 elapsedSec, AnimMachineUpdateContext& updateContext, f32 weight);
    void updateBlendspace2D(AnimMachineInstanceState& state, f32 elapsedSec, AnimMachineUpdateContext& updateContext, f32 weight);

    void updateOneShot(f32 elapsedSec, AnimMachineUpdateContext& updateContext);
    void blendAndSplitLayersForOneShot(AnimMachineUpdateContext& updateContext, f32 oneShotUpperWeight, f32 oneShotLowerWeight);

    void onBeginState(AnimMachineInstanceState& state);

    AssetHandlePtr<AnimMachineDef> machineDefHandle;
    const RigDef* rigDef = nullptr;
    std::unique_ptr<AnimMachineInstanceState[]> states;
    std::unique_ptr<Blendspace1DPlayer[]> blendspace1DPlayers;
    f32 currentTransitionTime = 0.0f;
    AnimTransitionID currentTransitionID = INVALID_ANIM_TRANSITION;
    AnimStateID currentStateID = 0;
    ui8 numBlendspace1DPlayers = 0;
    ui8 numStates = 0;
    const AnimationDef* oneShotAnim = nullptr;
    f32 oneShotTime = 0.0f;
};
