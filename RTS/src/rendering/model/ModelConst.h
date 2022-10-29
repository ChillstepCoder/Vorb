#pragma once

#include "rendering/model/AnimationConst.h"

typedef ui32 ModelID;
constexpr ui32 INVALID_MODEL_ID = UINT32_MAX;
constexpr ui32 NUM_ANIM_STATE_TRACKS = e_cast(AnimMachineState::COUNT);