#pragma once
// TODO: Hardware skinning? https://github.com/ConfettiFX/The-Forge/tree/master/Examples_3/Unit_Tests/src/28_Skinning

class ModelDef;
struct MeshSkeletonData;

#include "character/CharacterConst.h"


class CharacterAnimator {
public:
    bool updateAnimation(const MeshSkeletonData& skeletonData, CharacterAnimState& animState, CharacterLocomotionMode locomotionMode, const ModelDef& modelDef, ozz::vector<ozz::math::Float4x4>& models, f32 elapsedSec);

protected:
    void updateAnimationStates(CharacterAnimState& animState, CharacterLocomotionMode locomotionMode, f32 elapsedSec);
    bool tryInitializeCharacterAnimState(CharacterAnimState& animState, const ModelDef* modelDefPtr);
};

