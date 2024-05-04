#pragma once

#include "rendering/model/AnimationConst.h"
#include "rendering/model/skeletal/AnimTransitionCondition.h"

// TODO: REMOVE
#include "rendering/model/skeletal/AnimTransitionConditionDef.h"

class AnimMachineInstance;

namespace ozz {
    namespace animation {
        class Animation;
    };
};

typedef ui8 AnimStateID;
typedef ui8 AnimTransitionID;
constexpr auto INVALID_ANIM_STATE = std::numeric_limits<AnimStateID>::max();
constexpr auto MAX_ANIM_STATES = std::numeric_limits<AnimStateID>::max() - 1;
constexpr auto INVALID_ANIM_TRANSITION = std::numeric_limits<AnimTransitionID>::max();
constexpr auto MAX_ANIM_TRANSITIONS = std::numeric_limits<AnimTransitionID>::max() - 1;

struct AnimTransitionDef {
    AnimTransitionConditionFileData condition;
    SoftAssetReference transitionAnim = SoftAssetReference(AssetType::Animation);
    f32 transitionDuration = 0.0f;
    StrToken toState;
};
SERIALIZABLE_IMGUI_CONTROLLED(AnimTransitionDef,
    make_field(o.condition, "condition"sv),
    make_field(o.transitionAnim, "transition_anim"sv),
    make_field(o.transitionDuration, "transition_dur"sv),
    make_field(o.toState, "to_state"sv)
);

struct AnimTransition {
    // Deliberately just a single condition for now, as these are custom code driven
    AnimTransitionCondition condition;
    AssetID transitionAnimID = INVALID_ASSET_ID;
    f32 transitionDuration = 0.0f;
    AnimStateID toState = INVALID_ANIM_STATE;
};

enum class AnimStateType : ui8 {
    AnimSequence,
    Blendspace1D,
    Blendspace2D,
    COUNT,
    INVALID = COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(AnimStateType,
    ENUM_FIELD_SIMPLE(AnimStateType, AnimSequence),
    ENUM_FIELD_SIMPLE(AnimStateType, Blendspace1D),
    ENUM_FIELD_SIMPLE(AnimStateType, Blendspace2D),
);

struct AnimMachineStateDef {
    StrToken name;
    std::vector<AnimTransitionDef> transitions;
    SoftAssetReference assetRef; // Could be any of the state type
    AnimStateType stateType = AnimStateType::INVALID;
    
};
SERIALIZABLE_IMGUI_CONTROLLED(AnimMachineStateDef, 
    make_field(o.name, "name"sv),
    make_field(o.transitions, "transitions"sv),
    make_field(o.assetRef, "asset_name"sv),
    make_field(o.stateType, "state_type"sv)
);

// Efficient representation
struct AnimMachineState {
    // Sorted high to low priority
    std::unique_ptr<AnimTransition[]> transitions;
    AssetID assetId = INVALID_ASSET_ID;
    ui8 numTransitions = 0;
    AnimStateType stateType = AnimStateType::INVALID;
};

// Make sure order and contents of the animation machine name and animation arrays are the same
class AnimMachineDef : public IAsset {
public:
    AnimMachineDef(StrToken name, AssetID id);
    ~AnimMachineDef();
    AssetType getAssetType() const override { return AssetType::AnimMachine; }
    inline static constexpr AssetType ASSET_TYPE = AssetType::AnimMachine;

    SoftAssetReference rigDef = SoftAssetReference(AssetType::Rig);
    // TODO: USE
    // Editor representation, State 0 is entry state
    std::vector<AnimMachineStateDef> stateDefs;
    // Efficient representation used to spin off AnimMachineInstances, State 0 is entry state
    std::vector<AnimMachineState> states;
    // Useful for quickly initializing AnimMachineInstance
    int totalBlendspace1Ds = 0;
    int totalBlendspace2Ds = 0;

    // Used for fast initialization of new AnimMachineInstances
    // Created in AnimMachineRepository
    std::unique_ptr<AnimMachineInstance> instanceTemplate;
};
SERIALIZABLE_IMGUI_CONTROLLED(AnimMachineDef,
    make_field(o.rigDef, "rig"sv),
    make_field(o.stateDefs, "states"sv)
);


// TEST ANIM MACHINE
// States
// idle:
//   type: anim_sequence
//   transitions: 
//   - condition: "input.move" == 0
//     transition_duration: 0.2
//     transition_sequence: idle_to_walk
//     speed_curve: CURVE_LINEAR (TODO: Custom Curves)
//     TODO: extract root motion?
//     to_state: walk
// walk:
//   type: blendspace2d
//   transitions:
//   - condition: "input.move" == 0
//     transition_duration: 0.2
//     to_state: idle
//   - condition: "input.move" > 0
//     transition_duration: 0.2
//     to_state: walk
//   - condition: "input.move" < 0
//     transition_duration: 0.2
//     to_state: walk
