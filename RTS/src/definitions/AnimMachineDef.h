#pragma once

#include "rendering/model/AnimationConst.h"
#include "rendering/model/skeletal/AnimTransitionCondition.h"

// TODO: REMOVE
#include "rendering/model/skeletal/AnimTransitionConditionDef.h"

namespace ozz {
    namespace animation {
        class Animation;
    };
};

typedef ui8 AnimStateID;
constexpr auto MAX_ANIM_STATES = std::numeric_limits<AnimStateID>::max();

struct AnimTransitionDef {
    AnimTransitionConditionFileData condition;
    StrToken transitionAnim;
    f32 transitionDuration;
    StrToken toState;
};
SERIALIZABLE_IMGUI_CONTROLLED(AnimTransitionDef,
    make_field(o.condition, "condition"sv),
    make_field(o.transitionAnim, "tran_anim"sv),
    make_field(o.transitionDuration, "tran_dur"sv),
    make_field(o.toState, "to_state"sv)
);

struct AnimTransition {
    // Deliberately just a single condition for now, as these are custom code driven
    AnimTransitionCondition condition;
    AssetID transitionAnim = INVALID_ASSET_ID;
    f32 transitionDuration;
    AnimStateID toState;
};

enum class AnimStateType : ui8 {
    AnimSequence,
    Blendspace2D,
    Blendspace3D
};
SERIALIZABLE_ENUM_SAME_NAME(AnimStateType,
    ENUM_FIELD_SIMPLE(AnimStateType, AnimSequence),
    ENUM_FIELD_SIMPLE(AnimStateType, Blendspace2D),
    ENUM_FIELD_SIMPLE(AnimStateType, Blendspace3D),
);

struct AnimStateDef {
    StrToken name;
    std::vector<AnimTransitionDef> transitions;
    StrToken assetName; // Could be any of the state type
    AnimStateType stateType;
};
SERIALIZABLE_IMGUI_CONTROLLED(AnimStateDef, 
    make_field(o.name, "name"sv),
    make_field(o.transitions, "transitions"sv),
    make_field(o.assetName, "asset_name"sv),
    make_field(o.stateType, "state_type"sv)
);

// Efficient representation
struct AnimState {
    std::unique_ptr<AnimTransition[]> transitions;
    AssetID assetId;
    ui8 numTransitions;
    AnimStateType stateType;
};

// Make sure order and contents of the animation machine name and animation arrays are the same
class AnimMachineDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(AnimMachineDef, AssetType::AnimMachine);

    SoftAssetReference rigDef = SoftAssetReference(AssetType::Rig);
    // TODO: USE
    // Editor representation, State 0 is entry state
    std::vector<AnimStateDef> stateDefs;
    // Efficient representation, State 0 is entry state
    std::vector<AnimState> states;
};
SERIALIZABLE_IMGUI_CONTROLLED(AnimMachineDef,
    make_field(o.rigDef, "rig"sv)
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
