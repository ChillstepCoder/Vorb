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
            AssetRawPtr<AnimationDef> animDef;
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

private:
    // Initialize using the instanceTemplate on the AnimMachineDef
    void initInternal(const AnimMachineDef& def);

    void updateLoopingAnimSequence(AnimMachineInstanceState& state, f32 elapsedSec, AnimMachineUpdateContext& updateContext);
    void updateBlendspace1D(AnimMachineInstanceState& state, f32 elapsedSec, AnimMachineUpdateContext& updateContext);
    void updateBlendspace2D(AnimMachineInstanceState& state, f32 elapsedSec, AnimMachineUpdateContext& updateContext);

    AssetHandlePtr<AnimMachineDef> machineDefHandle;
    const RigDef* rigDef = nullptr;
    std::unique_ptr<AnimMachineInstanceState[]> states;
    std::unique_ptr<Blendspace1DPlayer[]> blendspace1DPlayers;
    f32 currentTransitionTime = 0.0f;
    AnimTransitionID currentTransition = INVALID_ANIM_TRANSITION;
    AnimStateID currentStateID = 0;
    ui8 numBlendspace1DPlayers = 0;
    ui8 numStates = 0;
};
