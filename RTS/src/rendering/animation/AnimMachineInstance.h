#pragma once

#include "definitions/AnimMachineDef.h"
#include "Blendspace1DPlayer.h"

struct AnimMachineInstanceState {
    union {
        struct {
            AssetID id;
            f32 time;
        } anim;
        struct {
            Blendspace1DPlayer* player;
        } blendspace1d;
        struct {
            //Blendspace2DPlayer* player;
        } blendspace2d;
    };
};
static_assert(e_count(AnimStateType) == 3);

class AnimMachineInstance {
public:
    AnimMachineInstance() = default;
    AnimMachineInstance(AssetID animMachineID);

    x; // TODO: Initialize

    AssetHandlePtr<AnimMachineDef> machineDefHandle;
    std::unique_ptr<AnimMachineInstanceState[]> states;
    std::unique_ptr<Blendspace1DPlayer[]> blendspacePlayers;
    f32 currentTransitionTime = 0.0f;
    AnimTransitionID currentTransition = INVALID_ANIM_TRANSITION;
    AnimStateID currentState = 0;
    ui8 numBlendspacePlayers = 0;
    ui8 numStates = 0;
};
