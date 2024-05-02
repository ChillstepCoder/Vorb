#pragma once

struct CharacterRenderState {
    entt::entity mEntityID;
    f32v3 mPos;
    f32v2 mVelocity2D;
    f32 mRotation;
    CharacterLocomotionMode mLocomotionMode;
};
static_assert(sizeof(CharacterRenderState) == 32, "Keep small");